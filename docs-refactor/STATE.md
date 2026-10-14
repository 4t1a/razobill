# Documentation Refactor State

- Phase: P3 (Systematic Rewrite & Execution)
- Status: Gate 2 Passed / Gate 3 In Progress
- Branch: docs/identity-refactor
- Last Updated: 2026-10-14
- Target Version: 1.1.1

## Done
- Confirmed CLI execution environment, Node.js, Python, and Git toolchains.
- Completed Phase 0 Inventory and ground truth verification in `docs-refactor/INVENTORY.md`.
- Completed Phase 1 Identity in `docs/STYLE.md`, `docs-refactor/DECISIONS.md`, `tools/lint_docs.py`, SVG wordmarks, and front-door `README.md`.
- Completed Phase 2 Architecture:
  - Authored complete migration table in `docs-refactor/MAP.md`.
  - Created centralized navigation index in `docs/README.md`.
  - Created controlled vocabulary in `docs/glossary.md`.
  - Created troubleshooting guide in `docs/troubleshooting.md` keyed by verbatim error strings.
  - Initialized Diataxis directory layout (`tutorials/`, `how-to/`, `explanation/`, `reference/`).

## Open
- Batch 1: Tutorials (`docs/tutorials/getting-started.md`, `docs/tutorials/first-pipeline.md`).
- Batch 2: Explanations (`docs/explanation/architecture.md`, `lossless-invertibility.md`, `semantic-vs-exact.md`, `data-oriented-design.md`, `simd-vectorization.md`, `concurrency-model.md`, `benchmarks.md`).
- Batch 3: How-To Guides (`docs/how-to/duckdb.md`, `python.md`, `opentelemetry.md`, `vector.md`, `s3-compaction.md`, `cpp-developer.md`, `neon-simd.md`, `duckdb-community.md`, `chdb.md`).
- Batch 4: API References (`docs/reference/sql-functions.md`, `python-api.md`, `c-abi.md`, `cpp-api.md`, `deprecation-policy.md`, `release-governance.md`).
- Batch 5: Sub-package READMEs, Governance (`CONTRIBUTING.md`, `CHANGELOG.md`, `.github/`), and legacy stubs.
- Run all code snippets and record real outputs.
- Initialize `docs-refactor/REVIEW.md` for discrepancies.

## Next
- Execute Batch 1 (Tutorials).
