"""Offline regression checks for the durable case-relations oracle evidence."""

import copy
import gzip
import importlib.util
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "case_reference_generator", ROOT / "scripts/generate-case-relations-reference-tests.py")
GENERATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GENERATOR)
RAW = gzip.decompress((ROOT / "docs/case-relations-reference.json.gz").read_bytes())
REPORT = json.loads(RAW)


class CaseRelationsEvidenceTests(unittest.TestCase):
    def reject(self, report):
        with self.assertRaises((ValueError, KeyError)):
            GENERATOR.generate(json.dumps(report).encode())

    def test_generated_suite_matches_durable_evidence(self):
        expected = (ROOT / "src/backend/tests/case_relations_reference_cases.hpp").read_text()
        self.assertEqual(GENERATOR.generate(RAW), expected)

    def test_truncated_or_reordered_observations_rejected(self):
        for change in (lambda rows: rows.pop(), lambda rows: rows.reverse()):
            report = copy.deepcopy(REPORT)
            change(report["rows"])
            self.reject(report)

    def test_reference_and_pandas_versions_enforced(self):
        for field in ("reference_version", "pandas_version"):
            report = copy.deepcopy(REPORT)
            report[field] = "0.0.0"
            self.reject(report)

    def test_time_observations_are_required_and_cross_checked(self):
        for field in ("rows", "reference_provenance", "pandas_version"):
            report = copy.deepcopy(REPORT)
            del report["time_reference"][field]
            self.reject(report)
        report = copy.deepcopy(REPORT)
        report["time_reference"]["rows"][0]["timestamps"][0] = "changed"
        self.reject(report)

    def test_mismatched_reference_bytes_rejected(self):
        report = copy.deepcopy(REPORT)
        report["reference_provenance"]["modules"][0]["installed_sha256"] = "0" * 64
        self.reject(report)

    def test_bool_and_mixed_dates_keep_scalar_kinds(self):
        rows = {row["name"]: row for row in REPORT["rows"]}
        boolean = rows["bool_numeric_timestamp"]["base_input"]["records"]
        self.assertTrue(all(type(row["values"]["when"]) is bool for row in boolean))
        self.assertTrue(all(row["kinds"]["when"] == "bool" for row in boolean))
        mixed = rows["mixed_datetime_string_timestamp"]["result"]["records"]
        self.assertEqual([row["kinds"]["when"] for row in mixed],
                         ["date", "date", "string", "string"])


if __name__ == "__main__":
    unittest.main()
