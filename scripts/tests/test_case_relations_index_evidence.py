"""Offline integrity gates for public index observations and native checks."""

import copy
import gzip
import json
from pathlib import Path
import runpy
import unittest

ROOT = Path(__file__).resolve().parents[2]
GENERATOR = runpy.run_path(str(ROOT / "scripts/generate-case-relations-index-tests.py"))
RAW = gzip.decompress(
    (ROOT / "docs/case-relations-index-reference.json.gz").read_bytes()
)
REPORT = json.loads(RAW)


class CaseIndexEvidenceTests(unittest.TestCase):
    def reject(self, report):
        with self.assertRaises((ValueError, KeyError)):
            GENERATOR["generate"](json.dumps(report).encode())

    def test_generated_checks_match_durable_observations(self):
        self.assertEqual(
            GENERATOR["generate"](RAW),
            (
                ROOT / "src/backend/tests/case_relations_index_reference_cases.hpp"
            ).read_text(encoding="utf-8"),
        )

    def test_runtime_and_manifest_gates(self):
        for field in ("reference_version", "pandas_version"):
            report = copy.deepcopy(REPORT)
            report[field] = "wrong"
            self.reject(report)
        for mutate in (lambda rows: rows.pop(), lambda rows: rows.reverse()):
            report = copy.deepcopy(REPORT)
            mutate(report["rows"])
            self.reject(report)

    def test_provenance_is_pinned_not_just_self_consistent(self):
        report = copy.deepcopy(REPORT)
        module = report["reference_provenance"]["modules"][0]
        module["installed_sha256"] = module["pinned_sha256"] = "0" * 64
        self.reject(report)
        report = copy.deepcopy(REPORT)
        report["reference_provenance"]["modules"][1] = report["reference_provenance"][
            "modules"
        ][0]
        self.reject(report)

    def test_incomplete_labels_or_mutated_inputs_rejected(self):
        for field in ("input_indexes", "output_index"):
            report = copy.deepcopy(REPORT)
            index = report["rows"][0][field]
            if isinstance(index, list):
                index = index[0]
            index["labels"].pop()
            self.reject(report)
        report = copy.deepcopy(REPORT)
        report["rows"][0]["inputs_unchanged"] = False
        self.reject(report)

    def test_tuple_and_float_types_are_asserted(self):
        generated = GENERATOR["generate"](RAW)
        self.assertIn("std::vector<EventTableIndexLabel>", generated)
        self.assertIn('reference_number("", double{0.0})', generated)
        self.assertIn("actual_index != expected_index", generated)
        report = copy.deepcopy(REPORT)
        report["rows"][0]["output_index"]["labels"][0]["kind"] = "string"
        self.reject(report)


if __name__ == "__main__":
    unittest.main()
