"""Capture public RUSTXES conversion extension-parameter behavior."""

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
from types import SimpleNamespace
import warnings
from xml.sax.saxutils import quoteattr


REFERENCE_VERSION = "2.7.22"
BACKEND_VERSION = "0.6.2"
REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"
REFERENCE_COMMIT_SCOPE = "configured_reference_pin"
pd = pl = pm4py = XESExtension = importer = None


def _load_public_reference_api():
    """Load the pinned public packages only when a capture is requested."""
    global pd, pl, pm4py, XESExtension, importer
    warnings.filterwarnings("ignore")
    import pandas as pd_module
    import polars as pl_module
    import pm4py as pm4py_module
    from pm4py.objects.log.obj import XESExtension as extension_type
    from pm4py.objects.log.importer.xes import importer as importer_module
    pd, pl, pm4py = pd_module, pl_module, pm4py_module
    XESExtension, importer = extension_type, importer_module
    return pm4py, pd, pl, XESExtension, importer


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


def parameter_repr(value):
    """Represent enum/object parameters without losing their public fields."""
    if XESExtension is not None and isinstance(value, XESExtension):
        return f"XESExtension.{value.name}"
    if isinstance(value, SimpleNamespace):
        return {"SimpleNamespace": {key: parameter_repr(item) for key, item in vars(value).items()}}
    if isinstance(value, dict):
        return {str(key): parameter_repr(item) for key, item in value.items()}
    if isinstance(value, tuple):
        return {"tuple": [parameter_repr(item) for item in value]}
    if isinstance(value, list):
        return [parameter_repr(item) for item in value]
    return value


def attribute(kind, key, value=_MISSING, null=False):
    parts = [f"<{kind} key={quoteattr(str(key))}"]
    if value is not _MISSING:
        parts.append(f" value={quoteattr(str(value))}")
    if null:
        parts.append(' null="true"')
    return "".join(parts) + "/>"


def event(name="A", extra=""):
    return "<event>" + attribute("string", "concept:name", name) + extra + "</event>"


def trace(name="c", events=None, extra=""):
    return "<trace>" + attribute("string", "concept:name", name) + extra + (event() if events is None else events) + "</trace>"


NONEMPTY_XML = (
    '<log><extension name="Declared" prefix="decl" uri="urn:decl"/>'
    + trace(
        "c",
        event(
            "A",
            attribute("string", "org:resource", "r")
            + attribute("date", "time:timestamp", "2024-01-01T00:00:00Z"),
        ),
        attribute("string", "case:group", "g"),
    )
    + "</log>"
)


EMPTY_XML = '<log><extension name="Declared" prefix="decl" uri="urn:decl"/></log>'


CAPTURE_ID = "rustxes_conversion_extensions_reference"
EXPECTED_OBSERVATION_COUNT = 26
EXPECTED_OBSERVATION_IDENTITIES = (
    "nonempty_omitted", "nonempty_none", "nonempty_list_empty", "nonempty_tuple_empty",
    "nonempty_dict_empty", "nonempty_concept", "nonempty_organizational",
    "nonempty_concept_organizational", "nonempty_duplicate_concept", "nonempty_concept_tuple_value",
    "nonempty_concept_string", "nonempty_bad_dict", "nonempty_custom_object",
    "nonempty_duplicate_custom_object", "nonempty_duplicate_name_forward", "nonempty_duplicate_name_reverse",
    "nonempty_same_prefix_distinct_names", "nonempty_missing_name_object", "nonempty_missing_prefix_object",
    "empty_omitted", "empty_none", "empty_list_empty", "empty_concept", "empty_custom_object",
    "raw_dataframe_bad_dict", "raw_lazy_concept",
)


def extension_objects():
    custom = SimpleNamespace(name="Custom", prefix="custom", uri="urn:custom")
    duplicate_name_forward = [
        SimpleNamespace(name="Dup", prefix="first", uri="urn:first"),
        SimpleNamespace(name="Dup", prefix="second", uri="urn:second"),
    ]
    duplicate_name_reverse = list(reversed(duplicate_name_forward))
    same_prefix_distinct_names = [
        SimpleNamespace(name="First", prefix="shared", uri="urn:first"),
        SimpleNamespace(name="Second", prefix="shared", uri="urn:second"),
    ]
    missing_name = SimpleNamespace(prefix="custom", uri="urn:custom")
    missing_prefix = SimpleNamespace(name="Custom", uri="urn:custom")
    return {
        "omitted": _MISSING,
        "none": None,
        "list_empty": [],
        "tuple_empty": (),
        "dict_empty": {},
        "concept": [XESExtension.Concept],
        "organizational": [XESExtension.Organizational],
        "concept_organizational": [XESExtension.Concept, XESExtension.Organizational],
        "duplicate_concept": [XESExtension.Concept, XESExtension.Concept],
        "concept_tuple_value": [XESExtension.Concept.value],
        "concept_string": ["Concept"],
        "bad_dict": [{"name": "Custom", "prefix": "custom", "uri": "urn:custom"}],
        "custom_object": [custom],
        "duplicate_custom_object": [custom, custom],
        "duplicate_name_forward": duplicate_name_forward,
        "duplicate_name_reverse": duplicate_name_reverse,
        "same_prefix_distinct_names": same_prefix_distinct_names,
        "missing_name_object": [missing_name],
        "missing_prefix_object": [missing_prefix],
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    pm4py, pd, pl, XESExtension, importer = _load_public_reference_api()
    _require_version(pm4py)
    backend = version("r4pm")
    _require_backend(backend)
    specs = extension_objects()
    calls = []

    def add(name, xml, spec_name, mode="legacy"):
        extension_value = specs[spec_name]
        parameters = {} if extension_value is _MISSING else {"extensions": extension_value}
        calls.append(
            {
                "name": name,
                "xml": xml,
                "spec": spec_name,
                "parameters": parameters,
                "mode": mode,
            }
        )

    for spec_name in specs:
        add("nonempty_" + spec_name, NONEMPTY_XML, spec_name)
    for spec_name in ("omitted", "none", "list_empty", "concept", "custom_object"):
        add("empty_" + spec_name, EMPTY_XML, spec_name)
    add(
        "raw_dataframe_bad_dict",
        NONEMPTY_XML,
        "bad_dict",
        mode="raw_dataframe",
    )
    calls[-1]["parameters"].update({"return_legacy_log_object": False, "return_pl_lazyframe": False})
    add(
        "raw_lazy_concept",
        NONEMPTY_XML,
        "concept",
        mode="raw_lazy",
    )
    calls[-1]["parameters"].update({"return_legacy_log_object": False, "return_pl_lazyframe": True})

    observations = []
    with tempfile.TemporaryDirectory(prefix="minera-rust-conversion-ext-") as directory:
        directory_path = Path(directory)
        for call in calls:
            path = directory_path / f"{call['name']}.xes"
            path.write_text(call["xml"], encoding="utf-8")
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
                    "mode": call["mode"],
                    "spec": call["spec"],
                    "parameters": parameter_repr(call["parameters"]),
                    "xml": call["xml"],
                    "result": result,
                }
            )
    observed_ids = tuple(observation.get("observation_id") for observation in observations)
    if (observed_ids != EXPECTED_OBSERVATION_IDENTITIES
            or len(set(observed_ids)) != EXPECTED_OBSERVATION_COUNT):
        raise RuntimeError("extension capture observation manifest is incomplete or duplicated")

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
        "rustxes_parameters": {parameter.name: parameter.value for parameter in importer.Variants.RUSTXES.value.Parameters},
        "public_xes_extension_enum": {
            "module": XESExtension.__module__,
            "members": {member.name: list(member.value) for member in XESExtension},
        },
        "column_order_note": "Raw table columns and dtype maps are sorted by key; event/trace list order is preserved.",
        "errors": [],
        "observations": observations,
    }
    args.output.write_text(json.dumps(artifact, indent=2, sort_keys=True, allow_nan=False) + "\n", encoding="utf-8")
    print(f"Captured {len(observations)} RUSTXES extension observations")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
