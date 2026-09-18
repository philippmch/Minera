"""Generate pinned direct-dataframe replay grouping checks."""

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import runpy

FMT = runpy.run_path(
    str(Path(__file__).with_name("generate-format-dataframe-tests.py"))
)
Q = FMT["QUOTE"]
NAMES = (
    "upper",
    "upper_reverse",
    "wide",
    "wide_float_first",
    "wide_only_int",
    "wide_float_unrelated",
    "negative",
    "uint_only",
    "uint_float",
    "bool_suppresses",
    "string_suppresses",
    "upper_repeat",
    "signed_unsigned",
    "bool_first",
    "float_before_bool",
    "int_before_float",
    "nan_keys",
    "none_keys",
    "infinity",
    "float_first_wide_twice",
    "nan_only",
    "prior_equal",
    "prior_wide_equal",
    "prior_wide_distinct",
    "prior_wide_mixed",
    "prior_upper",
    "prior_small_fraction",
    "prior_string_mixed",
)
EXCLUDED = ("nan_keys", "none_keys", "nan_only")
HASH = "17c44eaf3376645e3ed71ab6a521b5e0c4fb86840e69ad144f3419eb7434dbb1"
SIGNATURE = "(log: Union[pm4py.objects.log.obj.EventLog, pandas.DataFrame], petri_net: pm4py.objects.petri_net.obj.PetriNet, initial_marking: pm4py.objects.petri_net.obj.Marking, final_marking: pm4py.objects.petri_net.obj.Marking, activity_key: str = 'concept:name', timestamp_key: str = 'time:timestamp', case_id_key: str = 'case:concept:name', return_diagnostics_dataframe: bool = False, opt_parameters: Optional[Dict[Any, Any]] = None) -> List[Dict[str, Any]]"
NET = {
    "kind": "flower",
    "place": "p",
    "labels": list("ABCDEFGH"),
    "initial": {"p": 1},
    "final": {"p": 1},
}


def generate(raw):
    report = json.loads(raw)
    if (
        report["capture_id"] != "token_replay_dataframe_grouping"
        or report["reference_version"] != "2.7.22"
        or report["pandas_version"] != "3.0.5"
    ):
        raise ValueError("Wrong capture/runtime")
    if report["reference_provenance"] != {
        "commit": FMT["PIN"],
        "function": "conformance_diagnostics_token_based_replay",
        "module": "pm4py.conformance",
        "signature": SIGNATURE,
        "method": "opaque module bytes, CRLF normalized to LF, no source decoding",
        "installed_sha256": HASH,
        "pinned_sha256": HASH,
    }:
        raise ValueError("Unpinned callable")
    rows = report["rows"]
    if report["case_count"] != len(NAMES) or tuple(r["name"] for r in rows) != NAMES:
        raise ValueError("Incomplete manifest")
    if (
        report["net"] != NET
        or report["parameters"] != {"show_progress_bar": False}
        or tuple(report["exclusions"]) != EXCLUDED
    ):
        raise ValueError("Unmapped net/options/exclusions")
    out = (
        "// Public replay grouping SHA256: "
        + hashlib.sha256(raw).hexdigest()
        + '\n#include "minera/conformance.hpp"\n'
        + FMT["DTYPE"]["PREAMBLE"]
        + "\n}\n"
        + FMT["PREAMBLE"].replace(
            "namespace format_dataframe_reference",
            "namespace replay_dataframe_grouping_reference",
        )
    )
    included = []
    for i, row in enumerate(rows):
        for field in ("input", "input_after"):
            FMT["validate_frame"](row[field])
        if row["input"] != row["input_after"]:
            raise ValueError("Unexpected mutation")
        if row["name"] in EXCLUDED:
            if "result" in row or row["error"] != {
                "type": "Exception",
                "message": "the case ID column should not contain any empty value.",
            }:
                raise ValueError("Unexpected excluded outcome")
            continue
        if "error" in row:
            raise ValueError("Unexpected public error")
        included.append(i)
        out += (
            "inline void check_replay_grouping_"
            + str(i)
            + "() {\n"
            + FMT["frame_code"]("input", row["input"])
            + FMT["frame_code"]("expected_input", row["input_after"])
        )
        out += (
            "  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"
            + ", ".join(map(Q, NET["labels"]))
            + "});\n"
        )
        out += "  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});\n  const auto lazy = LazyEventTable::from_table(input);\n  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {\n    const auto actual = replay_token_traces(view, net);\n"
        out += (
            "    if (actual.size() != "
            + str(len(row["result"]))
            + ") throw std::runtime_error("
            + Q(row["name"] + ": diagnostic count differs")
            + ");\n"
        )
        for j, trace in enumerate(row["result"]):
            if type(trace["fit"]) is not bool or any(
                type(trace[k]) is not int or trace[k] < 0
                for k in ("consumed", "produced", "missing", "remaining")
            ):
                raise ValueError("Invalid diagnostics")
            indices = [NET["labels"].index(v) for v in trace["activities"]]
            out += (
                "    { const auto& trace = actual["
                + str(j)
                + "]; const std::vector<std::size_t> expected = {"
                + ", ".join(map(str, indices))
                + "};\n"
            )
            terms = [
                "trace.activated_transitions != expected",
                "trace.trace_is_fit != " + str(trace["fit"]).lower(),
                "trace.trace_fitness != " + repr(trace["fitness"]),
            ] + [
                "trace." + k + " != " + str(trace[k])
                for k in ("consumed", "produced", "missing", "remaining")
            ]
            out += (
                "      if ("
                + " || ".join(terms)
                + ") throw std::runtime_error("
                + Q(row["name"] + ": replay path/counters differ")
                + "); }\n"
            )
        out += (
            "  }\n  check_format_table("
            + Q(row["name"] + ": input after")
            + ", input, expected_input);\n}\n"
        )
    out += "}\ninline void check_public_replay_dataframe_grouping() {\n"
    for i in included:
        out += (
            "  replay_dataframe_grouping_reference::check_replay_grouping_"
            + str(i)
            + "();\n"
        )
    return out + "}\n"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    raw = args.reference.read_bytes()
    if args.reference.suffix == ".gz":
        raw = gzip.decompress(raw)
    args.output.write_text(generate(raw), encoding="utf-8", newline="\n")
