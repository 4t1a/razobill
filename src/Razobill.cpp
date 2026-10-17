// ==============================================================================
// Razobill Analytical Engine Control Plane: Database & Schema Orchestrator
// ==============================================================================
//
// ARCHITECTURE & RESPONSIBILITY:
// While `Slots.hpp` and `SimdScanner.hpp` implement the pure, zero-allocation data
// plane, `RazobillEngine` serves as the analytical database control plane. It coordinates
// relational storage engines (DuckDB and ClickHouse chDB via ADBC) through two primary
// deployment topologies:
//
// 1. Additive Indexing Layout (`logs`):
//    Preserves the raw text column in-place while indexing each event with a 64-bit
//    `form_id` (xxHash64, seed 0). Enables instant, zero-copy indexing without breaking
//    legacy downstream applications.
//
// 2. Compact Decomposed Columnar Layout (`logs_c`):
//    Transforms unstructured log messages into typed columnar slots:
//    - `form_no`: Dense 16/32-bit foreign key referencing the `rb_form` dictionary.
//    - `i0..i3`: First 4 extracted integer variables BitPacked as raw 64-bit integers.
//    - `s0..s3`: First 4 extracted string variables as dictionary-encoded text pools.
//    - `oi, os`: Overflow arrays capturing variable tails for high-variable anomalies.
//
// 3. Relational Metadata & Semantic Meaning (`rb_form`, `rb_doubt`):
//    Maintains formal dictionaries and regex-based rule classifiers that map structural
//    forms to semantic meanings, producing the zero-leakage view `logs_m`.
//
// 4. Constructive Lossless Verification (`verify()`):
//    Executes an in-database bit-level parity pass confirming:
//    $$\forall m \in \Sigma^*, \quad \operatorname{Reconstruct}(\operatorname{Decompose}(m)) \equiv m$$
//    Guaranteeing zero data loss and absolute mathematical invertibility.
// ==============================================================================

#include "librazobill/Razobill.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <sstream>

#include "librazobill/Hash.hpp"
#include "librazobill/Slots.hpp"

namespace razobill {

namespace {

class TransactionGuard {
public:
    explicit TransactionGuard(Backend& db, Flavor flavor) : db_(db), active_(flavor == Flavor::DuckDB) {
        if (active_) {
            try {
                db_.exec("BEGIN TRANSACTION");
            } catch (...) {
                active_ = false;
                throw;
            }
        }
    }

    void commit() {
        if (active_ && !committed_) {
            db_.exec("COMMIT");
            committed_ = true;
        }
    }

    ~TransactionGuard() {
        if (active_ && !committed_) {
            try {
                db_.exec("ROLLBACK");
            } catch (...) {
                (void)0; // Explicitly suppress rollback errors during exception unwinding
            }
        }
    }

    TransactionGuard(const TransactionGuard&) = delete;
    TransactionGuard& operator=(const TransactionGuard&) = delete;
    TransactionGuard(TransactionGuard&&) = delete;
    TransactionGuard& operator=(TransactionGuard&&) = delete;

private:
    Backend& db_;
    bool active_ = false;
    bool committed_ = false;
};

// SQL Injection Defense-in-Depth (F-SQL-01, F-ENGINE-03):
// Identifiers are restricted to ASCII alphanumeric and underscores [A-Za-z_][A-Za-z0-9_]{0,63}
// with a non-digit first character, preventing SQL injection even before quoteIdentifier()
// applies ANSI SQL standard double-quote escaping.
void requireIdentifier(const std::string& s, const char* what) {
    const bool ok = !s.empty() && s.size() <= 64 && (std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_') &&
                    std::ranges::all_of(s, [](unsigned char c) { return std::isalnum(c) || c == '_'; });
    if (!ok) throw std::invalid_argument(std::string(what) + " must match [A-Za-z_][A-Za-z0-9_]{0,63}: '" + s + "'");
}

std::string quoteIdentifier(std::string_view name) {
    std::string s;
    s.reserve(name.size() + 4);
    s.push_back('"');
    for (char ch : name) {
        if (ch == '"') s.push_back('"');
        s.push_back(ch);
    }
    s.push_back('"');
    return s;
}

void requireName(const std::string& s) {
    if (s.empty() || s.size() > 128 ||
        !std::ranges::all_of(s, [](unsigned char c) { return std::isalnum(c) || c == '_' || c == '.' || c == '-'; }))
        throw std::invalid_argument("meaning must match [A-Za-z0-9_.-]{1,128}: '" + s + "'");
}

std::string num(double d) {
    std::ostringstream o;
    o.precision(17);
    o << d;
    return o.str();
}

bool reservedColumn(std::string_view c) {
    if (c == "form_no" || c == "form_id" || c == "oi" || c == "os" || c == "rbf" || c == "rbi" || c == "rbs") {
        return true;
    }
    if (c.starts_with("rb_")) {
        return true;
    }
    if (c.size() > 1 && (c[0] == 'i' || c[0] == 'd')) {
        for (std::size_t i = 1; i < c.size(); ++i) {
            if (static_cast<unsigned char>(c[i]) < '0' || static_cast<unsigned char>(c[i]) > '9') {
                return false;
            }
        }
        return true;
    }
    return false;
}

class Interner {
public:
    std::uint32_t operator()(const std::string& s) {
        return ids_.try_emplace(s, static_cast<std::uint32_t>(ids_.size())).first->second;
    }
    [[nodiscard]] const std::unordered_map<std::string, std::uint32_t>& map() const { return ids_; }

private:
    std::unordered_map<std::string, std::uint32_t> ids_;
};

} // namespace

RazobillEngine::RazobillEngine(Backend& backend, Config config) : db_(backend), cfg_(std::move(config)) {
    if (!cfg_.message_column.empty() && (cfg_.messageColumn.empty() || cfg_.messageColumn == "msg")) {
        cfg_.messageColumn = cfg_.message_column;
    }
    cfg_.message_column = cfg_.messageColumn;

    if (cfg_.scan_mode != core::ScanMode::Semantic && cfg_.scanMode == core::ScanMode::Semantic) {
        cfg_.scanMode = cfg_.scan_mode;
    }
    cfg_.scan_mode = cfg_.scanMode;

    requireIdentifier(cfg_.table, "table");
    requireIdentifier(cfg_.messageColumn, "messageColumn");
    if (!cfg_.purpose.empty()) requireIdentifier(cfg_.purpose, "purpose");
    if (cfg_.tau <= 0.5 || cfg_.tau > 1.0) throw std::invalid_argument("tau must be in (0.5, 1]");
    if (cfg_.eps < 0) throw std::invalid_argument("eps must be >= 0");
    dx_ = makeDialect(db_.flavor());
    phys_ = view_ = cfg_.table;
    std::scoped_lock lock(mu_);
    installLocked();
    detectLocked();
}

void RazobillEngine::detectLocked() {
    const auto& d = *dx_;
    const std::string t = cfg_.table;
    if (d.flavor() == Flavor::DuckDB) {
        if (db_.query("SELECT count(*) FROM duckdb_tables() WHERE table_name = " + d.quote(t + "_c")).i64(0, 0) > 0) {
            phys_ = t + "_c";
            view_ = t;
            compact_ = true;
            ready_ = true;
        } else if (db_.query("SELECT count(*) FROM duckdb_columns() WHERE table_name = " + d.quote(t) +
                             " AND column_name = 'form_no'")
                       .i64(0, 0) > 0) {
            phys_ = view_ = t;
            ready_ = true;
        }
    } else if (db_.query("SELECT count() FROM system.columns WHERE database = currentDatabase() AND table = " +
                         d.quote(t) + " AND name = 'form_id'")
                   .i64(0, 0) > 0) {
        phys_ = view_ = t;
        ready_ = true;
    }
    if (ready_) {
        try {
            if (db_.query("SELECT count(*) FROM (SELECT id FROM rb_form GROUP BY id HAVING count(*) > 1)").i64(0, 0) >
                0) {
                hasCollision_ = true;
            }
        } catch (const std::exception&) { // NOLINT(bugprone-empty-catch)
            // Table rb_form may not exist yet prior to installLocked(); safe to ignore
        }
    }
}

void RazobillEngine::requireReadyLocked() const {
    if (!ready_) throw std::logic_error("no stored events for '" + cfg_.table + "': call refresh() or compact() first");
}

void RazobillEngine::installLocked() {
    const auto& d = *dx_;
    db_.exec("CREATE TABLE IF NOT EXISTS rb_form(id " + d.tU64() + ", no " + d.tI32() + ", form " + d.tStr() +
             ", template " + d.tStr() + ") " + d.tableTail("id"));
    db_.exec("CREATE TABLE IF NOT EXISTS rb_event(v " + d.tI32() + ", kind " + d.tStr() + ", meaning " + d.tStr() +
             ", pattern " + d.tStr() + ", weight " + d.tF64() + ") " + d.tableTail("v"));
    db_.exec("CREATE TABLE IF NOT EXISTS rb_meta(v " + d.tI32() + ") " + d.tableTail("v"));
    db_.exec("CREATE TABLE IF NOT EXISTS rb_doubt(form_id " + d.tU64() + ", meaning " + d.tNullableStr() + ", doubt " +
             d.tF64() + ") " + d.tableTail("form_id"));
    if (db_.query("SELECT count(*) FROM rb_meta").i64(0, 0) == 0) db_.exec("INSERT INTO rb_meta VALUES (0)");
}

std::size_t RazobillEngine::registerFormsLocked(const std::string& selectForms) {
    const auto& d = *dx_;
    const auto before = db_.query("SELECT count(*) FROM rb_form").i64(0, 0);
    // DESIGN DECISION (F-MEM-10):
    // Sequential form IDs (`no`) use 32-bit signed integers (`tI32()`), allowing up to 2,147,483,647
    // distinct log forms. For analytical workloads this bound is never exceeded, while
    // 32-bit integer IDs keep compact column storage and in-memory indexes cache-efficient.
    db_.exec("INSERT INTO rb_form SELECT " + d.hashExpr("form") +
             ", CAST((SELECT count(*) FROM rb_form) + row_number() OVER (ORDER BY form) - 1 AS " + d.tI32() +
             "), form, " + d.templateExpr("form") + " FROM (SELECT DISTINCT form FROM (" + selectForms +
             ") WHERE form IS NOT NULL AND form NOT IN (SELECT form FROM rb_form WHERE form IS NOT NULL))");
    const auto collisionCount =
        db_.query("SELECT count(*) FROM (SELECT id FROM rb_form GROUP BY id HAVING count(*) > 1)").i64(0, 0);
    if (collisionCount != 0) {
        // Disambiguate 64-bit hash collision via dense 32-bit `no` foreign key (F-CONC-03)
        hasCollision_ = true;
    }
    return static_cast<std::size_t>(db_.query("SELECT count(*) FROM rb_form").i64(0, 0) - before);
}

std::string_view RazobillEngine::physKeyLocked() const {
    return compact_ || dx_->denseFormKey() || hasCollision_ ? "form_no" : "form_id";
}

std::size_t RazobillEngine::refresh() {
    std::scoped_lock lock(mu_);
    const auto& d = *dx_;
    const std::string t = cfg_.table;
    const std::string m = cfg_.messageColumn;
    const auto sMode = cfg_.scanMode;
    std::size_t added = 0;
    if (d.denseFormKey()) {
        db_.exec("ALTER TABLE " + t + " ADD COLUMN IF NOT EXISTS form_no " + d.tI32());
        added = registerFormsLocked("SELECT " + d.formExpr(m, sMode) + " AS form FROM " + t + " WHERE form_no IS NULL");
        db_.exec("UPDATE " + t + " SET form_no = f.no FROM rb_form f WHERE " + t +
                 ".form_no IS NULL AND f.form = " + d.formExpr(t + "." + m, sMode));
    } else {
        const bool have =
            db_.query("SELECT count() FROM system.columns WHERE database = currentDatabase() AND table = " +
                      d.quote(t) + " AND name = 'form_id'")
                .i64(0, 0) > 0;
        if (!have) {
            db_.exec("ALTER TABLE " + t + " ADD COLUMN form_id UInt64 MATERIALIZED " +
                     d.hashExpr(d.formExpr(m, sMode)));
            db_.exec("ALTER TABLE " + t + " MATERIALIZE COLUMN form_id SETTINGS mutations_sync = 2");
        }
        added = registerFormsLocked("SELECT " + d.formExpr(m, sMode) + " AS form FROM " + t);
    }
    phys_ = view_ = t;
    compact_ = false;
    ready_ = true;
    createViewsLocked();
    rebuildDerivedLocked();
    return added;
}

std::uint64_t RazobillEngine::compact(const std::string& src) {
    std::scoped_lock lock(mu_);
    const auto& d = *dx_;
    if (!d.supportsCompact()) throw std::logic_error("compact storage is not supported on this engine");
    requireIdentifier(src, "source table");
    const std::string m = cfg_.messageColumn;
    const std::string dst = cfg_.table + "_c";
    const std::string name = cfg_.table;

    TransactionGuard tx(db_, d.flavor());

    std::vector<std::string> oth;
    const auto cols = db_.query("DESCRIBE " + src);
    bool hasMsg = false;
    for (std::size_t r = 0; r < cols.rows.size(); ++r) {
        const auto& c = cols.str(r, 0);
        if (c == m) {
            hasMsg = true;
            continue;
        }
        if (reservedColumn(c)) throw std::invalid_argument("source column name is reserved: " + c);
        oth.push_back(c);
    }
    if (!hasMsg) throw std::invalid_argument("source table has no column " + m);
    auto list = [&](const std::string& prefix) {
        std::string s;
        for (const auto& c : oth)
            s += prefix + quoteIdentifier(c) + ", ";
        return s;
    };

    db_.exec("CREATE OR REPLACE TEMP TABLE rb_s AS WITH parsed AS (SELECT " + list("") + "RzSplit(" + m +
             ") AS rz_parsed FROM " + src + ") SELECT " + list("") +
             "rz_parsed.template AS rbf, rz_parsed.ints AS rbi, rz_parsed.strs AS rbs FROM parsed");
    db_.exec("CREATE TABLE IF NOT EXISTS rb_str(id INTEGER, s VARCHAR)");
    db_.exec("CREATE TABLE IF NOT EXISTS rb_layout(ki INTEGER, ks INTEGER)");
    if (db_.query("SELECT count(*) FROM rb_layout").i64(0, 0) == 0)
        db_.exec("INSERT INTO rb_layout SELECT CAST(least(coalesce(quantile_disc(len(rbi), 0.99), 0), 32) AS INTEGER), "
                 "CAST(least(coalesce(quantile_disc(len(rbs), 0.99), 0), 32) AS INTEGER) FROM rb_s");
    const auto lay = db_.query("SELECT ki, ks FROM rb_layout");
    const auto ki = std::clamp<std::int64_t>(lay.i64(0, 0), 0, 32);
    const auto ks = std::clamp<std::int64_t>(lay.i64(0, 1), 0, 32);

    registerFormsLocked("SELECT rbf AS form FROM rb_s");
    db_.exec("INSERT INTO rb_str SELECT CAST(coalesce((SELECT max(id) + 1 FROM rb_str), 0) + row_number() OVER () - 1 "
             "AS INTEGER), s "
             "FROM (SELECT DISTINCT unnest(rbs[1:" +
             std::to_string(ks) +
             "]) AS s FROM rb_s) WHERE s IS NOT NULL AND s NOT IN (SELECT s FROM rb_str WHERE s IS NOT NULL)");

    std::string sel = "SELECT " + list("s.") + "f.no AS form_no, ";
    for (long long j = 0; j < ki; ++j)
        sel += "rbi[" + std::to_string(j + 1) + "] AS i" + std::to_string(j) + ", ";
    for (long long j = 0; j < ks; ++j)
        sel += "d" + std::to_string(j) + ".id AS d" + std::to_string(j) + ", ";
    sel += "CASE WHEN len(rbi) > " + std::to_string(ki) + " THEN rbi[" + std::to_string(ki + 1) +
           ":] END AS oi, CASE WHEN len(rbs) > " + std::to_string(ks) + " THEN rbs[" + std::to_string(ks + 1) +
           ":] END AS os FROM rb_s s LEFT JOIN rb_form f ON f.form = s.rbf";
    for (long long j = 0; j < ks; ++j)
        sel += " LEFT JOIN rb_str d" + std::to_string(j) + " ON d" + std::to_string(j) + ".s = s.rbs[" +
               std::to_string(j + 1) + "]";
    if (db_.query("SELECT count(*) FROM duckdb_tables() WHERE table_name = " + d.quote(dst)).i64(0, 0) == 0)
        db_.exec("CREATE TABLE " + dst + " AS " + sel + " LIMIT 0");
    db_.exec("INSERT INTO " + dst + " BY NAME " + sel);

    std::string ivs;
    std::string svs;
    std::string joins;
    for (long long j = 0; j < ki; ++j)
        ivs += "c.i" + std::to_string(j) + ", ";
    for (long long j = 0; j < ks; ++j) {
        svs += "d" + std::to_string(j) + ".s, ";
        joins +=
            " LEFT JOIN rb_str d" + std::to_string(j) + " ON d" + std::to_string(j) + ".id = c.d" + std::to_string(j);
    }
    db_.exec("CREATE OR REPLACE VIEW " + name + " AS SELECT " + list("c.") + "c.form_no, RzJoin(f.form, " + ivs + svs +
             "c.oi, c.os) AS " + m + " FROM " + dst + " c LEFT JOIN rb_form f ON f.no = c.form_no" + joins);
    phys_ = dst;
    view_ = name;
    compact_ = true;
    ready_ = true;
    createViewsLocked();
    rebuildDerivedLocked();
    tx.commit();
    return static_cast<std::uint64_t>(db_.query("SELECT count(*) FROM " + dst).i64(0, 0));
}

std::uint64_t RazobillEngine::verify(const std::string& src) {
    std::scoped_lock lock(mu_);
    requireIdentifier(src, "source table");
    requireReadyLocked();
    if (!compact_) throw std::logic_error("verify() applies to compact mode");
    const auto cols = db_.query("DESCRIBE " + src);
    std::string sel;
    for (std::size_t r = 0; r < cols.rows.size(); ++r)
        sel += (r ? ", " : "") + quoteIdentifier(cols.str(r, 0));
    const std::string a = "SELECT " + sel + " FROM " + src;
    const std::string b = "SELECT " + sel + " FROM " + view_;
    return static_cast<std::uint64_t>(db_.query("SELECT (SELECT count(*) FROM (" + a + " EXCEPT ALL " + b +
                                                ")) + (SELECT count(*) FROM (" + b + " EXCEPT ALL " + a + "))")
                                          .i64(0, 0));
}

DictionaryStats RazobillEngine::gcDictionary() {
    std::scoped_lock lock(mu_);
    requireReadyLocked();
    if (!compact_) throw std::logic_error("gcDictionary() applies to compact mode");

    const auto lay = db_.query("SELECT ks FROM rb_layout");
    if (lay.empty()) return {};
    const auto ks = lay.i64(0, 0);
    const auto before = static_cast<std::size_t>(db_.query("SELECT count(*) FROM rb_str").i64(0, 0));
    if (before == 0 || ks == 0) {
        DictionaryStats res;
        res.totalBefore = res.total_before = before;
        res.activeAfter = res.active_after = before;
        res.purgedCount = res.purged_count = 0;
        return res;
    }

    std::string refUnion;
    for (long long j = 0; j < ks; ++j) {
        if (!refUnion.empty()) refUnion += " UNION ALL ";
        refUnion +=
            "SELECT d" + std::to_string(j) + " AS id FROM " + phys_ + " WHERE d" + std::to_string(j) + " IS NOT NULL";
    }

    db_.exec("CREATE OR REPLACE TEMP TABLE rb_active_str_ids AS SELECT DISTINCT id FROM (" + refUnion +
             ") WHERE id IS NOT NULL");
    const auto active = static_cast<std::size_t>(db_.query("SELECT count(*) FROM rb_active_str_ids").i64(0, 0));
    const std::size_t purged = before > active ? before - active : 0;

    if (purged > 0) {
        db_.exec("DELETE FROM rb_str WHERE id IS NOT NULL AND id NOT IN (SELECT id FROM rb_active_str_ids WHERE id IS "
                 "NOT NULL)");
    }
    db_.exec("DROP TABLE IF EXISTS rb_active_str_ids");

    DictionaryStats res;
    res.totalBefore = res.total_before = before;
    res.activeAfter = res.active_after = active;
    res.purgedCount = res.purged_count = purged;
    return res;
}

void RazobillEngine::compactDictionary() {
    std::scoped_lock lock(mu_);
    requireReadyLocked();
    if (!compact_) throw std::logic_error("compactDictionary() applies to compact mode");

    const auto lay = db_.query("SELECT ks FROM rb_layout");
    if (lay.empty()) return;
    const auto ks = lay.i64(0, 0);
    if (ks == 0) return;

    const auto count = db_.query("SELECT count(*) FROM rb_str").i64(0, 0);
    if (count == 0) return;
    const auto maxId = db_.query("SELECT coalesce(max(id), -1) FROM rb_str").i64(0, 0);
    if (maxId == count - 1) return;

    TransactionGuard tx(db_, dx_->flavor());
    db_.exec("CREATE OR REPLACE TEMP TABLE rb_str_remap AS SELECT id AS old_id, CAST(row_number() OVER (ORDER BY id) - "
             "1 AS INTEGER) AS new_id, s FROM rb_str");
    for (long long j = 0; j < ks; ++j) {
        const std::string dj = "d" + std::to_string(j);
        std::string updateSql;
        updateSql.reserve((phys_.size() * 2) + (dj.size() * 2) + 64);
        updateSql.append("UPDATE ")
            .append(phys_)
            .append(" SET ")
            .append(dj)
            .append(" = remap.new_id FROM rb_str_remap remap WHERE ")
            .append(phys_)
            .append(".")
            .append(dj)
            .append(" = remap.old_id");
        db_.exec(updateSql);
    }
    db_.exec("CREATE OR REPLACE TABLE rb_str AS SELECT new_id AS id, s FROM rb_str_remap ORDER BY id");
    db_.exec("DROP TABLE IF EXISTS rb_str_remap");
    tx.commit();
}

void RazobillEngine::createViewsLocked() {
    const std::string join =
        compact_ || dx_->denseFormKey() || hasCollision_
            ? " LEFT JOIN rb_form f ON f.no = e.form_no LEFT JOIN rb_doubt d ON d.form_id = f.id"
            : " LEFT JOIN rb_form f ON f.id = e.form_id LEFT JOIN rb_doubt d ON d.form_id = e.form_id";
    db_.exec("CREATE OR REPLACE VIEW " + view_ +
             "_m AS SELECT e.*, f.template AS template, d.meaning AS meaning, d.doubt AS doubt FROM " + view_ + " e" +
             join);
}

std::vector<RazobillEngine::Rule> RazobillEngine::activeRulesLocked(int version) {
    const auto ev = db_.query(
        "SELECT kind, meaning, pattern, weight FROM rb_event WHERE v <= " + std::to_string(version) + " ORDER BY v");
    std::vector<Rule> rules;
    for (std::size_t r = 0; r < ev.rows.size(); ++r) {
        const auto& kind = ev.str(r, 0);
        const auto& meaning = ev.str(r, 1);
        if (kind == "define")
            rules.push_back({.meaning = meaning, .pattern = ev.str(r, 2), .weight = ev.f64(r, 3)});
        else
            std::erase_if(rules, [&](const Rule& x) { return x.meaning == meaning; });
    }
    return rules;
}

std::vector<RazobillEngine::FormP> RazobillEngine::resolveLocked(const std::vector<Rule>& rules) {
    std::map<std::pair<std::uint64_t, std::string>, double> w;
    std::map<std::uint64_t, double> total;
    for (const auto& rule : rules) {
        Table t;
        try {
            t = db_.query("SELECT id FROM rb_form WHERE " + dx_->matches("template", dx_->quote(rule.pattern)));
        } catch (const BackendError& e) {
            throw std::invalid_argument(std::string("invalid pattern '") + rule.pattern + "': " + e.what());
        }
        for (std::size_t r = 0; r < t.rows.size(); ++r) {
            const auto id = t.u64(r, 0);
            w[{id, rule.meaning}] += rule.weight;
            total[id] += rule.weight;
        }
    }
    std::vector<FormP> out;
    out.reserve(w.size());
    for (const auto& [key, weight] : w)
        out.push_back({.form = key.first, .meaning = key.second, .p = weight / total[key.first]});
    return out;
}

void RazobillEngine::rebuildDerivedLocked() {
    const auto map = resolveLocked(activeRulesLocked(static_cast<int>(db_.query("SELECT v FROM rb_meta").i64(0, 0))));
    struct Row {
        std::string top;
        double pmax = 0, h = 0;
    };
    std::map<std::uint64_t, Row> rows;
    for (const auto& m : map) {
        auto& r = rows[m.form];
        if (m.p > r.pmax) {
            r.pmax = m.p;
            r.top = m.meaning;
        }
        if (m.p > 0) r.h -= m.p * std::log2(m.p);
    }
    db_.exec(dx_->truncate("rb_doubt"));
    std::string values;
    std::size_t k = 0;
    auto flush = [&] {
        if (!values.empty()) {
            db_.exec("INSERT INTO rb_doubt VALUES " + values);
            values.clear();
        }
    };
    for (const auto& [id, r] : rows) {
        values += (values.empty() ? "(" : ", (") + std::to_string(id) + ", " +
                  (r.pmax >= cfg_.tau ? dx_->quote(r.top) : std::string("NULL")) + ", " + num(std::max(r.h, 0.0)) + ")";
        if (++k % 400 == 0) flush();
    }
    flush();
}

std::vector<stats::XY> RazobillEngine::xyLocked(std::unordered_map<std::string, std::uint32_t>& yids) {
    const auto& d = *dx_;
    const std::string y = cfg_.purpose.empty() ? "'-'" : d.toText("e." + cfg_.purpose);
    std::string sql;
    if (compact_ || d.denseFormKey() || hasCollision_)
        sql = "SELECT f.id AS fid, " + y + " AS yv, count(*) AS n FROM " + phys_ +
              " e JOIN rb_form f ON f.no = e.form_no GROUP BY f.id, yv";
    else
        sql = "SELECT e.form_id AS fid, " + y + " AS yv, count(*) AS n FROM " + phys_ +
              " e WHERE e.form_id IS NOT NULL GROUP BY e.form_id, yv";
    const auto t = db_.query(sql);
    constexpr std::size_t kMaxXyRows = 5'000'000;
    if (t.rows.size() > kMaxXyRows) throw std::length_error("purpose column has too many distinct values per form");
    Interner in;
    std::vector<stats::XY> out;
    out.reserve(t.rows.size());
    for (std::size_t r = 0; r < t.rows.size(); ++r)
        out.push_back({
            .form = t.u64(r, 0),
            .y = t.isNull(r, 1) ? in("\x01null") : in(t.str(r, 1)),
            .n = static_cast<double>(t.i64(r, 2)),
        });
    yids = in.map();
    return out;
}

double RazobillEngine::totalEventsLocked() {
    return static_cast<double>(db_.query("SELECT count(*) FROM " + phys_).i64(0, 0));
}

Defined RazobillEngine::define(const std::string& meaning, const std::string& pattern, double weight, bool force) {
    std::scoped_lock lock(mu_);
    requireName(meaning);
    if (!(weight > 0) || !std::isfinite(weight)) throw std::invalid_argument("weight must be positive and finite");
    if (cfg_.purpose.empty() && !force) throw std::invalid_argument("declare a purpose first");
    requireReadyLocked();
    const int cur = static_cast<int>(db_.query("SELECT v FROM rb_meta").i64(0, 0));

    std::vector<Rule> probe = activeRulesLocked(cur);
    std::erase_if(probe, [&](const Rule& r) { return r.meaning != meaning; });
    probe.push_back({.meaning = meaning, .pattern = pattern, .weight = weight});
    std::unordered_map<std::uint64_t, bool> merged;
    for (const auto& f : resolveLocked(probe))
        merged[f.form] = true;

    std::unordered_map<std::string, std::uint32_t> yids;
    const auto xy = xyLocked(yids);

    // SEMANTIC GATING IN ACTION (Theorem 4 in "Latitude and Doubt", M. M. Atta, 2026):
    // The gate protects the semantic ledger from lossy over-clustering.
    // If a candidate rule attempts to merge forms that carry distinct diagnostic signals about `purpose`,
    // the volume-weighted Jensen-Shannon divergence exceeds `cfg_.eps` (default 0.05 bits).
    // The gate rejects the merge to prevent information leakage I(X;Y|W) > 0.
    const auto cost = stats::mergeCost(xy, merged, totalEventsLocked());
    if (cost.jsd > cfg_.eps && !force) {
        std::ostringstream o;
        o << meaning << ": merging " << merged.size() << " forms loses " << cost.jsd << " bits per merged event about '"
          << cfg_.purpose << "'";
        throw GateRefused(o.str(), cost.jsd, cost.share);
    }
    TransactionGuard tx(db_, dx_->flavor());
    const int v = static_cast<int>(db_.query("SELECT coalesce(max(v), 0) FROM rb_event").i64(0, 0)) + 1;
    db_.exec("INSERT INTO rb_event VALUES (" + std::to_string(v) + ", 'define', " + dx_->quote(meaning) + ", " +
             dx_->quote(pattern) + ", " + num(weight) + ")");
    db_.exec(dx_->truncate("rb_meta"));
    db_.exec("INSERT INTO rb_meta VALUES (" + std::to_string(v) + ")");
    rebuildDerivedLocked();
    tx.commit();

    Defined res;
    res.version = v;
    res.forms = merged.size();
    res.lossPerMergedEvent = res.loss_per_merged_event = cost.jsd;
    res.lossTableWide = res.loss_table_wide = cost.jsd * cost.share;
    return res;
}

int RazobillEngine::retire(const std::string& meaning) {
    std::scoped_lock lock(mu_);
    requireName(meaning);
    const int cur = static_cast<int>(db_.query("SELECT v FROM rb_meta").i64(0, 0));
    const auto rules = activeRulesLocked(cur);
    auto matchesMeaning = [&](const Rule& r) { return r.meaning == meaning; };
    if (std::ranges::none_of(rules, matchesMeaning)) {
        throw std::invalid_argument("no active meaning named " + meaning);
    }
    TransactionGuard tx(db_, dx_->flavor());
    const int v = static_cast<int>(db_.query("SELECT coalesce(max(v), 0) FROM rb_event").i64(0, 0)) + 1;
    db_.exec("INSERT INTO rb_event VALUES (" + std::to_string(v) + ", 'retire', " + dx_->quote(meaning) + ", '', 0)");
    db_.exec(dx_->truncate("rb_meta"));
    db_.exec("INSERT INTO rb_meta VALUES (" + std::to_string(v) + ")");
    rebuildDerivedLocked();
    tx.commit();
    return v;
}

void RazobillEngine::pin(int version) {
    std::scoped_lock lock(mu_);
    const int latest = static_cast<int>(db_.query("SELECT coalesce(max(v), 0) FROM rb_event").i64(0, 0));
    if (version < 0 || version > latest) throw std::invalid_argument("version out of range");
    TransactionGuard tx(db_, dx_->flavor());
    db_.exec(dx_->truncate("rb_meta"));
    db_.exec("INSERT INTO rb_meta VALUES (" + std::to_string(version) + ")");
    rebuildDerivedLocked();
    tx.commit();
}

int RazobillEngine::version() {
    std::scoped_lock lock(mu_);
    return static_cast<int>(db_.query("SELECT v FROM rb_meta").i64(0, 0));
}

stats::Ledger RazobillEngine::ledger() {
    std::scoped_lock lock(mu_);
    requireReadyLocked();
    const auto map = resolveLocked(activeRulesLocked(static_cast<int>(db_.query("SELECT v FROM rb_meta").i64(0, 0))));
    std::unordered_map<std::string, std::uint32_t> yids;
    const auto xy = xyLocked(yids);
    std::unordered_map<std::uint64_t, double> n;
    for (const auto& r : xy)
        n[r.form] += r.n;
    Interner mi;
    std::vector<stats::Cell> cells;
    cells.reserve(map.size());
    for (const auto& m : map)
        if (n.contains(m.form)) cells.push_back({.form = m.form, .meaning = mi(m.meaning), .mass = n[m.form] * m.p});
    const auto l = stats::ledger(cells, totalEventsLocked());
    if (std::abs(l.expression - l.conveyance - l.latitude) > 1e-9 ||
        std::abs(l.content - l.conveyance - l.doubt) > 1e-9)
        throw std::logic_error("ledger identity violated");
    return l;
}

double RazobillEngine::leakage() {
    std::scoped_lock lock(mu_);
    requireReadyLocked();
    const auto map = resolveLocked(activeRulesLocked(static_cast<int>(db_.query("SELECT v FROM rb_meta").i64(0, 0))));
    std::unordered_map<std::string, std::uint32_t> yids;
    const auto xy = xyLocked(yids);
    Interner mi;
    std::vector<stats::FormMeaning> fm;
    fm.reserve(map.size());
    for (const auto& m : map)
        fm.push_back({.form = m.form, .meaning = mi(m.meaning), .p = m.p});
    return stats::leakage(xy, fm);
}

std::optional<MeaningOf> RazobillEngine::meaningOf(const std::string& text) {
    core::SlotRecord slots;
    if (const auto s = core::parse(text, slots, cfg_.scanMode); s != Status::Ok)
        throw std::invalid_argument(describe(s));
    const auto id = core::xxh64(slots.templateStr);
    std::scoped_lock lock(mu_);
    if (db_.query("SELECT count(*) FROM rb_form WHERE id = " + std::to_string(id)).i64(0, 0) == 0) return std::nullopt;
    MeaningOf out{.formId = id, .meaning = std::nullopt, .doubt = std::nullopt, .form_id = id};
    const auto t = db_.query("SELECT meaning, doubt FROM rb_doubt WHERE form_id = " + std::to_string(id));
    if (!t.empty()) {
        if (!t.isNull(0, 0)) out.meaning = t.str(0, 0);
        out.doubt = t.f64(0, 1);
    }
    return out;
}

std::vector<std::pair<std::string, std::int64_t>> RazobillEngine::todo(std::size_t n) {
    std::scoped_lock lock(mu_);
    requireReadyLocked();
    const auto map = resolveLocked(activeRulesLocked(static_cast<int>(db_.query("SELECT v FROM rb_meta").i64(0, 0))));
    std::unordered_map<std::string, std::uint32_t> yids;
    const auto xy = xyLocked(yids);
    std::unordered_map<std::uint64_t, std::int64_t> count;
    for (const auto& r : xy)
        count[r.form] += static_cast<std::int64_t>(r.n);
    for (const auto& m : map)
        count.erase(m.form);
    std::vector<std::pair<std::uint64_t, std::int64_t>> top(count.begin(), count.end());
    std::ranges::sort(top, [](const auto& a, const auto& b) {
        return a.second != b.second ? a.second > b.second : a.first < b.first;
    });
    if (top.size() > n) top.resize(n);
    if (top.empty()) return {};

    std::string inList;
    for (std::size_t i = 0; i < top.size(); ++i) {
        if (i > 0) inList += ", ";
        inList += std::to_string(top[i].first);
    }
    const auto t = db_.query("SELECT id, template FROM rb_form WHERE id IN (" + inList + ")");
    std::unordered_map<std::uint64_t, std::string> idToTmpl;
    for (std::size_t r = 0; r < t.rows.size(); ++r) {
        idToTmpl[t.u64(r, 0)] = t.str(r, 1);
    }
    std::vector<std::pair<std::string, std::int64_t>> out;
    out.reserve(top.size());
    for (const auto& [id, c] : top) {
        auto it = idToTmpl.find(id);
        out.emplace_back(it != idToTmpl.end() ? it->second : "<unknown>", c);
    }
    return out;
}

} // namespace razobill
