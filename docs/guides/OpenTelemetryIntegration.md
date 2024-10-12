# OpenTelemetry Collector Integration Guide

This guide describes how to deploy **librazobill** inside the **OpenTelemetry (OTel) Collector** using the [`razobillprocessor`](../../ext/otel/README.md) to eliminate 92%+ of log bytes at the host emission boundary.

---

## 1. Why Compact at the Collector Edge?

In enterprise architectures, applications emit unstructured log strings to stdout or local log files. When the OpenTelemetry Collector tails these files and exports them over the network:
* **Network Costs**: Cross-AZ or Internet egress fees accumulate rapidly ($0.09 / GB).
* **Downstream Load**: Centralized indexing clusters (Elasticsearch, OpenSearch, Loki) must ingest and index massive string payloads.
* **Storage Bloat**: Cloud object stores (Amazon S3) fill with redundant message templates.

By inserting `razobillprocessor` into the Collector pipeline:
* Raw strings are decomposed into canonical forms (`formKey`), numerical vectors (`ints`), and string pools (`strs`) in **$< 0.4\,\mu\text{s}$ per record**.
* Wire payloads are compressed by **92.2% to 93.8%** before leaving the local host.
* Original messages remain **100.000% exact byte-for-byte reconstructible** on demand.

---

## 2. Processor Architecture

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                     OPENTELEMETRY COLLECTOR EDGE COMPACTION PIPELINE                   │
└────────────────────────────────────────────────────────────────────────────────────────┘
  Application Logs (Files / Stdout)
                 │
                 ▼
  ┌─────────────────────────────┐
  │ OTel Receiver: `filelog`    │
  └──────────────┬──────────────┘
                 │ Raw Message String
                 ▼
  ┌────────────────────────────────────────────────────────────────────────────┐
  │ `razobillprocessor` (In-Process Cgo Link to librazobill Universal C ABI)   │
  │ • Vectorized SIMD slot separation (< 0.4 μs/record)                        │
  │ • Body -> formKey (Compact skeleton)                                       │
  │ • Attributes -> ints (BIGINT[]), strs (VARCHAR[]), formHash (UBIGINT)      │
  └──────────────────────────────────────┬─────────────────────────────────────┘
                                         │ 93.8% Smaller Wire Payload
                                         ▼
  ┌────────────────────────────────────────────────────────────────────────────┐
  │ OTel Exporters: `otlp` (gRPC), `parquetexporter` (S3), `clickhouse`        │
  └────────────────────────────────────────────────────────────────────────────┘
```

`razobillprocessor` operates as an in-process Go processor linking `librazobill` Universal C ABI:

```yaml
processors:
  razobill:
    mode: semantic       # "semantic" (preserves IPs, UUIDs) or "exact"
    drop_raw_body: true  # Replaces Body with formKey to save 92% bandwidth
    include_hash: true   # Attaches 64-bit XXH64 hash of formKey
```

### Data Mutation Behavior:
* **`LogRecord.Body`**: Becomes the structural skeleton string (e.g. `'Accepted publickey for \x12 from \x12 port \x12\x13SSI'`).
* **`Attributes["razobill.ints"]`**: Contiguous array of 64-bit signed integers (e.g. `[54322]`).
* **`Attributes["razobill.strs"]`**: Array of variable string parameters (e.g. `["ubuntu", "192.168.1.100"]`).
* **`Attributes["razobill.formHash"]`**: Fast 64-bit integer hash for instant group-by aggregations.

---

## 3. End-to-End Pipeline Example (`otel-collector-config.yaml`)

```yaml
receivers:
  filelog:
    include: ["/var/log/containers/*.log"]
    start_at: beginning

processors:
  memory_limiter:
    check_interval: 1s
    limit_percentage: 75
    spike_limit_percentage: 20

  batch:
    send_batch_size: 10000
    timeout: 1s

  razobill:
    mode: semantic
    drop_raw_body: true
    include_hash: true

exporters:
  awss3:
    s3_bucket: "telemetry"
    s3_prefix: "year=%Y/month=%m/day=%d/"
    s3_partition: "minute"
    file_prefix: "logs"

  clickhouse:
    endpoint: "tcp://clickhouse:9000"
    database: "telemetry"
    table: "logs_compact"

service:
  pipelines:
    logs:
      receivers: [filelog]
      processors: [memory_limiter, razobill, batch]
      exporters: [awss3, clickhouse]
```

---

## 4. Building Custom Collector Binary with `ocb`

To build the collector containing `razobillprocessor`:

1. **Install OpenTelemetry Collector Builder**:
   ```bash
   go install go.opentelemetry.io/collector/cmd/builder@latest
   ```

2. **Execute Build**:
   ```bash
   builder --config=ext/otel/BuilderConfig.yaml
   ```

3. **Run the Collector**:
   ```bash
   ./dist/otelcol-razobill --config=otel-collector-config.yaml
   ```
