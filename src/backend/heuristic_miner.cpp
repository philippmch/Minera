#include "minera/heuristic_miner.hpp"

#include "business_hours.hpp"
#include "exact_count_ratio.hpp"
#include "exact_mean.hpp"
#include "exact_stdev.hpp"
#include "minera/log_conversion.hpp"
#include "minera/timestamp.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace minera {
namespace {

// Direct-succession counts plus the start and end activities, the only log
// statistics the dependency measure needs.
struct Counts {
  std::vector<std::string> names;
  std::vector<std::vector<std::uint64_t>> follows;  // follows[a][b] = |a > b|
  std::set<std::size_t> starts;
  std::set<std::size_t> ends;

  [[nodiscard]] std::size_t size() const { return names.size(); }
};

Counts build_counts(const EventLog& log) {
  Counts counts;
  std::unordered_map<std::string, std::size_t> ids;
  const auto intern = [&](const std::string& name) {
    const auto [it, inserted] = ids.try_emplace(name, counts.names.size());
    if (inserted) counts.names.push_back(name);
    return it->second;
  };

  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) intern(event.activity);
  }

  const std::size_t n = counts.names.size();
  counts.follows.assign(n, std::vector<std::uint64_t>(n, 0));
  for (const auto& trace : log.traces) {
    const auto& events = trace.events;
    if (events.empty()) continue;
    counts.starts.insert(ids[events.front().activity]);
    counts.ends.insert(ids[events.back().activity]);
    for (std::size_t i = 1; i < events.size(); ++i) {
      ++counts.follows[ids[events[i - 1].activity]][ids[events[i].activity]];
    }
  }
  return counts;
}

double dependency(const Counts& counts, std::size_t a, std::size_t b) {
  const double forward = static_cast<double>(counts.follows[a][b]);
  if (a == b) {
    return forward / (forward + 1.0);
  }
  const double backward = static_cast<double>(counts.follows[b][a]);
  return (forward - backward) / (forward + backward + 1.0);
}

// Groups the neighbours on one side of `center` into AND bundles. Two edges
// leaving (or entering) the same activity are bundled when their targets (or
// sources) follow each other often enough relative to the edges themselves —
// the AND measure (see HeuristicMinerOptions::and_threshold). The relation is
// closed transitively (b~c and c~d put b, c, d in one bundle), so bundles
// partition the neighbours: XOR across bundles, AND within.
std::vector<HeuristicBundle> to_bundles(const Counts& counts,
                                        std::size_t center,
                                        const std::vector<std::size_t>& neighbours,
                                        bool outgoing,
                                        double threshold) {
  const std::size_t k = neighbours.size();
  std::vector<std::size_t> root(k);
  for (std::size_t i = 0; i < k; ++i) root[i] = i;
  const auto find = [&](std::size_t x) {
    while (root[x] != x) x = root[x] = root[root[x]];
    return x;
  };

  for (std::size_t i = 0; i < k; ++i) {
    for (std::size_t j = i + 1; j < k; ++j) {
      const std::size_t b = neighbours[i];
      const std::size_t c = neighbours[j];
      const double between =
          static_cast<double>(counts.follows[b][c] + counts.follows[c][b]);
      const double base = static_cast<double>(
          outgoing ? counts.follows[center][b] + counts.follows[center][c]
                   : counts.follows[b][center] + counts.follows[c][center]);
      if (between / (base + 1.0) >= threshold) root[find(i)] = find(j);
    }
  }

  // Collect the classes in first-appearance order so bundles are deterministic.
  std::vector<HeuristicBundle> bundles;
  std::unordered_map<std::size_t, std::size_t> bundle_of_root;
  for (std::size_t i = 0; i < k; ++i) {
    const auto [it, inserted] = bundle_of_root.emplace(find(i), bundles.size());
    if (inserted) bundles.emplace_back();
    bundles[it->second].push_back(neighbours[i]);
  }
  return bundles;
}

double effective_dependency_threshold(const HeuristicMinerOptions& options) {
  return options.dependency_threshold.value_or(0.5);
}

struct Interval {
  std::string activity;
  double start = 0.0;
  double complete = 0.0;
  std::int64_t start_microseconds = 0;
  std::int64_t complete_microseconds = 0;
  std::optional<std::int64_t> business_start;
  std::optional<std::int64_t> business_complete;
};

using NamePair = std::pair<std::string, std::string>;

struct PlusAbstraction {
  std::vector<std::string> activity_order;
  std::map<std::string, std::uint64_t> occurrences;
  std::map<std::string, std::uint64_t> starts;
  std::map<std::string, std::uint64_t> ends;
  std::map<NamePair, std::uint64_t> eventually_follows;
  std::map<NamePair, std::uint64_t> direct_adjacency;
  std::map<NamePair, double> wait_sums;
  std::map<NamePair, std::vector<double>> wait_samples;
  std::map<std::string, double> sojourn_sums;
  std::map<NamePair, std::uint64_t> concurrency;
};

std::string ascii_lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

bool standard_activity_attribute(std::string_view key) {
  return key.empty() || key == "concept:name" || key == "activity";
}

const std::string& selected_activity(const Event& event,
                                     const HeuristicMinerOptions& options) {
  if (standard_activity_attribute(options.activity_attribute))
    return event.activity;
  if (const auto* value = event.attribute(options.activity_attribute))
    return *value;
  throw std::invalid_argument(
      "Heuristics Miner: event is missing activity attribute '" +
      options.activity_attribute + "'");
}

const std::string& required_activity(const Event& event,
                                     const HeuristicMinerOptions& options) {
  if (standard_activity_attribute(options.activity_attribute) &&
      !event.has_activity()) {
    throw std::invalid_argument(
        "Heuristics Miner PLUSPLUS: event is missing activity attribute");
  }
  return selected_activity(event, options);
}

bool standard_timestamp_attribute(std::string_view key) {
  return key.empty() || key == "time:timestamp" || key == "timestamp";
}

const std::string& selected_timestamp(const Event& event,
                                      const HeuristicMinerOptions& options) {
  if (standard_timestamp_attribute(options.timestamp_attribute))
    return event.timestamp;
  if (const auto* value = event.attribute(options.timestamp_attribute))
    return *value;
  throw std::invalid_argument(
      "Heuristics Miner: event is missing timestamp attribute '" +
      options.timestamp_attribute + "'");
}

const std::string& required_timestamp_text(
    const Event& event, const HeuristicMinerOptions& options) {
  if (standard_timestamp_attribute(options.timestamp_attribute) &&
      !event.has_timestamp()) {
    throw std::invalid_argument(
        "Heuristics Miner PLUSPLUS: event is missing timestamp attribute");
  }
  return selected_timestamp(event, options);
}

const std::string* selected_start_timestamp(
    const Event& event, const HeuristicMinerOptions& options,
    bool plus_plus_default = false) {
  if (!options.start_timestamp_attribute) {
    return plus_plus_default ? event.attribute("start_timestamp") : nullptr;
  }
  if (options.start_timestamp_attribute->empty()) return nullptr;
  if (standard_timestamp_attribute(*options.start_timestamp_attribute))
    return &event.timestamp;
  return event.attribute(*options.start_timestamp_attribute);
}

enum class ClassicStartKind { kCompletion, kValue, kMissing, kNull, kNaT };

struct ClassicStartTimestamp {
  ClassicStartKind kind = ClassicStartKind::kCompletion;
  const std::string* value = nullptr;
};

bool is_typed_nat(const EventAttribute& attribute) {
  const auto* date = std::get_if<NativeDate>(&attribute.typed_value);
  return !attribute.is_null && attribute.kind == AttributeKind::kDate &&
         date != nullptr && date->value == "NaT";
}

const EventAttribute* selected_start_attribute(const Event& event,
                                               std::string_view key) {
  if (event.timestamp_attribute &&
      (event.timestamp_source_key == key ||
       event.timestamp_attribute->key == key))
    return &*event.timestamp_attribute;
  const auto found = std::find_if(
      event.attributes.begin(), event.attributes.end(),
      [&](const EventAttribute& attribute) { return attribute.key == key; });
  return found == event.attributes.end() ? nullptr : &*found;
}

ClassicStartTimestamp classic_start_timestamp(
    const Event& event, const HeuristicMinerOptions& options) {
  if (!options.start_timestamp_attribute ||
      options.start_timestamp_attribute->empty())
    return {};

  const auto& key = *options.start_timestamp_attribute;
  if (standard_timestamp_attribute(key)) {
    if (const auto* attribute = selected_start_attribute(event, key)) {
      if (attribute->is_null)
        return {ClassicStartKind::kNull, nullptr};
      if (is_typed_nat(*attribute)) return {ClassicStartKind::kNaT, nullptr};
    }
    if (event.timestamp_state == FirstClassAttributeState::kNull)
      return {ClassicStartKind::kNull, nullptr};
    return {ClassicStartKind::kValue, &event.timestamp};
  }

  const auto* attribute = selected_start_attribute(event, key);
  if (attribute == nullptr)
    return {ClassicStartKind::kMissing, nullptr};
  if (attribute->is_null)
    return {ClassicStartKind::kNull, nullptr};
  if (is_typed_nat(*attribute)) return {ClassicStartKind::kNaT, nullptr};
  return {ClassicStartKind::kValue, &attribute->value};
}

EventAttribute typed_nat_attribute(std::string key,
                                  std::string_view dtype) {
  EventAttribute value;
  value.key = std::move(key);
  value.value = "NaT";
  value.kind = AttributeKind::kDate;
  NativeDate date{"NaT"};
  if (dtype.find("datetime64[ns") == 0)
    date.resolution = NativeDateResolution::kNanosecond;
  const auto timezone_separator = dtype.find(", ");
  if (timezone_separator != std::string_view::npos && !dtype.empty() &&
      dtype.back() == ']') {
    date.timezone = std::string(dtype.substr(
        timezone_separator + 2, dtype.size() - timezone_separator - 3));
  }
  value.typed_value = std::move(date);
  return value;
}

void preserve_datetime_missing_start_cells(EventTable& table,
                                           const HeuristicMinerOptions& options) {
  if (!options.start_timestamp_attribute ||
      options.start_timestamp_attribute->empty() || table.dtypes.empty())
    return;
  const auto column = std::find(table.columns.begin(), table.columns.end(),
                                *options.start_timestamp_attribute);
  if (column == table.columns.end()) return;
  const auto index = static_cast<std::size_t>(column - table.columns.begin());
  if (index >= table.dtypes.size() ||
      !std::string_view(table.dtypes[index]).starts_with("datetime64["))
    return;
  for (auto& row : table.rows) {
    if (!row[index])
      row[index] = typed_nat_attribute(*options.start_timestamp_attribute,
                                       table.dtypes[index]);
  }
}

void honor_flat_case_selector(EventStream& stream,
                              std::string_view selected_key) {
  for (auto& record : stream.events) {
    for (auto& field : record.fields) {
      if (field.first_class_role != FirstClassAttributeRole::kCaseId)
        continue;
      if (field.key == selected_key ||
          field.first_class_source_key == selected_key)
        continue;
      field.first_class_role = FirstClassAttributeRole::kNone;
      field.first_class_state = FirstClassAttributeState::kMissing;
      field.first_class_source_key.clear();
    }
  }
}

EventLog classic_event_log(const EventDataView& data,
                           const HeuristicMinerOptions& options) {
  LogConversionOptions conversion;
  conversion.case_id_key = options.case_id_attribute;
  if (data.kind() == EventDataKind::kEventTable ||
      data.kind() == EventDataKind::kLazyEventTable) {
    auto table = event_data_to_table(data, conversion);
    if (options.variant == HeuristicMinerVariant::kClassic &&
        options.decoration == HeuristicNetDecoration::kPerformance)
      preserve_datetime_missing_start_cells(table, options);
    auto stream = event_table_to_event_stream(table, conversion);
    honor_flat_case_selector(stream, options.case_id_attribute);
    return event_stream_to_event_log(stream, conversion);
  }
  if (data.kind() == EventDataKind::kEventStream) {
    auto stream = *data.stream();
    honor_flat_case_selector(stream, options.case_id_attribute);
    return event_stream_to_event_log(stream, conversion);
  }
  return event_data_to_event_log(data, conversion);
}

std::string canonical_activity_attribute(
    const HeuristicMinerOptions& options) {
  return options.activity_attribute.empty() ? "concept:name"
                                            : options.activity_attribute;
}

std::string canonical_timestamp_attribute(
    const HeuristicMinerOptions& options) {
  return options.timestamp_attribute.empty() ? "time:timestamp"
                                             : options.timestamp_attribute;
}

std::int64_t required_timestamp_microseconds(
    const std::string& value, const std::string& description) {
  const auto parsed = parse_timestamp_microseconds(value);
  if (!parsed)
    throw std::invalid_argument("Heuristics Miner PLUSPLUS requires a valid " +
                                description + " timestamp");
  return *parsed;
}

std::int64_t required_business_timestamp_microseconds(
    std::string_view value, std::string_view description) {
  const auto local = detail::local_wall_timestamp_microseconds(value);
  if (!local)
    throw std::invalid_argument(
        "Heuristics Miner: invalid business-hours " + std::string(description) +
        " timestamp");
  return *local;
}

struct IntervalTrace {
  std::vector<Interval> sorted;
  std::vector<Interval> performance_order;
};

IntervalTrace to_intervals(const Trace& trace,
                           const HeuristicMinerOptions& options,
                           bool pre_interval_mode,
                           bool pandas_route = false) {
  using LifecycleKey = std::pair<std::string, std::string>;
  struct PendingStart {
    std::string timestamp_text;
  };
  std::map<LifecycleKey, std::deque<PendingStart>> pending_starts;
  std::vector<Interval> intervals;
  for (const auto& event : trace.events) {
    const auto& timestamp_text = required_timestamp_text(event, options);
    const auto& activity = required_activity(event, options);
    if (pre_interval_mode) {
      const auto timestamp_microseconds =
          required_timestamp_microseconds(timestamp_text, "event");
      const double timestamp = detail::epoch_seconds_from_microseconds(
          timestamp_microseconds);
      const auto business_complete =
          options.use_business_hours
              ? std::optional<std::int64_t>{
                    required_business_timestamp_microseconds(timestamp_text,
                                                             "event")}
              : std::optional<std::int64_t>{};
      const auto* value =
          pandas_route && options.start_timestamp_attribute &&
                  options.start_timestamp_attribute->empty()
              ? event.attribute("")
              : selected_start_timestamp(event, options, true);
      if (value == nullptr)
        throw std::invalid_argument(
            "Heuristics Miner PLUSPLUS interval input is missing the "
            "configured start timestamp");
      const auto start_microseconds =
          required_timestamp_microseconds(*value, "start");
      const double start =
          detail::epoch_seconds_from_microseconds(start_microseconds);
      const auto business_start =
          options.use_business_hours
              ? std::optional<std::int64_t>{
                    required_business_timestamp_microseconds(*value, "start")}
              : std::optional<std::int64_t>{};
      intervals.push_back({activity,
                          start,
                          timestamp,
                          start_microseconds,
                          timestamp_microseconds,
                          business_start,
                          business_complete});
      continue;
    }

    // Lifecycle events with an explicit non-start/complete transition are
    // ignored after required field presence has been checked.  An explicitly
    // present empty transition is also ignored; an omitted transition remains
    // the atomic/completion form below.
    const bool lifecycle_present = event.has_lifecycle();
    const std::string lifecycle = ascii_lower(event.lifecycle);
    std::string instance;
    if (const auto* value =
            event.attribute(options.lifecycle_instance_attribute))
      instance = *value;
    const LifecycleKey key{activity, instance};
    if ((lifecycle_present && lifecycle.empty()) ||
        (!lifecycle.empty() && lifecycle != "start" &&
         lifecycle != "complete"))
      continue;

    if (lifecycle == "start") {
      pending_starts[key].push_back({timestamp_text});
      continue;
    }

    const auto timestamp_microseconds =
        required_timestamp_microseconds(timestamp_text, "event");
    const double timestamp = detail::epoch_seconds_from_microseconds(
        timestamp_microseconds);
    const auto business_complete =
        options.use_business_hours
            ? std::optional<std::int64_t>{
                  required_business_timestamp_microseconds(timestamp_text,
                                                           "event")}
            : std::optional<std::int64_t>{};
    auto start_microseconds = timestamp_microseconds;
    double start = timestamp;
    auto business_start = business_complete;
    auto pending = pending_starts.find(key);
    if (pending != pending_starts.end() && !pending->second.empty()) {
      start_microseconds = required_timestamp_microseconds(
          pending->second.front().timestamp_text, "start");
      start = detail::epoch_seconds_from_microseconds(start_microseconds);
      if (options.use_business_hours) {
        business_start = required_business_timestamp_microseconds(
            pending->second.front().timestamp_text, "start");
      }
      pending->second.pop_front();
    }
    intervals.push_back({activity,
                        start,
                        timestamp,
                        start_microseconds,
                        timestamp_microseconds,
                        business_start,
                        business_complete});
  }
  IntervalTrace result;
  result.sorted = intervals;
  std::stable_sort(result.sorted.begin(), result.sorted.end(),
                   [](const Interval& lhs, const Interval& rhs) {
                     return lhs.start_microseconds < rhs.start_microseconds;
                   });
  result.performance_order =
      pre_interval_mode ? std::move(intervals) : result.sorted;
  return result;
}

PlusAbstraction build_plus_abstraction(
    const EventLog& log, const HeuristicMinerOptions& options,
    bool pandas_route = false) {
  PlusAbstraction out;
  std::map<NamePair, std::uint64_t> directional_concurrency;
  std::set<std::string> known;
  const auto business_slots =
      options.use_business_hours
          ? detail::normalize_business_hour_slots(
                options.business_hour_slots,
                options.business_hour_slots_explicit, "Heuristics Miner")
          : std::vector<BusinessHourSlot>{};
  const bool pre_interval_mode =
      !log.traces.empty() && !log.traces.front().events.empty() &&
      (pandas_route && options.start_timestamp_attribute &&
               options.start_timestamp_attribute->empty()
           ? log.traces.front().events.front().attribute("")
           : selected_start_timestamp(log.traces.front().events.front(),
                                      options, true)) != nullptr;
  for (const auto& trace : log.traces) {
    const auto converted =
        to_intervals(trace, options, pre_interval_mode, pandas_route);
    const auto& intervals = converted.sorted;
    if (intervals.empty()) continue;
    // In pre-interval mode discovery preserves trace order for start/end/attribute
    // statistics; lifecycle conversion has already produced start-sorted
    // traces, represented here by performance_order as well.
    const auto& converted_order = converted.performance_order;
    ++out.starts[converted_order.front().activity];
    ++out.ends[converted_order.back().activity];
    for (const auto& interval : converted_order) {
      if (known.insert(interval.activity).second)
        out.activity_order.push_back(interval.activity);
      ++out.occurrences[interval.activity];
      double sojourn = interval.complete - interval.start;
      if (options.use_business_hours) {
        sojourn = detail::business_elapsed_seconds_microseconds(
            *interval.business_start, *interval.business_complete,
            business_slots);
      }
      out.sojourn_sums[interval.activity] += sojourn;
    }

    // The PLUSPLUS abstraction keeps only the first activity that starts
    // after each interval has completed.
    for (std::size_t i = 0; i < intervals.size(); ++i) {
      for (std::size_t j = i + 1; j < intervals.size(); ++j) {
        if (intervals[i].complete_microseconds <=
            intervals[j].start_microseconds) {
          ++out.eventually_follows[
              {intervals[i].activity, intervals[j].activity}];
          break;
        }
      }
    }

    // Inclusive overlap: intervals touching at one endpoint are concurrent.
    for (std::size_t i = 0; i < intervals.size(); ++i) {
      for (std::size_t j = i + 1; j < intervals.size(); ++j) {
        if (std::max(intervals[i].start_microseconds,
                     intervals[j].start_microseconds) <=
            std::min(intervals[i].complete_microseconds,
                     intervals[j].complete_microseconds)) {
          if (pandas_route) {
            ++directional_concurrency[
                {intervals[i].activity, intervals[j].activity}];
          } else {
            ++out.concurrency[std::minmax(intervals[i].activity,
                                          intervals[j].activity)];
          }
        } else {
          break;
        }
      }
    }

    // Performance DFG uses the converted trace order: lifecycle conversion
    // sorts it by start time, while pre-interval input is preserved verbatim.
    const auto& performance = converted.performance_order;
    for (std::size_t i = 1; i < performance.size(); ++i) {
      const NamePair pair{performance[i - 1].activity,
                          performance[i].activity};
      ++out.direct_adjacency[pair];
      double wait = detail::epoch_seconds_from_microseconds(
          performance[i].start_microseconds -
          performance[i - 1].complete_microseconds);
      if (options.use_business_hours) {
        wait = detail::business_elapsed_seconds_microseconds(
            *performance[i - 1].business_complete,
            *performance[i].business_start, business_slots);
      }
      wait = std::max(0.0, wait);
      out.wait_sums[pair] += wait;
      out.wait_samples[pair].push_back(wait);
    }
  }
  if (pandas_route) {
    // Public pandas controls retain the last sorted directional count for a
    // canonical activity pair; opposite orientations do not add together.
    for (const auto& [pair, count] : directional_concurrency)
      out.concurrency[std::minmax(pair.first, pair.second)] = count;
  }
  return out;
}

std::uint64_t get_count(const std::map<NamePair, std::uint64_t>& counts,
                        const std::string& first,
                        const std::string& second) {
  const auto it = counts.find({first, second});
  return it == counts.end() ? 0 : it->second;
}

std::uint64_t get_concurrency(const PlusAbstraction& abstraction,
                              const std::string& first,
                              const std::string& second) {
  const auto it = abstraction.concurrency.find(std::minmax(first, second));
  return it == abstraction.concurrency.end() ? 0 : it->second;
}

double plus_dependency(const PlusAbstraction& abstraction,
                       const std::string& first,
                       const std::string& second,
                       std::uint64_t min_frequency) {
  const double forward = static_cast<double>(
      get_count(abstraction.eventually_follows, first, second));
  const auto raw_backward =
      get_count(abstraction.eventually_follows, second, first);
  const double backward = static_cast<double>(
      raw_backward >= min_frequency ? raw_backward : 0);
  const double concurrent =
      static_cast<double>(get_concurrency(abstraction, first, second));
  return (forward - backward) / (forward + backward + concurrent);
}

std::vector<HeuristicBundle> maximal_cliques(
    const std::vector<std::size_t>& neighbours,
    const std::vector<std::vector<bool>>& adjacent) {
  const std::size_t k = neighbours.size();
  if (k == 0) return {};
  std::vector<HeuristicBundle> bundles;
  const auto intersect_neighbours =
      [&](const std::vector<std::size_t>& values, std::size_t vertex) {
        std::vector<std::size_t> result;
        for (const auto value : values)
          if (adjacent[vertex][value]) result.push_back(value);
        return result;
      };
  std::function<void(std::vector<std::size_t>, std::vector<std::size_t>,
                     std::vector<std::size_t>)>
      bron_kerbosch;
  bron_kerbosch = [&](std::vector<std::size_t> clique,
                      std::vector<std::size_t> candidates,
                      std::vector<std::size_t> excluded) {
    if (candidates.empty() && excluded.empty()) {
      HeuristicBundle bundle;
      for (const auto index : clique) bundle.push_back(neighbours[index]);
      bundles.push_back(std::move(bundle));
      return;
    }
    const auto iteration = candidates;
    for (const auto vertex : iteration) {
      clique.push_back(vertex);
      bron_kerbosch(clique, intersect_neighbours(candidates, vertex),
                    intersect_neighbours(excluded, vertex));
      clique.pop_back();
      candidates.erase(
          std::find(candidates.begin(), candidates.end(), vertex));
      excluded.push_back(vertex);
    }
  };
  std::vector<std::size_t> candidates(k);
  for (std::size_t i = 0; i < k; ++i) candidates[i] = i;
  bron_kerbosch({}, std::move(candidates), {});
  std::sort(bundles.begin(), bundles.end());
  return bundles;
}

void sort_bundles_by_activity_name(
    std::vector<HeuristicBundle>& bundles,
    const std::vector<std::string>& activities) {
  const auto by_name = [&](std::size_t lhs, std::size_t rhs) {
    return activities[lhs] < activities[rhs];
  };
  for (auto& bundle : bundles)
    std::sort(bundle.begin(), bundle.end(), by_name);
  std::sort(bundles.begin(), bundles.end(), [&](const auto& lhs,
                                                const auto& rhs) {
    return std::lexicographical_compare(
        lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
        [&](std::size_t left, std::size_t right) {
          return activities[left] < activities[right];
        });
  });
}

std::vector<HeuristicBundle> plus_bundles(
    const PlusAbstraction& abstraction, HeuristicsNet& net,
    std::size_t center, const std::vector<std::size_t>& neighbours,
    bool outgoing, double threshold, std::uint64_t min_frequency) {
  const std::size_t k = neighbours.size();
  std::vector<std::vector<bool>> adjacent(k, std::vector<bool>(k, false));
  for (std::size_t i = 0; i < k; ++i) {
    for (std::size_t j = i + 1; j < k; ++j) {
      const auto& n1 = net.activities[neighbours[i]];
      const auto& n2 = net.activities[neighbours[j]];
      const auto& middle = net.activities[center];
      const double numerator = static_cast<double>(
          (get_count(abstraction.eventually_follows, n1, n2) >=
                   min_frequency
               ? get_count(abstraction.eventually_follows, n1, n2)
               : 0) +
          (get_count(abstraction.eventually_follows, n2, n1) >=
                   min_frequency
               ? get_count(abstraction.eventually_follows, n2, n1)
               : 0) +
          get_concurrency(abstraction, n1, n2));
      const double denominator = static_cast<double>(
          outgoing
              ? get_count(abstraction.eventually_follows, middle, n1) +
                    get_count(abstraction.eventually_follows, middle, n2)
              : get_count(abstraction.eventually_follows, n1, middle) +
                    get_count(abstraction.eventually_follows, n2, middle));
      const double value = numerator / denominator;
      if (value > threshold) {
        adjacent[i][j] = adjacent[j][i] = true;
        auto& measures =
            outgoing ? net.and_measures_out : net.and_measures_in;
        measures.push_back(
            {center, neighbours[i], neighbours[j], value});
      }
    }
  }

  // The AND-pair graph is converted through maximal cliques. Cliques may
  // overlap (unlike classic's transitive partitioning), which is significant
  // for shapes such as b--c--d where {b,c} and {c,d} are distinct bundles.
  return maximal_cliques(neighbours, adjacent);
}

HeuristicClassicDfgInput build_classic_input(
    const EventLog& log, const HeuristicMinerOptions& options) {
  HeuristicClassicDfgInput input;
  const auto business_slots =
      options.use_business_hours
          ? detail::normalize_business_hour_slots(
                options.business_hour_slots,
                options.business_hour_slots_explicit, "Heuristics Miner")
          : std::vector<BusinessHourSlot>{};
  std::vector<std::string> activities;
  std::set<std::string> known;
  std::map<std::string, std::uint64_t> occurrences;
  std::map<std::string, std::uint64_t> starts;
  std::map<std::string, std::uint64_t> ends;
  if (options.decoration == HeuristicNetDecoration::kPerformance)
    input.performance_dfg.emplace();
  for (const auto& trace : log.traces) {
    if (!trace.events.empty()) {
      ++starts[selected_activity(trace.events.front(), options)];
      ++ends[selected_activity(trace.events.back(), options)];
    }
    std::set<NamePair> seen_direct;
    std::set<NamePair> seen_window_two;
    for (const auto& event : trace.events) {
      const auto& activity = selected_activity(event, options);
      if (known.insert(activity).second) activities.push_back(activity);
      ++occurrences[activity];
    }
    for (std::size_t i = 1; i < trace.events.size(); ++i) {
      const NamePair pair{selected_activity(trace.events[i - 1], options),
                          selected_activity(trace.events[i], options)};
      if (!options.keep_once_per_case || seen_direct.insert(pair).second)
        ++input.dfg[pair];
      if (options.decoration == HeuristicNetDecoration::kPerformance) {
        const auto complete_microseconds = required_timestamp_microseconds(
            selected_timestamp(trace.events[i - 1], options), "event");
        const auto start =
            classic_start_timestamp(trace.events[i], options);
        if (start.kind == ClassicStartKind::kMissing ||
            start.kind == ClassicStartKind::kNull)
          throw std::invalid_argument(
              "Heuristics Miner CLASSIC is missing the configured start "
              "timestamp attribute");
        if (start.kind == ClassicStartKind::kNaT) {
          if (options.use_business_hours)
            throw std::invalid_argument(
                "cannot convert float NaN to integer");
          (*input.performance_dfg)[pair].push_back(0.0);
          continue;
        }
        const auto& start_text = start.kind == ClassicStartKind::kCompletion
                                     ? selected_timestamp(trace.events[i], options)
                                     : *start.value;
        const auto start_microseconds =
            required_timestamp_microseconds(start_text, "start");
        double elapsed = detail::epoch_seconds_from_microseconds(
            start_microseconds - complete_microseconds);
        if (options.use_business_hours) {
          const auto previous_local =
              required_business_timestamp_microseconds(
                  selected_timestamp(trace.events[i - 1], options), "event");
          const auto current_local =
              required_business_timestamp_microseconds(start_text, "start");
          elapsed = detail::business_elapsed_seconds_microseconds(
              previous_local, current_local, business_slots);
        }
        (*input.performance_dfg)[pair].push_back(
            std::max(0.0, elapsed));
      }
    }
    for (std::size_t i = 2; i < trace.events.size(); ++i) {
      const NamePair pair{selected_activity(trace.events[i - 2], options),
                          selected_activity(trace.events[i], options)};
      if (!options.keep_once_per_case || seen_window_two.insert(pair).second)
        ++input.window_two_dfg[pair];
      ++input.frequency_triples[{selected_activity(trace.events[i - 2], options),
                                 selected_activity(trace.events[i - 1], options),
                                 selected_activity(trace.events[i], options)}];
    }
  }
  input.activities = std::move(activities);
  input.activity_counts = std::move(occurrences);
  input.start_counts = std::move(starts);
  input.end_counts = std::move(ends);
  // Event-log discovery supplies these fields as part of its returned
  // HeuristicsNet even when the log contains no length-two paths.
  input.dfg_window_2_supplied = true;
  input.freq_triples_supplied = true;
  return input;
}

HeuristicFrequencyDfg clean_classic_dfg(
    const HeuristicClassicDfgInput& input,
    const HeuristicMinerOptions& options,
    const std::vector<std::string>& activities) {
  if (options.dfg_pre_cleaning_noise_threshold <= 0.0) return input.dfg;
  std::map<std::string, std::uint64_t> maximum;
  for (const auto& activity : activities) maximum[activity] = 0;
  for (const auto& [pair, count] : input.dfg) {
    maximum[pair.first] = std::max(maximum[pair.first], count);
    maximum[pair.second] = std::max(maximum[pair.second], count);
  }
  HeuristicFrequencyDfg cleaned;
  for (const auto& [pair, count] : input.dfg) {
    const double boundary =
        std::min(maximum[pair.first] *
                     options.dfg_pre_cleaning_noise_threshold,
                 maximum[pair.second] *
                     options.dfg_pre_cleaning_noise_threshold);
    if (options.most_common_paths.contains(pair) || !(count < boundary))
      cleaned.emplace(pair, count);
  }
  return cleaned;
}

struct WaitSummary {
  std::vector<double> samples;
  double mean = 0.0;
  double median = 0.0;
  double minimum = 0.0;
  double maximum = 0.0;
  double stdev = 0.0;
  double sum = 0.0;
};

WaitSummary summarize_waits(const std::vector<double>& values) {
  WaitSummary result;
  result.samples = values;
  if (values.empty()) return result;
  result.sum = std::accumulate(values.begin(), values.end(), 0.0);
  result.mean = detail::exact_mean(values);
  result.minimum = *std::min_element(values.begin(), values.end());
  result.maximum = *std::max_element(values.begin(), values.end());
  auto sorted = values;
  std::sort(sorted.begin(), sorted.end());
  const std::size_t middle = sorted.size() / 2;
  result.median =
      sorted.size() % 2 == 0 ? (sorted[middle - 1] + sorted[middle]) / 2.0
                             : sorted[middle];
  result.stdev = detail::exact_stdev(values);
  return result;
}

double performance_representation(
    const WaitSummary& summary, HeuristicPerformanceAggregation aggregation) {
  switch (aggregation) {
    case HeuristicPerformanceAggregation::kMedian:
      return summary.median;
    case HeuristicPerformanceAggregation::kMinimum:
      return summary.minimum;
    case HeuristicPerformanceAggregation::kMaximum:
      return summary.maximum;
    case HeuristicPerformanceAggregation::kStandardDeviation:
      return summary.stdev;
    case HeuristicPerformanceAggregation::kSum:
      return summary.sum;
    case HeuristicPerformanceAggregation::kMean:
    case HeuristicPerformanceAggregation::kRawValues:
    case HeuristicPerformanceAggregation::kAll:
      // Raw/all remain lossless in relation statistics. A scalar mean keeps
      // existing consumers source-compatible.
      return summary.mean;
  }
  return summary.mean;
}

HeuristicPerformanceSummary performance_summary_value(const WaitSummary& summary) {
  return {summary.mean, summary.median, summary.minimum, summary.maximum,
          summary.stdev, summary.sum};
}

std::optional<HeuristicPerformanceValue> structured_performance_representation(
    const WaitSummary& summary, HeuristicNetDecoration decoration,
    HeuristicPerformanceAggregation aggregation) {
  // PM4Py only exposes a structured performance value when the relation has
  // at least one directly observed wait.  EFG-only relations and native
  // connect_all fallbacks retain their scalar compatibility projection.
  if (decoration != HeuristicNetDecoration::kPerformance ||
      summary.samples.empty())
    return std::nullopt;
  switch (aggregation) {
    case HeuristicPerformanceAggregation::kRawValues:
      return HeuristicPerformanceValue{summary.samples};
    case HeuristicPerformanceAggregation::kAll:
      return HeuristicPerformanceValue{performance_summary_value(summary)};
    case HeuristicPerformanceAggregation::kMean:
    case HeuristicPerformanceAggregation::kMedian:
    case HeuristicPerformanceAggregation::kMinimum:
    case HeuristicPerformanceAggregation::kMaximum:
    case HeuristicPerformanceAggregation::kStandardDeviation:
    case HeuristicPerformanceAggregation::kSum:
      return std::nullopt;
  }
  return std::nullopt;
}

// Already-aggregated DFG values retain their full typed payload in
// representation_value.  The legacy scalar field has no faithful projection
// for arrays, objects, strings, booleans, or integers, so those opaque values
// use the documented zero compatibility fallback. Root null instead requests
// the public frequency fallback handled below.
bool is_null_performance_value(const HeuristicPerformanceValue& value) {
  const auto* structured = std::get_if<HeuristicStructuredValue>(&value);
  return structured &&
         std::holds_alternative<std::nullptr_t>(structured->value);
}

HeuristicPerformanceValue decorated_representation(
    const HeuristicPerformanceValue& value, std::uint64_t frequency) {
  // An explicit root null is the public edge constructor's frequency
  // fallback. The original map still retains null; nested nulls stay opaque.
  if (is_null_performance_value(value))
    return HeuristicStructuredValue{frequency};
  return value;
}

double compatibility_representation(
    const HeuristicPerformanceValue& value, std::uint64_t frequency) {
  if (is_null_performance_value(value)) return static_cast<double>(frequency);
  if (const auto* scalar = std::get_if<double>(&value)) return *scalar;
  if (const auto* structured = std::get_if<HeuristicStructuredValue>(&value)) {
    if (const auto* scalar = std::get_if<double>(&structured->value))
      return *scalar;
  }
  return 0.0;
}

std::optional<HeuristicPerformanceValue> exact_frequency_representation(
    std::uint64_t frequency, HeuristicNetDecoration decoration) {
  // Binary64 represents every integer through 2^53 exactly.  Keep the
  // established scalar-only shape for those values, and retain a typed
  // integer payload when the compatibility projection could lose a unit.
  constexpr std::uint64_t kMaxExactBinary64Integer = std::uint64_t{1} << 53;
  if (decoration != HeuristicNetDecoration::kFrequency ||
      frequency <= kMaxExactBinary64Integer)
    return std::nullopt;
  return HeuristicPerformanceValue{
      HeuristicStructuredValue{HeuristicStructuredValue::Value{frequency}}};
}

std::uint64_t pair_count(const HeuristicFrequencyDfg& dfg,
                         const std::string& source,
                         const std::string& target) {
  const auto it = dfg.find({source, target});
  return it == dfg.end() ? 0 : it->second;
}

double classic_dependency(const HeuristicFrequencyDfg& dfg,
                          const std::string& source,
                          const std::string& target) {
  const std::uint64_t forward = pair_count(dfg, source, target);
  if (source == target)
    return detail::exact_ratio(
        detail::CountUint128::from_uint64(forward),
        detail::add_one(detail::CountUint128::from_uint64(forward)));
  const std::uint64_t reverse = pair_count(dfg, target, source);
  return detail::exact_signed_count_ratio(forward, reverse);
}

std::map<std::string, HeuristicOccurrenceCount> infer_activity_counts(
    const HeuristicFrequencyDfg& dfg,
    const std::vector<std::string>& activities) {
  std::map<std::string, HeuristicOccurrenceCount> result;
  for (const auto& activity : activities) {
    HeuristicOccurrenceCount incoming{0};
    HeuristicOccurrenceCount outgoing{0};
    bool has_incoming = false;
    bool has_outgoing = false;
    for (const auto& [pair, count] : dfg) {
      if (pair.first == activity) {
        outgoing += count;
        has_outgoing = true;
      }
      if (pair.second == activity) {
        incoming += count;
        has_incoming = true;
      }
    }
    auto total = incoming + outgoing;
    if (has_incoming && has_outgoing) total = total.rounded_half();
    result[activity] = total;
  }
  return result;
}

HeuristicsNet discover_classic_dfg(
    const HeuristicClassicDfgInput& input,
    const HeuristicMinerOptions& options) {
  if (options.variant == HeuristicMinerVariant::kPlusPlus)
    throw std::invalid_argument(
        "Heuristics Miner PLUSPLUS does not support DFG input");
  if (input.dfg_order) {
    const auto& order = *input.dfg_order;
    if (order.size() != input.dfg.size())
      throw std::invalid_argument(
          "Classic heuristic DFG order must contain every DFG path exactly "
          "once");
    std::set<NamePair> seen;
    for (const auto& pair : order) {
      if (!input.dfg.contains(pair) || !seen.insert(pair).second)
        throw std::invalid_argument(
            "Classic heuristic DFG order must contain every DFG path exactly "
            "once");
    }
  }
  // Direct DFG inputs have no event-log decoration pass to establish the
  // performance mode.  An already-aggregated map is itself an explicit
  // performance request; with neither performance map, retain the public
  // frequency default even when the caller left the option at performance.
  const auto effective_decoration =
      input.performance_values
          ? HeuristicNetDecoration::kPerformance
          : input.performance_dfg ? options.decoration
                                  : HeuristicNetDecoration::kFrequency;
  const bool use_structured_values = input.performance_values.has_value();
  const bool use_legacy_samples =
      !use_structured_values && input.performance_dfg.has_value();

  HeuristicsNet net;
  net.variant = HeuristicMinerVariant::kClassic;
  net.decoration = effective_decoration;
  net.performance_aggregation = options.performance_aggregation;
  net.activity_attribute = canonical_activity_attribute(options);
  net.timestamp_attribute = canonical_timestamp_attribute(options);
  net.raw_dfg = input.dfg;
  net.window_two_dfg = input.window_two_dfg;
  net.frequency_triples = input.frequency_triples;
  net.performance_values = input.performance_values;
  net.classic_public_dfg_window_2_present =
      input.dfg_window_2_supplied || !input.window_two_dfg.empty();
  net.classic_public_freq_triples_present =
      input.freq_triples_supplied || !input.frequency_triples.empty();
  net.sojourn_times.emplace();

  // Keep declared names separate from the internal id table.  A direct DFG
  // without an explicit declaration uses its endpoint names as declarations;
  // an explicit declaration (including an empty one) remains authoritative
  // for fallback-node creation and metadata ordering.
  std::vector<std::string> declared_activities;
  if (input.activities) {
    declared_activities = *input.activities;
  } else {
    std::set<std::string> inferred;
    for (const auto& [pair, _] : input.dfg) {
      inferred.insert(pair.first);
      inferred.insert(pair.second);
    }
    declared_activities.assign(inferred.begin(), inferred.end());
  }

  // The registry starts in declared order (preserving EventLog ordering),
  // then receives names referenced by graph statistics.  Occurrence-only and
  // boundary-only metadata keys remain name-based and do not become nodes.
  std::unordered_map<std::string, std::size_t> ids;
  const auto intern = [&](const std::string& name) {
    const auto [it, inserted] = ids.try_emplace(name, net.activities.size());
    if (inserted) net.activities.push_back(name);
    return it->second;
  };
  for (const auto& activity : declared_activities) intern(activity);
  for (const auto& [pair, _] : input.dfg) {
    intern(pair.first);
    intern(pair.second);
  }
  for (const auto& [pair, _] : input.window_two_dfg) {
    intern(pair.first);
    intern(pair.second);
  }
  for (const auto& [triple, _] : input.frequency_triples) {
    const auto& [first, middle, last] = triple;
    intern(first);
    intern(middle);
    intern(last);
  }
  if (input.performance_dfg && !input.performance_values) {
    for (const auto& [pair, _] : *input.performance_dfg) {
      intern(pair.first);
      intern(pair.second);
    }
  }
  if (input.performance_values) {
    for (const auto& [pair, _] : *input.performance_values) {
      intern(pair.first);
      intern(pair.second);
    }
  }
  // Positive noise cleaning follows the public endpoint validation: foreign
  // DFG names are accepted only when pre-cleaning is explicitly disabled.
  if (input.activities && options.dfg_pre_cleaning_noise_threshold > 0.0) {
    for (const auto& [pair, _] : input.dfg) {
      if (std::find(declared_activities.begin(), declared_activities.end(),
                    pair.first) == declared_activities.end() ||
          std::find(declared_activities.begin(), declared_activities.end(),
                    pair.second) == declared_activities.end())
        throw std::invalid_argument(
            "Classic heuristic DFG contains an activity missing from the "
            "declared activity list");
    }
  }

  std::map<std::string, HeuristicOccurrenceCount> occurrences;
  if (input.activity_counts) {
    for (const auto& [name, count] : *input.activity_counts)
      occurrences.emplace(name, count);
  } else {
    occurrences = infer_activity_counts(input.dfg, declared_activities);
  }
  std::map<std::string, std::uint64_t> starts;
  std::map<std::string, std::uint64_t> ends;
  HeuristicBoundaryActivities metadata_starts;
  HeuristicBoundaryActivities metadata_ends;
  const auto inferred_boundaries = [&]() {
    std::set<std::string> incoming;
    std::set<std::string> outgoing;
    for (const auto& [pair, _] : input.dfg) {
      outgoing.insert(pair.first);
      incoming.insert(pair.second);
    }
    std::vector<std::string> inferred_starts;
    std::vector<std::string> inferred_ends;
    std::set<std::string> seen_starts;
    std::set<std::string> seen_ends;
    const auto append_boundary = [&](const NamePair& pair) {
      if (!incoming.contains(pair.first) &&
          seen_starts.insert(pair.first).second)
        inferred_starts.push_back(pair.first);
      if (!outgoing.contains(pair.second) &&
          seen_ends.insert(pair.second).second)
        inferred_ends.push_back(pair.second);
    };
    if (input.dfg_order) {
      for (const auto& pair : *input.dfg_order) append_boundary(pair);
    } else {
      for (const auto& [pair, _] : input.dfg) append_boundary(pair);
    }
    return std::pair{std::move(inferred_starts), std::move(inferred_ends)};
  };
  const auto [inferred_starts, inferred_ends] = inferred_boundaries();
  const auto boundary_counts = [](const HeuristicBoundaryActivities& value) {
    return std::visit(
        [](const auto& selected) {
          using Value = std::decay_t<decltype(selected)>;
          std::map<std::string, std::uint64_t> result;
          if constexpr (std::is_same_v<Value, std::vector<std::string>>) {
            for (const auto& name : selected) result[name] = 1;
          } else {
            result = selected;
          }
          return result;
        },
        value);
  };
  if (input.start_activities) {
    metadata_starts = *input.start_activities;
    starts = boundary_counts(*input.start_activities);
  } else if (input.start_counts) {
    metadata_starts = *input.start_counts;
    starts = *input.start_counts;
  } else {
    metadata_starts = inferred_starts;
    for (const auto& activity : inferred_starts) starts[activity] = 1;
  }
  if (input.end_activities) {
    metadata_ends = *input.end_activities;
    ends = boundary_counts(*input.end_activities);
  } else if (input.end_counts) {
    metadata_ends = *input.end_counts;
    ends = *input.end_counts;
  } else {
    metadata_ends = inferred_ends;
    for (const auto& activity : inferred_ends) ends[activity] = 1;
  }

  net.cleaned_dfg = clean_classic_dfg(input, options, net.activities);
  if (input.performance_dfg && !input.performance_values) {
    for (const auto& [pair, _] : net.cleaned_dfg)
      if (!input.performance_dfg->contains(pair))
        throw std::invalid_argument(
            "Classic heuristic performance DFG is missing a cleaned path");
  }
  if (input.performance_values) {
    for (const auto& [pair, _] : net.cleaned_dfg)
      if (!input.performance_values->contains(pair))
        throw std::invalid_argument(
            "Classic heuristic decorated performance DFG is missing a "
            "cleaned path");
  }
  const double dependency_threshold = effective_dependency_threshold(options);
  std::vector<bool> active(net.activities.size(), false);
  std::set<NamePair> initially_accepted;
  std::map<NamePair, double> dependencies;
  for (const auto& [pair, count] : net.cleaned_dfg)
    dependencies[pair] =
        classic_dependency(net.cleaned_dfg, pair.first, pair.second);
  for (const auto& [pair, value] : dependencies) {
    const auto source = ids.find(pair.first);
    const auto target = ids.find(pair.second);
    if (source != ids.end() && target != ids.end())
      net.dependency_measures.push_back(
          {source->second, target->second, value});
  }

  const auto activity_count = [&](const std::string& name) {
    const auto it = occurrences.find(name);
    return it == occurrences.end() ? HeuristicOccurrenceCount{0} : it->second;
  };
  const auto required_activity_count = [&](const std::string& name) {
    return occurrences.at(name);
  };
  const auto meets_required_activity_count = [&](const std::string& name) {
    const auto it = occurrences.find(name);
    return it != occurrences.end() &&
           it->second >= options.min_activity_count;
  };
  const auto accepted_by_threshold = [&](const NamePair& pair) {
    const auto dep = dependencies.find(pair);
    // Keep the public short-circuit order: rejected dependencies and
    // below-frequency paths must not touch occurrence metadata.  Once both
    // graph gates pass, source membership/count is checked before target.
    if (dep == dependencies.end() ||
        !(dep->second >= dependency_threshold))
      return false;
    const auto count = pair_count(net.cleaned_dfg, pair.first, pair.second);
    if (count < options.min_frequency) return false;
    const auto source_count = occurrences.find(pair.first);
    // Missing occurrence entries reject this candidate without throwing;
    // the declared fallback node path below still validates counts strictly.
    if (source_count == occurrences.end()) return false;
    if (source_count->second < options.min_activity_count)
      return false;
    const auto target_count = occurrences.find(pair.second);
    if (target_count == occurrences.end()) return false;
    return target_count->second >= options.min_activity_count;
  };
  for (const auto& [pair, count] : net.cleaned_dfg) {
    if (!accepted_by_threshold(pair)) continue;
    initially_accepted.insert(pair);
    active[ids.at(pair.first)] = active[ids.at(pair.second)] = true;
    const HeuristicPerformanceValue* decorated =
        use_structured_values ? &input.performance_values->at(pair) : nullptr;
    const auto waits = use_legacy_samples
                           ? input.performance_dfg->find(pair)
                           : HeuristicPerformanceDfg::const_iterator{};
    const WaitSummary summary =
        summarize_waits(!use_legacy_samples ||
                                waits == input.performance_dfg->end()
                            ? std::vector<double>{}
                            : waits->second);
    const double representation =
        decorated != nullptr
            ? compatibility_representation(*decorated, count)
            : effective_decoration == HeuristicNetDecoration::kFrequency
                  ? static_cast<double>(count)
                  : performance_representation(summary,
                                               options.performance_aggregation);
    HeuristicDependency edge{ids.at(pair.first), ids.at(pair.second),
                             dependencies.at(pair), count, representation,
                             false};
    if (decorated != nullptr)
      edge.representation_value = decorated_representation(*decorated, count);
    else if (!use_structured_values &&
             effective_decoration == HeuristicNetDecoration::kFrequency)
      edge.representation_value = exact_frequency_representation(
          count, effective_decoration);
    else
      edge.representation_value = structured_performance_representation(
          summary, effective_decoration, options.performance_aggregation);
    net.edges.push_back(std::move(edge));
  }

  // Optional compatibility extension, disabled by default.
  if (options.connect_all) {
    for (std::size_t source = 0; source < net.activities.size(); ++source) {
      bool has_out = false;
      for (const auto& edge : net.edges) has_out |= edge.source == source;
      if (has_out || ends.contains(net.activities[source])) continue;
      std::optional<NamePair> best;
      double best_dep = 0.0;
      for (const auto& [pair, dep] : dependencies) {
        if (pair.first != net.activities[source] || dep <= 0.0 ||
            pair_count(net.cleaned_dfg, pair.first, pair.second) <
                options.min_frequency ||
            activity_count(pair.first) < options.min_activity_count ||
            activity_count(pair.second) < options.min_activity_count)
          continue;
        if (!best || dep > best_dep) {
          best = pair;
          best_dep = dep;
        }
      }
      if (best && !initially_accepted.contains(*best)) {
        const auto count =
            pair_count(net.cleaned_dfg, best->first, best->second);
        HeuristicDependency edge{ids.at(best->first), ids.at(best->second),
                                 best_dep, count,
                                 static_cast<double>(count), false};
        edge.representation_value = exact_frequency_representation(
            count, effective_decoration);
        net.edges.push_back(std::move(edge));
        initially_accepted.insert(*best);
        active[ids.at(best->first)] = active[ids.at(best->second)] = true;
      }
    }
  }

  // Record AND pairs now; discovery does not recompute them after loop-two rescue.
  using AndKey = std::tuple<std::size_t, std::size_t, std::size_t>;
  std::set<AndKey> and_out;
  std::set<AndKey> and_in;
  for (std::size_t center = 0; center < net.activities.size(); ++center) {
    if (!active[center]) continue;
    std::vector<std::size_t> successors;
    std::vector<std::size_t> predecessors;
    for (const auto& edge : net.edges) {
      if (edge.source == center) successors.push_back(edge.target);
      if (edge.target == center) predecessors.push_back(edge.source);
    }
    const auto by_name = [&](std::size_t lhs, std::size_t rhs) {
      return net.activities[lhs] < net.activities[rhs];
    };
    std::sort(successors.begin(), successors.end(), by_name);
    std::sort(predecessors.begin(), predecessors.end(), by_name);
    const auto calculate = [&](const std::vector<std::size_t>& neighbours,
                               bool outgoing, std::set<AndKey>& destination) {
      for (std::size_t i = 0; i < neighbours.size(); ++i) {
        for (std::size_t j = i + 1; j < neighbours.size(); ++j) {
          const auto& first = net.activities[neighbours[i]];
          const auto& second = net.activities[neighbours[j]];
          const auto& middle = net.activities[center];
          const auto numerator = detail::add(
              pair_count(net.cleaned_dfg, first, second),
              pair_count(net.cleaned_dfg, second, first));
          const auto base = outgoing
                                ? detail::add(
                                      pair_count(net.cleaned_dfg, middle, first),
                                      pair_count(net.cleaned_dfg, middle, second))
                                : detail::add(
                                      pair_count(net.cleaned_dfg, first, middle),
                                      pair_count(net.cleaned_dfg, second, middle));
          const double value =
              detail::exact_ratio(numerator, detail::add_one(base));
          if (value >= options.and_threshold) {
            destination.insert({center, neighbours[i], neighbours[j]});
            auto& measures =
                outgoing ? net.and_measures_out : net.and_measures_in;
            measures.push_back(
                {center, neighbours[i], neighbours[j], value});
          }
        }
      }
    };
    calculate(successors, true, and_out);
    calculate(predecessors, false, and_in);
  }

  // Frequency triples are deliberately not deduplicated by keep-once.
  std::map<NamePair, std::uint64_t> loop_triples;
  for (const auto& [triple, count] : input.frequency_triples) {
    const auto& [first, middle, last] = triple;
    if (first == last && first != middle)
      loop_triples[{first, middle}] += count;
  }
  const auto nodes_before_loop = active;
  std::set<NamePair> added_loop_edges;
  for (std::size_t first_id = 0; first_id < net.activities.size();
       ++first_id) {
    if (!nodes_before_loop[first_id]) continue;
    const auto& first = net.activities[first_id];
    for (const auto& [pair, v1] : loop_triples) {
      if (pair.first != first) continue;
      const auto& second = pair.second;
      if (!ids.contains(second)) continue;
      const auto reverse = loop_triples.find({second, first});
      const std::uint64_t v2 =
          reverse == loop_triples.end() ? 0 : reverse->second;
      const auto loop_count = detail::add(v1, v2);
      const double l2 = detail::exact_ratio(loop_count,
                                            detail::add_one(loop_count));
      if (l2 < options.loop_length_two_threshold) continue;
      net.loop_length_two.push_back(
          {first_id, ids.at(second), pair_count(net.cleaned_dfg, first,
                                                second)});
      const NamePair forward{first, second};
      const NamePair backward{second, first};
      // A foreign triple is observable as loop metadata, but it only rescues
      // an edge when the cleaned DFG contains the forward path itself.  This
      // presence guard is distinct from min_frequency so a zero threshold
      // cannot manufacture zero-frequency edges from triple-only names.
      if (!net.cleaned_dfg.contains(forward) ||
          pair_count(net.cleaned_dfg, first, second) < options.min_frequency)
        continue;
      // Loop rescue treats occurrence membership as a graph gate: a missing
      // source or target count skips rescue, while an explicitly supplied
      // zero count is valid when the threshold is zero.  The strict lookup
      // remains reserved for the declared-node fallback below.
      if (!meets_required_activity_count(first) ||
          !meets_required_activity_count(second))
        continue;
      const bool forward_dep =
          dependencies.contains(forward) &&
          dependencies.at(forward) >= dependency_threshold;
      const bool backward_dep =
          dependencies.contains(backward) &&
          dependencies.at(backward) >= dependency_threshold;
      if (forward_dep || backward_dep) continue;
      active[ids.at(second)] = true;
      for (const auto& direction : {forward, backward}) {
        if (!added_loop_edges.insert(direction).second) continue;
        const auto frequency =
            pair_count(net.cleaned_dfg, direction.first, direction.second);
        const bool cleaned_direction = net.cleaned_dfg.contains(direction);
        const HeuristicPerformanceValue* decorated =
            use_structured_values && cleaned_direction
                ? &input.performance_values->at(direction)
                : nullptr;
        const auto waits = use_legacy_samples
                ? input.performance_dfg->find(direction)
                : HeuristicPerformanceDfg::const_iterator{};
        const WaitSummary summary =
            summarize_waits(!use_legacy_samples ||
                                    waits == input.performance_dfg->end()
                                ? std::vector<double>{}
                                : waits->second);
        const double representation =
            decorated != nullptr
                ? compatibility_representation(*decorated, frequency)
                : effective_decoration == HeuristicNetDecoration::kFrequency
                      ? static_cast<double>(frequency)
                      : performance_representation(
                            summary, options.performance_aggregation);
        HeuristicDependency edge{ids.at(direction.first), ids.at(direction.second),
                                 0.0, frequency, representation, true};
        if (decorated != nullptr)
          edge.representation_value =
              decorated_representation(*decorated, frequency);
        else if (!use_structured_values &&
                 effective_decoration == HeuristicNetDecoration::kFrequency)
          edge.representation_value = exact_frequency_representation(
              frequency, effective_decoration);
        else if (use_structured_values)
          edge.representation_value.reset();
        else
          edge.representation_value = structured_performance_representation(
              summary, effective_decoration, options.performance_aggregation);
        net.edges.push_back(std::move(edge));
      }
    }
  }

  if (std::none_of(active.begin(), active.end(), [](bool value) {
        return value;
      })) {
    // Fallback nodes come only from the declared activity list.  Validate
    // those counts strictly at the point where the public implementation
    // consults fallback nodes; inactive declared names may remain absent from
    // the raw metadata map when an edge already made nodes active.
    for (const auto& name : declared_activities) {
      (void)required_activity_count(name);
      active[ids.at(name)] = true;
    }
  }
  for (std::size_t id = 0; id < active.size(); ++id)
    if (active[id]) net.active_activities.push_back(id);
  for (const auto id : net.active_activities) {
    if (starts.contains(net.activities[id])) net.start_activities.push_back(id);
    if (ends.contains(net.activities[id])) net.end_activities.push_back(id);
  }

  net.output_bundles.assign(net.activities.size(), {});
  net.input_bundles.assign(net.activities.size(), {});
  for (const auto center : net.active_activities) {
    std::vector<std::size_t> successors;
    std::vector<std::size_t> predecessors;
    for (const auto& edge : net.edges) {
      if (edge.source == center) successors.push_back(edge.target);
      if (edge.target == center) predecessors.push_back(edge.source);
    }
    const auto by_name = [&](std::size_t lhs, std::size_t rhs) {
      return net.activities[lhs] < net.activities[rhs];
    };
    std::sort(successors.begin(), successors.end(), by_name);
    successors.erase(std::unique(successors.begin(), successors.end()),
                     successors.end());
    std::sort(predecessors.begin(), predecessors.end(), by_name);
    predecessors.erase(std::unique(predecessors.begin(), predecessors.end()),
                       predecessors.end());
    const auto bundle = [&](const std::vector<std::size_t>& neighbours,
                            const std::set<AndKey>& relations) {
      std::vector<std::vector<bool>> adjacent(
          neighbours.size(), std::vector<bool>(neighbours.size(), false));
      for (std::size_t i = 0; i < neighbours.size(); ++i)
        for (std::size_t j = i + 1; j < neighbours.size(); ++j)
          adjacent[i][j] = adjacent[j][i] =
              relations.contains({center, neighbours[i], neighbours[j]});
      return maximal_cliques(neighbours, adjacent);
    };
    net.output_bundles[center] = bundle(successors, and_out);
    net.input_bundles[center] = bundle(predecessors, and_in);
    sort_bundles_by_activity_name(net.output_bundles[center],
                                  net.activities);
    sort_bundles_by_activity_name(net.input_bundles[center],
                                  net.activities);
  }

  for (std::size_t id = 0; id < net.activities.size(); ++id) {
    const auto& name = net.activities[id];
    HeuristicActivityStatistics statistic{
        id, activity_count(name),
        starts.contains(name) ? starts.at(name) : 0,
        ends.contains(name) ? ends.at(name) : 0, 0.0};
    // These are the public CLASSIC node attributes, separate from the
    // boundary membership/counts retained above and in net metadata.
    statistic.is_start_activity = false;
    statistic.is_end_activity = false;
    net.activity_statistics.push_back(std::move(statistic));
  }
  std::set<NamePair> statistic_pairs;
  for (const auto& [pair, _] : input.dfg) statistic_pairs.insert(pair);
  for (const auto& edge : net.edges)
    statistic_pairs.insert(
        {net.activities[edge.source], net.activities[edge.target]});
  for (const auto& pair : statistic_pairs) {
    if (!ids.contains(pair.first) || !ids.contains(pair.second)) continue;
    const auto raw_count = pair_count(input.dfg, pair.first, pair.second);
    const auto cleaned = pair_count(net.cleaned_dfg, pair.first, pair.second);
    const HeuristicPerformanceValue* decorated = nullptr;
    if (input.performance_values && net.cleaned_dfg.contains(pair)) {
      const auto value = input.performance_values->find(pair);
      if (value != input.performance_values->end()) decorated = &value->second;
    }
    const auto waits = use_legacy_samples
                           ? input.performance_dfg->find(pair)
                           : HeuristicPerformanceDfg::const_iterator{};
    const WaitSummary summary =
        summarize_waits(!use_legacy_samples ||
                                waits == input.performance_dfg->end()
                            ? std::vector<double>{}
                            : waits->second);
    const auto edge_it = std::find_if(
        net.edges.begin(), net.edges.end(), [&](const auto& edge) {
          return edge.source == ids.at(pair.first) &&
                 edge.target == ids.at(pair.second);
        });
    const double representation =
        decorated != nullptr
            ? compatibility_representation(*decorated, cleaned)
            : effective_decoration == HeuristicNetDecoration::kFrequency
                  ? static_cast<double>(cleaned)
                  : performance_representation(summary,
                                               options.performance_aggregation);
    HeuristicRelationStatistics relation{
        ids.at(pair.first), ids.at(pair.second), cleaned,
        static_cast<std::uint64_t>(summary.samples.size()), summary.mean,
        raw_count, cleaned, representation,
        edge_it != net.edges.end() && edge_it->length_two_loop,
        summary.samples, summary.median, summary.minimum, summary.maximum,
        summary.stdev, summary.sum};
    if (decorated != nullptr)
      relation.representation_value =
          decorated_representation(*decorated, cleaned);
    else if (!use_structured_values &&
             effective_decoration == HeuristicNetDecoration::kFrequency)
      relation.representation_value = exact_frequency_representation(
          cleaned, effective_decoration);
    else if (use_structured_values)
      relation.representation_value.reset();
    else
      relation.representation_value = structured_performance_representation(
          summary, effective_decoration, options.performance_aggregation);
    net.relation_statistics.push_back(std::move(relation));
  }
  std::sort(net.edges.begin(), net.edges.end(),
            [](const auto& lhs, const auto& rhs) {
              return std::tie(lhs.source, lhs.target) <
                     std::tie(rhs.source, rhs.target);
            });
  HeuristicNetMetadata metadata;
  metadata.activities = input.activities ? *input.activities : declared_activities;
  metadata.activities_occurrences = occurrences;
  metadata.start_activities = std::move(metadata_starts);
  metadata.end_activities = std::move(metadata_ends);
  net.metadata = std::move(metadata);
  return net;
}

HeuristicsNet discover_plus_plus(const EventLog& log,
                                 const HeuristicMinerOptions& options,
                                 bool pandas_route = false) {
  const HeuristicNetDecoration effective_decoration =
      options.plusplus_decoration_selector
          ? (*options.plusplus_decoration_selector == "frequency"
                 ? HeuristicNetDecoration::kFrequency
                 : HeuristicNetDecoration::kPerformance)
          : options.decoration;
  const PlusAbstraction abstraction =
      build_plus_abstraction(log, options, pandas_route);
  HeuristicsNet net;
  net.variant = HeuristicMinerVariant::kPlusPlus;
  net.decoration = effective_decoration;
  net.performance_aggregation = options.performance_aggregation;
  net.activity_attribute = canonical_activity_attribute(options);
  net.timestamp_attribute = canonical_timestamp_attribute(options);
  net.activity_ids_are_local =
      !standard_activity_attribute(options.activity_attribute);

  net.plusplus_public_dfg_window_2_present = false;
  net.plusplus_public_freq_triples_present = false;
  if (effective_decoration == HeuristicNetDecoration::kPerformance)
    net.plusplus_computed_performance_dfg.emplace();

  std::unordered_map<std::string, std::size_t> ids;
  for (const auto& activity : abstraction.activity_order) {
    if (abstraction.occurrences.at(activity) < options.min_activity_count)
      continue;
    ids.emplace(activity, net.activities.size());
    net.activities.push_back(activity);
  }
  const std::size_t n = net.activities.size();
  net.active_activities.resize(n);
  std::iota(net.active_activities.begin(), net.active_activities.end(), 0);
  net.output_bundles.assign(n, {});
  net.input_bundles.assign(n, {});
  for (std::size_t id = 0; id < n; ++id) {
    const auto& activity = net.activities[id];
    const auto start = abstraction.starts.find(activity);
    const auto end = abstraction.ends.find(activity);
    if (start != abstraction.starts.end()) net.start_activities.push_back(id);
    if (end != abstraction.ends.end()) net.end_activities.push_back(id);
    net.activity_statistics.push_back(
        {id, abstraction.occurrences.at(activity),
         start == abstraction.starts.end() ? 0 : start->second,
         end == abstraction.ends.end() ? 0 : end->second,
         abstraction.sojourn_sums.at(activity) /
             static_cast<double>(abstraction.occurrences.at(activity))});
    net.activity_statistics.back().is_start_activity = false;
    net.activity_statistics.back().is_end_activity = false;
  }
  // Discovery keeps sojourn data before activity filtering. Preserve every activity
  // and attach an id only when it survived the node filter.
  net.sojourn_times.emplace();
  for (const auto& activity : abstraction.activity_order) {
    const double mean_sojourn =
        abstraction.sojourn_sums.at(activity) /
        static_cast<double>(abstraction.occurrences.at(activity));
    net.raw_activity_statistics.push_back(
        {activity, abstraction.occurrences.at(activity), mean_sojourn});
    net.sojourn_times->emplace(activity, mean_sojourn);
  }

  for (const auto& [pair, frequency] : abstraction.eventually_follows) {
    const auto source = ids.find(pair.first);
    const auto target = ids.find(pair.second);
    if (source == ids.end() || target == ids.end() ||
        frequency < options.min_frequency)
      continue;
    const auto direct = abstraction.direct_adjacency.find(pair);
    const std::uint64_t direct_frequency =
        direct == abstraction.direct_adjacency.end() ? 0 : direct->second;
    const auto samples = abstraction.wait_samples.find(pair);
    const WaitSummary summary =
        summarize_waits(samples == abstraction.wait_samples.end()
                            ? std::vector<double>{}
                            : samples->second);
    const double mean_wait =
        direct_frequency == 0 ? 0.0 : summary.mean;
    // These public mean maps follow the retained EFG domain, independently of
    // edge acceptance and the edge's chosen performance aggregation.
    net.plusplus_computed_performance_matrix.emplace(
        pair, effective_decoration == HeuristicNetDecoration::kFrequency
                  ? 0.0
                  : mean_wait);
    if (net.plusplus_computed_performance_dfg && direct_frequency > 0)
      net.plusplus_computed_performance_dfg->emplace(pair, mean_wait);
    const double representation =
        effective_decoration == HeuristicNetDecoration::kFrequency
            ? static_cast<double>(frequency)
            : performance_representation(summary,
                                         options.performance_aggregation);
    HeuristicRelationStatistics relation{
        source->second, target->second, frequency, direct_frequency, mean_wait,
        frequency, frequency, representation, false, summary.samples,
        summary.median, summary.minimum, summary.maximum, summary.stdev,
        summary.sum};
    // PLUSPLUS frequency values retain their integer type even when the count
    // fits exactly in the legacy double compatibility projection.
    const std::optional<HeuristicPerformanceValue> representation_value =
        effective_decoration == HeuristicNetDecoration::kFrequency
            ? HeuristicPerformanceValue{HeuristicStructuredValue{frequency}}
            : structured_performance_representation(
                  summary, effective_decoration, options.performance_aggregation);
    relation.representation_value = representation_value;
    net.relation_statistics.push_back(std::move(relation));
    const double dependency =
        plus_dependency(abstraction, pair.first, pair.second,
                        options.min_frequency);
    net.dependency_measures.push_back(
        {source->second, target->second, dependency});
    if (dependency > effective_dependency_threshold(options))
    {
      HeuristicDependency edge{source->second, target->second, dependency,
                               frequency, representation, false};
      edge.representation_value = representation_value;
      net.edges.push_back(std::move(edge));
    }
  }
  for (const auto& [pair, frequency] : abstraction.concurrency) {
    const auto first = ids.find(pair.first);
    const auto second = ids.find(pair.second);
    net.raw_concurrency_statistics.push_back(
        {pair.first, pair.second, frequency});
    if (first != ids.end() && second != ids.end())
      net.concurrency_statistics.push_back(
          {first->second, second->second, frequency});
  }
  std::sort(net.edges.begin(), net.edges.end(),
            [](const auto& lhs, const auto& rhs) {
              return std::tie(lhs.source, lhs.target) <
                     std::tie(rhs.source, rhs.target);
            });

  std::vector<std::vector<std::size_t>> successors(n);
  std::vector<std::vector<std::size_t>> predecessors(n);
  for (const auto& edge : net.edges) {
    successors[edge.source].push_back(edge.target);
    predecessors[edge.target].push_back(edge.source);
  }
  for (std::size_t activity = 0; activity < n; ++activity) {
    const auto by_name = [&](std::size_t lhs, std::size_t rhs) {
      return net.activities[lhs] < net.activities[rhs];
    };
    std::sort(successors[activity].begin(), successors[activity].end(),
              by_name);
    std::sort(predecessors[activity].begin(), predecessors[activity].end(),
              by_name);
    net.output_bundles[activity] =
        plus_bundles(abstraction, net, activity, successors[activity], true,
                     options.and_threshold, options.min_frequency);
    net.input_bundles[activity] =
        plus_bundles(abstraction, net, activity, predecessors[activity], false,
                     options.and_threshold, options.min_frequency);
    sort_bundles_by_activity_name(net.output_bundles[activity],
                                  net.activities);
    sort_bundles_by_activity_name(net.input_bundles[activity],
                                  net.activities);
  }
  HeuristicNetMetadata metadata;
  metadata.activities = net.activities;
  metadata.activities_occurrences = {};
  metadata.start_activities = std::map<std::string, std::uint64_t>{};
  metadata.end_activities = std::map<std::string, std::uint64_t>{};
  // PLUSPLUS exposes the retained activity table and filtered occurrence /
  // boundary dictionaries.  Low-count raw activities stay in the raw
  // statistics vectors but do not leak into this public metadata object.
  for (const auto& stat : net.activity_statistics) {
    const auto& name = net.activities[stat.activity];
    metadata.activities_occurrences[name] = stat.occurrences;
    if (stat.start_occurrences > 0)
      std::get<std::map<std::string, std::uint64_t>>(
          metadata.start_activities)[name] = stat.start_occurrences;
    if (stat.end_occurrences > 0)
      std::get<std::map<std::string, std::uint64_t>>(
          metadata.end_activities)[name] = stat.end_occurrences;
  }
  net.metadata = std::move(metadata);
  return net;
}

}  // namespace

HeuristicsNet discover_plus_plus_pandas(
    EventTable& data, const HeuristicMinerOptions& options) {
  data.validate();
  auto selected = options;
  selected.variant = HeuristicMinerVariant::kPlusPlus;
  selected.start_timestamp_attribute =
      options.start_timestamp_attribute.value_or("start_timestamp");
  const auto column = [&](const std::string& key) {
    const auto found = std::find(data.columns.begin(), data.columns.end(), key);
    if (found == data.columns.end())
      throw HeuristicPandasError("builtins.KeyError", "'" + key + "'");
    return static_cast<std::size_t>(found - data.columns.begin());
  };
  const auto completed = column(canonical_timestamp_attribute(selected));
  const auto validate_datetime = [&](std::size_t index) {
    const bool valid = !data.dtypes.empty()
        ? std::string_view(data.dtypes.at(index)).starts_with("datetime64[")
        : std::all_of(data.rows.begin(), data.rows.end(), [&](const auto& row) {
            return !row[index] || row[index]->is_null ||
                   row[index]->kind == AttributeKind::kDate;
          });
    if (!valid)
      throw HeuristicPandasError(
          "builtins.AttributeError",
          "Can only use .dt accessor with datetimelike values");
  };
  validate_datetime(completed);
  const auto append_column = [&](const std::string& key,
                                 const std::string& dtype) {
    const auto index = data.columns.size();
    data.columns.push_back(key);
    if (!data.dtypes.empty()) data.dtypes.push_back(dtype);
    if (!data.categorical_dtypes.empty())
      data.categorical_dtypes.push_back(std::nullopt);
    for (auto& row : data.rows) row.emplace_back();
    return index;
  };
  const auto start_key = *selected.start_timestamp_attribute;
  auto start = std::find(data.columns.begin(), data.columns.end(), start_key);
  std::size_t started;
  if (start == data.columns.end()) {
    const auto dtype = data.dtypes.empty() ? std::string{}
                                          : data.dtypes.at(completed);
    started = append_column(start_key, dtype);
    for (auto& row : data.rows) {
      row[started] = row[completed];
      if (row[started]) {
        row[started]->key = start_key;
        row[started]->first_class_role = FirstClassAttributeRole::kNone;
        row[started]->first_class_state = FirstClassAttributeState::kMissing;
        row[started]->first_class_source_key.clear();
      }
    }
  } else {
    started = static_cast<std::size_t>(start - data.columns.begin());
  }
  validate_datetime(started);
  std::vector<double> durations;
  durations.reserve(data.rows.size());
  for (const auto& row : data.rows) {
    const auto& end = row[completed];
    const auto& begin = row[started];
    if (!end || !begin || end->is_null || begin->is_null ||
        is_typed_nat(*end) || is_typed_nat(*begin)) {
      durations.push_back(std::numeric_limits<double>::quiet_NaN());
    } else {
      durations.push_back(detail::epoch_seconds_from_microseconds(
          required_timestamp_microseconds(end->value, "event") -
          required_timestamp_microseconds(begin->value, "start")));
    }
  }
  const auto existing = std::find(data.columns.begin(), data.columns.end(),
                                  "@@diff");
  const auto difference = existing == data.columns.end()
      ? append_column("@@diff", "float64")
      : static_cast<std::size_t>(existing - data.columns.begin());
  if (!data.dtypes.empty()) data.dtypes[difference] = "float64";
  if (!data.categorical_dtypes.empty())
    data.categorical_dtypes[difference] = std::nullopt;
  for (std::size_t i = 0; i < data.rows.size(); ++i) {
    EventAttribute value;
    value.key = "@@diff";
    value.kind = AttributeKind::kFloat;
    value.typed_value = durations[i];
    value.value = std::to_string(durations[i]);
    data.rows[i][difference] = std::move(value);
  }
  return discover_plus_plus(classic_event_log(EventDataView(data), selected),
                            selected, true);
}

HeuristicsNet discover_heuristics_net(
    const EventLog& log,
    const HeuristicMinerOptions& options) {
  if (options.variant == HeuristicMinerVariant::kPlusPlus)
    return discover_plus_plus(log, options);
  auto net = discover_classic_dfg(build_classic_input(log, options), options);
  net.activity_ids_are_local =
      !standard_activity_attribute(options.activity_attribute);
  return net;
}

HeuristicsNet discover_heuristics_net(
    const EventDataView& data,
    const HeuristicMinerOptions& options) {
  return discover_heuristics_net(classic_event_log(data, options), options);
}

HeuristicsNet discover_heuristics_net(
    const EventStream& data,
    const HeuristicMinerOptions& options) {
  return discover_heuristics_net(EventDataView(data), options);
}

HeuristicsNet discover_heuristics_net(
    const EventTable& data,
    const HeuristicMinerOptions& options) {
  return discover_heuristics_net(EventDataView(data), options);
}

HeuristicsNet discover_heuristics_net(
    const LazyEventTable& data,
    const HeuristicMinerOptions& options) {
  return discover_heuristics_net(EventDataView(data), options);
}

HeuristicsNet discover_heuristics_net(
    const HeuristicClassicDfgInput& input,
    const HeuristicMinerOptions& options) {
  auto net = discover_classic_dfg(input, options);
  // Direct DFG discovery has no external activity registry; ids are local to
  // the returned activity table even when names happen to be canonical.
  net.activity_ids_are_local = true;
  return net;
}

}  // namespace minera
