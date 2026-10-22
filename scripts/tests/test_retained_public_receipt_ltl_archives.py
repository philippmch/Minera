"""Offline integration with complete, committed public receipts; no reference imports."""

import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    'receipt_index_ltl_integration', ROOT / 'scripts/retained_public_receipt_index.py')
INDEX = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(INDEX)


class RetainedLtlArchiveIntegration(unittest.TestCase):
    def test_original_receipts_cache_and_archived_provenance(self):
        manifest = json.loads((ROOT / 'docs/retained-public-receipt-ltl-manifest.json').read_bytes())
        expected = json.loads((ROOT / 'docs/retained-public-receipt-ltl-expected.json').read_bytes())
        manifest['root'] = str(ROOT)
        with tempfile.TemporaryDirectory() as folder:
            index = INDEX.Index(Path(folder) / 'receipts.sqlite')
            try:
                report = index.run(manifest)
                self.assertFalse(report['catalogue_audit_complete'])
                self.assertEqual(report['reference_calls'], 0)
                self.assertEqual(report['cache_hits'], 0)
                self.assertEqual(report['containers'], 3)
                self.assertEqual(set(report['per_module']), set(expected['modules']))
                for module, oracle in expected['modules'].items():
                    with self.subTest(module=module):
                        stats = report['per_module'][module]
                        self.assertEqual(stats['blocking_gaps'], 0)
                        self.assertTrue(stats['complete_within_declared_inventory'])
                        self.assertFalse(stats['no_prior_calls_within_declared_inventory'])
                        self.assertEqual(stats['distinct_actual_receipts'], oracle['count'])
                        rows = index.db.execute(
                            "SELECT receipt_sha256,canonical FROM observations "
                            "WHERE module=? AND status='actual_outcome'", (module,)).fetchall()
                        digests = set()
                        for digest, canonical in rows:
                            self.assertEqual(hashlib.sha256(canonical).hexdigest(), digest)
                            digests.add(digest)
                        self.assertEqual(len(digests), oracle['count'])
                        actual = hashlib.sha256(('\n'.join(sorted(digests)) + '\n').encode()).hexdigest()
                        self.assertEqual(actual, oracle['sorted_receipt_sha256'])
                # Sources are rehashed even on immutable parse cache hits; provenance
                # is retained for admitted receipts and unlisted members separately.
                repeated = index.run(manifest)
                self.assertEqual(repeated['cache_hits'], 3)
                self.assertEqual(repeated['per_module'], report['per_module'])
                dispositions = dict(index.db.execute(
                    'SELECT disposition,COUNT(*) FROM members GROUP BY disposition'))
                self.assertGreater(dispositions['admitted'], 0)
                self.assertGreater(dispositions['unlisted-not-decoded'], 0)
            finally:
                index.db.close()


if __name__ == '__main__':
    unittest.main()
