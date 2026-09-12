"""Focused, offline checks for the Alpha+ audit gates."""
import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


SCRIPTS = Path(__file__).resolve().parents[1]


def load_script(filename, name):
    spec = importlib.util.spec_from_file_location(name, SCRIPTS / filename)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def native_empty_result():
    return {
        "family": "ALPHA_PLUS",
        "places": [{"id": 0, "name": {"boundary": "end"}, "initial": 0, "final": 1}],
        "transitions": [],
        "detached_initial_marking": [{"name": {"before": ["artificial_start"],
                                                  "after": ["artificial_end"]}, "tokens": 1}],
        "detached_final_marking": [],
    }


class AlphaPlusGateTests(unittest.TestCase):
    def setUp(self):
        with patch.dict(sys.modules, {"pm4py": None}):
            self.capture = load_script("capture-alpha-plus-reference.py", "alpha_capture_test")
        self.sampled = load_script("check-alpha-plus-sampled-support.py", "alpha_sampled_test")

    def run_sampled(self, expected, stdout, *, reference_commit=None):
        with tempfile.TemporaryDirectory(prefix="alpha-plus-gate-test-") as directory:
            directory = Path(directory)
            reference = directory / "reference.json"
            output = directory / "report.json"
            document = {
                "reference": "2.7.22",
                "reference_samples": 1,
                "rows": [{"traces": [[]], "remove_unconnected": False, "result": expected}],
            }
            if reference_commit is not None:
                document["reference_commit"] = reference_commit
            reference.write_text(json.dumps(document), encoding="utf-8")
            process = self.sampled.subprocess.CompletedProcess(
                [], 0, stdout, "")
            argv = ["check-alpha-plus-sampled-support.py", "--reference", str(reference),
                    "--native", "native", "--samples", "1", "--output", str(output)]
            with patch.object(self.sampled.subprocess, "run", return_value=process), \
                    patch.object(sys, "argv", argv):
                code = self.sampled.main()
            return code, json.loads(output.read_text(encoding="utf-8"))

    def test_supported_draw_returns_zero(self):
        summary = self.capture.summarize_native(native_empty_result())
        code, report = self.run_sampled(summary, json.dumps(native_empty_result()),
                                        reference_commit=self.capture.REFERENCE_COMMIT)
        self.assertEqual(code, 0)
        self.assertEqual(report["unsupported_draws"], 0)
        self.assertEqual(report["malformed_draws"], 0)

    def test_unsupported_draw_returns_nonzero(self):
        code, report = self.run_sampled({"different": True}, json.dumps(native_empty_result()),
                                        reference_commit=self.capture.REFERENCE_COMMIT)
        self.assertNotEqual(code, 0)
        self.assertEqual(report["unsupported_draws"], 1)

    def test_malformed_native_output_returns_nonzero(self):
        code, report = self.run_sampled({}, "not-json",
                                        reference_commit=self.capture.REFERENCE_COMMIT)
        self.assertNotEqual(code, 0)
        self.assertEqual(report["malformed_draws"], 1)

    def test_missing_reference_commit_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "reference commit"):
            self.run_sampled({}, json.dumps(native_empty_result()))

    def test_mismatched_reference_commit_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "reference commit"):
            self.run_sampled({}, json.dumps(native_empty_result()), reference_commit="wrong")

    def test_capture_module_does_not_import_pm4py_for_native_summary(self):
        self.assertNotIn("pm4py", self.capture.__dict__)
        self.assertEqual(len(self.capture.summarize_native(native_empty_result())["places"]), 1)


if __name__ == "__main__":
    unittest.main()
