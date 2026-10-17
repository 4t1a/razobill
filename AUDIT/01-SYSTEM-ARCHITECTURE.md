# Razobill System Architecture & Engineering Principles

**Audit Subject**: Architectural Topology, Layer Separation, and Design Paradigms  
**Standard**: DuckDB / ClickHouse Systems Engineering Quality  

---

## 1. High-Level Architectural Overview

Razobill is engineered with a strict two-tier architecture:
1. **The Pure Data Plane (`core`, `channel`, `stats`)**: Completely deterministic, zero-allocation, re-entrant, and free of database dependencies. It operates exclusively on memory spans and string views.
2. **The Database Control Plane (`razobill::RazobillEngine`, `Backend`, `Dialect`)**: Orchestrates relational tables, semantic rule dictionaries, schema transformations, and analytical query generation.

```mermaid
graph TD
    subgraph DataPlane["Pure Data Plane (Zero-Allocation / Thread-Safe)"]
        Scanner["Scanner.hpp<br/>(256-byte LUT / Lexer FSM)"]
        SIMD["SimdScanner.hpp<br/>(AVX-512 / AVX2 / NEON)"]
        Slots["Slots.hpp / Slots.cpp<br/>(SlotRecord / Zero-Copy)"]
        Hash["Hash.hpp<br/>(xxHash64 Seed 0)"]
        Stats["Stats.hpp / Stats.cpp<br/>(Ledger / Divergence / Neumaier)"]
        Channel["Channel.hpp / Channel.cpp<br/>(Steganographic Channel)"]
        Arena["MemoryArena.hpp<br/>(64-byte Bump Allocator)"]
        Catalog["FormCatalog.hpp<br/>(Thread-Safe Template Cache)"]
        Pool["StringPool.hpp<br/>(Interned Dictionary)"]
    end

    subgraph ControlPlane["Analytical Database Control Plane"]
        Engine["RazobillEngine (Razobill.hpp/.cpp)<br/>(Schema / Compaction / Meaning Gate)"]
        Backend["Backend.hpp<br/>(ADBC Columnar Bridge)"]
        Dialect["Dialect.hpp / Dialect.cpp<br/>(DuckDB / ClickHouse Strategy)"]
    end

    subgraph Adapters["Surface Adapters & Foreign Function Interfaces"]
        CABI["RazobillC.h / RazobillC.cpp<br/>(Universal C ABI)"]
        DuckDBExt["DuckdbExtension.cpp<br/>(Vectorized SQL UDFs)"]
        PythonExt["PyRazobill.cpp / _fallback.py<br/>(Python Bindings & Fallback)"]
        OTelExt["RazobillProcessor.go<br/>(OpenTelemetry Processor)"]
        VectorExt["RazobillVrl.rs<br/>(Vector VRL Transform)"]
    end

    Scanner --> Slots
    SIMD --> Scanner
    Slots --> Hash
    Slots --> CABI
    Slots --> DuckDBExt
    Slots --> PythonExt
    CABI --> OTelExt
    CABI --> VectorExt
    Engine --> Backend
    Engine --> Dialect
    Engine --> Stats
    Engine --> Slots
```

---

## 2. Data-Oriented Design (DOD) Analysis

Traditional log parsers (e.g., Drain3, Spell, LenMa) represent parsed logs as object graphs: trees of nodes, dynamically allocated strings, and heap-allocated objects for each token. Under heavy workloads (tens of millions of events), this causes extreme memory fragmentation, high cache miss rates, and massive GC/allocator overhead.

Razobill strictly adheres to **Data-Oriented Design (DOD)**:

### 2.1 Structure-of-Arrays (SoA) Representation
`SlotRecord` decomposes log messages into flat contiguous arrays:
```cpp
struct SlotRecord {
    std::string templateStr;            // Normalized template format string
    std::vector<std::int64_t> ints;     // Continuous primitive 64-bit integers
    std::vector<std::string_view> strs; // Zero-copy non-owning string slices
};
```
- **Contiguity**: Integers reside in a contiguous block of memory (`std::vector<int64_t>`), allowing vectorized SIMD processing, BitPacking, or direct copy into columnar database vectors (`BIGINT[]`).
- **Zero-Copy**: String slices borrow directly from the caller's immutable input message buffer. Zero dynamic heap allocation is incurred for extracted string parameters.
- **Cache-Conscious**: Sequential memory access maximizes CPU L1/L2 data cache hit rates (hardware prefetching).

### 2.2 64-Byte Cache Line Alignment
`MemoryArena` enforces strict 64-byte alignment across all allocated memory blocks:
```cpp
#if defined(_MSC_VER) || defined(__MINGW32__)
    newBlock = _aligned_malloc(required, 64);
#else
    posix_memalign(&newBlock, 64, required);
#endif
```
- Prevents cache line crossing penalties during 256-bit (AVX2) and 512-bit (AVX-512) vector loads.
- Completely eliminates false sharing when distinct threads operate on separate arena blocks.

### 2.3 Bump Allocation with Bulk Recycling
- Allocations within `MemoryArena` are $O(1)$ pointer increments (`offset_ += padding + bytes`).
- Reset is an $O(1)$ operation that rewinds `offset_ = 0` and preserves backing OS memory pages, avoiding kernel system call overhead (`mmap` / `brk` / `VirtualAlloc`).

---

## 3. Object-Oriented & SOLID Principles Evaluation

| Principle | Implementation in Codebase | Audit Finding & Assessment |
|:---|:---|:---|
| **S** - Single Responsibility | Separate compilation units for Scanning (`Scanner`), Decomposition (`Slots`), Accounting (`Stats`), Embedding (`Channel`), Storage (`Backend`), and Schema (`RazobillEngine`). | **EXCELLENT**. Clean separation between data parsing and database interaction. |
| **O** - Open/Closed | `Dialect` base class defines a polymorphic SQL strategy interface (`DuckDialect`, `ClickHouseDialect`). New SQL engines (e.g. PostgreSQL, Trino) can be introduced by subclassing `Dialect` without altering `RazobillEngine`. | **STRONG**. Extension points are well-defined. |
| **L** - Liskov Substitution | `DuckDialect` and `ClickHouseDialect` strictly fulfill `Dialect` virtual contracts. | **COMPLIANT**. No unexpected exceptions or broken contracts. |
| **I** - Interface Segregation | Interfaces are granular: `Backend` contains only query/exec primitives (`exec`, `query`). `Dialect` exposes only dialect-specific syntax builders. | **CLEAN**. No bloated kitchen-sink interfaces. |
| **D** - Dependency Inversion | `RazobillEngine` depends on the abstract `Backend` and `Dialect` interfaces, never directly on concrete ADBC or DuckDB symbols. | **DECOUPLED**. Enables headless testing via mock backends. |

---

## 4. Analytical Database Architecture: DuckDB vs. ClickHouse

Razobill provides dual-mode deployment strategies tailored to analytical engines:

### 4.1 Compact Decomposed Layout (DuckDB)
```sql
CREATE TABLE logs_c (
    form_no  USMALLINT,  -- 16-bit dictionary ID referencing rb_form
    i0       BIGINT,     -- BitPacked integer slot 0
    i1       BIGINT,     -- BitPacked integer slot 1
    i2       BIGINT,     -- BitPacked integer slot 2
    i3       BIGINT,     -- BitPacked integer slot 3
    s0       VARCHAR,    -- Dictionary-encoded string slot 0
    s1       VARCHAR,    -- Dictionary-encoded string slot 1
    s2       VARCHAR,    -- Dictionary-encoded string slot 2
    s3       VARCHAR,    -- Dictionary-encoded string slot 3
    oi       BIGINT[],   -- Overflow integer array for anomaly logs
    os       VARCHAR[]   -- Overflow string array for anomaly logs
);
```
- **DuckDB Integration**: Exposes `logs` as a virtual view that reconstructs raw logs on-the-fly using `RzJoin` over `logs_c`.
- **Compression**: Achieves 45-60% storage reduction inside native DuckDB tables and up to 93.8% in columnar Parquet with ZSTD.

### 4.2 Additive Layout (ClickHouse / chDB)
```sql
CREATE TABLE logs (
    msg      String,     -- Preserved raw log string
    form_id  UInt64      -- Materialized 64-bit xxHash64 partition key
) ENGINE = MergeTree ORDER BY (form_id, tuple());
```
- **Zero Disruption**: Keeps raw logs intact for legacy consumers while accelerating analytical group-by and filter operations by $10\times$ via partition pruning on `form_id`.
