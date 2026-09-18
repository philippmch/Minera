"""Capture typed case grouping through the public event-stream conversion API."""

import argparse
import json
import math
from pathlib import Path
import runpy

META = runpy.run_path(
    str(Path(__file__).with_name("capture-dataframe-conversion-metadata-reference.py"))
)
pm = META["pm"]


def scalar(value):
    if isinstance(value, float) and not math.isfinite(value):
        return {
            "kind": "float",
            "value": "NaN"
            if math.isnan(value)
            else ("Infinity" if value > 0 else "-Infinity"),
        }
    return {
        "kind": META["FORMAT"]["scalar_kind"](value),
        "value": META["FORMAT"]["normalize"](value),
    }


def capture(checkout):
    if pm.__version__ != "2.7.22":
        raise RuntimeError("Wrong reference runtime")
    proof = META["provenance"](pm.convert_to_event_log, checkout)
    from pm4py.objects.log.obj import EventStream, Event

    shared_nan = float("nan")
    cases = (
        ("integer_float_bool", [1, 1.0, True, 2]),
        ("bool_first", [True, 1, 1.0, False, 0, -0.0]),
        ("signed_zero", [0.0, -0.0, 0, False]),
        ("strings_separate", [1, "1", 1.0, True, "True"]),
        ("wide_exact", [9007199254740992, 9007199254740993, 9007199254740992.0]),
        (
            "unsigned",
            [18446744073709551615, 18446744073709551614, float(18446744073709551615)],
        ),
        (
            "signed_min",
            [-9223372036854775808, float(-9223372036854775808), -9223372036854775807],
        ),
        ("fractional", [1.5, 1, 1.5, -1.5]),
        ("null", [None, None, "None"]),
        ("different_nan", [float("nan"), float("nan")]),
        ("shared_nan", [shared_nan, shared_nan]),
        ("infinities", [float("inf"), float("inf"), float("-inf"), float("-inf")]),
        (
            "uint_float_exact",
            [9223372036854775808, float(9223372036854775808), 9223372036854775809],
        ),
        ("negative_wide", [-9007199254740993, -9007199254740992, -9007199254740992.0]),
        ("negative_fraction", [-1.0, -1, -1.5, -1.5]),
        (
            "positive_boundary",
            [18446744073709549568, float(18446744073709549568), 18446744073709549569],
        ),
        ("float_first", [1.0, True, 1, "1.0"]),
    )
    rows = []
    for name, values in cases:
        source = EventStream(
            [
                Event({"case:concept:name": v, "concept:name": "A" + str(i)})
                for i, v in enumerate(values)
            ]
        )
        row = {
            "name": name,
            "input": [
                {
                    "case": scalar(v),
                    "activity": "A" + str(i),
                    "identity": next(j for j in range(i + 1) if values[j] is v),
                }
                for i, v in enumerate(values)
            ],
        }
        try:
            output = pm.convert_to_event_log(source)
            row["result"] = [
                {
                    "case": scalar(trace.attributes["concept:name"]),
                    "activities": [event["concept:name"] for event in trace],
                    "event_keys": [list(event.keys()) for event in trace],
                }
                for trace in output
            ]
        except Exception as error:
            row["error"] = {"type": type(error).__name__, "message": str(error)}
        row["input_after"] = [
            {
                "case": scalar(event["case:concept:name"]),
                "activity": event["concept:name"],
            }
            for event in source
        ]
        rows.append(row)
    return {
        "capture_id": "conversion_case_grouping",
        "reference_version": pm.__version__,
        "reference_provenance": proof,
        "case_count": len(rows),
        "rows": rows,
        "limits": [
            "Finite scalar grouping observations, not exhaustive hashability, date-key, container-key or conversion error coverage.",
            "Scalar identity is recorded to distinguish reused NaN objects from separate NaNs.",
        ],
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--reference-checkout", type=Path, required=True)
    args = parser.parse_args()
    report = capture(args.reference_checkout)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(
        json.dumps(
            {
                "cases": report["case_count"],
                "errors": [r["name"] for r in report["rows"] if "error" in r],
            }
        )
    )
