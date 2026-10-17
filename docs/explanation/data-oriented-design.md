# Data-oriented design and memory layout

[Docs](../README.md) > [Explanations](../README.md#4-explanations-and-architecture-understand) > Data-oriented design and memory layout

How cache contiguity and Structure-of-Arrays layout maximize CPU efficiency during log parsing.
Read this article if you want to understand memory alignment, cache prefetching, and bump allocators in librazobill.
Skip this article if you are consuming pre-compiled packages.

---

## The memory wall in log parsing

Modern CPU cores operate in fractions of a nanosecond, while main memory access (DRAM) requires tens of nanoseconds.
When processing millions of log lines per second, traditional object-oriented designs stall:

- Allocating separate C++ heap objects for each message forces random pointer chasing across non-contiguous memory.
- Standard vector reallocations fragment the heap and trigger memory fragmentation under sustained telemetry spikes.
- Packing heterogeneous fields into a single struct wastes cache line bandwidth when only specific fields are queried.

Razobill resolves this bottleneck using data-oriented design (DOD) principles.

---

## Structure-of-Arrays (SoA) layout

In librazobill, decomposed lines are represented using Structure-of-Arrays (`SlotRecord` in C++, `RzRecord` in C ABI):

```cpp
struct SlotRecord {
    std::string_view templateView;
    std::vector<int64_t> ints;
    std::vector<std::string_view> strs;
};
```

### Benefits of SoA

1. **Contiguous integer storage**: The `ints` vector stores 64-bit numbers in contiguous cache lines.
   Vectorized analytical operations (such as computing sums or averages in DuckDB) can stream directly through CPU SIMD registers without inspecting string data.
2. **Zero-copy string views**: The `strs` vector holds lightweight `std::string_view` structures (pointer + length) pointing directly into the input buffer.
   No string characters are copied to the heap during the decomposition phase.

---

## 64-byte aligned memory arenas

The core engine relies on a monotonic bump allocator (`MemoryArena`):

- Memory is reserved in 64-kilobyte contiguous chunks.
- Allocations simply advance an internal offset pointer.
- Memory blocks are aligned to 64-byte boundaries, matching CPU cache line width.
- Freeing an arena resets the offset pointer to zero in constant time $O(1)$ without releasing memory to the OS kernel.

---

## Compacting string pool

For operations requiring string persistence, `StringPool` provides generational compaction:

- Unique string literals are deduplicated using 64-bit hashes.
- Strings are packed sequentially into monolithic contiguous buffers.
- Periodic garbage collection (`gc()`) compacts live strings monotonically, removing dead space while preserving contiguous layout for CPU prefetchers.

---

## Related

- [Systems architecture overview](architecture.md)
- [SIMD vectorization and hardware dispatch](simd-vectorization.md)
- [C++20 engine reference](../reference/cpp-api.md)
