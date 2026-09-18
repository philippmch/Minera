"""Offline contract tests for the public RUSTXES gzip checker."""

import gzip
import argparse
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


def load_checker():
    spec = importlib.util.spec_from_file_location(
        "xes_rust_public_gzip_test", SCRIPTS / "check-xes-rust-public-gzip.py"
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class PublicGzipCheckerTests(unittest.TestCase):
    def setUp(self):
        with patch.dict(
            sys.modules,
            {"pm4py": None, "pandas": None, "polars": None},
        ):
            self.checker = load_checker()

    def fake_api(self, apply):
        variant = types.SimpleNamespace(value=types.SimpleNamespace(apply=apply))

        class Importer:
            Variants = types.SimpleNamespace(RUSTXES=variant)
            __file__ = __file__

            @staticmethod
            def apply(path, variant=None, parameters=None):
                return apply(path, variant=variant, parameters=parameters)

        class PM4Py:
            __version__ = "2.7.22"
            __file__ = __file__

        return PM4Py, Importer, None, None, None

    def run_checker(
        self,
        document,
        api,
        capture,
        support_samples=None,
        skip_unsupported=False,
    ):
        with tempfile.TemporaryDirectory(prefix="xes-public-gzip-test-") as directory:
            directory = Path(directory)
            reference = directory / "reference.json"
            output = directory / "report.json"
            document = dict(document)
            document.setdefault("reference_commit", REFERENCE_COMMIT)
            document.setdefault(
                "reference_commit_scope", self.checker.REFERENCE_COMMIT_SCOPE
            )
            document.setdefault("r4pm", self.checker.BACKEND_VERSION)
            document.setdefault("reference_module", {"sha256": "0" * 64})
            document.setdefault("errors", [])
            reference.write_text(json.dumps(document), encoding="utf-8")
            argv = [
                "check-xes-rust-public-gzip.py",
                "--reference",
                str(reference),
                "--output",
                str(output),
            ]
            if support_samples is not None:
                argv.extend(["--support-samples", str(support_samples)])
            if skip_unsupported:
                argv.append("--skip-unsupported-parameters")
            with patch.object(
                self.checker, "_load_public_reference_api", return_value=api
            ), patch.object(self.checker, "capture_value", side_effect=capture), patch.object(
                self.checker, "_safe_package_version", return_value=self.checker.BACKEND_VERSION
            ), patch.object(sys, "argv", argv
            ):
                code = self.checker.main()
            return code, json.loads(output.read_text(encoding="utf-8"))

    def test_import_is_offline_and_pin_is_explicit(self):
        self.assertIsNone(self.checker.pm4py)
        self.assertEqual(self.checker.REFERENCE_VERSION, "2.7.22")
        self.assertEqual(self.checker.REFERENCE_COMMIT, REFERENCE_COMMIT)
        self.assertEqual(
            self.checker.REFERENCE_COMMIT_SCOPE, "configured_reference_pin"
        )
        with self.assertRaisesRegex(RuntimeError, "requires pm4py"):
            self.checker._require_version(types.SimpleNamespace(__version__="2.7.21"))
        with self.assertRaisesRegex(RuntimeError, "requires r4pm"):
            self.checker._require_backend("0.6.1")

    def test_reconstructs_only_explicit_extension_markers(self):
        member = types.SimpleNamespace(
            value=("Concept", "concept", "http://example/concept.xesext")
        )
        enum_type = types.SimpleNamespace(Concept=member)
        reference = {
            "public_xes_extension_enum": {
                "members": {
                    "Concept": [
                        "Concept",
                        "concept",
                        "http://example/concept.xesext",
                    ]
                }
            }
        }
        decoded = self.checker.decode_parameters(
            {
                "extensions": [
                    "XESExtension.Concept",
                    {"SimpleNamespace": {"name": "Custom", "prefix": "c", "uri": "u"}},
                ]
            },
            reference,
            enum_type,
        )
        self.assertIs(decoded["extensions"][0], member)
        self.assertEqual(vars(decoded["extensions"][1]), {"name": "Custom", "prefix": "c", "uri": "u"})
        with self.assertRaisesRegex(self.checker.UnsupportedParameter, "clear public"):
            self.checker.decode_parameters(
                {"extensions": ["XESExtension.Concept"]}, {}, enum_type
            )

    def test_gzip_success_and_input_hash_are_recorded(self):
        expected = {
            "type": "EventLog",
            "module": "fake.log",
            "traces": [],
        }
        seen = []

        def apply(path, **kwargs):
            with gzip.open(path, "rb") as source:
                seen.append(source.read())
            return object()

        api = self.fake_api(apply)
        code, report = self.run_checker(
            {
                "reference": "2.7.22",
                "reference_commit": REFERENCE_COMMIT,
                "rows": [
                    {
                        "name": "plain_success",
                        "parameters": {"return_legacy_log_object": True},
                        "xml": "<log/>",
                        "result": expected,
                    }
                ],
            },
            api,
            lambda value, include_metadata=False, include_row_count=False: expected,
        )
        self.assertEqual(code, 0)
        self.assertEqual(seen, [b"<log/>"])
        self.assertEqual(
            report["summary"],
            {
                "checked": 1,
                "matched": 1,
                "failed": 0,
                "sampled_support_matches": 0,
                "unresolved": 0,
                "skipped": 0,
                "malformed": 0,
            },
        )
        self.assertEqual(report["checks"][0]["input"]["uncompressed_bytes"], 6)
        self.assertEqual(report["checks"][0]["input"]["compressed_bytes"] > 0, True)
        self.assertTrue(report["native_independent"])

    def test_error_diagnostics_match_and_mismatch_is_reported(self):
        class PanicException(Exception):
            pass

        def apply(path, **kwargs):
            raise PanicException("not yet implemented: Empty event not handled?!")

        api = self.fake_api(apply)
        expected_error = {
            "status": "exception",
            "exception": "PanicException",
            "message": "not yet implemented: Empty event not handled?!",
            "panic_markers": ["not yet implemented: Empty event not handled?!"],
        }
        code, report = self.run_checker(
            {
                "reference": "2.7.22",
                "rows": [
                    {
                        "case": "empty_event",
                        "parameters": {},
                        "xml": "<log/>",
                        "result": expected_error,
                    }
                ],
            },
            api,
            lambda value, include_metadata=False, include_row_count=False: value,
        )
        self.assertEqual(code, 0)
        self.assertEqual(report["checks"][0]["actual"], expected_error)

        success = {"type": "EventLog", "module": "fake.log", "traces": []}
        mismatch_code, mismatch = self.run_checker(
            {
                "reference": "2.7.22",
                "rows": [
                    {
                        "name": "mismatch",
                        "parameters": {},
                        "xml": "<log/>",
                        "result": success,
                    },
                    {"name": "malformed"},
                ],
            },
            self.fake_api(lambda path, **kwargs: object()),
            lambda value, include_metadata=False, include_row_count=False: {
                "type": "EventLog",
                "module": "different.log",
                "traces": [],
            },
        )
        self.assertEqual(mismatch_code, 1)
        self.assertEqual(mismatch["summary"]["failed"], 1)
        self.assertEqual(mismatch["summary"]["malformed"], 1)
        self.assertEqual(mismatch["checks"][0]["difference"]["path"], "$.module")
        self.assertEqual(mismatch["malformed"][0]["name"], "malformed")

    def test_support_sampling_classifies_without_erasing_strict_failure(self):
        expected = {"type": "EventLog", "module": "fake.log", "traces": [{"id": "A"}]}
        alternate = {"type": "EventLog", "module": "fake.log", "traces": [{"id": "B"}]}
        outcomes = iter([alternate, expected, alternate, alternate, expected])

        api = self.fake_api(lambda path, **kwargs: object())
        code, report = self.run_checker(
            {
                "reference": "2.7.22",
                "rows": [
                    {
                        "name": "generated_column_order",
                        "parameters": {},
                        "xml": "<log/>",
                        "result": expected,
                    }
                ],
            },
            api,
            lambda value, include_metadata=False, include_row_count=False: next(outcomes),
            support_samples=2,
        )
        check = report["checks"][0]
        self.assertEqual(code, 0)
        self.assertFalse(check["match"])
        self.assertTrue(check["sampled_support_match"])
        self.assertEqual(check["classification"], "sampled_support_match")
        self.assertTrue(check["support_sampling"]["support_equal"])
        self.assertTrue(
            check["support_sampling"]["historical_expected_in_plain_support"]
        )
        self.assertTrue(
            check["support_sampling"]["historical_expected_in_gzip_support"]
        )
        self.assertTrue(
            check["support_sampling"]["initial_gzip_outcome_in_plain_support"]
        )
        self.assertTrue(
            check["support_sampling"]["initial_gzip_outcome_in_gzip_support"]
        )
        self.assertEqual(report["summary"]["failed"], 1)
        self.assertEqual(report["summary"]["unresolved"], 0)
        self.assertEqual(report["summary"]["sampled_support_matches"], 1)

    def test_support_sampling_rejects_initial_gzip_outcome_missing_from_support(self):
        expected = {"type": "EventLog", "module": "fake.log", "traces": [{"id": "A"}]}
        alternate = {"type": "EventLog", "module": "fake.log", "traces": [{"id": "B"}]}
        # The initial gzip replay returns B, then every sampled replay returns A.
        outcomes = iter([alternate, expected, expected, expected, expected])
        code, report = self.run_checker(
            {
                "reference": "2.7.22",
                "rows": [
                    {
                        "name": "initial_outcome_not_supported",
                        "parameters": {},
                        "xml": "<log/>",
                        "result": expected,
                    }
                ],
            },
            self.fake_api(lambda path, **kwargs: object()),
            lambda value, include_metadata=False, include_row_count=False: next(outcomes),
            support_samples=2,
        )
        check = report["checks"][0]
        self.assertEqual(code, 1)
        self.assertFalse(check["sampled_support_match"])
        self.assertTrue(check["support_sampling"]["support_equal"])
        self.assertFalse(
            check["support_sampling"]["initial_gzip_outcome_in_gzip_support"]
        )
        self.assertEqual(report["summary"]["unresolved"], 1)

    def test_unsupported_parameters_fail_unless_explicitly_skipped(self):
        unsupported = {
            "name": "unsupported",
            "parameters": {"extensions": ["XESExtension.Missing"]},
            "xml": "<log/>",
            "result": {"type": "EventLog", "module": "fake.log", "traces": []},
        }
        valid = {
            "name": "valid",
            "parameters": {},
            "xml": "<log/>",
            "result": {"type": "EventLog", "module": "fake.log", "traces": []},
        }

        code, report = self.run_checker(
            {"reference": "2.7.22", "rows": [unsupported]},
            self.fake_api(lambda path, **kwargs: object()),
            lambda value, include_metadata=False, include_row_count=False: valid["result"],
        )
        self.assertEqual(code, 1)
        self.assertEqual(report["summary"]["checked"], 0)
        self.assertEqual(report["summary"]["skipped"], 1)

        code, report = self.run_checker(
            {"reference": "2.7.22", "rows": [unsupported, valid]},
            self.fake_api(lambda path, **kwargs: object()),
            lambda value, include_metadata=False, include_row_count=False: valid["result"],
            skip_unsupported=True,
        )
        self.assertEqual(code, 0)
        self.assertEqual(report["summary"]["checked"], 1)
        self.assertEqual(report["summary"]["skipped"], 1)

        code, report = self.run_checker(
            {"reference": "2.7.22", "rows": [unsupported]},
            self.fake_api(lambda path, **kwargs: object()),
            lambda value, include_metadata=False, include_row_count=False: valid["result"],
            skip_unsupported=True,
        )
        self.assertEqual(code, 1)
        self.assertEqual(report["summary"]["checked"], 0)

    def test_reference_provenance_and_optional_manifest_are_gated(self):
        expected = {"type": "EventLog", "module": "fake.log", "traces": []}
        base = {
            "reference": "2.7.22",
            "rows": [
                {
                    "observation_id": "only",
                    "parameters": {},
                    "xml": "<log/>",
                    "result": expected,
                }
            ],
            "expected_observation_count": 1,
            "expected_observation_identities": ["only"],
        }
        code, report = self.run_checker(
            base,
            self.fake_api(lambda path, **kwargs: object()),
            lambda value, include_metadata=False, include_row_count=False: expected,
        )
        self.assertEqual(code, 0)
        self.assertEqual(report["summary"]["checked"], 1)

        for field, value in (
            ("reference_commit", "wrong"),
            ("reference_commit_scope", "wrong"),
            ("r4pm", "0.6.1"),
            ("reference_module", {"sha256": "bad"}),
            ("errors", ["capture failed"]),
            ("expected_observation_count", 2),
            ("expected_observation_identities", ["duplicate", "duplicate"]),
        ):
            with self.subTest(field=field):
                document = dict(base)
                document[field] = value
                code, report = self.run_checker(
                    document,
                    self.fake_api(lambda path, **kwargs: object()),
                    lambda value, include_metadata=False, include_row_count=False: expected,
                )
                self.assertEqual(code, 1)
                self.assertTrue(report["malformed"])

    def test_support_summary_retains_missing_outcomes_and_exact_counts(self):
        summary = self.checker._support_summary(
            {"value": "missing"},
            [{"value": "plain"}, {"value": "plain"}],
            [{"value": "gzip"}, {"value": "gzip"}],
        )
        self.assertFalse(summary["historical_expected_in_plain_support"])
        self.assertFalse(summary["support_equal"])
        self.assertEqual(summary["plain"]["outcomes"], [{"count": 2, "outcome": {"value": "plain"}}])
        self.assertEqual(summary["gzip"]["outcomes"], [{"count": 2, "outcome": {"value": "gzip"}}])

    def test_support_sample_count_must_be_positive(self):
        for value in ("0", "-1", "not-an-integer"):
            with self.assertRaises(argparse.ArgumentTypeError):
                self.checker._positive_int(value)


if __name__ == "__main__":
    unittest.main()
