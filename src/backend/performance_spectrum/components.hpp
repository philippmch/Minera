#pragma once

#include <string>
#include <vector>

#include "minera/performance_spectrum.hpp"

namespace minera::performance_spectrum_detail {

enum class Component {
  kLog,
  kDataframe,
  kDataframeDisconnected,
  kLogDisconnected,
  kLazyframe,
  kLazyframeDisconnected,
  kCompatibilityConnected,
  kCompatibilityDisconnected,
};

PerformanceSpectrum run_component(
    const EventLog& log, std::vector<std::string> activities,
    PerformanceSpectrumOptions options, Component component);

PerformanceSpectrum run_component(
    const EventTable& table, std::vector<std::string> activities,
    PerformanceSpectrumOptions options, Component component);

PerformanceSpectrum run_component(
    const EventStream& stream, std::vector<std::string> activities,
    PerformanceSpectrumOptions options, Component component);

PerformanceSpectrum run_component(
    const LazyEventTable& table, std::vector<std::string> activities,
    PerformanceSpectrumOptions options, Component component);

}  // namespace minera::performance_spectrum_detail
