# Data-Oriented Design & Memory Architecture

High-throughput log processing is bounded by memory bandwidth, cache hierarchy misses (L1/L2/L3), and OS heap allocation contention. `librazobill` addresses these bottlenecks by replacing traditional pointer-heavy object graphs with cache-line aligned contiguous Structure-of-Arrays (SoA) memory layouts and a monotonic bump allocator.

---

## 1. Structure-of-Arrays (SoA): `SlotRecord`

In conventional OOP log parsers, parsed tokens are modeled as arrays of polymorphic objects:
```cpp
// ANTI-PATTERN: Pointer indirection, cache thrashing, fragmentation
struct BadToken {
    TokenType type;
    std::string value; // Heap allocation per token
};
struct BadLogRecord {
    std::vector<BadToken> tokens; // Pointer to array of pointers
};
```
This layout scatters allocations across the heap, causes cache misses when accessing tokens, and requires dozens of heap allocations per log message.

### The `librazobill` DOD Layout

[`include/librazobill/Slots.hpp`](../../include/librazobill/Slots.hpp) defines `SlotRecord` as a contiguous Structure-of-Arrays:

```cpp
struct alignas(64) SlotRecord {
    std::string skeleton;                  // Constant template text with hole markers (0x12)
    std::string types;                     // Type descriptor string: 'I' for int, 'S' for string
    std::vector<std::int64_t> ints;        // Contiguous 64-bit integer values
    std::vector<std::string_view> strs;    // Contiguous zero-copy string views into source buffer
};
```

```
Memory Layout of SlotRecord (64-byte Cache-Line Aligned):
┌───────────────────────────┬───────────────────────────┬──────────────────────────────────────────┐
│ skeleton (std::string)    │ types (std::string)       │ ints (std::vector<int64_t>)              │
│ [ptr, size, cap]          │ [ptr, size, cap]          │ [data: int64, int64, int64, ...]         │
├───────────────────────────┴───────────────────────────┴──────────────────────────────────────────┤
│ strs (std::vector<std::string_view>)                                                             │
│ [data: (ptr, len), (ptr, len), (ptr, len), ...] -> Direct slices of input buffer                 │
└──────────────────────────────────────────────────────────────────────────────────────────────────┘
```

### Architectural Advantages
1. **Contiguous Integer Streaming**: Numeric slot values (`ints`) sit in dense, contiguous 64-bit arrays. Columnar databases (DuckDB) can copy or vector-load them directly into Arrow or DuckDB vectors via `std::memcpy`.
2. **Zero-Copy String Views**: String variables (`strs`) are non-owning `std::string_view` slices that point directly to byte offsets in the input message. No string allocations occur during the parsing pass.
3. **Hardware Cache Efficiency**: Because `SlotRecord` is aligned to 64 bytes (`alignas(64)`), it never straddles cache-line boundaries, eliminating false sharing on multi-core processors.

---

## 2. Monotonic Bump Memory Arena (`MemoryArena`)

During high-volume ingestion (e.g. 100,000 messages/second), repeated calls to `malloc()` or `operator new` cause heap lock contention, memory fragmentation, and latency spikes.

[`include/librazobill/MemoryArena.hpp`](../../include/librazobill/MemoryArena.hpp) implements a thread-safe, monotonic bump allocator:

```cpp
class MemoryArena {
public:
    explicit MemoryArena(std::size_t defaultChunkSize = 4 * 1024 * 1024); // 4 MiB chunks
    ~MemoryArena();

    void* allocate(std::size_t bytes, std::size_t alignment = 64);
    void reset() noexcept; // Reclaims all memory in O(1)
};
```

### Allocation Mechanics
1. **Chunk Chaining**: Memory is allocated from the operating system in large contiguous chunks (default 4 MiB).
2. **Bump Pointer**: Allocating $N$ bytes is a single pointer addition:
   ```cpp
   std::uintptr_t current = reinterpret_cast<std::uintptr_t>(currentChunk_->currentPtr);
   std::uintptr_t aligned = (current + (alignment - 1)) & ~(alignment - 1);
   currentChunk_->currentPtr = reinterpret_cast<char*>(aligned + bytes);
   return reinterpret_cast<void*>(aligned);
   ```
3. **$O(1)$ Batch Reset with Block Freelist**: Calling `arena.reset()` rewinds the chunk pointers to zero while preserving all allocated chunks in a block freelist. Subsequent batch iterations immediately reuse the warmed, physical RAM pages without incurring page faults, OS `free`/`malloc` calls, or system call overhead.

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        MEMORY ARENA CHUNK BLOCK-REUSE FREELIST                         │
└────────────────────────────────────────────────────────────────────────────────────────┘
  Active Execution (Batch Ingestion)
  Chunk 0 [4 MiB]           Chunk 1 [4 MiB]           Chunk 2 [4 MiB]
  ┌───────────────────────┐ ┌───────────────────────┐ ┌───────────────────────┐
  │ [Used Memory]     ▲   │ │ [Used Memory]     ▲   │ │ [Used Memory]   ▲     │
  │                   │   │ │                   │   │ │                 │     │
  └───────────────────┼───┘ └───────────────────┼───┘ └─────────────────┼─────┘
                      Bump Pointer              Bump Pointer            Bump Pointer
  
  arena.reset() -> O(1) Pointer Rewind (Pages Retained in Physical RAM Freelist)
  Chunk 0 [4 MiB]           Chunk 1 [4 MiB]           Chunk 2 [4 MiB]
  ┌───────────────────────┐ ┌───────────────────────┐ ┌───────────────────────┐
  │ ▲                     │ │ ▲                     │ │ ▲                     │
  │ │                     │ │ │                     │ │ │                     │
  └─┼─────────────────────┘ └─┼─────────────────────┘ └─┼─────────────────────┘
    Bump Pointer -> 0         Bump Pointer -> 0         Bump Pointer -> 0
    (Zero Page Faults, Zero OS Malloc/Free Calls on Next Iteration)
```

---

## 3. Generational Compacting String Pool (`StringPool`)

Log compaction engines store variable text values in string dictionaries. Traditional string interners grow indefinitely, leading to memory bloat over weeks of runtime.

[`include/librazobill/StringPool.hpp`](../../include/librazobill/StringPool.hpp) implements a generational string interning pool equipped with heterogeneous hashing, mark-and-sweep garbage collection, and in-place compaction:

```cpp
class StringPool {
public:
    std::uint32_t intern(std::string_view str);
    std::string_view get(std::uint32_t id) const;

    // Heterogeneous lookup avoiding temporary allocations
    bool contains(std::string_view str) const;

    // Mark-and-sweep garbage collection
    void gc(const std::unordered_set<std::uint32_t>& activeIds);
    void compact();
};
```

### Lifecycle & GC Algorithm
1. **Heterogeneous Interning**: Incoming slot strings are looked up via transparent string-view hashing (`XXH64`). If present, the existing 32-bit integer ID is returned without temporary `std::string` heap allocations; if new, the string is appended to a contiguous backing buffer.
2. **Mark Phase**: During sliding-window checkpoints or table compaction cycles, the caller passes the set of actively referenced string IDs (`activeIds`).
3. **Sweep & Compaction**: Unreferenced strings are discarded. Active strings are shifted monotonically toward the front of the buffer, compacting storage and defragmenting memory without pointer invalidation.

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                       STRING POOL COMPACTION & DEFRAGMENTATION                         │
└────────────────────────────────────────────────────────────────────────────────────────┘
  Before GC (Fragmented String Buffer with Tombstones):
  ┌──────────────┬──────────────┬──────────────┬──────────────┬──────────────┐
  │ ID 0: Active │ ID 1: Dead   │ ID 2: Active │ ID 3: Dead   │ ID 4: Active │
  └──────────────┴──────────────┴──────────────┴──────────────┴──────────────┘
         │                             │                             │
         ▼ Mark & Shift Monotonically  ▼                             ▼
  After GC (Dense Contiguous Array, Zero Fragmented Slack):
  ┌──────────────┬──────────────┬──────────────┬─────────────────────────────┐
  │ ID 0: Active │ ID 2: Active │ ID 4: Active │ Free Space Reclaimed        │
  └──────────────┴──────────────┴──────────────┴─────────────────────────────┘
```

