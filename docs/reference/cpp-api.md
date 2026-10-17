# C++20 engine reference

[Docs](../README.md) / [Reference](cpp-api.md) / C++ API

This document specifies the internal C++20 classes, headers, and core interfaces in `librazobill`.

- **Who this is for**: C++ developers contributing to the core engine or embedding native classes.
- **What you will have at the end**: Precise header organization, struct definitions, status codes, and memory allocators.

---

## Header overview

The C++20 engine is declared in `include/librazobill/`:

| Header | Primary role |
| :--- | :--- |
| `Version.hpp` | Version macros (`LIBRAZOBILL_VERSION_*`), ABI constants, and `VersionInfo` |
| `Status.hpp` | Error enumeration (`Status::Ok`, `Status::TooLarge`, `Status::Malformed`) |
| `Slots.hpp` | Data structures: `SlotRecord`, `ScanMode`, `parse()`, and `reconstruct()` |
| `Scanner.hpp` | 256-byte compile-time lookup table and tokenizer finite-state machine |
| `SimdScanner.hpp` | Hardware vector acceleration (AVX-512BW, AVX2, ARM NEON) and CPUID dispatch |
| `MemoryArena.hpp` | 64-byte cache-line aligned bump allocator for batch parsing |
| `StringPool.hpp` | String interning pool with generational compaction |
| `ThreadPool.hpp` | Work-stealing worker thread pool for parallel batch auditing |
| `Hash.hpp` | 64-bit unsigned hash functions (`xxh64`) |

---

## Data structures

### struct razobill::core::SlotRecord

Defined in `include/librazobill/Slots.hpp`:

```cpp
namespace razobill::core {

struct SlotRecord {
    std::string templateStr;            // format string with {i} and {s} placeholders
    std::vector<std::int64_t> ints;     // values of the '{i}' slots
    std::vector<std::string_view> strs; // values of the '{s}' slots borrowing from input

    void clear() noexcept;
    void formKey(std::string& out) const;
    [[nodiscard]] std::uint64_t formKeyHash() const noexcept;
};

using Slots = SlotRecord;

} // namespace razobill::core
```

#### Memory contract

`strs` contains zero-copy `std::string_view` elements referencing the caller's source message.
The source buffer must outlive `SlotRecord`.

---

## Core functions

### parse

```cpp
[[nodiscard]] Status parse(
    std::string_view msg,
    SlotRecord& out,
    ScanMode mode = ScanMode::Semantic
);
```

Splits `msg` into `out.templateStr`, `out.ints`, and `out.strs`. `out.strs` borrows slices directly from `msg`.

### reconstruct / join

```cpp
[[nodiscard]] Status reconstruct(
    std::string_view tmpl,
    std::span<const std::int64_t> ints,
    std::span<const std::string_view> strs,
    std::string& out
);
```

Reconstructs the original message from a template and parameter slices with 100% byte-for-byte fidelity.

---

## Error handling

Status codes are defined in `include/librazobill/Status.hpp` as a 1-byte enum class:

```cpp
namespace razobill {

enum class Status : unsigned char {
    Ok = 0,
    TooLarge,       // message or rebuilt message exceeds 16 MiB
    Malformed,      // template syntax is invalid
    SlotUnderflow,  // fewer slot values than the template requires
    SlotOverflow,   // more slot values than the template requires
    BufferTooSmall, // destination buffer capacity is insufficient
    InvalidParam    // null pointer or out-of-range argument
};

[[nodiscard]] constexpr const char* describe(Status s) noexcept;

} // namespace razobill
```

Data-plane operations return `Status` directly rather than throwing C++ exceptions, maintaining deterministic microsecond throughput.

---

## Memory management

### MemoryArena

Monotonic bump allocator aligned to 64-byte CPU cache lines (`include/librazobill/MemoryArena.hpp`).
Allows batch operations to allocate scratch memory without heap fragmentation:

- `void* allocate(std::size_t bytes)`: Allocates aligned memory.
- `void reset() noexcept`: Resets the bump pointer to zero without releasing underlying virtual memory blocks.

---

## Related

- [Universal C ABI reference](c-abi.md): C99 interface for FFI consumers.
- [Embedding librazobill in C++](../how-to/cpp-developer.md): CMake linking and compiler flags.
- [Data-oriented design](../explanation/data-oriented-design.md): Structure-of-Arrays cache locality.
