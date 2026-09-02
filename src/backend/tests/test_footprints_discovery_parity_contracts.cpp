#include "footprints/discovery_registry.hpp"

#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

using namespace minera;
using namespace minera::footprints_detail;

namespace {

EventLog make_log(std::initializer_list<std::initializer_list<const char*>> rows) {
  EventLog result;
  std::size_t index = 0;
  for (const auto row : rows) {
    Trace trace;
    trace.case_id = "case-" + std::to_string(index++);
    for (const auto activity : row) {
      Event event;
      event.case_id = trace.case_id;
      event.activity = activity;
      event.activity_state = FirstClassAttributeState::kPresent;
      trace.events.push_back(std::move(event));
    }
    result.traces.push_back(std::move(trace));
  }
  return result;
}

EventAttribute cell(const char* key, const char* value) {
  return EventAttribute{key, value};
}

EventAttribute null_cell(const char* key) {
  return EventAttribute{key, "", AttributeKind::kString, true};
}

template <typename Function>
void expect_invalid(Function&& function) {
  bool rejected = false;
  try {
    function();
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

EventTable make_table() {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp",
                  "@@index"};
  table.rows = {
      {cell("case:concept:name", "c1"), cell("concept:name", "B"),
       cell("time:timestamp", "2024-01-01T00:00:02Z"), cell("@@index", "2")},
      {cell("case:concept:name", "c1"), cell("concept:name", "A"),
       cell("time:timestamp", "2024-01-01T00:00:01Z"), cell("@@index", "1")},
      {cell("case:concept:name", "c2"), cell("concept:name", "C"),
       cell("time:timestamp", "2024-01-01T00:00:01Z"), cell("@@index", "1")},
      {cell("case:concept:name", "c2"), cell("concept:name", "D"),
       cell("time:timestamp", "2024-01-01T00:00:02Z"), cell("@@index", "2")},
  };
  return table;
}

EventTable make_interleaved_table() {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  table.rows = {
      {cell("case:concept:name", "c1"), cell("concept:name", "B"),
       cell("time:timestamp", "2024-01-01T00:00:01Z")},
      {cell("case:concept:name", "c2"), cell("concept:name", "Y"),
       cell("time:timestamp", "2024-01-01T00:00:02Z")},
      {cell("case:concept:name", "c1"), cell("concept:name", "A"),
       cell("time:timestamp", "2024-01-01T00:00:03Z")},
      {cell("case:concept:name", "c2"), cell("concept:name", "X"),
       cell("time:timestamp", "2024-01-01T00:00:04Z")},
  };
  return table;
}

EventTable make_start_timestamp_table() {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp",
                  "start:timestamp"};
  table.rows = {
      {cell("case:concept:name", "c"), cell("concept:name", "A"),
       cell("time:timestamp", "2024-01-01T00:00:01Z"),
       cell("start:timestamp", "2024-01-01T00:00:02Z")},
      {cell("case:concept:name", "c"), cell("concept:name", "B"),
       cell("time:timestamp", "2024-01-01T00:00:02Z"),
       cell("start:timestamp", "2024-01-01T00:00:01Z")},
  };
  return table;
}

EventTable make_start_timestamp_tie_table() {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp",
                  "start:timestamp"};
  table.rows = {
      {cell("case:concept:name", "c"), cell("concept:name", "B"),
       cell("time:timestamp", "2024-01-01T00:00:02Z"),
       cell("start:timestamp", "2024-01-01T00:00:01Z")},
      {cell("case:concept:name", "c"), cell("concept:name", "A"),
       cell("time:timestamp", "2024-01-01T00:00:01Z"),
       cell("start:timestamp", "2024-01-01T00:00:01Z")},
      {cell("case:concept:name", "c"), cell("concept:name", "C"),
       cell("time:timestamp", "2024-01-01T00:00:02Z"),
       cell("start:timestamp", "2024-01-01T00:00:01Z")},
  };
  return table;
}

EventTable make_start_timestamp_edge_table() {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp",
                  "start:timestamp"};
  table.rows = {
      {cell("case:concept:name", "c"), cell("concept:name", "N"),
       cell("time:timestamp", "2026-01-01"),
       null_cell("start:timestamp")},
      {cell("case:concept:name", "c"), cell("concept:name", "Q"),
       cell("time:timestamp", "2026-01-02"),
       cell("start:timestamp", "")},
      {cell("case:concept:name", "c"), cell("concept:name", "E"),
       cell("time:timestamp", "2026-01-03"),
       cell("start:timestamp", "2026-01-01")},
      {cell("case:concept:name", "c"), cell("concept:name", "X"),
       cell("time:timestamp", "2026-01-04"),
       cell("start:timestamp", "bad")},
  };
  return table;
}

EventTable make_timestamp_edge_table() {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp",
                  "@@index"};
  table.rows = {
      {cell("case:concept:name", "c"), cell("concept:name", "N"),
       null_cell("time:timestamp"), null_cell("@@index")},
      {cell("case:concept:name", "c"), cell("concept:name", "Q"),
       cell("time:timestamp", ""), null_cell("@@index")},
      {cell("case:concept:name", "c"), cell("concept:name", "X"),
       cell("time:timestamp", "bad"), null_cell("@@index")},
      {cell("case:concept:name", "c"), cell("concept:name", "L"),
       cell("time:timestamp", "2026-01-02"), null_cell("@@index")},
      {cell("case:concept:name", "c"), cell("concept:name", "E"),
       cell("time:timestamp", "2026-01-01"), null_cell("@@index")},
  };
  return table;
}

EventTable make_custom_selector_table() {
  EventTable table;
  table.columns = {"case_key", "activity_key", "completion_key"};
  table.rows = {
      {cell("case_key", "c"), cell("activity_key", "A"),
       cell("completion_key", "2026-01-01")},
      {cell("case_key", "c"), cell("activity_key", "B"),
       cell("completion_key", "2026-01-02")},
  };
  return table;
}

PetriNet make_net() {
  PetriNet net;
  const auto source = net.add_place();
  const auto middle = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A", "B"});
  net.add_transition(std::size_t{0}, {source}, {middle});
  net.add_transition(std::size_t{1}, {middle}, {sink});
  return net;
}

PowlNode make_powl() {
  return PowlNode::strict_partial_order(
      {PowlNode::transition("A"), PowlNode::transition("B")}, {{0, 1}});
}

ProcessTree make_tree() {
  return ProcessTree(
      {"A", "B"},
      ProcessTreeNode{ProcessTreeOperator::kSequence, 0,
                      {ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}});
}

void dfg_discovery_contract() {
  const std::map<FootprintPair, std::uint64_t> dfg = {
      {{"A", "B"}, 2}, {{"B", "A"}, 1}};
  const auto footprint = DfgFootprintsDiscovery::apply(dfg);
  assert(footprint.parallel.count({"A", "B"}) == 1);
  assert(footprint.sequence.empty());
  assert(!footprint.dfg_present && !footprint.min_trace_length_present);
}

void entire_dataframe_discovery_contract() {
  auto parameters = DiscoveryParameters{};
  parameters.sort_required = true;
  const auto footprint =
      EntireDataframeFootprintsDiscovery::apply(make_table(), parameters);
  assert(footprint.dfg.at({"A", "B"}) == 1);
  assert(footprint.dfg.at({"C", "D"}) == 1);
  assert(footprint.start_activities == std::set<std::string>({"A", "C"}));
  assert(footprint.end_activities == std::set<std::string>({"B", "D"}));

  auto unsorted = DiscoveryParameters{};
  unsorted.sort_required = false;
  const auto interleaved =
      EntireDataframeFootprintsDiscovery::apply(make_interleaved_table(),
                                                unsorted);
  assert(interleaved.dfg.empty());
  assert(interleaved.start_activities ==
         std::set<std::string>({"B", "Y"}));
  assert(interleaved.end_activities ==
         std::set<std::string>({"A", "X"}));
}

void table_selector_and_null_contract() {
  auto parameters = DiscoveryParameters{};
  parameters.activity_key = "activity_key";
  parameters.case_id_key = "case_key";
  parameters.timestamp_key = "completion_key";
  parameters.index_key = "missing_index";
  const auto custom = EntireDataframeFootprintsDiscovery::apply(
      make_custom_selector_table(), parameters);
  assert(custom.dfg.at({"A", "B"}) == 1);

  // Source selectors are exact names; familiar alternate spellings do not
  // get substituted when a selected column is absent.
  expect_invalid([&] {
    (void)EntireDataframeFootprintsDiscovery::apply(
        make_custom_selector_table(), DiscoveryParameters{});
  });
  parameters = DiscoveryParameters{};
  parameters.activity_key = "";
  expect_invalid([&] {
    (void)EntireDataframeFootprintsDiscovery::apply(make_table(), parameters);
  });
  parameters = DiscoveryParameters{};
  parameters.case_id_key = "   ";
  expect_invalid([&] {
    (void)EntireDataframeFootprintsDiscovery::apply(make_table(), parameters);
  });
  parameters = DiscoveryParameters{};
  parameters.timestamp_key = " time:timestamp ";
  expect_invalid([&] {
    (void)EntireDataframeFootprintsDiscovery::apply(make_table(), parameters);
  });
  parameters = DiscoveryParameters{};
  parameters.index_key = "";
  const auto blank_index =
      EntireDataframeFootprintsDiscovery::apply(make_table(), parameters);
  assert(blank_index.dfg.at({"A", "B"}) == 1);
  parameters.index_key = "   ";
  const auto whitespace_index =
      EntireDataframeFootprintsDiscovery::apply(make_table(), parameters);
  assert(whitespace_index.dfg.at({"A", "B"}) == 1);

  const auto edge_table = make_timestamp_edge_table();
  const auto eager = EntireDataframeFootprintsDiscovery::apply(edge_table);
  assert(eager.dfg.at({"Q", "E"}) == 1);
  assert(eager.dfg.at({"E", "L"}) == 1);
  assert(eager.dfg.at({"L", "X"}) == 1);
  assert(eager.dfg.at({"X", "N"}) == 1);
  const auto lazy = PolarsLazyframesFootprintsDiscovery::apply(
      LazyEventTable::from_table(edge_table));
  assert(lazy.dfg.at({"N", "Q"}) == 1);
  assert(lazy.dfg.at({"Q", "E"}) == 1);
  assert(lazy.dfg.at({"E", "L"}) == 1);
  assert(lazy.dfg.at({"L", "X"}) == 1);

  // Native footprint labels are strings, so a null activity is rejected as
  // an unsupported non-label rather than being fabricated as an empty name.
  auto null_activity = make_table();
  null_activity.rows.front()[1] = null_cell("concept:name");
  expect_invalid([&] {
    (void)EntireDataframeFootprintsDiscovery::apply(null_activity);
  });
  expect_invalid([&] {
    (void)PolarsLazyframesFootprintsDiscovery::apply(
        LazyEventTable::from_table(null_activity));
  });
  // Case identity is required by the native grouping representation.
  auto null_case = make_table();
  null_case.rows.front()[0] = null_cell("case:concept:name");
  expect_invalid([&] {
    (void)EntireDataframeFootprintsDiscovery::apply(null_case);
  });
  expect_invalid([&] {
    (void)PolarsLazyframesFootprintsDiscovery::apply(
        LazyEventTable::from_table(null_case));
  });

  EventTable empty;
  empty.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  expect_invalid([&] {
    (void)EntireDataframeFootprintsDiscovery::apply(empty);
  });
  expect_invalid([&] {
    (void)PolarsLazyframesFootprintsDiscovery::apply(
        LazyEventTable::from_table(empty));
  });
}

void entire_event_log_discovery_contract() {
  const auto footprint =
      EntireEventLogFootprintsDiscovery::apply(make_log({{"A", "B"}, {"A"}}));
  assert(footprint.dfg.at({"A", "B"}) == 1);
  assert(footprint.min_trace_length && *footprint.min_trace_length == 1);
  assert(footprint.start_activities.count("A") == 1);

  EventLog empty;
  const auto empty_footprint =
      EntireEventLogFootprintsDiscovery::apply(empty);
  assert(empty_footprint.dfg.empty());
  assert(empty_footprint.min_trace_length &&
         *empty_footprint.min_trace_length == 0);
  auto malformed = DiscoveryParameters{};
  malformed.activity_key = "  ";
  expect_invalid([&] {
    (void)EntireEventLogFootprintsDiscovery::apply(empty, malformed);
  });
}

void polars_lazyframes_discovery_contract() {
  auto parameters = DiscoveryParameters{};
  parameters.sort_required = false;
  const auto table = make_table();
  const auto lazy = LazyEventTable::from_table(table);
  const auto footprint =
      PolarsLazyframesFootprintsDiscovery::apply(lazy, parameters);
  assert(footprint.dfg.at({"B", "A"}) == 1);
  assert(footprint.dfg.at({"C", "D"}) == 1);

  const auto interleaved = LazyEventTable::from_table(make_interleaved_table());
  const auto grouped =
      PolarsLazyframesFootprintsDiscovery::apply(interleaved, parameters);
  assert(grouped.dfg.at({"B", "A"}) == 1);
  assert(grouped.dfg.at({"Y", "X"}) == 1);

  auto with_start = DiscoveryParameters{};
  with_start.start_timestamp_key = "start:timestamp";
  const auto sorted_with_start = PolarsLazyframesFootprintsDiscovery::apply(
      LazyEventTable::from_table(make_start_timestamp_table()), with_start);
  assert(sorted_with_start.dfg.at({"B", "A"}) == 1);
  const auto sorted_with_tied_start =
      PolarsLazyframesFootprintsDiscovery::apply(
          LazyEventTable::from_table(make_start_timestamp_tie_table()),
          with_start);
  assert(sorted_with_tied_start.dfg.at({"A", "B"}) == 1);
  assert(sorted_with_tied_start.dfg.at({"B", "C"}) == 1);
  const auto sorted_start_edges = PolarsLazyframesFootprintsDiscovery::apply(
      LazyEventTable::from_table(make_start_timestamp_edge_table()), with_start);
  assert(sorted_start_edges.dfg.at({"N", "Q"}) == 1);
  assert(sorted_start_edges.dfg.at({"Q", "E"}) == 1);
  assert(sorted_start_edges.dfg.at({"E", "X"}) == 1);
}

void eager_start_selector_contract() {
  auto parameters = DiscoveryParameters{};
  parameters.start_timestamp_key = "missing_start_timestamp";
  const auto eager = EntireDataframeFootprintsDiscovery::apply(
      make_table(), parameters);
  assert(eager.dfg.at({"A", "B"}) == 1);
  expect_invalid([&] {
    (void)PolarsLazyframesFootprintsDiscovery::apply(
        LazyEventTable::from_table(make_table()), parameters);
  });
}

void trace_by_trace_discovery_contract() {
  const auto traces =
      TraceByTraceFootprintsDiscovery::apply(make_log({{"A", "B"}, {}}));
  assert(traces.size() == 2);
  const std::vector<std::string> expected{"A", "B"};
  assert(traces[0].trace && *traces[0].trace == expected);
  assert(traces[1].trace && traces[1].trace->empty());
  assert(traces[0].sequence.count({"A", "B"}) == 1);
}

void petri_reachability_discovery_contract() {
  const auto net = make_net();
  const auto default_marking = PetriReachabilityFootprintsDiscovery::apply(net);
  assert(default_marking.start_activities == std::set<std::string>({"A"}));
  assert(default_marking.sequence.count({"A", "B"}) == 1);
  auto parameters = DiscoveryParameters{};
  parameters.initial_marking = std::vector<std::uint64_t>{1, 0, 0};
  const auto explicit_marking =
      PetriReachabilityFootprintsDiscovery::apply(net, parameters);
  assert(explicit_marking.sequence == default_marking.sequence);
  parameters.initial_marking = std::vector<std::uint64_t>{1};
  bool rejected = false;
  try {
    (void)PetriReachabilityFootprintsDiscovery::apply(net, parameters);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  // An explicit marking may begin in the middle of a sequential net.  The
  // prefix transition is then not an enabled predecessor of the suffix.
  PetriNet middle_net = make_net();
  parameters.initial_marking = std::vector<std::uint64_t>{0, 1, 0};
  const auto middle = PetriReachabilityFootprintsDiscovery::apply(
      middle_net, parameters);
  assert(middle.start_activities == std::set<std::string>({"B"}));
  assert(!middle.sequence.count({"A", "B"}));

  // Two tokens in distinct branches expose simultaneous visible transitions
  // as a parallel relation before a silent join consumes both branches.
  PetriNet concurrent;
  const auto left = concurrent.add_place();
  const auto right = concurrent.add_place();
  const auto left_done = concurrent.add_place();
  const auto right_done = concurrent.add_place();
  const auto sink = concurrent.add_place();
  concurrent.set_source(left);
  concurrent.set_sink(sink);
  concurrent.set_activities({"A", "B"});
  concurrent.add_transition(std::size_t{0}, {left}, {left_done});
  concurrent.add_transition(std::size_t{1}, {right}, {right_done});
  concurrent.add_transition(std::nullopt, {left_done, right_done}, {sink});
  parameters.initial_marking =
      std::vector<std::uint64_t>{1, 1, 0, 0, 0};
  const auto simultaneous = PetriReachabilityFootprintsDiscovery::apply(
      concurrent, parameters);
  assert(simultaneous.parallel.count({"A", "B"}) == 1);
  assert(simultaneous.parallel.count({"B", "A"}) == 1);
}

void powl_bottom_up_discovery_contract() {
  const auto footprint = PowlBottomUpFootprintsDiscovery::apply(make_powl());
  assert(footprint.sequence.count({"A", "B"}) == 1);
  assert(footprint.min_trace_length && *footprint.min_trace_length == 2);
}

void process_tree_bottom_up_discovery_contract() {
  const auto footprint =
      ProcessTreeBottomUpFootprintsDiscovery::apply(make_tree());
  assert(footprint.sequence.count({"A", "B"}) == 1);
  assert(footprint.max_trace_length_without_loops &&
         *footprint.max_trace_length_without_loops == 2);
}

FootprintsDiscoverySelector selector(const char* name) {
  const auto parsed = FootprintsDiscoverySelectorRegistry::parse(name);
  assert(parsed.has_value());
  return *parsed;
}

void entire_event_log_selector_contract() {
  const auto result = FootprintsDiscoverySelectorRegistry::dispatch(
      make_log({{"A", "B"}}), selector("ENTIRE_EVENT_LOG"));
  assert(std::get<Footprint>(result).dfg.at({"A", "B"}) == 1);
}

void entire_dataframe_selector_contract() {
  const auto result = FootprintsDiscoverySelectorRegistry::dispatch(
      make_table(), selector("ENTIRE_DATAFRAME"));
  assert(std::get<Footprint>(result).dfg.at({"A", "B"}) == 1);
}

void trace_by_trace_selector_contract() {
  const auto result = FootprintsDiscoverySelectorRegistry::dispatch(
      make_log({{"A", "B"}}), selector("TRACE_BY_TRACE"));
  const auto& traces = std::get<std::vector<Footprint>>(result);
  assert(traces.size() == 1 && traces.front().trace);
}

void petri_reach_graph_selector_contract() {
  const auto result = FootprintsDiscoverySelectorRegistry::dispatch(
      make_net(), selector("PETRI_REACH_GRAPH"));
  assert(std::get<Footprint>(result).sequence.count({"A", "B"}) == 1);
}

void process_tree_selector_contract() {
  const auto result = FootprintsDiscoverySelectorRegistry::dispatch(
      make_tree(), selector("PROCESS_TREE"));
  assert(std::get<Footprint>(result).sequence.count({"A", "B"}) == 1);
}

void powl_selector_contract() {
  const auto result = FootprintsDiscoverySelectorRegistry::dispatch(
      make_powl(), selector("POWL"));
  assert(std::get<Footprint>(result).sequence.count({"A", "B"}) == 1);
}

void dfg_selector_contract() {
  const std::map<FootprintPair, std::uint64_t> dfg = {{{"A", "B"}, 1}};
  const auto result = FootprintsDiscoverySelectorRegistry::dispatch(
      dfg, selector("DFG"));
  assert(std::get<Footprint>(result).sequence.count({"A", "B"}) == 1);
}

void polars_lazyframes_selector_contract() {
  const auto lazy = LazyEventTable::from_table(make_table());
  const auto result = FootprintsDiscoverySelectorRegistry::dispatch(
      lazy, selector("POLARS_LAZYFRAMES"));
  assert(std::get<Footprint>(result).dfg.at({"A", "B"}) == 1);
}

void cross_input_selector_contract() {
  // Compatible defaults retain the legacy conversion path.
  const auto event_log_result =
      FootprintsDiscoverySelectorRegistry::dispatch(
          make_table(), FootprintsDiscoverySelector::kEntireEventLog);
  assert(std::get<Footprint>(event_log_result).dfg.at({"B", "A"}) == 1);
  const auto trace_result = FootprintsDiscoverySelectorRegistry::dispatch(
      make_table(), FootprintsDiscoverySelector::kTraceByTrace);
  assert(std::get<std::vector<Footprint>>(trace_result).size() == 2);
  const auto dfg_result = FootprintsDiscoverySelectorRegistry::dispatch(
      make_table(), FootprintsDiscoverySelector::kDfg);
  assert(std::get<Footprint>(dfg_result).sequence.count({"B", "A"}) == 1);

  auto parameters = DiscoveryParameters{};
  parameters.activity_key = "activity_key";
  expect_invalid([&] {
    (void)FootprintsDiscoverySelectorRegistry::dispatch(
        make_table(), FootprintsDiscoverySelector::kEntireEventLog,
        parameters);
  });
  expect_invalid([&] {
    (void)FootprintsDiscoverySelectorRegistry::dispatch(
        LazyEventTable::from_table(make_table()),
        FootprintsDiscoverySelector::kTraceByTrace, parameters);
  });
  expect_invalid([&] {
    (void)FootprintsDiscoverySelectorRegistry::dispatch(
        make_table(), FootprintsDiscoverySelector::kDfg, parameters);
  });
}

void public_facade_compatibility_index_contract() {
  const auto log = make_log({{"A", "B"}});
  assert(discover_footprints(log, {}).sequence.count({"A", "B"}) == 1);
  assert(discover_trace_footprints(log, {}).size() == 1);

  FootprintsDiscoveryOptions options;
  options.index_attribute.clear();
  const auto eager = discover_footprints(make_table(), options);
  assert(eager.dfg.at({"A", "B"}) == 1);

  const auto lazy = discover_footprints(
      LazyEventTable::from_table(make_table()), options);
  assert(lazy.dfg.at({"A", "B"}) == 1);
}

}  // namespace

int main() {
  dfg_discovery_contract();
  entire_dataframe_discovery_contract();
  table_selector_and_null_contract();
  entire_event_log_discovery_contract();
  polars_lazyframes_discovery_contract();
  eager_start_selector_contract();
  trace_by_trace_discovery_contract();
  petri_reachability_discovery_contract();
  powl_bottom_up_discovery_contract();
  process_tree_bottom_up_discovery_contract();
  entire_event_log_selector_contract();
  entire_dataframe_selector_contract();
  trace_by_trace_selector_contract();
  petri_reach_graph_selector_contract();
  process_tree_selector_contract();
  powl_selector_contract();
  dfg_selector_contract();
  polars_lazyframes_selector_contract();
  cross_input_selector_contract();
  public_facade_compatibility_index_contract();
  assert(FootprintsDiscoverySelectorRegistry::all().size() == 8);
  return 0;
}
