"""Capture public dataframe-to-log/stream metadata without reading PM implementation."""

import argparse
import hashlib
import importlib
import inspect
import json
from pathlib import Path
import runpy
import subprocess

FORMAT = runpy.run_path(
    str(Path(__file__).with_name("capture-format-dataframe-reference.py"))
)
pd, pm = FORMAT["pd"], FORMAT["pm4py"]
PIN = FORMAT["REFERENCE_COMMIT"]


def provenance(function, checkout):
    module_name = function.__module__
    relative = module_name.replace(".", "/") + ".py"
    installed = (
        Path(importlib.import_module(module_name).__file__)
        .read_bytes()
        .replace(b"\r\n", b"\n")
    )
    pinned = subprocess.check_output(
        ["git", "-C", str(checkout), "show", PIN + ":" + relative]
    ).replace(b"\r\n", b"\n")
    if installed != pinned:
        raise RuntimeError("Reference module mismatch: " + relative)
    return {
        "commit": PIN,
        "function": function.__name__,
        "module": module_name,
        "signature": str(inspect.signature(function)),
        "method": "opaque module bytes, CRLF normalized to LF, no source decoding",
        "installed_sha256": hashlib.sha256(installed).hexdigest(),
        "pinned_sha256": hashlib.sha256(pinned).hexdigest(),
    }


def snapshot(value):
    return {
        "type": type(value).__name__,
        "attributes": FORMAT["normalize"](value.attributes),
        "properties": FORMAT["normalize"](value.properties),
        "extensions": FORMAT["normalize"](value.extensions),
        "classifiers": FORMAT["normalize"](value.classifiers),
        "omni_present": FORMAT["normalize"](value.omni_present),
        "length": len(value),
    }


def capture(checkout):
    if pm.__version__ != "2.7.22" or pd.__version__ != "3.0.5":
        raise RuntimeError("Incorrect public reference runtime")
    proof = [
        provenance(fn, checkout)
        for fn in (
            pm.convert_to_event_log,
            pm.convert_to_event_stream,
            pm.format_dataframe,
        )
    ]
    rows = []
    specs = (
        ("raw", {}, False, False, False),
        (
            "case_property",
            {"case_id_key": "ordinary", "pm4py:param:case_id_key": "alternate"},
            False,
            False,
            False,
        ),
        (
            "nested_attrs",
            {
                "nested": {"enabled": True, "values": [1, "two", None]},
                "source": "probe",
            },
            False,
            False,
            False,
        ),
        (
            "origin_property",
            {"origin": "custom", "pm4py:param:case_id_key": None},
            False,
            False,
            False,
        ),
        ("formatted", {"source": "formatted"}, False, True, False),
        (
            "custom_case",
            {"case_id_key": "cid", "pm4py:param:case_id_key": "cid"},
            True,
            False,
            False,
        ),
        ("custom_formatted", {"source": "custom"}, True, True, False),
        ("empty", {}, False, False, True),
        (
            "empty_attrs",
            {"pm4py:param:case_id_key": "x", "retained": False},
            False,
            False,
            True,
        ),
        ("empty_formatted", {"source": "empty"}, False, True, True),
    )
    for name, attrs, custom, formatted, empty in specs:
        case_key = "cid" if custom else "case:concept:name"
        frame = pd.DataFrame(
            {
                case_key: pd.Series([] if empty else ["C", "D", "C"], dtype="str"),
                "concept:name": pd.Series(
                    [] if empty else ["A", "B", "C"], dtype="str"
                ),
                "time:timestamp": pd.to_datetime(
                    [] if empty else ["2024-01-01", "2024-01-02", "2024-01-03"],
                    utc=True,
                ),
            }
        )
        frame.attrs.update(attrs)
        if formatted:
            frame = pm.format_dataframe(frame, case_id=case_key)
        for target, fn in (
            ("log", pm.convert_to_event_log),
            ("stream", pm.convert_to_event_stream),
        ):
            row = {
                "name": name + "_" + target,
                "source": "dataframe",
                "target": target,
                "case_id_key": case_key,
                "input": FORMAT["describe_frame"](frame),
            }
            try:
                row["result"] = snapshot(fn(frame, case_id_key=case_key))
            except Exception as error:
                row["error"] = {"type": type(error).__name__, "message": str(error)}
            row["input_after"] = FORMAT["describe_frame"](frame)
            rows.append(row)
    from pm4py.objects.log.obj import EventStream, Event

    for target, fn in (
        ("log", pm.convert_to_event_log),
        ("stream", pm.convert_to_event_stream),
    ):
        stream = EventStream(
            [Event({"case:concept:name": "C", "concept:name": "A"})],
            attributes={"origin": "custom", "source": "stream"},
            properties={
                "case_id_key": "ordinary",
                "pm4py:param:case_id_key": "preserved",
                "nested": {"enabled": True},
            },
        )
        row = {
            "name": "existing_stream_" + target,
            "source": "stream",
            "target": target,
            "case_id_key": "case:concept:name",
            "input": snapshot(stream),
        }
        try:
            row["result"] = snapshot(fn(stream))
        except Exception as error:
            row["error"] = {"type": type(error).__name__, "message": str(error)}
        row["input_after"] = snapshot(stream)
        rows.append(row)
    return {
        "capture_id": "dataframe_conversion_metadata",
        "reference_version": pm.__version__,
        "pandas_version": pd.__version__,
        "reference_provenance": proof,
        "case_count": len(rows),
        "rows": rows,
        "limits": [
            "Metadata and container length observations only; this capture does not establish all event-cell, dtype, identity, or conversion-error behavior.",
            "Native table-level log attributes have no dataframe counterpart and remain a native extension.",
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
