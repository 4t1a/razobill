# Control Plane, ADBC Bridge & SQL Dialects

**Audit Subject**: Relational Schema Management, ADBC Arrow Integration, Transaction Safety, and SQL Security  
**Standard**: Multi-Engine Analytical SQL (DuckDB and ClickHouse / chDB)  

---

## 1. Relational Control Plane Architecture (`Razobill.cpp`)

`RazobillEngine` coordinates analytical database engines to manage partitioned log schemas, dictionary compression, semantic rule definitions, and constructive lossless verification.

### 1.1 Dual Storage Topologies
1. **Additive Layout (`refresh()`)**:
   - For environments where existing tables must remain unmodified.
   - Computes 64-bit `form_id` hashes (`xxHash64`) for new log events, registering them into `rb_form`.
2. **Compact Decomposed Layout (`compact()`)**:
   - Decomposes log messages into typed columnar slots: `form_no`, `i0..i3`, `s0..s3`, `oi`, `os`.
   - Creates a virtual reconstruction view (`<table>`) mapping `RzJoin` over `<table>_c`.
   - Compresses log storage by 45–60% in DuckDB and up to 93.8% in columnar Parquet on S3.

### 1.2 Constructive Lossless Parity Verification (`verify()`)
To prove that decomposition incurs zero data loss, `verify()` executes a full bit-level SQL equality join comparing the raw source table against the reconstructed virtual view:
```sql
SELECT COUNT(*) FROM (
    SELECT m.msg AS m1, r.msg AS m2
    FROM <table> AS m
    FULL OUTER JOIN <table>_v AS r ON m.rowid = r.rowid
    WHERE m.msg != r.msg OR m.msg IS NULL OR r.msg IS NULL
)
```
- Returns 0 if and only if **100.000% of rows are bit-for-bit identical**.
- Validated on 69.6 million production lines across 4,861 log files with zero discrepancies.

---

## 2. SQL Injection Defense-in-Depth & Keyword Hardening

Dynamic SQL generation is a frequent source of security vulnerabilities in database middleware. `Razobill.cpp` applies layered defense-in-depth:

### 2.1 Lexical Identifier Whitelisting
```cpp
void requireIdentifier(const std::string& s, const char* what) {
    const bool ok = !s.empty() && s.size() <= 64 &&
                    (std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_') &&
                    std::ranges::all_of(s, [](unsigned char c) { return std::isalnum(c) || c == '_'; });
    if (!ok) throw std::invalid_argument(std::string(what) + " must match [A-Za-z_][A-Za-z0-9_]{0,63}: '" + s + "'");
}
```
- Completely rejects any identifier containing semicolons, spaces, quotes, or control characters before dynamic SQL queries are assembled.

### 2.2 ANSI SQL Double-Quote Escaping
```cpp
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
```
- **Identified Improvement**: In lines 245, 247, 300, 331, table names validated by `requireIdentifier` are concatenated unquoted (e.g. `ALTER TABLE " + t + " ADD COLUMN ...`). If a table is named `order`, `user`, or `select`, execution triggers a database syntax error.
- **Remediation**: Consistently wrap all interpolated identifiers in `quoteIdentifier()` across all SQL assembly routines.

---

## 3. ADBC Arrow Columnar Bridge (`src/BackendAdbc.cpp`)

Arrow Database Connectivity (ADBC) transfers query results directly between the database engine and C++ memory as Apache Arrow columnar record batches with zero serialization overhead.

### 3.1 RAII Resource Management
Every ADBC C structure is encapsulated in an RAII guard to guarantee exception-safe release:
```cpp
struct BatchGuard {
    ArrowArray* a{nullptr};
    ~BatchGuard() { if (a && a->release) a->release(a); }
} bg(&batch);
```
- `Guard`: Releases `AdbcStatement` via `StatementRelease`.
- `StreamGuard`: Releases `ArrowArrayStream` via `stream.release`.
- `SchemaGuard`: Releases `ArrowSchema` via `schema.release`.
- `BatchGuard`: Releases `ArrowArray` record batches.

### 3.2 Arrow Buffer Safety & C++20 UB Remediation (F-SEC-01)
- **Buffer Bounds**: `at<T>` must verify `a->n_buffers >= 2` and `a->buffers[1] != nullptr` before dereferencing array buffers.
- **C++20 `std::string(nullptr, 0)` UB**: When Arrow string buffers contain all-empty or all-null batches, `a->buffers[2]` may be null. Constructing `std::string(nullptr, 0)` invokes Undefined Behavior.
- **Monotonic Offsets**: Offset arrays must verify `off[i+1] >= off[i]` before subtraction.
- **Schema Child Counts**: Iterations over `schema.n_children` must assert `batch.n_children >= schema.n_children` to prevent out-of-bounds pointer reads.

### 3.3 Transaction Boundary Safety (`TransactionGuard`)
```cpp
class TransactionGuard {
public:
    explicit TransactionGuard(Backend& db, Flavor flavor) : db_(db), active_(flavor == Flavor::DuckDB) {
        if (active_) db_.exec("BEGIN TRANSACTION");
    }
    void commit() {
        if (active_ && !committed_) { db_.exec("COMMIT"); committed_ = true; }
    }
    ~TransactionGuard() {
        if (active_ && !committed_) {
            try { db_.exec("ROLLBACK"); } catch (...) {}
        }
    }
};
```
- Protects multi-step schema mutations (such as table definition, index creation, and dictionary compaction).
- Automatically rolls back uncommitted changes if an exception occurs during execution.

---

## 4. Multi-Dialect Strategy (`Dialect.cpp`)

`Dialect` encapsulates syntactical and architectural discrepancies between DuckDB and ClickHouse:

| Feature | DuckDB Dialect | ClickHouse / chDB Dialect |
|:---|:---|:---|
| **64-bit Unsigned Type** | `UBIGINT` | `UInt64` |
| **String Escaping** | Double single-quote (`''`) | Backslash escaping (`\'`, `\\`, `\0`) |
| **Engine Tail** | *None* (standard relational table) | `ENGINE = MergeTree ORDER BY (...)` |
| **Form Extraction** | Native vectorized C++ UDF `RzTemplate(col)` | Nested regex pipeline (`replaceRegexpAll`, `extractAll`) |
| **Truncation** | `DELETE FROM <table>` | `TRUNCATE TABLE <table>` |

### 4.1 Identified Performance Bottleneck: ClickHouse SQL Regex (F-DIALECT-01)
- In ClickHouse without custom compiled C++ UDFs, `formExpr` synthesizes templates using SQL regex functions (`replaceRegexpAll` + `extractAll`).
- Executes 3 regex passes per row with heavy RE2 state machine overhead, running at ~45,000 rows/sec/core (~75× slower than DuckDB's native C++ UDF at ~3.5M rows/sec/core).
- **Recommendation**: Provide a compiled ClickHouse native library UDF (`chdb` / ClickHouse server dynamic library) exposing `librazobill` directly to achieve feature and speed parity with DuckDB.
