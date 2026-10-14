# Documentation Technical Review and Discrepancies Log

This document records all technical inaccuracies identified in legacy documentation, commands that cannot be run locally (with reasons), and evidence from code audits and test execution.

---

## 1. Discrepancies in Legacy Documentation

| File & Line | Claimed in Legacy Docs | Ground Truth (Code & Tests) | Evidence / Reason |
| :--- | :--- | :--- | :--- |
| `README.md:24-27` | Hardcoded badges: "43/43 Passed", "1,082,866 Passed" | 41 C++ test cases passed, 1,082,496 assertions passed; 18 Python tests passed. | Output of `.\build\UnitTests.exe` and `python -m unittest`. |
| `ext/duckdb/README.md:29-41` | Claims 11 SQL functions: `rbDecompose`, `rbDecomposeSemantic`, `rbForm`, `rbFormSemantic`, `rbInts`, `rbStrs`, `rbReconstruct`, `rbRebuild`, `rbTemplate`, `rbHash`, `rbEntropy`. | Extension exports exactly 4 functions: `RzSplit`, `RzJoin`, `RzTemplate`, `RzHash`. Zero aliases. | Verified in `ext/duckdb/DuckdbExtension.cpp:447-490`. |
| `docs/reference/SqlFunctionsReference.md` | Documents `rbEntropy(counts)` and `rbMillerMadow(counts)`. | Shannon entropy and Miller-Madow bias correction functions were removed from DuckDB extension. | Confirmed via `DuckdbExtension.cpp` registration set. |
| `docs/reference/UniversalCApiReference.md` | Documents `RzParse`, `RzDecompose`, `RzReconstructSlots`, `RzTemplateOf`, `RzHash64`, `RzComputeLedger`. | Header `RazobillC.h` exports exactly 6 functions: `RzSplit`, `RzJoin`, `RzTemplate`, `RzFree`, `RzStatusMessage`, `RzVersion`. | Verified in `include/librazobill/RazobillC.h`. |
| `lib/python/README.md` | Documents `decompose`, `reconstruct`, `computeLedger`, `channelEmbed`. | Python module `pyrazobill` exports `RzSplit`, `RzJoin`, `RzTemplate`, `RzSplitMany`, `RzRecord`, `RzError`. | Verified in `lib/python/pyrazobill/__init__.py:259-267`. |
| `docs/guides/OpenTelemetryIntegration.md` | Uses non-printable ASCII `\x12` and `\x13` in templates. | Templates use `{s}` for strings and `{i}` for integers. | Verified in `librazobill/Slots.hpp` and `Dialect.cpp`. |

---

## 2. Commands Requiring Remote Services (Preserved Verbatim)

| Guide | Snippet / Command | Reason Not Run Directly | Mitigation |
| :--- | :--- | :--- | :--- |
| `docs/how-to/s3-compaction.md` | `aws s3 cp s3://production-telemetry-bucket/...` | Requires live AWS account credentials and remote S3 bucket. | Preserved standard AWS CLI configuration syntax with dummy bucket names; verified local Parquet generation via DuckDB. |
| `docs/how-to/opentelemetry.md` | `docker run -v /var/log:/var/log otel/opentelemetry-collector-contrib` | Requires live Docker daemon and Linux log mounts. | Verified Go compilation of `ext/otel` and configuration schema. |
