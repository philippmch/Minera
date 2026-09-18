"""Capture public RUSTXES raw table edge behavior without source reads."""

import argparse
import datetime
import hashlib
import inspect
import json
import math
import os
import tempfile
from importlib.metadata import version
from pathlib import Path
import warnings


REFERENCE_VERSION = "2.7.22"
BACKEND_VERSION = "0.6.2"
REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"
REFERENCE_COMMIT_SCOPE = "configured_reference_pin"
pd = pl = pm4py = importer = None


def _load_public_reference_api():
    global pd, pl, pm4py, importer
    warnings.filterwarnings("ignore")
    import pandas as pd_module
    import polars as pl_module
    import pm4py as pm4py_module
    from pm4py.objects.log.importer.xes import importer as importer_module
    pd, pl, pm4py, importer = pd_module, pl_module, pm4py_module, importer_module
    return pm4py, pd, pl, importer


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
    """Make pandas/polars values JSON-safe while preserving row semantics."""
    if pd is not None and value is pd.NaT:
        # Keep pandas' nullable datetime sentinel visible in the raw capture;
        # Polars represents the same cell as None.
        return {"date": "NaT"}
    if pd is not None and value is pd.NA:
        return None
    if isinstance(value, dict):
        return {str(key): normalize(value[key]) for key in sorted(value, key=str)}
    if isinstance(value, (list, tuple)):
        return [normalize(item) for item in value]
    if isinstance(value, (datetime.datetime, datetime.date)):
        return {"date": value.isoformat()}
    if isinstance(value, float) and not math.isfinite(value):
        return {"float": "nan" if math.isnan(value) else "inf" if value > 0 else "-inf"}
    # pandas may return numpy scalar values from to_dict in older versions.
    item = getattr(value, "item", None)
    if item is not None and not isinstance(value, (str, bytes)):
        try:
            return normalize(item())
        except (TypeError, ValueError):
            pass
    return value


def capture_value(value):
    """Capture a return value with schemas and rows keyed independently of order."""
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
        result["row_count"] = int(len(value))
        result["columns"] = sorted(str(column) for column in value.columns)
        result["dtypes"] = {
            str(column): str(dtype)
            for column, dtype in sorted(value.dtypes.items(), key=lambda item: str(item[0]))
        }
        result["rows"] = [normalize(row) for row in value.to_dict(orient="records")]
        return result
    if kind == "LazyFrame":
        materialized = value.collect()
        result["row_count"] = int(materialized.height)
        result["columns"] = sorted(str(column) for column in materialized.columns)
        result["dtypes"] = {
            str(column): str(dtype)
            for column, dtype in sorted(materialized.schema.items(), key=lambda item: str(item[0]))
        }
        result["rows"] = [normalize(row) for row in materialized.to_dicts()]
        return result
    raise RuntimeError(f"Unexpected RUSTXES return type: {kind}")


CASES = {
    "interleaved_cases": """<log>
<trace><string key="concept:name" value="c1"/><event><string key="concept:name" value="A"/><int key="n" value="1"/></event></trace>
<trace><string key="concept:name" value="c2"/><event><string key="concept:name" value="B"/><int key="n" value="2"/></event></trace>
<trace><string key="concept:name" value="c1"/><event><string key="concept:name" value="C"/><int key="n" value="3"/></event></trace>
</log>""",
    "missing_case_all": """<log>
<trace><event><string key="concept:name" value="A"/></event></trace>
<trace><event><string key="concept:name" value="B"/></event></trace>
</log>""",
    "missing_case_mixed": """<log>
<trace><event><string key="concept:name" value="A"/></event></trace>
<trace><string key="concept:name" value="c2"/><event><string key="concept:name" value="B"/></event></trace>
</log>""",
    "empty_trace": """<log>
<trace><string key="concept:name" value="empty"/></trace>
<trace><string key="concept:name" value="full"/><event><string key="concept:name" value="A"/></event></trace>
</log>""",
    "empty_named_extra_before": """<log>
<trace><string key="concept:name" value="empty"/><string key="trace_only" value="e"/></trace>
<trace><string key="concept:name" value="full"/><event><string key="concept:name" value="A"/></event></trace>
</log>""",
    "empty_only_attrs": '<log><trace><string key="concept:name" value="empty"/><string key="trace_only" value="e"/></trace></log>',
    "empty_trace_missing_case": '<log><trace/><trace><string key="concept:name" value="full"/><event><string key="concept:name" value="A"/></event></trace></log>',
    "empty_log": "<log/>",
    "trace_event_plain_collision": """<log><trace>
<string key="concept:name" value="c"/><string key="foo" value="trace"/>
<event><string key="concept:name" value="A"/><string key="foo" value="event"/></event>
</trace></log>""",
    "trace_event_case_prefix_collision": """<log><trace>
<string key="concept:name" value="c"/><string key="case:foo" value="tracecase"/>
<event><string key="concept:name" value="A"/><string key="case:foo" value="eventcase"/></event>
</trace></log>""",
    "trace_event_case_identity_collision": """<log><trace>
<string key="concept:name" value="c"/>
<event><string key="concept:name" value="A"/><string key="case:concept:name" value="eventcase"/></event>
</trace></log>""",
    "null_only_columns": """<log><trace><string key="concept:name" value="c"/>
<event><string key="concept:name" value="A"/><int key="n_int" null="true"/><float key="n_float" null="true"/><string key="n_text" null="true"/></event>
<event><string key="concept:name" value="B"/><int key="n_int" null="true"/><float key="n_float" null="true"/><string key="n_text" null="true"/></event>
</trace></log>""",
    "nullable_int": """<log><trace><string key="concept:name" value="c"/>
<event><string key="concept:name" value="A"/><int key="n" value="1"/></event>
<event><string key="concept:name" value="B"/><int key="n" null="true"/></event>
</trace></log>""",
    "mixed_int_float": """<log><trace><string key="concept:name" value="c"/>
<event><string key="concept:name" value="A"/><int key="n" value="1"/></event>
<event><string key="concept:name" value="B"/><float key="n" value="2.5"/></event>
</trace></log>""",
    "mixed_int_string": """<log><trace><string key="concept:name" value="c"/>
<event><string key="concept:name" value="A"/><int key="n" value="1"/></event>
<event><string key="concept:name" value="B"/><string key="n" value="s"/></event>
</trace></log>""",
    "nan_float": """<log><trace><string key="concept:name" value="c"/>
<event><string key="concept:name" value="A"/><float key="n" value="NaN"/></event>
<event><string key="concept:name" value="B"/><float key="n" value="1.5"/></event>
</trace></log>""",
    "nullable_date": """<log><trace><string key="concept:name" value="c"/>
<event><string key="concept:name" value="A"/><date key="d" value="2024-01-01T00:00:00Z"/></event>
<event><string key="concept:name" value="B"/><date key="d" null="true"/></event>
</trace></log>""",
    "null_only_date": """<log><trace><string key="concept:name" value="c"/>
<event><string key="concept:name" value="A"/><date key="d" null="true"/></event>
<event><string key="concept:name" value="B"/><date key="d" null="true"/></event>
</trace></log>""",
    "nullable_bool": """<log><trace><string key="concept:name" value="c"/>
<event><string key="concept:name" value="A"/><boolean key="b" value="true"/></event>
<event><string key="concept:name" value="B"/><boolean key="b" null="true"/></event>
</trace></log>""",
    "null_only_bool": """<log><trace><string key="concept:name" value="c"/>
<event><string key="concept:name" value="A"/><boolean key="b" null="true"/></event>
<event><string key="concept:name" value="B"/><boolean key="b" null="true"/></event>
</trace></log>""",
    "mixed_bool_string": """<log><trace><string key="concept:name" value="c"/>
<event><string key="concept:name" value="A"/><boolean key="b" value="true"/></event>
<event><string key="concept:name" value="B"/><string key="b" value="s"/></event>
</trace></log>""",
    "empty_event_key": """<log><trace><string key="concept:name" value="c"/>
<event><string key="concept:name" value="A"/><string key="" value="x"/></event>
<event><string key="concept:name" value="B"/><string key="" value="y"/></event>
</trace></log>""",
    "empty_trace_key": """<log><trace><string key="" value="tv"/><string key="concept:name" value="c"/>
<event><string key="concept:name" value="A"/></event>
</trace></log>""",
    "empty_trace_and_event_keys": """<log><trace><string key="" value="tv"/><string key="concept:name" value="c"/>
<event><string key="concept:name" value="A"/><string key="" value="ev"/></event>
</trace></log>""",
}


CAPTURE_ID = 'rustxes_raw_reference'
EXPECTED_OBSERVATION_COUNT = 77
EXPECTED_OBSERVATION_IDENTITIES = (
    'default',
    'interleaved_cases_legacy', 'interleaved_cases_dataframe', 'interleaved_cases_lazy',
    'missing_case_all_legacy', 'missing_case_all_dataframe', 'missing_case_all_lazy',
    'missing_case_mixed_legacy', 'missing_case_mixed_dataframe', 'missing_case_mixed_lazy',
    'empty_trace_legacy', 'empty_trace_dataframe', 'empty_trace_lazy',
    'empty_named_extra_before_legacy', 'empty_named_extra_before_dataframe', 'empty_named_extra_before_lazy',
    'empty_only_attrs_legacy', 'empty_only_attrs_dataframe', 'empty_only_attrs_lazy',
    'empty_trace_missing_case_legacy', 'empty_trace_missing_case_dataframe', 'empty_trace_missing_case_lazy',
    'empty_log_legacy', 'empty_log_dataframe', 'empty_log_lazy',
    'trace_event_plain_collision_legacy', 'trace_event_plain_collision_dataframe', 'trace_event_plain_collision_lazy',
    'trace_event_case_prefix_collision_legacy', 'trace_event_case_prefix_collision_dataframe', 'trace_event_case_prefix_collision_lazy',
    'trace_event_case_identity_collision_legacy', 'trace_event_case_identity_collision_dataframe', 'trace_event_case_identity_collision_lazy',
    'null_only_columns_legacy', 'null_only_columns_dataframe', 'null_only_columns_lazy',
    'nullable_int_legacy', 'nullable_int_dataframe', 'nullable_int_lazy',
    'mixed_int_float_legacy', 'mixed_int_float_dataframe', 'mixed_int_float_lazy',
    'mixed_int_string_legacy', 'mixed_int_string_dataframe', 'mixed_int_string_lazy',
    'nan_float_legacy', 'nan_float_dataframe', 'nan_float_lazy',
    'nullable_date_legacy', 'nullable_date_dataframe', 'nullable_date_lazy',
    'null_only_date_legacy', 'null_only_date_dataframe', 'null_only_date_lazy',
    'nullable_bool_legacy', 'nullable_bool_dataframe', 'nullable_bool_lazy',
    'null_only_bool_legacy', 'null_only_bool_dataframe', 'null_only_bool_lazy',
    'mixed_bool_string_legacy', 'mixed_bool_string_dataframe', 'mixed_bool_string_lazy',
    'empty_event_key_legacy', 'empty_event_key_dataframe', 'empty_event_key_lazy',
    'empty_trace_key_legacy', 'empty_trace_key_dataframe', 'empty_trace_key_lazy',
    'empty_trace_and_event_keys_legacy', 'empty_trace_and_event_keys_dataframe', 'empty_trace_and_event_keys_lazy',
    'legacy_false_lazy_false', 'legacy_false_lazy_true', 'legacy_true_lazy_false', 'legacy_true_lazy_true',
)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    pm4py, pd, pl, importer = _load_public_reference_api()
    _require_version(pm4py)
    backend = version("r4pm")
    _require_backend(backend)
    rust = importer.Variants.RUSTXES.value
    observations = []
    # Capture every edge fixture in both raw table modes.  Legacy mode is
    # included too where it exposes grouping/drop behavior useful for context.
    calls = [("default", "interleaved_cases", {})]
    for name in CASES:
        for mode_name, parameters in (
            ("legacy", {"return_legacy_log_object": True}),
            ("dataframe", {"return_legacy_log_object": False, "return_pl_lazyframe": False}),
            ("lazy", {"return_legacy_log_object": False, "return_pl_lazyframe": True}),
        ):
            calls.append((f"{name}_{mode_name}", name, parameters))
    for name, parameters in (
        ("legacy_false_lazy_false", {"return_legacy_log_object": False, "return_pl_lazyframe": False}),
        ("legacy_false_lazy_true", {"return_legacy_log_object": False, "return_pl_lazyframe": True}),
        ("legacy_true_lazy_false", {"return_legacy_log_object": True, "return_pl_lazyframe": False}),
        ("legacy_true_lazy_true", {"return_legacy_log_object": True, "return_pl_lazyframe": True}),
    ):
        calls.append((name, "interleaved_cases", parameters))

    with tempfile.TemporaryDirectory(prefix="minera-rust-raw-") as directory:
        directory_path = Path(directory)
        for name, case_name, parameters in calls:
            path = directory_path / f"{case_name}.xes"
            path.write_text(CASES[case_name], encoding="utf-8")
            try:
                value = importer.apply(str(path), variant=importer.Variants.RUSTXES, parameters=parameters)
                result = capture_value(value)
            except BaseException as error:
                result = {"error": type(error).__name__, "message": str(error)}
            observations.append(
                {
                    "name": name,
                    "observation_id": name,
                    "case": case_name,
                    "parameters": parameters,
                    "xml": CASES[case_name],
                    "result": result,
                }
            )

    observed_ids = tuple(observation.get("observation_id") for observation in observations)
    if (observed_ids != EXPECTED_OBSERVATION_IDENTITIES
            or len(set(observed_ids)) != EXPECTED_OBSERVATION_COUNT):
        raise RuntimeError("raw capture observation manifest is incomplete or duplicated")

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
        "pandas": pd.__version__,
        "polars": pl.__version__,
        "public_signature": str(inspect.signature(rust.apply)),
        "exposed_parameters": {parameter.name: parameter.value for parameter in rust.Parameters},
        "column_order_note": "RUSTXES table columns are hash-map ordered; columns and dtype maps are sorted here, while row order is preserved.",
        "errors": [],
        "observations": observations,
    }
    args.output.write_text(json.dumps(artifact, indent=2, sort_keys=True, allow_nan=False) + "\n", encoding="utf-8")
    print(f"Captured {len(observations)} raw RUSTXES observations")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
