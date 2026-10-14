#!/usr/bin/env python3
"""
==============================================================================
Razobill Experiment 7: DuckDB Physical Block-Size Compaction & Storage Study
==============================================================================

THE SCIENTIFIC QUESTION:
Can decomposing raw log lines into typed integer columns and dictionary-encoded
structural forms beat modern state-of-the-art columnar compression (DuckDB FSST and
Parquet ZSTD-19)?

THE ARCHITECTURAL PROOF:
General-purpose compression algorithms (LZ4, ZSTD, Snappy) treat log messages as
opaque byte streams. They cannot exploit domain-specific structural invariants:
1. Integer variables (e.g. ports, latencies) are stored as wasteful ASCII digits
   instead of compact 64-bit BitPacked integers.
2. Form skeletons are repeated millions of times across rows.
3. String variables (IPs, hostnames) belong to compact dictionary pools.

LAYOUTS TESTED:
- `R0 raw`        : Bloated unstructured VARCHAR message column.
- `R1 additive`   : Raw string + 32-bit form_id index.
- `CU-i compact`  : Decomposed slots, inline strings, preserved arrival time order.
- `CC-i compact`  : Decomposed slots, inline strings, clustered by (form_id, seq).
- `CU-d compact`  : Decomposed slots, dictionary string pool, time order.
- `CC-d compact`  : Decomposed slots, dictionary string pool, clustered by (form_id, seq).
"""

import sys
import os
from pathlib import Path
import duckdb
import pyarrow as pa
import lzma

import tempfile

repoRoot = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(repoRoot / "evidence"))
sys.path.insert(0, str(repoRoot / "lib" / "python"))
from rbtok import split
from E7Lib import corpus


def measureDuckDbStorage(tablesDict, variantName, blockSize=262144):
    """
    Creates a physical DuckDB database file, populates tables, forces a WAL checkpoint,
    and queries pragma_database_size() to obtain exact committed block bytes.
    """
    with tempfile.TemporaryDirectory() as tmpDir:
        dbFile = os.path.join(tmpDir, f"e7_{variantName}.db")
        con = duckdb.connect()
        # Normalize path with forward slashes for DuckDB ATTACH
        cleanDbPath = dbFile.replace("\\", "/")
        con.execute(f"ATTACH '{cleanDbPath}' AS targetDb (BLOCK_SIZE {blockSize})")
        con.execute("USE targetDb")

        for tableName, (pyarrowTbl, sortOrder) in tablesDict.items():
            con.register("stagedSource", pyarrowTbl)
            orderClause = f" ORDER BY {sortOrder}" if sortOrder else ""
            con.execute(f"CREATE TABLE {tableName} AS SELECT * FROM stagedSource{orderClause}")

        con.execute("CHECKPOINT")
        usedBlocks, sizePerBlock = con.execute(
            "SELECT used_blocks, block_size FROM pragma_database_size() WHERE database_name = 'targetDb'"
        ).fetchone()

        # Query internal DuckDB column compression encodings (BitPacking, RLE, Dictionary, FSST)
        encodings = con.execute(
            "SELECT compression, count(*) FROM ("
            + " UNION ALL ".join(f"SELECT compression FROM pragma_storage_info('{t}')" for t in tablesDict)
            + ") GROUP BY 1 ORDER BY 2 DESC"
        ).fetchall()

        con.close()
        return usedBlocks * sizePerBlock, encodings


def measureParquetStorage(tablesDict, variantName, useV2=False):
    """
    Exports tables to disk as Parquet files with maximum compression (ZSTD level 19)
    to compare against industry columnar standards.
    """
    totalBytes = 0
    with tempfile.TemporaryDirectory() as tmpDir:
        con = duckdb.connect()
        for tableName, (pyarrowTbl, sortOrder) in tablesDict.items():
            con.register("stagedSource", pyarrowTbl)
            parquetFile = os.path.join(tmpDir, f"e7_{variantName}_{tableName}.parquet").replace("\\", "/")
            orderClause = f" ORDER BY {sortOrder}" if sortOrder else ""
            v2Clause = ", PARQUET_VERSION 'V2'" if useV2 else ""
            con.execute(
                f"COPY (SELECT * FROM stagedSource{orderClause}) TO '{parquetFile}' "
                f"(FORMAT PARQUET, COMPRESSION ZSTD, COMPRESSION_LEVEL 19{v2Clause})"
            )
            totalBytes += os.path.getsize(parquetFile)
        con.close()
    return totalBytes


def main():
    print("[E7] Assembling canonical multi-system evaluation corpus (134,815 events)...")
    df = corpus()
    msgs = df["msg"].tolist()
    totalRows = len(df)

    # Decompose corpus using high-speed C++20 SIMD core (fallback to reference CLP tokenizer)
    print("[E7] Decomposing messages into typed structural slots...")
    pyrazobill = None
    try:
        import pyrazobill

        HAS_PYRAZOBILL = True
    except ImportError:
        HAS_PYRAZOBILL = False

    if HAS_PYRAZOBILL:
        if hasattr(pyrazobill, "RzSplitMany"):
            recs = pyrazobill.RzSplitMany(msgs)
            parsedTuples = [(r.template, r.ints, r.strs) for r in recs]
        else:
            parsedTuples = pyrazobill.decomposeBatch(msgs)
    else:
        parsedTuples = [split(m) for m in msgs]

    # Build form dictionary
    formsDict = {}
    formIds = [formsDict.setdefault(tpl, len(formsDict)) for tpl, _, _ in parsedTuples]

    extractedInts = [iList for _, iList, _ in parsedTuples]
    extractedStrs = [sList for _, _, sList in parsedTuples]

    # Calculate 99th percentile slot counts to dimension physical columns
    def percentile99(slotLists):
        return sorted(len(x) for x in slotLists)[int(0.99 * (totalRows - 1))]

    numIntCols = percentile99(extractedInts)
    numStrCols = percentile99(extractedStrs)

    # Build string dictionary pool safely without memory spikes
    stringDict = {}
    strDictIds = []
    for sList in extractedStrs:
        rowIds = []
        for token in sList:
            tid = stringDict.get(token)
            if tid is None:
                tid = len(stringDict)
                stringDict[token] = tid
            rowIds.append(tid)
        strDictIds.append(rowIds)

    def P(data, dtype):
        return pa.array(data, dtype)

    I32, I64, STR = pa.int32(), pa.int64(), pa.string()
    LIST_I64 = pa.list_(I64)
    LIST_STR = pa.list_(STR)
    LIST_I32 = pa.list_(I32)

    userMetadata = {
        "sys": P(df["sys"], STR),
        "level": P(df["level"], STR),
        "comp": P(df["comp"], STR),
    }

    def buildCompactTable(inlineStrings: bool):
        cols = {
            "seq": P(range(totalRows), I64),
            "form_id": P(formIds, I32),
            **userMetadata,
        }
        for j in range(numIntCols):
            cols[f"i{j}"] = P([x[j] if j < len(x) else None for x in extractedInts], I64)

        stringSource = extractedStrs if inlineStrings else strDictIds
        for j in range(numStrCols):
            cols[f"s{j}"] = P(
                [x[j] if j < len(x) else None for x in stringSource],
                STR if inlineStrings else I32,
            )

        cols["oi"] = P(
            [x[numIntCols:] if len(x) > numIntCols else None for x in extractedInts],
            LIST_I64,
        )
        cols["os"] = P(
            [x[numStrCols:] if len(x) > numStrCols else None for x in stringSource],
            LIST_STR if inlineStrings else LIST_I32,
        )
        return pa.table(cols)

    tableForms = pa.table({"id": P(range(len(formsDict)), I32), "form": P(list(formsDict), STR)})
    tableStrings = pa.table({"id": P(range(len(stringDict)), I32), "s": P(list(stringDict), STR)})
    tableRaw = pa.table({**userMetadata, "msg": P(msgs, STR)})
    tableAdditive = pa.table({**userMetadata, "msg": P(msgs, STR), "form_id": P(formIds, I32)})

    variants = {
        "R0 raw": {"t": (tableRaw, None)},
        "R1 additive": {"t": (tableAdditive, None)},
        "CU-i compact (inline, time)": {
            "t": (buildCompactTable(True), None),
            "f": (tableForms, None),
        },
        "CC-i compact (inline, clustered)": {
            "t": (buildCompactTable(True), "form_id, seq"),
            "f": (tableForms, None),
        },
        "CU-d compact (dict, time)": {
            "t": (buildCompactTable(False), None),
            "f": (tableForms, None),
            "d": (tableStrings, None),
        },
        "CC-d compact (dict, clustered)": {
            "t": (buildCompactTable(False), "form_id, seq"),
            "f": (tableForms, None),
            "d": (tableStrings, None),
        },
    }

    rawPayloadBytes = len("\n".join(msgs).encode("utf-8"))
    print("\n[E7] Corpus Statistics:")
    print(f"Total Rows       : {totalRows:,}")
    print(
        f"Raw Text Payload : {rawPayloadBytes:,} bytes (XZ-Extreme: {len(lzma.compress(rawPayloadBytes.to_bytes(4, 'big'))):,} B)"
    )
    print(f"Unique Forms     : {len(formsDict):,}")
    print(f"Distinct Strings : {len(stringDict):,}\n")

    baselineNative = None
    baselineParquet = None

    print(f"{'Storage Layout':<34} {'DuckDB Native':<18} {'Parquet ZSTD19':<16} {'Top Encodings'}")
    print("-" * 90)

    for layoutName, tablesConfig in variants.items():
        nativeBytes, encodings = measureDuckDbStorage(tablesConfig, layoutName[:3])
        parquetBytes = measureParquetStorage(tablesConfig, layoutName[:3])

        if baselineNative is None:
            baselineNative = nativeBytes
            baselineParquet = parquetBytes

        nativeRatio = nativeBytes / baselineNative
        parquetRatio = parquetBytes / baselineParquet

        print(
            f"{layoutName:<34} {nativeBytes:>10,} B ({nativeRatio:.2f}x)  {parquetBytes:>10,} B ({parquetRatio:.2f}x)  {encodings[:3]}"
        )


if __name__ == "__main__":
    main()
