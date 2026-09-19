"""Public contract checks for the still-open replay dataframe output path."""

import gzip
import hashlib
import json
from pathlib import Path
import unittest
import runpy

ROOT = Path(__file__).resolve().parents[2]
RAW = gzip.decompress(
    (ROOT / "docs/token-replay-diagnostics-dataframe-reference.json.gz").read_bytes()
)
REPORT = json.loads(RAW)
ROWS = {row["name"]: row for row in REPORT["rows"]}
LIST_REPORT = json.loads(
    gzip.decompress(
        (ROOT / "docs/token-replay-dataframe-grouping-reference.json.gz").read_bytes()
    )
)
DIGEST = "0b09ae6e916aeef34fce5cc616b1862d6f504ab5f821138ca472dffe75fb8840"
NAMES = (
    "empty",
    "typed_empty",
    "strings_source_order",
    "booleans",
    "empty_strings",
    "upper",
    "upper_reverse",
    "wide",
    "wide_float_first",
    "wide_only_int",
    "wide_float_unrelated",
    "negative",
    "uint_only",
    "uint_float",
    "bool_suppresses",
    "string_suppresses",
    "upper_repeat",
    "signed_unsigned",
    "bool_first",
    "float_before_bool",
    "int_before_float",
    "nan_keys",
    "none_keys",
    "infinity",
    "float_first_wide_twice",
    "nan_only",
    "prior_equal",
    "prior_wide_equal",
    "prior_wide_distinct",
    "prior_wide_mixed",
    "prior_upper",
    "prior_small_fraction",
    "prior_string_mixed",
    "linear_fit",
    "linear_missing",
    "linear_remaining",
    "linear_reverse",
    "linear_unknown",
    "linear_unknown_between",
    "linear_source_order",
    "custom_selectors",
)


class DiagnosticsDataframeEvidenceTests(unittest.TestCase):
    def test_native_fixture_and_tamper_gate(self):
        generator = runpy.run_path(
            str(ROOT / "scripts/generate-token-replay-diagnostics-dataframe-tests.py")
        )["generate"]
        generated = generator(RAW)
        self.assertEqual(
            generated,
            (
                ROOT
                / "src/backend/tests/token_replay_diagnostics_dataframe_reference_cases.hpp"
            ).read_text(encoding="utf-8"),
        )
        self.assertEqual(generated.count("inline void check_replay_dataframe_"), 41)
        changed = json.loads(RAW)
        changed["rows"][-1]["result"]["records"][0]["values"]["produced"] = 99
        with self.assertRaises(ValueError):
            generator(json.dumps(changed).encode())

    def test_pin_and_manifest(self):
        self.assertEqual(hashlib.sha256(RAW).hexdigest(), DIGEST)
        self.assertEqual(
            REPORT["reference_provenance"], LIST_REPORT["reference_provenance"]
        )
        self.assertEqual(
            REPORT["reference_provenance"]["commit"],
            "0715c31f3f1d85924107d6aa5329806db69df899",
        )
        self.assertEqual(REPORT["reference_version"], "2.7.22")
        self.assertEqual(REPORT["pandas_version"], "3.0.5")
        self.assertEqual(REPORT["case_count"], 41)
        self.assertEqual(tuple(ROWS), NAMES)
        self.assertEqual(
            REPORT["parameters"],
            {
                "return_diagnostics_dataframe": True,
                "opt_parameters": {"show_progress_bar": False},
            },
        )

    def test_schema_and_immutability(self):
        for row in REPORT["rows"]:
            self.assertEqual(row["input"], row["input_after"])
            if "result" not in row:
                continue
            result = row["result"]
            self.assertEqual(result["attrs"], {})
            self.assertEqual(result["index_metadata"]["class"], "RangeIndex")
            if row["name"] == "typed_empty":
                self.assertEqual(result["shape"], [0, 0])
                continue
            self.assertEqual(
                result["columns"],
                [
                    "case_id",
                    "is_fit",
                    "trace_fitness",
                    "missing",
                    "remaining",
                    "produced",
                    "consumed",
                ],
            )
            for key, dtype in {
                "is_fit": "bool",
                "trace_fitness": "float64",
                "missing": "int64",
                "remaining": "int64",
                "produced": "int64",
                "consumed": "int64",
            }.items():
                self.assertEqual(result["dtypes"][key], dtype)

    def test_exact_groups_differ_from_list_output(self):
        rows = {r["name"]: r for r in LIST_REPORT["rows"]}
        self.assertEqual(len(rows["upper"]["result"]), 1)
        self.assertEqual(ROWS["upper"]["result"]["shape"], [2, 7])
        self.assertEqual(len(rows["wide_float_first"]["result"]), 1)
        values = [r["values"] for r in ROWS["wide_float_first"]["result"]["records"]]
        self.assertEqual([r["produced"] for r in values], [3, 2])
        self.assertEqual([r["case_id"] for r in values], [float(2**53), float(2**53)])

    def test_source_order_and_dtype_inference(self):
        values = [
            r["values"] for r in ROWS["strings_source_order"]["result"]["records"]
        ]
        self.assertEqual([r["case_id"] for r in values], ["z", "a", "b"])
        self.assertEqual([r["produced"] for r in values], [3, 2, 2])
        for name, dtype in {
            "bool_first": "object",
            "float_before_bool": "float64",
            "int_before_float": "int64",
            "signed_unsigned": "object",
            "uint_only": "uint64",
            "booleans": "bool",
        }.items():
            self.assertEqual(ROWS[name]["result"]["dtypes"]["case_id"], dtype)

    def test_expected_rejections(self):
        errors = {r["name"]: r["error"] for r in REPORT["rows"] if "error" in r}
        self.assertEqual(set(errors), {"empty", "nan_keys", "none_keys", "nan_only"})
        for name in ("nan_keys", "none_keys", "nan_only"):
            self.assertEqual(
                errors[name],
                {
                    "type": "Exception",
                    "message": "the case ID column should not contain any empty value.",
                },
            )


if __name__ == "__main__":
    unittest.main()
