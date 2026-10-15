# Razobill DuckDB extension

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="../../assets/Logo.jpeg">
    <img src="../../assets/Logo-light.jpeg" alt="Razobill Logo" width="300">
  </picture>
</p>

<p align="center">
  <a href="https://github.com/duckdb/community-extensions/tree/main/extensions/razobill"><img src="https://img.shields.io/badge/DuckDB-Community%20Extension-blue.svg" alt="DuckDB Community Extension"></a>
  <a href="https://opensource.org/licenses/Apache-2.0"><img src="https://img.shields.io/badge/License-Apache_2.0-blue.svg" alt="License: Apache 2.0"></a>
  <a href="https://doi.org/10.5281/zenodo.23287286"><img src="https://img.shields.io/badge/DOI-10.5281%2Fzenodo.23287286-blue.svg" alt="DOI: 10.5281/zenodo.23287286"></a>
</p>

High-performance, lossless log parsing and telemetry compaction extension for DuckDB.
It decomposes raw log messages into structured columnar templates, 64-bit integer arrays, and variable string arrays.
Operating directly on database memory pointers, it guarantees zero data loss and bit-exact reconstruction.

---

## Lossless invertibility

Traditional log parsers discard formatting, alter timestamps, or leak unseen variable tokens into cluster definitions.
Razobill enforces an algebraic invariant across every parse:

$$\forall m \in \Sigma^*, \quad \text{RzJoin}(\text{RzSplit}(m)) \equiv m$$

Every byte of whitespace, delimiter punctuation, and parameter data is preserved with zero information destruction.

> [!NOTE]
> **Lossless guarantee**: `RzJoin(parsed.template, parsed.ints, parsed.strs) == msg` is guaranteed for all VARCHAR and BLOB inputs. No whitespace, delimiter, or casing is modified.

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

## Scientific provenance

Razobill is grounded in information-theoretic invariants established in:

> **Atta, Moustafa Mahmoud (2026).** *Latitude and Doubt: An exact ledger of meaning in messages with measurements on the genetic code and on English.*
> Zenodo. DOI: [10.5281/zenodo.23287286](https://doi.org/10.5281/zenodo.23287286)

---

## Documentation

- [DuckDB extension guide](../../docs/how-to/duckdb.md)
- [SQL functions reference](../../docs/reference/sql-functions.md)
- [Lossless invertibility](../../docs/explanation/lossless-invertibility.md)
