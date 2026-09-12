"""Sample public/native outcome support for mismatched extended DFG rows.

The input report is a single-call comparison.  Only its mismatched
``(model, scenario, word, operation)`` rows are sampled here; the four
operations remain independent observations.  Sampled frequencies and support
are evidence about these draws, not proof of exact probabilities or complete
support.
"""
import argparse
import collections
import datetime
import gzip
import hashlib
import importlib.util
import itertools
import json
import math
import os
import subprocess
import warnings
from pathlib import Path


REFERENCE_VERSION = "2.7.22"
PANDAS_VERSION = "3.0.5"
REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"
REFERENCE_COMMIT_SCOPE = "configured_reference_pin"
OPERATIONS = ("trace", "log", "projection", "metrics")
BATCH_SIZE = 4096


def _load_capture_module():
    spec = importlib.util.spec_from_file_location(
        "dfg_native_audit", Path(__file__).with_name("capture-dfg-native-reference.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _load_public_reference_api(capture):
    """Load public APIs only after input provenance has been validated."""
    warnings.filterwarnings("ignore")
    return capture._load_public_reference_api()


def _require_versions(pm4py, pandas):
    versions = (getattr(pm4py, "__version__", None),
                getattr(pandas, "__version__", None))
    if versions != (REFERENCE_VERSION, PANDAS_VERSION):
        raise RuntimeError(
            f"This audit requires pm4py=={REFERENCE_VERSION} and "
            f"pandas=={PANDAS_VERSION}; got pm4py=={versions[0]!r}, "
            f"pandas=={versions[1]!r}")


def _environment():
    return {"PYTHONHASHSEED": os.environ.get("PYTHONHASHSEED"),
            "TZ": os.environ.get("TZ")}


def _file_metadata(path):
    """Hash an executable or module as opaque bytes without reading source."""
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


def _validate_reference_module(document, loaded_reference_module):
    """Require the loaded public module to match the input report bytes."""
    expected = document.get("reference_module")
    if not isinstance(expected, dict) or not isinstance(expected.get("sha256"), str):
        raise ValueError("input report reference_module must include sha256")
    actual = (loaded_reference_module.get("sha256")
              if isinstance(loaded_reference_module, dict) else None)
    if actual != expected["sha256"]:
        raise RuntimeError(
            "loaded public reference module SHA256 does not match input report: "
            f"expected {expected['sha256']!r}, got {actual!r}")


def _extended_matrix(capture):
    """Return the exact extended matrix used by the native capture script."""
    models = dict(capture.MODELS)
    models.update({
        "selfloop": ({("a", "a"): 1}, {"a": 1}, {"a": 1}, "abx"),
        "multistart": ({("a", "c"): 1, ("b", "c"): 1}, {"a": 1, "b": 1}, {"c": 1}, "abcx"),
        "disconnected": ({("a", "b"): 1, ("c", "c"): 1}, {"a": 1}, {"b": 1}, "abcx"),
        "zero_frequency": ({("a", "b"): 0}, {"a": 0}, {"b": 0}, "abx"),
        "unreachable": ({("a", "b"): 1}, {"a": 1}, {"c": 1}, "abcx"),
        "empty": ({}, {}, {}, "abx"),
    })
    scenarios = dict(capture.SCENARIOS)
    scenarios["zero"] = {role + "_cost_function": dict(a=0, b=0, c=0, x=0)
                          for role in ("sync", "model_move", "log_move", "internal_log_move")}
    scenarios["equal"] = {role + "_cost_function": dict(a=1, b=1, c=1, x=1)
                           for role in ("sync", "model_move", "log_move", "internal_log_move")}
    scenarios["zero_internal"] = {
        "sync_cost_function": dict(a=0, b=0, c=0, x=0),
        "model_move_cost_function": dict(a=0, b=0, c=0, x=0),
        "internal_log_move_cost_function": dict(a=0, b=0, c=0, x=0),
        "log_move_cost_function": dict(a=2, b=3, c=4, x=5),
    }
    for name, role in (("sync", "sync"), ("model", "model_move"),
                       ("log", "log_move"), ("internal", "internal_log_move")):
        scenarios["empty_" + name] = {role + "_cost_function": {}}
        scenarios["partial_" + name] = {role + "_cost_function": {"a": 2}}
    return models, scenarios


def _read_reference_report(path):
    try:
        if path.suffix.lower() == ".gz":
            with gzip.open(path, "rt", encoding="utf-8") as source:
                document = json.load(source)
        else:
            document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ValueError(f"Unable to read reference report {path}: {error}") from error
    if not isinstance(document, dict):
        raise ValueError("reference report must be a JSON object")
    if document.get("reference") != REFERENCE_VERSION:
        raise ValueError(f"unexpected public reference version: {document.get('reference')!r}")
    if document.get("pandas") != PANDAS_VERSION:
        raise ValueError(f"unexpected pandas version: {document.get('pandas')!r}")
    if document.get("reference_commit") != REFERENCE_COMMIT:
        raise ValueError("missing or unexpected reference commit")
    if document.get("reference_commit_scope") != REFERENCE_COMMIT_SCOPE:
        raise ValueError("missing or unexpected reference commit scope")
    mismatches = document.get("mismatches")
    if not isinstance(mismatches, list):
        raise ValueError("reference report mismatches must be an array")
    return document, mismatches


def _select_cases(mismatches, models, scenarios):
    selected = []
    seen = set()
    for index, row in enumerate(mismatches):
        if not isinstance(row, dict):
            raise ValueError(f"mismatch row {index} must be an object")
        missing = {"model", "scenario", "word", "operation"} - set(row)
        if missing:
            raise ValueError(f"mismatch row {index} is missing {sorted(missing)}")
        model, scenario, word, operation = (row["model"], row["scenario"],
                                            row["word"], row["operation"])
        if not all(isinstance(value, str) for value in (model, scenario, word, operation)):
            raise ValueError(f"mismatch row {index} fields must be text")
        key = (model, scenario, word, operation)
        if model not in models:
            raise ValueError(f"mismatch row {index} has unknown model {model!r}")
        if scenario not in scenarios:
            raise ValueError(f"mismatch row {index} has unknown scenario {scenario!r}")
        if operation not in OPERATIONS:
            raise ValueError(f"mismatch row {index} has unknown operation {operation!r}")
        if key not in seen:
            selected.append((key, row))
            seen.add(key)
    return selected


def _public_outcome(operation, model, scenario, word, models, scenarios, api, capture):
    _, _, classic, Event, EventLog, Trace = api
    dfg, starts, ends, _ = models[model]
    parameters = scenarios[scenario]
    trace = Trace([Event({
        "concept:name": label,
        "time:timestamp": datetime.datetime(
            2026, 1, 1, tzinfo=datetime.timezone.utc)
            + datetime.timedelta(seconds=index),
        "extra": "discard", "org:resource": "discard",
    }) for index, label in enumerate(word)], attributes={"extra": "discard"})
    log = EventLog([trace], attributes={"extra": "discard"})
    calls = {
        "trace": lambda: capture.alignment(
            classic.apply_trace(trace, dfg, starts, ends, parameters=parameters)),
        "log": lambda: capture.alignment(
            classic.apply_log(log, dfg, starts, ends, parameters=parameters)[0]),
        "projection": lambda: capture.projection(
            classic.project_log_on_dfg(log, dfg, starts, ends, parameters=parameters)),
        "metrics": lambda: capture.metrics(
            classic.project_alignments_on_dfg(log, dfg, starts, ends, parameters=parameters)),
    }
    try:
        return calls[operation]()
    except Exception as error:
        return {"error": True, "type": type(error).__name__, "message": str(error)}


def _normalize_native(operation, value, capture):
    """Apply the same native projection timestamp normalization as the capture."""
    if operation != "projection" or not isinstance(value, dict) or value.get("error"):
        return value, None
    try:
        for projected_trace in value["traces"]:
            for event in projected_trace["events"]:
                event["timestamp"] = capture.timestamp(event["timestamp"])
    except (KeyError, TypeError, ValueError, AttributeError) as error:
        return value, f"malformed native projection result: {error}"
    return value, None


def _canonicalize_numbers(value):
    """Match numeric JSON spellings without collapsing booleans.

    The deterministic comparator treats integral integers and floats as the
    same value.  Keep that semantic normalization for sampled support while
    retaining nonintegral floats and boolean values exactly.
    """
    if isinstance(value, bool):
        return value
    if isinstance(value, float) and math.isfinite(value) and value.is_integer():
        return int(value)
    if isinstance(value, dict):
        return {key: _canonicalize_numbers(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_canonicalize_numbers(item) for item in value]
    return value


def _outcome_key(value):
    """Return the sampled-support key after semantic numeric normalization."""
    return json.dumps(_canonicalize_numbers(value), sort_keys=True)


def _rekey_counter(records):
    """Rekey saved ``result``/``occurrences`` records for offline rechecks."""
    if not isinstance(records, list):
        raise ValueError("saved support records must be an array")
    counter = collections.Counter()
    for record in records:
        if not isinstance(record, dict) or "result" not in record or "occurrences" not in record:
            raise ValueError("saved support record must contain result and occurrences")
        occurrences = record["occurrences"]
        if isinstance(occurrences, bool) or not isinstance(occurrences, int) or occurrences < 0:
            raise ValueError("saved support occurrences must be a nonnegative integer")
        counter[_outcome_key(record["result"])] += occurrences
    return counter


def _require_counter_total(counter, repetitions, row_index, side):
    total = sum(counter.values())
    if total != repetitions:
        raise ValueError(
            f"saved support row {row_index} {side} counter has {total} "
            f"occurrences; expected {repetitions}")


def _run_native(native_path, cases, repetitions, capture):
    counters = [{operation: collections.Counter() for operation in OPERATIONS}
                for _ in cases]
    errors = []
    requests = (f"{model} {scenario} {word or '-'}\n"
                for ((model, scenario, word, _), _) in cases
                for _ in range(repetitions))
    offset = 0
    while batch := list(itertools.islice(requests, BATCH_SIZE)):
        try:
            process = subprocess.run(
                [str(native_path.resolve())], input="".join(batch),
                capture_output=True, text=True, encoding="utf-8", timeout=180,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0), check=False)
        except Exception as error:
            errors.append({"kind": "native_process", "batch_offset": offset,
                           "message": f"{type(error).__name__}: {error}"})
            offset += len(batch)
            continue
        if process.returncode != 0:
            errors.append({"kind": "native_process", "batch_offset": offset,
                           "returncode": process.returncode,
                           "stderr": process.stderr})
        lines = (process.stdout or "").splitlines()
        if len(lines) != len(batch):
            errors.append({"kind": "native_output_count", "batch_offset": offset,
                           "expected": len(batch), "actual": len(lines),
                           "stderr": process.stderr})
        for local_index, line in enumerate(lines[:len(batch)]):
            case_index = (offset + local_index) // repetitions
            try:
                value = json.loads(line)
                if not isinstance(value, dict):
                    raise ValueError("native output line must be an object")
                missing = [operation for operation in OPERATIONS if operation not in value]
                if missing:
                    raise ValueError(f"native output is missing operations: {missing}")
                for operation in OPERATIONS:
                    normalized, normalization_error = _normalize_native(
                        operation, value[operation], capture)
                    if normalization_error:
                        errors.append({"kind": "malformed_native_output",
                                       "batch_offset": offset, "line_index": local_index,
                                       "operation": operation, "message": normalization_error,
                                       "raw": line})
                    signature = _outcome_key(normalized)
                    counters[case_index][operation][signature] += 1
            except (ValueError, TypeError, json.JSONDecodeError) as error:
                errors.append({"kind": "malformed_native_output",
                               "batch_offset": offset, "line_index": local_index,
                               "message": str(error), "raw": line})
        if len(lines) > len(batch):
            errors.append({"kind": "native_output_count", "batch_offset": offset,
                           "expected": len(batch), "actual": len(lines),
                           "extra_lines": lines[len(batch):]})
        offset += len(batch)
    return counters, errors


def _encode(counter):
    return [dict(result=json.loads(value), occurrences=count)
            for value, count in sorted(counter.items())]


def _compare(public, native, repetitions):
    public_only = public.keys() - native.keys()
    native_only = native.keys() - public.keys()
    max_standardized_difference = 0.0
    total_variation = 0.0
    for outcome in public.keys() | native.keys():
        public_frequency = public[outcome] / repetitions
        native_frequency = native[outcome] / repetitions
        difference = abs(public_frequency - native_frequency)
        total_variation += difference / 2
        pooled = (public_frequency + native_frequency) / 2
        standard_error = math.sqrt(pooled * (1 - pooled) * 2 / repetitions)
        if standard_error:
            max_standardized_difference = max(
                max_standardized_difference, difference / standard_error)
    return public_only, native_only, total_variation, max_standardized_difference


def recheck_saved_report(document):
    """Recompute support metrics from a saved report without rerunning calls."""
    if not isinstance(document, dict):
        raise ValueError("saved support report must be an object")
    repetitions = document.get("repetitions")
    if isinstance(repetitions, bool) or not isinstance(repetitions, int) or repetitions < 1:
        raise ValueError("saved support repetitions must be a positive integer")
    rows = document.get("rows")
    if not isinstance(rows, list):
        raise ValueError("saved support report rows must be an array")
    rechecked = []
    unshared = 0
    large_frequency_differences = 0
    for index, row in enumerate(rows):
        if not isinstance(row, dict):
            raise ValueError(f"saved support row {index} must be an object")
        public = _rekey_counter(row.get("public"))
        native = _rekey_counter(row.get("native"))
        _require_counter_total(public, repetitions, index, "public")
        _require_counter_total(native, repetitions, index, "native")
        public_only, native_only, total_variation, max_standardized = _compare(
            public, native, repetitions)
        if public_only or native_only:
            unshared += 1
        if max_standardized > 5:
            large_frequency_differences += 1
        updated = dict(row)
        updated.update(
            public=_encode(public), native=_encode(native),
            total_variation=total_variation,
            max_pooled_standard_error_difference=max_standardized,
            public_only=[json.loads(value) for value in sorted(public_only)],
            native_only=[json.loads(value) for value in sorted(native_only)])
        rechecked.append(updated)
    result = dict(document)
    result.update(
        rows=rechecked, cases=len(rechecked),
        cases_with_unshared_sampled_outcomes=unshared,
        cases_with_frequency_difference_over_five_standard_errors=large_frequency_differences,
        numeric_outcome_recheck="finite integral floats canonicalized to integers")
    return result


def _base_report(document, reference_module, native_metadata, input_report_metadata,
                 args, cases):
    return dict(
        reference=document["reference"], pandas=document["pandas"],
        reference_commit=REFERENCE_COMMIT,
        reference_commit_scope=REFERENCE_COMMIT_SCOPE,
        environment=_environment(), reference_module=reference_module,
        native=str(args.native.resolve()), native_executable=native_metadata,
        input_report=input_report_metadata["path"],
        input_report_sha256=input_report_metadata["sha256"],
        input_mismatch_rows=len(document["mismatches"]), selected_rows=len(cases),
        repetitions=args.repetitions,
        limitation=("Sampled supports and frequencies are observations, not proof of "
                    "exhaustive support or exact probabilities."),
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", "--report", dest="reference", required=True,
                        type=Path, help="Extended native comparison report")
    parser.add_argument("--native", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--repetitions", type=int, default=200)
    args = parser.parse_args()
    if args.repetitions < 1:
        parser.error("--repetitions must be positive")

    document, mismatches = _read_reference_report(args.reference)
    input_report_metadata = _file_metadata(args.reference)
    capture = _load_capture_module()
    models, scenarios = _extended_matrix(capture)
    selected = _select_cases(mismatches, models, scenarios)
    native_metadata = _file_metadata(args.native)

    if not selected:
        result = _base_report(document, None, native_metadata, input_report_metadata,
                              args, selected)
        result.update(cases=0, cases_with_unshared_sampled_outcomes=0,
                      cases_with_frequency_difference_over_five_standard_errors=0,
                      malformed_native_outputs=0, errors=[], rows=[])
        args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(json.dumps({key: value for key, value in result.items() if key != "rows"}, indent=2))
        return 0

    pm4py, pandas, classic, Event, EventLog, Trace = _load_public_reference_api(capture)
    _require_versions(pm4py, pandas)
    if document["reference"] != pm4py.__version__ or document["pandas"] != pandas.__version__:
        raise RuntimeError("loaded public dependency versions do not match the input report")
    api = (pm4py, pandas, classic, Event, EventLog, Trace)
    reference_module = _module_metadata(classic)
    _validate_reference_module(document, reference_module)
    native_counters, errors = _run_native(args.native, selected, args.repetitions, capture)
    rows = []
    unshared = 0
    large_frequency_differences = 0
    for index, (key, source) in enumerate(selected):
        model, scenario, word, operation = key
        public_counter = collections.Counter()
        for _ in range(args.repetitions):
            expected = _public_outcome(
                operation, model, scenario, word, models, scenarios, api, capture)
            public_counter[_outcome_key(expected)] += 1
        native_counter = native_counters[index][operation]
        public_only, native_only, total_variation, max_standardized = _compare(
            public_counter, native_counter, args.repetitions)
        if public_only or native_only:
            unshared += 1
        if max_standardized > 5:
            large_frequency_differences += 1
        row = dict(model=model, scenario=scenario, word=word, operation=operation,
                   public=_encode(public_counter), native=_encode(native_counter),
                   total_variation=total_variation,
                   max_pooled_standard_error_difference=max_standardized,
                   public_only=[json.loads(value) for value in sorted(public_only)],
                   native_only=[json.loads(value) for value in sorted(native_only)])
        for field in ("expected", "actual"):
            if field in source:
                row[field] = source[field]
        rows.append(row)
    result = _base_report(document, reference_module, native_metadata,
                          input_report_metadata, args, selected)
    result.update(
        cases=len(rows), cases_with_unshared_sampled_outcomes=unshared,
        cases_with_frequency_difference_over_five_standard_errors=large_frequency_differences,
        malformed_native_outputs=sum(error["kind"] == "malformed_native_output" for error in errors),
        errors=errors, rows=rows)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: value for key, value in result.items() if key != "rows"}, indent=2))
    return 1 if (errors or unshared or large_frequency_differences) else 0


if __name__ == "__main__":
    raise SystemExit(main())
