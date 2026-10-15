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

Install the verified binary wheel from PyPI:

```bash
pip install pyrazobill
```

### DuckDB extension

Install and load directly from the official DuckDB Community Repository:

```sql
INSTALL razobill FROM community;
LOAD razobill;
```

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
| **Python** (`pyrazobill`) | `RzSplit`, `RzJoin`, `RzTemplate`, `RzSplitMany` | High-throughput parsing releasing the Python GIL for multithreaded processing. |
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
