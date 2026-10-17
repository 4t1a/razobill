# Razobill Safety & Resource Management Audit

---

## A1. No Transactional Atomicity in Control Plane

**File:** [Razobill.cpp](file:///C:/FM/rzd/razobill/src/Razobill.cpp) — `define()`, `retire()`, `pin()`, `compact()`  
**Severity:** 🟠 HIGH  

**Problem:** Control plane operations modify multiple tables (`rb_form`, `rb_rule`, `rb_meta`, compact target table) across several SQL statements without `BEGIN`/`COMMIT`. If the process crashes mid-operation (e.g., after inserting into `rb_form` but before updating `rb_rule`), the database is left in an inconsistent state.

**Impact:** Data corruption on crash during compaction or rule mutation.

**Fix:** Wrap each control-plane operation in a transaction:

```cpp
void RazobillEngine::define(...) {
    std::scoped_lock lock(mu_);
    db_.exec("BEGIN TRANSACTION");
    try {
        // ... all mutations ...
        db_.exec("COMMIT");
    } catch (...) {
        db_.exec("ROLLBACK");
        throw;
    }
}
```

---

## A2. Dynamic Library Handle Leak

**File:** [BackendAdbc.cpp:142, 165–170](file:///C:/FM/rzd/razobill/src/BackendAdbc.cpp#L142)  
**Severity:** 🟠 HIGH  

```cpp
lib_ = sysLoadLibrary(driverPath.c_str(), o.deepbind, /*global=*/false);
// ...
~AdbcBackend() override {
    AdbcError err{};
    if (have_conn_) driver_.ConnectionRelease(&conn_, &err);
    if (have_db_) driver_.DatabaseRelease(&db_, &err);
    // lib_ is NEVER freed via FreeLibrary / dlclose
}
```

**Problem:** The `lib_` handle (from `LoadLibraryA` / `dlopen`) is never released. Similarly, preloaded libraries are never closed.

**Impact:** OS library handle leak. The loaded shared library's memory is never reclaimed. In long-running processes that create/destroy backends, this accumulates.

**Fix:** Add to destructor:

```cpp
~AdbcBackend() override {
    AdbcError err{};
    if (have_conn_) driver_.ConnectionRelease(&conn_, &err);
    if (have_db_) driver_.DatabaseRelease(&db_, &err);
    if (lib_) {
#if defined(_WIN32)
        FreeLibrary(reinterpret_cast<HMODULE>(lib_));
#else
        dlclose(lib_);
#endif
    }
}
```

---

## A3. `AdbcError` Resource Leak

**File:** [BackendAdbc.cpp:73–77](file:///C:/FM/rzd/razobill/src/BackendAdbc.cpp#L73-L77)  
**Severity:** 🟠 HIGH  

```cpp
[[noreturn]] void fail(std::string_view what, const AdbcError& e) {
    std::string m(what);
    if (e.message) m += std::string(": ") + e.message;
    throw BackendError(m);
    // e.release(&e) is NEVER called
}
```

**Problem:** The ADBC C specification requires callers to call `if (err.release) err.release(&err)` after reading the error. `fail()` throws without releasing, leaking the driver-allocated error string.

**Impact:** Memory leak on every failed database operation.

**Fix:**

```cpp
[[noreturn]] void fail(std::string_view what, AdbcError& e) {  // non-const
    std::string m(what);
    if (e.message) m += std::string(": ") + e.message;
    if (e.release) e.release(&e);  // release before throw
    throw BackendError(m);
}
```

Also apply at every call site where `AdbcError err{}` is declared, even on success paths where the error is unused.

---

## A4. 128-Bit Decimal Precision Loss

**File:** [BackendAdbc.cpp:109–118](file:///C:/FM/rzd/razobill/src/BackendAdbc.cpp#L109-L118)  
**Severity:** 🟡 MEDIUM  

```cpp
const auto hi = static_cast<std::int64_t>(w[1]);
double v = static_cast<double>(hi) * 18446744073709551616.0 + static_cast<double>(w[0]);
```

**Problem:** IEEE 754 `double` has only 53 bits of mantissa. Combining two 64-bit words (128-bit decimal) into `double` truncates precision. For financial data with >15 significant digits, results will be wrong.

**Impact:** Silent data corruption for high-precision decimal columns. Acceptable for log telemetry (where decimals are rare), but the code should document this limitation.

**Fix (minimal):** Add a comment documenting the ~15-digit precision limit. For full fix, return as `std::string` via `snprintf` or use `__int128`.

---

## A5. No Thread Safety in `AdbcBackend`

**File:** [BackendAdbc.cpp:133–250](file:///C:/FM/rzd/razobill/src/BackendAdbc.cpp#L133)  
**Severity:** 🟠 HIGH  

**Problem:** `AdbcBackend` has no internal mutex. The `run()` method accesses `conn_`, `driver_`, and creates statements without synchronization. Concurrent `exec()` or `query()` calls from different threads race on the ADBC connection handle.

**Mitigating factor:** `RazobillEngine` holds `mu_` (a `std::mutex`) around most backend calls. But direct `Backend` usage outside the engine (e.g., from Python bindings) has no protection.

**Fix:** Add a `std::mutex` to `AdbcBackend::run()`:

```cpp
void run(std::string_view sql, Table* out) {
    std::scoped_lock lock(connMutex_);
    // ... existing code ...
}
```

---

## A6. `parallelFor` Captures Callable by Reference

**File:** [ThreadPool.hpp — `parallelFor()`](file:///C:/FM/rzd/razobill/include/librazobill/ThreadPool.hpp)  
**Severity:** 🟡 MEDIUM  

**Problem:** If `fn` is a temporary lambda, and the caller doesn't block on the returned futures, the reference becomes dangling. Currently safe because `parallelFor` blocks on `future::get()`, but fragile under refactoring.

**Fix:** Capture by value or wrap in `std::shared_ptr<Fn>`:

```cpp
template <class Fn>
void parallelFor(std::size_t n, std::size_t chunk, Fn fn) {
    auto shared = std::make_shared<Fn>(std::move(fn));
    // ... use *shared in tasks ...
}
```

---

## Resource Management Summary

| ID | Resource | Leak Type | Fix |
|----|----------|-----------|-----|
| A1 | DB state | Partial mutation on crash | Transaction wrapping |
| A2 | `lib_` handle | Never freed | `FreeLibrary`/`dlclose` in dtor |
| A3 | `AdbcError.message` | Never released | Call `e.release(&e)` before throw |
| A4 | Decimal precision | Silent truncation | Document or use string |
| A5 | ADBC connection | Data race | Add `connMutex_` |
| A6 | Lambda lifetime | Dangling reference | Capture by value |
