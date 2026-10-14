# C++20 engine reference

Core C++20 classes, headers, and internal engine components in librazobill.
Read this reference if you are contributing to the core engine or embedding C++ classes.
Skip this reference if you are using the C ABI or Python library.

---

## Headers overview

The C++20 engine is located under `include/librazobill/`:

| Header | Primary Role |
| :--- | :--- |
| `Version.hpp` | Semantic version macros, ABI versioning, and `VersionInfo` struct. |
| `Status.hpp` | Engine status codes (`Status::Ok`, `Status::Malformed`, `Status::TooLarge`). |
| `Slots.hpp` | Data structures: `SlotRecord`, `ScanMode`, and parser entrypoints. |
| `MemoryArena.hpp` | Monotonic 64-byte cache-line aligned bump allocator. |
| `StringPool.hpp` | Generational string pool with mark-and-sweep compaction. |
| `ThreadPool.hpp` | Work-stealing thread pool for parallel batch auditing. |
| `SimdScanner.hpp` | Delimiter vector scanners (AVX-512, AVX2, ARM NEON). |

---

## Core classes

### `struct razobill::core::SlotRecord`

```cpp
namespace razobill::core {

struct SlotRecord {
    std::string_view templateView;
    std::vector<int64_t> ints;
    std::vector<std::string_view> strs;
};

} // namespace razobill::core
```

### `class razobill::core::MemoryArena`

Provides zero-allocation chunk recycling across batches:

- `void* allocate(size_t bytes)`: Allocates 64-byte aligned memory.
- `void reset()`: Resets offset counter to zero without freeing memory blocks.

### `class razobill::core::StringPool`

Compacts and deduplicates string literals:

- `std::string_view intern(std::string_view str)`: Deduplicates string using xxHash64.
- `void gc()`: Compacts live allocations monotonically.
