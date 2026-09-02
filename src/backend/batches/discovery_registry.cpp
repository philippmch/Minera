#include "discovery_registry.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>

#include "minera/log_conversion.hpp"

namespace minera::batches_detail {
namespace {

using Selector = BatchDiscoverySelector;

std::string upper(std::string_view value) {
  std::string result(value);
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::toupper(character));
                 });
  return result;
}

}  // namespace

const std::vector<BatchDiscoverySelectorDescriptor>&
BatchDiscoverySelectorRegistry::all() {
  static const std::vector<BatchDiscoverySelectorDescriptor> descriptors = {
      {Selector::kLog, "LOG"},
      {Selector::kPandas, "PANDAS"},
      {Selector::kPolars, "POLARS"},
  };
  return descriptors;
}

bool BatchDiscoverySelectorRegistry::supports(Selector selector) noexcept {
  return std::any_of(all().begin(), all().end(), [selector](const auto& entry) {
    return entry.selector == selector;
  });
}

std::optional<Selector> BatchDiscoverySelectorRegistry::parse(
    std::string_view name) {
  const auto normalized = upper(name);
  for (const auto& descriptor : all())
    if (descriptor.name == normalized) return descriptor.selector;
  return std::nullopt;
}

BatchDiscovery BatchDiscoverySelectorRegistry::discover(
    const EventLog& log, Selector selector, BatchDiscoveryOptions options) {
  switch (selector) {
    case Selector::kLog:
      return LogBatchesDiscovery::apply(log, std::move(options));
    case Selector::kPandas:
      return PandasBatchesDiscovery::apply(event_log_to_table(log),
                                           std::move(options));
    case Selector::kPolars:
      return PolarsBatchesDiscovery::apply(event_log_to_lazy_table(log),
                                           std::move(options));
  }
  throw std::invalid_argument("unsupported batches selector");
}

BatchDiscovery BatchDiscoverySelectorRegistry::discover(
    const EventTable& table, Selector selector, BatchDiscoveryOptions options) {
  switch (selector) {
    case Selector::kLog:
      return LogBatchesDiscovery::apply(event_table_to_event_log(table),
                                        std::move(options));
    case Selector::kPandas:
      return PandasBatchesDiscovery::apply(table, std::move(options));
    case Selector::kPolars:
      return PolarsBatchesDiscovery::apply(LazyEventTable::from_table(table),
                                            std::move(options));
  }
  throw std::invalid_argument("unsupported batches selector");
}

BatchDiscovery BatchDiscoverySelectorRegistry::discover(
    const LazyEventTable& table, Selector selector,
    BatchDiscoveryOptions options) {
  switch (selector) {
    case Selector::kLog:
      return LogBatchesDiscovery::apply(lazy_event_table_to_event_log(table),
                                        std::move(options));
    case Selector::kPandas:
      return PandasBatchesDiscovery::apply(table.materialize(),
                                           std::move(options));
    case Selector::kPolars:
      return PolarsBatchesDiscovery::apply(table, std::move(options));
  }
  throw std::invalid_argument("unsupported batches selector");
}

BatchDiscovery BatchDiscoverySelectorRegistry::discover(
    const EventDataView& data, Selector selector, BatchDiscoveryOptions options) {
  if (data.log() != nullptr) return discover(*data.log(), selector, std::move(options));
  if (data.stream() != nullptr) {
    // EventStream is the flat, value-semantic source used by conversion
    // adapters.  Route each selector through its corresponding native input
    // so the registry does not accidentally apply log semantics to a table
    // or lazy-table request.
    switch (selector) {
      case Selector::kLog:
        return discover(event_stream_to_event_log(*data.stream()), selector,
                        std::move(options));
      case Selector::kPandas:
        return discover(event_stream_to_table(*data.stream()), selector,
                        std::move(options));
      case Selector::kPolars:
        return discover(
            LazyEventTable::from_table(event_stream_to_table(*data.stream())),
            selector, std::move(options));
    }
    throw std::invalid_argument("unsupported batches selector");
  }
  if (data.table() != nullptr)
    return discover(*data.table(), selector, std::move(options));
  if (data.lazy_table() != nullptr)
    return discover(*data.lazy_table(), selector, std::move(options));
  throw std::invalid_argument("batches selector received an empty data view");
}

BatchDiscovery BatchDiscoverySelectorRegistry::dispatch(
    const EventLog& log, Selector selector, BatchDiscoveryOptions options) {
  return discover(log, selector, std::move(options));
}

BatchDiscovery BatchDiscoverySelectorRegistry::dispatch(
    const EventTable& table, Selector selector, BatchDiscoveryOptions options) {
  return discover(table, selector, std::move(options));
}

BatchDiscovery BatchDiscoverySelectorRegistry::dispatch(
    const LazyEventTable& table, Selector selector,
    BatchDiscoveryOptions options) {
  return discover(table, selector, std::move(options));
}

BatchDiscovery BatchDiscoverySelectorRegistry::dispatch(
    const EventDataView& data, Selector selector, BatchDiscoveryOptions options) {
  return discover(data, selector, std::move(options));
}

}  // namespace minera::batches_detail
