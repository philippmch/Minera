"""Capture public RUSTXES legacy conversion behavior without source reads."""

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
from xml.sax.saxutils import quoteattr
import warnings


REFERENCE_VERSION = "2.7.22"
BACKEND_VERSION = "0.6.2"
REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"
REFERENCE_COMMIT_SCOPE = "configured_reference_pin"
pd = pl = pm4py = converter = to_data_frame = to_event_log = to_event_stream = importer = None


def _load_public_reference_api():
    """Load public PM4Py conversion APIs after warning filters are active."""
    global pd, pl, pm4py, converter, to_data_frame, to_event_log, to_event_stream, importer
    warnings.filterwarnings("ignore")
    import pandas as pd_module
    import polars as pl_module
    import pm4py as pm4py_module
    from pm4py.objects.conversion.log import converter as converter_module
    from pm4py.objects.conversion.log.variants import to_data_frame as data_frame_module
    from pm4py.objects.conversion.log.variants import to_event_log as event_log_module
    from pm4py.objects.conversion.log.variants import to_event_stream as event_stream_module
    from pm4py.objects.log.importer.xes import importer as importer_module
    pd, pl, pm4py = pd_module, pl_module, pm4py_module
    converter = converter_module
    to_data_frame, to_event_log, to_event_stream = (
        data_frame_module, event_log_module, event_stream_module)
    importer = importer_module
    return pm4py, pd, pl, converter, to_data_frame, to_event_log, to_event_stream, importer


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


_MISSING = object()


def normalize(value):
    """Canonicalize mapping order and non-JSON scalar values."""
    if pd is not None and value is pd.NaT:
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
    item = getattr(value, "item", None)
    if item is not None and not isinstance(value, (str, bytes)):
        try:
            return normalize(item())
        except (TypeError, ValueError):
            pass
    return value


def capture_value(value):
    """Capture materialized values, including log metadata and table schemas."""
    kind = type(value).__name__
    result = {"type": kind, "module": type(value).__module__}
    if kind == "EventLog":
        metadata = {}
        for name in ("attributes", "extensions", "classifiers", "properties", "omni_present"):
            try:
                metadata[name] = normalize(getattr(value, name))
            except AttributeError:
                pass
        result["metadata"] = metadata
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
    raise RuntimeError(f"Unexpected RUSTXES return type: {kind}")


def attribute(kind, key, value=_MISSING, null=False):
    parts = [f"<{kind} key={quoteattr(str(key))}"]
    if value is not _MISSING:
        parts.append(f" value={quoteattr(str(value))}")
    if null:
        parts.append(' null="true"')
    return "".join(parts) + "/>"


def event(name="A", extra=""):
    return "<event>" + attribute("string", "concept:name", name) + extra + "</event>"


def trace(name="c", events=None, extra="", kind="string", null_name=False):
    name_attribute = "" if name is None else attribute(kind, "concept:name", name, null=null_name)
    return "<trace>" + name_attribute + extra + (event() if events is None else events) + "</trace>"


SELECTOR_XML = (
    "<log>"
    + trace(
        "t1",
        event("A", attribute("string", "customgroup", "X") + attribute("string", "event:group", "EX1") + attribute("string", "probe", "a"))
        + event("B", attribute("string", "customgroup", "Y") + attribute("string", "event:group", "EY1") + attribute("string", "probe", "b")),
        attribute("string", "case:group", "G") + attribute("string", "traceonly", "t1v"),
    )
    + trace(
        "t2",
        event("C", attribute("string", "customgroup", "X") + attribute("string", "event:group", "EX1") + attribute("string", "probe", "c")),
        attribute("string", "case:group", "G") + attribute("string", "traceonly", "t2v"),
    )
    + trace(
        "t3",
        event("D", attribute("string", "customgroup", "Z") + attribute("string", "event:group", "EZ1") + attribute("string", "probe", "d")),
        attribute("string", "case:group", "H") + attribute("string", "traceonly", "t3v"),
    )
    + "</log>"
)


FIRSTROW_XML = (
    "<log>"
    + trace(
        "first",
        event("A", attribute("string", "gid", "G") + attribute("string", "event_first", "a") + attribute("string", "event_second", "a2"))
        + event("B", attribute("string", "gid", "H") + attribute("string", "event_first", "b") + attribute("string", "event_second", "b2")),
        attribute("string", "case:group", "first-group") + attribute("string", "trace_marker", "first"),
    )
    + trace(
        "later",
        event("C", attribute("string", "gid", "G") + attribute("string", "event_first", "c") + attribute("string", "event_second", "c2")),
        attribute("string", "case:group", "later-group") + attribute("string", "trace_marker", "later"),
    )
    + trace(
        "third",
        event("D", attribute("string", "gid", "H") + attribute("string", "event_first", "d") + attribute("string", "event_second", "d2")),
        attribute("string", "case:group", "third-group") + attribute("string", "trace_marker", "third"),
    )
    + "</log>"
)


NUMERIC_XML = (
    "<log>"
    + trace("int1", event("A", attribute("int", "gid", 1)))
    + trace("int1b", event("B", attribute("int", "gid", 1)))
    + trace("int2", event("C", attribute("int", "gid", 2)))
    + "</log>"
)


BOOL_XML = (
    "<log>"
    + trace("bool1", event("A", attribute("boolean", "gid", "true")))
    + trace("bool2", event("B", attribute("boolean", "gid", "false")))
    + trace("bool3", event("C", attribute("boolean", "gid", "true")))
    + "</log>"
)


MIXED_NUMERIC_XML = (
    "<log>"
    + trace("mixed1", event("A", attribute("int", "gid", 1)))
    + trace("mixed1f", event("B", attribute("float", "gid", "1.0")))
    + trace("mixed2", event("C", attribute("int", "gid", 2)))
    + "</log>"
)


NULL_XML = (
    "<log>"
    + trace("null1", event("A", attribute("string", "gid", null=True)))
    + trace("null2", event("B", attribute("string", "gid", null=True)))
    + trace("null3", event("C", attribute("gid", "x")))
    + "</log>"
)


NAN_XML = (
    "<log>"
    + trace("nan1", event("A", attribute("float", "gid", "NaN")))
    + trace("nan2", event("B", attribute("float", "gid", "NaN")))
    + trace("nan3", event("C", attribute("float", "gid", "1.0")))
    + "</log>"
)


PREFIX_XML = (
    "<log>"
    + trace(
        "c",
        event("A", attribute("string", "org:resource", "r-event")),
        attribute("string", "case:foo", "f1")
        + attribute("string", "case:case:bar", "f2")
        + attribute("string", "xcase:baz", "f3"),
    )
    + "</log>"
)


FLAGS_XML = (
    "<log>"
    + trace(
        "c",
        event("A", attribute("string", "org:resource", "r") + attribute("string", "probe", "e")),
        attribute("string", "case:group", "g") + attribute("string", "trace_probe", "t"),
    )
    + "</log>"
)


CASES = {
    "selector_matrix": SELECTOR_XML,
    "firstrow": FIRSTROW_XML,
    "numeric_group": NUMERIC_XML,
    "boolean_group": BOOL_XML,
    "mixed_numeric_group": MIXED_NUMERIC_XML,
    "null_group": NULL_XML,
    "nan_group": NAN_XML,
    "prefix_matrix": PREFIX_XML,
    "conversion_flags": FLAGS_XML,
    "empty_log": "<log/>",
    "malformed_xml": "<log><trace>",
}


CAPTURE_ID = "rustxes_conversion_reference"
EXPECTED_OBSERVATION_COUNT = 41
EXPECTED_OBSERVATION_IDENTITIES = (
    "selector_default", "selector_event_concept_name", "selector_event_customgroup",
    "selector_event_group", "selector_trace_case_group", "selector_trace_case_group_actual",
    "selector_missing_key", "selector_none_key", "selector_empty_key",
    "firstrow_gid", "numeric_gid", "boolean_gid", "mixed_numeric_gid", "null_gid", "nan_gid",
    "prefix_default", "prefix_case", "prefix_org", "prefix_custom", "prefix_empty", "prefix_short", "prefix_none",
    "flag_stream_postprocessing_true", "flag_stream_postprocessing_false",
    "flag_deepcopy_true", "flag_deepcopy_false",
    "flag_include_case_attributes_true", "flag_include_case_attributes_false",
    "flag_compress_true", "flag_compress_false", "flag_extensions_custom", "flag_all_conversion_flags",
    "raw_dataframe_conversion_options", "raw_lazy_conversion_options",
    "raw_dataframe_null_conversion_options", "raw_lazy_null_conversion_options",
    "empty_log_default", "empty_log_none_case", "empty_log_none_prefix",
    "valid_row_both_none", "malformed_xml_default",
)


def enum_values(enum):
    return {member.name: member.value for member in enum}


def conversion_metadata():
    modules = {
        "to_event_log": to_event_log,
        "to_event_stream": to_event_stream,
        "to_data_frame": to_data_frame,
    }
    return {
        "converter_apply": str(inspect.signature(converter.apply)),
        "variants": {member.name: member.value.__name__ for member in converter.Variants},
        "variants_modules": {
            name: {
                "apply": str(inspect.signature(module.apply)),
                "parameters": enum_values(module.Parameters),
            }
            for name, module in modules.items()
        },
    }


def build_calls():
    calls = []

    def add(name, case, parameters, mode="legacy"):
        calls.append({"name": name, "case": case, "parameters": parameters, "mode": mode})

    selectors = [
        ("default", {}),
        ("event_concept_name", {"pm4py:param:case_id_key": "concept:name"}),
        ("event_customgroup", {"pm4py:param:case_id_key": "customgroup"}),
        ("event_group", {"pm4py:param:case_id_key": "event:group"}),
        ("trace_case_group", {"pm4py:param:case_id_key": "case:group"}),
        ("trace_case_group_actual", {"pm4py:param:case_id_key": "case:case:group"}),
        ("missing_key", {"pm4py:param:case_id_key": "missing"}),
        ("none_key", {"pm4py:param:case_id_key": None}),
        ("empty_key", {"pm4py:param:case_id_key": ""}),
    ]
    for name, parameters in selectors:
        add("selector_" + name, "selector_matrix", parameters)
    add("firstrow_gid", "firstrow", {"pm4py:param:case_id_key": "gid"})
    add("numeric_gid", "numeric_group", {"pm4py:param:case_id_key": "gid"})
    add("boolean_gid", "boolean_group", {"pm4py:param:case_id_key": "gid"})
    add("mixed_numeric_gid", "mixed_numeric_group", {"pm4py:param:case_id_key": "gid"})
    add("null_gid", "null_group", {"pm4py:param:case_id_key": "gid"})
    add("nan_gid", "nan_group", {"pm4py:param:case_id_key": "gid"})

    prefixes = [
        ("default", {}),
        ("case", {"case_attribute_prefix": "case:"}),
        ("org", {"case_attribute_prefix": "org:"}),
        ("custom", {"case_attribute_prefix": "x:"}),
        ("empty", {"case_attribute_prefix": ""}),
        ("short", {"case_attribute_prefix": "case"}),
        ("none", {"case_attribute_prefix": None}),
    ]
    for name, parameters in prefixes:
        add("prefix_" + name, "prefix_matrix", parameters)

    flags = [
        ("stream_postprocessing_true", {"stream_postprocessing": True}),
        ("stream_postprocessing_false", {"stream_postprocessing": False}),
        ("deepcopy_true", {"deepcopy": True}),
        ("deepcopy_false", {"deepcopy": False}),
        ("include_case_attributes_true", {"include_case_attributes": True}),
        ("include_case_attributes_false", {"include_case_attributes": False}),
        ("compress_true", {"compress": True}),
        ("compress_false", {"compress": False}),
        ("extensions_custom", {"extensions": {"Custom": {"prefix": "custom", "uri": "urn:custom"}}}),
        (
            "all_conversion_flags",
            {
                "stream_postprocessing": False,
                "deepcopy": False,
                "include_case_attributes": True,
                "compress": True,
                "extensions": {"Custom": {"prefix": "custom", "uri": "urn:custom"}},
            },
        ),
    ]
    for name, parameters in flags:
        add("flag_" + name, "conversion_flags", parameters)

    add(
        "raw_dataframe_conversion_options",
        "selector_matrix",
        {
            "return_legacy_log_object": False,
            "return_pl_lazyframe": False,
            "pm4py:param:case_id_key": "missing",
            "case_attribute_prefix": "x:",
            "stream_postprocessing": False,
        },
        mode="raw_dataframe",
    )
    add(
        "raw_lazy_conversion_options",
        "selector_matrix",
        {
            "return_legacy_log_object": False,
            "return_pl_lazyframe": True,
            "pm4py:param:case_id_key": "concept:name",
            "case_attribute_prefix": "x:",
            "include_case_attributes": True,
        },
        mode="raw_lazy",
    )
    add(
        "raw_dataframe_null_conversion_options",
        "selector_matrix",
        {
            "return_legacy_log_object": False,
            "return_pl_lazyframe": False,
            "pm4py:param:case_id_key": None,
            "case_attribute_prefix": None,
        },
        mode="raw_dataframe",
    )
    add(
        "raw_lazy_null_conversion_options",
        "selector_matrix",
        {
            "return_legacy_log_object": False,
            "return_pl_lazyframe": True,
            "pm4py:param:case_id_key": None,
            "case_attribute_prefix": None,
        },
        mode="raw_lazy",
    )
    add("empty_log_default", "empty_log", {})
    add("empty_log_none_case", "empty_log", {"pm4py:param:case_id_key": None})
    add("empty_log_none_prefix", "empty_log", {"case_attribute_prefix": None})
    add(
        "valid_row_both_none",
        "conversion_flags",
        {"pm4py:param:case_id_key": None, "case_attribute_prefix": None},
    )
    add("malformed_xml_default", "malformed_xml", {})
    return calls


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    pm4py, pd, pl, converter, to_data_frame, to_event_log, to_event_stream, importer = _load_public_reference_api()
    _require_version(pm4py)
    backend = version("r4pm")
    _require_backend(backend)
    calls = build_calls()
    observations = []
    with tempfile.TemporaryDirectory(prefix="minera-rust-conversion-") as directory:
        directory_path = Path(directory)
        for call in calls:
            path = directory_path / f"{call['name']}.xes"
            path.write_text(CASES[call["case"]], encoding="utf-8")
            try:
                value = importer.apply(
                    str(path),
                    variant=importer.Variants.RUSTXES,
                    parameters=call["parameters"],
                )
                result = capture_value(value)
            except BaseException as error:
                result = {"error": type(error).__name__, "message": str(error)}
            observations.append(
                {
                    "name": call["name"],
                    "observation_id": call["name"],
                    "case": call["case"],
                    "mode": call["mode"],
                    "parameters": call["parameters"],
                    "xml": CASES[call["case"]],
                    "result": result,
                }
            )
    observed_ids = tuple(observation.get("observation_id") for observation in observations)
    if (observed_ids != EXPECTED_OBSERVATION_IDENTITIES
            or len(set(observed_ids)) != EXPECTED_OBSERVATION_COUNT):
        raise RuntimeError("conversion capture observation manifest is incomplete or duplicated")
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
        "rustxes_apply": str(inspect.signature(importer.Variants.RUSTXES.value.apply)),
        "rustxes_parameters": enum_values(importer.Variants.RUSTXES.value.Parameters),
        "conversion": conversion_metadata(),
        "column_order_note": "Raw table columns and dtype maps are sorted by key; event/trace list order is preserved.",
        "errors": [],
        "observations": observations,
    }
    args.output.write_text(json.dumps(artifact, indent=2, sort_keys=True, allow_nan=False) + "\n", encoding="utf-8")
    print(f"Captured {len(observations)} RUSTXES conversion observations")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
