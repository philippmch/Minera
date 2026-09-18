import importlib.util
import hashlib
import json
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"
SCRIPT_NAMES = [
    "capture-xes-rust-conversion-extensions-reference.py",
    "capture-xes-rust-conversion-reference.py",
    "capture-xes-rust-conversion-schema-reference.py",
    "capture-xes-rust-empty-boundary-reference.py",
    "capture-xes-rust-grouping-reference.py",
    "capture-xes-rust-list-reference.py",
    "capture-xes-rust-metadata-reference.py",
    "capture-xes-rust-parameters-reference.py",
    "capture-xes-rust-raw-reference.py",
    "capture-xes-rust-scalar-reference.py",
    "capture-xes-rust-timezone-reference.py",
    "capture-xes-rust-unicode-reference.py",
    "check-xes-rust-raw-native.py",
    "check-xes-rust-sparse-schema.py",
]


def load_script(filename, suffix):
    path = ROOT / "scripts" / filename
    spec = importlib.util.spec_from_file_location("xes_rust_gate_%s" % suffix, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class RustXesEvidenceGateTests(unittest.TestCase):
    def test_all_scripts_import_without_public_dependencies(self):
        with patch.dict(sys.modules, {"pm4py": None, "pandas": None, "polars": None}):
            modules = [load_script(name, str(index)) for index, name in enumerate(SCRIPT_NAMES)]
        self.assertEqual(len(modules), len(SCRIPT_NAMES))
        for module in modules:
            self.assertEqual(module.REFERENCE_VERSION, "2.7.22")
            self.assertEqual(module.REFERENCE_COMMIT, REFERENCE_COMMIT)

    def test_pin_and_backend_checks_are_explicit(self):
        module = load_script("capture-xes-rust-scalar-reference.py", "pins")
        with self.assertRaisesRegex(RuntimeError, "requires pm4py"):
            module._require_version(types.SimpleNamespace(__version__="2.7.21"))
        with self.assertRaisesRegex(RuntimeError, "requires r4pm"):
            module._require_backend("0.6.1")

    def test_supported_capture_manifests_are_complete_and_unique(self):
        checker = load_script("check-xes-rust-raw-native.py", "manifest-table")
        producer_names = {
            "rustxes_raw_reference": "capture-xes-rust-raw-reference.py",
            "rustxes_conversion_reference": "capture-xes-rust-conversion-reference.py",
            "rustxes_conversion_schema_reference": "capture-xes-rust-conversion-schema-reference.py",
            "rustxes_conversion_extensions_reference": "capture-xes-rust-conversion-extensions-reference.py",
            "rustxes_empty_boundary_reference": "capture-xes-rust-empty-boundary-reference.py",
        }
        for capture_id, filename in producer_names.items():
            with self.subTest(capture_id=capture_id):
                producer = load_script(filename, capture_id)
                count, identities = checker.CAPTURE_MANIFESTS[capture_id]
                self.assertEqual(producer.CAPTURE_ID, capture_id)
                self.assertEqual(producer.EXPECTED_OBSERVATION_COUNT, count)
                self.assertEqual(tuple(producer.EXPECTED_OBSERVATION_IDENTITIES), identities)
                self.assertEqual(len(identities), count)
                self.assertEqual(len(set(identities)), count)

    def _reference_report(self, directory, capture_id="rustxes_raw_reference"):
        path = Path(directory) / "reference.json"
        checker = load_script("check-xes-rust-raw-native.py", "reference-manifest")
        observation_count, expected_ids = checker.CAPTURE_MANIFESTS[capture_id]
        observation_ids = list(expected_ids)
        path.write_text(json.dumps({
            "capture_id": capture_id,
            "expected_observation_count": observation_count,
            "expected_observation_identities": observation_ids,
            "reference": "2.7.22",
            "r4pm": "0.6.2",
            "reference_commit": REFERENCE_COMMIT,
            "reference_commit_scope": "configured_reference_pin",
            "reference_module": {"path": "opaque-public-module", "sha256": "0" * 64},
            "errors": [],
            "observations": [{
                "name": observation_id,
                "observation_id": observation_id,
                "case": "empty",
                "parameters": {"return_legacy_log_object": False,
                                "return_pl_lazyframe": False},
                "xml": "<log/>",
                "result": {"type": "DataFrame", "columns": [],
                           "dtypes": {}, "rows": [], "row_count": 0},
            } for observation_id in observation_ids],
        }), encoding="utf-8")
        return path

    def _read_reference_document(self, path):
        return json.loads(path.read_text(encoding="utf-8"))

    def _write_reference_document(self, path, document):
        path.write_text(json.dumps(document), encoding="utf-8")

    def test_raw_checker_rejects_invalid_module_digest(self):
        checker = load_script("check-xes-rust-raw-native.py", "raw-digest")
        with tempfile.TemporaryDirectory(prefix="xes-rust-raw-digest-") as directory:
            directory = Path(directory)
            reference = self._reference_report(directory)
            document = self._read_reference_document(reference)
            document["reference_module"]["sha256"] = "not-a-sha256"
            self._write_reference_document(reference, document)
            output = directory / "result.json"
            argv = ["check-xes-rust-raw-native.py", "--reference", str(reference),
                    "--native", str(directory / "missing-native"), "--output", str(output)]
            with patch.object(sys, "argv", argv):
                self.assertEqual(checker.main(), 1)
            report = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(report["errors"][0]["kind"], "invalid_reference_report")

    def test_raw_checker_rejects_nonstring_capture_id_and_all_skipped(self):
        checker = load_script("check-xes-rust-raw-native.py", "empty-gate")
        for malformed_id in (True, False):
            with self.subTest(malformed_id=malformed_id), tempfile.TemporaryDirectory() as directory:
                directory = Path(directory)
                reference = self._reference_report(directory)
                document = self._read_reference_document(reference)
                if malformed_id:
                    document["capture_id"] = []
                else:
                    for observation in document["observations"]:
                        observation["parameters"] = {"unmapped": True}
                self._write_reference_document(reference, document)
                native = directory / "native"
                native.write_bytes(b"unused executable")
                output = directory / "result.json"
                argv = ["check-xes-rust-raw-native.py", "--reference", str(reference),
                        "--native", str(native), "--output", str(output),
                        "--skip-unsupported-parameters"]
                with patch.object(sys, "argv", argv), patch.object(checker.subprocess, "run") as run:
                    self.assertEqual(checker.main(), 1)
                    run.assert_not_called()
                report = json.loads(output.read_text(encoding="utf-8"))
                expected = "invalid_reference_report" if malformed_id else "no_comparisons"
                self.assertEqual(report["errors"][0]["kind"], expected)

    def test_checker_rejects_empty_truncated_and_duplicate_observations(self):
        checker = load_script("check-xes-rust-raw-native.py", "manifest-errors")
        for capture_id in checker.CAPTURE_MANIFESTS:
            for mutation in ("empty", "truncated", "duplicate"):
                with self.subTest(capture_id=capture_id, mutation=mutation), tempfile.TemporaryDirectory(prefix="xes-rust-manifest-") as directory:
                    directory = Path(directory)
                    reference = self._reference_report(directory, capture_id)
                    document = self._read_reference_document(reference)
                    if mutation == "empty":
                        document["observations"] = []
                    elif mutation == "truncated":
                        document["observations"] = document["observations"][:-1]
                    else:
                        document["observations"][1]["observation_id"] = document["observations"][0]["observation_id"]
                    self._write_reference_document(reference, document)
                    output = directory / "result.json"
                    argv = ["check-xes-rust-raw-native.py", "--reference", str(reference),
                            "--native", str(directory / "missing-native"), "--output", str(output)]
                    with patch.object(sys, "argv", argv):
                        self.assertEqual(checker.main(), 1)
                    report = json.loads(output.read_text(encoding="utf-8"))
                    self.assertEqual(report["errors"][0]["kind"], "invalid_reference_report")

    def test_checker_rejects_reference_capture_errors(self):
        checker = load_script("check-xes-rust-raw-native.py", "capture-errors")
        for capture_id in checker.CAPTURE_MANIFESTS:
            for field in ("errors", "validation_errors"):
                with self.subTest(capture_id=capture_id, field=field), tempfile.TemporaryDirectory(prefix="xes-rust-capture-errors-") as directory:
                    directory = Path(directory)
                    reference = self._reference_report(directory, capture_id)
                    document = self._read_reference_document(reference)
                    document[field] = ["capture failed"]
                    self._write_reference_document(reference, document)
                    output = directory / "result.json"
                    argv = ["check-xes-rust-raw-native.py", "--reference", str(reference),
                            "--native", str(directory / "missing-native"), "--output", str(output)]
                    with patch.object(sys, "argv", argv):
                        self.assertEqual(checker.main(), 1)
                    report = json.loads(output.read_text(encoding="utf-8"))
                    self.assertEqual(report["errors"][0]["kind"], "invalid_reference_report")

    def test_raw_checker_retains_malformed_output_and_fails(self):
        checker = load_script("check-xes-rust-raw-native.py", "raw")
        with tempfile.TemporaryDirectory(prefix="xes-rust-raw-gate-") as directory:
            directory = Path(directory)
            reference = self._reference_report(directory)
            native = directory / "native"
            native.write_bytes(b"native fixture")
            output = directory / "result.json"
            process = types.SimpleNamespace(stdout="not-json", stderr="diagnostic", returncode=0)
            argv = ["check-xes-rust-raw-native.py", "--reference", str(reference),
                    "--native", str(native), "--output", str(output), "--samples", "1"]
            with patch.object(checker.subprocess, "run", return_value=process), \
                    patch.object(sys, "argv", argv):
                self.assertEqual(checker.main(), 1)
            report = json.loads(output.read_text(encoding="utf-8"))
            self.assertTrue(report["errors"])
            self.assertTrue(any(item.get("actual", {}).get("native_malformed_output")
                                for item in report["checks"]))
            self.assertEqual(report["native_executable"]["sha256"],
                             hashlib.sha256(b"native fixture").hexdigest())

    def test_raw_checker_rejects_nonzero_native_process(self):
        checker = load_script("check-xes-rust-raw-native.py", "raw-process")
        with tempfile.TemporaryDirectory(prefix="xes-rust-raw-process-") as directory:
            directory = Path(directory)
            reference = self._reference_report(directory)
            native = directory / "native"
            native.write_bytes(b"native fixture")
            output = directory / "result.json"
            process = types.SimpleNamespace(stdout="partial", stderr="process failed", returncode=9)
            argv = ["check-xes-rust-raw-native.py", "--reference", str(reference),
                    "--native", str(native), "--output", str(output), "--samples", "1"]
            with patch.object(checker.subprocess, "run", return_value=process), \
                    patch.object(sys, "argv", argv):
                self.assertEqual(checker.main(), 1)
            report = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(report["checks"][0]["actual"]["returncode"], 9)
            self.assertIn("process failed", report["checks"][0]["actual"]["stderr"])

    def test_raw_checker_rejects_native_binary_change(self):
        checker = load_script("check-xes-rust-raw-native.py", "raw-native-change")
        with tempfile.TemporaryDirectory(prefix="xes-rust-raw-native-change-") as directory:
            directory = Path(directory)
            reference = self._reference_report(directory)
            native = directory / "native"
            native.write_bytes(b"native fixture")
            output = directory / "result.json"
            process = types.SimpleNamespace(stdout="not-json", stderr="diagnostic", returncode=0)
            snapshots = [
                {"path": str(native.resolve()), "sha256": "0" * 64},
                {"path": str(native.resolve()), "sha256": "1" * 64},
            ]
            argv = ["check-xes-rust-raw-native.py", "--reference", str(reference),
                    "--native", str(native), "--output", str(output), "--samples", "1"]
            with patch.object(checker, "_native_metadata", side_effect=snapshots), \
                    patch.object(checker.subprocess, "run", return_value=process), \
                    patch.object(sys, "argv", argv):
                self.assertEqual(checker.main(), 1)
            report = json.loads(output.read_text(encoding="utf-8"))
            self.assertTrue(report["native_executable"]["changed"])
            self.assertTrue(any(error["kind"] == "native_executable_changed"
                                for error in report["errors"]))

    def test_sparse_checker_retains_malformed_native_output(self):
        checker = load_script("check-xes-rust-sparse-schema.py", "sparse")
        with tempfile.TemporaryDirectory(prefix="xes-rust-sparse-gate-") as directory:
            directory = Path(directory)
            native = directory / "native"
            native.write_bytes(b"native fixture")
            output = directory / "result.json"
            process = types.SimpleNamespace(stdout="not-json", stderr="sparse diagnostic", returncode=0)
            argv = ["check-xes-rust-sparse-schema.py", "--native", str(native),
                    "--traces", "1", "--output", str(output)]
            with patch.object(checker.subprocess, "run", return_value=process), \
                    patch.object(sys, "argv", argv):
                self.assertEqual(checker.main(), 1)
            report = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(report["errors"][0]["kind"], "malformed_native_output")
            self.assertEqual(report["errors"][0]["stdout"], "not-json")

    def test_sparse_checker_hash_error_is_failure(self):
        checker = load_script("check-xes-rust-sparse-schema.py", "sparse-hash")
        with tempfile.TemporaryDirectory(prefix="xes-rust-sparse-hash-") as directory:
            directory = Path(directory)
            native = directory / "native"
            native.write_bytes(b"native fixture")
            output = directory / "result.json"
            process = types.SimpleNamespace(
                stdout=json.dumps({"log": {"traces": [{
                    "case_id": "c0",
                    "attributes": [{"key": "p0", "value": "v0"}],
                    "events": [{"activity": "A"}],
                }]}}), stderr="", returncode=0)
            hash_error = {"path": str(native.resolve()), "sha256": None,
                          "error": "hash unavailable"}
            argv = ["check-xes-rust-sparse-schema.py", "--native", str(native),
                    "--traces", "1", "--output", str(output)]
            with patch.object(checker, "_file_metadata", side_effect=[hash_error, hash_error]), \
                    patch.object(checker.subprocess, "run", return_value=process), \
                    patch.object(sys, "argv", argv):
                self.assertEqual(checker.main(), 1)
            report = json.loads(output.read_text(encoding="utf-8"))
            self.assertTrue(any(error["kind"] == "native_hash_error"
                                for error in report["errors"]))

    def test_native_integrity_helpers_reject_binary_change(self):
        for filename, suffix in (
                ("capture-xes-rust-grouping-reference.py", "grouping-change"),
                ("capture-xes-rust-list-reference.py", "list-change"),
                ("capture-xes-rust-metadata-reference.py", "metadata-change"),
                ("capture-xes-rust-scalar-reference.py", "scalar-change"),
                ("capture-xes-rust-timezone-reference.py", "timezone-change"),
                ("capture-xes-rust-unicode-reference.py", "unicode-change")):
            with self.subTest(filename=filename):
                module = load_script(filename, suffix)
                before = {"path": "native", "sha256": "0" * 64}
                after = {"path": "native", "sha256": "1" * 64}
                metadata, errors = module._native_integrity(before, after)
                self.assertTrue(metadata["changed"])
                self.assertEqual(errors[0]["kind"], "native_executable_changed")

    def test_unicode_native_hash_error_is_reported_as_failure(self):
        module = load_script("capture-xes-rust-unicode-reference.py", "unicode-integrity")
        before = {"path": "native", "sha256": None, "error": "hash unavailable"}
        after = {"path": "native", "sha256": None, "error": "hash unavailable"}
        _, errors = module._native_integrity(before, after)
        self.assertTrue(errors)
        self.assertEqual({error["phase"] for error in errors}, {"before", "after"})


if __name__ == "__main__":
    unittest.main()
