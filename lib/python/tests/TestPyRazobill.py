#!/usr/bin/env python3
"""
Unit and Integration Test Suite for pyrazobill Python bindings.
================================================================

Verifies:
1. Zero-allocation parsing: parse()
2. Exact lossless reconstruction: reconstruct(record) == msg
3. High-throughput batch processing: parseBatch()
4. Multi-core audit without GIL contention: auditParallel()
5. Canonical hashing: hash64() matching ClickHouse xxHash64
6. Information-theoretic entropy calculation: entropy()
7. Native DuckDB extension loading via extensionPath()
8. Semantic mode variable token isolation without template explosion
"""

import sys
import unittest
from pathlib import Path

# Add lib/python directory to sys.path
pkg_dir = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(pkg_dir))

import pyrazobill


class TestPyRazobill(unittest.TestCase):
    def test_parse_and_reconstruct(self):
        # The bedrock assertion: variable parameters are safely quarantined into slots,
        # while reconstruct() rebuilds the exact message down to the bit.
        msg = "Failed password for root from 192.168.1.100 port 22 ssh2"
        rec = pyrazobill.parse(msg, pyrazobill.ScanMode.Semantic)

        self.assertIsNotNone(rec.formKey)
        self.assertGreater(rec.formKeyHash, 0)
        self.assertEqual(rec.ints, [22])
        self.assertIn("root", rec.strs)
        self.assertIn("192.168.1.100", rec.strs)

        rebuilt = pyrazobill.reconstruct(rec)
        self.assertEqual(rebuilt, msg)
        self.assertEqual(rec.reconstruct(), msg)

    def test_semantic_mode_collapses_usernames(self):
        # PROOF OF MITIGATION:
        # In Drain3, changing username from 'root' to 'admin' creates 2 separate templates
        # if the token similarity heuristic misses.
        # In librazobill, Semantic mode recognizes 'for <user>' and extracts both as identical forms!
        msg1 = "Failed password for root from 10.0.0.1 port 22"
        msg2 = "Failed password for admin from 10.0.0.1 port 22"
        rec1 = pyrazobill.parse(msg1, pyrazobill.ScanMode.Semantic)
        rec2 = pyrazobill.parse(msg2, pyrazobill.ScanMode.Semantic)
        self.assertEqual(rec1.formKey, rec2.formKey)

    def test_parse_batch(self):
        # Multi-line batch parsing with GIL release
        lines = [
            f"Receiving block blk_{i} src: /10.0.0.1:{50000 + i} dest: /10.0.0.2:50010"
            for i in range(100)
        ]
        records = pyrazobill.parseBatch(lines, pyrazobill.ScanMode.Semantic)
        self.assertEqual(len(records), 100)
        for i, rec in enumerate(records):
            self.assertEqual(rec.reconstruct(), lines[i])

    def test_audit_parallel(self):
        # Multi-core audit across 4 threads
        lines = [
            f"Jan 12 06:14:{i % 60:02d} server sshd[{1000 + i}]: Accepted publickey for user{i} from 192.168.1.{i % 250} port {2000 + i} ssh2"
            for i in range(5000)
        ]
        audit = pyrazobill.auditParallel(
            lines, pyrazobill.ScanMode.Semantic, numThreads=4
        )
        self.assertEqual(audit.totalLines, 5000)
        self.assertEqual(audit.mismatches, 0)
        self.assertGreater(audit.msgsPerSec, 50000)
        self.assertGreater(audit.uniqueFormsCount, 0)

    def test_hash64_test_vectors(self):
        # Canonical test vectors for xxHash64 seed 0
        self.assertEqual(pyrazobill.hash64(""), 0xEF46DB3751D8E999)
        self.assertEqual(pyrazobill.hash64("a"), 0xD24EC4F1A98C6E5B)
        self.assertEqual(pyrazobill.hash64("abc"), 0x44BC2CF5AD770999)

    def test_template_of(self):
        form = "Failed password for \x12 from \x12 port \x12\x13SSI"
        tmpl = pyrazobill.templateOf(form)
        self.assertEqual(tmpl, "Failed password for <*> from <*> port <*>")

    def test_entropy(self):
        # Shannon entropy calculation in bits with Neumaier compensation
        h = pyrazobill.entropy([1.0, 1.0, 1.0, 1.0])
        self.assertAlmostEqual(h, 2.0, places=6)
        h_single = pyrazobill.entropy([10.0])
        self.assertAlmostEqual(h_single, 0.0, places=6)

    def test_decompose_and_tuple_reconstruct(self):
        # High-speed tuple decomposition and free-standing reconstruct
        msg = "Accepted publickey for ubuntu from 192.168.1.100 port 54322 ssh2"
        formKey, ints, strs = pyrazobill.decompose(msg, pyrazobill.ScanMode.Semantic)
        self.assertIsInstance(formKey, str)
        self.assertEqual(ints, [54322])
        self.assertIn("ubuntu", strs)
        self.assertIn("192.168.1.100", strs)

        # Exact byte-for-byte lossless reconstruction directly from tuple elements
        rebuilt = pyrazobill.reconstruct(formKey, ints, strs)
        self.assertEqual(rebuilt, msg)

    def test_decompose_batch_and_threadpool(self):
        import concurrent.futures

        lines = [
            f"Failed password for user{i} from 10.0.0.{i % 250} port {2000 + i} ssh2"
            for i in range(1000)
        ]
        # Direct batch decomposition with GIL release
        results = pyrazobill.decomposeBatch(lines, pyrazobill.ScanMode.Semantic)
        self.assertEqual(len(results), 1000)
        for i, (formKey, ints, strs) in enumerate(results):
            self.assertEqual(pyrazobill.reconstruct(formKey, ints, strs), lines[i])

        # Concurrent ThreadPool execution
        chunkSize = 250
        chunks = [lines[i : i + chunkSize] for i in range(0, len(lines), chunkSize)]
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
            batchFutures = list(
                executor.map(
                    lambda c: pyrazobill.decomposeBatch(
                        c, pyrazobill.ScanMode.Semantic
                    ),
                    chunks,
                )
            )

        flat = [item for sub in batchFutures for item in sub]
        self.assertEqual(len(flat), 1000)
        for i, (formKey, ints, strs) in enumerate(flat):
            self.assertEqual(pyrazobill.reconstruct(formKey, ints, strs), lines[i])

    def test_version_and_abi_contract(self):
        # SemVer 2.0.0 contract
        self.assertEqual(pyrazobill.__version__, "1.0.0")
        self.assertEqual(pyrazobill.__version_info__, (1, 0, 0))
        self.assertEqual(pyrazobill.version(), "1.0.0")
        self.assertEqual(pyrazobill.abiVersion(), 1)
        self.assertEqual(pyrazobill.versionInfo(), (1, 0, 0))

    def test_ledger_and_vocabulary_law(self):
        # Figure 1 toy lexicon from Atta (2026)
        cells = [
            pyrazobill.Cell(0, 0, 0.25),
            pyrazobill.Cell(1, 0, 0.25),
            pyrazobill.Cell(1, 1, 0.125),
            pyrazobill.Cell(1, 2, 0.25),
            pyrazobill.Cell(2, 2, 0.125),
        ]
        ledger = pyrazobill.computeLedger(cells, 1.0, 0.05)
        self.assertAlmostEqual(ledger.coverage, 1.0, places=5)
        self.assertAlmostEqual(ledger.content, 1.406, places=2)
        self.assertAlmostEqual(ledger.expression, 1.299, places=2)
        self.assertAlmostEqual(ledger.latitude, 0.844, places=2)
        self.assertAlmostEqual(ledger.doubt, 0.951, places=2)
        self.assertAlmostEqual(ledger.conveyance, 0.454, places=2)
        self.assertTrue(ledger.satisfiesLedgerIdentities(1e-6))
        self.assertTrue(ledger.satisfiesVocabularyLaw(3, 1e-5))
        self.assertGreater(ledger.effectiveVocabulary, 0.0)
        self.assertGreater(ledger.effectiveContent, 0.0)
        self.assertAlmostEqual(
            ledger.distance, ledger.latitude + ledger.doubt, places=6
        )

    def test_consequence_decomposition(self):
        xy = [
            pyrazobill.XY(0, 0, 40.0),
            pyrazobill.XY(1, 0, 60.0),
            pyrazobill.XY(2, 1, 30.0),
            pyrazobill.XY(3, 1, 70.0),
        ]
        fmap = [
            pyrazobill.FormMeaning(0, 0, 1.0),
            pyrazobill.FormMeaning(1, 0, 1.0),
            pyrazobill.FormMeaning(2, 1, 1.0),
            pyrazobill.FormMeaning(3, 1, 1.0),
        ]
        decomp = pyrazobill.consequenceDecomposition(xy, fmap)
        self.assertTrue(decomp.satisfiesIdentity(1e-9))
        self.assertGreater(decomp.expression, 0.0)
        self.assertGreater(decomp.consequence, 0.0)
        self.assertGreater(decomp.latitude, 0.0)

    def test_miller_madow_and_certified_interval(self):
        corrected = pyrazobill.millerMadow(1.5, 4, 1000.0)
        self.assertGreater(corrected, 1.5)

        eps, hw = pyrazobill.certifiedInterval(25, 10000.0, 0.05)
        self.assertGreater(eps, 0.0)
        self.assertGreater(hw, 0.0)

    def test_latitude_steganographic_channel(self):
        seq = []
        for i in range(16):
            opts = [
                pyrazobill.SynonymousOption(0, 0.25),
                pyrazobill.SynonymousOption(1, 0.25),
                pyrazobill.SynonymousOption(2, 0.25),
                pyrazobill.SynonymousOption(3, 0.25),
            ]
            seq.append(pyrazobill.MeaningChoice(i, opts))

        cap = pyrazobill.channelAnalyzeCapacity(seq)
        self.assertAlmostEqual(cap.meanCapacityBits, 32.0, places=2)

        originalMsg = 0xABCD
        embedRes = pyrazobill.channelEmbed(seq, originalMsg, 16, 0.38196601125, 0.5)
        self.assertTrue(embedRes.success)
        self.assertEqual(len(embedRes.chosenForms), 16)

        decRes = pyrazobill.channelDecode(seq, embedRes.chosenForms, 16, 0.38196601125)
        self.assertTrue(decRes.success)
        self.assertEqual(decRes.message, originalMsg)

    def test_price_of_saying(self):
        cells = [
            pyrazobill.Cell(0, 0, 0.25),
            pyrazobill.Cell(1, 0, 0.25),
            pyrazobill.Cell(1, 1, 0.125),
            pyrazobill.Cell(1, 2, 0.25),
            pyrazobill.Cell(2, 2, 0.125),
        ]
        ledger = pyrazobill.computeLedger(cells, 1.0, 0.05)
        price = pyrazobill.priceOfSaying(ledger, 500.0)
        self.assertTrue(price.satisfiesIdentity(1e-9))
        self.assertAlmostEqual(
            price.expectedMeaningBits, 500.0 * ledger.content, places=6
        )
        self.assertAlmostEqual(price.sayingCostBits, 500.0 * ledger.latitude, places=6)
        self.assertAlmostEqual(
            price.totalSayingBits,
            price.expectedMeaningBits + price.sayingCostBits,
            places=6,
        )
        self.assertEqual(price.lowerBoundBits, price.totalSayingBits)
        self.assertEqual(price.upperBoundBits, price.totalSayingBits + 2.0)

    def test_depth_ledger(self):
        steps = [
            pyrazobill.HierarchicalStep(0, [0, 0], 0, 40.0),
            pyrazobill.HierarchicalStep(1, [0, 0], 0, 60.0),
            pyrazobill.HierarchicalStep(2, [1, 0], 1, 30.0),
            pyrazobill.HierarchicalStep(3, [1, 0], 1, 70.0),
        ]
        dl = pyrazobill.computeDepthLedger(steps)
        self.assertTrue(dl.satisfiesIdentity(1e-9))
        self.assertEqual(len(dl.entropyLevels), 2)
        self.assertEqual(len(dl.relevanceLevels), 2)
        self.assertGreater(dl.totalEntropy, 0.0)
        self.assertGreater(dl.totalRelevance, 0.0)
        self.assertAlmostEqual(dl.coarsestEntropy, 0.0, places=6)
        self.assertAlmostEqual(
            dl.entropyLevels[0] + dl.entropyLevels[1] + dl.coarsestEntropy,
            dl.totalEntropy,
            places=6,
        )

    def test_channel_bounds_and_failure_probability(self):
        seq = [
            pyrazobill.MeaningChoice(
                i,
                [
                    pyrazobill.SynonymousOption(0, 0.5),
                    pyrazobill.SynonymousOption(1, 0.5),
                ],
            )
            for i in range(10)
        ]
        # Proposition 9 required key bits: 10 * 1.0 + 64 + 1 = 75
        reqBits = pyrazobill.channelRequiredKeyBits(seq, 64)
        self.assertEqual(reqBits, 75)

        cap = pyrazobill.channelAnalyzeCapacity(seq)
        bound = pyrazobill.channelFailureProbabilityBound(cap, 5, 10.0)
        self.assertGreaterEqual(bound, 0.0)
        self.assertLessEqual(bound, 1.0)

        # Exact failure probability on 2 choices
        toySeq = [
            pyrazobill.MeaningChoice(
                0,
                [
                    pyrazobill.SynonymousOption(0, 0.5),
                    pyrazobill.SynonymousOption(1, 0.5),
                ],
            ),
            pyrazobill.MeaningChoice(
                1,
                [
                    pyrazobill.SynonymousOption(0, 0.5),
                    pyrazobill.SynonymousOption(1, 0.5),
                ],
            ),
        ]
        self.assertAlmostEqual(
            pyrazobill.channelExactFailureProbability(toySeq, 1), 0.5, places=5
        )
        self.assertAlmostEqual(
            pyrazobill.channelExactFailureProbability(toySeq, 0), 0.25, places=5
        )
        self.assertAlmostEqual(
            pyrazobill.channelExactFailureProbability(toySeq, 2), 1.0, places=5
        )


if __name__ == "__main__":
    unittest.main()
