# 02 — Core Engine Audit (Data Plane)

## Scope

Scanner.hpp, SimdScanner.hpp, Slots.hpp/cpp, Hash.hpp, Status.hpp

---

## Scanner.hpp — FSM Tokenizer

### Architecture

The scanner is a single-pass O(N) finite-state tokenizer using a compile-time 256-byte LUT (`detail::kClass`). No regex, no backtracking, no heap allocation. This is the correct architectural choice for 70M+ msgs/sec throughput.

### Findings

#### 🔴 F-SCAN-01: `isContextKeyword` uses stack buffer without bounds check

**File**: [`Scanner.hpp`](../include/librazobill/Scanner.hpp#L72-L84)
**Lines**: 72-84

```cpp
char buf[12];
for (std::size_t i = 0; i < w.size(); ++i) {
    buf[i] = c;  // w.size() is clamped to <= 10 by early return
}
```

The early `if (w.size() > 10) return false;` guard limits writes to 10 bytes into a 12-byte buffer. **Safe**, but the guard and buffer size are disconnected — if the guard is ever relaxed without updating the buffer, UB occurs. The `buf` is not NUL-terminated.

**Severity**: 🟢 LOW — currently safe, fragile coupling
**Fix**: Use `std::array<char, 12>` and add `static_assert` tying guard to buffer.

---

#### 🟡 F-SCAN-02: Quoted string scanning does not limit scan length

**File**: [`Scanner.hpp`](../include/librazobill/Scanner.hpp#L129-L157)

In Semantic mode, when a quote `"` is encountered, the scanner advances `p` forward looking for the closing quote with only `*p != quote && *p != '\n'` as termination. For a 16MB message consisting of `"aaaa...` (16M chars with no closing quote), this linearly scans the entire message before falling through to the "unclosed quote" path.

**Severity**: 🟢 LOW — bounded by `kMaxMessageBytes` (16MB), but no early-exit optimization
**Fix**: Consider a lookahead limit (e.g., 4096 bytes) for quoted string scanning.

---

#### ⚪ F-SCAN-03: `isContextKeyword` uses linear string comparisons

The function compares against 18 keywords using chained `==` comparisons. For a hot-path called on every non-digit token, this could be replaced by a minimal perfect hash or a switch on the first two characters. However, at 18 entries each ≤10 chars, this likely compiles to ~18 `memcmp` calls which are well within L1 cache. Modern branch predictors handle this well.

**Severity**: ⚪ NOTE — acceptable for current keyword count, document threshold for future growth.

---

## Slots.cpp — Decompose & Reconstruct

### Findings

#### 🔴 F-SLOT-01: `isIntToken` rejects negative integers but `reconstruct` allows them via `ints[]`

**File**: [`Scanner.hpp`](../include/librazobill/Scanner.hpp#L198-L204) and [`Slots.cpp`](../src/Slots.cpp#L104-L186)

`isIntToken()` only accepts positive integers (`t.front() == '0' ? size == 1 : all_digits`). But `reconstructNew()` at line 118 checks `if (ints[nInt] < 0) return Status::Malformed`. This means:
- Parsing never produces negative ints (correct)
- Reconstructing rejects negative ints in the `{i}` path (correct defensive check)

But `reconstructLegacy()` at line 80 also checks `if (ints[i] < 0) return Status::Malformed`. These checks are consistent. **No bug**, but:

**Observation**: The `isIntToken()` limit of 18 digits silently rejects integers from `10^18` to `999_999_999_999_999_999` (18 digits) that fit in `int64_t`. The max positive `int64_t` is `9,223,372,036,854,775,807` (19 digits). A token `9223372036854775807` (19 digits) is rejected as a string slot, not an int slot.

**Severity**: 🟡 MEDIUM — 19-digit integers that fit in int64_t are stored as string slots unnecessarily
**Fix**: Extend `isIntToken` to accept up to 19 digits with overflow check, or document as intentional.

---

#### 🟡 F-SLOT-02: `reconstructNew` double-parses the template

**File**: [`Slots.cpp`](../src/Slots.cpp#L104-L186)

The function parses the template twice: first to compute `estSize` (lines 111-139), then to emit bytes (lines 155-184). This is correct and safe — the first pass validates structure and pre-allocates exactly. However, it doubles the instruction count for the reconstruct path.

**Severity**: 🟡 MEDIUM — performance, not correctness
**Fix**: Single-pass with `std::string` `reserve` heuristic (template.size() + sum(int_digits) + sum(str_lengths)).

---

#### 🟢 F-SLOT-03: `digits10` returns 2 for `v == INT64_MIN`

**File**: [`Slots.cpp`](../src/Slots.cpp#L13-L19)

For `v = INT64_MIN = -9223372036854775808`:
```cpp
std::size_t n = (v < 0) ? 2 : 1;  // n = 2
uint64_t u = static_cast<uint64_t>(-(v + 1)) + 1;  // u = 9223372036854775808
```
The `-(v+1)` avoids UB on negating INT64_MIN. Then `u = 9223372036854775808` which has 19 decimal digits. Loop adds 18 → `n = 20`. **Correct**.

**Severity**: ⚪ NOTE — verified correct, including INT64_MIN edge case.

---

## Hash.hpp — xxHash64

### Findings

#### ⚪ F-HASH-01: xxHash64 implementation is byte-identical to reference

Verified constants match Yann Collet's specification:
- P1 = `0x9E3779B185EBCA87` ✓
- P2 = `0xC2B2AE3D27D4EB4F` ✓
- P3 = `0x165667B19E3779F9` ✓
- P4 = `0x85EBCA77C2B2AE63` ✓
- P5 = `0x27D4EB2F165667C5` ✓
- Seed = 0 ✓ (matches ClickHouse)
- Big-endian byte swap handling ✓
- Unaligned load via `memcpy` (correct, no UB) ✓

**Severity**: ⚪ NOTE — implementation is correct

---

#### 🟡 F-HASH-02: `std::rotl` requires `<bit>` header from C++20

**File**: [`Hash.hpp`](../include/librazobill/Hash.hpp#L65)

`std::rotl` is C++20. The project requires C++20 (`CMAKE_CXX_STANDARD 20`), so this is correct. But if any downstream consumer compiles with C++17, it fails.

**Severity**: 🟢 LOW — documented C++20 requirement is correct, but `Version.hpp` should assert `__cplusplus >= 202002L`.

---

## SimdScanner.hpp — SIMD Acceleration

### Findings

#### 🟡 F-SIMD-01: `findWhitespace*` functions are declared but never called in the main scanner

**File**: [`SimdScanner.hpp`](../include/librazobill/SimdScanner.hpp)

The SIMD whitespace scanner is included by `Scanner.hpp` but `scan()` never calls `findWhitespace()`. The main scanner loop uses `detail::kClass[*p]` byte-by-byte. The SIMD routines are utility functions available to users but not integrated into the core parse path.

**Severity**: 🟡 MEDIUM — advertised SIMD acceleration is not connected to the critical path
**Fix**: Either integrate `findWhitespace` into `scan()` for the delimiter-skipping loop, or document that SIMD is available for custom scanning only.

---

#### 🟢 F-SIMD-02: MSVC AVX-512 detection missing

**File**: [`SimdScanner.hpp`](../include/librazobill/SimdScanner.hpp#L68-L84)

`detectSimdArchitecture()` uses `__builtin_cpu_supports` (GCC/Clang only) or compile-time `__AVX2__` for MSVC. On MSVC at runtime with no `/arch:AVX512` flag, the function returns `Scalar` even on AVX-512-capable hardware.

**Severity**: 🟢 LOW — MSVC runtime CPUID detection is non-trivial; document as known limitation.

---

## Status.hpp — Error Codes

### Findings

#### ⚪ F-STATUS-01: `describe()` default case is unreachable

All 7 enum values are covered by the switch. The `return "unknown"` is defensive. **Correct**.

#### 🟢 F-STATUS-02: No `std::ostream& operator<<` for `Status`

Users cannot stream `Status` values to logs directly.

**Severity**: 🟢 LOW — convenience feature
