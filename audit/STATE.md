# Audit State: Razobill Surface Simplification

## Phase History
- **S0 (Baseline & Capability Check)**: COMPLETED [PROVED]
  - Tool inventory verified (CMake, Ninja, GCC, Clang, Python, Cargo, Go).
  - Clean build verified: C++ UnitTests (45/45), DuckDB Integration tests (13/13), Python tests (26/26), CTest (3/3).
  - Baseline benchmark recorded: 4.10 M msg/s parse, 1.97 M msg/s roundtrip.
  - Friction points F1–F8 confirmed with proof from code.
- **S1 (Design & Surface Rationalization)**: COMPLETED [PROVED]
  - `DECISIONS.md` created: surface budget allocations, naming conventions, `{i}` / `{s}` template format, semantic scan default (based on 4.3x template reduction on SSH corpus with 100% lossless fidelity), DuckDB collision check (0 collisions).
  - `MIGRATION.md` created: comprehensive legacy symbol map to new names or removed.
- **S2 Step 1 (C Engine & Universal C ABI)**: COMPLETED [PROVED]
  - Template format updated to `{i}` and `{s}` with `{{` / `}}` escaping in `src/Slots.cpp`.
  - Merged into `include/librazobill/RazobillC.h` with exactly 6 functions and 2 types (within &le; 6 functions, &le; 3 types budget).
  - Implemented `src/RazobillC.cpp` without any memory leakage or trailing buffer overflow.
  - Enforced surface budget & banned terms via `tools/CheckSurfaceC.py` (PASS: 6/6 functions, 2/3 types, 0 banned words).
  - Test suite `UnitTests.exe`: 41/41 test cases passed (1,082,496 assertions passed) [MEASURED].
  - Standalone fuzzer `FuzzDecomposeReconstruct.exe`: 10/10 test cases passed [MEASURED].
  - Benchmark `BenchCore.exe`: Parse speed **5.10 M msg/s** (469.5 MB/s); round-trip **2.81 M msg/s** (259.2 MB/s). Zero regression [MEASURED].
- **S2 Step 2 (Python Bindings Pass)**: COMPLETED [PROVED]
  - Re-implemented `PyRazobill.cpp`, `__init__.py`, `_fallback.py`, `Core.pyi`.
  - Exported exactly 7 public names in `__all__`: `RzSplit`, `RzJoin`, `RzTemplate`, `RzSplitMany`, `RzRecord`, `RzError`, `__version__` (within &le; 8 hard cap).
  - Implemented full support for both `str` and `bytes`, returning matching type. Empty strings/bytes round-trip losslessly.
  - GIL release enabled in `RzSplitMany` during parallel chunk scanning.
  - Actionable error format: `[razobill] What happened: ... Why: ... What to do: ... Offending input: ...`.
  - Test suite `TestPyRazobill.py`: 13/13 unit tests passed [MEASURED].
  - Hypothesis property tests `TestProperties.py`: 5/5 property tests passed over 850+ randomized messages [MEASURED].
  - Verified via `tools/CheckSurfacePython.py`: 7/8 names, 0 banned terms [PROVED].

## Current Phase: S2 Step 3 (DuckDB SQL Pass)
Target: `ext/duckdb/DuckdbExtension.cpp`
- Budget: &le; 4 functions, 0 aliases.
- Functions: `rz_split` / `RzSplit`, `rz_join` / `RzJoin`, `rz_template` / `RzTemplate`.
- NULL handling: NULL in -> NULL out.
- Empty string: valid, roundtrips losslessly.
- Arbitrary byte roundtrip: VARCHAR and BLOB signatures supported.
- Descriptions registered via DuckDB metadata API.
