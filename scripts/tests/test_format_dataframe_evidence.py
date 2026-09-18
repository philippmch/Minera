"""Offline integrity gates for the public formatter capture and native fixture."""

import copy
import gzip
import json
from pathlib import Path
import runpy
import unittest

ROOT = Path(__file__).resolve().parents[2]
GENERATOR = runpy.run_path(str(ROOT / "scripts/generate-format-dataframe-tests.py"))
RAW = gzip.decompress((ROOT / "docs/format-dataframe-reference.json.gz").read_bytes())
REPORT = json.loads(RAW)


class FormatDataFrameEvidenceTests(unittest.TestCase):
    def reject(self, report):
        with self.assertRaises((ValueError, KeyError)):
            GENERATOR["generate"](json.dumps(report).encode())

    def test_durable_fixture_matches(self):
        generated = GENERATOR["generate"](RAW)
        self.assertEqual(
            generated,
            (ROOT / "src/backend/tests/dataframe_format_reference_cases.hpp").read_text(
                encoding="utf-8"
            ),
        )
        self.assertEqual(generated.count("inline void check_case_"), 119)
        self.assertEqual(
            set(GENERATOR["EXCLUDED_CASES"]),
            {"non_string_column_labels_with_selectors"},
        )

    def test_runtime_signature_and_manifest(self):
        for field in (
            "reference_version",
            "pandas_version",
            "capture_id",
            "case_count",
            "public_signature",
        ):
            report = copy.deepcopy(REPORT)
            report[field] = "wrong"
            self.reject(report)
        report = copy.deepcopy(REPORT)
        report["rows"].reverse()
        self.reject(report)

    def test_provenance_is_pinned(self):
        report = copy.deepcopy(REPORT)
        provenance = report["reference_provenance"]
        provenance["installed_sha256"] = provenance["pinned_sha256"] = "0" * 64
        self.reject(report)

    def test_unexpected_capture_error_is_not_a_native_rejection_case(self):
        report = copy.deepcopy(REPORT)
        report["rows"][0].pop("result")
        report["rows"][0]["error"] = {"type": "Exception", "message": "capture failed"}
        report["successful_case_count"] -= 1
        report["error_case_count"] += 1
        self.reject(report)

    def test_mutation_snapshots_and_flags(self):
        for field in (
            "inputs_unchanged",
            "input_metadata_unchanged",
            "input_mutated",
            "input_metadata_mutated",
        ):
            report = copy.deepcopy(REPORT)
            report["rows"][0][field] = not report["rows"][0][field]
            self.reject(report)
        report = copy.deepcopy(REPORT)
        report["rows"][0]["input_scalar_snapshot_after"] = []
        self.reject(report)

    def test_row_dtype_and_index_metadata(self):
        for field, value in (
            ("shape", [0, 0]),
            ("dtypes", {}),
            ("column_metadata", []),
        ):
            report = copy.deepcopy(REPORT)
            report["rows"][0]["result"][field] = value
            self.reject(report)
        report = copy.deepcopy(REPORT)
        report["rows"][0]["result"]["index_metadata"]["labels"][0]["value"] = 999
        self.reject(report)
        report = copy.deepcopy(REPORT)
        tag = next(
            iter(report["rows"][0]["result"]["records"][0]["cell_tags"].values())
        )
        tag["missing"] = not tag["missing"]
        self.reject(report)

    def test_native_options_and_return_contract(self):
        report = copy.deepcopy(REPORT)
        report["rows"][0]["parameters"]["unmapped_option"] = True
        self.reject(report)
        report = copy.deepcopy(REPORT)
        report["rows"][0]["returned_same_object"] = True
        self.reject(report)

    def test_categorical_descriptor_is_required(self):
        report = copy.deepcopy(REPORT)
        row = next(
            row for row in report["rows"] if row["name"] == "categorical_case_activity"
        )
        row["input"]["categorical_dtypes"] = {}
        self.reject(report)

    def test_infinities_and_mutating_rejections_are_retained(self):
        row = next(
            row
            for row in REPORT["rows"]
            if row["name"] == "numeric_boundary_negative_infinite_same"
        )
        cell = row["input"]["records"][0]["cell_tags"]["time:timestamp"]
        self.assertEqual(
            cell,
            {
                "dtype": "object",
                "kind": "float",
                "missing": False,
                "value": "-Infinity",
            },
        )
        self.assertIn(
            "-std::numeric_limits<double>::infinity()",
            GENERATOR["scalar"](cell, "time:timestamp"),
        )
        failed = next(
            row
            for row in REPORT["rows"]
            if row["name"] == "numeric_boundary_custom_error_mutation"
        )
        self.assertTrue(failed["input_mutated"])
        self.assertIn("time:timestamp", failed["input_after"]["columns"])
        self.assertEqual(
            failed["error"]["message"], "Categorical categories must be unique"
        )


if __name__ == "__main__":
    unittest.main()
