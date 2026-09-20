"""Capture public typed-date formatter precision through public calls only."""

import argparse
import gzip
import json
from pathlib import Path
import runpy

META = runpy.run_path(
    str(Path(__file__).with_name("capture-dataframe-conversion-metadata-reference.py"))
)
pd, pm = META["pd"], META["pm"]
DESCRIBE = runpy.run_path(
    str(
        Path(__file__).with_name(
            "capture-token-replay-diagnostics-dataframe-reference.py"
        )
    )
)["describe_frame"]


def capture(checkout):
    if pm.__version__ != "2.7.22" or pd.__version__ != "3.0.5":
        raise RuntimeError("Wrong reference runtime")
    proof = META["provenance"](pm.format_dataframe, checkout)
    rows = []
    for zone in (None, "UTC", "Europe/Berlin"):
        for spec in (
            "object_us",
            "object_ns",
            "inferred_us",
            "inferred_ns",
            "mixed",
            "start_ns",
            "explicit_ns",
        ):
            unit = "us" if spec.endswith("_us") else "ns"
            value = pd.Timestamp("2024-01-01", tz=zone).as_unit(unit)
            values = [value]
            if spec == "mixed":
                values = [
                    value.as_unit("us"),
                    (value + pd.Timedelta(days=1)).as_unit("ns"),
                ]
            frame = pd.DataFrame(
                {
                    "case:concept:name": ["C"] * len(values),
                    "concept:name": ["A"] * len(values),
                    "time:timestamp": pd.Series(
                        values, dtype=None if spec.startswith("inferred") else object
                    ),
                }
            )
            options = {}
            if spec == "start_ns":
                frame["start"] = pd.Series(values, dtype=object)
                frame["time:timestamp"] = pd.Series([value.as_unit("us")])
                options["start_timestamp_key"] = "start"
            if spec == "explicit_ns":
                options["timest_format"] = "%d/%m/%Y"
            row = {
                "name": (zone or "naive").replace("/", "_") + "_" + spec,
                "options": options,
                "input": DESCRIBE(frame),
            }
            try:
                row["result"] = DESCRIBE(pm.format_dataframe(frame, **options))
            except Exception as error:
                row["error"] = {"type": type(error).__name__, "message": str(error)}
            row["input_after"] = DESCRIBE(frame)
            rows.append(row)
    return {
        "reference_version": pm.__version__,
        "pandas_version": pd.__version__,
        "reference_provenance": proof,
        "rows": rows,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--checkout", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    raw = (json.dumps(capture(args.checkout), indent=2) + "\n").encode("utf-8")
    args.output.write_bytes(gzip.compress(raw, mtime=0))
