#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "minera/event_data.hpp"
#include "minera/log_conversion.hpp"

namespace {

minera::EventAttribute cell(std::string key, std::string value,
                            minera::AttributeKind kind =
                                minera::AttributeKind::kString,
                            bool is_null = false) {
  return {std::move(key), std::move(value), kind, is_null};
}

minera::EventTable sample_table() {
  minera::EventTable table;
  table.columns = {"case:id", "concept:name", "note", "nullable"};
  table.attributes.push_back(cell("source", "table"));
  table.metadata.extensions.push_back({"demo", "d", "urn:demo"});
  table.metadata.classifiers.push_back({"activity", {"concept:name"}});
  table.metadata.event_globals.push_back(cell("nullable", "", {}, true));
  table.rows = {
      {cell("case:id", "c2", minera::AttributeKind::kId),
       cell("concept:name", "A"), cell("note", ""),
       cell("nullable", "", {}, true)},
      {cell("case:id", "c1", minera::AttributeKind::kId),
       cell("concept:name", "B"), std::nullopt, std::nullopt},
      {cell("case:id", "c2", minera::AttributeKind::kId),
       cell("concept:name", "C"), cell("note", "present"), std::nullopt},
  };
  return table;
}

minera::EventTableIndexLabel index_scalar(minera::EventAttribute value) {
  minera::EventTableIndexLabel label;
  label.value = std::move(value);
  return label;
}

minera::EventTableIndexLabel index_tuple(
    std::vector<minera::EventTableIndexLabel> values) {
  minera::EventTableIndexLabel label;
  label.value = std::move(values);
  return label;
}

minera::EventAttribute index_date(std::string value) {
  minera::EventAttribute result{"date", std::move(value),
                                minera::AttributeKind::kDate};
  result.typed_value = minera::NativeDate{result.value,
                                           minera::NativeDateResolution::kNanosecond};
  return result;
}

minera::EventAttribute index_nan() {
  minera::EventAttribute result{"number", "nan", minera::AttributeKind::kFloat};
  result.typed_value = std::numeric_limits<double>::quiet_NaN();
  return result;
}

minera::EventAttribute category_scalar(std::string key, std::string value,
                                       minera::AttributeKind kind =
                                           minera::AttributeKind::kString) {
  return {std::move(key), std::move(value), kind};
}

minera::EventAttribute category_tuple(
    std::vector<minera::EventAttribute> children) {
  minera::EventAttribute result{"tuple", "", minera::AttributeKind::kString};
  result.container = minera::AttributeContainerKind::kTuple;
  result.children = std::move(children);
  return result;
}

minera::EventTableCategoricalDtype category_descriptor() {
  minera::EventTableCategoricalDtype descriptor;
  descriptor.categories_dtype = "object";
  descriptor.ordered = true;
  auto first = category_scalar("source-a", "A");
  first.first_class_role = minera::FirstClassAttributeRole::kActivity;
  first.first_class_state = minera::FirstClassAttributeState::kPresent;
  descriptor.categories.push_back(std::move(first));
  descriptor.categories.push_back(category_tuple(
      {category_scalar("left", "x"),
       category_scalar("right", "1", minera::AttributeKind::kInt)}));
  return descriptor;
}

void native_scalar_integral_construction_is_stable() {
  using Value = minera::NativeAttributeValue;
  static_assert(std::is_constructible_v<Value, std::uint32_t>);
  static_assert(std::is_constructible_v<Value, std::int32_t>);
  static_assert(std::is_constructible_v<Value, bool>);
  static_assert(std::is_constructible_v<Value, std::int64_t&>);
  static_assert(std::is_constructible_v<Value, std::int64_t&&>);
  static_assert(std::is_constructible_v<Value, std::uint64_t&>);
  static_assert(std::is_constructible_v<Value, std::uint64_t&&>);

  minera::NativeAttributeValue narrow_unsigned = std::uint32_t{17};
  assert(std::holds_alternative<std::int64_t>(narrow_unsigned));
  assert(std::get<std::int64_t>(narrow_unsigned) == 17);
  narrow_unsigned = std::uint32_t{23};
  assert(std::holds_alternative<std::int64_t>(narrow_unsigned));
  assert(std::get<std::int64_t>(narrow_unsigned) == 23);

  minera::NativeAttributeValue signed_value = std::int32_t{-7};
  assert(std::holds_alternative<std::int64_t>(signed_value));
  assert(std::get<std::int64_t>(signed_value) == -7);

  std::int64_t signed_source = -11;
  Value exact_signed_lvalue = signed_source;
  assert(std::holds_alternative<std::int64_t>(exact_signed_lvalue));
  assert(std::get<std::int64_t>(exact_signed_lvalue) == -11);
  Value exact_signed_rvalue = std::int64_t{-13};
  assert(std::holds_alternative<std::int64_t>(exact_signed_rvalue));
  assert(std::get<std::int64_t>(exact_signed_rvalue) == -13);

  std::uint64_t unsigned_source = std::numeric_limits<std::uint64_t>::max();
  Value exact_unsigned_lvalue = unsigned_source;
  assert(std::holds_alternative<std::uint64_t>(exact_unsigned_lvalue));
  assert(std::get<std::uint64_t>(exact_unsigned_lvalue) == unsigned_source);
  Value exact_unsigned = std::uint64_t{std::numeric_limits<std::uint64_t>::max()};
  assert(std::holds_alternative<std::uint64_t>(exact_unsigned));
  assert(std::get<std::uint64_t>(exact_unsigned) ==
         std::numeric_limits<std::uint64_t>::max());

  minera::NativeAttributeValue boolean = true;
  assert(std::holds_alternative<bool>(boolean));
  assert(std::get<bool>(boolean));
}

void table_metadata_is_validated_and_lazy_round_trips() {
  auto table = sample_table();
  table.dtypes = {"string", "string", "string", "boolean"};
  minera::EventTableIndex multi;
  multi.kind = "MultiIndex";
  multi.dtype = "object";
  multi.names = {std::string{"case"}, std::string{"phase"}};
  multi.labels = {
      index_tuple({index_scalar(cell("case", "c2")),
                   index_scalar(minera::EventAttribute{"phase", "", {}, true})}),
      index_tuple({index_scalar(index_nan()), index_scalar(index_date("2024-01-01"))}),
      index_tuple({index_scalar(cell("case", "c2")),
                   index_scalar(index_date("2024-01-02"))}),
  };
  table.index = multi;
  table.dtypes[2] = "category";
  table.categorical_dtypes.resize(table.columns.size());
  table.categorical_dtypes[2] = category_descriptor();
  table.validate();

  const auto lazy = minera::LazyEventTable::from_table(table);
  assert(lazy.dtypes() == table.dtypes);
  assert(lazy.index() == table.index);
  const auto materialized = lazy.materialize();
  assert(materialized.columns == table.columns);
  assert(materialized.rows == table.rows);
  assert(materialized.dtypes == table.dtypes);
  assert(materialized.index == table.index);
  assert(lazy.categorical_dtypes() == table.categorical_dtypes);
  assert(materialized.categorical_dtypes == table.categorical_dtypes);

  // A plain Index still permits tuple-valued labels; only its name arity is
  // constrained to one.
  auto plain = table;
  plain.index->kind = "Index";
  plain.index->names = {std::nullopt};
  plain.index->labels = {
      index_tuple({index_scalar(cell("left", "L")),
                   index_scalar(cell("right", "R"))}),
      index_tuple({index_scalar(cell("left", "M")),
                   index_scalar(cell("right", "N"))}),
      index_tuple({index_scalar(cell("left", "O")),
                   index_scalar(cell("right", "P"))}),
  };
  plain.validate();
}

void categorical_metadata_is_validated_semantically() {
  auto table = sample_table();
  table.dtypes = {"string", "string", "category", "boolean"};
  table.categorical_dtypes.resize(table.columns.size());
  table.categorical_dtypes[2] = category_descriptor();
  table.validate();

  // A category descriptor is optional detail for a known category dtype.
  auto unknown = table;
  unknown.categorical_dtypes[2].reset();
  unknown.validate();

  const auto expect_invalid = [](minera::EventTable candidate) {
    bool threw = false;
    try {
      candidate.validate();
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  };

  auto misaligned = table;
  misaligned.categorical_dtypes.pop_back();
  expect_invalid(misaligned);

  auto wrong_dtype = table;
  wrong_dtype.dtypes[2] = "object";
  expect_invalid(wrong_dtype);

  auto missing_categories_dtype = table;
  missing_categories_dtype.categorical_dtypes[2]->categories_dtype.clear();
  expect_invalid(missing_categories_dtype);

  auto duplicate = table;
  auto duplicate_category = category_scalar("different-key", "A");
  duplicate_category.first_class_role = minera::FirstClassAttributeRole::kResource;
  duplicate.categorical_dtypes[2]->categories.push_back(
      std::move(duplicate_category));
  expect_invalid(duplicate);

  auto null_category = table;
  null_category.categorical_dtypes[2]->categories[0].is_null = true;
  expect_invalid(null_category);

  auto nan_category = table;
  auto nan = category_scalar("nan", "nan", minera::AttributeKind::kFloat);
  nan.typed_value = std::numeric_limits<double>::quiet_NaN();
  nan_category.categorical_dtypes[2]->categories.push_back(std::move(nan));
  expect_invalid(nan_category);

  auto bool_integer_duplicate = table;
  auto truth = category_scalar("truth", "true", minera::AttributeKind::kBoolean);
  truth.typed_value = true;
  auto one = category_scalar("one", "1", minera::AttributeKind::kInt);
  one.typed_value = std::int64_t{1};
  bool_integer_duplicate.categorical_dtypes[2]->categories = {
      std::move(truth), std::move(one)};
  expect_invalid(bool_integer_duplicate);

  auto integer_float_duplicate = table;
  auto integer = category_scalar("integer", "1", minera::AttributeKind::kInt);
  integer.typed_value = std::int64_t{1};
  auto real = category_scalar("real", "1.0", minera::AttributeKind::kFloat);
  real.typed_value = 1.0;
  integer_float_duplicate.categorical_dtypes[2]->categories = {
      std::move(integer), std::move(real)};
  expect_invalid(integer_float_duplicate);

  auto typed_date_duplicate = table;
  auto normalized_date = category_scalar(
      "date-a", "stale-a", minera::AttributeKind::kDate);
  normalized_date.typed_value = minera::NativeDate{
      "2026-01-01T00:00:00.000000000Z",
      minera::NativeDateResolution::kMicrosecond};
  auto stale_date = category_scalar(
      "date-b", "stale-b", minera::AttributeKind::kDate);
  stale_date.typed_value = minera::NativeDate{
      "2026-01-01T00:00:00.000000000Z",
      minera::NativeDateResolution::kNanosecond};
  typed_date_duplicate.categorical_dtypes[2]->categories = {
      std::move(normalized_date), std::move(stale_date)};
  // NativeDate equality ignores source precision and the lexical projection
  // may be stale after normalization, so these categories are duplicates.
  expect_invalid(typed_date_duplicate);

  auto uint64_boundary = table;
  auto maximum = category_scalar("maximum", "", minera::AttributeKind::kInt);
  maximum.typed_value = std::numeric_limits<std::uint64_t>::max();
  auto rounded = category_scalar("rounded", "", minera::AttributeKind::kFloat);
  rounded.typed_value = static_cast<double>(
      std::numeric_limits<std::uint64_t>::max());
  uint64_boundary.categorical_dtypes[2]->categories = {
      std::move(maximum), std::move(rounded)};
  // The float is 2^64 after conversion and must remain distinct from
  // UINT64_MAX; comparing through double-to-integer wrapping would be wrong.
  uint64_boundary.validate();

  auto tuple_null = category_scalar("none", "", minera::AttributeKind::kString);
  tuple_null.is_null = true;
  auto tuple_null_category = category_tuple({
      std::move(tuple_null), category_scalar("number", "1", minera::AttributeKind::kInt)});
  auto tuple_other_category = category_tuple({
      category_scalar("text", "x"), category_scalar("number", "2", minera::AttributeKind::kInt)});
  auto nested_null = table;
  nested_null.categorical_dtypes[2]->categories = {
      std::move(tuple_null_category), std::move(tuple_other_category)};
  nested_null.validate();

  auto tuple_nan_child = category_scalar("nan", "nan", minera::AttributeKind::kFloat);
  tuple_nan_child.typed_value = std::numeric_limits<double>::quiet_NaN();
  auto tuple_nan_category = category_tuple({
      std::move(tuple_nan_child), category_scalar("number", "1", minera::AttributeKind::kInt)});
  auto tuple_nan_other_category = category_tuple({
      category_scalar("text", "x"), category_scalar("number", "2", minera::AttributeKind::kInt)});
  auto nested_nan = table;
  nested_nan.categorical_dtypes[2]->categories = {
      std::move(tuple_nan_category), std::move(tuple_nan_other_category)};
  nested_nan.validate();

  auto null_nan_distinct = table;
  auto null_child = category_scalar("none", "", minera::AttributeKind::kString);
  null_child.is_null = true;
  auto nan_child = category_scalar("nan", "nan", minera::AttributeKind::kFloat);
  nan_child.typed_value = std::numeric_limits<double>::quiet_NaN();
  null_nan_distinct.categorical_dtypes[2]->categories = {
      category_tuple({std::move(null_child), category_scalar("n", "1", minera::AttributeKind::kInt)}),
      category_tuple({std::move(nan_child), category_scalar("n", "1", minera::AttributeKind::kInt)})};
  null_nan_distinct.validate();

  auto tuple_numeric_duplicate = table;
  auto tuple_truth_child = category_scalar("truth", "true", minera::AttributeKind::kBoolean);
  tuple_truth_child.typed_value = true;
  auto tuple_one_child = category_scalar("one", "1", minera::AttributeKind::kInt);
  tuple_one_child.typed_value = std::int64_t{1};
  tuple_numeric_duplicate.categorical_dtypes[2]->categories = {
      category_tuple({std::move(tuple_truth_child), category_scalar("n", "1", minera::AttributeKind::kInt)}),
      category_tuple({std::move(tuple_one_child), category_scalar("n", "1", minera::AttributeKind::kInt)})};
  expect_invalid(tuple_numeric_duplicate);

  auto tuple_null_duplicate = table;
  auto null_string = category_scalar("none-string", "", minera::AttributeKind::kString);
  null_string.is_null = true;
  auto null_integer = category_scalar("none-integer", "", minera::AttributeKind::kInt);
  null_integer.is_null = true;
  tuple_null_duplicate.categorical_dtypes[2]->categories = {
      category_tuple({std::move(null_string), category_scalar("n", "1", minera::AttributeKind::kInt)}),
      category_tuple({std::move(null_integer), category_scalar("n", "1", minera::AttributeKind::kInt)})};
  expect_invalid(tuple_null_duplicate);

  auto list_category = table;
  auto list = category_scalar("list", "[x]");
  list.container = minera::AttributeContainerKind::kList;
  list.children.push_back(category_scalar("0", "x"));
  list_category.categorical_dtypes[2]->categories.push_back(std::move(list));
  expect_invalid(list_category);

  auto nested_tuple = table;
  nested_tuple.categorical_dtypes[2]->categories.push_back(category_tuple(
      {category_tuple({category_scalar("nested", "z")})}));
  nested_tuple.validate();

  auto many_distinct = table;
  many_distinct.categorical_dtypes[2]->categories.clear();
  for (std::int64_t value = 0; value < 256; ++value) {
    auto category = category_scalar("value", std::to_string(value),
                                   minera::AttributeKind::kInt);
    category.typed_value = value;
    many_distinct.categorical_dtypes[2]->categories.push_back(std::move(category));
  }
  many_distinct.validate();
}

void table_metadata_rejects_malformed_shapes() {
  const auto expect_invalid = [](minera::EventTable table) {
    bool threw = false;
    try {
      table.validate();
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  };

  auto table = sample_table();
  table.dtypes = {"only-one"};
  expect_invalid(table);

  table = sample_table();
  table.categorical_dtypes.resize(table.columns.size());
  table.categorical_dtypes[0] = std::nullopt;
  expect_invalid(table);

  table = sample_table();
  table.dtypes = {"string", "", "string", "boolean"};
  expect_invalid(table);

  table = sample_table();
  minera::EventTableIndex index;
  index.labels.resize(2);
  table.index = index;
  expect_invalid(table);

  table = sample_table();
  index = {};
  index.kind.clear();
  index.labels.resize(table.rows.size());
  table.index = index;
  expect_invalid(table);

  table = sample_table();
  index = {};
  index.dtype.clear();
  index.labels.resize(table.rows.size());
  table.index = index;
  expect_invalid(table);

  table = sample_table();
  index = {};
  index.names.clear();
  index.labels.resize(table.rows.size());
  table.index = index;
  expect_invalid(table);

  table = sample_table();
  index = {};
  index.names = {std::string{"a"}, std::string{"b"}};
  index.labels.resize(table.rows.size());
  table.index = index;
  expect_invalid(table);

  table = sample_table();
  index = {};
  index.kind = "MultiIndex";
  index.names.clear();
  table.index = index;
  expect_invalid(table);

  table = sample_table();
  index = {};
  index.kind = "MultiIndex";
  index.names = {std::string{"a"}, std::string{"b"}};
  index.labels = {
      index_scalar(cell("a", "scalar")),
      index_tuple({index_scalar(cell("a", "one"))}),
      index_tuple({index_scalar(cell("a", "one")),
                   index_scalar(cell("b", "two"))}),
  };
  table.index = index;
  expect_invalid(table);

  bool scan_threw = false;
  try {
    minera::EventTableIndex mismatched;
    mismatched.labels.resize(2);
    const minera::LazyEventTable lazy(
        table.columns,
        [](const auto& consume) {
          consume(minera::EventTable::Row(4));
        },
        {}, {}, std::nullopt, {}, mismatched);
    (void)lazy.materialize();
  } catch (const std::invalid_argument&) {
    scan_threw = true;
  }
  assert(scan_threw);
}

void table_validation_and_null_contract() {
  auto table = sample_table();
  table.validate();
  const auto stream = minera::event_table_to_event_stream(table);
  assert(stream.events.size() == 3);
  assert(stream.events[0].fields.size() == 4);
  assert(stream.events[0].field("nullable")->is_null);
  assert(stream.events[0].field("note")->value.empty());
  assert(stream.events[1].field("note") == nullptr);

  minera::LogConversionOptions remove_nulls;
  remove_nulls.remove_null_fields = true;
  const auto filtered =
      minera::event_table_to_event_stream(table, remove_nulls);
  assert(filtered.events[0].field("nullable") == nullptr);
  assert(filtered.events[0].field("note") != nullptr);

  auto duplicate = table;
  duplicate.columns[1] = duplicate.columns[0];
  bool threw = false;
  try {
    duplicate.validate();
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  auto short_row = table;
  short_row.rows[0].pop_back();
  threw = false;
  try {
    short_row.validate();
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void table_prefixes_infer_only_known_extensions() {
  minera::EventTable table;
  table.columns = {"concept:name", "time:timestamp", "org:resource",
                   "lifecycle:transition", "artifactlifecycle:transition",
                   "micro:trace", "semantic:concept", "swcomm:message",
                   "swevent:type", "swtelemetry:value", "case:id",
                   "custom:value"};
  table.rows.push_back(
      {cell("concept:name", "A"), cell("time:timestamp", "2026-01-01T00:00:00Z",
                                       minera::AttributeKind::kDate),
       cell("org:resource", "alice"), cell("lifecycle:transition", "complete"),
       cell("artifactlifecycle:transition", "complete"),
       cell("micro:trace", "x"), cell("semantic:concept", "y"),
       cell("swcomm:message", "z"), cell("swevent:type", "event"),
       cell("swtelemetry:value", "1"),
       cell("case:id", "c1", minera::AttributeKind::kId),
       cell("custom:value", "x")});
  const auto stream = minera::event_table_to_event_stream(table);
  assert(stream.metadata.extensions.size() == 10);
  assert(stream.metadata.extensions[0].prefix == "concept");
  assert(stream.metadata.extensions[1].prefix == "time");
  assert(stream.metadata.extensions[2].prefix == "org");
  assert(stream.metadata.extensions[3].prefix == "lifecycle");
  assert(stream.metadata.extensions[4].prefix == "artifactlifecycle");
  assert(stream.metadata.extensions[5].prefix == "micro");
  assert(stream.metadata.extensions[6].prefix == "semantic");
  assert(stream.metadata.extensions[7].prefix == "swcomm");
  assert(stream.metadata.extensions[8].prefix == "swevent");
  assert(stream.metadata.extensions[9].prefix == "swtelemetry");

  table.metadata.extensions.push_back({"declared", "org", "urn:declared"});
  const auto authoritative = minera::event_table_to_event_stream(table);
  assert(authoritative.metadata.extensions.size() == 10);
  const auto org = std::find_if(
      authoritative.metadata.extensions.begin(),
      authoritative.metadata.extensions.end(),
      [](const auto& extension) { return extension.prefix == "org"; });
  assert(org != authoritative.metadata.extensions.end());
  assert(org->name == "declared");
  assert(org->uri == "urn:declared");
}

void empty_column_name_is_a_literal_schema_key() {
  minera::EventTable table;
  table.columns = {"", "concept:name"};
  table.rows = {{cell("", "case-1"), cell("concept:name", "A")},
                {cell("", "case-2"), cell("concept:name", "B")}};
  table.attributes = {cell("source", "empty-column")};
  table.metadata.properties = {cell("format", "literal")};
  table.dtypes = {"object", "string"};
  minera::EventTableIndex index;
  index.names = {std::string{"source-row"}};
  index.labels = {index_scalar(cell("source-row", "0")),
                  index_scalar(cell("source-row", "1"))};
  table.index = index;

  table.validate();
  assert(table.column_index("") == 0);
  const auto stream = minera::event_table_to_event_stream(table);
  assert(stream.events.size() == 2);
  assert(stream.events[0].field("") != nullptr);
  assert(stream.events[0].field("")->key.empty());
  assert(stream.events[0].field("")->value == "case-1");

  const auto lazy = minera::LazyEventTable::from_table(table);
  assert(lazy.columns() == table.columns);
  const auto materialized = lazy.materialize();
  assert(materialized.columns == table.columns);
  assert(materialized.rows == table.rows);
  assert(materialized.attributes == table.attributes);
  assert(materialized.metadata == table.metadata);
  assert(materialized.dtypes == table.dtypes);
  assert(materialized.index == table.index);

  const minera::EventDataView lazy_view(lazy);
  const auto generic_materialized = minera::event_data_to_table(lazy_view);
  assert(generic_materialized.columns == table.columns);
  assert(generic_materialized.rows == table.rows);
  assert(generic_materialized.dtypes == table.dtypes);
  assert(generic_materialized.index == table.index);

  auto duplicate = table;
  duplicate.columns[1].clear();
  bool rejected = false;
  try {
    duplicate.validate();
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  rejected = false;
  try {
    (void)minera::LazyEventTable(
        {"", ""}, [](const auto&) {});
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void lazy_scan_is_restartable_and_bounded() {
  auto table = sample_table();
  const auto lazy = minera::LazyEventTable::from_table(table);
  assert(lazy.valid());
  assert(lazy.row_count() == 3);
  std::size_t first = 0;
  lazy.scan([&](const auto&) { ++first; });
  std::size_t second = 0;
  lazy.scan([&](const auto&) { ++second; });
  assert(first == 3 && second == 3);
  assert(lazy.materialize() .rows == table.rows);

  bool threw = false;
  try {
    (void)lazy.materialize(2);
  } catch (const std::length_error&) {
    threw = true;
  }
  assert(threw);

  minera::LazyEventTable malformed(
      table.columns,
      [](const auto& consume) {
        minera::EventTable::Row row;
        consume(row);
      });
  threw = false;
  try {
    (void)malformed.materialize();
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void invalid_default_lazy_table_is_rejected() {
  const minera::LazyEventTable invalid;
  assert(!invalid.valid());

  bool threw = false;
  try {
    invalid.scan([](const auto&) {});
  } catch (const std::invalid_argument& error) {
    threw = true;
    assert(std::string(error.what()) ==
           "lazy event table: scanner is empty");
  }
  assert(threw);

  threw = false;
  try {
    (void)invalid.materialize();
  } catch (const std::invalid_argument& error) {
    threw = true;
    assert(std::string(error.what()) ==
           "lazy event table: scanner is empty");
  }
  assert(threw);
}

void lazy_inputs_apply_null_cleanup_across_generic_paths() {
  const auto table = sample_table();
  const auto lazy = minera::LazyEventTable::from_table(table);
  const minera::EventDataView view(lazy);
  minera::LogConversionOptions options;
  options.remove_null_fields = true;

  const auto converted_table = minera::event_data_to_table(view, options);
  assert(converted_table.rows.size() == table.rows.size());
  assert(converted_table.columns == table.columns);
  assert(converted_table.attributes == table.attributes);
  assert(converted_table.metadata == table.metadata);
  for (const auto& row : converted_table.rows) {
    for (const auto& cell : row)
      assert(!cell || !cell->is_null);
  }

  const minera::EventDataView eager_view(table);
  const auto direct_stream =
      minera::event_table_to_event_stream(table, options);
  const auto eager_cleaned = minera::event_data_to_table(eager_view, options);
  assert(eager_cleaned.columns == table.columns);
  assert(eager_cleaned.attributes == table.attributes);
  assert(eager_cleaned.metadata == table.metadata);
  const auto eager_stream =
      minera::event_data_to_event_stream(eager_view, options);
  assert(eager_stream.attributes == table.attributes);
  assert(eager_stream.metadata == direct_stream.metadata);

  const auto converted_stream =
      minera::event_data_to_event_stream(view, options);
  assert(converted_stream.attributes == table.attributes);
  assert(converted_stream.metadata == direct_stream.metadata);
  assert(converted_stream.metadata.extensions.size() == 2);
  assert(converted_stream.metadata.extensions[0].prefix == "d");
  assert(converted_stream.metadata.extensions[1].prefix == "concept");
  for (const auto& event : converted_stream.events)
    assert(event.field("nullable") == nullptr);

  // Returning a lazy form keeps the source restartable while filtering null
  // cells when its rows are consumed.
  const auto filtered_lazy = minera::event_data_to_lazy_table(view, options);
  assert(filtered_lazy.valid());
  assert(filtered_lazy.row_count() == table.rows.size());
  const auto filtered_materialized = filtered_lazy.materialize();
  assert(filtered_materialized.columns == table.columns);
  assert(filtered_materialized.attributes == table.attributes);
  assert(filtered_materialized.metadata == table.metadata);
  for (const auto& row : filtered_materialized.rows) {
    for (const auto& cell : row)
      assert(!cell || !cell->is_null);
  }

  options.max_rows_to_materialize = 2;
  const auto capped_lazy = minera::event_data_to_lazy_table(view, options);
  bool threw = false;
  try {
    (void)capped_lazy.materialize();
  } catch (const std::length_error&) {
    threw = true;
  }
  assert(threw);

  threw = false;
  try {
    (void)minera::event_data_to_table(view, options);
  } catch (const std::length_error& error) {
    threw = true;
    assert(std::string(error.what()) ==
           "lazy event table: row materialization cap exceeded");
  }
  assert(threw);
}

void tagged_views_and_compatibility_aliases() {
  auto table = sample_table();
  minera::LogConversionTable old_name = table;
  minera::ConvertedEventStream stream =
      minera::event_table_to_event_stream(old_name);
  minera::EventDataView table_view(table);
  assert(table_view.kind() == minera::EventDataKind::kEventTable);
  const auto round_trip = minera::event_data_to_table(table_view);
  assert(round_trip.columns == table.columns);
  assert(round_trip.rows == table.rows);

  minera::EventDataView stream_view(stream);
  assert(minera::event_data_to_event_stream(stream_view).events.size() == 3);
  const auto lazy = minera::event_data_to_lazy_table(table_view);
  minera::EventDataView lazy_view(lazy);
  assert(minera::event_data_to_table(lazy_view).rows == table.rows);
}

}  // namespace

int main() {
  native_scalar_integral_construction_is_stable();
  table_validation_and_null_contract();
  table_metadata_is_validated_and_lazy_round_trips();
  categorical_metadata_is_validated_semantically();
  table_metadata_rejects_malformed_shapes();
  table_prefixes_infer_only_known_extensions();
  empty_column_name_is_a_literal_schema_key();
  lazy_scan_is_restartable_and_bounded();
  invalid_default_lazy_table_is_rejected();
  lazy_inputs_apply_null_cleanup_across_generic_paths();
  tagged_views_and_compatibility_aliases();
}
