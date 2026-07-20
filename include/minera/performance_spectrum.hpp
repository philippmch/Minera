#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

enum class PerformanceSpectrumVariant { kConnected, kDisconnected };

struct PerformanceSpectrumOptions {
  PerformanceSpectrumVariant variant = PerformanceSpectrumVariant::kConnected;
  std::size_t sample_size = 10000;
  bool sort_log = true;
};

// One observed occurrence of the spectrum's activity pattern in one case: the
// instants (seconds since the Unix epoch) at which the pattern's activities
// happened, in pattern order.
struct SpectrumObservation {
  std::string case_id;
  std::vector<double> instants;
  // Labels corresponding one-to-one with instants. Connected observations
  // contain the requested pattern; disconnected observations may contain a
  // contiguous sub-pattern.
  std::vector<std::string> activities;
  std::size_t pattern_offset = 0;
};

// The performance spectrum of a log over an activity pattern: every time a
// case performs the listed activities as an uninterrupted run — ignoring
// events outside the pattern — one observation records when each step
// happened. Plotted as lines between the steps' time axes, the observations
// show how flow times between the steps evolve and where cases overtake each
// other, which an averaged waiting time hides.
struct PerformanceSpectrum {
  std::vector<std::string> activities;  // the pattern, at least two steps
  std::vector<SpectrumObservation> observations;  // in log order
  PerformanceSpectrumVariant variant = PerformanceSpectrumVariant::kConnected;
};

// Computes the spectrum after projecting each trace onto the pattern's
// activities. Every selected event must have a parseable timestamp. Connected
// mode emits full exact-pattern windows; disconnected mode emits labeled
// contiguous subpatterns. Windows may overlap. Invalid options, a pattern
// shorter than two activities, or an invalid selected timestamp throw
// std::invalid_argument.
[[nodiscard]] PerformanceSpectrum compute_performance_spectrum(
    const EventLog& log,
    std::vector<std::string> activities,
    PerformanceSpectrumOptions options = {});

}  // namespace minera
