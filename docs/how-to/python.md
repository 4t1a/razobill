# Parsing logs in Python

Process single messages, batch streams, and binary buffers using the pyrazobill library.
Read this guide if you are integrating Razobill into a Python logging or telemetry pipeline.
Skip this guide if you are querying logs inside a database.

---

## Problem

Parsing millions of log lines in pure Python using regular expressions or `split()` creates high CPU overhead and triggers garbage collection stalls.
Standard Python string allocations on large log corpora often consume tens of gigabytes of RAM.

---

## Solution

The `pyrazobill` package compiles high-performance C++20 parsing routines with pure-Python fallback support.
It exposes unified `split` and `join` primitives.

### Step 1: Install and import

Install the package from PyPI:

```bash
pip install pyrazobill
```

Import the module:

```python
import pyrazobill as rz
```

---

### Step 2: Parse a single log line

Call `rz.RzSplit` (or its ergonomic alias `rz.split`):

```python
line = "Accepted publickey for ubuntu from 10.0.0.15 port 54322 ssh2"
record = rz.RzSplit(line)

print("Template:", record.template)
print("Numbers: ", record.ints)
print("Words:   ", record.strs)
```

Output:

```text
Template: Accepted publickey for {s} from {s} port {i} {s}
Numbers:  [54322]
Words:    ['ubuntu', '10.0.0.15', 'ssh2']
```

The returned `record` is an instance of `RzRecord`, a named tuple with fields `template`, `ints`, and `strs`.

---

### Step 3: Reconstruct original text

Pass the record fields directly to `rz.RzJoin`:

```python
# Unpack the record tuple
restored = rz.RzJoin(*record)
assert restored == line
```

---

### Step 4: Batch parsing with `RzSplitMany`

When parsing arrays or files of log messages, call `rz.RzSplitMany`:

```python
lines = [
    "Connection reset by 192.168.1.1 port 80",
    "Connection reset by 192.168.1.2 port 443",
    "Connection reset by 192.168.1.3 port 8080"
]

records = rz.RzSplitMany(lines)
for rec in records:
    print(rec.template, rec.ints)
```

Output:

```text
Connection reset by {s} port {i} [80]
Connection reset by {s} port {i} [443]
Connection reset by {s} port {i} [8080]
```

`RzSplitMany` releases the Python GIL during parsing, enabling parallel C++ worker threads to execute simultaneously.

---

### Step 5: Handling raw bytes

`pyrazobill` accepts either `str` or `bytes` as input.
When `bytes` are provided, `RzSplit` returns byte-encoded templates and string views without UTF-8 decoding overhead:

```python
raw_bytes = b"\x00\x01 GET /health HTTP/1.1 200"
record = rz.RzSplit(raw_bytes)

assert isinstance(record.template, bytes)
assert rz.RzJoin(*record) == raw_bytes
```

---

### Step 6: Error handling

When malformed inputs or invalid arguments are supplied, `pyrazobill` raises an `RzError` (a subclass of `ValueError`):

```python
try:
    rz.RzSplit("valid message", mode="invalid_mode")
except rz.RzError as err:
    print("Caught expected error:", err)
```

Output:

```text
Caught expected error: [razobill] What happened: Invalid scan mode. Why: Expected 'semantic' or 'exact', got 'invalid_mode'. What to do: Use mode='semantic' (default) or mode='exact'.
```

---

## Verification

Run this test script to verify that both native and fallback implementations preserve exact invertibility:

```python
import pyrazobill as rz

sample = "Worker 12 finished task in 45 ms with status OK"
rec = rz.RzSplit(sample)
assert rz.RzJoin(*rec) == sample
print("Verification passed: pyrazobill", rz.__version__, "is functional.")
```
