"""Capture pinned public case-merging order for exact timestamp boundaries."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import warnings

warnings.filterwarnings("ignore")

import pandas as pd
import pm4py
from pm4py.algo.merging.case_relations import algorithm


REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"


def reference_provenance(checkout):
    """Compare opaque module bytes with the pin; never decode source content."""
    modules = []
    for module in (algorithm, algorithm.Variants.PANDAS.value):
        relative = module.__name__.replace(".", "/") + ".py"
        installed = Path(module.__file__).read_bytes().replace(b"\r\n", b"\n")
        pinned = subprocess.check_output(
            ["git", "-C", str(checkout), "show", REFERENCE_COMMIT + ":" + relative]
        ).replace(b"\r\n", b"\n")
        if installed != pinned:
            raise RuntimeError("Reference module does not match pin: " + relative)
        modules.append({"module": module.__name__,
                        "installed_sha256": hashlib.sha256(installed).hexdigest(),
                        "pinned_sha256": hashlib.sha256(pinned).hexdigest()})
    return {"commit": REFERENCE_COMMIT,
            "method": "opaque bytes, CRLF normalized to LF, no source decoding",
            "modules": modules}


def capture(checkout):
    if pm4py.__version__ != "2.7.22":
        raise RuntimeError("This oracle requires reference version 2.7.22")
    if pd.__version__ != "3.0.5":
        raise RuntimeError("This oracle requires pandas version 3.0.5")
    provenance = reference_provenance(checkout)
    rows = []
    for prefix in ("2024-01-01T00:00:00", "1969-12-31T23:59:59"):
        for timestamp_key in ("time:timestamp", "started"):
            base = pd.DataFrame([
                {"case:concept:name": "L1", "concept:name": name, timestamp_key: prefix + suffix}
                for name, suffix in (("base1", ".000000001Z"),
                                     ("base0", ".000000000Z"),
                                     ("base-last", ".999999999Z"))
            ])
            incoming = pd.DataFrame([
                {"case:concept:name": "R1", "concept:name": name, timestamp_key: prefix + suffix}
                for name, suffix in (("incoming2", ".000000002Z"),
                                     ("incoming1", ".000000001+00:00"))
            ])
            for frame in (base, incoming):
                frame[timestamp_key] = pd.to_datetime(frame[timestamp_key], format="mixed", utc=True)
            relations = pd.DataFrame([
                {"case:concept:name_LEFT": "L1", "case:concept:name_RIGHT": "R1"}
            ] * 2)
            output = algorithm.apply(base, incoming, relations, parameters={
                "pm4py:param:timestamp_key": timestamp_key,
            })
            rows.append({"prefix": prefix, "timestamp_key": timestamp_key,
                         "activities": output["concept:name"].tolist(),
                         "timestamps": [value.isoformat() for value in output[timestamp_key]]})
    for timestamp_key in ("time:timestamp", "started"):
        for mode in ("mixed", "all-null", "base-column-absent", "incoming-column-absent", "both-columns-absent", "invalid-text"):
            base = pd.DataFrame({"case:concept:name": ["L1"] * 3,
                                 "concept:name": ["base-null", "base2", "base-missing"]})
            incoming = pd.DataFrame({"case:concept:name": ["R1"] * 2,
                                     "concept:name": ["incoming-null", "incoming1"]})
            for frame, values in ((base, [None, "2024-01-01T00:00:00.000000002Z", pd.NaT]),
                                  (incoming, [pd.NaT, "2024-01-01T00:00:00.000000001Z"])):
                frame[timestamp_key] = pd.to_datetime(
                    [None] * len(frame) if mode == "all-null" else values, format="mixed", utc=True)
            if mode in ("base-column-absent", "both-columns-absent"):
                del base[timestamp_key]
            if mode in ("incoming-column-absent", "both-columns-absent"):
                del incoming[timestamp_key]
            if mode == "invalid-text":
                base[timestamp_key] = ["invalid", "", None]
            before = [frame.copy(deep=True) for frame in (base, incoming)]
            row = {"timestamp_key": timestamp_key, "mode": mode}
            try:
                output = algorithm.apply(base, incoming, relations, parameters={
                    "pm4py:param:timestamp_key": timestamp_key})
                row["activities"] = output["concept:name"].tolist()
                row["timestamps"] = [None if pd.isna(value) else str(value)
                                     for value in output[timestamp_key]]
            except Exception as error:
                row["error"] = type(error).__name__
                row["message"] = str(error)
            row["inputs_unchanged"] = all(frame.equals(original)
                                          for frame, original in zip((base, incoming), before))
            rows.append(row)
    return {"reference_version": pm4py.__version__, "pandas_version": pd.__version__,
            "reference_provenance": provenance, "rows": rows}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--reference-checkout", type=Path, required=True)
    args = parser.parse_args()
    result = capture(args.reference_checkout)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result))
