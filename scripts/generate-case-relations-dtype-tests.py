"""Generate strict column dtype, categorical descriptor and scalar checks."""

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import runpy

HELPERS = runpy.run_path(
    str(Path(__file__).with_name("generate-case-relations-reference-tests.py"))
)
INDEX_HELPERS = runpy.run_path(
    str(Path(__file__).with_name("generate-case-relations-index-tests.py"))
)

EXPECTED_CASES = (
    "dtype_integer_payload_missing_incoming",
    "dtype_float_payload_integer_incoming",
    "dtype_integer_payload_float_incoming",
    "dtype_boolean_payload_integer_incoming",
    "dtype_boolean_payload_missing_incoming",
    "dtype_boolean_timestamp_integer_incoming",
    "dtype_nullable_int64_nulls",
    "dtype_nullable_float64_nulls",
    "dtype_nullable_boolean_nulls",
    "dtype_nullable_string_nulls",
    "dtype_datetime_us_ns",
    "dtype_datetime_missing_incoming",
    "dtype_datetime_missing_base",
    "dtype_datetime_timezone_offsets",
    "dtype_all_empty_typed_inputs",
    "dtype_empty_typed_incoming",
    "dtype_empty_typed_relations",
    "dtype_categorical_case_ids_object_relations",
    "dtype_ordered_categorical_payload_same_categories",
    "dtype_ordered_categorical_payload_different_order",
    "dtype_nullable_string_case_ids_and_null_relation",
    "dtype_matrix_signed_width_int8_int16",
    "dtype_matrix_signed_width_int32_int64",
    "dtype_matrix_unsigned_width_uint8_uint16",
    "dtype_matrix_unsigned_width_uint32_uint64_max",
    "dtype_matrix_float_width_float32_float64",
    "dtype_matrix_bool_bool",
    "dtype_matrix_signed_unsigned_int64_uint64",
    "dtype_matrix_plain_int_missing_base",
    "dtype_matrix_plain_float_missing_incoming",
    "dtype_matrix_plain_bool_missing_base",
    "dtype_matrix_empty_typed_base_uint64",
    "dtype_matrix_empty_typed_incoming_float32",
    "dtype_matrix_empty_both_different_numeric",
    "dtype_matrix_nullable_uint64_nulls",
    "dtype_matrix_nullable_int64_missing_incoming",
    "dtype_matrix_nullable_float64_missing_base",
    "dtype_matrix_nullable_boolean_missing_incoming",
    "dtype_matrix_object_payload_nulls",
    "dtype_matrix_category_unordered_same_categories",
    "dtype_matrix_category_unordered_reordered_categories",
    "dtype_matrix_category_ordered_different_membership",
    "dtype_matrix_category_string_mixture",
    "dtype_matrix_category_numeric_mixture",
    "dtype_matrix_category_case_ids_matching_relation_categories",
    "dtype_matrix_category_missing_values",
    "dtype_matrix_category_tuple_values",
    "dtype_matrix_category_ordered_numeric_values",
)


PROMOTION_TYPES = (
    "int8",
    "int16",
    "int32",
    "int64",
    "uint8",
    "uint16",
    "uint32",
    "uint64",
    "float32",
    "float64",
    "bool",
    "Int64",
    "UInt64",
    "Float64",
    "boolean",
    "str",
    "string",
    "object",
)
EXPECTED_CASES += tuple(
    "dtype_promotion_" + base + "_" + incoming
    for base in PROMOTION_TYPES
    for incoming in PROMOTION_TYPES
)


EXPECTED_CASES += (
    "dtype_category_case_sort_ordered",
    "dtype_category_case_sort_unordered",
    "dtype_category_timestamp_sort",
)


EXPECTED_CASES += tuple(
    "dtype_category_" + dtype + "_" + side
    for dtype in ("str", "string", "object")
    for side in ("first", "last")
)
EXPECTED_CASES += (
    "dtype_numeric_then_category",
    "dtype_category_tuple_null_element",
    "dtype_extreme_signed_unsigned_float_promotion",
    "dtype_category_case_sort_missing",
)

EXPECTED_CASES += tuple(
    "dtype_incompatible_numeric_categories_" + suffix
    for suffix in ("float", "uint64", "signed_unsigned", "float32")
)

EXPECTED_CASES += ("dtype_datetime_same_timezone_mixed_resolution",)


def scalar(value, key="", dtype=None):
    kind, payload = value["kind"], value["value"]
    expected_types = {
        "int": int,
        "float": float,
        "bool": bool,
        "string": str,
        "date": str,
        "tuple": list,
        "null": type(None),
    }
    if kind not in expected_types or type(payload) is not expected_types[kind]:
        raise ValueError("Unknown or inconsistent scalar tag")
    quoted_key = HELPERS["quote"](key)
    if kind == "tuple":
        return (
            "dtype_tuple("
            + quoted_key
            + ", {"
            + ", ".join(scalar(part) for part in payload)
            + "})"
        )
    if kind == "int" and payload == -(2**63):
        return (
            "reference_number("
            + quoted_key
            + ", (std::numeric_limits<std::int64_t>::min)())"
        )
    if kind == "int" and payload is not None:
        if not -(2**63) <= payload < 2**64:
            raise ValueError("Integer exceeds native signed/unsigned 64-bit domain")
        if payload >= 2**63 or (dtype and dtype.lower().startswith("uint")):
            if payload < 0:
                raise ValueError("Negative unsigned scalar")
            return (
                "reference_number("
                + quoted_key
                + ", std::uint64_t{"
                + str(payload)
                + "ULL})"
            )
    return HELPERS["cell"](key, payload, kind).replace(
        "reference_number(", "dtype_number("
    )


def categorical(value):
    if value is None:
        return "std::nullopt"
    if type(value["ordered"]) is not bool or not value["categories_dtype"]:
        raise ValueError("Malformed categorical descriptor")
    return (
        "EventTableCategoricalDtype{"
        + HELPERS["quote"](value["categories_dtype"])
        + ", {"
        + ", ".join(
            scalar(item, dtype=value["categories_dtype"])
            for item in value["categories"]
        )
        + "}, "
        + str(value["ordered"]).lower()
        + "}"
    )


def table(name, frame):
    columns = frame["columns"]
    if len(set(columns)) != len(columns) or any(
        not isinstance(column, str) or not column for column in columns
    ):
        raise ValueError("Malformed source columns")
    if frame["shape"] != [len(frame["records"]), len(columns)] or len(
        frame["cell_tags"]
    ) != len(frame["records"]):
        raise ValueError("Frame shape or cell tags are incomplete")
    if set(frame["dtypes"]) != set(columns):
        raise ValueError("Column dtype declarations are incomplete")
    for record, tagged in zip(frame["records"], frame["cell_tags"], strict=True):
        if any(
            set(record[field]) != set(columns) for field in ("values", "kinds")
        ) or set(tagged["cells"]) != set(columns):
            raise ValueError("Row cells do not match declared schema")
        if record["index"] != tagged["index"]:
            raise ValueError("Cell tags refer to a different row")
        for column in columns:
            expected_tag = {
                "dtype": frame["dtypes"][column],
                "kind": record["kinds"][column],
                "missing": record["kinds"][column] == "null",
                "value": record["values"][column],
            }
            if tagged["cells"][column] != expected_tag:
                raise ValueError("Cell tags disagree with scalar/dtype observations")
    descriptors = frame["categorical_dtypes"]
    if set(descriptors) - set(columns):
        raise ValueError("Categorical descriptor has no source column")
    for column in columns:
        if (frame["dtypes"][column] == "category") != (
            descriptors.get(column) is not None
        ):
            raise ValueError("Categorical dtype descriptor is missing or inconsistent")
    rows = [
        "{"
        + ", ".join(
            scalar(
                {"kind": record["kinds"][column], "value": record["values"][column]},
                column,
                frame["dtypes"][column],
            )
            for column in columns
        )
        + "}"
        for record in frame["records"]
    ]
    result = (
        "  EventTable "
        + name
        + "{{"
        + ", ".join(map(HELPERS["quote"], columns))
        + "}, {\n"
        + ",\n".join(rows)
        + "}};\n"
    )
    result += (
        "  "
        + name
        + ".dtypes = {"
        + ", ".join(HELPERS["quote"](frame["dtypes"][column]) for column in columns)
        + "};\n"
    )
    result += (
        "  "
        + name
        + ".categorical_dtypes = {"
        + ", ".join(categorical(descriptors.get(column)) for column in columns)
        + "};\n"
    )
    return result


PREAMBLE = r"""#pragma once
#include "case_relations_reference_cases.hpp"
#include <limits>
#include <iomanip>
#include <sstream>
#include <charconv>

namespace case_relations_dtype_reference {
using namespace case_relations_reference;

template<class T> EventAttribute dtype_number(const std::string& key, T value) {
  auto cell = reference_number(key, value);
  if constexpr (std::is_floating_point_v<T>) {
    std::ostringstream text;
    text << std::setprecision(std::numeric_limits<T>::max_digits10) << value;
    cell.value = text.str();
  }
  return cell;
}
template<class T> bool dtype_integer_text(const std::string& text, T expected) {
  T parsed{};
  const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
  return result.ec == std::errc{} && result.ptr == text.data() + text.size() && parsed == expected;
}
inline bool dtype_projection_valid(const EventAttribute& cell) {
  if (cell.is_null || cell.container != AttributeContainerKind::kScalar) return true;
  if (const auto* v = std::get_if<std::int64_t>(&cell.typed_value)) return dtype_integer_text(cell.value, *v);
  if (const auto* v = std::get_if<std::uint64_t>(&cell.typed_value)) return dtype_integer_text(cell.value, *v);
  if (const auto* v = std::get_if<bool>(&cell.typed_value))
    return *v ? (cell.value == "true" || cell.value == "True" || cell.value == "1")
              : (cell.value == "false" || cell.value == "False" || cell.value == "0");
  if (const auto* v = std::get_if<double>(&cell.typed_value)) {
    try { std::size_t used{}; const auto parsed = std::stod(cell.value, &used);
      return used == cell.value.size() && parsed == *v;
    } catch (...) { return false; }
  }
  if (const auto* v = std::get_if<NativeDate>(&cell.typed_value)) return cell.value == v->value;
  return true;
}

inline EventAttribute dtype_tuple(const std::string& key,
                                  std::vector<EventAttribute> children) {
  EventAttribute result{key, "", AttributeKind::kString};
  result.container = AttributeContainerKind::kTuple;
  result.children = std::move(children);
  return result;
}
inline bool dtype_attribute_equal(const EventAttribute& actual, const EventAttribute& expected) {
  if (actual.is_null != expected.is_null) return false;
  if (actual.is_null) return true;
  if (!dtype_projection_valid(actual)) return false;
  if (actual.kind != expected.kind || actual.container != expected.container ||
      actual.children.size() != expected.children.size())
    return false;
  bool scalar_equal = actual.typed_value == expected.typed_value;
  if (const auto* a = std::get_if<std::uint64_t>(&actual.typed_value)) {
    if (const auto* e = std::get_if<std::int64_t>(&expected.typed_value))
      scalar_equal = *e >= 0 && *a == static_cast<std::uint64_t>(*e);
  }
  if (const auto* a = std::get_if<std::int64_t>(&actual.typed_value)) {
    if (const auto* e = std::get_if<std::uint64_t>(&expected.typed_value))
      scalar_equal = *a >= 0 && static_cast<std::uint64_t>(*a) == *e;
  }
  if (!scalar_equal) return false;
  if (std::holds_alternative<std::monostate>(expected.typed_value) && actual.value != expected.value)
    return false;
  for (std::size_t i = 0; i < expected.children.size(); ++i)
    if (!dtype_attribute_equal(actual.children[i], expected.children[i])) return false;
  return true;
}
inline void check_dtype_table(const std::string& name, const EventTable& actual,
                              const EventTable& expected) {
  actual.validate();
  if (actual.columns != expected.columns || actual.rows.size() != expected.rows.size())
    throw std::runtime_error(name + ": output shape/columns differ");
  if (actual.dtypes != expected.dtypes) {
    std::string detail;
    for (std::size_t i = 0; i < expected.columns.size(); ++i)
      detail += " " + expected.columns[i] + "=" +
          (i < actual.dtypes.size() ? actual.dtypes[i] : "<absent>") +
          " (expected " + expected.dtypes[i] + ")";
    throw std::runtime_error(name + ": column dtypes differ:" + detail);
  }
  for (std::size_t column = 0; column < expected.columns.size(); ++column) {
    const auto actual_category = actual.categorical_dtypes.empty()
        ? std::optional<EventTableCategoricalDtype>{} : actual.categorical_dtypes[column];
    const auto expected_category = expected.categorical_dtypes.empty()
        ? std::optional<EventTableCategoricalDtype>{} : expected.categorical_dtypes[column];
    if (actual_category != expected_category)
      throw std::runtime_error(name + ": categorical metadata differs for " + expected.columns[column]);
    for (std::size_t row = 0; row < expected.rows.size(); ++row) {
      const auto& observed = actual.rows[row][column];
      const auto& reference = expected.rows[row][column];
      if (observed && observed->key != actual.columns[column])
        throw std::runtime_error(name + ": output cell key differs");
      if (reference_missing(reference) != reference_missing(observed) ||
          (!reference_missing(reference) && !dtype_attribute_equal(*observed, *reference)))
        throw std::runtime_error(name + ": scalar value/kind differs for " + expected.columns[column]);
    }
  }
}
"""


def generate(source):
    report = json.loads(source)
    if (
        report["format_version"] != 2
        or report["algorithm"] != "pm4py.algo.merging.case_relations.algorithm.apply"
    ):
        raise ValueError("Wrong evidence format or public algorithm")
    if report["reference_version"] != "2.7.22" or report["pandas_version"] != "3.0.5":
        raise ValueError("Wrong public runtime")
    if (
        not EXPECTED_CASES
        or tuple(row["name"] for row in report["rows"]) != EXPECTED_CASES
    ):
        raise ValueError("Incomplete or reordered dtype observations")
    if report["case_count"] != len(EXPECTED_CASES):
        raise ValueError("Wrong observation count")
    provenance = report["reference_provenance"]
    if provenance["commit"] != "0715c31f3f1d85924107d6aa5329806db69df899":
        raise ValueError("Wrong reference pin")
    if report["reference_commit"] != provenance["commit"]:
        raise ValueError("Conflicting reference commits")
    expected_modules = INDEX_HELPERS["PINNED_MODULES"]
    if (
        len(provenance["modules"]) != len(expected_modules)
        or {module["module"] for module in provenance["modules"]}
        != set(expected_modules)
        or any(
            module["installed_sha256"] != module["pinned_sha256"]
            or module["pinned_sha256"] != expected_modules.get(module["module"])
            for module in provenance["modules"]
        )
    ):
        raise ValueError("Reference module provenance differs")
    output = (
        "// Public dtype report SHA256: "
        + hashlib.sha256(source).hexdigest()
        + "\n"
        + PREAMBLE
    )
    for position, row in enumerate(report["rows"]):
        if row["timestamp_key"] != "when" or row["parameters"] != {
            "pm4py:param:timestamp_key": row["timestamp_key"]
        }:
            raise ValueError("Public selectors differ from generated native options")
        if not row["inputs_unchanged"] or not row["input_metadata_unchanged"]:
            raise ValueError("Public call mutated input metadata or values")
        if "error" in row or "result" not in row:
            raise ValueError("Expected successful pinned dtype observation")
        if any(
            row[name + "_input"] != row["input_metadata_after"][name]
            for name in ("base", "incoming", "relations")
        ):
            raise ValueError("Before/after metadata observations disagree")
        if any(
            row["input_cell_tags"][name] != row[name + "_input"]["cell_tags"]
            for name in ("base", "incoming", "relations")
        ):
            raise ValueError("Input cell tag observations disagree")
        output += "inline void check_case_" + str(position) + "() {\n"
        for name in ("base", "incoming", "relations"):
            output += table(name, row[name + "_input"])
        output += '  CaseRelationsOptions options;\n  options.case_id_key = "case:concept:name";\n'
        output += (
            "  options.timestamp_key = "
            + HELPERS["quote"](row["timestamp_key"])
            + ";\n"
        )
        output += table("expected", row["result"])
        output += (
            "  check_dtype_table("
            + HELPERS["quote"](row["name"])
            + ", merge_case_relations(base, incoming, relations, options), expected);\n"
        )
        output += "}\n"
    output += "} // namespace case_relations_dtype_reference\n\n"
    output += "inline void check_public_case_relations_dtype_reference_cases() {\n"
    output += "  std::string failures;\n"
    for position in range(len(report["rows"])):
        output += (
            "  try { case_relations_dtype_reference::check_case_"
            + str(position)
            + "(); }\n"
            '  catch (const std::exception& error) { failures += std::string(error.what()) + "\\n"; }\n'
        )
    output += "  if (!failures.empty()) throw std::runtime_error(failures);\n"
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
