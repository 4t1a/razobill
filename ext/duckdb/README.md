<p align="center">
  <img src="../../assets/Logo.jpeg" alt="Razobill Logo" width="380" />
</p>

# `razobill` DuckDB Extension

High-performance, lossless log parsing and information-theoretic database compaction extension for **DuckDB**.

---

## 1. Quick Installation & Loading

### From Official DuckDB Community Repository
```sql
INSTALL razobill FROM community;
LOAD razobill;
```

### From Local Binary Build
```sql
-- For local development builds
LOAD 'build/razobill.duckdb_extension';
```

---

## 2. Key SQL Functions

| SQL Function | Signature | Description |
|---|---|---|
| `rbDecompose(msg)` | `VARCHAR -> STRUCT(form VARCHAR, ints BIGINT[], strs VARCHAR[])` | **Single-pass composite decomposition**. Decomposes message into struct, cutting CPU overhead by 66%. |
| `rbDecomposeSemantic(msg)` | `VARCHAR -> STRUCT(form VARCHAR, ints BIGINT[], strs VARCHAR[])` | Semantic single-pass decomposition abstracting high-entropy identifiers. |
| `rbForm(msg)` | `VARCHAR -> VARCHAR` | Parses log line into exact structural skeleton with typed slot tags. |
| `rbFormSemantic(msg)` | `VARCHAR -> VARCHAR` | Parses log line into semantic form key (ignoring volatile numbers/IPs). |
| `rbInts(msg)` | `VARCHAR -> BIGINT[]` | Extracts all extracted numerical variables as a strongly-typed integer array. |
| `rbStrs(msg)` | `VARCHAR -> VARCHAR[]` | Extracts all dynamic string tokens (IPs, UUIDs, hostnames) as a string array. |
| `rbReconstruct(form, ints, strs)` | `(VARCHAR, BIGINT[], VARCHAR[]) -> VARCHAR` | Losslessly reconstructs the exact raw log line byte-for-byte ($0$ mismatch). |
| `rbRebuild(form, val1, val2, ...)` | `(VARCHAR, ANY...) -> VARCHAR` | Replaces slot placeholders sequentially with passed arguments. |
| `rbTemplate(msg)` | `VARCHAR -> VARCHAR` | Produces Drain-compatible `<*>` wildcard template. |
| `rbHash(msg)` | `VARCHAR -> UBIGINT` | Computes fast 64-bit XXH64 hash matching ClickHouse `xxHash64`. |
| `rbEntropy(counts)` | `DOUBLE[] -> DOUBLE` | Calculates exact Shannon bit-entropy using Neumaier compensated summation. |

---

## 3. End-to-End Examples

### A. Real-Time Log Form Mining & Frequency Aggregation
Extract common log patterns across millions of log rows in milliseconds:

```sql
SELECT
    rbForm(message) AS log_pattern,
    COUNT(*) AS frequency,
    MIN(timestamp) AS first_seen,
    MAX(timestamp) AS last_seen
FROM system_logs
GROUP BY 1
ORDER BY frequency DESC
LIMIT 10;
```

---

### B. High-Density Storage Compaction (45%+ Disk Savings)
Decompose unstructured raw log strings into columnar integers, string pools, and dictionary-encoded structural forms in a single pass:

```sql
-- 1. Create a compacted analytical table using single-pass rbDecompose
CREATE TABLE compacted_logs AS
SELECT
    timestamp,
    level,
    d.form AS form_key,
    d.ints AS int_slots,
    d.strs AS str_slots
FROM (
    SELECT timestamp, level, rbDecompose(message) AS d FROM raw_logs
);

-- 2. Query against structured integer slots with vector speed
SELECT AVG(int_slots[1]) AS avg_latency_ms
FROM compacted_logs
WHERE form_key LIKE '%Query executed in % ms%';
```

---

### C. Exact Byte-For-Byte Lossless Reconstruction
Recreate raw text output on the fly with zero data loss ($E = I + \Lambda$):

```sql
SELECT
    rbReconstruct(form_key, int_slots, str_slots) AS raw_original_message
FROM compacted_logs
LIMIT 5;
```

---

## 4. Performance & Scalability

On modern AVX2 x86_64 hardware:
- **Vectorized Parsing**: > **2.3 Million logs/sec** within in-memory DuckDB queries.
- **Parallel Reconstruction**: > **9.0 Million logs/sec** during SQL scans.
- **Lossless Invertibility**: Exact bit-level preservation across adversarial whitespace, UTF-8 strings, and edge-case tokens.

---

## 5. License

The `razobill` DuckDB extension is licensed under the **Apache License, Version 2.0**.
See [LICENSE](../../LICENSE) and [NOTICE](../../NOTICE) for details.
