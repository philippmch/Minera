"""Offline pin, manifest, mutation and fixture gates for conversion metadata."""

import copy
import gzip
import json
from pathlib import Path
import runpy
import unittest

ROOT = Path(__file__).resolve().parents[2]
GENERATOR = runpy.run_path(
    str(ROOT / "scripts/generate-dataframe-conversion-metadata-tests.py")
)
RAW = gzip.decompress(
    (ROOT / "docs/dataframe-conversion-metadata-reference.json.gz").read_bytes()
)
REPORT = json.loads(RAW)


class ConversionMetadataEvidenceTests(unittest.TestCase):
    def reject(self, report):
        with self.assertRaises((ValueError, KeyError)):
            GENERATOR["generate"](json.dumps(report).encode())

    def test_fixture_matches(self):
        self.assertEqual(
            GENERATOR["generate"](RAW),
            (
                ROOT
                / "src/backend/tests/dataframe_conversion_metadata_reference_cases.hpp"
            ).read_text(encoding="utf-8"),
        )
        self.assertEqual(len(REPORT["rows"]), 22)

    def test_provenance_manifest_and_runtime(self):
        for key in ("reference_version", "pandas_version", "capture_id", "case_count"):
            report = copy.deepcopy(REPORT)
            report[key] = "wrong"
            self.reject(report)
        for key in (
            "installed_sha256",
            "pinned_sha256",
            "commit",
            "signature",
            "module",
            "method",
        ):
            report = copy.deepcopy(REPORT)
            report["reference_provenance"][0][key] = "wrong"
            self.reject(report)
        report = copy.deepcopy(REPORT)
        report["rows"].pop()
        self.reject(report)

    def test_unexpected_mutation_error_or_metadata_rejected(self):
        report = copy.deepcopy(REPORT)
        report["rows"][0]["input_after"]["attrs"]["new"] = True
        self.reject(report)
        report = copy.deepcopy(REPORT)
        report["rows"][0]["error"] = {"type": "ValueError", "message": "new"}
        self.reject(report)
        report = copy.deepcopy(REPORT)
        report["rows"][0]["result"]["classifiers"] = {"new": ["concept:name"]}
        self.reject(report)

    def test_dataframe_and_stream_property_distinction(self):
        by_name = {row["name"]: row for row in REPORT["rows"]}
        dataframe = by_name["case_property_log"]
        self.assertIn("pm4py:param:case_id_key", dataframe["input"]["attrs"])
        self.assertNotIn("pm4py:param:case_id_key", dataframe["result"]["properties"])
        self.assertEqual(dataframe["result"]["properties"]["case_id_key"], "ordinary")
        self.assertEqual(dataframe["result"]["attributes"], {"origin": "csv"})
        self.assertEqual(
            by_name["existing_stream_log"]["result"]["properties"][
                "pm4py:param:case_id_key"
            ],
            "preserved",
        )


if __name__ == "__main__":
    unittest.main()
