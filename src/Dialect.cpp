#include "librazobill/Dialect.hpp"

#include <string>

// ARCHITECTURE: Multi-Engine SQL Dialect Abstraction
// DuckDB and ClickHouse are both incredible columnar OLAP engines, but their SQL dialects
// disagree on almost everything:
// - Types: DuckDB uses `UBIGINT`, ClickHouse uses `UInt64`.
// - String escaping: DuckDB doubles single quotes (' -> ''), ClickHouse backslash-escapes (' -> \').
// - Storage: DuckDB is single-file relational, ClickHouse uses distributed MergeTree engines.
//
// We encapsulate these differences behind `Dialect` so higher-level pipelines stay pure
// across disparate relational and distributed analytical database backends.
#include "librazobill/Scanner.hpp"

namespace razobill {

namespace {

constexpr std::string_view kTokenRegex = R"([^\s=,;:()\[\]{}"'<>|/]*[0-9][^\s=,;:()\[\]{}"'<>|/]*)";
constexpr std::string_view kIntRegex = "^(0|[1-9][0-9]{0,17})$";
constexpr unsigned char kControlReserved17 = 0x11;

class DuckDialect final : public Dialect {
public:
    Flavor flavor() const noexcept override { return Flavor::DuckDB; }
    bool supportsCompact() const noexcept override { return true; }
    bool denseFormKey() const noexcept override { return true; }
    std::string quote(std::string_view s) const override {
        std::string o;
        o.reserve(s.size() + 16);
        o.push_back('\'');
        for (char c : s) {
            if (c == '\'') o.push_back('\'');
            o.push_back(c);
        }
        o.push_back('\'');
        return o;
    }
    std::string tU64() const override { return "UBIGINT"; }
    std::string tI32() const override { return "INTEGER"; }
    std::string tF64() const override { return "DOUBLE"; }
    std::string tStr() const override { return "VARCHAR"; }
    std::string tNullableStr() const override { return "VARCHAR"; }
    std::string tableTail(std::string_view orderBy) const override {
        (void)orderBy;
        return "";
    }
    std::string toText(std::string_view e) const override { return "CAST(" + std::string(e) + " AS VARCHAR)"; }
    std::string matches(std::string_view h, std::string_view p) const override {
        return "regexp_matches(" + std::string(h) + ", " + std::string(p) + ")";
    }
    std::string formExpr(std::string_view col, core::ScanMode /*mode*/ = core::ScanMode::Semantic) const override {
        return "RzTemplate(" + std::string(col) + ")";
    }
    std::string hashExpr(std::string_view t) const override { return "RzHash(" + std::string(t) + ")"; }
    std::string templateExpr(std::string_view f) const override { return std::string(f); }
    std::string truncate(std::string_view t) const override { return "DELETE FROM " + std::string(t); }
};

class ClickHouseDialect final : public Dialect {
public:
    Flavor flavor() const noexcept override { return Flavor::ClickHouse; }
    bool supportsCompact() const noexcept override { return false; }
    bool denseFormKey() const noexcept override { return false; }
    std::string quote(std::string_view s) const override {
        std::string o;
        o.reserve(s.size() + 16);
        o.push_back('\'');
        for (char c : s) {
            if (c == '\0') {
                o.push_back('\\');
                o.push_back('0');
            } else {
                if (c == '\\' || c == '\'') o.push_back('\\');
                o.push_back(c);
            }
        }
        o.push_back('\'');
        return o;
    }
    std::string tU64() const override { return "UInt64"; }
    std::string tI32() const override { return "Int32"; }
    std::string tF64() const override { return "Float64"; }
    std::string tStr() const override { return "String"; }
    std::string tNullableStr() const override { return "Nullable(String)"; }
    std::string tableTail(std::string_view orderBy) const override {
        return "ENGINE = MergeTree ORDER BY " + std::string(orderBy);
    }
    std::string toText(std::string_view e) const override { return "toString(" + std::string(e) + ")"; }
    std::string matches(std::string_view h, std::string_view p) const override {
        return "match(" + std::string(h) + ", " + std::string(p) + ")";
    }
    std::string formExpr(std::string_view col, core::ScanMode /*mode*/ = core::ScanMode::Semantic) const override {
        const std::string c(col);
        const std::string tok = quote(kTokenRegex);
        const std::string ints = quote(kIntRegex);
        const std::string ch17 = "char(" + std::to_string(static_cast<int>(kControlReserved17)) + ")";
        const std::string ch18 = "char(" + std::to_string(static_cast<int>(core::kPlaceholder)) + ")";
        const std::string ch19 = "char(" + std::to_string(static_cast<int>(core::kTypeSeparator)) + ")";
        return "if(position(" + c + ", " + ch17 + ") > 0 OR position(" + c + ", " + ch18 + ") > 0 OR position(" + c +
               ", " + ch19 + ") > 0, concat(" + ch18 + ", " + ch19 + ", 'S'), concat(replaceRegexpAll(" + c + ", " +
               tok + ", " + ch18 + "), " + ch19 + ", arrayStringConcat(arrayMap(t -> if(match(t, " + ints +
               "), 'I', 'S'), extractAll(" + c + ", " + tok + ")))))";
    }
    std::string hashExpr(std::string_view t) const override { return "xxHash64(" + std::string(t) + ")"; }
    std::string templateExpr(std::string_view f) const override {
        const std::string s(f);
        const std::string ch18 = "char(" + std::to_string(static_cast<int>(core::kPlaceholder)) + ")";
        const std::string ch19 = "char(" + std::to_string(static_cast<int>(core::kTypeSeparator)) + ")";
        return "if(position(" + s + ", " + ch19 + ") > 0, replaceAll(substring(" + s + ", 1, position(" + s + ", " +
               ch19 + ") - 1), " + ch18 + ", '<*>'), replaceAll(" + s + ", " + ch18 + ", '<*>'))";
    }
    std::string truncate(std::string_view t) const override { return "TRUNCATE TABLE " + std::string(t); }
};

} // namespace

std::unique_ptr<Dialect> makeDialect(Flavor f) {
    if (f == Flavor::DuckDB) return std::make_unique<DuckDialect>();
    return std::make_unique<ClickHouseDialect>();
}

} // namespace razobill
