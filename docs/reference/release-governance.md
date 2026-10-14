# Release governance and provenance

Standard operating procedures for validating, signing, and distributing release artifacts.
Read this document if you are preparing or verifying an official release of Razobill.
Skip this document if you are an application user.

---

## Release checklist

Every release candidate must complete this automated verification checklist:

1. **Unit and regression tests**:
   - All 41 C++ unit test cases must pass with 0 failures (`UnitTests.exe`).
   - Over 1,080,000 assertions must pass cleanly.
   - All 18 Python tests must pass under multiple Python versions (`3.9` through `3.13`).
2. **Database extension tests**:
   - DuckDB extension integration tests must pass (`IntegrationDuckdb.exe`).
3. **Documentation quality gate**:
   - `python tools/LintDocs.py` must report zero errors across all documentation files.
4. **Binary compatibility check**:
   - Verify that `LIBRAZOBILL_ABI_VERSION` matches the intended contract.

---

## Artifact distribution and signing

Official release artifacts include:

- Multi-platform precompiled Python wheels on PyPI (`pyrazobill`).
- DuckDB loadable extensions published to the official DuckDB Community repository.
- GitHub release archives accompanied by SHA-256 checksums and GPG cryptographic signatures.
