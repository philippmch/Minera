#include "minera/transition_system.hpp"

#include <algorithm>
#include <cassert>
#include <initializer_list>
#include <optional>
#include <stdexcept>
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

EventLog nullable_log(std::initializer_list<TransitionActivity> activities) {
  EventLog log;
  Trace trace;
  for (const auto& activity : activities) {
    Event event;
    event.activity_state = activity ? FirstClassAttributeState::kPresent : FirstClassAttributeState::kNull;
    event.activity_source_key = "concept:name";
    event.activity = activity.value_or("");
    trace.events.push_back(std::move(event));
  }
  log.traces.push_back(std::move(trace));
  return log;
}

// Python's repr of a Counter state name: keys by descending count, ties in
// first-seen order.
std::string counter_name(const TransitionSystemState& state) {
  std::vector<std::pair<std::string, std::size_t>> counts;
  for (const auto& key : state.name_order) {
    std::size_t count = 0;
    for (const auto& item : state.payload) count += item == key ? 1 : 0;
    counts.emplace_back(key.value_or("None"), count);
  }
  std::stable_sort(counts.begin(), counts.end(),
                   [](const auto& a, const auto& b) { return a.second > b.second; });
  if (counts.empty()) return "Counter()";
  std::string name = "Counter({";
  for (std::size_t i = 0; i < counts.size(); ++i) {
    if (i) name += ", ";
    name += "'" + counts[i].first + "': " + std::to_string(counts[i].second);
  }
  return name + "})";
}

std::vector<std::string> sorted_names(std::vector<std::string> names) {
  std::sort(names.begin(), names.end());
  return names;
}

std::vector<std::string> root_state_names(const TransitionSystem& system) {
  std::vector<std::string> names;
  for (const auto& state : system.states())
    if (state.in_root) names.push_back(counter_name(state));
  std::sort(names.begin(), names.end());
  return names;
}

std::vector<std::string> transition_names(const TransitionSystem& system) {
  std::vector<std::string> names;
  for (const auto& transition : system.transitions())
    names.push_back(counter_name(system.states()[transition.source]) + " -" +
                    transition.label.value_or("None") + "-> " +
                    counter_name(system.states()[transition.target]));
  std::sort(names.begin(), names.end());
  return names;
}

// pm4py 2.7.22 captures of discover_transition_system(window=2,
// view="multiset"): the reference looks a state up by its Counter value
// among the registered states, so equal-valued windows reached from either
// side share one state (backward), named after the first registration.
void transition_system_multiset_value_lookup_contract() {
  auto options = TransitionSystemOptions{};
  options.window = 2;
  options.view = TransitionView::kMultiset;
  options.direction = TransitionDirection::kBackward;
  const auto aba = discover_transition_system(nullable_log({"A", "B", "A"}), options);
  assert(aba.root_state_count() == 3);
  assert((root_state_names(aba) == sorted_names({
      "Counter()", "Counter({'A': 1, 'B': 1})", "Counter({'A': 1})"})));
  assert((transition_names(aba) == sorted_names({
      "Counter() -A-> Counter({'A': 1})",
      "Counter({'A': 1, 'B': 1}) -A-> Counter({'A': 1, 'B': 1})",
      "Counter({'A': 1}) -B-> Counter({'A': 1, 'B': 1})"})));
  const auto dadc = discover_transition_system(nullable_log({"d", "a", "d", "c"}), options);
  assert(dadc.root_state_count() == 4);
  assert((root_state_names(dadc) == sorted_names({
      "Counter()", "Counter({'d': 1, 'a': 1})", "Counter({'d': 1, 'c': 1})",
      "Counter({'d': 1})"})));
  assert((transition_names(dadc) == sorted_names({
      "Counter() -d-> Counter({'d': 1})",
      "Counter({'d': 1, 'a': 1}) -c-> Counter({'d': 1, 'c': 1})",
      "Counter({'d': 1, 'a': 1}) -d-> Counter({'d': 1, 'a': 1})",
      "Counter({'d': 1}) -a-> Counter({'d': 1, 'a': 1})"})));
  options.direction = TransitionDirection::kForward;
  const auto forward = discover_transition_system(nullable_log({"d", "a", "d", "c"}), options);
  assert(forward.root_state_count() == 5);
  assert((root_state_names(forward) == sorted_names({
      "Counter()", "Counter({'a': 1, 'd': 1})", "Counter({'c': 1})",
      "Counter({'d': 1, 'a': 1})", "Counter({'d': 1, 'c': 1})"})));
  assert((transition_names(forward) == sorted_names({
      "Counter({'c': 1}) -c-> Counter()",
      "Counter({'d': 1, 'a': 1}) -a-> Counter({'d': 1, 'c': 1})",
      "Counter({'d': 1, 'a': 1}) -d-> Counter({'a': 1, 'd': 1})",
      "Counter({'d': 1, 'c': 1}) -d-> Counter({'c': 1})"})));
}

void transition_system_nullable_and_missing_contract() {
  auto options = TransitionSystemOptions{};
  options.include_data = true;
  const auto singleton = discover_transition_system(nullable_log({std::nullopt}), options);
  assert(singleton.states().size() == 2 && singleton.root_state_count() == 2);
  assert(singleton.transitions().size() == 1 && !singleton.transitions()[0].label);
  assert(singleton.states()[singleton.transitions()[0].source].payload == TransitionPayload{std::nullopt});
  assert(singleton.states()[singleton.transitions()[0].target].payload.empty());
  const auto distinct = discover_transition_system(nullable_log({std::nullopt, "", "None", "null", std::nullopt}), options);
  assert(std::any_of(distinct.transitions().begin(), distinct.transitions().end(), [](const auto& e) { return e.label == TransitionActivity{""}; }));
  assert(std::any_of(distinct.transitions().begin(), distinct.transitions().end(), [](const auto& e) { return e.label == TransitionActivity{"None"}; }));
  assert(std::any_of(distinct.transitions().begin(), distinct.transitions().end(), [](const auto& e) { return e.label == TransitionActivity{"null"}; }));
  auto phase = nullable_log({"ignored"});
  EventAttribute attribute;
  attribute.key = "phase";
  attribute.is_null = true;
  phase.traces[0].events[0].attributes.push_back(attribute);
  options.activity_key = "phase";
  assert(!discover_transition_system(phase, options).transitions()[0].label);
  phase.traces[0].events[0].attributes.clear();
  bool missing = false;
  try { (void)discover_transition_system(phase, options); }
  catch (const std::invalid_argument& error) { missing = std::string(error.what()) == "'phase'"; }
  assert(missing);
}

void transition_system_endpoint_data_and_counter_contract() {
  auto options = TransitionSystemOptions{};
  options.window = 0;
  options.include_data = true;
  const auto zero = discover_transition_system(nullable_log({"A", "A", "A"}), options);
  assert(zero.states().size() == 2 && zero.root_state_count() == 1);
  assert(zero.states()[0].in_root && !zero.states()[1].in_root);
  assert(zero.transitions().size() == 1 && zero.transitions()[0].source == 0 && zero.transitions()[0].target == 1);
  assert(zero.states()[0].incoming.empty() && zero.states()[0].outgoing == std::vector<std::size_t>{0});
  assert(zero.states()[1].incoming == std::vector<std::size_t>{0} && zero.states()[1].outgoing.empty());
  assert((zero.states()[0].incoming_occurrences == std::vector<TransitionOccurrence>{{0, 1}, {0, 2}}));
  assert((zero.states()[1].incoming_occurrences == std::vector<TransitionOccurrence>{{0, 0}}));
  assert(zero.transitions()[0].occurrences.size() == 3);
  options.window = 2;
  options.view = TransitionView::kMultiset;
  const auto counter = discover_transition_system(nullable_log({"A", "B", "A"}), options);
  assert(counter.states().size() == 4 && counter.root_state_count() == 4);
  std::vector<TransitionPayload> orders;
  for (const auto& state : counter.states()) if (state.payload == TransitionPayload{"A", "B"}) orders.push_back(state.name_order);
  assert(orders.size() == 2 && orders[0] != orders[1]);
}

void transition_system_signed_window_contract() {
  auto options = TransitionSystemOptions{};
  options.window = -1;
  options.include_data = true;
  const auto forward = discover_transition_system(nullable_log({"A", std::nullopt, "A"}), options);
  assert(forward.states().size() == 2 && forward.root_state_count() == 2);
  assert(std::any_of(forward.states().begin(), forward.states().end(), [](const auto& state) { return state.payload == TransitionPayload{"A", std::nullopt}; }));
  options.direction = TransitionDirection::kBackward;
  const auto backward = discover_transition_system(nullable_log({"A", std::nullopt, "A"}), options);
  assert(backward.states().size() == 2 && backward.root_state_count() == 1);
  assert(backward.transitions().size() == 2);
  assert(std::all_of(backward.states().begin(), backward.states().end(), [](const auto& state) { return state.payload.empty(); }));
}

}  // namespace

int main() {
  transition_system_event_log_boundary_contract();
  transition_system_flat_adapter_contract();
  transition_system_first_class_selector_contract();
  transition_system_view_based_selector_contract();
  transition_system_nullable_and_missing_contract();
  transition_system_endpoint_data_and_counter_contract();
  transition_system_signed_window_contract();
  transition_system_multiset_value_lookup_contract();
  return 0;
}
