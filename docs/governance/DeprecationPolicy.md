# API Deprecation Policy (RFC 8594 Aligned)

This document establishes the deprecation, sunset, and removal lifecycle for all public APIs in **`librazobill`** and **`pyrazobill`**.

---

## 1. The 3-Stage Deprecation Lifecycle

```
[ Active ] ──> [ Deprecated + Warning ] ──> [ Sunset / Removed ]
                      │                               │
                Phase 1 & 2                        Phase 3
          (MINOR Release: 1.x.0)            (MAJOR Release: 2.0.0)
```

### Phase 1: Mark & Announce (MINOR Version Bump)
- The method or class is marked with `LIBRAZOBILL_DEPRECATED("use newMethod instead")` in C++20 and C headers.
- Python methods emit `warnings.warn("...", DeprecationWarning, stacklevel=2)`.
- The changelog documents the deprecation under the `Deprecated` section with explicit migration examples.
- **Strict Invariant**: The deprecated functionality remains fully operational with zero breaking behavior.

### Phase 2: Telemetry & Sunset Notice (Intermediate Bumps)
- For network/HTTP or cloud integrations, deprecation headers conform to RFC 8594:
  ```http
  Deprecation: @1767225600
  Sunset: Fri, 01 Jan 2027 00:00:00 GMT
  Link: <https://github.com/razobill/razobill/docs/migration-v2.md>; rel="deprecation"
  ```
- Debug builds output diagnostic warnings into the test console.

### Phase 3: Hard Removal (MAJOR Version Bump Only)
- Deprecated symbols and interfaces are removed **only** upon cutting a new MAJOR version (e.g., `2.0.0`).
- No symbol marked as active in `1.x.y` will ever be deleted in any subsequent `1.x.z` release.

---

## 2. Deprecation Code Standards

### C++20 / C Header
```cpp
#include "librazobill/Version.hpp"

// Deprecated in 1.1.0, to be removed in 2.0.0
LIBRAZOBILL_DEPRECATED("Use RazobillEngine::parseBatch instead")
void parseAll(const std::vector<std::string>& msgs);
```

### Python API
```python
import warnings

def oldFunction():
    warnings.warn(
        "oldFunction() is deprecated and will be removed in pyrazobill 2.0.0; use parseBatch() instead",
        category=DeprecationWarning,
        stacklevel=2
    )
    return parseBatch(...)
```
