#!/usr/bin/env python3
"""
==============================================================================
Razobill ClickHouse chDB Storage Experiment: Columnar MergeTree Compaction
==============================================================================

WHY THIS EXPERIMENT EXISTS:
ClickHouse is the gold standard of real-time analytical log databases. If Razobill
only compacted DuckDB, critics could claim: "That's just a DuckDB quirk!"

This experiment evaluates Razobill's decomposed columnar layout inside ClickHouse
MergeTree tables using embedded chDB. We measure:
1. `raw msg String` (ClickHouse default LZ4)
2. `raw msg String CODEC(ZSTD(3))`
3. `raw msg LowCardinality(String)`
4. `compact` decomposed slots:
   - `form_id`: UInt64 (xxHash64 with Seed 0 matching `librazobill/Hash.hpp`)
   - `i0..i3`: Nullable(Int64)
   - `s0..s3`: LowCardinality(Nullable(String))
   - `oi, os`: Overflow Array(Int64), Array(String)

THE XXHASH64 ZERO-OVERHEAD INTEROP CONTRACT:
Notice line 34: `xxHash64(concat(...)) fid`.
Because Razobill's C++ core (`Hash.hpp`) implements exact Yann Collet XXH64 with Seed 0,
ClickHouse's vectorized `xxHash64()` function produces IDENTICAL 64-bit integers.
You can partition ClickHouse MergeTree clusters directly on Razobill form hashes with
zero FFI translation!
"""

import sys
import os
import shutil
from pathlib import Path
import pyarrow as pa
import pyarrow.parquet as papq

import tempfile

repoRoot = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(repoRoot / "evidence"))
from E7Lib import corpus


def main():
    try:
        from chdb import session as S
    except ImportError:
        print("[ChStore] Warning: chdb Python package not installed. Skipping ClickHouse MergeTree experiment.")
        print("          Install via: pip install chdb")
        return

    print("[ChStore] Loading multi-system corpus (134,815 events)...")
    df = corpus()

    with tempfile.TemporaryDirectory() as tmpDir:
        sessionPath = os.path.join(tmpDir, "chs").replace("\\", "/")
        shutil.rmtree(sessionPath, ignore_errors=True)
        session = S.Session(sessionPath)
        session.query("CREATE DATABASE IF NOT EXISTS d")

        # Stage corpus via Parquet
        parquetStaging = os.path.join(tmpDir, "chs_src.parquet").replace("\\", "/")
        arrowTable = pa.Table.from_pandas(df, preserve_index=False)
        papq.write_table(arrowTable, parquetStaging)

        session.query(
            "CREATE TABLE d.src (sys LowCardinality(String), level LowCardinality(String), comp LowCardinality(String), msg String) ENGINE=MergeTree ORDER BY tuple()"
        )
        session.query(f"INSERT INTO d.src SELECT sys, level, comp, msg FROM file('{parquetStaging}', Parquet)")

        rowCount = session.query("SELECT count() FROM d.src", "CSV").bytes().decode().strip()
        print(f"[ChStore] Successfully staged {rowCount} rows in ClickHouse MergeTree.\n")

        # Helper: queries ClickHouse metadata for physical disk bytes on active parts
        def measureTableBytes(tableName: str):
            result = (
                session.query(
                    f"SELECT sum(data_compressed_bytes), sum(data_uncompressed_bytes) "
                    f"FROM system.parts WHERE database='d' AND table='{tableName}' AND active",
                    "CSV",
                )
                .bytes()
                .decode()
                .strip()
                .split(",")
            )
            return int(result[0]), int(result[1])

        # Helper: creates MergeTree table variant, forces OPTIMIZE FINAL, and measures disk footprint
        def evaluateVariant(tableName: str, ddlColumns: str, selectQuery: str, extraCodec: str = ""):
            session.query(f"DROP TABLE IF EXISTS d.{tableName}")
            session.query(f"CREATE TABLE d.{tableName} ({ddlColumns}) ENGINE=MergeTree ORDER BY tuple() {extraCodec}")
            session.query(f"INSERT INTO d.{tableName} {selectQuery}")
            session.query(f"OPTIMIZE TABLE d.{tableName} FINAL")
            return measureTableBytes(tableName)

        baseCompressed, baseUncompressed = measureTableBytes("src")
        print(f"{'ClickHouse Storage Layout':<60} {'Compressed Disk':<16} {'Ratio'}")
        print("-" * 85)
        print(f"{'Raw msg String (ClickHouse default LZ4)':<60} {baseCompressed:>12,} B   1.00x")

        # 1. Raw with ZSTD(3) and LowCardinality
        for codecName, ddlSnippet in [
            ("Raw msg String CODEC(ZSTD(3))", "msg String CODEC(ZSTD(3))"),
            ("Raw msg LowCardinality(String)", "msg LowCardinality(String)"),
        ]:
            res = evaluateVariant(
                "v",
                f"sys LowCardinality(String), level LowCardinality(String), comp LowCardinality(String), {ddlSnippet}",
                "SELECT sys, level, comp, msg FROM d.src",
            )
            print(f"{codecName:<60} {res[0]:>12,} B   {res[0] / baseCompressed:.2f}x")

        # 2. Decomposed Columnar Razobill Layout
        DELIMITERS = r"""\s=,;:()\[\]{}"'<>|/"""
        TOKEN_REGEX = (f"[^{DELIMITERS}]*[0-9][^{DELIMITERS}]*").replace("\\", "\\\\").replace("'", "\\'")

        formKeySql = f"concat(replaceRegexpAll(msg, '{TOKEN_REGEX}', char(18)), char(19), arrayStringConcat(arrayMap(t -> if(match(t, '^(0|[1-9][0-9]{{0,17}})$'), 'I', 'S'), extractAll(msg, '{TOKEN_REGEX}')), ''))"
        tokensSql = f"extractAll(msg, '{TOKEN_REGEX}')"
        intsSql = f"arrayMap(t -> toInt64(t), arrayFilter(t -> match(t, '^(0|[1-9][0-9]{{0,17}})$'), {tokensSql}))"
        strsSql = f"arrayFilter(t -> not match(t, '^(0|[1-9][0-9]{{0,17}})$'), {tokensSql})"

        compactCols = (
            "sys LowCardinality(String), level LowCardinality(String), comp LowCardinality(String), form_id UInt64, "
            + ", ".join(f"i{j} Nullable(Int64)" for j in range(4))
            + ", "
            + ", ".join(f"s{j} LowCardinality(Nullable(String))" for j in range(4))
            + ", oi Array(Int64), os Array(String)"
        )

        compactSelect = (
            f"SELECT sys, level, comp, xxHash64({formKeySql}) fid, "
            + ", ".join(f"if({j + 1} <= length(i), i[{j + 1}], NULL)" for j in range(4))
            + ", "
            + ", ".join(f"if({j + 1} <= length(s), s[{j + 1}], NULL)" for j in range(4))
            + ", arraySlice(i, 5), arraySlice(s, 5) "
            + f"FROM (SELECT sys, level, comp, msg, {intsSql} AS i, {strsSql} AS s FROM d.src)"
        )

        resCompact = evaluateVariant("c", compactCols, compactSelect)
        print(
            f"{'Razobill Compact (xxHash64, Int64 Slots, LowCard Strings)':<60} {resCompact[0]:>12,} B   {resCompact[0] / baseCompressed:.2f}x"
        )

        # 3. Compact with ZSTD(3) codecs on integer slots
        zstdCols = ", ".join(
            (
                "form_id UInt64 CODEC(ZSTD(3))"
                if c.strip().startswith("form_id")
                else (
                    c.strip().replace("Nullable(Int64)", "Nullable(Int64) CODEC(ZSTD(3))")
                    if c.strip().startswith("i") and "Nullable(Int64)" in c
                    else c.strip()
                )
            )
            for c in compactCols.split(", ")
        )
        resZstd = evaluateVariant("c2", zstdCols, compactSelect)
        print(
            f"{'Razobill Compact + ZSTD(3) on Integer Slots':<60} {resZstd[0]:>12,} B   {resZstd[0] / baseCompressed:.2f}x"
        )

        # 4. Additive layout (Raw message + 64-bit form index)
        resAdd = evaluateVariant(
            "c3",
            "sys LowCardinality(String), level LowCardinality(String), comp LowCardinality(String), form_id UInt64 CODEC(ZSTD(3)), msg String CODEC(ZSTD(3))",
            f"SELECT sys, level, comp, xxHash64({formKeySql}), msg FROM d.src",
        )
        print(
            f"{'Razobill Additive (Raw msg ZSTD(3) + form_id UInt64)':<60} {resAdd[0]:>12,} B   {resAdd[0] / baseCompressed:.2f}x"
        )


if __name__ == "__main__":
    main()
