# Python API Reference (`pyrazobill`)

Complete API specification for the `pyrazobill` Python package.

Package: `lib/python/pyrazobill`
Import: `import pyrazobill as rz`

---

## 1. Module Constants & Versioning

- `pyrazobill.__version__`: SemVer release string (e.g. `'1.0.0'`).
- `pyrazobill.__version_info__`: Integer tuple `(major, minor, patch)` (e.g. `(1, 0, 0)`).
- `pyrazobill.__abi_version__`: Integer ABI version token (e.g. `1`).
- `pyrazobill.version() -> str`: Returns `__version__`.
- `pyrazobill.abiVersion() -> int`: Returns `__abi_version__`.
- `pyrazobill.versionInfo() -> tuple[int, int, int]`: Returns `__version_info__`.

---

## 2. Enums

### `pyrazobill.ScanMode`
- `ScanMode.Exact`: Strict digit-bearing token identification.
- `ScanMode.Semantic`: Preserves usernames, hostnames, IPs, UUIDs as slots (default).
- `ScanMode.DigitOnly`: Alias for `Exact`.

---

## 3. Classes

### `pyrazobill.SlotRecord`
Represents the parsed structural breakdown of a log message. Owns its underlying string buffer to guarantee 100% memory safety.

#### Properties (Read-Only)
- `formKey`: `str` — Canonical form key (`skeleton` + `\x13` + `types`).
- `formKeyHash`: `int` — 64-bit XXH64 hash matching ClickHouse `xxHash64`.
- `ints`: `list[int]` — Extracted contiguous 64-bit integer values.
- `strs`: `list[str]` — Extracted variable string slot values.
- `skeleton`: `str` — Constant message text with `0x12` hole markers.
- `types`: `str` — String of `'I'` or `'S'` characters indicating slot types.

#### Methods
- `reconstruct() -> str`: Reassembles and returns the original message with 100% exact fidelity.

---

### `pyrazobill.AuditResult`
Result object returned by `auditParallel()`.

#### Attributes (Read-Only)
- `totalLines`: `int` — Number of log lines evaluated.
- `totalBytes`: `int` — Aggregate byte size of parsed lines.
- `elapsedSec`: `float` — Wall-clock audit duration in seconds.
- `uniqueFormsCount`: `int` — Count of distinct forms discovered.
- `msgsPerSec`: `float` — Processing throughput in lines per second.
- `mbPerSec`: `float` — Processing throughput in MB per second.
- `mismatches`: `int` — Byte mismatches detected upon round-trip reconstruction (always `0` for lossless guarantee).

---

## 4. Functions

### `parse(msg: str, mode: ScanMode = ScanMode.Semantic) -> SlotRecord`
Parses a single log line into a `SlotRecord`.

---

### `parseBatch(msgs: list[str], mode: ScanMode = ScanMode.Semantic) -> list[SlotRecord]`
Parses an array of log messages. Releases the Python GIL during C++ parsing for maximum execution speed.

---

### `decompose(msg: str, mode: ScanMode = ScanMode.Semantic) -> tuple[str, list[int], list[str]]`
Directly decomposes a single log message into a lightweight 3-element tuple `(formKey, ints, strs)`.
- **`formKey`**: `str` — Canonical structural form key.
- **`ints`**: `list[int]` — Extracted 64-bit integer values.
- **`strs`**: `list[str]` — Extracted variable string slot values.

Provides zero-overhead slot extraction without instantiating an intermediate `SlotRecord` wrapper.

---

### `decomposeBatch(msgs: list[str], mode: ScanMode = ScanMode.Semantic) -> list[tuple[str, list[int], list[str]]]`
Vectorized high-throughput batch decomposition that **releases the Python GIL** (`py::gil_scoped_release`).
- Evaluates log messages inside the native C++ SIMD parser.
- Yields a list of `(formKey, ints, strs)` tuples ready for direct insertion into columnar buffers (e.g. DuckDB Appender, PyArrow RecordBatch, Polars DataFrame).
- Delivers **515k+ logs/sec** on a single thread and scales linearly across worker threads (e.g. **737k+ logs/sec** across 4 threads via `concurrent.futures.ThreadPoolExecutor`).

---

### `reconstruct(record: SlotRecord) -> str`
Free-standing reconstruction function. Equivalent to `record.reconstruct()`.

---

### `reconstruct(formKey: str, ints: list[int], strs: list[str]) -> str`
Symmetrical tuple reconstruction function. Reconstructs the exact, bit-for-bit raw log message string directly from its decomposed form key, integer vector, and string vector with 100.000% lossless fidelity.

---

### `formKey(msg: str, mode: ScanMode = ScanMode.Semantic) -> str`
Directly extracts and returns the canonical form key without returning full slot arrays.

---

### `templateOf(formKey: str) -> str`
Converts an internal form key into a human-readable `<*>` template.

---

### `hash64(text: str) -> int`
Computes 64-bit XXH64 hash matching ClickHouse `xxHash64`.

---

### `entropy(counts: list[float]) -> float`
Calculates Shannon entropy in bits from observed frequency counts using Neumaier compensated summation.

---

### `auditParallel(lines: list[str], mode: ScanMode = ScanMode.Semantic, numThreads: int = 0) -> AuditResult`
Executes an embarrassingly parallel audit across all available CPU hardware threads (if `numThreads == 0`). Releases GIL. Verifies round-trip identity for every line and records throughput metrics.

---

### `extensionPath() -> str`
Returns the absolute path to the local compiled DuckDB loadable extension (`librazobill.duckdb_extension`).

---

## 5. Information Theory & Theoretical Ledger

Grounded in [*Latitude and Doubt*](https://doi.org/10.5281/zenodo.23287286) (M. M. Atta, 2026; Zenodo DOI: [10.5281/zenodo.23287286](https://doi.org/10.5281/zenodo.23287286)).

### Classes

#### `pyrazobill.Cell(form: int, meaning: int, mass: float)`
Discrete joint probability cell representing a `(form, meaning)` pair with observed count or probability `mass`.

#### `pyrazobill.XY(form: int, y: int, n: float)`
Joint observation of form identifier and diagnostic consequence $Y$ with occurrence count $n$.

#### `pyrazobill.FormMeaning(form: int, meaning: int, p: float)`
Mapping relation associating a form with semantic meaning class and prior weight $p$.

#### `pyrazobill.Ledger`
Complete information-theoretic ledger computed by `computeLedger()`.
- **Properties**:
  - `coverage`: Sample coverage estimator.
  - `content`: Content entropy $C = H(W)$ in bits.
  - `expression`: Expression entropy $E = H(X)$ in bits.
  - `latitude`: Variable slot latitude $\Lambda = H(X \mid W)$ in bits.
  - `doubt`: Semantic ambiguity $\Delta = H(W \mid X)$ in bits.
  - `conveyance`: Shared mutual information $I = I(W; X)$ in bits.
  - `distance`: Metric distance $D(W, X) = \Lambda + \Delta = E + C - 2I$.
  - `effectiveVocabulary`, `effectiveContent`, `effectiveLatitude`, `effectiveDoubt`: $2^H$ effective diversity numbers.
  - `contentMm`, `expressionMm`, `latitudeMm`, `doubtMm`, `conveyanceMm`: Stratified Miller-Madow bias-corrected values.
  - `contentEpsilon`, `contentHalfWidth`: Proposition 10 confidence intervals.
- **Methods**:
  - `satisfiesLedgerIdentities(tol: float = 1e-9) -> bool`: Asserts $|E - (I + \Lambda)| < \text{tol}$ and $|C - (I + \Delta)| < \text{tol}$.
  - `satisfiesVocabularyLaw(nDistinctForms: int, tol: float = 1e-9) -> bool`: Asserts Corollary 2 bounds.

#### `pyrazobill.ConsequenceDecomposition`
Container for consequence decomposition $E = I(X;Y) + H(M|Y) + H(X|M)$ (Theorem 4c).
- `expression`: Total expression entropy.
- `consequence`: Shared information with diagnostic purpose.
- `hedge`: Residual uncertainty in purpose given abstraction.
- `latitude`: Form latitude.
- `satisfiesIdentity(tol: float = 1e-9) -> bool`: Asserts consequence identity.

#### `pyrazobill.PriceOfSaying`
Container for prefix code expected lengths and wording premium (Theorem 7).
- `expectedMeaningBits`: Expected bits to transmit meaning directly ($nC$).
- `expectedExpressionBits`: Expected bits to transmit forms ($nE$).
- `expectedClarificationBits`: Expected bits of questions to resolve ambiguity ($n\Delta$).
- `totalSayingBits`: Total bits to transmit form and resolve meaning: $n(E + \Delta) = n(C + \Lambda)$.
- `sayingCostBits`: Cost premium of saying what is meant ($n\Lambda$).
- `lowerBoundBits`: Shannon lower bound $n(E + \Delta)$.
- `upperBoundBits`: Prefix code upper bound $n(E + \Delta) + 2$.
- `satisfiesIdentity(tol: float = 1e-9) -> bool`: Verifies Theorem 7 identity.

#### `pyrazobill.HierarchicalStep(form: int, levels: list[int], y: int, mass: float)`
Hierarchical step representation for multi-level functional coarsenings $W_k = f_k(W_{k-1})$.

#### `pyrazobill.DepthLedger`
Multi-level depth decomposition container (Proposition 6).
- `entropyLevels`: List of conditional entropies $H(W_{k-1} \mid W_k)$ from level 1 to $L$.
- `relevanceLevels`: List of conditional mutual informations $I(W_{k-1}; Y \mid W_k)$ from level 1 to $L$.
- `totalEntropy`: Total message entropy $H(X)$.
- `totalRelevance`: Total consequence information $I(X; Y)$.
- `coarsestEntropy`: Coarsest level entropy $H(W_L)$.
- `coarsestRelevance`: Coarsest level consequence information $I(W_L; Y)$.
- `satisfiesIdentity(tol: float = 1e-9) -> bool`: Verifies Proposition 6 telescopic identities.

### Functions

#### `computeLedger(cells: list[Cell], totalEvents: float, confidenceAlpha: float = 0.05) -> Ledger`
Calculates the exact information-theoretic ledger, proving Theorems 1 & 4 balance.

#### `priceOfSaying(ledger: Ledger, nMessages: float) -> PriceOfSaying`
Calculates prefix code length bounds and wording premium (Theorem 7, Atta 2026).

#### `computeDepthLedger(steps: list[HierarchicalStep]) -> DepthLedger`
Computes multi-level depth ledger across successive coarsenings (Proposition 6, Atta 2026).

#### `consequenceDecomposition(xy: list[XY], map: list[FormMeaning]) -> ConsequenceDecomposition`
Computes 3-way consequence decomposition screening off raw strings from consequence $Y$.

#### `millerMadow(pluginEntropy: float, observedCategories: int, sampleSize: float) -> float`
Calculates Miller-Madow bias-corrected entropy in bits.

#### `certifiedInterval(alphabetSize: int, sampleSize: float, confidenceAlpha: float = 0.05) -> tuple[float, float]`
Computes Proposition 10 certified confidence bounds `(epsilon, halfWidth)`.

---

## 6. Latitude Channel & Steganography

Implements Theorem 8 & Proposition 9.

### Classes
- `SynonymousOption(formIndex: int, probability: float)`
- `MeaningChoice(meaning: int, options: list[SynonymousOption])`
- `ChannelCapacity`: `meanCapacityBits`, `variance`, `safePayloadBits`.
- `EmbedResult`: `success: bool`, `payloadBits: int`, `embeddedMessage: int`, `chosenForms: list[int]`.
- `DecodeResult`: `success: bool`, `payloadBits: int`, `message: int`.

### Functions
- `channelAnalyzeCapacity(sequence: list[MeaningChoice]) -> ChannelCapacity`: Profiles transmission capacity $\mu$, variance $\sigma^2$, and safe rate $k = \lfloor \mu - 3\sigma - 7 \rfloor$.
- `channelRequiredKeyBits(sequence: list[MeaningChoice], s: int = 64) -> int`: Computes Proposition 9 minimum key length in bits $P = \lceil \sum_i \max_x (-\log_2 Q(x \mid w_i)) \rceil + s + 1$.
- `channelFailureProbabilityBound(capacity: ChannelCapacity, payloadBits: int, s: float = 64.0) -> float`: Evaluates Theorem 8 Chebyshev upper bound on transmission failure.
- `channelExactFailureProbability(sequence: list[MeaningChoice], payloadBits: int) -> float`: Computes exact analytical failure probability $\delta = \sum Q \min\{1, 2^k Q\}$ via discrete distribution convolution across independent positions.
- `channelEmbed(sequence: list[MeaningChoice], message: int, payloadBits: int, sharedKey: float, dither: float = 0.5) -> EmbedResult`: Embeds integer payload into synonymous form selection.
- `channelDecode(sequence: list[MeaningChoice], observedForms: list[int], payloadBits: int, sharedKey: float) -> DecodeResult`: Decodes payload from observed synonymous forms. Never outputs a corrupted message.

