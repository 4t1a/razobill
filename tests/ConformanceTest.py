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
dll_path = root_dir / "build" / "razobill.dll"
if not dll_path.exists():
    raise RuntimeError(f"Cannot find {dll_path}")

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

c_lib.RzTemplate.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
c_lib.RzTemplate.restype = ctypes.c_int

c_lib.RzFree.argtypes = [ctypes.POINTER(CRzRecord)]
c_lib.RzFree.restype = None

# Connect to DuckDB
import duckdb
con = duckdb.connect(config={"allow_unsigned_extensions": "true"})
ext_path = str(root_dir / "build" / "razobill.duckdb_extension").replace("\\", "/")
con.load_extension(ext_path)


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
    res = con.execute(f"SELECT (RzSplit('{sql_msg}')).template, (RzSplit('{sql_msg}')).ints, (RzSplit('{sql_msg}')).strs").fetchall()
    tmpl, ints, strs = res[0]
    join_res = con.execute(f"SELECT RzJoin((RzSplit('{sql_msg}')).template, (RzSplit('{sql_msg}')).ints, (RzSplit('{sql_msg}')).strs)").fetchall()
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

        # 3. DuckDB SQL
        db_tmpl, db_ints, db_strs, db_joined = run_duckdb(line)
        assert db_joined == line, f"DuckDB roundtrip mismatch line {idx}: {db_joined!r} != {line!r}"

        # Cross-language conformance assertions
        assert py_rec.template == c_tmpl == db_tmpl, (
            f"Template mismatch on line {idx}:\n"
            f"  Python: {py_rec.template}\n"
            f"  C ABI:  {c_tmpl}\n"
            f"  DuckDB: {db_tmpl}"
        )
        assert py_rec.ints == c_ints == db_ints, (
            f"Ints mismatch on line {idx}:\n"
            f"  Python: {py_rec.ints}\n"
            f"  C ABI:  {c_ints}\n"
            f"  DuckDB: {db_ints}"
        )
        assert py_rec.strs == c_strs == db_strs, (
            f"Strs mismatch on line {idx}:\n"
            f"  Python: {py_rec.strs}\n"
            f"  C ABI:  {c_strs}\n"
            f"  DuckDB: {db_strs}"
        )

    print(f"SUCCESS: All {len(lines)} lines produced 100% byte-identical results across Python, C, and DuckDB SQL!")


if __name__ == "__main__":
    main()
