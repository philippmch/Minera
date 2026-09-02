#include "minera/directly_follows_graph.hpp"
#include "minera/event_data.hpp"
#include "minera/inductive_miner.hpp"

#include <algorithm>
#include <cassert>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace minera;

EventLog log_from_variants(
    std::initializer_list<std::initializer_list<std::string>> variants) {
  EventLog log;
  std::size_t case_number = 0;
  for (const auto& activities : variants) {
    Trace trace;
    trace.case_id = "c" + std::to_string(case_number++);
    trace.case_id_state = FirstClassAttributeState::kPresent;
    for (const auto& activity : activities) {
      Event event;
      event.case_id = trace.case_id;
      event.activity = activity;
      event.activity_state = FirstClassAttributeState::kPresent;
      trace.events.push_back(std::move(event));
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

std::string render(const ProcessTree& tree, const ProcessTreeNode& node) {
  if (node.op == ProcessTreeOperator::kTau) return "tau";
  if (node.op == ProcessTreeOperator::kActivity)
    return tree.activities().at(node.activity);
  const char* symbol = "?";
  switch (node.op) {
    case ProcessTreeOperator::kSequence: symbol = "->"; break;
    case ProcessTreeOperator::kExclusive: symbol = "X"; break;
    case ProcessTreeOperator::kParallel: symbol = "+"; break;
    case ProcessTreeOperator::kLoop: symbol = "*"; break;
    case ProcessTreeOperator::kInclusiveChoice: symbol = "|"; break;
    case ProcessTreeOperator::kInterleaving: symbol = "||"; break;
    case ProcessTreeOperator::kTau:
    case ProcessTreeOperator::kActivity: break;
  }
  std::string result = std::string(symbol) + "(";
  for (std::size_t i = 0; i < node.children.size(); ++i) {
    if (i != 0) result += ",";
    result += render(tree, node.children[i]);
  }
  return result + ")";
}

bool has_operator(const ProcessTreeNode& node, ProcessTreeOperator op) {
  if (node.op == op) return true;
  return std::any_of(node.children.begin(), node.children.end(),
                     [&](const auto& child) { return has_operator(child, op); });
}

EventAttribute field(std::string key, std::string value,
                     bool is_null = false) {
  EventAttribute result;
  result.key = std::move(key);
  result.value = std::move(value);
  result.is_null = is_null;
  return result;
}

EventTable table_from_rows(std::vector<std::string> columns,
                           std::vector<EventTable::Row> rows) {
  EventTable table;
  table.columns = std::move(columns);
  table.rows = std::move(rows);
  return table;
}

EventTable::Row row(std::initializer_list<std::optional<EventAttribute>> cells) {
  return EventTable::Row(cells);
}

EventRecord record(std::initializer_list<EventAttribute> fields) {
  EventRecord result;
  result.fields = fields;
  return result;
}

template <typename Fn>
bool throws(Fn&& fn) {
  try {
    fn();
  } catch (const std::exception&) {
    return true;
  }
  return false;
}

void ordinary_variants_and_graph_input() {
  const auto log = log_from_variants({{"a", "b", "c"}});
  const auto tree = discover_process_tree(log);
  assert(render(tree, tree.root()) == "->(a,b,c)");

  const auto graph = discover_directly_follows_graph(log);
  InductiveMinerOptions options;
  options.variant = InductiveMinerVariant::kLog;
  const auto graph_tree = discover_process_tree(graph, options);
  InductiveMinerOptions dfg_options;
  dfg_options.variant = InductiveMinerVariant::kDfg;
  const auto expected = discover_process_tree(log, dfg_options);
  assert(render(graph_tree, graph_tree.root()) ==
         render(expected, expected.root()));

  EventLog empty;
  assert(discover_process_tree(empty).root().op == ProcessTreeOperator::kTau);
}

void graph_self_loop_keeps_unknown_skip_behavior() {
  const auto repeated = log_from_variants({{"a", "a"}});
  InductiveMinerOptions options;
  options.variant = InductiveMinerVariant::IMD;
  const auto from_log = discover_process_tree(repeated, options);
  assert(render(from_log, from_log.root()) == "*(tau,a)");

  const auto graph = discover_directly_follows_graph(repeated);
  const auto direct = discover_process_tree(graph, options);
  assert(render(direct, direct.root()) == "*(tau,a)");
}

void strict_sequence_toggle_is_observable() {
  const auto log = log_from_variants({{"a"}, {"a", "b", "c"}});
  const auto strict = discover_process_tree(log);
  InductiveMinerOptions relaxed_options;
  relaxed_options.disable_strict_sequence_cut = true;
  const auto relaxed = discover_process_tree(log, relaxed_options);
  assert(render(strict, strict.root()) == "->(a,X(tau,->(b,c)))");
  assert(render(relaxed, relaxed.root()) == "->(a,X(tau,b),X(tau,c))");
}

void specialized_fallthroughs_and_disabled_policy() {
  const auto once = log_from_variants({{"a", "c", "b", "b"},
                                       {"c", "b", "a"}});
  const auto once_tree = discover_process_tree(once);
  assert(once_tree.root().op == ProcessTreeOperator::kParallel);
  InductiveMinerOptions disabled;
  disabled.disable_fallthroughs = true;
  const auto once_disabled = discover_process_tree(once, disabled);
  assert(once_disabled.root().op == ProcessTreeOperator::kLoop);
  assert(once_disabled.root().children.front().op == ProcessTreeOperator::kTau);

  const auto concurrent = log_from_variants({{"c", "a", "c", "a", "c", "a"},
                                              {"c", "b", "a", "c"}});
  const auto concurrent_tree = discover_process_tree(concurrent);
  assert(concurrent_tree.root().op == ProcessTreeOperator::kParallel);
  assert(has_operator(concurrent_tree.root(), ProcessTreeOperator::kLoop));

  const auto strict_loop = log_from_variants({{"c", "c", "a", "c"},
                                               {"c", "c", "a", "a", "a"}});
  const auto strict_loop_tree = discover_process_tree(strict_loop);
  assert(strict_loop_tree.root().op == ProcessTreeOperator::kLoop);
  assert(has_operator(strict_loop_tree.root(), ProcessTreeOperator::kSequence));
  const auto strict_loop_disabled = discover_process_tree(strict_loop, disabled);
  assert(strict_loop_disabled.root().op == ProcessTreeOperator::kLoop);
  assert(strict_loop_disabled.root().children.front().op ==
         ProcessTreeOperator::kTau);

  const auto general_loop = log_from_variants(
      {{"c", "a", "a", "b", "a", "a"},
       {"c", "c", "a", "a", "a"}, {"c", "b", "a", "b", "c", "a"}});
  const auto general_tree = discover_process_tree(general_loop);
  assert(general_tree.root().op == ProcessTreeOperator::kLoop);
  const auto general_disabled = discover_process_tree(general_loop, disabled);
  assert(general_disabled.root().op == ProcessTreeOperator::kLoop);
  assert(general_disabled.root().children.front().op ==
         ProcessTreeOperator::kTau);

  // IMd has no ordinary-log repair fall-throughs, even when the policy is
  // enabled, so the graph-backed result remains a flower for this fixture.
  InductiveMinerOptions dfg;
  dfg.variant = InductiveMinerVariant::kDfg;
  const auto dfg_tree = discover_process_tree(general_loop, dfg);
  assert(dfg_tree.root().op == ProcessTreeOperator::kLoop);
  assert(dfg_tree.root().children.front().op == ProcessTreeOperator::kTau);
}

void event_log_custom_selectors_and_ordering() {
  EventLog log;
  Trace trace;
  trace.case_id = "native";
  for (const auto& item : std::vector<std::pair<std::string, std::string>>{
           {"B", "2026-01-01T00:00:02Z"}, {"A", "2026-01-01T00:00:01Z"}}) {
    Event event;
    event.activity = "ignored";
    event.timestamp = "2000-01-01T00:00:00Z";
    event.attributes.push_back(field("kind", item.first));
    event.attributes.push_back(field("when", item.second));
    trace.events.push_back(std::move(event));
  }
  log.traces.push_back(std::move(trace));
  InductiveMinerOptions options;
  options.activity_key = "kind";
  options.timestamp_key = "when";
  options.sort_events_by_timestamp = true;
  const auto prepared = prepare_inductive_event_log(log, options);
  assert(prepared.traces.size() == 1);
  assert(prepared.traces.front().events[0].activity == "A");
  assert(prepared.traces.front().events[1].activity == "B");
  const auto tree = discover_process_tree(log, options);
  assert(render(tree, tree.root()) == "->(A,B)");

  options.sort_events_by_timestamp = false;
  const auto unsorted = prepare_inductive_event_log(log, options);
  assert(unsorted.traces.front().events[0].activity == "B");
  assert(unsorted.traces.front().events[1].activity == "A");
  const auto unsorted_tree = discover_process_tree(log, options);
  assert(render(unsorted_tree, unsorted_tree.root()) == "->(B,A)");
}

void custom_case_grouping_handles_empty_traces() {
  EventLog log;
  Trace empty;
  empty.case_id = "native-empty";
  empty.attributes.push_back(field("group", "shared"));
  log.traces.push_back(std::move(empty));

  Trace populated;
  populated.case_id = "native-populated";
  populated.attributes.push_back(field("group", "shared"));
  Event event;
  event.activity = "a";
  populated.events.push_back(std::move(event));
  log.traces.push_back(std::move(populated));

  InductiveMinerOptions options;
  options.case_id_key = "group";
  const auto prepared = prepare_inductive_event_log(log, options);
  assert(prepared.traces.size() == 1);
  assert(prepared.traces.front().case_id == "shared");
  assert(prepared.traces.front().events.size() == 1);
  const auto tree = discover_process_tree(log, options);
  assert(render(tree, tree.root()) == "a");

  log.traces.front().attributes.clear();
  assert((throws([&] { (void)prepare_inductive_event_log(log, options); })));
}

EventTable standard_table(bool reverse_rows = true) {
  std::vector<EventTable::Row> rows = {
      row({field("case:concept:name", "c1"),
           field("concept:name", "B"),
           field("time:timestamp", "2026-01-01T00:00:02Z")}),
      row({field("case:concept:name", "c1"),
           field("concept:name", "A"),
           field("time:timestamp", "2026-01-01T00:00:01Z")}),
      row({field("case:concept:name", "c2"),
           field("concept:name", "A"),
           field("time:timestamp", "2026-01-01T00:00:01Z")}),
      row({field("case:concept:name", "c2"),
           field("concept:name", "B"),
           field("time:timestamp", "2026-01-01T00:00:02Z")})};
  if (reverse_rows) std::reverse(rows.begin(), rows.end());
  return table_from_rows({"case:concept:name", "concept:name", "time:timestamp"},
                         std::move(rows));
}

void table_stream_lazy_and_custom_selectors() {
  const auto table = standard_table();
  const auto eager = discover_process_tree(EventDataView(table));
  assert(render(eager, eager.root()) == "->(A,B)");

  const auto lazy = LazyEventTable::from_table(table);
  const auto lazy_tree = discover_process_tree(EventDataView(lazy));
  assert(render(lazy_tree, lazy_tree.root()) == render(eager, eager.root()));

  EventStream stream;
  stream.events.push_back(record({field("case", "c"), field("act", "B"),
                                  field("when", "2026-01-01T00:00:02Z")}));
  stream.events.push_back(record({field("case", "c"), field("act", "A"),
                                  field("when", "2026-01-01T00:00:01Z")}));
  InductiveMinerOptions custom;
  custom.activity_key = "act";
  custom.timestamp_key = "when";
  custom.case_id_key = "case";
  const auto stream_tree = discover_process_tree(EventDataView(stream), custom);
  assert(render(stream_tree, stream_tree.root()) == "->(A,B)");

  EventTable tie = table_from_rows(
      {"case", "act", "when"},
      {row({field("case", "c"), field("act", "B"),
            field("when", "2026-01-01T00:00:00Z")}),
       row({field("case", "c"), field("act", "A"),
            field("when", "2026-01-01T00:00:00Z")})});
  const auto tie_tree = discover_process_tree(EventDataView(tie), custom);
  assert(render(tie_tree, tie_tree.root()) == "->(B,A)");
}

void invalid_tabular_values_and_options() {
  auto bad = standard_table(false);
  bad.rows[0][2] = field("time:timestamp", "not-a-time");
  bool threw = false;
  try {
    (void)discover_process_tree(EventDataView(bad));
  } catch (const std::invalid_argument& error) {
    threw = std::string(error.what()).find("timestamp") != std::string::npos;
  }
  assert(threw);

  bad = standard_table(false);
  bad.rows[0][1] = field("concept:name", {}, true);
  threw = false;
  try {
    (void)discover_process_tree(EventDataView(bad));
  } catch (const std::invalid_argument& error) {
    threw = std::string(error.what()).find("activity") != std::string::npos;
  }
  assert(threw);

  InductiveMinerOptions options;
  options.noise_threshold = -0.01;
  threw = false;
  try {
    (void)discover_process_tree(EventLog{}, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  options.noise_threshold = 1.01;
  assert((throws([&] { (void)discover_process_tree(EventLog{}, options); })));
  options.noise_threshold = 0.0;
  options.activity_key.clear();
  assert((throws([&] { (void)discover_process_tree(EventLog{}, options); })));
  options.activity_key = "concept:name";
  options.variant = static_cast<InductiveMinerVariant>(99);
  assert((throws([&] { (void)discover_process_tree(EventLog{}, options); })));
}

void weighted_variant_input() {
  InductiveMinerVariantLog variants = {
      {{"a", "b"}, 12}, {{"a", "b"}, 3}};
  const auto weighted = discover_process_tree(variants);
  const auto ordinary = discover_process_tree(log_from_variants({{"a", "b"}}));
  assert(render(weighted, weighted.root()) == render(ordinary, ordinary.root()));
}

void variant_selector_contract() {
  const InductiveMinerVariantLog variants = {
      {{"a", "b", "c", "d"}, 20},
      {{"a", "c", "a", "b", "c", "d"}, 1}};

  InductiveMinerOptions classic;
  classic.variant = InductiveMinerVariant::IM;
  classic.noise_threshold = 0.2;
  const auto classic_with_threshold = discover_process_tree(variants, classic);
  classic.noise_threshold = 0.0;
  const auto classic_without_threshold = discover_process_tree(variants, classic);
  assert(render(classic_with_threshold, classic_with_threshold.root()) ==
         render(classic_without_threshold, classic_without_threshold.root()));

  InductiveMinerOptions infrequent;
  infrequent.variant = InductiveMinerVariant::IMF;
  infrequent.noise_threshold = 0.2;
  const auto filtered = discover_process_tree(variants, infrequent);
  assert(render(filtered, filtered.root()) !=
         render(classic_without_threshold, classic_without_threshold.root()));

  InductiveMinerOptions graph;
  graph.variant = InductiveMinerVariant::IMD;
  graph.noise_threshold = 0.2;
  const auto graph_with_threshold = discover_process_tree(variants, graph);
  graph.noise_threshold = 0.0;
  const auto graph_without_threshold = discover_process_tree(variants, graph);
  assert(render(graph_with_threshold, graph_with_threshold.root()) ==
         render(graph_without_threshold, graph_without_threshold.root()));
}

void sequence_projection_uses_ordered_min_cost_windows() {
  InductiveMinerVariantLog variants = {
      {{"a", "b", "c"}, 20},
      {{"a", "c", "a", "b", "c"}, 1}};
  InductiveMinerOptions options;
  options.variant = InductiveMinerVariant::IMF;
  options.noise_threshold = 0.2;
  const auto tree = discover_process_tree(variants, options);
  // The out-of-order c,a detour belongs to a later sequence window; retaining
  // the entire a subsequence would manufacture a self-loop on a.
  assert(render(tree, tree.root()) == "->(a,b,*(c,tau))");
}

void filtered_exclusive_ties_route_to_highest_group() {
  InductiveMinerVariantLog tie_variants = {
      {{"a", "b"}, 10},
      {{"a", "b", "c", "a"}, 1},
      {{"a", "c"}, 1},
      {{"c"}, 10},
  };
  InductiveMinerOptions tie_options;
  tie_options.variant = InductiveMinerVariant::IMF;
  tie_options.noise_threshold = 0.5;
  const auto tie_tree = discover_process_tree(tie_variants, tie_options);
  // The a,c variant has one hit in each filtered component.  The deterministic
  // tie rule routes it to the highest-index component, c, leaving the other
  // component as the clean a->b branch.
  assert(render(tie_tree, tie_tree.root()) == "X(c,->(a,b))");
}

}  // namespace

int main() {
  ordinary_variants_and_graph_input();
  graph_self_loop_keeps_unknown_skip_behavior();
  strict_sequence_toggle_is_observable();
  specialized_fallthroughs_and_disabled_policy();
  event_log_custom_selectors_and_ordering();
  custom_case_grouping_handles_empty_traces();
  table_stream_lazy_and_custom_selectors();
  invalid_tabular_values_and_options();
  weighted_variant_input();
  variant_selector_contract();
  sequence_projection_uses_ordered_min_cost_windows();
  filtered_exclusive_ties_route_to_highest_group();
  return 0;
}
