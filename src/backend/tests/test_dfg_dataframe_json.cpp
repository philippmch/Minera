#include "minera/dfg_dataframe_json.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <utility>

namespace {

minera::Event sample_event(std::string activity, std::size_t order) {
  minera::Event event;
  event.case_id = "case\"\\";
  event.case_id_state = minera::FirstClassAttributeState::kPresent;
  event.activity = std::move(activity);
  event.activity_state = minera::FirstClassAttributeState::kPresent;
  event.timestamp = "2026-01-01T00:00:00Z";
  event.timestamp_state = minera::FirstClassAttributeState::kPresent;
  event.input_order = order;
  event.attributes.push_back({"extra\nkey", "value\"\\", minera::AttributeKind::kString,
                              false});
  event.attributes.push_back({"missing", "", minera::AttributeKind::kString,
                              true});
  return event;
}

void graph_json_contract() {
  minera::DfgTableResult result;
  result.backend = minera::DfgTableBackend::kPandas;
  result.measure = minera::DfgTableMeasure::kBoth;
  result.frequency[{"A\"", "B\\"}] = 2;
  minera::DfgPerformanceStatistics statistics;
  statistics.summary.count = 1;
  statistics.summary.mean = 2.5;
  statistics.summary.median = 2.5;
  statistics.summary.minimum = 2.5;
  statistics.summary.maximum = 2.5;
  statistics.summary.sum = 2.5;
  statistics.raw_values = {2.5};
  statistics.value = 2.5;
  result.performance[{"A\"", "B\\"}] = statistics;

  const auto first = minera::serialize_dfg_dataframe_json(result);
  assert(first == minera::serialize_dfg_dataframe_json(result));
  assert(first.find("\"operation\":\"graph\"") != std::string::npos);
  assert(first.find("\"backend\":\"pandas\"") != std::string::npos);
  assert(first.find("\"measure\":\"both\"") != std::string::npos);
  assert(first.find("\"frequency\"") != std::string::npos);
  assert(first.find("\"performance\"") != std::string::npos);
  assert(first.find("A\\\"") != std::string::npos);
  assert(first.find("B\\\\") != std::string::npos);
}

void cost_json_contract() {
  minera::DfgTableResult result;
  result.backend = minera::DfgTableBackend::kPolars;
  result.measure = minera::DfgTableMeasure::kCost;
  minera::DfgPerformanceStatistics statistics;
  statistics.summary.count = 1;
  statistics.summary.mean = 7.0;
  statistics.summary.median = 7.0;
  statistics.summary.minimum = 7.0;
  statistics.summary.maximum = 7.0;
  statistics.summary.sum = 7.0;
  statistics.raw_values = {7.0};
  statistics.value = 7.0;
  result.performance[{"A", "B"}] = statistics;
  const auto body = minera::serialize_dfg_dataframe_json(result);
  assert(body.find("\"measure\":\"cost\"") != std::string::npos);
  assert(body.find("\"cost\"") != std::string::npos);
  assert(body.find("\"performance\"") == std::string::npos);
}

void partial_order_json_contract() {
  minera::DfgPartialOrderResult result;
  result.backend = minera::DfgTableBackend::kPandas;
  minera::DfgPartialOrderPair pair;
  pair.source = sample_event("A", 0);
  pair.target = sample_event("B", 1);
  pair.source_index = -3.0;
  pair.target_index = 9.5;
  pair.source_index_value = "-3";
  pair.target_index_value = "9.5";
  pair.flow_time_seconds = 4.0;
  result.pairs.push_back(std::move(pair));
  const auto body = minera::serialize_dfg_partial_order_json(result);
  assert(body.find("\"operation\":\"partial_order\"") !=
         std::string::npos);
  assert(body.find("\"source_index\":-3") != std::string::npos);
  assert(body.find("\"target_index\":9.5") != std::string::npos);
  assert(body.find("\"target_index_value\":\"9.5\"") !=
         std::string::npos);
  assert(body.find("extra\\nkey") != std::string::npos);
  assert(body.find("\"null\":true") != std::string::npos);
  assert(body.find("\"source\":{\"case_id\"") != std::string::npos);
}

void concurrent_json_contract() {
  minera::DfgConcurrentResult result;
  result.backend = minera::DfgTableBackend::kPolars;
  result.max_start_column = "max-start";
  result.min_complete_column = "min-complete";
  result.overlap_column = "overlap";
  minera::DfgConcurrentPair pair;
  pair.left = sample_event("left", 2);
  pair.right = sample_event("right", 4);
  pair.left_index = 2;
  pair.right_index = 4;
  pair.left_index_value = "2";
  pair.right_index_value = "4";
  pair.max_start_seconds = 2.0;
  pair.min_complete_seconds = 5.0;
  pair.overlap_seconds = 3.0;
  result.pairs.push_back(std::move(pair));
  const auto body = minera::serialize_dfg_concurrent_events_json(result);
  assert(body.find("\"operation\":\"concurrent_events\"") !=
         std::string::npos);
  assert(body.find("\"backend\":\"polars\"") != std::string::npos);
  assert(body.find("\"columns\":{\"max_start_column\":\"max-start\",") !=
         std::string::npos);
  assert(body.find("\"min_complete_column\":\"min-complete\"") !=
         std::string::npos);
  assert(body.find("\"overlap_column\":\"overlap\"") !=
         std::string::npos);
  assert(body.find("\"max_start_seconds\":2") != std::string::npos);
  assert(body.find("\"min_complete_seconds\":5") != std::string::npos);
  assert(body.find("\"overlap_seconds\":3") != std::string::npos);
  assert(body.find("\"left\":{\"case_id\"") != std::string::npos);
  assert(body.find("\"right\":{\"case_id\"") != std::string::npos);
}

}  // namespace

int main() {
  graph_json_contract();
  cost_json_contract();
  partial_order_json_contract();
  concurrent_json_contract();
  return 0;
}
