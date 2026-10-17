#!/usr/bin/env python3
"""
tools/CheckSurfaceDuckdb.py: CI surface budget and banned-term checker for DuckDB extension.
Enforces hard caps:
- SQL: at most 4 functions, 0 aliases.
- Zero banned terms in public SQL extension surface.
"""

import sys
from pathlib import Path

MAX_FUNCTIONS = 4
MAX_ALIASES = 0

BANNED_TERMS = [
    "entropy",
    "ledger",
    "latitude",
    "doubt",
    "conveyance",
    "leakage",
    "channel capacity",
    "miller-madow",
    "certified interval",
    "shannon",
    "arena",
    "slot",
    "form key",
]


def main():
    root_dir = Path(__file__).resolve().parent.parent
    ext_file = root_dir / "ext" / "duckdb" / "DuckdbExtension.cpp"

    if not ext_file.exists():
        print(f"Error: Could not find {ext_file}")
        sys.exit(1)

    content = ext_file.read_text(encoding="utf-8")

    # Inspect registered function names in DUCKDB_EXTENSION_ENTRYPOINT
    import re

    # Match FunctionSet <var>("Name")
    fn_matches = re.findall(r'FunctionSet\s+\w+\(\s*"([^"]+)"\s*\);', content)
    unique_fns = sorted(list(set(fn_matches)))

    print("DuckDB SQL Public Surface Audit:")
    print(f"  Registered Functions ({len(unique_fns)}/{MAX_FUNCTIONS}): {', '.join(unique_fns)}")

    errors = []

    if len(unique_fns) > MAX_FUNCTIONS:
        errors.append(f"Exceeded hard cap of {MAX_FUNCTIONS} functions: found {len(unique_fns)} ({unique_fns}).")

    # Check for legacy aliases like rb_join, rb_rebuild, rbForm, etc.
    legacy_aliases = [
        "rbForm",
        "rb_form",
        "rbInts",
        "rb_ints",
        "rbStrs",
        "rb_strs",
        "rbDecompose",
        "rb_decompose",
        "rbReconstruct",
        "rb_join",
        "rbRebuild",
        "rb_rebuild",
        "rbTemplate",
        "rb_template",
        "rbHash",
        "rb_hash",
        "rbEntropy",
        "rb_entropy",
        "rbMillerMadow",
        "rb_miller_madow",
    ]
    found_aliases = [alias for alias in legacy_aliases if f'"{alias}"' in content and "FunctionSet" in content]
    # Filter only those that are actually registered as functions
    actually_registered_aliases = [a for a in found_aliases if a in unique_fns]
    if len(actually_registered_aliases) > MAX_ALIASES:
        errors.append(
            f"Found {len(actually_registered_aliases)} legacy aliases: {actually_registered_aliases}. Cap is {MAX_ALIASES}."
        )

    # Check for banned terms in comments or error strings
    for term in BANNED_TERMS:
        # Check if term appears in error strings or comments outside of include/internal core references
        # Let's search lines
        for lineno, line in enumerate(content.splitlines(), start=1):
            lower_line = line.lower()
            if term in lower_line:
                # ignore internal includes and internal C++ Status::SlotUnderflow/SlotOverflow enum / core types
                if (
                    "#include" in line
                    or "slotunderflow" in lower_line
                    or "slotoverflow" in lower_line
                    or "core::slotrecord" in lower_line
                ):
                    continue
                errors.append(f"Banned term '{term}' on line {lineno}: {line.strip()}")

    if errors:
        print("FAILURES:")
        for err in errors:
            print(f"  - {err}")
        sys.exit(1)

    print("SUCCESS: DuckDB extension surface is within hard budget limits with zero banned terms.")
    sys.exit(0)


if __name__ == "__main__":
    main()
