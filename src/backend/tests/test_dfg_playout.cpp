#include <cassert>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "minera/dfg_playout.hpp"
#include "minera/timestamp.hpp"

namespace {

using minera::DfgActivityCounts;
using minera::DfgFrequency;

void classic_is_best_first_and_deterministic() {
  DfgFrequency dfg{{{"A", "B"}, 3}, {{"A", "C"}, 1}};
  DfgActivityCounts starts{{"A", 1}};
  DfgActivityCounts ends{{"B", 1}, {"C", 1}};
  minera::DfgPlayoutOptions options;
  options.max_variants = 2;
  options.max_occurrences_per_activity = 1;
  const auto first = minera::dfg_playout(dfg, starts, ends, options);
  const auto second = minera::dfg_playout(dfg, starts, ends, options);
  assert(first.traces.size() == 2);
  assert(first.traces[0].events[1].activity == "B");
  assert(first.traces[1].events[1].activity == "C");
  assert(first.traces[0].events[0].timestamp ==
         second.traces[0].events[0].timestamp);
}

void classic_respects_occurrence_cap() {
  DfgFrequency dfg{{{"A", "A"}, 10}};
  DfgActivityCounts starts{{"A", 1}};
  DfgActivityCounts ends{{"A", 1}};
  minera::DfgPlayoutOptions options;
  options.max_variants = 10;
  options.max_occurrences_per_activity = 2;
  const auto log = minera::dfg_playout(dfg, starts, ends, options);
  assert(log.traces.size() == 2);
  assert(log.traces[0].events.size() == 1);
  assert(log.traces[1].events.size() == 2);
}

void performance_sampling_is_seeded_and_timestamped() {
  DfgFrequency dfg{{{"A", "B"}, 1}};
  DfgActivityCounts starts{{"A", 1}};
  DfgActivityCounts ends{{"B", 1}};
  minera::DfgPerformance performance;
  minera::DfgPerformanceStatistics stats;
  stats.summary.mean = 5.0;
  stats.value = 5.0;
  performance[{"A", "B"}] = stats;
  minera::DfgPlayoutOptions options;
  options.variant = minera::DfgPlayoutVariant::kPerformance;
  options.traces = 4;
  options.seed = 42;
  options.start_time = "2026-01-01T00:00:00Z";
  options.case_arrival_rate = 10.0;
  const auto first = minera::dfg_playout(dfg, starts, ends, options, &performance);
  const auto second = minera::dfg_playout(dfg, starts, ends, options, &performance);
  assert(first.traces.size() == 4);
  assert(first.traces.size() == second.traces.size());
  for (std::size_t i = 0; i < first.traces.size(); ++i) {
    assert(first.traces[i].case_id == second.traces[i].case_id);
    assert(first.traces[i].events.size() == second.traces[i].events.size());
    for (std::size_t j = 0; j < first.traces[i].events.size(); ++j) {
      assert(first.traces[i].events[j].activity ==
             second.traces[i].events[j].activity);
      assert(first.traces[i].events[j].timestamp ==
             second.traces[i].events[j].timestamp);
    }
  }
  assert(minera::parse_timestamp(first.traces[0].events[1].timestamp).has_value());
  assert(first.traces[0].events[1].timestamp !=
         first.traces[0].events[0].timestamp);
}

void performance_uses_frequency_weighted_choices() {
  DfgFrequency dfg{{{"A", "B"}, 9}, {{"A", "C"}, 1}};
  DfgActivityCounts starts{{"A", 1}};
  DfgActivityCounts ends{{"B", 1}, {"C", 1}};
  minera::DfgPerformance performance;
  for (const auto& edge : {minera::DfgActivityPair{"A", "B"},
                           minera::DfgActivityPair{"A", "C"}}) {
    minera::DfgPerformanceStatistics stats;
    stats.summary.mean = 0.0;
    stats.value = 0.0;
    performance[edge] = stats;
  }
  minera::DfgPlayoutOptions options;
  options.variant = minera::DfgPlayoutVariant::kPerformance;
  options.traces = 1000;
  options.seed = 123;
  const auto log =
      minera::dfg_playout(dfg, starts, ends, options, &performance);
  std::size_t b = 0;
  std::size_t c = 0;
  for (const auto& trace : log.traces) {
    assert(trace.events.size() == 2);
    if (trace.events[1].activity == "B") ++b;
    if (trace.events[1].activity == "C") ++c;
  }
  assert(b > c * 3);
}

void invalid_performance_inputs_are_rejected() {
  DfgFrequency dfg{{{"A", "B"}, 1}};
  DfgActivityCounts starts{{"A", 1}};
  DfgActivityCounts ends{{"B", 1}};
  minera::DfgPlayoutOptions options;
  options.variant = minera::DfgPlayoutVariant::kPerformance;
  bool threw = false;
  try {
    (void)minera::dfg_playout(dfg, starts, ends, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  minera::DfgPerformance performance;
  minera::DfgPerformanceStatistics stats;
  stats.summary.mean = -1.0;
  stats.value = -1.0;
  performance[{"A", "B"}] = stats;
  threw = false;
  try {
    (void)minera::dfg_playout(dfg, starts, ends, options, &performance);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void classic_timeout_and_probability_settings_bound_work() {
  DfgFrequency dfg{{{"A", "B"}, 3}, {{"A", "C"}, 1}};
  DfgActivityCounts starts{{"A", 1}};
  DfgActivityCounts ends{{"B", 1}, {"C", 1}};
  minera::DfgPlayoutOptions options;
  options.max_variants = 1;
  options.return_only_if_complete = true;
  options.max_execution_time_ms = 1000;
  const auto log = minera::dfg_playout(dfg, starts, ends, options);
  assert(log.traces.empty());
}

void classic_complete_only_and_variant_metadata_are_effective() {
  DfgFrequency dfg{{{"A", "B"}, 1}, {{"X", "Y"}, 1}};
  DfgActivityCounts starts{{"A", 1}};
  DfgActivityCounts ends{{"B", 1}};
  minera::DfgPlayoutOptions incomplete;
  incomplete.max_variants = 10;
  incomplete.return_only_if_complete = true;
  const auto suppressed =
      minera::dfg_playout(dfg, starts, ends, incomplete);
  assert(suppressed.traces.empty());

  DfgFrequency simple{{{"A", "B"}, 1}};
  DfgActivityCounts simple_start{{"A", 1}};
  DfgActivityCounts simple_end{{"B", 1}};
  minera::DfgPlayoutOptions variants;
  variants.max_variants = 1;
  variants.return_variants = true;
  const auto with_metadata =
      minera::dfg_playout(simple, simple_start, simple_end, variants);
  assert(with_metadata.traces.size() == 1);
  bool found_variant = false;
  for (const auto& attribute : with_metadata.traces.front().attributes) {
    found_variant = found_variant || attribute.key == "variant";
  }
  assert(found_variant);
}

void classic_result_exposes_deterministic_variant_counts() {
  DfgFrequency dfg{{{"A", "B"}, 3}, {{"A", "C"}, 1}};
  DfgActivityCounts starts{{"A", 1}};
  DfgActivityCounts ends{{"B", 1}, {"C", 1}};
  minera::DfgPlayoutOptions options;
  options.max_variants = 2;
  options.return_variants = true;

  const auto result =
      minera::dfg_playout_result(dfg, starts, ends, options);
  assert(result.variant == minera::DfgPlayoutVariant::kClassic);
  assert(result.log.traces.size() == 2);
  assert(result.counts.size() == 2);
  assert(result.counts.at(std::vector<std::string>{"A", "B"}) == 2);
  assert(result.counts.at(std::vector<std::string>{"A", "C"}) == 1);
  assert(result.counts.begin()->first ==
         (std::vector<std::string>{"A", "B"}));

  const auto repeat =
      minera::dfg_playout_result(dfg, starts, ends, options);
  assert(repeat.counts == result.counts);
  assert(repeat.log.traces.size() == result.log.traces.size());
}

void classic_result_honors_minimum_variant_occurrences() {
  DfgFrequency dfg{{{"A", "B"}, 3}, {{"A", "C"}, 1}};
  DfgActivityCounts starts{{"A", 1}};
  DfgActivityCounts ends{{"B", 1}, {"C", 1}};
  minera::DfgPlayoutOptions options;
  options.max_variants = 2;
  options.return_variants = true;
  options.min_variant_occurrences = 2;

  const auto result =
      minera::dfg_playout_result(dfg, starts, ends, options);
  assert(result.counts.size() == 1);
  assert(result.counts.at(std::vector<std::string>{"A", "B"}) == 2);
  assert(result.log.traces.size() == 1);
}

void classic_result_complete_only_suppresses_partial_counts() {
  DfgFrequency dfg{{{"A", "B"}, 1}, {{"X", "Y"}, 1}};
  DfgActivityCounts starts{{"A", 1}};
  DfgActivityCounts ends{{"B", 1}};
  minera::DfgPlayoutOptions options;
  options.max_variants = 1;
  options.return_variants = true;
  options.return_only_if_complete = true;

  const auto result =
      minera::dfg_playout_result(dfg, starts, ends, options);
  assert(!result.complete);
  assert(result.log.traces.empty());
  assert(result.counts.empty());
}

void performance_result_keeps_legacy_log_without_variant_counts() {
  DfgFrequency dfg{{{"A", "B"}, 1}};
  DfgActivityCounts starts{{"A", 1}};
  DfgActivityCounts ends{{"B", 1}};
  minera::DfgPerformance performance;
  minera::DfgPerformanceStatistics stats;
  stats.summary.mean = 1.0;
  stats.value = 1.0;
  performance[{"A", "B"}] = stats;
  minera::DfgPlayoutOptions options;
  options.variant = minera::DfgPlayoutVariant::kPerformance;
  options.return_variants = true;
  options.traces = 2;
  options.max_events_per_trace = 2;

  const auto result = minera::dfg_playout_result(
      dfg, starts, ends, options, &performance);
  assert(result.variant == minera::DfgPlayoutVariant::kPerformance);
  assert(result.counts.empty());
  assert(result.log.traces.size() == 2);
  assert(result.complete && !result.interrupted);
  assert(result.completed_traces == 2 && result.truncated_traces == 0);
}

void performance_result_reports_cutoff_and_deadlock() {
  DfgFrequency loop{{{"A", "A"}, 1}};
  DfgActivityCounts starts{{"A", 1}};
  DfgActivityCounts ends;
  minera::DfgPerformance performance;
  minera::DfgPerformanceStatistics stats;
  stats.summary.mean = 0.0;
  stats.value = 0.0;
  performance[{"A", "A"}] = stats;
  minera::DfgPlayoutOptions options;
  options.variant = minera::DfgPlayoutVariant::kPerformance;
  options.traces = 3;
  options.max_events_per_trace = 2;
  const auto result = minera::dfg_playout_result(
      loop, starts, ends, options, &performance);
  assert(!result.complete && result.interrupted);
  assert(result.completed_traces == 0 && result.truncated_traces == 3);
}

void classic_completion_flag_controls_early_stop() {
  DfgFrequency dfg{{{"A", "A"}, 1}, {{"A", "B"}, 1}};
  DfgActivityCounts starts{{"A", 1}};
  DfgActivityCounts ends{{"A", 1}, {"B", 1}};

  minera::DfgPlayoutOptions continuing;
  continuing.max_variants = 8;
  continuing.max_occurrences_per_activity = 3;
  const auto all =
      minera::dfg_playout_result(dfg, starts, ends, continuing);
  assert(all.complete);
  assert(all.counts.size() > 2);

  continuing.interrupt_when_dfg_complete = true;
  const auto stopped =
      minera::dfg_playout_result(dfg, starts, ends, continuing);
  assert(stopped.complete);
  assert(stopped.counts.size() >= 3);
  assert(stopped.counts.size() < all.counts.size());
}

void custom_attribute_keys_are_emitted_for_both_variants() {
  DfgFrequency dfg{{{"A", "B"}, 1}};
  DfgActivityCounts starts{{"A", 1}};
  DfgActivityCounts ends{{"B", 1}};
  minera::DfgPlayoutOptions options;
  options.max_variants = 1;
  options.activity_key = "event:name";
  options.timestamp_key = "event:when";
  options.case_id_key = "case:external_id";
  options.artificial_start_activity = "BEGIN";
  options.artificial_end_activity = "FINISH";
  const auto classic = minera::dfg_playout(dfg, starts, ends, options);
  assert(classic.traces.size() == 1);
  assert(classic.traces.front().attribute("case:external_id") != nullptr);
  const auto& event = classic.traces.front().events.front();
  assert(event.attribute("event:name") != nullptr);
  assert(*event.attribute("event:name") == "A");
  assert(event.attribute("event:when") != nullptr);
  assert(*event.attribute("event:when") == event.timestamp);

  minera::DfgPerformance performance;
  minera::DfgPerformanceStatistics statistics;
  statistics.summary.mean = 0.0;
  statistics.value = 0.0;
  performance[{"A", "B"}] = statistics;
  options.variant = minera::DfgPlayoutVariant::kPerformance;
  options.traces = 2;
  options.max_events_per_trace = 2;
  const auto sampled =
      minera::dfg_playout(dfg, starts, ends, options, &performance);
  assert(sampled.traces.size() == 2);
  assert(sampled.traces.front().attribute("case:external_id") != nullptr);
  assert(sampled.traces.front().events.size() == 2);
  assert(sampled.traces.front().events.back().attribute("event:name") !=
         nullptr);
}

void performance_boundary_labels_are_collision_safe() {
  DfgFrequency dfg{{{"▶", "■"}, 1}};
  DfgActivityCounts starts{{"▶", 1}};
  DfgActivityCounts ends{{"■", 1}};
  minera::DfgPerformance performance;
  minera::DfgPerformanceStatistics statistics;
  statistics.summary.mean = 0.0;
  statistics.value = 0.0;
  performance[{"▶", "■"}] = statistics;
  minera::DfgPlayoutOptions options;
  options.variant = minera::DfgPlayoutVariant::kPerformance;
  options.traces = 3;
  options.max_events_per_trace = 3;
  options.artificial_start_activity = "▶";
  options.artificial_end_activity = "▶";
  const auto log =
      minera::dfg_playout(dfg, starts, ends, options, &performance);
  assert(log.traces.size() == 3);
  for (const auto& trace : log.traces) {
    assert(trace.events.size() == 2);
    assert(trace.events[0].activity == "▶");
    assert(trace.events[1].activity == "■");
  }
}

void custom_dfg_keys_reject_empty_values() {
  DfgFrequency dfg{{{"A", "B"}, 1}};
  DfgActivityCounts starts{{"A", 1}};
  DfgActivityCounts ends{{"B", 1}};
  minera::DfgPlayoutOptions options;
  options.activity_key.clear();
  bool threw = false;
  try {
    (void)minera::dfg_playout(dfg, starts, ends, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  options = {};
  options.artificial_end_activity.clear();
  threw = false;
  try {
    (void)minera::dfg_playout(dfg, starts, ends, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

}  // namespace

int main() {
  classic_is_best_first_and_deterministic();
  classic_respects_occurrence_cap();
  performance_sampling_is_seeded_and_timestamped();
  performance_uses_frequency_weighted_choices();
  invalid_performance_inputs_are_rejected();
  classic_timeout_and_probability_settings_bound_work();
  classic_complete_only_and_variant_metadata_are_effective();
  classic_result_exposes_deterministic_variant_counts();
  classic_result_honors_minimum_variant_occurrences();
  classic_result_complete_only_suppresses_partial_counts();
  performance_result_keeps_legacy_log_without_variant_counts();
  performance_result_reports_cutoff_and_deadlock();
  classic_completion_flag_controls_early_stop();
  custom_attribute_keys_are_emitted_for_both_variants();
  performance_boundary_labels_are_collision_safe();
  custom_dfg_keys_reject_empty_values();
}
