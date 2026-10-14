# Universal C ABI reference

Header declarations, memory ownership rules, and status codes for RazobillC.h.
Read this reference if you are calling librazobill through foreign function interfaces (FFI).
Skip this reference if you are using high-level Python or SQL bindings.

---

## Overview

The C ABI is declared in `include/librazobill/RazobillC.h`.
It provides zero-allocation log decomposition with stable binary layout.

| Symbol | Type | Description |
| :--- | :--- | :--- |
| `RzSplit` | Function | Decomposes a message into an `RzRecord`. |
| `RzJoin` | Function | Reconstructs the exact message into caller-provided buffer. |
| `RzTemplate` | Function | Writes the structural template into caller buffer. |
| `RzFree` | Function | Releases heap memory allocated by `RzSplit`. |
| `RzStatusMessage` | Function | Returns human-readable description for any `RzStatus`. |
| `RzVersion` | Function | Returns the semantic version string (`"1.1.1"`). |
| `RzRecord` | Struct | Container for decomposed message data. |
| `RzStatus` | Enum | Numeric status code indicating success or error reason. |

---

## Types

### `typedef struct RzRecord`

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

**Lifetimes**:

1. Pointers in `strValues` point directly into the caller's message buffer (zero-copy string views).
   The message buffer must outlive `outRecord`.
2. Memory allocated for `templateStr`, `intValues`, and pointer arrays must be released with `RzFree(outRecord)`.

### `typedef enum RzStatus`

```c
typedef enum RzStatus {
    RZ_OK = 0,
    RZ_ERR_INVALID_ARGUMENT = 1,
    RZ_ERR_BUFFER_TOO_SMALL = 2,
    RZ_ERR_MALFORMED_TEMPLATE = 3,
    RZ_ERR_COUNT_MISMATCH = 4,
    RZ_ERR_MESSAGE_TOO_LARGE = 5,
    RZ_ERR_OUT_OF_MEMORY = 6,
    RZ_ERR_INTERNAL = 7
} RzStatus;
```

---

## Functions

### `RzStatus RzSplit(const char* message, size_t messageLength, RzRecord* outRecord)`

Splits a message into a template string, 64-bit integers, and string slices.

- **Parameters**:
  - `message`: Pointer to input bytes (does not require NUL termination).
  - `messageLength`: Byte length of message.
  - `outRecord`: Pointer to caller-allocated `RzRecord` struct.
- **Returns**: `RZ_OK` on success, or an error status code.

---

### `RzStatus RzJoin(const RzRecord* record, char* outBuffer, size_t bufferCapacity, size_t* outLength)`

Reconstructs the original message from a decomposed record.

- **Parameters**:
  - `record`: Pointer to the record to join.
  - `outBuffer`: Target buffer (may be `NULL` if probing capacity).
  - `bufferCapacity`: Byte capacity of `outBuffer`.
  - `outLength`: Pointer receiving bytes written or required.
- **Returns**: `RZ_OK` on success, or `RZ_ERR_BUFFER_TOO_SMALL`.

---

### `RzStatus RzTemplate(const char* message, size_t messageLength, char* outBuffer, size_t bufferCapacity, size_t* outLength)`

Extracts the template representation without allocating parsed value arrays.

---

### `void RzFree(RzRecord* record)`

Releases heap memory associated with a record. Safe to call on zeroed records or `NULL`.

---

### `const char* RzStatusMessage(RzStatus status)`

Returns a static string description for any status code.
