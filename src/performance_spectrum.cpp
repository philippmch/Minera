#include "minera/performance_spectrum.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>
#include <unordered_set>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

struct TimedEvent {
  std::string activity;
  double instant = 0.0;
  std::size_t input_position = 0;
  std::size_t input_order = 0;
};

std::vector<SpectrumObservation> sample_points(
    std::vector<SpectrumObservation> points, std::size_t sample_size) {
  if (points.size() <= sample_size) return points;
  // PM4Py's points_subset partitions the points into sample_size buckets and
  // chooses each bucket's midpoint. The first and last points are retained as
  // extrema, hence the result can contain sample_size + 2 rows.
  std::vector<std::size_t> indices;
  indices.reserve(sample_size + 2);
  const std::size_t n = points.size();
  const std::size_t half_bucket = n / (2 * sample_size);
  for (std::size_t bucket = 0; bucket < sample_size; ++bucket) {
    const std::size_t midpoint = (bucket * n) / sample_size + half_bucket;
    if (midpoint < n) indices.push_back(midpoint);
  }
  indices.insert(indices.begin(), 0);
  indices.push_back(n - 1);
  std::vector<SpectrumObservation> result;
  result.reserve(indices.size());
  std::size_t last_index = n;
  for (const auto index : indices) {
    if (index != last_index) {
      result.push_back(points[index]);
      last_index = index;
    }
  }
  return result;
}

}  // namespace

PerformanceSpectrum compute_performance_spectrum(
    const EventLog& log, std::vector<std::string> activities,
    PerformanceSpectrumOptions options) {
  if (activities.size() < 2) {
    throw std::invalid_argument(
        "performance spectrum: the pattern needs at least two activities");
  }
  if (options.sample_size == 0) {
    throw std::invalid_argument("performance spectrum: sample_size must be positive");
  }
  if (options.variant != PerformanceSpectrumVariant::kConnected &&
      options.variant != PerformanceSpectrumVariant::kDisconnected) {
    throw std::invalid_argument("performance spectrum: unknown variant");
  }

  PerformanceSpectrum spectrum;
  spectrum.activities = std::move(activities);
  spectrum.variant = options.variant;
  const auto& pattern = spectrum.activities;
  const std::unordered_set<std::string> requested(pattern.begin(), pattern.end());

  // A sequence number gives deterministic ordering even when case IDs and
  // timestamps tie.
  for (const auto& trace : log.traces) {
    std::vector<TimedEvent> projected;
    for (std::size_t pos = 0; pos < trace.events.size(); ++pos) {
      const auto& event = trace.events[pos];
      if (requested.find(event.activity) == requested.end()) continue;
      const auto parsed = parse_timestamp(event.timestamp);
      if (!parsed) {
        throw std::invalid_argument(
            "performance spectrum: selected event has an invalid timestamp");
      }
      projected.push_back(
          TimedEvent{event.activity, *parsed, pos, event.input_order});
    }
    if (options.sort_log) {
      std::stable_sort(projected.begin(), projected.end(),
                       [](const TimedEvent& left, const TimedEvent& right) {
                         if (left.instant != right.instant)
                           return left.instant < right.instant;
                         if (left.input_order != right.input_order)
                           return left.input_order < right.input_order;
                         return left.input_position < right.input_position;
                       });
    }

    if (options.variant == PerformanceSpectrumVariant::kConnected) {
      for (std::size_t start = 0; start + pattern.size() <= projected.size();
           ++start) {
        bool matches = true;
        for (std::size_t offset = 0; offset < pattern.size(); ++offset) {
          if (projected[start + offset].activity != pattern[offset]) {
            matches = false;
            break;
          }
        }
        if (!matches) continue;
        SpectrumObservation observation;
        observation.case_id = trace.case_id;
        observation.pattern_offset = 0;
        observation.activities = pattern;
        observation.instants.reserve(pattern.size());
        for (std::size_t offset = 0; offset < pattern.size(); ++offset)
          observation.instants.push_back(projected[start + offset].instant);
        spectrum.observations.push_back(std::move(observation));
      }
    } else {
      // Disconnected mode discovers every contiguous requested sub-pattern,
      // longest first. Events consumed by a longer match are removed except
      // for its endpoint, preventing the same full match being rediscovered
      // as a subset while still permitting endpoint overlap.
      std::vector<bool> removed(projected.size(), false);
      for (std::size_t length = pattern.size(); length >= 2; --length) {
        std::vector<std::size_t> active_indices;
        active_indices.reserve(projected.size());
        for (std::size_t index = 0; index < projected.size(); ++index)
          if (!removed[index]) active_indices.push_back(index);
        struct Match {
          std::size_t offset;
          std::size_t active_start;
        };
        std::vector<Match> matches;
        // Discover all matches using the state at the start of this length.
        // For repeated requested subpatterns, retain the first offset so an
        // identical activity window is emitted only once.
        for (std::size_t start = 0; start + length <= active_indices.size(); ++start) {
          for (std::size_t offset = 0; offset + length <= pattern.size(); ++offset) {
            bool matches_pattern = true;
            for (std::size_t i = 0; i < length; ++i) {
              if (projected[active_indices[start + i]].activity !=
                  pattern[offset + i]) {
                matches_pattern = false;
                break;
              }
            }
            if (matches_pattern) {
              matches.push_back(Match{offset, start});
              break;
            }
          }
        }
        for (const auto match : matches) {
          SpectrumObservation observation;
          observation.case_id = trace.case_id;
          observation.pattern_offset = match.offset;
          observation.activities.assign(pattern.begin() + match.offset,
                                        pattern.begin() + match.offset + length);
          observation.instants.reserve(length);
          for (std::size_t i = 0; i < length; ++i)
            observation.instants.push_back(
                projected[active_indices[match.active_start + i]].instant);
          spectrum.observations.push_back(std::move(observation));
        }
        // Apply removals only after all matches for this length are known.
        for (const auto match : matches) {
          for (std::size_t i = 0; i + 1 < length; ++i)
            removed[active_indices[match.active_start + i]] = true;
        }
        if (length == 2) break;
      }
    }
  }

  std::stable_sort(spectrum.observations.begin(), spectrum.observations.end(),
                   [](const SpectrumObservation& left,
                      const SpectrumObservation& right) {
                     const double li = left.instants.empty()
                                           ? 0.0
                                           : left.instants.front();
                     const double ri = right.instants.empty()
                                           ? 0.0
                                           : right.instants.front();
                     return li < ri;
                   });
  spectrum.observations =
      sample_points(std::move(spectrum.observations), options.sample_size);
  return spectrum;
}

}  // namespace minera
