# DuckDB Native Extension Guide

The `librazobill` DuckDB extension implements high-throughput, pure scalar functions for log tokenization, columnar compaction, and on-the-fly reconstruction directly inside DuckDB SQL.

---

## 1. Extension Loading

The extension is compiled as a standalone C-API loadable binary (`librazobill.duckdb_extension`). Because it is locally compiled, enable unsigned extensions in DuckDB:

### From DuckDB CLI
```sql
SET allow_unsigned_extensions = true;
LOAD 'build/librazobill.duckdb_extension';
```

### From Python
```python
import duckdb

con = duckdb.connect(config={"allow_unsigned_extensions": "true"})
con.load_extension("build/librazobill.duckdb_extension")
```

---

## 2. Scalar SQL Functions Reference

| Function | Signature | Description |
|---|---|---|
| **`rbDecompose(msg)`** | `VARCHAR → STRUCT(form VARCHAR, ints BIGINT[], strs VARCHAR[])` | **Single-pass composite decomposition**. Decomposes message in one scan into a composite struct, cutting CPU overhead by 66%. |
| **`rbDecomposeSemantic(msg)`** | `VARCHAR → STRUCT(form VARCHAR, ints BIGINT[], strs VARCHAR[])` | Semantic single-pass decomposition abstracting high-entropy identifiers. |
| **`rbForm(msg)`** | `VARCHAR → VARCHAR` | Extracts the canonical form key (`ScanMode::Exact`). |
| **`rbFormSemantic(msg)`** | `VARCHAR → VARCHAR` | Extracts the semantic form key (`ScanMode::Semantic`). |
| **`rbInts(msg)`** | `VARCHAR → BIGINT[]` | Extracts contiguous integer slot values as a SQL list. |
| **`rbStrs(msg)`** | `VARCHAR → VARCHAR[]` | Extracts variable string slot values as a SQL list. |
| **`rbReconstruct(form, ints, strs)`** | `VARCHAR, BIGINT[], VARCHAR[] → VARCHAR` | Exact 100% byte-for-byte inverse reconstruction of the raw message. |
| **`rbTemplate(form)`** | `VARCHAR → VARCHAR` | Converts an internal form key to human-readable `<*>` template. |
| **`rbHash(str)`** | `VARCHAR → UBIGINT` | Computes 64-bit XXH64 hash matching ClickHouse `xxHash64`. |
| **`rbEntropy(counts)`** | `DOUBLE[] → DOUBLE` | Calculates exact Shannon bit-entropy using Neumaier compensated summation. |

*Note: For backward compatibility, snake_case aliases (`rb_decompose`, `rb_form`, `rb_ints`, `rb_strs`, `rb_reconstruct`, `rb_template`, `rb_hash`, `rb_entropy`) are also registered.*

---

## 3. Step-by-Step Table Compaction Walkthrough

Instead of storing massive repetitive text strings in a raw log table, `librazobill` allows replacing the table with a compact columnar representation:

### Step 1: Create Raw Log Table
```sql
CREATE TABLE rawLogs (
    timestamp TIMESTAMP,
    host VARCHAR,
    msg VARCHAR
);
```

### Step 2: Create Compact Storage Table (`logsCompact`)
Extract and persist the form key and typed slot arrays in a single vectorized pass using `rbDecompose`:

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│               SINGLE-PASS STRUCT (rbDecompose) VS MULTI-PASS PARSING                   │
├───────────────────────────────────────────┬────────────────────────────────────────────┤
│ Legacy Multi-Pass Approach (3 Scans)      │ Razobill Single-Pass Struct (1 Scan)       │
├───────────────────────────────────────────┼────────────────────────────────────────────┤
│ 1. Scan 1: rbForm(msg)   ──► form         │ 1. Single SIMD Scan:                       │
│ 2. Scan 2: rbInts(msg)   ──► ints         │    d = rbDecompose(msg)                    │
│ 3. Scan 3: rbStrs(msg)   ──► strs         │    ──► STRUCT(form, ints, strs)            │
│ Result: 3× redundant parsing overhead     │ Result: 66% fewer CPU cycles, 3× throughput│
└───────────────────────────────────────────┴────────────────────────────────────────────┘
```
```sql
CREATE TABLE logsCompact AS
SELECT
    timestamp,
    host,
    d.form,
    d.ints,
    d.strs
FROM (
    SELECT timestamp, host, rbDecompose(msg) AS d FROM rawLogs
);
```

### Step 3: Create Rebuilding View
Create a SQL view matching the original schema:
```sql
CREATE VIEW logs AS
SELECT
    timestamp,
    host,
    rbReconstruct(form, ints, strs) AS msg
FROM logsCompact;
```

### Step 4: Drop Raw Table
Drop `rawLogs` to reclaim disk space. Queries executing against view `logs` will reassemble the original message string on the fly with zero data loss.

---

## 4. Query Performance Best Practices

1. **Query Forms Directly**: Avoid full message scans (`WHERE msg LIKE '%failed%'`) when filtering templates. Instead, query the template column:
   ```sql
   -- FAST (Scans lightweight template column, avoids string reconstruction)
   SELECT * FROM logsCompact WHERE rbTemplate(form) LIKE '%Failed password%';
   ```
2. **Aggregate by Form**:
   ```sql
   SELECT rbTemplate(form), count(*)
   FROM logsCompact
   GROUP BY form
   ORDER BY count(*) DESC;
   ```
