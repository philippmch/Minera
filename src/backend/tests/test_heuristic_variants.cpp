#include "minera/heuristic_miner.hpp"
#include "minera/model_decoration.hpp"
#include "minera/petri_net.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace minera;

namespace {

Event event(std::string activity, std::string timestamp,
            std::string lifecycle = {}) {
  Event result;
  result.activity = std::move(activity);
  result.timestamp = std::move(timestamp);
  result.lifecycle = std::move(lifecycle);
  return result;
}

void attr(Event& event, std::string key, std::string value) {
  event.attributes.push_back(
      {std::move(key), std::move(value), AttributeKind::kString});
}

Event interval(std::string activity, std::string start,
               std::string complete, std::string start_key = "start_timestamp") {
  Event result = event(std::move(activity), std::move(complete));
  attr(result, std::move(start_key), std::move(start));
  return result;
}

Trace trace(std::vector<Event> events) {
  Trace result;
  result.events = std::move(events);
  return result;
}

EventLog log(std::vector<Trace> traces) {
  EventLog result;
  result.traces = std::move(traces);
  return result;
}

HeuristicMinerOptions plus_options() {
  HeuristicMinerOptions options;
  options.variant = HeuristicMinerVariant::kPlusPlus;
  return options;
}

std::size_t activity(const HeuristicsNet& net, const std::string& name) {
  for (std::size_t i = 0; i < net.activities.size(); ++i)
    if (net.activities[i] == name) return i;
  assert(false);
  return 0;
}

const HeuristicDependency* edge(const HeuristicsNet& net,
                                const std::string& source,
                                const std::string& target) {
  const auto a = activity(net, source);
  const auto b = activity(net, target);
  for (const auto& candidate : net.edges)
    if (candidate.source == a && candidate.target == b) return &candidate;
  return nullptr;
}

const HeuristicRelationStatistics& relation(const HeuristicsNet& net,
                                             const std::string& source,
                                             const std::string& target) {
  const auto a = activity(net, source);
  const auto b = activity(net, target);
  for (const auto& candidate : net.relation_statistics)
    if (candidate.source == a && candidate.target == b) return candidate;
  assert(false);
  return net.relation_statistics.front();
}

const HeuristicActivityStatistics& activity_stat(const HeuristicsNet& net,
                                                  const std::string& name) {
  const auto id = activity(net, name);
  for (const auto& candidate : net.activity_statistics)
    if (candidate.activity == id) return candidate;
  assert(false);
  return net.activity_statistics.front();
}

void interval_abstraction_and_strict_threshold() {
  auto a_start = event("A", "2026-01-01T00:00:00Z", "start");
  attr(a_start, "concept:instance", "a1");
  auto b_start = event("B", "2026-01-01T00:02:00Z", "start");
  attr(b_start, "concept:instance", "b1");
  auto a_complete = event("A", "2026-01-01T00:05:00Z", "complete");
  attr(a_complete, "concept:instance", "a1");
  auto b_complete = event("B", "2026-01-01T00:06:00Z", "complete");
  attr(b_complete, "concept:instance", "b1");
  auto c = interval("C", "2026-01-01T00:06:00Z",
                    "2026-01-01T00:06:00Z");

  auto options = plus_options();
  options.decoration = HeuristicNetDecoration::kPerformance;
  const auto net = discover_heuristics_net(
      log({trace({a_start, b_start, a_complete, b_complete, c})}), options);
  assert(net.variant == HeuristicMinerVariant::kPlusPlus);
  assert(net.decoration == HeuristicNetDecoration::kPerformance);
  assert(net.start_activities == std::vector<std::size_t>{activity(net, "A")});
  assert(net.end_activities == std::vector<std::size_t>{activity(net, "C")});
  assert(std::abs(activity_stat(net, "A").mean_sojourn_seconds - 300.0) <
         1e-9);
  assert(std::abs(activity_stat(net, "B").mean_sojourn_seconds - 240.0) <
         1e-9);
  assert(activity_stat(net, "C").mean_sojourn_seconds == 0.0);

  // First non-overlapping eventually-follow: A skips overlapping B and reaches
  // C. B and C touch, so they are both EFG and inclusively concurrent.
  assert(relation(net, "A", "C").eventually_follows_frequency == 1);
  const auto& bc = relation(net, "B", "C");
  assert(bc.eventually_follows_frequency == 1);
  assert(bc.direct_adjacency_frequency == 1);
  assert(bc.mean_wait_seconds == 0.0);
  assert(edge(net, "A", "C") != nullptr);
  assert(edge(net, "B", "C") == nullptr);  // dependency == default 0.5
  bool saw_ab = false;
  bool saw_bc = false;
  for (const auto& concurrent : net.concurrency_statistics) {
    const auto first = net.activities[concurrent.first];
    const auto second = net.activities[concurrent.second];
    saw_ab |= first == "A" && second == "B" && concurrent.frequency == 1;
    saw_bc |= first == "B" && second == "C" && concurrent.frequency == 1;
  }
  assert(saw_ab && saw_bc);

  // An eventually-follows-only relation has no direct wait sample.  Even for
  // raw/all performance selections it keeps the scalar zero projection.
  const auto efg_only = log({trace({
      interval("A", "2026-01-02T00:00:00Z", "2026-01-02T00:00:05Z"),
      interval("B", "2026-01-02T00:00:01Z", "2026-01-02T00:00:10Z"),
      interval("C", "2026-01-02T00:00:06Z", "2026-01-02T00:00:07Z"),
  })});
  options.dependency_threshold = 0.0;
  options.performance_aggregation =
      HeuristicPerformanceAggregation::kRawValues;
  const auto raw_efg = discover_heuristics_net(efg_only, options);
  const auto* raw_efg_edge = edge(raw_efg, "A", "C");
  assert(raw_efg_edge != nullptr);
  assert(raw_efg_edge->representation == 0.0);
  assert(!raw_efg_edge->representation_value.has_value());
  options.performance_aggregation = HeuristicPerformanceAggregation::kAll;
  const auto all_efg = discover_heuristics_net(efg_only, options);
  const auto* all_efg_edge = edge(all_efg, "A", "C");
  assert(all_efg_edge != nullptr);
  assert(all_efg_edge->representation == 0.0);
  assert(!all_efg_edge->representation_value.has_value());

  options.dependency_threshold = 0.49;
  const auto relaxed = discover_heuristics_net(
      log({trace({a_start, b_start, a_complete, b_complete, c})}), options);
  assert(edge(relaxed, "B", "C") != nullptr);
  assert(std::abs(edge(relaxed, "B", "C")->dependency - 0.5) < 1e-9);
}

void lifecycle_fifo_and_configurable_interval_attribute() {
  auto s1 = event("A", "2026-01-01T00:00:00Z", "START");
  auto s2 = event("A", "2026-01-01T00:00:01Z", "start");
  auto c1 = event("A", "2026-01-01T00:00:02Z", "complete");
  auto c2 = event("A", "2026-01-01T00:00:04Z", "COMPLETE");
  for (auto* e : {&s1, &s2, &c1, &c2}) attr(*e, "instance", "same");
  auto atomic = interval("B", "2026-01-01T00:00:10Z",
                         "2026-01-01T00:00:13Z", "begin");
  auto options = plus_options();
  options.lifecycle_instance_attribute = "instance";
  options.start_timestamp_attribute = "begin";
  options.dependency_threshold = -1.0;
  const auto net = discover_heuristics_net(
      log({trace({s1, s2, c1, c2, atomic})}), options);
  // FIFO durations are 2 and 3 seconds.
  assert(std::abs(activity_stat(net, "A").mean_sojourn_seconds - 2.5) <
         1e-9);
  // In lifecycle-conversion mode, an unmatched complete is atomic and any
  // pre-existing start attribute is overwritten by the completion time.
  assert(activity_stat(net, "B").mean_sojourn_seconds == 0.0);

  auto pre_interval = interval("B", "2026-01-01T00:00:10Z",
                               "2026-01-01T00:00:13Z", "begin");
  const auto interval_net = discover_heuristics_net(
      log({trace({pre_interval})}), options);
  assert(std::abs(activity_stat(interval_net, "B").mean_sojourn_seconds -
                  3.0) < 1e-9);

  auto missing = event("C", "2026-01-01T00:00:14Z");
  bool threw = false;
  try {
    (void)discover_heuristics_net(
        log({trace({pre_interval, missing})}), options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void lifecycle_input_shapes_defer_ignored_timestamp_parsing() {
  auto options = plus_options();
  options.decoration = HeuristicNetDecoration::kPerformance;
  options.use_business_hours = true;

  auto unknown_bad_time = event("X", "garbage", "schedule");
  unknown_bad_time.timestamp_state = FirstClassAttributeState::kPresent;
  auto unknown_empty_time = event("Y", "", "schedule");
  unknown_empty_time.timestamp_state = FirstClassAttributeState::kPresent;
  const auto valid = log({trace({
      unknown_bad_time,
      unknown_empty_time,
      event("A", "2026-01-05T10:00:00Z"),
      event("B", "2026-01-05T12:00:00Z"),
  })});
  const auto ignored = discover_heuristics_net(valid, options);
  assert(std::abs(relation(ignored, "A", "B").mean_wait_seconds -
                  7200.0) < 1e-9);

  auto missing_time = event("X", "", "schedule");
  bool missing_time_threw = false;
  try {
    (void)discover_heuristics_net(
        log({trace({missing_time, event("A", "2026-01-05T10:00:00Z")})}),
        options);
  } catch (const std::invalid_argument&) {
    missing_time_threw = true;
  }
  assert(missing_time_threw);

  auto unmatched_bad_start = event("X", "garbage", "start");
  unmatched_bad_start.timestamp_state = FirstClassAttributeState::kPresent;
  const auto unmatched = discover_heuristics_net(
      log({trace({unmatched_bad_start, event("A", "2026-01-05T10:00:00Z"),
                  event("B", "2026-01-05T12:00:00Z")})}),
      options);
  assert(std::abs(relation(unmatched, "A", "B").mean_wait_seconds -
                  7200.0) < 1e-9);

  auto paired_bad_start = event("A", "garbage", "start");
  paired_bad_start.timestamp_state = FirstClassAttributeState::kPresent;
  bool paired_start_threw = false;
  try {
    (void)discover_heuristics_net(
        log({trace({paired_bad_start,
                    event("A", "2026-01-05T10:00:00Z"),
                    event("B", "2026-01-05T12:00:00Z")})}),
        options);
  } catch (const std::invalid_argument&) {
    paired_start_threw = true;
  }
  assert(paired_start_threw);

  auto explicit_empty_lifecycle = event("A", "garbage", "");
  explicit_empty_lifecycle.timestamp_state =
      FirstClassAttributeState::kPresent;
  explicit_empty_lifecycle.lifecycle_state =
      FirstClassAttributeState::kPresent;
  const auto empty_lifecycle = discover_heuristics_net(
      log({trace({explicit_empty_lifecycle,
                  event("B", "2026-01-05T12:00:00Z")})}),
      options);
  assert((empty_lifecycle.activities == std::vector<std::string>{"B"}));
  assert(empty_lifecycle.relation_statistics.empty());

  // A missing lifecycle field is the atomic/completion form and still parses
  // its timestamp, unlike the explicit empty lifecycle above.
  const auto omitted_lifecycle = discover_heuristics_net(
      log({trace({event("A", "2026-01-05T10:00:00Z"),
                  event("B", "2026-01-05T12:00:00Z")})}),
      options);
  assert(std::abs(relation(omitted_lifecycle, "A", "B").mean_wait_seconds -
                  7200.0) < 1e-9);

  auto custom_unknown = event("X", "", "schedule");
  attr(custom_unknown, "occurred", "garbage");
  auto custom_a = event("A", "");
  attr(custom_a, "occurred", "2026-01-05T10:00:00Z");
  auto custom_b = event("B", "");
  attr(custom_b, "occurred", "2026-01-05T12:00:00Z");
  auto custom_options = options;
  custom_options.timestamp_attribute = "occurred";
  const auto custom = discover_heuristics_net(
      log({trace({custom_unknown, custom_a, custom_b})}), custom_options);
  assert(std::abs(relation(custom, "A", "B").mean_wait_seconds -
                  7200.0) < 1e-9);

  const auto custom_missing = event("X", "", "schedule");
  bool custom_missing_threw = false;
  try {
    (void)discover_heuristics_net(
        log({trace({custom_missing, custom_a, custom_b})}), custom_options);
  } catch (const std::invalid_argument&) {
    custom_missing_threw = true;
  }
  assert(custom_missing_threw);
}

void filtering_and_unfiltered_statistics() {
  const auto source = log({
      trace({interval("A", "2026-01-01T00:00:00Z",
                      "2026-01-01T00:00:00Z"),
             interval("B", "2026-01-01T00:00:01Z",
                      "2026-01-01T00:00:01Z")}),
      trace({interval("A", "2026-01-02T00:00:00Z",
                      "2026-01-02T00:00:00Z"),
             interval("B", "2026-01-02T00:00:01Z",
                      "2026-01-02T00:00:01Z")}),
      trace({interval("A", "2026-01-03T00:00:00Z",
                      "2026-01-03T00:00:00Z"),
             interval("C", "2026-01-03T00:00:01Z",
                      "2026-01-03T00:00:02Z")}),
  });
  auto options = plus_options();
  options.min_frequency = 2;
  const auto path_filtered = discover_heuristics_net(source, options);
  assert(path_filtered.relation_statistics.size() == 1);
  assert(relation(path_filtered, "A", "B").eventually_follows_frequency == 2);
  assert(edge(path_filtered, "A", "B") != nullptr);
  assert(edge(path_filtered, "A", "C") == nullptr);

  options.min_activity_count = 2;
  const auto activity_filtered = discover_heuristics_net(source, options);
  assert(activity_filtered.activities.size() == 2);
  assert(activity_filtered.raw_activity_statistics.size() == 3);
  bool retained_c_sojourn = false;
  for (const auto& stat : activity_filtered.raw_activity_statistics)
    retained_c_sojourn |=
        stat.activity == "C" && stat.mean_sojourn_seconds == 1.0;
  assert(retained_c_sojourn);

  const auto reverse_noise = log({
      trace({interval("A", "2026-02-01T00:00:00Z",
                      "2026-02-01T00:00:00Z"),
             interval("B", "2026-02-01T00:00:01Z",
                      "2026-02-01T00:00:01Z")}),
      trace({interval("A", "2026-02-02T00:00:00Z",
                      "2026-02-02T00:00:00Z"),
             interval("B", "2026-02-02T00:00:01Z",
                      "2026-02-02T00:00:01Z")}),
      trace({interval("B", "2026-02-03T00:00:00Z",
                      "2026-02-03T00:00:00Z"),
             interval("A", "2026-02-03T00:00:01Z",
                      "2026-02-03T00:00:01Z")}),
  });
  options.min_activity_count = 1;
  options.dependency_threshold = 0.9;
  const auto reverse_filtered =
      discover_heuristics_net(reverse_noise, options);
  // The one-count reverse path is removed before dependency calculation.
  assert(std::abs(edge(reverse_filtered, "A", "B")->dependency - 1.0) <
         1e-9);
}

void dependency_concurrency_and_no_connect_all() {
  const auto source = log({
      trace({interval("A", "2026-01-01T00:00:00Z",
                      "2026-01-01T00:00:00Z"),
             interval("B", "2026-01-01T00:00:01Z",
                      "2026-01-01T00:00:01Z")}),
      trace({interval("A", "2026-01-02T00:00:00Z",
                      "2026-01-02T00:00:02Z"),
             interval("B", "2026-01-02T00:00:01Z",
                      "2026-01-02T00:00:03Z")}),
  });
  auto options = plus_options();
  options.dependency_threshold = 0.49;
  auto net = discover_heuristics_net(source, options);
  assert(std::abs(edge(net, "A", "B")->dependency - 0.5) < 1e-9);
  options.dependency_threshold = 0.5;
  options.connect_all = true;
  net = discover_heuristics_net(source, options);
  assert(edge(net, "A", "B") == nullptr);

  const auto self_source =
      log({trace({interval("A", "2026-05-01T00:00:00Z",
                           "2026-05-01T00:00:00Z"),
                  interval("A", "2026-05-01T00:00:01Z",
                           "2026-05-01T00:00:01Z"),
                  interval("B", "2026-05-01T00:00:02Z",
                           "2026-05-01T00:00:02Z")})});
  options.dependency_threshold = -0.1;
  options.and_threshold = 0.4;
  net = discover_heuristics_net(self_source, options);
  const auto a = activity(net, "A");
  const auto b = activity(net, "B");
  assert(edge(net, "A", "A") != nullptr);
  assert((net.output_bundles[a] == std::vector<HeuristicBundle>{{a, b}}));
}

void and_bundles_are_overlapping_maximal_cliques() {
  const auto zero = "2026-01-01T00:00:00Z";
  const auto one = "2026-01-01T00:00:01Z";
  const auto two = "2026-01-01T00:00:02Z";
  const auto source = log({
      trace({interval("A", zero, zero), interval("B", one, one)}),
      trace({interval("A", zero, zero), interval("C", one, one)}),
      trace({interval("A", zero, zero), interval("D", one, one)}),
      trace({interval("B", zero, two), interval("C", one, two)}),
      trace({interval("C", zero, two), interval("D", one, two)}),
  });
  auto options = plus_options();
  options.dependency_threshold = 0.4;
  options.and_threshold = 0.4;
  const auto net = discover_heuristics_net(source, options);
  const auto a = activity(net, "A");
  const auto b = activity(net, "B");
  const auto c = activity(net, "C");
  const auto d = activity(net, "D");
  assert(net.output_bundles[a].size() == 2);
  assert((net.output_bundles[a][0] == HeuristicBundle{b, c}));
  assert((net.output_bundles[a][1] == HeuristicBundle{c, d}));

  options.and_threshold = 0.5;
  const auto strict = discover_heuristics_net(source, options);
  const auto strict_a = activity(strict, "A");
  assert(strict.output_bundles[strict_a].size() == 3);
  for (const auto& bundle : strict.output_bundles[strict_a])
    assert(bundle.size() == 1);
}

void performance_sorting_and_mean() {
  // EFG uses a sorted copy, while a pre-interval log's performance DFG keeps
  // the original order. The reversed second trace therefore contributes to
  // EFG A->B but not to its direct-adjacency performance.
  const auto source = log({
      trace({interval("A", "2026-01-01T00:00:00Z",
                      "2026-01-01T00:00:01Z"),
             interval("B", "2026-01-01T00:00:06Z",
                      "2026-01-01T00:00:06Z")}),
      trace({interval("B", "2026-01-02T00:00:16Z",
                      "2026-01-02T00:00:16Z"),
             interval("A", "2026-01-02T00:00:00Z",
                      "2026-01-02T00:00:01Z")}),
  });
  auto options = plus_options();
  options.decoration = HeuristicNetDecoration::kPerformance;
  const auto net = discover_heuristics_net(source, options);
  const auto& ab = relation(net, "A", "B");
  assert(ab.eventually_follows_frequency == 2);
  assert(ab.direct_adjacency_frequency == 1);
  assert(std::abs(ab.mean_wait_seconds - 5.0) < 1e-9);

  const auto aggregation_source = log({
      trace({interval("A", "2026-04-01T00:00:00Z",
                      "2026-04-01T00:00:01Z"),
             interval("B", "2026-04-01T00:00:06Z",
                      "2026-04-01T00:00:06Z")}),
      trace({interval("A", "2026-04-02T00:00:00Z",
                      "2026-04-02T00:00:01Z"),
             interval("B", "2026-04-02T00:00:16Z",
                      "2026-04-02T00:00:16Z")}),
  });
  options.performance_aggregation =
      HeuristicPerformanceAggregation::kMaximum;
  const auto maximum =
      discover_heuristics_net(aggregation_source, options);
  assert(edge(maximum, "A", "B")->representation == 15.0);
  options.performance_aggregation = HeuristicPerformanceAggregation::kSum;
  const auto sum = discover_heuristics_net(aggregation_source, options);
  assert(edge(sum, "A", "B")->representation == 20.0);
  options.performance_aggregation =
      HeuristicPerformanceAggregation::kRawValues;
  const auto raw = discover_heuristics_net(aggregation_source, options);
  assert((relation(raw, "A", "B").wait_samples ==
          std::vector<double>{5.0, 15.0}));
}

void invalid_timestamps_throw_only_for_plus_plus() {
  const auto source = log({trace({event("A", "not-a-time")})});
  auto options = plus_options();
  bool threw = false;
  try {
    (void)discover_heuristics_net(source, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  // Classic's established untimed behavior is unchanged.
  const auto classic = discover_heuristics_net(source);
  assert(classic.variant == HeuristicMinerVariant::kClassic);
  assert(classic.activities == std::vector<std::string>{"A"});
}

HeuristicClassicDfgInput classic_input(
    std::initializer_list<
        std::pair<const std::pair<std::string, std::string>, std::uint64_t>>
        edges) {
  HeuristicClassicDfgInput input;
  input.dfg = HeuristicFrequencyDfg(edges);
  return input;
}

void classic_defaults_dependency_filters_and_self_loop() {
  auto input = classic_input({{{"A", "B"}, 1}});
  auto net = discover_heuristics_net(input);
  assert(net.variant == HeuristicMinerVariant::kClassic);
  assert(edge(net, "A", "B") != nullptr);
  assert(edge(net, "A", "B")->dependency == 0.5);
  assert(!HeuristicMinerOptions{}.connect_all);

  HeuristicMinerOptions options;
  options.dependency_threshold = 0.500001;
  net = discover_heuristics_net(input, options);
  assert(net.edges.empty());
  assert(net.active_activities.size() == 2);  // no-node fallback

  input = classic_input({{{"A", "A"}, 2}});
  net = discover_heuristics_net(input);
  assert(edge(net, "A", "A") != nullptr);
  assert(std::abs(edge(net, "A", "A")->dependency - 2.0 / 3.0) < 1e-9);

  input = classic_input({{{"A", "B"}, 2}});
  input.activities = std::vector<std::string>{"A", "B", "C"};
  input.activity_counts =
      std::map<std::string, std::uint64_t>{{"A", 2}, {"B", 2}, {"C", 1}};
  options = {};
  options.min_activity_count = 3;
  assert(discover_heuristics_net(input, options).edges.empty());
  options = {};
  options.min_frequency = 3;
  assert(discover_heuristics_net(input, options).edges.empty());
}

void classic_noise_cleaning_and_keep_once() {
  auto make_noise = [](std::uint64_t weak) {
    auto input = classic_input(
        {{{"A", "B"}, weak}, {{"A", "C"}, 100}, {{"D", "B"}, 100}});
    input.activities = std::vector<std::string>{"A", "B", "C", "D"};
    input.activity_counts = std::map<std::string, std::uint64_t>{
        {"A", 200}, {"B", 200}, {"C", 100}, {"D", 100}};
    input.window_two_dfg[{"A", "B"}] = weak;
    return input;
  };
  auto at_boundary = discover_heuristics_net(make_noise(5));
  assert(at_boundary.cleaned_dfg.contains({"A", "B"}));
  auto below = discover_heuristics_net(make_noise(4));
  assert(!below.cleaned_dfg.contains({"A", "B"}));
  assert(below.window_two_dfg.contains({"A", "B"}));  // main DFG only
  HeuristicMinerOptions options;
  options.most_common_paths.insert({"A", "B"});
  assert(discover_heuristics_net(make_noise(4), options)
             .cleaned_dfg.contains({"A", "B"}));
  options = {};
  options.dfg_pre_cleaning_noise_threshold = 0.0;
  assert(discover_heuristics_net(make_noise(4), options)
             .cleaned_dfg.contains({"A", "B"}));

  const auto repeated =
      log({trace({event("A", ""), event("B", ""), event("A", ""),
                  event("B", ""), event("A", ""), event("B", "")})});
  options = {};
  options.keep_once_per_case = true;
  const auto deduplicated = discover_heuristics_net(repeated, options);
  assert(deduplicated.raw_dfg.at({"A", "B"}) == 1);
  assert(deduplicated.raw_dfg.at({"B", "A"}) == 1);
  assert(deduplicated.frequency_triples.at({"A", "B", "A"}) == 2);
  assert(deduplicated.frequency_triples.at({"B", "A", "B"}) == 2);
}

void classic_and_maximal_cliques_at_equality() {
  auto input = classic_input(
      {{{"A", "B"}, 1}, {{"A", "C"}, 1}, {{"A", "D"}, 1},
       {{"B", "C"}, 1}, {{"C", "D"}, 1}});
  input.activity_counts = std::map<std::string, std::uint64_t>{
      {"A", 3}, {"B", 2}, {"C", 3}, {"D", 2}};
  HeuristicMinerOptions options;
  options.and_threshold = 1.0 / 3.0;
  options.dfg_pre_cleaning_noise_threshold = 0.0;
  const auto net = discover_heuristics_net(input, options);
  const auto a = activity(net, "A");
  const auto b = activity(net, "B");
  const auto c = activity(net, "C");
  const auto d = activity(net, "D");
  assert(net.output_bundles[a].size() == 2);
  assert((net.output_bundles[a][0] == HeuristicBundle{b, c}));
  assert((net.output_bundles[a][1] == HeuristicBundle{c, d}));
}

void classic_length_two_rescue_and_reference_quirk() {
  auto loop = classic_input(
      {{{"A", "B"}, 2}, {{"A", "C"}, 10}});
  loop.activities = std::vector<std::string>{"A", "B", "C"};
  loop.activity_counts = std::map<std::string, std::uint64_t>{
      {"A", 12}, {"B", 2}, {"C", 10}};
  loop.frequency_triples[{"A", "B", "A"}] = 1;  // exactly l2 == 0.5
  HeuristicMinerOptions options;
  options.dependency_threshold = 0.9;
  options.dfg_pre_cleaning_noise_threshold = 0.0;
  auto net = discover_heuristics_net(loop, options);
  assert(edge(net, "A", "C") != nullptr);  // creates A before loop pass
  assert(edge(net, "A", "B") != nullptr);
  assert(edge(net, "B", "A") != nullptr);
  assert(edge(net, "A", "B")->length_two_loop);
  assert(edge(net, "A", "B")->frequency == 2);
  assert(edge(net, "B", "A")->frequency == 0);
  assert(edge(net, "B", "A")->dependency == 0.0);

  auto performance_loop = loop;
  performance_loop.performance_dfg.emplace();
  (*performance_loop.performance_dfg)[{"A", "B"}] = {2.0};
  (*performance_loop.performance_dfg)[{"A", "C"}] = {10.0};
  (*performance_loop.performance_dfg)[{"B", "A"}] = {4.0};
  auto performance_options = options;
  performance_options.decoration = HeuristicNetDecoration::kPerformance;
  performance_options.performance_aggregation =
      HeuristicPerformanceAggregation::kRawValues;
  const auto performance_net =
      discover_heuristics_net(performance_loop, performance_options);
  assert((std::get<std::vector<double>>(
              *edge(performance_net, "A", "B")->representation_value) ==
          std::vector<double>{2.0}));
  assert((std::get<std::vector<double>>(
              *edge(performance_net, "B", "A")->representation_value) ==
          std::vector<double>{4.0}));

  options.loop_length_two_threshold = 0.500001;
  assert(edge(discover_heuristics_net(loop, options), "A", "B") == nullptr);

  auto only_loop = classic_input(
      {{{"A", "B"}, 1}, {{"B", "A"}, 1}});
  only_loop.frequency_triples[{"A", "B", "A"}] = 2;
  only_loop.activity_counts =
      std::map<std::string, std::uint64_t>{{"A", 2}, {"B", 2}};
  options.loop_length_two_threshold = 0.5;
  const auto fallback = discover_heuristics_net(only_loop, options);
  assert(fallback.edges.empty());  // no pre-existing node => no rescue
  assert(fallback.active_activities.size() == 2);
}

void classic_active_nodes_conversion_and_dfg_inference() {
  auto input = classic_input({{{"B", "C"}, 2}, {{"A", "B"}, 4}});
  auto inferred = discover_heuristics_net(input);
  assert((inferred.activities == std::vector<std::string>{"A", "B", "C"}));
  assert(activity_stat(inferred, "A").occurrences == 4);
  assert(activity_stat(inferred, "B").occurrences == 3);
  assert(activity_stat(inferred, "C").occurrences == 2);
  assert(inferred.start_activities ==
         std::vector<std::size_t>{activity(inferred, "A")});
  assert(inferred.end_activities ==
         std::vector<std::size_t>{activity(inferred, "C")});

  input.activities = std::vector<std::string>{"A", "B", "C", "D"};
  input.activity_counts = std::map<std::string, std::uint64_t>{
      {"A", 4}, {"B", 3}, {"C", 2}, {"D", 100}};
  const auto partial = discover_heuristics_net(input);
  assert(partial.active_activities.size() == 3);
  const auto petri = to_petri_net(partial);
  bool has_d = false;
  for (const auto& transition : petri.transitions())
    has_d |= !transition.silent() &&
             petri.activities()[*transition.activity] == "D";
  assert(!has_d);

  HeuristicMinerOptions plus = plus_options();
  bool rejected = false;
  try {
    (void)discover_heuristics_net(input, plus);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void classic_performance_aggregations_and_validation() {
  auto a1 = event("A", "2026-03-01T00:00:01Z");
  auto b1 = event("B", "2026-03-01T00:00:06Z");
  attr(b1, "start_timestamp", "2026-03-01T00:00:06Z");
  auto a2 = event("A", "2026-03-02T00:00:01Z");
  auto b2 = event("B", "2026-03-02T00:00:16Z");
  attr(b2, "start_timestamp", "2026-03-02T00:00:16Z");
  HeuristicMinerOptions options;
  options.decoration = HeuristicNetDecoration::kPerformance;
  options.performance_aggregation = HeuristicPerformanceAggregation::kAll;
  const auto net =
      discover_heuristics_net(log({trace({a1, b1}), trace({a2, b2})}),
                              options);
  const auto& stat = relation(net, "A", "B");
  assert((stat.wait_samples == std::vector<double>{5.0, 15.0}));
  assert(stat.mean_wait_seconds == 10.0);
  assert(stat.median_wait_seconds == 10.0);
  assert(stat.min_wait_seconds == 5.0);
  assert(stat.max_wait_seconds == 15.0);
  assert(stat.sum_wait_seconds == 20.0);
  assert(std::abs(stat.stdev_wait_seconds - std::sqrt(50.0)) < 1e-9);
  assert(edge(net, "A", "B")->representation == 10.0);
  const auto* all_edge = edge(net, "A", "B");
  assert(all_edge->representation_value.has_value());
  const auto& all_edge_summary =
      std::get<HeuristicPerformanceSummary>(*all_edge->representation_value);
  assert(all_edge_summary.mean == 10.0);
  assert(all_edge_summary.median == 10.0);
  assert(all_edge_summary.minimum == 5.0);
  assert(all_edge_summary.maximum == 15.0);
  assert(std::abs(all_edge_summary.stdev - std::sqrt(50.0)) < 1e-9);
  assert(all_edge_summary.sum == 20.0);
  assert(stat.representation_value.has_value());
  assert(std::get<HeuristicPerformanceSummary>(*stat.representation_value).sum ==
         20.0);

  options.performance_aggregation = HeuristicPerformanceAggregation::kRawValues;
  const auto raw = discover_heuristics_net(
      log({trace({a1, b1}), trace({a2, b2})}), options);
  const auto* raw_edge = edge(raw, "A", "B");
  assert(raw_edge->representation == 10.0);  // scalar compatibility value
  assert(raw_edge->representation_value.has_value());
  assert((std::get<std::vector<double>>(*raw_edge->representation_value) ==
          std::vector<double>{5.0, 15.0}));
  assert((std::get<std::vector<double>>(
              *relation(raw, "A", "B").representation_value) ==
          std::vector<double>{5.0, 15.0}));

  // PLUSPLUS uses the same public raw/all value shapes while retaining its
  // established scalar compatibility projection.
  auto plus = plus_options();
  plus.decoration = HeuristicNetDecoration::kPerformance;
  plus.performance_aggregation = HeuristicPerformanceAggregation::kRawValues;
  const auto plus_raw = discover_heuristics_net(
      log({trace({a1, b1}), trace({a2, b2})}), plus);
  const auto* plus_raw_edge = edge(plus_raw, "A", "B");
  assert(plus_raw_edge->representation == 10.0);
  assert((std::get<std::vector<double>>(
              *plus_raw_edge->representation_value) ==
          std::vector<double>{5.0, 15.0}));
  plus.performance_aggregation = HeuristicPerformanceAggregation::kAll;
  const auto plus_all = discover_heuristics_net(
      log({trace({a1, b1}), trace({a2, b2})}), plus);
  const auto& plus_all_summary = std::get<HeuristicPerformanceSummary>(
      *edge(plus_all, "A", "B")->representation_value);
  assert(plus_all_summary.mean == 10.0);
  assert(plus_all_summary.sum == 20.0);

  auto input = classic_input({{{"A", "B"}, 1}});
  input.performance_dfg.emplace();  // explicitly supplied but incomplete
  bool incomplete = false;
  try {
    (void)discover_heuristics_net(input, options);
  } catch (const std::invalid_argument&) {
    incomplete = true;
  }
  assert(incomplete);

  auto invalid_a = event("A", "invalid");
  auto valid_b = event("B", "2026-03-01T00:00:01Z");
  bool invalid = false;
  try {
    (void)discover_heuristics_net(log({trace({invalid_a, valid_b})}),
                                  options);
  } catch (const std::invalid_argument&) {
    invalid = true;
  }
  assert(invalid);
}

void start_selector_defaults_and_validation() {
  auto first = interval("A", "2026-07-01T00:00:00Z",
                        "2026-07-01T00:00:00Z");
  auto second = interval("B", "2026-07-01T00:00:03Z",
                         "2026-07-01T00:00:19Z");
  attr(second, "custom_start", "2026-07-01T00:00:04Z");
  const auto source = log({trace({first, second})});

  HeuristicMinerOptions classic;
  classic.decoration = HeuristicNetDecoration::kPerformance;
  const auto completion = discover_heuristics_net(source, classic);
  assert(relation(completion, "A", "B").mean_wait_seconds == 19.0);

  classic.start_timestamp_attribute = "start_timestamp";
  const auto conventional = discover_heuristics_net(source, classic);
  assert(relation(conventional, "A", "B").mean_wait_seconds == 3.0);

  classic.start_timestamp_attribute = "custom_start";
  const auto custom = discover_heuristics_net(source, classic);
  assert(relation(custom, "A", "B").mean_wait_seconds == 4.0);

  auto sparse_first = event("A", "");
  attr(sparse_first, "finished", "2026-07-01T00:00:00Z");
  auto sparse_second = event("B", "");
  attr(sparse_second, "started", "2026-07-01T00:00:03Z");
  classic.timestamp_attribute = "finished";
  classic.start_timestamp_attribute = "started";
  const auto sparse = discover_heuristics_net(
      log({trace({sparse_first, sparse_second})}), classic);
  assert(relation(sparse, "A", "B").mean_wait_seconds == 3.0);

  classic.start_timestamp_attribute = "time:timestamp";
  classic.timestamp_attribute = "time:timestamp";
  const auto explicit_completion = discover_heuristics_net(source, classic);
  assert(relation(explicit_completion, "A", "B").mean_wait_seconds == 19.0);

  classic.start_timestamp_attribute = "missing_start";
  bool missing_threw = false;
  try {
    (void)discover_heuristics_net(source, classic);
  } catch (const std::invalid_argument&) {
    missing_threw = true;
  }
  assert(missing_threw);

  auto negative = event("B", "2026-07-01T00:00:20Z");
  attr(negative, "start_timestamp", "2026-06-30T23:59:58Z");
  classic.start_timestamp_attribute = "start_timestamp";
  const auto clamped = discover_heuristics_net(
      log({trace({interval("A", "2026-07-01T00:00:00Z",
                           "2026-07-01T00:00:10Z"),
                  negative})}),
      classic);
  assert(relation(clamped, "A", "B").mean_wait_seconds == 0.0);

  auto plus = plus_options();
  plus.decoration = HeuristicNetDecoration::kPerformance;
  const auto plus_default = discover_heuristics_net(source, plus);
  assert(relation(plus_default, "A", "B").mean_wait_seconds == 3.0);

  plus.start_timestamp_attribute = "missing_start";
  const auto plus_lifecycle = discover_heuristics_net(source, plus);
  assert(relation(plus_lifecycle, "A", "B").mean_wait_seconds == 19.0);
}

void business_hours_cover_heuristic_waits_and_sojourns() {
  const auto friday_to_monday = log({trace({
      event("A", "2024-01-05T16:00:00Z"),
      event("B", "2024-01-08T10:00:00Z"),
  })});

  HeuristicMinerOptions classic;
  classic.decoration = HeuristicNetDecoration::kPerformance;
  classic.use_business_hours = true;
  const auto default_schedule =
      discover_heuristics_net(friday_to_monday, classic);
  assert(std::abs(relation(default_schedule, "A", "B").mean_wait_seconds -
                  14400.0) < 1e-9);

  // A custom all-week slot exposes the raw 66-hour span, while an explicitly
  // empty schedule measures no elapsed business time.
  classic.business_hour_slots = {{0.0, 604800.0}};
  classic.business_hour_slots_explicit = true;
  const auto all_day = discover_heuristics_net(friday_to_monday, classic);
  assert(std::abs(relation(all_day, "A", "B").mean_wait_seconds -
                  237600.0) < 1e-9);

  classic.business_hour_slots.clear();
  const auto no_business_time =
      discover_heuristics_net(friday_to_monday, classic);
  assert(relation(no_business_time, "A", "B").mean_wait_seconds == 0.0);

  // Business time follows local wall-clock values after parsing UTC offsets;
  // this pair is positive in UTC but reversed in local wall time.
  classic.business_hour_slots_explicit = false;
  const auto mixed_offsets = log({trace({
      event("A", "2024-01-08T12:00:00+05:30"),
      event("B", "2024-01-08T10:00:00+00:00"),
  })});
  const auto mixed_net = discover_heuristics_net(mixed_offsets, classic);
  assert(relation(mixed_net, "A", "B").mean_wait_seconds == 0.0);

  auto plus = plus_options();
  plus.decoration = HeuristicNetDecoration::kFrequency;
  plus.use_business_hours = true;
  const auto intervals = log({trace({
      interval("A", "2024-01-05T15:00:00+05:30",
               "2024-01-05T16:00:00+05:30"),
      interval("B", "2024-01-08T09:00:00+00:00",
               "2024-01-08T10:00:00+00:00"),
  })});
  const auto plus_net = discover_heuristics_net(intervals, plus);
  assert(std::abs(activity_stat(plus_net, "A").mean_sojourn_seconds -
                  3600.0) < 1e-9);
  assert(std::abs(activity_stat(plus_net, "B").mean_sojourn_seconds -
                  3600.0) < 1e-9);
  assert(std::abs(relation(plus_net, "A", "B").mean_wait_seconds -
                  10800.0) < 1e-9);

  const auto negative_interval = log({trace({
      interval("N", "2024-01-08T10:00:00+00:00",
               "2024-01-08T09:00:00+00:00"),
  })});
  const auto negative_net = discover_heuristics_net(negative_interval, plus);
  assert(activity_stat(negative_net, "N").mean_sojourn_seconds == 0.0);

  auto a_start = event("A", "2024-01-05T15:00:00+05:30", "start");
  auto a_complete = event("A", "2024-01-05T16:00:00+05:30", "complete");
  auto b_start = event("B", "2024-01-08T09:00:00+00:00", "start");
  auto b_complete = event("B", "2024-01-08T10:00:00+00:00", "complete");
  for (auto* current : {&a_start, &a_complete})
    attr(*current, "concept:instance", "a1");
  for (auto* current : {&b_start, &b_complete})
    attr(*current, "concept:instance", "b1");
  const auto lifecycle_net = discover_heuristics_net(
      log({trace({a_start, b_start, a_complete, b_complete})}), plus);
  assert(std::abs(activity_stat(lifecycle_net, "A").mean_sojourn_seconds -
                  3600.0) < 1e-9);
  assert(std::abs(activity_stat(lifecycle_net, "B").mean_sojourn_seconds -
                  3600.0) < 1e-9);
  assert(std::abs(relation(lifecycle_net, "A", "B").mean_wait_seconds -
                  10800.0) < 1e-9);
}

void subsecond_timestamps_preserve_exact_order_and_business_prefix() {
  const auto source = log({trace({
      event("A", "2026-01-02T16:59:59.999998Z"),
      event("B", "2026-01-05T07:00:00.000003Z"),
  })});
  auto classic = HeuristicMinerOptions{};
  classic.decoration = HeuristicNetDecoration::kPerformance;
  classic.start_timestamp_attribute = "start_timestamp";
  auto b = source.traces.front().events.back();
  attr(b, "start_timestamp", "2026-01-05T07:00:00.000001Z");
  const auto exact = discover_heuristics_net(
      log({trace({source.traces.front().events.front(), b})}), classic);
  assert(relation(exact, "A", "B").mean_wait_seconds == 223200.000003);

  classic.use_business_hours = true;
  const auto business = discover_heuristics_net(
      log({trace({source.traces.front().events.front(), b})}), classic);
  assert(std::abs(relation(business, "A", "B").mean_wait_seconds -
                  3.00002284348011e-6) < 1e-18);

  // At the upper calendar boundary, binary64 epoch values cannot distinguish
  // adjacent microseconds. Exact interval keys must still retain the EFG edge
  // and reject the false overlap.
  auto plus = plus_options();
  plus.decoration = HeuristicNetDecoration::kPerformance;
  const auto large_year = log({trace({
      interval("A", "9999-01-01T00:00:00.000000Z",
               "9999-01-01T00:00:00.000001Z"),
      interval("B", "9999-01-01T00:00:00.000002Z",
               "9999-01-01T00:00:00.000003Z"),
  })});
  const auto net = discover_heuristics_net(large_year, plus);
  assert(relation(net, "A", "B").eventually_follows_frequency == 1);
  assert(relation(net, "A", "B").mean_wait_seconds == 1e-6);
  assert(net.concurrency_statistics.empty());

  // Sojourns retain the endpoint-float subtraction used by datetime.timestamp
  // even though their ordering and waits use exact microseconds.
  const auto pre_epoch = log({trace({
      interval("A", "1900-01-01T00:00:00.000001Z",
               "1900-01-01T00:00:00.000003Z"),
      interval("B", "1900-01-01T00:00:00.000004Z",
               "1900-01-01T00:00:00.000007Z"),
  })});
  const auto old_net = discover_heuristics_net(pre_epoch, plus);
  assert(activity_stat(old_net, "A").mean_sojourn_seconds ==
         1.9073486328125e-6);

  const auto epoch_boundary = discover_heuristics_net(log({trace({
      interval("A", "1969-12-31T23:59:59.999999Z",
               "1970-01-01T00:00:00.000000Z"),
      interval("B", "1970-01-01T00:00:00.000001Z",
               "1970-01-01T00:00:00.000002Z"),
  })}), plus);
  assert(activity_stat(epoch_boundary, "A").mean_sojourn_seconds == 1e-6);
  assert(relation(epoch_boundary, "A", "B").mean_wait_seconds == 1e-6);

  const auto long_interval = log({trace({
      interval("A", "0001-01-01T00:00:00.000000Z",
               "0001-01-01T00:00:00.000000Z"),
      interval("B", "9999-01-01T00:00:00.000017Z",
               "9999-01-01T00:00:00.000017Z"),
  })});
  // Public timedelta.total_seconds(): round the integer duration only once.
  for (auto options : {classic, plus}) {
    options.use_business_hours = false;
    const auto long_net = discover_heuristics_net(long_interval, options);
    assert(relation(long_net, "A", "B").mean_wait_seconds ==
           0x1.25d6941400000p+38);
  }
}

void configurable_activity_and_timestamp_fields() {
  auto a = event("canonical-a", "not-a-canonical-time");
  auto b = event("canonical-b", "also-not-a-canonical-time");
  attr(a, "phase", "A");
  attr(b, "phase", "B");
  attr(a, "finished_at", "2026-06-01T00:00:01Z");
  attr(b, "finished_at", "2026-06-01T00:05:01Z");

  HeuristicMinerOptions classic;
  classic.activity_attribute = "phase";
  classic.timestamp_attribute = "finished_at";
  classic.decoration = HeuristicNetDecoration::kPerformance;
  const auto net = discover_heuristics_net(log({trace({a, b})}), classic);
  assert((net.activities == std::vector<std::string>{"A", "B"}));
  assert(net.activity_attribute == "phase");
  assert(net.timestamp_attribute == "finished_at");
  assert(net.activity_ids_are_local);
  assert(edge(net, "A", "B") != nullptr);
  assert(std::abs(edge(net, "A", "B")->representation - 300.0) < 1e-9);

  auto interval_a = event("canonical-a", "invalid-completion");
  auto interval_b = event("canonical-b", "invalid-completion");
  attr(interval_a, "phase", "A");
  attr(interval_b, "phase", "B");
  attr(interval_a, "finished_at", "2026-06-02T00:00:05Z");
  attr(interval_b, "finished_at", "2026-06-02T00:00:09Z");
  attr(interval_a, "started_at", "2026-06-02T00:00:00Z");
  attr(interval_b, "started_at", "2026-06-02T00:00:06Z");
  auto plus = plus_options();
  plus.activity_attribute = "phase";
  plus.timestamp_attribute = "finished_at";
  plus.start_timestamp_attribute = "started_at";
  const auto plus_net =
      discover_heuristics_net(log({trace({interval_a, interval_b})}), plus);
  assert((plus_net.activities == std::vector<std::string>{"A", "B"}));
  assert(plus_net.activity_ids_are_local);
  assert(std::abs(activity_stat(plus_net, "A").mean_sojourn_seconds - 5.0) <
         1e-9);
  assert(std::abs(activity_stat(plus_net, "B").mean_sojourn_seconds - 3.0) <
         1e-9);
  const auto decorated = decorate_model(
      log({trace({interval_a, interval_b})}), to_petri_net(plus_net), "phase",
      "finished_at");
  assert(decorated.trace_count == 1);
  assert(decorated.decorated_traces == 1);

  auto missing = event("canonical", "2026-06-02T00:00:10Z");
  bool threw = false;
  try {
    (void)discover_heuristics_net(log({trace({missing, b})}), classic);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

}  // namespace

int main() {
  interval_abstraction_and_strict_threshold();
  lifecycle_fifo_and_configurable_interval_attribute();
  lifecycle_input_shapes_defer_ignored_timestamp_parsing();
  filtering_and_unfiltered_statistics();
  dependency_concurrency_and_no_connect_all();
  and_bundles_are_overlapping_maximal_cliques();
  performance_sorting_and_mean();
  invalid_timestamps_throw_only_for_plus_plus();
  classic_defaults_dependency_filters_and_self_loop();
  classic_noise_cleaning_and_keep_once();
  classic_and_maximal_cliques_at_equality();
  classic_length_two_rescue_and_reference_quirk();
  classic_active_nodes_conversion_and_dfg_inference();
  classic_performance_aggregations_and_validation();
  start_selector_defaults_and_validation();
  business_hours_cover_heuristic_waits_and_sojourns();
  subsecond_timestamps_preserve_exact_order_and_business_prefix();
  configurable_activity_and_timestamp_fields();
  std::cout << "heuristic variant tests passed\n";
}
