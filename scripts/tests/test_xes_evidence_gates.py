"""Focused, offline checks for the XES evidence capture gates."""
import hashlib
import importlib.util
import json
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest.mock import patch


SCRIPTS = Path(__file__).resolve().parents[1]
REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"
SCRIPT_NAMES = (
    "capture-xes-null-reference.py",
    "capture-xes-boolean-reference.py",
    "capture-xes-date-reference.py",
    "capture-xes-empty-wrapper-import.py",
    "capture-xes-scalar-children-reference.py",
    "capture-xes-line-reference.py",
)


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


class XesEvidenceGateTests(unittest.TestCase):
    def setUp(self):
        with patch.dict(sys.modules, {"pm4py": None, "pandas": None}):
            self.modules = {
                name: load_script(filename, f"xes_gate_{name}")
                for name, filename in enumerate(SCRIPT_NAMES)
            }
        (self.null, self.boolean, self.date, self.empty,
         self.scalar, self.line) = self.modules.values()

    def test_scripts_import_without_pinned_dependencies_and_pin_explicitly(self):
        for module in self.modules.values():
            self.assertEqual(module.REFERENCE_COMMIT, REFERENCE_COMMIT)
            self.assertEqual(module.REFERENCE_COMMIT_SCOPE, "configured_reference_pin")
        with self.assertRaisesRegex(RuntimeError, "requires pm4py"):
            self.null._require_version(types.SimpleNamespace(__version__="other"))
        with self.assertRaisesRegex(RuntimeError, "requires pm4py"):
            self.date._require_versions(
                types.SimpleNamespace(__version__="other"),
                types.SimpleNamespace(__version__="3.0.5"))

    def _scalar_api(self):
        variant = types.SimpleNamespace(name="FAKE")

        class Importer:
            Variants = [variant]

            @staticmethod
            def deserialize(*args, **kwargs):
                return [[{"probe": True}]]

        class Exporter:
            Variants = [variant]

            @staticmethod
            def serialize(*args, **kwargs):
                return (b"<log><trace><event>"
                        b'<boolean key="probe" value="true"/>'
                        b"</event></trace></log>")

        reference = types.SimpleNamespace(__version__="2.7.22")
        return reference, Exporter, Importer, FakeEvent, FakeEventLog, FakeTrace

    def test_scalar_native_malformed_output_fails_and_is_retained(self):
        process = types.SimpleNamespace(stdout="not-json", stderr="", returncode=0)
        with tempfile.TemporaryDirectory(prefix="xes-scalar-gate-test-") as directory:
            directory = Path(directory)
            output = directory / "report.json"
            native = directory / "native"
            native.write_bytes(b"native executable")
            argv = ["capture-xes-scalar-children-reference.py", "--output",
                    str(output), "--native", str(native)]
            with patch.object(self.scalar, "_load_public_reference_api",
                              return_value=self._scalar_api()), \
                    patch.object(self.scalar.subprocess, "run", return_value=process), \
                    patch.object(sys, "argv", argv):
                self.assertEqual(self.scalar.main(), 1)
            report = json.loads(output.read_text(encoding="utf-8"))
            self.assertTrue(report["errors"])
            self.assertEqual(report["errors"][0]["kind"], "native_malformed_output")
            self.assertTrue(report["imports"][0]["native"]["error"])
            self.assertEqual(
                report["native_executable"]["sha256"],
                hashlib.sha256(b"native executable").hexdigest())

    def test_line_native_mismatch_fails_and_full_rows_remain(self):
        variant = types.SimpleNamespace(name="LINE_BY_LINE")

        class Importer:
            Variants = types.SimpleNamespace(LINE_BY_LINE=variant)

            @staticmethod
            def deserialize(*args, **kwargs):
                return FakeEventLog(
                    [FakeTrace([FakeEvent({"public": "value"})],
                               attributes={"trace": "public"})],
                    attributes={"log": "public"})

        reference = types.SimpleNamespace(__version__="2.7.22")
        api = (reference, Importer)
        process = types.SimpleNamespace(
            stdout=json.dumps({"log": {"attributes": [], "traces": []}}),
            stderr="", returncode=0)
        with tempfile.TemporaryDirectory(prefix="xes-line-gate-test-") as directory:
            directory = Path(directory)
            output = directory / "report.json"
            native = directory / "native"
            native.write_bytes(b"native executable")
            argv = ["capture-xes-line-reference.py", "--output", str(output),
                    "--native", str(native)]
            with patch.object(self.line, "_load_public_reference_api", return_value=api), \
                    patch.object(self.line, "fixtures", return_value={"one": "<log/>"}), \
                    patch.object(self.line.subprocess, "run", return_value=process), \
                    patch.object(sys, "argv", argv):
                self.assertEqual(self.line.main(), 1)
            report = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(len(report["rows"]), 2)
            self.assertFalse(report["rows"][0]["matches"])
            self.assertEqual(report["errors"], [])
            self.assertIn("native_executable", report)

    def test_line_native_decoder_keeps_event_attribute_maps_flat(self):
        process = types.SimpleNamespace(
            stdout=json.dumps({"log": {
                "attributes": [{"key": "log", "value": "L"}],
                "traces": [{
                    "attributes": [{"key": "trace", "value": "T"}],
                    "events": [{"attributes": [
                        {"key": "a", "value": "A"},
                        {"key": "b", "value": "B"},
                    ]}],
                }],
            }}),
            stderr="", returncode=0)
        decoded, error = self.line._decode_native_import(process)
        self.assertIsNone(error)
        self.assertEqual(decoded, {
            "attributes": {"log": "L"},
            "traces": [{"attributes": {"trace": "T"},
                         "events": [{"a": "A", "b": "B"}]}],
        })

    def test_native_comparison_outputs_use_explicit_provenance(self):
        for module in (self.scalar, self.line):
            self.assertEqual(module.REFERENCE_VERSION, "2.7.22")
            self.assertIn("PYTHONHASHSEED", module._environment())
            self.assertIn("TZ", module._environment())


if __name__ == "__main__":
    unittest.main()
