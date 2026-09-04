#include "minera/dfg_precision.hpp"

#include <cassert>
#include <cmath>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace minera;

EventLog log(std::initializer_list<std::vector<std::string>> traces) {
  EventLog result;
  std::size_t order = 0;
  std::size_t case_number = 0;
  for (const auto& activities : traces) {
    Trace trace{"case" + std::to_string(case_number++), {}, {}};
    for (const auto& activity : activities)
      trace.events.push_back(
          Event{trace.case_id, activity, "", "", order++, {}, ""});
    result.traces.push_back(std::move(trace));
  }
  return result;
}

DfgCleanAnalysis model(
    std::initializer_list<std::pair<std::string, std::string>> edges,
    std::initializer_list<std::string> starts,
    std::initializer_list<std::string> ends) {
  DfgCleanAnalysis result;
  for (const auto& edge : edges) result.graph[edge] = 1;
  for (const auto& activity : starts) result.start_activities[activity] = 1;
  for (const auto& activity : ends) result.end_activities[activity] = 1;
  return result;
}

EventStream flat_stream() {
  EventStream stream;
  for (const auto& row : {std::pair{"case-a", "A"},
                          std::pair{"case-a", "B"},
                          std::pair{"case-b", "A"},
                          std::pair{"case-b", "C"}}) {
    EventRecord record;
    record.fields.push_back({"case_id", row.first, AttributeKind::kString});
    record.fields.push_back({"label", row.second, AttributeKind::kString});
    stream.events.push_back(std::move(record));
  }
  return stream;
}

EventTable flat_table() {
  EventTable table;
  table.columns = {"case_id", "label"};
  for (const auto& row : {std::pair{"case-a", "A"},
                          std::pair{"case-a", "B"},
                          std::pair{"case-b", "A"},
                          std::pair{"case-b", "C"}}) {
    EventTable::Row values;
    values.emplace_back(
        EventAttribute{"case_id", row.first, AttributeKind::kString});
    values.emplace_back(
        EventAttribute{"label", row.second, AttributeKind::kString});
    table.rows.push_back(std::move(values));
  }
  return table;
}

LazyEventTable flat_lazy_table() {
  return LazyEventTable(
      {"case_id", "label"},
      [](const LazyEventTable::RowConsumer& consume) {
        for (const auto& row : {std::pair{"case-a", "A"},
                                std::pair{"case-a", "B"},
                                std::pair{"case-b", "A"},
                                std::pair{"case-b", "C"}}) {
          LazyEventTable::Row values;
          values.emplace_back(
              EventAttribute{"case_id", row.first, AttributeKind::kString});
          values.emplace_back(
              EventAttribute{"label", row.second, AttributeKind::kString});
          consume(values);
        }
      },
      {}, {}, 4);
}

int main() {
  assert(DfgPrecisionOptions{}.activity_attribute == "concept:name");
  const auto precise = model({{"A", "B"}, {"A", "C"}}, {"A"}, {"B", "C"});
  const auto perfect = evaluate_dfg_precision(log({{"A", "B"}, {"A", "C"}}), precise);
  assert(perfect.computed && perfect.precision == 1.0);
  assert(perfect.escaping_transitions == 0);
  assert(perfect.activated_transitions == 6);

  const auto escaping_model = model({{"A", "B"}, {"A", "C"}}, {"A"}, {"B"});
  const auto escaping = evaluate_dfg_precision(
      log({{"A", "B"}, {"A", "B"}}), escaping_model);
  assert(escaping.computed);
  assert(std::abs(escaping.precision - (2.0 / 3.0)) < 1e-12);
  assert(escaping.prefix_occurrences == 4);
  assert(escaping.allowed_prefix_occurrences == 4);
  assert(escaping.activated_transitions == 6);
  assert(escaping.escaping_transitions == 2);
  const auto json = serialize_dfg_precision_json(escaping);
  assert(json.find("DFG_PRECISION") != std::string::npos);

  const auto blocked = evaluate_dfg_precision(
      log({{"X", "B"}}), model({{"A", "B"}}, {"A"}, {}));
  assert(blocked.precision == 0.0);
  assert(blocked.activated_transitions == 1);
  assert(blocked.escaping_transitions == 1);

  EventLog custom = log({{"A", "B"}});
  custom.traces.front().events[0].attributes.push_back(
      {"label", "A", AttributeKind::kString});
  custom.traces.front().events[1].attributes.push_back(
      {"label", "B", AttributeKind::kString});
  const auto custom_result = evaluate_dfg_precision(
      custom, model({{"A", "B"}}, {"A"}, {}),
      DfgPrecisionOptions{"label"});
  assert(custom_result.precision == 1.0);

  // Imported graph maps may retain declared relations with a zero frequency.
  // Precision is relation-based, so those entries still define the model's
  // enabled edges and starts.
  DfgCleanAnalysis zero_frequency;
  zero_frequency.graph[{"A", "B"}] = 0;
  zero_frequency.start_activities["A"] = 0;
  const auto zero_frequency_result = evaluate_dfg_precision(
      log({{"A", "B"}}), zero_frequency);
  assert(zero_frequency_result.computed);
  assert(zero_frequency_result.precision == 1.0);
  assert(zero_frequency_result.activated_transitions == 2);
  assert(zero_frequency_result.escaping_transitions == 0);

  bool threw = false;
  try {
    (void)evaluate_dfg_precision(log({{"A"}}), precise,
                                 DfgPrecisionOptions{"missing"});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  DfgPrecisionOptions adapter_options;
  adapter_options.activity_attribute = "label";
  adapter_options.case_id_attribute = "case_id";
  const auto stream_result =
      evaluate_dfg_precision(flat_stream(), precise, adapter_options);
  const auto table_result =
      evaluate_dfg_precision(flat_table(), precise, adapter_options);
  const auto lazy = flat_lazy_table();
  const auto lazy_result =
      evaluate_dfg_precision(lazy, precise, adapter_options);
  assert(stream_result.computed && table_result.computed &&
         lazy_result.computed && stream_result.precision == 1.0 &&
         table_result.precision == 1.0 && lazy_result.precision == 1.0);

  // A stream can carry a first-class activity under a canonical key while
  // retaining the caller's source spelling. The requested source selector
  // must still win, and the requested case field must drive grouping.
  auto projected = flat_stream();
  for (auto& record : projected.events) {
    auto& activity = record.fields[1];
    activity.key = "renamed_label";
    activity.first_class_role = FirstClassAttributeRole::kActivity;
    activity.first_class_state = FirstClassAttributeState::kPresent;
    activity.first_class_source_key = "label";
    record.fields.push_back(
        {"concept:name", "wrong", AttributeKind::kString});
    auto& case_field = record.fields[0];
    case_field.first_class_role = FirstClassAttributeRole::kCaseId;
    case_field.first_class_state = FirstClassAttributeState::kPresent;
    case_field.first_class_source_key = "case_id";
  }
  const auto projected_result =
      evaluate_dfg_precision(projected, precise, adapter_options);
  assert(projected_result.computed && projected_result.precision == 1.0);

  auto exact_activity = flat_stream();
  for (auto& record : exact_activity.events) {
    record.fields[1].key = "activity";
    record.fields.push_back(
        {"concept:name", "wrong", AttributeKind::kString});
  }
  auto exact_options = adapter_options;
  exact_options.activity_attribute = "activity";
  const auto exact_result =
      evaluate_dfg_precision(exact_activity, precise, exact_options);
  assert(exact_result.computed && exact_result.precision == 1.0);

  auto alternate_case = flat_stream();
  for (std::size_t index = 0; index < alternate_case.events.size(); ++index) {
    auto& old_case = alternate_case.events[index].fields[0];
    old_case.value = "legacy-case";
    old_case.first_class_role = FirstClassAttributeRole::kCaseId;
    old_case.first_class_state = FirstClassAttributeState::kPresent;
    old_case.first_class_source_key = "legacy_case";
    alternate_case.events[index].fields.push_back(
        {"new_case", index < 2 ? "case-a" : "case-b",
         AttributeKind::kString});
  }
  adapter_options.case_id_attribute = "new_case";
  const auto alternate_result =
      evaluate_dfg_precision(alternate_case, precise, adapter_options);
  assert(alternate_result.computed && alternate_result.precision == 1.0 &&
         alternate_result.allowed_prefix_occurrences == 4);
  adapter_options.case_id_attribute = "case_id";

  auto conflicting = flat_stream();
  conflicting.events.front().fields.push_back(
      {"label", "A", AttributeKind::kString});
  threw = false;
  try {
    (void)evaluate_dfg_precision(conflicting, precise, adapter_options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  adapter_options.max_rows_to_materialize = 3;
  threw = false;
  try {
    (void)evaluate_dfg_precision(lazy, precise, adapter_options);
  } catch (const std::length_error&) {
    threw = true;
  }
  assert(threw);
  return 0;
}
