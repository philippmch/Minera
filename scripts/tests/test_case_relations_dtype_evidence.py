"""Offline integrity gates for pinned dtype observations."""

import copy
import gzip
import json
from pathlib import Path
import runpy
import unittest

ROOT = Path(__file__).resolve().parents[2]
GENERATOR = runpy.run_path(str(ROOT / "scripts/generate-case-relations-dtype-tests.py"))
RAW = gzip.decompress(
    (ROOT / "docs/case-relations-dtype-reference.json.gz").read_bytes()
)
REPORT = json.loads(RAW)


class DtypeEvidenceTests(unittest.TestCase):
    def reject(self, report):
        with self.assertRaises((ValueError, KeyError)):
            GENERATOR["generate"](json.dumps(report).encode())

    def test_generated_fixture_matches(self):
        self.assertEqual(
            GENERATOR["generate"](RAW),
            (
                ROOT / "src/backend/tests/case_relations_dtype_reference_cases.hpp"
            ).read_text(encoding="utf-8"),
        )

    def test_runtime_manifest_and_selectors(self):
        for field in (
            "format_version",
            "algorithm",
            "reference_version",
            "pandas_version",
            "reference_commit",
            "case_count",
        ):
            report = copy.deepcopy(REPORT)
            report[field] = "wrong"
            self.reject(report)
        for field in (
            "timestamp_key",
            "parameters",
            "inputs_unchanged",
            "input_metadata_unchanged",
        ):
            report = copy.deepcopy(REPORT)
            report["rows"][0][field] = None
            self.reject(report)
        report = copy.deepcopy(REPORT)
        report["rows"].reverse()
        self.reject(report)

    def test_pin_is_not_self_certified(self):
        report = copy.deepcopy(REPORT)
        module = report["reference_provenance"]["modules"][0]
        module["installed_sha256"] = module["pinned_sha256"] = "0" * 64
        self.reject(report)

    def test_result_tags_and_shape_are_checked(self):
        for field, value in (("shape", [0, 0]), ("cell_tags", []), ("dtypes", {})):
            report = copy.deepcopy(REPORT)
            report["rows"][0]["result"][field] = value
            self.reject(report)
        report = copy.deepcopy(REPORT)
        cell = next(iter(report["rows"][0]["result"]["cell_tags"][0]["cells"].values()))
        cell["missing"] = not cell["missing"]
        self.reject(report)

    def test_before_after_and_input_tags_are_checked(self):
        for field in ("input_metadata_after", "input_cell_tags"):
            report = copy.deepcopy(REPORT)
            report["rows"][0][field]["base"] = []
            self.reject(report)

    def test_category_descriptor_required(self):
        report = copy.deepcopy(REPORT)
        row = next(row for row in report["rows"] if row["result"]["categorical_dtypes"])
        row["result"]["categorical_dtypes"] = {}
        self.reject(report)

    def test_scalar_domain_is_exact(self):
        scalar = GENERATOR["scalar"]
        self.assertIn("uint64_t", scalar({"kind": "int", "value": 2**64 - 1}))
        for value in (2**64, -(2**63) - 1):
            with self.assertRaises(ValueError):
                scalar({"kind": "int", "value": value})
        with self.assertRaises(ValueError):
            scalar({"kind": "int", "value": True})


if __name__ == "__main__":
    unittest.main()
