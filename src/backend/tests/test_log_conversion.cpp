#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <string>

#include "minera/log_conversion.hpp"

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
  assert(restored.traces[0].events[0].activity.empty());
  assert(restored.traces[0].events[0].attributes.empty());
  assert(restored.traces[0].attribute("concept:name") != nullptr);
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

int main() {
  stream_round_trip_preserves_typed_attributes();
  empty_prefix_and_trace_properties_follow_flattening_contract();
  stream_grouping_is_first_seen_and_first_event_wins();
  stream_without_case_attributes_uses_stable_trace_indexes();
  custom_case_glue_and_prefix_round_trip();
  null_prefixed_case_label_does_not_erase_glue();
  table_projection_is_rectangular_and_typed();
  lazy_output_and_metadata_json_are_explicit();
  dataframe_row_cap_counts_events_and_is_deterministic();
  graph_null_selectors_are_absent();
  graph_projection_builds_cases_events_attributes_and_df();
  graph_projection_handles_thousands_of_indexed_edges();
  variant_projections_preserve_frequency_contracts();
  selectors_json_and_failures_are_strict();
}
