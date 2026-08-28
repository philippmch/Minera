#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

enum class TraceVariantQueryVariant {
  kLaplace,
  kSacofa,
  LAPLACE = kLaplace,
  SACOFA = kSacofa,
};

using TraceVariantQueryPrefix = std::vector<std::string>;
using TraceVariantQueryFrequencies =
    std::map<TraceVariantQueryPrefix, std::uint64_t>;

struct TraceVariantQueryOptions {
  TraceVariantQueryVariant variant = TraceVariantQueryVariant::kSacofa;
  double epsilon = 1.0;
  std::size_t max_prefix_length = 0;
  std::uint64_t prune_threshold = 1;
  std::uint64_t seed = 0;
  // When enabled, replace the deterministic seed with one OS entropy draw.
  bool secure_random = false;
  // Safety limits apply before materialising the returned EventLog.
  std::size_t max_output_traces = 100000;
  std::size_t max_output_variants = 10000;
};

struct TraceVariantQueryDiagnostics {
  TraceVariantQueryVariant variant = TraceVariantQueryVariant::kLaplace;
  double epsilon = 0.0;
  std::size_t max_prefix_length = 0;
  std::uint64_t prune_threshold = 0;
  std::uint64_t seed = 0;
  bool secure_random = false;
  std::size_t candidate_count = 0;
  std::size_t selected_violation_count = 0;
  std::size_t retained_variant_count = 0;
  std::uint64_t released_trace_count = 0;
  bool output_truncated = false;
  std::string message;
};

struct TraceVariantQueryResult {
  EventLog log;
  TraceVariantQueryFrequencies frequencies;
  TraceVariantQueryDiagnostics diagnostics;
};

[[nodiscard]] TraceVariantQueryResult anonymize_trace_variant_query(
    const EventLog& log, const TraceVariantQueryOptions& options = {});

// Short aliases used by adapters and clients.
[[nodiscard]] inline TraceVariantQueryResult query_trace_variants(
    const EventLog& log, const TraceVariantQueryOptions& options = {}) {
  return anonymize_trace_variant_query(log, options);
}

[[nodiscard]] inline TraceVariantQueryResult discover_trace_variant_query(
    const EventLog& log, const TraceVariantQueryOptions& options = {}) {
  return anonymize_trace_variant_query(log, options);
}

[[nodiscard]] inline TraceVariantQueryResult anonymize_trace_variants(
    const EventLog& log, const TraceVariantQueryOptions& options = {}) {
  return anonymize_trace_variant_query(log, options);
}

}  // namespace minera
