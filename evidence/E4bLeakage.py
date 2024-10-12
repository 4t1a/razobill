#!/usr/bin/env python3
"""
==============================================================================
Razobill Experiment 4b: Empirical Verification of Theorem 5 (Semantic Leakage)
==============================================================================

THE SCIENTIFIC QUESTION:
Can you naively collapse log templates into coarse "semantic" buckets (e.g. merging
all log lines containing 'error' or 'failed' into an 'ERROR_EVENT' class) without
losing critical forensic signal?

Industry vendors often market "AI Semantic Clustering" that clusters log messages
by text embeddings or keywords. This experiment mathematically tests whether that
causes data leakage using Theorem 5 from *Latitude and Doubt* (M. M. Atta, 2026).

THEOREM 5: ADEQUACY & LEAKAGE:
Let $X$ be the structural form (what was said), $W$ be the declared meaning (the cluster),
and $Y$ be an operational downstream consequence (e.g. ground-truth severity Level).
The Semantic Leakage is defined as:
$$L_Y(W) = I(X; Y \\mid W) = H(X, W) + H(Y, W) - H(W) - H(X, Y, W)$$

If $L_Y(W) > 0$, the meaning map $W$ FAILS to screen off form from consequence.
The software is leaking information, producing "Silent Wrong Answers" in monitoring
dashboards where false alerts trigger or real outages are masked.
"""

import sys
import os
import glob
import re
from pathlib import Path
from collections import Counter
import numpy as np
import pandas as pd

repoRoot = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(repoRoot / "evidence"))
from rbtok import split


def entropy(counts):
    total = sum(counts)
    if total == 0:
        return 0.0
    p = np.array(list(counts), dtype=np.float64) / total
    return float(-np.sum(p * np.log2(p + 1e-18)))


def H(values):
    return entropy(Counter(values).values())


def computeLeakage(X, W, Y):
    """
    Computes conditional mutual information I(X; Y | W):
    I(X; Y | W) = H(X, W) + H(Y, W) - H(W) - H(X, Y, W)
    """
    xw = list(zip(X, W))
    yw = list(zip(Y, W))
    xyw = list(zip(X, Y, W))
    return H(xw) + H(yw) - H(W) - H(xyw)


# Common heuristic keyword classes used by naive log grouping tools
KEYWORD_RULES = [
    (
        "fail",
        r"fail|error|exception|refused|denied|invalid|abort|timeout|timed out|unable|cannot|could not|fatal",
    ),
    (
        "ok",
        r"success|accepted|started|complete|finish|done|ready|connected|opened|created|received",
    ),
]


def classifyKeyword(templateText: str) -> str:
    t = templateText.lower()
    for name, pattern in KEYWORD_RULES:
        if re.search(pattern, t):
            return name
    return "other"


def main():
    logDirs = sorted(
        glob.glob(str(repoRoot / "logs" / "*")) + glob.glob("/home/claude/loghub/*")
    )
    results = []
    print("[E4b] Auditing semantic leakage L_Y(W) across enterprise datasets...\n")

    for d in logDirs:
        ds = os.path.basename(d.rstrip("/\\"))
        f = os.path.join(d, f"{ds}_2k.log_structured.csv")
        if not os.path.exists(f):
            continue

        df = pd.read_csv(f, dtype=str, keep_default_na=False)
        if "Level" not in df or df["Level"].nunique() < 2:
            continue  # Need at least 2 distinct severity levels to measure mutual information

        # Extract deterministic structural forms
        formKeys = [
            split(m)[0].replace("\x11", "<*>").replace("\x12", "<*>")
            for m in df["Content"]
        ]
        uniqueIds = {}
        clusterIds = [uniqueIds.setdefault(fk, len(uniqueIds)) for fk in formKeys]
        idToForm = {i: fk for fk, i in uniqueIds.items()}

        X = clusterIds  # What was emitted (forms)
        Y = df["Level"].tolist()  # Downstream consequence (ground-truth severity)
        W_kw = [
            classifyKeyword(idToForm[c]) for c in clusterIds
        ]  # Naive keyword meaning
        W_single = [0] * len(X)  # Monolithic single-bucket map

        # Mutual information between form and severity
        I_XY = H(Y) - (H(list(zip(X, Y))) - H(X))

        # Measure Silent Wrong Answers:
        # If an operator searches for a template and uses transparent semantic expansion,
        # how many returned rows belong to an entirely different severity Level?
        wrongCount = 0
        totalExpanded = 0
        counts = Counter(X)
        majorityLevel = {
            c: Counter(y for x, y in zip(X, Y) if x == c).most_common(1)[0][0]
            for c in counts
        }

        for c in counts:
            cls = classifyKeyword(idToForm[c])
            expandedRows = [i for i, w in enumerate(W_kw) if w == cls]
            extraRows = [i for i in expandedRows if X[i] != c]
            totalExpanded += len(extraRows)
            wrongCount += sum(Y[i] != majorityLevel[c] for i in extraRows)

        leakIdentity = computeLeakage(X, X, Y)
        leakKeyword = computeLeakage(X, W_kw, Y)
        leakSingle = computeLeakage(X, W_single, Y)

        results.append(
            {
                "dataset": ds,
                "levels": len(set(Y)),
                "templates": len(counts),
                "I_XY": round(I_XY, 3),
                "leak_identity": round(leakIdentity, 3),
                "leak_kwclass": round(leakKeyword, 3),
                "leak_one_bucket": round(leakSingle, 3),
                "leak_kw_share": round(leakKeyword / I_XY, 3) if I_XY > 0 else np.nan,
                "wrong_level_share": (
                    round(wrongCount / totalExpanded, 3) if totalExpanded > 0 else 0.0
                ),
            }
        )

    summaryDf = pd.DataFrame(results)
    pd.set_option("display.width", 250)
    print(summaryDf.to_string(index=False))
    print("\n[E4b] EMPIRICAL PROOF OF THEOREM 5:")
    print(
        "Mean Information Leakage by Keyword Clustering:",
        round(summaryDf["leak_kw_share"].mean() * 100, 1),
        "%",
    )
    print(
        "Mean Rate of Silent Wrong Answers in Semantic Search:",
        round(summaryDf["wrong_level_share"].mean() * 100, 1),
        "%",
    )


if __name__ == "__main__":
    main()
