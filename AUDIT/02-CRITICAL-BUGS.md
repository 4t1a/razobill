# Razobill Critical Bugs

> [!CAUTION]
> These 7 bugs produce wrong results, crash under load, or generate dead/unreachable code.
> Every one must be fixed before production deployment.

---

## B1. Tautological Dead Code

**File:** [Razobill.cpp:101](file:///C:/FM/rzd/razobill/src/Razobill.cpp#L101)  
**Severity:** 🔴 CRITICAL  

```cpp
if (cfg_.table.empty() && !cfg_.table.empty()) cfg_.table = cfg_.table;
```

**Problem:** `x.empty() && !x.empty()` is always `false`. The entire statement is dead code. The self-assignment `cfg_.table = cfg_.table` is also a no-op.

**Likely intent:** Backward-compat migration from a legacy `table` field, similar to line 102–105 pattern for `messageColumn` / `message_column`.

**Fix:** Remove the line entirely, or implement the intended backward-compat logic if a legacy field exists.

---

## B2. Config Precedence Ambiguity

**File:** [Razobill.cpp:102–105](file:///C:/FM/rzd/razobill/src/Razobill.cpp#L102-L105)  
**Severity:** 🔴 CRITICAL  

```cpp
const std::string msgCol = !cfg_.messageColumn.empty() && cfg_.messageColumn != "msg"
                               ? cfg_.messageColumn
                               : (!cfg_.message_column.empty() ? cfg_.message_column : cfg_.messageColumn);
cfg_.messageColumn = msgCol;
```

**Problem:** If user explicitly sets `messageColumn = "msg"`, the condition `cfg_.messageColumn != "msg"` is `false`, so it falls through to the legacy `message_column` field. A legacy field silently overrides an explicit modern setting.

**Fix:** Simplify to: prefer `messageColumn` if non-empty, else fall back to `message_column`, else default to `"msg"`.

```cpp
if (cfg_.messageColumn.empty() && !cfg_.message_column.empty())
    cfg_.messageColumn = cfg_.message_column;
if (cfg_.messageColumn.empty())
    cfg_.messageColumn = "msg";
```

---

## B3. N+1 SQL Query Anti-Pattern

**File:** [Razobill.cpp:615–616](file:///C:/FM/rzd/razobill/src/Razobill.cpp#L615-L616)  
**Severity:** 🔴 CRITICAL  

```cpp
for (const auto& [id, c] : top)
    out.emplace_back(db_.query("SELECT template FROM rb_form WHERE id = " + std::to_string(id)).str(0, 0), c);
```

**Problem:**  
1. Executes one SQL query per form ID in a loop. With 1000 unresolved forms, that's 1000 round-trips.
2. If the query returns zero rows, `.str(0, 0)` calls `rows.at(0)` which throws `std::out_of_range` — **unhandled crash**.

**Fix:** Batch into a single `SELECT template, id FROM rb_form WHERE id IN (...)` query. Handle empty results.

---

## B4. Triple RzSplit Evaluation

**File:** [Razobill.cpp:232–233](file:///C:/FM/rzd/razobill/src/Razobill.cpp#L232-L233)  
**Severity:** 🔴 CRITICAL (performance correctness)

```sql
SELECT (RzSplit(msg)).template AS rbf,
       (RzSplit(msg)).ints AS rbi,
       (RzSplit(msg)).strs AS rbs
FROM source
```

**Problem:** `RzSplit(msg)` is a scalar function invoked 3 times per row. DuckDB may or may not CSE this. ClickHouse will not. On a 10M-row table, this triples the CPU cost of the most expensive operation.

**Fix:** Use a CTE or lateral join:

```sql
WITH parsed AS (SELECT *, RzSplit(msg) AS r FROM source)
SELECT r.template AS rbf, r.ints AS rbi, r.strs AS rbs FROM parsed
```

---

## B5. SQL `NOT IN` with Potential NULLs

**File:** [Razobill.cpp:164, 247, 328](file:///C:/FM/rzd/razobill/src/Razobill.cpp#L164)  
**Severity:** 🔴 CRITICAL  

```sql
WHERE form NOT IN (SELECT form FROM rb_form)
```

**Problem:** In SQL, `x NOT IN (subquery)` returns `NULL` (not `TRUE`) if any row in the subquery has `form = NULL`. This causes the entire WHERE clause to filter out **all** rows — silently producing an empty result set.

**Fix:** Use `NOT EXISTS`:

```sql
WHERE NOT EXISTS (SELECT 1 FROM rb_form f WHERE f.form = t.form)
```

Or add `WHERE form IS NOT NULL` to the subquery.

---

## B6. ClickHouse Substring Underflow

**File:** [Dialect.cpp:90](file:///C:/FM/rzd/razobill/src/Dialect.cpp#L90)  
**Severity:** 🔴 CRITICAL  

```cpp
return "replaceAll(substring(" + s + ", 1, position(" + s + ", char(19)) - 1), char(18), '<*>')";
```

**Problem:** ClickHouse `position()` returns 0 if `char(19)` is not found. Then `position - 1 = -1`, and `substring(s, 1, -1)` either throws or returns garbage depending on the ClickHouse version.

**Fix:** Guard with `if(position(...) > 0, ..., s)` or use `substringUTF8` with a clamp.

---

## B7. FormCatalog Dangling Pointer on Rehash

**File:** [FormCatalog.hpp:112–113](file:///C:/FM/rzd/razobill/include/librazobill/FormCatalog.hpp#L112-L113)  
**Severity:** 🔴 CRITICAL  

```cpp
auto inserted = formsByKey_.emplace(entry.key, std::move(entry));
formsById_.push_back(&inserted.first->second);  // raw pointer into unordered_map
```

**Problem:** `std::unordered_map` invalidates **all** pointers, references, and iterators on rehash (when `size() > bucket_count() * max_load_factor()`). After a rehash, every pointer in `formsById_` is dangling. Subsequent `findFormById()` dereferences dangling pointers → **undefined behavior / crash**.

> [!WARNING]
> This is the most dangerous bug in the codebase. It is latent — it only triggers when the form count crosses a rehash threshold (typically at ~75% load factor). In production with thousands of templates, this WILL crash.

**Fix options:**
1. **Use `std::deque<FormEntry>` as owner** — `std::deque` never invalidates pointers on push_back. Store entries in a `deque`, store only iterators/indices in the map.
2. **Use `node_extract` / `std::unordered_map` with `reserve()`** — pre-reserve to prevent rehash. Fragile.
3. **Store indices instead of pointers** — `formsById_` stores `std::string` keys, look up via `formsByKey_` on access.

Recommended: Option 1 (deque as stable storage).
