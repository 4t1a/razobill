# Razobill Available Engineering Resources & Tooling

**Standard:** Exhaustive Inventory of MCP Servers, Subagents, Skills, Compilers, and Reference Literature  
**Role:** Formal registry for executing the Razobill Hardening & Fix Plan  

---

## 1. Model Context Protocol (MCP) Servers

| Server Name | Loading Mode | Key Tools | Core Capability for Razobill |
|-------------|:------------:|-----------|------------------------------|
| **`serena`** | Lazy | `search_for_pattern`, `get_symbols_overview`, `find_symbol`, `find_referencing_symbols`, `find_implementations`, `find_declaration`, `get_diagnostics_for_file`, `replace_symbol_body`, `insert_after_symbol`, `insert_before_symbol` | Semantic AST code intelligence, precise symbol inspection, zero-overhead targeted C++ modifications. |
| **`context7`** | Lazy | `resolve-library-id`, `query-docs` | Up-to-date documentation queries for external libraries (Apache Arrow, DuckDB C API, pybind11, OpenTelemetry, Vector VRL). |
| **`gemini-api_gemini-api-docs`** | Eager | `gemini_search_docs`, `gemini_get_doc` | Official documentation and reference guides. |
| **`headroom`** | Lazy | `headroom_compress`, `headroom_retrieve`, `headroom_stats` | Context-window compression and state retrieval for long audit sessions. |
| **`playwright`** | Lazy | `browser_navigate`, `browser_click`, `browser_snapshot`, `browser_evaluate` | End-to-end browser automation and documentation validation. |

---

## 2. Specialized Autonomous Subagents

| Subagent | Domain Specialization | Direct Application in Razobill Fixes |
|----------|-----------------------|--------------------------------------|
| **`cpp-pro`** | C++20 Systems Programming | Memory layout, bump arenas, constexpr LUTs, AVX2/AVX-512 SIMD, strict RAII, thread pools, zero-copy spans. |
| **`python-pro`** | Python 3.12+ & C-API Bindings | Pybind11 memory safety, GIL release boundaries, pure-Python fallback parity, wheel packaging. |
| **`rust-pro`** | Rust Systems Programming | Vector Remap Language (VRL) transform, zero-allocation C-FFI bindings, slice safety. |
| **`golang-pro`** | Go Concurrency & Cgo | OpenTelemetry Collector processor, cgo memory management, error logging. |
| **`c-pro`** | C99 ABI & Interop | Stable C ABI (`RazobillC.h`), binary compatibility, memory lifetime contracts, ABI exports. |
| **`database-optimizer`** | SQL & Query Optimization | Elimination of N+1 queries, CTE rewrite of `RzSplit` in `compact()`, index structures, dialect SQL tuning. |
| **`performance-engineer`** | Hardware & Cache Optimization | Mechanical sympathy, cache-line alignment (64B), eliminating `std::regex`, ping-pong buffers, instruction profiling. |
| **`backend-architect`** | Columnar Architecture | Apache Arrow integration, ADBC driver lifecycle, transaction atomicity in control plane. |
| **`architect-review`** | Architecture & SOLID | Enforcing Single Responsibility, Open/Closed dialects, Liskov-safe capabilities, Interface Segregation. |
| **`debugger`** | Root-Cause Diagnosis | AddressSanitizer, UndefinedBehaviorSanitizer, and ThreadSanitizer analysis; dangling pointer investigation. |
| **`code-reviewer`** | Code Quality & Standards | DuckDB/ClickHouse level code review, zero AI slop, no unused macros, defensive error checking. |
| **`security-auditor`** | Vulnerability Assessment | Buffer safety in tokenizer, SQL injection guards, integer overflow protection in slot parsers. |
| **`threat-modeling-expert`** | Threat Modeling | STRIDE analysis of C ABI boundaries, untrusted log inputs, malicious format strings. |
| **`qa`** | Quality Assurance | Edge-case verification, fuzz test harnesses, regression testing. |
| **`devops-troubleshooter`** | Build & Toolchain Engineering | CMake 4.1 configuration, MSVC/GCC/Clang multi-compiler flags, CPack packaging. |

---

## 3. Specialized Skills

| Skill Category | Skill Name | Application to Razobill |
|----------------|------------|-------------------------|
| **Process & Governance** | `using-superpowers` | Enforces systematic workflow, skill checklists, and disciplined verification. |
| | `writing-plans` | Structures detailed implementation plans with clear gates before touching code. |
| | `executing-plans` | Executes plans step-by-step with continuous verification and progress tracking. |
| | `systematic-debugging` | Rigorous hypothesis-driven root cause identification before applying fixes. |
| | `test-driven-development` | Red-Green-Refactor test cycle: writing failing regression tests before applying bug fixes. |
| **Language & Systems** | `python-testing-patterns` | Property-based testing (Hypothesis), pytest fixtures, edge case testing for Python bindings. |
| | `python-type-safety` | Strict type hint enforcement and stub file (`Core.pyi`) validation. |
| | `python-packaging` | PyPI binary wheel building, auditwheel/delocate, pyproject.toml configuration. |
| | `go-concurrency-patterns` | Goroutine management and cgo memory lifecycle in OTel processor. |
| | `rust-async-patterns` | Zero-cost abstractions and safe FFI lifetimes in Vector VRL transform. |
| | `bash-defensive-patterns` | Robust CI scripts, error-trapping, and build automation. |
| **Data & Architecture** | `data-quality-frameworks` | Data validation contracts and lossless reconstruction invariants. |
| | `postgresql-table-design` | Relational table schema design and SQL syntax standards. |
| | `similarity-search-patterns` | Multi-dimensional distance metrics and entropy calculation patterns. |
| | `secrets-management` | Safe handling of credentials and API keys in CI/CD pipelines. |

---

## 4. Local Build & Runtime Environment

- **Operating System:** Windows 11 (x86_64)
- **Compilers:**
  - `gcc.exe (MSYS2) 15.1.0` (Full C++20/C++23 support, `-march=native`, `-fsanitize=address,undefined`)
  - `clang.exe 23.1.3` (LLVM 23.1.3, clang-cl, libFuzzer support)
  - Visual Studio 2022 C++ Build Tools (`vs_2022_cpp_build_tools`)
- **Build Systems:**
  - `CMake 4.1.1` (`C:\Program Files\CMake\bin\cmake.exe`)
- **Runtimes:**
  - `Python 3.13.11` (x86_64)
- **Rust Toolchain:**
  - `cargo` / `rustc` (for `ext/vector` VRL extension)
- **Go Toolchain:**
  - `go` 1.21+ (for `ext/otel` OpenTelemetry processor)

---

## 5. Foundational Academic Research & Reference Literature

1. **Information Theory & Entropy Estimation:**
   - Atta, M. M. (2026). *"Latitude and Doubt: An exact ledger of meaning in messages."* Source paper for Theorems 1, 3, 4c, 5, 7, 8 and Propositions 6, 9, 10.
   - Shannon, C. E. (1948). *"A Mathematical Theory of Communication."* Bell System Technical Journal, 27(3), 379–423.
   - Miller, G. A. (1955). *"Note on the bias of information estimates."* Information Theory in Psychology: Problems and Methods, II-B, 95–100.
   - Paninski, L. (2003). *"Estimation of entropy and mutual information."* Neural Computation, 15(6), 1191–1253.
   - Cover, T. M., & Thomas, J. A. (2006). *"Elements of Information Theory."* 2nd Edition, Wiley-Interscience.

2. **Numerical Analysis & Compensated Summation:**
   - Neumaier, A. (1974). *"Rundungsfehleranalyse einiger Verfahren zur Summation endlicher Summen."* ZAMM - Journal of Applied Mathematics and Mechanics, 54(1), 39–51.
   - Higham, N. J. (1993). *"The accuracy of floating point summation."* SIAM Journal on Scientific Computing, 14(4), 783–799.

3. **High-Performance Parsing & SIMD Data Processing:**
   - Lemire, D., & Boytsov, L. (2015). *"Decoding billions of integers per second through vectorization."* Software: Practice and Experience, 45(1), 1–29.
   - Langdale, G., & Lemire, D. (2019). *"Parsing Gigabytes of JSON per Second."* VLDB Journal, 28(6), 941–960.

4. **Vectorized & Columnar Database Architecture:**
   - Raasveldt, M., & Mühleisen, H. (2019). *"DuckDB: an Embeddable Analytical Database."* Proceedings of the 2019 ACM SIGMOD International Conference on Management of Data, 1981–1984.
   - ClickHouse Architecture: Yandex / ClickHouse Inc. *"ClickHouse: Fast Open-Source OLAP DBMS - Architecture and Vectorized Execution Engine."*
   - Apache Arrow: *"Arrow Database Connectivity (ADBC) Specification v1.1.0."* Apache Software Foundation.
