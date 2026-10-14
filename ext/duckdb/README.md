# Razobill DuckDB extension

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="../../assets/wordmark-dark.svg">
    <img src="../../assets/wordmark-light.svg" alt="Razobill Logo" width="300">
  </picture>
</p>

High-performance, lossless log parsing and telemetry compaction extension for DuckDB.
It decomposes raw log messages into structured columnar templates, 64-bit integer arrays, and variable string arrays.
Operating directly on database memory pointers, it guarantees zero data loss and bit-exact reconstruction.

---

## Quick installation

### From DuckDB community repository

```sql
INSTALL razobill FROM community;
LOAD razobill;
```

### From local binary build

```sql
LOAD 'build/razobill.duckdb_extension';
```

---

## Registered SQL functions

The extension registers four scalar functions with zero aliases:

| Function | Signature | Description |
| :--- | :--- | :--- |
| `RzSplit` | `VARCHAR/BLOB -> STRUCT(template, ints, strs)` | Decomposes a message into a template, numbers, and words. |
| `RzJoin` | `(template, ints, strs) -> VARCHAR/BLOB` | Reconstructs the exact raw log line byte-for-byte. |
| `RzTemplate` | `VARCHAR/BLOB -> VARCHAR/BLOB` | Extracts only the structural template without allocating arrays. |
| `RzHash` | `VARCHAR/BLOB -> UBIGINT` | Computes a fast 64-bit template hash for indexing. |

---

## Example usage

```sql
-- 1. Parse log messages into typed columns
SELECT RzSplit('Failed password for root from 192.168.1.100 port 22') AS parsed;
-- Output:
-- {'template': 'Failed password for {s} from {s} port {i}', 'ints': [22], 'strs': ['root', '192.168.1.100']}

-- 2. Aggregate log event frequencies by template
SELECT
    RzTemplate(message) AS log_pattern,
    COUNT(*) AS frequency
FROM system_logs
GROUP BY 1
ORDER BY frequency DESC;

-- 3. Reconstruct raw messages on demand
SELECT RzJoin(parsed.template, parsed.ints, parsed.strs) AS reconstructed
FROM (SELECT RzSplit(message) AS parsed FROM system_logs);
```

---

## Documentation

- [DuckDB extension guide](../../docs/how-to/duckdb.md)
- [SQL functions reference](../../docs/reference/sql-functions.md)
- [Lossless invertibility](../../docs/explanation/lossless-invertibility.md)
