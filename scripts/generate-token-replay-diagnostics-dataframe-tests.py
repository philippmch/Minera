"""Generate native checks for pinned public dataframe replay diagnostics."""

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import runpy

FMT = runpy.run_path(
    str(Path(__file__).with_name("generate-format-dataframe-tests.py"))
)
DIGEST = "0b09ae6e916aeef34fce5cc616b1862d6f504ab5f821138ca472dffe75fb8840"
NAMES = (
    "empty",
    "typed_empty",
    "strings_source_order",
    "booleans",
    "empty_strings",
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
    "linear_fit",
    "linear_missing",
    "linear_remaining",
    "linear_reverse",
    "linear_unknown",
    "linear_unknown_between",
    "linear_source_order",
    "custom_selectors",
)


def generate(raw):
    if hashlib.sha256(raw).hexdigest() != DIGEST:
        raise ValueError("Unreviewed diagnostics dataframe artifact")
    report = json.loads(raw)
    if (
        report["case_count"] != len(NAMES)
        or tuple(r["name"] for r in report["rows"]) != NAMES
    ):
        raise ValueError("Incomplete manifest")
    out = (
        "// Public artifact SHA256: "
        + DIGEST
        + '\n#include "minera/conformance.hpp"\n'
        + FMT["DTYPE"]["PREAMBLE"]
        + "\n}\n"
        + FMT["PREAMBLE"].replace(
            "namespace format_dataframe_reference",
            "namespace replay_dataframe_reference",
        )
    )
    Q = json.dumps
    for i, row in enumerate(report["rows"]):
        if row["input"] != row["input_after"]:
            raise ValueError("Unexpected input mutation")
        out += "inline void check_replay_dataframe_" + str(i) + "() {\n"
        out += FMT["frame_code"]("input", row["input"])
        out += FMT["frame_code"]("expected_input", row["input_after"])
        if row.get("net_kind") == "linear":
            out += '  PetriNet net; const auto p=net.add_place(), q=net.add_place(), r=net.add_place(); net.set_source(p); net.set_sink(r); net.set_activities({"A","B"}); net.add_transition(0,{p},{q}); net.add_transition(1,{q},{r});\n'
        else:
            out += '  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});\n'
        out += "  TokenReplayOptions options;\n"
        for key, value in row.get("parameters", {}).items():
            field = {
                "case_id_key": "case_id_attribute",
                "activity_key": "activity_attribute",
                "timestamp_key": "timestamp_attribute",
            }[key]
            out += "  options." + field + " = " + Q(value) + ";\n"
        if "error" in row:
            out += "  std::string error; try { (void)replay_dataframe_diagnostics(input,net,options); } catch (const std::exception& caught) { error=caught.what(); }\n"
            out += (
                "  if (error != "
                + Q(row["error"]["message"])
                + ") throw std::runtime_error("
                + Q(row["name"] + ": rejection differs")
                + ");\n"
            )
        else:
            out += FMT["frame_code"]("expected", row["result"])
            out += (
                "  check_format_table("
                + Q(row["name"])
                + ", replay_dataframe_diagnostics(input,net,options), expected);\n"
            )
        out += (
            "  check_format_table("
            + Q(row["name"] + ": input after")
            + ", input, expected_input);\n}\n"
        )
    out += "}\ninline void check_public_replay_dataframe_diagnostics() {\n"
    for i in range(len(NAMES)):
        out += (
            "  replay_dataframe_reference::check_replay_dataframe_" + str(i) + "();\n"
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
