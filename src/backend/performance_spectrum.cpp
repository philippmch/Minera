#include "minera/performance_spectrum.hpp"

#include <stdexcept>
#include <utility>

#include "performance_spectrum/components.hpp"

namespace minera {
namespace {

using performance_spectrum_detail::Component;

Component component_for_variant(PerformanceSpectrumVariant variant) {
  switch (variant) {
    case PerformanceSpectrumVariant::kConnected:
      return Component::kCompatibilityConnected;
    case PerformanceSpectrumVariant::kDisconnected:
      return Component::kCompatibilityDisconnected;
    case PerformanceSpectrumVariant::kDataframe:
      return Component::kDataframe;
    case PerformanceSpectrumVariant::kLog:
      return Component::kLog;
    case PerformanceSpectrumVariant::kDataframeDisconnected:
      return Component::kDataframeDisconnected;
    case PerformanceSpectrumVariant::kLogDisconnected:
      return Component::kLogDisconnected;
    case PerformanceSpectrumVariant::kLazyframe:
      return Component::kLazyframe;
    case PerformanceSpectrumVariant::kLazyframeDisconnected:
      return Component::kLazyframeDisconnected;
  }
  throw std::invalid_argument("performance spectrum: unknown variant");
}

Component component_for_table_variant(PerformanceSpectrumVariant variant) {
  switch (variant) {
    case PerformanceSpectrumVariant::kConnected:
      return Component::kDataframe;
    case PerformanceSpectrumVariant::kDisconnected:
      return Component::kDataframeDisconnected;
    case PerformanceSpectrumVariant::kDataframe:
      return Component::kDataframe;
    case PerformanceSpectrumVariant::kLog:
      return Component::kLog;
    case PerformanceSpectrumVariant::kDataframeDisconnected:
      return Component::kDataframeDisconnected;
    case PerformanceSpectrumVariant::kLogDisconnected:
      return Component::kLogDisconnected;
    case PerformanceSpectrumVariant::kLazyframe:
      return Component::kLazyframe;
    case PerformanceSpectrumVariant::kLazyframeDisconnected:
      return Component::kLazyframeDisconnected;
  }
  throw std::invalid_argument("performance spectrum: unknown variant");
}

Component component_for_lazy_variant(PerformanceSpectrumVariant variant) {
  switch (variant) {
    case PerformanceSpectrumVariant::kConnected:
      return Component::kLazyframe;
    case PerformanceSpectrumVariant::kDisconnected:
      return Component::kLazyframeDisconnected;
    case PerformanceSpectrumVariant::kDataframe:
      return Component::kDataframe;
    case PerformanceSpectrumVariant::kLog:
      return Component::kLog;
    case PerformanceSpectrumVariant::kDataframeDisconnected:
      return Component::kDataframeDisconnected;
    case PerformanceSpectrumVariant::kLogDisconnected:
      return Component::kLogDisconnected;
    case PerformanceSpectrumVariant::kLazyframe:
      return Component::kLazyframe;
    case PerformanceSpectrumVariant::kLazyframeDisconnected:
      return Component::kLazyframeDisconnected;
  }
  throw std::invalid_argument("performance spectrum: unknown variant");
}

}  // namespace

PerformanceSpectrum compute_performance_spectrum(
    const EventLog& log, std::vector<std::string> activities,
    PerformanceSpectrumOptions options) {
  const auto component = component_for_variant(options.variant);
  return performance_spectrum_detail::run_component(
      log, std::move(activities), std::move(options), component);
}

PerformanceSpectrum discover_performance_spectrum_log(
    const EventLog& log, std::vector<std::string> activities,
    PerformanceSpectrumOptions options) {
  options.variant = PerformanceSpectrumVariant::kLog;
  return performance_spectrum_detail::run_component(
      log, std::move(activities), std::move(options), Component::kLog);
}

PerformanceSpectrum discover_performance_spectrum_dataframe(
    const EventTable& table, std::vector<std::string> activities,
    PerformanceSpectrumOptions options) {
  options.variant = PerformanceSpectrumVariant::kDataframe;
  return performance_spectrum_detail::run_component(
      table, std::move(activities), std::move(options), Component::kDataframe);
}

PerformanceSpectrum discover_performance_spectrum_dataframe_disconnected(
    const EventTable& table, std::vector<std::string> activities,
    PerformanceSpectrumOptions options) {
  options.variant = PerformanceSpectrumVariant::kDataframeDisconnected;
  return performance_spectrum_detail::run_component(
      table, std::move(activities), std::move(options),
      Component::kDataframeDisconnected);
}

PerformanceSpectrum discover_performance_spectrum_log_disconnected(
    const EventLog& log, std::vector<std::string> activities,
    PerformanceSpectrumOptions options) {
  options.variant = PerformanceSpectrumVariant::kLogDisconnected;
  return performance_spectrum_detail::run_component(
      log, std::move(activities), std::move(options),
      Component::kLogDisconnected);
}

PerformanceSpectrum discover_performance_spectrum_lazyframe(
    const LazyEventTable& table, std::vector<std::string> activities,
    PerformanceSpectrumOptions options) {
  options.variant = PerformanceSpectrumVariant::kLazyframe;
  return performance_spectrum_detail::run_component(
      table, std::move(activities), std::move(options), Component::kLazyframe);
}

PerformanceSpectrum discover_performance_spectrum_lazyframe_disconnected(
    const LazyEventTable& table, std::vector<std::string> activities,
    PerformanceSpectrumOptions options) {
  options.variant = PerformanceSpectrumVariant::kLazyframeDisconnected;
  return performance_spectrum_detail::run_component(
      table, std::move(activities), std::move(options),
      Component::kLazyframeDisconnected);
}

PerformanceSpectrum compute_performance_spectrum(
    const EventTable& table, std::vector<std::string> activities,
    PerformanceSpectrumOptions options) {
  const auto component = component_for_table_variant(options.variant);
  return performance_spectrum_detail::run_component(
      table, std::move(activities), std::move(options), component);
}

PerformanceSpectrum compute_performance_spectrum(
    const EventStream& stream, std::vector<std::string> activities,
    PerformanceSpectrumOptions options) {
  const auto component = component_for_variant(options.variant);
  return performance_spectrum_detail::run_component(
      stream, std::move(activities), std::move(options), component);
}

PerformanceSpectrum compute_performance_spectrum(
    const LazyEventTable& table, std::vector<std::string> activities,
    PerformanceSpectrumOptions options) {
  const auto component = component_for_lazy_variant(options.variant);
  return performance_spectrum_detail::run_component(
      table, std::move(activities), std::move(options), component);
}

PerformanceSpectrum compute_performance_spectrum(
    const EventDataView& data, std::vector<std::string> activities,
    PerformanceSpectrumOptions options) {
  switch (data.kind()) {
    case EventDataKind::kEventLog:
      return compute_performance_spectrum(*data.log(), std::move(activities),
                                          std::move(options));
    case EventDataKind::kEventTable:
      return compute_performance_spectrum(*data.table(), std::move(activities),
                                          std::move(options));
    case EventDataKind::kLazyEventTable:
      return compute_performance_spectrum(*data.lazy_table(),
                                          std::move(activities),
                                          std::move(options));
    case EventDataKind::kEventStream:
      return compute_performance_spectrum(*data.stream(),
                                          std::move(activities),
                                          std::move(options));
  }
  throw std::invalid_argument("performance spectrum: unsupported event data");
}

}  // namespace minera
