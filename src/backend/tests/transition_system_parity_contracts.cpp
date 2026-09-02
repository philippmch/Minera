#include "minera/transition_system.hpp"

#include <algorithm>
#include <cassert>
#include <initializer_list>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using namespace minera;

EventLog make_log() {
  EventLog log;
  Trace first;
  first.case_id = "outer";
  for (const auto [index, activity, case_id] :
       {std::tuple<std::size_t, const char*, const char*>{0, "A", "x"},
        {1, "B", "y"}, {2, "A", "z"}}) {
    Event event;
    event.case_id = case_id;
    event.activity = activity;
    event.input_order = index;
    first.events.push_back(std::move(event));
  }
  log.traces.push_back(std::move(first));
  return log;
}

EventAttribute cell(const char* key, const char* value) {
  return EventAttribute{key, value};
}

EventTable make_table() {
  EventTable table;
  table.columns = {"case_key", "activity_key"};
  table.rows = {{cell("case_key", "c1"), cell("activity_key", "A")},
               {cell("case_key", "c2"), cell("activity_key", "X")},
               {cell("case_key", "c1"), cell("activity_key", "B")},
               {cell("case_key", "c2"), cell("activity_key", "Y")},
               {cell("case_key", "c1"), cell("activity_key", "A")},
               {cell("case_key", "c2"), cell("activity_key", "X")}};
  return table;
}

EventStream make_stream(const EventTable& table) {
  EventStream stream;
  for (const auto& row : table.rows) {
    EventRecord record;
    record.fields = {row[0].value(), row[1].value()};
    stream.events.push_back(std::move(record));
  }
  return stream;
}

EventLog make_first_class_log() {
  EventLog log;
  Trace trace;
  trace.case_id = "outer";
  trace.case_id_state = FirstClassAttributeState::kPresent;
  trace.case_id_source_key = "case_col";
  for (const auto [index, activity] :
       {std::tuple<std::size_t, const char*>{0, "A"},
        {1, "B"}, {2, "A"}}) {
    Event event;
    event.case_id = "selected";
    event.case_id_state = FirstClassAttributeState::kPresent;
    event.case_id_source_key = "case_col";
    event.activity = activity;
    event.activity_state = FirstClassAttributeState::kPresent;
    event.activity_source_key = "activity_col";
    event.activity_attribute = EventAttribute{
        "activity_col", activity, AttributeKind::kString, false, {},
        AttributeContainerKind::kScalar, {}, FirstClassAttributeRole::kActivity,
        FirstClassAttributeState::kPresent, "activity_col"};
    event.input_order = index;
    trace.events.push_back(std::move(event));
  }
  log.traces.push_back(std::move(trace));
  return log;
}

void assert_flat_shape(const TransitionSystem& system) {
  assert(system.options().window == 1);
  assert(system.options().activity_key == "activity_key");
  assert(system.options().case_id_key == "case_key");
  assert(system.state_count() == 5);
  assert(system.transition_count() == 6);
  assert(system.transitions().front().occurrences.empty());
}

void transition_system_event_log_boundary_contract() {
  auto options = TransitionSystemOptions{};
  options.window = 1;
  options.include_data = true;
  const auto system = discover_transition_system(make_log(), options);
  assert(system.state_count() == 3);
  assert(system.transition_count() == 3);
  assert(system.transitions().front().occurrences.size() == 1);
  assert(std::any_of(system.states().begin(), system.states().end(),
                     [](const auto& state) { return state.start_count == 1; }));
}

void transition_system_flat_adapter_contract() {
  auto options = TransitionSystemOptions{};
  options.window = 1;
  options.activity_key = "activity_key";
  options.case_id_key = "case_key";
  const auto table = make_table();
  const auto stream = make_stream(table);
  const auto eager = discover_transition_system(table, options);
  const auto lazy = discover_transition_system(LazyEventTable::from_table(table),
                                                options);
  const auto streamed = discover_transition_system(stream, options);
  const auto viewed = discover_transition_system(EventDataView(table), options);
  assert_flat_shape(eager);
  assert_flat_shape(lazy);
  assert_flat_shape(streamed);
  assert_flat_shape(viewed);
  assert(eager.transitions() == streamed.transitions());
  assert(eager.transitions() == lazy.transitions());
}

void transition_system_first_class_selector_contract() {
  auto options = TransitionSystemOptions{};
  options.window = 1;
  options.activity_attribute = "activity_col";
  options.case_id_attribute = "case_col";
  const auto system = discover_transition_system(make_first_class_log(), options);
  assert(system.state_count() == 3);
  assert(system.transition_count() == 3);
}

void transition_system_view_based_selector_contract() {
  const auto log = make_log();
  auto options = TransitionSystemOptions{};
  options.window = 0;
  const auto direct = discover_view_based_transition_system(log, options);
  const auto callable = ViewBasedTransitionSystemDiscovery::apply(log, options);
  assert(direct.transitions() == callable.transitions());
  assert(TransitionSystemSelectorRegistry::all().size() == 1);
  assert(TransitionSystemSelectorRegistry::supports(
      TransitionSystemVariant::VIEW_BASED));
  const auto parsed = TransitionSystemSelectorRegistry::parse("view_based");
  assert(parsed && *parsed == TransitionSystemVariant::VIEW_BASED);
  assert(transition_system_variant_name(*parsed) == "VIEW_BASED");
  assert(parse_transition_system_variant("VIEW_BASED") ==
         TransitionSystemVariant::VIEW_BASED);
  const auto selected = TransitionSystemSelectorRegistry::dispatch(
      log, TransitionSystemVariant::VIEW_BASED, options);
  assert(selected.transitions() == direct.transitions());
  assert(TransitionSystemSelectorRegistry::dispatch(
             EventDataView(make_table()), TransitionSystemVariant::VIEW_BASED,
             TransitionSystemOptions{.window = 1,
                                      .activity_key = "activity_key",
                                      .case_id_key = "case_key"})
             .state_count() == 5);
}

}  // namespace

int main() {
  transition_system_event_log_boundary_contract();
  transition_system_flat_adapter_contract();
  transition_system_first_class_selector_contract();
  transition_system_view_based_selector_contract();
  return 0;
}
