# Evidence and experimental reproducibility index

[Docs](../docs/README.md) > Evidence index

This directory contains standalone Python verification scripts, benchmark harnesses, and historical run logs that validate the theoretical claims, throughput metrics, and database compaction results reported in Razobill documentation and research.

---

## Experiment scripts index

| Script | Purpose and hypothesis tested | Input dataset | Output artifact |
| --- | --- | --- | --- |
| [`E1Forms.py`](E1Forms.py) | Verifies the Vocabulary Law: finite structural dictionary of `printf` call-sites in production software. | 16 LogHub systems (2,000 events each) | Console matrix showing median 0.28 uniqueness |
| [`E4bLeakage.py`](E4bLeakage.py) | Verifies Theorem 5 (Semantic Leakage): mutual information lost by keyword-based clustering. | 7 LogHub systems with `Level` labels | [`ResultsE4bLeakage.txt`](ResultsE4bLeakage.txt) |
| [`E6Logtype.py`](E6Logtype.py) | Compares deterministic tokenization against Drain3 prefix-tree clustering. | 16 LogHub benchmark datasets (32,000 events) | [`ResultsE6Logtype.txt`](ResultsE6Logtype.txt) |
| [`E7Default.py`](E7Default.py) | Measures physical on-disk storage layout in DuckDB (256 KiB blocks) and Parquet Zstd. | Multi-system corpus (134,815 events) | [`ResultsE7Default.txt`](ResultsE7Default.txt) |
| [`E7Lib.py`](E7Lib.py) | Assembles and standardizes the 134,815-event multi-system benchmark corpus. | 16 heterogeneous LogHub log files | Normalized Arrow table |
| [`ChStore.py`](ChStore.py) | Verifies ClickHouse MergeTree storage footprint and xxHash64 bit-parity with chDB. | 134,815-event corpus in embedded chDB | Console storage breakdown |
| [`rbtok.py`](rbtok.py) | Pure-Python reference model of deterministic slot tokenization and lossless reconstruction. | Synthetic and real log streams | Golden reference assertions |

---

## Result logs index

| Result log | Generating script | Key verified finding |
| --- | --- | --- |
| [`ResultsE4bLeakage.txt`](ResultsE4bLeakage.txt) | `E4bLeakage.py` | 69.1% average mutual information lost by keyword clustering; 45.0% wrong-severity alert rate. |
| [`ResultsE6Logtype.txt`](ResultsE6Logtype.txt) | `E6Logtype.py` | Median grouping accuracy 0.808 with zero entropy doubt ($\Delta = 0.000$); top 50 templates cover 97.5% volume. |
| [`ResultsE7Default.txt`](ResultsE7Default.txt) | `E7Default.py` | Clustered dictionary layout (`CC-d`) reduces DuckDB disk usage from 6.29 MB to 2.88 MB (0.46×). |

---

## Reproducing experimental measurements

### Prerequisites

```bash
pip install pandas numpy pyarrow duckdb drain3 chdb
```

### Execution commands

```bash
# 1. Structural template cardinality across 16 systems
python evidence/E1Forms.py

# 2. Semantic leakage and alert misclassification audit
python evidence/E4bLeakage.py

# 3. Stateless lexer vs stateful Drain3 parse trees
python evidence/E6Logtype.py

# 4. DuckDB physical on-disk compaction benchmarks
python evidence/E7Default.py

# 5. ClickHouse MergeTree storage and xxHash64 parity
python evidence/ChStore.py
```

---

## Related documentation

- [Benchmark methodology and empirical results](../docs/explanation/benchmarks.md)
- [System comparisons: Drain3, CLP, and Razobill](../docs/explanation/comparisons.md)
- [Known limits and engineering boundaries](../docs/explanation/limits.md)
