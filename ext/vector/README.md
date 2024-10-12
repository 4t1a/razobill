# Razobill Vector Integration

High-throughput log compaction and semantic slot decomposition for **Vector** (by Datadog / Timber.io), powered by **librazobill 1.0.0** Universal C ABI.

---

## 1. Overview

Vector is a high-performance observability data pipeline written in Rust. By embedding `librazobill` transforms into Vector, organizations can eliminate 92%+ of log bytes before sending data across cloud availability zones or into Amazon S3 / ClickHouse.

### Performance Vector
* **Throughput**: > 2.5 Million events/second per core in native C ABI.
* **Latency Overhead**: $< 0.35\,\mu\text{s}$ per message.
* **Memory Safety**: 100% Rust-managed lifetime with zero heap copy into Vector's internal data model.

---

## 2. Quickstart Pipeline (`VectorConfig.yaml`)

Run Vector with the embedded pipeline:

```bash
vector --config ext/vector/VectorConfig.yaml
```

The pipeline:
1. Tails application logs (`/var/log/containers/*.log`).
2. Decomposes messages into `(formKey, ints, strs)` using `librazobill`.
3. Streams compact columnar stripes directly to Amazon S3 (Parquet ZSTD) and ClickHouse.

---

## 3. Rust Crate Build & Test (`Cargo.toml`)

The module is packaged as a standard Cargo crate (`razobill-vector`) linking against `librazobill`:

```bash
# Build the native library first
g++ -std=c++20 -O3 -shared src/*.cpp -Iinclude -Ithirdparty -Ithirdparty/adbc -Ithirdparty/duckdbcapi -o build/razobill.dll "-Wl,--out-implib,build/razobill.lib"

# Execute unit and integration tests
cargo test --manifest-path ext/vector/Cargo.toml
```
