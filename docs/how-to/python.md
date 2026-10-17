# Parsing logs in Python

[Docs](../README.md) / [How-to guides](python.md) / Python

This guide shows you how to parse log strings, process batches with multithreading, handle raw bytes, and reconstruct original messages using the `pyrazobill` package.

- **Who this is for**: Python developers building telemetry ingestion scripts, CLI utilities, or ETL data pipelines.
- **What you will have at the end**: A Python script that decomposes log strings and batches into relational records and verifies exact lossless reconstruction.

---

## The problem

Processing millions of log lines in Python with regular expressions or custom `split()` logic causes high CPU utilization and large memory allocations:

1. Standard regex engines backtrack on unexpected log tokens, slowing down batch throughput.
2. Creating intermediate strings for every token triggers garbage collection overhead.
3. Discarding parameters makes it impossible to reproduce original logs for compliance.

---

## Prerequisites

- Python 3.9 or newer.
- Package `pyrazobill` installed via `pip install pyrazobill`.

---

## Step 1: Parse a single log line

Call `rz.split(line)`:

```python
import pyrazobill as rz

line = "Failed password for root from 192.168.1.100 port 22"
record = rz.split(line)

print("Template:", record.template)
print("Integers:", record.ints)
print("Strings: ", record.strs)
```

Expected output:

```text
Template: Failed password for {s} from {s} port {i}
Integers: [22]
Strings:  ['root', '192.168.1.100']
```

The returned object is an `RzRecord` named tuple with fields `template`, `ints`, and `strs`.

---

## Step 2: Reconstruct original text

Pass the decomposed record back to `rz.join`:

```python
restored = rz.join(record.template, record.ints, record.strs)
assert restored == line
```

Every character and delimiter matches the original input line.

---

## Step 3: Batch parsing with split_many

When parsing lists or streams of log messages, call `rz.split_many`:

```python
lines = [
    "Failed password for root from 192.168.1.100 port 22",
    "Failed password for admin from 10.0.0.1 port 2222",
]

records = rz.split_many(lines)
for rec in records:
    print(rec.template, rec.ints, rec.strs)
```

Expected output:

```text
Failed password for {s} from {s} port {i} [22] ['root', '192.168.1.100']
Failed password for {s} from {s} port {i} [2222] ['admin', '10.0.0.1']
```

When the input collection exceeds 128 items, the compiled C++ engine releases the Python Global Interpreter Lock (GIL) and dispatches parsing across worker threads.

---

## Step 4: Handle raw binary bytes

`pyrazobill` accepts either `str` or `bytes`.
When `bytes` are provided, `split()` operates without UTF-8 decoding overhead and returns bytes:

```python
raw_bytes = b"Failed password for root from 192.168.1.100 port 22"
record = rz.split(raw_bytes)

assert isinstance(record.template, bytes)
assert rz.join(record.template, record.ints, record.strs) == raw_bytes
```

---

## Step 5: Fast template hashing

Compute a 64-bit unsigned hash of the template string for indexing or database partitioning:

```python
line = "Failed password for root from 192.168.1.100 port 22"
rec = rz.split(line)
template_hash = rz.hash(rec.template)

print("Template hash:", template_hash)
```

Expected output:

```text
Template hash: 54855824174769941
```

The resulting integer matches ClickHouse `xxHash64` and DuckDB `RzHash` bit for bit.

---

## How to verify

Verify your installation by running a round-trip check on your running example:

```python
import pyrazobill as rz

sample = "Failed password for root from 192.168.1.100 port 22"
rec = rz.split(sample)
assert rz.join(rec.template, rec.ints, rec.strs) == sample
print("Verification passed on pyrazobill version", rz.__version__)
print("Native SIMD engine active:", rz.isNative())
```

---

## Troubleshooting

### RzError: Invalid scan mode

**What happened**:
Calling `rz.split(line, mode="custom")` raises an `RzError`:

```text
RzError: [razobill] What happened: Invalid scan mode. Why: Expected 'semantic' or 'exact', got 'custom'.
```

**How to resolve**:
Pass either `mode="semantic"` (default) or `mode="exact"`:

```python
rec = rz.split(line, mode="semantic")
```

### RzError: Unsupported input type

**What happened**:
Passing an integer or dictionary to `split()` raises an `RzError`:

```text
RzError: [razobill] What happened: Unsupported input type. Why: Expected str or bytes, got int.
```

**How to resolve**:
Cast or format the argument as `str` or `bytes` before passing to `split()`.

---

## Related

- [Python API reference](../reference/python-api.md): parameter types, error classes, and return signatures.
- [DuckDB extension guide](duckdb.md): parsing logs with analytical SQL.
- [Lossless invertibility](../explanation/lossless-invertibility.md): two-pass reconstruction mechanics.
