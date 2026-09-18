"""Capture public PM4Py case-relation dtype behavior.

This is a deliberately finite, public-API-only probe.  It records the input
and output pandas dtypes, every cell's public value/kind/missing tag, public
errors, and input immutability for dtype boundaries that are not represented
by the ordinary case-relation fixture.  It does not inspect PM4Py source,
globals, or bytecode.
"""

import argparse
import json
import runpy
import warnings
from pathlib import Path

warnings.filterwarnings("ignore")

import pandas as pd
import pm4py


REFERENCE_VERSION = "2.7.22"
REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"
TIMESTAMP_PARAMETER = "pm4py:param:timestamp_key"
CASE_KEY = "case:concept:name"
LEFT_KEY = CASE_KEY + "_LEFT"
RIGHT_KEY = CASE_KEY + "_RIGHT"


def _load_public_table_api():
    """Load only helper functions from the existing public table probe."""

    path = (
        Path(__file__).resolve().parents[1]
        / "scripts"
        / "capture-case-relations-table-reference.py"
    )
    return runpy.run_path(str(path))


def _reference_provenance(reference_checkout):
    """Reuse the existing opaque-byte pin check without decoding source."""

    path = (
        Path(__file__).resolve().parents[1]
        / "scripts"
        / "capture-case-relations-time-reference.py"
    )
    time_api = runpy.run_path(str(path))
    return time_api["reference_provenance"](reference_checkout)


TABLE_API = _load_public_table_api()
algorithm = TABLE_API["algorithm"]
normalize = TABLE_API["normalize"]
scalar_kind = TABLE_API["scalar_kind"]
describe_frame = TABLE_API["describe_frame"]
describe_output = TABLE_API["describe_output"]
algorithm = TABLE_API["algorithm"]


def _is_missing(value):
    try:
        missing = pd.isna(value)
    except Exception:
        return False
    return isinstance(missing, bool) and missing


def _cell_tag(value, dtype):
    """Return a stable public tag for one dataframe cell."""

    typed = _typed_scalar(value)
    return {
        "dtype": str(dtype),
        "kind": typed["kind"],
        "missing": _is_missing(value),
        "value": typed["value"],
    }


def _frame_cell_tags(frame):
    return [
        {
            "index": normalize(index),
            "cells": {
                str(column): _cell_tag(
                    frame[column].iloc[position], frame[column].dtype
                )
                for column in frame.columns
            },
        }
        for position, index in enumerate(frame.index)
    ]


def _typed_scalar(value):
    """Encode one public pandas scalar, including recursive tuple values."""

    if isinstance(value, tuple):
        return {
            "kind": "tuple",
            "value": [_typed_scalar(item) for item in value],
        }
    return {
        "kind": scalar_kind(value),
        "value": normalize(value),
    }


def _categorical_dtypes(frame):
    """Capture public categorical dtype metadata for every categorical column."""

    result = {}
    for column in frame.columns:
        series = frame[column]
        dtype = series.dtype
        if isinstance(dtype, pd.CategoricalDtype):
            result[str(column)] = {
                "categories_dtype": str(dtype.categories.dtype),
                "ordered": bool(dtype.ordered),
                "categories": [_typed_scalar(value) for value in dtype.categories],
            }
    return result


def _frame_description(frame):
    """Extend the ordinary public frame description with dtype metadata."""

    description = describe_frame(frame)
    for position, record in enumerate(description["records"]):
        for column in description["columns"]:
            typed = _typed_scalar(frame[column].iloc[position])
            record["kinds"][column] = typed["kind"]
            record["values"][column] = typed["value"]
    description["categorical_dtypes"] = _categorical_dtypes(frame)
    description["index_metadata"] = {
        "class": type(frame.index).__name__,
        "dtype": str(frame.index.dtype),
        "name": normalize(frame.index.name),
    }
    description["cell_tags"] = _frame_cell_tags(frame)
    return description


def _output_description(frame):
    """Extend the ordinary public output description with dtype metadata."""

    description = describe_output(frame)
    for position, record in enumerate(description["records"]):
        for column in description["columns"]:
            typed = _typed_scalar(frame[column].iloc[position])
            record["kinds"][column] = typed["kind"]
            record["values"][column] = typed["value"]
    description["categorical_dtypes"] = _categorical_dtypes(frame)
    description["index_metadata"] = {
        "class": type(frame.index).__name__,
        "dtype": str(frame.index.dtype),
        "name": normalize(frame.index.name),
    }
    description["cell_tags"] = _frame_cell_tags(frame)
    return description


def _description_cell_tags(description):
    """Add cell tags to the public description returned by run_case."""

    dtypes = description["dtypes"]
    return [
        {
            "index": record["index"],
            "cells": {
                column: {
                    "dtype": dtypes[column],
                    "kind": record["kinds"][column],
                    "missing": record["values"][column] is None,
                    "value": record["values"][column],
                }
                for column in description["columns"]
            },
        }
        for record in description["records"]
    ]


def _relation(left="L1", right="R1", dtype=None):
    kwargs = {}
    if dtype is None:
        return pd.DataFrame({LEFT_KEY: [left], RIGHT_KEY: [right]})
    kwargs["dtype"] = dtype
    return pd.DataFrame(
        {
            LEFT_KEY: pd.Series([left], **kwargs),
            RIGHT_KEY: pd.Series([right], **kwargs),
        }
    )


def _empty_relation(dtype="object"):
    return pd.DataFrame(
        {
            LEFT_KEY: pd.Series([], dtype=dtype),
            RIGHT_KEY: pd.Series([], dtype=dtype),
        }
    )


def _run_dtype_case(
    name, base, incoming, relations, timestamp_key="when", parameters=None
):
    """Run one public case_merge call and preserve before/after metadata."""

    before = [frame.copy(deep=True) for frame in (base, incoming, relations)]
    row = {
        "name": name,
        "timestamp_key": timestamp_key,
        "base_input": _frame_description(before[0]),
        "incoming_input": _frame_description(before[1]),
        "relations_input": _frame_description(before[2]),
    }
    call_parameters = dict(parameters or {})
    if timestamp_key is not None:
        call_parameters[TIMESTAMP_PARAMETER] = timestamp_key
    try:
        output = algorithm.apply(base, incoming, relations, parameters=call_parameters)
        row["result"] = _output_description(output)
    except Exception as error:
        row["error"] = {
            "type": type(error).__name__,
            "message": str(error),
        }
    row["parameters"] = dict(parameters or {})
    if timestamp_key is not None:
        row["parameters"][TIMESTAMP_PARAMETER] = timestamp_key
    row["input_metadata_after"] = {
        "base": _frame_description(base),
        "incoming": _frame_description(incoming),
        "relations": _frame_description(relations),
    }
    row["input_metadata_unchanged"] = all(
        _frame_description(frame) == _frame_description(original)
        for frame, original in zip((base, incoming, relations), before)
    )
    row["inputs_unchanged"] = all(
        frame.equals(original)
        for frame, original in zip((base, incoming, relations), before)
    )
    row["input_cell_tags"] = {
        "base": _frame_cell_tags(before[0]),
        "incoming": _frame_cell_tags(before[1]),
        "relations": _frame_cell_tags(before[2]),
    }
    if "result" in row:
        row["result"]["cell_tags"] = _description_cell_tags(row["result"])
    return row


def _event(case_id, activity, when, **extra):
    return {
        CASE_KEY: case_id,
        "concept:name": activity,
        "when": when,
        **extra,
    }


def _capture_rows():
    rows = []

    # Plain numeric union and an omitted incoming payload column establish the
    # two ordinary pandas promotion paths used by the merge operation.
    rows.append(
        _run_dtype_case(
            "dtype_integer_payload_missing_incoming",
            pd.DataFrame(
                [_event("L1", "base", "a", score=1)],
                columns=[CASE_KEY, "concept:name", "when", "score"],
            ),
            pd.DataFrame([_event("R1", "incoming", "b")]),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_float_payload_integer_incoming",
            pd.DataFrame(
                {
                    CASE_KEY: ["L1"],
                    "concept:name": ["base"],
                    "when": ["a"],
                    "score": pd.Series([1.5], dtype="float64"),
                }
            ),
            pd.DataFrame(
                {
                    CASE_KEY: ["R1"],
                    "concept:name": ["incoming"],
                    "when": ["b"],
                    "score": pd.Series([2], dtype="int64"),
                }
            ),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_integer_payload_float_incoming",
            pd.DataFrame(
                {
                    CASE_KEY: ["L1"],
                    "concept:name": ["base"],
                    "when": ["a"],
                    "score": pd.Series([1], dtype="int64"),
                }
            ),
            pd.DataFrame(
                {
                    CASE_KEY: ["R1"],
                    "concept:name": ["incoming"],
                    "when": ["b"],
                    "score": pd.Series([2.5], dtype="float64"),
                }
            ),
            _relation(dtype="object"),
        )
    )

    # Boolean values participate in numeric union when both sides provide the
    # column; an omitted side instead creates an object column for the null.
    rows.append(
        _run_dtype_case(
            "dtype_boolean_payload_integer_incoming",
            pd.DataFrame(
                {
                    CASE_KEY: ["L1"],
                    "concept:name": ["base"],
                    "when": ["a"],
                    "flag": pd.Series([True], dtype="bool"),
                }
            ),
            pd.DataFrame(
                {
                    CASE_KEY: ["R1"],
                    "concept:name": ["incoming"],
                    "when": ["b"],
                    "flag": pd.Series([1], dtype="int64"),
                }
            ),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_boolean_payload_missing_incoming",
            pd.DataFrame(
                {
                    CASE_KEY: ["L1"],
                    "concept:name": ["base"],
                    "when": ["a"],
                    "flag": pd.Series([True], dtype="bool"),
                }
            ),
            pd.DataFrame([_event("R1", "incoming", "b")]),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_boolean_timestamp_integer_incoming",
            pd.DataFrame(
                {
                    CASE_KEY: ["L1"],
                    "concept:name": ["base"],
                    "when": pd.Series([True], dtype="bool"),
                }
            ),
            pd.DataFrame(
                {
                    CASE_KEY: ["R1"],
                    "concept:name": ["incoming"],
                    "when": pd.Series([1], dtype="int64"),
                }
            ),
            _relation(dtype="object"),
        )
    )

    # Nullable extension dtypes retain their null-aware dtype through both
    # event concatenation and relation expansion.
    rows.append(
        _run_dtype_case(
            "dtype_nullable_int64_nulls",
            pd.DataFrame(
                {
                    CASE_KEY: ["L1", "L1"],
                    "concept:name": ["base-value", "base-null"],
                    "when": ["a", "c"],
                    "score": pd.Series([1, pd.NA], dtype="Int64"),
                }
            ),
            pd.DataFrame(
                {
                    CASE_KEY: ["R1"],
                    "concept:name": ["incoming-null"],
                    "when": ["b"],
                    "score": pd.Series([pd.NA], dtype="Int64"),
                }
            ),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_nullable_float64_nulls",
            pd.DataFrame(
                {
                    CASE_KEY: ["L1"],
                    "concept:name": ["base"],
                    "when": ["a"],
                    "score": pd.Series([1.5], dtype="Float64"),
                }
            ),
            pd.DataFrame(
                {
                    CASE_KEY: ["R1"],
                    "concept:name": ["incoming-null"],
                    "when": ["b"],
                    "score": pd.Series([pd.NA], dtype="Float64"),
                }
            ),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_nullable_boolean_nulls",
            pd.DataFrame(
                {
                    CASE_KEY: ["L1"],
                    "concept:name": ["base"],
                    "when": ["a"],
                    "flag": pd.Series([True], dtype="boolean"),
                }
            ),
            pd.DataFrame(
                {
                    CASE_KEY: ["R1"],
                    "concept:name": ["incoming-null"],
                    "when": ["b"],
                    "flag": pd.Series([pd.NA], dtype="boolean"),
                }
            ),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_nullable_string_nulls",
            pd.DataFrame(
                {
                    CASE_KEY: ["L1", "L1"],
                    "concept:name": ["base-value", "base-null"],
                    "when": ["a", "c"],
                    "label": pd.Series(["x", pd.NA], dtype="string"),
                }
            ),
            pd.DataFrame(
                {
                    CASE_KEY: ["R1"],
                    "concept:name": ["incoming-null"],
                    "when": ["b"],
                    "label": pd.Series([pd.NA], dtype="string"),
                }
            ),
            _relation(dtype="object"),
        )
    )

    # Datetime resolution is normalized to pandas' result resolution.  A
    # missing timestamp column is materialized as NaT using the available
    # side's dtype; offset-aware columns retain object dtype when timezones
    # differ, while their public values remain timezone-aware.
    rows.append(
        _run_dtype_case(
            "dtype_datetime_us_ns",
            pd.DataFrame(
                {
                    CASE_KEY: ["L1"],
                    "concept:name": ["base-us"],
                    "when": pd.Series(
                        [pd.Timestamp("2024-01-01")], dtype="datetime64[us]"
                    ),
                }
            ),
            pd.DataFrame(
                {
                    CASE_KEY: ["R1"],
                    "concept:name": ["incoming-ns"],
                    "when": pd.Series(
                        [pd.Timestamp("2024-01-02")], dtype="datetime64[ns]"
                    ),
                }
            ),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_datetime_missing_incoming",
            pd.DataFrame(
                {
                    CASE_KEY: ["L1"],
                    "concept:name": ["base"],
                    "when": pd.Series(
                        [pd.Timestamp("2024-01-01")], dtype="datetime64[us]"
                    ),
                }
            ),
            pd.DataFrame({CASE_KEY: ["R1"], "concept:name": ["incoming"]}),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_datetime_missing_base",
            pd.DataFrame({CASE_KEY: ["L1"], "concept:name": ["base"]}),
            pd.DataFrame(
                {
                    CASE_KEY: ["R1"],
                    "concept:name": ["incoming"],
                    "when": pd.Series(
                        [pd.Timestamp("2024-01-02")], dtype="datetime64[ns]"
                    ),
                }
            ),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_datetime_timezone_offsets",
            pd.DataFrame(
                {
                    CASE_KEY: ["L1"],
                    "concept:name": ["base-utc"],
                    "when": pd.Series([pd.Timestamp("2024-01-01T00:00:00+00:00")]),
                }
            ),
            pd.DataFrame(
                {
                    CASE_KEY: ["R1"],
                    "concept:name": ["incoming-offset"],
                    "when": pd.Series([pd.Timestamp("2024-01-01T01:00:00+01:00")]),
                }
            ),
            _relation(dtype="object"),
        )
    )

    # Empty, explicitly typed tables exercise dtype-driven schema creation and
    # prove that an empty relation table still emits the relation columns.
    rows.append(
        _run_dtype_case(
            "dtype_all_empty_typed_inputs",
            pd.DataFrame(
                {
                    CASE_KEY: pd.Series([], dtype="string"),
                    "concept:name": pd.Series([], dtype="string"),
                    "when": pd.Series([], dtype="datetime64[us]"),
                    "score": pd.Series([], dtype="Int64"),
                }
            ),
            pd.DataFrame(
                {
                    CASE_KEY: pd.Series([], dtype="string"),
                    "concept:name": pd.Series([], dtype="string"),
                    "when": pd.Series([], dtype="datetime64[ns]"),
                    "score": pd.Series([], dtype="Int64"),
                }
            ),
            _empty_relation(dtype="string"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_empty_typed_incoming",
            pd.DataFrame(
                {
                    CASE_KEY: pd.Series(["L1"], dtype="string"),
                    "concept:name": ["base"],
                    "when": pd.Series(
                        [pd.Timestamp("2024-01-01")], dtype="datetime64[us]"
                    ),
                    "score": pd.Series([1], dtype="Int64"),
                }
            ),
            pd.DataFrame(
                {
                    CASE_KEY: pd.Series([], dtype="string"),
                    "concept:name": pd.Series([], dtype="string"),
                    "when": pd.Series([], dtype="datetime64[ns]"),
                    "score": pd.Series([], dtype="Int64"),
                }
            ),
            _empty_relation(dtype="string"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_empty_typed_relations",
            pd.DataFrame(
                {
                    CASE_KEY: pd.Series(["L1"], dtype="string"),
                    "concept:name": ["base"],
                    "when": pd.Series(
                        [pd.Timestamp("2024-01-01")], dtype="datetime64[us]"
                    ),
                    "score": pd.Series([1], dtype="Int64"),
                }
            ),
            pd.DataFrame(
                {
                    CASE_KEY: pd.Series(["R1"], dtype="string"),
                    "concept:name": ["incoming"],
                    "when": pd.Series(
                        [pd.Timestamp("2024-01-02")], dtype="datetime64[ns]"
                    ),
                    "score": pd.Series([2], dtype="Int64"),
                }
            ),
            _empty_relation(dtype="string"),
        )
    )

    # Categorical metadata is public pandas state.  Identifiers are coerced to
    # object by the relation merge, while matching ordered payload categories
    # survive and differing category order falls back to strings.
    rows.append(
        _run_dtype_case(
            "dtype_categorical_case_ids_object_relations",
            pd.DataFrame(
                {
                    CASE_KEY: pd.Series(["L1"], dtype="category"),
                    "concept:name": ["base"],
                    "when": ["a"],
                }
            ),
            pd.DataFrame(
                {
                    CASE_KEY: pd.Series(["R1"], dtype="category"),
                    "concept:name": ["incoming"],
                    "when": ["b"],
                }
            ),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_ordered_categorical_payload_same_categories",
            pd.DataFrame(
                {
                    CASE_KEY: ["L1"],
                    "concept:name": ["base"],
                    "when": ["a"],
                    "label": pd.Series(
                        pd.Categorical(
                            ["low"], categories=["low", "high"], ordered=True
                        )
                    ),
                }
            ),
            pd.DataFrame(
                {
                    CASE_KEY: ["R1"],
                    "concept:name": ["incoming"],
                    "when": ["b"],
                    "label": pd.Series(
                        pd.Categorical(
                            ["high"], categories=["low", "high"], ordered=True
                        )
                    ),
                }
            ),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_ordered_categorical_payload_different_order",
            pd.DataFrame(
                {
                    CASE_KEY: ["L1"],
                    "concept:name": ["base"],
                    "when": ["a"],
                    "label": pd.Series(
                        pd.Categorical(
                            ["low"], categories=["low", "high"], ordered=True
                        )
                    ),
                }
            ),
            pd.DataFrame(
                {
                    CASE_KEY: ["R1"],
                    "concept:name": ["incoming"],
                    "when": ["b"],
                    "label": pd.Series(
                        pd.Categorical(
                            ["high"], categories=["high", "low"], ordered=True
                        )
                    ),
                }
            ),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_nullable_string_case_ids_and_null_relation",
            pd.DataFrame(
                {
                    CASE_KEY: pd.Series(["L1", pd.NA], dtype="string"),
                    "concept:name": ["base-valid", "base-null"],
                    "when": ["a", "z"],
                }
            ),
            pd.DataFrame(
                {
                    CASE_KEY: pd.Series(["R1", pd.NA], dtype="string"),
                    "concept:name": ["incoming-valid", "incoming-null"],
                    "when": ["b", "y"],
                }
            ),
            pd.DataFrame(
                {
                    LEFT_KEY: pd.Series(["L1", pd.NA], dtype="string"),
                    RIGHT_KEY: pd.Series(["R1", pd.NA], dtype="string"),
                }
            ),
        )
    )

    # The finite matrix below probes width and family promotion explicitly.
    # Names are stable manifest identifiers: downstream generators should use
    # this list, rather than relying on report order or case_count alone.
    def matrix_frame(case_id, activity, when, payload, values, dtype):
        frame = pd.DataFrame(
            {
                CASE_KEY: [case_id] * len(values),
                "concept:name": [activity] * len(values),
                "when": [when + str(position) for position in range(len(values))],
            }
        )
        frame[payload] = pd.Series(values, dtype=dtype)
        return frame

    rows.append(
        _run_dtype_case(
            "dtype_matrix_signed_width_int8_int16",
            matrix_frame("L1", "base", "a", "payload", [1, 2], "int8"),
            matrix_frame("R1", "incoming", "b", "payload", [3, 4], "int16"),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_signed_width_int32_int64",
            matrix_frame("L1", "base", "a", "payload", [1, 2], "int32"),
            matrix_frame("R1", "incoming", "b", "payload", [3, 4], "int64"),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_unsigned_width_uint8_uint16",
            matrix_frame("L1", "base", "a", "payload", [1, 2], "uint8"),
            matrix_frame("R1", "incoming", "b", "payload", [3, 4], "uint16"),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_unsigned_width_uint32_uint64_max",
            matrix_frame("L1", "base", "a", "payload", [1, 2], "uint32"),
            matrix_frame(
                "R1",
                "incoming",
                "b",
                "payload",
                [3, 18446744073709551615],
                "uint64",
            ),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_float_width_float32_float64",
            matrix_frame("L1", "base", "a", "payload", [1.25, 2.5], "float32"),
            matrix_frame("R1", "incoming", "b", "payload", [3.75, 4.5], "float64"),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_bool_bool",
            matrix_frame("L1", "base", "a", "payload", [True, False], "bool"),
            matrix_frame("R1", "incoming", "b", "payload", [False, True], "bool"),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_signed_unsigned_int64_uint64",
            matrix_frame("L1", "base", "a", "payload", [1, 2], "int64"),
            matrix_frame("R1", "incoming", "b", "payload", [3, 4], "uint64"),
            _relation(dtype="object"),
        )
    )

    # Missing-side and empty-side cases distinguish a missing column from an
    # explicitly typed empty column.  They also make null promotion visible in
    # the cell tags and metadata snapshots.
    rows.append(
        _run_dtype_case(
            "dtype_matrix_plain_int_missing_base",
            pd.DataFrame({CASE_KEY: ["L1"], "concept:name": ["base"], "when": ["a"]}),
            matrix_frame("R1", "incoming", "b", "payload", [2], "int64"),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_plain_float_missing_incoming",
            matrix_frame("L1", "base", "a", "payload", [1.5], "float64"),
            pd.DataFrame(
                {CASE_KEY: ["R1"], "concept:name": ["incoming"], "when": ["b"]}
            ),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_plain_bool_missing_base",
            pd.DataFrame({CASE_KEY: ["L1"], "concept:name": ["base"], "when": ["a"]}),
            matrix_frame("R1", "incoming", "b", "payload", [True], "bool"),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_empty_typed_base_uint64",
            pd.DataFrame(
                {
                    CASE_KEY: pd.Series([], dtype="string"),
                    "concept:name": pd.Series([], dtype="string"),
                    "when": pd.Series([], dtype="string"),
                    "payload": pd.Series([], dtype="uint64"),
                }
            ),
            matrix_frame("R1", "incoming", "b", "payload", [2], "uint64"),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_empty_typed_incoming_float32",
            matrix_frame("L1", "base", "a", "payload", [1.5], "float32"),
            pd.DataFrame(
                {
                    CASE_KEY: pd.Series([], dtype="string"),
                    "concept:name": pd.Series([], dtype="string"),
                    "when": pd.Series([], dtype="string"),
                    "payload": pd.Series([], dtype="float32"),
                }
            ),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_empty_both_different_numeric",
            pd.DataFrame(
                {
                    CASE_KEY: pd.Series([], dtype="string"),
                    "concept:name": pd.Series([], dtype="string"),
                    "when": pd.Series([], dtype="string"),
                    "payload": pd.Series([], dtype="int16"),
                }
            ),
            pd.DataFrame(
                {
                    CASE_KEY: pd.Series([], dtype="string"),
                    "concept:name": pd.Series([], dtype="string"),
                    "when": pd.Series([], dtype="string"),
                    "payload": pd.Series([], dtype="float32"),
                }
            ),
            _empty_relation(dtype="string"),
        )
    )

    # Nullable extension families are kept distinct from their plain pandas
    # counterparts, including unsigned integers and object nulls.
    rows.append(
        _run_dtype_case(
            "dtype_matrix_nullable_uint64_nulls",
            matrix_frame("L1", "base", "a", "payload", [1, pd.NA], "UInt64"),
            matrix_frame("R1", "incoming", "b", "payload", [pd.NA, 3], "UInt64"),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_nullable_int64_missing_incoming",
            matrix_frame("L1", "base", "a", "payload", [1], "Int64"),
            pd.DataFrame(
                {CASE_KEY: ["R1"], "concept:name": ["incoming"], "when": ["b"]}
            ),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_nullable_float64_missing_base",
            pd.DataFrame({CASE_KEY: ["L1"], "concept:name": ["base"], "when": ["a"]}),
            matrix_frame("R1", "incoming", "b", "payload", [1.5], "Float64"),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_nullable_boolean_missing_incoming",
            matrix_frame("L1", "base", "a", "payload", [True], "boolean"),
            pd.DataFrame(
                {CASE_KEY: ["R1"], "concept:name": ["incoming"], "when": ["b"]}
            ),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_object_payload_nulls",
            matrix_frame("L1", "base", "a", "payload", ["x", None], "object"),
            matrix_frame("R1", "incoming", "b", "payload", [None, "y"], "object"),
            _relation(dtype="object"),
        )
    )

    def categorical_frame(
        case_id, activity, when, values, categories, ordered, payload="label"
    ):
        return pd.DataFrame(
            {
                CASE_KEY: [case_id] * len(values),
                "concept:name": [activity] * len(values),
                "when": [when + str(position) for position in range(len(values))],
                payload: pd.Series(
                    pd.Categorical(values, categories=categories, ordered=ordered)
                ),
            }
        )

    # Category metadata cases cover unordered/reordered categories, membership
    # differences, and category mixtures with strings and numeric payloads.
    rows.append(
        _run_dtype_case(
            "dtype_matrix_category_unordered_same_categories",
            categorical_frame("L1", "base", "a", ["low"], ["low", "high"], False),
            categorical_frame("R1", "incoming", "b", ["high"], ["low", "high"], False),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_category_unordered_reordered_categories",
            categorical_frame("L1", "base", "a", ["low"], ["low", "high"], False),
            categorical_frame("R1", "incoming", "b", ["high"], ["high", "low"], False),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_category_ordered_different_membership",
            categorical_frame("L1", "base", "a", ["low"], ["low", "high"], True),
            categorical_frame("R1", "incoming", "b", ["new"], ["low", "new"], True),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_category_string_mixture",
            categorical_frame("L1", "base", "a", ["low"], ["low", "high"], False),
            matrix_frame("R1", "incoming", "b", "label", ["high"], "object"),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_category_numeric_mixture",
            categorical_frame("L1", "base", "a", [1], [1, 2], False, payload="score"),
            matrix_frame("R1", "incoming", "b", "score", [2], "int64"),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_category_case_ids_matching_relation_categories",
            pd.DataFrame(
                {
                    CASE_KEY: pd.Series(
                        pd.Categorical(["L1"], categories=["L1", "L2"], ordered=False)
                    ),
                    "concept:name": ["base"],
                    "when": ["a"],
                }
            ),
            pd.DataFrame(
                {
                    CASE_KEY: pd.Series(
                        pd.Categorical(["R1"], categories=["R1", "R2"], ordered=False)
                    ),
                    "concept:name": ["incoming"],
                    "when": ["b"],
                }
            ),
            pd.DataFrame(
                {
                    LEFT_KEY: pd.Series(
                        pd.Categorical(["L1"], categories=["L1", "L2"], ordered=False)
                    ),
                    RIGHT_KEY: pd.Series(
                        pd.Categorical(["R1"], categories=["R1", "R2"], ordered=False)
                    ),
                }
            ),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_category_missing_values",
            categorical_frame("L1", "base", "a", ["low", None], ["low", "high"], False),
            categorical_frame(
                "R1", "incoming", "b", ["high", None], ["low", "high"], False
            ),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_category_tuple_values",
            categorical_frame(
                "L1",
                "base",
                "a",
                [("low", 1)],
                [("low", 1), ("high", 2)],
                False,
            ),
            categorical_frame(
                "R1",
                "incoming",
                "b",
                [("high", 2)],
                [("low", 1), ("high", 2)],
                False,
            ),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_matrix_category_ordered_numeric_values",
            categorical_frame("L1", "base", "a", [1], [1, 2, 3], True, payload="score"),
            categorical_frame(
                "R1", "incoming", "b", [3], [1, 2, 3], True, payload="score"
            ),
            _relation(dtype="object"),
        )
    )

    # Exercise both operand orders for every supported primitive/nullable
    # family. These payload columns do not participate in join or sort keys,
    # isolating public concatenation coercion from identifier semantics.
    promotion_types = (
        "int8",
        "int16",
        "int32",
        "int64",
        "uint8",
        "uint16",
        "uint32",
        "uint64",
        "float32",
        "float64",
        "bool",
        "Int64",
        "UInt64",
        "Float64",
        "boolean",
        "str",
        "string",
        "object",
    )

    def promotion_values(dtype, incoming=False):
        if dtype in ("bool", "boolean"):
            return [not incoming]
        if dtype in ("str", "string"):
            return ["incoming-text" if incoming else "base-text"]
        if "float" in dtype.lower():
            return [2.1 if incoming else 1.1]
        return [5 if incoming else 7]

    for base_dtype in promotion_types:
        for incoming_dtype in promotion_types:
            rows.append(
                _run_dtype_case(
                    "dtype_promotion_" + base_dtype + "_" + incoming_dtype,
                    matrix_frame(
                        "L1",
                        "base",
                        "a",
                        "payload",
                        promotion_values(base_dtype),
                        base_dtype,
                    ),
                    matrix_frame(
                        "R1",
                        "incoming",
                        "b",
                        "payload",
                        promotion_values(incoming_dtype, True),
                        incoming_dtype,
                    ),
                    _relation(dtype="object"),
                )
            )
    for ordered in (True, False):
        category_type = pd.CategoricalDtype(["B", "A"], ordered=ordered)
        base = pd.DataFrame(
            {
                CASE_KEY: pd.Series(["A", "B"], dtype=category_type),
                "concept:name": ["base-a", "base-b"],
                "when": ["a", "a"],
            }
        )
        incoming = pd.DataFrame(
            {
                CASE_KEY: pd.Series(["R1", "R2"], dtype="category"),
                "concept:name": ["incoming-a", "incoming-b"],
                "when": ["b", "b"],
            }
        )
        relations = pd.DataFrame(
            {
                LEFT_KEY: pd.Series(["A", "B"], dtype=category_type),
                RIGHT_KEY: pd.Series(["R1", "R2"], dtype="category"),
            }
        )
        rows.append(
            _run_dtype_case(
                "dtype_category_case_sort_" + ("ordered" if ordered else "unordered"),
                base,
                incoming,
                relations,
            )
        )
    category_type = pd.CategoricalDtype(["high", "low"], ordered=True)
    rows.append(
        _run_dtype_case(
            "dtype_category_timestamp_sort",
            pd.DataFrame(
                {
                    CASE_KEY: ["L1", "L1"],
                    "concept:name": ["base-low", "base-high"],
                    "when": pd.Series(["low", "high"], dtype=category_type),
                }
            ),
            pd.DataFrame(
                {
                    CASE_KEY: ["R1"],
                    "concept:name": ["incoming-high"],
                    "when": pd.Series(["high"], dtype=category_type),
                }
            ),
            _relation(dtype="object"),
        )
    )
    for plain_type in ("str", "string", "object"):
        for category_first in (True, False):
            categorical = categorical_frame(
                "L1" if category_first else "R1",
                "category",
                "a" if category_first else "b",
                ["low"],
                ["low", "high"],
                False,
            )
            plain = matrix_frame(
                "R1" if category_first else "L1",
                "plain",
                "b" if category_first else "a",
                "label",
                ["high"],
                plain_type,
            )
            rows.append(
                _run_dtype_case(
                    "dtype_category_"
                    + plain_type
                    + "_"
                    + ("first" if category_first else "last"),
                    categorical if category_first else plain,
                    plain if category_first else categorical,
                    _relation(dtype="object"),
                )
            )
    rows.append(
        _run_dtype_case(
            "dtype_numeric_then_category",
            matrix_frame("L1", "base", "a", "score", [2], "int64"),
            categorical_frame(
                "R1", "incoming", "b", [1], [1, 2], False, payload="score"
            ),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_category_tuple_null_element",
            categorical_frame(
                "L1", "base", "a", [(None, 1)], [(None, 1), ("x", 2)], False
            ),
            categorical_frame(
                "R1", "incoming", "b", [("x", 2)], [(None, 1), ("x", 2)], False
            ),
            _relation(dtype="object"),
        )
    )
    rows.append(
        _run_dtype_case(
            "dtype_extreme_signed_unsigned_float_promotion",
            matrix_frame("L1", "base", "a", "payload", [-(2**63)], "int64"),
            matrix_frame("R1", "incoming", "b", "payload", [2**64 - 1], "uint64"),
            _relation(dtype="object"),
        )
    )
    category_type = pd.CategoricalDtype(["B", "A"], ordered=False)
    rows.append(
        _run_dtype_case(
            "dtype_category_case_sort_missing",
            pd.DataFrame(
                {
                    CASE_KEY: pd.Series(["A", None], dtype=category_type),
                    "concept:name": ["base-a", "base-null"],
                    "when": ["a", "b"],
                }
            ),
            pd.DataFrame(
                {
                    CASE_KEY: pd.Series(["R1"], dtype="category"),
                    "concept:name": ["incoming-b"],
                    "when": ["c"],
                }
            ),
            pd.DataFrame(
                {
                    LEFT_KEY: pd.Series(["B"], dtype=category_type),
                    RIGHT_KEY: pd.Series(["R1"], dtype="category"),
                }
            ),
        )
    )
    for suffix, left, right in (
        (
            "float",
            pd.Index([1.25, 2.5], dtype="float64"),
            pd.Index([3.75, 4.5], dtype="float64"),
        ),
        (
            "uint64",
            pd.Index([2**64 - 2, 2**64 - 1], dtype="uint64"),
            pd.Index([2**64 - 3, 2**64 - 1], dtype="uint64"),
        ),
        (
            "signed_unsigned",
            pd.Index([-1, 2], dtype="int64"),
            pd.Index([2**64 - 2, 2**64 - 1], dtype="uint64"),
        ),
        (
            "float32",
            pd.Index([1.25, 2.5], dtype="float32"),
            pd.Index([3.75, 4.5], dtype="float32"),
        ),
    ):
        rows.append(
            _run_dtype_case(
                "dtype_incompatible_numeric_categories_" + suffix,
                categorical_frame("L1", "base", "a", [left[0]], left, False),
                categorical_frame("R1", "incoming", "b", [right[-1]], right, False),
                _relation(dtype="object"),
            )
        )
    rows.append(
        _run_dtype_case(
            "dtype_datetime_same_timezone_mixed_resolution",
            pd.DataFrame(
                {
                    CASE_KEY: ["L1"],
                    "concept:name": ["base"],
                    "when": pd.Series(
                        ["2024-01-01T00:00:00.000000123Z"], dtype="datetime64[ns, UTC]"
                    ),
                }
            ),
            pd.DataFrame(
                {
                    CASE_KEY: ["R1"],
                    "concept:name": ["incoming"],
                    "when": pd.Series(
                        ["2024-01-01T00:00:00.000001Z"], dtype="datetime64[us, UTC]"
                    ),
                }
            ),
            _relation(dtype="object"),
        )
    )
    return rows


def _findings(rows):
    by_name = {row["name"]: row for row in rows}

    def dtypes(name):
        row = by_name[name]
        if "result" not in row:
            return None
        return row["result"]["dtypes"]

    def dtype_for(name, column):
        values = dtypes(name)
        return None if values is None else values.get(column)

    findings = [
        {
            "id": "numeric-promotion",
            "cases": [
                "dtype_float_payload_integer_incoming",
                "dtype_integer_payload_float_incoming",
            ],
            "rule": "float64 plus int64 yields float64 for the shared payload column; integer-only data remains integer unless a missing value is introduced.",
            "observed_output_dtypes": {
                name: dtypes(name)["score"]
                for name in (
                    "dtype_float_payload_integer_incoming",
                    "dtype_integer_payload_float_incoming",
                )
            },
        },
        {
            "id": "missing-plain-numeric",
            "cases": [
                "dtype_integer_payload_missing_incoming",
                "dtype_boolean_payload_missing_incoming",
            ],
            "rule": "An omitted payload column is filled with missing values; plain int64 becomes float64 and plain bool becomes object.",
            "observed_output_dtypes": {
                "integer_score": dtypes("dtype_integer_payload_missing_incoming")[
                    "score"
                ],
                "boolean_flag": dtypes("dtype_boolean_payload_missing_incoming")[
                    "flag"
                ],
            },
        },
        {
            "id": "boolean-numeric",
            "cases": [
                "dtype_boolean_payload_integer_incoming",
                "dtype_boolean_timestamp_integer_incoming",
            ],
            "rule": "bool combined with int64 is represented as int64, including when the timestamp column is the mixed field.",
            "observed_output_dtypes": {
                "payload_flag": dtypes("dtype_boolean_payload_integer_incoming")[
                    "flag"
                ],
                "timestamp": dtypes("dtype_boolean_timestamp_integer_incoming")["when"],
            },
        },
        {
            "id": "nullable-extension-preservation",
            "cases": [
                "dtype_nullable_int64_nulls",
                "dtype_nullable_float64_nulls",
                "dtype_nullable_boolean_nulls",
                "dtype_nullable_string_nulls",
            ],
            "rule": "Pandas nullable Int64, Float64, boolean, and string payload columns retain their extension dtype and missing cells through relation expansion.",
            "observed_output_dtypes": {
                "Int64": dtypes("dtype_nullable_int64_nulls")["score"],
                "Float64": dtypes("dtype_nullable_float64_nulls")["score"],
                "boolean": dtypes("dtype_nullable_boolean_nulls")["flag"],
                "string": dtypes("dtype_nullable_string_nulls")["label"],
            },
        },
        {
            "id": "datetime-resolution-and-missing",
            "cases": [
                "dtype_datetime_us_ns",
                "dtype_datetime_missing_incoming",
                "dtype_datetime_missing_base",
            ],
            "rule": "datetime64[us] and datetime64[ns] are combined as datetime64[ns]; a missing timestamp side is materialized as NaT using the available side's datetime dtype.",
            "observed_output_dtypes": {
                "mixed_resolution": dtypes("dtype_datetime_us_ns")["when"],
                "missing_incoming": dtypes("dtype_datetime_missing_incoming")["when"],
                "missing_base": dtypes("dtype_datetime_missing_base")["when"],
            },
        },
        {
            "id": "timezone-offset-boundary",
            "cases": ["dtype_datetime_timezone_offsets"],
            "rule": "Offset-aware timestamps with different timezone objects remain object dtype while retaining each public timezone-aware cell.",
            "observed_output_dtypes": {
                "when": dtypes("dtype_datetime_timezone_offsets")["when"]
            },
        },
        {
            "id": "empty-typed-schema",
            "cases": [
                "dtype_all_empty_typed_inputs",
                "dtype_empty_typed_incoming",
                "dtype_empty_typed_relations",
            ],
            "rule": "Empty typed inputs are accepted; the result carries the union schema and pandas-selected dtypes, and an empty typed relation table preserves base rows with null relation columns.",
            "observed_output_dtypes": {
                name: by_name[name].get("result", {}).get("dtypes")
                for name in (
                    "dtype_all_empty_typed_inputs",
                    "dtype_empty_typed_incoming",
                    "dtype_empty_typed_relations",
                )
            },
        },
        {
            "id": "categorical-metadata-boundary",
            "cases": [
                "dtype_categorical_case_ids_object_relations",
                "dtype_ordered_categorical_payload_same_categories",
                "dtype_ordered_categorical_payload_different_order",
            ],
            "rule": "Categorical case identifiers are coerced to object when the relation keys are generic object columns; matching ordered payload categories survive, while different ordered category metadata falls back to string.",
            "observed_output_dtypes": {
                "case_id": dtypes("dtype_categorical_case_ids_object_relations")[
                    CASE_KEY
                ],
                "same_categories": dtypes(
                    "dtype_ordered_categorical_payload_same_categories"
                )["label"],
                "different_order": dtypes(
                    "dtype_ordered_categorical_payload_different_order"
                )["label"],
            },
        },
        {
            "id": "nullable-identifier-boundary",
            "cases": ["dtype_nullable_string_case_ids_and_null_relation"],
            "rule": "Nullable string case identifiers and relation keys retain string extension dtype, including matching null keys; an implementation needs explicit nullable key metadata to reproduce this without relying on generic object values.",
            "observed_output_dtypes": {
                "case_id": dtypes("dtype_nullable_string_case_ids_and_null_relation")[
                    CASE_KEY
                ],
                "left_relation": dtypes(
                    "dtype_nullable_string_case_ids_and_null_relation"
                )[LEFT_KEY],
                "right_relation": dtypes(
                    "dtype_nullable_string_case_ids_and_null_relation"
                )[RIGHT_KEY],
            },
        },
    ]
    findings.extend(
        [
            {
                "id": "plain-width-matrix",
                "cases": [
                    "dtype_matrix_signed_width_int8_int16",
                    "dtype_matrix_signed_width_int32_int64",
                    "dtype_matrix_unsigned_width_uint8_uint16",
                    "dtype_matrix_unsigned_width_uint32_uint64_max",
                    "dtype_matrix_float_width_float32_float64",
                    "dtype_matrix_bool_bool",
                    "dtype_matrix_signed_unsigned_int64_uint64",
                ],
                "rule": "The public merge exposes pandas-selected promotion across signed, unsigned, floating, boolean, and signed/unsigned payload families, including the maximum uint64 value.",
                "observed_output_dtypes": {
                    name: dtype_for(name, "payload")
                    for name in (
                        "dtype_matrix_signed_width_int8_int16",
                        "dtype_matrix_signed_width_int32_int64",
                        "dtype_matrix_unsigned_width_uint8_uint16",
                        "dtype_matrix_unsigned_width_uint32_uint64_max",
                        "dtype_matrix_float_width_float32_float64",
                        "dtype_matrix_bool_bool",
                        "dtype_matrix_signed_unsigned_int64_uint64",
                    )
                },
            },
            {
                "id": "missing-and-empty-matrix",
                "cases": [
                    "dtype_matrix_plain_int_missing_base",
                    "dtype_matrix_plain_float_missing_incoming",
                    "dtype_matrix_plain_bool_missing_base",
                    "dtype_matrix_empty_typed_base_uint64",
                    "dtype_matrix_empty_typed_incoming_float32",
                    "dtype_matrix_empty_both_different_numeric",
                ],
                "rule": "Missing columns, explicitly typed empty columns, and empty inputs are separate public states whose output schema and promotion are recorded independently.",
                "observed_output_dtypes": {
                    name: by_name[name].get("result", {}).get("dtypes")
                    for name in (
                        "dtype_matrix_plain_int_missing_base",
                        "dtype_matrix_plain_float_missing_incoming",
                        "dtype_matrix_plain_bool_missing_base",
                        "dtype_matrix_empty_typed_base_uint64",
                        "dtype_matrix_empty_typed_incoming_float32",
                        "dtype_matrix_empty_both_different_numeric",
                    )
                },
            },
            {
                "id": "nullable-extension-matrix",
                "cases": [
                    "dtype_matrix_nullable_uint64_nulls",
                    "dtype_matrix_nullable_int64_missing_incoming",
                    "dtype_matrix_nullable_float64_missing_base",
                    "dtype_matrix_nullable_boolean_missing_incoming",
                    "dtype_matrix_object_payload_nulls",
                ],
                "rule": "Nullable unsigned, signed, floating, boolean, string, and object payloads retain their public extension/object metadata across null and missing-side cases.",
                "observed_output_dtypes": {
                    name: dtype_for(name, "payload")
                    for name in (
                        "dtype_matrix_nullable_uint64_nulls",
                        "dtype_matrix_nullable_int64_missing_incoming",
                        "dtype_matrix_nullable_float64_missing_base",
                        "dtype_matrix_nullable_boolean_missing_incoming",
                        "dtype_matrix_object_payload_nulls",
                    )
                },
            },
            {
                "id": "categorical-matrix",
                "cases": [
                    "dtype_matrix_category_unordered_same_categories",
                    "dtype_matrix_category_unordered_reordered_categories",
                    "dtype_matrix_category_ordered_different_membership",
                    "dtype_matrix_category_string_mixture",
                    "dtype_matrix_category_numeric_mixture",
                    "dtype_matrix_category_case_ids_matching_relation_categories",
                    "dtype_matrix_category_missing_values",
                    "dtype_matrix_category_tuple_values",
                    "dtype_matrix_category_ordered_numeric_values",
                ],
                "rule": "Categorical output depends on orderedness, category order, membership, and mixture with ordinary string or numeric columns; the report preserves category descriptors for every categorical input and output column.",
                "observed_output_dtypes": {
                    name: (
                        dtype_for(name, "label")
                        if dtype_for(name, "label") is not None
                        else dtype_for(name, "score")
                    )
                    for name in (
                        "dtype_matrix_category_unordered_same_categories",
                        "dtype_matrix_category_unordered_reordered_categories",
                        "dtype_matrix_category_ordered_different_membership",
                        "dtype_matrix_category_string_mixture",
                        "dtype_matrix_category_numeric_mixture",
                        "dtype_matrix_category_case_ids_matching_relation_categories",
                        "dtype_matrix_category_missing_values",
                        "dtype_matrix_category_tuple_values",
                        "dtype_matrix_category_ordered_numeric_values",
                    )
                },
            },
        ]
    )
    return findings


def capture(reference_checkout):
    if pm4py.__version__ != REFERENCE_VERSION:
        raise RuntimeError("This oracle requires reference version 2.7.22")
    if pd.__version__ != "3.0.5":
        raise RuntimeError("This oracle requires pandas version 3.0.5")
    provenance = _reference_provenance(reference_checkout)
    rows = _capture_rows()
    report = {
        "format_version": 2,
        "scope": "Finite public dtype and categorical metadata boundary evidence for PM4Py case_merge; not a claim of full pandas dtype-family parity.",
        "reference_version": pm4py.__version__,
        "pandas_version": pd.__version__,
        "reference_commit": REFERENCE_COMMIT,
        "reference_provenance": provenance,
        "reference_checkout": str(Path(reference_checkout).resolve()),
        "algorithm": "pm4py.algo.merging.case_relations.algorithm.apply",
        "case_count": len(rows),
        "rows": rows,
        "findings": _findings(rows),
        "limits": [
            "The probe covers the listed pandas dtypes and null patterns only; it does not establish behavior for every extension dtype, Arrow dtype, period, interval, sparse, decimal, or timezone combination.",
            "Values and missingness are recorded through public pandas scalar access; this artifact does not claim byte-for-byte pandas internals or index equivalence.",
            "Categorical results depend on category membership/order metadata, so a string-only native table needs separate metadata to reproduce those boundaries; the descriptor schema records only public pandas category state.",
            "The matrix is intentionally finite and named; unsupported native representations must not be treated as covered merely because a pandas case appears in this report.",
        ],
    }
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--reference-checkout", type=Path, required=True)
    args = parser.parse_args()
    result = capture(args.reference_checkout)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(
        json.dumps(
            {
                "reference_version": result["reference_version"],
                "pandas_version": result["pandas_version"],
                "case_count": result["case_count"],
            }
        )
    )
