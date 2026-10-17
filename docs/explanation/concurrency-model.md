# Concurrency model and thread safety

[Docs](../README.md) > [Explanations](../README.md#4-explanations-and-architecture-understand) > Concurrency model and thread safety

How Razobill scales across multi-core systems without locks on the query path.
Read this article if you want to understand thread management, GIL release in Python, and multi-core scaling.
Skip this article if you are processing single messages sequentially.

---

## The multi-core scaling challenge

In high-volume telemetry ingestion, logs arrive from thousands of concurrent microservices.
Common multi-threading architectures often hit synchronization bottlenecks:

- Global locks on template dictionaries create contention across CPU cores.
- Thread context switching and thread pool lock contention degrade throughput.
- Python GIL (Global Interpreter Lock) serializes execution across worker threads.

Razobill eliminates shared-state contention by designing parsing as an embarrassingly parallel, lock-free operation.

---

## Lock-free thread-local scratchpads

When executing inside a multi-threaded database (such as DuckDB) or across native worker threads:

- Each thread operates on an isolated `Scratch` buffer.
- Parsing routines allocate zero locks, atomic counters, or mutual exclusion primitives on the hot path.
- Memory allocated by thread $A$ is never read or written by thread $B$.

---

## Python GIL release in `RzSplitMany`

In Python, parsing millions of strings in a Python loop is CPU-bound and constrained by the GIL:

```python
# Sequential Python loop (GIL constrained)
records = [rz.RzSplit(line) for line in millions_of_lines]
```

When you call `rz.split_many(lines)` (or `rz.RzSplitMany`):

1. Python strings are transferred into internal string views.
2. For batches larger than 128 lines, the Python GIL is explicitly released using `py::gil_scoped_release`.
3. Background worker threads in `librazobill` parse chunks of messages simultaneously.
4. Python named tuples (`SlotRecord`) are constructed and returned to the caller once parallel work completes.

---

## Work-stealing thread pool (`ThreadPool.hpp`)

For batch tasks, librazobill includes a cache-aware work-stealing thread pool:

- Worker threads maintain local work queues.
- When an idle worker exhausts its local batch, it steals tasks from the tail of another worker's queue.
- This balances uneven workloads (such as mixed short syslog messages and long Java stack traces) across all CPU cores.

---

## Related

- [Systems architecture overview](architecture.md)
- [Batch processing in Python](../how-to/python.md)
- [Python API reference](../reference/python-api.md)
