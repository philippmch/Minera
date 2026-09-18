"""Generate exact index metadata checks from pinned public case-merge outputs."""

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import runpy

HELPERS = runpy.run_path(
    str(Path(__file__).with_name("generate-case-relations-reference-tests.py"))
)
EXPECTED_CASES = (
    "default_range",
    "integer_labels",
    "string_labels",
    "shared_name",
    "null_label",
    "datetime_labels",
    "multiindex",
    "empty_relations",
    "empty_base",
    "multiindex_empty_relations",
    "string_empty_relations",
    "datetime_empty_relations",
    "all_inputs_empty",
    "nullable_integer_index",
    "float_index",
    "default_empty_base",
    "default_all_empty",
    "default_range_sorted_no_relations",
    "default_range_empty_relations",
    "default_empty_base_reordered",
    "offset_range_empty_relations",
    "offset_range_sorted_empty_relations",
    "range_nonarithmetic_sort",
    "range_contiguous_concat",
    "range_discontinuous_concat_reordered",
    "range_extreme_step",
)
PINNED_MODULES = {
    "pm4py.algo.merging.case_relations.algorithm": "21c9af044eda66e346b9003cd9bd7420c2074f2ccd4a81fa67f6e7e39c47a73c",
    "pm4py.algo.merging.case_relations.variants.pandas": "6cb7003257c67205ecb34bafb1241a1857b749c45d93dec64a0d57450ef269fb",
}


def label(value):
    kind = value["kind"]
    payload = value["value"]
    if kind == "tuple":
        return (
            "EventTableIndexLabel{std::vector<EventTableIndexLabel>{"
            + ", ".join(label(part) for part in payload)
            + "}}"
        )
    if kind not in ("int", "float", "string", "date", "bool", "null"):
        raise ValueError("Unknown index scalar kind")
    if payload is not None:
        expected = {
            "int": int,
            "float": float,
            "string": str,
            "date": str,
            "bool": bool,
        }.get(kind)
        if expected is None or type(payload) is not expected:
            raise ValueError("Index scalar kind/value mismatch")
    if kind == "int" and payload == -(2**63):
        return 'EventTableIndexLabel{reference_number("", (std::numeric_limits<std::int64_t>::min)())}'
    return "EventTableIndexLabel{" + HELPERS["cell"]("", payload, kind) + "}"


def index(value, row_count):
    if len(value["labels"]) != row_count or not value["names"]:
        raise ValueError("Incomplete index labels/names")
    if value["class"] not in ("Index", "RangeIndex", "MultiIndex", "DatetimeIndex"):
        raise ValueError("Unsupported captured index class")
    if not isinstance(value["dtype"], str) or not value["dtype"]:
        raise ValueError("Missing index dtype")
    if any(name is not None and not isinstance(name, str) for name in value["names"]):
        raise ValueError("Unsupported nonstring index name")
    names = [
        "std::nullopt" if name is None else HELPERS["quote"](name)
        for name in value["names"]
    ]
    return (
        "EventTableIndex{"
        + HELPERS["quote"](value["class"])
        + ", "
        + HELPERS["quote"](value["dtype"])
        + ", {"
        + ", ".join(names)
        + "}, {"
        + ", ".join(label(part) for part in value["labels"])
        + "}}"
    )


def generate(source):
    report = json.loads(source)
    if report["reference_version"] != "2.7.22" or report["pandas_version"] != "3.0.5":
        raise ValueError("Wrong public runtime")
    if tuple(row["name"] for row in report["rows"]) != EXPECTED_CASES:
        raise ValueError("Incomplete or reordered index observations")
    provenance = report["reference_provenance"]
    if provenance["commit"] != "0715c31f3f1d85924107d6aa5329806db69df899":
        raise ValueError("Wrong reference commit")
    if (
        len(provenance["modules"]) != 2
        or any(
            module["installed_sha256"] != module["pinned_sha256"]
            or module["pinned_sha256"] != PINNED_MODULES.get(module["module"])
            for module in provenance["modules"]
        )
        or {module["module"] for module in provenance["modules"]} != set(PINNED_MODULES)
    ):
        raise ValueError("Reference module provenance differs")
    output = (
        "// Public index report SHA256: " + hashlib.sha256(source).hexdigest() + "\n"
    )
    output += """#pragma once
#include "case_relations_reference_cases.hpp"
#include <limits>

inline void check_public_case_relations_index_reference_cases() {
  using namespace case_relations_reference;
"""
    for row in report["rows"]:
        if not row["inputs_unchanged"] or "error" in row:
            raise ValueError("Mutated input or failed public observation")
        if len(row["inputs"]) != 3 or len(row["input_indexes"]) != 3:
            raise ValueError("Missing source table/index")
        output += "  {\n"
        for name, frame, metadata in zip(
            ("base", "incoming", "relations"),
            row["inputs"],
            row["input_indexes"],
            strict=True,
        ):
            output += "    auto " + name + " = " + HELPERS["table"](frame) + ";\n"
            output += (
                "    "
                + name
                + ".index = "
                + index(metadata, len(frame["records"]))
                + ";\n"
            )
        output += "    const auto expected = " + HELPERS["table"](row["result"]) + ";\n"
        output += (
            "    const auto expected_index = "
            + index(row["output_index"], len(row["result"]["records"]))
            + ";\n"
        )
        output += """    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
"""
        output += (
            "    reference_check("
            + HELPERS["quote"](row["name"])
            + ", actual, expected);\n"
        )
        output += """    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
"""
        output += (
            "    if (actual_index != expected_index) throw std::runtime_error("
            + HELPERS["quote"](row["name"] + ": public index metadata differs")
            + ");\n  }\n"
        )
    return output + "}\n"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    raw = args.reference.read_bytes()
    if args.reference.suffix == ".gz":
        raw = gzip.decompress(raw)
    args.output.write_text(generate(raw), encoding="utf-8", newline="\n")
