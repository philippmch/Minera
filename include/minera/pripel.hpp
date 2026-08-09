#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

struct PripelOptions {
  double epsilon = 1.0;
  std::vector<std::string> blocklist;
  std::uint64_t seed = 0;
  // When enabled, replace the deterministic seed with one OS entropy draw.
  bool secure_random = false;
  std::size_t max_output_traces = 100000;
  std::size_t max_distance = 3;
};

struct PripelDiagnostics {
  double epsilon = 0.0;
  std::uint64_t seed = 0;
  bool secure_random = false;
  std::size_t query_trace_count = 0;
  std::size_t released_trace_count = 0;
  std::size_t exact_matches = 0;
  std::size_t nearest_matches = 0;
  std::size_t synthesized_traces = 0;
  std::size_t synthesized_events = 0;
  bool output_truncated = false;
  std::string message;

  [[nodiscard]] std::string to_json() const;
};

struct PripelResult {
  EventLog log;
  PripelDiagnostics diagnostics;
};

[[nodiscard]] PripelResult anonymize_pripel(
    const EventLog& source, const EventLog& released_query,
    const PripelOptions& options = {});

inline PripelResult pripel_anonymize(const EventLog& source,
                                     const EventLog& released_query,
                                     const PripelOptions& options = {}) {
  return anonymize_pripel(source, released_query, options);
}

inline PripelResult anonymize_pripel_log(const EventLog& source,
                                         const EventLog& released_query,
                                         const PripelOptions& options = {}) {
  return anonymize_pripel(source, released_query, options);
}

}  // namespace minera
