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
</p>

Python client library for the Razobill telemetry decomposition engine.
It splits log lines into structured templates, numbers, and words, and restores original strings bit-for-bit.
Built with a high-performance C++20 core and includes a resilient pure-Python fallback.

---

## Quick installation

Install the precompiled binary wheel from PyPI:

```bash
pip install pyrazobill
```

Pre-built binary wheels support Python 3.9 through 3.13 on Linux (`manylinux`), macOS (`arm64`, `x86_64`), and Windows (`amd64`).

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

# 2. Restore the exact original text
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

## Documentation

Full documentation is available in the project repository:

- [Python developer guide](../../docs/how-to/python.md)
- [Python API reference](../../docs/reference/python-api.md)
- [Lossless invertibility](../../docs/explanation/lossless-invertibility.md)
