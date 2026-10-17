# Semantic versus exact scanning

[Docs](../README.md) > [Explanations](../README.md#4-explanations-and-architecture-understand) > Semantic versus exact scanning

Understanding template extraction heuristics, token boundaries, and cardinality reduction.
Read this article if you want to understand how Razobill groups variable tokens into templates.
Skip this article if you are satisfied with default semantic scanning.

---

## The challenge of template explosion

When parsing telemetry logs, pure delimiter tokenizers encounter combinatorial explosion:

- Log messages contain volatile tokens such as IPv4/IPv6 addresses, hex hashes, UUIDs, and absolute file paths.
- If every distinct word token is treated as part of the constant template, a system emitting 100,000 log lines can produce 20,000 distinct templates.
- This cardinality explosion destroys database partition pruning and ruins analytical group-by performance.

Conversely, aggressive heuristic parsers often over-generalize, merging distinct error conditions into identical clusters.

Razobill addresses this with two scanning modes: **semantic mode** and **exact mode**.

---

## Mode comparison

| Characteristic | Semantic Mode (Default) | Exact Mode |
| :--- | :--- | :--- |
| **Primary focus** | Production observability & database compaction | Strict token analysis & deterministic lexing |
| **Numeric parsing** | Extracted into 64-bit integer array (`{i}`) | Extracted into 64-bit integer array (`{i}`) |
| **Volatile tokens** | Grouped into variable string array (`{s}`) | Preserved literally inside the template |
| **Template cardinality** | Highly consolidated (up to 4.3x fewer templates) | Literal token structure |
| **Lossless invertibility** | 100% exact bit-for-bit | 100% exact bit-for-bit |

---

## Semantic grouping heuristics

In semantic mode, the scanner examines token character compositions:

1. **IP addresses**: Dotted-quad and colon-delimited hex strings (e.g. `192.168.1.100`, `fe80::1`) are extracted as `{s}` words.
2. **File paths and URIs**: Tokens containing slashes (e.g. `/var/log/syslog`, `https://api.internal/v1`) are extracted as `{s}` words.
3. **Hexadecimal hashes and UUIDs**: High-entropy strings containing mixed hex digits and hyphens are extracted as `{s}` words.
4. **Standard keywords and status codes**: Identifiers matching common verb lists (such as `INFO`, `ERROR`, `Failed`, `Accepted`, `timeout`) remain part of the structural template.

Because the variable values are saved in the `strs` array, no information is discarded.
`RzJoin` restores the exact path, IP, or hash at its original position during reconstruction.

---

## Empirical evidence on real-world logs

The difference in template consolidation was evaluated across standard benchmark corpora:

| Dataset | Evaluated Lines | Exact Mode Unique Templates | Semantic Mode Unique Templates | Cardinality Reduction | Lossless Invertibility |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **SSH** (`logs/SSH/SSH.log`) | 10,000 | 517 | 120 | **4.31x** | 100% verified |
| **Zookeeper** (`logs/Zookeeper/Zookeeper.log`) | 10,000 | 86 | 86 | **1.00x** | 100% verified |
| **Linux Syslog** (`logs/Linux/Linux.log`) | 10,000 | 388 | 142 | **2.73x** | 100% verified |

In high-entropy workloads (such as SSH authentication attempts with varying remote client IPs and usernames), semantic mode cuts unique templates by over 4x while maintaining zero loss of diagnostic bytes.

---

## When to choose which mode

- Use **semantic mode** (default) for:
  - Streaming logs into DuckDB, ClickHouse, or Amazon S3.
  - Telemetry compression and analytical aggregation.
  - Edge collectors (OpenTelemetry and Vector).
- Use **exact mode** for:
  - Compilers, DSLs, or configuration format validation.
  - Scenarios where IP addresses or paths are intended to form distinct cluster categories.

---

## Related

- [Lossless invertibility mechanics](lossless-invertibility.md)
- [Known limits and engineering boundaries](limits.md)
- [System comparisons: Drain3, CLP, and Razobill](comparisons.md)
