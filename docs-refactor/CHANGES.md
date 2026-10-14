# Documentation Migration and Changes Summary (Phase 5)

This document provides the complete record of the documentation refactor under version 1.1.1.

---

## 1. Migration Overview

All documentation has been reorganized from an ad-hoc directory structure into the Diataxis documentation architecture:
- `docs/tutorials/`: Action-oriented tutorials for new adopters.
- `docs/how-to/`: Task-oriented recipes for solving specific engineering integrations.
- `docs/reference/`: Technical API definitions, signatures, and stability policies.
- `docs/explanation/`: Deep systems engineering articles and theoretical proofs.
- `docs/web/`: Website landing page updated to version 1.1.1 with interactive demo and updated links, preserving the visual identity.

---

## 2. Old-to-New Document Mapping

| Old Path | New Path | Status / Notes |
| :--- | :--- | :--- |
| `README.md` | `README.md` | Rewritten to Diataxis front-door standard (three-sentence summary, quick install, 5-minute success, and navigation hub). |
| `RAZOBILL.md` | `README.md` & `docs/explanation/lossless-invertibility.md` | Removed 32KB duplicate treatise; preserved mathematical proofs and citations in dedicated explanation. |
| `docs/Index.md` | `docs/README.md` | Replaced by clean navigation hub with 2-click discovery to all pages. |
| `docs/guides/GettingStarted.md` | `docs/tutorials/getting-started.md` | Rewritten as 5-minute hands-on tutorial with verified outputs. |
| `docs/guides/DuckDbExtensionGuide.md` | `docs/how-to/duckdb.md` | Streamlined to exact 4 `Rz*` functions with real SQL examples. |
| `docs/guides/PythonDeveloperGuide.md` | `docs/how-to/python.md` | Unified Python recipes, batch parsing, and multi-threading. |
| `docs/guides/OpenTelemetryIntegration.md` | `docs/how-to/opentelemetry.md` | Edge collector deployment, configuration, and attribute mapping. |
| `docs/guides/VectorIntegration.md` | `docs/how-to/vector.md` | Rust transform setup and pipeline configuration. |
| `docs/guides/AwsS3Integration.md` | `docs/how-to/s3-compaction.md` | Practical guide for writing compact Parquet stripes to S3. |
| `docs/guides/CPlusPlusDeveloperGuide.md` | `docs/how-to/cpp-developer.md` | CMake integration, compiler flags, and direct C++20 header usage. |
| `docs/guides/ArmNeonOptimization.md` | `docs/how-to/neon-simd.md` | Hardware configuration and dynamic CPUID dispatcher instructions. |
| `docs/reference/SqlFunctionsReference.md` | `docs/reference/sql-functions.md` | Strictly documents 4 registered functions (`RzSplit`, `RzJoin`, `RzTemplate`, `RzHash`). Removed phantom legacy functions. |
| `docs/reference/PythonApiReference.md` | `docs/reference/python-api.md` | Formal API specification for `pyrazobill` exports, types, and exceptions. |
| `docs/reference/UniversalCApiReference.md` | `docs/reference/c-abi.md` | Strict C ABI specification for `RazobillC.h`. |
| `docs/reference/CPlusPlusApiReference.md` | `docs/reference/cpp-api.md` | Core C++20 engine classes and methods. |
| `docs/architecture/ArchitectureOverview.md` | `docs/explanation/architecture.md` | Engine overview, data plane vs control plane, zero-allocation pipeline. |
| `docs/architecture/DataOrientedDesign.md` | `docs/explanation/data-oriented-design.md` | Structure-of-Arrays (SoA), memory arena bump allocation, and cache locality. |
| `docs/architecture/SimdVectorization.md` | `docs/explanation/simd-vectorization.md` | AVX-512, AVX2, and ARM NEON vectorization with branchless trailing-zero counts. |
| `docs/architecture/ConcurrencyModel.md` | `docs/explanation/concurrency-model.md` | Work-stealing thread pool, lock-free thread-local scratchpads, and GIL release. |
| `docs/theory/SemanticGating.md` | `docs/explanation/semantic-vs-exact.md` | Heuristics for token grouping, variable boundaries, and template cardinality. |
| `docs/theory/InformationLedger.md` | `docs/explanation/lossless-invertibility.md` | Mathematical formulation of exact log decomposition and lossless proofs. |
| `docs/benchmarks/DrainComparison.md` | `docs/explanation/benchmarks.md` | Benchmark methodology, throughput comparisons, and memory utilization. |
| `ext/duckdb/README.md` | `ext/duckdb/README.md` | Rewritten to reflect the 4 canonical functions. |
| `ext/otel/README.md` | `ext/otel/README.md` | Rewritten to reflect correct `razobill.ints` and `razobill.strs` attributes. |
| `ext/vector/README.md` | `ext/vector/README.md` | Rewritten with verified pipeline configuration. |
| `lib/python/README.md` | `lib/python/README.md` | Rewritten to match `pyrazobill` v1.1.1. |
| `lib/python/Guide.md` | Removed | Removed duplicate 22KB guide; content consolidated into `docs/how-to/python.md`. |
| `docs/governance/DeprecationPolicy.md` | `docs/reference/deprecation-policy.md` | Preserved SemVer rules and RFC 8594 deprecation cycle. |
| `docs/governance/ReleaseGovernance.md` | `docs/reference/release-governance.md` | Preserved release engineering procedures and checklist. |

---

## 3. Removed Content and Technical Rationale

1. **Phantom SQL Functions**:
   - Removed documentation for `rbDecompose`, `rbForm`, `rbInts`, `rbStrs`, `rbReconstruct`, `rbRebuild`, `rbEntropy`, and `rbMillerMadow`.
   - *Rationale*: These functions were removed from the compiled DuckDB extension during the recent refactor; documenting them broke truth invariants.
2. **Obsolete Non-Printable ASCII Delimiters**:
   - Removed references to `\x12` and `\x13` control bytes in templates.
   - *Rationale*: Modern templates use readable, self-describing `{i}` and `{s}` placeholders.
3. **Internal Information-Theoretic Jargon**:
   - Replaced ungrounded user-facing paper terms (*entropy, ledger, doubt, leakage, Miller-Madow*) with plain software concepts (*template, numbers, words, line, split, join*).
   - *Rationale*: Telemetry operators and database analysts require clear API contracts, not abstract academic derivations. The academic paper citation is preserved respectfully in `NOTICE` and `README.md`.
4. **Hardcoded Test Badges**:
   - Removed 15+ hardcoded badges from `README.md` (e.g. "43/43 Passed", "1,082,866 Passed").
   - *Rationale*: Hardcoded badges become false over time as tests and assertions change. Replaced with live CI and release status badges.

---

## 4. Open Questions for Maintainers

1. **DuckDB Community Extension PR #3023**:
   - Confirm status with upstream DuckDB repository once the streamlined 4-function extension is tagged.
2. **chDB Native ADBC Driver**:
   - `IntegrationChdb.exe` skipped execution because `libchdb.so` / `chdb.dll` was absent in the local build environment. Re-enable in CI once chDB provides official Windows dynamic libraries.
