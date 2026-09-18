"""Offline provenance and outcome gates for dataframe validation."""

import copy
import gzip
import json
from pathlib import Path
import runpy
import unittest

ROOT = Path(__file__).resolve().parents[2]
GEN = runpy.run_path(
    str(ROOT / "scripts/generate-dataframe-column-validation-tests.py")
)
RAW = gzip.decompress(
    (ROOT / "docs/dataframe-column-validation-reference.json.gz").read_bytes()
)
REPLAY = gzip.decompress(
    (ROOT / "docs/token-replay-dataframe-grouping-reference.json.gz").read_bytes()
)
REPORT = json.loads(RAW)


class ValidationEvidenceTests(unittest.TestCase):
    def reject(self, report):
        with self.assertRaises((ValueError, KeyError)):
            GEN["generate"](json.dumps(report).encode(), REPLAY)

    def test_fixture_matches(self):
        generated = GEN["generate"](RAW, REPLAY)
        self.assertEqual(
            generated,
            (
                ROOT
                / "src/backend/tests/dataframe_column_validation_reference_cases.hpp"
            ).read_text(encoding="utf-8"),
        )
        self.assertEqual(generated.count("inline void check_validation_"), 64)
        self.assertEqual(generated.count("inline void check_validated_replay_"), 28)

    def test_provenance_and_manifest(self):
        for field in (
            "reference_version",
            "pandas_version",
            "capture_id",
            "case_count",
        ):
            report = copy.deepcopy(REPORT)
            report[field] = "wrong"
            self.reject(report)
        for field in (
            "commit",
            "installed_sha256",
            "pinned_sha256",
            "signature",
            "method",
        ):
            report = copy.deepcopy(REPORT)
            report["reference_provenance"][field] = "wrong"
            self.reject(report)
        report = copy.deepcopy(REPORT)
        report["rows"].pop()
        self.reject(report)

    def test_mutation_and_errors(self):
        report = copy.deepcopy(REPORT)
        report["rows"][0]["input_after"]["attrs"]["new"] = True
        self.reject(report)
        report = copy.deepcopy(REPORT)
        next(row for row in report["rows"] if "error" in row)["error"]["message"] = (
            "changed"
        )
        self.reject(report)

    def test_replay_error_integrity(self):
        report = json.loads(REPLAY)
        self.assertEqual(sum("error" in row for row in report["rows"]), 3)
        next(row for row in report["rows"] if "error" in row)["error"]["message"] = (
            "changed"
        )
        with self.assertRaises((ValueError, KeyError)):
            GEN["generate"](RAW, json.dumps(report).encode())


if __name__ == "__main__":
    unittest.main()
