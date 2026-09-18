"""Capture direct dataframe replay grouping with a publicly constructed flower net."""

import argparse
import json
from pathlib import Path
import runpy

META = runpy.run_path(
    str(Path(__file__).with_name("capture-dataframe-conversion-metadata-reference.py"))
)
pm, pd = META["pm"], META["pd"]


def capture(checkout):
    if pm.__version__ != "2.7.22" or pd.__version__ != "3.0.5":
        raise RuntimeError("Wrong reference runtime")
    proof = META["provenance"](pm.conformance_diagnostics_token_based_replay, checkout)
    from pm4py.objects.petri_net.obj import PetriNet, Marking
    from pm4py.objects.petri_net.utils import petri_utils

    labels = list("ABCDEFGH")
    net = PetriNet("flower")
    place = PetriNet.Place("p")
    net.places.add(place)
    for label in labels:
        transition = PetriNet.Transition(label, label)
        net.transitions.add(transition)
        petri_utils.add_arc_from_to(place, transition, net)
        petri_utils.add_arc_from_to(transition, place, net)
    initial = Marking({place: 1})
    final = Marking({place: 1})
    cases = (
        ("upper", [float(2**63), 2**63 - 1]),
        ("upper_reverse", [2**63 - 1, float(2**63)]),
        ("wide", [2**53 + 1, 2**53, float(2**53)]),
        ("wide_float_first", [float(2**53), 2**53 + 1, 2**53]),
        ("wide_only_int", [2**53 + 1, 2**53]),
        ("wide_float_unrelated", [2**53 + 1, 2**53, 1.5]),
        ("negative", [-(2**53) - 1, -(2**53), -1.5]),
        ("uint_only", [2**64 - 1, 2**64 - 2]),
        ("uint_float", [2**64 - 1, 2**64 - 2, 0.5]),
        ("bool_suppresses", [2**53 + 1, 2**53, 0.5, True]),
        ("string_suppresses", [2**53 + 1, 2**53, 0.5, "x"]),
        ("upper_repeat", [float(2**63), 2**63 - 1, float(2**63), 2**63 - 1]),
        ("signed_unsigned", [-1, 2**64 - 1, 2**64 - 2]),
        ("bool_first", [True, 1.0, 2**53 + 1, 2**53, 0.5]),
        ("float_before_bool", [1.0, True, 2**53 + 1, 2**53, 0.5]),
        ("int_before_float", [1, 1.0, 2**53 + 1, 2**53]),
        ("nan_keys", [float("nan"), 1, 1.0, None]),
        ("none_keys", [None, None, 1]),
        ("infinity", [float("inf"), float("-inf"), 2**64 - 1, 2**64 - 2]),
        ("float_first_wide_twice", [float(2**53), 2**53 + 1, float(2**53), 2**53 + 1]),
        ("nan_only", [float("nan"), float("nan")]),
        ("prior_equal", [1, 1.0]),
        ("prior_wide_equal", [2**53, float(2**53)]),
        ("prior_wide_distinct", [2**53 + 1, 2**53]),
        ("prior_wide_mixed", [2**53 + 1, 2**53, float(2**53)]),
        ("prior_upper", [float(2**63), 2**63 - 1]),
        ("prior_small_fraction", [-1, -1.5]),
        ("prior_string_mixed", [1, 1.0, "1"]),
    )
    rows = []
    for name, values in cases:
        frame = pd.DataFrame(
            {
                "case:concept:name": pd.Series(values, dtype="object"),
                "concept:name": labels[: len(values)],
                "time:timestamp": pd.to_datetime(
                    ["2024-01-01"] * len(values), utc=True
                ),
            }
        )
        row = {"name": name, "input": META["FORMAT"]["describe_frame"](frame)}
        try:
            result = pm.conformance_diagnostics_token_based_replay(
                frame, net, initial, final, opt_parameters={"show_progress_bar": False}
            )
            row["result"] = [
                {
                    "activities": [t.label for t in trace["activated_transitions"]],
                    "fit": trace["trace_is_fit"],
                    "fitness": trace["trace_fitness"],
                    "consumed": trace["consumed_tokens"],
                    "produced": trace["produced_tokens"],
                    "missing": trace["missing_tokens"],
                    "remaining": trace["remaining_tokens"],
                }
                for trace in result
            ]
        except Exception as error:
            row["error"] = {"type": type(error).__name__, "message": str(error)}
        row["input_after"] = META["FORMAT"]["describe_frame"](frame)
        rows.append(row)
    return {
        "capture_id": "token_replay_dataframe_grouping",
        "reference_version": pm.__version__,
        "pandas_version": pd.__version__,
        "reference_provenance": proof,
        "net": {
            "kind": "flower",
            "place": "p",
            "labels": labels,
            "initial": {"p": 1},
            "final": {"p": 1},
        },
        "parameters": {"show_progress_bar": False},
        "case_count": len(rows),
        "rows": rows,
        "exclusions": {
            name: "High-level public empty-case validation is not implemented by the native lower-level normalization API."
            for name in ("nan_keys", "none_keys", "nan_only")
        },
        "limits": [
            "Finite inferred numeric grouping and replay diagnostic matrix; not full pandas dtype/index, validation, or replay parity.",
            "Case labels are not exposed by the public list diagnostics and are not inferred from transition sequences.",
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
