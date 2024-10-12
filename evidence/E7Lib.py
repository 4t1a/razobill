#!/usr/bin/env python3
"""
==============================================================================
Razobill Experiment 7: Multi-System Canonical Corpus Generation Pipeline
==============================================================================

WHY THIS PIPELINE EXISTS:
Evaluating a log parser on a single toy log file (e.g. 1,000 lines from Apache) is
meaningless. Real distributed infrastructure produces logs from hundreds of heterogenous
services running simultaneously (Java VMs, C++ daemons, Python microservices, OS kernels).

This module constructs the canonical multi-system benchmark corpus:
- HDFS_100k (104,815 events): High-throughput distributed filesystem logs.
- 15 diverse Loghub systems x 2,000 events: Android, Apache, BGL, Hadoop, Linux,
  Mac, OpenSSH, OpenStack, Proxifier, Spark, Thunderbird, Windows, Zookeeper.
- Total: 134,815 diverse, adversarial events spanning every major delimiter convention.
"""

import os
import glob
from pathlib import Path
import pandas as pd

import tempfile

repoRoot = Path(__file__).resolve().parent.parent


def corpus() -> pd.DataFrame:
    """
    Constructs and returns the standardized multi-system evaluation DataFrame
    with canonical columns: `sys`, `level`, `comp`, `msg`.
    """
    parquetCandidates = [
        Path("/tmp/corpus.parquet"),
        Path(tempfile.gettempdir()) / "corpus.parquet",
        repoRoot / "logs" / "corpus.parquet",
    ]
    for p in parquetCandidates:
        if p.exists():
            return pd.read_parquet(str(p))

    dataFrames = []
    searchDirs = [str(repoRoot / "logs" / "*")]
    if "LOGHUB_DIR" in os.environ:
        searchDirs.append(os.path.join(os.environ["LOGHUB_DIR"], "*"))

    logDirs = sorted(
        [d for pattern in searchDirs for d in glob.glob(pattern) if os.path.isdir(d)]
    )

    # 1. Ingest all satellite Loghub datasets
    for directory in logDirs:
        systemName = os.path.basename(directory.rstrip("/\\"))
        if systemName == "HDFS":
            continue  # Exclude 2k HDFS to avoid overlapping with full HDFS_100k

        csvPath = os.path.join(directory, f"{systemName}_2k.log_structured.csv")
        if os.path.exists(csvPath):
            df = pd.read_csv(csvPath, dtype=str, keep_default_na=False)
            dataFrames.append(
                pd.DataFrame(
                    {
                        "sys": systemName,
                        "level": df["Level"] if "Level" in df else "",
                        "comp": df["Component"] if "Component" in df else "",
                        "msg": df["Content"],
                    }
                )
            )
        else:
            # Fallback: scan for any .log file in system folder, bounded to 2,000 lines per file
            logFiles = glob.glob(os.path.join(directory, "*.log"))
            if logFiles:
                lines = []
                for lf in logFiles[:5]:  # sample up to 5 files
                    with open(lf, "r", encoding="utf-8", errors="replace") as f:
                        for idx, line in enumerate(f):
                            if idx >= 2000:
                                break
                            lines.append(line.rstrip("\r\n"))
                if lines:
                    dataFrames.append(
                        pd.DataFrame(
                            {
                                "sys": systemName,
                                "level": "",
                                "comp": "",
                                "msg": lines,
                            }
                        )
                    )

    # 2. Ingest canonical HDFS 100k dataset
    hdfsCandidates = [
        repoRoot / "logs" / "HDFS" / "HDFS_100k.log_structured.csv",
        repoRoot / "logs" / "HDFS_100k.log_structured.csv",
    ]
    if "LOGHUB_DIR" in os.environ:
        hdfsCandidates.append(
            Path(os.environ["LOGHUB_DIR"]) / "HDFS" / "HDFS_100k.log_structured.csv"
        )

    for hdfsPath in hdfsCandidates:
        if hdfsPath.exists():
            h = pd.read_csv(str(hdfsPath), dtype=str, keep_default_na=False)
            dataFrames.append(
                pd.DataFrame(
                    {
                        "sys": "HDFS100k",
                        "level": h["Level"] if "Level" in h else "",
                        "comp": h["Component"] if "Component" in h else "",
                        "msg": h["Content"],
                    }
                )
            )
            break

    if not dataFrames:
        raise FileNotFoundError(
            "Could not find Loghub datasets. Check repository logs/ directory or set LOGHUB_DIR."
        )

    return pd.concat(dataFrames, ignore_index=True)
