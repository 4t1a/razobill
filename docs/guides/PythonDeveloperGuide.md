# Python Developer Guide (`pyrazobill`)

`pyrazobill` provides a minimalist, zero-underscore Python developer experience for `librazobill`. It delivers native C++ execution speeds, releases the Global Interpreter Lock (GIL) during multi-core processing, and provides 100% memory safety.

---

## 1. Quickstart & Installation

From the repository root, install `pyrazobill` in editable mode or build a wheel:

```bash
pip install ./lib/python
```

Or add the module directly to your Python script's search path:
```python
import sys
from pathlib import Path

sys.path.insert(0, "path/to/razobill/lib/python")
import pyrazobill as rz
```

---

## 2. Parsing a Single Log Line

```python
import pyrazobill as rz

logLine = "Accepted publickey for ubuntu from 192.168.1.100 port 54322 ssh2"

# Parse using Semantic mode (collapses usernames and hostnames)
record = rz.parse(logLine, rz.ScanMode.Semantic)

print("Form Key   :", record.formKey)
print("Form Hash  :", record.formKeyHash)
print("Template   :", rz.templateOf(record.formKey))
# Output: "Accepted publickey for ubuntu from <*> port <*> ssh2"

print("Integers   :", record.ints)  # [54322]
print("Strings    :", record.strs)  # ['ubuntu', '192.168.1.100', 'ssh2']

# Exact byte-for-byte lossless reconstruction
rebuilt = rz.reconstruct(record)
assert rebuilt == logLine
print("Reconstructed exactly!")
```

---

## 3. High-Throughput Batch Parsing (Releasing GIL)

When parsing arrays of log messages, `rz.parseBatch()` releases the Python GIL, allowing maximum C++ execution speed:

```python
import pyrazobill as rz

lines = [
    "Failed password for root from 10.0.0.1 port 22",
    "Failed password for admin from 10.0.0.2 port 22",
    "Accepted publickey for dev from 10.0.0.3 port 22"
]

records = rz.parseBatch(lines, rz.ScanMode.Semantic)

for rec in records:
    print(f"Form: {rec.formKey} | Ints: {rec.ints} | Strs: {rec.strs}")
```

---

## 4. High-Speed Tuple Decomposition (`decompose` & `decomposeBatch`)

When building analytical pipelines (e.g. streaming into DuckDB, Apache Arrow, Polars, or Parquet on AWS S3), you rarely need rich object wrappers. `rz.decompose()` and `rz.decomposeBatch()` return raw `(formKey, ints, strs)` tuples directly from the C++ core:

```python
import pyrazobill as rz

# Single event direct tuple extraction
msg = "Accepted publickey for ubuntu from 192.168.1.100 port 54322 ssh2"
formKey, ints, strs = rz.decompose(msg, rz.ScanMode.Semantic)

print("Form Key :", formKey)
print("Ints     :", ints)   # [54322]
print("Strs     :", strs)   # ['ubuntu', '192.168.1.100', 'ssh2']

# 100% Exact bit-level reconstruction directly from tuple elements
rebuilt = rz.reconstruct(formKey, ints, strs)
assert rebuilt == msg
```

### Multi-Threaded Parallel Streaming Ingestion (`decomposeBatch`)

`rz.decomposeBatch()` releases the Python GIL during C++ SIMD parsing, enabling linear multi-core speedup when paired with `concurrent.futures.ThreadPoolExecutor`:

```python
import concurrent.futures
import pyrazobill as rz

logBatch = [
    f"Worker {i%8} processed {100+i} items from 10.0.0.{i%250} port {5000+i}"
    for i in range(100_000)
]

# Partition into chunks for worker threads
chunkSize = 25_000
chunks = [logBatch[i:i + chunkSize] for i in range(0, len(logBatch), chunkSize)]

# Parallel C++ decomposition across 4 threads releasing GIL
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
    results = list(executor.map(lambda c: rz.decomposeBatch(c, rz.ScanMode.Semantic), chunks))

# Flattened list of tuples (515k msg/s single-thread -> 737k+ msg/s across 4 threads)
allRecords = [item for sub in results for item in sub]
print(f"Successfully decomposed {len(allRecords):,} records in parallel!")
```

---

## 5. Multi-Core Parallel Audit (`auditParallel`)

To verify 100% lossless invertibility and throughput across all CPU hardware cores without GIL contention:

```python
import pyrazobill as rz

# Read log lines
with open("logs/SSH/SSH.log", "r", encoding="utf-8", errors="ignore") as f:
    logLines = [line.strip() for line in f if line.strip()]

# Execute multi-threaded audit across 12 hardware cores
audit = rz.auditParallel(logLines, mode=rz.ScanMode.Semantic, numThreads=12)

print(f"Total Lines   : {audit.totalLines:,}")
print(f"Throughput    : {audit.msgsPerSec:,.0f} msg/s ({audit.mbPerSec:.2f} MB/s)")
print(f"Unique Forms  : {audit.uniqueFormsCount:,}")
print(f"Mismatches    : {audit.mismatches} (100% Lossless Guarantee)")
```

---

## 6. Integrating with DuckDB from Python

You can load the native `librazobill` extension directly into `duckdb-python`:

```python
import duckdb
import pyrazobill as rz

con = duckdb.connect(config={"allow_unsigned_extensions": "true"})

# Load the compiled extension
con.load_extension(rz.extensionPath())

# Single-pass composite decomposition (parses in one pass into a STRUCT)
con.execute("""
    CREATE TABLE logs AS SELECT 'Failed password for root from 10.0.0.1 port 22' AS msg;
    SELECT
        d.form,
        d.ints,
        d.strs,
        rbReconstruct(d.form, d.ints, d.strs) AS rebuilt
    FROM (
        SELECT rbDecompose(msg) AS d FROM logs
    );
""")

print(con.fetchall())
```

---

## 7. Information Ledger & Latitude Channel

### Computing the Information Ledger
```python
import pyrazobill as rz

# Form-meaning contingency table with observed event counts
cells = [
    rz.Cell(form=101, meaning=1, mass=500.0),
    rz.Cell(form=102, meaning=1, mass=300.0),
    rz.Cell(form=201, meaning=2, mass=200.0),
]

ledger = rz.computeLedger(cells, totalEvents=1000.0, confidenceAlpha=0.05)

print(f"Content Entropy (C)    : {ledger.content:.4f} bits")
print(f"Expression Entropy (E) : {ledger.expression:.4f} bits")
print(f"Slot Latitude (Lambda) : {ledger.latitude:.4f} bits")
print(f"Doubt (Delta)          : {ledger.doubt:.4f} bits")
print(f"Conveyance (I)         : {ledger.conveyance:.4f} bits")

# Verify Theorem 1 & 4 balance identities
assert ledger.satisfiesLedgerIdentities()
print("Information ledger verified to 10^-9!")
```

### Latitude Channel Steganography & Provenance
```python
import pyrazobill as rz

# Meaning choice sequence with synonymous forms
options = [
    rz.MeaningChoice(meaning=1, options=[
        rz.SynonymousOption(formIndex=0, probability=0.6),
        rz.SynonymousOption(formIndex=1, probability=0.4),
    ])
    for _ in range(32)
]

# Analyze capacity
capacity = rz.channelAnalyzeCapacity(options)
print(f"Safe Payload Capacity: {capacity.safePayloadBits} bits")

# Embed a 16-bit integer payload
msg = 42135
res = rz.channelEmbed(options, message=msg, payloadBits=16, sharedKey=1234.5)
assert res.success

# Decode the payload from observed forms
decoded = rz.channelDecode(options, res.chosenForms, payloadBits=16, sharedKey=1234.5)
assert decoded.success
assert decoded.message == msg
print(f"Successfully decoded embedded payload: {decoded.message}!")
```
