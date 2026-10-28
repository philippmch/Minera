#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "minera/event_data.hpp"

namespace minera {

enum class BatchType {
  kSimultaneous,
  kBatchingOnStart,
  kBatchingOnEnd,
  kSequential,
  kConcurrent,
  simultaneous = kSimultaneous,
  batching_on_start = kBatchingOnStart,
  batching_on_end = kBatchingOnEnd,
  sequential = kSequential,
  concurrent = kConcurrent,
};

// The log, pandas, and polars entry points share the same deterministic
// interval semantics in the native adapter. Keeping the variation explicit
// preserves the public selector vocabulary without requiring a columnar
// runtime in the core library.
enum class BatchDiscoveryVariant {
  kLog,
  kPandas,
  kPolars,

  LOG = kLog,
  PANDAS = kPandas,
  POLARS = kPolars,
};

struct BatchEvent {
  double start = 0.0;
  double complete = 0.0;
  std::string case_id;
  std::size_t trace_index = 0;
  std::size_t event_index = 0;
  // Present only when a table adapter was given an event-id selector.  The
  // selector is part of tuple identity, so rows with distinct ids remain
  // distinct even when every other event value matches.
  std::string event_id;
};

struct Batch {
  BatchType type = BatchType::kConcurrent;
  double start = 0.0;
  double complete = 0.0;
  std::vector<BatchEvent> events;
};

struct BatchGroup {
  std::string activity;
  std::string resource;
  std::size_t batch_count = 0;
  std::size_t event_count = 0;
  std::vector<Batch> batches;
  // The Polars adapter groups a null resource as its own key.
  bool resource_is_null = false;
};

struct BatchDiscoveryOptions {
  BatchDiscoveryVariant variant = BatchDiscoveryVariant::kLog;
  double merge_distance_seconds = 900.0;
  std::size_t min_batch_size = 2;
  // Empty means use the event timestamp as completion and start.
  std::string start_timestamp_attribute;
  // Event/trace selectors. Standard aliases resolve to the first-class event
  // and trace fields; any other name is read from the generic attributes.
  // The completion timestamp defaults to the event timestamp and the case id
  // defaults to the trace's case identifier.
  std::string activity_attribute = "concept:name";
  std::string resource_attribute = "org:resource";
  std::string timestamp_attribute = "time:timestamp";
  std::string case_id_attribute = "case:concept:name";
  // Empty disables the optional table event-id projection.  When selected,
  // the value participates in event identity and is retained in BatchEvent.
  std::string event_id_attribute;
};

struct BatchDiscovery {
  BatchDiscoveryOptions options;
  BatchDiscoveryVariant variant = BatchDiscoveryVariant::kLog;
  std::vector<BatchGroup> groups;
};

[[nodiscard]] BatchDiscovery discover_batches(
    const EventLog& log, BatchDiscoveryOptions options = {});
[[nodiscard]] BatchDiscovery discover_batches(
    const EventTable& table, BatchDiscoveryOptions options = {});
[[nodiscard]] BatchDiscovery discover_batches(
    const LazyEventTable& table, BatchDiscoveryOptions options = {});

// The three adapter components intentionally remain separate callables.  The
// result shape is shared, while each entry point fixes the corresponding
// source variation in the returned metadata.
class BatchLogDiscovery {
 public:
  [[nodiscard]] static BatchDiscovery apply(
      const EventLog& log, BatchDiscoveryOptions options = {});
};

class BatchPandasDiscovery {
 public:
  [[nodiscard]] static BatchDiscovery apply(
      const EventTable& table, BatchDiscoveryOptions options = {});
};

class BatchPolarsDiscovery {
 public:
  [[nodiscard]] static BatchDiscovery apply(
      const LazyEventTable& table, BatchDiscoveryOptions options = {});
};

using LogBatchDiscovery = BatchLogDiscovery;
using PandasBatchDiscovery = BatchPandasDiscovery;
using PolarsBatchDiscovery = BatchPolarsDiscovery;

}  // namespace minera
