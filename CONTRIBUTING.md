# Contributing to Razobill

Thank you for your interest in contributing to **Razobill**! Razobill is an open-source, high-performance C++20 engine and Python library for lossless log parsing, analytical database integration, and information-theoretic semantic accounting.

---

## 1. Code of Conduct

This project adheres to the [Contributor Covenant](CODE_OF_CONDUCT.md). By participating, you are expected to uphold this code.

---

## 2. Architecture & Design Principles

Before submitting code, please familiarize yourself with our core design invariants:

1. **Strict Zero-Underscore Design Contract**:
   Every public function, method, struct, class, and member variable must strictly adhere to zero-underscore naming:
   - **Classes & Structs**: `PascalCase` (e.g., `RazobillEngine`, `SlotRecord`, `MemoryArena`)
   - **Functions & Methods**: `camelCase` (e.g., `parse()`, `parseBatch()`, `reconstruct()`, `formKey()`)
   - **Constants & Enums**: `PascalCase` or scoped `EnumClass::Value`
   - *No underscores anywhere in public APIs or internal identifiers unless dictated by external third-party C ABIs.*

2. **Zero-Overhead Memory Allocation**:
   All per-message allocations must route through bump-pointer memory arenas (`MemoryArena`). Do not use heap allocations (`malloc`, `new`, `std::string` allocations) inside hot loops.

3. **Lossless Invertibility**:
   The parsing engine must guarantee exact byte-for-byte reconstruction:
   $$\text{reconstruct}(\text{parse}(S)) \equiv S$$

---

## 3. Development Setup

### Prerequisites
- **C++20 Compiler**: GCC 12+, Clang 15+, or MSVC 2022+ (C++20 support required)
- **CMake**: Version 3.20 or newer
- **Ninja**: Fast build system (recommended)
- **Python**: Version 3.9 or newer with `pybind11` and `setuptools`

### Building the C++ Core
```bash
# Configure release build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build all libraries, tests, and extensions
ninja -C build
```

### Running C++ Test Suites
```bash
# 1. Run core unit test suite (34 test cases, 976,444 assertions)
./build/UnitTests.exe

# 2. Run DuckDB extension integration suite (12 test cases, 134,890 assertions)
./build/IntegrationDuckdb.exe
```

### Building & Testing Python Bindings (`pyrazobill`)
```bash
# Install in development mode
cd lib/python
pip install -e .

# Run Python verification suite (14 test cases)
python tests/TestPyRazobill.py
```

---

## 4. Submitting a Pull Request

1. **Create a Topic Branch**:
   Create a dedicated branch with a descriptive name (e.g., `feature/avx512-scanner` or `fix/whitespace-reconstruction`).
2. **Follow Code Formatting**:
   Ensure C++ code conforms to `.clang-format`:
   ```bash
   clang-format -i include/librazobill/*.hpp src/*.cpp
   ```
3. **Verify All Tests**:
   Ensure 100% of unit tests, integration tests, and benchmarks pass without regressions.
4. **License & Academic Citation**:
   All contributions are licensed under the **Apache License, Version 2.0**. Please retain the [`NOTICE`](NOTICE) file and respect the citation requirements outlined in [`LICENSE`](LICENSE).
