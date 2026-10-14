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
  - Created `include/razobill.h` with exactly 6 functions and 2 types (within &le; 6 functions, &le; 3 types budget).
  - Implemented `src/RazobillC.cpp` without any memory leakage or trailing buffer overflow.
  - Enforced surface budget & banned terms via `tools/check_surface_c.py` (PASS: 6/6 functions, 2/3 types, 0 banned words).
  - Test suite `UnitTests.exe`: 41/41 test cases passed (1,082,496 assertions passed) [MEASURED].
  - Standalone fuzzer `FuzzDecomposeReconstruct.exe`: 10/10 test cases passed [MEASURED].
  - Benchmark `BenchCore.exe`: Parse speed **5.10 M msg/s** (469.5 MB/s); round-trip **2.81 M msg/s** (259.2 MB/s). Zero regression [MEASURED].

## Current Phase: S2 Step 2 (Python Bindings Pass)
Next target: `lib/python/pyrazobill`
- Surface budget: &le; 8 public names in `__all__`
- Names: `split`, `join`, `template`, `split_many`, `Decomposed`, `Error`, `__version__`
- Accept both `str` and `bytes`, returning matching type
- GIL release during batch parsing
