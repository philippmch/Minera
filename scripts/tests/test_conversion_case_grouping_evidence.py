"""Offline integrity gates for typed stream grouping evidence."""

import copy
import gzip
import json
from pathlib import Path
import runpy
import unittest

ROOT = Path(__file__).resolve().parents[2]
GEN = runpy.run_path(str(ROOT / "scripts/generate-conversion-case-grouping-tests.py"))
RAW = gzip.decompress(
    (ROOT / "docs/conversion-case-grouping-reference.json.gz").read_bytes()
)
REPORT = json.loads(RAW)


class GroupingEvidenceTests(unittest.TestCase):
    def reject(self, report):
        with self.assertRaises((ValueError, KeyError)):
            GEN["generate"](json.dumps(report).encode())

    def test_fixture_matches(self):
        self.assertEqual(
            GEN["generate"](RAW),
            (
                ROOT / "src/backend/tests/conversion_case_grouping_reference_cases.hpp"
            ).read_text(encoding="utf-8"),
        )
        self.assertEqual(len(REPORT["rows"]), 17)

    def test_pin_and_manifest(self):
        for field in (
            "commit",
            "installed_sha256",
            "pinned_sha256",
            "signature",
            "method",
        ):
            r = copy.deepcopy(REPORT)
            r["reference_provenance"][field] = "wrong"
            self.reject(r)
        r = copy.deepcopy(REPORT)
        r["rows"].pop()
        self.reject(r)

    def test_mutation_identity_and_partition(self):
        r = copy.deepcopy(REPORT)
        r["rows"][0]["input_after"][0]["case"]["value"] = 42
        self.reject(r)
        r = copy.deepcopy(REPORT)
        r["rows"][0]["input"][0]["identity"] = 99
        self.reject(r)
        r = copy.deepcopy(REPORT)
        r["rows"][0]["result"][0]["activities"].pop()
        self.reject(r)

    def test_exact_numeric_and_nan_cases(self):
        rows = {r["name"]: r for r in REPORT["rows"]}
        self.assertEqual(len(rows["integer_float_bool"]["result"]), 2)
        self.assertEqual(len(rows["wide_exact"]["result"]), 2)
        self.assertEqual(
            rows["wide_exact"]["result"][1]["case"]["value"], 9007199254740993
        )
        self.assertEqual(len(rows["different_nan"]["result"]), 2)
        self.assertEqual(len(rows["shared_nan"]["result"]), 1)
        self.assertNotEqual(
            rows["different_nan"]["input"][0]["identity"],
            rows["different_nan"]["input"][1]["identity"],
        )
        self.assertEqual(
            rows["shared_nan"]["input"][0]["identity"],
            rows["shared_nan"]["input"][1]["identity"],
        )


if __name__ == "__main__":
    unittest.main()
