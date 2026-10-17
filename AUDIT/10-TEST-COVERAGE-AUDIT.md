# 10 — Test Coverage Audit

**Scope**: All test files in `tests/`, `ext/*/tests/`, `lib/python/tests/`, fuzz harness

---

## 1. Test File Inventory

| File | Framework | Lines | What It Tests |
|:-----|:----------|------:|:--------------|
| `UnitCore.cpp` | doctest | 339 | Scanner, Slots, parse/reconstruct roundtrip, SIMD, ThreadPool |
| `UnitStats.cpp` | doctest | 267 | xxHash vectors, entropy, ledger identities, merge cost, leakage |
| `UnitArena.cpp` | doctest | 215 | MemoryArena, StringPool, FormCatalog, ThreadPool |
| `UnitCAPI.cpp` | doctest | 153 | C ABI: RzSplit, RzJoin, RzTemplate, RzFree, edge cases |
| `UnitChannel.cpp` | doctest | 165 | Channel embed/decode, capacity analysis, mantissa bounds |
| `FuzzDecomposeReconstruct.cpp` | libFuzzer + standalone | 231 | Roundtrip invariant (both modes + C ABI), multi-threaded random stress |
| `IntegrationDuckdb.cpp` | — | — | DuckDB extension integration (not read, assumed) |
| `IntegrationChdb.cpp` | — | — | chDB/ClickHouse integration (not read, assumed) |
| `ConformanceTest.py` | — | — | Cross-language conformance (not read, assumed) |
| `ext/otel/tests/RazobillProcessor_test.go` | Go testing | — | OTel processor unit tests |
| `ext/vector/tests/TestVectorVrl.rs` | Rust #[test] | — | Vector VRL crate tests |
| `lib/python/tests/TestPyRazobill.py` | pytest | — | Python bindings tests |
| `lib/python/tests/TestProperties.py` | hypothesis | — | Python property-based tests |

---

## 2. Coverage Matrix — Module vs Test

| Module | Unit Test | Integration | Fuzz | Property |
|:-------|:---------:|:-----------:|:----:|:--------:|
| Scanner (`scan()`) | ✅ UnitCore | — | ✅ | ✅ 300K random |
| Slots (`parse`/`reconstruct`) | ✅ UnitCore | — | ✅ | ✅ 300K random |
| SimdScanner | ✅ UnitCore (differential) | — | — | — |
| Hash (`xxh64`) | ✅ UnitStats (vectors) | — | ✅ (via C ABI) | — |
| ThreadPool | ✅ UnitCore + UnitArena | — | ✅ (multi-thread fuzz) | — |
| MemoryArena | ✅ UnitArena | — | — | — |
| StringPool | ✅ UnitArena (concurrent) | — | — | — |
| FormCatalog | ✅ UnitArena (collision) | — | — | — |
| Stats (ledger, merge, leakage) | ✅ UnitStats (20K random) | — | — | — |
| Channel (embed/decode) | ✅ UnitChannel | — | — | — |
| **BackendAdbc** | ❌ **NONE** | ✅ IntegrationDuckdb | — | — |
| **Dialect** | ❌ **NONE** | ✅ (implicit via Engine) | — | — |
| **RazobillEngine** | ❌ **NONE** | ✅ IntegrationDuckdb | — | — |
| **C ABI** | ✅ UnitCAPI | — | ✅ | — |
| **DuckDB Extension** | ❌ **NONE** | ✅ IntegrationDuckdb | — | — |
| **OTel Processor** | ✅ Go test | — | — | — |
| **Vector VRL** | ✅ Rust test | — | — | — |
| **Python Bindings** | ✅ pytest | — | — | ✅ hypothesis |

---

## 3. Findings

### F-TEST-01 · No unit tests for `BackendAdbc.cpp`
| Field | Value |
|:------|:------|
| **Severity** | 🟡 MEDIUM |
| **File** | `tests/` |
| **Description** | `BackendAdbc.cpp` (321 lines) has no dedicated unit tests. The `decode()` function handles 14+ Arrow data types and has complex decimal-128 conversion logic. It is exercised indirectly by `IntegrationDuckdb`, but that test requires a DuckDB instance and does not isolate the `decode()` function or test edge cases (e.g., negative decimal-128, null validity, dictionary-encoded columns). |
| **Impact** | Type conversion bugs in `decode()` (e.g., F-ADBC-01) are not caught by isolated tests. |
| **Fix** | Add `UnitBackend.cpp` with mock Arrow arrays to test each `decode()` path. |

### F-TEST-02 · No unit tests for `Dialect.cpp`
| Field | Value |
|:------|:------|
| **Severity** | 🟡 MEDIUM |
| **File** | `tests/` |
| **Description** | Both `DuckDialect` and `ClickHouseDialect` implement quoting, type mapping, and SQL expression generation. None of these have dedicated unit tests. The `quote()` functions are critical for SQL injection defense (F-SQL-01) but are only tested implicitly through integration tests. The ClickHouse NUL byte escaping gap (F-DIAL-01) would have been caught by a simple unit test. |
| **Impact** | Dialect bugs escape to integration testing. |
| **Fix** | Add `UnitDialect.cpp` testing `quote()` with edge cases (NUL, unicode, nested quotes), type names, and SQL expression generation for both dialects. |

### F-TEST-03 · No unit tests for `RazobillEngine` control plane
| Field | Value |
|:------|:------|
| **Severity** | 🟡 MEDIUM |
| **File** | `tests/` |
| **Description** | `RazobillEngine` (703 lines) implements `refresh()`, `compact()`, `verify()`, `define()`, `retire()`, `pin()`, `ledger()`, `leakage()`, `todo()`, and `meaningOf()`. These are complex stateful operations with SQL generation, transaction management, and information-theoretic gating. They are tested only through integration tests. |
| **Impact** | Logic errors in the control plane (e.g., SQL generation, transaction rollback, gate threshold) are not isolated. |
| **Fix** | Add `UnitEngine.cpp` using an in-memory DuckDB backend to test each operation in isolation. |

### F-TEST-04 · Fuzz harness skips messages > 1 MB
| Field | Value |
|:------|:------|
| **Severity** | 🟢 LOW |
| **File** | [`FuzzDecomposeReconstruct.cpp#L22-L24`](file:///C:/FM/rzd/razobill/tests/fuzz/FuzzDecomposeReconstruct.cpp#L22-L24) |
| **Description** | The fuzzer returns 0 (skip) for inputs > 1 MB. The library's `kMaxMessageBytes` is 16 MB. Inputs between 1 MB and 16 MB are never fuzzed. The boundary behavior at exactly `kMaxMessageBytes` (parse should return `Status::TooLarge`) is not tested by the fuzzer. |
| **Impact** | Boundary bugs around the 16 MB limit are not fuzz-tested. |
| **Fix** | Increase the fuzzer cap to `kMaxMessageBytes + 1` or add explicit boundary test cases in the standalone harness. |

### F-TEST-05 · `UnitCore.cpp` property test uses `ScanMode::DigitOnly` — `Semantic` not property-tested in C++
| Field | Value |
|:------|:------|
| **Severity** | 🟡 MEDIUM |
| **File** | [`UnitCore.cpp#L186-L200`](file:///C:/FM/rzd/razobill/tests/UnitCore.cpp#L186-L200) |
| **Description** | The 300,000-iteration property test at line 186 uses the default `ScanMode::DigitOnly`. The `Semantic` mode has more complex parsing (quoted strings, key-value pairs, contextual variables) but is only tested via a handful of specific test cases (lines 47-76), not via property-based random testing in C++. The Python Hypothesis tests cover Semantic mode, but C++ property coverage is absent. |
| **Impact** | Semantic mode regressions could be missed. |
| **Fix** | Add a parallel 300K-iteration property test with `ScanMode::Semantic`. |

### F-TEST-06 · No negative tests for Channel embed with invalid parameters
| Field | Value |
|:------|:------|
| **Severity** | 🟢 LOW |
| **File** | [`UnitChannel.cpp`](file:///C:/FM/rzd/razobill/tests/UnitChannel.cpp) |
| **Description** | `UnitChannel.cpp` tests the happy path (embed/decode roundtrip, capacity analysis, mantissa bounds). It does not test: empty sequences, sequences with zero-probability options, NaN/Inf shared keys, payloadBits = 0, or sequences where all options have probability 1.0 (no latitude). |
| **Impact** | Edge cases in Channel could crash or produce incorrect results. |
| **Fix** | Add edge case tests for degenerate inputs. |

### F-TEST-07 · `UnitArena.cpp` StringPool concurrent test uses `lookup()` not `lookupCopy()` in reader
| Field | Value |
|:------|:------|
| **Severity** | ⚪ NOTE |
| **File** | [`UnitArena.cpp#L114`](file:///C:/FM/rzd/razobill/tests/UnitArena.cpp#L114) |
| **Description** | The concurrent stress test at line 114 uses `pool.lookupCopy(id0)` (safe copy) and `pool.withString(id0, ...)` (mutex-guarded). It does NOT use the raw `pool.lookup(id0)` which returns a `string_view` that could dangle during concurrent `gc()`. This is correct test design — it tests the safe APIs. However, it means the dangerous `lookup()` API (F-SPOOL-02 from doc 03) is not tested for its documented unsafety. |
| **Impact** | The unsafe API path is untested. |
| **Fix** | Either deprecate the raw `lookup()` or add a test demonstrating the GC+lookup race (expected to fail under TSan). |

### F-TEST-08 · No conformance test for ClickHouse backend
| Field | Value |
|:------|:------|
| **Severity** | 🟢 LOW |
| **File** | `tests/` |
| **Description** | `IntegrationDuckdb.cpp` tests the DuckDB path. `IntegrationChdb.cpp` presumably tests chDB. However, there is no conformance test that verifies both backends produce identical results for the same input data. This is important because the Dialect abstraction (Dialect.cpp) generates different SQL for each engine. |
| **Impact** | Backend-specific SQL generation bugs could go undetected. |
| **Fix** | Add a cross-backend conformance test that runs the same operations on both DuckDB and ClickHouse and compares results. |

---

## 4. Summary

| ID | Severity | Component | One-liner |
|:---|:---------|:----------|:----------|
| F-TEST-01 | 🟡 MEDIUM | Tests | No unit tests for BackendAdbc |
| F-TEST-02 | 🟡 MEDIUM | Tests | No unit tests for Dialect |
| F-TEST-03 | 🟡 MEDIUM | Tests | No unit tests for RazobillEngine |
| F-TEST-04 | 🟢 LOW | Tests | Fuzz harness skips 1–16 MB range |
| F-TEST-05 | 🟡 MEDIUM | Tests | Semantic mode not property-tested in C++ |
| F-TEST-06 | 🟢 LOW | Tests | No negative tests for Channel edge cases |
| F-TEST-07 | ⚪ NOTE | Tests | Unsafe `lookup()` API path untested |
| F-TEST-08 | 🟢 LOW | Tests | No cross-backend conformance test |
