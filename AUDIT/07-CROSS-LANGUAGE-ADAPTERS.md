# Cross-Language Adapters, Foreign Function Interfaces (FFI) & Extensions

**Audit Subject**: Universal C ABI, DuckDB Extension, Python pybind11, Go OpenTelemetry, and Rust Vector VRL  
**Standard**: Binary compatibility, ABI stability, and zero-copy foreign boundary safety (DuckDB / ClickHouse parity)  

---

## 1. Universal C ABI (`RazobillC.h` / `RazobillC.cpp`)

The Universal C ABI provides a stable C binary interface for languages that interoperate via C linkage (Python ctypes/cffi, Go cgo, Rust FFI, Zig, C#, Java JNI).

### 1.1 Interface Definition & Public Surface
Declared functions:
1. `RzSplit(message, messageLength, outRecord)`: Decomposes message into an `RzRecord`.
2. `RzJoin(record, outBuffer, bufferCapacity, outLength)`: Reconstructs exact message from record.
3. `RzTemplate(message, messageLength, outBuffer, bufferCapacity, outLength)`: Fast-path template extraction.
4. `RzFree(record)`: Frees internal heap scratch allocated by `RzSplit`.
5. `RzStatusMessage(status)`: Returns constant description string for status code.
6. `RzVersion()`: Returns semantic library version string (`"1.1.2"`).
7. `RzAbiVersion()`: Returns numeric ABI contract version (`1`).

### 1.2 Surface Budget Violation (F-CABI-01)
- `tools/CheckSurfaceC.py` enforces a hard cap of **$\le 6$ functions** in `RazobillC.h`.
- When `RzAbiVersion()` was added to support formal ABI governance, the total function count reached **7**.
- Running `python tools/CheckSurfaceC.py` results in:
  `FAILED: Function count 7 exceeds maximum allowance of 6!`
- **Remediation**: Reconcile the surface policy in `CheckSurfaceC.py` to permit 7 functions (acknowledging `RzAbiVersion` as an approved governance entry point), or consolidate `RzVersion` and `RzAbiVersion`.

### 1.3 Memory Lifecycle & Manual Record Support
- `RzFree` is completely safe on zeroed, partially populated, or manually constructed foreign records:
```cpp
RZ_API void RzFree(RzRecord* record) {
    if (!record) return;
    if (record->internalHandle) {
        delete static_cast<InternalRecord*>(record->internalHandle);
        record->internalHandle = nullptr;
    }
    std::memset(record, 0, sizeof(RzRecord));
}
```
- Foreign callers (such as Rust or Go) can populate an `RzRecord` manually with `internalHandle = NULL` to call `RzJoin` without ever invoking `RzSplit`.

---

## 2. DuckDB Vectorized Extension (`ext/duckdb/DuckdbExtension.cpp`)

Exposes Razobill scalar functions directly into DuckDB SQL query plans using DuckDB's native C extension API.

### 2.1 Vectorized Processing & Thread-Local Scratch
- DuckDB executes scalar queries in chunks of up to 2,048 rows across multiple worker threads.
- `DuckdbExtension.cpp` maintains a `thread_local Scratch` per worker thread:
```cpp
struct Scratch {
    core::SlotRecord record;
    std::string text;
    std::vector<std::int64_t> ints;
    std::vector<std::string_view> strs;
    std::vector<Arg> args;
    void trim() { /* frees buffers exceeding 1 MB */ }
};
```
- **Zero Contention**: Each thread accesses its own scratch buffer without mutexes or atomic operations on the row hot path.
- **Memory Cap**: `trim()` releases vector memory if an anomalous log message caused scratch capacity to expand beyond 1 MB (`kScratchKeepBytes`).

### 2.2 SQL Struct Mapping (`RzSplit`)
`RzSplit` constructs DuckDB composite structs:
$$\text{STRUCT}(\text{template VARCHAR}, \text{ints BIGINT[]}, \text{strs VARCHAR[]})$$
- Elements are inserted directly into DuckDB's child vectors with zero intermediate string copies.

### 2.3 Community ABI Signing & Dual Linker Entrypoints
- `description.yml` conforms to `duckdb/community-extensions` standards (v1.1.2 metadata, Apache-2.0 license, explicit platform exclusions for WebAssembly and MinGW).
- `tools/AppendExtensionMetadata.py` appends the mandatory 512-byte footer trailer with header signature, platform string, target DuckDB C API version (`v1.2.0`), and ABI type `C_STRUCT`.
- Dual entrypoints: exports both `razobill_init_c_api` and `librazobill_init_c_api`, ensuring compatibility whether the user issues `LOAD razobill;` or `LOAD librazobill;`.

---

## 3. Python Ecosystem: `pyrazobill` (`lib/python/`)

### 3.1 GIL Release & Concurrency in `split_many`
- For batches larger than 128 items, `splitMany` in `PyRazobill.cpp` releases the Python GIL (`py::gil_scoped_release`) and dispatches work across `ThreadPool::parallelFor`.
- Scales linearly across 12+ CPU cores, reaching 29+ million lines/s.

### 3.2 Serial Input Copying Bottleneck (F-PERF-01)
- **Bottleneck Analysis**: Before the GIL is released, lines 318–343 perform a sequential, single-threaded copy:
  ```cpp
  std::vector<InputItem> rawInputs;
  rawInputs.reserve(total);
  for (size_t i = 0; i < total; ++i) {
      py::object item = inputs[i];
      if (py::isinstance<py::str>(item)) {
          PyObject* enc = PyUnicode_AsEncodedString(item.ptr(), "utf-8", "surrogateescape");
          rawInputs.push_back(InputItem{
              .bytes = std::string(PyBytes_AS_STRING(enc), PyBytes_GET_SIZE(enc)),
              .isBytes = false,
          });
          Py_DECREF(enc);
      }
  }
  ```
- **Architectural Impact**: For large inputs (e.g. 1M+ rows), 70–80% of total batch runtime is spent in this single-threaded initialization loop under the GIL, limiting multi-threaded speedup via Amdahl's Law.
- **Remediation**: Adopt chunked buffer streaming (e.g. 4,096 lines per task) or accept PyArrow RecordBatches directly to bypass Python object iteration under the GIL.

### 3.3 Surrogateescape Encoding Invariant
- Python bindings use `"surrogateescape"` error handling during decode and encode:
  `PyUnicode_DecodeUTF8(..., "surrogateescape")`
- Ensures that arbitrary binary byte sequences round-trip bit-for-bit through Python without throwing `UnicodeDecodeError`.

### 3.4 Pure-Python Fallback (`_fallback.py`)
- Provides a 100% pure-Python implementation of `RzSplit`, `RzJoin`, `RzTemplate`, and `RzHash` with bit-identical xxHash64 output.
- Automatically loaded if the compiled C++ extension (`Core.pyd` / `Core.so`) is absent.

---

## 4. OpenTelemetry Collector Go Processor (`ext/otel/`)

Processes log telemetry within the OpenTelemetry Collector pipeline:
- Invokes native C ABI via `cgo`.
- Maps decomposed components into OpenTelemetry log attributes:
  - `razobill.template`: Structural template.
  - `razobill.ints`: 64-bit integer slice.
  - `razobill.strs`: Variable string slice.
  - `razobill.formHash`: 64-bit xxHash partition key.
- **Identified Defects**:
  1. **Cgo Call Overhead**: For every log record in a batch, Go executes `C.CString`, `C.RzSplit`, `C.GoStringN`, and `C.RzFree`. At 500,000 logs/sec, multiple Cgo crossings per record limit throughput.
  2. **`DropRawBody` Attribute Omission**: When `DropRawBody == true`, the processor writes the template string to `rec.Body()` but omits the `razobill.template` attribute, breaking downstream attribute queries.
  3. **Signed Int64 Cast**: 64-bit uint hash wraps into negative numbers because OpenTelemetry attributes only support signed `int64`.

---

## 5. Vector VRL Transform (`ext/vector/`)

Provides a native Vector Remap Language (VRL) transform implemented in Rust.

### 5.1 Critical Key Discrepancy (F-EXT-01)
In `ext/vector/RazobillVrl.rs` line 111:
```rust
map.insert("form".to_string(), serde_json::Value::from(form_str));
map.insert("ints".to_string(), serde_json::Value::Array(ints));
map.insert("strs".to_string(), serde_json::Value::Array(strs));
```
However, the official documentation in `ext/vector/README.md` (lines 28–30) and `docs/how-to/vector.md` instructs users to write:
```yaml
parsed = razobill_split(.message)
.template = parsed.template
.ints = parsed.ints
.strs = parsed.strs
```
- **Severity: High.** A customer following the official documentation receives `null` for `.template`, silently dropping template data.
- **Remediation**: Update `RazobillVrl.rs` to insert `"template"` (and retain `"form"` as an alias for backwards compatibility).

---

## 6. Integration Test Harness Portability (`tests/Support.hpp`)

In `tests/Support.hpp` line 34:
```cpp
const char* forkeLib = "C:/Users/Forke/AppData/Roaming/Python/Python313/site-packages/_duckdb.cp313-win_amd64.pyd";
```
- **Portability Defect (F-PORT-01)**: Hardcoded local developer path breaks test execution on any other Windows workstation or CI runner without `RZB_DUCKDB_LIB`.
- **Remediation**: Remove `forkeLib` and rely strictly on standard system `duckdb.dll` or the `RZB_DUCKDB_LIB` environment variable.
