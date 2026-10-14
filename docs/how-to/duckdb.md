# Compacting and querying logs in DuckDB

Parse unstructured log columns into typed structs and reconstruct original lines on demand.
Read this guide if you are analyzing log data inside DuckDB.
Skip this guide if you are writing application code in Python or C++.

---

## Problem

Storing telemetry logs as raw string columns in DuckDB leads to performance issues:
1. Regex queries (`regexp_extract`) scan entire strings repeatedly, consuming CPU cycles.
2. High-cardinality numbers and identifiers prevent columnar compression algorithms (Snappy, ZSTD) from achieving optimal compression ratios.
3. Discarding raw strings during parsing loses forensic detail needed for security audits.

---

## Solution

The Razobill DuckDB extension exposes four scalar functions operating directly on database string pointers without intermediate heap copies.

### Step 1: Load the extension

In production, load the signed extension from the official DuckDB Community Repository:

```sql
INSTALL razobill FROM community;
LOAD razobill;
```

For local development builds, specify the binary path:
```sql
LOAD 'build/razobill.duckdb_extension';
```

---

### Step 2: Decompose logs into structured columns

Use `RzSplit(line)` to parse raw log strings into a composite struct containing `template`, `ints`, and `strs`:

```sql
-- Create an example table with raw log records
CREATE TABLE raw_logs (msg VARCHAR);
INSERT INTO raw_logs VALUES
    ('Connection lost to 10.0.0.1 after 3 retries'),
    ('Connection lost to 10.0.0.2 after 5 retries'),
    ('User admin authenticated from 192.168.1.50'),
    ('Connection lost to 10.0.0.1 after 1 retries');

-- Decompose every row in a single pass
SELECT
    RzSplit(msg) AS parsed
FROM raw_logs;
```

Output:
```text
┌────────────────────────────────────────────────────────────────────────┐
│                                 parsed                                 │
│        struct(template varchar, ints bigint[], strs varchar[])         │
├────────────────────────────────────────────────────────────────────────┤
│ {'template': 'Connection lost to {s} after {i} retries', 'ints': [3]… │
│ {'template': 'Connection lost to {s} after {i} retries', 'ints': [5]… │
│ {'template': 'User {s} authenticated from {s}', 'ints': [], 'strs':…   │
│ {'template': 'Connection lost to {s} after {i} retries', 'ints': [1]… │
└────────────────────────────────────────────────────────────────────────┘
```

---

### Step 3: Aggregate by template pattern

Use `RzTemplate(line)` to extract the template directly when parameter arrays are not needed:

```sql
SELECT
    RzTemplate(msg) AS pattern,
    COUNT(*) AS event_count
FROM raw_logs
GROUP BY 1
ORDER BY event_count DESC;
```

Output:
```text
┌──────────────────────────────────────────────┬─────────────┐
│                   pattern                    │ event_count │
│                   varchar                    │    int64    │
├──────────────────────────────────────────────┼─────────────┤
│ Connection lost to {s} after {i} retries     │           3 │
│ User {s} authenticated from {s}              │           1 │
└──────────────────────────────────────────────┴─────────────┘
```

---

### Step 4: Aggregate numerical parameter arrays

Query extracted integers directly with array index operations:

```sql
SELECT
    parsed.template,
    COUNT(*) AS total_failures,
    AVG(parsed.ints[1]) AS avg_retries
FROM (SELECT RzSplit(msg) AS parsed FROM raw_logs)
WHERE parsed.template LIKE '%Connection lost%'
GROUP BY parsed.template;
```

Output:
```text
┌──────────────────────────────────────────┬────────────────┬─────────────┐
│                 template                 │ total_failures │ avg_retries │
│                 varchar                  │     int64      │   double    │
├──────────────────────────────────────────┼────────────────┼─────────────┤
│ Connection lost to {s} after {i} retries │              3 │         3.0 │
└──────────────────────────────────────────┴────────────────┴─────────────┘
```

---

### Step 5: Fast identity hashing with `RzHash`

Compute deterministic 64-bit template hashes for join indexing:

```sql
SELECT
    RzHash(msg) AS template_hash,
    RzTemplate(msg) AS template
FROM raw_logs
LIMIT 2;
```

---

## Verification

Confirm bit-exact reconstruction of the original text using `RzJoin`:

```sql
SELECT
    msg == RzJoin(parsed.template, parsed.ints, parsed.strs) AS is_exact
FROM (
    SELECT msg, RzSplit(msg) AS parsed FROM raw_logs
);
```

Output:
```text
┌──────────┐
│ is_exact │
│ boolean  │
├──────────┤
│ true     │
│ true     │
│ true     │
│ true     │
└──────────┘
```

Every row evaluates to `true`.
No whitespace, delimiter, or casing was altered during parsing.
