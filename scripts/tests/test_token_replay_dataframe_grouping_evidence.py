"""Offline gates for the public dataframe replay grouping matrix."""

import copy
import gzip
import json
from pathlib import Path
import runpy
import unittest

ROOT = Path(__file__).resolve().parents[2]
GEN = runpy.run_path(
    str(ROOT / "scripts/generate-token-replay-dataframe-grouping-tests.py")
)
RAW = gzip.decompress(
    (ROOT / "docs/token-replay-dataframe-grouping-reference.json.gz").read_bytes()
)
REPORT = json.loads(RAW)


class ReplayGroupingEvidenceTests(unittest.TestCase):
    def reject(self, report):
        with self.assertRaises((ValueError, KeyError)):
            GEN["generate"](json.dumps(report).encode())

    def test_fixture_matches(self):
        generated = GEN["generate"](RAW)
        self.assertEqual(
            generated,
            (
                ROOT
                / "src/backend/tests/token_replay_dataframe_grouping_reference_cases.hpp"
            ).read_text(encoding="utf-8"),
        )
        self.assertEqual(generated.count("inline void check_replay_grouping_"), 25)

    def test_pin_manifest_and_net(self):
        for field in (
            "reference_version",
            "pandas_version",
            "capture_id",
            "case_count",
        ):
            r = copy.deepcopy(REPORT)
            r[field] = "wrong"
            self.reject(r)
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
        r["net"]["initial"]["p"] = 2
        self.reject(r)
        r = copy.deepcopy(REPORT)
        r["rows"].pop()
        self.reject(r)

    def test_exclusions_mutation_and_counter_gates(self):
        r = copy.deepcopy(REPORT)
        r["exclusions"].pop("none_keys")
        self.reject(r)
        r = copy.deepcopy(REPORT)
        next(row for row in r["rows"] if row["name"] == "none_keys")["error"][
            "message"
        ] = "changed"
        self.reject(r)
        r = copy.deepcopy(REPORT)
        r["rows"][0]["input_after"]["attrs"]["new"] = True
        self.reject(r)
        r = copy.deepcopy(REPORT)
        r["rows"][0]["result"][0]["consumed"] = -1
        self.reject(r)

    def test_replacement_and_suppression_observations(self):
        rows = {r["name"]: r for r in REPORT["rows"]}
        paths = lambda name: [r["activities"] for r in rows[name]["result"]]
        self.assertEqual(paths("upper"), [["A"]])
        self.assertEqual(paths("upper_reverse"), [["B"]])
        self.assertEqual(paths("wide_float_first"), [["B"]])
        self.assertEqual(paths("wide"), [["B", "C"], ["A"]])
        self.assertEqual(len(paths("bool_suppresses")), 4)
        self.assertEqual(len(paths("string_suppresses")), 4)


if __name__ == "__main__":
    unittest.main()
