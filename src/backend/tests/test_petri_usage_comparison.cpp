#include <cassert>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "minera/csv_event_log_importer.hpp"
#include "minera/json_export.hpp"
#include "minera/petri_usage_comparison.hpp"
#include "minera/xes_event_log_importer.hpp"

namespace {

minera::EventLog log_of(std::initializer_list<std::string> activities) {
  minera::Trace trace;
  trace.case_id = "case";
  for (const auto& activity : activities) {
    minera::Event event;
    event.case_id = trace.case_id;
    event.activity = activity;
    trace.events.push_back(std::move(event));
  }
  return minera::EventLog{{std::move(trace)}};
}

minera::PetriNet simple_net() {
  minera::PetriNet net;
  const auto p0 = net.add_place();
  const auto p1 = net.add_place();
  const auto p2 = net.add_place();
  const auto p3 = net.add_place();  // deliberately unused
  (void)p3;
  net.set_source(p0);
  net.set_sink(p2);
  net.set_activities({"A"});
  net.add_transition(0, {p0}, {p1});
  net.add_transition(std::nullopt, {p1}, {p2});
  return net;
}

void usage_directions_and_silent_activation() {
  const auto net = simple_net();
  const auto one = log_of({"A"});
  const auto empty = log_of({});
  const auto equal = minera::compare_petri_usage(net, one, one);
  assert(equal.transitions.size() == 2);
  assert(equal.transitions[0].log1_occurrences == 1);
  assert(equal.transitions[0].log2_occurrences == 1);
  assert(equal.transitions[0].direction == 0.0);
  assert(equal.transitions[0].label == "A (1/1/2)");
  assert(equal.transitions[0].color == "#AAAAAA");
  assert(equal.transitions[1].label == "(1/1/2)");
  assert(equal.arcs[0].label == equal.transitions[0].label);
  assert(equal.arcs[0].color == equal.transitions[0].color);
  assert(equal.transitions[1].log1_occurrences == 1);  // silent routing
  assert(equal.places[3].total_occurrences == 0);
  assert(equal.places[3].direction == 0.0);  // zero-total is finite and neutral

  const auto negative = minera::compare_element_usage_two_logs(net, one, empty);
  assert(negative.transitions[0].direction == -1.0);
  assert(negative.transitions[1].direction == -1.0);
  assert(negative.transitions[0].color == "#00BFFF");
  const auto positive = minera::compare_petri_usage(net, empty, one);
  assert(positive.transitions[0].direction == 1.0);
  assert(positive.transitions[0].color == "#FF6347");
  assert(std::isfinite(positive.places[0].direction));
}

void weighted_arc_and_duplicate_label_contract() {
  minera::PetriNet net;
  const auto p0 = net.add_place();
  const auto p1 = net.add_place();
  const auto p2 = net.add_place();
  net.set_source(p0);
  net.set_sink(p2);
  net.set_activities({"A"});
  net.add_transition(0, {p0, p0}, {p1});  // weighted input arc
  net.add_transition(0, {p1}, {p2});      // duplicate label, distinct index

  const auto result = minera::compare_petri_usage(net, log_of({"A"}), log_of({}));
  assert(result.transitions.size() == 2);
  assert(result.transitions[0].log1_occurrences == 1);
  assert(result.transitions[1].log1_occurrences == 0);
  assert(result.places[p0].log1_occurrences == 2);  // weighted consumption
  assert(result.arcs.size() == 5);
  assert(result.arcs[0].place == p0 && result.arcs[1].place == p0);
  assert(result.arcs[0].total_occurrences == result.transitions[0].total_occurrences);
}

void finish_routes_when_sink_has_a_token_and_other_work_remains() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto branch = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A"});
  net.add_transition(0, {source}, {sink, branch});
  net.add_transition(std::nullopt, {branch, sink}, {sink});

  const auto result = minera::compare_petri_usage(net, log_of({"A"}),
                                                   log_of({}));
  assert(result.transitions[1].log1_occurrences == 1);
  assert(result.places[branch].log1_occurrences == 1);
}

void explicit_markings_control_replay_boundaries() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto entry = net.add_place();
  const auto after = net.add_place();
  const auto alternate_final = net.add_place();
  net.set_source(source);
  net.set_sink(after);
  net.set_activities({"A"});
  net.add_transition(std::nullopt, {source}, {entry});
  net.add_transition(0, {entry}, {after});
  net.add_transition(std::nullopt, {after}, {alternate_final});

  minera::PetriUsageComparisonOptions options;
  options.initial_marking = {0, 1, 0, 0};
  options.final_marking = {0, 0, 0, 1};
  const auto result = minera::compare_petri_usage(
      net, log_of({"A"}), log_of({}), options);
  assert(result.transitions[0].log1_occurrences == 0);
  assert(result.transitions[1].log1_occurrences == 1);
  assert(result.transitions[2].log1_occurrences == 1);
  assert(result.places[alternate_final].log1_occurrences == 1);
}

void replay_controls_are_honored() {
  minera::PetriNet routed;
  const auto source = routed.add_place();
  const auto entry = routed.add_place();
  const auto sink = routed.add_place();
  routed.set_source(source);
  routed.set_sink(sink);
  routed.set_activities({"A"});
  routed.add_transition(std::nullopt, {source}, {entry});
  routed.add_transition(0, {entry}, {sink});

  const auto default_routing =
      minera::compare_petri_usage(routed, log_of({"A"}), log_of({}));
  assert(default_routing.transitions[0].log1_occurrences == 1);
  assert(default_routing.transitions[1].log1_occurrences == 1);

  minera::PetriUsageComparisonOptions no_entry_routing;
  no_entry_routing.walk_through_hidden_trans = false;
  const auto forced = minera::compare_petri_usage(
      routed, log_of({"A"}), log_of({}), no_entry_routing);
  assert(forced.transitions[0].log1_occurrences == 0);
  assert(forced.transitions[1].log1_occurrences == 1);

  minera::PetriUsageComparisonOptions no_final_routing;
  no_final_routing.try_to_reach_final_marking_through_hidden = false;
  const auto unfinished = minera::compare_petri_usage(
      simple_net(), log_of({"A"}), log_of({}), no_final_routing);
  assert(unfinished.transitions[0].log1_occurrences == 1);
  assert(unfinished.transitions[1].log1_occurrences == 0);
  assert(unfinished.places[1].log1_occurrences == 1);

  minera::PetriUsageComparisonOptions stop;
  stop.stop_immediately_unfit = true;
  const auto stopped_on_unknown = minera::compare_petri_usage(
      simple_net(), log_of({"unknown", "A"}), log_of({}), stop);
  assert(stopped_on_unknown.transitions[0].log1_occurrences == 0);
  const auto continued_after_unknown = minera::compare_petri_usage(
      simple_net(), log_of({"unknown", "A"}), log_of({}));
  assert(continued_after_unknown.transitions[0].log1_occurrences == 1);

  const auto stopped_after_forced = minera::compare_petri_usage(
      simple_net(), log_of({"A", "A", "A"}), log_of({}), stop);
  assert(stopped_after_forced.transitions[0].log1_occurrences == 2);
  const auto continued_after_forced = minera::compare_petri_usage(
      simple_net(), log_of({"A", "A", "A"}), log_of({}));
  assert(continued_after_forced.transitions[0].log1_occurrences == 3);
}

void extended_arcs_are_replayed_and_counted() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto buffered = net.add_place();
  const auto gate = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A"});
  net.add_transition(0, {source}, {buffered, buffered, gate});
  const auto reset = net.add_transition(std::nullopt, {gate}, {sink});
  net.add_reset_arc(reset, buffered);

  const auto result = minera::compare_petri_usage(
      net, log_of({"A"}), log_of({}));
  assert(result.transitions[1].log1_occurrences == 1);
  assert(result.places[buffered].log1_occurrences == 2);
  assert(result.arcs.size() == 7);
  assert(result.arcs.back().transition == reset);
  assert(result.arcs.back().place == buffered);
  assert(result.arcs.back().kind == minera::PetriArcUsageKind::kReset);
  assert(result.arcs.back().input);
  assert(result.arcs.back().log1_occurrences == 1);
  const auto json = minera::petri_usage_comparison_json(result);
  assert(json.find("\"kind\":\"reset\"") != std::string::npos);
}

void inhibitor_arcs_gate_silent_completion() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto guard = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(std::nullopt, {source}, {sink});
  net.add_inhibitor_arc(0, guard);

  const auto enabled_result = minera::compare_petri_usage(
      net, log_of({}), log_of({}));
  assert(enabled_result.transitions[0].log1_occurrences == 1);
  assert(enabled_result.arcs.size() == 3);
  assert(enabled_result.arcs.back().place == guard);
  assert(enabled_result.arcs.back().kind ==
         minera::PetriArcUsageKind::kInhibitor);
  assert(enabled_result.arcs.back().input);
  assert(enabled_result.arcs.back().log1_occurrences == 1);
  assert(minera::petri_usage_comparison_json(enabled_result).find(
             "\"kind\":\"inhibitor\"") != std::string::npos);

  minera::PetriUsageComparisonOptions blocked_options;
  blocked_options.initial_marking = {1, 1, 0};
  blocked_options.final_marking = {1, 1, 0};
  const auto blocked_result = minera::compare_petri_usage(
      net, log_of({}), log_of({}), blocked_options);
  assert(blocked_result.transitions[0].log1_occurrences == 0);
}

void marking_dimensions_are_checked() {
  const auto net = simple_net();
  minera::PetriUsageComparisonOptions options;
  options.initial_marking = {1};
  bool rejected = false;
  try {
    (void)minera::compare_petri_usage(net, log_of({}), log_of({}), options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void custom_activity_attribute_controls_replay() {
  const auto net = simple_net();
  minera::EventLog selected;
  minera::Trace trace;
  trace.case_id = "custom";
  minera::Event event;
  event.case_id = trace.case_id;
  event.activity = "ignored";
  event.attributes.push_back({"business_activity", "A",
                              minera::AttributeKind::kString});
  trace.events.push_back(std::move(event));
  selected.traces.push_back(std::move(trace));

  minera::PetriUsageComparisonOptions options;
  options.activity_attribute = "business_activity";
  const auto custom = compare_petri_usage(net, selected, log_of({}), options);
  assert(custom.transitions[0].log1_occurrences == 1);
  assert(custom.transitions[1].log1_occurrences == 1);

  options.activity_attribute = "missing_activity";
  const auto missing = compare_petri_usage(net, selected, log_of({}), options);
  assert(missing.transitions[0].log1_occurrences == 0);
  assert(missing.transitions[1].log1_occurrences == 0);

  options.activity_attribute = "concept:name";
  const auto canonical = compare_petri_usage(net, log_of({"A"}), log_of({}),
                                             options);
  assert(canonical.transitions[0].log1_occurrences == 1);

  selected.traces.front().events.front().attributes.clear();
  selected.traces.front().events.front().resource = "A";
  for (const auto attribute : {"resource", "org:resource"}) {
    options.activity_attribute = attribute;
    const auto resource =
        compare_petri_usage(net, selected, log_of({}), options);
    assert(resource.transitions[0].log1_occurrences == 1);
  }

  selected.traces.front().events.front().resource.clear();
  selected.traces.front().events.front().timestamp = "A";
  for (const auto attribute : {"timestamp", "time:timestamp"}) {
    options.activity_attribute = attribute;
    const auto timestamp =
        compare_petri_usage(net, selected, log_of({}), options);
    assert(timestamp.transitions[0].log1_occurrences == 1);
  }

  selected.traces.front().events.front().attributes.clear();
  selected.traces.front().events.front().timestamp.clear();
  selected.traces.front().events.front().lifecycle = "A";
  for (const auto attribute : {"lifecycle", "lifecycle:transition"}) {
    options.activity_attribute = attribute;
    const auto lifecycle =
        compare_petri_usage(net, selected, log_of({}), options);
    assert(lifecycle.transitions[0].log1_occurrences == 1);
  }
}

void aliases_prefer_first_class_fields_and_reject_blank_selectors() {
  const auto net = simple_net();
  minera::EventLog selected;
  minera::Trace trace;
  trace.case_id = "aliases";
  minera::Event event;
  event.case_id = trace.case_id;
  event.activity = "ignored";
  event.resource = "A";
  event.attributes.push_back({"resource", "not-the-resource",
                              minera::AttributeKind::kString});
  trace.events.push_back(std::move(event));
  selected.traces.push_back(std::move(trace));

  minera::PetriUsageComparisonOptions options;
  options.activity_attribute = "  org:resource  ";
  const auto result = compare_petri_usage(net, selected, log_of({}), options);
  assert(result.transitions[0].log1_occurrences == 1);

  options.activity_attribute = " \t ";
  bool rejected = false;
  try {
    (void)compare_petri_usage(net, selected, log_of({}), options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void imported_custom_activity_attributes_are_replayable() {
  const auto net = simple_net();

  minera::CsvImportOptions csv_options;
  csv_options.activity_column = "business_activity";
  std::istringstream csv("case_id,business_activity\ncase-1,A\n");
  const auto csv_log = minera::import_csv_event_log(csv, csv_options);
  assert(csv_log.traces.size() == 1);
  assert(csv_log.traces.front().events.front().activity == "A");
  assert(csv_log.traces.front().events.front().attribute("business_activity") !=
         nullptr);

  minera::PetriUsageComparisonOptions options;
  options.activity_attribute = "business_activity";
  const auto csv_result = compare_petri_usage(net, csv_log, log_of({}), options);
  assert(csv_result.transitions[0].log1_occurrences == 1);

  minera::XesImportOptions xes_options;
  xes_options.activity_attribute = "business_activity";
  std::istringstream xes(
      "<log><trace><string key=\"concept:name\" value=\"case-1\"/>"
      "<event><string key=\"business_activity\" value=\"A\"/>"
      "</event></trace></log>");
  const auto xes_log = minera::import_xes_event_log(xes, xes_options);
  assert(xes_log.traces.front().events.front().activity == "A");
  assert(xes_log.traces.front().events.front().attribute("business_activity") !=
         nullptr);
  const auto xes_result = compare_petri_usage(net, xes_log, log_of({}), options);
  assert(xes_result.transitions[0].log1_occurrences == 1);
}

void imported_first_class_aliases_work_without_default_activity_columns() {
  const auto net = simple_net();
  const std::vector<std::pair<const char*, const char*>> csv_aliases = {
      {"org:resource", "resource"},
      {"time:timestamp", "timestamp"},
      {"lifecycle:transition", "lifecycle"},
  };
  for (const auto& [selector, column] : csv_aliases) {
    minera::CsvImportOptions import_options;
    import_options.activity_column = selector;
    std::istringstream csv(std::string("case_id,") + column +
                           "\ncase-1,A\n");
    const auto log = minera::import_csv_event_log(csv, import_options);
    assert(log.traces.size() == 1);
    minera::PetriUsageComparisonOptions comparison_options;
    comparison_options.activity_attribute = selector;
    const auto result =
        compare_petri_usage(net, log, log_of({}), comparison_options);
    assert(result.transitions[0].log1_occurrences == 1);
  }

  const std::vector<std::pair<const char*, const char*>> xes_aliases = {
      {"org:resource", "org:resource"},
      {"time:timestamp", "time:timestamp"},
      {"lifecycle:transition", "lifecycle:transition"},
  };
  for (const auto& [selector, key] : xes_aliases) {
    minera::XesImportOptions import_options;
    import_options.activity_attribute = selector;
    std::istringstream xes(
        std::string("<log><trace><string key=\"concept:name\" value=\"case-1\"/>"
                    "<event><string key=\"") +
        key + "\" value=\"A\"/></event></trace></log>");
    const auto log = minera::import_xes_event_log(xes, import_options);
    assert(log.traces.size() == 1);
    minera::PetriUsageComparisonOptions comparison_options;
    comparison_options.activity_attribute = selector;
    const auto result =
        compare_petri_usage(net, log, log_of({}), comparison_options);
    assert(result.transitions[0].log1_occurrences == 1);
  }
}

void xes_exact_selector_beats_alias_in_any_order() {
  const auto net = simple_net();
  const std::vector<std::string> event_attributes = {
      "<string key=\"activity\" value=\"B\"/>"
      "<string key=\"concept:name\" value=\"A\"/>",
      "<string key=\"concept:name\" value=\"A\"/>"
      "<string key=\"activity\" value=\"B\"/>"};
  for (const auto& attributes : event_attributes) {
    std::istringstream xes(
        std::string("<log><trace><string key=\"concept:name\" value=\"case-1\"/>"
                    "<event>") +
        attributes + "</event></trace></log>");
    const auto log = minera::import_xes_event_log(xes);
    const auto result = compare_petri_usage(net, log, log_of({}));
    assert(result.transitions[0].log1_occurrences == 1);
  }
}

void invalid_net_indices_are_rejected() {
  minera::PetriNet net;
  const auto p0 = net.add_place();
  net.set_source(p0);
  net.set_sink(p0);
  net.set_activities({"A"});
  net.add_transition(4, {}, {});
  bool rejected = false;
  try {
    (void)minera::compare_petri_usage(net, log_of({}), log_of({}));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void silent_search_budget_is_reported() {
  constexpr std::size_t branch_count = 13;  // 2^13 reachable silent markings
  minera::PetriNet net;
  const auto source = net.add_place();
  std::vector<std::size_t> pending, yes, no;
  pending.reserve(branch_count);
  yes.reserve(branch_count);
  no.reserve(branch_count);
  for (std::size_t i = 0; i < branch_count; ++i) {
    pending.push_back(net.add_place());
    yes.push_back(net.add_place());
    no.push_back(net.add_place());
  }
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A"});
  net.add_transition(std::nullopt, {source}, pending);
  for (std::size_t i = 0; i < branch_count; ++i) {
    // Add the non-target choice first so the target combination is reached
    // only after the breadth-first frontier has exceeded the safety budget.
    net.add_transition(std::nullopt, {pending[i]}, {no[i]});
    net.add_transition(std::nullopt, {pending[i]}, {yes[i]});
  }
  net.add_transition(0, yes, {sink});

  bool rejected = false;
  try {
    (void)minera::compare_petri_usage(net, log_of({"A"}), log_of({}));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  assert(rejected);
}

}  // namespace

int main() {
  usage_directions_and_silent_activation();
  weighted_arc_and_duplicate_label_contract();
  finish_routes_when_sink_has_a_token_and_other_work_remains();
  explicit_markings_control_replay_boundaries();
  replay_controls_are_honored();
  extended_arcs_are_replayed_and_counted();
  inhibitor_arcs_gate_silent_completion();
  marking_dimensions_are_checked();
  custom_activity_attribute_controls_replay();
  aliases_prefer_first_class_fields_and_reject_blank_selectors();
  imported_custom_activity_attributes_are_replayable();
  imported_first_class_aliases_work_without_default_activity_columns();
  xes_exact_selector_beats_alias_in_any_order();
  invalid_net_indices_are_rejected();
  silent_search_budget_is_reported();
}
