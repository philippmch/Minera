"""Generate native table regressions from captured public input/output values.

Checks columns, row order, scalar values/kinds and native validation rejection
for public error cases. Integral floats and integers compare by exact numeric
value; this does not assert pandas dtype promotion or exact exception messages.
Dataframe index labels and dtype metadata are not claimed by these checks.
"""

import argparse
import gzip
import hashlib
import json
from pathlib import Path

EXPECTED_CASES = (
    "reversed_multi_case_global_order",
    "reversed_relation_order_with_unrelated_base",
    "numeric_integer_timestamp", "numeric_float_timestamp",
    "string_timestamp_lexical", "string_numeric_looking_timestamp_lexical",
    "typed_datetime_timestamp_custom_selector", "mixed_numeric_string_timestamp",
    "mixed_datetime_string_timestamp", "missing_case_id_in_base",
    "missing_case_id_in_incoming", "missing_case_id_in_relations",
    "nan_case_id_in_relations", "duplicate_valid_relations",
    "unmatched_relation_with_valid_relation", "custom_string_timestamp_selector",
    "relation_extra_with_incoming_collision", "numeric_case_ids_string_relation_ids",
    "mixed_object_event_ids_numeric_relations", "mixed_object_event_ids_mixed_relations",
    "empty_relation_table", "null_right_matches_null_incoming",
    "mixed_datetime_numeric_timestamp", "bool_numeric_timestamp",
    "large_int_float_object_timestamp_precision_collision",
    "incoming_preexisting_link_x", "relation_preexisting_link_y",
) + tuple("time_" + era + "_" + selector for era in ("modern", "pre_epoch")
          for selector in ("default", "custom")) + tuple(
    "time_" + selector + "_" + mode for selector in ("default", "custom")
    for mode in ("mixed", "all-null", "base-column-absent", "incoming-column-absent",
                 "both-columns-absent", "invalid-text"))


def quote(value):
    return json.dumps(str(value), ensure_ascii=True)


def cell(column, value, kind):
    key = quote(column)
    if value is None:
        return f"reference_null({key})"
    if isinstance(value, bool):
        return f"reference_number({key}, {'true' if value else 'false'})"
    if isinstance(value, int):
        return f"reference_number({key}, std::int64_t{{{value}}})"
    if isinstance(value, float):
        return f"reference_number({key}, double{{{value!r}}})"
    if kind == "date":
        return f"reference_date({key}, {quote(value)})"
    return f"EventAttribute{{{key}, {quote(value)}, AttributeKind::kString}}"


def table(frame):
    columns = frame["columns"]
    rows = []
    for record in frame["records"]:
        rows.append("{" + ", ".join(
            cell(column, record["values"][column], record["kinds"][column])
            for column in columns) + "}")
    return "EventTable{{" + ", ".join(map(quote, columns)) + "}, {\n" + ",\n".join(rows) + "}}"


PREAMBLE = r'''// Generated from public PM4Py observations; do not edit by hand.
#pragma once

#include "minera/case_relations.hpp"
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace case_relations_reference {
using namespace minera;

inline EventAttribute reference_null(const std::string& key) {
  EventAttribute cell{key, "", AttributeKind::kString};
  cell.is_null = true;
  return cell;
}
template<class T> EventAttribute reference_number(const std::string& key, T value) {
  EventAttribute cell{key, std::to_string(value),
      std::is_same_v<T, bool> ? AttributeKind::kBoolean :
      std::is_integral_v<T> ? AttributeKind::kInt : AttributeKind::kFloat};
  cell.typed_value = value;
  return cell;
}
inline EventAttribute reference_date(const std::string& key, const std::string& value) {
  EventAttribute cell{key, value, AttributeKind::kDate};
  cell.typed_value = NativeDate{value};
  return cell;
}
inline bool reference_missing(const std::optional<EventAttribute>& cell) {
  return !cell || cell->is_null;
}
inline bool reference_cell_equal(const std::optional<EventAttribute>& actual,
                                 const std::optional<EventAttribute>& expected) {
  if (reference_missing(actual) || reference_missing(expected))
    return reference_missing(actual) == reference_missing(expected);
  if (const auto* number = std::get_if<std::int64_t>(&expected->typed_value)) {
    if (const auto* value = std::get_if<std::int64_t>(&actual->typed_value))
      return *value == *number;
    if (const auto* value = std::get_if<double>(&actual->typed_value))
      return std::isfinite(*value) && *value >= -9223372036854775808.0 &&
             *value < 9223372036854775808.0 && std::trunc(*value) == *value &&
             static_cast<std::int64_t>(*value) == *number;
    return false;
  }
  if (const auto* number = std::get_if<double>(&expected->typed_value)) {
    if (const auto* value = std::get_if<double>(&actual->typed_value)) return *value == *number;
    if (const auto* value = std::get_if<std::int64_t>(&actual->typed_value))
      return *number >= -9223372036854775808.0 && *number < 9223372036854775808.0 &&
             std::trunc(*number) == *number && static_cast<std::int64_t>(*number) == *value;
    return false;
  }
  if (const auto* boolean = std::get_if<bool>(&expected->typed_value)) {
    const auto* value = std::get_if<bool>(&actual->typed_value);
    return value && *value == *boolean;
  }
  if (const auto* expected_date = std::get_if<NativeDate>(&expected->typed_value)) {
    const auto* date = std::get_if<NativeDate>(&actual->typed_value);
    return date && date->value == expected_date->value;
  }
  return std::holds_alternative<std::monostate>(actual->typed_value) &&
         actual->value == expected->value;
}
inline void reference_check(const std::string& name, const EventTable& actual,
                            const EventTable& expected) {
  if (actual.columns != expected.columns || actual.rows.size() != expected.rows.size())
    throw std::runtime_error(name + ": public table shape/columns differ");
  actual.validate();
  for (std::size_t row = 0; row < expected.rows.size(); ++row)
    for (std::size_t col = 0; col < expected.columns.size(); ++col)
      if (!reference_cell_equal(actual.rows[row][col], expected.rows[row][col]))
        throw std::runtime_error(name + ": public cell differs at " +
            std::to_string(row) + ", " + expected.columns[col]);
}
} // namespace case_relations_reference

inline void check_public_case_relations_reference_cases() {
  using namespace case_relations_reference;
'''


def generate(source):
    report = json.loads(source)
    rows = report["rows"]
    if report["reference_version"] != "2.7.22" or report["pandas_version"] != "3.0.5" or not rows:
        raise ValueError("Expected nonempty pinned public observations")
    names = [row["name"] for row in rows]
    if tuple(names) != EXPECTED_CASES or not all(row["inputs_unchanged"] for row in rows):
        raise ValueError("Incomplete observations or mutated public inputs")
    provenance = report["reference_provenance"]
    if provenance["commit"] != "0715c31f3f1d85924107d6aa5329806db69df899":
        raise ValueError("Wrong reference pin")
    modules = provenance["modules"]
    if len(modules) != 2 or any(module["installed_sha256"] != module["pinned_sha256"]
                                for module in modules):
        raise ValueError("Reference module provenance differs")
    time = report["time_reference"]
    if time["reference_version"] != report["reference_version"] or time["pandas_version"] != report["pandas_version"] or time["reference_provenance"] != provenance:
        raise ValueError("Timestamp reference provenance differs")
    expected_times = [(prefix, key, None) for prefix in ("2024-01-01T00:00:00", "1969-12-31T23:59:59")
                      for key in ("time:timestamp", "started")] + [
        (None, key, mode) for key in ("time:timestamp", "started")
        for mode in ("mixed", "all-null", "base-column-absent", "incoming-column-absent", "both-columns-absent", "invalid-text")]
    if [(row.get("prefix"), row["timestamp_key"], row.get("mode")) for row in time["rows"]] != expected_times:
        raise ValueError("Incomplete timestamp observations")
    for observed, legacy in zip(rows[-16:], time["rows"], strict=True):
        if "error" in legacy:
            if observed.get("error", {}).get("type") != legacy["error"]:
                raise ValueError("Timestamp reference error differs")
        elif [record["values"]["concept:name"] for record in observed["result"]["records"]] != legacy["activities"]:
            raise ValueError("Timestamp reference row order differs")
        else:
            actual_times = [record["values"][observed["timestamp_key"]] for record in observed["result"]["records"]]
            expected_values = [value.replace(" ", "T", 1) if value is not None else None for value in legacy["timestamps"]]
            if actual_times != expected_values:
                raise ValueError("Timestamp reference values differ")
    result = "// Reference report SHA256: " + hashlib.sha256(source).hexdigest() + "\n" + PREAMBLE
    for row in rows:
        result += "  {\n    const auto base = " + table(row["base_input"]) + ";\n"
        result += "    const auto incoming = " + table(row["incoming_input"]) + ";\n"
        result += "    const auto relations = " + table(row["relations_input"]) + ";\n"
        result += '    CaseRelationsOptions options;\n    options.case_id_key = "case:concept:name";\n'
        result += "    options.timestamp_key = " + quote(row["timestamp_key"]) + ";\n"
        call = "merge_case_relations(base, incoming, relations, options)"
        if "error" in row:
            result += "    bool rejected = false;\n    try { (void)" + call + "; }\n"
            result += "    catch (const std::invalid_argument&) { rejected = true; }\n"
            result += "    if (!rejected) throw std::runtime_error(" + quote(row["name"] + ": public rejection not reproduced") + ");\n"
        else:
            result += "    const auto expected = " + table(row["result"]) + ";\n"
            result += "    reference_check(" + quote(row["name"]) + ", " + call + ", expected);\n"
        result += "  }\n"
    return result + "}\n"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    raw = args.reference.read_bytes()
    if args.reference.suffix == ".gz":
        raw = gzip.decompress(raw)
    args.output.write_text(generate(raw), encoding="utf-8", newline="\n")
