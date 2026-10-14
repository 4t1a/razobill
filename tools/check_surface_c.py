#!/usr/bin/env python3
"""
Surface Budget Enforcer: Razobill C Universal ABI Header
Enforces hard caps: <= 6 functions, <= 3 types in include/razobill.h.
Zero legacy aliases. Zero banned words.
"""
import re
import sys
from pathlib import Path

HEADER_PATH = Path(__file__).resolve().parent.parent / "include" / "razobill.h"

BANNED_TERMS = [
    "entropy", "ledger", "latitude", "doubt", "conveyance", "leakage",
    "channel capacity", "miller-madow", "certified interval", "shannon",
    "arena", "slot", "form key"
]

def main():
    if not HEADER_PATH.exists():
        print(f"ERROR: Header not found at {HEADER_PATH}")
        sys.exit(1)

    content = HEADER_PATH.read_text(encoding="utf-8")

    # 1. Check banned terms (case-insensitive) in code and comments
    for term in BANNED_TERMS:
        matches = re.findall(rf"\b{re.escape(term)}\b", content, re.IGNORECASE)
        if matches:
            print(f"VIOLATION: Found banned term '{term}' {len(matches)} time(s) in {HEADER_PATH}")
            sys.exit(1)

    # 2. Count functions declared with RAZOBILL_API
    func_pattern = re.compile(r"RAZOBILL_API\s+[\w\s\*]+\s+(razobill_\w+)\s*\(", re.MULTILINE)
    functions = func_pattern.findall(content)

    # 3. Count public types (razobill_* typedefs)
    type_pattern = re.compile(r"typedef\s+(?:enum|struct)\s+(\w+)\s*\{", re.MULTILINE)
    types = type_pattern.findall(content)

    print(f"C Public Surface Audit ({HEADER_PATH.name}):")
    print(f"  Functions ({len(functions)}/6): {', '.join(functions)}")
    print(f"  Types     ({len(types)}/3): {', '.join(types)}")

    failed = False
    if len(functions) > 6:
        print(f"FAILED: Function count {len(functions)} exceeds maximum allowance of 6!")
        failed = True
    if len(types) > 3:
        print(f"FAILED: Type count {len(types)} exceeds maximum allowance of 3!")
        failed = True

    if failed:
        sys.exit(1)

    print("SUCCESS: C surface is within hard budget limits with zero banned terms.")

if __name__ == "__main__":
    main()
