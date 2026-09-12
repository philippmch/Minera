"""Compare the compiled native DFG audit driver with pinned public APIs.

No reference implementation source is read. Each operation is called
independently, and generated sink UUIDs are canonicalized only in model moves.
"""
import argparse
import datetime
import hashlib
import itertools
import json
import math
import os
import re
import subprocess
import warnings
from pathlib import Path


REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"
REFERENCE_COMMIT_SCOPE = "configured_reference_pin"


def _load_public_reference_api():
    """Load public reference modules only when running the comparison."""
    warnings.filterwarnings("ignore")
    import pandas
    import pm4py
    from pm4py.algo.conformance.alignments.dfg.variants import classic
    from pm4py.objects.log.obj import Event, EventLog, Trace

    return pm4py, pandas, classic, Event, EventLog, Trace


def _environment():
    return {"PYTHONHASHSEED": os.environ.get("PYTHONHASHSEED"),
            "TZ": os.environ.get("TZ")}


def _require_versions(pm4py, pandas):
    versions = (getattr(pm4py, "__version__", None),
                getattr(pandas, "__version__", None))
    if versions != ("2.7.22", "3.0.5"):
        raise RuntimeError(
            f"This audit requires pm4py==2.7.22 and pandas==3.0.5; got "
            f"pm4py=={versions[0]!r}, pandas=={versions[1]!r}")


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

MODELS = {
    "path": ({("a", "b"): 1}, {"a": 1}, {"b": 1}, "abx"),
    "singleton": ({}, {"a": 1}, {"a": 1}, "ax"),
    "cycle": ({("a", "b"): 1, ("b", "a"): 1}, {"a": 1}, {"a": 1}, "abx"),
    "fork": ({("a", "b"): 1, ("a", "c"): 1}, {"a": 1}, {"b": 1, "c": 1}, "abcx"),
    "chain3": ({("a", "b"): 1, ("b", "c"): 1}, {"a": 1}, {"c": 1}, "abcx"),
}
SCENARIOS = {
    "default": {},
    "sync": {"sync_cost_function": dict(a=2, b=3, c=4, x=5)},
    "model": {"model_move_cost_function": dict(a=2, b=3, c=4, x=5)},
    "log": {"log_move_cost_function": dict(a=2, b=3, c=4, x=5)},
    "internal": {"internal_log_move_cost_function": dict(a=2, b=3, c=4, x=5)},
    "unequal": {
        "sync_cost_function": dict(a=2, b=3, c=7, x=11),
        "log_move_cost_function": dict(a=123, b=456, c=147, x=59),
        "model_move_cost_function": dict(a=789, b=135, c=249, x=41),
    },
}


def alignment(value):
    if value is None:
        return None
    result = dict(value)
    result["alignment"] = [
        [log, "@@dfg_sink" if re.fullmatch(
            r"[0-9a-f]{8}(-[0-9a-f]{4}){3}-[0-9a-f]{12}", model) else model]
        for log, model in value["alignment"]]
    return result


def timestamp(value):
    if isinstance(value, str):
        value = datetime.datetime.fromisoformat(value.replace("Z", "+00:00"))
    return value.isoformat()


def projection(value):
    return {
        "attributes": dict(value.attributes),
        "traces": [{
            "attributes": dict(trace.attributes),
            "events": [{
                "activity": event["concept:name"],
                "timestamp": timestamp(event["time:timestamp"]),
                "conforming": event["@@is_conforming"],
                "extra_attributes": {key: val for key, val in event.items()
                                     if key not in {"concept:name", "time:timestamp", "@@is_conforming"}},
            } for event in trace],
        } for trace in value],
    }


def metrics(value):
    edges, activities = value
    return {"activities": activities, "edges": [
        dict(source=source, target=target, **counts)
        for (source, target), counts in sorted(edges.items())]}


def equivalent(a, b):
    if isinstance(a, dict) and isinstance(b, dict):
        if a.get("error") is True and b.get("error") is True:
            return True  # Exception class/message cross-language differences are reported separately.
        return a.keys() == b.keys() and all(equivalent(a[k], b[k]) for k in a)
    if isinstance(a, list) and isinstance(b, list):
        return len(a) == len(b) and all(equivalent(x, y) for x, y in zip(a, b))
    if type(a) in (int, float) and type(b) in (int, float):
        return math.isclose(a, b, rel_tol=1e-12, abs_tol=1e-12)
    return type(a) is type(b) and a == b


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--native", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--max-length", type=int, default=4, choices=range(0, 6))
    parser.add_argument("--extended", action="store_true", help="Include boundary graphs and zero, partial, and empty cost functions")
    args = parser.parse_args()
    pm4py, pandas, classic, Event, EventLog, Trace = _load_public_reference_api()
    _require_versions(pm4py, pandas)
    native_metadata = _file_metadata(args.native)
    if args.extended:
        MODELS.update({
            "selfloop": ({("a", "a"): 1}, {"a": 1}, {"a": 1}, "abx"),
            "multistart": ({("a", "c"): 1, ("b", "c"): 1}, {"a": 1, "b": 1}, {"c": 1}, "abcx"),
            "disconnected": ({("a", "b"): 1, ("c", "c"): 1}, {"a": 1}, {"b": 1}, "abcx"),
            "zero_frequency": ({("a", "b"): 0}, {"a": 0}, {"b": 0}, "abx"),
            "unreachable": ({("a", "b"): 1}, {"a": 1}, {"c": 1}, "abcx"),
            "empty": ({}, {}, {}, "abx"),
        })
        SCENARIOS["zero"] = {role + "_cost_function": dict(a=0, b=0, c=0, x=0)
                             for role in ("sync", "model_move", "log_move", "internal_log_move")}
        SCENARIOS["equal"] = {role + "_cost_function": dict(a=1, b=1, c=1, x=1)
                              for role in ("sync", "model_move", "log_move", "internal_log_move")}
        SCENARIOS["zero_internal"] = {
            "sync_cost_function": dict(a=0, b=0, c=0, x=0),
            "model_move_cost_function": dict(a=0, b=0, c=0, x=0),
            "internal_log_move_cost_function": dict(a=0, b=0, c=0, x=0),
            "log_move_cost_function": dict(a=2, b=3, c=4, x=5),
        }
        for name, role in (("sync", "sync"), ("model", "model_move"),
                           ("log", "log_move"), ("internal", "internal_log_move")):
            SCENARIOS["empty_" + name] = {role + "_cost_function": {}}
            SCENARIOS["partial_" + name] = {role + "_cost_function": {"a": 2}}
    cases = [(model, scenario, "".join(word))
             for model, (_, _, _, alphabet) in MODELS.items()
             for scenario in SCENARIOS
             for length in range(args.max_length + 1)
             for word in itertools.product(alphabet, repeat=length)]
    process = subprocess.run(
        [str(args.native.resolve())],
        input="".join(f"{model} {scenario} {word or '-'}\n" for model, scenario, word in cases),
        capture_output=True, text=True, encoding="utf-8", timeout=180,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0), check=True)
    native = [json.loads(line) for line in process.stdout.splitlines()]
    if len(native) != len(cases):
        raise RuntimeError(
            f"Native output count {len(native)} does not match case count {len(cases)}: "
            f"{process.stderr}")
    summary = {name: {"cases": 0, "mismatches": 0, "matched_errors": 0}
               for name in ("trace", "log", "projection", "metrics")}
    mismatches = []
    matched_errors = []
    for (model, scenario, word), actual in zip(cases, native):
        dfg, starts, ends, _ = MODELS[model]
        parameters = SCENARIOS[scenario]
        trace = Trace([Event({
            "concept:name": label,
            "time:timestamp": datetime.datetime(2026, 1, 1, tzinfo=datetime.timezone.utc)
                              + datetime.timedelta(seconds=index),
            "extra": "discard", "org:resource": "discard",
        }) for index, label in enumerate(word)], attributes={"extra": "discard"})
        log = EventLog([trace], attributes={"extra": "discard"})
        operations = {
            "trace": lambda: alignment(classic.apply_trace(trace, dfg, starts, ends, parameters=parameters)),
            "log": lambda: alignment(classic.apply_log(log, dfg, starts, ends, parameters=parameters)[0]),
            "projection": lambda: projection(classic.project_log_on_dfg(log, dfg, starts, ends, parameters=parameters)),
            "metrics": lambda: metrics(classic.project_alignments_on_dfg(log, dfg, starts, ends, parameters=parameters)),
        }
        for operation, call in operations.items():
            try:
                expected = call()
            except Exception as error:
                expected = {"error": True, "type": type(error).__name__, "message": str(error)}
            observed = actual[operation]
            if operation == "projection" and not observed.get("error"):
                for projected_trace in observed["traces"]:
                    for event in projected_trace["events"]:
                        event["timestamp"] = timestamp(event["timestamp"])
            stats = summary[operation]
            stats["cases"] += 1
            if not equivalent(expected, observed):
                stats["mismatches"] += 1
                mismatches.append(dict(model=model, scenario=scenario, word=word,
                                       operation=operation, expected=expected, actual=observed))
            elif isinstance(expected, dict) and expected.get("error"):
                stats["matched_errors"] += 1
                matched_errors.append(dict(model=model, scenario=scenario, word=word,
                                           operation=operation, expected=expected,
                                           actual=observed))
    result = dict(reference=pm4py.__version__, pandas=pandas.__version__,
                  reference_commit=REFERENCE_COMMIT,
                  reference_commit_scope=REFERENCE_COMMIT_SCOPE,
                  environment=_environment(), reference_module=_module_metadata(classic),
                  native=str(args.native.resolve()), native_executable=native_metadata,
                  summary=summary, mismatches=mismatches, matched_errors=matched_errors)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2))
    return 1 if mismatches else 0


if __name__ == "__main__":
    raise SystemExit(main())
