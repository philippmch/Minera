"""Integrity and semantic gates for public container case grouping evidence."""

import gzip
import json
from pathlib import Path
import runpy
import unittest

ROOT = Path(__file__).resolve().parents[2]
GEN = runpy.run_path(str(ROOT / "scripts/generate-container-case-grouping-tests.py"))
RAW = gzip.decompress(
    (ROOT / "docs/container-case-grouping-reference.json.gz").read_bytes()
)
REPORT = json.loads(RAW)
ROWS = {row["name"]: row for row in REPORT["rows"]}


class ContainerGroupingEvidence(unittest.TestCase):
    def test_fixture_and_integrity(self):
        self.assertEqual(
            GEN["generate"](RAW),
            (
                ROOT / "src/backend/tests/container_case_grouping_reference_cases.hpp"
            ).read_text(encoding="utf-8"),
        )
        self.assertEqual(len(ROWS), 15)
        with self.assertRaises(ValueError):
            GEN["generate"](RAW + b" ")
        proof = REPORT["reference_provenance"]
        self.assertEqual(proof["commit"], "0715c31f3f1d85924107d6aa5329806db69df899")
        self.assertEqual(proof["installed_sha256"], proof["pinned_sha256"])
        self.assertEqual(proof["function"], "convert_to_event_log")

    def test_recursive_equality_and_domains(self):
        self.assertEqual(
            ROWS["tuple_numeric"]["result"][0]["activities"], ["A0", "A1", "A2"]
        )
        for name in ("nested_tuple", "nested_frozenset"):
            self.assertEqual(len(ROWS[name]["result"]), 1)
        for name in (
            "tuple_order",
            "tuple_wide",
            "tuple_null",
            "tuple_vs_frozenset",
            "frozenset_numeric",
        ):
            self.assertEqual(len(ROWS[name]["result"]), 2)
        self.assertEqual(len(ROWS["empty_domains"]["result"]), 4)
        for row in ROWS.values():
            self.assertEqual(row["input"], row["input_after"])

    def test_unhashable_errors(self):
        for name, kind in (
            ("list_rejected", "list"),
            ("dict_rejected", "dict"),
            ("set_rejected", "set"),
            ("nested_list_rejected", "list"),
            ("late_unhashable", "list"),
        ):
            self.assertEqual(
                ROWS[name]["error"],
                {"type": "TypeError", "message": "unhashable type: '" + kind + "'"},
            )


if __name__ == "__main__":
    unittest.main()
