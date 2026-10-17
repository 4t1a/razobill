# Razobill Missing & Incomplete Audit

---

## M1. Python Fallback Behavioral Divergence

**Files:**  
- [_fallback.py:107–110](file:///C:/FM/rzd/razobill/lib/python/pyrazobill/_fallback.py#L107-L110)  
- [Scanner.hpp:169–184](file:///C:/FM/rzd/razobill/include/librazobill/Scanner.hpp#L169-L184)  
**Severity:** 🟡 MEDIUM  

**Problem:** The pure-Python fallback `_scan()` applies extra heuristics not present in the C++ `Scanner`:

| Heuristic | Python Fallback | C++ Scanner |
|-----------|:---------------:|:-----------:|
| `/`-containing tokens treated as variables | ✅ (lines 107-108) | ❌ (`/` is a delimiter) |
| Pure-alpha hex strings (`[a-fA-F]{8,}`) treated as variables | ✅ (lines 109-110) | ❌ (requires `seen & kDigit`) |
| Leading-zero integers classified as string | ✅ | ✅ |
| Semantic key-value (`k=v`) extraction | ✅ | ✅ |

**Impact:** The same log message produces different templates between native and fallback implementations. For example, a pure-alpha hex hash like `deadbeef` is tokenized as a variable slot in Python fallback but kept as literal syntax in C++ core. Conformance tests (`ConformanceTest.py`) have a corpus of only 525 bytes, missing this divergence.

**Fix:** Align Python fallback to C++ Scanner by removing the extra heuristics that do not match C++ core behavior.

---

## M2. C ABI `strLengths == nullptr` Acceptance & Null Pointers

**File:** [RazobillC.cpp:89–94](file:///C:/FM/rzd/razobill/src/RazobillC.cpp#L89-L94)  
**Severity:** 🟡 MEDIUM  

```cpp
const size_t sLen = record->strLengths ? record->strLengths[i] : 0;
```

**Problem:** If a foreign-language caller (Go, Rust, C#, Python ctypes) manually constructs an `RzRecord` with `strCount > 0` and valid `strValues` pointers, but leaves `strLengths == nullptr`, all string lengths silently default to 0. Reconstruction produces empty strings instead of the actual values. Furthermore, if `intCount > 0` but `intValues == nullptr`, it passes `nullptr` to `std::span`, triggering undefined behavior on dereference.

**Fix:** Add strict defensive validation at the entry of `RzJoin`:
```cpp
if (record->strCount > 0 && (!record->strValues || !record->strLengths))
    return RZ_ERR_INVALID_ARGUMENT;
if (record->intCount > 0 && !record->intValues)
    return RZ_ERR_INVALID_ARGUMENT;
```

---

## M3. Benchmark Uses Legacy Format Only

**File:** [bench/BenchCore.cpp:171–176](file:///C:/FM/rzd/razobill/bench/BenchCore.cpp#L171-L176)  
**Severity:** 🔵 LOW  

**Problem:** The reconstruct benchmark in `BenchCore.cpp` tests only `formKey` (legacy `skeleton\x13types` format) and never benchmarks the new `{i}/{s}` template format path (`record.templateStr`).

**Fix:** Add a benchmark case measuring reconstruction speed using modern `{i}/{s}` format templates.

---

## M4. `ScanMode` Ignored in ClickHouse Dialect

**File:** [Dialect.cpp:80–86](file:///C:/FM/rzd/razobill/src/Dialect.cpp#L80-L86)  
**Severity:** 🟡 MEDIUM  

```cpp
std::string formExpr(std::string_view col, core::ScanMode /*mode*/) const override {
```

**Problem:** The `ScanMode` parameter is commented out and ignored. ClickHouse SQL dialect always uses regex-based extraction that only recognizes numeric tokens. Semantic-mode features (key-value pairs, contextual keywords, file suffixes) are not supported at the SQL layer in ClickHouse.

**Impact:** ClickHouse backend produces different templates than DuckDB for the same log data.

**Fix:** Document the limitation explicitly, and ensure queries with `ScanMode::Semantic` fall back gracefully or document that semantic mode requires UDF compilation.

---

## M5. OpenTelemetry Go Extension Unchecked Status & Memory Hygiene

**File:** [ext/otel/RazobillProcessor.go:65–109](file:///C:/FM/rzd/razobill/ext/otel/RazobillProcessor.go#L65-L109)  
**Severity:** 🟡 MEDIUM  

**Problem:** 
1. If `RzSplit` fails (`status != C.RZ_OK`), the error is silently dropped and the raw log line remains unmodified with no metric or log warning emitted.
2. In `ConsumeLogs`, `cMsg := C.CString(msg)` allocates via `malloc` and frees via `C.free` per log line. For millions of logs, this creates severe cgo memory allocation churn.

**Fix:** Emit a warning/counter on non-OK status, and consider batch-processing or string re-use in cgo.

---

## M6. Vector VRL Rust Extension Unnecessary CString Heap Allocation

**File:** [ext/vector/RazobillVrl.rs:48–50](file:///C:/FM/rzd/razobill/ext/vector/RazobillVrl.rs#L48-L50)  
**Severity:** 🟡 MEDIUM  

```rust
let c_str = CString::new(raw_msg).map_err(|e| e.to_string())?;
let status = unsafe { RzSplit(c_str.as_ptr(), raw_msg.len(), &mut record) };
```

**Problem:**
1. `CString::new(raw_msg)` fails if `raw_msg` contains embedded null bytes (`\0`), returning an error even though Razobill's `std::string_view` parser handles arbitrary byte sequences.
2. Allocates a new heap string (`CString`) on every single log line in Rust.

**Fix:** Since `RzSplit` takes an explicit length parameter `message_length`, it does not require null-termination. Pass `raw_msg.as_ptr() as *const c_char` directly, avoiding the heap allocation and eliminating failure on embedded nulls.

---

## M7. Python C-API Memory Leak on Type Conversion Failure

**File:** [lib/python/PyRazobill.cpp:237–277](file:///C:/FM/rzd/razobill/lib/python/PyRazobill.cpp#L237-L277)  
**Severity:** 🟡 MEDIUM  

**Problem:** In `joinOne()`, `encodedTmpl` is created via `PyUnicode_AsEncodedString`. If `intsSeq` contains an invalid element that throws on `item.cast<int64_t>()`, the exception propagates before the `cleanup()` lambda is invoked, leaking `encodedTmpl`.

**Fix:** Use RAII wrappers (like `py::gil_scoped_release` and `py::object` or a custom smart pointer for raw `PyObject*`) to ensure deterministic cleanup on exception.

---

## M8. Conformance Corpus Too Small

**File:** [tests/ConformanceCorpus.txt](file:///C:/FM/rzd/razobill/tests/ConformanceCorpus.txt) — 525 bytes  
**Severity:** 🟡 MEDIUM  

**Problem:** The conformance corpus contains only 5 test lines. This is inadequate to verify cross-implementation parity between C++, Python native, Python fallback, DuckDB extension, Go OTel processor, and Rust VRL plugin.

**Fix:** Expand corpus with:
- Edge cases: empty strings, pure whitespace, single characters, max-length messages, UTF-8 multibyte, nested braces `{{` and `}}`.
- Real-world log samples: SSH, Syslog, Apache/Nginx access & error logs, HDFS, Spark, Kubernetes audit logs.
- Adversarial inputs: SQL injection attempts, null bytes, ASCII control characters (0x11, 0x12, 0x13).

---

## Completeness Matrix

| Component | Headers | Source | Tests | Bench | Docs | Status |
|-----------|:-------:|:------:|:-----:|:-----:|:----:|:------:|
| Scanner/Slots | ✅ | ✅ | ✅ | ✅ | ✅ | Solid |
| Stats (Math) | ✅ | ✅ | ✅ | ❌ | ✅ | Needs bench |
| Channel (Stego) | ✅ | ✅ | ✅ | ❌ | ✅ | Needs bench |
| Razobill Engine | ✅ | ✅ | ✅ | ❌ | ⚠️ | Needs refactoring |
| Backend/ADBC | ✅ | ✅ | ⚠️ | ❌ | ⚠️ | Needs leak fix |
| C ABI | ✅ | ✅ | ✅ | ❌ | ✅ | Needs defensive guards |
| DuckDB Ext | — | ✅ | ✅ | ❌ | ⚠️ | Solid |
| Vector Rust Ext | — | ✅ | ✅ | ❌ | ⚠️ | Needs zero-alloc fix |
| OTel Go Ext | — | ✅ | ✅ | ❌ | ⚠️ | Needs error logging |
| Python Native | — | ✅ | ✅ | ❌ | ✅ | Needs leak fix |
| Python Fallback | — | ✅ | ⚠️ | ❌ | ❌ | Needs parity alignment |

*Legend: ✅ Complete & verified, ⚠️ Minor gaps, ❌ Missing*
