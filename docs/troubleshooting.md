# Troubleshooting guide

Diagnose and resolve common build, configuration, and runtime errors.
Read this page if you encountered an error string in your terminal or logs.
Skip this page if your pipeline is running normally.

---

## Runtime error strings

### `Template contains malformed placeholders or unescaped braces`

**What happened**:
`RzJoin` or the C ABI `RzJoin` encountered a template string containing an unescaped `{` or `}` that was not part of a valid `{i}` or `{s}` placeholder.

**Why it occurs**:
Literal curly braces inside raw log lines must be escaped as `{{` or `}}` if you are manually creating or editing template strings.

**How to resolve**:

1. If constructing templates manually, replace `{` with `{{` and `}` with `}}` for literal characters.
2. If splitting input lines with `RzSplit`, unescaped braces in the input message are automatically escaped into `{{` or `}}`.
3. Check your template string for unclosed placeholders like `{` or `{invalid}`.

---

### `Number of provided values does not match template placeholders`

**What happened**:
`RzJoin` was called with a template expecting a specific number of integer or string placeholders, but the provided arrays contained a different count.

**Why it occurs**:
The template contains $N$ `{i}` placeholders and $M$ `{s}` placeholders, but the arguments passed to `RzJoin` had $N' \neq N$ integers or $M' \neq M$ strings.

**How to resolve**:
Ensure that the number of elements in the `ints` array equals the count of `{i}` markers in the template, and the number of elements in `strs` equals the count of `{s}` markers:

```python
import pyrazobill as rz

# Ensure unpacked record fields match the template
rec = rz.RzSplit(line)
reconstructed = rz.RzJoin(rec.template, rec.ints, rec.strs)
```

---

### `Destination buffer too small`

**What happened**:
In the C ABI, `RzJoin` or `RzTemplate` was called with a destination buffer capacity smaller than the bytes required for the output.

**Why it occurs**:
The caller allocated an output buffer with insufficient bytes to hold the reconstructed message or template string.

**How to resolve**:
Check `outLength` after the call. When `RZ_ERR_BUFFER_TOO_SMALL` is returned, `*outLength` is populated with the exact required byte capacity:

```c
size_t required = 0;
RzStatus status = RzJoin(&record, buffer, capacity, &required);
if (status == RZ_ERR_BUFFER_TOO_SMALL) {
    buffer = (char*)realloc(buffer, required);
    RzJoin(&record, buffer, required, &required);
}
```

---

### `Invalid scan mode`

**What happened**:
Python raised an `RzError` stating `[razobill] What happened: Invalid scan mode.`

**Why it occurs**:
A mode argument other than `"semantic"` or `"exact"` was passed to `RzSplit` or `RzSplitMany`.

**How to resolve**:
Specify either `"semantic"` (default grouping behavior) or `"exact"` (strict non-grouping tokenizer):

```python
import pyrazobill as rz

# Valid invocations
rec1 = rz.RzSplit(line, mode="semantic")
rec2 = rz.RzSplit(line, mode="exact")
```

---

### `Message exceeds maximum allowed size`

**What happened**:
`RzSplit` returned status code `RZ_ERR_MESSAGE_TOO_LARGE` (code 5).

**Why it occurs**:
The input message exceeded the maximum buffer boundary configured in the engine scanner (typically 16 MB per single log line).

**How to resolve**:

1. Check if an unescaped binary dump or crash log was erroneously passed as a single line.
2. Truncate or pre-split the input message into individual log lines before calling `RzSplit`.

---

## Database and extension error strings

### `Extension could not be loaded because its signature is either missing or invalid`

**What happened**:
DuckDB failed to load `razobill.duckdb_extension` with an I/O error regarding missing or invalid signatures.

**Why it occurs**:
Local development builds of DuckDB extensions are unsigned. DuckDB blocks loading unsigned extensions by default for security.

**How to resolve**:

1. For production environments, install the signed community build:

   ```sql
   INSTALL razobill FROM community;
   LOAD razobill;
   ```

2. For local testing, allow unsigned extensions in your DuckDB connection:
   - In DuckDB CLI: `duckdb -unsigned`
   - In Python: `duckdb.connect(config={'allow_unsigned_extensions': 'true'})`

---

## Build and compiler error strings

### `Cannot open include file: 'io.h'`

**What happened**:
During `pip install pyrazobill` on Windows, MSVC failed with `fatal error C1083: Cannot open include file: 'io.h'`.

**Why it occurs**:
The Universal C Runtime (UCRT) SDK headers are missing or not in MSVC's include search paths.

**How to resolve**:

1. `setup.py` automatically detects UCRT paths in Windows 10/11 SDK installations. Ensure the Windows SDK is installed via Visual Studio Installer.
2. Alternatively, install pre-built binary wheels directly from PyPI which do not require a local C++ compiler:

   ```bash
   pip install --only-binary :all: pyrazobill
   ```
