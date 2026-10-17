#!/usr/bin/env python3
"""
tests/ConformanceTest.py: Cross-language byte-for-byte conformance verification.
Runs tricky log lines from tests/ConformanceCorpus.txt through:
1. Python (pyrazobill)
2. C ABI (razobill.dll / RzSplit, RzJoin, RzTemplate)
3. DuckDB SQL (RzSplit, RzJoin, RzTemplate)

Verifies:
- All three languages return byte-identical templates.
- All three languages return identical integers and strings.
- join(split(x)) == x byte-for-byte in all three languages.
"""

import sys
import ctypes
from pathlib import Path

root_dir = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(root_dir / "lib" / "python"))

import pyrazobill as rz

# Load C ABI
def find_c_lib():
    names = [
        "razobill.dll", "librazobill.dll",
        "librazobill.so", "razobill.so",
        "librazobill.dylib", "razobill.dylib",
    ]
    subdirs = ["build", "build/Release", "build/Debug", "bin", "."]
    for sub in subdirs:
        for name in names:
            p = root_dir / sub / name
            if p.exists():
                return p
    raise RuntimeError(f"Cannot find compiled C ABI shared library in {root_dir / 'build'}")

dll_path = find_c_lib()
c_lib = ctypes.CDLL(str(dll_path))


class CRzRecord(ctypes.Structure):
    _fields_ = [
        ("template_str", ctypes.c_char_p),
        ("template_len", ctypes.c_size_t),
        ("ints", ctypes.POINTER(ctypes.c_int64)),
        ("ints_len", ctypes.c_size_t),
        ("strs", ctypes.POINTER(ctypes.c_char_p)),
        ("str_lens", ctypes.POINTER(ctypes.c_size_t)),
        ("strs_len", ctypes.c_size_t),
        ("internal_handle", ctypes.c_void_p),
    ]


c_lib.RzSplit.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(CRzRecord)]
c_lib.RzSplit.restype = ctypes.c_int

c_lib.RzJoin.argtypes = [ctypes.POINTER(CRzRecord), ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
c_lib.RzJoin.restype = ctypes.c_int

c_lib.RzTemplate.argtypes = [
    ctypes.c_char_p,
    ctypes.c_size_t,
    ctypes.c_char_p,
    ctypes.c_size_t,
    ctypes.POINTER(ctypes.c_size_t),
]
c_lib.RzTemplate.restype = ctypes.c_int

c_lib.RzFree.argtypes = [ctypes.POINTER(CRzRecord)]
c_lib.RzFree.restype = None

# Connect to DuckDB
import duckdb

con = duckdb.connect(config={"allow_unsigned_extensions": "true"})

def find_duckdb_ext():
    names = ["razobill.duckdb_extension", "librazobill.duckdb_extension"]
    subdirs = ["build", "build/Release", "build/Debug", "bin", "."]
    for sub in subdirs:
        for name in names:
            p = root_dir / sub / name
            if p.exists():
                return p
    return None

ext_path = find_duckdb_ext()
has_duckdb_ext = False
if ext_path and ext_path.exists():
    con.load_extension(str(ext_path).replace("\\", "/"))
    has_duckdb_ext = True
else:
    print(f"[ConformanceTest] Notice: DuckDB extension not found in {root_dir / 'build'}, skipping DuckDB SQL stage.")


def run_c(msg_bytes: bytes):
    rec = CRzRecord()
    st = c_lib.RzSplit(msg_bytes, len(msg_bytes), ctypes.byref(rec))
    if st != 0:
        raise RuntimeError(f"C RzSplit failed: code {st}")

    # Extract template
    tmpl = ctypes.string_at(rec.template_str, rec.template_len).decode("utf-8")

    # Extract ints
    ints = [rec.ints[i] for i in range(rec.ints_len)]

    # Extract strs
    strs = []
    for i in range(rec.strs_len):
        s_ptr = rec.strs[i]
        s_len = rec.str_lens[i]
        strs.append(ctypes.string_at(s_ptr, s_len).decode("utf-8"))

    # Reconstruct
    buf = ctypes.create_string_buffer(len(msg_bytes) + 256)
    out_len = ctypes.c_size_t(0)
    jst = c_lib.RzJoin(ctypes.byref(rec), buf, len(buf), ctypes.byref(out_len))
    if jst != 0:
        raise RuntimeError(f"C RzJoin failed: code {jst}")
    reconstructed = ctypes.string_at(buf, out_len.value)

    c_lib.RzFree(ctypes.byref(rec))
    return tmpl, ints, strs, reconstructed


def run_duckdb(msg: str):
    # Escape single quotes for SQL
    sql_msg = msg.replace("'", "''")
    res = con.execute(
        f"SELECT (RzSplit('{sql_msg}')).template, (RzSplit('{sql_msg}')).ints, (RzSplit('{sql_msg}')).strs"
    ).fetchall()
    tmpl, ints, strs = res[0]
    join_res = con.execute(
        f"SELECT RzJoin((RzSplit('{sql_msg}')).template, (RzSplit('{sql_msg}')).ints, (RzSplit('{sql_msg}')).strs)"
    ).fetchall()
    reconstructed = join_res[0][0]
    return tmpl, list(ints), list(strs), reconstructed


def main():
    corpus_file = Path(__file__).resolve().parent / "ConformanceCorpus.txt"
    lines = corpus_file.read_text(encoding="utf-8").splitlines()

    print(f"Running Cross-Language Conformance Test on {len(lines)} lines from {corpus_file.name}...")

    for idx, line in enumerate(lines):
        line_bytes = line.encode("utf-8")

        # 1. Python
        py_rec = rz.RzSplit(line)
        py_joined = rz.RzJoin(py_rec.template, py_rec.ints, py_rec.strs)
        assert py_joined == line, f"Python roundtrip mismatch line {idx}: {py_joined!r} != {line!r}"

        # 2. C ABI
        c_tmpl, c_ints, c_strs, c_joined_bytes = run_c(line_bytes)
        assert c_joined_bytes == line_bytes, f"C roundtrip mismatch line {idx}: {c_joined_bytes!r} != {line_bytes!r}"

        # 3. DuckDB SQL (if extension loaded)
        if has_duckdb_ext:
            db_tmpl, db_ints, db_strs, db_joined = run_duckdb(line)
            assert db_joined == line, f"DuckDB roundtrip mismatch line {idx}: {db_joined!r} != {line!r}"

        # Cross-language conformance assertions
        assert py_rec.template == c_tmpl, (
            f"Template mismatch on line {idx}:\n  Python: {py_rec.template}\n  C ABI:  {c_tmpl}"
        )
        assert py_rec.ints == c_ints, (
            f"Ints mismatch on line {idx}:\n  Python: {py_rec.ints}\n  C ABI:  {c_ints}"
        )
        assert py_rec.strs == c_strs, (
            f"Strs mismatch on line {idx}:\n  Python: {py_rec.strs}\n  C ABI:  {c_strs}"
        )

        if has_duckdb_ext:
            assert c_tmpl == db_tmpl, f"Template mismatch with DuckDB on line {idx}:\n  C ABI:  {c_tmpl}\n  DuckDB: {db_tmpl}"
            assert c_ints == db_ints, f"Ints mismatch with DuckDB on line {idx}:\n  C ABI:  {c_ints}\n  DuckDB: {db_ints}"
            assert c_strs == db_strs, f"Strs mismatch with DuckDB on line {idx}:\n  C ABI:  {c_strs}\n  DuckDB: {db_strs}"

    status_suffix = "across Python, C ABI, and DuckDB SQL!" if has_duckdb_ext else "across Python and C ABI!"
    print(f"SUCCESS: All {len(lines)} lines produced 100% byte-identical results {status_suffix}")


if __name__ == "__main__":
    main()
