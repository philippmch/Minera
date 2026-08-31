#include "minera/case_grouping.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "minera/xes_export.hpp"

#include <cassert>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

minera::EventLog sample_log() {
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "case-1";
  trace.events = {
      {"case-1", "A", "2024-01-01T00:00:00Z", "", 0, {}, ""},
      {"case-1", "A", "2024-01-01T00:00:00Z", "", 1, {}, ""},
      {"case-1", "B", "2024-01-01T00:00:00Z", "", 2, {}, ""},
      {"case-1", "B", "2024-01-01T00:01:00Z", "", 3, {}, ""},
      {"case-1", "A", "2024-01-01T00:01:00Z", "", 4, {}, ""},
  };
  log.traces.push_back(std::move(trace));
  return log;
}

std::vector<std::string> activities(const minera::Trace& trace) {
  std::vector<std::string> result;
  for (const auto& event : trace.events) result.push_back(event.activity);
  return result;
}

void grouping_preserves_trace_projection_context() {
  auto log = sample_log();
  auto& trace = log.traces.front();
  trace.case_id_state = minera::FirstClassAttributeState::kPresent;
  trace.case_id_source_key = "case.number";
  minera::EventAttribute selected{"case.number", "42",
                                 minera::AttributeKind::kInt};
  selected.typed_value = std::int64_t{42};
  trace.case_id_attribute = selected;
  trace.properties.push_back({"owner", "team-a", minera::AttributeKind::kString});

  const auto consecutive =
      minera::group_consecutive_activities(log, {});
  assert(consecutive.traces.front().case_id_source_key == "case.number");
  assert(consecutive.traces.front().case_id_attribute &&
         consecutive.traces.front().case_id_attribute->kind ==
             minera::AttributeKind::kInt &&
         std::get<std::int64_t>(
             consecutive.traces.front().case_id_attribute->typed_value) == 42);
  assert(consecutive.traces.front().properties == trace.properties);

  const auto timestamp = minera::group_same_timestamp_events(log, {});
  assert(timestamp.traces.front().case_id_source_key == "case.number");
  assert(timestamp.traces.front().case_id_attribute &&
         timestamp.traces.front().case_id_attribute->kind ==
             minera::AttributeKind::kInt);
}

void grouping_resolves_custom_first_class_selectors() {
  const std::string xml =
      "<log><trace><string key=\"concept:name\" value=\"case\"/>"
      "<event><int key=\"activity.code\" value=\"7\"/>"
      "<date key=\"when.value\" value=\"2026-01-01T00:00:00Z\"/>"
      "</event><event><int key=\"activity.code\" value=\"8\"/>"
      "<date key=\"when.value\" value=\"2026-01-01T00:00:00Z\"/>"
      "</event><event><int key=\"activity.code\" value=\"9\"/>"
      "<date key=\"when.value\" value=\"2026-01-01T00:01:00Z\"/>"
      "</event></trace></log>";
  minera::XesImportOptions import_options;
  import_options.activity_attribute = "activity.code";
  import_options.timestamp_attribute = "when.value";
  std::istringstream input(xml);
  const auto imported = minera::import_xes_event_log(input, import_options);
  assert(imported.traces.front().events.front().activity_attribute &&
         imported.traces.front().events.front().activity_attribute->kind ==
             minera::AttributeKind::kInt);
  assert(imported.traces.front().events.front().timestamp_attribute &&
         imported.traces.front().events.front().timestamp_attribute->kind ==
             minera::AttributeKind::kDate);

  minera::TimestampCaseGroupingOptions options;
  options.filter_type = minera::TimestampCaseGroupingType::kConcat;
  options.activity_key = "activity.code";
  options.timestamp_key = "when.value";
  options.concat_separator = "+";
  const auto grouped = minera::group_same_timestamp_events(imported, options);
  assert(grouped.traces.size() == 1 && grouped.traces.front().events.size() == 2);
  const auto& first = grouped.traces.front().events.front();
  assert(first.activity == "7+8" &&
         first.activity_state == minera::FirstClassAttributeState::kPresent &&
         first.activity_source_key == "activity.code");
  assert(first.activity_attribute &&
         first.activity_attribute->key == "activity.code" &&
         first.activity_attribute->value == "7+8" &&
         first.activity_attribute->kind == minera::AttributeKind::kString &&
         first.activity_attribute->children.empty() &&
         std::holds_alternative<std::monostate>(
             first.activity_attribute->typed_value));
  assert(first.timestamp == "2026-01-01T00:00:00.000Z" &&
         first.timestamp_state == minera::FirstClassAttributeState::kPresent &&
         first.timestamp_source_key == "when.value" &&
         first.timestamp_attribute &&
         first.timestamp_attribute->kind == minera::AttributeKind::kDate);

  const auto exported = minera::to_xes(grouped);
  assert(exported.find("<string key=\"activity.code\" value=\"7+8\"/>") !=
         std::string::npos);
  assert(exported.find(
             "<date key=\"when.value\" value=\"2026-01-01T00:00:00.000Z\"/>") !=
         std::string::npos);
  std::istringstream round_input(exported);
  const auto round = minera::import_xes_event_log(round_input, import_options);
  assert(round.traces.front().events.front().activity == "7+8");
  assert(round.traces.front().events.front().activity_attribute &&
         round.traces.front().events.front().activity_attribute->kind ==
             minera::AttributeKind::kString);
  assert(round.traces.front().events.front().timestamp_attribute &&
         round.traces.front().events.front().timestamp_attribute->kind ==
             minera::AttributeKind::kDate);
}

}  // namespace

int main() {
  const auto log = sample_log();
  grouping_preserves_trace_projection_context();
  grouping_resolves_custom_first_class_selectors();
  minera::ConsecutiveActivityGroupingOptions first;
  const auto first_grouped = minera::group_consecutive_activities(log, first);
  assert(activities(first_grouped.traces[0]) ==
         std::vector<std::string>({"A", "B", "A"}));

  first.filter_type = minera::CaseGroupingFilterType::kLast;
  const auto last_grouped = minera::filter_consecutive_activity_groups(log, first);
  assert(activities(last_grouped.traces[0]) ==
         std::vector<std::string>({"A", "B", "A"}));
  assert(last_grouped.traces[0].events[0].input_order == 1);

  minera::TimestampCaseGroupingOptions timestamp;
  const auto timestamp_first = minera::group_same_timestamp_events(log, timestamp);
  assert(activities(timestamp_first.traces[0]) ==
         std::vector<std::string>({"A", "B"}));
  timestamp.filter_type = minera::TimestampCaseGroupingType::kLast;
  const auto timestamp_last = minera::filter_timestamp_case_groups(log, timestamp);
  assert(activities(timestamp_last.traces[0]) ==
         std::vector<std::string>({"B", "A"}));

  timestamp.filter_type = minera::TimestampCaseGroupingType::kConcat;
  const auto concatenated = minera::group_same_timestamp_events(log, timestamp);
  assert(activities(concatenated.traces[0]) ==
         std::vector<std::string>({"A & A & B", "A & B"}));

  timestamp.activity_key = "channel";
  timestamp.concat_separator = "+";
  minera::EventLog generic = log;
  generic.traces[0].events[0].attributes.push_back(
      {"channel", "z", minera::AttributeKind::kString});
  generic.traces[0].events[1].attributes.push_back(
      {"channel", "a", minera::AttributeKind::kString});
  const auto generic_result = minera::group_same_timestamp_events(generic, timestamp);
  assert(generic_result.traces[0].events[0].attribute("channel") != nullptr);
  assert(*generic_result.traces[0].events[0].attribute("channel") == "a+z");

  timestamp.concat_separator.clear();
  bool threw = false;
  try {
    (void)minera::group_same_timestamp_events(log, timestamp);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  return 0;
}
