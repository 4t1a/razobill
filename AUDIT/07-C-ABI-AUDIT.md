# 07 — C ABI Surface Audit

**Scope**: `RazobillC.h`, `RazobillC.cpp`, `Version.hpp`

---

## 1. ABI Surface Inventory

| Symbol | Signature | Status |
|:-------|:----------|:-------|
| `RzSplit` | `(const char*, size_t, RzRecord*) → RzStatus` | ✅ Exported |
| `RzJoin` | `(const RzRecord*, char*, size_t, size_t*) → RzStatus` | ✅ Exported |
| `RzTemplate` | `(const char*, size_t, char*, size_t, size_t*) → RzStatus` | ✅ Exported |
| `RzFree` | `(RzRecord*) → void` | ✅ Exported |
| `RzStatusMessage` | `(RzStatus) → const char*` | ✅ Exported |
| `RzVersion` | `(void) → const char*` | ✅ Exported |
| `RZ_ABI_VERSION` | `#define 1` | ✅ Defined |

### Missing from C ABI (available in C++ only)

| C++ Function | Purpose | C ABI Gap |
|:-------------|:--------|:----------|
| `core::xxh64()` | xxHash64 computation | No `RzHash` in C API |
| `core::parse()` batch | Multi-message parsing | No `RzSplitMany` in C API |
| `core::templateOf()` | Human-readable template (`<*>`) | No `RzHumanTemplate` in C API |
| `ScanMode` selection | Exact vs Semantic mode | `RzSplit` hardcodes `Semantic` mode |

---

## 2. Findings

### F-CABI-01 · No `RzHash` function exposed in C ABI
| Field | Value |
|:------|:------|
| **Severity** | 🟡 MEDIUM |
| **File** | [`RazobillC.h`](file:///C:/FM/rzd/razobill/include/librazobill/RazobillC.h) |
| **Description** | The C++ core exposes `core::xxh64()` and the DuckDB extension exposes `RzHash()` as a SQL function, but the C ABI has no `RzHash` function. FFI consumers (Go OTel processor, Rust Vector crate) must bring their own xxHash implementation (Go processor uses `github.com/cespare/xxhash/v2`), creating a hash divergence risk if seeds or algorithms differ. |
| **Impact** | FFI consumers cannot compute form hashes consistent with the library. The Go processor already works around this with a Go-native xxhash, but if the library ever changes hash seeds, these would silently diverge. |
| **Fix** | Add `RZ_API uint64_t RzHash(const char* data, size_t length)` to the C ABI. |

### F-CABI-02 · `RzSplit` hardcodes `ScanMode::Semantic` — no mode selection
| Field | Value |
|:------|:------|
| **Severity** | 🟡 MEDIUM |
| **File** | [`RazobillC.cpp#L47`](file:///C:/FM/rzd/razobill/src/RazobillC.cpp#L47) |
| **Description** | `RzSplit` always calls `core::parse(msgView, internal->slots, ScanMode::Semantic)`. There is no way for C ABI consumers to select `ScanMode::Exact` (digit-only mode). The Python bindings accept a `mode` parameter, and the C++ API accepts `ScanMode`, but the C ABI does not. |
| **Impact** | C/Go/Rust FFI consumers cannot use exact mode. |
| **Fix** | Add `RZ_API RzStatus RzSplitEx(const char* message, size_t messageLength, int mode, RzRecord* outRecord)` or add a mode parameter to a v2 function. Preserve `RzSplit` as the semantic-mode convenience wrapper. |

### F-CABI-03 · `RzRecord.templateStr` points into `InternalRecord` — dangling after `RzFree`
| Field | Value |
|:------|:------|
| **Severity** | ⚪ NOTE |
| **File** | [`RazobillC.cpp#L62-L69`](file:///C:/FM/rzd/razobill/src/RazobillC.cpp#L62-L69) |
| **Description** | After `RzSplit`, `outRecord->templateStr` points into `InternalRecord::slots.templateStr` which is owned by the `InternalRecord` on the heap. After `RzFree`, the record is zeroed (`memset(record, 0, ...)`), so `templateStr` becomes NULL. This is safe as documented — the header clearly states "any memory allocated by RzSplit must be freed with RzFree(outRecord)". |
| **Impact** | No issue — correctly documented lifetime contract. |
| **Fix** | None required. |

### F-CABI-04 · `RzRecord.strValues` pointers alias into caller's input buffer
| Field | Value |
|:------|:------|
| **Severity** | 🟡 MEDIUM |
| **File** | [`RazobillC.cpp#L57-L59`](file:///C:/FM/rzd/razobill/src/RazobillC.cpp#L57-L59) |
| **Description** | String slot views (`slots.strs`) are `string_view` into the original parsed message. The C API copies these data pointers directly into `strPointers`. The header documents this ("Pointers in outRecord->strs point directly into the caller's input message buffer"), but there is no runtime guard if the caller frees or modifies the input buffer before reading strs. |
| **Impact** | Correctly documented zero-copy design. Callers must keep the input buffer alive. However, the OTel Go processor uses `C.CString(msg)` which creates a copy that outlives the record, so it is safe. |
| **Fix** | None required — the contract is clearly documented in the header. |

### F-CABI-05 · Empty message returns a static `const char` pointer — safe but unusual
| Field | Value |
|:------|:------|
| **Severity** | ⚪ NOTE |
| **File** | [`RazobillC.cpp#L35-L39`](file:///C:/FM/rzd/razobill/src/RazobillC.cpp#L35-L39) |
| **Description** | For zero-length messages, `templateStr` is set to a function-local `static const char emptyStr = '\0'`. This is safe (static storage duration), but `RzFree` will `memset` the record to zero, making `templateStr` NULL afterwards. If a caller checks `templateStr != NULL` to determine if split was called, the empty-message case behaves differently (templateStr is non-NULL before free, NULL after). |
| **Impact** | Consistent with general contract. No issue. |
| **Fix** | None required. |

### F-CABI-06 · `RZ_API` macro uses `__declspec(dllexport)` only when `LIBRAZOBILL_EXPORTS` is defined
| Field | Value |
|:------|:------|
| **Severity** | 🟢 LOW |
| **File** | [`RazobillC.h#L28-L36`](file:///C:/FM/rzd/razobill/include/librazobill/RazobillC.h#L28-L36) |
| **Description** | On Windows, if `LIBRAZOBILL_EXPORTS` is not defined, `RZ_API` expands to nothing. This means the symbols are not exported from the DLL unless the build system defines this macro. CMakeLists.txt does set it via `target_compile_definitions(razobill PRIVATE LIBRAZOBILL_EXPORTS)`. However, on non-Windows, `RZ_API` uses `visibility("default")` which is always applied. There is no `__declspec(dllimport)` path for consumers — Windows consumers linking against the DLL should ideally see `__declspec(dllimport)`. |
| **Impact** | Windows static linking works. Windows dynamic linking may have suboptimal performance without `dllimport`. |
| **Fix** | Add a consumer-side `#define RZ_API __declspec(dllimport)` when `LIBRAZOBILL_EXPORTS` is NOT defined and `_WIN32` IS defined, or document that consumers should add it. |

### F-CABI-07 · `RZ_ABI_VERSION` is a preprocessor define, not queryable at runtime
| Field | Value |
|:------|:------|
| **Severity** | 🟢 LOW |
| **File** | [`RazobillC.h#L37`](file:///C:/FM/rzd/razobill/include/librazobill/RazobillC.h#L37) |
| **Description** | `RZ_ABI_VERSION 1` is compile-time only. There is no `RzAbiVersion()` runtime function. If a consumer is dynamically loading the library, they cannot verify ABI compatibility at runtime before calling functions. |
| **Impact** | ABI mismatch detection is compile-time only. |
| **Fix** | Add `RZ_API int RzAbiVersion(void)` returning `RZ_ABI_VERSION`. |

---

## 3. Summary

| ID | Severity | Component | One-liner |
|:---|:---------|:----------|:----------|
| F-CABI-01 | 🟡 MEDIUM | C ABI | No `RzHash` function exposed |
| F-CABI-02 | 🟡 MEDIUM | C ABI | No scan mode selection (`RzSplitEx`) |
| F-CABI-03 | ⚪ NOTE | C ABI | Dangling pointers after `RzFree` — correctly documented |
| F-CABI-04 | 🟡 MEDIUM | C ABI | String pointers alias caller buffer — correctly documented |
| F-CABI-05 | ⚪ NOTE | C ABI | Empty message static pointer pattern |
| F-CABI-06 | 🟢 LOW | C ABI | Missing `dllimport` for Windows consumers |
| F-CABI-07 | 🟢 LOW | C ABI | ABI version not queryable at runtime |
