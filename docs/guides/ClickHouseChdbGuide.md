# ClickHouse & chDB Integration Guide

`librazobill` integrates with ClickHouse and embedded chDB through Apache Arrow ADBC (Arrow Database Connectivity), providing unified form registration, information ledger auditing, and cross-engine hash equality.

---

## 1. Cross-Engine Hash Parity (`xxh64` $\equiv$ `xxHash64`)

A core invariant of `librazobill` is cross-engine identity parity. The C++ implementation [`include/librazobill/Hash.hpp`](../../include/librazobill/Hash.hpp) produces 64-bit hashes that match ClickHouse's native `xxHash64()` function across all input lengths and algorithm branches.

### Verification in ClickHouse SQL
```sql
SELECT xxHash64('Failed password for root from 192.168.1.1 port 22');
-- Result matches razobill::core::xxh64(...) and pyrazobill.hash64(...) exactly.
```

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│               RAZOBILL & CLICKHOUSE / chDB ZERO-COPY INTEROPERABILITY                  │
└────────────────────────────────────────────────────────────────────────────────────────┘
  Input Log Event: "User 1042 authenticated"
        │
        ├───────────────────────────────────────┐
        ▼                                       ▼
  librazobill C++20 Core                  ClickHouse / chDB SQL
  xxh64(formKey)                          xxHash64(formKey)
        │                                       │
        ▼                                       ▼
  0x8f4d92a17b0c34e5                      0x8f4d92a17b0c34e5
        │                                       │
        └───────────────────┬───────────────────┘
                            ▼
              Exact 64-Bit Parity (0 Drift)
              Enables Cross-Engine Joins & Zero-Copy Arrow ADBC Streaming
```

---

## 2. Additive Mode on ClickHouse

Unlike DuckDB—where physical columnar compaction halves disk space—ClickHouse's native `ZSTD(3)` compression on raw text already compresses log messages to ~0.53× raw size. Physical compaction does not yield additional storage savings in ClickHouse MergeTree tables.

Therefore, the recommended ClickHouse deployment pattern is **Additive Mode**:
1. Keep the raw `String` message column with `CODEC(ZSTD(3))`.
2. Add a `MATERIALIZED` 64-bit form hash column.

### ClickHouse DDL
```sql
ALTER TABLE system_logs
ADD COLUMN formId UInt64 MATERIALIZED xxHash64(
    replaceRegexpAll(msg, '[^\\s=,;:()\\[\\]{}"\'<>|/]*[0-9][^\\s=,;:()\\[\\]{}"\'<>|/]*', '<*>')
);

-- Synchronously materialize existing rows
ALTER TABLE system_logs MATERIALIZE COLUMN formId SETTINGS mutations_sync = 2;
```

This adds only **+6% storage overhead** while enabling instant template filtering and group-by aggregations.

---

## 3. ADBC Driver Connection (C++)

[`src/BackendAdbc.cpp`](../../src/BackendAdbc.cpp) connects to chDB's embedded shared object:

```cpp
#include "librazobill/Backend.hpp"
#include "librazobill/Razobill.hpp"

razobill::AdbcOptions options;
options.flavor = razobill::Flavor::ClickHouse;
options.driverPath = "/path/to/_chdb.abi3.so";
options.entrypoint = "AdbcDriverInit";
options.deepbind = true; // Required on Linux to isolate C++ standard library runtimes

auto backend = razobill::makeAdbcBackend(options);
razobill::RazobillEngine engine(*backend, {.table = "system_logs", .purpose = "level"});

// Audit information ledger
auto ledger = engine.ledger();
std::cout << "Message Entropy : " << ledger.entropy << "\n";
std::cout << "Form Entropy    : " << ledger.formEntropy << "\n";
std::cout << "Residual Latitude: " << ledger.latitude << "\n";
```
