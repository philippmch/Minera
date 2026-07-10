#pragma once

#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

// One observed occurrence of the spectrum's activity pattern in one case: the
// instants (seconds since the Unix epoch) at which the pattern's activities
// happened, in pattern order.
struct SpectrumObservation {
  std::string case_id;
  std::vector<double> instants;
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
};

// Computes the spectrum: each trace is projected onto the pattern's
// activities (keeping only events whose timestamp parses), and every window
// of the projection that equals the pattern yields one observation. Windows
// may overlap, so a looping case contributes one observation per iteration.
// A pattern with fewer than two activities throws std::invalid_argument.
[[nodiscard]] PerformanceSpectrum compute_performance_spectrum(
    const EventLog& log,
    std::vector<std::string> activities);

}  // namespace minera
