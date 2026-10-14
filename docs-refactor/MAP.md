# Documentation Migration Map (Phase 2)

This document maps every legacy documentation file to its new home under the Diataxis documentation architecture.
All old URLs are preserved with one-line backward-compatible stubs.

---

## 1. Top-Level and Meta Documentation

| Legacy File | New Location | Category | Rationale |
| :--- | :--- | :--- | :--- |
| `README.md` | `README.md` | Front Door | Rewritten as three-sentence summary, quick install, 5-minute success, and navigation hub. |
| `RAZOBILL.md` | `RAZOBILL.md` (stub) & `docs/explanation/lossless-invertibility.md` | Explanation | Removed 32KB manifesto duplicate; preserved mathematical proofs and citations in dedicated explanation. |
| `CONTRIBUTING.md` | `CONTRIBUTING.md` | Governance | Updated with documentation style instructions linking to `docs/STYLE.md`. |
| `CHANGELOG.md` | `CHANGELOG.md` | Governance | Updated with release notes for version 1.1.1 and streamlined `Rz*` API. |
| `SECURITY.md` | `SECURITY.md` | Governance | Preserved verbatim vulnerability disclosure protocol. |
| `CODE_OF_CONDUCT.md` | `CODE_OF_CONDUCT.md` | Governance | Preserved standard contributor covenant. |
| `.github/PULL_REQUEST_TEMPLATE.md` | `.github/PULL_REQUEST_TEMPLATE.md` | Governance | Updated PR checklist to include documentation style and link verification. |

---

## 2. Tutorials (`docs/tutorials/`)

| Legacy File | New Location | Category | Rationale |
| :--- | :--- | :--- | :--- |
| `docs/guides/GettingStarted.md` | `docs/tutorials/getting-started.md` | Tutorial | Rewritten as a 5-minute hands-on tutorial with verified commands and outputs across Python and DuckDB. |
| *(New)* | `docs/tutorials/first-pipeline.md` | Tutorial | End-to-end tutorial showing ingestion of a raw log file into DuckDB Parquet storage. |

---

## 3. How-To Guides (`docs/how-to/`)

| Legacy File | New Location | Category | Rationale |
| :--- | :--- | :--- | :--- |
| `docs/guides/DuckDbExtensionGuide.md` | `docs/how-to/duckdb.md` | How-To | Streamlined to exact 4 `Rz*` functions with real SQL examples. |
| `docs/guides/PythonDeveloperGuide.md` | `docs/how-to/python.md` | How-To | Unified Python recipes, batch parsing, and multi-threaded processing. |
| `docs/guides/OpenTelemetryIntegration.md` | `docs/how-to/opentelemetry.md` | How-To | Edge collector deployment, configuration, and attribute mapping. |
| `docs/guides/VectorIntegration.md` | `docs/how-to/vector.md` | How-To | Rust transform setup and pipeline configuration. |
| `docs/guides/AwsS3Integration.md` | `docs/how-to/s3-compaction.md` | How-To | Practical guide for writing compact Parquet stripes to object storage. |
| `docs/guides/CPlusPlusDeveloperGuide.md` | `docs/how-to/cpp-developer.md` | How-To | CMake integration, compiler flags, and direct C++20 header usage. |
| `docs/guides/ArmNeonOptimization.md` | `docs/how-to/neon-simd.md` | How-To | Hardware configuration and dynamic CPUID dispatcher instructions. |
| `docs/guides/DuckDbCommunityPublishing.md` | `docs/how-to/duckdb-community.md` | How-To | Community extension packaging, vcpkg ports, and CI publishing. |
| `docs/guides/ClickHouseChdbGuide.md` | `docs/how-to/chdb.md` | How-To | In-process ClickHouse (chDB) SQL integration via Python. |

---

## 4. Reference (`docs/reference/`)

| Legacy File | New Location | Category | Rationale |
| :--- | :--- | :--- | :--- |
| `docs/reference/SqlFunctionsReference.md` | `docs/reference/sql-functions.md` | Reference | Strictly documents the 4 registered functions (`RzSplit`, `RzJoin`, `RzTemplate`, `RzHash`). Removed phantom legacy functions. |
| `docs/reference/PythonApiReference.md` | `docs/reference/python-api.md` | Reference | Formal API specification for `pyrazobill` exports, types, and exceptions. |
| `docs/reference/UniversalCApiReference.md` | `docs/reference/c-abi.md` | Reference | Strict C ABI specification for `RazobillC.h` (`RzSplit`, `RzJoin`, etc.). |
| `docs/reference/CPlusPlusApiReference.md` | `docs/reference/cpp-api.md` | Reference | Core C++20 engine classes and methods (`MemoryArena`, `StringPool`, `Scanner`). |
| `docs/governance/DeprecationPolicy.md` | `docs/reference/deprecation-policy.md` | Reference | Semantic versioning, ABI stability guarantees, and deprecation cycle rules. |
| `docs/governance/ReleaseGovernance.md` | `docs/reference/release-governance.md` | Reference | Release engineering checklist, signing, and artifact verification. |
| `lib/python/Publishing.md` | `docs/reference/python-publishing.md` | Reference | Wheel building, cibuildwheel workflows, and PyPI release steps. |

---

## 5. Explanations (`docs/explanation/`)

| Legacy File | New Location | Category | Rationale |
| :--- | :--- | :--- | :--- |
| `docs/architecture/ArchitectureOverview.md` | `docs/explanation/architecture.md` | Explanation | Engine overview, data plane vs control plane, zero-allocation pipeline. |
| `docs/architecture/DataOrientedDesign.md` | `docs/explanation/data-oriented-design.md` | Explanation | Structure-of-Arrays (SoA), memory arena bump allocation, and cache locality. |
| `docs/architecture/SimdVectorization.md` | `docs/explanation/simd-vectorization.md` | Explanation | AVX-512, AVX2, and ARM NEON vectorization with branchless trailing-zero counts. |
| `docs/architecture/ConcurrencyModel.md` | `docs/explanation/concurrency-model.md` | Explanation | Work-stealing thread pool, lock-free thread-local scratchpads, and GIL release. |
| `docs/theory/SemanticGating.md` | `docs/explanation/semantic-vs-exact.md` | Explanation | Heuristics for token grouping, variable boundaries, and template cardinality. |
| `docs/theory/InformationLedger.md` | `docs/explanation/information-ledger.md` | Explanation | Mathematical formulation of exact log decomposition and lossless proofs. |
| `docs/theory/FiniteSampleEstimators.md` | `docs/explanation/finite-sample-estimators.md` | Explanation | Theoretical background on sample efficiency and confidence intervals. |
| `docs/theory/OverSplittingMitigation.md` | `docs/explanation/over-splitting-mitigation.md` | Explanation | Prevention of cluster proliferation and combinatorial explosion. |
| `docs/benchmarks/DrainComparison.md` | `docs/explanation/benchmarks.md` | Explanation | Benchmark methodology, throughput comparisons, and memory utilization. |
| `docs/benchmarks/Drain3VsRazobillS3.md` | `docs/explanation/benchmarks.md` | Explanation | Merged into unified benchmark explanation. |
| `docs/benchmarks/AwsS3CompactionReport.md` | `docs/explanation/s3-benchmarks.md` | Explanation | Compaction performance, Parquet stripe sizes, and cloud egress impact. |
| `docs/benchmarks/StorageCompaction.md` | `docs/explanation/s3-benchmarks.md` | Explanation | Merged into unified S3 storage compaction explanation. |
| `docs/benchmarks/MultiCoreStressAudit.md` | `docs/explanation/stress-audit.md` | Explanation | Multi-core scaling, thread-pool saturation, and memory stability audits. |

---

## 6. Central Hubs and Support Pages

| New File | Category | Purpose |
| :--- | :--- | :--- |
| `docs/README.md` | Index | Navigation hub providing 2-click discovery for every document. |
| `docs/glossary.md` | Reference | Controlled vocabulary, canonical definitions, and banned synonyms. |
| `docs/troubleshooting.md` | How-To / Support | Diagnostic table indexed by verbatim compiler and runtime error strings. |

---

## 7. Sub-Package READMEs

| Sub-Package File | Target State | Purpose |
| :--- | :--- | :--- |
| `lib/python/README.md` | Rewritten | Standalone PyPI package front door matching v1.1.1 API. |
| `lib/python/Guide.md` | Redirect stub | Points to `docs/how-to/python.md` and `docs/reference/python-api.md`. |
| `ext/duckdb/README.md` | Rewritten | DuckDB Community Extension README matching exact 4 functions. |
| `ext/otel/README.md` | Rewritten | OpenTelemetry Collector processor README with correct attributes. |
| `ext/vector/README.md` | Rewritten | Vector transform README with runnable pipeline snippet. |
