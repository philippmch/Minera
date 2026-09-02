#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "minera/event_data.hpp"
#include "minera/event_log.hpp"

namespace minera {

enum class PerformanceSpectrumVariant {
  // Connected/disconnected are the historical native names and remain the
  // default aliases for the log-based implementations.
  kConnected,
  kDisconnected,
  kDataframe,
  kLog,
  kDataframeDisconnected,
  kLogDisconnected,
  kLazyframe,
  kLazyframeDisconnected,

  CONNECTED = kConnected,
  DISCONNECTED = kDisconnected,
  DATAFRAME = kDataframe,
  LOG = kLog,
  DATAFRAME_DISCONNECTED = kDataframeDisconnected,
  LOG_DISCONNECTED = kLogDisconnected,
  LAZYFRAME = kLazyframe,
  LAZYFRAME_DISCONNECTED = kLazyframeDisconnected,
};

struct PerformanceSpectrumOptions {
  PerformanceSpectrumVariant variant = PerformanceSpectrumVariant::kConnected;
  std::size_t sample_size = 10000;
  bool sort_log = true;
  // Event/trace attributes used by the selected implementation. Built-in
  // aliases are activity, timestamp, and case_id; generic attributes are
  // resolved from the event or trace attribute maps.
  std::string activity_attribute = "activity";
  std::string timestamp_attribute = "timestamp";
  std::string case_id_attribute = "case_id";
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
  std::string activity_attribute = "activity";
  std::string timestamp_attribute = "timestamp";
  std::string case_id_attribute = "case_id";
};

// Computes the spectrum after projecting each trace onto the pattern's
// activities. Every selected event must have a parseable timestamp. Connected
// mode emits full exact-pattern windows; disconnected mode emits labeled
// contiguous subpatterns. The explicit adapter names select distinct
// projections while preserving the historical connected/disconnected names.
// Windows may overlap. Invalid options, a pattern shorter than two activities,
// or an invalid selected timestamp throw std::invalid_argument.
[[nodiscard]] PerformanceSpectrum compute_performance_spectrum(
    const EventLog& log,
    std::vector<std::string> activities,
    PerformanceSpectrumOptions options = {});

// Input-specific components.  The explicit names make the six selectable
// implementations independently callable, while the shared result type
// keeps callers independent of the source adapter's representation.
[[nodiscard]] PerformanceSpectrum discover_performance_spectrum_log(
    const EventLog& log, std::vector<std::string> activities,
    PerformanceSpectrumOptions options = {});
[[nodiscard]] PerformanceSpectrum discover_performance_spectrum_dataframe(
    const EventTable& table, std::vector<std::string> activities,
    PerformanceSpectrumOptions options = {});
[[nodiscard]] PerformanceSpectrum discover_performance_spectrum_dataframe_disconnected(
    const EventTable& table, std::vector<std::string> activities,
    PerformanceSpectrumOptions options = {});
[[nodiscard]] PerformanceSpectrum discover_performance_spectrum_log_disconnected(
    const EventLog& log, std::vector<std::string> activities,
    PerformanceSpectrumOptions options = {});
[[nodiscard]] PerformanceSpectrum discover_performance_spectrum_lazyframe(
    const LazyEventTable& table, std::vector<std::string> activities,
    PerformanceSpectrumOptions options = {});
[[nodiscard]] PerformanceSpectrum discover_performance_spectrum_lazyframe_disconnected(
    const LazyEventTable& table, std::vector<std::string> activities,
    PerformanceSpectrumOptions options = {});

// The generic overloads select the adapter-specific canonical implementation
// from the input type. Explicit options variants still select the requested
// component, including the historical compatibility spellings.
[[nodiscard]] PerformanceSpectrum compute_performance_spectrum(
    const EventTable& table, std::vector<std::string> activities,
    PerformanceSpectrumOptions options = {});
[[nodiscard]] PerformanceSpectrum compute_performance_spectrum(
    const EventStream& stream, std::vector<std::string> activities,
    PerformanceSpectrumOptions options = {});
[[nodiscard]] PerformanceSpectrum compute_performance_spectrum(
    const LazyEventTable& table, std::vector<std::string> activities,
    PerformanceSpectrumOptions options = {});
[[nodiscard]] PerformanceSpectrum compute_performance_spectrum(
    const EventDataView& data, std::vector<std::string> activities,
    PerformanceSpectrumOptions options = {});

[[nodiscard]] inline PerformanceSpectrum discover_performance_spectrum(
    const EventLog& log, std::vector<std::string> activities,
    PerformanceSpectrumOptions options = {}) {
  return compute_performance_spectrum(log, std::move(activities),
                                      std::move(options));
}

[[nodiscard]] inline PerformanceSpectrum discover_performance_spectrum(
    const EventTable& table, std::vector<std::string> activities,
    PerformanceSpectrumOptions options = {}) {
  return compute_performance_spectrum(table, std::move(activities),
                                      std::move(options));
}

[[nodiscard]] inline PerformanceSpectrum discover_performance_spectrum(
    const EventStream& stream, std::vector<std::string> activities,
    PerformanceSpectrumOptions options = {}) {
  return compute_performance_spectrum(stream, std::move(activities),
                                      std::move(options));
}

[[nodiscard]] inline PerformanceSpectrum discover_performance_spectrum(
    const LazyEventTable& table, std::vector<std::string> activities,
    PerformanceSpectrumOptions options = {}) {
  return compute_performance_spectrum(table, std::move(activities),
                                      std::move(options));
}

[[nodiscard]] inline PerformanceSpectrum discover_performance_spectrum(
    const EventDataView& data, std::vector<std::string> activities,
    PerformanceSpectrumOptions options = {}) {
  return compute_performance_spectrum(data, std::move(activities),
                                      std::move(options));
}

}  // namespace minera
