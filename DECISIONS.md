# Architecture & Surface Decisions: Razobill v2.0

## 1. Public Surface Budget & Kept Symbols

Every public name serves exactly one concrete developer task. Redundant aliases, deprecated variants, and internal engine stubs are removed.

### 1.1 SQL (DuckDB Extension) — Budget: <= 4 functions, 0 aliases (Used: 4 functions)
| Function | Return Type | Developer Task Served |
| :--- | :--- | :--- |
| `RzSplit(line)` | `STRUCT(template, ints, strs)` | Decomposes a log message into structured template, numbers, and word strings for analytical indexing. |
| `RzJoin(template, ints, strs)` | `VARCHAR` / `BLOB` | Reconstructs the exact original log message byte-for-byte from decomposed components (also supports varargs). |
| `RzTemplate(line)` | `VARCHAR` / `BLOB` | Extracts only the structural template for high-cardinality aggregation without slot allocation. |
| `RzHash(line)` | `UBIGINT` | Computes fast, deterministic 64-bit hash (xxHash64) of a template or message for identity and indexing. |

- *Prefix collision check*: Verified against `duckdb_functions()`. Zero collisions exist for `Rz` prefix in DuckDB built-ins.
- *Aliases*: 0 aliases registered. `rb_join`, `rb_rebuild`, `rbForm`, `rbInts`, `rbEntropy`, etc. are removed.

### 1.2 Python (`pyrazobill`) — Budget: <= 8 names in `__all__` (Used: 7 names)
| Symbol | Type | Developer Task Served |
| :--- | :--- | :--- |
| `split(line, mode=...)` | Function | Parses a single log line into a named tuple `Decomposed(template, ints, strs)`. |
| `join(template, ints, strs)` | Function | Reconstructs the exact original message such that `join(*split(x)) == x`. |
| `template(line, mode=...)` | Function | Extracts only the template string or bytes without allocating slot lists. |
| `split_many(lines, mode=...)` | Function | Parses an iterable of messages releasing the GIL across background worker threads. |
| `Decomposed` | Named Tuple | Structured return container with `template`, `ints`, and `strs` fields. |
| `Error` | Exception | Base exception class (subclass of `ValueError`) raised on invalid inputs or malformed templates. |
| `__version__` | String | Exposes semantic package version. |

### 1.3 C ABI (`razobill.h`) — Budget: <= 6 functions, <= 3 types (Used: 6 functions, 2 types)
| Function / Type | Role | Developer Task Served |
| :--- | :--- | :--- |
| `razobill_split` | Function | Parses a buffer into decomposed template string and integer/string arrays. |
| `razobill_join` | Function | Reconstructs the exact byte string from a `razobill_record_t` structure. |
| `razobill_template` | Function | Formats the template directly into caller-provided buffer without allocating slot lists. |
| `razobill_free` | Function | Safely frees heap buffers allocated by `razobill_split`. |
| `razobill_status_message` | Function | Returns a static human-readable description for any `razobill_status_t`. |
| `razobill_version` | Function | Returns the runtime library version string. |
| `razobill_record_t` | Type (Struct) | Standard container for decomposed log data and caller-filled input to `razobill_join`. |
| `razobill_status_t` | Type (Enum) | Numeric status code indicating success or specific failure reason. |

---

## 2. Template Format

### Format Specification
- Number placeholder: `{i}` (64-bit integer parsed and formatted via `std::to_chars` / `std::from_chars`).
- String placeholder: `{s}` (variable word, path, IP, or token).
- Literal brace escaping: `{{` represents a literal `{` character; `}}` represents a literal `}` character.

### Rationale & Invariants
1. **Unambiguity**: Replaces ambiguous `{}` and `<*>` with typed, self-describing placeholders `{i}` and `{s}`.
2. **Lossless Invertibility**: Any arbitrary byte string containing `{` or `}` escapes cleanly as `{{` or `}}` and reconstructs without distortion.
3. **Parquet & Storage Migration**: For Parquet datasets previously partitioned on legacy `\x12`/`\x13` or `<*>` forms, a one-line SQL regex migration is provided:
   ```sql
   REPLACE(REPLACE(legacy_form, '\x12', '{s}'), '<*>', '{s}')
   ```

---

## 3. Mode Default: Semantic Scan

### Measured Evidence on Real Corpora
| Dataset | Lines Evaluated | Exact Mode Unique Templates | Semantic Mode Unique Templates | Reduction Factor | Lossless Invertibility |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `logs/SSH/SSH.log` | 10,000 | 517 | 120 | **4.31x** | 100% byte-identical |
| `logs/Zookeeper/Zookeeper.log` | 10,000 | 86 | 86 | **1.00x** | 100% byte-identical |

- **Decision**: **Semantic mode** is the single default across C, Python, and DuckDB SQL.
- **Behavior**: Variable tokens like IP addresses, usernames, and hex hashes are grouped into string slots `{s}`, avoiding template explosion while maintaining zero loss of input bytes.
- The underlying exact tokenizer remains accessible via the explicit `mode="exact"` optional argument in Python and C flags if strict token-boundary splitting is required.

---

## 4. Naming Conventions & Plain Language

### Ecosystem Naming Rules
- **SQL & DuckDB**: `lower_snake_case` prefixed by `razobill_` (e.g. `razobill_split`, `razobill_join`). No camelCase, no legacy prefixes.
- **Python**: PEP 8 compliant `lower_snake_case` (e.g. `split`, `join`, `split_many`).
- **C ABI**: `razobill_` prefix with `lower_snake_case` for functions/types; `RAZOBILL_` prefix with uppercase for enum constants. Replaces the legacy "zero-underscore" rule.
- **Header**: Single unified header `include/razobill.h`. Library file named `razobill` (shared) and `razobill_core` (static), eliminating double prefixes (`liblib...`).

### Plain Language Policy
The public API and documentation strictly ban internal information-theory terms in user-facing surfaces:
- Banned terms: *entropy, ledger, latitude, doubt, conveyance, leakage, channel capacity, Miller-Madow, certified interval, Shannon, arena, slot, form key*.
- Plain vocabulary: *template, numbers, words, line, split, join, exact, semantic*.
- Historical paper citation is retained respectfully in `NOTICE` and in a brief footnote at the bottom of `README.md`.
