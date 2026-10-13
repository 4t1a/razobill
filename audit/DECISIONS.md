# Razobill Engineering Audit Decisions & Restatements

This document records architectural, mathematical, and environment decisions taken during the Razobill audit, restating any requirements into measurable targets where necessary.

---

## Decision 1: Target Platform Baseline and Toolchain
- **Context**: The audit is being performed on Windows x86_64 host (Intel Core i5-10500H, 6 cores / 12 threads).
- **Toolchain**: GCC 15.1.0 (MSYS2 UCRT64), CMake 4.1.1, Ninja 1.13.2, Python 3.13.11.
- **Decision**: Native C++20 compilation uses `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wnon-virtual-dtor -Woverloaded-virtual`. Static and unit tests run with MSYS2 MinGW-w64 toolchain. Sanitizers (ASan/UBSan/TSan) require specific compiler/runtime support on Windows or Linux CI; on Windows MinGW GCC, sanitizers will be executed where supported or validated through dedicated property and stress test harnesses.

---

## Decision 2: Audit Gates and Evidence Policy
- **Correctness Gate (Priority 1)**: Lossless round-trip condition: for all input messages $m \in \Sigma^*$, $\text{reconstruct}(\text{parse}(m)) \equiv m$ down to exact byte identity. Validated via property tests and exhaustive corpus sweeps.
- **Truth Gate (Priority 2)**: Every quantitative claim must be tagged `[PROVED]`, `[MEASURED]`, `[CITED]`, or `[CONJECTURE]`. Unlabeled claims are prohibited.
- **Reliability Gate (Priority 3)**: Arena allocation must guarantee zero heap `malloc` on parsing hot paths; hostile/malformed inputs must terminate in well-defined error results without crash, UB, or infinite loops.
- **Performance Gate (Priority 4)**: Benchmarks measured against baseline commits, reporting hardware, flags, dataset hash, runs, median, and spread.
- **Simplicity Gate (Priority 5)**: Minimal code footprint, clean RAII, zero-copy `std::string_view` where lifetime permits.

---

## Decision 3: Audit Phase Sequence
- **Phase 0**: Environment baseline, build verification, and initial test suite execution.
- **Phase 1**: Invariant & Correctness Audit (Scanner, Slots, Form extraction, Reconstruct round-trip).
- **Phase 2**: Memory Safety, Undefined Behavior & Security Audit (Arena, StringPool, bounds, integer overflow, fuzz targets).
- **Phase 3**: Concurrency, Thread Safety & Race Conditions (ThreadPool, Channel, thread-safety contracts).
- **Phase 4**: Mathematical & Information-Theoretic Rigor (Atta 2026 ledger formulas, Miller-Madow correction, confidence intervals, Gated Semantics).
- **Phase 5**: API Boundaries, C ABI, Public Contracts (SOLID boundaries, RAII, opaque C ABI, error containment).
- **Phase 6**: Performance, Micro-benchmarks & Storage Engine Hardening (SIMD dispatch, hot-path data orientation, columnar compaction).

---

## Decision 4: Production Hardening Targets & Operational Bounds
- **Context**: The phrase "handle all world traffic" from the original target document is an ungrounded marketing generalization. In production systems engineering (ClickHouse / DuckDB standards), capacity must be stated as measurable operational bounds.
- **Sustained Throughput Targets**:
  - Single-Core Pinned: $\ge 400\text{ MB/s}$ throughput ($\ge 4.0\text{ M msg/s}$ on representative 100-byte server logs).
  - Multi-Core Saturated (12 hardware threads): $\ge 2.5\text{ GB/s}$ throughput ($\ge 25.0\text{ M msg/s}$).
- **Bounded RSS per Worker with Backpressure**:
  - Batch Chunk Ceiling: Maximum $B \le 65,536$ messages per chunk.
  - Scratch Memory Ceiling: Each worker thread operates within a bounded $\le 32\text{ MB}$ bump arena with $O(1)$ bulk recycling between batches, preventing heap fragmentation.
  - Channel Backpressure: Bounded capacity queues block upstream producer threads when ingestion exceeds consumption thresholds, preventing unbounded heap inflation.
- **Lossless Raw Fallback Contract**:
  - For any input message $m$ where standard structural tokenization is inhibited (e.g. payload exceeds $16\text{ MB}$ `kMaxMessageBytes`, contains reserved delimiter bytes $0x11..0x13$, or is malformed), the engine wraps the message as a verbatim single-slot form (`\x12\x13S` with $strs[0] = m$).
  - Invariant: $\forall m \in \Sigma^*, \operatorname{reconstruct}(\operatorname{parse}(m)) \equiv m$ holds with zero exceptions, byte-for-byte.
- **Crash-Safe Storage**:
  - Parquet and DuckDB database files must be written to staging temporary paths (`.tmp`) and committed via atomic filesystem rename, with explicit write-ahead log flush (`CHECKPOINT`) before catalog registration.
- **Stateless Horizontal Sharding**:
  - Scale out via shared-nothing worker shards partitioned statelessly by ingestion stream hash (OpenTelemetry Collector stream ID, Vector partition key), committing self-contained columnar shards with embedded form dictionaries to object storage.
