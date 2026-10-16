# Razobill

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/Logo.jpeg">
    <img src="assets/Logo-light.jpeg" alt="Razobill Logo" width="340">
  </picture>
</p>

<p align="center">
  <a href="https://github.com/razobill/razobill/actions/workflows/CI.yml"><img src="https://github.com/razobill/razobill/actions/workflows/CI.yml/badge.svg" alt="CI Status"></a>
  <a href="https://github.com/razobill/razobill/actions/workflows/Release.yml"><img src="https://github.com/razobill/razobill/actions/workflows/Release.yml/badge.svg" alt="Release Status"></a>
  <a href="https://github.com/duckdb/community-extensions/tree/main/extensions/razobill"><img src="https://img.shields.io/badge/DuckDB%20Community-razobill-blue?logo=duckdb" alt="DuckDB Community Extension"></a>
  <a href="https://pypi.org/project/pyrazobill/"><img src="https://img.shields.io/pypi/v/pyrazobill.svg" alt="PyPI Package Version"></a>
  <a href="https://opensource.org/licenses/Apache-2.0"><img src="https://img.shields.io/badge/License-Apache_2.0-blue.svg" alt="License: Apache 2.0"></a>
</p>

Razobill is an exact, invertible telemetry engine that turns unstructured log strings into queryable columnar data.
It splits every log line into a constant template, an array of 64-bit integers, and an array of variable words.
Joining those components reconstructs the original raw bytes bit-for-bit with zero data loss.

**Website & Interactive Visualizer**: [razobill.github.io/razobill](https://razobill.github.io/razobill/)

---

## Quick installation

Choose the runtime environment for your stack.

### Python package

Install from PyPI:

```bash
pip install pyrazobill
```

Pre-compiled binary wheels cover Python 3.9 through 3.13 on Windows (x86-64), Linux (x86-64), and macOS (Apple Silicon and Intel). On Linux ARM architectures (such as AWS Graviton), `pip install` builds from source via the source distribution (`sdist`), or runs via the built-in pure-Python fallback.

### DuckDB extension

Install and load directly from the official DuckDB Community Repository (DuckDB 1.2+ and upcoming 2.0):

```sql
INSTALL razobill FROM community;
LOAD razobill;
```

For offline environments or custom builds, load the compiled binary extension directly:

```sql
LOAD 'build/razobill.duckdb_extension';
```

*(Both current stable DuckDB and the upcoming DuckDB 2.0.0 release are continuously built and validated in the official DuckDB community extensions matrix via `ref` and `ref_next`).*

### C ABI shared library

Link against the native C++20 shared library `razobill.dll` or `librazobill.so`:

```c
#include "librazobill/RazobillC.h"
```

---

## First success in five minutes

Parse a message into structured columns and reconstruct the original text.

### In Python

Run this self-contained script in your terminal:

```python
import pyrazobill as rz

# Split a log line into a template and parameter arrays
line = "Failed password for root from 192.168.1.100 port 22"
record = rz.RzSplit(line)

print(record.template)
# 'Failed password for {s} from {s} port {i}'

print(record.ints)
# [22]

print(record.strs)
# ['root', '192.168.1.100']

# Reconstruct the exact original string bit-for-bit
restored = rz.RzJoin(*record)
assert restored == line
```

> [!TIP]
> **Try it**: Run `python -c "import pyrazobill as rz; print(rz.RzSplit('port 22 open'))"` in your shell.

### In DuckDB SQL

Decompose and query logs directly using analytical SQL:

```sql
-- 1. Decompose a message into a structured composite struct
SELECT RzSplit('Failed password for root from 192.168.1.100 port 22') AS parsed;
-- Output struct:
-- {'template': 'Failed password for {s} from {s} port {i}', 'ints': [22], 'strs': ['root', '192.168.1.100']}

-- 2. Aggregate log frequencies across millions of rows by template
SELECT
    RzTemplate(message) AS template,
    COUNT(*) AS frequency
FROM system_logs
GROUP BY 1
ORDER BY frequency DESC;

-- 3. Reconstruct raw messages on demand
SELECT RzJoin(parsed.template, parsed.ints, parsed.strs) AS reconstructed
FROM (SELECT RzSplit('Failed password for root from 192.168.1.100 port 22') AS parsed);
```

---

## Core capabilities

Razobill provides a uniform data model across all supported environments.

| Environment | Primary Functions | Description |
| :--- | :--- | :--- |
| **Python** (`pyrazobill`) | `RzSplit`, `RzJoin`, `RzTemplate`, `RzHash`, `RzSplitMany` | High-throughput parsing releasing the Python GIL for multithreaded processing. |
| **DuckDB** (`ext/duckdb`) | `RzSplit`, `RzJoin`, `RzTemplate`, `RzHash` | Vectorized SQL functions operating directly on database string pointers without copies. |
| **ClickHouse & chDB** | `xxHash64`, Python ADBC | Bit-identical xxHash64 partition keys and additive MergeTree form indexing. |
| **C ABI** (`include/librazobill`) | `RzSplit`, `RzJoin`, `RzTemplate`, `RzFree` | Zero-allocation C ABI with stable binary layout and zero-copy string views. |
| **OTel Collector** (`ext/otel`) | `razobillprocessor` | Edge processor replacing string bodies with templates and slice attributes. |
| **Vector** (`ext/vector`) | `razobill-vector` | High-speed Rust crate transform streaming columnar stripes to object storage. |

---

## Lossless invertibility

Traditional log parsers discard formatting, alter timestamps, or leak unseen variable tokens into cluster definitions.
Razobill enforces an algebraic invariant across every parse:

$$\text{RzJoin}(\text{RzSplit}(x)) = x$$

Every byte of whitespace, delimiter punctuation, and parameter data is preserved.
You can store logs as compact columnar Parquet files and restore the exact original raw byte stream during compliance audits or security forensics.

> [!NOTE]
> **Real-world impact**: Eliminates up to 92% of raw log storage bytes in Parquet ZSTD stripes while preserving 100% of forensic raw bytes.

---

## Performance and benchmarks

All figures come from the project's own benchmarks on one Windows 11 machine with 12 logical cores. Reproduction scripts and raw run artifacts are located in [`tools/BenchDrain.py`](tools/BenchDrain.py), [`tools/BenchCLP.py`](tools/BenchCLP.py), and [`evidence/`](evidence/).

| Result | Measurement | Context |
| :--- | :--- | :--- |
| **Round trip** | 0 mismatches | 69.6 million lines, 4,861 log files, 6.82 GB audit |
| **Speed, one thread** | 2.5× to 15.1× faster than Drain3 | Five LogHub datasets (SSH, Linux, Proxifier, Zookeeper, BGL), average 5.6× |
| **Speed, long run** | 0.09 s vs 26.5 s | 100,000 SSH lines. Drain3 slowed to 3,779 lines/s |
| **Speed, 12 threads** | 29 million lines/s average | Peak 69.4 million lines/s on the SSH dataset |
| **On disk in DuckDB** | 6.29 MB to 2.88 MB | 134,815 events, 0.46× the raw table size |
| **Parquet on S3** | 93.8% smaller than raw text | 20,000 events. 27.3% smaller than plain Parquet |

For detailed methodology and full comparisons, see [Benchmark methodology and empirical results](docs/explanation/benchmarks.md).

---

## Razobill and CLP

YScope's Compressed Log Processor (CLP, OSDI 2021) is an established system used in production (e.g., at Uber) that also separates logs into a log type and variable values to achieve lossless compression and fast search.

Razobill shares this core conceptual motivation, but differs in architecture and deployment target:

- **In-database relational columns vs. custom archive format**: CLP encodes logs into custom compressed archives (`.clp`) and intermediate representations (CLP IR) queried through dedicated CLP command-line utilities. Razobill decomposes lines into standard relational columns (`STRUCT`, `VARCHAR`, `BIGINT[]`, `VARCHAR[]`, and `UInt64` xxHash64 partition keys) natively inside general-purpose analytical databases (DuckDB, ClickHouse, chDB) and standard columnar files (Parquet with standard ZSTD or Snappy compression).
- **Analytical SQL integration**: Because Razobill outputs standard relational arrays and scalar values, users can immediately apply SQL filters, aggregations, window functions, and joins across parsed log attributes without specialized search daemons.
- **Execution model**: CLP is a full end-to-end log management and search system. Razobill is an embedded tokenization engine and C/Python/DuckDB extension library.

Both engines were benchmarked head-to-head on identical hardware using [`tools/BenchCLP.py`](tools/BenchCLP.py): both achieved 100% exact bit-for-bit lossless roundtripping with 0 mismatches. CLP achieves higher raw file compression via its specialized dictionary bitstream (11.6× to 48.7× vs 8.0× to 17.9× for Parquet), while Razobill's `RzJoin` reconstructs original strings faster or on par (594k to 770k lines/s vs 390k to 753k lines/s) and provides direct SQL queryability.

---

## Known limits

We designed Razobill for specific production trade-offs. Here is where it falls short:

- **It is not a clustering tool**: On the project's 16-dataset benchmark, median grouping accuracy was 0.808 for Razobill versus 0.871 for Drain3. Razobill also retains more unique templates (median 70.5 vs 45.5). If your primary goal is finding the absolute minimum number of cluster archetypes regardless of variable loss, Drain3 is better suited. If you need lossless reconstruction and SQL analytical queries, Razobill is designed for that.
- **The benchmarks are author-run**: All benchmarks were measured on a single author machine under Windows 11. We encourage verifying throughput on your specific production hardware and workloads.
- **S3 cost models use emulators**: The S3 storage savings reported in evidence benchmarks were measured using LocalStack and simulated cost equations, rather than production AWS billing statements.
- **Platform support constraints**: Pre-compiled DuckDB community binaries cover Linux (x86-64, aarch64), macOS (Apple Silicon, Intel), and Windows MSVC (x86-64). WebAssembly and MinGW builds are currently not included.

---

## Citation and license

Razobill is licensed under the [Apache License, Version 2.0](LICENSE).

If you use Razobill in academic, scientific, or benchmark publications, please retain the [NOTICE](NOTICE) file and cite the foundational paper:

```bibtex
@software{atta2026razobill,
  author       = {Atta, M. M.},
  title        = {librazobill: Exact Lossless Log Parsing and Information-Theoretic Storage Engine},
  year         = {2026},
  publisher    = {Zenodo},
  doi          = {10.5281/zenodo.23287286},
  url          = {https://doi.org/10.5281/zenodo.23287286}
}
```

---

## Where to go next

Navigate the documentation system:

- **Interactive Web Visualizer**: [razobill.github.io/razobill](https://razobill.github.io/razobill/)
- **DuckDB Community Extension**: [duckdb/community-extensions](https://github.com/duckdb/community-extensions/tree/main/extensions/razobill)
- **Tutorials**: [Getting started in 5 minutes](docs/tutorials/getting-started.md)
- **How-to guides**:
  - [DuckDB extension guide](docs/how-to/duckdb.md)
  - [ClickHouse and chDB integration](docs/how-to/chdb.md)
  - [Python developer guide](docs/how-to/python.md)
  - [OpenTelemetry integration](docs/how-to/opentelemetry.md)
  - [Vector integration](docs/how-to/vector.md)
  - [AWS S3 storage compaction](docs/how-to/s3-compaction.md)
  - [C++ developer guide](docs/how-to/cpp-developer.md)
  - [Hardware acceleration with SIMD](docs/how-to/neon-simd.md)
- **Reference**:
  - [DuckDB SQL reference](docs/reference/sql-functions.md)
  - [Python API reference](docs/reference/python-api.md)
  - [Universal C ABI reference](docs/reference/c-abi.md)
  - [C++20 engine reference](docs/reference/cpp-api.md)
  - [Deprecation policy](docs/reference/deprecation-policy.md)
  - [Release governance](docs/reference/release-governance.md)
- **Explanations**:
  - [Architecture overview](docs/explanation/architecture.md)
  - [Lossless invertibility](docs/explanation/lossless-invertibility.md)
  - [Semantic versus exact scanning](docs/explanation/semantic-vs-exact.md)
  - [Data-oriented design](docs/explanation/data-oriented-design.md)
  - [SIMD vectorization](docs/explanation/simd-vectorization.md)
  - [Concurrency model](docs/explanation/concurrency-model.md)
  - [Benchmark methodology](docs/explanation/benchmarks.md)

---

## Citation

If you use Razobill in academic research, benchmarks, or systems software, please cite both the foundational theory paper and the software implementation:

```bibtex
@article{atta2026latitude,
  author  = {Atta, Moustafa Mahmoud},
  title   = {Latitude and Doubt: An exact ledger of meaning in messages with measurements on the genetic code and on English},
  journal = {Zenodo Preprint},
  year    = {2026},
  doi     = {10.5281/zenodo.23287286},
  url     = {https://doi.org/10.5281/zenodo.23287286}
}

@software{razobill2026,
  author    = {Atta, M. M. and contributors},
  title     = {Razobill: In-Database Telemetry Decomposition and Exact Lossless Compaction},
  year      = {2026},
  publisher = {GitHub},
  url       = {https://github.com/razobill/razobill}
}
```

A machine-readable [CITATION.cff](CITATION.cff) is provided in the repository root.
