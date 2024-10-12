# Architecture Overview & Subsystem Isolation

`librazobill` is organized as a modular monolith in modern C++20. It decouples high-speed parsing execution (the data plane) from database interaction, metadata catalogs, and information-theoretic governance (the control plane).

---

## 1. System Topology

```
                              ┌─────────────────────────────────────────────────────────┐
                              │            Application / Foreign Language SDK           │
                              │        (Python pyrazobill, Universal C ABI, Rust)       │
                              └────────────────────────────┬────────────────────────────┘
                                                           │
                                             ┌─────────────▼─────────────┐
                                             │     RazobillEngine        │
                                             │  (High-Level Engine API)  │
                                             └──────┬─────────────┬──────┘
                                                    │             │
                    ┌───────────────────────────────┘             └───────────────────────────────┐
                    │                                                                             │
    ┌───────────────▼───────────────┐                                             ┌───────────────▼───────────────┐
    │       Data Plane (Core)       │                                             │     Control Plane (State)     │
    │  - Pure, stateless functions  │                                             │  - ADBC database abstraction  │
    │  - SIMD / Scalar Tokenizer    │                                             │  - Event log & versioning     │
    │  - DOD SlotRecord contiguity  │                                             │  - Form & doubt metadata      │
    │  - Monotonic MemoryArena      │                                             │  - Gated semantic admission   │
    │  - Zero per-row allocations   │                                             │  - Information ledger math    │
    └───────────────┬───────────────┘                                             └───────────────┬───────────────┘
                    │                                                                             │
                    │                                                                             ▼
                    │                                                             ┌───────────────────────────────┐
                    │                                                             │       Database Engines        │
                    │                                                             │   DuckDB / chDB (ClickHouse)  │
                    │                                                             └───────────────────────────────┘
                    ▼
     ┌─────────────────────────────┐
     │ DuckDB Native C Extension   │
     │  - Single-pass rbDecompose  │
     │    (composite STRUCT)       │
     │  - Scalar UDFs (rbForm,     │
     │    rbInts, rbStrs, rbRec)   │
     └─────────────────────────────┘
```

---

## 2. Compile-Time Dependency Graph

```text
┌────────────────────────────────────────────────────────────────────────┐
│                   COMPILE-TIME LAYER DEPENDENCY GRAPH                  │
└────────────────────────────────────────────────────────────────────────┘
                    ┌───────────────────────────────┐
                    │      High-Level Bindings      │
                    │  pyrazobill / DuckDB / C ABI  │
                    └───────────────┬───────────────┘
                                    │
                                    ▼
                    ┌───────────────────────────────┐
                    │     RazobillEngine Facade     │
                    │         (Control Plane)       │
                    └───────┬───────────────┬───────┘
                            │               │
            ┌───────────────┘               └───────────────┐
            ▼                                               ▼
  ┌───────────────────┐                           ┌───────────────────┐
  │  Backend/Dialect  │                           │   Stats/Channel   │
  │    (ADBC Port)    │                           │ (Info Ledger Math)│
  └─────────┬─────────┘                           └─────────┬─────────┘
            │                                               │
            └───────────────────────┬───────────────────────┘
                                    │
                                    ▼
                    ┌───────────────────────────────┐
                    │         C++20 Core            │
                    │ SIMD, Arena, Hash, DOD Slots  │
                    └───────────────────────────────┘
```

The modular architecture enforces strict unidirectional dependencies across modules:

1. **`core` (`Scanner.hpp`, `SimdScanner.hpp`, `Slots.hpp`, `MemoryArena.hpp`, `StringPool.hpp`, `Hash.hpp`, `Status.hpp`)**:
   - Depends **strictly** on the C++20 standard library.
   - Contains zero database dependencies, zero third-party framework dependencies, and zero OS-specific GUI/network headers.
   - `MemoryArena` provides 64-byte aligned monotonic allocation with chunk block freelist recycling across `reset()` calls.
   - `StringPool` provides heterogeneous string lookup and mark-and-sweep generational garbage collection.
   - Completely independent and embeddable into any environment (including bare-metal or embedded runtimes).
2. **`stats` & `channel` (`Stats.hpp`, `Channel.hpp`)**:
   - Pure mathematical implementation of Shannon entropy, mutual information, conditional latitude/doubt, stratified Miller-Madow bias correction, Proposition 10 confidence intervals, consequence decomposition, and Latitude Channel capacity profiling / payload embedding.
   - Depends only on `core` and standard mathematical algorithms (`<cmath>`, `<vector>`, `<algorithm>`, `<string_view>`).
3. **`backend` & `dialect` (`Backend.hpp`, `Dialect.hpp`)**:
   - Defines the abstract database port (`Backend`) and SQL dialect strategy (`Dialect`).
   - Does not link directly against `libduckdb` or ClickHouse shared libraries; driver loading is performed at runtime via dynamic symbol resolution (`ADBC`).
4. **`control` (`Razobill.hpp`)**:
   - The engine coordinator orchestrating table compaction, form registration, rule admission, semantic doubt computation, and ledger audits.
   - Depends on `core`, `stats`, `backend`, and `dialect`.
5. **Universal C ABI (`RazobillC.h`) & Extensions (`ext/duckdb/`, `lib/python/`)**:
   - Foreign language boundaries wrap the native core without exposing C++ types, templates, or runtime exceptions across ABI boundaries.

---

## 3. Data Plane vs. Control Plane Separation

| Dimension | Data Plane (`core`) | Control Plane (`RazobillEngine`) |
|---|---|---|
| **Purity** | Pure mathematical functions ($W = f(X)$, $X = f^{-1}(W, S)$). | Stateful database operations and catalog persistence. |
| **Concurrency** | Reentrant, embarrassingly parallel, lock-free. | Serialized per engine instance via internal mutex. |
| **Allocation** | Monotonic bump arena or thread-local buffer reuse. Zero per-row heap allocations. | Standard vectors and tables for aggregate catalog storage. |
| **Error Handling** | Strongly-typed `Status` enum (zero exceptions thrown). | Strongly-typed exceptions (`GateRefusedException`, `BackendException`). |
| **Execution Speed** | Sub-microsecond (1.5M+ events/sec per core). | Millisecond range (bounded by database I/O and SQL queries). |

---

## 4. Zero-Exception ABI Boundary Contract

No C++ exception is permitted to cross foreign language boundaries:
- In the **Universal C ABI** ([`include/librazobill/RazobillC.h`](../reference/UniversalCApiReference.md)), every API function returns `RzStatusCode`. Any internal standard library exception is caught at the boundary and mapped to `RZ_ERR_MALFORMED`, `RZ_ERR_TOO_LARGE`, or `RZ_ERR_BUFFER_TOO_SMALL`.
- In the **DuckDB Extension** ([`ext/duckdb/DuckdbExtension.cpp`](../reference/SqlFunctionsReference.md)), functions use DuckDB's C error reporting (`duckdb_scalar_function_set_error`) to gracefully surface parsing failures without crashing the database process.
- In **Python bindings** ([`lib/python/PyRazobill.cpp`](../reference/PythonApiReference.md)), pybind11 translates exceptions into idiomatic Python `ValueError` or `RuntimeError` instances.
