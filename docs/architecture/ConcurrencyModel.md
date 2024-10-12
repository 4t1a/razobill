# Concurrency Model & Multi-Threading Architecture

`librazobill` achieves multi-core scaling by structuring its data plane as an embarrassingly parallel, lock-free pipeline. It decouples high-throughput worker threads from central coordination locks, eliminating lock contention and cache invalidation.

---

## 1. Concurrency Architecture

```
                                  Input Batch (e.g. 100,000 Log Lines)
                                                    │
                                  ┌─────────────────▼─────────────────┐
                                  │      Chunk Partitioning Logic     │
                                  │   (Chunk Size: e.g. 2,000 Lines)  │
                                  └─────────────────┬─────────────────┘
                                                    │
                 ┌──────────────────────────────────┼──────────────────────────────────┐
                 │                                  │                                  │
      ┌──────────▼──────────┐            ┌──────────▼──────────┐            ┌──────────▼──────────┐
      │   Worker Thread 0   │            │   Worker Thread 1   │            │   Worker Thread N   │
      ├─────────────────────┤            ├─────────────────────┤            ├─────────────────────┤
      │ Thread-Local Slots  │            │ Thread-Local Slots  │            │ Thread-Local Slots  │
      │ Thread-Local Buffer │            │ Thread-Local Buffer │            │ Thread-Local Buffer │
      │ Local Mismatch Count│            │ Local Mismatch Count│            │ Local Mismatch Count│
      └──────────┬──────────┘            └──────────┬──────────┘            └──────────┬──────────┘
                 │                                  │                                  │
                 └──────────────────────────────────┼──────────────────────────────────┘
                                                    │
                                  ┌─────────────────▼─────────────────┐
                                  │     Lock-Free Aggregator Join     │
                                  │  (Combines Unique Form Hashes)    │
                                  └───────────────────────────────────┘
```

---

## 2. Lock-Free Data Plane & Thread-Local Storage

### Zero Cross-Thread Synchronization
During the parsing and reconstruction pass:
- **No shared mutable state**: Worker threads operate on immutable string views of the input array.
- **Thread-local scratch buffers**: Each worker thread maintains its own instance of `SlotRecord`, `std::string` scratch buffers, and local metrics.
- **No mutexes on the hot path**: Workers do not touch shared locks or atomic fetch-and-add counters per row.

### Preventing False Sharing
When multiple CPU cores write to variables located on the same 64-byte hardware cache line, CPU cache coherency protocols force repeated invalidation of L1/L2 caches across cores (known as false sharing).
`librazobill` protects against this by ensuring:
1. `SlotRecord` is explicitly 64-byte aligned (`alignas(64)`).
2. Per-thread metrics (e.g., `localMismatches[numThreads]`) are padded or allocated separately to ensure each thread's counters reside in separate cache lines.

---

## 3. Work-Stealing `ThreadPool`

[`include/librazobill/ThreadPool.hpp`](../../include/librazobill/ThreadPool.hpp) implements a cache-aware thread pool:

```cpp
class ThreadPool {
public:
    explicit ThreadPool(std::size_t numThreads = std::thread::hardware_concurrency());
    ~ThreadPool();

    template <typename Func>
    void parallelFor(std::size_t totalItems, std::size_t chunkSize, Func&& func);
};
```

### Partitioning & Scheduling
- `parallelFor` breaks large inputs into dynamic chunks (typically 1,000 to 4,000 messages).
- When a worker finishes its assigned chunk, it fetches the next chunk atomically via an atomic index increment (`std::atomic<std::size_t>`).
- If one chunk encounters complex multiline logs while another processes short messages, fast workers dynamically absorb remaining chunks, ensuring balanced core utilization.

---

## 4. Python GIL Release Semantics

In Python environments, CPU-bound multi-threading is constrained by the Global Interpreter Lock (GIL).
In `pyrazobill`:
```cpp
m.def("auditParallel", [](const std::vector<std::string>& lines, core::ScanMode mode, std::size_t numThreads) {
    AuditResult res;
    {
        py::gil_scoped_release release; // GIL explicitly dropped here
        res = auditBatchParallel(lines, mode, numThreads); // Full multi-core execution
    } // GIL reacquired cleanly before returning
    return res;
});
```
By dropping the GIL inside a scoped block, `pyrazobill` allows all physical and logical CPU cores to execute simultaneously at native C++ speeds.
