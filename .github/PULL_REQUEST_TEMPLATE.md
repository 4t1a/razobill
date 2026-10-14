## Summary of changes

A clear and concise explanation of what this pull request changes and why.

---

## Safety and quality checklist

- [ ] **Lossless invertibility**: Any changes to parsing preserve exact roundtrip reconstruction (`RzJoin(*RzSplit(s)) == s`).
- [ ] **Zero allocation on hot path**: No heap allocations (`malloc`, `new`, `std::string` reallocations) added in per-event loops.
- [ ] **Testing**:
  - [ ] Core unit tests pass (`build/UnitTests.exe`)
  - [ ] DuckDB integration tests pass (`build/IntegrationDuckdb.exe`)
  - [ ] Python unit tests pass (`python -m unittest discover -s lib/python/tests`)
- [ ] **Documentation**:
  - [ ] Documentation updated in `docs/` according to `docs/STYLE.md`.
  - [ ] Documentation linter passes cleanly (`python tools/LintDocs.py`).
