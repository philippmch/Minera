#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

#include "minera/json_export.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/log_conversion.hpp"
#include "minera/log_util_json.hpp"
#include "minera/timestamp.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "dataframe_conversion_metadata_reference_cases.hpp"
#include "conversion_case_grouping_reference_cases.hpp"
#include "container_case_grouping_reference_cases.hpp"

namespace {

minera::EventLog attributed_log() {
  minera::EventLog log;
  log.attributes.push_back({"source", "unit"});
  minera::Trace trace;
  trace.case_id = "case-1";
  trace.attributes.push_back({"region", "west"});
  trace.events.push_back(
      {"case-1", "A", "2026-01-01T00:00:00Z", "alice", 0});
  trace.events.back().attributes.push_back(
      {"cost", "1.5", minera::AttributeKind::kFloat});
  trace.events.push_back(
      {"case-1", "B", "2026-01-01T00:01:00Z", "bob", 1});
  trace.events.back().lifecycle = "complete";
  log.traces.push_back(std::move(trace));
  return log;
}

void stream_round_trip_preserves_typed_attributes() {
  auto source = attributed_log();
  source.metadata.extensions.push_back(
      {"demo", "d", "urn:demo"});
  source.metadata.classifiers.push_back({"activity", {"concept:name"}});
  source.metadata.event_globals.push_back(
      {"nullable", "", minera::AttributeKind::kString, true});
  source.traces[0].events[0].attributes.push_back(
      {"case:region", "event-value"});
  const auto stream = minera::event_log_to_event_stream(source);
  assert(stream.events.size() == 2);
  assert(stream.attributes.size() == 1);
  assert(stream.events[0].field("concept:name")->value == "A");
  assert(stream.events[0].field("case:concept:name")->value == "case-1");
  assert(stream.events[0].field("case:region")->value == "west");
  assert(stream.events[0].field("cost")->kind ==
         minera::AttributeKind::kFloat);

  const auto restored = minera::event_stream_to_event_log(stream);
  assert(restored.trace_count() == 1 && restored.event_count() == 2);
  assert(restored.traces[0].case_id == "case-1");
  assert(*restored.traces[0].attribute("region") == "west");
  assert(restored.traces[0].events[0].activity == "A");
  assert(restored.traces[0].events[0].timestamp ==
         "2026-01-01T00:00:00Z");
  assert(restored.traces[0].events[0].resource == "alice");
  assert(restored.traces[0].events[0].attributes[0].kind ==
         minera::AttributeKind::kFloat);
  assert(restored.traces[0].events[1].lifecycle == "complete");
  assert(*restored.attribute("source") == "unit");
  assert(restored.metadata == source.metadata);
}

void empty_prefix_and_trace_properties_follow_flattening_contract() {
  auto source = attributed_log();
  source.traces[0].properties.push_back({"internal", "not-an-event"});
  minera::LogConversionOptions options;
  options.case_attribute_prefix.clear();
  const auto stream = minera::event_log_to_event_stream(source, options);
  assert(stream.events[0].field("internal") == nullptr);
  const auto restored = minera::event_stream_to_event_log(stream, options);
  assert(restored.traces.size() == 1);
  assert(restored.traces[0].properties.empty());
  assert(restored.traces[0].events[0].activity == "A");
  assert(restored.traces[0].events[0].attributes.empty());
  assert(restored.traces[0].attribute("concept:name") == nullptr);
  assert(restored.traces[0].attribute("cost") != nullptr);
}

void stream_grouping_is_first_seen_and_first_event_wins() {
  minera::ConvertedEventStream stream;
  const auto record = [](std::string activity, std::string case_id,
                         std::string region) {
    minera::LogConversionRecord result;
    result.fields = {
        {"concept:name", std::move(activity)},
        {"case:concept:name", std::move(case_id)},
        {"case:region", std::move(region)},
    };
    return result;
  };
  stream.events.push_back(record("A", "c2", "west"));
  stream.events.push_back(record("B", "c1", "north"));
  stream.events.push_back(record("C", "c2", "east"));

  const auto log = minera::event_stream_to_event_log(stream);
  assert(log.trace_count() == 2);
  assert(log.traces[0].case_id == "c2");
  assert(log.traces[0].events[0].activity == "A");
  assert(log.traces[0].events[1].activity == "C");
  assert(*log.traces[0].attribute("region") == "west");
  assert(log.traces[1].case_id == "c1");

  stream.events[0].fields.erase(stream.events[0].fields.begin() + 1);
  bool threw = false;
  try {
    (void)minera::event_stream_to_event_log(stream);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  minera::ConvertedEventStream typed_ids;
  auto string_id = record("S", "1", "west");
  auto integer_id = record("I", "1", "west");
  integer_id.fields[1].kind = minera::AttributeKind::kInt;
  typed_ids.events = {std::move(string_id), std::move(integer_id)};
  const auto typed_log = minera::event_stream_to_event_log(typed_ids);
  assert(typed_log.trace_count() == 2);
  assert(typed_log.traces[0].events[0].activity == "S");
  assert(typed_log.traces[1].events[0].activity == "I");
}

void stream_without_case_attributes_uses_stable_trace_indexes() {
  auto source = attributed_log();
  source.traces[0].events[0].attributes.push_back(
      {"case:diagnostic", "preserved"});
  auto second = source.traces[0];
  second.case_id = "case-2";
  for (auto& event : second.events) event.case_id = second.case_id;
  source.traces.push_back(std::move(second));
  minera::LogConversionOptions options;
  options.include_case_attributes = false;
  const auto stream = minera::event_log_to_event_stream(source, options);
  assert(stream.events[0].field("case:concept:name")->value == "0");
  assert(stream.events[2].field("case:concept:name")->value == "1");
  const auto restored = minera::event_stream_to_event_log(stream, options);
  assert(restored.trace_count() == 2);
  assert(restored.traces[0].case_id == "0");
  assert(restored.traces[1].case_id == "1");
  assert(restored.traces[0].attributes.empty());
  assert(*restored.traces[0].events[0].attribute("case:diagnostic") ==
         "preserved");
}

void custom_case_glue_and_prefix_round_trip() {
  auto source = attributed_log();
  minera::LogConversionOptions options;
  options.case_id_key = "trace-id";
  options.case_attribute_prefix = "trace.";
  const auto stream = minera::event_log_to_event_stream(source, options);
  assert(stream.events[0].field("trace-id")->value == "case-1");
  assert(stream.events[0].field("trace.region")->value == "west");
  const auto restored = minera::event_stream_to_event_log(stream, options);
  assert(restored.traces.size() == 1);
  assert(restored.traces[0].case_id == "case-1");
  assert(*restored.traces[0].attribute("region") == "west");
  assert(restored.traces[0].events[0].activity == "A");
}

void source_keys_and_missing_case_ids_survive_flat_adapters() {
  minera::EventLog source;

  minera::Trace custom;
  custom.case_id = "business-1";
  custom.case_id_state = minera::FirstClassAttributeState::kPresent;
  custom.case_id_source_key = "business.case";
  custom.attributes.push_back({"concept:name", "alias-must-not-win"});
  minera::Event custom_event;
  custom_event.activity = "Review";
  custom_event.activity_state = minera::FirstClassAttributeState::kPresent;
  custom_event.activity_source_key = "business.activity";
  custom_event.timestamp = "2026-01-01T00:00:00Z";
  custom_event.timestamp_state = minera::FirstClassAttributeState::kPresent;
  custom_event.timestamp_source_key = "business.when";
  custom_event.resource = "alice";
  custom_event.resource_state = minera::FirstClassAttributeState::kPresent;
  custom_event.resource_source_key = "business.owner";
  custom_event.lifecycle = "start";
  custom_event.lifecycle_state = minera::FirstClassAttributeState::kPresent;
  custom_event.lifecycle_source_key = "business.lifecycle";
  custom.events.push_back(std::move(custom_event));
  source.traces.push_back(std::move(custom));

  minera::Trace alternate;
  alternate.case_id = "business-2";
  alternate.case_id_state = minera::FirstClassAttributeState::kPresent;
  alternate.case_id_source_key = "order.case";
  minera::Event alternate_event;
  alternate_event.activity = "Approve";
  alternate_event.activity_state = minera::FirstClassAttributeState::kPresent;
  alternate_event.activity_source_key = "order.activity";
  alternate_event.timestamp = "2026-01-02T00:00:00Z";
  alternate_event.timestamp_state = minera::FirstClassAttributeState::kPresent;
  alternate_event.timestamp_source_key = "order.when";
  alternate_event.resource = "bob";
  alternate_event.resource_state = minera::FirstClassAttributeState::kPresent;
  alternate_event.resource_source_key = "order.owner";
  alternate_event.lifecycle = "complete";
  alternate_event.lifecycle_state = minera::FirstClassAttributeState::kPresent;
  alternate_event.lifecycle_source_key = "order.lifecycle";
  alternate_event.attributes.push_back(
      {"concept:name", "canonical-alias-must-not-win"});
  alternate.events.push_back(std::move(alternate_event));
  source.traces.push_back(std::move(alternate));

  minera::Trace missing;
  minera::Event missing_event;
  missing_event.activity = "Unassigned";
  missing_event.activity_state = minera::FirstClassAttributeState::kPresent;
  missing.events.push_back(std::move(missing_event));
  source.traces.push_back(std::move(missing));

  const auto stream = minera::event_log_to_event_stream(source);
  assert(stream.metadata == source.metadata);
  const auto* business_activity =
      stream.events[0].field("business.activity");
  assert(business_activity != nullptr);
  assert(business_activity->first_class_role ==
         minera::FirstClassAttributeRole::kActivity);
  assert(business_activity->first_class_source_key == "business.activity");
  assert(stream.events[0].field("business.owner") != nullptr);
  assert(stream.events[0].field("business.lifecycle") != nullptr);
  const auto* order_activity = stream.events[1].field("order.activity");
  assert(order_activity != nullptr);
  assert(order_activity->first_class_role ==
         minera::FirstClassAttributeRole::kActivity);
  assert(order_activity->first_class_source_key == "order.activity");
  for (const auto& record : stream.events)
    for (const auto& field : record.fields)
      assert(!field.key.starts_with("@@"));
  assert(stream.events[0].field("business.case") == nullptr);
  const auto* case_glue = stream.events[0].field("case:concept:name");
  assert(case_glue != nullptr && case_glue->value == "business-1");
  assert(case_glue->first_class_role ==
         minera::FirstClassAttributeRole::kCaseId);
  assert(case_glue->first_class_source_key == "business.case");

  const auto restored = minera::event_stream_to_event_log(stream);
  assert(restored.traces.size() == 3);
  assert(restored.traces[0].case_id == "business-1");
  assert(restored.traces[0].case_id_source_key == "business.case");
  assert(restored.traces[0].events[0].activity == "Review");
  assert(restored.traces[0].events[0].activity_source_key ==
         "business.activity");
  assert(restored.traces[0].events[0].timestamp_source_key == "business.when");
  assert(restored.traces[0].events[0].resource == "alice");
  assert(restored.traces[0].events[0].resource_source_key == "business.owner");
  assert(restored.traces[0].events[0].lifecycle == "start");
  assert(restored.traces[0].events[0].lifecycle_source_key ==
         "business.lifecycle");
  assert(restored.traces[1].case_id == "business-2");
  assert(restored.traces[1].case_id_source_key == "order.case");
  assert(restored.traces[1].events[0].activity == "Approve");
  assert(restored.traces[1].events[0].activity_source_key == "order.activity");
  assert(restored.traces[1].events[0].timestamp_source_key == "order.when");
  assert(restored.traces[1].events[0].resource_source_key == "order.owner");
  assert(restored.traces[1].events[0].lifecycle_source_key == "order.lifecycle");
  assert(restored.traces[2].case_id.empty());
  assert(restored.traces[2].case_id_state ==
         minera::FirstClassAttributeState::kMissing);

  const auto table = minera::event_stream_to_table(stream);
  assert(std::none_of(table.columns.begin(), table.columns.end(),
                      [](const auto& column) {
                        return column.starts_with("@@");
                      }));
  const auto table_round = minera::event_table_to_event_log(table);
  assert(table_round.traces.size() == 3);
  assert(table_round.traces[0].case_id_source_key == "business.case");
  assert(table_round.traces[0].events[0].activity_source_key ==
         "business.activity");
  assert(table_round.traces[0].events[0].resource_source_key ==
         "business.owner");
  assert(table_round.traces[1].case_id_source_key == "order.case");
  assert(table_round.traces[1].events[0].activity_source_key ==
         "order.activity");
  assert(table_round.traces[1].events[0].timestamp_source_key == "order.when");
  assert(table_round.traces[1].events[0].resource_source_key == "order.owner");
  assert(table_round.traces[1].events[0].lifecycle_source_key ==
         "order.lifecycle");
  assert(table_round.traces[2].case_id_state ==
         minera::FirstClassAttributeState::kMissing);
}

void table_json_keeps_recursive_cell_shape() {
  minera::EventLog source;
  minera::Trace trace;
  trace.case_id = "case";
  minera::Event event;
  event.activity = "A";
  minera::EventAttribute list{"payload", "", minera::AttributeKind::kString};
  list.container = minera::AttributeContainerKind::kList;
  minera::EventAttribute child{"count", "42", minera::AttributeKind::kInt};
  child.typed_value = std::int64_t{42};
  list.children.push_back(std::move(child));
  event.attributes.push_back(std::move(list));
  trace.events.push_back(std::move(event));
  source.traces.push_back(std::move(trace));

  minera::LogConversionOptions options;
  options.variant = minera::LogConversionVariant::kToDataFrame;
  const auto json = minera::log_conversion_json(
      minera::convert_event_log(source, options));
  assert(json.find("\"container\":\"list\"") != std::string::npos);
  assert(json.find("\"typed_value\":42") != std::string::npos);
  assert(json.find("\"children\":[") != std::string::npos);
}

void null_prefixed_case_label_does_not_erase_glue() {
  minera::ConvertedEventStream stream;
  minera::LogConversionRecord record;
  record.fields = {
      {"case:id", "c1", minera::AttributeKind::kString},
      {"case:concept:name", "", minera::AttributeKind::kString, true},
      {"concept:name", "A"},
  };
  stream.events.push_back(std::move(record));
  minera::LogConversionOptions options;
  options.case_id_key = "case:id";
  const auto log = minera::event_stream_to_event_log(stream, options);
  assert(log.traces.size() == 1);
  assert(log.traces[0].case_id == "c1");
  assert(log.traces[0].attribute("concept:name") == nullptr);
}

void empty_prefix_preserves_untagged_canonical_event_fields() {
  minera::ConvertedEventStream stream;
  minera::LogConversionRecord record;
  record.fields = {
      {"case:concept:name", "c1"},
      {"concept:name", "A"},
      {"time:timestamp", "2026-01-01T00:00:00Z",
       minera::AttributeKind::kDate},
      {"org:resource", "alice"},
      {"lifecycle:transition", "complete"},
  };
  stream.events.push_back(std::move(record));
  minera::LogConversionOptions options;
  options.case_attribute_prefix.clear();
  const auto log = minera::event_stream_to_event_log(stream, options);
  assert(log.traces.size() == 1);
  assert(log.traces[0].case_id == "c1");
  assert(log.traces[0].case_id_source_key == "concept:name");
  assert(log.traces[0].attributes.empty());
  assert(log.traces[0].events.size() == 1);
  const auto& event = log.traces[0].events[0];
  assert(event.activity == "A");
  assert(event.timestamp == "2026-01-01T00:00:00Z");
  assert(event.resource == "alice");
  assert(event.lifecycle == "complete");
  assert(event.attributes.empty());
}

void role_fields_and_legacy_states_are_serialized_explicitly() {
  minera::EventLog source;
  minera::Trace trace;
  trace.case_id = "legacy-case";
  minera::Event event;
  event.case_id = trace.case_id;
  event.activity = "A";
  event.timestamp = "2026-01-01T00:00:00Z";
  trace.events.push_back(std::move(event));
  source.traces.push_back(std::move(trace));

  minera::LogConversionOptions options;
  options.variant = minera::LogConversionVariant::kToEventStream;
  const auto stream_json = minera::log_conversion_json(
      minera::convert_event_log(source, options));
  assert(stream_json.find("\"first_class_role\":\"activity\"") !=
         std::string::npos);
  assert(stream_json.find("\"first_class_role\":\"case_id\"") !=
         std::string::npos);
  assert(stream_json.find("\"first_class_state\":\"present\"") !=
         std::string::npos);

  options.variant = minera::LogConversionVariant::kToEventLog;
  const auto log_json = minera::log_conversion_json(
      minera::convert_event_log(source, options));
  assert(log_json.find("\"case_id_state\":\"present\"") !=
         std::string::npos);
  assert(log_json.find("\"activity_state\":\"present\"") !=
         std::string::npos);
  const auto util_json = minera::log_prefix_json(source,
      minera::LogUtilityVariant::kFixedPrefixes);
  assert(util_json.find("\"case_id_state\":\"present\"") !=
         std::string::npos);
  assert(util_json.find("\"activity_state\":\"present\"") !=
         std::string::npos);
}

void selected_typed_attributes_survive_all_event_log_json_paths() {
  const std::string xml =
      "<log><trace><int key=\"case.number\" value=\"42\"><string "
      "key=\"case-meta\" value=\"kept\"/></int><event><int "
      "key=\"activity.code\" value=\"7\"><string key=\"activity-meta\" "
      "value=\"nested\"/></int><date key=\"when.value\" "
      "value=\"2026-01-01T00:00:00Z\"><int key=\"precision\" "
      "value=\"3\"/></date><string key=\"owner\" value=\"alice\"/>"
      "<string key=\"phase\" value=\"complete\"/></event></trace></log>";
  minera::XesImportOptions import_options;
  import_options.case_attribute = "case.number";
  import_options.activity_attribute = "activity.code";
  import_options.timestamp_attribute = "when.value";
  import_options.resource_attribute = "owner";
  import_options.lifecycle_attribute = "phase";
  std::istringstream input(xml);
  const auto log = minera::import_xes_event_log(input, import_options);
  assert(log.traces.size() == 1 && log.traces[0].case_id_attribute);
  assert(log.traces[0].case_id_attribute->kind == minera::AttributeKind::kInt);
  assert(std::get<std::int64_t>(log.traces[0].case_id_attribute->typed_value) ==
         42);
  assert(log.traces[0].case_id_attribute->children.size() == 1);

  const auto& event = log.traces[0].events[0];
  assert(event.activity_attribute && event.timestamp_attribute &&
         event.resource_attribute && event.lifecycle_attribute);
  assert(event.activity_attribute->kind == minera::AttributeKind::kInt);
  assert(std::get<std::int64_t>(event.activity_attribute->typed_value) == 7);
  assert(event.activity_attribute->children.size() == 1);
  assert(event.timestamp_attribute->kind == minera::AttributeKind::kDate);
  assert(event.timestamp_attribute->children.size() == 1);

  const auto graph = minera::discover_directly_follows_graph(log);
  const auto analysis_json = minera::analysis_json("typed", log, graph);
  assert(analysis_json.find(
             "\"case_id_attribute\": {\"key\":\"case.number\",\"value\":\"42\",\"kind\":\"int\",\"container\":\"scalar\",\"null\":false,\"typed_value\":42,\"children\":[") !=
         std::string::npos);
  assert(analysis_json.find(
             "\"activity_attribute\": {\"key\":\"activity.code\",\"value\":\"7\",\"kind\":\"int\",\"container\":\"scalar\",\"null\":false,\"typed_value\":7,\"children\":[") !=
         std::string::npos);
  assert(analysis_json.find(
             "\"timestamp_attribute\": {\"key\":\"when.value\",\"value\":\"2026-01-01T00:00:00.000Z\",\"kind\":\"date\",\"container\":\"scalar\",\"null\":false,\"typed_value\":\"2026-01-01T00:00:00.000Z\",\"children\":[") !=
         std::string::npos);
  assert(analysis_json.find(
             "\"resource_attribute\": {\"key\":\"owner\",\"value\":\"alice\",\"kind\":\"string\",\"container\":\"scalar\",\"null\":false") !=
         std::string::npos);
  assert(analysis_json.find(
             "\"lifecycle_attribute\": {\"key\":\"phase\",\"value\":\"complete\",\"kind\":\"string\",\"container\":\"scalar\",\"null\":false") !=
         std::string::npos);

  minera::LogConversionOptions conversion_options;
  conversion_options.variant = minera::LogConversionVariant::kToEventLog;
  const auto conversion_json = minera::log_conversion_json(
      minera::convert_event_log(log, conversion_options));
  assert(conversion_json.find(
             "\"case_id_attribute\":{\"key\":\"case.number\",\"value\":\"42\",\"kind\":\"int\",\"container\":\"scalar\",\"null\":false,\"typed_value\":42,\"children\":[") !=
         std::string::npos);
  assert(conversion_json.find(
             "\"activity_attribute\":{\"key\":\"activity.code\",\"value\":\"7\",\"kind\":\"int\",\"container\":\"scalar\",\"null\":false,\"typed_value\":7,\"children\":[") !=
         std::string::npos);
  assert(conversion_json.find(
             "\"timestamp_attribute\":{\"key\":\"when.value\",\"value\":\"2026-01-01T00:00:00.000Z\",\"kind\":\"date\",\"container\":\"scalar\",\"null\":false,\"typed_value\":\"2026-01-01T00:00:00.000Z\",\"children\":[") !=
         std::string::npos);
  assert(conversion_json.find("\"resource_attribute\":{\"key\":\"owner\"") !=
         std::string::npos);
  assert(conversion_json.find("\"lifecycle_attribute\":{\"key\":\"phase\"") !=
         std::string::npos);

  const auto utility_json = minera::log_prefix_json(
      log, minera::LogUtilityVariant::kFixedPrefixes);
  assert(utility_json.find(
             "\"case_id_attribute\":{\"key\":\"case.number\",\"value\":\"42\",\"kind\":\"int\",\"container\":\"scalar\",\"null\":false,\"typed_value\":42,\"children\":[") !=
         std::string::npos);
  assert(utility_json.find(
             "\"activity_attribute\":{\"key\":\"activity.code\",\"value\":\"7\",\"kind\":\"int\",\"container\":\"scalar\",\"null\":false,\"typed_value\":7,\"children\":[") !=
         std::string::npos);
  assert(utility_json.find(
             "\"timestamp_attribute\":{\"key\":\"when.value\",\"value\":\"2026-01-01T00:00:00.000Z\",\"kind\":\"date\",\"container\":\"scalar\",\"null\":false,\"typed_value\":\"2026-01-01T00:00:00.000Z\",\"children\":[") !=
         std::string::npos);
  assert(utility_json.find("\"resource_attribute\":{\"key\":\"owner\"") !=
         std::string::npos);
  assert(utility_json.find("\"lifecycle_attribute\":{\"key\":\"phase\"") !=
         std::string::npos);
}

void table_projection_is_rectangular_and_typed() {
  auto source = attributed_log();
  source.traces[0].events[1].attributes.push_back(
      {"approved", "true", minera::AttributeKind::kBoolean});
  minera::LogConversionOptions options;
  options.variant = minera::LogConversionVariant::kToDataFrame;
  const auto result = minera::convert_event_log(source, options);
  assert(result.table.rows.size() == 2);
  assert(std::find(result.table.columns.begin(), result.table.columns.end(),
                   "cost") != result.table.columns.end());
  assert(std::find(result.table.columns.begin(), result.table.columns.end(),
                   "approved") != result.table.columns.end());
  for (const auto& row : result.table.rows)
    assert(row.size() == result.table.columns.size());
  const auto cost = static_cast<std::size_t>(std::find(
      result.table.columns.begin(), result.table.columns.end(), "cost") -
      result.table.columns.begin());
  const auto approved = static_cast<std::size_t>(std::find(
      result.table.columns.begin(), result.table.columns.end(), "approved") -
      result.table.columns.begin());
  assert(result.table.rows[0][cost]->kind == minera::AttributeKind::kFloat);
  assert(!result.table.rows[0][approved]);
  assert(!result.table.rows[1][cost]);
  assert(result.table.rows[1][approved]->kind ==
         minera::AttributeKind::kBoolean);
}

void lazy_output_and_metadata_json_are_explicit() {
  auto source = attributed_log();
  source.metadata.extensions.push_back({"demo", "d", "urn:demo"});
  minera::LogConversionOptions options;
  options.variant = minera::LogConversionVariant::kToDataFrame;
  options.lazy_output = true;
  const auto result = minera::convert_event_log(source, options);
  assert(result.lazy_table.has_value());
  assert(result.lazy_table->materialize().rows == result.table.rows);
  const auto json = minera::log_conversion_json(result);
  assert(json.find("\"metadata\":") != std::string::npos);
  assert(json.find("\"extensions\":[{\"name\":\"demo\"") !=
         std::string::npos);
  assert(json.find("\"lazy\":true") != std::string::npos);
}

void lexical_numeric_case_values_group_without_losing_strings() {
  minera::EventStream stream;
  for (const auto& key : std::vector<minera::EventAttribute>{
           {"case:concept:name", "0001", minera::AttributeKind::kInt},
           {"case:concept:name", "+1", minera::AttributeKind::kInt},
           {"case:concept:name", "1.0", minera::AttributeKind::kFloat},
           {"case:concept:name", "true", minera::AttributeKind::kBoolean},
           {"case:concept:name", "1", minera::AttributeKind::kString}})
    stream.events.push_back({{key, {"concept:name", "A"}}});
  const auto log = minera::event_stream_to_event_log(stream);
  assert(log.traces.size() == 2);
  assert(log.traces[0].events.size() == 4);
  assert(log.traces[0].case_id == "0001");
  assert(log.traces[1].events.size() == 1);
}

void explicit_native_table_origin_survives_conversion() {
  minera::EventTable table;
  table.attributes = {{"origin", "native"}, {"source", "preserved"}};
  table.metadata.extensions = {{"Custom", "custom", "urn:custom"}};
  table.metadata.properties = {{"pm4py:param:case_id_key", "custom-case"},
                               {"case_id_key", "ordinary"}};
  const auto before = table;
  const auto stream = minera::event_table_to_event_stream(table);
  assert(stream.attributes == before.attributes);
  assert(stream.metadata.extensions == before.metadata.extensions);
  assert(stream.metadata.properties.size() == 1);
  assert(stream.metadata.properties.front().key == "case_id_key");
  assert(stream.metadata.properties.front().value == "ordinary");
  assert(table.metadata == before.metadata);
  assert(table.attributes == before.attributes);
  table.attributes = {{"source", "native-without-origin"}};
  assert(minera::event_table_to_event_stream(table).attributes == table.attributes);
}

void table_metadata_survives_conversion_and_serialization() {
  minera::EventTable table;
  table.columns = {"activity", "amount"};
  table.rows = {{minera::EventAttribute{"activity", "A"},
                 minera::EventAttribute{"amount", "1"}}};
  table.dtypes = {"str", "category"};
  minera::EventTableCategoricalDtype category;
  category.categories_dtype = "object";
  category.ordered = true;
  category.categories = {
      minera::EventAttribute{"first-key", "1"},
      [] {
        minera::EventAttribute tuple{"tuple", ""};
        tuple.container = minera::AttributeContainerKind::kTuple;
        tuple.children = {minera::EventAttribute{"left", "x"},
                          minera::EventAttribute{"right", "2",
                                                   minera::AttributeKind::kInt}};
        return tuple;
      }(),
  };
  table.categorical_dtypes = {std::nullopt, category};
  minera::EventTableIndex explicit_index;
  explicit_index.kind = "Index";
  explicit_index.dtype = "object";
  explicit_index.names = {std::nullopt};
  explicit_index.labels = {{minera::EventAttribute{"run", "r1"}}};
  table.index = explicit_index;
  table.validate();

  const auto identity =
      minera::event_data_to_table(minera::EventDataView(table));
  assert(identity.dtypes == table.dtypes);
  assert(identity.index == table.index);
  assert(identity.categorical_dtypes == table.categorical_dtypes);
  const auto lazy = minera::LazyEventTable::from_table(table);
  assert(lazy.dtypes() == table.dtypes);
  assert(lazy.index() == table.index);
  assert(lazy.categorical_dtypes() == table.categorical_dtypes);
  assert(lazy.materialize().index == table.index);
  assert(lazy.materialize().categorical_dtypes == table.categorical_dtypes);

  minera::LogConversionResult table_result;
  table_result.variant = minera::LogConversionVariant::kToDataFrame;
  table_result.table = table;
  const auto table_json = minera::log_conversion_json(table_result);
  table_result.table.categorical_dtypes.pop_back();
  bool malformed_rejected = false;
  try { (void)minera::log_conversion_json(table_result); }
  catch (const std::invalid_argument&) { malformed_rejected = true; }
  assert(malformed_rejected);
  table_result.table = table;
  assert(table_json.find("\"dtypes\":[\"str\",\"category\"]") !=
         std::string::npos);
  assert(table_json.find("\"categorical_dtypes\":[null,{\"categories_dtype\":\"object\",\"ordered\":true") !=
         std::string::npos);
  assert(table_json.find("\"container\":\"tuple\"") !=
         std::string::npos);
  assert(table_json.find("\"index\":{\"class\":\"Index\"") !=
         std::string::npos);
  assert(table_json.find("\"kind\":\"scalar\"") !=
         std::string::npos);

  explicit_index.kind = "MultiIndex";
  explicit_index.names = {std::string{"run"}, std::string{"step"}};
  minera::EventTableIndexLabel tuple;
  tuple.value = std::vector<minera::EventTableIndexLabel>{
      {minera::EventAttribute{"run", "r1"}},
      {minera::EventAttribute{"step", "1"}},
  };
  explicit_index.labels = {std::move(tuple)};
  table.index = explicit_index;
  table.validate();
  table_result.table = table;
  const auto multi_json = minera::log_conversion_json(table_result);
  assert(multi_json.find("\"kind\":\"tuple\"") !=
         std::string::npos);
  assert(multi_json.find("\"items\":[") != std::string::npos);
}

void exact_uint64_values_are_serialized_without_rounding() {
  minera::EventTable table;
  table.columns = {"amount"};
  minera::EventAttribute amount{"amount", "18446744073709551615",
                                minera::AttributeKind::kInt};
  amount.typed_value = std::uint64_t{18446744073709551615ULL};
  table.rows = {{amount}};
  table.validate();

  minera::LogConversionResult result;
  result.variant = minera::LogConversionVariant::kToDataFrame;
  result.table = table;
  const auto conversion_json = minera::log_conversion_json(result);
  assert(conversion_json.find("\"typed_value\":18446744073709551615") !=
         std::string::npos);

  const auto relation_json = minera::case_relations_json(table);
  assert(relation_json.find("\"typed_value\":18446744073709551615") !=
         std::string::npos);
}

void native_date_payload_controls_case_grouping() {
  assert(!minera::parse_timestamp_exact(
      "2024-01-01T00:00:00+01:"));
  const auto valid_offset = minera::parse_timestamp_exact(
      "2024-01-01T00:00:00+01:00");
  assert(valid_offset && valid_offset->timezone_aware &&
         valid_offset->offset_minutes == 60);

  minera::EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  minera::EventAttribute first{"case:concept:name", "stale-first",
                               minera::AttributeKind::kDate};
  first.typed_value = minera::NativeDate{"2024-01-01T00:00:00Z"};
  minera::EventAttribute second{"case:concept:name", "stale-second",
                                minera::AttributeKind::kDate};
  second.typed_value =
      minera::NativeDate{"2024-01-01T01:00:00+01:00"};
  table.rows = {{first, minera::EventAttribute{"concept:name", "A"},
                 minera::EventAttribute{"time:timestamp", "2024-01-01T00:00:00Z",
                                        minera::AttributeKind::kDate}},
                {second, minera::EventAttribute{"concept:name", "B"},
                 minera::EventAttribute{"time:timestamp", "2024-01-01T00:00:00Z",
                                        minera::AttributeKind::kDate}}};
  const auto converted = minera::event_table_to_event_log(table);
  assert(converted.traces.size() == 1);
  assert(converted.traces.front().events.size() == 2);
  assert(converted.traces.front().case_id ==
         "2024-01-01T00:00:00Z");
  assert(converted.traces.front().case_id_attribute);
  assert(converted.traces.front().case_id_attribute->value ==
         "2024-01-01T00:00:00Z");
  const auto* native = std::get_if<minera::NativeDate>(
      &converted.traces.front().case_id_attribute->typed_value);
  assert(native && native->value == "2024-01-01T00:00:00Z");
}

void csv_projection_preserves_schema_and_source_index_labels() {
  std::istringstream input(
      "case_id,activity,timestamp\n"
      "c0,A,2026-01-01T00:00:00Z\n"
      "c1,B,2026-01-01T00:01:00Z\n"
      "c2,C,2026-01-01T00:02:00Z\n");
  const auto imported = minera::import_csv_event_data(input);
  assert(imported.table.dtypes ==
         std::vector<std::string>({"str", "str", "str"}));
  assert(!imported.table.index);

  minera::EventLog filtered;
  filtered.traces.push_back(imported.log.traces[2]);
  filtered.traces.push_back(imported.log.traces[0]);
  const auto projected =
      minera::project_csv_event_table(imported.table, filtered);
  assert(projected.dtypes == imported.table.dtypes);
  assert(projected.index);
  assert(projected.index->labels.size() == 2);
  const auto& first = projected.index->labels[0].value;
  const auto& second = projected.index->labels[1].value;
  assert(std::get<minera::EventAttribute>(first).value == "0");
  assert(std::get<minera::EventAttribute>(second).value == "2");

  const auto identity =
      minera::project_csv_event_table(imported.table, imported.log);
  assert(!identity.index);
  assert(identity.dtypes == imported.table.dtypes);
}

void dataframe_row_cap_counts_events_and_is_deterministic() {
  // A flattened EventLog has exactly one table row per event, regardless of
  // how many traces contain those events.
  auto source = attributed_log();
  minera::LogConversionOptions options;
  options.variant = minera::LogConversionVariant::kToDataFrame;
  options.max_rows_to_materialize = 1;
  bool threw = false;
  try {
    (void)minera::convert_event_log(source, options);
  } catch (const std::length_error& error) {
    threw = true;
    assert(std::string(error.what()) ==
           "log conversion: row materialization cap exceeded");
  }
  assert(threw);

  options.max_rows_to_materialize = 2;
  options.lazy_output = true;
  const auto result = minera::convert_event_log(source, options);
  assert(result.table.rows.size() == 2);
  assert(result.lazy_table && result.lazy_table->row_count() == 2);

  options.max_rows_to_materialize = 0;
  threw = false;
  try {
    (void)minera::convert_event_log(source, options);
  } catch (const std::length_error&) {
    threw = true;
  }
  assert(threw);
}

void graph_null_selectors_are_absent() {
  auto source = attributed_log();
  source.traces[0].attributes.push_back(
      {"nullable_case", "", minera::AttributeKind::kString, true});
  source.traces[0].events[0].attributes.push_back(
      {"nullable_event", "", minera::AttributeKind::kString, true});

  minera::LogConversionOptions options;
  options.graph_case_id_attribute = "nullable_case";
  bool threw = false;
  try {
    (void)minera::event_log_to_attributed_graph(source, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  options.graph_case_id_attribute = "concept:name";
  options.case_attributes_as_nodes = {"nullable_case"};
  options.event_attributes_as_nodes = {"nullable_event"};
  const auto graph = minera::event_log_to_attributed_graph(source, options);
  // Only the case and two event nodes plus their membership/DF edges remain.
  assert(graph.nodes.size() == 3);
  assert(graph.edges.size() == 3);
}

void graph_projection_builds_cases_events_attributes_and_df() {
  minera::LogConversionOptions options;
  options.variant = minera::LogConversionVariant::kToAttributedGraph;
  options.case_attributes_as_nodes = {"region"};
  options.event_attributes_as_nodes = {"org:resource"};
  const auto result = minera::convert_event_log(attributed_log(), options);
  assert(result.graph.nodes.size() == 6);
  assert(result.graph.edges.size() == 6);
  assert(std::count_if(result.graph.nodes.begin(), result.graph.nodes.end(),
                       [](const auto& node) {
                         return node.type == minera::LogGraphNodeType::kCase;
                       }) == 1);
  assert(std::count_if(result.graph.nodes.begin(), result.graph.nodes.end(),
                       [](const auto& node) {
                         return node.type == minera::LogGraphNodeType::kEvent;
                       }) == 2);
  assert(std::count_if(result.graph.edges.begin(), result.graph.edges.end(),
                       [](const auto& edge) {
                         return edge.type ==
                                minera::LogGraphEdgeType::kDirectlyFollows;
                       }) == 1);
  const auto has_edge = [&](std::size_t source, std::size_t target,
                            minera::LogGraphEdgeType type) {
    return std::any_of(result.graph.edges.begin(), result.graph.edges.end(),
                       [&](const auto& edge) {
                         return edge.source == source && edge.target == target &&
                                edge.type == type;
                       });
  };
  assert(has_edge(1, 0, minera::LogGraphEdgeType::kBelongsTo));
  assert(has_edge(1, 3, minera::LogGraphEdgeType::kDirectlyFollows));
  assert(has_edge(1, 2, minera::LogGraphEdgeType::kAttribute));
  assert(has_edge(0, 5, minera::LogGraphEdgeType::kAttribute));

  options.include_directly_follows = false;
  const auto without_df =
      minera::event_log_to_attributed_graph(attributed_log(), options);
  assert(without_df.edges.size() == 5);

  auto collision_log = attributed_log();
  collision_log.traces[0].events.resize(1);
  collision_log.traces[0].events[0].attributes.push_back({"left", "shared"});
  collision_log.traces[0].events[0].attributes.push_back({"right", "shared"});
  options.event_attributes_as_nodes = {"left", "right"};
  options.case_attributes_as_nodes.clear();
  const auto collision =
      minera::event_log_to_attributed_graph(collision_log, options);
  assert(collision.nodes.size() == 3);
  assert(collision.edges.size() == 2);
  const auto overwritten = std::find_if(
      collision.edges.begin(), collision.edges.end(), [](const auto& edge) {
        return edge.type == minera::LogGraphEdgeType::kAttribute;
      });
  assert(overwritten != collision.edges.end());
  assert(overwritten->attribute_name == "right");

  collision_log.traces[0].events[0].attributes.push_back(
      {"collides", "CASE=case-1"});
  options.event_attributes_as_nodes = {"collides"};
  const auto identifier_collision =
      minera::event_log_to_attributed_graph(collision_log, options);
  assert(identifier_collision.nodes.size() == 2);
  assert(identifier_collision.edges.size() == 1);
  assert(identifier_collision.nodes[0].type ==
         minera::LogGraphNodeType::kCase);
  assert(identifier_collision.edges[0].type ==
         minera::LogGraphEdgeType::kAttribute);

  collision_log.traces[0].events[0].attributes.push_back(
      {"typed_string", "1", minera::AttributeKind::kString});
  collision_log.traces[0].events[0].attributes.push_back(
      {"typed_integer", "1", minera::AttributeKind::kInt});
  options.event_attributes_as_nodes = {"typed_string", "typed_integer"};
  const auto typed_nodes =
      minera::event_log_to_attributed_graph(collision_log, options);
  assert(typed_nodes.nodes.size() == 4);
  assert(typed_nodes.edges.size() == 3);
  const auto string_node = std::find_if(
      typed_nodes.nodes.begin(), typed_nodes.nodes.end(), [](const auto& node) {
        return node.type == minera::LogGraphNodeType::kAttribute &&
               !node.attributes.empty() &&
               node.attributes[0].kind == minera::AttributeKind::kString;
      });
  const auto integer_node = std::find_if(
      typed_nodes.nodes.begin(), typed_nodes.nodes.end(), [](const auto& node) {
        return node.type == minera::LogGraphNodeType::kAttribute &&
               !node.attributes.empty() &&
               node.attributes[0].kind == minera::AttributeKind::kInt;
      });
  assert(string_node != typed_nodes.nodes.end());
  assert(integer_node != typed_nodes.nodes.end());
  assert(string_node->id == "1" && integer_node->id == "1");
}

void graph_projection_handles_thousands_of_indexed_edges() {
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "large";
  constexpr std::size_t event_count = 4096;
  trace.events.reserve(event_count);
  for (std::size_t index = 0; index < event_count; ++index)
    trace.events.push_back({trace.case_id, "A", {}, {}, index});
  log.traces.push_back(std::move(trace));

  minera::LogConversionOptions options;
  options.variant = minera::LogConversionVariant::kToAttributedGraph;
  options.max_graph_nodes = event_count + 1;
  options.max_graph_edges = event_count * 2 - 1;
  const auto graph = minera::event_log_to_attributed_graph(log, options);
  assert(graph.nodes.size() == event_count + 1);
  assert(graph.edges.size() == event_count * 2 - 1);

  --options.max_graph_edges;
  bool threw = false;
  try {
    (void)minera::event_log_to_attributed_graph(log, options);
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);
}

minera::EventLog variant_log() {
  minera::EventLog log;
  for (const auto& [case_id, activities] :
       std::vector<std::pair<std::string, std::vector<std::string>>>{
           {"c1", {"A", "B"}},
           {"c2", {"A", "B"}},
           {"c3", {"C"}},
       }) {
    minera::Trace trace;
    trace.case_id = case_id;
    for (std::size_t index = 0; index < activities.size(); ++index)
      trace.events.push_back({case_id, activities[index], {}, {}, index});
    log.traces.push_back(std::move(trace));
  }
  return log;
}

void variant_projections_preserve_frequency_contracts() {
  minera::LogConversionOptions one;
  one.variant = minera::LogConversionVariant::kOneTracePerVariant;
  const auto one_result = minera::convert_event_log(variant_log(), one);
  assert(one_result.log.trace_count() == 2);
  assert(one_result.log.traces[0].events.size() == 2);
  assert(one_result.log.traces[0].events[0].activity == "A");
  assert(one_result.log.traces[0].case_id.empty());

  minera::LogConversionOptions all;
  all.variant = minera::LogConversionVariant::kAllTracesPerVariant;
  all.return_variants = true;
  const auto all_result = minera::convert_event_log(variant_log(), all);
  assert(all_result.log.trace_count() == 3);
  assert(all_result.variants.size() == 2);
  assert(all_result.variants[0].activities ==
         std::vector<std::string>({"A", "B"}));
  assert(all_result.variants[0].count == 2);
  assert(all_result.variants[0].trace_indexes ==
         std::vector<std::size_t>({0, 1}));
  assert(all_result.variants[1].trace_indexes ==
         std::vector<std::size_t>({2}));
  assert(all_result.log.attributes.empty());
  assert(all_result.log.metadata == minera::EventDataMetadata{});

  all.max_variants_to_return = 1;
  const auto limited = minera::convert_event_log(variant_log(), all);
  assert(limited.log.trace_count() == 2);
  assert(limited.variants.size() == 1);

  one.max_variants_to_return = 0;
  assert(minera::convert_event_log(variant_log(), one).log.trace_count() == 2);
}

void selectors_json_and_failures_are_strict() {
  for (const auto& [name, expected] :
       std::vector<std::pair<std::string, minera::LogConversionVariant>>{
           {"to_event_log", minera::LogConversionVariant::kToEventLog},
           {"TO_EVENT_STREAM", minera::LogConversionVariant::kToEventStream},
           {"to_dataframe", minera::LogConversionVariant::kToDataFrame},
           {"to_nx", minera::LogConversionVariant::kToAttributedGraph},
           {"df_to_event_log_1v",
            minera::LogConversionVariant::kOneTracePerVariant},
           {"df_to_event_log_nv",
            minera::LogConversionVariant::kAllTracesPerVariant},
       })
    assert(minera::parse_log_conversion_variant(name) == expected);

  minera::LogConversionOptions graph;
  graph.variant = minera::LogConversionVariant::kToAttributedGraph;
  const auto json =
      minera::log_conversion_json(minera::convert_event_log(attributed_log(),
                                                            graph));
  assert(json.find("\"family\":\"LOG_CONVERSION\"") !=
         std::string::npos);
  assert(json.find("\"variant\":\"TO_NX\"") != std::string::npos);
  assert(json.find("\"type\":\"BELONGS_TO\"") != std::string::npos);

  minera::LogConversionOptions serialized;
  serialized.variant = minera::LogConversionVariant::kToEventStream;
  assert(minera::log_conversion_json(
             minera::convert_event_log(attributed_log(), serialized))
             .find("\"stream\":") != std::string::npos);
  serialized.variant = minera::LogConversionVariant::kToDataFrame;
  assert(minera::log_conversion_json(
             minera::convert_event_log(attributed_log(), serialized))
             .find("\"table\":") != std::string::npos);
  serialized.variant = minera::LogConversionVariant::kToEventLog;
  assert(minera::log_conversion_json(
             minera::convert_event_log(attributed_log(), serialized))
             .find("\"log\":") != std::string::npos);

  bool threw = false;
  try {
    (void)minera::parse_log_conversion_variant("unknown");
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  graph.max_graph_nodes = 1;
  threw = false;
  try {
    (void)minera::convert_event_log(attributed_log(), graph);
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);

  graph.max_graph_nodes = 100;
  graph.graph_case_id_attribute = "missing";
  threw = false;
  try {
    (void)minera::convert_event_log(attributed_log(), graph);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

}  // namespace

void empty_scalar_wrappers_survive_conversion() {
  auto log = attributed_log();
  minera::EventAttribute wrapped{"flag", "true", minera::AttributeKind::kBoolean};
  wrapped.typed_value = true;
  wrapped.scalar_wrapper = true;
  wrapped.scalar_children_list = true;
  log.attributes.push_back(wrapped);
  log.traces.front().events.front().attributes.push_back(wrapped);
  auto selected = wrapped;
  selected.key = "concept:name";
  selected.kind = minera::AttributeKind::kString;
  selected.value = "A";
  selected.typed_value = std::monostate{};
  log.traces.front().events.front().activity_attribute = selected;
  const auto stream = minera::event_log_to_event_stream(log);
  assert(stream.events.front().field("flag")->scalar_wrapper);
  assert(stream.events.front().field("flag")->scalar_children_list);
  assert(stream.events.front().field("concept:name")->scalar_wrapper);
  assert(stream.events.front().field("concept:name")->scalar_children_list);
  auto legacy = log;
  legacy.traces.front().events.front().activity_attribute.reset();
  legacy.traces.front().events.front().attributes.push_back(selected);
  assert(minera::event_log_to_event_stream(legacy).events.front().field("concept:name")->scalar_wrapper);
  const auto restored = minera::event_stream_to_event_log(stream);
  assert(restored.traces.front().events.front().activity_attribute->scalar_wrapper);
  assert(restored.traces.front().events.front().activity_attribute->scalar_children_list);
  assert(minera::log_conversion_json(minera::convert_event_log(restored)).find(
      "\"scalar_wrapper\":true") != std::string::npos);
  assert(minera::log_conversion_json(minera::convert_event_log(restored)).find(
      "\"scalar_children_list\":true") != std::string::npos);

  for (auto variant : {minera::XesImportVariant::kIterparse,
                       minera::XesImportVariant::kIterparseMemoryCompressed,
                       minera::XesImportVariant::kIterparse20,
                       minera::XesImportVariant::kLineByLine,
                       minera::XesImportVariant::kChunkRegex}) {
    for (const auto inner : {std::string{}, std::string{"<values/>"}, std::string{"<values></values>"}}) {
      std::istringstream xml("<log>\n<trace>\n<event>\n<boolean key=\"flag\" value=\"true\">" + inner +
                             "</boolean>\n</event>\n</trace>\n</log>");
      minera::XesImportOptions options;
      options.variant = variant;
      const auto imported = minera::import_xes_event_log(xml, options);
      const auto& field = imported.traces.front().events.front().attributes.front();
      const bool iterparse = variant != minera::XesImportVariant::kLineByLine &&
                             variant != minera::XesImportVariant::kChunkRegex;
      assert(field.scalar_wrapper == (iterparse && !inner.empty()));
      assert(field.scalar_children_list == (iterparse && variant != minera::XesImportVariant::kIterparse20 && !inner.empty()));
    }
  }
}

void xes_result_serialization_preserves_missing_cells_and_schema() {
  minera::XesImportResult result;
  result.kind = minera::EventDataKind::kEventTable;
  result.table.columns = {"case:concept:name", "flag"};
  result.table.rows = {{minera::EventAttribute{"case:concept:name", "c"}, std::nullopt}};
  result.table.dtypes = {"str", "object"};
  result.dtypes = {"str", "object"};
  auto json = minera::xes_import_result_json(result);
  assert(json.find("\"type\":\"DataFrame\"") != std::string::npos);
  assert(json.find("\"dtypes\":[\"str\",\"object\"]") != std::string::npos);
  assert(json.find(",null]") != std::string::npos);
  result.kind = minera::EventDataKind::kLazyEventTable;
  result.lazy_table = minera::LazyEventTable::from_table(result.table);
  result.table = {};
  result.dtypes.clear();
  json = minera::xes_import_result_json(result);
  assert(json.find("\"type\":\"LazyFrame\"") != std::string::npos);
  assert(json.find("\"lazy\":true") != std::string::npos);
  assert(json.find("case:concept:name") != std::string::npos);
  result.dtypes = {"String", "Boolean"};
  bool rejected = false;
  try { (void)minera::xes_import_result_json(result); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
  result.kind = minera::EventDataKind::kEventTable;
  result.table.columns = {"one", "two"};
  result.table.rows = {{std::nullopt}};
  result.dtypes = {"Null", "Null"};
  rejected = false;
  try { (void)minera::xes_import_result_json(result); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
}

void null_container_case_keys_remain_null() {
  using namespace minera;
  EventStream input;
  for (const auto kind : {AttributeContainerKind::kScalar,
                         AttributeContainerKind::kTuple,
                         AttributeContainerKind::kFrozenSet,
                         AttributeContainerKind::kList,
                         AttributeContainerKind::kContainer,
                         AttributeContainerKind::kSet}) {
    EventAttribute key{"case:concept:name", ""};
    key.is_null = true;
    key.container = kind;
    input.events.push_back({{key, EventAttribute{"concept:name", "A"}}});
  }
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 1 || actual.traces[0].events.size() != 6)
    throw std::runtime_error("null container case keys split or lost events");
  for (auto& record : input.events) {
    auto child = record.fields[0];
    child.key.clear();
    EventAttribute key{"case:concept:name", ""};
    key.container = AttributeContainerKind::kTuple;
    key.children.push_back(child);
    record.fields[0] = key;
  }
  const auto nested = event_stream_to_event_log(input);
  if (nested.traces.size() != 1 || nested.traces[0].events.size() != 6)
    throw std::runtime_error("nested null case keys split or lost events");
}

int main() {
  null_container_case_keys_remain_null();
  check_public_dataframe_conversion_metadata();
  check_public_conversion_case_grouping();
  check_public_container_case_grouping();
  lexical_numeric_case_values_group_without_losing_strings();
  explicit_native_table_origin_survives_conversion();
  xes_result_serialization_preserves_missing_cells_and_schema();
  empty_scalar_wrappers_survive_conversion();
  stream_round_trip_preserves_typed_attributes();
  empty_prefix_and_trace_properties_follow_flattening_contract();
  stream_grouping_is_first_seen_and_first_event_wins();
  stream_without_case_attributes_uses_stable_trace_indexes();
  custom_case_glue_and_prefix_round_trip();
  source_keys_and_missing_case_ids_survive_flat_adapters();
  table_json_keeps_recursive_cell_shape();
  null_prefixed_case_label_does_not_erase_glue();
  empty_prefix_preserves_untagged_canonical_event_fields();
  role_fields_and_legacy_states_are_serialized_explicitly();
  selected_typed_attributes_survive_all_event_log_json_paths();
  table_projection_is_rectangular_and_typed();
  lazy_output_and_metadata_json_are_explicit();
  table_metadata_survives_conversion_and_serialization();
  exact_uint64_values_are_serialized_without_rounding();
  native_date_payload_controls_case_grouping();
  csv_projection_preserves_schema_and_source_index_labels();
  dataframe_row_cap_counts_events_and_is_deterministic();
  graph_null_selectors_are_absent();
  graph_projection_builds_cases_events_attributes_and_df();
  graph_projection_handles_thousands_of_indexed_edges();
  variant_projections_preserve_frequency_contracts();
  selectors_json_and_failures_are_strict();
}
