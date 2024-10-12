<p align="center">
  <img src="../assets/Logo.jpeg" alt="Razobill Logo" width="420" />
</p>

# librazobill & pyrazobill Documentation Suite

Welcome to the authoritative engineering documentation for **`librazobill`** (high-performance C++20 core engine and universal C ABI) and **`pyrazobill`** (minimalist zero-underscore Python client library).

`librazobill` provides **exact byte-for-byte lossless log compaction**, zero-allocation token scanning, and **information-theoretic semantic gating** based on [*Latitude and Doubt: An exact ledger of meaning in messages with measurements on the genetic code and on English*](https://doi.org/10.5281/zenodo.23287286) (M. M. Atta, 2026; Zenodo DOI: [10.5281/zenodo.23287286](https://doi.org/10.5281/zenodo.23287286)), with native integration for **DuckDB** and **chDB (ClickHouse)**.

---

## Documentation Navigation Map

```
docs/
├── Index.md                            # You are here: Navigation map and quick overview
├── architecture/                       # Deep Systems Engineering & Hardware Mechanics
│   ├── ArchitectureOverview.md         # Modular monolith layout, data vs control planes
│   ├── DataOrientedDesign.md           # Structure-of-Arrays (SoA), cache contiguity, MemoryArena
│   ├── SimdVectorization.md            # AVX-512, AVX2, and NEON vectorization, branchless delimiter scans
│   └── ConcurrencyModel.md             # Work-stealing thread pool, chunked scheduling, thread safety
├── theory/                             # Mathematical & Information-Theoretic Rigor
│   ├── InformationLedger.md            # Exact ledger (E = I + Λ, C = I + Δ), proofs, identities
│   ├── SemanticGating.md               # Information leakage L_Y(W), Jensen-Shannon divergence gate
│   ├── FiniteSampleEstimators.md       # Miller-Madow bias correction, Proposition 10 confidence intervals
│   └── OverSplittingMitigation.md      # Exact vs Semantic scan modes, non-numeric variable resolution
├── guides/                             # Developer Workflows & Hands-On Tutorials
│   ├── GettingStarted.md               # Toolchain setup, building from source, running tests
│   ├── CPlusPlusDeveloperGuide.md      # Direct C++20 engine integration, memory arenas, custom backends
│   ├── PythonDeveloperGuide.md         # pyrazobill usage: decompose, decomposeBatch, reconstruct, computeLedger, channelEmbed
│   ├── DuckDBExtensionGuide.md         # Loading extension, rbDecompose (STRUCT), physical table compaction
│   ├── DuckDBCommunityPublishing.md    # Official DuckDB Community Extensions submission manual
│   ├── ClickHouseChdbGuide.md          # chDB ADBC connectivity, xxHash64 parity, materialized columns
│   ├── AwsS3Integration.md             # S3 Parquet export, cost modeling, MinIO/LocalStack, multi-threading
│   ├── ArmNeonOptimization.md          # ARM NEON 128-bit SIMD specialization and cross-arch dispatch
│   ├── OpenTelemetryIntegration.md     # OpenTelemetry Collector processor (razobillprocessor) guide
│   └── VectorIntegration.md            # Vector edge agent remap transform (Rust FFI & VRL) guide
├── reference/                          # Exhaustive Symbol-by-Symbol & API Specifications
│   ├── CPlusPlusApiReference.md        # Complete reference for include/librazobill/ types and methods
│   ├── UniversalCApiReference.md       # Universal C ABI (RazobillC.h) functions and memory ownership
│   ├── PythonApiReference.md           # Comprehensive reference for pyrazobill classes and functions
│   └── SqlFunctionsReference.md        # DuckDB scalar functions: rbDecompose, rbForm, rbInts, rbStrs, rbEntropy, rbReconstruct
├── benchmarks/                         # Empirical Verification & Large-Scale Evidence
│   ├── DrainComparison.md              # Head-to-head empirical comparison vs Drain3 (single & multi-thread)
│   ├── MultiCoreStressAudit.md         # Stress test across 4,861 logs (69.6M events, 6.82 GB, 0 mismatches)
│   ├── StorageCompaction.md            # Physical DuckDB storage compaction ratios vs raw text and Parquet
│   ├── AwsS3CompactionReport.md        # 93.8% S3 reduction, 100k LocalStack stress test, 1 TB/mo & 30 TB/mo cloud savings
│   └── Drain3VsRazobillS3.md           # Final multi-threaded head-to-head Drain3 vs Razobill on S3
└── governance/                         # Software Governance & Provenance
    ├── ReleaseGovernance.md            # SemVer 2.0.0 contract, ABI stability model, CI/CD automation
    ├── DeprecationPolicy.md            # RFC 8594-compliant 3-phase deprecation lifecycle
    └── SbomSpdx.json                   # Machine-readable SPDX 2.3 Software Bill of Materials

ext/                                    # Edge Collector & Analytical Ingestion Extensions
├── duckdb/                             # Official DuckDB community extension build manifest & tests
├── otel/                               # OpenTelemetry Collector processor (Go module with Cgo bindings)
└── vector/                             # Vector Remap Language transform (Cargo crate with Rust FFI bindings)

lib/python/                             # Python Native Package Directory
├── README.md                           # Package overview, quickstart, installation, and benchmarks
└── Guide.md                            # Comprehensive Python developer guide and production recipes
```

---

## Key Invariants & Architectural Contracts

| Invariant | Guarantee | Mathematical / Empirical Proof |
|---|---|---|
| **Lossless Invertibility** | $\text{reconstruct}(\text{parse}(m)) \equiv m$ | 100.000% byte-for-byte exact across 69,619,748 real-world events (0 mismatches). |
| **Exact Information Ledger** | $E = I + \Lambda, \quad C = I + \Delta$ | Maintained across every batch audit; asserted to $|E - (I + \Lambda)| < 10^{-9}$. |
| **Zero Per-Row Heap Allocation** | Memory allocated once per batch | Monotonic 64-byte bump arena (`MemoryArena`) and thread-local scratch reuse. |
| **Zero Underscore Policy** | Modern, ergonomic API naming | Strict PascalCase for classes and camelCase for methods, files, and properties. |
| **Universal FFI Interop** | Pure C ABI (`extern "C"`) | Independent of C++ runtime; callable from Rust, Go, Python, C#, Zig, Java. |
