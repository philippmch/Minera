"""Check public RUSTXES gzip behavior against saved public observations.

The Rust/native transport checks already exercise gzip files.  This checker
keeps the public side independent: it replays each saved observation through
PM4Py's public ``RUSTXES`` importer with a deterministic gzip input and
compares the captured value or error with the saved result.

Only JSON primitives are passed through automatically.  The capture format
also records a small number of public Python objects used by the extension
tests; those objects are rebuilt only when their explicit type marker and
public enum metadata are present.  Anything else is reported as a skip so a
missing reconstruction never becomes a false parity result.
"""

from __future__ import annotations

import argparse
import copy
import datetime
import gzip
import hashlib
import inspect
import json
import math
import os
import platform
import re
import sys
import tempfile
import warnings
from importlib.metadata import version as package_version
from pathlib import Path
from types import SimpleNamespace


REFERENCE_VERSION = "2.7.22"
BACKEND_VERSION = "0.6.2"
REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"
REFERENCE_COMMIT_SCOPE = "configured_reference_pin"
CHECKER_VERSION = 1

# Keep public imports lazy.  In particular, callers should be able to inspect
# this checker and run its offline tests without installing PM4Py or r4pm.
pd = pl = pm4py = importer = XESExtension = None


class UnsupportedParameter(ValueError):
    """A captured parameter cannot be reconstructed safely from JSON."""


class MalformedObservation(ValueError):
    """A saved observation is missing data required for a replay."""

    def __init__(self, message, *, raw=None):
        super().__init__(message)
        self.raw = raw


class CaptureError(RuntimeError):
    """The public return value cannot be represented by the capture schema."""


def _load_public_reference_api():
    """Load PM4Py and its optional table dependencies after warning filters."""
    global pd, pl, pm4py, importer, XESExtension

    # This must precede every public package import.  The pinned r4pm backend
    # can emit dependency warnings while it is imported.
    warnings.filterwarnings("ignore")
    import pm4py as pm4py_module
    from pm4py.objects.log.importer.xes import importer as importer_module

    try:
        import pandas as pandas_module
    except BaseException:
        pandas_module = None
    try:
        import polars as polars_module
    except BaseException:
        polars_module = None
    try:
        from pm4py.objects.log.obj import XESExtension as extension_type
    except (ImportError, AttributeError):
        extension_type = None

    pd, pl, pm4py = pandas_module, polars_module, pm4py_module
    importer, XESExtension = importer_module, extension_type
    return pm4py, importer, pd, pl, XESExtension


def _require_version(pm4py_module):
    """Reject a runtime from another PM4Py release, including under ``-O``."""
    actual = getattr(pm4py_module, "__version__", None)
    if actual != REFERENCE_VERSION:
        raise RuntimeError(
            f"This checker requires pm4py=={REFERENCE_VERSION}; got {actual!r}"
        )


def _require_backend(actual):
    """Reject a runtime from another r4pm release, including under ``-O``."""
    if actual != BACKEND_VERSION:
        raise RuntimeError(
            f"This checker requires r4pm=={BACKEND_VERSION}; got {actual!r}"
        )


def _environment():
    return {
        "PYTHONHASHSEED": os.environ.get("PYTHONHASHSEED"),
        "TZ": os.environ.get("TZ"),
        "python": platform.python_version(),
        "implementation": platform.python_implementation(),
    }


def _file_metadata(path):
    """Return a path and SHA-256 without decoding the file."""
    if not path:
        return None
    resolved = Path(path).resolve()
    digest = hashlib.sha256()
    try:
        with resolved.open("rb") as source:
            for chunk in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(chunk)
    except OSError as error:
        return {"path": str(resolved), "error": f"{type(error).__name__}: {error}"}
    return {"path": str(resolved), "sha256": digest.hexdigest()}


def _module_metadata(module):
    return _file_metadata(getattr(module, "__file__", None)) if module else None


def _safe_package_version(name):
    try:
        return package_version(name)
    except Exception:
        return None


def _reject_json_constant(value):
    raise ValueError(f"non-finite JSON constant {value!r} is not allowed")


def _load_json(path):
    """Load JSON and return its original bytes for one-pass provenance."""
    source = Path(path)
    try:
        raw = source.read_bytes()
        document = json.loads(
            raw.decode("utf-8"),
            parse_constant=_reject_json_constant,
        )
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as error:
        raise MalformedObservation(
            f"unable to read JSON {source.resolve()}: {type(error).__name__}: {error}",
            raw=locals().get("raw"),
        ) from error
    return document, raw


def _raw_file_metadata(path, raw):
    """Describe already-read bytes without opening the path a second time."""
    return {
        "path": str(Path(path).resolve()),
        "sha256": hashlib.sha256(raw).hexdigest(),
    }


def normalize(value):
    """Use the same public value normalization as the Rust capture scripts."""
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
        return {
            "float": (
                "nan" if math.isnan(value) else "inf" if value > 0 else "-inf"
            )
        }
    item = getattr(value, "item", None)
    if item is not None and not isinstance(value, (str, bytes)):
        try:
            return normalize(item())
        except (TypeError, ValueError):
            pass
    return value


def _dataframe_dtypes(value, columns):
    dtypes = getattr(value, "dtypes", None)
    if dtypes is None:
        raise CaptureError("DataFrame has no dtypes")
    if hasattr(dtypes, "items"):
        pairs = dtypes.items()
    else:
        pairs = zip(columns, dtypes)
    return {
        str(column): str(dtype)
        for column, dtype in sorted(pairs, key=lambda item: str(item[0]))
    }


def capture_value(value, include_metadata=False, include_row_count=False):
    """Capture EventLog/DataFrame/LazyFrame values in the saved schema."""
    kind = type(value).__name__
    result = {"type": kind, "module": type(value).__module__}
    if kind == "EventLog":
        if include_metadata:
            metadata = {}
            for name in (
                "attributes",
                "extensions",
                "classifiers",
                "properties",
                "omni_present",
            ):
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
        columns = [str(column) for column in value.columns]
        try:
            rows = value.to_dict(orient="records")
        except (AttributeError, TypeError):
            try:
                rows = value.to_dicts()
            except AttributeError as error:
                raise CaptureError(f"DataFrame rows unavailable: {error}") from error
        result.update(
            columns=sorted(columns),
            dtypes=_dataframe_dtypes(value, columns),
            rows=[normalize(row) for row in rows],
        )
        if include_row_count:
            result["row_count"] = len(rows)
        return result
    if kind == "LazyFrame":
        try:
            materialized = value.collect()
            columns = [str(column) for column in materialized.columns]
            schema = getattr(materialized, "schema", None)
            if schema is None:
                schema = dict(zip(columns, getattr(materialized, "dtypes", [])))
            rows = materialized.to_dicts()
        except (AttributeError, TypeError, ValueError) as error:
            raise CaptureError(f"LazyFrame materialization failed: {error}") from error
        result.update(
            columns=sorted(columns),
            dtypes={
                str(column): str(dtype)
                for column, dtype in sorted(schema.items(), key=lambda item: str(item[0]))
            },
            rows=[normalize(row) for row in rows],
        )
        if include_row_count:
            result["row_count"] = len(rows)
        return result
    raise CaptureError(f"Unexpected RUSTXES return type: {kind}")


def _panic_markers(text):
    markers = []
    for line in str(text).splitlines():
        stripped = line.strip()
        if "not yet implemented:" in stripped or "PanicException:" in stripped:
            markers.append(stripped)
    return markers


def _strict_difference(expected, actual, path="$", limit=1):
    """Return a compact typed first difference, or ``None`` when equal."""
    if type(expected) is not type(actual):
        return {
            "path": path,
            "expected": expected,
            "actual": actual,
            "reason": f"type differs ({type(expected).__name__} vs {type(actual).__name__})",
        }
    if isinstance(expected, dict):
        expected_keys, actual_keys = set(expected), set(actual)
        if expected_keys != actual_keys:
            return {
                "path": path,
                "expected_keys": sorted(expected_keys),
                "actual_keys": sorted(actual_keys),
                "reason": "mapping keys differ",
            }
        for key in sorted(expected_keys, key=str):
            difference = _strict_difference(
                expected[key], actual[key], f"{path}.{key}", limit
            )
            if difference:
                return difference
        return None
    if isinstance(expected, list):
        if len(expected) != len(actual):
            return {
                "path": path,
                "expected_length": len(expected),
                "actual_length": len(actual),
                "reason": "list lengths differ",
            }
        for index, (left, right) in enumerate(zip(expected, actual)):
            difference = _strict_difference(left, right, f"{path}[{index}]", limit)
            if difference:
                return difference
        return None
    if expected != actual:
        return {
            "path": path,
            "expected": expected,
            "actual": actual,
            "reason": "values differ",
        }
    return None


def _positive_int(value):
    try:
        parsed = int(value)
    except (TypeError, ValueError) as error:
        raise argparse.ArgumentTypeError("support sample count must be an integer") from error
    if parsed <= 0:
        raise argparse.ArgumentTypeError("support sample count must be positive")
    return parsed


def _outcome_key(value):
    """Serialize an outcome without changing generated names or value types."""
    try:
        return json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False)
    except (TypeError, ValueError) as error:
        raise CaptureError(f"outcome is not JSON-safe: {error}") from error


def _support_summary(
    expected, plain_outcomes, gzip_outcomes, initial_gzip_outcome=None
):
    """Summarize exact sampled supports and counts for paired replays."""
    def counts(outcomes):
        buckets = {}
        for outcome in outcomes:
            key = _outcome_key(outcome)
            if key not in buckets:
                buckets[key] = {"outcome": outcome, "count": 0}
            buckets[key]["count"] += 1
        return [buckets[key] for key in sorted(buckets)]

    plain = counts(plain_outcomes)
    compressed = counts(gzip_outcomes)
    plain_by_key = {_outcome_key(item["outcome"]): item["count"] for item in plain}
    gzip_by_key = {_outcome_key(item["outcome"]): item["count"] for item in compressed}
    expected_key = _outcome_key(expected)
    initial_gzip_key = (
        _outcome_key(initial_gzip_outcome)
        if initial_gzip_outcome is not None
        else None
    )
    return {
        "sample_count": len(plain_outcomes),
        "plain": {"sample_count": len(plain_outcomes), "outcomes": plain},
        "gzip": {"sample_count": len(gzip_outcomes), "outcomes": compressed},
        "historical_expected_in_plain_support": expected_key in plain_by_key,
        "historical_expected_in_gzip_support": expected_key in gzip_by_key,
        "initial_gzip_outcome": initial_gzip_outcome,
        "initial_gzip_outcome_in_plain_support": (
            initial_gzip_key is not None and initial_gzip_key in plain_by_key
        ),
        "initial_gzip_outcome_in_gzip_support": (
            initial_gzip_key is not None and initial_gzip_key in gzip_by_key
        ),
        # This is exact support-set equality.  Generated column_N names remain
        # untouched in both outcome keys; no distribution claim is made.
        "support_equal": set(plain_by_key) == set(gzip_by_key),
    }


def _plain_bytes(xml):
    try:
        raw = xml.encode("utf-8")
    except (AttributeError, UnicodeError) as error:
        raise MalformedObservation(f"XML is not UTF-8 text: {error}") from error
    metadata = {
        "encoding": "utf-8",
        "uncompressed_bytes": len(raw),
        "uncompressed_sha256": hashlib.sha256(raw).hexdigest(),
    }
    return raw, metadata


def _gzip_bytes(xml):
    raw, metadata = _plain_bytes(xml)
    # mtime=0 makes the report's compressed-input hash reproducible.
    compressed = gzip.compress(raw, mtime=0)
    metadata.update(
        {
            "compressed_bytes": len(compressed),
            "compressed_sha256": hashlib.sha256(compressed).hexdigest(),
        }
    )
    return compressed, metadata


def _enum_member(name, reference, enum_type):
    if not isinstance(name, str) or not name.startswith("XESExtension."):
        raise UnsupportedParameter(f"unsupported extension enum marker {name!r}")
    member_name = name.split(".", 1)[1]
    enum_metadata = reference.get("public_xes_extension_enum")
    members = enum_metadata.get("members") if isinstance(enum_metadata, dict) else None
    if not isinstance(members, dict) or member_name not in members:
        raise UnsupportedParameter(
            f"{name} has no clear public_xes_extension_enum metadata"
        )
    if enum_type is None:
        raise UnsupportedParameter(f"public XESExtension is unavailable for {name}")
    try:
        member = getattr(enum_type, member_name)
    except AttributeError as error:
        raise UnsupportedParameter(f"public XESExtension has no {member_name}") from error
    stored_value = members[member_name]
    try:
        current_value = list(member.value)
    except (AttributeError, TypeError) as error:
        raise UnsupportedParameter(f"public enum value for {name} is not inspectable") from error
    if current_value != stored_value:
        raise UnsupportedParameter(
            f"stored public enum metadata disagrees for {name}"
        )
    return member


def _decode_parameter(value, reference, enum_type, path="parameter"):
    """Decode JSON primitives and explicit capture markers safely."""
    if value is None or isinstance(value, (str, bool, int, float)):
        if isinstance(value, float) and not math.isfinite(value):
            raise UnsupportedParameter(f"non-finite parameter at {path}")
        if path.endswith(".extensions") or path == "extensions":
            # An ordinary string remains an ordinary string.  Enum markers are
            # handled in the recursive branch below where they are explicit.
            pass
        return value
    if isinstance(value, list):
        return [
            _decode_parameter(item, reference, enum_type, f"{path}[{index}]")
            for index, item in enumerate(value)
        ]
    if isinstance(value, dict):
        if set(value) == {"tuple"}:
            items = value["tuple"]
            if not isinstance(items, list):
                raise UnsupportedParameter(f"tuple marker at {path} is malformed")
            return tuple(
                _decode_parameter(item, reference, enum_type, f"{path}.tuple[{index}]")
                for index, item in enumerate(items)
            )
        if set(value) == {"SimpleNamespace"}:
            descriptor = value["SimpleNamespace"]
            if not isinstance(descriptor, dict):
                raise UnsupportedParameter(
                    f"SimpleNamespace marker at {path} is malformed"
                )
            return SimpleNamespace(
                **{
                    str(key): _decode_parameter(
                        item, reference, enum_type, f"{path}.{key}"
                    )
                    for key, item in descriptor.items()
                }
            )
        return {
            str(key): _decode_parameter(item, reference, enum_type, f"{path}.{key}")
            for key, item in value.items()
        }
    raise UnsupportedParameter(
        f"parameter at {path} has unsupported JSON type {type(value).__name__}"
    )


def _decode_extension_markers(value, reference, enum_type, path="extensions"):
    """Decode extension enum strings after checking explicit capture metadata."""
    if isinstance(value, str) and value.startswith("XESExtension."):
        return _enum_member(value, reference, enum_type)
    if isinstance(value, list):
        return [
            _decode_extension_markers(item, reference, enum_type, f"{path}[{i}]")
            for i, item in enumerate(value)
        ]
    if isinstance(value, dict):
        if set(value) == {"tuple"}:
            items = value["tuple"]
            if not isinstance(items, list):
                raise UnsupportedParameter(f"tuple marker at {path} is malformed")
            return tuple(
                _decode_extension_markers(item, reference, enum_type, f"{path}[{i}]")
                for i, item in enumerate(items)
            )
        if set(value) == {"SimpleNamespace"}:
            descriptor = value["SimpleNamespace"]
            if not isinstance(descriptor, dict):
                raise UnsupportedParameter(f"SimpleNamespace marker at {path} is malformed")
            return SimpleNamespace(
                **{
                    str(key): _decode_parameter(item, reference, enum_type, f"{path}.{key}")
                    for key, item in descriptor.items()
                }
            )
        return {
            str(key): _decode_extension_markers(item, reference, enum_type, f"{path}.{key}")
            for key, item in value.items()
        }
    return value


def decode_parameters(parameters, reference, enum_type):
    if not isinstance(parameters, dict):
        raise MalformedObservation("parameters must be a JSON object")
    decoded = {}
    for key, value in parameters.items():
        if not isinstance(key, str):
            raise MalformedObservation("parameter keys must be strings")
        if key == "extensions":
            decoded[key] = _decode_extension_markers(
                value, reference, enum_type, "extensions"
            )
        else:
            decoded[key] = _decode_parameter(
                value, reference, enum_type, f"parameters.{key}"
            )
    return decoded


def _expected_contract(result):
    if not isinstance(result, dict):
        raise MalformedObservation("observation result must be a JSON object")
    if "status" in result:
        status = result.get("status")
        if status == "ok":
            if "result" not in result:
                raise MalformedObservation("ok result is missing result payload")
            if not isinstance(result["result"], dict):
                raise MalformedObservation("ok result payload must be an object")
            return "status", result
        if status == "exception":
            for key in ("exception", "message"):
                if not isinstance(result.get(key), str):
                    raise MalformedObservation(
                        f"exception result is missing string {key!r}"
                    )
            return "status", result
        raise MalformedObservation(f"unknown saved result status {status!r}")
    if "error" in result:
        if not isinstance(result.get("error"), str) or not isinstance(
            result.get("message"), str
        ):
            raise MalformedObservation("error result needs string error/message")
        return "direct", result
    return "direct", result


def _public_call(path, parameters, expected, api):
    pm4py_module, importer_module, _, _, _ = api
    del pm4py_module
    try:
        value = importer_module.apply(
            str(path), variant=importer_module.Variants.RUSTXES, parameters=parameters
        )
        expected_payload = expected.get("result", expected)
        include_metadata = "metadata" in expected_payload
        include_row_count = "row_count" in expected_payload
        captured = capture_value(
            value,
            include_metadata=include_metadata,
            include_row_count=include_row_count,
        )
        if expected.get("status") == "ok":
            return {"status": "ok", "result": captured}
        return captured
    except BaseException as error:  # pyo3_runtime.PanicException is not Exception.
        markers = _panic_markers(str(error))
        if expected.get("status") == "exception":
            result = {
                "status": "exception",
                "exception": type(error).__name__,
                "message": str(error),
            }
            if markers:
                result["panic_markers"] = markers
            return result
        return {"error": type(error).__name__, "message": str(error)}


def _sample_support(
    xml,
    parameters,
    expected,
    api,
    directory,
    index,
    sample_count,
    initial_gzip_outcome=None,
):
    """Replay paired plain/gzip inputs and retain every exact outcome count."""
    plain_data, plain_input = _plain_bytes(xml)
    gzip_data, gzip_input = _gzip_bytes(xml)
    plain_path = Path(directory) / f"observation-{index}.xes"
    gzip_path = Path(directory) / f"observation-{index}.xes.gz"
    plain_path.write_bytes(plain_data)
    gzip_path.write_bytes(gzip_data)
    plain_outcomes = []
    gzip_outcomes = []
    for _ in range(sample_count):
        # Each replay receives a fresh parameter mapping so a public importer
        # cannot make later samples depend on an earlier call's mutation.
        plain_outcomes.append(
            _public_call(plain_path, copy.deepcopy(parameters), expected, api)
        )
        gzip_outcomes.append(
            _public_call(gzip_path, copy.deepcopy(parameters), expected, api)
        )
    summary = _support_summary(
        expected,
        plain_outcomes,
        gzip_outcomes,
        initial_gzip_outcome=initial_gzip_outcome,
    )
    summary["plain_input"] = plain_input
    summary["gzip_input"] = gzip_input
    return summary


def _observation_label(observation, index):
    if isinstance(observation.get("observation_id"), str):
        return observation["observation_id"]
    if isinstance(observation.get("name"), str):
        return observation["name"]
    if isinstance(observation.get("case"), str):
        return observation["case"]
    return f"observation_{index}"


def _reference_metadata_errors(reference, observations):
    """Validate capture provenance and optional observation manifests."""
    errors = []
    required_values = (
        ("reference", REFERENCE_VERSION),
        ("reference_commit", REFERENCE_COMMIT),
        ("reference_commit_scope", REFERENCE_COMMIT_SCOPE),
        ("r4pm", BACKEND_VERSION),
    )
    for field, expected in required_values:
        if reference.get(field) != expected:
            errors.append(
                {
                    "kind": "reference_metadata",
                    "field": field,
                    "expected": expected,
                    "actual": reference.get(field),
                    "message": (
                        f"saved reference {field} {reference.get(field)!r} "
                        f"does not match configured {expected!r}"
                    ),
                }
            )

    # Captures use ``reference`` for the PM4Py release.  Accept an explicit
    # pm4py alias when present, but never let it contradict the configured pin.
    if "pm4py" in reference and reference.get("pm4py") != REFERENCE_VERSION:
        errors.append(
            {
                "kind": "reference_metadata",
                "field": "pm4py",
                "expected": REFERENCE_VERSION,
                "actual": reference.get("pm4py"),
                "message": "saved reference pm4py version is not configured PM4Py",
            }
        )

    module = reference.get("reference_module")
    module_hash = module.get("sha256") if isinstance(module, dict) else None
    if not isinstance(module_hash, str) or re.fullmatch(r"[0-9a-fA-F]{64}", module_hash) is None:
        errors.append(
            {
                "kind": "reference_metadata",
                "field": "reference_module.sha256",
                "actual": module_hash,
                "message": "reference_module must include a 64-hex sha256",
            }
        )

    for field in ("errors", "validation_errors", "capture_errors"):
        value = reference.get(field)
        if value:
            errors.append(
                {
                    "kind": "reference_capture_errors",
                    "field": field,
                    "actual": value,
                    "message": f"reference capture contains non-empty {field}",
                }
            )

    if not isinstance(observations, list):
        return errors
    if not observations:
        errors.append(
            {
                "kind": "reference_observations",
                "message": "reference observations/rows must be non-empty",
            }
        )
        return errors

    identities = []
    for index, observation in enumerate(observations):
        if not isinstance(observation, dict):
            identities.append(None)
            continue
        identity = None
        for field in ("observation_id", "name", "case"):
            candidate = observation.get(field)
            if isinstance(candidate, str) and candidate.strip():
                identity = candidate
                break
        identities.append(identity)
    if any(identity is None for identity in identities):
        errors.append(
            {
                "kind": "reference_observations",
                "message": "every observation must have a non-empty observation_id, name, or case",
            }
        )
    if len(set(identity for identity in identities if identity is not None)) != len(
        [identity for identity in identities if identity is not None]
    ):
        errors.append(
            {
                "kind": "reference_observations",
                "message": "observation identities must be unique",
            }
        )

    expected_count = reference.get("expected_observation_count")
    if "expected_observation_count" in reference:
        if isinstance(expected_count, bool) or not isinstance(expected_count, int):
            errors.append(
                {
                    "kind": "reference_manifest",
                    "field": "expected_observation_count",
                    "actual": expected_count,
                    "message": "expected_observation_count must be an integer",
                }
            )
        elif expected_count != len(observations):
            errors.append(
                {
                    "kind": "reference_manifest",
                    "field": "expected_observation_count",
                    "expected": expected_count,
                    "actual": len(observations),
                    "message": "observation count does not match manifest",
                }
            )

    if "expected_observation_identities" in reference:
        expected_ids = reference.get("expected_observation_identities")
        if not isinstance(expected_ids, list) or not all(
            isinstance(identity, str) and identity.strip() for identity in expected_ids
        ):
            errors.append(
                {
                    "kind": "reference_manifest",
                    "field": "expected_observation_identities",
                    "actual": expected_ids,
                    "message": "expected_observation_identities must be non-empty strings",
                }
            )
        elif len(set(expected_ids)) != len(expected_ids):
            errors.append(
                {
                    "kind": "reference_manifest",
                    "field": "expected_observation_identities",
                    "actual": expected_ids,
                    "message": "expected observation identities must be unique",
                }
            )
        elif expected_ids != identities:
            errors.append(
                {
                    "kind": "reference_manifest",
                    "field": "expected_observation_identities",
                    "expected": expected_ids,
                    "actual": identities,
                    "message": "observation identities do not match manifest",
                }
            )
    return errors


def _validate_observation(observation, index):
    if not isinstance(observation, dict):
        raise MalformedObservation("observation must be a JSON object")
    for key in ("xml", "parameters", "result"):
        if key not in observation:
            raise MalformedObservation(f"missing observation field {key!r}")
    if not isinstance(observation["xml"], str):
        raise MalformedObservation("observation xml must be a string")
    _expected_contract(observation["result"])
    return _observation_label(observation, index)


def _base_report(reference_path, reference=None, api=None, reference_bytes=None):
    reference_json = (
        _raw_file_metadata(reference_path, reference_bytes)
        if reference_bytes is not None
        else _file_metadata(reference_path)
    )
    report = {
        "checker": "check-xes-rust-public-gzip",
        "checker_version": CHECKER_VERSION,
        "native_independent": True,
        "configured_reference": {
            "version": REFERENCE_VERSION,
            "commit": REFERENCE_COMMIT,
            "commit_scope": REFERENCE_COMMIT_SCOPE,
        },
        "environment": _environment(),
        "reference_json": reference_json,
        "reference_artifact_commit": (
            reference.get("reference_commit") if isinstance(reference, dict) else None
        ),
        "reference_module": _module_metadata(api[1]) if api else None,
        "pm4py_module": _module_metadata(api[0]) if api else None,
        "pm4py": getattr(api[0], "__version__", None) if api else None,
        "r4pm": _safe_package_version("r4pm") if api else None,
        "public_signature": None,
        "checks": [],
        "skipped": [],
        "malformed": [],
    }
    if api:
        try:
            report["public_signature"] = str(
                inspect.signature(api[1].Variants.RUSTXES.value.apply)
            )
        except (AttributeError, TypeError, ValueError):
            report["public_signature"] = None
    return report


def _write_report(path, report):
    strict_failed = sum(not check["match"] for check in report["checks"])
    sampled_support_matches = sum(
        check.get("sampled_support_match", False) for check in report["checks"]
    )
    unresolved = sum(
        not check["match"] and not check.get("sampled_support_match", False)
        for check in report["checks"]
    )
    report["summary"] = {
        "checked": len(report["checks"]),
        "matched": sum(check["match"] for check in report["checks"]),
        # ``failed`` remains the strict result count even when an explicitly
        # requested support sample classifies a mismatch as support-equivalent.
        "failed": strict_failed,
        "sampled_support_matches": sampled_support_matches,
        "unresolved": unresolved,
        "skipped": len(report["skipped"]),
        "malformed": len(report["malformed"]),
    }
    output = Path(path)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(report, indent=2, sort_keys=True, allow_nan=False) + "\n",
        encoding="utf-8",
    )


def main():
    parser = argparse.ArgumentParser(
        description="Replay saved public RUSTXES observations with gzip input."
    )
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--support-samples",
        type=_positive_int,
        help=(
            "for strict mismatches, replay plain and gzip inputs this many "
            "times and compare their exact sampled supports"
        ),
    )
    parser.add_argument(
        "--skip-unsupported-parameters",
        action="store_true",
        help="allow explicitly named observations that cannot be reconstructed from JSON",
    )
    args = parser.parse_args()

    reference = None
    reference_bytes = None
    try:
        reference, reference_bytes = _load_json(args.reference)
    except MalformedObservation as error:
        report = _base_report(
            args.reference,
            reference_bytes=getattr(error, "raw", None),
        )
        report["support_samples"] = args.support_samples
        report["skip_unsupported_parameters"] = args.skip_unsupported_parameters
        report["malformed"].append({"kind": "reference", "message": str(error)})
        _write_report(args.output, report)
        print(f"0/0 public gzip checks matched; malformed reference: {error}")
        return 1

    report = _base_report(args.reference, reference, reference_bytes=reference_bytes)
    report["support_samples"] = args.support_samples
    report["skip_unsupported_parameters"] = args.skip_unsupported_parameters
    if not isinstance(reference, dict):
        report["malformed"].append(
            {"kind": "reference", "message": "reference document must be an object"}
        )
        _write_report(args.output, report)
        return 1
    if "observations" in reference:
        observations = reference["observations"]
    elif "rows" in reference:
        observations = reference["rows"]
    else:
        observations = None
        report["malformed"].append(
            {"kind": "reference", "message": "reference has neither observations nor rows"}
        )
    if observations is not None and not isinstance(observations, list):
        report["malformed"].append(
            {"kind": "reference", "message": "observations/rows must be a list"}
        )
        observations = []
    report["malformed"].extend(_reference_metadata_errors(reference, observations))

    try:
        api = _load_public_reference_api()
        _require_version(api[0])
        runtime_r4pm = _safe_package_version("r4pm")
        _require_backend(runtime_r4pm)
        report["reference_module"] = _module_metadata(api[1])
        report["pm4py_module"] = _module_metadata(api[0])
        report["pm4py"] = getattr(api[0], "__version__", None)
        report["r4pm"] = runtime_r4pm
        try:
            report["public_signature"] = str(
                inspect.signature(api[1].Variants.RUSTXES.value.apply)
            )
        except (AttributeError, TypeError, ValueError):
            pass
    except BaseException as error:
        report["malformed"].append(
            {
                "kind": "public_runtime",
                "error": type(error).__name__,
                "message": str(error),
            }
        )
        _write_report(args.output, report)
        print(f"0/0 public gzip checks matched; public runtime error: {error}")
        return 1

    with tempfile.TemporaryDirectory(prefix="minera-rust-public-gzip-") as directory:
        input_path = Path(directory) / "input.xes.gz"
        for index, observation in enumerate(observations or []):
            label = _observation_label(observation, index) if isinstance(observation, dict) else f"observation_{index}"
            try:
                _validate_observation(observation, index)
                _, expected = _expected_contract(observation["result"])
                parameters = decode_parameters(
                    observation["parameters"], reference, api[4]
                )
                compressed, input_metadata = _gzip_bytes(observation["xml"])
            except (MalformedObservation, UnsupportedParameter) as error:
                if isinstance(error, UnsupportedParameter):
                    skipped = {
                        "index": index,
                        "name": label,
                        "reason": str(error),
                        "parameters": observation.get("parameters")
                        if isinstance(observation, dict)
                        else None,
                    }
                    if isinstance(observation, dict):
                        for field in ("case", "mode", "spec"):
                            if field in observation:
                                skipped[field] = observation[field]
                    report["skipped"].append(skipped)
                else:
                    report["malformed"].append(
                        {"index": index, "name": label, "message": str(error)}
                    )
                continue

            input_path.write_bytes(compressed)
            actual = _public_call(input_path, parameters, expected, api)
            difference = _strict_difference(expected, actual)
            check = {
                "index": index,
                "name": label,
                "parameters": observation["parameters"],
                "gzip": True,
                "input": input_metadata,
                "match": difference is None,
                "expected": expected,
                "actual": actual,
            }
            for field in ("case", "mode", "spec"):
                if field in observation:
                    check[field] = observation[field]
            if difference is not None:
                check["difference"] = difference
                if args.support_samples is not None:
                    try:
                        support = _sample_support(
                            observation["xml"],
                            parameters,
                            expected,
                            api,
                            directory,
                            index,
                            args.support_samples,
                            initial_gzip_outcome=actual,
                        )
                        check["support_sampling"] = support
                        check["sampled_support_match"] = bool(
                            support["support_equal"]
                            and support["historical_expected_in_plain_support"]
                            and support["historical_expected_in_gzip_support"]
                            and support["initial_gzip_outcome_in_plain_support"]
                            and support["initial_gzip_outcome_in_gzip_support"]
                        )
                        if check["sampled_support_match"]:
                            check["classification"] = "sampled_support_match"
                    except BaseException as error:
                        check["support_sampling_error"] = {
                            "error": type(error).__name__,
                            "message": str(error),
                        }
                        report["malformed"].append(
                            {
                                "index": index,
                                "name": label,
                                "kind": "support_sampling",
                                "message": str(error),
                            }
                        )
            report["checks"].append(check)

    _write_report(args.output, report)
    summary = report["summary"]
    print(
        f"{summary['matched']}/{summary['checked']} public gzip checks matched; "
        f"{summary['skipped']} skipped; {summary['malformed']} malformed; "
        f"{summary['sampled_support_matches']} sampled support matches"
    )
    for check in report["checks"]:
        if not check["match"]:
            print(json.dumps(check, sort_keys=True))
            break
    return 1 if (
        summary["unresolved"]
        or summary["malformed"]
        or summary["checked"] == 0
        or (summary["skipped"] and not args.skip_unsupported_parameters)
    ) else 0


if __name__ == "__main__":
    raise SystemExit(main())
