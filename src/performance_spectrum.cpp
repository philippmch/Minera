#include "minera/performance_spectrum.hpp"

#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "minera/timestamp.hpp"

namespace minera {

PerformanceSpectrum compute_performance_spectrum(
    const EventLog& log,
    std::vector<std::string> activities) {
  if (activities.size() < 2) {
    throw std::invalid_argument(
        "performance spectrum: the pattern needs at least two activities");
  }

  const std::unordered_set<std::string> pattern_set(activities.begin(),
                                                    activities.end());

  PerformanceSpectrum spectrum;
  spectrum.activities = std::move(activities);
  const auto& pattern = spectrum.activities;

  // Projection of one trace onto the pattern's activities: only their timed
  // events, in trace order.
  std::vector<std::pair<const std::string*, double>> projected;
  for (const auto& trace : log.traces) {
    projected.clear();
    for (const auto& event : trace.events) {
      if (pattern_set.find(event.activity) == pattern_set.end()) continue;
      const auto instant = parse_timestamp(event.timestamp);
      if (!instant) continue;
      projected.emplace_back(&event.activity, *instant);
    }
    if (projected.size() < pattern.size()) continue;

    // Every window of the projection that spells the pattern is one
    // observation; windows overlap, so loop iterations each count.
    for (std::size_t start = 0; start + pattern.size() <= projected.size();
         ++start) {
      bool matches = true;
      for (std::size_t i = 0; i < pattern.size(); ++i) {
        if (*projected[start + i].first != pattern[i]) {
          matches = false;
          break;
        }
      }
      if (!matches) continue;
      SpectrumObservation observation;
      observation.case_id = trace.case_id;
      observation.instants.reserve(pattern.size());
      for (std::size_t i = 0; i < pattern.size(); ++i) {
        observation.instants.push_back(projected[start + i].second);
      }
      spectrum.observations.push_back(std::move(observation));
    }
  }

  return spectrum;
}

}  // namespace minera
