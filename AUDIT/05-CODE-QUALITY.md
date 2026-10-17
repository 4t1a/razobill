# Razobill Code Quality Audit

---

## Q1. Duplicate Source Copies in `lib/python/`

**Location:** `lib/python/includeCore/` and `lib/python/srcCore/`  
**Severity:** 🟠 HIGH  

**Problem:** Complete byte-for-byte copies of all 16 headers and 7 source files exist under `lib/python/includeCore/` and `lib/python/srcCore/`. These are maintained by manual copy. The Python `CMakeLists.txt` / `setup.py` uses the original `include/` directory, so these copies may already be stale.

**Risk:** A bug fix applied to `src/Stats.cpp` but not to `lib/python/srcCore/Stats.cpp` creates a silent behavioral divergence between the native library and the Python wheel.

**Fix:** Remove the copies. Use `setup.py` / `CMakeLists.txt` to reference `../../include/` and `../../src/` directly:

```python
# setup.py
include_dirs = [str(Path(__file__).parent.parent.parent / "include")]
sources = list((Path(__file__).parent.parent.parent / "src").glob("*.cpp"))
```

Or use symlinks. Or use a build-time copy step in CMake that always copies from the canonical source.

---

## Q2. Missing `[[nodiscard]]` on Important Functions

**Files:** Various  
**Severity:** 🔵 LOW  

Functions that return critical status values but are not marked `[[nodiscard]]`:

| Function | File | Returns |
|----------|------|---------|
| `refresh()` | [Razobill.hpp](file:///C:/FM/rzd/razobill/include/librazobill/Razobill.hpp) | void (should be Status) |
| `compact()` | [Razobill.hpp](file:///C:/FM/rzd/razobill/include/librazobill/Razobill.hpp) | void |
| `verify()` | [Razobill.hpp](file:///C:/FM/rzd/razobill/include/librazobill/Razobill.hpp) | bool |
| `parse()` | [Slots.hpp](file:///C:/FM/rzd/razobill/include/librazobill/Slots.hpp) | Status |
| `reconstruct()` | [Slots.hpp](file:///C:/FM/rzd/razobill/include/librazobill/Slots.hpp) | Status |

**Fix:** Add `[[nodiscard]]` to all functions returning `Status`, `bool`, or error-carrying types.

---

## Q3. Redundant Backward-Compat Field Duplication

**Files:** [Backend.hpp](file:///C:/FM/rzd/razobill/include/librazobill/Backend.hpp#L64-L78), [Razobill.hpp](file:///C:/FM/rzd/razobill/include/librazobill/Razobill.hpp), [Stats.hpp](file:///C:/FM/rzd/razobill/include/librazobill/Stats.hpp)  
**Severity:** 🟡 MEDIUM  

**Problem:** Multiple structs carry both camelCase and snake_case fields:

```cpp
// Backend.hpp — AdbcOptions
std::string driverPath;
std::string driver_path;         // backward compat
std::vector<std::string> preloadGlobal;
std::vector<std::string> preload_global; // backward compat
```

These are manually synchronized at use sites with patterns like:

```cpp
const auto& preloads = !o.preloadGlobal.empty() ? o.preloadGlobal : o.preload_global;
```

**Risk:** Incorrect precedence logic (as in B2), silent data loss if both are set.

**Fix:** Keep only the canonical naming. Add deprecated aliases as inline accessor methods:

```cpp
struct AdbcOptions {
    std::string driverPath;
    // Deprecated: use driverPath
    [[deprecated("use driverPath")]]
    void set_driver_path(std::string v) { driverPath = std::move(v); }
};
```

---

## Q4. Missing `#include <tuple>` in Stats.cpp

**File:** [Stats.cpp](file:///C:/FM/rzd/razobill/src/Stats.cpp)  
**Severity:** 🔵 LOW  

**Problem:** Uses `std::tuple` in `TripleHash` and `TripleHashU64` but relies on transitive include from `<unordered_map>`. This is not guaranteed by the standard and breaks on some toolchains (Clang libc++).

**Fix:** Add `#include <tuple>`.

---

## Q5. Unchecked `std::from_chars` Return

**File:** [Slots.cpp:188–189](file:///C:/FM/rzd/razobill/src/Slots.cpp#L188-L189)  
**Severity:** 🔵 LOW  

```cpp
std::from_chars(tok.data(), tok.data() + tok.size(), v);
// return value ignored
```

**Problem:** While `isIntToken()` pre-validates the format (≤18 digits, no leading zeros), ignoring `std::from_chars_result::ec` is poor defensive practice. If `isIntToken()` is ever relaxed, this becomes a silent bug.

**Fix:**

```cpp
auto [ptr, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), v);
if (ec != std::errc{}) return Status::Malformed;
```

---

## Q6. Inconsistent Slot Count Validation

**File:** [Slots.cpp:60 vs 130](file:///C:/FM/rzd/razobill/src/Slots.cpp#L60)  
**Severity:** 🔵 LOW  

| Path | Validation | Behavior |
|------|-----------|----------|
| `reconstructLegacy` | `ints.size() < nInt` | Allows extra slots |
| `reconstructNew` | `ints.size() != nInt` | Rejects extra slots with `SlotUnderflow` |

**Problem:** Inconsistent contract. `reconstructNew` returns `SlotUnderflow` even when extra parameters are provided (overflow, not underflow).

**Fix:** Both paths should use the same validation. Recommended: allow extra slots (ignore surplus) or add a `SlotOverflow` status code.

---

## Q7. Hardcoded Magic Bytes in SQL Expressions

**File:** [Dialect.cpp](file:///C:/FM/rzd/razobill/src/Dialect.cpp)  
**Severity:** 🔵 LOW  

Direct use of `char(17)`, `char(18)`, `char(19)` in SQL strings without referencing the named constants `kTokenMark`, `kPlaceholder`, `kTypeSeparator` from `Scanner.hpp`.

**Fix:** Use `std::to_string(kTokenMark)` etc., or define SQL-side constants in `Dialect.hpp`.

---

## Q8. `templateOf()` Does Not Unescape New Format

**File:** [Slots.cpp:237–242](file:///C:/FM/rzd/razobill/src/Slots.cpp#L237-L242)  
**Severity:** 🔵 LOW  

```cpp
// New format path:
out.assign(form);  // raw assignment — no unescaping of {{ / }} or {i}/{s} → <*>
return Status::Ok;
```

**Problem:** For new-format templates, `templateOf()` returns the raw template string without converting `{i}` / `{s}` placeholders to `<*>` wildcards, and without unescaping `{{` / `}}` to `{` / `}`.

**Fix:** Apply the same wildcard conversion as the legacy path:

```cpp
// Replace {i} and {s} with <*>, unescape {{ → { and }} → }
```

---

## Quality Summary

| Category | Issues | Severity Range |
|----------|--------|----------------|
| Duplicate files | 1 | 🟠 HIGH |
| Missing attributes | 1 | 🔵 LOW |
| Field duplication | 1 | 🟡 MEDIUM |
| Missing includes | 1 | 🔵 LOW |
| Unchecked returns | 1 | 🔵 LOW |
| Inconsistent contracts | 1 | 🔵 LOW |
| Magic numbers | 1 | 🔵 LOW |
| Missing unescaping | 1 | 🔵 LOW |
