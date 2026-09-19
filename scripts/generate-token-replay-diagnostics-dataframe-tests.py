"""Generate native checks for pinned public dataframe replay diagnostics."""

import argparse
import copy
import gzip
import hashlib
import json
from pathlib import Path
import runpy

FMT = runpy.run_path(
    str(Path(__file__).with_name("generate-format-dataframe-tests.py"))
)
DIGEST = "7d00363d7fe3f03c09eb0075e87f33b0b928d25aa617f5c755aa8cfe98fd971f"
NAMES = (
    "date_berlin_dst",
    "date_named_same_offset",
    "date_named_equal_instant",
    "date_named_equal_utc_first",
    "date_named_mixed_precision",
    "date_named_fold",
    "date_naive",
    "date_utc",
    "date_same_instant_offsets",
    "date_mixed_offsets",
    "date_mixed_naive_aware",
    "date_mixed_string",
    "date_nanoseconds",
    "date_mixed_precision",
    "date_pre_epoch",
    "date_offset_nanoseconds_equal",
    "date_equal_mixed_precision",
    "date_ns_whole",
    "date_fixed_offset",
    "date_nat",
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


def frame_code(name, frame):
    transport = copy.deepcopy(frame)
    for row in transport["records"]:
        for tag in row["cell_tags"].values():
            if "native_resolution" in tag:
                if (
                    tag["kind"] != "date"
                    or tag["missing"]
                    or tag["native_resolution"] not in ("us", "ns")
                ):
                    raise ValueError("Invalid date resolution metadata")
                tag.pop("native_resolution")
            if "native_timezone" in tag:
                if (
                    tag["kind"] != "date"
                    or tag["missing"]
                    or not isinstance(tag["native_timezone"], str)
                    or not tag["native_timezone"]
                ):
                    raise ValueError("Invalid date timezone metadata")
                tag.pop("native_timezone")
    out = FMT["frame_code"](name, transport)
    for row in frame["records"]:
        for column, label in enumerate(frame["columns"]):
            tag = row["cell_tags"][str(label)]
            if tag.get("native_resolution") == "ns":
                out += (
                    "  std::get<NativeDate>("
                    + name
                    + ".rows["
                    + str(row["position"])
                    + "]["
                    + str(column)
                    + "]->typed_value).resolution = NativeDateResolution::kNanosecond;\n"
                )
            if "native_timezone" in tag:
                out += (
                    "  std::get<NativeDate>("
                    + name
                    + ".rows["
                    + str(row["position"])
                    + "]["
                    + str(column)
                    + "]->typed_value).timezone = "
                    + json.dumps(tag["native_timezone"])
                    + ";\n"
                )
    return out


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
        + '\n#include "minera/conformance.hpp"\n#include "minera/log_conversion.hpp"\n'
        + FMT["DTYPE"]["PREAMBLE"]
        + "\n}\n"
        + FMT["PREAMBLE"].replace(
            "namespace format_dataframe_reference",
            "namespace replay_dataframe_reference",
        )
    )
    out += 'inline void check_replay_table(const std::string& name, const EventTable& actual, const EventTable& expected) {\n  check_format_table(name, actual, expected);\n  for (std::size_t i=0; i<expected.rows.size(); ++i) for (std::size_t j=0; j<expected.columns.size(); ++j) {\n    if (!expected.rows[i][j]) continue;\n    const auto* date=std::get_if<NativeDate>(&expected.rows[i][j]->typed_value);\n    if (!date) continue;\n    const auto* observed=actual.rows[i][j] ? std::get_if<NativeDate>(&actual.rows[i][j]->typed_value) : nullptr;\n    if (!observed || observed->resolution != date->resolution || observed->timezone != date->timezone) throw std::runtime_error(name + ": date metadata differs");\n  }\n}\n'
    Q = json.dumps
    for i, row in enumerate(report["rows"]):
        if row["input"] != row["input_after"]:
            raise ValueError("Unexpected input mutation")
        out += "inline void check_replay_dataframe_" + str(i) + "() {\n"
        out += frame_code("input", row["input"])
        out += frame_code("expected_input", row["input_after"])
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
            out += frame_code("expected", row["result"])
            out += (
                "  check_replay_table("
                + Q(row["name"])
                + ", replay_dataframe_diagnostics(input,net,options), expected);\n"
            )
        if "converted_activities" in row:
            out += "  const auto converted = event_table_to_event_log(input);\n"
            out += (
                "  if (converted.traces.size() != "
                + str(len(row["converted_activities"]))
                + ") throw std::runtime_error("
                + Q(row["name"] + ": conversion group count differs")
                + ");\n"
            )
            for j, activities in enumerate(row["converted_activities"]):
                out += (
                    "  { std::vector<std::string> activities; for (const auto& event : converted.traces["
                    + str(j)
                    + "].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"
                    + ",".join(map(Q, activities))
                    + "}) throw std::runtime_error("
                    + Q(row["name"] + ": converted membership differs")
                    + "); }\n"
                )
        out += (
            "  check_replay_table("
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
