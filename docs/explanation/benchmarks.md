# Benchmark methodology and empirical results

Reproducible performance measurements, memory scaling, and comparison against Drain3.
Read this article if you want to inspect benchmark numbers, test corpora, and testing methodology.
Skip this article if you only need the developer guides.

---

## Benchmark environment

All benchmarks reported below were measured on an AMD Ryzen 9 7950X 16-Core Processor (32 Threads, 64 GB DDR5 RAM) running Linux kernel 6.8 with GCC 13.2 `-O3`.
Results were collected using `BenchCore.exe` and `BenchStorage.exe`.

---

## Throughput comparison against Drain3

Head-to-head comparison evaluating parsing throughput on standard LogHub benchmark corpora:

| Dataset | Total Lines | Drain3 Throughput (lines/sec) | Razobill Throughput (lines/sec) | Speedup Factor |
| :--- | :--- | :--- | :--- | :--- |
| **Android v2** | 2,000 | 24,100 | **2,840,000** | **117x** |
| **BGL** | 2,000 | 18,900 | **2,620,000** | **138x** |
| **Hadoop** | 2,000 | 21,300 | **2,490,000** | **116x** |
| **Linux Syslog** | 2,000 | 29,400 | **3,110,000** | **105x** |
| **SSH** | 2,000 | 31,200 | **3,450,000** | **110x** |

### Why Razobill is over 100x faster

1. **Branchless SIMD scanning**: Razobill processes delimiters in 32-byte chunks using AVX2 instructions, whereas Drain3 scans characters sequentially in Python.
2. **Deterministic state machine**: Drain3 maintains a variable-depth tree search requiring dictionary lookups and tree traversal per token.
   Razobill executes a single linear pass over the message buffer.
3. **Zero heap allocation**: Drain3 creates thousands of Python string and list objects per message.
   Razobill operates directly on raw memory pointers.

---

## Memory scaling under load

Memory utilization evaluated during continuous streaming of 100,000 log events:

| Metric | Drain3 | Razobill |
| :--- | :--- | :--- |
| **Peak heap memory** | 182 MB | **14 MB** |
| **Parse tree nodes** | 14,280 objects | **0 objects** |
| **Garbage collection overhead** | Continuous Python GC cycles | **Zero GC pressure** |

---

## Parquet storage compaction

Storage footprint after ingesting 100,000 lines of SSH authentication logs into analytical storage:

| Format | Storage Size | Compression vs Raw |
| :--- | :--- | :--- |
| **Raw text logs** | 9.8 MB | 1.0x (baseline) |
| **Raw text with Gzip** | 1.9 MB | 5.1x |
| **Razobill Parquet ZSTD** | **0.8 MB** | **12.2x** |

By decomposing messages into typed columns, columnar compressors (Snappy, ZSTD) achieve superior dictionary encoding and run-length compression over homogeneous numerical and string vectors.

---

## ClickHouse MergeTree storage footprint

Evaluated on a multi-system corpus of 134,815 log events inside embedded chDB (`evidence/ChStore.py`):

| Layout | Compression Codec | Storage Ratio vs LZ4 Baseline | Notes |
| :--- | :--- | :--- | :--- |
| **Raw message text** | Default LZ4 | 1.00x | High disk usage, slow regex scanning. |
| **Raw message text** | `CODEC(ZSTD(3))` | 0.53x | Standard production ClickHouse baseline. |
| **Razobill Additive layout** | `form_id UInt64` + ZSTD text | 0.56x | Preserves raw text with +6% overhead for instant index lookups. |
| **Razobill Compact layout** | Decomposed integer and string slots | **0.46x** | Maximum columnar compression on high-volume clusters. |
