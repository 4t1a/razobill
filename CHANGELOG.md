# Changelog

All notable changes to **`librazobill`** and **`pyrazobill`** will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html).

---

## [1.0.0] - 2026-10-07

### Added
- **`librazobill` Core C++20 Modular Monolith**:
  - Zero-underscore, publication-grade C++20 engine under [`include/librazobill/`](include/librazobill/).
  - High-performance, zero-allocation token scanning with `ScanMode::Exact` and `ScanMode::Semantic`.
  - Contiguous Structure-of-Arrays (SoA) layout (`SlotRecord`) for cache locality and vectorization.
  - Generational compacting `StringPool` with mark-and-sweep `gc()`, monotonic compaction, and heterogeneous `std::string_view` lookup.
  - Monotonic 64-byte cache-line aligned `MemoryArena` bump allocator with block freelist recycling chunks across resets.
  - Cache-aware work-stealing `ThreadPool` supporting embarrassingly parallel multi-core log auditing.
  - Latitude Channel module (`Channel.hpp`) for channel capacity calculation (Theorem 4c) and slot-based payload embedding/decoding.
- **AVX-512, AVX2 & ARM NEON SIMD Delimiter Scanners**:
  - 64-byte AVX-512BW, 32-byte AVX2, and 16-byte ARM NEON vectorization in `SimdScanner.hpp` utilizing branchless hardware trailing-zero count (`_tzcnt_u64` / `__builtin_ctzll`).
  - Dynamic CPUID multiversion dispatcher (`findWhitespaceDynamic`).
- **Universal C ABI (`RazobillC.h`)**:
  - Zero-overhead `extern "C"` functions (`RzParse`, `RzDecompose`, `RzReconstruct`, `RzReconstructSlots`, `RzTemplateOf`, `RzHash64`, `RzFreeRecord`).
  - Statistical analysis exports (`RzComputeLedger`, `RzMillerMadow`, `RzComputeCertifiedInterval`).
  - Runtime version query functions (`RzVersion`, `RzVersionMajor`, `RzVersionMinor`, `RzVersionPatch`, `RzAbiVersion`).
  - Binary compatibility assertions guaranteeing struct alignment and cross-language FFI safety.
- **Python Client Library (`pyrazobill`)**:
  - Minimalist zero-underscore Python API under `lib/python/pyrazobill`.
  - High-speed direct decomposition tuple API (`decompose`, `decomposeBatch`, `reconstruct`).
  - Mathematical information ledger calculation (`computeLedger`), consequence decomposition, and certified confidence intervals.
  - Latitude Channel capacity profiling, payload embedding, and extraction (`channelAnalyzeCapacity`, `channelEmbed`, `channelDecode`).
  - Memory-safe `PySlotRecord` with heap-pinned string buffer lifetime management.
  - High-throughput multi-threaded parsing releasing GIL.
  - Native version inspection: `pyrazobill.__version__`, `__version_info__`, `version()`, `abiVersion()`.
- **Database Engine Integrations**:
  - Native loadable DuckDB extension (`RazobillDuckdb`) exporting single-pass composite struct function `rbDecompose` returning `STRUCT(form VARCHAR, ints BIGINT[], strs VARCHAR[])` with 66% lower parsing overhead.
  - Individual scalar UDFs: `rbForm`, `rbInts`, `rbStrs`, `rbReconstruct`, `rbDecomposeSemantic`, `rbEntropy`, `rbTemplate`, `rbHash`.
  - Cross-engine identity matching ClickHouse `xxHash64` and ADBC port support.
- **Information Theory Grounding (*Latitude and Doubt*, M. M. Atta 2026)**:
  - Exact information ledger assertions: $E = I + \Lambda$ and $C = I + \Delta$ verified to $10^{-9}$.
  - Adequacy and leakage auditing ($L_Y(W) = I(X;Y) - I(W;Y)$) with Jensen-Shannon divergence gating ($\varepsilon = 0.05$ bits/event).
  - Stratified Miller-Madow finite-sample bias correction and Proposition 10 certified distribution-free confidence intervals.
  - Vocabulary Law (Corollary 2) bounds and variation of information metric distance (Proposition 3).
- **Empirical Validation**:
  - Multi-core stress test across all 4,861 real-world log files (69.6M events, 6.82 GB) at 1.11M msgs/sec with **0 mismatches (100% byte-for-byte exact)**.
  - Head-to-head empirical benchmark against Drain3 demonstrating up to 283.7× throughput speedup, zero entropy doubt ($\Delta = 0.0000$), and 100% lossless reconstruction.
  - DuckDB scale test parsing 1,000,000 rows at 3.7M msg/s with composite struct `rbDecompose` and rebuilding at 10.37M msg/s with 0 errors.
  - 34/34 core unit tests passed (976,444 assertions).
  - 12/12 DuckDB extension integration tests passed (134,890 assertions).
  - 14/14 Python test cases passed.

### Fixed
- Eliminated all dangling string views in Python batch processing via buffer pinning.
- Fixed DuckDB Windows extension loading compatibility requiring `allow_unsigned_extensions`.
- Eliminated dictionary memory bloat via generational mark-and-sweep compaction.
- Mitigated token over-splitting on non-numeric identifiers (usernames, hostnames) via semantic scanning mode.

### Changed
- Promoted project status from experimental prototype (`0.1.0`) to production release (`1.0.0`) under strict SemVer 2.0.0 governance.
- Renamed all public C++, C, Python, and CMake interfaces to adhere to an absolute zero-underscore naming policy.
- Restructured repository layout placing multi-language libraries under `lib/` (`lib/python/`).

---

[1.0.0]: https://github.com/librazobill/razobill/releases/tag/v1.0.0
