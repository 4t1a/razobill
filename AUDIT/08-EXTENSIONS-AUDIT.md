# 08 — Extensions & Bindings Audit

**Scope**: `ext/duckdb/DuckdbExtension.cpp`, `ext/otel/*.go`, `ext/vector/RazobillVrl.rs`, `lib/python/PyRazobill.cpp`

---

## 1. Component Map

| Extension | Language | Integration Point | Lines |
|:----------|:---------|:-------------------|------:|
| DuckDB Extension | C++ | DuckDB C Extension API | 518 |
| OTel Processor | Go + cgo | OpenTelemetry Collector SDK | 178 |
| Vector VRL | Rust + FFI | librazobill C ABI (`RzSplit`/`RzFree`) | 111 |
| Python Bindings | C++ (pybind11) | Direct C++ linkage | 448 |

---

## 2. Findings

### F-EXT-01 · DuckDB `thread_local Scratch` — no destructor ordering guarantee
| Field | Value |
|:------|:------|
| **Severity** | 🟢 LOW |
| **File** | [`DuckdbExtension.cpp#L101-L104`](file:///C:/FM/rzd/razobill/ext/duckdb/DuckdbExtension.cpp#L101-L104) |
| **Description** | `scratch()` returns a `thread_local Scratch` struct. DuckDB worker threads are managed by DuckDB's scheduler. When a worker thread exits, the `thread_local` destructor runs, which deallocates `std::string` and `std::vector` members. This is safe under normal conditions, but if the shared library is unloaded (`dlclose`) before thread destruction, the destructor code page is unmapped. DuckDB extensions are typically kept loaded for the connection lifetime, so this is unlikely in practice. |
| **Impact** | Theoretical crash on unusual unload ordering. |
| **Fix** | Document the assumption. Optionally use `atexit`-registered cleanup or leak-on-unload pattern. |

### F-EXT-02 · DuckDB `RzSplitImpl` re-fetches `intsGrandchild` data pointer after `reserve` — stale pointer
| Field | Value |
|:------|:------|
| **Severity** | 🟠 HIGH |
| **File** | [`DuckdbExtension.cpp#L220-L224`](file:///C:/FM/rzd/razobill/ext/duckdb/DuckdbExtension.cpp#L220-L224) |
| **Description** | In `RzSplitImpl`, `dstInts` is obtained from `duckdb_vector_get_data(intsGrandchild)` **after** `duckdb_list_vector_reserve(intsChild, totalInts + numInts)`. The reserve call may reallocate the underlying buffer. However, `dstInts` is re-fetched on every row iteration (line 222 is inside the per-row loop), so the pointer is refreshed after each reserve. This is **correct** — the pointer is not cached across reserve calls. Verified safe. |
| **Impact** | No issue — pointer is correctly re-fetched after reserve. Downgraded to NOTE. |
| **Fix** | None required. Add a comment explaining why `dstInts` is re-fetched inside the loop. |

### F-EXT-03 · OTel Go processor: `C.CString` leak on `RzSplit` failure path
| Field | Value |
|:------|:------|
| **Severity** | 🟠 HIGH |
| **File** | [`RazobillProcessor.go#L63-L111`](file:///C:/FM/rzd/razobill/ext/otel/RazobillProcessor.go#L63-L111) |
| **Description** | `C.free(unsafe.Pointer(cMsg))` is called at line 111 inside the inner loop. However, if `RzSplit` returns a non-OK status (line 71), the code falls through to the `else` branch (line 108-110), then continues to `C.free` at line 111. This path is correct — `cMsg` is always freed. **However**, `RzFree(&cRecord)` is only called on the success path (line 107). On failure, `cRecord` was zero-initialized and `RzSplit` did not allocate, so `RzFree` is not needed. Verified safe. |
| **Impact** | No leak. Code is correct. |
| **Fix** | None required. |

### F-EXT-04 · OTel Go processor: `Config.Mode` field is accepted but never used
| Field | Value |
|:------|:------|
| **Severity** | 🟡 MEDIUM |
| **File** | [`Config.go#L4`](file:///C:/FM/rzd/razobill/ext/otel/Config.go#L4), [`RazobillProcessor.go`](file:///C:/FM/rzd/razobill/ext/otel/RazobillProcessor.go) |
| **Description** | `Config` has a `Mode` field (`"semantic"` or `"exact"`), and `Validate()` silently corrects invalid values. However, `ConsumeLogs` never reads `p.cfg.Mode` — it always calls `C.RzSplit` which hardcodes `ScanMode::Semantic`. The mode configuration is dead code. |
| **Impact** | User sets `mode: "exact"` in OTel config but gets semantic mode. Silent misconfiguration. |
| **Fix** | Either (a) remove the `Mode` field until `RzSplitEx` is added to the C ABI (see F-CABI-02), or (b) document that only semantic mode is supported. |

### F-EXT-05 · OTel Go processor: hash divergence risk with Go-native xxhash
| Field | Value |
|:------|:------|
| **Severity** | 🟡 MEDIUM |
| **File** | [`RazobillProcessor.go#L15`](file:///C:/FM/rzd/razobill/ext/otel/RazobillProcessor.go#L15), [`RazobillProcessor.go#L82`](file:///C:/FM/rzd/razobill/ext/otel/RazobillProcessor.go#L82) |
| **Description** | The processor imports `github.com/cespare/xxhash/v2` and computes `xxhash.Sum64String(tmplStr)` for the `razobill.formHash` attribute. The C++ library uses `core::xxh64()` with seed 0. The Go `xxhash/v2` package also uses seed 0, so hashes currently match. However, this coupling is implicit — if the C++ library ever changes to a different hash or seed, the Go processor silently diverges. |
| **Impact** | Fragile implicit contract. Works today but creates maintenance risk. |
| **Fix** | Use `RzHash` from the C ABI once F-CABI-01 is fixed, eliminating the Go-native dependency. |

### F-EXT-06 · Rust Vector VRL: `from_utf8_lossy` silently replaces invalid UTF-8
| Field | Value |
|:------|:------|
| **Severity** | 🟡 MEDIUM |
| **File** | [`RazobillVrl.rs#L74`](file:///C:/FM/rzd/razobill/ext/vector/RazobillVrl.rs#L74), [`RazobillVrl.rs#L96`](file:///C:/FM/rzd/razobill/ext/vector/RazobillVrl.rs#L96) |
| **Description** | The Rust crate uses `String::from_utf8_lossy()` for both the template string and extracted string slots. Razobill's core engine is byte-exact and supports arbitrary binary data. `from_utf8_lossy` replaces invalid UTF-8 sequences with the Unicode replacement character U+FFFD (`�`), silently corrupting binary log data. |
| **Impact** | Data corruption for binary log messages routed through Vector. The lossless invertibility guarantee is broken. |
| **Fix** | Use `Vec<u8>` / raw bytes in the JSON output instead of forcing UTF-8 conversion, or use `serde_json::Value::String` with a base64 encoding fallback for non-UTF-8 data. |

### F-EXT-07 · Rust Vector VRL: no `Send`/`Sync` safety for `RzRecord`
| Field | Value |
|:------|:------|
| **Severity** | 🟢 LOW |
| **File** | [`RazobillVrl.rs#L9-L19`](file:///C:/FM/rzd/razobill/ext/vector/RazobillVrl.rs#L9-L19) |
| **Description** | The `RzRecord` struct contains raw pointers (`*const c_char`, `*mut c_void`). Rust does not auto-implement `Send` or `Sync` for types with raw pointers. The `parse_razobill` function creates, uses, and frees the record within a single call, so it never crosses thread boundaries. This is safe. |
| **Impact** | No issue — record never escapes the function. |
| **Fix** | None required. |

### F-EXT-08 · Python `splitMany` silently discards parse errors
| Field | Value |
|:------|:------|
| **Severity** | 🟡 MEDIUM |
| **File** | [`PyRazobill.cpp#L359`](file:///C:/FM/rzd/razobill/lib/python/PyRazobill.cpp#L359) |
| **Description** | In `splitMany`, the worker threads call `(void)core::parse(...)` — the return status is explicitly discarded. If any message fails to parse (e.g., exceeds `kMaxMessageBytes`), the corresponding `SlotRecord` remains default-initialized with an empty template and no slots. The caller receives a tuple `("", [], [])` with no indication of failure. |
| **Impact** | Silent data loss in batch processing. Failed messages produce empty results indistinguishable from genuinely empty messages. |
| **Fix** | Collect parse statuses in a parallel vector. After the GIL is reacquired, check for failures and either raise an exception with the first failure index or return a list of `(template, ints, strs, status)` tuples. |

### F-EXT-09 · Python `splitMany` string_view into `rawInputs[idx].bytes` — lifetime safe
| Field | Value |
|:------|:------|
| **Severity** | ⚪ NOTE |
| **File** | [`PyRazobill.cpp#L349-L369`](file:///C:/FM/rzd/razobill/lib/python/PyRazobill.cpp#L349-L369) |
| **Description** | Each `rawInputs[idx].bytes` is a `std::string` that owns its data. The `parse()` call creates `string_view` references into `rawInputs[idx].bytes` (for `slots.strs`). These views are valid because `rawInputs` is not modified during parallel processing and lives until after the Python list is built. Verified safe. |
| **Impact** | No issue. |
| **Fix** | None required. |

### F-EXT-10 · Python `pthread_atfork` handler — ThreadPool recreation on fork
| Field | Value |
|:------|:------|
| **Severity** | ⚪ NOTE |
| **File** | [`PyRazobill.cpp#L37-L41`](file:///C:/FM/rzd/razobill/lib/python/PyRazobill.cpp#L37-L41) |
| **Description** | On Unix, a `pthread_atfork` child handler recreates the ThreadPool after `fork()`. This correctly avoids inheriting stale thread state in child processes (e.g., multiprocessing). On Windows, `pthread_atfork` is not available, but `fork()` is also not used on Windows. Correct. |
| **Impact** | No issue. Good defensive pattern. |
| **Fix** | None required. |

---

## 3. Summary

| ID | Severity | Component | One-liner |
|:---|:---------|:----------|:----------|
| F-EXT-01 | 🟢 LOW | DuckDB Ext | `thread_local` destructor vs `dlclose` ordering |
| F-EXT-02 | ⚪ NOTE | DuckDB Ext | `dstInts` correctly re-fetched after reserve (verified) |
| F-EXT-03 | ⚪ NOTE | OTel Go | `CString` free path verified correct |
| F-EXT-04 | 🟡 MEDIUM | OTel Go | `Config.Mode` accepted but never used |
| F-EXT-05 | 🟡 MEDIUM | OTel Go | Hash divergence risk with Go-native xxhash |
| F-EXT-06 | 🟡 MEDIUM | Rust Vector | `from_utf8_lossy` silently corrupts binary data |
| F-EXT-07 | 🟢 LOW | Rust Vector | Raw pointer Send/Sync — safe in practice |
| F-EXT-08 | 🟡 MEDIUM | Python | `splitMany` silently discards parse errors |
| F-EXT-09 | ⚪ NOTE | Python | `string_view` lifetimes verified safe |
| F-EXT-10 | ⚪ NOTE | Python | `pthread_atfork` handler correct |
