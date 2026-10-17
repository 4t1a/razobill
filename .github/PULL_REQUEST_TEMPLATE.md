## Summary of changes

A clear and concise explanation of what this pull request changes and why.

---

## Safety and quality checklist

- [ ] **Lossless invertibility**: All changes preserve exact round-trip reconstruction (`join(split(s)) == s`).
- [ ] **Zero allocation on hot path**: No heap allocations (`malloc`, `new`, `std::string` reallocations) added in per-event loops.
- [ ] **Testing**:
  - [ ] Core unit tests pass (`./build/UnitTests` or `.\build\UnitTests.exe`)
  - [ ] DuckDB integration tests pass (`./build/IntegrationDuckdb` or `.\build\IntegrationDuckdb.exe`)
  - [ ] Python unit tests pass (`python -m unittest discover -s lib/python/tests`)
  - [ ] Python property tests pass (`python -m pytest lib/python/tests/TestProperties.py`)
- [ ] **Documentation**:
  - [ ] Documentation updated in `docs/` adhering to Diataxis structure.
  - [ ] Markdownlint passes cleanly (`npx markdownlint-cli2 "**/*.md"`).
  - [ ] Documentation linter passes cleanly (`python tools/LintDocs.py README.md CONTRIBUTING.md docs/`).
  - [ ] Link verifier passes cleanly (`python tools/VerifyMarkdownLinks.py`).
