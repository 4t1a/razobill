# AWS S3 & Cloud Object Storage Integration Guide

Razobill delivers an exact, lossless decomposition of telemetry streams into dictionary form identifiers, 64-bit integer vectors, and string pools. When coupled with modern analytical engines like DuckDB, Razobill enables streaming structured columnar data directly to **AWS S3**, **Cloudflare R2**, **Google Cloud Storage (GCS)**, or **MinIO** in Apache Parquet format.

This guide provides complete instructions for exporting, querying, and reconstructing Razobill logs in cloud object storage, integrating upstream collectors (Vector and OpenTelemetry), and testing locally for free.

---

## 1. Cloud Architecture & Economics

In legacy cloud observability architectures, raw log strings are uploaded directly to object storage or ingested into elastic search clusters:

```
[Raw Log Strings] ──► Uploaded to S3 (Unstructured Text) ──► Massive S3 Storage Bills
                                                         ──► High S3 PUT Egress Cost
                                                         ──► Multi-Terabyte Athena Scans
```

By decomposing logs at emission or ingestion using Razobill and DuckDB's vectorized streaming engine, the data pipeline transforms into an optimized columnar store:

```
                                  ┌───────────────────────────┐
                                  │      Incoming Logs        │
                                  └─────────────┬─────────────┘
                                                │
                                                ▼
                                  ┌───────────────────────────┐
                                  │   DuckDB Vector Engine    │
                                  │  • `rbForm(msg)`          │
                                  │  • `rbInts(msg)`          │
                                  │  • `rbStrs(msg)`          │
                                  └─────────────┬─────────────┘
                                                │
                                                ▼ (Direct HTTP/S3 Streaming)
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                   AWS S3 Parquet Store (`s3://telemetry-bucket/logs.parquet`)           │
├────────────────────────────────┬───────────────────────────────┬───────────────────────┤
│ Form ID (Template Dictionary)  │ Typed Int Slots (`BIGINT[]`)   │ Residual String Slots │
│ ZSTD compressed (~98% ratio)   │ Bit-packed / Dictionary       │ Snappy / ZSTD         │
└────────────────────────────────┴───────────────────────────────┴───────────────────────┘
                                                ▲
                                                │ Analytical Predicate Pushdown
                                  ┌─────────────┴─────────────┐
                                  │  AWS Athena / DuckDB SQL  │
                                  │ `WHERE ints[1] > 500`     │
                                  └───────────────────────────┘
```

### Quantified Cloud Economic Benefits (Dual-Tier Analysis)

#### 1. Common Developer / Mid-Market Workload (1 TB / Month — ~33 GB / Day)
* **S3 Standard Storage Reduction:** 1,024 GB raw ($23.55/mo) compacts down to 63.5 GB ($1.46/mo) ➔ **Saves $22.09 / month (93.8% reduction)**.
* **S3 PUT API Optimization:** Batched columnar parquet chunks slash unbatched PUT costs from $5.00/mo to $0.50/mo ➔ **Saves $4.50 / month**.
* **AWS Athena Column Pushdown:** Querying numerical variables (e.g. status code, latency) scans only the 8-byte integer slot column rather than full rows:
  * *Active Analytics (100 q/day):* Drops from $4,500/mo to $540/mo ➔ **Saves $1,320.00 / month**.
  * *Conservative Analytics (25 q/day):* Drops from $750/mo to $90/mo ➔ **Saves $660.00 / month**.
* **Total Savings (Common 1 TB Tier):** **$1,346.59 / month ($16,159.08 / year)** on active analytics, or **$290.59 / month ($3,487.08 / year)** on conservative analytics.

#### 2. Enterprise Workload (30 TB / Month — 1 TB / Day)
* **S3 Standard Storage:** Drops from $706.56/mo to $43.62/mo ➔ **Saves $662.94 / month**.
* **AWS Athena Query Scans (1,000 q/day):** Drops from $45,000/mo to $5,400/mo ➔ **Saves $39,600.00 / month**.
* **Total Savings (Enterprise 30 TB Tier):** **$40,262.94 / month ($483,155.28 / year)**.

---

## 2. DuckDB S3 Export Configuration

DuckDB features native S3 and object storage support via its official `httpfs` extension.

### Step 1: Install and Load `httpfs`

```sql
INSTALL httpfs;
LOAD httpfs;
```

### Step 2: Configure S3 Credentials

```sql
-- AWS Region
SET s3_region = 'us-east-1';

-- AWS IAM Credentials (or use IAM Instance Roles)
SET s3_access_key_id = 'AKIAIOSFODNN7EXAMPLE';
SET s3_secret_access_key = 'wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY';

-- Optional: Session Token for STS temporary credentials
-- SET s3_session_token = 'AQoDYXdzEJr1...';
```

### Step 3: Stream Decomposed Logs to S3 Parquet

Execute a single `COPY` statement to parse and stream incoming raw messages into columnar Parquet directly on S3:

```sql
COPY (
    SELECT
        d.form,
        d.ints,
        d.strs
    FROM (
        SELECT rbDecompose(msg) AS d FROM incoming_logs
    )
) TO 's3://your-telemetry-bucket/year=2026/month=10/day=07/logs.parquet'
(FORMAT PARQUET, COMPRESSION ZSTD);
```

### Hive-Style Partitioning

For high-volume production datasets, partition output keys directly within DuckDB:

```sql
COPY (
    SELECT
        service_name,
        log_date,
        rbForm(msg) AS form,
        rbInts(msg) AS ints,
        rbStrs(msg) AS strs
    FROM incoming_logs
) TO 's3://your-telemetry-bucket/lake/'
(FORMAT PARQUET, PARTITION_BY (service_name, log_date), COMPRESSION ZSTD);
```

---

## 3. Querying & Reconstructing from S3

### In-Place Columnar Analytics (Zero Full-File Egress)

You can query your Parquet files directly on S3 without downloading the entire dataset:

```sql
-- Query latency statistics directly over S3
SELECT
    form,
    COUNT(*) AS event_count,
    AVG(ints[1]) AS avg_latency_ms,
    MAX(ints[1]) AS max_latency_ms
FROM 's3://your-telemetry-bucket/lake/*/*/*.parquet'
GROUP BY form
ORDER BY event_count DESC;
```

### Exact Lossless Reconstruction over S3

Whenever an engineer investigates an incident and requires the original unformatted raw log text byte-for-byte:

```sql
-- Reconstruct the exact original string on the fly from S3
SELECT
    rbReconstruct(form, ints, strs) AS raw_log
FROM 's3://your-telemetry-bucket/lake/service_name=auth/log_date=2026-10-07/*.parquet'
WHERE ints[1] >= 500  -- HTTP 500+ errors only
LIMIT 100;
```

---

## 4. Upstream Log Aggregator Integrations (`RazobillC.h`)

Upstream telemetry shippers can parse logs at the edge or ingress point using Razobill’s universal C ABI (`include/librazobill/RazobillC.h`) before forwarding batches to S3.

### Vector (Datadog / Timber.io)

Vector can invoke Razobill through a custom Lua or VRL transform using C FFI:

```toml
# vector.toml
[sources.app_logs]
type = "file"
include = ["/var/log/app/*.log"]

[transforms.razobill_parse]
type = "lua"
inputs = ["app_logs"]
version = "2"
hooks.process = """
local ffi = require("ffi")
ffi.cdef[[
    typedef struct {
        uint64_t formKeyHash;
        const char* formKey;
        size_t formKeyLength;
        const int64_t* intValues;
        size_t intCount;
        const char* const* strValues;
        const size_t* strLengths;
        size_t strCount;
        void* internalHandle;
    } RzSlotRecord;

    int RzParse(const char* message, size_t messageLength, int mode, RzSlotRecord* outRecord);
    void RzFreeRecord(RzSlotRecord* record);
]]

local lib = ffi.load("librazobill.so")

function process(event, emit)
    local msg = event.log.message
    local rec = ffi.new("RzSlotRecord")
    local status = lib.RzParse(msg, #msg, 0, rec)

    if status == 0 then
        event.log.form = ffi.string(rec.formKey, rec.formKeyLength)
        event.log.ints = {}
        for i = 0, tonumber(rec.intCount) - 1 do
            table.insert(event.log.ints, tonumber(rec.intValues[i]))
        end
        lib.RzFreeRecord(rec)
    end
    emit(event)
end
"""

[sinks.aws_s3_parquet]
type = "aws_s3"
inputs = ["razobill_parse"]
bucket = "your-telemetry-bucket"
key_prefix = "year=%Y/month=%m/day=%d/"
compression = "gzip"
encoding.codec = "json"
```

### OpenTelemetry Collector (OTel)

For Go-based collectors, integrate `RazobillC.h` via Go CGo within a custom processor:

```go
package razobillprocessor

/*
#cgo CFLAGS: -I../../include
#cgo LDFLAGS: -L../../build -lrazobill
#include "librazobill/RazobillC.h"
*/
import "C"
import (
	"context"
	"unsafe"
	"go.opentelemetry.io/collector/pdata/plog"
)

type RazobillProcessor struct{}

func (p *RazobillProcessor) ProcessLogs(ctx context.Context, ld plog.Logs) (plog.Logs, error) {
	rls := ld.ResourceLogs()
	for i := 0; i < rls.Len(); i++ {
		sls := rls.At(i).ScopeLogs()
		for j := 0; j < sls.Len(); j++ {
			records := sls.At(j).LogRecords()
			for k := 0; k < records.Len(); k++ {
				record := records.At(k)
				msg := record.Body().AsString()

				cMsg := C.CString(msg)
				var rec C.RzSlotRecord
				status := C.RzParse(cMsg, C.size_t(len(msg)), C.RZ_SCAN_EXACT, &rec)
				C.free(unsafe.Pointer(cMsg))

				if status == C.RZ_SUCCESS {
					// Store parsed form and slot tokens as structured attributes
					attrs := record.Attributes()
					attrs.PutStr("razobill.form", C.GoStringN(rec.formKey, C.int(rec.formKeyLength)))
					C.RzFreeRecord(&rec)
				}
			}
		}
	}
	return ld, nil
}
```

Then route directly to the standard OTel `awss3exporter`.

---

## 5. Free Local Testing with LocalStack or MinIO

You do not need an active AWS account or cloud budget to verify S3 streaming.

### Option A: LocalStack with Docker (Official AWS Cloud Emulator)

Using the official LocalStack CLI (`lstk`):

```bash
# 1. Install LocalStack CLI
npm install -g @localstack/lstk

# 2. Launch LocalStack AWS Emulator
lstk start --type aws --non-interactive

# 3. Verify S3 endpoint status
lstk status
```

DuckDB configuration for LocalStack S3:

```sql
INSTALL httpfs; LOAD httpfs;

-- Point to LocalStack edge endpoint
SET s3_endpoint = 'localhost:4566';
SET s3_use_ssl = false;
SET s3_url_style = 'path';
SET s3_access_key_id = 'test';
SET s3_secret_access_key = 'test';
SET s3_region = 'us-east-1';

-- Stream Parquet export directly to LocalStack S3
COPY (
    SELECT rbForm(msg) AS form, rbInts(msg) AS ints, rbStrs(msg) AS strs
    FROM incoming_logs
) TO 's3://telemetry/logs.parquet' (FORMAT PARQUET, COMPRESSION ZSTD);
```

### Option B: MinIO with Docker

```bash
docker run -d \
  -p 9000:9000 -p 9001:9001 \
  --name minio-local \
  -e MINIO_ROOT_USER=minioadmin \
  -e MINIO_ROOT_PASSWORD=minioadmin \
  minio/minio server /data
```

In DuckDB:
```sql
SET s3_endpoint = 'localhost:9000';
SET s3_use_ssl = false;
SET s3_url_style = 'path';
SET s3_access_key_id = 'minioadmin';
SET s3_secret_access_key = 'minioadmin';
```

### Option C: Zero-Dependency Python S3 Emulator (No Docker Required)

Razobill includes a standalone, zero-dependency Python S3 mock (`tests/s3/MockS3Server.py`) that runs out of the box with zero external dependencies:

```bash
# Run automated end-to-end local S3 export & roundtrip verification
python tests/s3/TestAwsS3Export.py
```

---

## 6. Benchmarks, Stress Testing & Cost Verification

Run the reproducible benchmark suites locally to inspect real-world metrics across single-thread and multi-threaded modes:

### Storage Compaction & Query Scan Benchmark
```bash
# Single-Thread baseline
python bench/s3/BenchAwsS3Compaction.py 20000 --threads 1

# Multi-Threaded streaming (4 worker threads, 93.8% S3 reduction, dual-tier cloud savings)
python bench/s3/BenchAwsS3Compaction.py 20000 --threads 4
```

### 100,000-Event LocalStack Stress Test
```bash
# Single-Thread execution
python bench/s3/StressAwsS3LocalStack.py 100000 --threads 1

# Multi-Threaded streaming across 6 service partitions (Hive-partitioned on S3)
python bench/s3/StressAwsS3LocalStack.py 100000 --threads 4
```

### Head-to-Head Drain3 vs Razobill on S3
```bash
# Single-Thread head-to-head comparison
python bench/s3/BenchDrain3VsRazobillS3.py 20000 --threads 1

# Multi-Threaded head-to-head comparison (4 threads, 737k logs/sec, 83.5 - 171.8 QPS)
python bench/s3/BenchDrain3VsRazobillS3.py 20000 --threads 4
```

### Operational Cost Guidance: Serverless Compute Optimization
- **Thread Count Invariance on S3 Space:** S3 storage compression is **100% invariant to thread count** (~92.2% - 93.8% raw space reduction).
- **Serverless vCPU-Seconds:** Allocating multiple vCPUs to ingestion tasks (AWS Lambda, ECS Fargate, or Kubernetes daemonsets) slashes processing duration by **60% to 75%** with `pyrazobill` (up to **99%** with `librazobill` C++ core), directly reducing execution billing.

Detailed measurement reports and reproduction logs are published in [AWS S3 Compaction Report](../benchmarks/AwsS3CompactionReport.md) and [Drain3 vs Razobill on S3](../benchmarks/Drain3VsRazobillS3.md).
