# 04 — Memory Safety Audit

## Scope

Lifetime contracts, allocation patterns, UB risks, buffer safety across all modules

---

## Lifetime & Dangling Reference Analysis

### 🔴 F-MEM-01: `SlotRecord::strs` holds `string_view` into caller's message buffer

**File**: [`Slots.hpp`](../include/librazobill/Slots.hpp#L25)

```cpp
std::vector<std::string_view> strs; // zero-copy views into the message
```

**Risk**: If the original message string is destroyed or modified while `SlotRecord` is live, all `string_view` entries become dangling pointers. This is by design for performance (zero-copy), but:

1. The C ABI (`RzSplit`) copies the `string_view` data pointers into `RzRecord.strValues`, which also borrow from the caller's message buffer. The `RazobillC.h` header documents this correctly: *"The message buffer MUST outlive outRecord."*
2. The Python binding creates owning `std::string` copies via pybind11, so Python is safe.
3. The DuckDB extension receives `string_t` from DuckDB's string heap which is valid for the function call duration, so it's safe.

**Severity**: 🟡 MEDIUM — correctly documented, but no runtime guard possible. C ABI callers can easily violate this.
**Fix**: Add a comment to `SlotRecord` documenting the lifetime requirement. Consider an optional owning mode for safety-critical callers.

---

### 🟡 F-MEM-02: `RazobillC.cpp` — `InternalRecord` stores `SlotRecord` with views into… itself

**File**: [`RazobillC.cpp`](../src/RazobillC.cpp#L19-L23)

```cpp
struct InternalRecord {
    razobill::core::SlotRecord slots;
    std::vector<const char*> strPointers;
    std::vector<size_t> strLengths;
};
```

After `parse()`, `slots.strs` contains `string_view` into the **caller's message buffer** (not into `InternalRecord`). The `strPointers` vector stores raw `char*` from these views. As long as the caller's message buffer is alive when `RzJoin` is called, this is safe.

But `outRecord->templateStr = internal->slots.templateStr.data()` points into the `std::string` owned by `InternalRecord`. Since `InternalRecord` is heap-allocated and kept alive via `internalHandle`, this is safe until `RzFree()`.

**Severity**: ⚪ NOTE — safe, but the mixed ownership (template owned by handle, strs borrowed from caller) is subtle.

---

### 🟡 F-MEM-03: `RzFree` zeroes the struct after delete

**File**: [`RazobillC.cpp`](../src/RazobillC.cpp#L165-L172)

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

After `delete`, `record->internalHandle` is set to `nullptr`, then `memset` zeros the whole struct. This is correct — prevents double-free and dangling pointer access. But `RzFree` doesn't prevent the caller from reading `record->templateStr` after free (it's now NULL due to memset). **Defensive and correct**.

**Severity**: ⚪ NOTE — correct defensive pattern.

---

## Buffer Overrun Analysis

### 🟢 F-MEM-04: `reconstruct` functions pre-validate output size

Both `reconstructLegacy` (line 85) and `reconstructNew` (line 150) check `if (size > kMaxMessageBytes) return Status::TooLarge;` before allocating. Combined with `std::string::reserve`, this prevents unbounded allocation. **Correct**.

---

### 🟢 F-MEM-05: `RzJoin` / `RzTemplate` check `bufferCapacity` before `memcpy`

```cpp
if (!outBuffer || bufferCapacity < reqLen) {
    return RZ_ERR_BUFFER_TOO_SMALL;
}
if (reqLen > 0) {
    std::memcpy(outBuffer, result.data(), reqLen);
}
```

**Correct** — no NUL termination is written (documented). Buffer is never overrun.

---

## Allocation Pattern Analysis

### 🟡 F-MEM-06: `MemoryArena::allocate()` uses `_aligned_malloc` on Windows, `posix_memalign` on Unix

These are platform-correct. The `BlockGuard` RAII pattern ensures no leak on `push_back` failure. **Correct**.

But `_aligned_malloc` must be freed with `_aligned_free`, not `free()`. The code correctly uses:
```cpp
#if defined(_MSC_VER) || defined(__MINGW32__)
    _aligned_free(block.ptr);
#else
    std::free(block.ptr);
#endif
```

**Correct** — platform-matched alloc/free pairs.

---

### 🟡 F-MEM-07: `AdbcBackend` destructor does not check ADBC release errors

**File**: [`BackendAdbc.cpp`](../src/BackendAdbc.cpp#L190-L202)

```cpp
~AdbcBackend() override {
    AdbcError err{};
    if (have_conn_) driver_.ConnectionRelease(&conn_, &err);
    if (have_db_) driver_.DatabaseRelease(&db_, &err);
    // err is ignored
```

ADBC release errors are silently ignored. This is standard practice in destructors (cannot throw), but if `ConnectionRelease` fails, the connection may leak in the underlying driver.

**Severity**: 🟢 LOW — standard destructor pattern
**Fix**: Consider logging the error if a logging facility is available.

---

### 🟠 F-MEM-08: `AdbcBackend` destructor unloads `lib_` while ADBC objects may still reference it

**File**: [`BackendAdbc.cpp`](../src/BackendAdbc.cpp#L194-L197)

```cpp
if (lib_) {
    sysUnloadLibrary(lib_);
    lib_ = nullptr;
}
```

The destructor releases ADBC connection/database first, then unloads the driver library. This is correct order — but if `ConnectionRelease` or `DatabaseRelease` fail (leaving internal state), then `sysUnloadLibrary` unmaps the code that owns that state. Any later access (e.g., by a background thread in the driver) causes SEGFAULT.

**Severity**: 🟡 MEDIUM — theoretical risk with badly-behaved ADBC drivers
**Fix**: Consider not unloading the library at all (let OS reclaim on process exit), or document that the driver must fully release before unload.

---

## Integer Overflow Analysis

### 🟢 F-MEM-09: `MemoryArena::allocate()` overflow check

```cpp
if (bytes > std::numeric_limits<std::size_t>::max() - alignment - 64) [[unlikely]] {
    throw std::bad_alloc();
}
```

Correct — prevents `bytes + alignment` from wrapping. The `required` computation is also safe due to this guard.

---

### 🟡 F-MEM-10: `registerFormsLocked` uses `row_number() OVER` for sequential IDs

**File**: [`Razobill.cpp`](../src/Razobill.cpp#L216-L219)

```cpp
"CAST((SELECT count(*) FROM rb_form) + row_number() OVER (ORDER BY form) - 1 AS " + d.tI32() + ")"
```

If `rb_form` has more than `INT32_MAX` entries (~2 billion), the cast to `INTEGER` overflows silently in SQL. The `rb_form.no` column is `INTEGER` (32-bit), limiting forms to ~2B.

**Severity**: 🟢 LOW — 2 billion distinct log templates is unrealistic in practice
**Fix**: Document the 2B form limit.
