# Razobill OpenTelemetry Collector Processor (`razobillprocessor`)

High-performance, edge-level log compaction processor for the **OpenTelemetry (OTel) Collector**, powered by **librazobill 1.0.0** Universal C ABI.

---

## 1. Executive Overview

Deploying the OpenTelemetry Collector as a Kubernetes DaemonSet or VM sidecar allows organizations to intercept raw log streams (e.g. from container runtimes, syslog, or journald) before they are sent over the network.

Traditional OTel pipelines transmit raw unstructured strings, incurring:
- Heavy cloud network egress costs ($0.09 / GB).
- High disk utilization on downstream analytical clusters.
- Worker thread bottlenecks in high-volume logging spikes.

`razobillprocessor` uses `librazobill` to decompose each incoming `plog.LogRecord` in sub-microsecond time ($< 0.4\,\mu\text{s}$ per record):
1. **Replaces `LogRecord.Body`** with the structural canonical form key (`formKey`).
2. **Attaches `razobill.ints`** (contiguous 64-bit integer values) as a strongly typed slice attribute.
3. **Attaches `razobill.strs`** (residual string tokens) as a slice attribute.
4. **Guarantees 100.000% Lossless Invertibility**: Raw logs can be reconstructed bit-for-bit on demand via `RzReconstructSlots` or DuckDB SQL (`rbReconstruct`).

---

## 2. Architecture & Dataflow

```
Container / System Logs
        │ (stdout / stderr)
        ▼
[Receiver: filelog / journald]
        │
        ▼
[Processor: memory_limiter]
        │
        ▼
[Processor: razobillprocessor]  <--- Calls librazobill C ABI in < 0.4 μs
        │  • Extracts formKey, ints, strs
        │  • Replaces raw text payload with compact form
        ▼
[Exporter: otlp / awss3 / clickhouse]  <--- 92.2% - 93.8% Wire Egress Reduction
```

---

## 3. Configuration (`config.yaml`)

Add `razobill` to your collector configuration:

```yaml
processors:
  razobill:
    mode: semantic      # Options: semantic (recommended) or exact
    drop_raw_body: true # Replaces Body with formKey to save 92% bandwidth
    include_hash: true  # Populates razobill.hash (xxHash64) attribute

service:
  pipelines:
    logs:
      receivers: [filelog]
      processors: [memory_limiter, razobill]
      exporters: [otlp, awss3]
```

---

## 4. Building with OpenTelemetry Collector Builder (`ocb`)

Use the OpenTelemetry Collector Builder manifest (`BuilderConfig.yaml`) to build a custom binary:

```bash
ocb --config=ext/otel/BuilderConfig.yaml
```

The compiled collector binary (`otelcol-razobill`) links `librazobill.a` statically with zero runtime dependencies.

### 5. Running Module Unit Tests

To test `razobillprocessor` locally:

```bash
# Build librazobill static library
g++ -std=c++20 -O3 -c src/*.cpp -Iinclude -Ithirdparty -Ithirdparty/adbc -Ithirdparty/duckdbcapi
ar rcs build/librazobill.a *.o

# Run Go tests with Cgo enabled
cd ext/otel
go test -v
```
