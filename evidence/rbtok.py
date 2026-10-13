#!/usr/bin/env python3
"""
==============================================================================
Razobill Deterministic Lexer: Python Pure-Reference Model & Proof of Reversibility
==============================================================================

WHY THIS MODEL EXISTS:
This file is the canonical Python reference specification for Razobill's C++ SIMD
scanner (`SimdScanner.hpp`). When writing high-performance C++20 vector intrinsics
with AVX2 and BMI2 instructions, it is easy to introduce off-by-one bugs. This
educational reference implementation serves as the mathematical golden model for
lossless round-trip testing.

THE CLP (COMPRESSED LOG PARSER) DESIGN CONTRACT:
What is a dynamic variable in an enterprise log line?
Empirical analysis across billions of lines reveals a simple, universal truth:
Every variable token contains at least one digit (timestamps, port numbers, latencies,
memory addresses, IP addresses, block hashes, user IDs).

- Pure Non-Negative Integers:
  `^(0|[1-9][0-9]{0,17})$` -> Mapped directly to typed 64-bit integer slots (`\\x11`).
  Stored with zero floating-point loss or string overhead.
- Composite Variables:
  Any delimiter-bounded token with digits that is not a pure integer (e.g. `10.0.0.1`,
  `blk_-12938`, `0x7fffbeef`, `v2.4.0`) -> Mapped to string pool slots (`\\x12`).

WHY IN-BAND SENTINELS (\\x11 and \\x12)?
ASCII 0x11 (Device Control 1) and 0x12 (Device Control 2) are non-printable control
codes that never occur in legitimate log text. They provide ultra-fast single-byte
placeholders for integers and strings.

THE ESCAPE HATCH:
What if an adversarial user intentionally logs binary garbage containing `\\x11`?
Instead of corrupting or crashing, the escape hatch triggers: the entire message
is treated as a single dynamic string variable (`PS`), guaranteeing 100% safety.
"""

import re
from typing import Tuple, List

# Standard delimiters: whitespace, structural punctuation, brackets, quotes, slashes
DELIMITERS = r'\s=,;:()\[\]{}"\'<>|/'

# Token regex: sequence of non-delimiter characters
TOKEN_PATTERN = re.compile(rf"[^{DELIMITERS}]+")

# Pure integer regex: up to 18 digits (fits comfortably in signed 64-bit int without overflow)
INT_PATTERN = re.compile(r"0|[1-9][0-9]{0,17}")

# Slot sentinel characters
SLOT_INT = "\x11"  # DC1: Integer slot marker
SLOT_STR = "\x12"  # DC2: String slot marker


def split(msg: str) -> Tuple[str, List[int], List[str]]:
    """
    Decomposes a raw log message into:
    1. A structural skeleton string containing delimiter templates and slot markers
    2. A list of strongly-typed extracted integers
    3. A list of dynamic string variables

    Lossless guarantee: `join(*split(msg)) == msg` for ALL inputs.
    """
    # Defensive escape hatch: if adversarial log line already contains slot sentinels,
    # preserve it losslessly as a single string variable.
    if SLOT_INT in msg or SLOT_STR in msg:
        return SLOT_STR, [], [msg]

    skeletonParts = []
    intSlots = []
    strSlots = []
    lastOffset = 0

    for match in TOKEN_PATTERN.finditer(msg):
        token = match.group()
        if not any('0' <= c <= '9' for c in token):
            continue

        # Append literal static text prior to the variable token
        skeletonParts.append(msg[lastOffset : match.start()])
        lastOffset = match.end()

        # Classify variable token
        if INT_PATTERN.fullmatch(token):
            skeletonParts.append(SLOT_INT)
            intSlots.append(int(token))
        else:
            skeletonParts.append(SLOT_STR)
            strSlots.append(token)

    # Append remaining trailing static text
    skeletonParts.append(msg[lastOffset:])
    return "".join(skeletonParts), intSlots, strSlots


def join(template: str, intSlots: List[int], strSlots: List[str]) -> str:
    """
    Exact, bit-for-bit lossless inverse projection.
    Rebuilds the original raw log line from its template skeleton and slot vectors.
    """
    rebuiltChunks = []
    intIdx = 0
    strIdx = 0

    # Split template on slot markers while preserving delimiters
    for chunk in re.split(f"([{SLOT_INT}{SLOT_STR}])", template):
        if chunk == SLOT_INT:
            rebuiltChunks.append(str(intSlots[intIdx]))
            intIdx += 1
        elif chunk == SLOT_STR:
            rebuiltChunks.append(strSlots[strIdx])
            strIdx += 1
        else:
            rebuiltChunks.append(chunk)

    return "".join(rebuiltChunks)
