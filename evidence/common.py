#!/usr/bin/env python3
"""
==============================================================================
Razobill Empirical Evidence: Shared Testing Infrastructure & Drain3 Baseline
==============================================================================

WHY THIS FILE EXISTS:
To scientifically prove that Razobill outperforms existing academic state-of-the-art
(Drain3, Spell, AEL), we must establish an identical, uncompromised baseline on the
canonical Loghub benchmark datasets.

WHAT IS DRAIN3?
Drain (He et al., ICWS 2017) parses logs using a fixed-depth search tree guided by
message length and leading tokens. While a major improvement over quadratic pairwise
clustering, Drain requires extensive manual regex preprocessing (the 'masking' rules
below) and suffers from stateful tree explosion, unbounded memory growth, and lossy
variable extraction.

THE CANONICAL REGEX MASKS:
Drain cannot handle raw tokens like IP addresses, hex addresses, or block IDs without
pre-masking them into static labels ('<IP>', '<HEX>', '<NUM>'). Without these masks,
Drain splits templates into thousands of spurious clusters. Razobill's SIMD scanner
needs none of this handcrafted regex baggage.
"""

import os
from pathlib import Path
from drain3 import TemplateMiner
from drain3.template_miner_config import TemplateMinerConfig
from drain3.masking import MaskingInstruction

# Standard Loghub regex masks established in academic literature
MASKS = [
    (r"\b\d{1,3}(\.\d{1,3}){3}(:\d+)?\b", "IP"),
    (r"blk_-?\d+", "BLK"),
    (r"\b0x[0-9a-fA-F]+\b", "HEX"),
    (r"\b[0-9a-fA-F]{8,}\b", "HEX"),
    (r"(?<![A-Za-z])[-+]?\d+(\.\d+)?(?![A-Za-z])", "NUM"),
]


def miner(sim: float = 0.4, depth: int = 4) -> TemplateMiner:
    """
    Constructs a Drain3 TemplateMiner configured with canonical academic defaults:
    - `drain_sim_th = 0.4`: Similarity threshold for cluster merging.
    - `drain_depth = 4`: Maximum depth of the internal prefix parse tree.
    - `profiling_enabled = False`: Disable internal telemetry to measure pure parsing time.
    """
    config = TemplateMinerConfig()
    config.profiling_enabled = False
    config.drain_sim_th = sim
    config.drain_depth = depth
    config.masking_instructions = [MaskingInstruction(pattern, name) for pattern, name in MASKS]
    return TemplateMiner(config=config)


def load(datasetName: str):
    """
    Loads a Loghub dataset into a pandas DataFrame.
    Gracefully searches:
    1. Local repository logs directory (`logs/<datasetName>/...`)
    2. Environment-configured directory via `LOGHUB_DIR`
    3. Raw log file fallback (`logs/<datasetName>/<datasetName>.log`)
    """
    import pandas as pd

    repoRoot = Path(__file__).resolve().parent.parent
    candidates = [
        repoRoot / "logs" / datasetName / f"{datasetName}_2k.log_structured.csv",
        repoRoot / "logs" / f"{datasetName}_2k.log_structured.csv",
        repoRoot / "logs" / datasetName / f"{datasetName}.log",
    ]
    if "LOGHUB_DIR" in os.environ:
        candidates.append(Path(os.environ["LOGHUB_DIR"]) / datasetName / f"{datasetName}_2k.log_structured.csv")
        candidates.append(Path(os.environ["LOGHUB_DIR"]) / datasetName / f"{datasetName}.log")

    for p in candidates:
        if p.exists():
            if str(p).endswith(".csv"):
                return pd.read_csv(str(p), dtype=str, keep_default_na=False)
            else:
                with open(p, "r", encoding="utf-8", errors="replace") as f:
                    lines = [line.rstrip("\r\n") for line in f]
                return pd.DataFrame({"Content": lines})

    raise FileNotFoundError(f"Could not locate dataset '{datasetName}'. Searched: {[str(c) for c in candidates]}")
