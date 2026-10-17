# Razobill Performance Audit

---

## P1. `std::regex` in Hot Path — `reservedColumn()`

**File:** [Razobill.cpp:83](file:///C:/FM/rzd/razobill/src/Razobill.cpp#L83)  
**Severity:** 🟡 MEDIUM  

```cpp
static const std::regex kReserved("^(rb_|rbf|rbi|rbs|oi|os|form|form_no|d[0-9]+|i[0-9]+)$");
```

**Problem:** `std::regex` is 10–100× slower than simple string operations (benchmarked by Hinnant, 2016; also documented in DuckDB and ClickHouse source comments). This is called per column name during `compact()`.

**Impact:** On tables with 50+ columns, adds measurable overhead to every compaction pass.

**Fix:** Replace with a prefix/exact-match check:

```cpp
bool reservedColumn(const std::string& c) {
    if (c.starts_with("rb_")) return true;
    if (c == "rbf" || c == "rbi" || c == "rbs" || c == "oi" || c == "os"
        || c == "form" || c == "form_no") return true;
    if ((c[0] == 'd' || c[0] == 'i') && c.size() > 1) {
        return std::all_of(c.begin() + 1, c.end(), ::isdigit);
    }
    return false;
}
```

---

## P2. Batch INSERT Without Transaction Wrapping

**File:** [Razobill.cpp — `rebuildDerivedLocked()`](file:///C:/FM/rzd/razobill/src/Razobill.cpp)  
**Severity:** 🟡 MEDIUM  

**Problem:** Batches every 400 rows into separate `INSERT` statements without wrapping them in `BEGIN`/`COMMIT`. Each statement incurs individual transaction overhead (WAL flush, lock acquisition).

**Impact:** On a 100K-row rebuild, this creates ~250 individual transactions instead of 1.

**Fix:** Wrap the batch loop in a single transaction:

```cpp
db_.exec("BEGIN TRANSACTION");
// ... batch INSERTs ...
db_.exec("COMMIT");
```

---

## P3. `xyLocked()` — Millions of Heap Allocations

**File:** [Razobill.cpp — `xyLocked()`](file:///C:/FM/rzd/razobill/src/Razobill.cpp)  
**Severity:** 🟡 MEDIUM  

**Problem:** Loads up to 5M rows into `std::vector<std::vector<Value>>`. Each `Value` is a `std::variant` containing `std::string` (28 bytes on MSVC). Each row creates a new `vector<Value>` allocation. Each string cell creates a `std::string` heap allocation.

For 5M rows × 4 columns: ~20M `Value` objects, ~5M `vector` allocations, ~10M `string` allocations.

**Impact:** GC pressure, heap fragmentation, poor cache locality. Dominates memory usage during `ledger()` computation.

**Fix (minimal):** Reserve row capacity upfront:

```cpp
t.rows.reserve(expectedRowCount);
```

**Fix (ideal):** Columnar result storage — store results as `vector<int64_t>`, `vector<string>` per column instead of row-of-variants. But this changes the `Table` interface (larger scope).

---

## P4. `num()` Uses `std::ostringstream`

**File:** [Razobill.cpp](file:///C:/FM/rzd/razobill/src/Razobill.cpp)  
**Severity:** 🔵 LOW  

**Problem:** `std::ostringstream` allocates a heap buffer per call. Used for double-to-string formatting.

**Fix:** Use `std::to_chars` (C++17) or `snprintf` with a stack buffer:

```cpp
std::array<char, 32> buf;
auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
return std::string(buf.data(), ptr);
```

---

## P5. Channel `exactFailureProbability()` — Hash Map Convolution

**File:** [Channel.cpp:107–129](file:///C:/FM/rzd/razobill/src/Channel.cpp#L107-L129)  
**Severity:** 🟡 MEDIUM  

```cpp
std::unordered_map<std::int64_t, double> dist{{0, 1.0}};
// ... convolution loop creating new maps per step
```

**Problem:** Allocates a new `unordered_map` per sequence step. Each map insertion involves hashing, bucket lookup, and potential rehash. For long sequences (n=100, M=50 options each), the map can grow to thousands of entries.

**Impact:** Heap churn dominates the exact failure probability computation.

**Fix:** Use two flat `std::vector` ping-pong buffers indexed by bin offset. Pre-compute the bin range from the sequence to allocate once.

---

## P6. Triple SQL Evaluation in `compact()`

**File:** [Razobill.cpp:232–233](file:///C:/FM/rzd/razobill/src/Razobill.cpp#L232-L233)  
**Severity:** 🔴 HIGH (overlaps with B4)  

Already documented in [02-CRITICAL-BUGS.md](file:///C:/FM/rzd/razobill/AUDIT/02-CRITICAL-BUGS.md#b4-triple-rzsplit-evaluation). The `RzSplit()` UDF is the most expensive per-row operation, and it runs 3× per row.

**Impact:** On a 10M-row table, this wastes ~66% of the total CPU time for the compaction step.

---

## P7. `ClickHouseDialect::quote()` — No String Reservation

**File:** [Dialect.cpp:60–66](file:///C:/FM/rzd/razobill/src/Dialect.cpp#L60-L66)  
**Severity:** 🔵 LOW  

```cpp
std::string o = "'";  // no reserve()
for (char c : s) {
    if (c == '\\' || c == '\'') o.push_back('\\');
    o.push_back(c);
}
```

**Problem:** Multiple heap reallocations when quoting long SQL strings. `DuckDialect::quote()` correctly calls `o.reserve(s.size() + 16)` but `ClickHouseDialect::quote()` does not.

**Fix:** Add `o.reserve(s.size() + 16)` after initialization.

---

## Performance Priority Matrix

| ID | Severity | Expected Speedup | Fix Complexity |
|----|----------|-------------------|----------------|
| P6/B4 | 🔴 HIGH | 2–3× on compact | Low (CTE) |
| P3 | 🟡 MED | ~30% on ledger | Low (reserve) |
| P1 | 🟡 MED | 10–100× on column check | Low (string ops) |
| P2 | 🟡 MED | ~5× on rebuild | Low (transaction wrap) |
| P5 | 🟡 MED | ~3× on exact failure | Med (vector buffer) |
| P4 | 🔵 LOW | Marginal | Low (to_chars) |
| P7 | 🔵 LOW | Marginal | Trivial (reserve) |
