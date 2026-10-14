# Documentation navigation hub

The authoritative index for all Razobill guides, API specifications, and architectural documentation.
Read this page if you want to find the right document for your current task.
Skip this page if you already know the specific guide or reference page you need.

---

## 1. Tutorials

Learning-oriented walk-throughs designed to get you running quickly.

- [Getting started in 5 minutes](tutorials/getting-started.md): Install Razobill, parse your first log line, and verify bit-exact reconstruction in Python and DuckDB.
- [Building an ingestion pipeline](tutorials/first-pipeline.md): Read raw log files, decompose messages, and write compact columnar Parquet files with DuckDB.

---

## 2. How-to guides

Task-oriented recipes for solving specific engineering problems.

- [Compacting logs in DuckDB](how-to/duckdb.md): Load the DuckDB community extension, parse lines with `RzSplit`, and aggregate by template.
- [Parsing logs in Python](how-to/python.md): Batch processing, multithreaded parsing releasing the GIL, and error handling with `pyrazobill`.
- [Deploying OpenTelemetry processor](how-to/opentelemetry.md): Configure `razobillprocessor` inside the OTel Collector to strip raw log payloads at the edge.
- [Transforming logs with Vector](how-to/vector.md): Embed the native Rust transform into your Vector pipeline.
- [Writing compact Parquet to Amazon S3](how-to/s3-compaction.md): Compress telemetry streams into S3 object storage with ZSTD compression.
- [Embedding librazobill in C++](how-to/cpp-developer.md): CMake integration, compiler flags, and direct C++20 header usage.
- [Hardware acceleration with SIMD](how-to/neon-simd.md): Configure AVX-512, AVX2, and ARM NEON hardware acceleration.

---

## 3. Reference specifications

Information-oriented formal technical specifications and API definitions.

- [DuckDB SQL reference](reference/sql-functions.md): Exact signatures, types, and return values for `RzSplit`, `RzJoin`, `RzTemplate`, and `RzHash`.
- [Python API reference](reference/python-api.md): Specification for `pyrazobill` exports, named tuples, parameters, and exceptions.
- [Universal C ABI reference](reference/c-abi.md): Memory lifetimes, return status codes, and function declarations in `RazobillC.h`.
- [C++20 engine reference](reference/cpp-api.md): Core C++ class declarations, allocators, pools, and scanner APIs.
- [Deprecation policy](reference/deprecation-policy.md): Versioning rules, ABI stability guarantees, and the 3-phase deprecation cycle.
- [Release governance](reference/release-governance.md): Release checklist, artifact signatures, and provenance verification.
- [Controlled glossary](glossary.md): Canonical terms, definitions, and banned synonyms.

---

## 4. Explanations and architecture

Understanding-oriented articles explaining design choices, theoretical foundations, and performance trade-offs.

- [Architecture overview](explanation/architecture.md): Systems architecture, modular layout, and data-plane design.
- [Lossless invertibility](explanation/lossless-invertibility.md): The algebraic invertibility guarantee and byte reconstruction mechanics.
- [Semantic versus exact scanning](explanation/semantic-vs-exact.md): Token boundaries, slot grouping, and template cardinality trade-offs.
- [Data-oriented design](explanation/data-oriented-design.md): Structure-of-Arrays (SoA), memory arena bump allocation, and cache efficiency.
- [SIMD vectorization](explanation/simd-vectorization.md): Branchless delimiter scanning using AVX-512, AVX2, and ARM NEON hardware instructions.
- [Concurrency model](explanation/concurrency-model.md): Work-stealing thread pool and lock-free thread-local working buffers.
- [Benchmark methodology](explanation/benchmarks.md): Reproducible performance measurements, corpus details, and comparison benchmarks.

---

## 5. Troubleshooting and support

- [Troubleshooting guide](troubleshooting.md): Solutions indexed by exact compiler errors and runtime status codes.
- [Documentation style guide](STYLE.md): Style rules, component templates, and writing standards.
