## Summary of Changes

A clear and concise explanation of what this pull request changes and why.

---

## Architectural & Safety Checklist

- [ ] **Zero-Underscore Convention**: All public symbols, functions, types, and files strictly follow PascalCase / camelCase.
- [ ] **Lossless Invertibility**: Any changes to parsing or tokenization preserve exact roundtrip reconstruction (`reconstruct(parse(s)) == s`).
- [ ] **Memory Safety**: No raw heap allocations (`malloc`, `new`, `std::string` allocations) added in hot per-row loops.
- [ ] **Testing**:
  - [ ] Core unit tests pass (`build/UnitTests.exe` or `ctest`)
  - [ ] DuckDB integration tests pass (`build/IntegrationDuckdb.exe`)
  - [ ] Python verification tests pass (`python lib/python/tests/TestPyRazobill.py`)
- [ ] **Documentation**: Updated public documentation in `docs/` and header comments where appropriate.
