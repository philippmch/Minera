#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "minera/batches.hpp"

namespace minera::batches_detail {

using LogBatchesDiscovery = ::minera::BatchLogDiscovery;
using PandasBatchesDiscovery = ::minera::BatchPandasDiscovery;
using PolarsBatchesDiscovery = ::minera::BatchPolarsDiscovery;

enum class BatchDiscoverySelector {
  kLog,
  kPandas,
  kPolars,

  LOG = kLog,
  PANDAS = kPandas,
  POLARS = kPolars,
};

struct BatchDiscoverySelectorDescriptor {
  BatchDiscoverySelector selector;
  std::string name;
};

class BatchDiscoverySelectorRegistry {
 public:
  [[nodiscard]] static const std::vector<BatchDiscoverySelectorDescriptor>&
  all();
  [[nodiscard]] static bool supports(
      BatchDiscoverySelector selector) noexcept;
  [[nodiscard]] static std::optional<BatchDiscoverySelector> parse(
      std::string_view name);

  [[nodiscard]] static BatchDiscovery discover(
      const EventLog& log, BatchDiscoverySelector selector,
      BatchDiscoveryOptions options = {});
  [[nodiscard]] static BatchDiscovery discover(
      const EventTable& table, BatchDiscoverySelector selector,
      BatchDiscoveryOptions options = {});
  [[nodiscard]] static BatchDiscovery discover(
      const LazyEventTable& table, BatchDiscoverySelector selector,
      BatchDiscoveryOptions options = {});
  [[nodiscard]] static BatchDiscovery discover(
      const EventDataView& data, BatchDiscoverySelector selector,
      BatchDiscoveryOptions options = {});

  [[nodiscard]] static BatchDiscovery dispatch(
      const EventLog& log, BatchDiscoverySelector selector,
      BatchDiscoveryOptions options = {});
  [[nodiscard]] static BatchDiscovery dispatch(
      const EventTable& table, BatchDiscoverySelector selector,
      BatchDiscoveryOptions options = {});
  [[nodiscard]] static BatchDiscovery dispatch(
      const LazyEventTable& table, BatchDiscoverySelector selector,
      BatchDiscoveryOptions options = {});
  [[nodiscard]] static BatchDiscovery dispatch(
      const EventDataView& data, BatchDiscoverySelector selector,
      BatchDiscoveryOptions options = {});
};

}  // namespace minera::batches_detail
