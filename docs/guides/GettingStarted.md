# Getting Started with librazobill

This guide walks you through setting up the toolchain, compiling `librazobill` and `pyrazobill`, running the verification test suite, and executing benchmarks.

---

## 1. System Requirements

- **C++ Compiler**: Modern C++20 compliant compiler
  - GCC 13.0+
  - Clang 16.0+
  - MSVC 2022+ (v143 toolset)
- **Build System**: CMake 3.20+ and Ninja (recommended)
- **Python**: Python 3.9 to 3.13 (for `pyrazobill` bindings and benchmark tools)
- **Operating Systems**: Windows (x86_64), Linux (x86_64 / aarch64), macOS (Apple Silicon / Intel)

---

## 2. Building from Source

### Step 1: Configure CMake
From the root of the repository:
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

### Step 2: Build All Targets
```bash
cmake --build build
```
This compiles:
1. `build/liblibrazobill.a`: Core static C++20 library.
2. `build/librazobill.duckdb_extension`: Signed, native DuckDB loadable extension.
3. `lib/python/pyrazobill/Core.*.pyd` (or `.so`): Native pybind11 module for Python.
4. `build/UnitTests.exe`: Zero-dependency C++ unit test binary.
5. `build/IntegrationDuckdb.exe` and `IntegrationChdb.exe`: Database integration test harnesses.
6. `build/BenchCore.exe` and `BenchStorage.exe`: C++ throughput benchmarks.

---

## 3. Running the Test Suite

### C++ Test Suites (CTest)
```bash
ctest --test-dir build --output-on-failure
```
Expected output:
```
1/3 Test #1: unit ............................. Passed (34 test cases, 976,444 assertions)
2/3 Test #2: integration_duckdb ............... Passed (12 test cases, 134,890 assertions)
3/3 Test #3: integration_chdb ................. Passed (4 test cases)
100% tests passed, 0 tests failed out of 3
```

### Python Test Suite
```bash
python lib/python/tests/TestPyRazobill.py
```
Expected output:
```
..............
Ran 14 tests in 0.025s
OK
```

### Documentation & Link Verification
```bash
python tools/VerifyMarkdownLinks.py
```

---

## 4. Running Benchmarks

### 1. Multi-Core Scaling Audit Across 4,861 Log Files
Audits all 4,861 logs in `logs/` (69.6M events, 6.82 GB) utilizing all CPU hardware threads:
```bash
python tools/BenchScale.py
```

### 2. Head-to-Head Comparison vs Drain3
Compares single-threaded and multi-threaded throughput and lossless invertibility against Drain3:
```bash
python tools/BenchDrain.py
```

### 3. DuckDB 1,000,000-Row Scale Test
Evaluates native DuckDB parsing and reconstruction speed over 1,000,000 rows:
```bash
python tools/BenchDuckdb.py
```
