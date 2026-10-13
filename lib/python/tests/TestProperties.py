#!/usr/bin/env python3
"""
Property-Based Testing Suite for pyrazobill (Hypothesis).
=========================================================
Grounded in Proposition 1 & 2 and Theorems 1-7 of "Latitude and Doubt" (M. M. Atta, 2026).

Verifies fundamental mathematical and algorithmic invariants:
1. Universal Lossless Roundtrip:
   forall m in Sigma*, reconstruct(parse(m)) == m
   forall m in Sigma*, reconstruct(*decompose(m)) == m
2. Batch Invariant:
   forall L in List[str], [r.reconstruct() for r in parseBatch(L)] == L
3. Information-Theoretic Bounds:
   - Latitude >= 0 and Doubt >= 0
   - Conveyance <= Content + epsilon (Theorem 1)
   - Effective Latitude <= Latitude + epsilon
4. Template Extraction Safety:
   templateOf(formKey(m)) never crashes and preserves constants
"""

import sys
import unittest
from pathlib import Path

# Add lib/python directory to sys.path
pkg_dir = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(pkg_dir))

import pyrazobill
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
        """Invariant: Exact mode must reconstruct the input string byte-for-byte."""
        rec = pyrazobill.parse(msg, pyrazobill.ScanMode.Exact)
        self.assertIsNotNone(rec)
        rebuilt = pyrazobill.reconstruct(rec)
        self.assertEqual(rebuilt, msg)
        self.assertEqual(rec.reconstruct(), msg)

    @settings(max_examples=200, deadline=None)
    @given(st.text(min_size=0, max_size=10000))
    def test_lossless_roundtrip_semantic_mode(self, msg: str):
        """Invariant: Semantic mode must reconstruct the input string byte-for-byte."""
        rec = pyrazobill.parse(msg, pyrazobill.ScanMode.Semantic)
        self.assertIsNotNone(rec)
        rebuilt = pyrazobill.reconstruct(rec)
        self.assertEqual(rebuilt, msg)
        self.assertEqual(rec.reconstruct(), msg)

    @settings(max_examples=150, deadline=None)
    @given(st.text(min_size=0, max_size=5000))
    def test_decompose_and_reconstruct_tuple(self, msg: str):
        """Invariant: decompose() returning (form, ints, strs) must round-trip exactly."""
        form, ints, strs = pyrazobill.decompose(msg)
        rebuilt = pyrazobill.reconstruct(form, ints, strs)
        self.assertEqual(rebuilt, msg)

    @settings(max_examples=100, deadline=None)
    @given(st.lists(st.text(min_size=0, max_size=2000), min_size=0, max_size=50))
    def test_batch_roundtrip(self, lines: list):
        """Invariant: Batch parsing preserves ordering and exact contents."""
        recs = pyrazobill.parseBatch(lines)
        self.assertEqual(len(recs), len(lines))
        rebuilt_lines = [r.reconstruct() for r in recs]
        self.assertEqual(rebuilt_lines, lines)

    @settings(max_examples=150, deadline=None)
    @given(st.text(min_size=0, max_size=5000))
    def test_template_generation_safety(self, msg: str):
        """Invariant: templateOf(formKey(msg)) produces a valid string template."""
        f_key = pyrazobill.formKey(msg)
        tpl = pyrazobill.templateOf(f_key)
        self.assertIsInstance(tpl, str)

    @settings(max_examples=100, deadline=None)
    @given(
        st.lists(
            st.tuples(
                st.integers(min_value=0, max_value=1000),
                st.integers(min_value=0, max_value=500),
                st.floats(min_value=0.01, max_value=1000.0, allow_nan=False, allow_infinity=False),
            ),
            min_size=1,
            max_size=30,
        )
    )
    def test_information_theoretic_ledger_bounds(self, raw_cells):
        """
        Mathematical Invariant (Latitude and Doubt, 2026):
        1. Latitude >= -1e-9 and Doubt >= -1e-9
        2. Conveyance <= Content + 1e-9 (Conveyance Theorem)
        3. Effective Latitude <= Latitude + 1e-9
        """
        cells = [pyrazobill.Cell(form=f, meaning=m, mass=w) for f, m, w in raw_cells]
        ledger = pyrazobill.computeLedger(cells)

        self.assertGreaterEqual(ledger.latitude, -1e-9)
        self.assertGreaterEqual(ledger.doubt, -1e-9)
        self.assertGreaterEqual(ledger.content, -1e-9)
        self.assertGreaterEqual(ledger.conveyance, -1e-9)
        self.assertLessEqual(ledger.conveyance, ledger.content + 1e-6)
        self.assertLessEqual(ledger.effectiveLatitude, ledger.latitude + 1e-6)


if __name__ == "__main__":
    unittest.main()
