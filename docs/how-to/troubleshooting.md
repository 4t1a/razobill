# Troubleshooting

[Docs](../README.md) / [How-to guides](troubleshooting.md) / Troubleshooting

This guide provides solutions for common error messages and unexpected behaviors indexed by their exact error strings.

- **Who this is for**: Anyone encountering an error or unexpected output during development or operations.
- **What you will have at the end**: The root cause and resolution for each specific error message.

---

## Errors indexed by message

### `Invalid Input Error: [razobill] What happened: RzJoin failed. Why: Count mismatch`

**What happened**:
`RzJoin` failed because the number of `{i}` and `{s}` placeholders in the template did not match the lengths of the integer and string arrays.

**Why it occurs**:
The template contains $N$ `{i}` markers and $M$ `{s}` markers, but the caller supplied $N' \neq N$ integers or $M' \neq M$ strings.

**How to resolve**:
Ensure your parameter arrays match the placeholder counts exactly:

```sql
-- Template has two {s} and one {i}:
SELECT RzJoin('Failed password for {s} from {s} port {i}', [22]::BIGINT[], ['root', '192.168.1.100']);
```

---

### `IO Error: Extension could not be loaded because its signature is either missing or invalid`

**What happened**:
DuckDB refused to load a locally built `razobill.duckdb_extension` binary:

```text
IO Error: Extension "./build/razobill.duckdb_extension" could not be loaded because its signature is either missing or invalid and unsigned extensions are disabled by configuration.
```

**Why it occurs**:
DuckDB enforces cryptographic signature checks on loadable extensions by default. Local builds and pre-release binaries are not signed with the official DuckDB root key.

**How to resolve**:
Start DuckDB with the `-unsigned` flag:

```bash
duckdb -unsigned
```

Or within Python DuckDB connections:

```python
import duckdb

con = duckdb.connect(config={"allow_unsigned_extensions": "true"})
con.execute("LOAD './build/razobill.duckdb_extension';")
```

---

### Manually constructed integer array produces corrupted numbers in RzJoin

**What happened**:
A query like `SELECT RzJoin('port {i}', [22], []::VARCHAR[]);` outputs an unexpected large number like `port 2774548873238`.

**Why it occurs**:
In DuckDB SQL, literal integer lists like `[22]` default to 32-bit `INTEGER[]`.
`RzJoin` expects 64-bit `BIGINT[]`. Reading 32-bit integers through 64-bit array pointers causes memory aliasing that corrupts the parsed value.

**How to resolve**:
Explicitly cast literal integer arrays to `BIGINT[]`:

```sql
SELECT RzJoin('port {i}', [22]::BIGINT[], []::VARCHAR[]);
-- Output: port 22
```

---

### `RzError: [razobill] What happened: Invalid scan mode`

**What happened**:
Python raised `RzError: [razobill] What happened: Invalid scan mode. Why: Expected 'semantic' or 'exact', got '...'`.

**Why it occurs**:
An unsupported mode string was passed to `split()` or `split_many()`.

**How to resolve**:
Use either `"semantic"` (default multi-modal grouping) or `"exact"` (strict digit-only tokenizer):

```python
import pyrazobill as rz

# Valid calls:
rec1 = rz.split(line, mode="semantic")
rec2 = rz.split(line, mode="exact")
```

---

### `RzError: [razobill] What happened: Unsupported input type`

**What happened**:
Python raised `RzError: [razobill] What happened: Unsupported input type. Why: Expected str or bytes, got int`.

**Why it occurs**:
A non-string object (such as an integer, float, or dictionary) was passed to `split()` or `template()`.

**How to resolve**:
Pass either a `str` or `bytes` object:

```python
import pyrazobill as rz

rec = rz.split(str(payload))
```

---

### RZ_ERR_BUFFER_TOO_SMALL (Code 2)

**What happened**:
In the C ABI, `RzJoin()` or `RzTemplate()` returned `RZ_ERR_BUFFER_TOO_SMALL`.

**Why it occurs**:
The caller provided a destination buffer capacity smaller than the bytes needed to hold the reconstructed message or template.

**How to resolve**:
Inspect `*outLength` after the call. It contains the exact byte capacity needed:

```c
size_t required = 0;
RzStatus status = RzJoin(&record, buffer, capacity, &required);
if (status == RZ_ERR_BUFFER_TOO_SMALL) {
    buffer = (char*)realloc(buffer, required);
    RzJoin(&record, buffer, required, &required);
}
```

---

### RZ_ERR_MESSAGE_TOO_LARGE (Code 5)

**What happened**:
The engine returned `RZ_ERR_MESSAGE_TOO_LARGE` or threw `Status::TooLarge`.

**Why it occurs**:
A single input line or reconstructed message exceeded the 16 MiB boundary (`kMaxMessageBytes = 16 * 1024 * 1024`).

**How to resolve**:
Pre-chunk large log payloads before sending them to Razobill, or verify that the input line does not contain accidental multi-megabyte binary dumps.

---

## Related

- [DuckDB extension guide](duckdb.md): step-by-step SQL workflows.
- [Python developer guide](python.md): Python error handling and thread pools.
- [Universal C ABI reference](../reference/c-abi.md): return codes and memory lifetimes.
