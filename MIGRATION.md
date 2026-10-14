# Migration Guide: Razobill v1.x to v2.0

Razobill v2.0 simplifies the public interface across C, Python, and DuckDB SQL. Deprecated aliases, scientific metrics, and redundant function variants have been eliminated in favor of unified `split` and `join` verbs.

---

## 1. SQL (DuckDB Extension)

| Legacy Symbol (v1.x) | New Symbol (v2.0) | Status / Notes |
| :--- | :--- | :--- |
| `rbDecompose(line)` | `razobill_split(line)` | Replaced. Default mode is now semantic. |
| `rb_decompose(line)` | `razobill_split(line)` | Removed alias. |
| `rbDecomposeSemantic(line)` | `razobill_split(line)` | Merged into default behavior. |
| `rb_decompose_semantic(line)` | `razobill_split(line)` | Removed alias. |
| `rbReconstruct(t, i, s)` | `razobill_join(t, i, s)` | Replaced. |
| `rb_join(t, i, s)` | `razobill_join(t, i, s)` | Standardized to `razobill_join`. |
| `rbRebuild(...)` | `razobill_join(...)` | Merged into `razobill_join`. |
| `rb_rebuild(...)` | `razobill_join(...)` | Removed alias. |
| `rbTemplate(line)` | `razobill_template(line)` | Replaced. Returns readable `{i}` / `{s}` format. |
| `rb_template(line)` | `razobill_template(line)` | Removed alias. |
| `rbForm(line)` | `razobill_template(line)` | Merged into `razobill_template`. |
| `rb_form(line)` | `razobill_template(line)` | Removed alias. |
| `rbFormSemantic(line)` | `razobill_template(line)` | Merged into `razobill_template`. |
| `rb_form_semantic(line)` | `razobill_template(line)` | Removed alias. |
| `rbInts(line)` | `razobill_split(line).ints` | Use `razobill_split(line).ints`. Dedicated scalar removed. |
| `rb_ints(line)` | `razobill_split(line).ints` | Removed alias. |
| `rbIntsSemantic(line)` | `razobill_split(line).ints` | Use `razobill_split(line).ints`. Dedicated scalar removed. |
| `rb_ints_semantic(line)` | `razobill_split(line).ints` | Removed alias. |
| `rbStrs(line)` | `razobill_split(line).strs` | Use `razobill_split(line).strs`. Dedicated scalar removed. |
| `rb_strs(line)` | `razobill_split(line).strs` | Removed alias. |
| `rbStrsSemantic(line)` | `razobill_split(line).strs` | Use `razobill_split(line).strs`. Dedicated scalar removed. |
| `rb_strs_semantic(line)` | `razobill_split(line).strs` | Removed alias. |
| `rbHash(line)` | Removed | Use DuckDB native `hash(line)`. |
| `rb_hash(line)` | Removed | Use DuckDB native `hash(line)`. |
| `rbEntropy(counts)` | Removed | Moved to `research/`. Use DuckDB native `entropy()`. |
| `rb_entropy(counts)` | Removed | Removed alias. |
| `rbMillerMadow(...)` | Removed | Moved to `research/`. |
| `rb_miller_madow(...)` | Removed | Removed alias. |

---

## 2. Python (`pyrazobill`)

| Legacy Symbol (v1.x) | New Symbol (v2.0) | Status / Notes |
| :--- | :--- | :--- |
| `parse(msg)` | `split(msg)` | Replaced with plain verb `split`. |
| `decompose(msg)` | `split(msg)` | Merged into `split`. |
| `decomposeBytes(msg)` | `split(msg)` | `split` now accepts `str` or `bytes` and returns matching types. |
| `parseBatch(msgs)` | `split_many(msgs)` | Replaced with `split_many`. |
| `decomposeBatch(msgs)` | `split_many(msgs)` | Merged into `split_many`. |
| `decomposeBatchBytes(msgs)` | `split_many(msgs)` | Unified in `split_many`. |
| `reconstruct(rec)` | `join(template, ints, strs)` | Replaced with `join`. Accepts record or arguments: `join(*split(x))`. |
| `reconstructBytes(...)` | `join(template, ints, strs)` | Unified in `join`. |
| `templateOf(form)` | `template(line)` | Replaced with direct `template(line)`. |
| `templateOfBytes(form)` | `template(line)` | Unified in `template`. |
| `formKey(rec)` | Removed | Replaced by `rec.template`. |
| `formKeyBytes(rec)` | Removed | Replaced by `rec.template`. |
| `hash64(text)` | Removed | Use standard library `hashlib` or built-in hashes. |
| `auditParallel(...)` | Removed | High-speed multi-core processing supported via `split_many`. |
| `extensionPath()` | Removed | Extensions are installed via DuckDB package management. |
| `isNative()` | Removed | Native extension is standard. |
| `ScanMode` | Literal["semantic", "exact"] | Replaced enum with optional string literal `mode="exact"`. |
| `SlotRecord` | `Decomposed` | Replaced by clean named tuple `Decomposed(template, ints, strs)`. |
| `AuditResult` | Removed | Moved to `research/`. |
| `entropy(...)` | Removed | Moved to `research/`. |
| `millerMadow(...)` | Removed | Moved to `research/`. |
| `certifiedInterval(...)` | Removed | Moved to `research/`. |
| `computeLedger(...)` | Removed | Moved to `research/`. |
| `priceOfSaying(...)` | Removed | Moved to `research/`. |
| `computeDepthLedger(...)` | Removed | Moved to `research/`. |
| `consequenceDecomposition(...)` | Removed | Moved to `research/`. |
| `Cell`, `XY`, `FormMeaning`, `Ledger`, `PriceOfSaying`, `DepthLedger`, `HierarchicalStep`, `ConsequenceDecomposition` | Removed | All theoretical metrics moved to `research/`. |
| `channel*` (classes and functions) | Removed | Steganographic channel prototype moved to `research/`. |
| `version()` | `__version__` | Standardized to `__version__`. |
| `abiVersion()` | Removed | Internal ABI versioning. |
| `versionInfo()` | Removed | Replaced by standard `__version__`. |

---

## 3. C ABI (`include/librazobill/RazobillC.h` -> `include/razobill.h`)

| Legacy Symbol (v1.x) | New Symbol (v2.0) | Status / Notes |
| :--- | :--- | :--- |
| `RzParse` | `razobill_split` | Replaced with standard snake_case naming. |
| `RzDecompose` | `razobill_split` | Duplicate entrypoint removed. |
| `RzReconstruct` | `razobill_join` | Replaced. Uses caller-filled `razobill_record_t`. |
| `RzReconstructSlots` | `razobill_join` | Redundant entrypoint removed. |
| `RzTemplateOf` | `razobill_template` | Formats template directly into caller buffer. |
| `RzFreeRecord` | `razobill_free` | Renamed. |
| `RzCreateEngine` | Removed | Unused stub removed. |
| `RzDestroyEngine` | Removed | Unused stub removed. |
| `RzHash64` | Removed | Non-essential helper removed. |
| `RzComputeLedger` | Removed | Moved to `research/`. |
| `RzMillerMadow` | Removed | Moved to `research/`. |
| `RzComputeCertifiedInterval` | Removed | Moved to `research/`. |
| `RzComputePriceOfSaying` | Removed | Moved to `research/`. |
| `RzVersion` | `razobill_version` | Renamed. |
| `RzVersionMajor`, `RzVersionMinor`, `RzVersionPatch` | Removed | Use `razobill_version()`. |
| `RzAbiVersion` | Removed | Internal symbol removed from public header. |
| `RzSlotRecord` | `razobill_record_t` | Renamed. Opaque internal handle hidden. |
| `RzStatusCode` | `razobill_status_t` | Renamed. `razobill_status_message()` added. |
| `RzScanMode` | `razobill_mode_t` | Internal / optional mode parameter. |
| `RzCell`, `RzLedger`, `RzCertifiedInterval`, `RzPriceOfSaying` | Removed | Research types moved to `research/`. |
