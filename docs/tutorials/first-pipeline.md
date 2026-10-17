# Build an ingestion pipeline

[Docs](../README.md) > [Tutorials](../README.md#tutorials) > Build an ingestion pipeline

Build an end-to-end ingestion pipeline in Python that decomposes log lines in parallel, stores them in DuckDB, queries extracted parameters directly in SQL, and verifies byte-exact reconstruction.

- **Audience**: Data engineers and telemetry developers designing ingestion and compaction workflows.
- **Completion time**: 5 minutes.
- **What you will have at the end**: A running Python script that ingests log batches into a DuckDB table, runs analytical aggregations over native integer arrays, and proves lossless invertibility.

---

## Prerequisites

Ensure you have Python 3.9+ and the required packages installed:

```bash
pip install pyrazobill duckdb
```

Verify your installation:

```bash
python -c "import pyrazobill, duckdb; print('Ready')"
```

Expected output:

```text
Ready
```

---

## Step 1: Decompose a batch of log records

Create a script named `pipeline.py`. Use `rz.split_many` to process a batch of log events across background worker threads:

```python
import duckdb
import pyrazobill as rz

# 1. Synthetic log stream (RFC 1918 private IP addresses)
raw_logs = [
    "Failed password for root from 192.168.1.100 port 22",
    "Failed password for admin from 192.168.1.101 port 22",
    "Failed password for root from 192.168.1.102 port 2222",
    "Failed password for guest from 192.168.1.103 port 8080",
]

# 2. Decompose batch across worker threads
records = rz.split_many(raw_logs)

print(f"Decomposed {len(records)} log events.")
print("Sample template:", records[0].template)
print("Sample numbers: ", records[0].ints)
print("Sample strings: ", records[0].strs)
```

Run the script:

```bash
python pipeline.py
```

Expected output:

```text
Decomposed 4 log events.
Sample template: Failed password for {s} from {s} port {i}
Sample numbers:  [22]
Sample strings:  ['root', '192.168.1.100']
```

---

## Step 2: Store structured events in DuckDB

Append DuckDB storage logic to `pipeline.py`. Store templates, integers, and strings into typed relational columns:

```python
# Connect to an in-memory DuckDB session
con = duckdb.connect(":memory:")

# Create structured table schema
con.execute("""
    CREATE TABLE auth_failures (
        template VARCHAR,
        ints BIGINT[],
        strs VARCHAR[]
    );
""")

# Insert records directly
for r in records:
    con.execute(
        "INSERT INTO auth_failures VALUES (?, ?, ?);",
        [r.template, r.ints, r.strs]
    )

row_count = con.execute("SELECT COUNT(*) FROM auth_failures;").fetchone()[0]
print(f"Stored {row_count} rows in auth_failures table.")
```

Run the script:

```bash
python pipeline.py
```

Expected output:

```text
Decomposed 4 log events.
Sample template: Failed password for {s} from {s} port {i}
Sample numbers:  [22]
Sample strings:  ['root', '192.168.1.100']
Stored 4 rows in auth_failures table.
```

---

## Step 3: Run analytical SQL queries on extracted parameters

Query parameter columns directly in SQL without regular expressions. In DuckDB, arrays use 1-based indexing (`ints[1]` accesses the first extracted integer):

```python
# Compute attempts count and average target port grouped by template
query = """
    SELECT
        template,
        COUNT(*) AS attempts,
        AVG(ints[1]) AS avg_port
    FROM auth_failures
    GROUP BY template;
"""

results = con.execute(query).fetchall()
for row in results:
    print(f"Template: {row[0]}")
    print(f"Attempts: {row[1]}")
    print(f"Avg port: {row[2]:.1f}")
```

Run the script:

```bash
python pipeline.py
```

Expected output:

```text
Template: Failed password for {s} from {s} port {i}
Attempts: 4
Avg port: 2586.5
```

Because `ints` is stored as `BIGINT[]`, numerical filtering and averaging run on columnar data without regex evaluation.

---

## Step 4: Verify byte-exact reconstruction

Retrieve the decomposed rows from DuckDB and reconstruct the original text byte-for-byte:

```python
# Verify byte-exact round-trip invariant
rows = con.execute("SELECT template, ints, strs FROM auth_failures;").fetchall()
for original, (template, ints, strs) in zip(raw_logs, rows):
    restored = rz.join(template, ints, strs)
    assert restored == original

print("Audit verification: 100% byte fidelity verified.")
```

Run the script:

```bash
python pipeline.py
```

Expected output:

```text
Audit verification: 100% byte fidelity verified.
```

---

## What you just did

1. Decomposed unstructured log lines into typed relational components: `template`, `ints`, and `strs`.
2. Loaded the decomposed columns into DuckDB using native array types (`BIGINT[]`, `VARCHAR[]`).
3. Aggregated numerical metrics directly in SQL without regex parsing.
4. Proved that `rz.join(template, ints, strs)` reconstructs the original log lines without data loss.

---

## Next steps

- [DuckDB extension guide](../how-to/duckdb.md): parse and compact logs directly inside SQL.
- [Batch processing in Python](../how-to/python.md): stream gigabyte log files with multithreading and GIL release.
- [Lossless invertibility mechanics](../explanation/lossless-invertibility.md): read the formal algebraic proof.
