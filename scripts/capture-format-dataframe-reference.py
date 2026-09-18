"""Capture the public PM4Py ``format_dataframe`` contract.

The probe uses only the public function, signature/docstring, pandas objects,
and opaque module-byte pin checks.  It does not inspect PM4Py source, globals,
or bytecode.  Each row keeps a complete public frame description before and
after the call, including values, scalar kinds, missingness, dtypes,
categorical metadata, index labels, and ``DataFrame.attrs``.
"""

import argparse
import hashlib
import importlib
import inspect
import json
import math
import numbers
import subprocess
import warnings
from datetime import date, datetime
from pathlib import Path

warnings.filterwarnings("ignore")

import pandas as pd
import pm4py


REFERENCE_VERSION = "2.7.22"
PANDAS_VERSION = "3.0.5"
REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"


def _missing(value):
    try:
        result = pd.isna(value)
    except Exception:
        return False
    if isinstance(result, bool):
        return result
    # numpy.bool_ is intentionally handled without importing a private or
    # implementation module: scalar pandas values expose a bool conversion.
    if not hasattr(result, "__len__"):
        try:
            return bool(result)
        except (TypeError, ValueError):
            return False
    return False


def normalize(value):
    """Convert public pandas/Python values to deterministic JSON values."""

    if value is None or _missing(value):
        return None
    if isinstance(value, tuple):
        return [normalize(item) for item in value]
    if isinstance(value, list):
        return [normalize(item) for item in value]
    if isinstance(value, dict):
        return {str(key): normalize(item) for key, item in value.items()}
    if isinstance(value, (pd.Timestamp, datetime, date)):
        return value.isoformat()
    if isinstance(value, pd.Timedelta):
        return value.isoformat()
    if isinstance(value, bool):
        return value
    if isinstance(value, numbers.Integral):
        return int(value)
    if isinstance(value, numbers.Real):
        scalar = float(value)
        return (
            scalar
            if math.isfinite(scalar)
            else ("Infinity" if scalar > 0 else "-Infinity")
        )
    if isinstance(value, bytes):
        return {"encoding": "hex", "value": value.hex()}
    if isinstance(value, str):
        return value
    if hasattr(value, "item"):
        try:
            return normalize(value.item())
        except (AttributeError, TypeError, ValueError):
            pass
    return str(value)


def scalar_kind(value):
    if value is None or _missing(value):
        return "null"
    if isinstance(value, tuple):
        return "tuple"
    if isinstance(value, (pd.Timestamp, datetime, date)):
        return "date"
    if isinstance(value, pd.Timedelta):
        return "timedelta"
    if isinstance(value, bool):
        return "bool"
    if isinstance(value, numbers.Integral):
        return "int"
    if isinstance(value, numbers.Real):
        return "float"
    if isinstance(value, bytes):
        return "bytes"
    if isinstance(value, str):
        return "string"
    return type(value).__name__


def typed_value(value):
    if isinstance(value, tuple):
        return {
            "kind": "tuple",
            "value": [typed_value(item) for item in value],
        }
    return {
        "kind": scalar_kind(value),
        "value": normalize(value),
    }


def _column_key(column, position, counts):
    label = str(column)
    return label if counts[label] == 1 else f"{label}#{position}"


def _categorical_dtypes(frame):
    result = {}
    counts = {}
    for column in frame.columns:
        counts[str(column)] = counts.get(str(column), 0) + 1
    for position, column in enumerate(frame.columns):
        dtype = frame.iloc[:, position].dtype
        if isinstance(dtype, pd.CategoricalDtype):
            result[_column_key(column, position, counts)] = {
                "categories_dtype": str(dtype.categories.dtype),
                "ordered": bool(dtype.ordered),
                "categories": [typed_value(value) for value in dtype.categories],
            }
    return result


def describe_frame(frame):
    """Describe a pandas frame while retaining duplicate column positions."""

    if not isinstance(frame, pd.DataFrame):
        return {
            "type": type(frame).__name__,
            "value": normalize(frame),
        }
    counts = {}
    for column in frame.columns:
        counts[str(column)] = counts.get(str(column), 0) + 1
    column_keys = [
        _column_key(column, position, counts)
        for position, column in enumerate(frame.columns)
    ]
    column_metadata = [
        {
            "position": position,
            "label": normalize(column),
            "key": column_keys[position],
            "dtype": str(frame.iloc[:, position].dtype),
        }
        for position, column in enumerate(frame.columns)
    ]
    records = []
    for row_position, index in enumerate(frame.index):
        values = {}
        kinds = {}
        cell_tags = {}
        for column_position, key in enumerate(column_keys):
            value = frame.iloc[row_position, column_position]
            tag = {
                "dtype": str(frame.iloc[:, column_position].dtype),
                "kind": typed_value(value)["kind"],
                "missing": _missing(value),
                "value": normalize(value),
            }
            if isinstance(value, tuple):
                tag["children"] = typed_value(value)["value"]
            values[key] = tag["value"]
            kinds[key] = tag["kind"]
            cell_tags[key] = tag
        records.append(
            {
                "position": row_position,
                "index": normalize(index),
                "values": values,
                "kinds": kinds,
                "cell_tags": cell_tags,
            }
        )
    return {
        "type": type(frame).__name__,
        "columns": [normalize(column) for column in frame.columns],
        "column_metadata": column_metadata,
        "dtypes": {
            key: metadata["dtype"]
            for key, metadata in zip(column_keys, column_metadata)
        },
        "categorical_dtypes": _categorical_dtypes(frame),
        "shape": [int(frame.shape[0]), int(frame.shape[1])],
        "records": records,
        "index_metadata": {
            "class": type(frame.index).__name__,
            "dtype": str(frame.index.dtype),
            "name": normalize(frame.index.name),
            "names": [normalize(name) for name in frame.index.names],
            "values": [normalize(value) for value in frame.index],
            "labels": [typed_value(value) for value in frame.index],
        },
        "attrs": normalize(frame.attrs),
    }


def _frame_equal(left, right):
    if not isinstance(left, pd.DataFrame) or not isinstance(right, pd.DataFrame):
        return left == right
    return (
        left.equals(right)
        and normalize(left.attrs) == normalize(right.attrs)
        and [normalize(value) for value in left.index]
        == [normalize(value) for value in right.index]
        and [normalize(value) for value in left.columns]
        == [normalize(value) for value in right.columns]
    )


def _public_provenance(reference_checkout):
    function = pm4py.format_dataframe
    module_name = getattr(function, "__module__", "")
    module = importlib.import_module(module_name)
    module_path = Path(module.__file__)
    relative = module_name.replace(".", "/") + ".py"
    installed = module_path.read_bytes().replace(b"\r\n", b"\n")
    pinned = subprocess.check_output(
        [
            "git",
            "-C",
            str(reference_checkout),
            "show",
            REFERENCE_COMMIT + ":" + relative,
        ]
    ).replace(b"\r\n", b"\n")
    if installed != pinned:
        raise RuntimeError("Reference module does not match pin: " + relative)
    return {
        "commit": REFERENCE_COMMIT,
        "method": "opaque defining-module bytes, CRLF normalized to LF, no source decoding",
        "function_module": module_name,
        "function_qualname": getattr(function, "__qualname__", ""),
        "module_relative_path": relative,
        "installed_sha256": hashlib.sha256(installed).hexdigest(),
        "pinned_sha256": hashlib.sha256(pinned).hexdigest(),
    }


def _run_case(name, frame, parameters):
    before = frame.copy(deep=True)
    before_description = describe_frame(frame)
    row = {
        "name": name,
        "parameters": dict(parameters),
        "input": before_description,
        "returned_same_object": None,
    }
    try:
        result = pm4py.format_dataframe(frame, **parameters)
        row["returned_type"] = type(result).__name__
        row["returned_same_object"] = result is frame
        row["result"] = describe_frame(result)
    except Exception as error:
        row["error"] = {
            "type": type(error).__name__,
            "message": str(error),
        }
    after_description = describe_frame(frame)
    row["input_after"] = after_description
    row["inputs_unchanged"] = _frame_equal(frame, before)
    row["input_metadata_unchanged"] = after_description == before_description
    row["input_mutated"] = not row["inputs_unchanged"]
    row["input_metadata_mutated"] = not row["input_metadata_unchanged"]
    row["input_scalar_snapshot_before"] = before_description.get("records", [])
    row["input_scalar_snapshot_after"] = after_description.get("records", [])
    return row


def _attrs_conversion_probe():
    """Observe public attrs-to-event-log metadata for one formatted frame."""

    frame = _frame(
        [
            {
                "case:concept:name": "L1",
                "concept:name": "A",
                "time:timestamp": "2024-01-01T00:00:00Z",
            }
        ],
        attrs={"source": "attrs-probe", "nested": {"enabled": True}},
    )
    before = frame.copy(deep=True)
    row = {
        "input": describe_frame(before),
        "input_after_format": None,
        "formatted": None,
    }
    try:
        formatted = pm4py.format_dataframe(frame)
        row["formatted"] = describe_frame(formatted)
        row["returned_same_object"] = formatted is frame
        row["input_after_format"] = describe_frame(frame)
        try:
            event_log = pm4py.convert_to_event_log(formatted)
            row["event_log_type"] = type(event_log).__name__
            row["event_log_attributes"] = normalize(
                getattr(event_log, "attributes", None)
            )
            row["event_log_properties"] = normalize(
                getattr(event_log, "properties", None)
            )
        except Exception as error:
            row["conversion_error"] = {
                "type": type(error).__name__,
                "message": str(error),
            }
    except Exception as error:
        row["format_error"] = {
            "type": type(error).__name__,
            "message": str(error),
        }
        row["input_after_format"] = describe_frame(frame)
    row["inputs_unchanged"] = _frame_equal(frame, before)
    row["input_mutated"] = not row["inputs_unchanged"]
    return row


def _frame(rows, columns=None, index=None, attrs=None):
    result = pd.DataFrame(rows, columns=columns, index=index)
    if attrs:
        result.attrs.update(attrs)
    return result


def _typed_empty(columns, dtypes):
    result = pd.DataFrame(
        {column: pd.Series([], dtype=dtype) for column, dtype in zip(columns, dtypes)}
    )
    return result


def _numeric_boundary_cases():
    """Public numeric sorting/conversion boundaries with same and split cases."""
    inputs = {
        "duplicate_float_late": [1, 1.0, 2**64 - 1, 2**64 - 2],
        "duplicate_float_first": [1.0, 1, 2**64 - 1, 2**64 - 2],
        "duplicate_bool_late": [1, True, 1.5, 2**64 - 1, 2**64 - 2],
        "duplicate_bool_first": [True, 1, 1.5, 2**64 - 1, 2**64 - 2],
        "float_negative_zero": [0, -0.0, 2**64 - 1, 2**64 - 2],
        "bool_float_wide": [True, 1.5, 2**64 - 1, 2**64 - 2],
        "string_float_wide": ["z", 1.5, 2**64 - 1, 2**64 - 2],
        "float_exact_equal": [1, 1.0, 2**64 - 1],
        "float_rounding_pair": [float(2**53), 2**53 + 1, 2**64 - 1],
        "float_bool_rounding": [True, float(2**53), 2**53 + 1, 2**64 - 1],
        "timestamp_text_before_number": ["!bad", 2],
        "timestamp_boolean_text": ["!bad", True, 0],
        "nanosecond_nat_with_bad": [-(2**63), "bad"],
        "numeric_datetime_string": [0, "2024-01-01"],
        "numeric_signed_nat_and_bool": [-(2**63), True],
        "wide_float_collision": [2**53 + 1, float(2**53), 2**64 - 1, 2**64 - 2],
        "wide_float_safe": [2**53 + 2, float(2**53), 2**64 - 1],
        "wide_integer_collision": [2**53 + 1, 2**53, 2**64 - 1, 2**64 - 2],
        "wide_only_float": [float(1), 2**64 - 1, 2**64 - 2],
        "wide_one_float": [float(1), 2**64 - 1],
        "wide_bool_collision": [True, 2**64 - 1, 2**64 - 2],
        "bool_exact_collision": [True, 2**53, 2**53 + 1],
        "negative_wide": [-1, 2**64 - 1, 2**64 - 2],
        "negative_wide_float": [-1.0, 2**64 - 1, 2**64 - 2],
        "infinite_collision": [float("inf"), 2**53, 2**53 + 1],
        "negative_infinite": [float("-inf"), -(2**63), -(2**63) + 1],
        "numeric_string": [1, "2"],
        "mixed_object_int_float": [2**53 + 1, float(2**53)],
        "object_large_signed": [-(2**63), 2**63 - 1],
        "object_signed_nat": [-(2**63), 0],
        "object_unsigned_epoch": [2**63, 2**63 + 1],
        "object_unsigned_wrap": [2**64 - 1, 0],
        "object_overflow_float": [float(2**63), 2**63 - 1],
    }
    cases = []
    for name, values in inputs.items():
        for mode in ("same", "split"):
            cases.append(
                (
                    "numeric_boundary_" + name + "_" + mode,
                    _frame(
                        {
                            "case:concept:name": ["C"] * len(values)
                            if mode == "same"
                            else [str(i) for i in range(len(values))],
                            "concept:name": [str(i) for i in range(len(values))],
                            "time:timestamp": pd.Series(values, dtype="object"),
                        }
                    ),
                    {},
                )
            )
    values = inputs["wide_float_collision"]
    cases.append(
        (
            "numeric_boundary_custom_error_mutation",
            _frame(
                {
                    "cid": ["C"] * len(values),
                    "act": [str(i) for i in range(len(values))],
                    "when": pd.Series(values, dtype="object"),
                }
            ),
            {"case_id": "cid", "activity_key": "act", "timestamp_key": "when"},
        )
    )
    return cases


def _cases():
    """Return a finite matrix of public boundary cases."""

    cases = []
    add = lambda name, frame, parameters=None: cases.append(
        (name, frame, parameters or {})
    )

    canonical = {
        "case:concept:name": ["L2", "L1", "L1"],
        "concept:name": ["late", "second", "first"],
        "time:timestamp": [
            "2024-01-02T00:00:00Z",
            "2024-01-01T00:00:01Z",
            "2024-01-01T00:00:00Z",
        ],
    }
    add("normal_defaults_unsorted", pd.DataFrame(canonical))
    add(
        "normal_explicit_default_selectors",
        pd.DataFrame(canonical),
        {
            "case_id": "case:concept:name",
            "activity_key": "concept:name",
            "timestamp_key": "time:timestamp",
            "start_timestamp_key": "start_timestamp",
            "timest_format": None,
        },
    )
    add(
        "custom_selectors_and_existing_start",
        _frame(
            [
                {
                    "cid": "L1",
                    "act": "A",
                    "ended": "01/01/2024 00:00",
                    "began": "31/12/2023 23:59",
                },
                {
                    "cid": "L1",
                    "act": "B",
                    "ended": "01/01/2024 00:01",
                    "began": "01/01/2024 00:00",
                },
            ]
        ).astype({"cid": "category"}),
        {
            "case_id": "cid",
            "activity_key": "act",
            "timestamp_key": "ended",
            "start_timestamp_key": "began",
            "timest_format": "%d/%m/%Y %H:%M",
        },
    )
    add(
        "custom_start_missing_is_added",
        _frame([{"cid": "L1", "act": "A", "ended": "2024-01-01T00:00:00Z"}]),
        {
            "case_id": "cid",
            "activity_key": "act",
            "timestamp_key": "ended",
            "start_timestamp_key": "began",
        },
    )
    add(
        "default_start_existing_values",
        _frame(
            [
                {
                    "case:concept:name": "L1",
                    "concept:name": "A",
                    "time:timestamp": "2024-01-01T00:01:00Z",
                    "start_timestamp": "2024-01-01T00:00:00Z",
                }
            ]
        ),
    )
    add(
        "extra_timestamp_like_columns",
        _frame(
            [
                {
                    "case:concept:name": "L1",
                    "concept:name": "A",
                    "time:timestamp": "2024-01-01T00:00:00Z",
                    "timestamp": "not-selected",
                    "start_time": "also-not-selected",
                }
            ]
        ),
    )
    add(
        "attrs_nested_metadata",
        _frame(
            [
                {
                    "case:concept:name": "L1",
                    "concept:name": "A",
                    "time:timestamp": "2024-01-01",
                }
            ],
            attrs={"source": "probe", "nested": {"count": 2, "enabled": True}},
        ),
    )
    add(
        "named_index_labels",
        _frame(
            [
                {
                    "case:concept:name": "L1",
                    "concept:name": "A",
                    "time:timestamp": "2024-01-01",
                },
                {
                    "case:concept:name": "L2",
                    "concept:name": "B",
                    "time:timestamp": "2024-01-02",
                },
            ],
            index=pd.Index([40, 10], name="source_row"),
        ),
    )
    add(
        "tie_order_preserves_input_order",
        _frame(
            [
                {
                    "case:concept:name": "L1",
                    "concept:name": "second",
                    "time:timestamp": "2024-01-01",
                },
                {
                    "case:concept:name": "L1",
                    "concept:name": "first",
                    "time:timestamp": "2024-01-01",
                },
            ],
            index=[9, 3],
        ),
    )
    add(
        "numeric_case_and_activity_ids",
        _frame(
            {
                "case:concept:name": pd.Series([10, 2], dtype="int64"),
                "concept:name": pd.Series([100, 20], dtype="int64"),
                "time:timestamp": ["2024-01-02", "2024-01-01"],
            }
        ),
    )
    add(
        "boolean_case_and_activity_ids",
        _frame(
            {
                "case:concept:name": pd.Series([True, False], dtype="bool"),
                "concept:name": pd.Series([False, True], dtype="bool"),
                "time:timestamp": ["2024-01-02", "2024-01-01"],
            }
        ),
    )
    add(
        "float_case_ids_and_object_activity",
        _frame(
            {
                "case:concept:name": pd.Series([1.5, 1.0], dtype="float64"),
                "concept:name": pd.Series([1, "A"], dtype="object"),
                "time:timestamp": ["2024-01-02", "2024-01-01"],
            }
        ),
    )
    add(
        "nullable_case_activity_values",
        _frame(
            {
                "case:concept:name": pd.Series(["L1", pd.NA], dtype="string"),
                "concept:name": pd.Series(["A", pd.NA], dtype="string"),
                "time:timestamp": ["2024-01-01", "2024-01-02"],
            }
        ),
    )
    add(
        "missing_case_column",
        _frame([{"concept:name": "A", "time:timestamp": "2024-01-01"}]),
    )
    add(
        "missing_activity_column",
        _frame([{"case:concept:name": "L1", "time:timestamp": "2024-01-01"}]),
    )
    add(
        "missing_timestamp_column",
        _frame([{"case:concept:name": "L1", "concept:name": "A"}]),
    )
    add(
        "null_timestamp_values",
        _frame(
            {
                "case:concept:name": ["L1", "L1"],
                "concept:name": ["A", "B"],
                "time:timestamp": [None, pd.NaT],
            }
        ),
    )
    add(
        "invalid_timestamp_text",
        _frame(
            [
                {
                    "case:concept:name": "L1",
                    "concept:name": "A",
                    "time:timestamp": "not-a-date",
                }
            ]
        ),
    )
    add(
        "explicit_format_success",
        _frame(
            [
                {
                    "case:concept:name": "L1",
                    "concept:name": "A",
                    "time:timestamp": "31/12/2024 23:59",
                }
            ]
        ),
        {"timest_format": "%d/%m/%Y %H:%M"},
    )
    add(
        "explicit_format_failure",
        _frame(
            [
                {
                    "case:concept:name": "L1",
                    "concept:name": "A",
                    "time:timestamp": "31/12/2024",
                }
            ]
        ),
        {"timest_format": "%m-%d-%Y"},
    )
    add(
        "mixed_format_keyword",
        _frame(
            {
                "case:concept:name": ["L1", "L1"],
                "concept:name": ["A", "B"],
                "time:timestamp": ["2024-01-01", "2024-01-01T00:00:01Z"],
            }
        ),
        {"timest_format": "mixed"},
    )
    add(
        "timezone_offsets_as_strings",
        _frame(
            {
                "case:concept:name": ["L1", "L1"],
                "concept:name": ["offset", "utc"],
                "time:timestamp": ["2024-01-01T01:00:00+01:00", "2024-01-01T00:00:00Z"],
            }
        ),
    )
    add(
        "timezone_aware_typed_series",
        _frame(
            {
                "case:concept:name": ["L1", "L1"],
                "concept:name": ["late", "early"],
                "time:timestamp": pd.to_datetime(
                    ["2024-01-01T00:00:00Z", "2024-01-01T00:00:01Z"], utc=True
                ),
            }
        ),
    )
    add(
        "datetime_microsecond_series",
        _frame(
            {
                "case:concept:name": ["L1"],
                "concept:name": ["micro"],
                "time:timestamp": pd.Series(
                    pd.to_datetime(["2024-01-01T00:00:00.123456"])
                ).astype("datetime64[us]"),
            }
        ),
    )
    add(
        "datetime_nanosecond_series",
        _frame(
            {
                "case:concept:name": ["L1"],
                "concept:name": ["nano"],
                "time:timestamp": pd.to_datetime(["2024-01-01T00:00:00.123456789"]),
            }
        ),
    )
    add(
        "numeric_timestamp_values",
        _frame(
            {
                "case:concept:name": ["L1"] * 5,
                "concept:name": ["ten", "two", "zero", "negative", "one"],
                "time:timestamp": pd.Series([10, 2, 0, -3, 1], dtype="int64"),
            }
        ),
    )
    add(
        "boolean_timestamp_values",
        _frame(
            {
                "case:concept:name": ["L1", "L1"],
                "concept:name": ["false", "true"],
                "time:timestamp": pd.Series([False, True], dtype="bool"),
            }
        ),
    )
    add(
        "typed_empty_object_schema",
        _typed_empty(
            ["case:concept:name", "concept:name", "time:timestamp"],
            ["object", "object", "object"],
        ),
    )
    add(
        "typed_empty_extension_schema",
        _typed_empty(
            ["case:concept:name", "concept:name", "time:timestamp", "start_timestamp"],
            ["string", "string", "datetime64[us]", "datetime64[us]"],
        ),
    )
    add(
        "empty_missing_required_columns",
        _typed_empty(["extra"], ["string"]),
    )
    add(
        "empty_custom_selector_schema",
        _typed_empty(["cid", "act", "ended"], ["string", "string", "string"]),
        {
            "case_id": "cid",
            "activity_key": "act",
            "timestamp_key": "ended",
            "start_timestamp_key": "began",
        },
    )
    add(
        "categorical_case_activity",
        _frame(
            {
                "case:concept:name": pd.Series(
                    pd.Categorical(["L2", "L1"], categories=["L1", "L2"])
                ),
                "concept:name": pd.Series(pd.Categorical(["B", "A"], ordered=True)),
                "time:timestamp": ["2024-01-02", "2024-01-01"],
            }
        ),
    )
    add(
        "canonical_column_collision_extra_start",
        _frame(
            {
                "case:concept:name": ["L1"],
                "concept:name": ["A"],
                "time:timestamp": ["2024-01-01"],
                "start_timestamp": ["2023-12-31"],
                "case_id": ["unrelated"],
            }
        ),
    )
    add(
        "custom_selector_same_case_and_activity",
        _frame([{"same": "L1", "when": "2024-01-01"}]),
        {"case_id": "same", "activity_key": "same", "timestamp_key": "when"},
    )
    add(
        "custom_selector_same_timestamp_and_start",
        _frame([{"cid": "L1", "act": "A", "when": "2024-01-01"}]),
        {
            "case_id": "cid",
            "activity_key": "act",
            "timestamp_key": "when",
            "start_timestamp_key": "when",
        },
    )
    duplicate = pd.DataFrame(
        [["L1", "A", "2024-01-01", "duplicate"]],
        columns=["case:concept:name", "concept:name", "time:timestamp", "concept:name"],
    )
    add("duplicate_canonical_columns", duplicate)
    add(
        "multiindex_input_labels",
        _frame(
            [
                {
                    "case:concept:name": "L1",
                    "concept:name": "A",
                    "time:timestamp": "2024-01-01",
                },
                {
                    "case:concept:name": "L2",
                    "concept:name": "B",
                    "time:timestamp": "2024-01-02",
                },
            ],
            index=pd.MultiIndex.from_tuples(
                [("x", 2), ("x", 1)], names=["group", "row"]
            ),
        ),
    )
    add(
        "object_mixed_timestamp_values",
        _frame(
            {
                "case:concept:name": ["L1", "L1"],
                "concept:name": ["text", "timestamp"],
                "time:timestamp": pd.Series(
                    ["2024-01-01", pd.Timestamp("2024-01-02")], dtype="object"
                ),
            }
        ),
    )
    add(
        "explicit_start_missing_value",
        _frame(
            {
                "case:concept:name": ["L1", "L1"],
                "concept:name": ["A", "B"],
                "time:timestamp": ["2024-01-01", "2024-01-02"],
                "start_timestamp": [None, "2024-01-01"],
            }
        ),
    )
    add(
        "explicit_start_timezone_offset",
        _frame(
            {
                "case:concept:name": ["L1"],
                "concept:name": ["A"],
                "time:timestamp": ["2024-01-01T01:00:00+01:00"],
                "start_timestamp": ["2024-01-01T00:00:00Z"],
            }
        ),
    )
    add(
        "non_string_column_labels_with_selectors",
        _frame(
            [["L1", "A", "2024-01-01"]],
            columns=[1, 2, 3],
        ),
        {"case_id": 1, "activity_key": 2, "timestamp_key": 3},
    )
    add(
        "start_selector_none",
        _frame(
            [
                {
                    "case:concept:name": "L1",
                    "concept:name": "A",
                    "time:timestamp": "2024-01-01",
                }
            ]
        ),
        {"start_timestamp_key": None},
    )
    add(
        "extra_valid_date_columns",
        _frame(
            {
                "case:concept:name": ["C"],
                "concept:name": ["A"],
                "time:timestamp": ["2024-01-01"],
                "timestamp": ["2024-01-02"],
                "start_time": ["2024-01-03"],
                "other": ["2024-01-04"],
                "numeric_text": ["123"],
            }
        ),
    )
    add(
        "custom_selectors_overwrite_canonical",
        _frame(
            {
                "cid": ["C"],
                "act": ["A"],
                "ended": ["2024-01-01"],
                "began": ["2023-12-31"],
                "case:concept:name": ["stale-case"],
                "concept:name": ["stale-activity"],
                "time:timestamp": ["2020-01-01"],
                "start_timestamp": ["2020-01-01"],
            }
        ),
        {
            "case_id": "cid",
            "activity_key": "act",
            "timestamp_key": "ended",
            "start_timestamp_key": "began",
        },
    )
    add(
        "existing_generated_index_columns",
        _frame(
            {
                "case:concept:name": ["B", "A"],
                "concept:name": ["second", "first"],
                "time:timestamp": ["2024-01-02", "2024-01-01"],
                "@@index": [99, 98],
                "@@case_index": [77, 76],
            }
        ),
    )
    add(
        "custom_missing_activity_partial_mutation",
        _frame({"cid": ["C"], "ended": ["2024-01-01"]}),
        {"case_id": "cid", "activity_key": "missing", "timestamp_key": "ended"},
    )
    add(
        "custom_missing_timestamp_partial_mutation",
        _frame({"cid": ["C"], "act": ["A"]}),
        {"case_id": "cid", "activity_key": "act", "timestamp_key": "missing"},
    )
    add(
        "mixed_valid_invalid_timestamp_text",
        _frame(
            {
                "case:concept:name": ["C", "C"],
                "concept:name": ["valid", "invalid"],
                "time:timestamp": ["2024-01-01", "!not-a-date"],
            }
        ),
    )
    add(
        "nanosecond_timestamp_strings",
        _frame(
            {
                "case:concept:name": ["C", "C"],
                "concept:name": ["later", "earlier"],
                "time:timestamp": [
                    "2024-01-01T00:00:00.000002000Z",
                    "2024-01-01T00:00:00.000001001Z",
                ],
            }
        ),
    )
    add(
        "cross_canonical_selectors",
        _frame(
            {
                "case:concept:name": ["C"],
                "concept:name": ["A"],
                "time:timestamp": ["2024-01-01"],
            }
        ),
        {"case_id": "concept:name", "activity_key": "case:concept:name"},
    )
    for name, values in [
        ("object_boolean_integer_timestamps", [True, 2, False, 1]),
        ("object_exact_numeric_timestamps", [9007199254740993, 9007199254740992.0]),
        ("object_unsigned_timestamps", [18446744073709551615, 18446744073709551614]),
    ]:
        add(
            name,
            _frame(
                {
                    "case:concept:name": ["C"] * len(values),
                    "concept:name": ["A" + str(i) for i in range(len(values))],
                    "time:timestamp": pd.Series(values, dtype="object"),
                }
            ),
        )
    cases.extend(_numeric_boundary_cases())
    for name, values in (
        ("invalid_february", ["2024/02/31", "2024/03/01"]),
        ("nonleap_february", ["2023/02/29"]),
        ("leap_february", ["2024/02/29"]),
        ("invalid_april", ["2024/04/31"]),
        ("valid_april", ["2024/04/30"]),
        ("century_nonleap", ["1900/02/29"]),
        ("century_leap", ["2000/02/29"]),
    ):
        cases.append(
            (
                "calendar_" + name,
                _frame(
                    {
                        "case:concept:name": ["C"] * len(values),
                        "concept:name": ["A"] * len(values),
                        "time:timestamp": values,
                    }
                ),
                {"timest_format": "%Y/%m/%d"},
            )
        )
    for name, values, timestamp_format in (
        ("fraction_one", ["2024/01/01 01:02:03.1"], "%Y/%m/%d %H:%M:%S.%f"),
        ("fraction_six", ["2024/01/01 01:02:03.123456"], "%Y/%m/%d %H:%M:%S.%f"),
        ("fraction_nine", ["2024/01/01 01:02:03.123456789"], "%Y/%m/%d %H:%M:%S.%f"),
        ("fraction_ten", ["2024/01/01 01:02:03.1234567899"], "%Y/%m/%d %H:%M:%S.%f"),
        ("fraction_zero", ["2024/01/01 01:02:03.000000"], "%Y/%m/%d %H:%M:%S.%f"),
        ("fraction_empty", ["2024/01/01 01:02:03."], "%Y/%m/%d %H:%M:%S.%f"),
        ("fraction_bad", ["2024/01/01 01:02:03.abc"], "%Y/%m/%d %H:%M:%S.%f"),
        ("offset_positive", ["2024/01/01 01:02:03+0230"], "%Y/%m/%d %H:%M:%S%z"),
        ("offset_colon", ["2024/01/01 01:02:03+02:30"], "%Y/%m/%d %H:%M:%S%z"),
        ("offset_negative", ["2024/01/01 01:02:03-0530"], "%Y/%m/%d %H:%M:%S%z"),
        ("offset_utc", ["2024/01/01 01:02:03Z"], "%Y/%m/%d %H:%M:%S%z"),
        ("offset_lower", ["2024/01/01 01:02:03z"], "%Y/%m/%d %H:%M:%S%z"),
        ("offset_hour", ["2024/01/01 01:02:03+02"], "%Y/%m/%d %H:%M:%S%z"),
        ("offset_seconds", ["2024/01/01 01:02:03+02:30:15"], "%Y/%m/%d %H:%M:%S%z"),
        (
            "offset_compact_seconds",
            ["2024/01/01 01:02:03+023015"],
            "%Y/%m/%d %H:%M:%S%z",
        ),
        ("offset_large", ["2024/01/01 01:02:03+2300"], "%Y/%m/%d %H:%M:%S%z"),
        ("offset_day", ["2024/01/01 01:02:03+2400"], "%Y/%m/%d %H:%M:%S%z"),
        ("offset_minutes", ["2024/01/01 01:02:03+0260"], "%Y/%m/%d %H:%M:%S%z"),
        ("offset_mixed_colon", ["2024/01/01 01:02:03+02:3015"], "%Y/%m/%d %H:%M:%S%z"),
        ("offset_fraction", ["2024/01/01 01:02:03+02:30:15.5"], "%Y/%m/%d %H:%M:%S%z"),
        (
            "fraction_offset",
            ["2024/01/01 01:02:03.123456789+0230"],
            "%Y/%m/%d %H:%M:%S.%f%z",
        ),
        (
            "fraction_mixed",
            ["2024/01/01 01:02:03.1", "2024/01/02 01:02:03.123456789"],
            "%Y/%m/%d %H:%M:%S.%f",
        ),
        (
            "offset_mixed",
            ["2024/01/01 01:02:03+0230", "2024/01/01 01:02:03-0530"],
            "%Y/%m/%d %H:%M:%S%z",
        ),
        ("literal_fraction", ["01|01|2024 01:02:03."], "%d|%m|%Y %H:%M:%S.%f"),
        (
            "literal_offset_seconds",
            ["01|01|2024 01:02:03+02:30:15"],
            "%d|%m|%Y %H:%M:%S%z",
        ),
        (
            "fraction_before_literal",
            ["2024/01/01 01:02:03.123END"],
            "%Y/%m/%d %H:%M:%S.%fEND",
        ),
        (
            "fraction_empty_offset",
            ["2024/01/01 01:02:03.+0230"],
            "%Y/%m/%d %H:%M:%S.%f%z",
        ),
        (
            "offset_invalid_seconds",
            ["2024/01/01 01:02:03+02:30:99"],
            "%Y/%m/%d %H:%M:%S%z",
        ),
        (
            "offset_fraction_seven",
            ["2024/01/01 01:02:03+02:30:15.1234567"],
            "%Y/%m/%d %H:%M:%S%z",
        ),
        (
            "offset_before_literal",
            ["2024/01/01 01:02:03+0230END"],
            "%Y/%m/%d %H:%M:%S%zEND",
        ),
        ("literal_fraction_one", ["01|01|2024 01:02:03.1"], "%d|%m|%Y %H:%M:%S.%f"),
        (
            "literal_fraction_six",
            ["01|01|2024 01:02:03.123456"],
            "%d|%m|%Y %H:%M:%S.%f",
        ),
        (
            "literal_fraction_nine",
            ["01|01|2024 01:02:03.123456789"],
            "%d|%m|%Y %H:%M:%S.%f",
        ),
        (
            "literal_fraction_ten",
            ["01|01|2024 01:02:03.1234567899"],
            "%d|%m|%Y %H:%M:%S.%f",
        ),
        (
            "literal_fraction_zero",
            ["01|01|2024 01:02:03.000000"],
            "%d|%m|%Y %H:%M:%S.%f",
        ),
        ("literal_fraction_empty", ["01|01|2024 01:02:03."], "%d|%m|%Y %H:%M:%S.%f"),
        ("literal_fraction_bad", ["01|01|2024 01:02:03.abc"], "%d|%m|%Y %H:%M:%S.%f"),
        (
            "literal_offset_positive",
            ["01|01|2024 01:02:03+0230"],
            "%d|%m|%Y %H:%M:%S%z",
        ),
        ("literal_offset_colon", ["01|01|2024 01:02:03+02:30"], "%d|%m|%Y %H:%M:%S%z"),
        (
            "literal_offset_negative",
            ["01|01|2024 01:02:03-0530"],
            "%d|%m|%Y %H:%M:%S%z",
        ),
        ("literal_offset_utc", ["01|01|2024 01:02:03Z"], "%d|%m|%Y %H:%M:%S%z"),
        ("literal_offset_lower", ["01|01|2024 01:02:03z"], "%d|%m|%Y %H:%M:%S%z"),
        ("literal_offset_hour", ["01|01|2024 01:02:03+02"], "%d|%m|%Y %H:%M:%S%z"),
        (
            "literal_offset_compact_seconds",
            ["01|01|2024 01:02:03+023015"],
            "%d|%m|%Y %H:%M:%S%z",
        ),
        ("literal_offset_large", ["01|01|2024 01:02:03+2300"], "%d|%m|%Y %H:%M:%S%z"),
        ("literal_offset_day", ["01|01|2024 01:02:03+2400"], "%d|%m|%Y %H:%M:%S%z"),
        ("literal_offset_minutes", ["01|01|2024 01:02:03+0260"], "%d|%m|%Y %H:%M:%S%z"),
        (
            "literal_offset_mixed_colon",
            ["01|01|2024 01:02:03+02:3015"],
            "%d|%m|%Y %H:%M:%S%z",
        ),
        (
            "literal_offset_fraction",
            ["01|01|2024 01:02:03+02:30:15.5"],
            "%d|%m|%Y %H:%M:%S%z",
        ),
        (
            "literal_fraction_offset",
            ["01|01|2024 01:02:03.123456789+0230"],
            "%d|%m|%Y %H:%M:%S.%f%z",
        ),
        (
            "literal_fraction_mixed",
            ["01|01|2024 01:02:03.1", "02|01|2024 01:02:03.123456789"],
            "%d|%m|%Y %H:%M:%S.%f",
        ),
        (
            "literal_offset_mixed",
            ["01|01|2024 01:02:03+0230", "01|01|2024 01:02:03-0530"],
            "%d|%m|%Y %H:%M:%S%z",
        ),
    ):
        add(
            "explicit_date_" + name,
            _frame(
                {
                    "case:concept:name": ["C"] * len(values),
                    "concept:name": ["A"] * len(values),
                    "time:timestamp": values,
                }
            ),
            {"timest_format": timestamp_format},
        )
    for name, values, timestamp_format in (
        ("dash_fraction_empty", ["2024-01-01T01:02:03."], "%Y-%m-%dT%H:%M:%S.%f"),
        ("dash_offset_hour", ["2024-01-01T01:02:03+02"], "%Y-%m-%dT%H:%M:%S%z"),
        ("dot_fraction_empty", ["2024.01.01T01:02:03."], "%Y.%m.%dT%H:%M:%S.%f"),
        ("dot_offset_hour", ["2024.01.01T01:02:03+02"], "%Y.%m.%dT%H:%M:%S%z"),
        ("space_fraction_empty", ["2024 01 01T01:02:03."], "%Y %m %dT%H:%M:%S.%f"),
        ("space_offset_hour", ["2024 01 01T01:02:03+02"], "%Y %m %dT%H:%M:%S%z"),
        (
            "literal_offset_invalid_seconds",
            ["01|01|2024 01:02:03+02:30:99"],
            "%d|%m|%Y %H:%M:%S%z",
        ),
        (
            "literal_offset_fraction_seven",
            ["01|01|2024 01:02:03+02:30:15.1234567"],
            "%d|%m|%Y %H:%M:%S%z",
        ),
        (
            "literal_offset_fraction_empty",
            ["01|01|2024 01:02:03+02:30:15."],
            "%d|%m|%Y %H:%M:%S%z",
        ),
        (
            "literal_offset_negative_seconds",
            ["01|01|2024 01:02:03-00:00:45"],
            "%d|%m|%Y %H:%M:%S%z",
        ),
        (
            "escaped_fraction",
            ["01|01|2024 %f 01:02:03.123"],
            "%d|%m|%Y %%f %H:%M:%S.%f",
        ),
        ("escaped_offset", ["01|01|2024 %z 01:02:03+0230"], "%d|%m|%Y %%z %H:%M:%S%z"),
    ):
        add(
            "explicit_date_" + name,
            _frame(
                {
                    "case:concept:name": ["C"] * len(values),
                    "concept:name": ["A"] * len(values),
                    "time:timestamp": values,
                }
            ),
            {"timest_format": timestamp_format},
        )
    return cases


def capture(reference_checkout):
    if pm4py.__version__ != REFERENCE_VERSION:
        raise RuntimeError("This oracle requires reference version 2.7.22")
    if pd.__version__ != PANDAS_VERSION:
        raise RuntimeError("This oracle requires pandas version 3.0.5")
    function = pm4py.format_dataframe
    provenance = _public_provenance(reference_checkout)
    rows = [_run_case(name, frame, parameters) for name, frame, parameters in _cases()]
    errors = [row for row in rows if "error" in row]
    successes = [row for row in rows if "result" in row]
    return {
        "format": "pm4py.format_dataframe public contract capture",
        "capture_id": "format_dataframe_reference",
        "reference_version": pm4py.__version__,
        "pandas_version": pd.__version__,
        "reference_provenance": provenance,
        "public_signature": str(inspect.signature(function)),
        "public_doc": inspect.getdoc(function),
        "case_count": len(rows),
        "successful_case_count": len(successes),
        "error_case_count": len(errors),
        "rows": rows,
        "attrs_mapping_probe": _attrs_conversion_probe(),
        "findings": [
            "Each row records complete public input and post-call frame metadata, including attrs and index labels.",
            "The finite matrix covers selector defaults/customization, canonical collisions, sorting/ties, index labels, attrs, missing/invalid values, numeric and boolean identifiers, typed empty schemas, start timestamps, timezone/resolution values, and explicit timest_format parsing.",
            "Mutation and returned-object identity are recorded independently so in-place behavior is observable without inspecting implementation details.",
        ],
        "observed_rules": [
            "Default formatting returns a new DataFrame in the successful default cases and leaves the caller frame unchanged in this capture.",
            "Custom selector cases add canonical case, activity, and timestamp columns to the caller frame; the returned frame also carries @@index and @@case_index helper columns.",
            "Successful formatting converts selected timestamps to timezone-aware UTC values while retaining the original timestamp-like columns.",
            "The public attrs probe preserves DataFrame.attrs on the formatted frame; convert_to_event_log exposes those values through EventLog.properties and reports origin=csv in EventLog.attributes.",
            "Required-column and duplicate-canonical-column failures are captured as public exception type/message pairs with the input frame preserved.",
        ],
        "limits": [
            "This is a finite public-call capture and does not claim exhaustive pandas dtype or timezone coverage.",
            "Only the defining PM4Py module bytes are compared to the pinned checkout; bytes are hashed and never decoded.",
            "Errors record public exception type and message without traceback or source inspection.",
        ],
        "representability_gaps": [
            {
                "case": "non_string_column_labels_with_selectors",
                "reason": "The native formatter API currently models selector names as strings; this public case records the reference outcome for a non-string-label boundary.",
            },
        ],
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--reference-checkout", type=Path, required=True)
    args = parser.parse_args()
    report = capture(args.reference_checkout)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(
        json.dumps(
            {
                "reference_version": report["reference_version"],
                "pandas_version": report["pandas_version"],
                "case_count": report["case_count"],
                "successful_case_count": report["successful_case_count"],
                "error_case_count": report["error_case_count"],
            }
        )
    )
