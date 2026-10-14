# Documentation Inventory (Phase 0)

- **Audit Date**: 2026-10-14
- **Target Version**: 1.1.1
- **Total Files Audited**: 65
- **Files with Broken Links**: 1 (`docs/guides/CPlusPlusDeveloperGuide.md`)
- **Files with Deprecated Symbols**: 20

## 1. Toolchain & Environment

| Tool | Version / Path | Execution Status | Notes |
| :--- | :--- | :--- | :--- |
| PowerShell | pwsh (Windows) | ACTIVE | Native command execution verified |
| Python | 3.13.2 | ACTIVE | Unit tests run: 18/18 passed in 1.11s |
| C++ Test Suite | UnitTests.exe | ACTIVE | 41/41 test cases passed, 1,082,496 assertions passed |
| DuckDB Extension Test | IntegrationDuckdb.exe | ACTIVE | 13/13 test cases passed, 134,890 assertions passed |
| Node.js / npx | v22.13.0 | ACTIVE | Available for markdownlint-cli2 and cspell |
| Cargo / Rust | 1.84.0 | ACTIVE | Available for Vector adapter testing |
| Git | 2.50.0 | ACTIVE | Branch `docs/identity-refactor` |

## 2. Codebase Ground Truth Audit

An automated audit of C++ headers, Python exports, and DuckDB extension entrypoints established the following ground truths:

1. **Version**: Standardized to `1.1.1` across `VERSION`, `Version.hpp`, `pyproject.toml`, `setup.py`, and `pyrazobill/__init__.py`.
2. **Universal C ABI (`include/librazobill/RazobillC.h`)**:
   - Exactly 6 functions: `RzSplit`, `RzJoin`, `RzTemplate`, `RzFree`, `RzStatusMessage`, `RzVersion`.
   - Exactly 2 types: `RzRecord`, `RzStatus`.
   - Removed/Deprecated: `RzParse`, `RzDecompose`, `RzReconstructSlots`, `RzTemplateOf`, `RzHash64`, `RzComputeLedger`, `RzMillerMadow`, `RzComputeCertifiedInterval`.
3. **Python Package (`pyrazobill`)**:
   - Primary functions: `RzSplit`, `RzJoin`, `RzTemplate`, `RzSplitMany`.
   - Ergonomic aliases: `split`, `join`, `template`, `split_many`.
   - Classes: `RzRecord` (alias `Record`), `RzError` (alias `Error`).
   - Version: `__version__ = "1.1.1"`.
   - Removed: `decompose`, `reconstruct`, `computeLedger`, `entropy`, `channelAnalyzeCapacity`.
4. **DuckDB Extension (`ext/duckdb`)**:
   - Exactly 4 registered functions, 0 aliases: `RzSplit`, `RzJoin`, `RzTemplate`, `RzHash`.
   - Removed/Deprecated: `rbDecompose`, `rbForm`, `rbInts`, `rbStrs`, `rbReconstruct`, `rbRebuild`, `rbEntropy`, `rbMillerMadow`.
5. **Template Placeholder Syntax**:
   - Integer slot: `{i}`.
   - String slot: `{s}`.
   - Literal brace escaping: `{{` and `}}`.
   - Removed: Non-printable bytes `\x12`, `\x13`, and ambiguous `<*>`.
6. **Default Mode**:
   - `mode="semantic"` is default across C, Python, and SQL.
   - `mode="exact"` is the strict non-grouping mode.

## 3. Comprehensive File Inventory

| File Path | Size | Last Changed | Purpose | Audience | Duplicates / Overlap | Broken Links | Unverified Claims / Deprecated APIs |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `.github/PULL_REQUEST_TEMPLATE.md` | 16L (876B) | 2024-10-12 | Documentation | General Developers | None | None | None |
| `CHANGELOG.md` | 84L (6272B) | 2026-10-13 | Release notes and version history | All Users | None | None | Deprecated v1.x symbols: rbDecompose, rbForm, rbInts, rbStrs, rbReconstruct, rbEntropy, RzParse, RzDecompose, RzReconstructSlots, RzTemplateOf, RzHash64, RzComputeLedger, RzMillerMadow, RzComputeCertifiedInterval |
| `CODE_OF_CONDUCT.md` | 65L (2671B) | 2024-10-12 | Community standards and conduct rules | Community | None | None | None |
| `CONTRIBUTING.md` | 83L (3085B) | 2024-10-12 | Contribution and development guidelines | Contributors | None | None | None |
| `docs/architecture/ArchitectureOverview.md` | 120L (10724B) | 2024-10-12 | Internal engine architecture and design principles | Systems Engineers / Architects | None | None | Deprecated v1.x symbols: rbDecompose, rbForm, rbInts, rbStrs |
| `docs/architecture/ConcurrencyModel.md` | 89L (5842B) | 2024-10-12 | Internal engine architecture and design principles | Systems Engineers / Architects | None | None | None |
| `docs/architecture/DataOrientedDesign.md` | 146L (11041B) | 2024-10-12 | Internal engine architecture and design principles | Systems Engineers / Architects | None | None | None |
| `docs/architecture/SimdVectorization.md` | 111L (6797B) | 2024-10-12 | Internal engine architecture and design principles | Systems Engineers / Architects | None | None | None |
| `docs/benchmarks/AwsS3CompactionReport.md` | 188L (15811B) | 2024-10-12 | Empirical performance and compression reports | Performance Engineers | None | None | Deprecated v1.x symbols: rbForm, rbInts, rbStrs |
| `docs/benchmarks/Drain3VsRazobillS3.md` | 89L (8546B) | 2024-10-12 | Empirical performance and compression reports | Performance Engineers | None | None | Deprecated v1.x symbols: rbForm, rbInts, rbStrs, rbReconstruct |
| `docs/benchmarks/DrainComparison.md` | 233L (20771B) | 2024-10-12 | Empirical performance and compression reports | Performance Engineers | None | None | Deprecated v1.x symbols: RzParse, RzDecompose, RzReconstructSlots |
| `docs/benchmarks/MultiCoreStressAudit.md` | 149L (8721B) | 2024-10-12 | Empirical performance and compression reports | Performance Engineers | None | None | None |
| `docs/benchmarks/StorageCompaction.md` | 139L (8555B) | 2024-10-12 | Empirical performance and compression reports | Performance Engineers | None | None | Deprecated v1.x symbols: rbDecompose, rbReconstruct |
| `docs/governance/DeprecationPolicy.md` | 59L (2141B) | 2026-10-13 | Project versioning, deprecation, and release governance | Maintainers / Contributors | None | None | None |
| `docs/governance/ReleaseGovernance.md` | 77L (4081B) | 2024-10-12 | Project versioning, deprecation, and release governance | Maintainers / Contributors | None | None | None |
| `docs/guides/ArmNeonOptimization.md` | 101L (5419B) | 2024-10-12 | Integration recipes and development tutorials | Application Integrators | None | None | None |
| `docs/guides/AwsS3Integration.md` | 383L (14050B) | 2024-10-12 | Integration recipes and development tutorials | Application Integrators | None | None | Deprecated v1.x symbols: rbDecompose, rbForm, rbInts, rbStrs, rbReconstruct, RzParse |
| `docs/guides/ClickHouseChdbGuide.md` | 84L (4128B) | 2024-10-12 | Integration recipes and development tutorials | Application Integrators | None | None | None |
| `docs/guides/CPlusPlusDeveloperGuide.md` | 131L (3872B) | 2024-10-12 | Integration recipes and development tutorials | Application Integrators | None | std::size_t start, std::size_t end | None |
| `docs/guides/DuckDbCommunityPublishing.md` | 128L (4822B) | 2026-10-13 | Integration recipes and development tutorials | Application Integrators | None | None | None |
| `docs/guides/DuckDbExtensionGuide.md` | 116L (5589B) | 2024-10-12 | Integration recipes and development tutorials | Application Integrators | None | None | Deprecated v1.x symbols: rbDecompose, rbForm, rbInts, rbStrs, rbReconstruct, rbEntropy |
| `docs/guides/GettingStarted.md` | 91L (2504B) | 2024-10-12 | Integration recipes and development tutorials | Application Integrators | None | None | None |
| `docs/guides/OpenTelemetryIntegration.md` | 128L (5820B) | 2024-10-12 | Integration recipes and development tutorials | Application Integrators | None | None | None |
| `docs/guides/PythonDeveloperGuide.md` | 227L (6906B) | 2024-10-12 | Integration recipes and development tutorials | Application Integrators | None | None | Deprecated v1.x symbols: rbDecompose, rbReconstruct |
| `docs/guides/VectorIntegration.md` | 189L (6370B) | 2024-10-12 | Integration recipes and development tutorials | Application Integrators | None | None | Deprecated v1.x symbols: RzDecompose |
| `docs/Index.md` | 75L (6500B) | 2024-10-12 | Documentation | General Developers | None | None | Deprecated v1.x symbols: rbDecompose, rbForm, rbInts, rbStrs, rbReconstruct, rbEntropy |
| `docs/reference/CPlusPlusApiReference.md` | 137L (10063B) | 2024-10-12 | Formal API signatures and syntax specifications | Software Developers | None | None | None |
| `docs/reference/PythonApiReference.md` | 236L (11629B) | 2024-10-12 | Formal API signatures and syntax specifications | Software Developers | None | None | None |
| `docs/reference/SqlFunctionsReference.md` | 300L (12184B) | 2024-10-12 | Formal API signatures and syntax specifications | Software Developers | None | None | Deprecated v1.x symbols: rbDecompose, rbForm, rbInts, rbStrs, rbReconstruct, rbRebuild, rbEntropy, rbMillerMadow |
| `docs/reference/UniversalCApiReference.md` | 283L (9134B) | 2024-10-12 | Formal API signatures and syntax specifications | Software Developers | None | None | Deprecated v1.x symbols: RzParse, RzDecompose, RzReconstructSlots, RzTemplateOf, RzHash64, RzComputeLedger, RzMillerMadow, RzComputeCertifiedInterval |
| `docs/results/BenchCore.txt` | 6L (498B) | 2024-10-12 | Raw test and benchmark execution output logs | Maintainers | None | None | None |
| `docs/results/BenchStorage.txt` | 25L (1645B) | 2024-10-12 | Raw test and benchmark execution output logs | Maintainers | None | None | None |
| `docs/results/IntegrationChdb.txt` | 11L (515B) | 2024-10-12 | Raw test and benchmark execution output logs | Maintainers | None | None | None |
| `docs/results/IntegrationDuckdb.txt` | 20L (805B) | 2024-10-12 | Raw test and benchmark execution output logs | Maintainers | None | None | None |
| `docs/results/StressAsanUbsan.txt` | 1L (48B) | 2024-10-12 | Raw test and benchmark execution output logs | Maintainers | None | None | None |
| `docs/results/StressTsan.txt` | 1L (48B) | 2024-10-12 | Raw test and benchmark execution output logs | Maintainers | None | None | None |
| `docs/results/UnitTests.txt` | 6L (310B) | 2024-10-12 | Raw test and benchmark execution output logs | Maintainers | None | None | None |
| `docs/results/UnitTestsAsanUbsan.txt` | 6L (310B) | 2024-10-12 | Raw test and benchmark execution output logs | Maintainers | None | None | None |
| `docs/theory/FiniteSampleEstimators.md` | 131L (8219B) | 2024-10-12 | Mathematical proofs and information theory derivations | Researchers / Academics | None | None | None |
| `docs/theory/InformationLedger.md` | 204L (13508B) | 2024-10-12 | Mathematical proofs and information theory derivations | Researchers / Academics | None | None | None |
| `docs/theory/OverSplittingMitigation.md` | 97L (7015B) | 2024-10-12 | Mathematical proofs and information theory derivations | Researchers / Academics | None | None | Deprecated v1.x symbols: rbDecompose |
| `docs/theory/SemanticGating.md` | 126L (7780B) | 2024-10-12 | Mathematical proofs and information theory derivations | Researchers / Academics | None | None | None |
| `docs/web/llms.txt` | 24L (1929B) | 2026-10-13 | Documentation | General Developers | None | None | None |
| `docs/web/robots.txt` | 16L (308B) | 2026-10-13 | Documentation | General Developers | None | None | None |
| `docs/web/SETUP.md` | 12L (1115B) | 2026-10-13 | Documentation | General Developers | None | None | None |
| `docs-refactor/STATE.md` | 21L (889B) | | Documentation | General Developers | None | None | None |
| `evidence/README.md` | 240L (19877B) | 2024-10-12 | Experimental audit logs and empirical verification outputs | Researchers / Maintainers | None | None | None |
| `evidence/ResultsE4bLeakage.txt` | 11L (1241B) | 2024-10-12 | Experimental audit logs and empirical verification outputs | Researchers / Maintainers | None | None | None |
| `evidence/ResultsE6Logtype.txt` | 19L (1812B) | 2024-10-12 | Experimental audit logs and empirical verification outputs | Researchers / Maintainers | None | None | None |
| `evidence/ResultsE7Default.txt` | 7L (1414B) | 2024-10-12 | Experimental audit logs and empirical verification outputs | Researchers / Maintainers | None | None | None |
| `ext/duckdb/README.md` | 112L (3902B) | 2024-10-12 | DuckDB community extension quickstart and documentation | DuckDB Analysts | Overlaps with docs/guides/DuckDbExtensionGuide.md and docs/reference/SqlFunctionsReference.md | None | Deprecated v1.x symbols: rbDecompose, rbForm, rbInts, rbStrs, rbReconstruct, rbRebuild, rbEntropy |
| `ext/otel/README.md` | 88L (2888B) | 2024-10-12 | OpenTelemetry Collector processor documentation | SREs / Observability Engineers | Overlaps with docs/guides/OpenTelemetryIntegration.md | None | Deprecated v1.x symbols: rbReconstruct, RzReconstructSlots |
| `ext/vector/README.md` | 43L (1553B) | 2024-10-12 | Vector transform adapter documentation | SREs / Pipeline Engineers | Overlaps with docs/guides/VectorIntegration.md | None | None |
| `extension-ci-tools/docker/README.md` | 5L (388B) | | Documentation | General Developers | None | None | None |
| `extension-ci-tools/README.md` | 27L (2249B) | | Documentation | General Developers | None | None | None |
| `extension-ci-tools/vcpkg_ports/README.md` | 4L (200B) | | Documentation | General Developers | None | None | None |
| `lib/python/Guide.md` | 487L (22369B) | 2024-10-12 | Python package documentation, guide, and publishing notes | Python Developers | Overlaps with docs/guides/PythonDeveloperGuide.md and docs/reference/PythonApiReference.md | None | Deprecated v1.x symbols: rbDecompose, rbReconstruct |
| `lib/python/Publishing.md` | 119L (3623B) | 2024-10-12 | Python package documentation, guide, and publishing notes | Python Developers | Overlaps with docs/guides/PythonDeveloperGuide.md and docs/reference/PythonApiReference.md | None | None |
| `lib/python/README.md` | 280L (12523B) | 2026-10-13 | Python package documentation, guide, and publishing notes | Python Developers | Overlaps with docs/guides/PythonDeveloperGuide.md and docs/reference/PythonApiReference.md | None | Deprecated v1.x symbols: rbDecompose, rbReconstruct |
| `logs/Android_v2/README.md` | 7L (912B) | 2024-10-12 | Log dataset corpus descriptions and annotations | Benchmark Evaluators | None | None | None |
| `logs/BGL/README.md` | 9L (1246B) | 2024-10-12 | Log dataset corpus descriptions and annotations | Benchmark Evaluators | None | None | None |
| `logs/Hadoop/README.md` | 18L (2141B) | 2024-10-12 | Log dataset corpus descriptions and annotations | Benchmark Evaluators | None | None | None |
| `RAZOBILL.md` | 442L (32393B) | 2026-10-14 | Full comprehensive treatise and technical manifesto | General Audience | Overlaps almost 100% with README.md | None | Deprecated v1.x symbols: rbDecompose, rbForm, rbInts, rbStrs, rbReconstruct, rbEntropy, RzParse, RzDecompose, RzReconstructSlots, RzTemplateOf, RzHash64, RzComputeLedger, RzMillerMadow, RzComputeCertifiedInterval |
| `README.md` | 392L (34418B) | 2026-10-14 | Project front door, overview, quickstart | All Developers | Overlaps almost 100% with RAZOBILL.md | None | Deprecated v1.x symbols: rbDecompose, rbForm, rbInts, rbStrs, rbReconstruct, rbEntropy, RzDecompose, RzReconstructSlots, RzComputeLedger, RzMillerMadow, RzComputeCertifiedInterval |
| `SECURITY.md` | 27L (1147B) | 2024-10-12 | Vulnerability disclosure policy | Security Researchers | None | None | None |

## 4. Key Duplication Clusters

1. **Manifesto Duplication**: `README.md` and `RAZOBILL.md` are nearly identical 34KB documents containing academic arguments, obsolete function names, and 15+ hardcoded badges.
2. **Python Guide Duplication**: `lib/python/README.md`, `lib/python/Guide.md`, `docs/guides/PythonDeveloperGuide.md`, and `docs/reference/PythonApiReference.md` duplicate code snippets and explanations.
3. **DuckDB Guide Duplication**: `ext/duckdb/README.md`, `docs/guides/DuckDbExtensionGuide.md`, and `docs/reference/SqlFunctionsReference.md` present conflicting lists of SQL functions.
4. **Adapter Guides**: `ext/otel/README.md` mirrors `docs/guides/OpenTelemetryIntegration.md`; `ext/vector/README.md` mirrors `docs/guides/VectorIntegration.md`.

## 5. Gate 0 Completion Signoff

- [x] All 65 files cataloged with size, purpose, audience, and change dates.
- [x] Codebase ground truths extracted from headers, sources, and passing test suites.
- [x] Deprecated symbols and broken links identified.
- [x] Gate P0 passed. Ready for Phase 1 (Identity & STYLE.md).
