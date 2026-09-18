"""Capture finite public case-merge index behavior without reading implementation.

Uses PM4Py 2.7.22 and pandas 3.0.5. Scalar/tuple index labels are recorded
separately from the legacy table description so tuple identity is not lost.
"""

import warnings

warnings.filterwarnings("ignore")
import argparse
import json
from pathlib import Path
import runpy
import pandas as pd

api = runpy.run_path(
    str(Path(__file__).with_name("capture-case-relations-table-reference.py"))
)
if api["pm4py"].__version__ != "2.7.22" or pd.__version__ != "3.0.5":
    raise RuntimeError("Wrong public runtime")


def label(value):
    if isinstance(value, tuple):
        return {"kind": "tuple", "value": [label(part) for part in value]}
    return {"kind": api["scalar_kind"](value), "value": api["normalize"](value)}


def index_info(frame):
    return {
        "class": type(frame.index).__name__,
        "dtype": str(frame.index.dtype),
        "names": [api["normalize"](name) for name in frame.index.names],
        "labels": [label(value) for value in frame.index],
        "column_dtypes": {
            str(column): str(frame[column].dtype) for column in frame.columns
        },
    }


def frames():
    base = pd.DataFrame(
        {
            "case:concept:name": ["L2", "L1", "L1"],
            "concept:name": ["b2", "b1-late", "b1-early"],
            "when": ["b", "c", "a"],
        }
    )
    incoming = pd.DataFrame(
        {
            "case:concept:name": ["R1", "R2"],
            "concept:name": ["i1", "i2"],
            "when": ["b", "a"],
        }
    )
    relations = pd.DataFrame(
        {
            "case:concept:name_LEFT": ["L1", "L1", "L2"],
            "case:concept:name_RIGHT": ["R1", "R1", "R2"],
        }
    )
    return base, incoming, relations


def capture(checkout):
    provenance_api = runpy.run_path(
        str(Path(__file__).with_name("capture-case-relations-time-reference.py"))
    )
    provenance = provenance_api["reference_provenance"](checkout)
    rows = []
    for mode in (
        "default_range",
        "integer_labels",
        "string_labels",
        "shared_name",
        "null_label",
        "datetime_labels",
        "multiindex",
        "empty_relations",
        "empty_base",
        "multiindex_empty_relations",
        "string_empty_relations",
        "datetime_empty_relations",
        "all_inputs_empty",
        "nullable_integer_index",
        "float_index",
        "default_empty_base",
        "default_all_empty",
        "default_range_sorted_no_relations",
        "default_range_empty_relations",
        "default_empty_base_reordered",
        "offset_range_empty_relations",
        "offset_range_sorted_empty_relations",
        "range_nonarithmetic_sort",
        "range_contiguous_concat",
        "range_discontinuous_concat_reordered",
        "range_extreme_step",
    ):
        base, incoming, relations = frames()
        base.index = pd.Index([40, -7, 40], name="source_row")
        incoming.index = pd.Index([101, 99], name="incoming_row")
        relations.index = pd.Index(["rel-a", "rel-b", "rel-c"], name="relation_row")
        if mode == "default_range":
            for frame in (base, incoming, relations):
                frame.index = pd.RangeIndex(len(frame))
        if mode == "string_labels":
            base.index = pd.Index(["z", "a", "a"], name="row")
        if mode == "shared_name":
            for frame in (base, incoming, relations):
                frame.index.name = "row"
        if mode == "null_label":
            base.index = pd.Index([None, "x", None], dtype=object, name="row")
        if mode == "datetime_labels":
            base.index = pd.to_datetime(
                ["2024-01-03", "2024-01-02", "2024-01-01"], utc=True
            ).rename("row")
        if mode == "multiindex":
            base.index = pd.MultiIndex.from_tuples(
                [("B", 2), ("A", 1), ("A", 0)], names=["batch", "row"]
            )
        if mode == "empty_relations":
            relations = relations.iloc[:0]
        if mode == "empty_base":
            base = base.iloc[:0]
        if mode == "multiindex_empty_relations":
            base.index = pd.MultiIndex.from_tuples(
                [("B", 2), ("A", 1), ("A", 0)], names=["batch", "row"]
            )
            relations = relations.iloc[:0]
        if mode == "string_empty_relations":
            base.index = pd.Index(["z", "a", "a"], name="row")
            relations = relations.iloc[:0]
        if mode == "datetime_empty_relations":
            base.index = pd.to_datetime(
                ["2024-01-03", "2024-01-02", "2024-01-01"], utc=True
            ).rename("row")
            relations = relations.iloc[:0]
        if mode == "all_inputs_empty":
            base, incoming, relations = [
                frame.iloc[:0] for frame in (base, incoming, relations)
            ]
        if mode == "nullable_integer_index":
            base.index = pd.Index([pd.NA, 1, 2], dtype="Int64", name="nullable")
        if mode == "float_index":
            base.index = pd.Index([float("nan"), 1.5, 2.5], name="floats")
        if mode in (
            "default_empty_base",
            "default_all_empty",
            "default_range_sorted_no_relations",
        ):
            for frame in (base, incoming, relations):
                frame.index = pd.RangeIndex(len(frame))
            if mode == "default_empty_base":
                base = base.iloc[:0]
            if mode == "default_all_empty":
                base, incoming, relations = [
                    frame.iloc[:0] for frame in (base, incoming, relations)
                ]
            if mode == "default_range_sorted_no_relations":
                base = base.iloc[[2, 1, 0]].reset_index(drop=True)
                relations = relations.iloc[:0]
        if mode == "default_range_empty_relations":
            base.index = pd.RangeIndex(len(base))
            relations = relations.iloc[:0]
        if mode == "default_empty_base_reordered":
            base = base.iloc[:0]
            base.index = pd.RangeIndex(0)
            relations["case:concept:name_LEFT"] = ["Z", "Z", "A"]
        if mode in (
            "offset_range_empty_relations",
            "offset_range_sorted_empty_relations",
        ):
            if mode == "offset_range_sorted_empty_relations":
                base = base.iloc[[2, 1, 0]]
            base.index = pd.RangeIndex(5, 11, 2, name="offset")
            relations = relations.iloc[:0]
        if mode == "range_nonarithmetic_sort":
            base = base.iloc[[1, 2, 0]].reset_index(drop=True)
            relations = relations.iloc[:0]
        if mode == "range_contiguous_concat":
            base["case:concept:name"] = ["L1"] * 3
            base["when"] = ["a", "b", "c"]
            base.index = pd.RangeIndex(-3, 0)
            incoming["when"] = ["d", "e"]
            relations["case:concept:name_LEFT"] = ["L1"] * 3
        if mode == "range_discontinuous_concat_reordered":
            base["case:concept:name"] = ["L1"] * 3
            base["when"] = ["d", "e", "f"]
            base.index = pd.RangeIndex(3, 6)
            incoming["when"] = ["a", "b"]
            relations["case:concept:name_LEFT"] = ["L1"] * 3
        if mode == "range_extreme_step":
            base = base.iloc[[2, 1]]
            base.index = pd.RangeIndex(-(2**63), 2**63, 2**64 - 1)
            incoming = incoming.iloc[:0]
            relations = relations.iloc[:0]
        entry = {
            "name": mode,
            "input_indexes": [
                index_info(frame) for frame in (base, incoming, relations)
            ],
            "inputs": [
                api["describe_frame"](frame) for frame in (base, incoming, relations)
            ],
        }
        before = [frame.copy(deep=True) for frame in (base, incoming, relations)]
        try:
            output = api["algorithm"].apply(
                base,
                incoming,
                relations,
                parameters={"pm4py:param:timestamp_key": "when"},
            )
            entry["output_index"] = index_info(output)
            entry["result"] = api["describe_output"](output)
            entry["activities"] = output["concept:name"].tolist()
        except Exception as error:
            entry["error"] = {"type": type(error).__name__, "message": str(error)}
        entry["inputs_unchanged"] = all(
            frame.equals(old) and index_info(frame) == index_info(old)
            for frame, old in zip((base, incoming, relations), before)
        )
        rows.append(entry)
    return {
        "reference_version": "2.7.22",
        "pandas_version": "3.0.5",
        "reference_provenance": provenance,
        "rows": rows,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Capture public case-merge index metadata behavior."
    )
    parser.add_argument("output", type=Path)
    parser.add_argument("--reference-checkout", type=Path, required=True)
    args = parser.parse_args()
    report = capture(args.reference_checkout)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(
        json.dumps(
            {
                "cases": len(report["rows"]),
                "all_inputs_unchanged": all(
                    row["inputs_unchanged"] for row in report["rows"]
                ),
            }
        )
    )
