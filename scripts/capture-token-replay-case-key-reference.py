"""Capture the open dataframe-versus-converted-log replay grouping boundary."""

import argparse
import runpy, json
from pathlib import Path

m = runpy.run_path(
    str(Path(__file__).with_name("capture-dataframe-conversion-metadata-reference.py"))
)
pm, pd = m["pm"], m["pd"]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("output", type=Path)
parser.add_argument("--reference-checkout", type=Path, required=True)
args = parser.parse_args()
pin = args.reference_checkout
if pm.__version__ != "2.7.22" or pd.__version__ != "3.0.5":
    raise RuntimeError("Wrong reference runtime")
from pm4py.objects.petri_net.obj import PetriNet, Marking
from pm4py.objects.petri_net.utils import petri_utils

proof = [
    m["provenance"](fn, pin)
    for fn in (pm.convert_to_event_log, pm.conformance_diagnostics_token_based_replay)
]
net = PetriNet("one A")
p0 = PetriNet.Place("start")
p1 = PetriNet.Place("end")
t = PetriNet.Transition("a", "A")
net.places.update([p0, p1])
net.transitions.add(t)
petri_utils.add_arc_from_to(p0, t, net)
petri_utils.add_arc_from_to(t, p1, net)
im = Marking({p0: 1})
fm = Marking({p1: 1})
rows = []
for name, values in [
    ("equal", [1, 1.0]),
    ("wide_equal", [9007199254740992, 9007199254740992.0]),
    ("wide_distinct", [9007199254740993, 9007199254740992]),
    ("wide_mixed", [9007199254740993, 9007199254740992, 9007199254740992.0]),
    ("upper", [float(9223372036854775808), 9223372036854775807]),
    ("small_fraction", [-1, -1.5]),
    ("string_mixed", [1, 1.0, "1"]),
]:
    frame = pd.DataFrame(
        {
            "case:concept:name": pd.Series(values, dtype="object"),
            "concept:name": ["A"] * len(values),
            "time:timestamp": pd.to_datetime(["2024-01-01"] * len(values), utc=True),
        }
    )
    row = {"name": name, "input": m["FORMAT"]["describe_frame"](frame)}
    for kind in ("dataframe", "converted_log"):
        try:
            obj = frame if kind == "dataframe" else pm.convert_to_event_log(frame)
            out = pm.conformance_diagnostics_token_based_replay(
                obj, net, im, fm, opt_parameters={"show_progress_bar": False}
            )
            row[kind] = [
                {
                    "fit": x["trace_is_fit"],
                    "consumed": x["consumed_tokens"],
                    "missing": x["missing_tokens"],
                }
                for x in out
            ]
        except Exception as error:
            row[kind] = {"error": {"type": type(error).__name__, "message": str(error)}}
    row["input_after"] = m["FORMAT"]["describe_frame"](frame)
    rows.append(row)
report = {
    "capture_id": "token_replay_case_key_boundary",
    "reference_version": pm.__version__,
    "pandas_version": pd.__version__,
    "reference_provenance": proof,
    "net": {
        "places": ["start", "end"],
        "transition": {"name": "a", "label": "A"},
        "arcs": [["start", "a"], ["a", "end"]],
        "initial": {"start": 1},
        "final": {"end": 1},
    },
    "parameters": {"show_progress_bar": False},
    "case_count": len(rows),
    "rows": rows,
    "status": "Open native parity gap: dataframe replay can produce different case counts from conversion followed by replay for wide mixed numeric keys.",
}
args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
print(
    json.dumps(
        {
            "cases": len(rows),
            "errors": [
                r["name"]
                for r in rows
                if any("error" in r[k] for k in ("dataframe", "converted_log"))
            ],
        }
    )
)
