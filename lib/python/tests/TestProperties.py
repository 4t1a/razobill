#!/usr/bin/env python3
"""
Property-Based Testing Suite for pyrazobill (Hypothesis).
=========================================================

Verifies fundamental mathematical and algorithmic invariants:
1. Universal Lossless Roundtrip for arbitrary Unicode strings:
   forall m in Sigma*, RzJoin(*RzSplit(m)) == m
   forall m in Sigma*, RzJoin(RzSplit(m)) == m
2. Universal Lossless Roundtrip for arbitrary byte sequences (including invalid UTF-8 and NUL):
   forall b in Bytes*, RzJoin(*RzSplit(b)) == b
3. Batch Invariant:
   forall L in List[str], [RzJoin(r) for r in RzSplitMany(L)] == L
4. Template Extraction Equivalence:
   forall m in Sigma*, RzTemplate(m) == RzSplit(m).template
"""

import sys
import unittest
from pathlib import Path

# Add lib/python directory to sys.path
pkg_dir = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(pkg_dir))

import pyrazobill as rz
from hypothesis import given, settings, strategies as st, example


class TestHypothesisProperties(unittest.TestCase):
    @settings(max_examples=200, deadline=None)
    @given(st.text(min_size=0, max_size=10000))
    @example("")
    @example("0")
    @example("Failed password for root from 192.168.1.1 port 22 ssh2")
    @example("Receiving block blk_-1608999687919862906 src: /10.250.19.102:54106 dest: /10.250.19.102:50010")
    @example("Special chars: \x12 \x13 \x00 \t \n \r \\ \" ' [ ] { } : =")
    @example("🚀🔥 Unicode emojis and accents: é à ö ü ñ 日本語 12345")
    def test_lossless_roundtrip_exact_mode(self, msg: str):
        """Invariant: Exact mode must reconstruct any input string byte-for-byte."""
        rec = rz.RzSplit(msg, mode="exact")
        self.assertIsNotNone(rec)
        self.assertEqual(rz.RzJoin(*rec), msg)
        self.assertEqual(rz.RzJoin(rec), msg)

    @settings(max_examples=200, deadline=None)
    @given(st.text(min_size=0, max_size=10000))
    @example("")
    @example("0")
    @example("Leading zeros 00123 000 0")
    @example("Literal braces {{foo}} {bar} {")
    def test_lossless_roundtrip_semantic_mode(self, msg: str):
        """Invariant: Semantic mode must reconstruct any input string byte-for-byte."""
        rec = rz.RzSplit(msg, mode="semantic")
        self.assertIsNotNone(rec)
        self.assertEqual(rz.RzJoin(*rec), msg)
        self.assertEqual(rz.RzJoin(rec), msg)

    @settings(max_examples=200, deadline=None)
    @given(st.binary(min_size=0, max_size=10000))
    @example(b"")
    @example(b"\x00")
    @example(b"\xff\xfe\x00\x80 invalid utf8 with 123 numbers")
    def test_lossless_roundtrip_bytes(self, b_msg: bytes):
        """Invariant: Raw bytes must reconstruct byte-for-byte without distortion."""
        rec = rz.RzSplit(b_msg)
        self.assertIsInstance(rec.template, bytes)
        self.assertEqual(rz.RzJoin(*rec), b_msg)
        self.assertEqual(rz.RzJoin(rec), b_msg)

    @settings(max_examples=100, deadline=None)
    @given(st.lists(st.text(min_size=0, max_size=500), min_size=0, max_size=50))
    def test_batch_reconstruct_invariant(self, lines: list):
        """Invariant: Batch split and sequential join must match all original lines."""
        records = rz.RzSplitMany(lines)
        self.assertEqual(len(records), len(lines))
        for original, rec in zip(lines, records):
            self.assertEqual(rz.RzJoin(rec), original)

    @settings(max_examples=100, deadline=None)
    @given(st.text(min_size=0, max_size=500))
    def test_template_equivalence(self, msg: str):
        """Invariant: RzTemplate(m) must return identical template as RzSplit(m).template."""
        tmpl = rz.RzTemplate(msg)
        rec = rz.RzSplit(msg)
        self.assertEqual(tmpl, rec.template)


if __name__ == "__main__":
    unittest.main()
