"""Focused, offline checks for the DFG evidence gate exit semantics."""
import importlib.util
import gzip
import json
import hashlib
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest.mock import patch


SCRIPTS = Path(__file__).resolve().parents[1]
REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"


def load_script(filename, name):
    spec = importlib.util.spec_from_file_location(name, SCRIPTS / filename)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class FakeEvent(dict):
    pass


class FakeTrace(list):
    def __init__(self, events=None, attributes=None):
        super().__init__(events or [])
        self.attributes = attributes or {}


class FakeEventLog(list):
    def __init__(self, traces=None, attributes=None):
        super().__init__(traces or [])
        self.attributes = attributes or {}


class DfgEvidenceGateTests(unittest.TestCase):
    def setUp(self):
        # The scripts must be importable by report/check tooling without the
        # pinned reference environment installed.
        with patch.dict(sys.modules, {"pm4py": None, "pandas": None}):
            self.alignment = load_script(
                "capture-dfg-alignment-reference.py", "dfg_alignment_gate_test")
            self.native = load_script(
                "capture-dfg-native-reference.py", "dfg_native_gate_test")
            self.search = load_script(
                "capture-dfg-search-reference.py", "dfg_search_gate_test")
            self.zero_tie = load_script(
                "capture-dfg-zero-tie-reference.py", "dfg_zero_tie_gate_test")
            self.extended = load_script(
                "check-dfg-extended-support.py", "dfg_extended_gate_test")

    def test_scripts_pin_commit_and_import_without_pm4py(self):
        for module in (self.alignment, self.native, self.search, self.zero_tie, self.extended):
            self.assertEqual(module.REFERENCE_COMMIT, REFERENCE_COMMIT)

    def test_native_comparator_returns_nonzero_for_recorded_mismatch(self):
        class Classic:
            @staticmethod
            def apply_trace(*args, **kwargs):
                return None

            @staticmethod
            def apply_log(*args, **kwargs):
                return [None]

            @staticmethod
            def project_log_on_dfg(*args, **kwargs):
                return FakeEventLog()

            @staticmethod
            def project_alignments_on_dfg(*args, **kwargs):
                return {}, {}

        reference = types.SimpleNamespace(__version__="2.7.22")
        pandas = types.SimpleNamespace(__version__="3.0.5")
        api = (reference, pandas, Classic, FakeEvent, FakeEventLog, FakeTrace)
        native_line = json.dumps({
            "trace": None,
            "log": None,
            "projection": {"attributes": {}, "traces": []},
            "metrics": {"activities": {}, "edges": []},
        })
        with tempfile.TemporaryDirectory(prefix="dfg-native-gate-test-") as directory:
            output = Path(directory) / "report.json"
            native_path = Path(directory) / "native"
            native_path.write_bytes(b"native test executable")
            argv = ["capture-dfg-native-reference.py", "--native", str(native_path),
                    "--output", str(output), "--max-length", "0"]
            process = types.SimpleNamespace(stdout=native_line, stderr="", returncode=0)
            with patch.object(self.native, "_load_public_reference_api", return_value=api), \
                    patch.object(self.native.subprocess, "run", return_value=process), \
                    patch.object(self.native, "MODELS", {
                        "only": ({}, {}, {}, "a")}), \
                    patch.object(self.native, "SCENARIOS", {"default": {}}), \
                    patch.object(sys, "argv", argv):
                self.assertEqual(self.native.main(), 0)
            self.assertEqual(json.loads(output.read_text(encoding="utf-8"))["mismatches"], [])

            mismatch = json.dumps({
                "trace": {"cost": 1},
                "log": None,
                "projection": {"attributes": {}, "traces": []},
                "metrics": {"activities": {}, "edges": []},
            })
            process.stdout = mismatch
            with patch.object(self.native, "_load_public_reference_api", return_value=api), \
                    patch.object(self.native.subprocess, "run", return_value=process), \
                    patch.object(self.native, "MODELS", {
                        "only": ({}, {}, {}, "a")}), \
                    patch.object(self.native, "SCENARIOS", {"default": {}}), \
                    patch.object(sys, "argv", argv):
                self.assertEqual(self.native.main(), 1)
            report = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(len(report["mismatches"]), 1)
            self.assertEqual(report["reference_commit"], REFERENCE_COMMIT)
            self.assertIn("PYTHONHASHSEED", report["environment"])
            self.assertIn("TZ", report["environment"])

    def test_native_comparator_retains_matched_error_details(self):
        class Classic:
            @staticmethod
            def apply_trace(*args, **kwargs):
                raise ValueError("public trace failure")

            @staticmethod
            def apply_log(*args, **kwargs):
                raise ValueError("public log failure")

            @staticmethod
            def project_log_on_dfg(*args, **kwargs):
                raise ValueError("public projection failure")

            @staticmethod
            def project_alignments_on_dfg(*args, **kwargs):
                raise ValueError("public metrics failure")

        reference = types.SimpleNamespace(__version__="2.7.22")
        pandas = types.SimpleNamespace(__version__="3.0.5")
        api = (reference, pandas, Classic, FakeEvent, FakeEventLog, FakeTrace)
        native_line = json.dumps({
            operation: {"error": True, "type": "NativeError", "message": "native failure"}
            for operation in ("trace", "log", "projection", "metrics")})
        with tempfile.TemporaryDirectory(prefix="dfg-native-errors-test-") as directory:
            output = Path(directory) / "report.json"
            native_path = Path(directory) / "native"
            native_path.write_bytes(b"native test executable")
            argv = ["capture-dfg-native-reference.py", "--native", str(native_path),
                    "--output", str(output), "--max-length", "0"]
            process = types.SimpleNamespace(stdout=native_line, stderr="", returncode=0)
            with patch.object(self.native, "_load_public_reference_api", return_value=api), \
                    patch.object(self.native.subprocess, "run", return_value=process), \
                    patch.object(self.native, "MODELS", {
                        "only": ({}, {}, {}, "a")}), \
                    patch.object(self.native, "SCENARIOS", {"default": {}}), \
                    patch.object(sys, "argv", argv):
                self.assertEqual(self.native.main(), 0)
            report = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(len(report["matched_errors"]), 4)
            self.assertEqual(report["summary"]["trace"]["matched_errors"], 1)
            self.assertEqual(report["matched_errors"][0]["actual"]["type"], "NativeError")

    def test_search_comparator_returns_nonzero_for_recorded_mismatch(self):
        class Classic:
            @staticmethod
            def apply_trace(*args, **kwargs):
                return {"visited_states": -1, "closed": -1,
                        "internal_cost": -1, "cost": -1}

        reference = types.SimpleNamespace(__version__="2.7.22")
        pandas = types.SimpleNamespace(__version__="3.0.5")
        api = (reference, pandas, Classic, FakeTrace, FakeEvent)
        with tempfile.TemporaryDirectory(prefix="dfg-search-gate-test-") as directory:
            output = Path(directory) / "report.json"
            argv = ["capture-dfg-search-reference.py", "--output", str(output)]
            with patch.object(self.search, "_load_public_reference_api", return_value=api), \
                    patch.object(self.search.itertools, "product", return_value=[tuple()]), \
                    patch.object(sys, "argv", argv):
                self.assertEqual(self.search.main(), 1)
            report = json.loads(output.read_text(encoding="utf-8"))
            self.assertTrue(report["mismatches"])
            self.assertEqual(report["reference_commit"], REFERENCE_COMMIT)

    def test_zero_tie_gate_returns_nonzero_for_unshared_support(self):
        class Classic:
            @staticmethod
            def apply_trace(*args, **kwargs):
                return {"cost": 0, "internal_cost": 0, "alignment": []}

        reference = types.SimpleNamespace(__version__="2.7.22")
        pandas = types.SimpleNamespace(__version__="3.0.5")
        api = (reference, pandas, Classic, FakeEvent, FakeTrace)
        audit = types.SimpleNamespace(
            MODELS={}, alignment=lambda value: value)
        spec = types.SimpleNamespace(
            loader=types.SimpleNamespace(exec_module=lambda module: None))

        def run_zero_tie(output, mismatch):
            def native_run(*args, **kwargs):
                lines = []
                for index, _ in enumerate(kwargs["input"].splitlines()):
                    trace = {"cost": 1 if mismatch and index == 0 else 0,
                             "internal_cost": 0, "alignment": []}
                    lines.append(json.dumps({"trace": trace}))
                return types.SimpleNamespace(stdout="\n".join(lines), stderr="", returncode=0)

            native_path = output.with_name("native")
            native_path.write_bytes(b"native test executable")
            argv = ["capture-dfg-zero-tie-reference.py", "--native", str(native_path),
                    "--output", str(output), "--repetitions", "1"]
            with patch.object(self.zero_tie, "_load_public_reference_api", return_value=api), \
                    patch.object(self.zero_tie.importlib.util, "spec_from_file_location",
                                 return_value=spec), \
                    patch.object(self.zero_tie.importlib.util, "module_from_spec",
                                 return_value=audit), \
                    patch.object(self.zero_tie.itertools, "product", return_value=[tuple()]), \
                    patch.object(self.zero_tie.subprocess, "run", side_effect=native_run), \
                    patch.object(sys, "argv", argv):
                return self.zero_tie.main()

        with tempfile.TemporaryDirectory(prefix="dfg-zero-tie-gate-test-") as directory:
            output = Path(directory) / "report.json"
            self.assertEqual(run_zero_tie(output, mismatch=False), 0)
            report = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(report["cases_with_unshared_sampled_outcomes"], 0)
            self.assertEqual(report["reference_commit"], REFERENCE_COMMIT)
            self.assertIn("PYTHONHASHSEED", report["environment"])
            self.assertIn("TZ", report["environment"])

            self.assertEqual(run_zero_tie(output, mismatch=True), 1)
            report = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(report["cases_with_unshared_sampled_outcomes"], 1)
            self.assertTrue(report["rows"][0]["public_only"])
            self.assertTrue(report["rows"][0]["native_only"])

        self.assertEqual(self.zero_tie._support_exit_code(0, 0), 0)
        self.assertEqual(self.zero_tie._support_exit_code(0, 1), 1)

    def test_extended_gate_reports_support_difference_and_fails(self):
        class Classic:
            @staticmethod
            def apply_trace(*args, **kwargs):
                return {"cost": 0, "internal_cost": 0, "alignment": []}

            @staticmethod
            def apply_log(*args, **kwargs):
                return [None]

            @staticmethod
            def project_log_on_dfg(*args, **kwargs):
                return FakeEventLog()

            @staticmethod
            def project_alignments_on_dfg(*args, **kwargs):
                return {}, {}

        reference = types.SimpleNamespace(__version__="2.7.22")
        pandas = types.SimpleNamespace(__version__="3.0.5")
        api = (reference, pandas, Classic, FakeEvent, FakeEventLog, FakeTrace)

        def run_extended(output, native_trace):
            def native_run(*args, **kwargs):
                line = json.dumps({
                    "trace": native_trace,
                    "log": None,
                    "projection": {"attributes": {}, "traces": []},
                    "metrics": {"activities": {}, "edges": []},
                })
                count = len(kwargs["input"].splitlines())
                return types.SimpleNamespace(stdout="\n".join([line] * count),
                                             stderr="", returncode=0)

            argv = ["check-dfg-extended-support.py", "--reference", str(reference_path),
                    "--native", str(native_path), "--output", str(output),
                    "--repetitions", "1"]
            with patch.object(self.extended, "_load_capture_module",
                              return_value=self.native), \
                    patch.object(self.extended, "_load_public_reference_api",
                                 return_value=api), \
                    patch.object(self.extended.subprocess, "run", side_effect=native_run), \
                    patch.object(sys, "argv", argv):
                return self.extended.main()

        with tempfile.TemporaryDirectory(prefix="dfg-extended-gate-test-") as directory:
            directory = Path(directory)
            reference_path = directory / "reference.json"
            native_path = directory / "native"
            classic_module_path = directory / "classic.py"
            output = directory / "report.json"
            native_path.write_bytes(b"native test executable")
            classic_module_path.write_bytes(b"fake public reference module")
            Classic.__file__ = str(classic_module_path)
            reference_module = self.extended._module_metadata(Classic)
            reference_path.write_text(json.dumps({
                "reference": "2.7.22", "pandas": "3.0.5",
                "reference_commit": REFERENCE_COMMIT,
                "reference_commit_scope": "configured_reference_pin",
                "reference_module": reference_module,
                "mismatches": [{"model": "selfloop", "scenario": "zero",
                                 "word": "", "operation": "trace",
                                 "expected": {"cost": 1}, "actual": {"cost": 2}}],
            }), encoding="utf-8")
            expected = {"cost": 0, "internal_cost": 0, "alignment": []}
            self.assertEqual(run_extended(output, expected), 0)
            report = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(report["selected_rows"], 1)
            self.assertEqual(report["cases_with_unshared_sampled_outcomes"], 0)
            self.assertEqual(report["rows"][0]["expected"], {"cost": 1})
            self.assertEqual(report["native_executable"]["sha256"],
                             "b4fa9357dddd4be79bd2bdb61d70ed47a3a91e2c3e733d63bbddce08d5dc3f74")
            self.assertEqual(report["input_report_sha256"],
                             hashlib.sha256(reference_path.read_bytes()).hexdigest())

            self.assertEqual(run_extended(output, {"cost": 1, "internal_cost": 0,
                                                    "alignment": []}), 1)
            report = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(report["cases_with_unshared_sampled_outcomes"], 1)
            self.assertTrue(report["rows"][0]["public_only"])
            self.assertTrue(report["rows"][0]["native_only"])

    def test_extended_gate_rejects_reference_module_hash_mismatch(self):
        document = {"reference_module": {"sha256": "expected"}}
        loaded = {"path": "classic.py", "sha256": "actual"}
        with self.assertRaisesRegex(RuntimeError, "SHA256.*does not match"):
            self.extended._validate_reference_module(document, loaded)

    def test_extended_gate_rejects_unpinned_report(self):
        with tempfile.TemporaryDirectory(prefix="dfg-extended-metadata-test-") as directory:
            path = Path(directory) / "reference.json"
            path.write_text(json.dumps({"reference": "2.7.22", "pandas": "3.0.5",
                                        "mismatches": []}), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "reference commit"):
                self.extended._read_reference_report(path)

    def test_extended_gate_reads_gzipped_reference_report(self):
        with tempfile.TemporaryDirectory(prefix="dfg-extended-gzip-test-") as directory:
            path = Path(directory) / "reference.json.gz"
            document = {"reference": "2.7.22", "pandas": "3.0.5",
                        "reference_commit": REFERENCE_COMMIT,
                        "reference_commit_scope": "configured_reference_pin",
                        "mismatches": []}
            with gzip.open(path, "wt", encoding="utf-8") as target:
                json.dump(document, target)
            loaded, mismatches = self.extended._read_reference_report(path)
            self.assertEqual(loaded["reference_commit"], REFERENCE_COMMIT)
            self.assertEqual(mismatches, [])

    def test_extended_gate_retains_malformed_native_output_as_failure(self):
        cases = [(("selfloop", "zero", "", "trace"), {})]
        process = types.SimpleNamespace(stdout="not-json", stderr="", returncode=0)
        with patch.object(self.extended.subprocess, "run", return_value=process):
            counters, errors = self.extended._run_native(
                Path("native"), cases, 1, self.native)
        self.assertEqual(counters[0]["trace"], {})
        self.assertEqual(len(errors), 1)
        self.assertEqual(errors[0]["kind"], "malformed_native_output")

    def test_extended_gate_canonicalizes_integral_float_spellings_only(self):
        integer = {"value": 1, "nested": [True, 0, 1.5]}
        integral_float = {"value": 1.0, "nested": [True, 0.0, 1.5]}
        self.assertEqual(self.extended._outcome_key(integer),
                         self.extended._outcome_key(integral_float))
        self.assertNotEqual(self.extended._outcome_key(integer),
                            self.extended._outcome_key({"value": 1,
                                                        "nested": [1, 0, 1.5]}))
        records = [{"result": integer, "occurrences": 2},
                   {"result": integral_float, "occurrences": 3}]
        self.assertEqual(self.extended._rekey_counter(records)[
            self.extended._outcome_key(integer)], 5)
        saved = {"repetitions": 5, "rows": [{
            "model": "selfloop", "scenario": "zero", "word": "",
            "operation": "trace", "public": records,
            "native": [{"result": integral_float, "occurrences": 5}],
        }]}
        checked = self.extended.recheck_saved_report(saved)
        self.assertEqual(checked["cases_with_unshared_sampled_outcomes"], 0)
        self.assertEqual(checked["cases_with_frequency_difference_over_five_standard_errors"], 0)
        self.assertEqual(checked["numeric_outcome_recheck"],
                         "finite integral floats canonicalized to integers")

    def test_extended_gate_recheck_rejects_truncated_or_extra_counts(self):
        for side in ("public", "native"):
            for occurrences in (4, 6):
                with self.subTest(side=side, occurrences=occurrences):
                    records = [{"result": {"cost": 0}, "occurrences": 5}]
                    records[0]["occurrences"] = occurrences
                    saved = {"repetitions": 5, "rows": [{
                        "model": "selfloop", "scenario": "zero", "word": "",
                        "operation": "trace",
                        "public": [{"result": {"cost": 0}, "occurrences": 5}],
                        "native": [{"result": {"cost": 0}, "occurrences": 5}],
                    }]}
                    saved["rows"][0][side] = records
                    with self.assertRaisesRegex(
                            ValueError,
                            rf"row 0 {side} counter has {occurrences} occurrences; expected 5"):
                        self.extended.recheck_saved_report(saved)


if __name__ == "__main__":
    unittest.main()
