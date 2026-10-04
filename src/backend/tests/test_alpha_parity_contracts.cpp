#include "minera/alpha_miner.hpp"
#include "minera/alpha_relations.hpp"

#include <algorithm>
#include <cassert>
#include <initializer_list>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {
using namespace minera;

EventLog make_log(std::initializer_list<std::vector<std::string>> traces) {
  EventLog result;
  for (const auto& labels : traces) {
    Trace trace;
    for (const auto& label : labels) {
      Event event;
      event.activity = label;
      event.activity_state = FirstClassAttributeState::kPresent;
      trace.events.push_back(std::move(event));
    }
    result.traces.push_back(std::move(trace));
  }
  return result;
}

using PlaceShape = std::tuple<bool, bool, std::multiset<std::string>,
                              std::multiset<std::string>>;
std::multiset<PlaceShape> places(const PetriNet& net) {
  std::multiset<PlaceShape> result;
  for (std::size_t p = 0; p < net.place_count(); ++p) {
    std::multiset<std::string> before;
    std::multiset<std::string> after;
    for (const auto& transition : net.transitions()) {
      const auto label = transition.activity
                             ? net.activities()[*transition.activity]
                             : "<silent>";
      if (std::find(transition.outputs.begin(), transition.outputs.end(), p) !=
          transition.outputs.end())
        before.insert(label);
      if (std::find(transition.inputs.begin(), transition.inputs.end(), p) !=
          transition.inputs.end())
        after.insert(label);
    }
    result.emplace(p == net.source(), p == net.sink(), before, after);
  }
  return result;
}

void classic_abstraction_retains_weighted_and_key_relations() {
  const auto abstraction = make_alpha_classic_abstraction(
      {{{"a", "b"}, 4}, {{"b", "a"}, 0}, {{"x", "x"}, 0}});
  assert((abstraction.causal == std::set<DfgActivityPair>{{"a", "b"}}));
  assert((abstraction.parallel ==
          std::set<DfgActivityPair>{{"a", "b"}, {"b", "a"}, {"x", "x"}}));
}

void classic_place_choice_does_not_require_matching_outgoing_presence() {
  const auto source = make_log({{"a", "b", "x"}, {"a", "c"}});
  const auto net = discover_alpha_classic(source);
  const std::multiset<PlaceShape> expected{
      {true, false, {}, {"a"}}, {false, true, {"c", "x"}, {}},
      {false, false, {"a"}, {"b", "c"}},
      {false, false, {"b"}, {"x"}}};
  assert(places(net) == expected);
  assert(places(discover_petri_net(source)) == expected);
}

void alpha_endpoints_distinguish_log_order_and_graph_keys() {
  const auto endpoints = alpha_log_endpoints(make_log({{}, {"b", "a"}, {"x"}}));
  assert((endpoints.starts == std::set<std::string>{"b", "x"}));
  assert((endpoints.ends == std::set<std::string>{"a", "x"}));
  const auto inferred = infer_alpha_endpoints(
      {{{"a", "b"}, 0}, {{"b", "c"}, 2}, {{"x", "x"}, 1}});
  assert((inferred.starts == std::set<std::string>{"a"}));
  assert((inferred.ends == std::set<std::string>{"c"}));
}

void sparse_classic_preserves_explicit_empty_and_zero_endpoints() {
  const DfgFrequency graph{{{"a", "b"}, 3}};
  const auto inferred = discover_petri_net(graph);
  assert(places(inferred) == places(discover_petri_net(make_log({{"a", "b"}}))));
  const auto empty = discover_petri_net(
      graph, std::map<std::string, std::uint64_t>{},
      std::map<std::string, std::uint64_t>{});
  const std::multiset<PlaceShape> expected_empty{
      {true, false, {}, {}}, {false, true, {}, {}},
      {false, false, {"a"}, {"b"}}};
  assert(places(empty) == expected_empty);
  const auto explicit_net = discover_petri_net(graph, {{"new", 0}}, {{"new", 0}});
  const std::multiset<PlaceShape> expected_explicit{
      {true, false, {}, {"new"}}, {false, true, {"new"}, {}},
      {false, false, {"a"}, {"b"}}};
  assert(places(explicit_net) == expected_explicit);
  assert(explicit_net.activities().size() == 3);
  bool rejected = false;
  try {
    (void)discover_petri_net(graph, {.short_loops = true});
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void sparse_classic_distinguishes_zero_frequency_from_missing_keys() {
  const auto zero = discover_petri_net(DfgFrequency{{{"a", "b"}, 0}});
  const std::multiset<PlaceShape> expected_zero{
      {true, false, {}, {"a"}}, {false, true, {"b"}, {}}};
  assert(places(zero) == expected_zero);
  assert(zero.transitions().size() == 2);

  const auto reciprocal = discover_petri_net(
      DfgFrequency{{{"a", "b"}, 4}, {{"b", "a"}, 0}});
  const std::multiset<PlaceShape> expected_reciprocal{
      {true, false, {}, {}}, {false, true, {}, {}},
      {false, false, {"a"}, {"b"}}};
  assert(places(reciprocal) == expected_reciprocal);

  const auto self = discover_petri_net(
      DfgFrequency{{{"a", "a"}, 0}, {{"a", "b"}, 4}});
  const std::multiset<PlaceShape> expected_self{
      {true, false, {}, {}}, {false, true, {"b"}, {}}};
  assert(places(self) == expected_self);
  assert(self.transitions().size() == 2);
}

void grouped_alpha_materialization_cap_precedes_copy() {
  const auto log = make_log({{"a", "b"}, {}, {"c"}});
  AlphaMinerOptions options;
  options.max_rows_to_materialize = 3;
  assert(normalize_alpha_event_log(log, options).traces.size() == 3);
  options.max_rows_to_materialize = 2;
  bool rejected = false;
  try {
    (void)normalize_alpha_event_log(log, options);
  } catch (const std::length_error&) {
    rejected = true;
  }
  assert(rejected);
  options.max_rows_to_materialize = 0;
  assert(normalize_alpha_event_log(make_log({{}, {}}), options).traces.size() == 2);
}

void classic_distinguishes_null_blank_and_missing_activities() {
  auto log = make_log({{"", ""}});
  log.traces.front().events.front().activity_state = FirstClassAttributeState::kNull;
  const auto net = discover_alpha_classic(log);
  assert(net.activities() == std::vector<std::string>{""});
  assert(net.transitions().size() == 2 && net.place_count() == 3);
  assert(net.transitions()[0].silent() && !net.transitions()[1].silent());
  assert(net.transitions()[0].inputs == std::vector<std::size_t>{net.source()});
  assert(net.transitions()[1].outputs == std::vector<std::size_t>{net.sink()});
  assert(net.transitions()[0].outputs == net.transitions()[1].inputs);
  assert(log.traces.front().events.front().activity_state == FirstClassAttributeState::kNull);

  EventLog missing;
  missing.traces.push_back(Trace{"case", {Event{}}});
  const auto isolated = discover_petri_net(missing);
  assert(isolated.transitions().empty() && isolated.place_count() == 2);
  missing.traces.front().events.push_back(log.traces.front().events.back());
  bool rejected = false;
  try { (void)discover_alpha_classic(missing); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);

  auto selected = make_log({{"ignored"}});
  EventAttribute phase{"phase", ""};
  phase.is_null = true;
  selected.traces.front().events.front().attributes.push_back(phase);
  const auto nullable = discover_alpha_classic(selected, {.activity_attribute = "phase"});
  assert(nullable.activities().empty() && nullable.transitions().size() == 1);
  assert(nullable.transitions().front().silent());
  assert(nullable.transitions().front().inputs == std::vector<std::size_t>{nullable.source()});
  assert(nullable.transitions().front().outputs == std::vector<std::size_t>{nullable.sink()});
}

void plus_preserves_null_labels_and_structural_names() {
  auto log = make_log({{"ignored"}});
  auto& event = log.traces.front().events.front();
  event.activity.clear();
  event.activity_state = FirstClassAttributeState::kNull;
  const auto result = discover_alpha_plus_result(log);
  assert(result.net.activities().empty() && result.net.transitions().size() == 1);
  const auto& transition = result.net.transitions().front();
  assert(transition.silent());
  assert(transition.inputs == std::vector<std::size_t>{result.net.source()});
  assert(transition.outputs == std::vector<std::size_t>{result.net.sink()});
  assert(result.initial_marking[result.net.source()] == 1);
  assert(result.final_marking[result.net.sink()] == 1);
  assert(result.place_names[result.net.source()].after ==
         std::vector<std::optional<std::string>>{std::nullopt});
  assert(event.activity_state == FirstClassAttributeState::kNull);

  EventAttribute phase{"phase", ""};
  phase.is_null = true;
  event.attributes.push_back(phase);
  const auto selected = discover_alpha_plus_result(log, {.activity_attribute = "phase"});
  assert(selected.net.activities().empty() && selected.net.transitions().front().silent());
  event.attributes.clear();
  bool rejected = false;
  try { (void)discover_alpha_plus_result(log, {.activity_attribute = "phase"}); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
}

void plus_interior_one_loop_has_a_fresh_self_only_place() {
  const auto source = make_log({{"a", "L", "L", "b"}, {"a", "b"}});
  const auto net = discover_alpha_plus(source);
  const std::multiset<PlaceShape> expected{
      {true, false, {}, {"a"}}, {false, true, {"b"}, {}},
      {false, false, {"a"}, {"b"}}, {false, false, {"L"}, {"L"}}};
  assert(places(net) == expected);
  assert(net.transitions().size() == 3);
  for (int repeat = 0; repeat < 4; ++repeat) {
    const auto repeated = discover_petri_net(source, {.short_loops = true});
    assert(places(repeated) == expected);
    assert(repeated.activities() == net.activities());
    for (std::size_t i = 0; i < net.transitions().size(); ++i) {
      assert(repeated.transitions()[i].inputs == net.transitions()[i].inputs);
      assert(repeated.transitions()[i].outputs == net.transitions()[i].outputs);
    }
  }
}

void plus_keeps_an_end_boundary_without_one_loops() {
  const auto source = make_log({{"a"}, {"a", "b"}});
  const auto net = discover_alpha_plus(source);
  const std::multiset<PlaceShape> expected{
      {true, false, {}, {"a"}}, {false, true, {"<silent>"}, {}},
      {false, false, {"a"}, {"b"}},
      {false, false, {"a"}, {"<silent>"}},
      {false, false, {"b"}, {"<silent>"}}};
  assert(places(net) == expected);
  assert(net.transitions().size() == 3);
  assert(places(discover_alpha_classic(source)) != expected);
}

void plus_sharp_preserves_boundary_outgoing_presence() {
  const auto net = discover_alpha_plus(make_log({{"a"}, {"a", "b", "c"}}));
  const std::multiset<PlaceShape> expected{
      {true, false, {}, {"a"}}, {false, true, {"<silent>"}, {}},
      {false, false, {"a"}, {"b"}}, {false, false, {"b"}, {"c"}},
      {false, false, {"a", "c"}, {"<silent>"}}};
  assert(places(net) == expected);
  assert(net.transitions().size() == 4);
}

void plus_empty_trace_retains_both_boundaries() {
  const auto net = discover_alpha_plus(make_log({{}, {"a"}}));
  const std::multiset<PlaceShape> expected{
      {true, false, {}, {"<silent>"}},
      {false, true, {"<silent>"}, {}},
      {false, false, {"<silent>"}, {"a"}},
      {false, false, {"a"}, {"<silent>"}},
      {false, false, {"<silent>"}, {"<silent>"}}};
  assert(places(net) == expected);
  assert(net.transitions().size() == 3);
}

void plus_reduced_self_loops_retain_degenerate_boundaries() {
  for (const bool remove : {false, true}) {
    const auto raw = discover_alpha_plus_result(make_log({{"a", "L", "L", "a"}}),
                                                {.remove_unconnected = remove});
    assert(raw.detached_initial_marking.empty());
    assert(raw.detached_final_marking.empty());
    assert(raw.place_names.size() == 2);
    assert(raw.place_names[raw.net.source()].boundary == "start");
    assert(raw.place_names[raw.net.sink()].boundary == "end");
    assert(raw.initial_marking[raw.net.source()] == 1);
    assert(raw.final_marking[raw.net.sink()] == 1);
    const auto net = discover_alpha_plus(make_log({{"a", "L", "L", "a"}}),
                                         {.remove_unconnected = remove});
    const std::multiset<PlaceShape> expected{
        {true, false, {}, {"<silent>"}}, {false, true, {"<silent>"}, {}}};
    assert(places(net) == expected);
    assert(net.transitions().size() == (remove ? 2 : 4));
    const auto tail = discover_alpha_plus(make_log({{"a", "L", "L", "a", "b"}}),
                                          {.remove_unconnected = remove});
    const std::multiset<PlaceShape> tail_expected{
        {true, false, {}, {"<silent>"}}, {false, true, {"b"}, {}},
        {false, false, {"L"}, {"L"}}};
    assert(places(tail) == tail_expected);
    assert(tail.transitions().size() == (remove ? 3 : 4));
  }
}

void plus_rejects_domains_with_no_valid_initial_marking() {
  for (const auto& log : {make_log({}), make_log({{}}),
                         make_log({{"L", "L"}}),
                         make_log({{"L", "L"}, {"M", "M"}}),
                         make_log({{"L", "L"}, {"a", "L", "L", "a"}})}) {
    for (const bool remove : {false, true}) {
      bool rejected = false;
      try {
        (void)discover_alpha_plus(log, {.remove_unconnected = remove});
      } catch (const std::invalid_argument&) {
        rejected = true;
      }
      assert(rejected);
    }
  }
}

void plus_loop_context_changes_when_trace_order_changes() {
  const auto forward = make_log({{"L", "L", "a", "b"},
                                  {"a", "L", "L", "b"}});
  const auto reverse = make_log({{"a", "L", "L", "b"},
                                  {"L", "L", "a", "b"}});
  const auto first = discover_alpha_plus(forward);
  const auto second = discover_alpha_plus(reverse);
  const PlaceShape loop{false, false, {"L"}, {"L"}};
  assert(places(first).count(loop) == 1);
  assert(places(second).count(loop) == 0);
  assert(second.transitions().size() == 3);
  assert(discover_alpha_plus(reverse, {.remove_unconnected = true})
             .transitions().size() == 2);
}

void plus_two_loop_uses_bidirectional_causal_places() {
  const auto net = discover_alpha_plus(make_log(
      {{"x", "a", "b", "a", "b", "a", "y"}, {"x", "a", "y"}}));
  const std::multiset<PlaceShape> expected{
      {true, false, {}, {"x"}}, {false, true, {"y"}, {}},
      {false, false, {"b", "x"}, {"a"}},
      {false, false, {"a"}, {"b", "y"}}};
  assert(places(net) == expected);
  assert(net.transitions().size() == 4);
}

void plus_empty_context_differences_create_one_loop_place_per_maximal_pair() {
  const auto source = make_log({{"a", "L", "L", "a"}, {"x", "y"}});
  const auto retained = discover_alpha_plus(source);
  const std::multiset<PlaceShape> expected{
      {true, false, {}, {"x"}}, {false, true, {"y"}, {}},
      {false, false, {"x"}, {"y"}},
      {false, false, {"L"}, {"L"}},
      {false, false, {"L"}, {"L"}},
      {false, false, {"L"}, {"L"}}};
  assert(places(retained) == expected);
  assert(retained.transitions().size() == 4);
  const auto cleaned = discover_alpha_plus(source, {.remove_unconnected = true});
  assert(places(cleaned) == expected);
  assert(cleaned.transitions().size() == 3);
}

}  // namespace

int main() {
  classic_abstraction_retains_weighted_and_key_relations();
  classic_place_choice_does_not_require_matching_outgoing_presence();
  alpha_endpoints_distinguish_log_order_and_graph_keys();
  sparse_classic_preserves_explicit_empty_and_zero_endpoints();
  sparse_classic_distinguishes_zero_frequency_from_missing_keys();
  grouped_alpha_materialization_cap_precedes_copy();
  classic_distinguishes_null_blank_and_missing_activities();
  plus_preserves_null_labels_and_structural_names();
  plus_interior_one_loop_has_a_fresh_self_only_place();
  plus_keeps_an_end_boundary_without_one_loops();
  plus_sharp_preserves_boundary_outgoing_presence();
  plus_empty_trace_retains_both_boundaries();
  plus_reduced_self_loops_retain_degenerate_boundaries();
  plus_rejects_domains_with_no_valid_initial_marking();
  plus_loop_context_changes_when_trace_order_changes();
  plus_two_loop_uses_bidirectional_causal_places();
  plus_empty_context_differences_create_one_loop_place_per_maximal_pair();
}
