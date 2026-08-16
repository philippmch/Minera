#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

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
};

struct BatchDiscoveryOptions {
  BatchDiscoveryVariant variant = BatchDiscoveryVariant::kLog;
  double merge_distance_seconds = 900.0;
  std::size_t min_batch_size = 2;
  // Empty means use the event timestamp as completion and start.
  std::string start_timestamp_attribute;
};

struct BatchDiscovery {
  BatchDiscoveryOptions options;
  BatchDiscoveryVariant variant = BatchDiscoveryVariant::kLog;
  std::vector<BatchGroup> groups;
};

[[nodiscard]] BatchDiscovery discover_batches(
    const EventLog& log, BatchDiscoveryOptions options = {});

}  // namespace minera
