"""Generate native comparisons for pinned public container grouping calls."""

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import runpy

BASE = runpy.run_path(
    str(Path(__file__).with_name("generate-conversion-case-grouping-tests.py"))
)
Q = BASE["Q"]
DIGEST = "5884713b425bf4bb32da9b13f0354664174e1d3616665bec7e5e9c1213ac4870"


def cell(value, key):
    if "children" not in value:
        return BASE["scalar"](value, key)
    kind = {
        "tuple": "kTuple",
        "frozenset": "kFrozenSet",
        "list": "kList",
        "dict": "kContainer",
        "set": "kSet",
    }[value["kind"]]
    return (
        "container_cell("
        + Q(key)
        + ", AttributeContainerKind::"
        + kind
        + ", {"
        + ", ".join(cell(child, "") for child in value["children"])
        + "})"
    )


def generate(raw):
    if hashlib.sha256(raw).hexdigest() != DIGEST:
        raise ValueError("Container grouping public artifact hash differs")
    report = json.loads(raw)
    out = (
        """// Pinned public container grouping SHA256: """
        + DIGEST
        + """
#pragma once
#include "conversion_case_grouping_reference_cases.hpp"
namespace container_case_grouping_reference {
using namespace conversion_case_grouping_reference;
inline EventAttribute container_cell(std::string key, AttributeContainerKind kind, std::vector<EventAttribute> children) {
  EventAttribute value{std::move(key), ""};
  value.container = kind;
  value.children = std::move(children);
  return value;
}
"""
    )
    for i, row in enumerate(report["rows"]):
        if row["input"] != row["input_after"]:
            raise ValueError("Unexpected public input mutation")
        out += f"inline void check_{i}() {{\n  EventStream input;\n"
        for j, value in enumerate(row["input"]):
            out += (
                "  input.events.push_back({{"
                + cell(value, "case:concept:name")
                + ', EventAttribute{"concept:name", '
                + Q("A" + str(j))
                + "}}});\n"
            )
        out += "  const auto before = input;\n"
        if "error" in row:
            out += (
                "  bool rejected = false;\n  try { (void)event_stream_to_event_log(input); } catch (const std::invalid_argument& error) { rejected = std::string(error.what()) == "
                + Q(row["error"]["message"])
                + "; }\n"
            )
            out += (
                "  if (!rejected) throw std::runtime_error("
                + Q(row["name"] + ": expected rejection differs")
                + ");\n"
            )
        else:
            out += "  const auto actual = event_stream_to_event_log(input);\n"
            out += (
                "  if (actual.traces.size() != "
                + str(len(row["result"]))
                + ") throw std::runtime_error("
                + Q(row["name"] + ": trace count differs")
                + ");\n"
            )
            for j, trace in enumerate(row["result"]):
                out += "  { const auto& trace = actual.traces[" + str(j) + "];\n"
                out += (
                    "    const auto expected_case = "
                    + cell(trace["case"], "concept:name")
                    + ";\n"
                )
                out += (
                    "    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("
                    + Q(row["name"] + ": typed representative differs")
                    + ");\n"
                )
                out += "    std::vector<std::string> observed; for (const auto& event : trace.events) observed.push_back(event.activity);\n"
                out += (
                    "    if (observed != std::vector<std::string>{"
                    + ", ".join(map(Q, trace["activities"]))
                    + "}) throw std::runtime_error("
                    + Q(row["name"] + ": membership/order differs")
                    + "); }\n"
                )
        out += '  for (std::size_t i = 0; i < input.events.size(); ++i) if (input.events[i].fields != before.events[i].fields) throw std::runtime_error("input changed");\n}\n'
    out += "}\ninline void check_public_container_case_grouping() {\n"
    for i in range(len(report["rows"])):
        out += f"  container_case_grouping_reference::check_{i}();\n"
    return out + "}\n"


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("reference", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_text(
        generate(gzip.decompress(args.reference.read_bytes())), encoding="utf-8"
    )
