# DuckDB SQL functions reference

[Docs](../README.md) / [Reference](sql-functions.md) / SQL functions

This document specifies the scalar functions exposed by the Razobill DuckDB extension.

- **Who this is for**: Engineers querying or maintaining Razobill SQL operations in DuckDB.
- **What you will have at the end**: The exact type signatures, return contracts, edge case behaviors, and error codes for all four functions.

---

## Function summary

| Function | Signature | Return type | Stability |
| :--- | :--- | :--- | :---: |
| `RzSplit` | `RzSplit(line)` | `STRUCT(template, ints, strs)` | Stable |
| `RzJoin` | `RzJoin(template, ints, strs)` | `VARCHAR` / `BLOB` | Stable |
| `RzTemplate` | `RzTemplate(line)` | `VARCHAR` / `BLOB` | Stable |
| `RzHash` | `RzHash(line)` | `UBIGINT` | Stable |

---

## RzSplit

Decomposes a log message into a template and parameter arrays.

### Signature

```sql
RzSplit(line: VARCHAR) -> STRUCT(template VARCHAR, ints BIGINT[], strs VARCHAR[])
RzSplit(line: BLOB) -> STRUCT(template BLOB, ints BIGINT[], strs BLOB[])
```

### Parameters

- `line`: The log message to decompose. Supports `VARCHAR` and `BLOB`.

### Returns

A DuckDB struct with three fields:

- `template`: Format string containing `{i}` for 64-bit non-negative integers and `{s}` for variable strings. Literal braces are escaped as `{{` and `}}`.
- `ints`: `BIGINT[]` array of extracted 64-bit integers.
- `strs`: `VARCHAR[]` or `BLOB[]` array of extracted variable strings.

### Edge behavior

- **NULL input**: Returns `NULL`.
- **Empty input (`''`)**: Returns `{'template': '', 'ints': [], 'strs': []}`.
- **Invalid UTF-8**: Processed without failure when passed as `BLOB`.
- **Oversized input (> 16 MiB)**: Throws `Invalid Input Error: [razobill] ... Message too large`.

### Example

```sql
SELECT RzSplit('Failed password for root from 192.168.1.100 port 22') AS parsed;
```

Output:

```text
┌────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                                 parsed                                                 │
│                       struct("template" varchar, ints bigint[], strs varchar[])                        │
├────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│ {'template': 'Failed password for {s} from {s} port {i}', 'ints': [22], 'strs': [root, 192.168.1.100]} │
└────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

- **Stability**: Stable.
- **Since version**: 1.0.0.

---

## RzJoin

Reconstructs the original log message byte for byte from a template and parameter arrays.

### Signature

```sql
RzJoin(template: VARCHAR, ints: BIGINT[], strs: VARCHAR[]) -> VARCHAR
RzJoin(template: BLOB, ints: BIGINT[], strs: BLOB[]) -> BLOB
RzJoin(template: VARCHAR, arg1: ANY, arg2: ANY, ...) -> VARCHAR
```

### Parameters

- `template`: Structural format string containing `{i}` and `{s}` placeholders.
- `ints`: `BIGINT[]` array matching the number of `{i}` placeholders.
- `strs`: `VARCHAR[]` or `BLOB[]` array matching the number of `{s}` placeholders.
- *Varargs variant*: Accepts sequential arguments matching placeholder order in the template.

### Returns

The reconstructed string or blob, matching the original message bit for bit.

### Edge behavior

- **NULL template**: Returns `NULL`.
- **Empty template (`''`)**: Returns `''` if parameter arrays are empty; otherwise throws count mismatch.
- **Array type requirements**: Manually constructed integer arrays must be explicitly cast to `BIGINT[]` (e.g. `[22]::BIGINT[]`). Uncast integer lists default to 32-bit `INTEGER[]` in DuckDB.
- **Oversized output (> 16 MiB)**: Throws `Invalid Input Error: ... Message too large`.

### Errors

- **Count mismatch**: Throws `Invalid Input Error: [razobill] What happened: RzJoin failed. Why: Count mismatch (template placeholders do not match the number of provided integers or strings).`
- **Malformed template**: Throws `Invalid Input Error` if unescaped single braces appear without a valid `i` or `s` marker.

### Example

```sql
SELECT RzJoin(
    'Failed password for {s} from {s} port {i}',
    [22]::BIGINT[],
    ['root', '192.168.1.100']
) AS line;
```

Output:

```text
┌─────────────────────────────────────────────────────┐
│                        line                         │
│                       varchar                       │
├─────────────────────────────────────────────────────┤
│ Failed password for root from 192.168.1.100 port 22 │
└─────────────────────────────────────────────────────┘
```

- **Stability**: Stable.
- **Since version**: 1.0.0.

---

## RzTemplate

Extracts only the structural template representation of a log message without allocating parameter arrays.

### Signature

```sql
RzTemplate(line: VARCHAR) -> VARCHAR
RzTemplate(line: BLOB) -> BLOB
```

### Parameters

- `line`: The log message to parse.

### Returns

A format string with `{i}` and `{s}` placeholders.

### Edge behavior

- **NULL input**: Returns `NULL`.
- **Empty input (`''`)**: Returns `''`.
- **Oversized input (> 16 MiB)**: Throws `Invalid Input Error`.

### Example

```sql
SELECT RzTemplate('Failed password for root from 192.168.1.100 port 22') AS tmpl;
```

Output:

```text
┌───────────────────────────────────────────┐
│                   tmpl                    │
│                  varchar                  │
├───────────────────────────────────────────┤
│ Failed password for {s} from {s} port {i} │
└───────────────────────────────────────────┘
```

- **Stability**: Stable.
- **Since version**: 1.0.0.

---

## RzHash

Computes a 64-bit unsigned hash of the template string for indexing and partitioning.

### Signature

```sql
RzHash(line: VARCHAR) -> UBIGINT
RzHash(line: BLOB) -> UBIGINT
```

### Parameters

- `line`: The log message or template string.

### Returns

A 64-bit unsigned integer (`UBIGINT`) bit-identical to ClickHouse `xxHash64`.

### Edge behavior

- **NULL input**: Returns `NULL`.
- **Empty input (`''`)**: Returns `17241709254077376921` (`xxh64("")`).

### Example

```sql
SELECT RzHash('Failed password for root from 192.168.1.100 port 22') AS template_hash;
```

Output:

```text
┌─────────────────────┐
│    template_hash    │
│       uint64        │
├─────────────────────┤
│ 922071830964756268  │
└─────────────────────┘
```

- **Stability**: Stable.
- **Since version**: 1.0.0.

---

## Related

- [Compacting and querying logs in DuckDB](../how-to/duckdb.md): step-by-step query recipes.
- [Universal C ABI reference](c-abi.md): native C API functions and memory contracts.
- [Lossless invertibility](../explanation/lossless-invertibility.md): invariant proof and reconstruction rules.
