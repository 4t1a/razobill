# 06 — Backend, Dialect & Control Plane Audit

**Scope**: `Backend.hpp`, `BackendAdbc.cpp`, `Dialect.hpp`, `Dialect.cpp`, `Razobill.hpp`, `Razobill.cpp`

---

## 1. Architecture Overview

```
┌─────────────────────────────────────────────────┐
│              RazobillEngine (Razobill.cpp)       │
│  refresh() / compact() / verify() / define()    │
│  ledger() / leakage() / todo() / meaningOf()    │
└────────┬──────────────────────┬─────────────────┘
         │ SQL strings          │ SQL strings
    ┌────▼────┐            ┌────▼────┐
    │ Dialect │            │ Backend │
    │ (quote) │            │ (exec)  │
    └────┬────┘            └────┬────┘
         │                      │ ADBC C ABI
    ┌────▼──────────────────────▼────┐
    │     AdbcBackend (BackendAdbc)   │
    │  dlopen → AdbcDriver → Arrow   │
    └────────────────────────────────┘
```

---

## 2. Findings

### F-ADBC-01 · `sscanf` for decimal format parsing — no bounds check
| Field | Value |
|:------|:------|
| **Severity** | 🟡 MEDIUM |
| **File** | [`BackendAdbc.cpp#L123-L125`](file:///C:/FM/rzd/razobill/src/BackendAdbc.cpp#L123-L125) |
| **Description** | `decode()` uses `std::sscanf(fmt + 2, "%d,%d,%d", &precision, &scale, &bits)` to parse Arrow decimal format strings. If the format string is malformed or truncated, `sscanf` returns fewer matched fields and `precision`/`scale` retain their zero-initialized values silently. There is no check on the return value of `sscanf`. |
| **Impact** | Wrong precision/scale could produce incorrect decimal-to-double conversion. Not a buffer overflow (inputs come from Arrow metadata), but silent data corruption. |
| **Fix** | Check `sscanf` return value ≥ 2, throw `BackendError` on parse failure. |

### F-ADBC-02 · Dead code: `get<T,V>` template function never called
| Field | Value |
|:------|:------|
| **Severity** | ⚪ NOTE |
| **File** | [`BackendAdbc.cpp#L285-L289`](file:///C:/FM/rzd/razobill/src/BackendAdbc.cpp#L285-L289) |
| **Description** | The `get<T,V>` anonymous-namespace template is defined but never called from any code path. `Table::str()` calls it, but the `V` parameter is always `void` and serves no purpose. |
| **Impact** | Dead code clutter. The template compiles but the second type parameter is meaningless. |
| **Fix** | Remove the unused `V` template parameter or replace with a simple non-template helper. |

### F-ADBC-03 · Destructor calls `ConnectionRelease` / `DatabaseRelease` but ignores errors
| Field | Value |
|:------|:------|
| **Severity** | 🟢 LOW |
| **File** | [`BackendAdbc.cpp#L190-L202`](file:///C:/FM/rzd/razobill/src/BackendAdbc.cpp#L190-L202) |
| **Description** | `~AdbcBackend()` calls `driver_.ConnectionRelease` and `driver_.DatabaseRelease` but discards the `AdbcError` struct without releasing its message. If the driver allocates an error message, it leaks. |
| **Impact** | Minor memory leak on teardown errors. |
| **Fix** | After each call, check if `err.release` is set, and if so call `err.release(&err)`. |

### F-ADBC-04 · `sysUnloadLibrary` called after driver function pointers — use-after-free risk
| Field | Value |
|:------|:------|
| **Severity** | 🟡 MEDIUM |
| **File** | [`BackendAdbc.cpp#L194-L201`](file:///C:/FM/rzd/razobill/src/BackendAdbc.cpp#L194-L201) |
| **Description** | The destructor calls `ConnectionRelease` and `DatabaseRelease` (which are function pointers loaded from `lib_`), then calls `sysUnloadLibrary(lib_)`. If the driver's release functions trigger deferred work or callbacks, the code pages are already unmapped. The order is correct for normal drivers, but `preloadHandles_` are unloaded *after* `lib_`, which could cause issues if the main driver depends on preloaded symbols. |
| **Impact** | Potential crash on teardown if preloaded libraries are used by the driver's release path. |
| **Fix** | Reverse the unload order: unload `preloadHandles_` *last* (they were loaded first). |

### F-DIAL-01 · ClickHouse `quote()` does not escape NUL bytes
| Field | Value |
|:------|:------|
| **Severity** | 🟡 MEDIUM |
| **File** | [`Dialect.cpp#L63-L72`](file:///C:/FM/rzd/razobill/src/Dialect.cpp#L63-L72) |
| **Description** | `ClickHouseDialect::quote()` escapes `\` and `'` but not NUL (`\0`) bytes. If a string slot contains embedded NUL characters (valid in Razobill — binary data is supported), the ClickHouse SQL literal will be silently truncated at the NUL byte. |
| **Impact** | Data truncation for binary-containing strings on ClickHouse backend. |
| **Fix** | Escape `\0` as `\\0` in the ClickHouse dialect. |

### F-DIAL-02 · `DuckDialect::formExpr` returns `"RzTemplate(col)"` — requires DuckDB extension loaded
| Field | Value |
|:------|:------|
| **Severity** | ⚪ NOTE |
| **File** | [`Dialect.cpp#L50-L52`](file:///C:/FM/rzd/razobill/src/Dialect.cpp#L50-L52) |
| **Description** | The DuckDB dialect's `formExpr()` emits `RzTemplate(col)` which is a UDF provided by the DuckDB extension. If the extension is not loaded, the SQL will fail with "Scalar Function RzTemplate does not exist". There is no check or helpful error message guiding the user to load the extension first. |
| **Impact** | Confusing runtime error if extension not loaded. |
| **Fix** | Document the dependency. Optionally, add a preflight check in `RazobillEngine` constructor that verifies the extension is loaded when using DuckDB flavor. |

### F-ENGINE-03 · `requireIdentifier` rejects valid identifiers with digits in first position
| Field | Value |
|:------|:------|
| **Severity** | ⚪ NOTE |
| **File** | [`Razobill.cpp#L88-L92`](file:///C:/FM/rzd/razobill/src/Razobill.cpp#L88-L92) |
| **Description** | `requireIdentifier()` enforces `[A-Za-z_][A-Za-z0-9_]{0,63}` which is stricter than SQL's quoted identifier rules. This is intentional safety, but prevents table names like `"2026_logs"`. Since all identifiers are also passed through `quoteIdentifier()`, leading digits would be safe. |
| **Impact** | User-facing restriction. Documented as intentional. |
| **Fix** | Keep as-is (defense-in-depth) but document the restriction. |

### F-ENGINE-04 · `rebuildDerivedLocked()` builds unbounded SQL `VALUES` list
| Field | Value |
|:------|:------|
| **Severity** | 🟡 MEDIUM |
| **File** | [`Razobill.cpp#L490-L503`](file:///C:/FM/rzd/razobill/src/Razobill.cpp#L490-L503) |
| **Description** | The function batches 400 rows per `INSERT` but accumulates all rows into a single `std::string values` before flushing. With millions of forms, each batch of 400 is fine, but the outer loop correctly flushes every 400. However, the `flush()` lambda re-enters `db_.exec()` which holds the engine mutex — this is safe because `rebuildDerivedLocked()` is always called with the mutex already held. No reentrant lock issue here because `connMutex_` in ADBC is separate from the engine `mu_`. |
| **Impact** | Correct as-is. The batch size of 400 is reasonable. |
| **Fix** | None required. Could add a comment explaining the batching rationale. |

### F-ENGINE-05 · `TransactionGuard` only wraps DuckDB transactions — ClickHouse operations are not atomic
| Field | Value |
|:------|:------|
| **Severity** | 🟡 MEDIUM |
| **File** | [`Razobill.cpp#L48-L86`](file:///C:/FM/rzd/razobill/src/Razobill.cpp#L48-L86) |
| **Description** | `TransactionGuard` is a no-op for ClickHouse (`active_ = false` when flavor != DuckDB). This means `compact()`, `define()`, `retire()`, and `pin()` are not transactional on ClickHouse. If any step fails mid-operation, the database is left in an inconsistent state. |
| **Impact** | Partial writes on ClickHouse if an error occurs mid-operation. |
| **Fix** | Document the limitation. ClickHouse MergeTree does not support multi-statement transactions, so this is an inherent engine constraint. Consider adding explicit cleanup/rollback logic for ClickHouse paths. |

### F-SQL-01 · SQL injection defense — analysis
| Field | Value |
|:------|:------|
| **Severity** | ⚪ NOTE |
| **File** | [`Razobill.cpp`](file:///C:/FM/rzd/razobill/src/Razobill.cpp) |
| **Description** | **Defense is adequate.** All user-supplied identifiers pass through `requireIdentifier()` (alphanumeric + underscore, max 64 chars) before being passed to `quoteIdentifier()`. All user-supplied string values pass through `Dialect::quote()` which properly escapes single quotes (DuckDB: doubling, ClickHouse: backslash). Numeric values are formatted via `std::to_string()` or `num()` which produce safe decimal strings. The regex patterns in `define()` are passed through `Dialect::quote()` as string literals to the `regexp_matches` / `match` SQL functions, not interpolated into SQL structure. |
| **Impact** | No SQL injection vectors found. |
| **Fix** | None required. |

---

## 3. Summary

| ID | Severity | Component | One-liner |
|:---|:---------|:----------|:----------|
| F-ADBC-01 | 🟡 MEDIUM | BackendAdbc | `sscanf` return value unchecked in decimal decode |
| F-ADBC-02 | ⚪ NOTE | BackendAdbc | Dead `get<T,V>` template |
| F-ADBC-03 | 🟢 LOW | BackendAdbc | Destructor leaks ADBC error message |
| F-ADBC-04 | 🟡 MEDIUM | BackendAdbc | Preload unload order may cause use-after-free |
| F-DIAL-01 | 🟡 MEDIUM | Dialect | ClickHouse `quote()` does not escape NUL bytes |
| F-DIAL-02 | ⚪ NOTE | Dialect | DuckDB `formExpr` requires loaded extension |
| F-ENGINE-03 | ⚪ NOTE | Engine | `requireIdentifier` stricter than necessary |
| F-ENGINE-04 | 🟡 MEDIUM | Engine | Batch INSERT is correctly bounded (verified OK) |
| F-ENGINE-05 | 🟡 MEDIUM | Engine | ClickHouse operations are not transactional |
| F-SQL-01 | ⚪ NOTE | Engine | SQL injection defense verified adequate |
