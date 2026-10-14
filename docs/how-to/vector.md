# Transforming logs with Vector

Embed the native Razobill Rust transform into your Vector pipeline for edge log compaction.
Read this guide if you are deploying Vector to ship logs from hosts or containers.
Skip this guide if you are using OpenTelemetry Collector or Python.

---

## Problem

Vector is a high-performance observability agent.
When forwarding raw text logs to centralized storage, high network transfer fees and storage indexing costs accumulate quickly.

---

## Solution

The `razobill-vector` crate embeds librazobill directly into Vector pipelines.
It converts unstructured log strings into `(template, ints, strs)` tuples at over 2.5 million events per second per CPU core.

---

## Pipeline configuration

Create a Vector pipeline configuration (`vector.yaml`):

```yaml
sources:
  container_logs:
    type: file
    include:
      - /var/log/containers/*.log

transforms:
  razobill_compact:
    type: remap
    inputs:
      - container_logs
    source: |
      parsed = razobill_split(.message)
      .template = parsed.template
      .ints = parsed.ints
      .strs = parsed.strs
      del(.message)

sinks:
  s3_archive:
    type: aws_s3
    inputs:
      - razobill_compact
    bucket: production-telemetry-archive
    region: us-east-1
    key_prefix: logs/year=%Y/month=%m/
    compression: zstd
    encoding:
      codec: parquet
```

---

## Building the crate

The transform links against the native `librazobill` shared library:

```bash
# 1. Build the native engine
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# 2. Run Rust crate tests
cargo test --manifest-path ext/vector/Cargo.toml
```

---

## Verification

Run Vector in test mode to verify message parsing:

```bash
vector --config vector.yaml --dry-run
```

Verify that the output records contain the compact template, 64-bit integer slice, and string slice without raw string redundancy.
