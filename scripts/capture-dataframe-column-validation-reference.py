"""Capture public dataframe schema/selector validation through its public utility."""

import argparse
import json
from pathlib import Path
import runpy

META = runpy.run_path(
    str(Path(__file__).with_name("capture-dataframe-conversion-metadata-reference.py"))
)
pd, pm = META["pd"], META["pm"]
PARAMS = {
    "case_id_key": "case:concept:name",
    "activity_key": "concept:name",
    "timestamp_key": "time:timestamp",
}


def cases():
    base = pd.DataFrame(
        {
            "case:concept:name": pd.Series(["C"], dtype="str"),
            "concept:name": pd.Series(["A"], dtype="str"),
            "time:timestamp": pd.to_datetime(["2024-01-01"], utc=True),
        }
    )
    result = [
        ("valid", base, PARAMS),
        ("valid_no_selectors", base, {}),
        ("empty_no_columns", pd.DataFrame(), PARAMS),
        ("two_strings_only", base.drop(columns="time:timestamp"), PARAMS),
        ("one_string_date", base.drop(columns="concept:name"), PARAMS),
    ]
    for column, label in [
        ("case:concept:name", "case"),
        ("concept:name", "activity"),
        ("time:timestamp", "timestamp"),
    ]:
        for dtype in (
            "object",
            "str",
            "string",
            "int64",
            "float64",
            "bool",
            "category",
        ):
            frame = base.copy()
            frame[column] = pd.Series(
                [1] if dtype in ("int64", "float64", "bool") else ["x"], dtype=dtype
            )
            result.append((label + "_" + dtype, frame, PARAMS))
        for value, label2 in [
            (None, "none"),
            (float("nan"), "nan"),
            ("", "empty"),
            (" ", "space"),
        ]:
            frame = base.copy()
            frame[column] = pd.Series([value], dtype="object")
            result.append((label + "_" + label2, frame, PARAMS))
    for role in PARAMS:
        params = dict(PARAMS)
        params[role] = "missing"
        result.append(("missing_" + role, base, params))
    extended = base.copy()
    extended["extra_string"] = "s"
    extended["extra_date"] = pd.to_datetime(["2024-01-02"], utc=True)
    for column, label in [
        ("case:concept:name", "case"),
        ("concept:name", "activity"),
        ("time:timestamp", "timestamp"),
    ]:
        for dtype, values in [("int64", [1]), ("category", ["x"]), ("object", [None])]:
            frame = extended.copy()
            frame[column] = pd.Series(values, dtype=dtype)
            result.append(("selected_" + label + "_" + dtype, frame, PARAMS))
    for label, value in [("nat", pd.NaT), ("valid", pd.Timestamp("2024-01-01"))]:
        frame = extended.copy()
        frame["time:timestamp"] = pd.to_datetime([value])
        result.append(("selected_timestamp_" + label, frame, PARAMS))
    for label, series in [
        ("valid", pd.to_datetime(["2024-01-01"])),
        ("null", pd.to_datetime([pd.NaT])),
        ("text", pd.Series(["x"], dtype="str")),
        ("absent", None),
    ]:
        frame = extended.copy()
        if series is not None:
            frame["start"] = series
        result.append(
            ("start_" + label, frame, dict(PARAMS, start_timestamp_key="start"))
        )
    for role in PARAMS:
        params = dict(PARAMS)
        params[role] = ""
        result.append(("empty_selector_" + role, extended, params))
    frame = extended.copy()
    frame["case:concept:name"] = None
    frame["concept:name"] = None
    frame["time:timestamp"] = pd.to_datetime([pd.NaT])
    result.append(("all_null", frame, PARAMS))
    result.append(("null_no_selectors", frame, {}))
    result.append(("typed_empty", base.iloc[:0].copy(), PARAMS))
    frame = base.copy()
    frame.attrs = {"nested": {"value": True}}
    result.append(("attrs_preserved", frame, PARAMS))
    result.append(
        ("shared_selector", base, dict(PARAMS, activity_key="case:concept:name"))
    )
    return result


def capture(checkout):
    if pm.__version__ != "2.7.22" or pd.__version__ != "3.0.5":
        raise RuntimeError("Wrong runtime")
    function = pm.utils.check_pandas_dataframe_columns
    proof = META["provenance"](function, checkout)
    rows = []
    for name, frame, parameters in cases():
        row = {
            "name": name,
            "parameters": parameters,
            "input": META["FORMAT"]["describe_frame"](frame),
        }
        try:
            row["result"] = function(frame, **parameters)
        except Exception as error:
            row["error"] = {"type": type(error).__name__, "message": str(error)}
        row["input_after"] = META["FORMAT"]["describe_frame"](frame)
        rows.append(row)
    return {
        "capture_id": "dataframe_column_validation",
        "reference_version": pm.__version__,
        "pandas_version": pd.__version__,
        "reference_provenance": proof,
        "case_count": len(rows),
        "rows": rows,
        "limits": [
            "Finite declared-dtype/string-selector validation observations; non-string/duplicate labels and all extension dtypes are not established.",
            "Native validation preserves rejection messages; Python exception-class identity is not claimed.",
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
                "successes": sum("result" in r for r in report["rows"]),
                "errors": sum("error" in r for r in report["rows"]),
            }
        )
    )
