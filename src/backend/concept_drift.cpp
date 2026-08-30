#include "minera/concept_drift.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>
#include <string_view>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

constexpr std::size_t kMaxPermutations = 1'000'000;
constexpr long double kMaxPermutationWork = 200'000'000.0L;
constexpr long double kMaxMaterializedTraceCopies = 2'000'000.0L;
constexpr long double kMaxCumulativeEventCopies = 20'000'000.0L;

void validate_options(const ConceptDriftOptions& options, std::size_t trace_count,
                     std::size_t event_count, std::size_t feature_size) {
  if (options.variant != ConceptDriftVariant::kBose) {
    throw std::invalid_argument("concept drift: unsupported variant");
  }
  if (options.activity_attribute.empty() ||
      options.timestamp_attribute.empty() || options.case_id_attribute.empty()) {
    throw std::invalid_argument(
        "concept drift: activity, timestamp, and case-id attributes must not be empty");
  }
  if (options.sub_log_size == 0) {
    throw std::invalid_argument("concept drift: sub_log_size must be positive");
  }
  if (options.window_size == 0) {
    throw std::invalid_argument("concept drift: window_size must be positive");
  }
  if (options.window_size > std::numeric_limits<std::size_t>::max() / 2) {
    throw std::invalid_argument("concept drift: window_size is too large");
  }
  if (options.num_permutations == 0 ||
      options.num_permutations > kMaxPermutations) {
    throw std::invalid_argument(
        "concept drift: num_permutations must be in [1, 1000000]");
  }
  // A very large cap is usually a wrapped negative value at an API boundary;
  // rejecting it also keeps result allocation bounded.
  if (options.max_no_change_points > kMaxPermutations) {
    throw std::invalid_argument(
        "concept drift: max_no_change_points exceeds safe bound");
  }
  if (!std::isfinite(options.threshold) || options.threshold < 0.0 ||
      options.threshold > 1.0) {
    throw std::invalid_argument(
        "concept drift: threshold must be finite and in [0,1]");
  }

  const std::size_t sublog_count =
      trace_count == 0 ? 0 : (trace_count - 1) / options.sub_log_size + 1;
  if (sublog_count > 0) {
    const std::size_t complete_sublogs = trace_count / options.sub_log_size;
    const std::size_t remainder = trace_count % options.sub_log_size;
    const long double complete = static_cast<long double>(complete_sublogs);
    long double cumulative_trace_copies =
        static_cast<long double>(options.sub_log_size) * complete *
        (complete + 1.0L) / 2.0L;
    if (remainder != 0) cumulative_trace_copies += static_cast<long double>(trace_count);
    const long double materialized_trace_copies =
        cumulative_trace_copies + 2.0L * static_cast<long double>(trace_count);
    const long double cumulative_event_copies =
        static_cast<long double>(event_count) *
        static_cast<long double>(sublog_count);
    if (materialized_trace_copies > kMaxMaterializedTraceCopies ||
        cumulative_event_copies > kMaxCumulativeEventCopies) {
      throw std::invalid_argument(
          "concept drift: cumulative sub-log materialization exceeds safe bound");
    }
  }
  if (feature_size != 0 && sublog_count > 2 * options.window_size - 1) {
    const std::size_t windows = sublog_count - 2 * options.window_size + 1;
    const long double work = static_cast<long double>(windows) *
                             static_cast<long double>(options.num_permutations) *
                             static_cast<long double>(std::max<std::size_t>(1, feature_size)) *
                             static_cast<long double>(2 * options.window_size);
    if (work > kMaxPermutationWork) {
      throw std::invalid_argument(
          "concept drift: requested permutation work exceeds safe bound");
    }
  }
}

const std::string* event_value(const Event& event, std::string_view key) {
  if (key.empty() || key == "activity" || key == "concept:name")
    return &event.activity;
  if (key == "timestamp" || key == "time:timestamp")
    return &event.timestamp;
  if (key == "resource" || key == "org:resource") return &event.resource;
  if (key == "lifecycle" || key == "lifecycle:transition")
    return &event.lifecycle;
  return event.attribute(key);
}

const std::string* trace_value(const Trace& trace, std::string_view key) {
  if (key.empty() || key == "case_id" || key == "case:id" ||
      key == "case:concept:name" || key == "concept:name") {
    return &trace.case_id;
  }
  if (const auto* value = trace.attribute(key)) return value;
  if (!trace.events.empty()) return event_value(trace.events.front(), key);
  return nullptr;
}

std::vector<std::string> sorted_activities_from_traces(
    const std::vector<Trace>& traces, std::string_view activity_attribute) {
  std::vector<std::string> activities;
  for (const auto& trace : traces) {
    for (const auto& event : trace.events) {
      const auto* value = event_value(event, activity_attribute);
      if (value && !value->empty()) activities.push_back(*value);
    }
  }
  std::sort(activities.begin(), activities.end());
  activities.erase(std::unique(activities.begin(), activities.end()),
                   activities.end());
  return activities;
}

double permutation_p_value(const std::vector<std::vector<double>>& first,
                           const std::vector<std::vector<double>>& second,
                           std::mt19937_64& rng) {
  const std::size_t n1 = first.size();
  const std::size_t n2 = second.size();
  const std::size_t dimensions = first.front().size();
  std::vector<std::vector<double>> all;
  all.reserve(n1 + n2);
  all.insert(all.end(), first.begin(), first.end());
  all.insert(all.end(), second.begin(), second.end());
  std::vector<std::size_t> indices(all.size());
  std::iota(indices.begin(), indices.end(), 0);

  std::vector<double> mean1(dimensions, 0.0), mean2(dimensions, 0.0);
  for (const auto& row : first)
    for (std::size_t d = 0; d < dimensions; ++d) mean1[d] += row[d];
  for (const auto& row : second)
    for (std::size_t d = 0; d < dimensions; ++d) mean2[d] += row[d];
  for (double& value : mean1) value /= static_cast<double>(n1);
  for (double& value : mean2) value /= static_cast<double>(n2);
  double observed_squared = 0.0;
  for (std::size_t d = 0; d < dimensions; ++d) {
    const double delta = mean1[d] - mean2[d];
    observed_squared += delta * delta;
  }

  // The caller requests the number of permutations by repeating this helper;
  // this function performs one shuffle and returns an indicator in [0, 1].
  std::shuffle(indices.begin(), indices.end(), rng);
  std::vector<double> perm1(dimensions, 0.0), perm2(dimensions, 0.0);
  for (std::size_t i = 0; i < n1; ++i)
    for (std::size_t d = 0; d < dimensions; ++d)
      perm1[d] += all[indices[i]][d];
  for (std::size_t i = n1; i < n1 + n2; ++i)
    for (std::size_t d = 0; d < dimensions; ++d)
      perm2[d] += all[indices[i]][d];
  double perm_squared = 0.0;
  for (std::size_t d = 0; d < dimensions; ++d) {
    const double delta = perm1[d] / static_cast<double>(n1) -
                         perm2[d] / static_cast<double>(n2);
    perm_squared += delta * delta;
  }
  return perm_squared >= observed_squared ? 1.0 : 0.0;
}

}  // namespace

std::vector<std::string> extract_concept_drift_activities(
    const EventLog& log) {
  return extract_concept_drift_activities(log, "concept:name");
}

std::vector<std::string> extract_concept_drift_activities(
    const EventLog& log, std::string_view activity_attribute) {
  if (activity_attribute.empty()) {
    throw std::invalid_argument(
        "concept drift: activity_attribute must not be empty");
  }
  return sorted_activities_from_traces(log.traces, activity_attribute);
}

std::vector<double> extract_concept_drift_features(
    const std::vector<Trace>& traces,
    const std::vector<std::string>& sorted_activities) {
  return extract_concept_drift_features(traces, sorted_activities,
                                         "concept:name");
}

std::vector<double> extract_concept_drift_features(
    const std::vector<Trace>& traces,
    const std::vector<std::string>& sorted_activities,
    std::string_view activity_attribute) {
  if (activity_attribute.empty()) {
    throw std::invalid_argument(
        "concept drift: activity_attribute must not be empty");
  }
  // Normalize the supplied alphabet as well: this keeps the standalone
  // extractor deterministic when callers provide activities in input order.
  std::vector<std::string> activities = sorted_activities;
  std::sort(activities.begin(), activities.end());
  activities.erase(std::unique(activities.begin(), activities.end()),
                   activities.end());
  const std::size_t n = activities.size();
  if (n > std::numeric_limits<std::size_t>::max() / 3) {
    throw std::invalid_argument("concept drift: activity feature vector is too large");
  }
  std::unordered_map<std::string, std::size_t> ids;
  ids.reserve(n);
  for (std::size_t i = 0; i < n; ++i) ids.emplace(activities[i], i);
  std::vector<std::size_t> trace_counts(n, 0);
  std::vector<std::size_t> follows(n * n, 0);

  for (const auto& trace : traces) {
    std::unordered_set<std::size_t> present;
    present.reserve(trace.events.size());
    std::unordered_set<std::size_t> relation;
    relation.reserve(trace.events.size() * 2);
    std::vector<std::size_t> seen;
    seen.reserve(trace.events.size());
    for (const auto& event : trace.events) {
      const auto* activity = event_value(event, activity_attribute);
      if (!activity) continue;
      const auto target_it = ids.find(*activity);
      if (target_it == ids.end()) continue;
      const std::size_t target = target_it->second;
      present.insert(target);
      for (const std::size_t source : seen) relation.insert(source * n + target);
      seen.push_back(target);
    }
    for (const auto source : present) ++trace_counts[source];
    for (const auto key : relation) ++follows[key];
  }

  std::vector<double> features;
  features.reserve(n * 3);
  for (std::size_t target = 0; target < n; ++target) {
    std::size_t always = 0, sometimes = 0, never = 0;
    for (std::size_t source = 0; source < n; ++source) {
      if (trace_counts[source] == 0 || follows[source * n + target] == 0) {
        ++never;
      } else if (follows[source * n + target] == trace_counts[source]) {
        ++always;
      } else {
        ++sometimes;
      }
    }
    features.push_back(static_cast<double>(always));
    features.push_back(static_cast<double>(sometimes));
    features.push_back(static_cast<double>(never));
  }
  return features;
}

std::vector<double> extract_concept_drift_features(const EventLog& log) {
  return extract_concept_drift_features(log.traces,
                                        extract_concept_drift_activities(log),
                                        "concept:name");
}

ConceptDriftResult detect_concept_drift(const EventLog& log,
                                        ConceptDriftOptions options) {
  const auto activities = extract_concept_drift_activities(
      log, options.activity_attribute);
  if (activities.size() > std::numeric_limits<std::size_t>::max() / 3) {
    throw std::invalid_argument("concept drift: activity feature vector is too large");
  }
  std::size_t event_count = 0;
  for (const auto& trace : log.traces) {
    if (trace.events.size() > std::numeric_limits<std::size_t>::max() - event_count)
      throw std::invalid_argument("concept drift: event count is too large");
    event_count += trace.events.size();
  }
  validate_options(options, log.traces.size(), event_count,
                   activities.size() * 3);
  for (const auto& trace : log.traces) {
    if (!trace_value(trace, options.case_id_attribute)) {
      throw std::invalid_argument(
          "concept drift: selected case-id attribute is missing");
    }
  }

  ConceptDriftResult result;
  result.variant = options.variant;
  result.activities = activities;
  result.activity_attribute = options.activity_attribute;
  result.timestamp_attribute = options.timestamp_attribute;
  result.case_id_attribute = options.case_id_attribute;
  if (!log.traces.empty()) {
    const std::size_t sublog_count =
        (log.traces.size() - 1) / options.sub_log_size + 1;
    result.sublogs.reserve(sublog_count);
    result.feature_vectors.reserve(sublog_count);
    result.cumulative_sublogs.reserve(sublog_count);
    result.sublog_start_timestamps.reserve(sublog_count);
  }

  for (std::size_t start = 0; start < log.traces.size();
       start += options.sub_log_size) {
    const std::size_t end = std::min(
        log.traces.size(),
        start > std::numeric_limits<std::size_t>::max() - options.sub_log_size
            ? std::numeric_limits<std::size_t>::max()
            : start + options.sub_log_size);
    EventLog raw;
    raw.attributes = log.attributes;
    raw.traces.insert(raw.traces.end(), log.traces.begin() + start,
                      log.traces.begin() + end);
    result.sublogs.push_back(raw);
    result.feature_vectors.push_back(
        extract_concept_drift_features(raw.traces, activities,
                                       options.activity_attribute));
    // Keep this field cumulative from the beginning, as requested by the API.
    EventLog cumulative_copy;
    cumulative_copy.attributes = log.attributes;
    cumulative_copy.traces.insert(cumulative_copy.traces.end(), log.traces.begin(),
                                   log.traces.begin() + end);
    result.cumulative_sublogs.push_back(std::move(cumulative_copy));
    if (options.include_timestamps) {
      if (log.traces[start].events.empty()) {
        throw std::invalid_argument(
            "concept drift: requested timestamp for an empty case");
      }
      const auto* timestamp = event_value(
          log.traces[start].events.front(), options.timestamp_attribute);
      if (!timestamp || timestamp->empty()) {
        throw std::invalid_argument(
            "concept drift: selected case-start timestamp is missing");
      }
      const auto parsed = parse_timestamp(*timestamp);
      if (!parsed || !std::isfinite(*parsed)) {
        throw std::invalid_argument(
            "concept drift: case-start timestamp is not finite");
      }
      result.sublog_start_timestamps.push_back(*parsed);
    }
  }

  if (result.feature_vectors.size() >= 2 * options.window_size) {
    std::mt19937_64 rng(options.seed);
    const std::size_t windows =
        result.feature_vectors.size() - 2 * options.window_size + 1;
    std::vector<ConceptDriftChangePoint> candidates;
    candidates.reserve(windows);
    for (std::size_t i = 0; i < windows; ++i) {
      const auto first_begin = result.feature_vectors.begin() + i;
      const auto first_end = first_begin + options.window_size;
      const auto second_end = first_end + options.window_size;
      std::vector<std::vector<double>> first(first_begin, first_end);
      std::vector<std::vector<double>> second(first_end, second_end);
      double exceedances = 0.0;
      for (std::size_t permutation = 0; permutation < options.num_permutations;
           ++permutation) {
        exceedances += permutation_p_value(first, second, rng);
      }
      const double p_value = exceedances / options.num_permutations;
      if (p_value < options.threshold) {
        candidates.push_back({i + options.window_size, p_value});
      }
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& lhs, const auto& rhs) {
                if (lhs.p_value != rhs.p_value) return lhs.p_value < rhs.p_value;
                return lhs.sublog_index < rhs.sublog_index;
              });
    if (candidates.size() > options.max_no_change_points)
      candidates.resize(options.max_no_change_points);
    result.change_points = candidates;
    if (options.include_timestamps) {
      result.change_timestamps.reserve(candidates.size());
      for (const auto& point : candidates) {
        result.change_timestamps.push_back(
            result.sublog_start_timestamps.at(point.sublog_index));
      }
    }
  }

  if (!result.sublogs.empty()) {
    std::vector<bool> drift_boundaries(result.sublogs.size() + 1, false);
    for (const auto& point : result.change_points) {
      if (point.sublog_index > 0 &&
          point.sublog_index < drift_boundaries.size()) {
        drift_boundaries[point.sublog_index] = true;
      }
    }

    EventLog segment;
    segment.attributes = log.attributes;
    result.returned_sublogs.reserve(result.change_points.size() + 1);
    for (std::size_t i = 0; i < result.sublogs.size(); ++i) {
      const auto& raw = result.sublogs[i];
      segment.traces.insert(segment.traces.end(), raw.traces.begin(),
                            raw.traces.end());
      if (drift_boundaries[i + 1] || i + 1 == result.sublogs.size()) {
        result.returned_sublogs.push_back(std::move(segment));
        segment = EventLog{};
        segment.attributes = log.attributes;
      }
    }
  }
  return result;
}

}  // namespace minera
