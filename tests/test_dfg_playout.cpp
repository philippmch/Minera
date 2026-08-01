#include <cassert>
#include <map>
#include <stdexcept>
#include <string>

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

}  // namespace

int main() {
  classic_is_best_first_and_deterministic();
  classic_respects_occurrence_cap();
  performance_sampling_is_seeded_and_timestamped();
  performance_uses_frequency_weighted_choices();
  invalid_performance_inputs_are_rejected();
  classic_timeout_and_probability_settings_bound_work();
  classic_complete_only_and_variant_metadata_are_effective();
}
