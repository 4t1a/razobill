// DuckDB adapter: exposes librazobill data plane as scalar functions through the stable C extension API.
//
// ARCHITECTURE: Vectorized SQL Scalar Execution
// DuckDB processes queries using vector-at-a-time execution (typically 2,048 rows per chunk).
// Rather than allocating intermediate C++ strings or structures for each row in the chunk,
// we process directly from raw memory pointers (`duckdb_string_t`) into the result vector.
//
// CONCURRENCY & ZERO ALLOCATION:
// DuckDB executes scalar functions across multiple background worker threads simultaneously.
// We use a `thread_local Scratch` struct. Every OS thread has its own private parsing arena:
// - ZERO locks or atomic operations on the query hot path.
// - Buffers expand once to steady-state capacity and are reused across all subsequent chunks.
// - A 1 MB retention cap (`kScratchKeepBytes`) automatically prunes memory if an anomalous log
//   caused temporary buffer spikes.
#include <cstring>
#include <exception>
#include <string>
#include <string_view>
#include <vector>

#include "duckdb_extension.h"
#include "librazobill/Hash.hpp"
#include "librazobill/Slots.hpp"
#include "librazobill/Stats.hpp"

DUCKDB_EXTENSION_EXTERN

namespace {

using razobill::Status;
namespace core = razobill::core;

// 1 MB high-water mark: if a thread parses gigantic logs, prune buffers back to 1MB
constexpr std::size_t kScratchKeepBytes = std::size_t{1} << 20;

enum class Kind : unsigned char { Int, Str, IntList, StrList, Unsupported };

struct Arg {
    Kind kind = Kind::Unsupported;
    void* data = nullptr;
    std::uint64_t* validity = nullptr;
    void* childData = nullptr;
    std::uint64_t* childValidity = nullptr;
};

// Thread-local scratchpad: holds slot buffers and working memory for one DuckDB worker thread.
struct Scratch {
    core::SlotRecord slots;
    std::string text;
    std::vector<std::int64_t> ints;
    std::vector<std::string_view> strs;
    std::vector<double> doubleBuf;
    std::vector<Arg> args;

    // Prunes excess capacity after processing a data chunk
    void trim() {
        if (text.capacity() > kScratchKeepBytes) std::string().swap(text);
        if (slots.skeleton.capacity() > kScratchKeepBytes) std::string().swap(slots.skeleton);
        if (ints.capacity() * sizeof(std::int64_t) > kScratchKeepBytes) std::vector<std::int64_t>().swap(ints);
        if (strs.capacity() * sizeof(std::string_view) > kScratchKeepBytes) std::vector<std::string_view>().swap(strs);
        if (doubleBuf.capacity() * sizeof(double) > kScratchKeepBytes) std::vector<double>().swap(doubleBuf);
    }
};

Scratch& scratch() {
    thread_local Scratch s;
    return s;
}

// Zero-copy string view from DuckDB string representation.
// NOTE: DuckDB stores short strings (<= 12 bytes) inline inside the struct,
// while longer strings point to external heap pages. duckdb_string_t_data(&s) abstracts both.
inline std::string_view viewOf(duckdb_string_t& s) {
    return {duckdb_string_t_data(&s), duckdb_string_t_length(s)};
}
inline bool isValid(std::uint64_t* validity, idx_t row) {
    return validity == nullptr || duckdb_validity_row_is_valid(validity, row);
}
inline void setNull(duckdb_vector out, idx_t row) {
    duckdb_vector_ensure_validity_writable(out);
    duckdb_validity_set_row_invalid(duckdb_vector_get_validity(out), row);
}
inline void fail(duckdb_function_info info, const char* fn, Status s) {
    std::string m = std::string(fn) + ": " + razobill::describe(s);
    duckdb_scalar_function_set_error(info, m.c_str());
}
inline void put(duckdb_vector out, idx_t row, std::string_view v) {
    duckdb_vector_assign_string_element_len(out, row, v.data(), v.size());
}

// ---- rb_form / rbForm ----
template <core::ScanMode Mode = core::ScanMode::Exact>
void rbFormImpl(duckdb_function_info info, duckdb_data_chunk in, duckdb_vector out) {
    auto& sc = scratch();
    const idx_t n = duckdb_data_chunk_get_size(in);
    auto vec = duckdb_data_chunk_get_vector(in, 0);
    auto* data = static_cast<duckdb_string_t*>(duckdb_vector_get_data(vec));
    auto* vv = duckdb_vector_get_validity(vec);
    for (idx_t r = 0; r < n; ++r) {
        if (!isValid(vv, r)) {
            setNull(out, r);
            continue;
        }
        if (auto s = core::parse(viewOf(data[r]), sc.slots, Mode); s != Status::Ok) {
            fail(info, "rb_form", s);
            break;
        }
        sc.slots.formKey(sc.text);
        put(out, r, sc.text);
    }
    sc.trim();
}

// ---- rb_ints / rb_strs ----
template <bool Ints, core::ScanMode Mode = core::ScanMode::Exact>
void rbListImpl(duckdb_function_info info, duckdb_data_chunk in, duckdb_vector out) {
    auto& sc = scratch();
    const idx_t n = duckdb_data_chunk_get_size(in);
    auto vec = duckdb_data_chunk_get_vector(in, 0);
    auto* data = static_cast<duckdb_string_t*>(duckdb_vector_get_data(vec));
    auto* vv = duckdb_vector_get_validity(vec);
    auto child = duckdb_list_vector_get_child(out);
    duckdb_list_entry* entries = static_cast<duckdb_list_entry*>(duckdb_vector_get_data(out));

    idx_t total = 0;
    for (idx_t r = 0; r < n; ++r) {
        if (!isValid(vv, r)) {
            entries[r] = {total, 0};
            setNull(out, r);
            continue;
        }
        if (auto s = core::parse(viewOf(data[r]), sc.slots, Mode); s != Status::Ok) {
            fail(info, "rb_list", s);
            break;
        }
        const idx_t len = Ints ? sc.slots.ints.size() : sc.slots.strs.size();
        duckdb_list_vector_reserve(out, total + len);
        entries[r] = {total, len};
        if constexpr (Ints) {
            auto* dst = static_cast<std::int64_t*>(duckdb_vector_get_data(child));
            for (idx_t i = 0; i < len; ++i)
                dst[total + i] = sc.slots.ints[i];
        } else {
            for (idx_t i = 0; i < len; ++i)
                put(child, total + i, sc.slots.strs[i]);
        }
        total += len;
    }
    duckdb_list_vector_set_size(out, total);
    sc.trim();
}

// ---- rb_template ----
void rbTemplateImpl(duckdb_function_info info, duckdb_data_chunk in, duckdb_vector out) {
    auto& sc = scratch();
    const idx_t n = duckdb_data_chunk_get_size(in);
    auto vec = duckdb_data_chunk_get_vector(in, 0);
    auto* data = static_cast<duckdb_string_t*>(duckdb_vector_get_data(vec));
    auto* vv = duckdb_vector_get_validity(vec);
    for (idx_t r = 0; r < n; ++r) {
        if (!isValid(vv, r)) {
            setNull(out, r);
            continue;
        }
        if (auto s = core::templateOf(viewOf(data[r]), sc.text); s != Status::Ok) {
            fail(info, "rb_template", s);
            break;
        }
        put(out, r, sc.text);
    }
    sc.trim();
}

// ---- rb_hash ----
void rbHashImpl(duckdb_function_info /*info*/, duckdb_data_chunk in, duckdb_vector out) {
    const idx_t n = duckdb_data_chunk_get_size(in);
    auto vec = duckdb_data_chunk_get_vector(in, 0);
    auto* data = static_cast<duckdb_string_t*>(duckdb_vector_get_data(vec));
    auto* vv = duckdb_vector_get_validity(vec);
    auto* dst = static_cast<std::uint64_t*>(duckdb_vector_get_data(out));
    for (idx_t r = 0; r < n; ++r) {
        if (!isValid(vv, r)) {
            setNull(out, r);
            continue;
        }
        dst[r] = core::xxh64(viewOf(data[r]));
    }
}

// ---- rb_decompose / rbDecompose ----
template <core::ScanMode Mode = core::ScanMode::Exact>
void rbDecomposeImpl(duckdb_function_info info, duckdb_data_chunk in, duckdb_vector out) {
    auto& sc = scratch();
    const idx_t n = duckdb_data_chunk_get_size(in);
    auto vec = duckdb_data_chunk_get_vector(in, 0);
    auto* data = static_cast<duckdb_string_t*>(duckdb_vector_get_data(vec));
    auto* vv = duckdb_vector_get_validity(vec);

    auto formChild = duckdb_struct_vector_get_child(out, 0);
    auto intsChild = duckdb_struct_vector_get_child(out, 1);
    auto strsChild = duckdb_struct_vector_get_child(out, 2);

    auto intsGrandchild = duckdb_list_vector_get_child(intsChild);
    auto* intsEntries = static_cast<duckdb_list_entry*>(duckdb_vector_get_data(intsChild));

    auto strsGrandchild = duckdb_list_vector_get_child(strsChild);
    auto* strsEntries = static_cast<duckdb_list_entry*>(duckdb_vector_get_data(strsChild));

    idx_t totalInts = 0;
    idx_t totalStrs = 0;

    for (idx_t r = 0; r < n; ++r) {
        if (!isValid(vv, r)) {
            intsEntries[r] = {totalInts, 0};
            strsEntries[r] = {totalStrs, 0};
            setNull(out, r);
            continue;
        }
        if (auto s = core::parse(viewOf(data[r]), sc.slots, Mode); s != Status::Ok) {
            fail(info, "rb_decompose", s);
            break;
        }
        sc.slots.formKey(sc.text);
        put(formChild, r, sc.text);

        const idx_t numInts = sc.slots.ints.size();
        duckdb_list_vector_reserve(intsChild, totalInts + numInts);
        intsEntries[r] = {totalInts, numInts};
        auto* dstInts = static_cast<std::int64_t*>(duckdb_vector_get_data(intsGrandchild));
        for (idx_t i = 0; i < numInts; ++i) {
            dstInts[totalInts + i] = sc.slots.ints[i];
        }
        totalInts += numInts;

        const idx_t numStrs = sc.slots.strs.size();
        duckdb_list_vector_reserve(strsChild, totalStrs + numStrs);
        strsEntries[r] = {totalStrs, numStrs};
        for (idx_t i = 0; i < numStrs; ++i) {
            put(strsGrandchild, totalStrs + i, sc.slots.strs[i]);
        }
        totalStrs += numStrs;
    }
    duckdb_list_vector_set_size(intsChild, totalInts);
    duckdb_list_vector_set_size(strsChild, totalStrs);
    sc.trim();
}

// ---- rb_entropy / rbEntropy ----
void rbEntropyImpl(duckdb_function_info /*info*/, duckdb_data_chunk in, duckdb_vector out) {
    auto& sc = scratch();
    const idx_t n = duckdb_data_chunk_get_size(in);
    auto vec = duckdb_data_chunk_get_vector(in, 0);
    auto* entries = static_cast<const duckdb_list_entry*>(duckdb_vector_get_data(vec));
    auto* vv = duckdb_vector_get_validity(vec);
    auto child = duckdb_list_vector_get_child(vec);
    auto* childData = static_cast<const double*>(duckdb_vector_get_data(child));
    auto* childVv = duckdb_vector_get_validity(child);
    auto* dst = static_cast<double*>(duckdb_vector_get_data(out));

    for (idx_t r = 0; r < n; ++r) {
        if (!isValid(vv, r)) {
            setNull(out, r);
            continue;
        }
        const auto entry = entries[r];
        sc.doubleBuf.clear();
        sc.doubleBuf.reserve(entry.length);
        for (idx_t i = 0; i < entry.length; ++i) {
            if (isValid(childVv, entry.offset + i)) {
                sc.doubleBuf.push_back(childData[entry.offset + i]);
            }
        }
        dst[r] = razobill::stats::entropyOfCounts(sc.doubleBuf);
    }
    sc.trim();
}

// ---- rb_miller_madow / rbMillerMadow ----
void rbMillerMadowImpl(duckdb_function_info /*info*/, duckdb_data_chunk in, duckdb_vector out) {
    const idx_t n = duckdb_data_chunk_get_size(in);
    auto vecH = duckdb_data_chunk_get_vector(in, 0);
    auto vecK = duckdb_data_chunk_get_vector(in, 1);
    auto vecN = duckdb_data_chunk_get_vector(in, 2);

    auto* dataH = static_cast<const double*>(duckdb_vector_get_data(vecH));
    auto* dataK = static_cast<const int64_t*>(duckdb_vector_get_data(vecK));
    auto* dataN = static_cast<const double*>(duckdb_vector_get_data(vecN));

    auto* vvH = duckdb_vector_get_validity(vecH);
    auto* vvK = duckdb_vector_get_validity(vecK);
    auto* vvN = duckdb_vector_get_validity(vecN);

    auto* dst = static_cast<double*>(duckdb_vector_get_data(out));

    for (idx_t r = 0; r < n; ++r) {
        if (!isValid(vvH, r) || !isValid(vvK, r) || !isValid(vvN, r)) {
            setNull(out, r);
            continue;
        }
        const double h = dataH[r];
        const int64_t k = dataK[r];
        const double sampleCount = dataN[r];
        dst[r] = razobill::stats::millerMadow(h, k > 0 ? static_cast<std::size_t>(k) : 0, sampleCount);
    }
}

// ---- rebuild / reconstruct ----
enum Shape { kJoin, kRebuild };

Kind classify(duckdb_logical_type t) {
    switch (duckdb_get_type_id(t)) {
    case DUCKDB_TYPE_BIGINT:
        return Kind::Int;
    case DUCKDB_TYPE_VARCHAR:
        return Kind::Str;
    case DUCKDB_TYPE_LIST: {
        auto elem = duckdb_list_type_child_type(t);
        const auto id = duckdb_get_type_id(elem);
        duckdb_destroy_logical_type(&elem);
        if (id == DUCKDB_TYPE_BIGINT) return Kind::IntList;
        if (id == DUCKDB_TYPE_VARCHAR) return Kind::StrList;
        return Kind::Unsupported;
    }
    default:
        return Kind::Unsupported;
    }
}

template <Shape S> void rebuildImpl(duckdb_function_info info, duckdb_data_chunk in, duckdb_vector out) {
    auto& sc = scratch();
    const idx_t n = duckdb_data_chunk_get_size(in);
    const idx_t numCols = duckdb_data_chunk_get_column_count(in);
    if (numCols < 1) return;

    auto formVec = duckdb_data_chunk_get_vector(in, 0);
    auto* formData = static_cast<duckdb_string_t*>(duckdb_vector_get_data(formVec));
    auto* formVv = duckdb_vector_get_validity(formVec);

    sc.args.resize(numCols - 1);
    for (idx_t c = 1; c < numCols; ++c) {
        auto vec = duckdb_data_chunk_get_vector(in, c);
        auto ltype = duckdb_vector_get_column_type(vec);
        auto& a = sc.args[c - 1];
        a.kind = classify(ltype);
        duckdb_destroy_logical_type(&ltype);
        a.data = duckdb_vector_get_data(vec);
        a.validity = duckdb_vector_get_validity(vec);
        if (a.kind == Kind::IntList || a.kind == Kind::StrList) {
            auto ch = duckdb_list_vector_get_child(vec);
            a.childData = duckdb_vector_get_data(ch);
            a.childValidity = duckdb_vector_get_validity(ch);
        }
    }

    for (idx_t r = 0; r < n; ++r) {
        if (!isValid(formVv, r)) {
            setNull(out, r);
            continue;
        }
        sc.ints.clear();
        sc.strs.clear();

        for (const auto& a : sc.args) {
            if (!isValid(a.validity, r)) continue;
            switch (a.kind) {
            case Kind::Int:
                sc.ints.push_back(static_cast<const std::int64_t*>(a.data)[r]);
                break;
            case Kind::Str:
                sc.strs.push_back(viewOf(static_cast<duckdb_string_t*>(a.data)[r]));
                break;
            case Kind::IntList: {
                const auto entry = static_cast<const duckdb_list_entry*>(a.data)[r];
                const auto* p = static_cast<const std::int64_t*>(a.childData);
                for (idx_t i = 0; i < entry.length; ++i) {
                    if (isValid(a.childValidity, entry.offset + i)) sc.ints.push_back(p[entry.offset + i]);
                }
                break;
            }
            case Kind::StrList: {
                const auto entry = static_cast<const duckdb_list_entry*>(a.data)[r];
                auto* p = static_cast<duckdb_string_t*>(a.childData);
                for (idx_t i = 0; i < entry.length; ++i) {
                    if (isValid(a.childValidity, entry.offset + i)) sc.strs.push_back(viewOf(p[entry.offset + i]));
                }
                break;
            }
            case Kind::Unsupported:
                break;
            }
        }

        if (auto s = core::reconstruct(viewOf(formData[r]), sc.ints, sc.strs, sc.text); s != Status::Ok) {
            fail(info, S == kJoin ? "rb_join" : "rb_rebuild", s);
            break;
        }
        put(out, r, sc.text);
    }
    sc.trim();
}

template <void (*Fn)(duckdb_function_info, duckdb_data_chunk, duckdb_vector)>
void guarded(duckdb_function_info info, duckdb_data_chunk in, duckdb_vector out) {
    try {
        Fn(info, in, out);
    } catch (const std::exception& ex) {
        duckdb_scalar_function_set_error(info, ex.what());
    } catch (...) {
        duckdb_scalar_function_set_error(info, "razobill: unknown exception");
    }
}

class Type {
public:
    explicit Type(duckdb_type id) : t(duckdb_create_logical_type(id)) {}
    static Type listOf(const Type& elem) { return Type(duckdb_create_list_type(elem.t)); }
    static Type structOf(const std::vector<const char*>& names, const std::vector<duckdb_logical_type>& types) {
        return Type(duckdb_create_struct_type(const_cast<duckdb_logical_type*>(types.data()),
                                              const_cast<const char**>(names.data()), names.size()));
    }
    Type(const Type&) = delete;
    Type& operator=(const Type&) = delete;
    Type(Type&& o) noexcept : t(o.t) { o.t = nullptr; }
    Type& operator=(Type&&) = delete;
    ~Type() {
        if (t) duckdb_destroy_logical_type(&t);
    }
    duckdb_logical_type t;

private:
    explicit Type(duckdb_logical_type owned) : t(owned) {}
};

bool addVarargs(duckdb_connection con, const char* name, duckdb_logical_type any, duckdb_logical_type ret,
                void (*fn)(duckdb_function_info, duckdb_data_chunk, duckdb_vector)) {
    auto f = duckdb_create_scalar_function();
    duckdb_scalar_function_set_name(f, name);
    duckdb_scalar_function_set_varargs(f, any);
    duckdb_scalar_function_set_special_handling(f);
    duckdb_scalar_function_set_return_type(f, ret);
    duckdb_scalar_function_set_function(f, fn);
    const bool ok = duckdb_register_scalar_function(con, f) == DuckDBSuccess;
    duckdb_destroy_scalar_function(&f);
    return ok;
}

bool add(duckdb_connection con, const char* name, std::initializer_list<duckdb_logical_type> params,
         duckdb_logical_type ret, void (*fn)(duckdb_function_info, duckdb_data_chunk, duckdb_vector)) {
    auto f = duckdb_create_scalar_function();
    duckdb_scalar_function_set_name(f, name);
    for (auto p : params)
        duckdb_scalar_function_add_parameter(f, p);
    duckdb_scalar_function_set_return_type(f, ret);
    duckdb_scalar_function_set_function(f, fn);
    const bool ok = duckdb_register_scalar_function(con, f) == DuckDBSuccess;
    duckdb_destroy_scalar_function(&f);
    return ok;
}

} // namespace

DUCKDB_EXTENSION_ENTRYPOINT(duckdb_connection connection, duckdb_extension_info, struct duckdb_extension_access*) {
    Type varchar(DUCKDB_TYPE_VARCHAR), bigint(DUCKDB_TYPE_BIGINT), ubigint(DUCKDB_TYPE_UBIGINT),
        doubleType(DUCKDB_TYPE_DOUBLE);
    Type any(DUCKDB_TYPE_ANY);
    Type bigintList = Type::listOf(bigint), varcharList = Type::listOf(varchar), doubleList = Type::listOf(doubleType);

    std::vector<const char*> structNames = {"form", "ints", "strs"};
    std::vector<duckdb_logical_type> structTypes = {varchar.t, bigintList.t, varcharList.t};
    Type decomposeStruct = Type::structOf(structNames, structTypes);

    // Register primary camelCase functions
    bool ok = true;
    ok = ok && add(connection, "rbForm", {varchar.t}, varchar.t, guarded<rbFormImpl<core::ScanMode::Exact>>);
    ok = ok && add(connection, "rbFormSemantic", {varchar.t}, varchar.t, guarded<rbFormImpl<core::ScanMode::Semantic>>);
    ok = ok && add(connection, "rbInts", {varchar.t}, bigintList.t, guarded<rbListImpl<true, core::ScanMode::Exact>>);
    ok = ok && add(connection, "rbIntsSemantic", {varchar.t}, bigintList.t,
                   guarded<rbListImpl<true, core::ScanMode::Semantic>>);
    ok = ok && add(connection, "rbStrs", {varchar.t}, varcharList.t, guarded<rbListImpl<false, core::ScanMode::Exact>>);
    ok = ok && add(connection, "rbStrsSemantic", {varchar.t}, varcharList.t,
                   guarded<rbListImpl<false, core::ScanMode::Semantic>>);
    ok = ok && add(connection, "rbDecompose", {varchar.t}, decomposeStruct.t,
                   guarded<rbDecomposeImpl<core::ScanMode::Exact>>);
    ok = ok && add(connection, "rbDecomposeSemantic", {varchar.t}, decomposeStruct.t,
                   guarded<rbDecomposeImpl<core::ScanMode::Semantic>>);
    ok = ok && add(connection, "rbEntropy", {doubleList.t}, doubleType.t, guarded<rbEntropyImpl>);
    ok = ok && add(connection, "rbMillerMadow", {doubleType.t, bigint.t, doubleType.t}, doubleType.t,
                   guarded<rbMillerMadowImpl>);
    ok = ok && add(connection, "rbReconstruct", {varchar.t, bigintList.t, varcharList.t}, varchar.t,
                   guarded<rebuildImpl<kJoin>>);
    ok = ok && addVarargs(connection, "rbRebuild", any.t, varchar.t, guarded<rebuildImpl<kRebuild>>);
    ok = ok && add(connection, "rbTemplate", {varchar.t}, varchar.t, guarded<rbTemplateImpl>);
    ok = ok && add(connection, "rbHash", {varchar.t}, ubigint.t, guarded<rbHashImpl>);

    // Register standard backward-compatibility aliases
    ok = ok && add(connection, "rb_form", {varchar.t}, varchar.t, guarded<rbFormImpl<core::ScanMode::Exact>>);
    ok = ok &&
         add(connection, "rb_form_semantic", {varchar.t}, varchar.t, guarded<rbFormImpl<core::ScanMode::Semantic>>);
    ok = ok && add(connection, "rb_ints", {varchar.t}, bigintList.t, guarded<rbListImpl<true, core::ScanMode::Exact>>);
    ok = ok && add(connection, "rb_ints_semantic", {varchar.t}, bigintList.t,
                   guarded<rbListImpl<true, core::ScanMode::Semantic>>);
    ok =
        ok && add(connection, "rb_strs", {varchar.t}, varcharList.t, guarded<rbListImpl<false, core::ScanMode::Exact>>);
    ok = ok && add(connection, "rb_strs_semantic", {varchar.t}, varcharList.t,
                   guarded<rbListImpl<false, core::ScanMode::Semantic>>);
    ok = ok && add(connection, "rb_decompose", {varchar.t}, decomposeStruct.t,
                   guarded<rbDecomposeImpl<core::ScanMode::Exact>>);
    ok = ok && add(connection, "rb_decompose_semantic", {varchar.t}, decomposeStruct.t,
                   guarded<rbDecomposeImpl<core::ScanMode::Semantic>>);
    ok = ok && add(connection, "rb_entropy", {doubleList.t}, doubleType.t, guarded<rbEntropyImpl>);
    ok = ok && add(connection, "rb_miller_madow", {doubleType.t, bigint.t, doubleType.t}, doubleType.t,
                   guarded<rbMillerMadowImpl>);
    ok = ok &&
         add(connection, "rb_join", {varchar.t, bigintList.t, varcharList.t}, varchar.t, guarded<rebuildImpl<kJoin>>);
    ok = ok && addVarargs(connection, "rb_rebuild", any.t, varchar.t, guarded<rebuildImpl<kRebuild>>);
    ok = ok && add(connection, "rb_template", {varchar.t}, varchar.t, guarded<rbTemplateImpl>);
    ok = ok && add(connection, "rb_hash", {varchar.t}, ubigint.t, guarded<rbHashImpl>);

    return ok;
}

// Dual-entrypoint alias: allows loading via both 'LOAD razobill;' (Community Standard)
// and 'LOAD librazobill;' (Legacy C API ABI).
DUCKDB_EXTENSION_EXTERN_C_GUARD_OPEN
DUCKDB_CAPI_ENTRY_VISIBILITY DUCKDB_EXTENSION_API bool librazobill_init_c_api(duckdb_extension_info info,
                                                                              struct duckdb_extension_access* access) {
    return razobill_init_c_api(info, access);
}
DUCKDB_EXTENSION_EXTERN_C_GUARD_CLOSE
