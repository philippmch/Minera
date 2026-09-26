#pragma once

#include "../root_cause_trace_key.hpp"
#include "minera/dataframe_format_json.hpp"
#include "minera/dfg_dataframe_json.hpp"
#include "minera/event_table_json.hpp"
#include "minera/log_conversion.hpp"
#include "minera/log_util_json.hpp"
#include "minera/root_cause.hpp"
#include "minera/xes_export.hpp"

#include <cassert>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>

namespace minera::tests {

static_assert(std::variant_size_v<NativeAttributeValue> == 7);
static_assert(std::is_same_v<std::variant_alternative_t<0, NativeAttributeValue>,
                             std::monostate>);
static_assert(std::is_same_v<std::variant_alternative_t<1, NativeAttributeValue>,
                             std::int64_t>);
static_assert(std::is_same_v<std::variant_alternative_t<2, NativeAttributeValue>,
                             double>);
static_assert(std::is_same_v<std::variant_alternative_t<3, NativeAttributeValue>,
                             bool>);
static_assert(std::is_same_v<std::variant_alternative_t<4, NativeAttributeValue>,
                             NativeDate>);
static_assert(std::is_same_v<std::variant_alternative_t<5, NativeAttributeValue>,
                             std::uint64_t>);
static_assert(std::is_same_v<std::variant_alternative_t<6, NativeAttributeValue>,
                             NativeNA>);
static_assert(static_cast<int>(AttributeKind::kBytes) == 6);
static_assert(static_cast<int>(AttributeKind::kNA) == 7);

inline bool native_na_request_rejected(std::string cell) {
  const auto request =
      std::string(R"({"table":{"columns":["case:concept:name","concept:name","time:timestamp","payload"],"rows":[[{"value":"case-1"},{"value":"A"},{"kind":"date","value":"2024-01-01T00:00:00Z"},{"key":"payload",)") +
      std::move(cell) + R"(}]]}})";
  try {
    (void)format_dataframe_json(request);
  } catch (const std::invalid_argument&) {
    return true;
  }
  return false;
}

inline void native_na_model_values_and_key_identity() {
  auto value = make_native_na_attribute("payload");
  assert(value.kind == AttributeKind::kNA);
  assert(!value.is_null);
  assert(value.value == "<NA>");
  assert(std::holds_alternative<NativeNA>(value.typed_value));

  const auto copy = value;
  assert(copy == value);
  assert(root_cause_scalar_keys_equal(value, copy));
  assert(root_cause_scalar_key_fingerprint(value) ==
         root_cause_scalar_key_fingerprint(copy));

  EventAttribute tuple{"payload", "", AttributeKind::kString};
  tuple.container = AttributeContainerKind::kTuple;
  tuple.children.push_back(value);
  const auto tuple_copy = tuple;
  assert(root_cause_scalar_keys_equal(tuple, tuple_copy));
  assert(root_cause_scalar_key_fingerprint(tuple) ==
         root_cause_scalar_key_fingerprint(tuple_copy));

  const EventAttribute none{"payload", "", AttributeKind::kString, true};
  const EventAttribute nan = [] {
    EventAttribute result{"payload", "NaN", AttributeKind::kFloat};
    result.typed_value = std::numeric_limits<double>::quiet_NaN();
    return result;
  }();
  const EventAttribute literal_na{"payload", "NA", AttributeKind::kString};
  const EventAttribute literal_marker{"payload", "<NA>",
                                      AttributeKind::kString};
  for (const auto* other : {&none, &nan, &literal_na, &literal_marker}) {
    assert(!root_cause_scalar_keys_equal(value, *other));
    assert(root_cause_scalar_key_fingerprint(value) !=
           root_cause_scalar_key_fingerprint(*other));
  }

  EventAttribute tuple_literal = tuple;
  tuple_literal.children.front() = literal_marker;
  assert(!root_cause_scalar_keys_equal(tuple, tuple_literal));
  assert(root_cause_scalar_key_fingerprint(tuple) !=
         root_cause_scalar_key_fingerprint(tuple_literal));

  bool unordered = false;
  try {
    (void)native_scalar_key_compare(value, copy);
  } catch (const std::invalid_argument&) {
    unordered = true;
  }
  assert(unordered);
}

inline void native_na_model_json_contract() {
  const auto current = native_na_request_rejected(
      R"("kind":"na","value":"<NA>","typed_value":"NA")");
  assert(!current);
  const auto legacy = format_dataframe_json(
      R"({"table":{"columns":["case:concept:name","concept:name","time:timestamp","payload"],"rows":[[{"value":"case-1"},{"value":"A"},{"kind":"date","value":"2024-01-01T00:00:00Z"},{"key":"payload","kind":"na","value":"<NA>"}]]}})");
  assert(legacy.body.find("\"kind\":\"na\"") != std::string::npos);
  assert(legacy.body.find("\"value\":\"<NA>\"") != std::string::npos);
  assert(legacy.body.find("\"typed_value\":\"NA\"") !=
         std::string::npos);

  const auto omitted = format_dataframe_json(
      R"({"table":{"columns":["case:concept:name","concept:name","time:timestamp","payload"],"rows":[[{"value":"case-1"},{"value":"A"},{"kind":"date","value":"2024-01-01T00:00:00Z"},{"key":"payload","kind":"na"}]]}})");
  assert(omitted.body.find("\"kind\":\"na\"") != std::string::npos);
  assert(omitted.body.find("\"value\":\"<NA>\"") != std::string::npos);
  assert(omitted.body.find("\"typed_value\":\"NA\"") !=
         std::string::npos);

  const auto literals = format_dataframe_json(
      R"({"table":{"columns":["case:concept:name","concept:name","time:timestamp","plain_na","plain_marker"],"rows":[[{"value":"case-1"},{"value":"A"},{"kind":"date","value":"2024-01-01T00:00:00Z"},{"kind":"string","value":"NA"},{"kind":"string","value":"<NA>"}]]}})");
  const auto assert_literal_scalar = [&](std::string_view value) {
    const auto value_field = "\"value\":\"" + std::string(value) + "\"";
    const auto value_position = literals.body.find(value_field);
    assert(value_position != std::string::npos);
    const auto object_start = literals.body.rfind('{', value_position);
    const auto object_end = literals.body.find('}', value_position);
    assert(object_start != std::string::npos && object_end != std::string::npos);
    const auto object = literals.body.substr(object_start, object_end - object_start);
    assert(object.find("\"kind\":\"string\"") != std::string::npos);
    assert(object.find("\"container\":\"scalar\"") != std::string::npos);
    // This JSON writer omits the null flag for present values.
    assert(object.find("\"null\":true") == std::string::npos);
    assert(object.find("\"typed_value\":\"NA\"") == std::string::npos);
  };
  assert_literal_scalar("NA");
  assert_literal_scalar("<NA>");

  const auto tuple_leaf = format_dataframe_json(
      R"({"table":{"columns":["case:concept:name","concept:name","time:timestamp","tuple_payload"],"rows":[[{"value":"case-1"},{"value":"A"},{"kind":"date","value":"2024-01-01T00:00:00Z"},{"key":"tuple_payload","kind":"string","container":"tuple","children":[{"key":"0","kind":"na","value":"<NA>","typed_value":"NA"}]}]]}})");
  assert(tuple_leaf.body.find("\"container\":\"tuple\"") !=
         std::string::npos);
  assert(tuple_leaf.body.find("\"kind\":\"na\"") != std::string::npos);
  assert(tuple_leaf.body.find("\"typed_value\":\"NA\"") !=
         std::string::npos);

  for (const auto& bad : {
           R"("kind":"na","null":true)",
           R"("kind":"na","value":"NA")",
           R"("kind":"na","typed_value":null)",
           R"("kind":"na","typed_value":"nan")",
           R"("kind":"na","typed_value":1)",
           R"("kind":"na","container":"tuple","typed_value":"NA")",
           R"("kind":"na","children":[],"typed_value":"NA")",
           R"("kind":"na","scalar_wrapper":true,"typed_value":"NA")",
           R"("kind":"string","value":"NA","typed_value":"NA")",
           R"("kind":"na","typed_value":"NA","first_class_role":"activity","first_class_state":"null","first_class_source_key":"concept:name")"}) {
    assert(native_na_request_rejected(bad));
  }

  EventTable table;
  table.columns = {"payload"};
  table.rows = {{make_native_na_attribute("payload")}};
  const auto serialized = serialize_event_table_json(table);
  assert(serialized.find("\"kind\":\"na\"") != std::string::npos);
  assert(serialized.find("\"value\":\"<NA>\"") != std::string::npos);
  assert(serialized.find("\"typed_value\":\"NA\"") !=
         std::string::npos);

  EventTable categories;
  categories.columns = {"payload"};
  categories.rows = {{make_native_na_attribute("payload")}};
  categories.dtypes = {"category"};
  EventAttribute tuple_category{"payload", "", AttributeKind::kString};
  tuple_category.container = AttributeContainerKind::kTuple;
  tuple_category.children = {make_native_na_attribute("0")};
  categories.categorical_dtypes = {EventTableCategoricalDtype{
      "object", {tuple_category,
                 EventAttribute{"payload", "<NA>", AttributeKind::kString}},
      false}};
  categories.validate();
  categories.categorical_dtypes.front()->categories = {
      make_native_na_attribute("payload")};
  bool top_level_na_category_rejected = false;
  try {
    categories.validate();
  } catch (const std::invalid_argument&) {
    top_level_na_category_rejected = true;
  }
  assert(top_level_na_category_rejected);
}

inline void native_na_model_first_class_and_generic_writers() {
  Event event;
  event.activity = "<NA>";
  event.activity_state = FirstClassAttributeState::kPresent;
  event.activity_source_key = "custom:activity";
  auto selected = make_native_na_attribute("custom:activity");
  selected.first_class_role = FirstClassAttributeRole::kActivity;
  selected.first_class_state = FirstClassAttributeState::kPresent;
  selected.first_class_source_key = "custom:activity";
  event.activity_attribute = selected;

  const auto projected = native_event_attribute(event, "custom:activity");
  assert(projected);
  assert(projected->kind == AttributeKind::kNA);
  assert(std::holds_alternative<NativeNA>(projected->typed_value));

  EventLog log;
  Trace trace;
  trace.case_id = "case-1";
  trace.case_id_state = FirstClassAttributeState::kPresent;
  trace.events.push_back(event);
  log.traces.push_back(trace);
  const auto stream = event_log_to_event_stream(log);
  assert(stream.events.size() == 1);
  const auto* activity = stream.events.front().field("custom:activity");
  assert(activity && activity->kind == AttributeKind::kNA);
  assert(std::holds_alternative<NativeNA>(activity->typed_value));

  LogConversionResult conversion;
  conversion.variant = LogConversionVariant::kToEventStream;
  conversion.stream = stream;
  const auto conversion_json = log_conversion_json(conversion);
  assert(conversion_json.find("\"kind\":\"na\"") != std::string::npos);
  assert(conversion_json.find("\"typed_value\":\"NA\"") !=
         std::string::npos);
  assert(conversion_json.find("\"value\":\"<NA>\"") !=
         std::string::npos);

  const auto log_util_json =
      log_prefix_json(log, LogUtilityVariant::kFixedPrefixes);
  assert(log_util_json.find("\"kind\":\"na\"") != std::string::npos);
  assert(log_util_json.find("\"typed_value\":\"NA\"") !=
         std::string::npos);

  DfgPartialOrderResult partial;
  partial.backend = DfgTableBackend::kPandas;
  DfgPartialOrderPair pair;
  pair.source.attributes.push_back(selected);
  partial.pairs.push_back(std::move(pair));
  const auto dfg_json = serialize_dfg_partial_order_json(partial);
  assert(dfg_json.find("\"kind\":\"na\"") != std::string::npos);
  assert(dfg_json.find("\"typed_value\":\"NA\"") !=
         std::string::npos);
  assert(dfg_json.find("\"value\":\"<NA>\"") !=
         std::string::npos);

  EventLog xes_log;
  Trace xes_trace;
  xes_trace.case_id = "case-1";
  Event xes_event;
  xes_event.activity = "<NA>";
  xes_event.activity_state = FirstClassAttributeState::kPresent;
  xes_event.activity_attribute = make_native_na_attribute("concept:name");
  xes_trace.events.push_back(std::move(xes_event));
  xes_log.traces.push_back(std::move(xes_trace));
  bool rejected = false;
  try {
    (void)to_xes(xes_log);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

inline void test_native_na_model() {
  native_na_model_values_and_key_identity();
  native_na_model_json_contract();
  native_na_model_first_class_and_generic_writers();
}

}  // namespace minera::tests
