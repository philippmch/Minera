#include "minera/log_skeleton.hpp"

#include <algorithm>
#include <set>

namespace minera {
namespace {

using Activities = std::vector<std::string>;
using CountMap = std::map<std::string, std::size_t>;
using VariantMap = std::map<Activities, std::size_t>;

bool considered(const LogSkeletonOptions& options, const std::string& name) {
  return options.considered_constraints.empty() ||
         std::find(options.considered_constraints.begin(),
                   options.considered_constraints.end(), name) !=
             options.considered_constraints.end();
}

long double threshold(std::size_t denominator, double noise) {
  return static_cast<long double>(denominator) *
         (1.0L - static_cast<long double>(noise));
}

Activities activities_of(const Trace& trace) {
  Activities result;
  result.reserve(trace.events.size());
  for (const auto& event : trace.events) result.push_back(event.activity);
  return result;
}

CountMap frequencies(const Activities& trace) {
  CountMap result;
  for (const auto& activity : trace) ++result[activity];
  return result;
}

std::set<LogSkeletonPair> relation_set(const Activities& trace,
                                       const std::string& family) {
  std::set<LogSkeletonPair> result;
  const auto counts = frequencies(trace);
  if (family == "equivalence") {
    for (const auto& [a, ca] : counts)
      for (const auto& [b, cb] : counts)
        if (a != b && ca == cb) result.insert({a, b});
  } else if (family == "always_after" || family == "always_before") {
    for (std::size_t i = 0; i < trace.size(); ++i)
      for (std::size_t j = 0; j < trace.size(); ++j) {
        if (i == j) continue;
        const bool after = j > i;
        if ((family == "always_after" && after) ||
            (family == "always_before" && !after))
          result.insert({trace[i], trace[j]});
      }
  } else if (family == "never_together") {
    for (const auto& a : counts)
      for (const auto& b : counts)
        if (a.first != b.first) result.insert({a.first, b.first});
  } else if (family == "directly_follows") {
    for (std::size_t i = 1; i < trace.size(); ++i)
      result.insert({trace[i - 1], trace[i]});
  }
  return result;
}

std::vector<LogSkeletonPair> sorted_pairs(const std::set<LogSkeletonPair>& pairs) {
  return {pairs.begin(), pairs.end()};
}

}  // namespace

LogSkeletonModel discover_log_skeleton(const EventLog& log,
                                       const LogSkeletonOptions& options) {
  LogSkeletonModel model;
  // The native API preserves PM4Py's raw arithmetic for unusual callers.
  // User-facing request/CLI boundaries validate the conventional [0,1] range.
  const double noise = options.noise_threshold;
  VariantMap variants;
  std::vector<Activities> variant_order;
  CountMap all_activs;
  std::set<std::string> seen_activities;
  std::vector<std::string> activity_order;
  std::size_t events_count = 0;
  for (const auto& trace : log.traces) {
    auto acts = activities_of(trace);
    auto variant_it = variants.find(acts);
    if (variant_it == variants.end()) {
      variants.emplace(acts, 1);
      variant_order.push_back(acts);
    } else {
      ++variant_it->second;
    }
    events_count += acts.size();
    for (const auto& activity : acts) {
      ++all_activs[activity];
      if (seen_activities.insert(activity).second) activity_order.push_back(activity);
    }
  }

  if (considered(options, "equivalence")) {
    std::map<LogSkeletonPair, std::size_t> support;
    for (const auto& [trace, weight] : variants)
      for (const auto& pair : relation_set(trace, "equivalence"))
        support[pair] += weight * frequencies(trace).at(pair.source);
    std::set<LogSkeletonPair> kept;
    for (const auto& [pair, count] : support)
      if (count + 1e-12 >= threshold(all_activs[pair.source], noise))
        kept.insert(pair);
    model.equivalence = sorted_pairs(kept);
  }

  for (const std::string family : {"always_after", "always_before"}) {
    if (!considered(options, family)) continue;
    std::map<LogSkeletonPair, std::size_t> support;
    for (const auto& [trace, weight] : variants) {
      for (const auto& pair : relation_set(trace, family)) support[pair] += weight;
    }
    std::set<LogSkeletonPair> kept;
    for (const auto& [pair, count] : support)
      if (count + 1e-12 >= threshold(all_activs[pair.source], noise))
        kept.insert(pair);
    if (family == "always_after") model.always_after = sorted_pairs(kept);
    else model.always_before = sorted_pairs(kept);
  }

  if (considered(options, "never_together")) {
    std::map<LogSkeletonPair, long long> support;
    for (const auto& [a, _] : all_activs)
      for (const auto& [b, __] : all_activs)
        if (a != b) support[{a, b}] = static_cast<long long>(all_activs[a]);
    for (const auto& [trace, weight] : variants) {
      for (const auto& pair : relation_set(trace, "never_together")) {
        auto it = support.find(pair);
        if (it == support.end()) continue;
        it->second -= static_cast<long long>(weight);
        // Python Counter's in-place subtraction discards zero and negative
        // counts after every variant, so a relation reaching zero cannot be
        // resurrected by a later subtraction or a zero threshold.
        if (it->second <= 0) support.erase(it);
      }
    }
    std::set<LogSkeletonPair> kept;
    for (const auto& [pair, count] : support)
      if (count >= threshold(all_activs[pair.source], noise)) kept.insert(pair);
    model.never_together = sorted_pairs(kept);
  }

  if (considered(options, "directly_follows")) {
    std::map<LogSkeletonPair, std::size_t> support;
    for (const auto& [trace, weight] : variants) {
      for (std::size_t i = 1; i < trace.size(); ++i)
        support[{trace[i - 1], trace[i]}] += weight;
    }
    std::set<LogSkeletonPair> kept;
    for (const auto& [pair, count] : support)
      if (count + 1e-12 >= threshold(all_activs[pair.source], noise)) kept.insert(pair);
    model.directly_follows = sorted_pairs(kept);
  }

  if (considered(options, "activ_freq")) {
    std::map<std::string, std::vector<std::pair<std::size_t, std::size_t>>> support;
    for (const auto& trace : variant_order) {
      const auto weight = variants.at(trace);
      const auto counts = frequencies(trace);
      for (const auto& activity : activity_order) {
        const auto frequency = counts.count(activity) ? counts.at(activity) : 0;
        auto& histogram = support[activity];
        auto hit = std::find_if(histogram.begin(), histogram.end(),
                                [&](const auto& entry) { return entry.first == frequency; });
        if (hit == histogram.end()) histogram.push_back({frequency, weight});
        else hit->second += weight;
      }
    }
    for (auto& [activity, histogram] : support) {
      auto ranked = histogram;
      std::stable_sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
        if (left.second != right.second) return left.second > right.second;
        return false;  // stable tie: first-seen frequency bucket wins
      });
      std::size_t cumulative = 0;
      std::vector<std::size_t> allowed;
      // PM4Py truncates its ranked working list at the first bucket whose
      // cumulative variant weight reaches the event-count threshold.
      for (const auto& [frequency, count] : ranked) {
        allowed.push_back(frequency);
        cumulative += count;
        if (static_cast<double>(cumulative) + 1e-12 >= threshold(events_count, noise)) break;
      }
      std::sort(allowed.begin(), allowed.end());
      model.activ_freq.emplace(activity, std::move(allowed));
    }
  }
  return model;
}

LogSkeletonConformance check_log_skeleton_conformance(
    const std::vector<std::string>& trace, const LogSkeletonModel& model,
    const LogSkeletonOptions& options) {
  LogSkeletonConformance result;
  const auto counts = frequencies(trace);
  auto present = std::set<std::string>(trace.begin(), trace.end());
  auto check_pairs = [&](const std::string& family,
                         const std::vector<LogSkeletonPair>& expected) {
    if (!considered(options, family)) return;
    std::set<LogSkeletonPair> actual = relation_set(trace, family);
    std::set<LogSkeletonPair> filtered;
    for (const auto& pair : expected)
      if (present.count(pair.source)) filtered.insert(pair);
    result.no_constr_total += filtered.size();
    for (const auto& pair : filtered)
      if (family == "never_together" ? actual.count(pair) != 0 : actual.count(pair) == 0) {
        ++result.no_dev_total;
        LogSkeletonDeviation deviation;
        deviation.constraint = family;
        deviation.relation = pair;
        result.deviations.push_back(std::move(deviation));
      }
  };
  check_pairs("equivalence", model.equivalence);
  check_pairs("always_after", model.always_after);
  check_pairs("always_before", model.always_before);
  check_pairs("never_together", model.never_together);
  check_pairs("directly_follows", model.directly_follows);

  if (considered(options, "activ_freq")) {
    std::size_t constraints = 0;
    // trace_skel.get_trace_info returns an activity-frequency Counter; its
    // iteration is over distinct activities, not every event occurrence.
    for (const auto& [activity, observed] : counts) {
      ++constraints;
      auto it = model.activ_freq.find(activity);
      if (it == model.activ_freq.end()) {
        ++result.no_dev_total;
        LogSkeletonDeviation deviation;
        deviation.constraint = "activ_freq";
        deviation.activity = activity;
        // PM4Py reports an unknown activity as the absent (zero) model
        // frequency, rather than echoing its observed count.
        deviation.observed_frequency = 0;
        result.deviations.push_back(std::move(deviation));
      } else if (std::find(it->second.begin(), it->second.end(), observed) == it->second.end()) {
        ++result.no_dev_total;
        LogSkeletonDeviation deviation;
        deviation.constraint = "activ_freq";
        deviation.activity = activity;
        deviation.observed_frequency = observed;
        deviation.allowed_frequencies = it->second;
        result.deviations.push_back(std::move(deviation));
      }
    }
    for (const auto& [activity, allowed] : model.activ_freq) {
      if (!allowed.empty() && allowed.front() > 0 && !present.count(activity)) {
        ++constraints;
        ++result.no_dev_total;
        LogSkeletonDeviation deviation;
        deviation.constraint = "activ_freq";
        deviation.activity = activity;
        deviation.allowed_frequencies = allowed;
        result.deviations.push_back(std::move(deviation));
      }
    }
    result.no_constr_total += constraints;
  }
  std::sort(result.deviations.begin(), result.deviations.end(), [](const auto& a, const auto& b) {
    if (a.constraint != b.constraint) return a.constraint < b.constraint;
    if (a.relation.source != b.relation.source) return a.relation.source < b.relation.source;
    if (a.relation.target != b.relation.target) return a.relation.target < b.relation.target;
    return a.activity < b.activity;
  });
  result.dev_fitness = result.no_constr_total == 0
                           ? 1.0
                           : 1.0 - static_cast<double>(result.no_dev_total) /
                                     static_cast<double>(result.no_constr_total);
  result.is_fit = result.deviations.empty();
  return result;
}

LogSkeletonConformance check_log_skeleton_conformance(
    const Trace& trace, const LogSkeletonModel& model,
    const LogSkeletonOptions& options) {
  return check_log_skeleton_conformance(activities_of(trace), model, options);
}

std::vector<LogSkeletonConformance> check_log_skeleton_conformance(
    const EventLog& log, const LogSkeletonModel& model,
    const LogSkeletonOptions& options) {
  std::vector<LogSkeletonConformance> result;
  result.reserve(log.traces.size());
  for (const auto& trace : log.traces)
    result.push_back(check_log_skeleton_conformance(trace, model, options));
  return result;
}

}  // namespace minera
