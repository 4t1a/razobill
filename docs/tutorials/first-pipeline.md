# Building an ingestion pipeline

Build an end-to-end ingestion pipeline that decomposes raw logs into structured Parquet files with DuckDB.
Read this guide if you are designing a high-throughput telemetry ingestion or compaction pipeline.
Skip this guide if you only need single-line API signatures.

---

## What you will build

You will build a Python and DuckDB pipeline that:
1. Decomposes raw log streams in parallel using `RzSplitMany`.
2. Stores structured templates, numbers, and words into a columnar table.
3. Computes analytical metrics over variable integer arrays.
4. Restores raw byte streams on demand for forensic audits.

---

## Step 1: Decompose a batch of log records

Create a script named `pipeline.py`:

```python
import duckdb
import pyrazobill as rz

# 1. Sample production log stream
raw_logs = [
    "2026-10-14 06:01:00 [INFO] Node 1 connected with 16 workers",
    "2026-10-14 06:01:05 [WARN] Node 2 heartbeat delayed by 450 ms",
    "2026-10-14 06:01:10 [INFO] Node 3 connected with 32 workers",
    "2026-10-14 06:01:15 [WARN] Node 2 heartbeat delayed by 120 ms",
]

# 2. Split lines in parallel across worker threads
records = rz.RzSplitMany(raw_logs)

print(f"Decomposed {len(records)} log events.")
print("Sample template:", records[1].template)
print("Sample numbers: ", records[1].ints)
```

Run the script:
```bash
python pipeline.py
```

Expected output:
```text
Decomposed 4 log events.
Sample template: {s} {s}:{s}:{s} [WARN] Node {i} heartbeat delayed by {i} ms
Sample numbers:  [2, 450]
```

---

## Step 2: Write structured events into DuckDB

Append DuckDB storage logic to `pipeline.py`:

```python
# Connect to in-memory or persistent database
con = duckdb.connect("telemetry.db")

# Create structured table schema
con.execute("""
    CREATE TABLE IF NOT EXISTS telemetry_events (
        event_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        template VARCHAR,
        ints BIGINT[],
        strs VARCHAR[]
    );
""")

# Insert records directly
for r in records:
    con.execute(
        "INSERT INTO telemetry_events (template, ints, strs) VALUES (?, ?, ?);",
        [r.template, r.ints, r.strs]
    )

print("Inserted records into telemetry_events table.")
```

---

## Step 3: Query template frequencies and metrics

Run analytical SQL queries directly on extracted parameters without regex parsing:

```python
# Calculate total warnings and average delay per node
query = """
    SELECT
        template,
        COUNT(*) AS event_count,
        AVG(ints[2]) AS average_delay_ms
    FROM telemetry_events
    WHERE template LIKE '%heartbeat delayed%'
    GROUP BY template;
"""

results = con.execute(query).fetchall()
for row in results:
    print(f"Template: {row[0]}")
    print(f"Count:    {row[1]}")
    print(f"Avg delay:{row[2]:.1f} ms")
```

Expected output:
```text
Template: {s} {s}:{s}:{s} [WARN] Node {i} heartbeat delayed by {i} ms
Count:    2
Avg delay: 285.0 ms
```

Because `ints` is stored as a native typed array, numerical aggregations run at native database speeds.

---

## Step 4: Verify lossless reconstruction

Prove that the original log events can be restored byte-for-byte:

```python
# Fetch row from database and reconstruct original message
rows = con.execute("SELECT template, ints, strs FROM telemetry_events;").fetchall()
for original_line, (template, ints, strs) in zip(raw_logs, rows):
    restored = rz.RzJoin(template, ints, strs)
    assert restored == original_line

print("Audit verification successful: 100% byte fidelity.")
```

Expected output:
```text
Audit verification successful: 100% byte fidelity.
```

---

## Next steps

- Learn how to deploy the edge collector in [OpenTelemetry integration](../how-to/opentelemetry.md).
- Stream decomposed logs to S3 in [AWS S3 storage compaction](../how-to/s3-compaction.md).
- Review function signatures in [DuckDB SQL reference](../reference/sql-functions.md).
