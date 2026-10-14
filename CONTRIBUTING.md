# Contributing to Razobill

Thank you for your interest in contributing to Razobill.
Razobill is an open-source, high-performance C++20 engine and Python library for lossless log parsing and telemetry compaction.

---

## Code of conduct

This project adheres to the [Contributor Covenant](CODE_OF_CONDUCT.md).
By participating, you are expected to uphold this code.

---

## Core engineering invariants

Before submitting code, please review our core invariants:

1. **Lossless invertibility**:
   The parsing engine must guarantee exact byte-for-byte reconstruction across all valid inputs:
   $$\text{RzJoin}(\text{RzSplit}(x)) = x$$

2. **Zero heap allocation on the hot path**:
   All per-message parsing operations must route through bump-pointer memory arenas or reusable scratch buffers.
   Do not allocate heap memory (`malloc`, `new`, `std::string` reallocations) inside hot loops.

3. **Public API stability**:
   Follow the naming and type conventions specified in [Deprecation policy](docs/reference/deprecation-policy.md) and [Universal C ABI reference](docs/reference/c-abi.md).

---

## Development setup

### Prerequisites
- **C++20 compiler**: GCC 12+, Clang 15+, or MSVC 2022+
- **CMake**: Version 3.20 or newer
- **Ninja**: Fast build system (recommended)
- **Python**: Version 3.9 or newer with `pybind11` and `setuptools`

### Building the C++ core
```bash
# Configure release build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build all libraries, tests, and extensions
ninja -C build
```

### Running C++ test suites
```bash
# 1. Run core unit test suite (41 test cases, 1,082,496 assertions)
./build/UnitTests.exe

# 2. Run DuckDB extension integration suite (13 test cases, 134,890 assertions)
./build/IntegrationDuckdb.exe
```

### Building and testing Python bindings
```bash
# Run Python unit test suite
python -m unittest discover -s lib/python/tests
```

---

## Writing documentation

Documentation in Razobill follows strict identity, clarity, and truth standards defined in [`docs/STYLE.md`](docs/STYLE.md).
When submitting or updating documentation:

1. **Diataxis structure**: Place pages into one of the four quadrants:
   - `docs/tutorials/`: Learning-oriented tutorials.
   - `docs/how-to/`: Task-oriented recipes.
   - `docs/reference/`: Technical specifications.
   - `docs/explanation/`: Conceptual and architectural discussions.
2. **Formatting**:
   - Use sentence-case headings (e.g. `## Quick installation`).
   - One sentence per line for clean diffs.
   - Tag every code block with an explicit language.
   - Keep average sentence length under 20 words.
3. **Glossary compliance**:
   - Use only canonical terms defined in [`docs/glossary.md`](docs/glossary.md).
   - Avoid marketing hype words ("blazing", "revolutionary", "seamless").
4. **Automated verification**:
   - Run the documentation linter to verify compliance before opening a pull request:
     ```bash
     python tools/LintDocs.py
     ```
