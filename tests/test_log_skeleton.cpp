#include "minera/log_skeleton.hpp"

#include <cassert>
#include <initializer_list>
#include <limits>
#include <string>

using namespace minera;

namespace {
EventLog make_log(std::initializer_list<std::initializer_list<const char*>> rows) {
  EventLog log;
  for (const auto& row : rows) {
    Trace trace;
    for (const auto* activity : row) trace.events.push_back(Event{"", activity});
    log.traces.push_back(std::move(trace));
  }
  return log;
}

bool has(const std::vector<LogSkeletonPair>& relations, const char* source,
         const char* target) {
  for (const auto& relation : relations)
    if (relation.source == source && relation.target == target) return true;
  return false;
}
}  // namespace

int main() {
  const auto base = discover_log_skeleton(make_log({{"A", "B", "A"}, {"A", "B"}}));
  assert(has(base.equivalence, "A", "B") == false);
  assert(!has(base.always_after, "A", "B"));
  assert(has(base.always_before, "B", "A"));
  assert(!has(base.directly_follows, "A", "B"));
  assert(has(discover_log_skeleton(make_log({{"A", "B"}})).directly_follows,
             "A", "B"));
  assert(base.activ_freq.at("A") == std::vector<std::size_t>({1, 2}));
  assert(base.activ_freq.at("B") == std::vector<std::size_t>({1}));

  const auto never = discover_log_skeleton(make_log({{"A"}, {"B"}}));
  assert(has(never.never_together, "A", "B"));
  assert(has(never.never_together, "B", "A"));
  LogSkeletonOptions zero_noise;
  zero_noise.noise_threshold = 1.0;
  assert(!has(discover_log_skeleton(make_log({{"A", "B"}}), zero_noise)
                  .never_together,
              "A", "B"));

  // A repeated activity contributes twice to all_activs, while a relation
  // observed in one weighted variant contributes only once per variant.
  EventLog weighted = make_log({{"A", "A"}, {"A", "B"}});
  LogSkeletonOptions noisy;
  noisy.noise_threshold = 0.5;
  const auto weighted_model = discover_log_skeleton(weighted, noisy);
  assert(!has(weighted_model.always_after, "A", "A"));
  noisy.noise_threshold = 2.0 / 3.0;
  assert(has(discover_log_skeleton(weighted, noisy).always_after, "A", "A"));

  // The pinned frequency loop drops lower-support buckets once its cumulative
  // event-count threshold is reached early.
  LogSkeletonOptions frequency_noise;
  frequency_noise.noise_threshold = 0.5;
  const auto all_buckets = discover_log_skeleton(
      make_log({{"A"}, {"A"}, {"A"}, {"A", "A"}}), frequency_noise);
  assert(all_buckets.activ_freq.at("A") == std::vector<std::size_t>({1}));

  // Equal-support buckets retain first-seen variant order: [AA] precedes [A],
  // so a threshold reached by the first bucket selects frequency 2.
  LogSkeletonOptions tie_noise;
  tie_noise.noise_threshold = 0.7;
  const auto tie_model = discover_log_skeleton(make_log({{"A", "A"}, {"A"}}), tie_noise);
  assert(tie_model.activ_freq.at("A") == std::vector<std::size_t>({2}));

  // The core preserves PM4Py's raw threshold arithmetic; user-facing parsers
  // validate [0,1], but direct callers are not silently normalized.
  LogSkeletonOptions negative_noise;
  negative_noise.noise_threshold = -0.5;
  assert(!has(discover_log_skeleton(make_log({{"A", "B"}}), negative_noise)
                  .directly_follows,
              "A", "B"));
  LogSkeletonOptions nan_noise;
  nan_noise.noise_threshold = std::numeric_limits<double>::quiet_NaN();
  assert(discover_log_skeleton(make_log({{"A", "B"}}), nan_noise)
             .directly_follows.empty());

  const auto fit = check_log_skeleton_conformance(
      std::vector<std::string>{"A", "B", "A"}, base);
  assert(fit.is_fit);
  assert(fit.no_dev_total == 0);
  const auto deviating = check_log_skeleton_conformance(
      std::vector<std::string>{"B", "A", "B"}, base);
  assert(!deviating.is_fit && deviating.no_dev_total > 0);

  // Unknown activities and missing positive frequencies are diagnostics.
  LogSkeletonModel frequency_model;
  frequency_model.activ_freq["A"] = {1};
  const auto unknown = check_log_skeleton_conformance(
      std::vector<std::string>{"A", "X"}, frequency_model);
  assert(!unknown.is_fit && unknown.no_dev_total == 1 &&
         unknown.deviations.front().observed_frequency == 0);
  const auto missing = check_log_skeleton_conformance(
      std::vector<std::string>{"X"}, frequency_model);
  assert(missing.no_dev_total == 2);  // X unknown, A required but absent

  // Family filtering and source-absent vacuity.
  LogSkeletonOptions only_after;
  only_after.considered_constraints = {"always_after"};
  const auto filtered = check_log_skeleton_conformance(
      std::vector<std::string>{"C"}, base, only_after);
  assert(filtered.no_constr_total == 0);  // source-absent constraints are vacuous
  assert(filtered.is_fit);

  const auto duplicate_results = check_log_skeleton_conformance(
      make_log({{"A", "B", "A"}, {"A", "B", "A"}}), base);
  assert(duplicate_results.size() == 2 && duplicate_results[0].is_fit &&
         duplicate_results[1].is_fit);
  const auto empty_model = discover_log_skeleton(EventLog{});
  assert(empty_model.equivalence.empty() && empty_model.activ_freq.empty());
  assert(check_log_skeleton_conformance(std::vector<std::string>{}, empty_model)
             .is_fit);
  assert(check_log_skeleton_conformance(make_log({{}}), empty_model)[0].is_fit);
}
