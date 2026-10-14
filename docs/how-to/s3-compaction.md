# Writing compact Parquet to Amazon S3

Compact log archives into Amazon S3 using DuckDB and Parquet ZSTD columnar stripes.
Read this guide if you are designing a cold-storage or analytical log lakehouse on AWS S3.
Skip this guide if you only process logs in local memory.

---

## Problem

Long-term retention of raw text logs in Amazon S3 or Google Cloud Storage results in high storage bills:

- Uncompressed text requires substantial object storage space.
- Querying raw JSON or text via Amazon Athena requires scanning gigabytes of data per query.
- Standard compression (Gzip) compresses well but prevents column pruning during queries.

---

## Solution

Decomposing log streams with Razobill produces typed columnar structures that compress efficiently in Parquet format.
When writing to S3 via DuckDB, you can store structured templates, numbers, and words while preserving 100% bit-exact reconstructibility.

---

## Step-by-step implementation

### Step 1: Configure DuckDB with S3 credentials

In your DuckDB script, install and load `httpfs` alongside `razobill`:

```sql
INSTALL httpfs;
LOAD httpfs;

INSTALL razobill FROM community;
LOAD razobill;

-- Set AWS authentication
SET s3_region = 'us-east-1';
SET s3_access_key_id = 'YOUR_ACCESS_KEY';
SET s3_secret_access_key = 'YOUR_SECRET_KEY';
```

---

### Step 2: Decompose and write directly to Parquet on S3

Transform your incoming logs table into partitioned Parquet files:

```sql
COPY (
    SELECT
        current_timestamp AS event_time,
        parsed.template AS template,
        parsed.ints AS numbers,
        parsed.strs AS words
    FROM (
        SELECT RzSplit(msg) AS parsed FROM raw_stream_buffer
    )
) TO 's3://my-telemetry-bucket/year=2026/month=10/events.parquet'
(FORMAT PARQUET, COMPRESSION ZSTD);
```

---

### Step 3: Query S3 data with partition pruning

Query only the relevant columns directly over object storage:

```sql
SELECT
    template,
    COUNT(*) AS occurrences,
    AVG(numbers[1]) AS avg_duration_ms
FROM read_parquet('s3://my-telemetry-bucket/year=2026/month=10/*.parquet')
WHERE template LIKE '%Connection timed out%'
GROUP BY template;
```

Because Parquet stores columns independently, Athena or DuckDB downloads only the `template` and `numbers` byte ranges, cutting network transfer costs.

---

## Verification

Verify that any row in the S3 Parquet dataset can be restored to its raw string on demand:

```sql
SELECT
    RzJoin(template, numbers, words) AS original_raw_line
FROM read_parquet('s3://my-telemetry-bucket/year=2026/month=10/*.parquet')
LIMIT 5;
```
