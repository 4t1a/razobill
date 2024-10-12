# C++ API Reference (`librazobill`)

Complete API specification for all public types, classes, structs, and functions in `include/librazobill/`.

---

## 1. Core Data Plane (`namespace razobill::core`)

### `enum class ScanMode`
Defined in [`include/librazobill/Scanner.hpp`](../../include/librazobill/Scanner.hpp).
- `ScanMode::Exact`: Variable tokens are strictly those containing at least one ASCII digit.
- `ScanMode::Semantic`: Variable tokens include digit-bearing tokens plus high-entropy identifiers (usernames, hostnames, IPs, UUIDs).
- `ScanMode::DigitOnly`: Alias for `Exact`.

---

### `struct alignas(64) SlotRecord`
Defined in [`include/librazobill/Slots.hpp`](../../include/librazobill/Slots.hpp).

#### Members
- `std::string skeleton`: Constant template text with hole markers (`0x12`).
- `std::string types`: Type signature string containing `'I'` (integer) or `'S'` (string) per slot.
- `std::vector<std::int64_t> ints`: Contiguous 64-bit integer slot values.
- `std::vector<std::string_view> strs`: Non-owning string slices into the parsed message buffer.

#### Methods
- `[[nodiscard]] std::string formKey() const`: Assembles and returns the canonical form key (`skeleton` + `\x13` + `types`).
- `void formKey(std::string& out) const`: Appends canonical form key into an existing string buffer (zero heap allocation).
- `[[nodiscard]] std::uint64_t formKeyHash() const noexcept`: Computes XXH64 hash of the form key.
- `void clear() noexcept`: Resets all internal vectors without releasing capacity.

---

### Functions

#### `Status parse(std::string_view msg, SlotRecord& out, ScanMode mode = ScanMode::Semantic)`
Tokenizes `msg` into constant skeleton text and typed slot values.
- **Parameters**:
  - `msg`: Raw log message view. Max size: 16 MiB.
  - `out`: Output `SlotRecord` to populate.
  - `mode`: Token classification strategy.
- **Returns**: `Status::Ok` on success, or error status code.

#### `Status reconstruct(std::string_view formKey, const std::vector<std::int64_t>& ints, const std::vector<std::string_view>& strs, std::string& out)`
Inverse of `parse`. Reassembles the raw message with 100% byte-for-byte exact fidelity.
- **Complexity**: $O(L)$ where $L$ is message length. Zero dynamic allocations if `out` capacity is pre-reserved.

#### `Status templateOf(std::string_view formKey, std::string& out)`
Converts internal form key into human-readable template replacing holes with `<*>`.

#### `std::uint64_t xxh64(std::string_view s) noexcept`
Computes 64-bit non-cryptographic hash matching ClickHouse `xxHash64`.

---

## 2. Memory Allocators & Utilities

### `class MemoryArena`
Defined in [`include/librazobill/MemoryArena.hpp`](../../include/librazobill/MemoryArena.hpp).
- `explicit MemoryArena(std::size_t defaultChunkSize = 4 * 1024 * 1024)`: Creates bump allocator with 4 MiB initial chunk.
- `void* allocate(std::size_t bytes, std::size_t alignment = 64)`: Bumps allocation pointer on 64-byte alignment.
- `void reset() noexcept`: Reclaims all memory in $O(1)$ by rewinding bump pointers while retaining chunk blocks in a freelist for zero-allocation reuse.
- `std::size_t allocatedBytes() const noexcept`: Total capacity held across all chunks.
- `std::size_t usedBytes() const noexcept`: Currently used bytes in the active chunk sequence.

### `class StringPool`
Defined in [`include/librazobill/StringPool.hpp`](../../include/librazobill/StringPool.hpp).
- `std::uint32_t intern(std::string_view str)`: Interns string, returning unique 32-bit ID. Uses transparent `std::string_view` heterogeneous lookup avoiding temporary allocations.
- `std::string_view get(std::uint32_t id) const`: Retrieves string view by ID.
- `bool contains(std::string_view str) const`: Fast existence check without string copy.
- `void gc(const std::unordered_set<std::uint32_t>& activeIds)`: Mark-and-sweep garbage collection.
- `void compact()`: Defragments string storage.
- `std::size_t size() const noexcept`: Number of unique interned strings.
- `std::size_t allocatedBytes() const noexcept`: Backing memory buffer capacity.

### `class ThreadPool`
Defined in [`include/librazobill/ThreadPool.hpp`](../../include/librazobill/ThreadPool.hpp).
- `explicit ThreadPool(std::size_t numThreads = std::thread::hardware_concurrency())`: Spawns worker pool.
- `template <typename Func> void parallelFor(std::size_t totalItems, std::size_t chunkSize, Func&& func)`: Chunked work-stealing execution.

---

## 3. Information Theory (`namespace razobill::stats`)
Defined in [`include/librazobill/Stats.hpp`](../../include/librazobill/Stats.hpp).

- `Ledger ledger(std::span<const Cell> cells, double totalEvents, double confidenceAlpha = 0.05)`: Computes exact information ledger ($C, E, \Lambda, \Delta, I$, effective vocabulary/perplexities, stratified Miller-Madow corrections, and certified confidence intervals).
- `PriceOfSaying priceOfSaying(const Ledger& ledger, double nMessages) noexcept`: Evaluates Theorem 7 prefix-code expected length bounds and wording premium $n\Lambda$ bits.
- `DepthLedger computeDepthLedger(std::span<const HierarchicalStep> steps)`: Evaluates Proposition 6 multi-level depth decomposition of entropy $H(X)$ and purpose relevance $I(X; Y)$ across successive functional coarsenings.
- `ConsequenceDecomposition consequenceDecomposition(std::span<const XY> xy, std::span<const FormMeaning> map)`: Theorem 4c three-way consequence decomposition $E = I(X; Y) + H(M \mid Y) + H(X \mid M)$ (Consequence + Hedge + Latitude).
- `double millerMadow(double pluginH, std::size_t observedCategories, double sampleCount) noexcept`: Miller-Madow bias correction for finite-sample plug-in entropy.
- `CertifiedInterval certifiedInterval(std::size_t kCategories, double sampleCount, double delta = 0.05) noexcept`: Proposition 10 certified distribution-free confidence interval for small alphabets.
- `double leakage(std::span<const XY> xy, std::span<const FormMeaning> map)`: Theorem 5 information leakage $L_Y(W) = I(X; Y \mid W)$.
- `double entropyOfCounts(std::span<const double> counts) noexcept`: Shannon entropy in bits with Neumaier compensated summation.

---

## 4. Latitude Channel & Steganography (`namespace razobill::channel`)
Defined in [`include/librazobill/Channel.hpp`](../../include/librazobill/Channel.hpp).

- `ChannelCapacity analyzeCapacity(std::span<const MeaningChoice> sequence) noexcept`: Evaluates mean capacity $\mu = \sum H(X \mid w_i)$, variance $\sigma^2 = \sum \mathrm{Var}[-\log_2 Q]$, and safe payload rate $k = \lfloor \mu - 3\sigma - 7 \rfloor$ (Theorem 8).
- `std::size_t requiredKeyBits(std::span<const MeaningChoice> sequence, std::size_t s = 64) noexcept`: Evaluates Proposition 9 minimum key length $P = \lceil \sum_i \max_x(-\log_2 Q(x \mid w_i)) \rceil + s + 1$ bits for dyadic precision.
- `double failureProbabilityBound(const ChannelCapacity& capacity, std::size_t payloadBits, double s = 64.0) noexcept`: Computes Chebyshev upper bound on transmission failure probability (Theorem 8).
- `double exactFailureProbability(std::span<const MeaningChoice> sequence, std::size_t payloadBits) noexcept`: Computes exact analytical failure probability $\delta = \sum Q \min\{1, 2^k Q\}$ via discrete distribution convolution across independent positions.
- `EmbedResult embed(std::span<const MeaningChoice> sequence, std::uint64_t message, std::size_t payloadBits, double sharedKey, double dither = 0.5)`: Arithmetic cumulative interval encoder embedding integer messages into synonymous choices without altering text statistics.
- `DecodeResult decode(std::span<const MeaningChoice> sequence, std::span<const std::size_t> observedForms, std::size_t payloadBits, double sharedKey)`: Decodes embedded payload from observed synonymous forms using shared key $R$. Guaranteed never to output a corrupted message.

---

## 5. High-Level Engine (`class RazobillEngine`)
Defined in [`include/librazobill/Razobill.hpp`](../../include/librazobill/Razobill.hpp).

- `explicit RazobillEngine(Backend& backend, Config config)`: Initializes engine against an ADBC backend.
- `std::size_t compact(std::string_view sourceTable)`: Ingests and compacts table in DuckDB.
- `std::size_t verify(std::string_view sourceTable)`: Executes multiset difference between raw and rebuilt view. Returns number of mismatches (0 = success).
- `void defineRule(std::string_view name, std::string_view pattern, bool force = false)`: Evaluates gate $\text{JS}_\pi \le \varepsilon$; appends event or throws `GateRefusedException`.
- `void retireRule(std::string_view name)`: Appends retire event.
- `LedgerResult ledger()`: Computes exact information ledger ($E, C, \Lambda, \Delta, I$).

---

## 6. Hardware SIMD Vector Scanners (`namespace razobill::simd`)
Defined in [`include/librazobill/SimdScanner.hpp`](../../include/librazobill/SimdScanner.hpp).

### `enum class SimdArch`
- `SimdArch::Scalar`: Standard C++ scalar byte-by-byte traversal with lookup table.
- `SimdArch::Avx2`: Intel/AMD 256-bit AVX2 vector processing (32 bytes per cycle).
- `SimdArch::Avx512`: Intel/AMD 512-bit AVX-512BW vector processing (64 bytes per cycle).
- `SimdArch::Neon`: ARM 128-bit NEON vector processing (16 bytes per cycle).

### Functions
- `SimdArch detectSimdArchitecture() noexcept`: Dynamically probes host CPUID feature flags (`avx512bw`, `avx2`) to identify the optimal SIMD engine.
- `const char* findWhitespaceDynamic(const char* data, std::size_t length) noexcept`: Dynamic multi-version dispatcher that atomically resolves and caches the fastest available SIMD scanning kernel on the host CPU.
- `const char* findWhitespaceAvx512(const char* data, std::size_t length) noexcept`: 64-byte vector scanner using AVX-512BW intrinsics.
- `const char* findWhitespaceAvx2(const char* data, std::size_t length) noexcept`: 32-byte vector scanner using AVX2 intrinsics.
- `const char* findWhitespaceNeon(const char* data, std::size_t length) noexcept`: 16-byte vector scanner using ARM NEON intrinsics.
- `const char* findWhitespaceScalar(const char* data, std::size_t length) noexcept`: Fallback scalar whitespace scanner.

