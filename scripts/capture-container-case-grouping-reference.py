"""Capture container case-key behavior using pinned public conversion calls."""

import argparse
import gzip
import json
from pathlib import Path
import runpy

META = runpy.run_path(
    str(Path(__file__).with_name("capture-dataframe-conversion-metadata-reference.py"))
)
pm = META["pm"]


def encode(value):
    kinds = {
        tuple: "tuple",
        frozenset: "frozenset",
        list: "list",
        dict: "dict",
        set: "set",
    }
    if type(value) in kinds:
        items = list(value.items()) if isinstance(value, dict) else list(value)
        children = [encode(item) for item in items]
        if isinstance(value, (set, frozenset)):
            children.sort(key=lambda item: json.dumps(item, sort_keys=True))
        return {"kind": kinds[type(value)], "children": children}
    return {
        "kind": META["FORMAT"]["scalar_kind"](value),
        "value": META["FORMAT"]["normalize"](value),
    }


def capture(checkout):
    if pm.__version__ != "2.7.22":
        raise RuntimeError("Wrong reference runtime")
    from pm4py.objects.log.obj import EventStream, Event

    proof = META["provenance"](pm.convert_to_event_log, checkout)
    cases = (
        ("tuple_numeric", [(1,), (1.0,), (True,), (2,)]),
        ("tuple_order", [(1, 2), (2, 1)]),
        ("nested_tuple", [((1,), 2), ((True,), 2.0)]),
        (
            "frozenset_numeric",
            [frozenset([1, 2]), frozenset([2.0, True]), frozenset([3])],
        ),
        ("empty_domains", [(), frozenset(), "", None]),
        ("tuple_strings", [("a:b", "c"), ("a", "b:c"), ("a:b", "c")]),
        (
            "tuple_wide",
            [(9007199254740992,), (9007199254740993,), (9007199254740992.0,)],
        ),
        ("tuple_null", [(None,), (None,), ("None",)]),
        ("nested_frozenset", [(frozenset([1, 2]),), (frozenset([2.0, True]),)]),
        ("tuple_vs_frozenset", [(1, 2), frozenset([1, 2])]),
        ("list_rejected", [[1], [1]]),
        ("dict_rejected", [{"a": 1}]),
        ("set_rejected", [{1}]),
        ("nested_list_rejected", [([1],)]),
        ("late_unhashable", [(1,), (2,), [3]]),
    )
    rows = []
    for name, values in cases:
        source = EventStream(
            [
                Event({"case:concept:name": value, "concept:name": "A" + str(i)})
                for i, value in enumerate(values)
            ]
        )
        row = {"name": name, "input": [encode(value) for value in values]}
        try:
            result = pm.convert_to_event_log(source)
            row["result"] = [
                {
                    "case": encode(trace.attributes["concept:name"]),
                    "activities": [event["concept:name"] for event in trace],
                }
                for trace in result
            ]
        except Exception as error:
            row["error"] = {"type": type(error).__name__, "message": str(error)}
        row["input_after"] = [encode(event["case:concept:name"]) for event in source]
        rows.append(row)
    return {
        "reference_version": pm.__version__,
        "reference_provenance": proof,
        "rows": rows,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--checkout", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    raw = (json.dumps(capture(args.checkout), indent=2) + "\n").encode()
    args.output.write_bytes(gzip.compress(raw, mtime=0))
