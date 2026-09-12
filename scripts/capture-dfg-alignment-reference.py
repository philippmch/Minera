"""Capture CLASSIC through public calls; never inspect implementation source.

Generated sink UUIDs are normalized to @@dfg_sink, the native canonical name.
Run with the pinned reference runtime/dependencies on PYTHONPATH.
"""
import argparse
import datetime
import hashlib
import json
import os
import re
import warnings
from pathlib import Path


REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"
REFERENCE_COMMIT_SCOPE = "configured_reference_pin"
_EVENT_LOG_TYPE = None
_TRACE_TYPE = None


def _load_public_reference_api():
    """Load the pinned public reference API only when capture is requested.

    Import-time warnings from the reference dependencies are not part of an
    observation, and the lazy import keeps report helpers usable offline.
    """
    warnings.filterwarnings("ignore")
    import pandas as pd
    import pm4py
    from pm4py.algo.conformance.alignments.dfg.variants import classic
    from pm4py.objects.log.obj import Event, EventLog, Trace

    return pm4py, pd, classic, Event, EventLog, Trace


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


def normalize(value):
    if isinstance(value, datetime.datetime):
        return {"datetime": value.isoformat()}
    if _EVENT_LOG_TYPE is not None and isinstance(value, _EVENT_LOG_TYPE):
        return {"attributes": normalize(value.attributes),
                "traces": [normalize(t) for t in value]}
    if _TRACE_TYPE is not None and isinstance(value, _TRACE_TYPE):
        return {"attributes": normalize(value.attributes),
                "events": [normalize(dict(e)) for e in value]}
    if isinstance(value, dict):
        return {str(k): normalize(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)):
        return [normalize(v) for v in value]
    if isinstance(value, str) and re.fullmatch(
            r"[0-9a-f]{8}(-[0-9a-f]{4}){3}-[0-9a-f]{12}", value):
        return "@@dfg_sink"
    return value


def capture():
    global _EVENT_LOG_TYPE, _TRACE_TYPE
    pm4py, pd, classic, Event, EventLog, Trace = _load_public_reference_api()
    _EVENT_LOG_TYPE, _TRACE_TYPE = EventLog, Trace
    _require_versions(pm4py, pd)
    rows = []
    models = {
        "path": ({("a", "b"): 5}, {"a": 2}, {"b": 3}),
        "empty": ({}, {}, {}),
        "unreachable": ({("a", "b"): 1}, {"a": 1}, {"c": 1}),
        "disconnected": ({("a", "b"): 1, ("c", "d"): 1}, {"a": 1}, {"b": 1}),
        "singleton": ({}, {"a": 1}, {"a": 1}),
        "loop": ({("a", "b"): 1, ("b", "a"): 1}, {"a": 1}, {"a": 1}),
    }
    costs = {
        "default": {},
        "sync": {"sync_cost_function": {"a": 2, "b": 3}},
        "model": {"model_move_cost_function": {"a": 2, "b": 3}},
        "log": {"log_move_cost_function": {"a": 2, "b": 3, "x": 4}},
        "internal": {"internal_log_move_cost_function": {"a": 2, "b": 3, "x": 4}},
        "partial_model": {"model_move_cost_function": {"a": 2}},
        "empty_sync": {"sync_cost_function": {}},
        "empty_model": {"model_move_cost_function": {}},
        "empty_log": {"log_move_cost_function": {}},
        "empty_internal": {"internal_log_move_cost_function": {}},
        "partial_sync": {"sync_cost_function": {"a": 2}},
        "partial_log": {"log_move_cost_function": {"x": 4}},
        "partial_internal": {"internal_log_move_cost_function": {"x": 4}},
    }
    def record(name, call, **context):
        try:
            rows.append(dict(operation=name, **context, result=normalize(call())))
        except Exception as error:
            rows.append(dict(operation=name, **context, error=type(error).__name__,
                             message=str(error)))
    for model_name, (dfg, starts, ends) in models.items():
        for word in ((), ("a", "b"), ("x",), ("a",), ("b",),
                     ("a", "x", "b"), ("a", "b", "a"), ("b", "a"),
                     ("a", "a", "b"), ("a", "b", "a", "b", "a")):
            trace = Trace([Event({"concept:name": label,
                                  "time:timestamp": datetime.datetime(2026, 1, i + 1),
                                  "extra": i}) for i, label in enumerate(word)],
                          attributes={"concept:name": "case", "extra": True})
            log = EventLog([trace], attributes={"extra": True})
            for cost_name, parameters in costs.items():
                if model_name != "path" and cost_name != "default":
                    continue
                for name, fn, obj in (("trace", classic.apply, trace),
                                      ("log", classic.apply, log),
                                      ("projection", classic.project_log_on_dfg, log),
                                      ("metrics", classic.project_alignments_on_dfg, log)):
                    record(name, lambda: fn(obj, dfg, starts, ends, parameters=parameters),
                           model=model_name, word=word, costs=cost_name)
    for timestamp in (False, True):
        event = Event({"phase": "a", "extra": "discard"})
        if timestamp:
            event["when"] = datetime.datetime(2026, 1, 1)
        record("selected_projection", lambda: classic.project_log_on_dfg(
            EventLog([Trace([event])]), *models["path"], parameters={
                "pm4py:param:activity_key": "phase", "pm4py:param:timestamp_key": "when"}),
            timestamp=timestamp)
    record("empty_log_with_empty_model_cost", lambda: classic.apply(
        EventLog([]), *models["path"], parameters={"model_move_cost_function": {}}))
    return dict(reference_version=pm4py.__version__, pandas_version=pd.__version__,
                reference_commit=REFERENCE_COMMIT,
                reference_commit_scope=REFERENCE_COMMIT_SCOPE,
                environment=_environment(), reference_module=_module_metadata(classic),
                rows=rows)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    args.output.write_text(json.dumps(capture(), indent=2) + "\n", encoding="utf-8")
