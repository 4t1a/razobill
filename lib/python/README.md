<p align="center">
  <img src="../../assets/Logo.jpeg" alt="Razobill Logo" width="380" />
</p>

# pyrazobill

High-performance, lossless log parsing and information-theoretic semantic accounting library for Python, powered by **librazobill 1.0.0** C++20 core.

[![License: Apache 2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![Python: 3.9+](https://img.shields.io/badge/python-3.9%2B-blue.svg)](https://www.python.org/)
[![SemVer: 2.0.0](https://img.shields.io/badge/SemVer-2.0.0-green.svg)](https://semver.org/)
[![Lossless: 100%](https://img.shields.io/badge/Lossless-100%25-brightgreen.svg)](https://github.com/razobill/razobill)

---

## Highlights

- **Lossless Invertibility**: Guarantees $E = I + \Lambda$. Reconstruct raw logs byte-for-byte with $0$ bit-level mismatches (`reconstruct(record) == msg` or `reconstruct(formKey, ints, strs) == msg`).
- **Extreme Speed**: Up to **69.4 Million logs/second** across 12 hardware cores in C++ (**5.6× to 283.7× faster than Drain3**), and **515k – 737k+ logs/second** in Python.
- **Sub-Microsecond Latency**: Median latency $0.1\,\mu\text{s} - 0.8\,\mu\text{s}$, $p_{99}$ tail latency $< 5.7\,\mu\text{s}$ (up to **151× lower tail latency** than Drain).
- **GIL-Free Multi-Threading**: `decomposeBatch()` and `parseBatch()` release CPython's Global Interpreter Lock (GIL) to saturate all hardware threads.
- **Direct Columnar Decomposition**: `decompose()` and `decomposeBatch()` return lightweight `(formKey, ints, strs)` tuples ready for instant ingestion into Arrow, DuckDB, or Polars.
- **Native SQL Engine Interop**: Direct integration with **DuckDB** and **ClickHouse chDB** for analytical queries and 45%+ storage compaction.
- **Zero-Underscore Naming**: Clean, modern developer ergonomics (`parse`, `parseBatch`, `decompose`, `decomposeBatch`, `auditParallel`, `formKey`, `reconstruct`).

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│               CPYTHON RAII LIFETIME & BUFFER OWNERSHIP ARCHITECTURE                    │
└────────────────────────────────────────────────────────────────────────────────────────┘
  Python User Space
  rec = pyrazobill.parse("User 1042 logged in")
         │
         ▼
  ┌────────────────────────────────────────────────────────────────────────────┐
  │ PySlotRecord (Native C++ Extension Wrapper)                                │
  │ ┌────────────────────────────────────────────────────────────────────────┐ │
  │ │ msgBuffer: std::shared_ptr<std::string>                                │ │
  │ │ (Takes full heap ownership of raw string bytes; safe from Python GC)   │ │
  │ ├────────────────────────────────────────────────────────────────────────┤ │
  │ │ slots.strs: std::vector<std::string_view>                              │ │
  │ │ (Zero-copy slices point safely into msgBuffer; zero dangling pointers) │ │
  │ ├────────────────────────────────────────────────────────────────────────┤ │
  │ │ slots.ints: std::vector<int64_t>                                       │ │
  │ │ (Dense contiguous 64-bit integers ready for DuckDB / Arrow SIMD copy)  │ │
  │ └────────────────────────────────────────────────────────────────────────┘ │
  └────────────────────────────────────────────────────────────────────────────┘
         │
         ▼
  rec.reconstruct() == "User 1042 logged in" (100% Exact Round-Trip)
```

---

## Installation

### From Source
```bash
git clone https://github.com/razobill/razobill.git
cd razobill/lib/python
pip install .
```

### Editable Development Mode
```bash
pip install -e .
```

---

## Quickstart

```python
import pyrazobill

# 1. Parse an individual log message
msg = "2026-10-07 14:02:11 [WARN] Connection to 10.0.0.1:5432 failed after 3 retries"
record = pyrazobill.parse(msg, pyrazobill.ScanMode.Semantic)

print("Form Key    :", record.formKey)
# -> '2026-10-07 14:02:11 [WARN] Connection to {}:{} failed after {} retries'

print("Form Hash   :", record.formKeyHash)
# -> 1284729104857201948 (64-bit uint)

print("Integers    :", record.ints)
# -> [5432, 3]

print("Strings     :", record.strs)
# -> ['10.0.0.1']

# 2. Losslessly reconstruct the exact original string
rebuilt = record.reconstruct()
assert rebuilt == msg
print("Lossless Reconstruct:", rebuilt)

# 3. High-Speed direct tuple decomposition (zero wrapper overhead)
formKey, ints, strs = pyrazobill.decompose(msg, pyrazobill.ScanMode.Semantic)
print("Decomposed :", formKey, ints, strs)

# 4. Symmetrical tuple reconstruction
assert pyrazobill.reconstruct(formKey, ints, strs) == msg
```

---

## High-Speed Tuple Decomposition & Parallel Processing

For streaming pipelines into DuckDB, Polars, PyArrow, or AWS S3, `pyrazobill.decomposeBatch()` operates without creating Python wrapper objects and releases the GIL:

```python
import concurrent.futures
import pyrazobill

lines = [
    f"Worker {i%4} processed {100+i} items from 10.0.0.{i%250} port {5000+i}"
    for i in range(100_000)
]

# 1. Single-threaded batch decomposition (515k+ logs/sec)
tuples = pyrazobill.decomposeBatch(lines[:1000], pyrazobill.ScanMode.Semantic)
formKey, ints, strs = tuples[0]
assert pyrazobill.reconstruct(formKey, ints, strs) == lines[0]

# 2. Multi-threaded scaling with ThreadPoolExecutor (737k+ logs/sec across 4 threads)
chunkSize = 25_000
chunks = [lines[i:i + chunkSize] for i in range(0, len(lines), chunkSize)]

with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    batchedResults = list(pool.map(lambda c: pyrazobill.decomposeBatch(c, pyrazobill.ScanMode.Semantic), chunks))

allTuples = [t for batch in batchedResults for t in batch]
print(f"Decomposed {len(allTuples):,} logs in parallel across 4 worker threads!")
```

---

## High-Throughput Batch Processing

Process millions of logs using all hardware cores without GIL contention:

```python
import pyrazobill

lines = [
    "Worker 1 processed 100 items",
    "Worker 2 processed 250 items",
    "Worker 3 processed 80 items",
] * 100000

# Releases GIL and parallelizes across all available hardware threads
res = pyrazobill.auditParallel(
    lines,
    mode=pyrazobill.ScanMode.Semantic,
    numThreads=0 # 0 detects hardware concurrency automatically
)

print(f"Throughput   : {res.msgsPerSec:,.0f} msgs/sec ({res.mbPerSec:.2f} MB/sec)")
print(f"Elapsed Time : {res.elapsedSec:.3f} seconds")
print(f"Unique Forms : {res.uniqueFormsCount:,}")
print(f"Mismatches   : {res.mismatches} (Zero data loss guarantee)")
```

---

## DuckDB Extension Integration

`pyrazobill` bundles the native DuckDB compiled extension. Load and run directly in SQL:

```python
import duckdb
import pyrazobill

con = duckdb.connect(config={"allow_unsigned_extensions": "true"})

# Dynamically locate and load the extension
extPath = pyrazobill.extensionPath().replace("\\", "/")
con.execute(f"LOAD '{extPath}';")

# Run vectorized scalar parsing in DuckDB using single-pass rbDecomposeSemantic
con.execute("""
    CREATE TABLE parsedLogs AS
    SELECT
        d.form,
        d.ints,
        d.strs
    FROM (
        SELECT rbDecomposeSemantic(msg) AS d FROM rawTable
    );
""")

# Reconstruct on the fly in analytical queries
con.execute("""
    SELECT rbReconstruct(form, ints, strs) AS msg
    FROM parsedLogs
    LIMIT 10;
""")
```

---

## Scanning Modes

| ScanMode | Description | Ideal Workloads |
| :--- | :--- | :--- |
| `ScanMode.Semantic` | Identifies semantic tokens (IPs, UUIDs, hashes, paths, numbers). *(Recommended)* | Production syslog, cloud microservices, K8s logs. |
| `ScanMode.Exact` | Strict lexical separation of integer/hex words and exact strings. | High-precision audit logs, cryptographic event trails. |
| `ScanMode.DigitOnly` | Fast single-character ASCII digit detection. | Telemetry counters, metrics streams. |

---

## API Summary

```python
# Lexical Parsing (Rich SlotRecord)
record = pyrazobill.parse(msg: str, mode: ScanMode = ScanMode.Semantic) -> SlotRecord
records = pyrazobill.parseBatch(msgs: list[str], mode: ScanMode = ScanMode.Semantic) -> list[SlotRecord]

# Direct Tuple Decomposition (Zero-Wrapper Overhead, GIL Released)
formKey, ints, strs = pyrazobill.decompose(msg: str, mode: ScanMode = ScanMode.Semantic) -> tuple[str, list[int], list[str]]
tuples = pyrazobill.decomposeBatch(msgs: list[str], mode: ScanMode = ScanMode.Semantic) -> list[tuple[str, list[int], list[str]]]

# Exact Lossless Reconstruction
original = pyrazobill.reconstruct(record: SlotRecord) -> str
original = pyrazobill.reconstruct(formKey: str, ints: list[int], strs: list[str]) -> str

# Templates & Hashing
key = pyrazobill.formKey(msg: str, mode: ScanMode = ScanMode.Semantic) -> str
tmpl = pyrazobill.templateOf(form: str) -> str
h = pyrazobill.hash64(text: str) -> int

# Information Theory & Theoretical Ledger (Atta 2026, DOI: 10.5281/zenodo.23287286)
H = pyrazobill.entropy(counts: list[float]) -> float
ledger = pyrazobill.computeLedger(cells: list[Cell], totalEvents: float, confidenceAlpha: float = 0.05) -> Ledger
decomp = pyrazobill.consequenceDecomposition(xy: list[XY], map: list[FormMeaning]) -> ConsequenceDecomposition
H_mm = pyrazobill.millerMadow(pluginEntropy: float, observedCategories: int, sampleSize: float) -> float
ci = pyrazobill.certifiedInterval(alphabetSize: int, sampleSize: float, confidenceAlpha: float = 0.05) -> tuple[float, float]

# Latitude Channel & Steganography (Theorem 4c)
cap = pyrazobill.channelAnalyzeCapacity(sequence: list[MeaningChoice]) -> ChannelCapacity
res = pyrazobill.channelEmbed(sequence: list[MeaningChoice], message: int, payloadBits: int, sharedKey: float, dither: float = 0.5) -> EmbedResult
dec = pyrazobill.channelDecode(sequence: list[MeaningChoice], observedForms: list[int], payloadBits: int, sharedKey: float) -> DecodeResult

# Parallel Multi-Core Audit
res = pyrazobill.auditParallel(lines: list[str], mode: ScanMode = ScanMode.Semantic, numThreads: int = 0) -> AuditResult

# System & Metadata
path = pyrazobill.extensionPath() -> str
ver = pyrazobill.version() -> str
abi = pyrazobill.abiVersion() -> int
info = pyrazobill.versionInfo() -> tuple[int, int, int]
```

---

## Documentation & Deep Dive

For an exhaustive technical guide with production architecture recipes, see:
- [lib/python/Guide.md](Guide.md)
- [Project Documentation Master Index](../../docs/Index.md)

---

## Academic Citation & Foundations

The information-theoretic ledger and channel capacity mechanisms implemented in `pyrazobill` originate from:

> **Atta, Moustafa Mahmoud (2026).** *Latitude and Doubt: An exact ledger of meaning in messages with measurements on the genetic code and on English*.  
> Zenodo. DOI: [10.5281/zenodo.23287286](https://doi.org/10.5281/zenodo.23287286) | Record: [zenodo.org/records/23287286](https://zenodo.org/records/23287286)

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

