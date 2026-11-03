#include "minera/sampling.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

#include "minera/secure_random.hpp"

namespace minera {
namespace sampling {

std::vector<std::size_t> pick_points_subset(std::size_t m, std::size_t n) {
  if (n == 0) return {};
  std::vector<std::size_t> points{0, n - 1};
  if (m == 0) {  // no inner points: the two extremes only
    points.erase(std::unique(points.begin(), points.end()), points.end());
    return points;
  }
  if (m >= n) {
    // The inner points step by at most one, so every index is chosen.
    points.resize(n);
    for (std::size_t i = 0; i < n; ++i) points[i] = i;
    return points;
  }
  if (n > 0xffffffffU)
    throw std::invalid_argument("sample_cases: too many cases to sample");
  const std::uint64_t offset = n / (2 * static_cast<std::uint64_t>(m));
  for (std::size_t j = 0; j < m; ++j)
    points.push_back(static_cast<std::size_t>(
        static_cast<std::uint64_t>(j) * n / m + offset));
  std::sort(points.begin(), points.end());
  points.erase(std::unique(points.begin(), points.end()), points.end());
  return points;
}

}  // namespace sampling

namespace {

struct EventRef {
  std::size_t trace = 0;
  std::size_t event = 0;
};

// Rebuilds a log from sampled events: traces in the order their first event
// was sampled, each with its events in original order.
EventLog regroup(const EventLog& log, const std::vector<EventRef>& rows,
                 const std::vector<std::size_t>& order) {
  std::vector<std::vector<std::size_t>> kept(log.traces.size());
  std::vector<std::size_t> trace_order;
  for (const auto index : order) {
    const auto& row = rows[index];
    if (kept[row.trace].empty()) trace_order.push_back(row.trace);
    kept[row.trace].push_back(row.event);
  }
  EventLog result;
  copy_event_log_context(log, result);
  result.traces.reserve(trace_order.size());
  for (const auto trace_index : trace_order) {
    const auto& source = log.traces[trace_index];
    auto& events = kept[trace_index];
    std::sort(events.begin(), events.end());
    Trace trace;
    copy_trace_context(source, trace);
    trace.events.reserve(events.size());
    for (const auto event : events) trace.events.push_back(source.events[event]);
    result.traces.push_back(std::move(trace));
  }
  return result;
}

std::vector<EventRef> event_rows(const EventLog& log) {
  std::vector<EventRef> rows;
  rows.reserve(log.event_count());
  for (std::size_t t = 0; t < log.traces.size(); ++t)
    for (std::size_t e = 0; e < log.traces[t].events.size(); ++e)
      rows.push_back({t, e});
  return rows;
}

EventLog sample_traces(const EventLog& log, std::size_t count,
                       sampling::PythonRandom& random) {
  const auto picked = random.sample(log.traces.size(),
                                    std::min(count, log.traces.size()));
  EventLog result;
  copy_event_log_context(log, result);
  result.traces.reserve(picked.size());
  for (const auto index : picked) result.traces.push_back(log.traces[index]);
  return result;
}

EventLog sample_table_cases(const EventLog& log, std::size_t count,
                            sampling::PythonRandom& random) {
  // Distinct case ids in row order; a trace without events has no rows.
  std::vector<std::string> ids;
  std::unordered_set<std::string> seen;
  for (const auto& trace : log.traces)
    if (!trace.events.empty() && seen.insert(trace.case_id).second)
      ids.push_back(trace.case_id);
  EventLog result;
  copy_event_log_context(log, result);
  if (ids.empty()) return result;
  std::vector<std::size_t> order(ids.size());
  for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
  random.shuffle(order);
  std::unordered_set<std::string> chosen;
  for (const auto point : sampling::pick_points_subset(count, ids.size()))
    chosen.insert(ids[order[point]]);
  for (const auto& trace : log.traces)
    if (!trace.events.empty() && chosen.contains(trace.case_id))
      result.traces.push_back(trace);
  return result;
}

}  // namespace

std::optional<SampleInput> parse_sample_input(std::string_view name) {
  if (name == "dataframe") return SampleInput::kDataFrame;
  if (name == "event_log") return SampleInput::kEventLog;
  if (name == "event_stream") return SampleInput::kEventStream;
  return std::nullopt;
}

std::string_view sample_input_name(SampleInput input) noexcept {
  switch (input) {
    case SampleInput::kDataFrame:
      return "dataframe";
    case SampleInput::kEventLog:
      return "event_log";
    case SampleInput::kEventStream:
      return "event_stream";
  }
  return "dataframe";
}

std::string sampling_options_json(const LogSamplingOptions& options) {
  const auto count = [](const std::optional<std::size_t>& value) {
    return value ? std::to_string(*value) : std::string("null");
  };
  return "{\"cases\":" + count(options.cases) + ",\"events\":" +
         count(options.events) + ",\"seed\":" + std::to_string(options.seed) +
         ",\"input\":\"" + std::string(sample_input_name(options.input)) +
         "\"}";
}

std::uint32_t random_sample_seed() {
  return static_cast<std::uint32_t>(secure_random_u64() & 0xffffffffU);
}

EventLog sample_event_log(const EventLog& log,
                          const LogSamplingOptions& options) {
  // A script seeds both generators up front: random.seed(s) and
  // numpy.random.seed(s); each call then draws from the one it uses.
  sampling::PythonRandom python(options.seed);
  sampling::NumpyRandomState numpy(options.seed);
  EventLog current = log;
  if (options.cases) {
    switch (options.input) {
      case SampleInput::kDataFrame:
        current = sample_table_cases(current, *options.cases, python);
        break;
      case SampleInput::kEventLog:
        current = sample_traces(current, *options.cases, python);
        break;
      case SampleInput::kEventStream:
        throw std::invalid_argument(
            "sample_cases: not defined for an event stream; use dataframe or "
            "event_log");
    }
  }
  if (options.events) {
    const auto count = *options.events;
    switch (options.input) {
      case SampleInput::kDataFrame: {
        const auto rows = event_rows(current);
        if (count > rows.size())
          throw std::invalid_argument(
              "sample_events: cannot take a larger sample (" +
              std::to_string(count) + ") than the " +
              std::to_string(rows.size()) + " events");
        auto order = numpy.permutation(rows.size());
        order.resize(count);
        current = regroup(current, rows, order);
        break;
      }
      case SampleInput::kEventLog:
        current = sample_traces(current, count, python);
        break;
      case SampleInput::kEventStream: {
        const auto rows = event_rows(current);
        const auto order =
            python.sample(rows.size(), std::min(count, rows.size()));
        current = regroup(current, rows, order);
        break;
      }
    }
  }
  return current;
}

}  // namespace minera
