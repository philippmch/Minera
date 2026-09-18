"""Generate pinned dataframe formatting checks, including input mutations."""

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import runpy

ROOT = Path(__file__).resolve().parent
DTYPE = runpy.run_path(str(ROOT / "generate-case-relations-dtype-tests.py"))
INDEX = runpy.run_path(str(ROOT / "generate-case-relations-index-tests.py"))
QUOTE = DTYPE["HELPERS"]["quote"]
PIN = "0715c31f3f1d85924107d6aa5329806db69df899"
MODULE_HASH = "cf480411c34ff169468b726424a6563aedb6c94274b283973ca194ec51235a88"
EXPECTED_CASES = (
    "normal_defaults_unsorted",
    "normal_explicit_default_selectors",
    "custom_selectors_and_existing_start",
    "custom_start_missing_is_added",
    "default_start_existing_values",
    "extra_timestamp_like_columns",
    "attrs_nested_metadata",
    "named_index_labels",
    "tie_order_preserves_input_order",
    "numeric_case_and_activity_ids",
    "boolean_case_and_activity_ids",
    "float_case_ids_and_object_activity",
    "nullable_case_activity_values",
    "missing_case_column",
    "missing_activity_column",
    "missing_timestamp_column",
    "null_timestamp_values",
    "invalid_timestamp_text",
    "explicit_format_success",
    "explicit_format_failure",
    "mixed_format_keyword",
    "timezone_offsets_as_strings",
    "timezone_aware_typed_series",
    "datetime_microsecond_series",
    "datetime_nanosecond_series",
    "numeric_timestamp_values",
    "boolean_timestamp_values",
    "typed_empty_object_schema",
    "typed_empty_extension_schema",
    "empty_missing_required_columns",
    "empty_custom_selector_schema",
    "categorical_case_activity",
    "canonical_column_collision_extra_start",
    "custom_selector_same_case_and_activity",
    "custom_selector_same_timestamp_and_start",
    "duplicate_canonical_columns",
    "multiindex_input_labels",
    "object_mixed_timestamp_values",
    "explicit_start_missing_value",
    "explicit_start_timezone_offset",
    "non_string_column_labels_with_selectors",
    "start_selector_none",
)
EXPECTED_CASES += (
    "extra_valid_date_columns",
    "custom_selectors_overwrite_canonical",
    "existing_generated_index_columns",
    "custom_missing_activity_partial_mutation",
    "custom_missing_timestamp_partial_mutation",
    "mixed_valid_invalid_timestamp_text",
    "nanosecond_timestamp_strings",
    "cross_canonical_selectors",
    "object_boolean_integer_timestamps",
    "object_exact_numeric_timestamps",
    "object_unsigned_timestamps",
)

NUMERIC_BOUNDARY_CASES = (
    "numeric_boundary_duplicate_float_late_same",
    "numeric_boundary_duplicate_float_late_split",
    "numeric_boundary_duplicate_float_first_same",
    "numeric_boundary_duplicate_float_first_split",
    "numeric_boundary_duplicate_bool_late_same",
    "numeric_boundary_duplicate_bool_late_split",
    "numeric_boundary_duplicate_bool_first_same",
    "numeric_boundary_duplicate_bool_first_split",
    "numeric_boundary_float_negative_zero_same",
    "numeric_boundary_float_negative_zero_split",
    "numeric_boundary_bool_float_wide_same",
    "numeric_boundary_bool_float_wide_split",
    "numeric_boundary_string_float_wide_same",
    "numeric_boundary_string_float_wide_split",
    "numeric_boundary_float_exact_equal_same",
    "numeric_boundary_float_exact_equal_split",
    "numeric_boundary_float_rounding_pair_same",
    "numeric_boundary_float_rounding_pair_split",
    "numeric_boundary_float_bool_rounding_same",
    "numeric_boundary_float_bool_rounding_split",
    "numeric_boundary_timestamp_text_before_number_same",
    "numeric_boundary_timestamp_text_before_number_split",
    "numeric_boundary_timestamp_boolean_text_same",
    "numeric_boundary_timestamp_boolean_text_split",
    "numeric_boundary_nanosecond_nat_with_bad_same",
    "numeric_boundary_nanosecond_nat_with_bad_split",
    "numeric_boundary_numeric_datetime_string_same",
    "numeric_boundary_numeric_datetime_string_split",
    "numeric_boundary_numeric_signed_nat_and_bool_same",
    "numeric_boundary_numeric_signed_nat_and_bool_split",
    "numeric_boundary_wide_float_collision_same",
    "numeric_boundary_wide_float_collision_split",
    "numeric_boundary_wide_float_safe_same",
    "numeric_boundary_wide_float_safe_split",
    "numeric_boundary_wide_integer_collision_same",
    "numeric_boundary_wide_integer_collision_split",
    "numeric_boundary_wide_only_float_same",
    "numeric_boundary_wide_only_float_split",
    "numeric_boundary_wide_one_float_same",
    "numeric_boundary_wide_one_float_split",
    "numeric_boundary_wide_bool_collision_same",
    "numeric_boundary_wide_bool_collision_split",
    "numeric_boundary_bool_exact_collision_same",
    "numeric_boundary_bool_exact_collision_split",
    "numeric_boundary_negative_wide_same",
    "numeric_boundary_negative_wide_split",
    "numeric_boundary_negative_wide_float_same",
    "numeric_boundary_negative_wide_float_split",
    "numeric_boundary_infinite_collision_same",
    "numeric_boundary_infinite_collision_split",
    "numeric_boundary_negative_infinite_same",
    "numeric_boundary_negative_infinite_split",
    "numeric_boundary_numeric_string_same",
    "numeric_boundary_numeric_string_split",
    "numeric_boundary_mixed_object_int_float_same",
    "numeric_boundary_mixed_object_int_float_split",
    "numeric_boundary_object_large_signed_same",
    "numeric_boundary_object_large_signed_split",
    "numeric_boundary_object_signed_nat_same",
    "numeric_boundary_object_signed_nat_split",
    "numeric_boundary_object_unsigned_epoch_same",
    "numeric_boundary_object_unsigned_epoch_split",
    "numeric_boundary_object_unsigned_wrap_same",
    "numeric_boundary_object_unsigned_wrap_split",
    "numeric_boundary_object_overflow_float_same",
    "numeric_boundary_object_overflow_float_split",
    "numeric_boundary_custom_error_mutation",
)
EXPECTED_CASES += NUMERIC_BOUNDARY_CASES
EXPECTED_CASES += (
    "calendar_invalid_february",
    "calendar_nonleap_february",
    "calendar_leap_february",
    "calendar_invalid_april",
    "calendar_valid_april",
    "calendar_century_nonleap",
    "calendar_century_leap",
)

EXCLUDED_CASES = {
    "non_string_column_labels_with_selectors": "EventTable column labels are strings."
}

EXPECTED_ERRORS = {
    "missing_case_column": {
        "type": "Exception",
        "message": "case:concept:name column (case ID) is not in the dataframe!",
    },
    "missing_activity_column": {
        "type": "Exception",
        "message": "concept:name column (activity) is not in the dataframe!",
    },
    "missing_timestamp_column": {
        "type": "Exception",
        "message": "time:timestamp column (timestamp) is not in the dataframe!",
    },
    "empty_missing_required_columns": {
        "type": "Exception",
        "message": "case:concept:name column (case ID) is not in the dataframe!",
    },
    "duplicate_canonical_columns": {
        "type": "AttributeError",
        "message": "'DataFrame' object has no attribute 'dtype'",
    },
    "custom_missing_activity_partial_mutation": {
        "type": "Exception",
        "message": "missing column (activity) is not in the dataframe!",
    },
    "custom_missing_timestamp_partial_mutation": {
        "type": "Exception",
        "message": "missing column (timestamp) is not in the dataframe!",
    },
}
NUMERIC_BOUNDARY_ERRORS = (
    "numeric_boundary_duplicate_float_first_same",
    "numeric_boundary_duplicate_float_first_split",
    "numeric_boundary_duplicate_bool_late_same",
    "numeric_boundary_duplicate_bool_late_split",
    "numeric_boundary_float_rounding_pair_same",
    "numeric_boundary_float_rounding_pair_split",
    "numeric_boundary_wide_float_collision_same",
    "numeric_boundary_wide_float_collision_split",
    "numeric_boundary_wide_only_float_same",
    "numeric_boundary_wide_only_float_split",
    "numeric_boundary_negative_wide_float_same",
    "numeric_boundary_negative_wide_float_split",
    "numeric_boundary_infinite_collision_same",
    "numeric_boundary_infinite_collision_split",
    "numeric_boundary_negative_infinite_same",
    "numeric_boundary_negative_infinite_split",
    "numeric_boundary_object_overflow_float_same",
    "numeric_boundary_object_overflow_float_split",
    "numeric_boundary_custom_error_mutation",
)
EXPECTED_ERRORS.update(
    {
        name: {"type": "ValueError", "message": "Categorical categories must be unique"}
        for name in NUMERIC_BOUNDARY_ERRORS
    }
)

EXPECTED_SIGNATURE = "(df, case_id: str = 'case:concept:name', activity_key: str = 'concept:name', timestamp_key: str = 'time:timestamp', start_timestamp_key: str = 'start_timestamp', timest_format: Optional[str] = None) -> Any"


def tag(value):
    if value is None:
        return {"kind": "null", "value": None}
    kinds = {bool: "bool", int: "int", float: "float", str: "string"}
    if type(value) in kinds:
        return {"kind": kinds[type(value)], "value": value}
    raise ValueError("Unsupported untagged scalar")


def scalar(value, key="", dtype=None):
    if value["kind"] == "float" and value["value"] in ("Infinity", "-Infinity"):
        sign = "-" if value["value"] == "-Infinity" else ""
        return (
            "dtype_number("
            + QUOTE(key)
            + ", "
            + sign
            + "std::numeric_limits<double>::infinity())"
        )
    if value["kind"] == "tuple":
        parts = value["value"]
        value = {
            "kind": "tuple",
            "value": [part if isinstance(part, dict) else tag(part) for part in parts],
        }
    return DTYPE["scalar"](value, key, dtype)


def property_value(key, value):
    if isinstance(value, dict):
        return (
            "format_container("
            + QUOTE(key)
            + ", AttributeContainerKind::kContainer, {"
            + ", ".join(
                property_value(child_key, child) for child_key, child in value.items()
            )
            + "})"
        )
    if isinstance(value, list):
        return (
            "format_container("
            + QUOTE(key)
            + ", AttributeContainerKind::kList, {"
            + ", ".join(property_value("", child) for child in value)
            + "})"
        )
    return scalar(tag(value), key)


def index_value(label):
    if label["kind"] == "tuple":
        return [index_value(child) for child in label["value"]]
    scalar(label)
    return label["value"]


def validate_frame(frame):
    if frame["type"] != "DataFrame":
        raise ValueError("Unexpected public frame type")
    columns = frame["columns"]
    metadata = frame["column_metadata"]
    records = frame["records"]
    if len(metadata) != len(columns) or frame["shape"] != [len(records), len(columns)]:
        raise ValueError("Incomplete frame shape")
    keys = [column["key"] for column in metadata]
    if len(set(keys)) != len(keys) or set(keys) != set(frame["dtypes"]):
        raise ValueError("Incomplete positional column metadata")
    for position, column in enumerate(metadata):
        if (
            column["position"] != position
            or column["label"] != columns[position]
            or column["dtype"] != frame["dtypes"][column["key"]]
            or not column["dtype"]
        ):
            raise ValueError("Column metadata disagrees")
    index = frame["index_metadata"]
    if (
        len(index["values"]) != len(records)
        or len(index["labels"]) != len(records)
        or not index["names"]
    ):
        raise ValueError("Incomplete index metadata")
    if [index_value(label) for label in index["labels"]] != index["values"]:
        raise ValueError("Typed index labels disagree with public values")
    if index["name"] != (index["names"][0] if len(index["names"]) == 1 else None):
        raise ValueError("Index name observations disagree")
    for position, record in enumerate(records):
        if (
            record["position"] != position
            or record["index"] != index["values"][position]
        ):
            raise ValueError("Row position/index disagrees")
        if any(
            set(record[field]) != set(keys)
            for field in ("values", "kinds", "cell_tags")
        ):
            raise ValueError("Incomplete row metadata")
        for key in keys:
            observed = record["cell_tags"][key]
            expected = {
                "dtype": frame["dtypes"][key],
                "kind": record["kinds"][key],
                "missing": record["kinds"][key] == "null",
                "value": record["values"][key],
            }
            if observed != expected:
                raise ValueError("Cell tag disagrees")
    categories = frame["categorical_dtypes"]
    if set(categories) - set(keys):
        raise ValueError("Categorical descriptor has no column")
    if any(
        (frame["dtypes"][key] == "category") != (categories.get(key) is not None)
        for key in keys
    ):
        raise ValueError("Missing categorical descriptor")


def frame_code(name, frame):
    validate_frame(frame)
    if any(not isinstance(column, str) for column in frame["columns"]):
        raise ValueError("Non-string native column")
    keys = [column["key"] for column in frame["column_metadata"]]
    rows = []
    for record in frame["records"]:
        rows.append(
            "{"
            + ", ".join(
                scalar(
                    {"kind": record["kinds"][key], "value": record["values"][key]},
                    column,
                    frame["dtypes"][key],
                )
                for column, key in zip(frame["columns"], keys, strict=True)
            )
            + "}"
        )
    result = (
        "  EventTable "
        + name
        + "{{"
        + ", ".join(map(QUOTE, frame["columns"]))
        + "}, {\n"
        + ",\n".join(rows)
        + "}};\n"
    )
    result += (
        "  "
        + name
        + ".dtypes = {"
        + ", ".join(QUOTE(frame["dtypes"][key]) for key in keys)
        + "};\n"
    )
    result += (
        "  "
        + name
        + ".categorical_dtypes = {"
        + ", ".join(
            DTYPE["categorical"](frame["categorical_dtypes"].get(key)) for key in keys
        )
        + "};\n"
    )
    result += (
        "  "
        + name
        + ".index = "
        + INDEX["index"](frame["index_metadata"], len(rows))
        + ";\n"
    )
    result += (
        "  "
        + name
        + ".metadata.properties = {"
        + ", ".join(property_value(key, value) for key, value in frame["attrs"].items())
        + "};\n"
    )
    return result


PREAMBLE = r"""
#include "minera/dataframe_format.hpp"
#include <algorithm>
namespace format_dataframe_reference {
using namespace case_relations_dtype_reference;
inline EventAttribute format_container(const std::string& key,
    AttributeContainerKind kind, std::vector<EventAttribute> children) {
  EventAttribute result{key, "", AttributeKind::kString};
  result.container = kind;
  result.children = std::move(children);
  return result;
}
inline EventTableIndex format_index(const EventTable& table) {
  if (table.index) return *table.index;
  EventTableIndex result;
  result.kind = "RangeIndex";
  for (std::size_t i = 0; i < table.rows.size(); ++i)
    result.labels.push_back({reference_number("", static_cast<std::int64_t>(i))});
  return result;
}
inline void check_format_table(const std::string& name, const EventTable& actual,
                               const EventTable& expected) {
  if (actual.columns != expected.columns || actual.rows.size() != expected.rows.size() ||
      actual.dtypes != expected.dtypes)
    throw std::runtime_error(name + ": columns, rows or dtypes differ");
  if (actual.categorical_dtypes != expected.categorical_dtypes) {
    const auto has_category = [](const auto& values) {
      return std::any_of(values.begin(), values.end(), [](const auto& value) { return value.has_value(); });
    };
    if (has_category(actual.categorical_dtypes) || has_category(expected.categorical_dtypes))
      throw std::runtime_error(name + ": categorical metadata differs");
  }
  for (std::size_t row = 0; row < expected.rows.size(); ++row) {
    if (actual.rows[row].size() != expected.rows[row].size())
      throw std::runtime_error(name + ": row width differs");
    for (std::size_t column = 0; column < expected.columns.size(); ++column) {
      const auto& observed = actual.rows[row][column];
      const auto& reference = expected.rows[row][column];
      if (observed && observed->key != expected.columns[column])
        throw std::runtime_error(name + ": cell key differs");
      if (reference_missing(observed) != reference_missing(reference) ||
          (!reference_missing(reference) && !dtype_attribute_equal(*observed, *reference)))
        throw std::runtime_error(name + ": cell differs at row " + std::to_string(row) + " column " + expected.columns[column]);
    }
  }
  if (format_index(actual) != format_index(expected))
    throw std::runtime_error(name + ": index differs");
  if (actual.metadata.properties != expected.metadata.properties)
    throw std::runtime_error(name + ": dataframe attrs differ");
}
"""


def generate(source):
    report = json.loads(source)
    if (
        report["reference_version"] != "2.7.22"
        or report["pandas_version"] != "3.0.5"
        or report["capture_id"] != "format_dataframe_reference"
    ):
        raise ValueError("Wrong public formatter/runtime")
    if report["public_signature"] != EXPECTED_SIGNATURE:
        raise ValueError("Public formatter signature differs")
    provenance = report["reference_provenance"]
    if (
        provenance["commit"] != PIN
        or provenance["function_module"] != "pm4py.utils"
        or provenance["function_qualname"] != "format_dataframe"
        or provenance["module_relative_path"] != "pm4py/utils.py"
        or provenance["installed_sha256"] != MODULE_HASH
        or provenance["pinned_sha256"] != MODULE_HASH
    ):
        raise ValueError("Reference defining module is not pinned")
    rows = report["rows"]
    if (
        not EXPECTED_CASES
        or tuple(row["name"] for row in rows) != EXPECTED_CASES
        or report["case_count"] != len(EXPECTED_CASES)
    ):
        raise ValueError("Incomplete formatter manifest")
    if report["successful_case_count"] != sum(
        "result" in row for row in rows
    ) or report["error_case_count"] != sum("error" in row for row in rows):
        raise ValueError("Outcome counts disagree")
    output = (
        "// Public formatter report SHA256: "
        + hashlib.sha256(source).hexdigest()
        + "\n"
    )
    output += (
        DTYPE["PREAMBLE"]
        + "\n} // namespace case_relations_dtype_reference\n"
        + PREAMBLE
    )
    included = []
    for position, row in enumerate(rows):
        if ("result" in row) == ("error" in row):
            raise ValueError("Expected exactly one public outcome")
        if row.get("error") != EXPECTED_ERRORS.get(row["name"]):
            raise ValueError("Unexpected public success/error observation")
        for field in ("input", "input_after", "result"):
            if field in row:
                validate_frame(row[field])
        if (
            row["input_scalar_snapshot_before"] != row["input"]["records"]
            or row["input_scalar_snapshot_after"] != row["input_after"]["records"]
        ):
            raise ValueError("Mutation snapshots disagree")
        if row["input_metadata_unchanged"] != (row["input"] == row["input_after"]):
            raise ValueError("Metadata mutation flag disagrees")
        if (
            row["inputs_unchanged"] != row["input_metadata_unchanged"]
            or row["input_mutated"] == row["inputs_unchanged"]
            or row["input_metadata_mutated"] == row["input_metadata_unchanged"]
        ):
            raise ValueError("Mutation flags disagree")
        if "result" in row and (
            row["returned_type"] != "DataFrame"
            or row["returned_same_object"] is not False
        ):
            raise ValueError("Unexpected public return contract")
        if row["name"] in EXCLUDED_CASES:
            if all(isinstance(column, str) for column in row["input"]["columns"]):
                raise ValueError("Excluded input no longer has non-string columns")
            continue
        included.append(position)
        output += "inline void check_case_" + str(position) + "() {\n"
        output += frame_code("input", row["input"])
        output += frame_code("expected_input", row["input_after"])
        output += "  DataFrameFormatOptions options;\n"
        for key, value in row["parameters"].items():
            native = "timestamp_format" if key == "timest_format" else key
            if native not in (
                "case_id",
                "activity_key",
                "timestamp_key",
                "start_timestamp_key",
                "timestamp_format",
            ):
                raise ValueError("Unmapped public option")
            if value is None and native not in (
                "start_timestamp_key",
                "timestamp_format",
            ):
                raise ValueError("Unrepresentable null selector")
            output += (
                "  options."
                + native
                + " = "
                + ("std::nullopt" if value is None else QUOTE(value))
                + ";\n"
            )
        if "error" in row:
            output += "  bool rejected = false;\n  try { (void)format_dataframe(input, options); }\n  catch (const std::exception&) { rejected = true; }\n"
            output += (
                "  if (!rejected) throw std::runtime_error("
                + QUOTE(row["name"] + ": expected public rejection")
                + ");\n"
            )
        else:
            if (
                row["returned_type"] != "DataFrame"
                or row["returned_same_object"] is not False
            ):
                raise ValueError("Unexpected returned object contract")
            output += frame_code("expected", row["result"])
            output += "  const auto actual = format_dataframe(input, options);\n  actual.validate();\n"
            output += (
                "  check_format_table("
                + QUOTE(row["name"] + ": result")
                + ", actual, expected);\n"
            )
        output += (
            "  check_format_table("
            + QUOTE(row["name"] + ": input after call")
            + ", input, expected_input);\n}\n"
        )
    output += "} // namespace format_dataframe_reference\ninline void check_public_format_dataframe_reference_cases() {\n  std::string failures;\n"
    for position in included:
        output += (
            "  try { format_dataframe_reference::check_case_"
            + str(position)
            + '(); }\n  catch (const std::exception& error) { failures += std::string(error.what()) + "\\n"; }\n'
        )
    output += "  if (!failures.empty()) throw std::runtime_error(failures);\n}\n"
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    source = args.reference.read_bytes()
    if args.reference.suffix == ".gz":
        source = gzip.decompress(source)
    args.output.write_text(generate(source), encoding="utf-8", newline="\n")
