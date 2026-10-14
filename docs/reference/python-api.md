# Python API reference

Technical reference for pyrazobill module exports, classes, and function signatures.
Read this reference if you need exact parameter types, default values, and exceptions for Python.
Skip this reference if you need an introductory guide.

---

## Overview

The `pyrazobill` module exports four primary functions, two container classes, and one version constant.

| Symbol | Type | Description |
| :--- | :--- | :--- |
| `RzSplit` | Function | Decomposes a log line into an `RzRecord`. |
| `RzJoin` | Function | Reconstructs the exact original line from decomposed components. |
| `RzTemplate` | Function | Extracts the template string or bytes without allocating value lists. |
| `RzSplitMany` | Function | Parses an iterable of messages in parallel releasing the GIL. |
| `RzRecord` | Named Tuple | Structured return container with `(template, ints, strs)`. |
| `RzError` | Exception | Base exception class (subclass of `ValueError`) raised on failures. |
| `__version__` | String | Package semantic version string (`"1.1.1"`). |

Ergonomic aliases are also exported: `split`, `join`, `template`, `split_many`, `Record`, `Error`.

---

## Function reference

### `RzSplit(line: Union[str, bytes], mode: str = "semantic") -> RzRecord`

Decomposes a single log line into a structured record.

- **Parameters**:
  - `line` (*str* or *bytes*): The message to split. Empty string or bytes are valid.
  - `mode` (*{"semantic", "exact"}*, default: `"semantic"`):
    - `"semantic"`: Groups volatile tokens (IPs, paths, hashes) into `{s}` placeholders.
    - `"exact"`: Preserves non-numeric tokens literally in the template.
- **Returns**:
  - `RzRecord`: Named tuple `(template, ints, strs)`.
- **Raises**:
  - `RzError`: If `mode` is invalid or `line` is not `str` or `bytes`.
- **Example**:
  ```python
  import pyrazobill as rz
  rec = rz.RzSplit("port 22 open")
  # rec.template == 'port {i} open'
  # rec.ints == [22]
  # rec.strs == []
  ```

---

### `RzJoin(*args) -> Union[str, bytes]`

Reconstructs the original raw message.

- **Signatures**:
  - `RzJoin(record: RzRecord) -> Union[str, bytes]`
  - `RzJoin(template: Union[str, bytes], ints: Sequence[int], strs: Sequence[Union[str, bytes]]) -> Union[str, bytes]`
- **Parameters**:
  - `template`: Template containing `{i}` and `{s}` markers.
  - `ints`: Sequence of integers matching `{i}` count.
  - `strs`: Sequence of strings or bytes matching `{s}` count.
- **Returns**:
  - The reconstructed string or bytes matching input byte-for-byte.
- **Raises**:
  - `RzError`: If counts do not match or template syntax is malformed.

---

### `RzTemplate(line: Union[str, bytes], mode: str = "semantic") -> Union[str, bytes]`

Extracts only the structural template without allocating numeric or string value arrays.

- **Parameters**:
  - `line` (*str* or *bytes*): Input message.
  - `mode` (*str*): Token scanning mode.
- **Returns**:
  - The template string or bytes.

---

### `RzSplitMany(lines: Iterable[Union[str, bytes]], mode: str = "semantic") -> List[RzRecord]`

Parses an iterable of lines using multi-threaded C++ workers.

- **Parameters**:
  - `lines` (*Iterable*): Sequence or generator of string or byte lines.
  - `mode` (*str*): Token scanning mode.
- **Returns**:
  - List of `RzRecord` instances corresponding to input lines.

---

## Classes

### `class RzRecord`
Named tuple representing a decomposed message:
- `template` (*Union[str, bytes]*): Extracted template.
- `ints` (*List[int]*): Extracted 64-bit signed integers.
- `strs` (*List[Union[str, bytes]]*): Extracted variable string slices.

### `class RzError(ValueError)`
Exception raised on parsing or argument validation failures.
Attributes:
- Includes structured diagnostic messages with what happened, why, and what to do.
