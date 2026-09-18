"""Capture public RUSTXES empty trace/event boundary behavior."""

import argparse
import datetime
import hashlib
import inspect
import json
import math
import os
import subprocess
import sys
import tempfile
from importlib.metadata import version
from pathlib import Path
import warnings


REFERENCE_VERSION = "2.7.22"
BACKEND_VERSION = "0.6.2"
REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"
REFERENCE_COMMIT_SCOPE = "configured_reference_pin"
pm4py = importer = None


def _load_public_reference_api():
    global pm4py, importer
    warnings.filterwarnings("ignore")
    import pm4py as pm4py_module
    from pm4py.objects.log.importer.xes import importer as importer_module
    pm4py, importer = pm4py_module, importer_module
    return pm4py, importer


def _require_version(pm4py_module):
    actual = getattr(pm4py_module, "__version__", None)
    if actual != REFERENCE_VERSION:
        raise RuntimeError(
            f"This oracle requires pm4py=={REFERENCE_VERSION}; got {actual!r}")


def _require_backend(actual):
    if actual != BACKEND_VERSION:
        raise RuntimeError(f"This oracle requires r4pm=={BACKEND_VERSION}; got {actual!r}")


def _environment():
    return {"PYTHONHASHSEED": os.environ.get("PYTHONHASHSEED"),
            "TZ": os.environ.get("TZ")}


def _file_metadata(path):
    resolved = Path(path).resolve()
    digest = hashlib.sha256()
    try:
        with resolved.open("rb") as source:
            for chunk in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(chunk)
    except OSError as error:
        raise RuntimeError(f"Unable to hash {resolved}: {error}") from error
    return {"path": str(resolved), "sha256": digest.hexdigest()}


def _module_metadata(module):
    filename = getattr(module, "__file__", None)
    return _file_metadata(filename) if filename else None


def normalize(value):
    if isinstance(value, dict):
        return {str(key): normalize(value[key]) for key in sorted(value, key=str)}
    if isinstance(value, (list, tuple)):
        return [normalize(item) for item in value]
    if isinstance(value, (datetime.datetime, datetime.date)):
        return {"date": value.isoformat()}
    if isinstance(value, float) and not math.isfinite(value):
        return {"float": "nan" if math.isnan(value) else "inf" if value > 0 else "-inf"}
    item = getattr(value, "item", None)
    if item is not None and not isinstance(value, (str, bytes)):
        try:
            return normalize(item())
        except (TypeError, ValueError):
            pass
    return value


def capture_value(value):
    kind = type(value).__name__
    result = {"type": kind, "module": type(value).__module__}
    if kind == "EventLog":
        result["traces"] = [
            {
                "attributes": normalize(dict(trace.attributes)),
                "events": [normalize(dict(event)) for event in trace],
            }
            for trace in value
        ]
        return result
    if kind == "DataFrame":
        result["columns"] = sorted(str(column) for column in value.columns)
        result["dtypes"] = {
            str(column): str(dtype)
            for column, dtype in sorted(value.dtypes.items(), key=lambda item: str(item[0]))
        }
        result["rows"] = [normalize(row) for row in value.to_dict(orient="records")]
        return result
    if kind == "LazyFrame":
        materialized = value.collect()
        result["columns"] = sorted(str(column) for column in materialized.columns)
        result["dtypes"] = {
            str(column): str(dtype)
            for column, dtype in sorted(materialized.schema.items(), key=lambda item: str(item[0]))
        }
        result["rows"] = [normalize(row) for row in materialized.to_dicts()]
        return result
    raise AssertionError(f"Unexpected return type: {kind}")


CASES = {
    "single_selfclosing_trace": "<log><trace/></log>",
    "single_explicit_empty_trace": "<log><trace></trace></log>",
    "explicit_empty_then_selfclosing": "<log><trace></trace><trace/></log>",
    "selfclosing_before_live": "<log><trace/><trace><string key=\"concept:name\" value=\"live\"/><event><string key=\"concept:name\" value=\"A\"/></event></trace></log>",
    "selfclosing_after_live": "<log><trace><string key=\"concept:name\" value=\"live\"/><event><string key=\"concept:name\" value=\"A\"/></event></trace><trace/></log>",
    "two_selfclosing_after_live": "<log><trace><string key=\"concept:name\" value=\"live\"/><event><string key=\"concept:name\" value=\"A\"/></event></trace><trace/><trace/></log>",
    "explicit_empty_before_live": "<log><trace></trace><trace><string key=\"concept:name\" value=\"live\"/><event><string key=\"concept:name\" value=\"A\"/></event></trace></log>",
    "explicit_empty_after_live": "<log><trace><string key=\"concept:name\" value=\"live\"/><event><string key=\"concept:name\" value=\"A\"/></event></trace><trace></trace></log>",
    "named_empty_trace": "<log><trace><string key=\"concept:name\" value=\"named\"/></trace></log>",
    "named_empty_then_selfclosing": "<log><trace><string key=\"concept:name\" value=\"named\"/></trace><trace/></log>",
    "named_empty_before_live": "<log><trace><string key=\"concept:name\" value=\"named\"/></trace><trace><string key=\"concept:name\" value=\"live\"/><event><string key=\"concept:name\" value=\"A\"/></event></trace></log>",
    "named_empty_after_live": "<log><trace><string key=\"concept:name\" value=\"live\"/><event><string key=\"concept:name\" value=\"A\"/></event></trace><trace><string key=\"concept:name\" value=\"named\"/></trace></log>",
    "selfclosing_event": "<log><trace><string key=\"concept:name\" value=\"named\"/><event/></trace></log>",
    "explicit_empty_event": "<log><trace><string key=\"concept:name\" value=\"named\"/><event></event></trace></log>",
}


MODES = {
    "legacy": {},
    "dataframe": {"return_legacy_log_object": False, "return_pl_lazyframe": False},
    "lazy": {"return_legacy_log_object": False, "return_pl_lazyframe": True},
}


CAPTURE_ID = "rustxes_empty_boundary_reference"
EXPECTED_OBSERVATION_COUNT = 42
EXPECTED_OBSERVATION_IDENTITIES = (
    "single_selfclosing_trace:legacy", "single_selfclosing_trace:dataframe", "single_selfclosing_trace:lazy",
    "single_explicit_empty_trace:legacy", "single_explicit_empty_trace:dataframe", "single_explicit_empty_trace:lazy",
    "explicit_empty_then_selfclosing:legacy", "explicit_empty_then_selfclosing:dataframe", "explicit_empty_then_selfclosing:lazy",
    "selfclosing_before_live:legacy", "selfclosing_before_live:dataframe", "selfclosing_before_live:lazy",
    "selfclosing_after_live:legacy", "selfclosing_after_live:dataframe", "selfclosing_after_live:lazy",
    "two_selfclosing_after_live:legacy", "two_selfclosing_after_live:dataframe", "two_selfclosing_after_live:lazy",
    "explicit_empty_before_live:legacy", "explicit_empty_before_live:dataframe", "explicit_empty_before_live:lazy",
    "explicit_empty_after_live:legacy", "explicit_empty_after_live:dataframe", "explicit_empty_after_live:lazy",
    "named_empty_trace:legacy", "named_empty_trace:dataframe", "named_empty_trace:lazy",
    "named_empty_then_selfclosing:legacy", "named_empty_then_selfclosing:dataframe", "named_empty_then_selfclosing:lazy",
    "named_empty_before_live:legacy", "named_empty_before_live:dataframe", "named_empty_before_live:lazy",
    "named_empty_after_live:legacy", "named_empty_after_live:dataframe", "named_empty_after_live:lazy",
    "selfclosing_event:legacy", "selfclosing_event:dataframe", "selfclosing_event:lazy",
    "explicit_empty_event:legacy", "explicit_empty_event:dataframe", "explicit_empty_event:lazy",
)


def worker(args):
    try:
        pm4py_module, importer_module = _load_public_reference_api()
        _require_version(pm4py_module)
    except BaseException as error:
        result = {"status": "configuration_error", "exception": type(error).__name__,
                  "message": str(error)}
        print(json.dumps(result, sort_keys=True, allow_nan=False))
        return 1
    try:
        parameters = MODES[args.mode]
        value = importer_module.apply(args.input, variant=importer_module.Variants.RUSTXES, parameters=parameters)
        result = {"status": "ok", "result": capture_value(value)}
    except BaseException as error:  # pyo3_runtime.PanicException is not Exception.
        result = {"status": "exception", "exception": type(error).__name__, "message": str(error)}
    print(json.dumps(result, sort_keys=True, allow_nan=False))
    return 0


def panic_markers(stderr):
    markers = []
    for line in stderr.splitlines():
        stripped = line.strip()
        if "not yet implemented:" in stripped or "PanicException:" in stripped or "Failed to parse XES:" in stripped:
            markers.append(stripped)
    return markers


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path)
    parser.add_argument("--worker", action="store_true")
    parser.add_argument("--input")
    parser.add_argument("--mode", choices=tuple(MODES))
    args = parser.parse_args()
    if args.worker:
        return worker(args)
    if not args.output:
        parser.error("--output is required outside worker mode")
    pm4py, importer = _load_public_reference_api()
    _require_version(pm4py)
    backend = version("r4pm")
    _require_backend(backend)
    observations = []
    errors = []
    with tempfile.TemporaryDirectory(prefix="minera-rust-empty-boundary-") as directory:
        directory_path = Path(directory)
        for case_name, xml in CASES.items():
            path = directory_path / f"{case_name}.xes"
            path.write_text(xml, encoding="utf-8")
            for mode, parameters in MODES.items():
                try:
                    completed = subprocess.run(
                        [sys.executable, str(Path(__file__).resolve()), "--worker", "--input", str(path), "--mode", mode],
                        capture_output=True,
                        text=True,
                        encoding="utf-8",
                        errors="replace",
                    )
                except (OSError, subprocess.TimeoutExpired) as error:
                    completed = None
                    result = {"status": "worker_process_error", "error": type(error).__name__,
                              "message": str(error)}
                    errors.append({"case": case_name, "mode": mode,
                                   "kind": "worker_process_error", "error": str(error)})
                if completed is not None:
                    output_lines = [line for line in completed.stdout.splitlines() if line.strip()]
                    if output_lines:
                        try:
                            result = json.loads(output_lines[-1])
                            if not isinstance(result, dict) or result.get('status') not in {
                                    'ok', 'exception', 'configuration_error'}:
                                raise ValueError('worker result must have a recognized status')
                        except (json.JSONDecodeError, TypeError, ValueError) as error:
                            result = {"status": "unreadable_worker_output", "stdout": completed.stdout,
                                      "stderr": completed.stderr, "error": str(error)}
                            errors.append({"case": case_name, "mode": mode,
                                           "kind": "unreadable_worker_output", "stdout": completed.stdout,
                                           "stderr": completed.stderr})
                    else:
                        result = {"status": "process_exit", "stdout": completed.stdout,
                                  "stderr": completed.stderr}
                        errors.append({"case": case_name, "mode": mode,
                                       "kind": "empty_worker_output", "stdout": completed.stdout,
                                       "stderr": completed.stderr})
                    if completed.returncode:
                        result["returncode"] = completed.returncode
                        errors.append({"case": case_name, "mode": mode,
                                       "kind": "worker_nonzero_exit", "returncode": completed.returncode,
                                       "stdout": completed.stdout, "stderr": completed.stderr})
                    markers = panic_markers(completed.stderr)
                    if markers:
                        result["panic_markers"] = markers
                    if completed.returncode and completed.stderr:
                        result["stderr"] = completed.stderr
                observations.append(
                    {
                        "observation_id": f"{case_name}:{mode}",
                        "case": case_name,
                        "mode": mode,
                        "parameters": parameters,
                        "xml": xml,
                        "result": result,
                    }
                )
    observed_ids = tuple(observation.get("observation_id") for observation in observations)
    if (observed_ids != EXPECTED_OBSERVATION_IDENTITIES
            or len(set(observed_ids)) != EXPECTED_OBSERVATION_COUNT):
        raise RuntimeError("empty-boundary capture observation manifest is incomplete or duplicated")
    artifact = {
        "capture_id": CAPTURE_ID,
        "expected_observation_count": EXPECTED_OBSERVATION_COUNT,
        "expected_observation_identities": list(EXPECTED_OBSERVATION_IDENTITIES),
        "reference": pm4py.__version__,
        "reference_commit": REFERENCE_COMMIT,
        "reference_commit_scope": REFERENCE_COMMIT_SCOPE,
        "environment": _environment(),
        "reference_module": _module_metadata(importer),
        "r4pm": backend,
        "public_signature": str(inspect.signature(importer.Variants.RUSTXES.value.apply)),
        "modes": MODES,
        "panic_marker_note": "panic markers are additionally extracted from stderr; full stderr is retained for worker failures.",
        "errors": errors,
        "observations": observations,
    }
    args.output.write_text(json.dumps(artifact, indent=2, sort_keys=True, allow_nan=False) + "\n", encoding="utf-8")
    print(f"Captured {len(observations)} empty-boundary observations")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
