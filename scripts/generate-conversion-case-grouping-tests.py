"""Generate exact typed grouping checks from pinned public event-stream calls."""

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import runpy

META = runpy.run_path(
    str(Path(__file__).with_name("generate-dataframe-conversion-metadata-tests.py"))
)
FMT, Q = META["FMT"], META["Q"]
NAMES = (
    "integer_float_bool",
    "bool_first",
    "signed_zero",
    "strings_separate",
    "wide_exact",
    "unsigned",
    "signed_min",
    "fractional",
    "null",
    "different_nan",
    "shared_nan",
    "infinities",
    "uint_float_exact",
    "negative_wide",
    "negative_fraction",
    "positive_boundary",
    "float_first",
)


def scalar(value, key):
    if value == {"kind": "float", "value": "NaN"}:
        return "dtype_number(" + Q(key) + ", std::numeric_limits<double>::quiet_NaN())"
    return FMT["scalar"](value, key)


def generate(raw):
    report = json.loads(raw)
    if (
        report["capture_id"] != "conversion_case_grouping"
        or report["reference_version"] != "2.7.22"
    ):
        raise ValueError("Wrong capture/runtime")
    proof = report["reference_provenance"]
    module, digest, signature = META["MODULES"]["convert_to_event_log"]
    if proof != {
        "commit": FMT["PIN"],
        "function": "convert_to_event_log",
        "module": module,
        "signature": signature,
        "method": "opaque module bytes, CRLF normalized to LF, no source decoding",
        "installed_sha256": digest,
        "pinned_sha256": digest,
    }:
        raise ValueError("Unpinned public callable")
    rows = report["rows"]
    if report["case_count"] != len(NAMES) or tuple(r["name"] for r in rows) != NAMES:
        raise ValueError("Incomplete grouping manifest")
    out = (
        "// Public grouping SHA256: "
        + hashlib.sha256(raw).hexdigest()
        + """\n#pragma once
#include "dataframe_conversion_metadata_reference_cases.hpp"
namespace conversion_case_grouping_reference {
using namespace dataframe_conversion_metadata_reference;
inline bool grouping_scalar_equal(const EventAttribute& actual, const EventAttribute& expected) {
  const auto* number = std::get_if<double>(&expected.typed_value);
  if (number && std::isnan(*number)) {
    const auto* observed = std::get_if<double>(&actual.typed_value);
    return actual.kind == expected.kind && !actual.is_null && observed && std::isnan(*observed);
  }
  return dtype_attribute_equal(actual, expected);
}
"""
    )
    for i, row in enumerate(rows):
        inputs = row["input"]
        if "error" in row or row["input_after"] != [
            {"case": v["case"], "activity": v["activity"]} for v in inputs
        ]:
            raise ValueError("Unexpected mutation/error")
        out += "inline void check_grouping_" + str(i) + "() {\n  EventStream input;\n"
        for j, value in enumerate(inputs):
            identity = value["identity"]
            if (
                type(identity) is not int
                or not 0 <= identity <= j
                or inputs[identity]["case"] != value["case"]
            ):
                raise ValueError("Invalid scalar identity")
            expression = (
                scalar(value["case"], "case:concept:name")
                if identity == j
                else "input.events[" + str(identity) + "].fields[0]"
            )
            out += (
                "  input.events.push_back({{"
                + expression
                + ', EventAttribute{"concept:name", '
                + Q(value["activity"])
                + "}}});\n"
            )
        out += "  const auto actual = event_stream_to_event_log(input);\n"
        out += (
            "  if (actual.traces.size() != "
            + str(len(row["result"]))
            + ") throw std::runtime_error("
            + Q(row["name"] + ": trace count differs")
            + ");\n"
        )
        activities = []
        for j, trace in enumerate(row["result"]):
            if trace["event_keys"] != [["concept:name"] for _ in trace["activities"]]:
                raise ValueError("Unexpected public event fields")
            activities.extend(trace["activities"])
            out += (
                "  { const auto& trace = actual.traces["
                + str(j)
                + "]; const auto expected_case = "
                + scalar(trace["case"], "concept:name")
                + ";\n"
            )
            out += (
                "    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("
                + Q(row["name"] + ": typed representative differs")
                + ");\n"
            )
            out += (
                "    const std::vector<std::string> expected = {"
                + ", ".join(map(Q, trace["activities"]))
                + "};\n"
            )
            out += '    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }\n'
            out += (
                "    if (observed != expected) throw std::runtime_error("
                + Q(row["name"] + ": event grouping/order differs")
                + "); }\n"
            )
        if sorted(activities) != sorted(v["activity"] for v in inputs):
            raise ValueError("Public event partition incomplete")
        for j, value in enumerate(inputs):
            out += (
                "  if (!grouping_scalar_equal(input.events["
                + str(j)
                + "].fields[0], "
                + scalar(value["case"], "case:concept:name")
                + ')) throw std::runtime_error("input scalar changed");\n'
            )
        out += "}\n"
    out += "}\ninline void check_public_conversion_case_grouping() {\n"
    for i in range(len(rows)):
        out += (
            "  conversion_case_grouping_reference::check_grouping_" + str(i) + "();\n"
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
