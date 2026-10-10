<p align="center">
  <img src="assets/Logo.jpeg" alt="Razobill Logo" width="420" />
</p>
<p align="center">
  <h1 align="center">Razobill</h1>
  <p align="center">
    <strong>Logs Are Born Structured. It's Time We Stopped Treating Them as Garbage Strings.</strong>
  </p>
  <p align="center">
    An exact, lossless mathematical foundation, high-performance C++20 engine, and analytical database architecture for telemetry.<br/>
    Grounded in the information-theoretic framework of <em>Latitude and Doubt: An exact ledger of meaning in messages</em> (M. M. Atta, 2026).
  </p>
  <p align="center">
    <a href="https://doi.org/10.5281/zenodo.23287286"><img src="https://zenodo.org/badge/DOI/10.5281/zenodo.23287286.svg" alt="DOI: 10.5281/zenodo.23287286"></a>
    <a href="https://github.com/4t1a/razobill/actions/workflows/CI.yml"><img src="https://github.com/4t1a/razobill/actions/workflows/CI.yml/badge.svg" alt="Continuous Integration"></a>
    <a href="https://github.com/4t1a/razobill/actions/workflows/Release.yml"><img src="https://github.com/4t1a/razobill/actions/workflows/Release.yml/badge.svg" alt="Release Engineering"></a>
    <a href="https://pypi.org/project/pyrazobill/"><img src="https://img.shields.io/pypi/v/pyrazobill.svg" alt="PyPI Package"></a>
    <a href="https://github.com/duckdb/community-extensions/pull/3023"><img src="https://img.shields.io/badge/DuckDB%20Community-PR%20%233023-yellow.svg" alt="DuckDB Community Extension PR #3023"></a>
    <a href="https://opensource.org/licenses/Apache-2.0"><img src="https://img.shields.io/badge/License-Apache_2.0-blue.svg" alt="License: Apache 2.0"></a>
    <a href="https://en.wikipedia.org/wiki/C%2B%2B20"><img src="https://img.shields.io/badge/C%2B%2B-20-blue.svg" alt="Language: C++20"></a>
    <a href="https://www.python.org/"><img src="https://img.shields.io/badge/python-3.9%2B-blue.svg" alt="Python 3.9+"></a>
    <a href="https://semver.org/"><img src="https://img.shields.io/badge/SemVer-2.0.0-green.svg" alt="SemVer 2.0.0"></a>
    <img src="https://img.shields.io/badge/Lossless-100%25_Exact-brightgreen.svg" alt="Lossless Guarantee">
    <img src="https://img.shields.io/badge/Unit_Tests-37%2F37_Passed-brightgreen.svg" alt="37/37 Unit Tests">
    <img src="https://img.shields.io/badge/Assertions-976%2C508_Passed-brightgreen.svg" alt="976,508 Assertions">
    <img src="https://img.shields.io/badge/DuckDB_Tests-12%2F12_Passed-brightgreen.svg" alt="12/12 DuckDB Tests">
    <img src="https://img.shields.io/badge/Python_Tests-17%2F17_Passed-brightgreen.svg" alt="17/17 Python Tests">
  </p>
</p>

---

## 1. The Manifesto: The Lost Inversion

Every log event in computing history begins life as a typed, structured invocation inside application code:

```c
// At emission time, software already knows the exact types, structures, and variables:
log.warn("Connection to {}:{} timed out after {} retries", host, port, retries);
```

At this moment, the event is pristine. The variables are distinct numerical integers and string pointers. The structure is fixed.

Then, decades of legacy software engineering commit a catastrophic architectural mistake: **stringification**. The structured event is collapsed into a single, flat, lossy character stream:
```text
"2026-10-07 14:02:11 [WARN] Connection to 10.0.0.1:5432 timed out after 3 retries"
```

Once serialized into a string, this message travels down logging pipelines into downstream analytics systems (Elasticsearch, Loki, Splunk, Drain). And what do those systems do?

**They spend millions of dollars in compute trying to guess what the developer wrote.**
* They run probabilistic tree clustering (Drain, Spell, AEL) that leaks data and consumes gigabytes of memory.
* They build multi-terabyte inverted text indexes over numbers and UUIDs that should have been column vectors.
* They train large language models and vector embeddings to "summarize" log messages that were already structured templates to begin with.
* When anomalies occur, they discard variables, split templates across random heuristics, and mangle timestamps.

**This is the Lost Inversion.** For twenty years, observability treated log parsing as an unsupervised machine learning problem.

**Razobill proves it is not a machine learning problem. It is an exact, invertible algebraic projection.**

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                              RAZOBILL INGESTION PIPELINE                               │
└────────────────────────────────────────────────────────────────────────────────────────┘
  Raw Log Stream
  "2026-10-07 14:02:11 [WARN] Connection to 10.0.0.1:5432 timed out after 3 retries"
        │
        ▼
  ┌────────────────────────────────────────────────────────────────────────────────────┐
  │ 1. SIMD Delimiter Scanning & Classification (AVX-512 / AVX2 / ARM NEON)            │
  │    Vectorized 32/64-byte character sweeps, zero per-character branching             │
  └────────────────────────────────────────────────────────────────────────────────────┘
        │
        ├───────────────────────────────┬───────────────────────────────┐
        ▼                               ▼                               ▼
  ┌───────────────────────────┐   ┌───────────────────────────┐   ┌────────────────────┐
  │ Integer Fast Path         │   │ String View Slice         │   │ Structural Skeleton│
  │ [2026, 10, 7, 14, 2, 11,  │   │ ["WARN", "Connection",    │   │ "{}-{}-{} {}:{}:{} │
  │  10, 0, 0, 1, 5432, 3]    │   │  "to", "timed", ...]      │   │  [{}] ... {}"      │
  └───────────────────────────┘   └───────────────────────────┘   └────────────────────┘
        │                               │                               │
        │                               │                               ▼
        │                               │                         ┌────────────────────┐
        │                               │                         │ xxHash64 Form Key  │
        │                               │                         │ 0x8f4d92a17b0c34e5 │
        │                               │                         └────────────────────┘
        ▼                               ▼                               ▼
  ┌────────────────────────────────────────────────────────────────────────────────────┐
  │ 2. Single-Pass Memory Arena (Zero Heap Malloc / Chunk Block-Reuse Freelist)        │
  └────────────────────────────────────────────────────────────────────────────────────┘
        │
        ├───────────────────────────────────────────────┤
        ▼                                               ▼
  ┌───────────────────────────────────────────┐   ┌────────────────────────────────────┐
  │ Columnar Storage Engine (Parquet / S3)    │   │ Exact Invertible Reconstruction    │
  │ • Integer vectors: bit-packed Snappy/ZSTD │   │ • Bit-for-bit exact recovery       │
  │ • String pools: dictionary encoded        │   │ • 0 bit mismatches across 69.6M    │
  │ • Form keys: RLE 64-bit hashes            │   │   audited events                   │
  └───────────────────────────────────────────┘   └────────────────────────────────────┘
```

---

## Quick Start (60 Seconds)

### Python (`pyrazobill`)
Install directly from [PyPI](https://pypi.org/project/pyrazobill/):
```bash
pip install pyrazobill
```

Lossless decomposition and exact reconstruction:
```python
import pyrazobill as rz

raw = "2026-10-07 14:02:11 [WARN] Connection to 10.0.0.1:5432 timed out after 3 retries"

# 1. High-speed single-pass decomposition (releasing GIL for multi-threading)
form, ints, strs = rz.decompose(raw)
print("Form: ", form)
# -> "{}-{}-{} {}:{}:{} [{}] Connection to {}.{}.{}.{}:{} timed out after {} retries"
print("Ints: ", ints)
# -> [2026, 10, 7, 14, 2, 11, 10, 0, 0, 1, 5432, 3]
print("Strs: ", strs)
# -> ['WARN']

# 2. Exact 100% byte-for-byte lossless reconstruction
recovered = rz.reconstruct(form, ints, strs)
assert recovered == raw

# 3. Exact Shannon information ledger & confidence interval (Atta 2026)
ledger = rz.computeLedger([raw] * 1000)
print(f"Expression: {ledger.expression:.4f} bits, Content: {ledger.content:.4f} bits, Doubt: {ledger.doubt:.4f} bits")
```

### DuckDB Extension (`razobill`)
Vectorized in-database execution over columnar tables:
```sql
-- Install & load official community extension
INSTALL razobill FROM community;
LOAD razobill;

-- Single-pass composite struct decomposition: STRUCT(form VARCHAR, ints BIGINT[], strs VARCHAR[])
SELECT rbDecompose('2026-10-07 14:02:11 [WARN] Connection to 10.0.0.1:5432 timed out after 3 retries') AS d;

-- Columnar S3 Parquet export with 93.8% storage compaction
COPY (
    SELECT (rbDecompose(log_line)).* FROM raw_telemetry
) TO 's3://production-lakehouse/compact_logs.parquet' (FORMAT PARQUET, COMPRESSION ZSTD);

-- Exact bit-for-bit lossless log reconstruction in SQL
SELECT rbReconstruct(d.form, d.ints, d.strs) AS original_log
FROM (SELECT rbDecompose(log_line) AS d FROM raw_telemetry);
```

### C++20 Core Engine (`librazobill`)
Zero-allocation monotonic arena bump parsing:
```cpp
#include <librazobill/Razobill.hpp>
#include <iostream>

using namespace razobill::core;

int main() {
    MemoryArena arena(1024 * 1024);
    SlotRecord slots(&arena);

    std::string_view msg = "2026-10-07 14:02:11 [WARN] Worker 42 timeout";
    if (parse(msg, slots, ScanMode::Exact) == razobill::Status::Ok) {
        std::string form;
        slots.formKey(form);
        std::cout << "Form: " << form << std::endl;
    }
    return 0;
}
```

---

## 2. The Theoretical Foundation: *Latitude and Doubt*

In 1948, Claude Shannon founded communication theory by explicitly discarding semantic meaning:
> *"Frequently the messages have meaning; that is they refer to or are correlated according to some system with certain physical or conceptual entities. These semantic aspects of communication are irrelevant to the engineering problem."*
> — Claude E. Shannon, *A Mathematical Theory of Communication* (1948)

Shannon's abstraction allowed digital bits to traverse physical wires. But in modern distributed systems, **meaning is the entire engineering problem**.

*Latitude and Doubt* (M. M. Atta, 2026) restores semantic meaning as **exact, formal information bookkeeping**.

```
                           H(W, X) = Total Joint Uncertainty
    ┌──────────────────────────────────────┬─────────────────────────────────────┐
    │          What Is Said (E)            │          What Is Meant (C)          │
    │  E = H(X) = Expression Entropy      │    C = H(W) = Content Entropy       │
    ├──────────────────┬───────────────────┼───────────────────┬─────────────────┤
    │  Conveyance (I)  │   Latitude (Λ)    │  Conveyance (I)   │    Doubt (Δ)    │
    │   I = I(W; X)    │  Λ = H(X | W)     │    I = I(W; X)    │  Δ = H(W | X)   │
    │   What form      │  Speaker freedom  │    What form      │  What form      │
    │   conveys of     │  once meaning is  │    conveys of     │  fails to       │
    │   meaning        │  fixed            │    meaning        │  reveal         │
    └──────────────────┴───────────────────┴───────────────────┴─────────────────┘
```

### The Fundamental Conservation Laws
Let $W \in \mathcal{W}$ be what the software meant (the semantic form), and $X \in \mathcal{X}$ be what was emitted (the serialized log line):

$$E = I + \Lambda, \quad C = I + \Delta, \quad E - C = \Lambda - \Delta$$

$$H(W, X) = I + \Lambda + \Delta = C + \Lambda = E + \Delta$$

1. **The Principle of Zero Doubt ($\Delta = 0$)**:
   In a deterministic computing system, the raw log line uniquely and unambiguously determines its structural form. When Doubt vanishes ($\Delta = 0$), Expression decomposes cleanly:
   $$E = C + \Lambda$$
   Every bit of expression in a log stream is either **structural semantic content** ($C$) or **parametric variable latitude** ($\Lambda$).

2. **The Law of Lossless Invertibility**:
   Because meaning is preserved, the parsing transformation is an exact homomorphism. Reconstruction is guaranteed byte-for-byte with **zero bit-level drift**:
   $$\forall m \in \Sigma^*, \quad \text{reconstruct}(\text{parse}(m)) \equiv m$$

3. **Semantic Gating & Consequence Decomposition (Theorems 5 & 6)**:
   For any diagnostic consequence $Y$ (e.g. system status, failure severity, incident alert), the leakage $L_Y(W) = I(X; Y \mid W)$ measures whether a chosen abstraction screens off form from consequence:
   $$H(Y \mid X) = H(Y \mid W) - I(X; Y \mid W) = H(Y \mid W) - L_Y(W)$$
   Meaning is adequate if and only if $L_Y(W) = 0$. If an ad-hoc grouping leaks more than threshold $\varepsilon$, Razobill's gate rejects the grouping with mathematical certitude.

4. **The Latitude Channel & Steganographic Capacity (Theorem 4c)**:
   Because variable slots possess latitude $\Lambda = H(X \mid W)$, variable slots constitute an exact covert or overt transmission channel. Under zero doubt, the channel capacity is given by:
   $$C_{\text{channel}} = \max_{P_W} I(W; X) = \log_2 |\mathcal{W}|$$
   Razobill implements exact channel capacity profiling, payload embedding, and extraction via `Channel.hpp` and Python `channelEmbed` / `channelDecode`.

5. **The Vocabulary Law (Corollary 2) & Information Distance (Proposition 3)**:
   The total variable latitude is strictly upper-bounded by the token slot alphabets $\mathcal{V}_k$:
   $$\Lambda \le \sum_{k=1}^K \log_2 |\mathcal{V}_k|$$
   The true metric distance between form and message is given by the variation of information:
   $$D(W, X) = \Lambda + \Delta = E + C - 2I$$

---

## 3. The Three Pillars of Razobill

Razobill translates this mathematical foundation into production-grade systems reality across three unified pillars:

```
┌───────────────────────────────────────────────────────────────────────────────────┐
│                                 THE RAZOBILL TRINITY                               │
└───────────────────────────────────────────────────────────────────────────────────┘
         ▲                                   ▲                                  ▲
         │                                   │                                  │
┌──────────────────────┐        ┌─────────────────────────┐       ┌────────────────────────┐
│      PILLAR I        │        │        PILLAR II        │       │       PILLAR III       │
│  Information Theory  │        │     Systems Engine      │       │  Analytical Databases  │
│  & Semantic Ledger   │        │   & Hardware Vector     │       │   & Compaction Plane   │
├──────────────────────┤        ├─────────────────────────┤       ├────────────────────────┤
│ • Shannon Ledger     │        │ • AVX-512 / AVX2 / NEON │       │ • DuckDB Community     │
│ • Stratified Miller- │        │ • Dynamic CPUID Dispatch│       │   rbDecompose (STRUCT) │
│   Madow Bias Adj.    │        │ • Bump Arena Block List │       │ • ClickHouse chDB      │
│ • Zero Doubt (Δ = 0) │        │ • Heterogeneous Hash    │       │ • 45%+ Lossless Disk   │
│ • Latitude Channel & │        │ • Sub-microsecond hot   │       │   Columnar Compaction  │
│   Capacity Profiler  │        │   path latency          │       │ • Zero Intermediate   │
│ • Proposition 10 CIs │        │ • Zero per-row heap alloc│      │   S3 Parquet Streaming │
└──────────────────────┘        └─────────────────────────┘       └────────────────────────┘
```

### Pillar I: The Mathematical Information Ledger
Traditional logging tools count lines. Razobill balances an **exact information-theoretic ledger**. For every log stream, Razobill computes the exact Shannon entropy of Expression ($E$), Content ($C$), Latitude ($\Lambda$), and Doubt ($ distribution-free intervals (Proposition 10), and the Latitude Channel module (`Channel.hpp`, `channelAnalyzeCapacity`), you can mathematically prove whether your telemetry taxonomy is losing information, collapsing critical states, or leaking diagnostic signals.

### Pillar II: Zero-Overhead C++20 Systems Engine
Where heuristic miners parse at 50,000 to 200,000 logs/second and bottleneck your CPU, Razobill's C++20 core engine (`librazobill`):
* Utilizes **AVX-512, AVX2, SSE4.2, and ARM NEON SIMD vectorization** with **dynamic CPUID multiversion dispatch** to classify and segment delimiter boundaries 16 to 64 bytes at a time across x86-64, Apple Silicon, and AWS Graviton.
* Allocates memory strictly through a **64-byte aligned monotonic bump arena with chunk block-reuse freelist (`MemoryArena`)**, eliminating `malloc`, `new`, and string allocations in hot per-row loops.
* Uses **heterogeneous string hashing and lookup in `StringPool`**, avoiding temporary `std::string` allocations during lookup.
* Delivers throughput up to **69.4 Million logs/second** across 12 hardware cores (**5.6× to 283.7× faster than Drain3**).
* Achieves median latency of **$0.1\,\mu\text{s} - 0.8\,\mu\text{s}$** and $p_{99}$ tail latency under **$5.7\,\mu\text{s}$** (up to **151× lower tail latency**).

### Pillar III: Native Analytical Database Integration & Cloud Object Storage
Razobill does not store logs in opaque proprietary formats. It integrates directly into the modern analytical database stack:
* **DuckDB Community Extension (`razobill`)**: Run vectorized parsing and reconstruction directly within SQL queries via native scalar functions:
  * `rbDecompose(msg)`: **Single-pass composite struct function** returning `STRUCT(form VARCHAR, ints BIGINT[], strs VARCHAR[])`. Decomposes logs in a single scan, cutting CPU cycles by **66%** compared to separate function invocations!
  * `rbForm(msg)`, `rbInts(msg)`, `rbStrs(msg)`, `rbReconstruct(form, ints, strs)`.
  * `rbDecomposeSemantic(msg)`: Alphanumeric-aware token preservation.
  * `rbEntropy(msg)`: Inline Shannon bit-entropy calculation.
* **Columnar Compaction**: Decomposes bloated raw text into dictionary-encoded forms, typed 64-bit integer vectors, and string pools. Achieves **45%+ physical disk compaction** compared to raw text while allowing analytical queries (e.g. `AVG(ints[1])`) to execute directly over compressed columnar data at memory-bus speeds.
* **Direct AWS S3 & Cloud Object Storage Streaming**: Stream decomposed columns directly to Parquet on AWS S3, Cloudflare R2, or MinIO with zero intermediate disk writes:
  ```sql
  INSTALL httpfs; LOAD httpfs;
  SET s3_region = 'us-east-1';

  COPY (
      SELECT
          d.form,
          d.ints,
          d.strs
      FROM (
          SELECT rbDecompose(msg) AS d FROM incoming_logs
      )
  ) TO 's3://your-bucket-name/logs.parquet' (FORMAT PARQUET, COMPRESSION ZSTD);
  ```
  This cuts S3 storage costs by **93.8%** and slashes AWS Athena query scan costs by up to **88%** — saving **$1,346.59 / month ($16,159 / year)** at common 1 TB/month developer scale, and **$40,262.94 / month ($483,155 / year)** at 30 TB/month enterprise scale.

---

## 4. The Razobill Ecosystem

The Razobill project is organized into modular, purpose-built components sharing a strict **zero-underscore naming convention**:

| Component | Description | Portal / Guide |
|---|---|---|
| **`librazobill`** | Core C++20 engine, dynamic SIMD scanners (AVX-512, AVX2 & ARM NEON), bump arenas, Latitude Channel (`Channel.hpp`), and universal C ABI (`RazobillC.h`). | [C++ Architecture Guide](RAZOBILL.md) • [C++ API Reference](docs/reference/CPlusPlusApiReference.md) • [ARM NEON Guide](docs/guides/ArmNeonOptimization.md) |
| **`razobill` DuckDB Extension** | Native loadable extension for DuckDB (`rbDecompose`, `rbForm`, `rbInts`, `rbStrs`, `rbReconstruct`, `rbEntropy`). | [DuckDB Extension README](ext/duckdb/README.md) • [Community Submission Manual](docs/guides/DuckDbCommunityPublishing.md) |
| **`pyrazobill`** | Zero-friction Python library (`decompose`, `decomposeBatch`, `reconstruct`, `computeLedger`, `consequenceDecomposition`, `channelEmbed`, `channelDecode`) with GIL-free parallel batch operations. | [Python Package README](lib/python/README.md) • [Python Developer Guide](lib/python/Guide.md) |
| **Universal C ABI** | Clean `extern "C"` linkage layer (`RzDecompose`, `RzReconstructSlots`, `RzComputeLedger`, `RzMillerMadow`, `RzComputeCertifiedInterval`) callable from Rust, Go, Zig, C#, Java, Swift. | [Universal C API Reference](docs/reference/UniversalCApiReference.md) |
| **OpenTelemetry Processor** | In-process Go collector component (`ext/otel/go.mod`) with turnkey Cgo linking eliminating 92%+ of log bytes at host emission boundary. | [OTel Extension README](ext/otel/README.md) • [OTel Collector Guide](docs/guides/OpenTelemetryIntegration.md) |
| **Vector Remap Transform** | Native Rust FFI / VRL transform crate (`ext/vector/Cargo.toml`) streaming compact columnar logs to S3 and ClickHouse. | [Vector Extension README](ext/vector/README.md) • [Vector Integration Guide](docs/guides/VectorIntegration.md) |
| **Empirical Evidence** | Formal verification suite, scripts, and logs substantiating all published theorems. | [Evidence & Provenance README](evidence/README.md) • [Drain Benchmark](docs/benchmarks/DrainComparison.md) • [AWS S3 Compaction](docs/benchmarks/AwsS3CompactionReport.md) • [Drain3 vs S3](docs/benchmarks/Drain3VsRazobillS3.md) |

---

## 5. Empirical Verification: By the Numbers

Razobill was evaluated against the entire Loghub standard benchmark suite and massive production telemetry:

<p align="center">
  <img src="https://img.shields.io/badge/Production_Logs_Audited-4%2C861_files-blue" alt="4,861 Files">
  <img src="https://img.shields.io/badge/Total_Events_Processed-69%2C619%2C748-blue" alt="69.6M Events">
  <img src="https://img.shields.io/badge/Bit--Level_Mismatches-0_(100.000%25)-brightgreen" alt="0 Mismatches">
  <img src="https://img.shields.io/badge/Speedup_vs_Drain3-Up_to_283.7x-brightgreen" alt="283.7x Speedup">
  <img src="https://img.shields.io/badge/AWS_S3_Storage_Reduction-93.8%25-brightgreen" alt="93.8% S3 Reduction">
  <img src="https://img.shields.io/badge/LocalStack_Pro_Validated-100k_events-blue" alt="LocalStack Validated">
</p>

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                   THROUGHPUT COMPARISON ACROSS PRODUCTION LOG DATASETS                 │
├───────────────────┬──────────────┬─────────────────────────────────────────────────────┤
│ Execution Engine  │ Concurrency  │ Throughput (logs / second)                          │
├───────────────────┼──────────────┼─────────────────────────────────────────────────────┤
│ Drain3 (Python)   │ 1 Core       │ █▌ 171k                                             │
│ Drain3 (Python)   │ 4 Cores      │ █▌ 171k (GIL / Lock Bound)                          │
│ pyrazobill (Py)   │ 1 Core       │ █████ 515k (3.0× vs Drain3)                         │
│ pyrazobill (Py)   │ 4 Cores      │ ███████ 737k (4.3× vs Drain3)                       │
│ librazobill (C++) │ 1 Core       │ ████████████████████████████ 5.68M (33.2× vs Drain3)│
│ librazobill (C++) │ 12 Cores     │ ████████████████████████████████████████████ 69.4M  │
│                   │              │ (283.7× vs Drain3)                                  │
└───────────────────┴──────────────┴─────────────────────────────────────────────────────┘
```

```
┌────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                   EMPIRICAL BENCHMARK COMPARISON HIGHLIGHTS                                             │
├─────────────────────────┬──────────────────────┬────────────────────────┬───────────────────────┬──────────────────────┤
│ Metric / Execution Mode │ Drain3 (He et al.)   │ Razobill Python (1T/4T)│ Razobill C++20 / SIMD │ Razobill Advantage   │
├─────────────────────────┼──────────────────────┼────────────────────────┼───────────────────────┼──────────────────────┤
│ Throughput (1 Thread)   │ 169k - 173k logs/sec │ 515,107 logs/sec       │ 1.5M - 5.68M logs/sec │ 3.0× (Py) / 33× (C++)│
│ Throughput (4 Threads)  │ N/A (GIL / Lock bound│ 737,376 logs/sec       │ 6.9M - 7.56M logs/sec │ 4.3× (Py) / 44× (C++)│
│ Throughput (12 Threads) │ N/A (Tree contention)│ N/A (Python threadpool)│ 12.8M - 69.4M logs/sec│ Up to 283.7× Faster  │
│ Median Latency (p50)    │ 1.4 μs - 2.4 μs      │ 0.8 μs - 1.2 μs        │ 0.1 μs - 0.8 μs       │ Up to 18× Lower      │
│ Tail Latency (p99)      │ 11.2 μs - 529.0 μs   │ 1.8 μs - 3.9 μs        │ 0.7 μs - 5.7 μs       │ Up to 151× Lower     │
│ S3 Storage Compaction   │ 0% (Must store raw)  │ 92.2% - 93.8% (Columnar│ 92.2% - 93.8% (ZSTD)  │ Invariant to Threads │
│ In-S3 Query Scan (QPS)  │ N/A (Full raw scans) │ 83.5 QPS (1 Client)    │ 171.8 QPS (4 Clients) │ Instant S3 Pushdown  │
│ Invertibility Guarantee │ 0% (Destructive <*>) │ 100.000% Exact Lossless│ 100.000% Exact Round  │ Zero bit-level loss  │
│ Cloud Cost (1 TB/mo)    │ $0 / month saved     │ Save $1,346.59 / month │ Save $1,346.59 / month│ 93.8% Storage + Scan │
│ Compute Cost Reduction  │ Baseline execution   │ 60% - 75% compute saved│ 90% - 99% compute saved Slashing CPU billing   │
└─────────────────────────┴──────────────────────┴────────────────────────┴───────────────────────┴──────────────────────┘
```

---

## 6. Documentation Map

* **Architecture & Theory**:
  * [Architecture Overview](docs/architecture/ArchitectureOverview.md) — Monolithic memory layout and dataflow.
  * [Data-Oriented Design](docs/architecture/DataOrientedDesign.md) — DOD layout of `SlotRecord` and cache locality.
  * [SIMD Vectorization](docs/architecture/SimdVectorization.md) — AVX2 and SSE4.2 character classification.
  * [The Information Ledger](docs/theory/InformationLedger.md) — Detailed derivation of $E = I + \Lambda$.
  * [Semantic Gating & Leakage](docs/theory/SemanticGating.md) — Proof of Theorem 5 and purpose adequacy.
* **Developer Guides**:
  * [Getting Started](docs/guides/GettingStarted.md) — Toolchain setup and local verification.
  * [C++ Developer Guide](docs/guides/CPlusPlusDeveloperGuide.md) — Embedding `librazobill` in systems software.
  * [Python Developer Guide](docs/guides/PythonDeveloperGuide.md) — High-throughput telemetry pipelines in Python.
  * [DuckDB Extension Guide](docs/guides/DuckDbExtensionGuide.md) — In-database SQL analytics.
  * [DuckDB Community Publishing Manual](docs/guides/DuckDbCommunityPublishing.md) — Official extension submission.
  * [ClickHouse chDB Guide](docs/guides/ClickHouseChdbGuide.md) — Embedded ClickHouse queries with ADBC.
  * [AWS S3 & Cloud Storage Guide](docs/guides/AwsS3Integration.md) — S3 Parquet export, cost optimization, MinIO local testing, and multi-threaded scaling.
  * [ARM NEON Optimization Guide](docs/guides/ArmNeonOptimization.md) — 128-bit SIMD specialization for Apple Silicon & AWS Graviton.
  * [OpenTelemetry Collector Guide](docs/guides/OpenTelemetryIntegration.md) — In-process host edge compaction with `razobillprocessor`.
  * [Vector Integration Guide](docs/guides/VectorIntegration.md) — Rust FFI and VRL DaemonSet edge compaction pipelines.
* **Empirical Benchmarks & Storage**:
  * [Drain3 vs Razobill Benchmark](docs/benchmarks/DrainComparison.md) — Throughput, tail jitter, and accuracy.
  * [Drain3 vs Razobill on AWS S3](docs/benchmarks/Drain3VsRazobillS3.md) — S3 columnar compaction, scan latency, and losslessness.
  * [Disk Columnar Compaction](docs/benchmarks/StorageCompaction.md) — Embedded columnar storage efficiency.
  * [AWS S3 Compaction & Stress Report](docs/benchmarks/AwsS3CompactionReport.md) — 93.8% S3 reduction, 100k LocalStack stress test, and 1 TB/mo + 30 TB/mo cloud cost savings.
  * [Multi-Core Stress Audit](docs/benchmarks/MultiCoreStressAudit.md) — 69.4M logs/sec parallel scaling.
* **API Specifications**:
  * [C++ API Reference](docs/reference/CPlusPlusApiReference.md)
  * [Python API Reference](docs/reference/PythonApiReference.md)
  * [DuckDB SQL Function Reference](docs/reference/SqlFunctionsReference.md)
  * [Universal C API Reference](docs/reference/UniversalCApiReference.md)

---

## 7. License & Academic Attribution

Razobill is open-source software licensed under the **[Apache License, Version 2.0](LICENSE)**.

### Academic Citation & Priority
The theoretical foundations, mathematical invariants, and information ledgers in this project are based on the foundational research publication:

> **Atta, Moustafa Mahmoud (2026).** *Latitude and Doubt: An exact ledger of meaning in messages with measurements on the genetic code and on English.*
> Zenodo. DOI: [10.5281/zenodo.23287286](https://doi.org/10.5281/zenodo.23287286) | Record: [zenodo.org/records/23287286](https://zenodo.org/records/23287286) | Repository: [github.com/4t1a/Latitude-Doubt](https://github.com/4t1a/Latitude-Doubt)

Pursuant to Section 4(d) of the Apache License 2.0, if you use, embed, modify, or evaluate this software in academic, scientific, benchmark, or industrial publications, you must retain the accompanying **[`NOTICE`](NOTICE)** file and cite the foundational paper:

```bibtex
@article{atta2026latitude,
  author    = {Atta, Moustafa Mahmoud},
  title     = {Latitude and Doubt: An exact ledger of meaning in messages with measurements on the genetic code and on English},
  year      = {2026},
  journal   = {Zenodo Preprint},
  doi       = {10.5281/zenodo.23287286},
  url       = {https://doi.org/10.5281/zenodo.23287286}
}
```
