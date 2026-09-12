"""Compare sampled native/public outcome support for randomized DFG ties.

Finite samples do not prove probabilities or exhaustive support. Counts and
unshared outcomes remain in the report; this script does not turn statistical
uncertainty into a claim of exact parity. No upstream source is inspected.
"""
import argparse
import collections
import hashlib
import importlib.util
import itertools
import json
import math
import os
import subprocess
import warnings
from pathlib import Path


REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"
REFERENCE_COMMIT_SCOPE = "configured_reference_pin"


def _load_public_reference_api():
    """Load public reference modules only when running the stochastic audit."""
    warnings.filterwarnings("ignore")
    import pandas
    import pm4py
    from pm4py.algo.conformance.alignments.dfg.variants import classic
    from pm4py.objects.log.obj import Event, Trace

    return pm4py, pandas, classic, Event, Trace


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


def _support_exit_code(unshared_cases, large_frequency_differences=0):
    """Return a failing status for unshared support or large deviations."""
    return 1 if (unshared_cases or large_frequency_differences) else 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--native", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--repetitions", type=int, default=200)
    args = parser.parse_args()
    if args.repetitions < 1:
        parser.error("--repetitions must be positive")
    pm4py, pandas, classic, Event, Trace = _load_public_reference_api()
    _require_versions(pm4py, pandas)
    native_metadata = _file_metadata(args.native)
    spec = importlib.util.spec_from_file_location(
        "dfg_native_audit", Path(__file__).with_name("capture-dfg-native-reference.py"))
    audit = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(audit)
    models = dict(audit.MODELS)
    models.update({
        "selfloop": ({("a", "a"): 1}, {"a": 1}, {"a": 1}, "abx"),
        "multistart": ({("a", "c"): 1, ("b", "c"): 1}, {"a": 1, "b": 1}, {"c": 1}, "abcx"),
        "disconnected": ({("a", "b"): 1, ("c", "c"): 1}, {"a": 1}, {"b": 1}, "abcx"),
    })
    scenarios = {"zero": {role + "_cost_function": dict(a=0, b=0, c=0, x=0)
                           for role in ("sync", "model_move", "log_move", "internal_log_move")},
                 "default": {}}
    scenarios["equal"] = {role + "_cost_function": dict(a=1, b=1, c=1, x=1)
                           for role in ("sync", "model_move", "log_move", "internal_log_move")}
    scenarios["zero_internal"] = {
        "sync_cost_function": dict(a=0, b=0, c=0, x=0),
        "model_move_cost_function": dict(a=0, b=0, c=0, x=0),
        "internal_log_move_cost_function": dict(a=0, b=0, c=0, x=0),
        "log_move_cost_function": dict(a=2, b=3, c=4, x=5),
    }
    cases = []
    for model, (_, _, _, alphabet) in models.items():
        words = ["".join(word) for length in range(3)
                 for word in itertools.product(alphabet, repeat=length)]
        words += ["aaa", "aba", "axb", "bba"]
        for scenario in scenarios:
            cases += [(model, scenario, word) for word in words]
    observed_native_by_case = [collections.Counter() for _ in cases]
    requests = (f"{model} {scenario} {word or '-'}\n"
                for model, scenario, word in cases for _ in range(args.repetitions))
    offset = 0
    while batch := list(itertools.islice(requests, 4096)):
        native = subprocess.run(
            [str(args.native.resolve())], input="".join(batch),
            capture_output=True, text=True, encoding="utf-8", timeout=180,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0), check=True)
        lines = native.stdout.splitlines()
        if len(lines) != len(batch):
            raise RuntimeError(
                f"Native output count {len(lines)} does not match batch count {len(batch)}")
        for index, line in enumerate(lines):
            case_index = (offset + index) // args.repetitions
            signature = json.dumps(json.loads(line)["trace"], sort_keys=True)
            observed_native_by_case[case_index][signature] += 1
        offset += len(batch)
    rows = []
    unshared_cases = 0
    large_frequency_differences = 0
    for index, (model, scenario, word) in enumerate(cases):
        dfg, starts, ends, _ = models[model]
        observed_native = observed_native_by_case[index]
        observed_public = collections.Counter()
        trace = Trace([Event({"concept:name": label}) for label in word])
        for _ in range(args.repetitions):
            result = audit.alignment(classic.apply_trace(
                trace, dfg, starts, ends, parameters=scenarios[scenario]))
            # Costs are integral in these fixtures; normalize int/float spelling
            # without discarding any alignment or diagnostic field.
            for key in ("cost", "internal_cost"):
                result[key] = int(result[key])
            observed_public[json.dumps(result, sort_keys=True)] += 1
        public_only = observed_public.keys() - observed_native.keys()
        native_only = observed_native.keys() - observed_public.keys()
        if public_only or native_only:
            unshared_cases += 1
        max_standardized_difference = 0.0
        total_variation = 0.0
        for outcome in observed_public.keys() | observed_native.keys():
            public_frequency = observed_public[outcome] / args.repetitions
            native_frequency = observed_native[outcome] / args.repetitions
            difference = abs(public_frequency - native_frequency)
            total_variation += difference / 2
            pooled = (public_frequency + native_frequency) / 2
            standard_error = math.sqrt(pooled * (1 - pooled) * 2 / args.repetitions)
            if standard_error:
                max_standardized_difference = max(max_standardized_difference, difference / standard_error)
        if max_standardized_difference > 5:
            large_frequency_differences += 1
        encode = lambda counts: [dict(result=json.loads(value), occurrences=count)
                                 for value, count in sorted(counts.items())]
        rows.append(dict(model=model, scenario=scenario, word=word,
                         public=encode(observed_public), native=encode(observed_native),
                         total_variation=total_variation,
                         max_pooled_standard_error_difference=max_standardized_difference,
                         public_only=[json.loads(value) for value in sorted(public_only)],
                         native_only=[json.loads(value) for value in sorted(native_only)]))
    result = dict(reference=pm4py.__version__, pandas=pandas.__version__,
                  reference_commit=REFERENCE_COMMIT,
                  reference_commit_scope=REFERENCE_COMMIT_SCOPE,
                  environment=_environment(), reference_module=_module_metadata(classic),
                  native=str(args.native.resolve()), native_executable=native_metadata,
                  repetitions=args.repetitions, cases=len(cases),
                  cases_with_unshared_sampled_outcomes=unshared_cases,
                  cases_with_frequency_difference_over_five_standard_errors=large_frequency_differences,
                  limitation="Sampled supports and frequencies are observations, not proof of exhaustive support or exact probabilities.",
                  rows=rows)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: value for key, value in result.items() if key != "rows"}, indent=2))
    return _support_exit_code(unshared_cases, large_frequency_differences)


if __name__ == "__main__":
    raise SystemExit(main())
