# pyrazobill

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="../../assets/Logo.jpeg">
    <img src="../../assets/Logo-light.jpeg" alt="Razobill Logo" width="300">
  </picture>
</p>

<p align="center">
  <a href="https://pypi.org/project/pyrazobill/"><img src="https://img.shields.io/pypi/v/pyrazobill.svg" alt="PyPI Package Version"></a>
  <a href="https://opensource.org/licenses/Apache-2.0"><img src="https://img.shields.io/badge/License-Apache_2.0-blue.svg" alt="License: Apache 2.0"></a>
  <a href="https://doi.org/10.5281/zenodo.23287286"><img src="https://img.shields.io/badge/DOI-10.5281%2Fzenodo.23287286-blue.svg" alt="DOI: 10.5281/zenodo.23287286"></a>
</p>

Python client library for the Razobill telemetry decomposition engine.
It splits unstructured log lines into structured templates, numbers, and words, and restores original strings bit-for-bit.
Built with a high-performance C++20 core and includes a resilient pure-Python fallback.

---

## Lossless invertibility

Traditional log parsers discard formatting, alter timestamps, or leak unseen variable tokens into cluster definitions.
Razobill enforces an algebraic invariant across every parse:

$$\forall m \in \Sigma^*, \quad \text{RzJoin}(*\text{RzSplit}(m)) \equiv m$$

Every byte of whitespace, delimiter punctuation, and parameter data is preserved with zero information destruction.

> [!NOTE]
> **Lossless guarantee**: `RzJoin(*RzSplit(x)) == x` is guaranteed for all string and binary inputs. No whitespace, delimiter, or casing is modified.

---

## Quick installation

Install the precompiled binary wheel from PyPI:

```bash
pip install pyrazobill
```

Pre-built binary wheels support Python 3.9 through 3.13 on Linux (`manylinux`), macOS (`arm64`, `x86_64`), and Windows (`amd64`).

---

## Public API reference

The `pyrazobill` module exports five primary functions with zero memory leaks:

| Function | Signature | Description |
| :--- | :--- | :--- |
| `RzSplit` | `(line: Union[str, bytes], mode="semantic") -> RzRecord` | Splits a message into template, numbers, and words. |
| `RzJoin` | `(*record) -> Union[str, bytes]` | Reconstructs the exact raw log line byte-for-byte. |
| `RzTemplate` | `(line: Union[str, bytes], mode="semantic") -> Union[str, bytes]` | Extracts the template string without allocating value lists. |
| `RzHash` | `(line: Union[str, bytes]) -> int` | Computes 64-bit xxHash64 (seed 0) matching DuckDB and ClickHouse. |
| `RzSplitMany` | `(lines: Iterable, mode="semantic") -> List[RzRecord]` | Parses an iterable of messages in parallel releasing the GIL. |

---

## Usage

### Splitting and joining lines

```python
import pyrazobill as rz

# 1. Parse a log message
line = "Accepted publickey for ubuntu from 192.168.1.100 port 54322 ssh2"
record = rz.RzSplit(line)

print(record.template)
# 'Accepted publickey for {s} from {s} port {i} {s}'

print(record.ints)
# [54322]

print(record.strs)
# ['ubuntu', '192.168.1.100', 'ssh2']

# 2. Compute deterministic 64-bit template hash
print(rz.RzHash(record.template))
# 64-bit integer matching ClickHouse xxHash64() exactly

# 3. Restore the exact original text
restored = rz.RzJoin(*record)
assert restored == line
```

---

### Batch processing with multi-threading

Use `RzSplitMany` to parse collections of messages in parallel:

```python
import pyrazobill as rz

lines = [
    "Worker 1 finished job in 12 ms",
    "Worker 2 finished job in 19 ms",
    "Worker 3 finished job in 8 ms",
]

records = rz.RzSplitMany(lines)
for rec in records:
    print(rec.template, rec.ints)
```

`RzSplitMany` releases the Python GIL, allowing background worker threads to parse chunks simultaneously.

---

## Scientific provenance

Razobill is grounded in information-theoretic invariants established in:

> **Atta, Moustafa Mahmoud (2026).** *Latitude and Doubt: An exact ledger of meaning in messages with measurements on the genetic code and on English.*
> Zenodo. DOI: [10.5281/zenodo.23287286](https://doi.org/10.5281/zenodo.23287286)

---

## Documentation

- [Python developer guide](../../docs/how-to/python.md)
- [Python API reference](../../docs/reference/python-api.md)
- [Lossless invertibility](../../docs/explanation/lossless-invertibility.md)
- [ClickHouse & chDB guide](../../docs/how-to/chdb.md)
