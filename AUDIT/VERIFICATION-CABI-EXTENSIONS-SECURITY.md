# Comprehensive Security, ABI, and Systems Verification Report
## Universal C ABI, Foreign Extension Adapters, and Build Architecture

**Auditor:** Senior Security & ABI Systems Auditor  
**Date:** 2026-10-17  
**Scope:** Universal C ABI (`librazobill`), Foreign Language Adapters (Rust VRL, Go OpenTelemetry, DuckDB C Extension), Test Harnesses, and CMake Build Architecture  
**Status:** Verification Completed — Vulnerabilities & Architectural Defects Identified  
**Mandate Compliance:** Strict Read-Only Verification; Zero Source Code Modifications Applied.

---

## 1. Executive Summary & Audit Matrix

A deep line-by-line verification was conducted across all 11 files comprising Razobill's Universal C ABI layer, foreign language bindings (Rust, Go, DuckDB), C API test suite, and CMake build configuration. Razobill implements a high-throughput, zero-copy log template decomposition and exact byte-for-byte reconstruction engine. 

While the core C++ data plane shows sophisticated design (thread-local buffer recycling, zero allocations on steady-state scalar paths, and strict UTF-8/byte preservation), critical defects were identified in FFI boundary defensive programming, memory retention trim logic, foreign language type marshaling, and cross-platform build target definitions.

### Summary Findings Matrix

| Finding ID | Severity | File & Line | Component | Category | Description |
|---|---|---|---|---|---|
| **SEC-01** | **High** | `src/RazobillC.cpp`:89-94 | C ABI (`RzJoin`) | Memory Safety / Data Corruption | Missing defensive null checks for `strLengths` and `strValues` when `strCount > 0`. Missing pointer null check when constructing `std::span` for integers. |
| **SEC-02** | **High** | `ext/duckdb/DuckdbExtension.cpp`:445-487 | DuckDB Extension Entrypoint | ABI / Exception Safety | `DUCKDB_EXTENSION_ENTRYPOINT` lacks a `try ... catch` boundary; C++ heap exceptions can escape across the `extern "C"` DuckDB ABI boundary. |
| **SEC-03** | **Medium** | `ext/duckdb/DuckdbExtension.cpp`:83-97 | DuckDB Extension Scratch | Memory Leak / Resource Retention | `Scratch::trim()` prunes `sc.ints` and `sc.strs` but fails to check and trim `sc.record.ints` and `sc.record.strs`, causing persistent heap retention after spike logs. |
| **SEC-04** | **Medium** | `ext/duckdb/DuckdbExtension.cpp`:284-332 | DuckDB Vectorized Join | Functional Bug / Query Abort | In varargs and list-element paths, encountering a `NULL` value skips pushing the slot instead of setting the row to NULL, causing unexpected `SlotUnderflow` query errors. |
| **SEC-05** | **Medium** | `ext/vector/RazobillVrl.rs`:48, 56-79 | Rust VRL Adapter | Undefined Behavior & Overhead | Unnecessary `CString::new` per log line (allocates + copies + rejects NUL bytes); unsafe `slice::from_raw_parts` invoked on null pointers if string or template length is 0. |
| **SEC-06** | **Medium** | `ext/otel/RazobillProcessor.go`:63, 71 | Go OpenTelemetry Processor | Observability / Heap Churn | `C.CString(msg)` per log line induces heavy C-heap allocation churn; `status != C.RZ_OK` failures are silently dropped without logs or metrics. |
| **SEC-07** | **Low** | `ext/otel/Config.go`:3-7 & `RazobillProcessor.go`:65 | Go OpenTelemetry Processor | Configuration Inconsistency | `Config.Mode` ("semantic" vs "exact") is dead configuration; `RzSplit` hardcodes `ScanMode::Semantic`. |
| **SEC-08** | **High** | `CMakeLists.txt`:99-135 | CMake Build Architecture | Linker Collision & Toolchain Incompatibility | Shared library target name collision on Windows between `librazobill` and `RazobillDuckdb`; hardcoded MinGW-specific linker flag `-Wl,--out-implib` breaks MSVC builds. |
| **SEC-09** | **Medium** | `CMakeLists.txt`:84, 99 | CMake Build Architecture | ODR / Linkage Anomaly | `src/RazobillC.cpp` is compiled directly into `RazobillDuckdb` AND simultaneously linked via `librazobill`, introducing duplicate symbols. |
| **SEC-10** | **Low** | `include/librazobill/RazobillC.h`:28-36 | C ABI Header | ABI / Linkage Inefficiency | Missing `__declspec(dllimport)` on Windows client side; missing explicit calling convention (`__cdecl`). |
| **SEC-11** | **Low** | `tests/UnitCAPI.cpp`:1-134 | Unit Test Suite | Test Coverage Gap | Missing tests for negative FFI scenarios (null arrays with non-zero counts, buffer capacity truncation, malformed input to `RzJoin`). |

---

## 2. Line-by-Line Verification: Universal C ABI

### 2.1 `include/librazobill/RazobillC.h`
The header establishes the C-compatible contract for librazobill.

- **Lines 1–21 (Architecture & Lifetimes):**
  - Accurately describes zero-copy lifetimes, lossless invertibility, buffer safety, and FFI conventions.
  - Specifically documents that `outRecord->strs` point directly into caller message buffer and caller must preserve the input message lifetime.
  - Documents that foreign language callers can manually populate an `RzRecord` with `internalHandle = NULL` to call `RzJoin`.
- **Lines 28–36 (Export Macro Definition):**
  ```c
  #if defined(_WIN32) || defined(__CYGWIN__)
  #if defined(LIBRAZOBILL_EXPORTS)
  #define RZ_API __declspec(dllexport)
  #else
  #define RZ_API
  #endif
  #else
  #define RZ_API __attribute__((visibility("default")))
  #endif
  ```
  - *Finding (SEC-10):* When client code includes `RazobillC.h` on Windows without defining `LIBRAZOBILL_EXPORTS`, `RZ_API` expands to nothing. For Windows dynamic linking (.dll), standard practice is `__declspec(dllimport)`. Omitting `dllimport` forces the MSVC compiler to emit indirect call thunks rather than direct import table lookups.
  - *Calling Convention:* No explicit calling convention (such as `__cdecl` or `RZ_CC`) is specified. While x86_64 has a single uniform ABI on Windows (Microsoft x64 ABI), 32-bit x86 targets default to different conventions depending on compiler flags (`__cdecl` vs `__stdcall`), which can cause stack imbalance if client and library differ.
- **Line 37 (`RZ_ABI_VERSION`):**
  - Correctly set to `1`, matching `LIBRAZOBILL_ABI_VERSION` in `Version.hpp`.
- **Lines 46–55 (`RzStatus` Enum):**
  - Values 0–7 explicitly map engine errors: `RZ_OK`, `RZ_ERR_INVALID_ARGUMENT`, `RZ_ERR_BUFFER_TOO_SMALL`, `RZ_ERR_MALFORMED_TEMPLATE`, `RZ_ERR_COUNT_MISMATCH`, `RZ_ERR_MESSAGE_TOO_LARGE`, `RZ_ERR_OUT_OF_MEMORY`, `RZ_ERR_INTERNAL`.
  - Underlying type in C is implementation-defined (`int`, 32-bit signed). Matches Rust `#[repr(C)]` enum definition.
- **Lines 60–69 (`RzRecord` Struct Layout):**
  ```c
  typedef struct RzRecord {
      const char* templateStr;
      size_t templateLength;
      const int64_t* intValues;
      size_t intCount;
      const char* const* strValues;
      const size_t* strLengths;
      size_t strCount;
      void* internalHandle;
  } RzRecord;
  ```
  - *Layout & Alignment:*
    - 64-bit platforms: Each field is 8 bytes.
    - `templateStr`: offset 0 (8B)
    - `templateLength`: offset 8 (8B)
    - `intValues`: offset 16 (8B)
    - `intCount`: offset 24 (8B)
    - `strValues`: offset 32 (8B)
    - `strLengths`: offset 40 (8B)
    - `strCount`: offset 48 (8B)
    - `internalHandle`: offset 56 (8B)
    - Total struct size = 64 bytes. Zero interior padding holes. Strict natural alignment of 8 bytes.
- **Lines 79–120 (Function Prototypes):**
  - `RzSplit`, `RzJoin`, `RzTemplate`, `RzFree`, `RzStatusMessage`, `RzVersion`.
  - All signatures use const correctness for input buffers and records.

---

### 2.2 `src/RazobillC.cpp`
Implements the Universal C ABI functions.

- **Lines 19–25 (`InternalRecord`):**
  ```cpp
  struct InternalRecord {
      razobill::core::SlotRecord slots;
      std::vector<const char*> strPointers;
      std::vector<size_t> strLengths;
  };
  ```
  - Encapsulates dynamic C++ memory so that the outer `RzRecord` exposes stable flat C arrays without exposing C++ vector objects across the ABI.
  - `strPointers` and `strLengths` mirror `slots.strs` views.
- **Lines 29–41 (`RzSplit` Entry & Empty Message Handling):**
  ```cpp
  RZ_API RzStatus RzSplit(const char* message, size_t messageLength, RzRecord* outRecord) {
      if (!outRecord) return RZ_ERR_INVALID_ARGUMENT;
      if (!message && messageLength > 0) return RZ_ERR_INVALID_ARGUMENT;

      std::memset(outRecord, 0, sizeof(RzRecord));

      if (messageLength == 0) {
          static const char emptyStr = '\0';
          outRecord->templateStr = &emptyStr;
          outRecord->templateLength = 0;
          return RZ_OK;
      }
  ```
  - *Verification of `messageLength == 0` path:*
    - `outRecord` is completely zeroed with `memset`.
    - `templateStr` points to static const `emptyStr` (non-null, valid pointer).
    - `internalHandle` remains `nullptr`.
    - *Is `RzFree` safe when `internalHandle == nullptr`?*
      Looking at line 160:
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
      YES: When `internalHandle` is `nullptr`, `delete` is skipped. `memset` clears `record` to zeros. The static constant `emptyStr` is never deallocated.
- **Lines 42–76 (`RzSplit` Core Parsing & Exception Safety):**
  - Parsing allocates `InternalRecord` using `std::make_unique`.
  - Maps internal status codes to `RzStatus`:
    - `Status::TooLarge` -> `RZ_ERR_MESSAGE_TOO_LARGE`
    - `Status::Malformed` -> `RZ_ERR_MALFORMED_TEMPLATE`
    - Default error -> `RZ_ERR_INVALID_ARGUMENT`
  - Exception boundary:
    ```cpp
    } catch (const std::bad_alloc&) {
        return RZ_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return RZ_ERR_INTERNAL;
    }
    ```
    Strictly wraps all execution. C++ exceptions cannot leak to foreign callers.
- **Lines 79–121 (`RzJoin` Implementation & Defensive Defect SEC-01):**
  Let us inspect lines 84–95:
  ```cpp
  const std::string_view tmplView(record->templateStr ? record->templateStr : "", record->templateLength);
  std::span<const int64_t> intSpan(record->intValues ? record->intValues : nullptr, record->intCount);

  std::vector<std::string_view> strViews;
  strViews.reserve(record->strCount);
  for (size_t i = 0; i < record->strCount; ++i) {
      const char* sPtr = record->strValues ? record->strValues[i] : nullptr;
      const size_t sLen = record->strLengths ? record->strLengths[i] : 0;
      if (!sPtr && sLen > 0) return RZ_ERR_INVALID_ARGUMENT;
      strViews.emplace_back(sPtr ? sPtr : "", sLen);
  }
  ```
  - *Detailed Breakdown of SEC-01:*
    1. **Undefined Behavior in `std::span` Constructor:**
       If an FFI caller provides `record->intCount > 0` but `record->intValues == nullptr`, `intSpan` is constructed as `std::span<const int64_t>(nullptr, record->intCount)`. Under C++20 standard [span.cons], constructing `std::span` with a null pointer and non-zero size has undefined behavior. In debug standard libraries, this triggers a runtime assertion failure. When `reconstruct` indexes `intSpan[k]`, it dereferences a null pointer.
       *Missing Check:* `if (!record->intValues && record->intCount > 0) return RZ_ERR_INVALID_ARGUMENT;`
    2. **Silent String Corruption when `strLengths` is Null:**
       If an FFI caller sets `record->strCount > 0` and populates `record->strValues`, but inadvertently leaves `record->strLengths == nullptr`:
       Line 91 executes: `const size_t sLen = record->strLengths ? record->strLengths[i] : 0;`.
       Because `record->strLengths` is null, `sLen` evaluates to `0`.
       Line 92: `!sPtr && sLen > 0` evaluates to `false` (since `sLen == 0`).
       Line 93: `strViews.emplace_back(sPtr ? sPtr : "", 0)` is pushed!
       All string variables are silently replaced with empty strings (`""`)! The reconstructed message is silently corrupted without returning an error code.
    3. **Missing Strict Array Validation:**
       If `record->strCount > 0`, both `record->strValues` and `record->strLengths` MUST be non-null. If either is null, the function must immediately return `RZ_ERR_INVALID_ARGUMENT`.
- **Lines 105–115 (Buffer Capacity Validation in `RzJoin`):**
  ```cpp
  const size_t reqLen = result.size();
  if (outLength) *outLength = reqLen;

  if (!outBuffer || bufferCapacity < reqLen) {
      return RZ_ERR_BUFFER_TOO_SMALL;
  }

  if (reqLen > 0) {
      std::memcpy(outBuffer, result.data(), reqLen);
  }
  return RZ_OK;
  ```
  - Correct capacity probe behavior: If `outBuffer == nullptr` or `bufferCapacity < reqLen`, `*outLength` receives the required length and `RZ_ERR_BUFFER_TOO_SMALL` is returned.
  - Zero trailing NUL byte written, adhering strictly to specification.
- **Lines 123–158 (`RzTemplate` Implementation):**
  - *Probing Inconsistency on Empty String:*
    Lines 128–131:
    ```cpp
    if (messageLength == 0) {
        if (outLength) *outLength = 0;
        return RZ_OK;
    }
    ```
    If a caller calls `RzTemplate("", 0, nullptr, 0, &outLen)` to probe capacity, it returns `RZ_OK` with `outLen = 0`. In contrast, `RzJoin` with an empty record and `outBuffer == nullptr` returns `RZ_ERR_BUFFER_TOO_SMALL`.
  - *Allocation Overhead:*
    `RzTemplate` calls `razobill::core::parse(msgView, slots, ScanMode::Semantic)`. This parses all integer tokens and string tokens into `slots.ints` and `slots.strs` before copying `slots.templateStr`. A direct template extraction scanner or calling `core::templateOf` would eliminate the token vector allocations.

## 3. Line-by-Line Verification: DuckDB C Extension (`ext/duckdb/DuckdbExtension.cpp`)

The DuckDB extension implements vectorized scalar functions: `RzSplit`, `RzJoin`, `RzTemplate`, and `RzHash`.

- **Lines 1–26 (Architecture & Inclusions):**
  - Accurately details vector-at-a-time execution (typically 2,048 rows/chunk) and zero intermediate string allocation.
  - Includes stable C extension API header `duckdb_extension.h` and internal `librazobill/Slots.hpp`.
- **Lines 32–81 (Constants, Kind Enum, Classifier):**
  - `kScratchKeepBytes = 1 << 20` (1 MiB).
  - `Kind` enum classifies DuckDB logical types (integers 8-bit to 64-bit signed/unsigned, varchar, blob, list of integers, list of strings).
  - `classify()` properly destroys logical child types via `duckdb_destroy_logical_type(&elem)` on line 69, avoiding type memory leaks during schema introspection.
- **Lines 83–97 (Scratch Structure & Memory Trim Logic SEC-03):**
  ```cpp
  struct Scratch {
      core::SlotRecord record;
      std::string text;
      std::vector<std::int64_t> ints;
      std::vector<std::string_view> strs;
      std::vector<Arg> args;

      void trim() {
          if (text.capacity() > kScratchKeepBytes) std::string().swap(text);
          if (record.skeleton.capacity() > kScratchKeepBytes) std::string().swap(record.skeleton);
          if (record.templateStr.capacity() > kScratchKeepBytes) std::string().swap(record.templateStr);
          if (ints.capacity() * sizeof(std::int64_t) > kScratchKeepBytes) std::vector<std::int64_t>().swap(ints);
          if (strs.capacity() * sizeof(std::string_view) > kScratchKeepBytes) std::vector<std::string_view>().swap(strs);
      }
  };
  ```
  - *Detailed Breakdown of SEC-03:*
    1. Look at `record`: it is an instance of `core::SlotRecord`.
    2. `core::SlotRecord` contains:
       `std::string templateStr;`
       `std::vector<std::int64_t> ints;`
       `std::vector<std::string_view> strs;`
       `std::string skeleton;`
       `std::string types;`
    3. Notice: `trim()` checks `record.skeleton` and `record.templateStr`.
    4. But `trim()` checks `ints` and `strs` (which are `sc.ints` and `sc.strs`, the vectors used exclusively in `RzJoinImpl`)!
    5. It NEVER checks `record.ints` or `record.strs`!
    6. In `RzSplitImpl`, parsing massive log entries with high slot cardinality will cause `sc.record.ints` and `sc.record.strs` to expand. Because `trim()` never inspects `record.ints.capacity()` or `record.strs.capacity()`, that capacity remains permanently allocated across all worker threads.
- **Lines 99–102 (Thread-Local Concurrency):**
  ```cpp
  Scratch& scratch() {
      thread_local Scratch s;
      return s;
  }
  ```
  - Concurrency design is completely lock-free and thread-safe. DuckDB worker threads execute chunks independently; each thread receives its own dedicated `Scratch` instance.
- **Lines 108–120 (Vector Validity & String Helpers):**
  - `isValid(validity, row)`: accurately handles null validity masks (`validity == nullptr` implies all rows valid) and queries `duckdb_validity_row_is_valid`.
  - `setNull(out, row)`: ensures validity is writable via `duckdb_vector_ensure_validity_writable` before marking invalid.
  - `put(out, row, sv)`: uses `duckdb_vector_assign_string_element_len` for zero-copy string assignment into DuckDB's string heap.
- **Lines 147–170 (`RzTemplateImpl`):**
  - Iterates rows in data chunk.
  - If row is null, calls `setNull(out, r)`.
  - Empty string maps to empty output.
  - On parse error, calls `fail(info, "RzTemplate", s, sv)` and breaks chunk execution.
  - Calls `sc.trim()` upon completion.
- **Lines 173–236 (`RzSplitImpl`):**
  - Returns a STRUCT with three fields: `template` (VARCHAR/BLOB), `ints` (LIST of BIGINT), `strs` (LIST of VARCHAR/BLOB).
  - Handles null inputs at line 194:
    ```cpp
    if (!isValid(vv, r)) {
        setNull(tmplChild, r);
        setNull(intsChild, r);
        setNull(strsChild, r);
        intsEntries[r] = {totalInts, 0};
        strsEntries[r] = {totalStrs, 0};
        setNull(out, r);
        continue;
    }
    ```
    Setting both outer struct and inner child validity masks, while recording 0-length list entries, adheres to DuckDB's vector format specifications.
  - Reserves list vector capacity dynamically with `duckdb_list_vector_reserve`.
  - Calls `duckdb_list_vector_set_size` at the end to guarantee list length validity.
- **Lines 239–350 (`RzJoinImpl` & Functional Defect SEC-04):**
  - Lines 265–278: Standard three-argument invocation: `RzJoin(template, ints_list, strs_list)`.
    If either `template`, `ints_list`, or `strs_list` is NULL, it calls `setNull(out, r); continue;`. This correctly outputs SQL NULL.
  - *Detailed Breakdown of SEC-04:*
    Look at lines 283–336 (varargs and list-element decoding):
    ```cpp
    for (const auto& a : sc.args) {
        if (!isValid(a.validity, r)) continue;
        ...
        case Kind::IntList: {
            const auto entry = static_cast<const duckdb_list_entry*>(a.data)[r];
            const auto* p = static_cast<const std::int64_t*>(a.childData);
            for (idx_t i = 0; i < entry.length; ++i) {
                if (isValid(a.childValidity, entry.offset + i)) {
                    sc.ints.push_back(p[entry.offset + i]);
                }
            }
            break;
        }
    ```
    1. **Varargs Null Handling:**
       In the varargs version (`RzJoin(template, arg1, arg2, ...)`), if any column is NULL for row `r`, `if (!isValid(a.validity, r)) continue;` simply skips adding that parameter to `sc.ints` or `sc.strs`.
       Consequently, `sc.ints` or `sc.strs` will have fewer elements than the template contains placeholders.
       When `core::reconstruct` is called at line 343, it returns `Status::SlotUnderflow`.
       Line 344 calls `fail(info, "RzJoin", s, tmplSv)`, which sets a fatal query error and terminates query processing!
       *Correct Behavior:* If an input argument to `RzJoin` is NULL, the entire result for that row should be NULL (`setNull(out, r); continue;`), rather than crashing the SQL query.
    2. **Null Elements in Lists:**
       In `Kind::IntList` and `Kind::StrList`, if a list element is invalid (`!isValid(a.childValidity, ...)`), it is silently skipped! This likewise desynchronizes slot counts, triggering `SlotUnderflow` and aborting the query.
- **Lines 353–366 (`RzHashImpl`):**
  - Computes 64-bit XXH64 hash of input strings using `core::xxh64`.
  - Respects null validity.
- **Lines 368–377 (Exception Boundary Wrapper):**
  ```cpp
  template <void (*Fn)(duckdb_function_info, duckdb_data_chunk, duckdb_vector)>
  void guarded(duckdb_function_info info, duckdb_data_chunk in, duckdb_vector out) {
      try {
          Fn(info, in, out);
      } catch (const std::exception& ex) {
          duckdb_scalar_function_set_error(info, ex.what());
      } catch (...) {
          duckdb_scalar_function_set_error(info, "razobill: unexpected exception");
      }
  }
  ```
  - All scalar functions registered in DuckDB are wrapped with `guarded<Fn>`.
  - Any C++ exception thrown during scalar execution is caught and converted to a DuckDB scalar error via `duckdb_scalar_function_set_error`. No C++ exceptions escape scalar callbacks.
- **Lines 445–487 (`DUCKDB_EXTENSION_ENTRYPOINT` Defect SEC-02):**
  ```cpp
  DUCKDB_EXTENSION_ENTRYPOINT(duckdb_connection connection, duckdb_extension_info, struct duckdb_extension_access*) {
      Type varchar(DUCKDB_TYPE_VARCHAR), blob(DUCKDB_TYPE_BLOB), bigint(DUCKDB_TYPE_BIGINT), ubigint(DUCKDB_TYPE_UBIGINT);
      Type any(DUCKDB_TYPE_ANY);
      Type bigintList = Type::listOf(bigint), varcharList = Type::listOf(varchar), blobList = Type::listOf(blob);

      std::vector<const char*> structNames = {"template", "ints", "strs"};
      std::vector<duckdb_logical_type> structTypesVarchar = {varchar.t, bigintList.t, varcharList.t};
      std::vector<duckdb_logical_type> structTypesBlob = {blob.t, bigintList.t, blobList.t};
      Type splitStructVarchar = Type::structOf(structNames, structTypesVarchar);
      Type splitStructBlob = Type::structOf(structNames, structTypesBlob);

      bool ok = true;
      ...
      return ok;
  }
  ```
  - *Detailed Breakdown of SEC-02:*
    The entrypoint function `razobill_init_c_api` is declared with C linkage (`extern "C"`).
    However, the function body performs multiple C++ operations that can throw:
    - `std::vector` construction and heap allocation for `structNames`, `structTypesVarchar`, `structTypesBlob`.
    - `FunctionSet` object creations with internal dynamic allocations.
    - `Type` helper object movements.
    There is NO `try ... catch` block surrounding the entrypoint!
    If `std::bad_alloc` or any other exception is thrown during extension loading, the C++ exception will unwind across the `extern "C"` boundary into DuckDB's C host process, which causes immediate process termination (`std::terminate`) via the C runtime ABI violation.
- **Lines 490–495 (Dual-Entrypoint Alias):**
  - Correctly exports `librazobill_init_c_api` alongside `razobill_init_c_api`, allowing loading via both `LOAD razobill;` and `LOAD librazobill;`.

## 4. Line-by-Line Verification: Rust VRL Adapter (`ext/vector/`)

### 4.1 `ext/vector/RazobillVrl.rs`
Provides a native Vector Remap Language (VRL) binding for librazobill.

- **Lines 9–20 (`RzRecord` Struct Definition):**
  ```rust
  #[repr(C)]
  #[derive(Debug)]
  pub struct RzRecord {
      pub template_str: *const c_char,
      pub template_length: usize,
      pub int_values: *const i64,
      pub int_count: usize,
      pub str_values: *const *const c_char,
      pub str_lengths: *const usize,
      pub str_count: usize,
      pub internal_handle: *mut std::ffi::c_void,
  }
  ```
  - Matches the exact field names, data types, and ordering of `struct RzRecord` in `RazobillC.h`.
  - `#[repr(C)]` guarantees identical C struct memory layout and alignment.
- **Lines 23–33 (`RzStatus` Enum Definition):**
  - `#[repr(C)]` enum matches C enumeration values 0–7.
- **Lines 35–43 (FFI Bindings):**
  - Declares foreign C functions `RzSplit` and `RzFree`.
- **Lines 46–92 (`parse_razobill` & Vulnerabilities in SEC-05):**
  Let us inspect lines 46–55:
  ```rust
  pub fn parse_razobill(raw_msg: &str) -> Result<BTreeMap<String, serde_json::Value>, String> {
      let mut record: RzRecord = unsafe { std::mem::zeroed() };
      let c_str = CString::new(raw_msg).map_err(|e| e.to_string())?;

      let status = unsafe { RzSplit(c_str.as_ptr(), raw_msg.len(), &mut record) };

      if !matches!(status, RzStatus::Ok) {
          return Err("Razobill decomposition failed".to_string());
      }
  ```
  - *Detailed Breakdown of SEC-05:*
    1. **Unnecessary Allocation & NUL Rejection:**
       `CString::new(raw_msg)` scans `raw_msg` for interior `\0` bytes and allocates a new buffer on the heap with a trailing NUL byte.
       However, `RzSplit` accepts an explicit length (`raw_msg.len()`) and `RazobillC.h` explicitly documents that NUL termination is NOT required.
       In high-throughput Vector ingest pipelines (e.g. 50k–200k logs/sec), this creates heap allocation and memory copying overhead on every log message.
       Furthermore, if a log contains an embedded NUL byte (common in binary syslog or truncated payloads), `CString::new` returns an error, failing legitimate log records.
       *Remediation:* Call `RzSplit(raw_msg.as_ptr() as *const c_char, raw_msg.len(), &mut record)` directly with zero allocations.
    2. **Rust Undefined Behavior in `slice::from_raw_parts`:**
       Lines 56–59:
       ```rust
       let form_slice = unsafe {
           std::slice::from_raw_parts(record.template_str as *const u8, record.template_length)
       };
       ```
       In Rust, the safety contract of `std::slice::from_raw_parts(ptr, len)` states:
       *"The pointer must be non-null and aligned, even for zero-length slices."*
       If `record.template_str` is null (for instance, on an empty record or if an error condition arose), calling `from_raw_parts(ptr, 0)` is immediate Undefined Behavior in Rust.
       *Remediation:* Validate `!record.template_str.is_null() && record.template_length > 0` before invoking `from_raw_parts`.
    3. **Unsafe String Slice Reconstruction:**
       Lines 70–80:
       ```rust
       for i in 0..record.str_count {
           let slice =
               unsafe { std::slice::from_raw_parts(str_ptrs[i] as *const u8, str_lens[i]) };
           strs.push(serde_json::Value::from(
               String::from_utf8_lossy(slice).to_string(),
           ));
       }
       ```
       If a string slot is empty (`str_lens[i] == 0`) and `str_ptrs[i]` is null, calling `from_raw_parts(null, 0)` is UB.
    4. **Lifetime and Drop Ordering:**
       `RzFree(&mut record)` is called on line 83 AFTER slices have been converted to owned Rust `String` values (`String::from_utf8_lossy(...).to_string()`). This guarantees that no use-after-free occurs when `internalHandle` is deallocated by `RzFree`.

---

### 4.2 `ext/vector/Cargo.toml` & `ext/vector/build.rs`
- **`Cargo.toml`:**
  - Package `razobill-vector` configured as both `lib` and `cdylib`.
  - Dependencies: `serde` and `serde_json`.
- **`build.rs`:**
  - Hardcoded build directory path:
    ```rust
    let build_dir = manifest_dir.parent().unwrap().parent().unwrap().join("build");
    ```
    Assumes CMake build outputs are strictly in `../../build`. If an out-of-tree build directory is used (e.g. `build_release`, `target`), Cargo build will fail to link.
  - Copies `razobill.dll` on Windows to the Cargo target directory and `deps` folder for unit testing discovery.
  - Missing `-Wl,-rpath` on Linux/macOS: When running `cargo test` on Unix systems, dynamic linking will fail at runtime unless `LD_LIBRARY_PATH` is manually populated.

## 5. Line-by-Line Verification: OpenTelemetry Collector Processor (`ext/otel/`)

### 5.1 `ext/otel/RazobillProcessor.go`
Implements an OpenTelemetry Collector log processor component using cgo.

- **Lines 1–10 (Cgo Directives):**
  ```go
  /*
  #cgo CFLAGS: -I../../include
  #cgo LDFLAGS: -L../../build -lrazobill -lstdc++
  #include "librazobill/RazobillC.h"
  #include <stdlib.h>
  */
  import "C"
  ```
  - Linker flag `-lstdc++` is GCC-specific. On macOS (Clang), this requires `-lc++`.
  - Assumes librazobill is located in `../../build`.
- **Lines 21–47 (Component Lifecycle & Capabilities):**
  - Defines `RazobillProcessor` holding `*Config` and `consumer.Logs`.
  - Declares `Capabilities{MutatesData: true}` properly since logs are mutated in-place.
- **Lines 49–70 (Log Batch Processing & Cgo Churn SEC-06):**
  ```go
  rec := records.At(k)
  msg := rec.Body().AsString()
  if len(msg) == 0 {
      continue
  }

  var cRecord C.RzRecord
  cMsg := C.CString(msg)

  status := C.RzSplit(
      cMsg,
      C.size_t(len(msg)),
      &cRecord,
  )
  ```
  - *Detailed Breakdown of SEC-06:*
    1. **C-Heap Churn:**
       `C.CString(msg)` invokes C `malloc` and `memcpy` for every log message in the OpenTelemetry stream. At high throughput (100,000 logs/sec), this creates severe C-heap fragmentation and allocation latency, negating Go's native GC efficiency.
       Because `RzSplit` does not require NUL-termination, `cMsg` can be passed using zero-allocation unsafe pointers: `unsafe.Pointer(unsafe.StringData(msg))` in Go 1.20+ (with string pinned during synchronous cgo invocation).
    2. **Silent Failure / Observability Black Hole:**
       Lines 71–109:
       ```go
       if status == C.RZ_OK {
           ...
           C.RzFree(&cRecord)
       }
       C.free(unsafe.Pointer(cMsg))
       ```
       If `status != C.RZ_OK` (e.g. malformed template, message exceeds 16 MB limit, or out of memory):
       - The error is silently swallowed.
       - The processor does not log any warning or error (the struct does not even maintain an instance of `zap.Logger`).
       - No OpenTelemetry metric or counter (such as `processor_razobill_failed_total`) is incremented.
       - The log record is passed down to the next consumer in its unmodified, raw form without any diagnostic indicator.
- **Lines 86–105 (Slice Casting Safety):**
  ```go
  intPtrs := (*[1 << 28]C.int64_t)(unsafe.Pointer(cRecord.intValues))[:cRecord.intCount:cRecord.intCount]
  ...
  strPtrs := (*[1 << 28]*C.char)(unsafe.Pointer(cRecord.strValues))[:cRecord.strCount:cRecord.strCount]
  strLens := (*[1 << 28]C.size_t)(unsafe.Pointer(cRecord.strLengths))[:cRecord.strCount:cRecord.strCount]
  ```
  - Slicing through a fixed-size pointer `(*[1 << 28]T)` is an archaic idiom. In Go 1.17+, standard library `unsafe.Slice(ptr, len)` is the supported, type-safe mechanism.
  - On 32-bit platforms, `1 << 28` elements of `int64` is 2 GiB, exceeding the addressable array limit and failing compilation.
  - If `cRecord.intCount > 0` but `cRecord.intValues == nil`, casting a nil pointer to `(*[1<<28]T)(nil)[:count:count]` triggers an immediate runtime panic.

---

### 5.2 `ext/otel/Config.go` & `ext/otel/Factory.go` (Defect SEC-07)
- **`Config.go`:**
  ```go
  type Config struct {
      Mode        string `mapstructure:"mode"`
      DropRawBody bool   `mapstructure:"drop_raw_body"`
      IncludeHash bool   `mapstructure:"include_hash"`
  }
  ```
  - *Detailed Breakdown of SEC-07:*
    `Config` exposes `Mode: "semantic"` or `"exact"`, and validates it in `Validate()`.
    However, `RazobillProcessor.go` line 65 calls `C.RzSplit(cMsg, C.size_t(len(msg)), &cRecord)`.
    The Universal C API `RzSplit` does not accept a mode parameter; in `src/RazobillC.cpp`, it unconditionally hardcodes `razobill::core::ScanMode::Semantic`.
    Consequently, the `mode` configuration setting is completely inert and does nothing.
- **`Factory.go`:**
  - Standard OpenTelemetry Collector factory implementation using `processor.NewFactory`.
  - Registers `component.StabilityLevelBeta`.

## 6. Line-by-Line Verification: Test Harness & Build Architecture

### 6.1 `tests/UnitCAPI.cpp` (Test Suite & Gaps SEC-11)
Verifies the Universal C ABI functions via doctest.

- **Lines 9–51 (`Universal C ABI: RzSplit and RzJoin roundtrip`):**
  - Tests decomposition and reconstruction of SSH log message: `"Failed password for root from 192.168.1.1 port 22"`.
  - Verifies extracted template: `"Failed password for {s} from {s} port {i}"`.
  - Verifies extracted integer: `22`.
  - Verifies extracted strings count: `2` (`"root"`, `"192.168.1.1"`).
  - Verifies capacity probing: `RzJoin(&record, nullptr, 0, &requiredLen) == RZ_ERR_BUFFER_TOO_SMALL`.
  - Verifies exact-capacity join without null-terminator requirement: `RzJoin(&record, outBuf.data(), outBuf.size(), &outLen) == RZ_OK`.
  - Verifies `RzTemplate` outputs identical template string.
  - Verifies `RzFree` zeroes `record.templateStr` and `record.internalHandle`.
- **Lines 53–75 (`Universal C ABI: Literal brace escaping in templates`):**
  - Tests escaping of literal braces in messages containing JSON-like tokens: `"Payload {action: login} port=443 {status}"`.
  - Verifies template escaping to `"{{"` and `"}}"`.
  - Confirms 100% roundtrip byte-exact fidelity.
- **Lines 77–104 (`Universal C ABI: FFI Caller manually populating record for join`):**
  - Verifies foreign language caller flow where an `RzRecord` is allocated on stack with `internalHandle = nullptr`.
  - Confirms `RzJoin` reconstructs message properly from raw arrays.
  - Confirms `RzFree` is safe to call on non-allocated FFI records.
- **Lines 106–134 (`Universal C ABI: Edge cases, empty string, errors and metadata`):**
  - Tests empty message: `RzSplit("", 0, &emptyRecord)`.
  - Tests null argument rejection on `RzSplit` and `RzJoin`.
  - Verifies `RzStatusMessage` descriptions.
  - Verifies `RzVersion() == "1.1.2"`.
- *Finding (SEC-11 - Test Gaps):*
  - No unit tests for negative FFI record states:
    - `intCount > 0` with `intValues == nullptr`.
    - `strCount > 0` with `strLengths == nullptr` (which triggers SEC-01 silent data corruption).
    - `strCount > 0` with `strValues == nullptr`.
  - No unit tests for undersized buffer truncation prevention (verifying no buffer overflow when `bufferCapacity < reqLen`).
  - No tests for `RzFree(nullptr)` null-pointer safety or double `RzFree` calls.

---

### 6.2 `CMakeLists.txt` (Build Architecture SEC-08 & SEC-09)
The project CMake configuration controls compiler flags, sanitizer definitions, library targets, and packaging.

- **Lines 1–18 (Standards & Options):**
  - `cmake_minimum_required(VERSION 3.20)`
  - Strict C++20 requirement: `CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`.
  - `CMAKE_POSITION_INDEPENDENT_CODE ON` correctly enabled for all targets.
- **Lines 35–74 (Compiler Warnings & Sanitizers):**
  - MSVC Flags: `/W4 /wd4996`
  - GCC/Clang Flags: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wnon-virtual-dtor -Woverloaded-virtual`
  - Sanitizer Options:
    - `ENABLE_SANITIZER_ASAN_UBSAN`: On GCC/Clang adds `-fsanitize=address,undefined -fno-omit-frame-pointer -g`.
      Under MSVC, correctly configures `/fsanitize=address`, but note that MSVC does not support UndefinedBehaviorSanitizer (`undefined`).
    - `ENABLE_SANITIZER_TSAN`: Configures `-fsanitize=thread`. Correctly skipped on MSVC (TSan not supported on MSVC).
- **Lines 76–98 (`librazobill` Core Target):**
  ```cmake
  add_library(librazobill
    src/Slots.cpp
    src/Stats.cpp
    src/Channel.cpp
    src/Dialect.cpp
    src/BackendAdbc.cpp
    src/Razobill.cpp
    src/RazobillC.cpp
  )
  set_target_properties(librazobill PROPERTIES OUTPUT_NAME razobill ARCHIVE_OUTPUT_NAME razobill)
  ```
- **Lines 99–135 (`RazobillDuckdb` Target & Defects SEC-08 / SEC-09):**
  ```cmake
  add_library(RazobillDuckdb SHARED ext/duckdb/DuckdbExtension.cpp src/RazobillC.cpp)
  ...
  target_link_libraries(RazobillDuckdb PRIVATE librazobill)
  if(WIN32)
    target_link_options(RazobillDuckdb PRIVATE "-Wl,--out-implib,${CMAKE_CURRENT_BINARY_DIR}/razobill.lib")
    set_target_properties(RazobillDuckdb PROPERTIES
      PREFIX ""
      OUTPUT_NAME razobill
      ARCHIVE_OUTPUT_NAME razobill_duckdb
      ...
    )
  ```
  - *Detailed Breakdown of SEC-08 (Target Collision & MSVC Failure):*
    1. **DLL Target Collision on Windows:**
       If `BUILD_SHARED_LIBS=ON` is set, `librazobill` is compiled as a shared library with `OUTPUT_NAME razobill`, outputting `razobill.dll`.
       Simultaneously, `RazobillDuckdb` is explicitly `SHARED` with `OUTPUT_NAME razobill` and `PREFIX ""`, which ALSO outputs `razobill.dll` into `${CMAKE_CURRENT_BINARY_DIR}`!
       Both targets write to the same output file `razobill.dll`, resulting in binary overwrite, race conditions, and corrupted builds.
    2. **MSVC Linker Incompatibility:**
       Line 112 specifies: `"-Wl,--out-implib,${CMAKE_CURRENT_BINARY_DIR}/razobill.lib"`.
       The `-Wl,...` syntax is specific to GNU/Clang MinGW ld linkers. Under MSVC (`cl.exe` / `link.exe`), `-Wl` is not a recognized compiler/linker option and causes a fatal build failure (`LINK : fatal error LNK1181` or unrecognized option).
       MSVC requires `/IMPLIB:...`.
       *Remediation:* Wrap with `if(WIN32 AND MINGW)` or use CMake's native `ARCHIVE_OUTPUT_NAME` property.
  - *Detailed Breakdown of SEC-09 (ODR / Duplicate Symbol Violation):*
    Line 99 includes `src/RazobillC.cpp` as a direct source of `RazobillDuckdb`.
    Line 110 links `RazobillDuckdb` against `librazobill`.
    However, `librazobill` ALREADY includes `src/RazobillC.cpp` at line 84!
    Compiling `src/RazobillC.cpp` into the shared library while linking a static archive containing the identical compiled object violates the One Definition Rule (ODR) and causes duplicate symbol warnings or linkage anomalies.
- **Lines 227–233 (Reference to `bench/BenchStorage.cpp`):**
  - `BenchStorage` executable is defined and linked against `librazobill`.
  - Verified: Source file `bench/BenchStorage.cpp` exists, correctly benchmarking DuckDB ADBC columnar compaction.
- **Lines 246–306 (Installation & Packaging Exports):**
  - `librazobillTargets` export and `librazobillConfig.cmake` generation adhere to modern CMake package standards.
  - Public headers in `include/librazobill` correctly installed to `include`.
  - DuckDB extension installed to `share/razobill/extensions`.
  - `librazobill.pc` correctly generated for pkg-config integration.

## 7. Recommended Defensive Patches & Action Plan

*(Note: Per auditor mandate, NO code files have been modified. The following snippets are provided as reference specifications for implementation teams).*

### 7.1 Defensive Null Checking in `src/RazobillC.cpp` (Remediates SEC-01)
```cpp
// In RzJoin:
if (!record) return RZ_ERR_INVALID_ARGUMENT;
if (!record->templateStr && record->templateLength > 0) return RZ_ERR_INVALID_ARGUMENT;

// STRICT DEFENSIVE ARRAYS VALIDATION:
if (record->intCount > 0 && !record->intValues) {
    return RZ_ERR_INVALID_ARGUMENT;
}
if (record->strCount > 0 && (!record->strValues || !record->strLengths)) {
    return RZ_ERR_INVALID_ARGUMENT;
}

try {
    const std::string_view tmplView(record->templateStr ? record->templateStr : "", record->templateLength);
    std::span<const int64_t> intSpan(record->intValues, record->intCount);

    std::vector<std::string_view> strViews;
    strViews.reserve(record->strCount);
    for (size_t i = 0; i < record->strCount; ++i) {
        const char* sPtr = record->strValues[i];
        const size_t sLen = record->strLengths[i];
        if (!sPtr && sLen > 0) return RZ_ERR_INVALID_ARGUMENT;
        strViews.emplace_back(sPtr ? sPtr : "", sLen);
    }
    ...
```

### 7.2 Exception Boundary in `ext/duckdb/DuckdbExtension.cpp` (Remediates SEC-02)
```cpp
DUCKDB_EXTENSION_ENTRYPOINT(duckdb_connection connection, duckdb_extension_info info, struct duckdb_extension_access* access) {
    try {
        // ... existing type setup and registration ...
        return ok;
    } catch (const std::exception& ex) {
        return false;
    } catch (...) {
        return false;
    }
}
```

### 7.3 Scratch Memory Retention Trim in `ext/duckdb/DuckdbExtension.cpp` (Remediates SEC-03)
```cpp
void trim() {
    if (text.capacity() > kScratchKeepBytes) std::string().swap(text);
    if (record.skeleton.capacity() > kScratchKeepBytes) std::string().swap(record.skeleton);
    if (record.templateStr.capacity() > kScratchKeepBytes) std::string().swap(record.templateStr);
    if (record.ints.capacity() * sizeof(std::int64_t) > kScratchKeepBytes) std::vector<std::int64_t>().swap(record.ints);
    if (record.strs.capacity() * sizeof(std::string_view) > kScratchKeepBytes) std::vector<std::string_view>().swap(record.strs);
    if (ints.capacity() * sizeof(std::int64_t) > kScratchKeepBytes) std::vector<std::int64_t>().swap(ints);
    if (strs.capacity() * sizeof(std::string_view) > kScratchKeepBytes) std::vector<std::string_view>().swap(strs);
}
```

### 7.4 Zero-Allocation String Passing in `ext/vector/RazobillVrl.rs` (Remediates SEC-05)
```rust
pub fn parse_razobill(raw_msg: &str) -> Result<BTreeMap<String, serde_json::Value>, String> {
    let mut record: RzRecord = unsafe { std::mem::zeroed() };

    let status = unsafe {
        RzSplit(
            raw_msg.as_ptr() as *const c_char,
            raw_msg.len(),
            &mut record,
        )
    };

    if !matches!(status, RzStatus::Ok) {
        return Err("Razobill decomposition failed".to_string());
    }

    let form_str = if !record.template_str.is_null() && record.template_length > 0 {
        let form_slice = unsafe {
            std::slice::from_raw_parts(record.template_str as *const u8, record.template_length)
        };
        String::from_utf8_lossy(form_slice).to_string()
    } else {
        String::new()
    };
    // ...
```

### 7.5 Status Checking & Logger in `ext/otel/RazobillProcessor.go` (Remediates SEC-06)
```go
if status == C.RZ_OK {
    // Process attrs ...
    C.RzFree(&cRecord)
} else {
    // Record metric and warn
    p.logger.Warn("Razobill decomposition failed",
        zap.Int("status", int(status)),
        zap.String("status_msg", C.GoString(C.RzStatusMessage(status))),
    )
    p.telemetry.DecomposeErrors.Add(ctx, 1)
}
```

### 7.6 Cross-Platform MinGW/MSVC Linker Fix in `CMakeLists.txt` (Remediates SEC-08 & SEC-09)
```cmake
# Remove src/RazobillC.cpp from RazobillDuckdb sources:
add_library(RazobillDuckdb SHARED ext/duckdb/DuckdbExtension.cpp)

# Use MSVC-compatible import library generation:
if(WIN32)
  if(MINGW)
    target_link_options(RazobillDuckdb PRIVATE "-Wl,--out-implib,${CMAKE_CURRENT_BINARY_DIR}/razobill.lib")
  endif()
  # Set separate output names to avoid collision with librazobill when BUILD_SHARED_LIBS=ON
  set_target_properties(RazobillDuckdb PROPERTIES
    PREFIX ""
    OUTPUT_NAME razobill_duckdb
    ARCHIVE_OUTPUT_NAME razobill_duckdb
  )
endif()
```

---

## 8. Conclusion & Sign-Off

The Razobill Universal C ABI and its foreign extension adapters demonstrate exceptional low-level architectural rigor, high throughput data-plane design, and strict memory safety when operated along standard paths. 

However, defensive validation at the FFI boundary (`RzJoin`), DuckDB memory trim completeness, entrypoint exception safety, and build target names require the targeted hardening detailed above prior to enterprise deployment.

**Verification Completed By:** Senior Security & ABI Systems Auditor  
**Artifact Written To:** `C:\FM\rzd\razobill\AUDIT\VERIFICATION-CABI-EXTENSIONS-SECURITY.md`  
**Compliance Statement:** Verified line-by-line across all 11 domain files; zero code modifications performed.
