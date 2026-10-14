# DuckDB SQL functions reference

Exact function signatures, parameter types, and return values for the Razobill DuckDB extension.
Read this reference if you need formal specifications for Razobill SQL functions.
Skip this reference if you need a step-by-step tutorial.

---

## Overview

The extension exposes exactly four scalar functions with zero aliases.

| Function | Signature | Return Type | Description |
| :--- | :--- | :--- | :--- |
| `RzSplit` | `RzSplit(line: VARCHAR/BLOB)` | `STRUCT(template, ints, strs)` | Decomposes a message into a template, numbers, and words. |
| `RzJoin` | `RzJoin(template, ints, strs)` | `VARCHAR` / `BLOB` | Reconstructs the exact raw log line byte-for-byte. |
| `RzTemplate` | `RzTemplate(line: VARCHAR/BLOB)` | `VARCHAR` / `BLOB` | Extracts only the structural template string. |
| `RzHash` | `RzHash(line: VARCHAR/BLOB)` | `UBIGINT` | Computes a fast 64-bit template hash for indexing. |

---

## Function reference

### `RzSplit(line: VARCHAR) -> STRUCT(template VARCHAR, ints BIGINT[], strs VARCHAR[])`
### `RzSplit(line: BLOB) -> STRUCT(template BLOB, ints BIGINT[], strs BLOB[])`

Decomposes an input string or binary buffer into a composite struct.

- **Parameters**:
  - `line`: The log message to split. Must be `VARCHAR` or `BLOB`. `NULL` inputs yield `NULL`.
- **Returns**:
  - A DuckDB struct with three fields:
    - `template`: Structural template containing typed `{i}` and `{s}` placeholders.
    - `ints`: Contiguous `BIGINT[]` array of extracted 64-bit signed integers.
    - `strs`: `VARCHAR[]` or `BLOB[]` array of extracted variable words.
- **Example**:
  ```sql
  SELECT RzSplit('Failed password for root from 192.168.1.100 port 22') AS res;
  -- res.template: 'Failed password for {s} from {s} port {i}'
  -- res.ints:     [22]
  -- res.strs:     ['root', '192.168.1.100']
  ```

---

### `RzJoin(template, ints, strs) -> VARCHAR / BLOB`
### `RzJoin(template, arg1, arg2, ...) -> VARCHAR / BLOB`

Restores the exact original log line byte-for-byte from decomposed components.

- **Parameters**:
  - `template`: The template string or blob containing `{i}` and `{s}` placeholders.
  - `ints`: `BIGINT[]` array matching the count of `{i}` markers.
  - `strs`: `VARCHAR[]` or `BLOB[]` array matching the count of `{s}` markers.
  - *Varargs variant*: Accepts sequential arguments matching placeholder order.
- **Returns**:
  - Reconstructed string or blob matching the original input line bit-for-bit.
- **Errors**:
  - Throws an error if placeholder counts do not match argument counts.
- **Example**:
  ```sql
  SELECT RzJoin('Failed password for {s} from {s} port {i}', [22], ['root', '192.168.1.100']);
  -- Output: 'Failed password for root from 192.168.1.100 port 22'
  ```

---

### `RzTemplate(line: VARCHAR) -> VARCHAR`
### `RzTemplate(line: BLOB) -> BLOB`

Extracts only the structural template representation of a message.

- **Parameters**:
  - `line`: The input message.
- **Returns**:
  - The template string without allocating arrays for integer or string values.
- **Example**:
  ```sql
  SELECT RzTemplate('Worker 12 finished in 45 ms');
  -- Output: 'Worker {i} finished in {i} ms'
  ```

---

### `RzHash(line: VARCHAR) -> UBIGINT`
### `RzHash(line: BLOB) -> UBIGINT`

Computes a deterministic 64-bit hash (xxHash64) of the structural template.

- **Parameters**:
  - `line`: Input message or template.
- **Returns**:
  - 64-bit unsigned integer (`UBIGINT`) suitable for join partitioning and indexing.
- **Example**:
  ```sql
  SELECT RzHash('Worker 12 finished in 45 ms');
  ```
