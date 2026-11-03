#include "minera/passed_time.hpp"

#include <cmath>
#include <map>
#include <set>
#include <vector>
#include <stdexcept>
#include <string_view>

namespace minera {
namespace {

bool builtin_activity_key(std::string_view key) {
  return key.empty() || key == "activity" || key == "concept:name";
}

bool builtin_timestamp_key(std::string_view key) {
  return key.empty() || key == "timestamp" || key == "time:timestamp";
}

std::string required_activity(const Event& event, std::string_view key) {
  if (builtin_activity_key(key)) return event.activity;
  if (key == "resource") return event.resource;
  if (const auto* value = event.attribute(key)) return *value;
  throw std::invalid_argument("passed time: event is missing activity attribute '" +
                              std::string(key) + "'");
}

std::string required_timestamp(const Event& event, std::string_view key,
                               std::string_view label) {
  if (builtin_timestamp_key(key)) return event.timestamp;
  if (const auto* value = event.attribute(key)) return *value;
  throw std::invalid_argument("passed time: event is missing " +
                              std::string(label) + " attribute '" +
                              std::string(key) + "'");
}

bool scalar_aggregation(DfgPerformanceAggregation aggregation) {
  return aggregation != DfgPerformanceAggregation::kRawValues &&
         aggregation != DfgPerformanceAggregation::kAll;
}

void append_entry(std::vector<PassedTimeEntry>& destination,
                  double& weighted_total, std::uint64_t& total_frequency,
                  const DfgActivityPair& pair,
                  const DfgPerformanceStatistics& performance,
                  std::uint64_t frequency, bool pre) {
  if (!std::holds_alternative<double>(performance.value)) {
    throw std::invalid_argument(
        "passed time: performance aggregation must be scalar");
  }
  const double value = std::get<double>(performance.value);
  if (!std::isfinite(value))
    throw std::invalid_argument("passed time: performance is not finite");
  destination.push_back({pre ? pair.first : pair.second, value, frequency});
  weighted_total += value * static_cast<double>(frequency);
  total_frequency += frequency;
}

}  // namespace

const char* passed_time_variant_name(PassedTimeVariant variant) {
  switch (variant) {
    case PassedTimeVariant::kPre:
      return "PRE";
    case PassedTimeVariant::kPost:
      return "POST";
    case PassedTimeVariant::kPrePost:
      return "PREPOST";
  }
  throw std::invalid_argument("passed time: unknown variant");
}

PassedTimeResult discover_passed_time(const EventLog& log,
                                      const PassedTimeOptions& options) {
  if (!scalar_aggregation(options.performance_aggregation)) {
    throw std::invalid_argument(
        "passed time: aggregation must produce one scalar per edge");
  }

  EventLog projected = log;
  for (auto& trace : projected.traces) {
    for (auto& event : trace.events) {
      event.activity = required_activity(event, options.activity_key);
      // The performance calculation only reads timestamps for adjacent
      // pairs. A singleton trace contributes no edge and therefore does not
      // require a timestamp, matching the underlying DFG statistic.
      if (trace.events.size() > 1) {
        event.timestamp = required_timestamp(event, options.timestamp_key,
                                             "timestamp");
      } else if (builtin_timestamp_key(options.timestamp_key)) {
        event.timestamp = event.timestamp;
      } else if (const auto* value = event.attribute(options.timestamp_key)) {
        event.timestamp = *value;
      } else {
        event.timestamp.clear();
      }
    }
  }

  DfgStatisticalOptions dfg_options;
  dfg_options.window = 1;
  dfg_options.performance_aggregation = options.performance_aggregation;
  dfg_options.use_business_hours = options.use_business_hours;
  dfg_options.business_hour_slots = options.business_hour_slots;
  if (!options.start_timestamp_key.empty() &&
      !builtin_timestamp_key(options.start_timestamp_key)) {
    dfg_options.start_timestamp_attribute = options.start_timestamp_key;
  } else {
    dfg_options.start_timestamp_attribute.clear();
  }

  const auto frequency = discover_frequency_dfg(projected, dfg_options);
  const auto performance = discover_performance_dfg(projected, dfg_options);

  PassedTimeResult result;
  result.variant = options.variant;
  result.activity = options.activity;
  double pre_total = 0.0;
  double post_total = 0.0;
  std::uint64_t pre_count = 0;
  std::uint64_t post_count = 0;

  // Rows follow the order in which each directly-follows pair first occurs,
  // walking the traces in log order, as the reference's rows do.
  std::vector<DfgActivityPair> order;
  {
    std::set<DfgActivityPair> seen;
    for (const auto& trace : projected.traces) {
      for (std::size_t i = 1; i < trace.events.size(); ++i) {
        DfgActivityPair pair{trace.events[i - 1].activity, trace.events[i].activity};
        if (performance.count(pair) && seen.insert(pair).second)
          order.push_back(std::move(pair));
      }
    }
    for (const auto& entry : performance)
      if (seen.insert(entry.first).second) order.push_back(entry.first);
  }

  for (const auto& pair : order) {
    const auto& statistics = performance.at(pair);
    const auto frequency_it = frequency.find(pair);
    if (frequency_it == frequency.end()) {
      throw std::invalid_argument("passed time: frequency/performance mismatch");
    }
    const auto count = frequency_it->second;
    if (options.variant == PassedTimeVariant::kPre ||
        options.variant == PassedTimeVariant::kPrePost) {
      if (pair.second == options.activity) {
        append_entry(result.pre, pre_total, pre_count, pair, statistics, count,
                     true);
      }
    }
    if (options.variant == PassedTimeVariant::kPost ||
        options.variant == PassedTimeVariant::kPrePost) {
      if (pair.first == options.activity) {
        append_entry(result.post, post_total, post_count, pair, statistics, count,
                     false);
      }
    }
  }

  if (pre_count != 0)
    result.pre_avg_perf = pre_total / static_cast<double>(pre_count);
  if (post_count != 0)
    result.post_avg_perf = post_total / static_cast<double>(post_count);
  return result;
}

}  // namespace minera
