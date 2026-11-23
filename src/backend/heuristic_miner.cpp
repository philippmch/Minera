#include "minera/heuristic_miner.hpp"

#include "business_hours.hpp"
#include "exact_count_ratio.hpp"
#include "exact_mean.hpp"
#include "exact_stdev.hpp"
#include "frame_times.hpp"
#include "heuristic_plus_frame_intervals.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/frame_keys.hpp"
#include "minera/log_conversion.hpp"
#include "minera/timestamp.hpp"
#include "python_list_sort.hpp"
#include "python_scalar.hpp"
#include "root_cause_unicode.hpp"
#include "xes_frame_columns.hpp"
#include "xes_frame_typed.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
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
  // A CSV upload's exact instants, nanoseconds included (round FR).
  std::optional<ParsedExactTimestamp> start_exact;
  std::optional<ParsedExactTimestamp> complete_exact;
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
  // DataFrame means exclude missing durations while occurrence counts keep
  // those rows. Generic discovery retains its existing sum/count projection.
  std::map<std::string, double> sojourn_means;
  bool pandas_sojourn_statistics = false;
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
  // A table read as events keeps its activity column as the first-class
  // activity under that column's name (a CSV upload's table shapes; round
  // BN): the column still answers for its key, as in pm4py's frame.
  if (!event.activity_source_key.empty() && event.activity_source_key == options.activity_attribute)
    return event.activity;
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
  // The timestamp column likewise (see selected_activity).
  if (!event.timestamp_source_key.empty() && event.timestamp_source_key == options.timestamp_attribute)
    return event.timestamp;
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
  // The timestamp column under its own header (a CSV upload's) is a column
  // of the frame too (round FR, observed black-box).
  if (attribute == nullptr && !event.timestamp_source_key.empty() &&
      key == event.timestamp_source_key) {
    if (event.timestamp_state == FirstClassAttributeState::kNull)
      return {ClassicStartKind::kNull, nullptr};
    return {ClassicStartKind::kValue, &event.timestamp};
  }
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

// end - begin in whole microseconds, floored from the exact instants (their
// nanoseconds included).
std::int64_t floored_elapsed_microseconds(const ParsedExactTimestamp& last,
                                          const ParsedExactTimestamp& first) {
  const auto nanos = static_cast<std::int64_t>(last.nanoseconds) -
                     static_cast<std::int64_t>(first.nanoseconds);
  const auto fraction = nanos >= 0 ? nanos / 1'000 : -((-nanos + 999) / 1'000);
  return (last.seconds - first.seconds) * 1'000'000 + fraction;
}

// The same for two texts; `fallback` when either has no exact instant.
std::int64_t floored_elapsed_microseconds(const std::string& end, const std::string& begin,
                                          std::int64_t fallback) {
  const auto last = parse_timestamp_exact(end);
  const auto first = parse_timestamp_exact(begin);
  if (!last || !first) return fallback;
  return floored_elapsed_microseconds(*last, *first);
}

// A wait of `microseconds` in seconds as pm4py measures it on an upload's
// frame (round FR, observed black-box on CSV and XES uploads, the classic and
// plusplus alike): the whole seconds plus the microseconds / 1e6, so 4.999997
// seconds is 4.9999970000000005 there, where microseconds / 1e6 gives
// 4.999997.
double frame_elapsed_seconds(std::int64_t microseconds) {
  auto whole = microseconds / 1'000'000;
  auto remainder = microseconds % 1'000'000;
  if (remainder < 0) {
    --whole;
    remainder += 1'000'000;
  }
  return static_cast<double>(whole) + static_cast<double>(remainder) / 1e6;
}

// An instant's seconds for the plusplus sojourn times: on a CSV upload's
// frame pm4py subtracts Timestamp.timestamp() values (round(ns / 1e9, 6), round
// FR, observed black-box: 10.0000002 less 10.0000007 is -9.5367431640625e-07);
// else the microseconds' seconds.
double interval_seconds(const std::optional<ParsedExactTimestamp>& exact,
                        std::int64_t microseconds) {
  return exact ? frame_times::epoch_seconds(*exact)
               : detail::epoch_seconds_from_microseconds(microseconds);
}

// `utc_clock` (round FL): a CSV upload, whose frame holds UTC Timestamps (an
// offset is converted), so pm4py's BusinessHours reads the UTC clock there
// (observed black-box, fl t4/c1.csv); else the text's own wall clock.
std::int64_t required_business_timestamp_microseconds(
    std::string_view value, std::string_view description, bool utc_clock = false) {
  const auto local = utc_clock ? parse_timestamp_microseconds(value)
                               : detail::local_wall_timestamp_microseconds(value);
  if (!local)
    throw std::invalid_argument(
        "Heuristics Miner: invalid business-hours " + std::string(description) +
        " timestamp");
  return *local;
}

// An interval endpoint as an exact (seconds, nanoseconds) instant: a CSV
// upload's own nanoseconds where they were parsed, else its microseconds.
// pm4py orders, follows and overlaps an upload's intervals by its Timestamps,
// nanoseconds included (round FS, observed black-box).
std::pair<std::int64_t, std::int64_t> exact_instant(
    const std::optional<ParsedExactTimestamp>& exact, std::int64_t microseconds) {
  if (exact) return {exact->seconds, static_cast<std::int64_t>(exact->nanoseconds)};
  auto seconds = microseconds / 1'000'000;
  auto fraction = microseconds % 1'000'000;
  if (fraction < 0) {
    fraction += 1'000'000;
    --seconds;
  }
  return {seconds, fraction * 1'000};
}

std::pair<std::int64_t, std::int64_t> start_instant(const Interval& interval) {
  return exact_instant(interval.start_exact, interval.start_microseconds);
}

std::pair<std::int64_t, std::int64_t> complete_instant(const Interval& interval) {
  return exact_instant(interval.complete_exact, interval.complete_microseconds);
}

struct IntervalTrace {
  std::vector<Interval> sorted;
  std::vector<Interval> performance_order;
};

// How the lifecycle pairing compares two instance values (round GF, observed
// black-box): pm4py keys a start by its activity and instance as Python
// values, so 2 pairs with 2.0 and True with 1 but never with '2'. A missing
// instance pairs only with another missing one on a log; NaN never pairs
// there (each parsed NaN is its own object). An XES upload's log keeps the
// log's rule although it carries its frame's columns: the log shape follows
// the legacy log (round GG).
//
// On a pandas frame the column's dtype decides (round GG, observed
// black-box): a float64 column (floats, ints beside floats or a gap) holds
// each NaN as its own float, so a missing or NaN instance pairs with
// nothing (kPythonFrameFloat); a str column holds pandas' one NaN for both,
// which pairs with itself (kPythonFrame); any other (object) column holds
// that NaN for a gap but keeps each parsed NaN float its own, as on a log
// (kPythonLog). Polars frames hold the column's one dtype and keep the text
// comparison, a NaN null as pl.from_pandas makes it (kText).
enum class InstanceKeyMode { kText, kPythonLog, kPythonFrame, kPythonFrameFloat };

bool nan_instance(const EventAttribute& cell) {
  if (cell.container != AttributeContainerKind::kScalar) return false;
  const auto scalar = python::from_attribute(cell);
  return scalar.kind == python::Scalar::Kind::kFloat && std::isnan(scalar.real);
}

std::string lifecycle_instance_key(const EventAttribute* cell, InstanceKeyMode mode, std::size_t position) {
  if (mode == InstanceKeyMode::kText)
    return cell && !cell->is_null && !nan_instance(*cell) ? cell->value : std::string();
  const auto own = [&] { return "Fnan#" + std::to_string(position); };
  if (!cell || cell->is_null) return mode == InstanceKeyMode::kPythonFrameFloat ? own() : std::string("N");
  if (cell->container != AttributeContainerKind::kScalar) return "S" + cell->value;
  const auto scalar = python::from_attribute(*cell);
  if (scalar.kind == python::Scalar::Kind::kFloat && std::isnan(scalar.real))
    return mode == InstanceKeyMode::kPythonFrame ? std::string("N") : own();
  return python::identity(scalar);
}

// The pandas mode of an instance column from its cells, as pandas types
// them (a missing or NaN cell a gap; see typed_columns in
// dataframe_columns.cpp).
InstanceKeyMode pandas_instance_mode(const std::vector<const EventAttribute*>& cells) {
  bool gap = false, integer = false, real = false, text = false, date = false, other = false;
  for (const auto* cell : cells) {
    if (!cell || cell->is_null || nan_instance(*cell)) {
      gap = true;
      continue;
    }
    if (cell->container != AttributeContainerKind::kScalar) {
      other = true;
      continue;
    }
    switch (python::from_attribute(*cell).kind) {
      case python::Scalar::Kind::kInt: integer = true; break;
      case python::Scalar::Kind::kFloat: real = true; break;
      case python::Scalar::Kind::kText: text = true; break;
      case python::Scalar::Kind::kDate: date = true; break;
      default: other = true; break;
    }
  }
  if (!text && !date && !other && (real || (integer && gap) || !integer))
    return InstanceKeyMode::kPythonFrameFloat;
  // A str column's gaps are its one NaN, a datetime column's pandas' NaT.
  if (!integer && !real && !other && text != date) return InstanceKeyMode::kPythonFrame;
  return InstanceKeyMode::kPythonLog;
}

// The mode of a CSV log's instance column (its frame's dtype).
InstanceKeyMode csv_instance_mode(const DataFrameColumns& frame, const std::string& key) {
  for (const auto& column : frame.columns)
    if (column.key == key) {
      if (column.type == DataFrameColumnType::kFloat64) return InstanceKeyMode::kPythonFrameFloat;
      if (column.type == DataFrameColumnType::kString) return InstanceKeyMode::kPythonFrame;
      return InstanceKeyMode::kPythonLog;
    }
  return InstanceKeyMode::kPythonFrame;
}

const EventAttribute* event_attribute_cell(const Event& event, std::string_view key) {
  for (const auto& entry : event.attributes)
    if (entry.key == key) return &entry;
  return nullptr;
}

// The text of a CSV event's lifecycle:transition cell, or nullptr when the
// cell is missing or the frame has no such column.
const std::string* csv_transition_text(const Event& event) {
  if (event.lifecycle_source_key == "lifecycle:transition")
    return event.lifecycle_state == FirstClassAttributeState::kPresent && !event.lifecycle.empty()
               ? &event.lifecycle
               : nullptr;
  const auto* cell = event_attribute_cell(event, "lifecycle:transition");
  return cell && !cell->is_null && !cell->value.empty() ? &cell->value : nullptr;
}

std::string python_str_repr(const std::string& text) {
  const bool double_quote = text.find('\'') != std::string::npos && text.find('"') == std::string::npos;
  const char quote = double_quote ? '"' : '\'';
  std::string out(1, quote);
  for (const char ch : text) {
    if (ch == '\\' || ch == quote) out += '\\';
    out += ch;
  }
  return out + quote;
}

// How the events of a CSV upload's log (pm4py's convert_to_event_log of
// format_dataframe's frame, or that frame and its Polars copy) hold `key`
// (rounds GH-GK, observed black-box): every event holds every event column
// of the frame, typed by the column (a date column Timestamp or NaT, on
// Polars datetime.datetime or None; a str column str, a pandas NA word or a
// gap NaN (None); a float64 column float or NaN (None); an int64, bool or
// object column int or bool; Polars' copy of an object column str). The
// copies concept:name and time:timestamp, the activity and timestamp
// columns under their headers, a case column under a header without the
// `case:` prefix, the resource and lifecycle columns are event columns too;
// a `case:` key is a trace attribute. Returns nullopt when no event holds
// the key; the reader gives nullopt for a cell it does not model.
using CsvCellReader = std::function<std::optional<detail::PlusCell>(const Trace&, const Event&)>;

std::optional<detail::PlusCell> csv_typed_cell(const DataFrameColumn& column, const std::string* text, bool polars) {
  using detail::PlusCell;
  using detail::PlusCellKind;
  PlusCell cell;
  const auto type = column.type;
  if (!text || text->empty() || (type == DataFrameColumnType::kString && is_pandas_csv_na_text(*text))) {
    cell.kind = polars ? PlusCellKind::kNone
                       : type == DataFrameColumnType::kDate ? PlusCellKind::kNaT : PlusCellKind::kNaN;
    return cell;
  }
  switch (type) {
    case DataFrameColumnType::kString:
      cell.kind = PlusCellKind::kText;
      cell.text = *text;
      return cell;
    case DataFrameColumnType::kDate: {
      const auto utc = csv_utc_date_cell(column.key, *text);
      const auto micros = utc ? parse_timestamp_microseconds(utc->value) : std::nullopt;
      if (!micros) return std::nullopt;
      cell.kind = polars ? PlusCellKind::kDatetime : PlusCellKind::kTimestamp;
      cell.micros = *micros;
      if (const auto exact = parse_timestamp_exact(utc->value)) cell.nanos = static_cast<std::int32_t>(exact->nanoseconds % 1000);
      return cell;
    }
    default: break;
  }
  const auto typed = csv_dataframe_cell(column, *text);
  if (type == DataFrameColumnType::kObject && polars) {
    cell.kind = PlusCellKind::kText;
    cell.text = typed.value;
    return cell;
  }
  if (const auto* value = std::get_if<std::int64_t>(&typed.typed_value)) {
    cell.kind = PlusCellKind::kInt;
    cell.micros = *value;
  } else if (std::holds_alternative<std::uint64_t>(typed.typed_value) ||
             (type == DataFrameColumnType::kObject && typed.kind == AttributeKind::kInt)) {
    cell.kind = PlusCellKind::kInt;
    cell.text = typed.value;
  } else if (const auto* flag = std::get_if<bool>(&typed.typed_value)) {
    cell.kind = PlusCellKind::kBool;
    cell.micros = *flag ? 1 : 0;
  } else if (const auto* number = std::get_if<double>(&typed.typed_value)) {
    cell.kind = std::isnan(*number) ? (polars ? PlusCellKind::kNone : PlusCellKind::kNaN) : PlusCellKind::kFloat;
    cell.number = *number;
  } else {
    return std::nullopt;
  }
  return cell;
}

std::optional<detail::PlusCell> csv_timestamp_cell(const Event& event, bool polars) {
  detail::PlusCell cell;
  if (!event.has_timestamp() || event.timestamp_state == FirstClassAttributeState::kNull) {
    cell.kind = polars ? detail::PlusCellKind::kNone : detail::PlusCellKind::kNaT;
    return cell;
  }
  const auto utc = csv_utc_date_cell("time:timestamp", event.timestamp);
  const auto micros = utc ? parse_timestamp_microseconds(utc->value) : std::nullopt;
  if (!micros) return std::nullopt;
  cell.kind = polars ? detail::PlusCellKind::kDatetime : detail::PlusCellKind::kTimestamp;
  cell.micros = *micros;
  if (const auto exact = parse_timestamp_exact(utc->value)) cell.nanos = static_cast<std::int32_t>(exact->nanoseconds % 1000);
  return cell;
}

std::optional<CsvCellReader> csv_log_cell_reader(const DataFrameColumns& frame, const std::string& key, bool polars,
                                                 std::string_view activity_header) {
  if (key.empty() || key.starts_with("case:")) return std::nullopt;
  if (key == "time:timestamp")
    return CsvCellReader([polars](const Trace&, const Event& event) { return csv_timestamp_cell(event, polars); });
  if (key == "concept:name")
    return CsvCellReader([](const Trace&, const Event& event) -> std::optional<detail::PlusCell> {
      detail::PlusCell cell;
      cell.kind = detail::PlusCellKind::kText;
      cell.text = event.activity;
      return cell;
    });
  const DataFrameColumn* column = nullptr;
  for (const auto& candidate : frame.columns)
    if (candidate.key == key) column = &candidate;
  if (!column) {
    // The activity column under its own header holds read_csv's dtype.
    if (key != activity_header) return std::nullopt;
    auto activity = std::make_shared<DataFrameColumn>();
    activity->key = key;
    activity->type = frame.activity_type;
    return CsvCellReader([activity, key, polars](const Trace&, const Event& event) -> std::optional<detail::PlusCell> {
      if (event.activity_source_key != key) return std::nullopt;
      return csv_typed_cell(*activity, &event.activity, polars);
    });
  }
  if (column->role == FirstClassAttributeRole::kTimestamp)
    return CsvCellReader([polars](const Trace&, const Event& event) { return csv_timestamp_cell(event, polars); });
  return CsvCellReader([column, polars](const Trace& trace, const Event& event) -> std::optional<detail::PlusCell> {
    const std::string* text = nullptr;
    bool read = false;
    switch (column->role) {
      case FirstClassAttributeRole::kResource:
        if (event.resource_source_key == column->key) {
          read = true;
          if (event.resource_state == FirstClassAttributeState::kPresent) text = &event.resource;
        }
        break;
      case FirstClassAttributeRole::kLifecycle:
        if (event.lifecycle_source_key == column->key) {
          read = true;
          if (event.lifecycle_state == FirstClassAttributeState::kPresent) text = &event.lifecycle;
        }
        break;
      case FirstClassAttributeRole::kCaseId:
        if (trace.case_id_source_key == column->key) {
          read = true;
          if (trace.case_id_state == FirstClassAttributeState::kPresent) text = &trace.case_id;
        }
        break;
      default: break;
    }
    if (!read) {
      const auto* cell = event_attribute_cell(event, column->key);
      if (cell && !cell->is_null && !is_typed_nat(*cell)) text = &cell->value;
    }
    return csv_typed_cell(*column, text, polars);
  });
}

// A pandas object column's dict cell (an XES <list> with children, or a
// scalar with children): the classic waits name it 'dict' (round GL).
constexpr auto kDictCell = detail::kPlusDictCell;

const char* classic_cell_type(detail::PlusCellKind kind) {
  return kind == kDictCell ? "dict" : detail::plus_cell_type(kind);
}

// check_plus_business_hours with dict cells (round GL).
void check_classic_business_hours(const detail::PlusCell& d1, const detail::PlusCell& d2) {
  for (const auto* cell : {&d1, &d2})
    if (cell->kind == kDictCell)
      throw HeuristicPandasError("builtins.AttributeError", "'dict' object has no attribute 'replace'");
    else if (!detail::plus_date_like(cell->kind) && cell->kind != detail::PlusCellKind::kNaT)
      break;
  detail::check_plus_business_hours(d1, d2);
}

// A date cell's wall clock for BusinessHours: a pandas object column's
// Python datetime keeps its own offset (`text`, its XES value), any other
// date is UTC.
std::int64_t classic_wall_micros(const detail::PlusCell& cell) {
  if (cell.kind == detail::PlusCellKind::kDatetime && !cell.text.empty())
    if (const auto wall = detail::local_wall_timestamp_microseconds(cell.text)) return *wall;
  return cell.micros;
}

// pm4py's classic performance DFG with business hours or a configured start
// key (round GK, observed black-box on pandas, on Polars and on the event
// log of either): trace by trace, each directly-follows pair (earlier,
// later) in the trace's order measures BusinessHours(complete(earlier),
// start(later)) with business hours (check_plus_business_hours' errors),
// else start(later) - complete(earlier): a non-date on either side raises
// TypeError (the later start's type first), a NaT on either side gives NaT,
// read as a zero wait, and a negative wait is 0. A start key no event holds
// raises KeyError at the first pair (`missing_key`, its repr). Returns each
// pair's wait in seconds, waits[t][i] for the pair ending at event i: on
// pandas Timedelta.total_seconds() of the exact difference floored to the
// microsecond (frame_elapsed_seconds, 1.118 s is 1.1179999999999999), on
// Polars' Python datetimes (`python_seconds`) the microseconds / 1e6 of the
// instants truncated to the microsecond (observed black-box, round GK).
// Two numbers subtract and their difference has no total_seconds (an int
// for ints and booleans, else a float), pandas' Timestamp minus an int
// raises its own TypeError, and NaT beside an int (not a bool) gives NaT
// (round GL, observed black-box).
std::vector<std::vector<double>> classic_cell_waits(const std::vector<std::vector<detail::PlusCell>>& start,
                                                    const std::vector<std::vector<detail::PlusCell>>& complete,
                                                    bool business_hours, const std::vector<BusinessHourSlot>& slots,
                                                    const std::optional<std::string>& missing_key,
                                                    bool python_seconds) {
  using detail::PlusCellKind;
  const auto subtractable = [](PlusCellKind kind) { return detail::plus_date_like(kind) || kind == PlusCellKind::kNaT; };
  const auto number = [](PlusCellKind kind) {
    return kind == PlusCellKind::kInt || kind == PlusCellKind::kBool || kind == PlusCellKind::kFloat ||
           kind == PlusCellKind::kNaN;
  };
  const auto real = [](PlusCellKind kind) { return kind == PlusCellKind::kFloat || kind == PlusCellKind::kNaN; };
  std::vector<std::vector<double>> waits(complete.size());
  for (std::size_t t = 0; t < complete.size(); ++t) {
    waits[t].assign(complete[t].size(), 0.0);
    for (std::size_t i = 1; i < complete[t].size(); ++i) {
      if (missing_key) throw HeuristicPandasError("builtins.KeyError", *missing_key);
      const auto& later = start[t][i];
      const auto& earlier = complete[t][i - 1];
      double wait = 0.0;
      if (business_hours) {
        check_classic_business_hours(earlier, later);
        wait = detail::business_elapsed_seconds_microseconds(classic_wall_micros(earlier), classic_wall_micros(later),
                                                             slots);
      } else {
        if (number(later.kind) && number(earlier.kind))
          throw HeuristicPandasError("builtins.AttributeError",
                                     std::string("'") + (real(later.kind) || real(earlier.kind) ? "float" : "int") +
                                         "' object has no attribute 'total_seconds'");
        // NaT beside an int is NaT (a zero wait).
        if ((later.kind == PlusCellKind::kNaT && earlier.kind == PlusCellKind::kInt) ||
            (later.kind == PlusCellKind::kInt && earlier.kind == PlusCellKind::kNaT)) {
          waits[t][i] = 0.0;
          continue;
        }
        if (later.kind == PlusCellKind::kTimestamp && earlier.kind == PlusCellKind::kInt)
          throw HeuristicPandasError("builtins.TypeError",
                                     "Addition/subtraction of integers and integer-arrays with Timestamp is no longer "
                                     "supported.  Instead of adding/subtracting `n`, use `n * obj.freq`");
        if (!subtractable(later.kind) || !subtractable(earlier.kind))
          throw HeuristicPandasError("builtins.TypeError", std::string("unsupported operand type(s) for -: '") +
                                                               classic_cell_type(later.kind) + "' and '" +
                                                               classic_cell_type(earlier.kind) + "'");
        if (later.kind != PlusCellKind::kNaT && earlier.kind != PlusCellKind::kNaT) {
          if (python_seconds) {
            wait = detail::epoch_seconds_from_microseconds(later.micros - earlier.micros);
          } else {
            const auto nanos = (later.micros - earlier.micros) * 1000 + (later.nanos - earlier.nanos);
            wait = frame_elapsed_seconds(nanos >= 0 ? nanos / 1000 : -((-nanos + 999) / 1000));
          }
        }
      }
      waits[t][i] = std::max(0.0, wait);
    }
  }
  return waits;
}

// The waits of a CSV log (or a CSV frame's log, `frame` its columns) with
// business hours or a configured start key; nullopt when a cell is not
// modelled (Minera's own path then). Another timestamp key reads its own
// column as the complete, typed like a start column, the start staying
// time:timestamp (round GL, observed black-box): a key no column holds
// raises KeyError at the first pair, after the start key's.
std::optional<std::vector<std::vector<double>>> csv_classic_waits(const EventLog& log, const DataFrameColumns& frame,
                                                                  const HeuristicMinerOptions& options, bool polars) {
  const std::string key = options.start_timestamp_attribute && !options.start_timestamp_attribute->empty()
                              ? *options.start_timestamp_attribute
                              : "time:timestamp";
  const std::string_view activity_header =
      log.traces.empty() || log.traces.front().events.empty()
          ? std::string_view{}
          : std::string_view(log.traces.front().events.front().activity_source_key);
  const auto reader = csv_log_cell_reader(frame, key, polars, activity_header);
  const bool custom_time = !standard_timestamp_attribute(options.timestamp_attribute);
  const auto complete_reader =
      custom_time ? csv_log_cell_reader(frame, options.timestamp_attribute, polars, activity_header) : std::nullopt;
  std::vector<std::vector<detail::PlusCell>> starts, completes;
  for (const auto& trace : log.traces) {
    auto& start_row = starts.emplace_back();
    auto& complete_row = completes.emplace_back();
    for (const auto& event : trace.events) {
      const auto complete = !custom_time     ? csv_timestamp_cell(event, polars)
                            : complete_reader ? (*complete_reader)(trace, event)
                                              : std::optional<detail::PlusCell>(detail::PlusCell{});
      if (!complete) return std::nullopt;
      complete_row.push_back(*complete);
      if (reader) {
        auto start = (*reader)(trace, event);
        if (!start) return std::nullopt;
        start_row.push_back(std::move(*start));
      } else {
        start_row.emplace_back();
      }
    }
  }
  const auto slots = options.use_business_hours
                         ? detail::normalize_business_hour_slots(options.business_hour_slots,
                                                                 options.business_hour_slots_explicit,
                                                                 "Heuristics Miner")
                         : std::vector<BusinessHourSlot>{};
  return classic_cell_waits(starts, completes, options.use_business_hours, slots,
                            !reader ? std::optional<std::string>(python_str_repr(key))
                            : custom_time && !complete_reader
                                ? std::optional<std::string>(python_str_repr(options.timestamp_attribute))
                                : std::nullopt,
                            polars);
}

// The classic performance view with another timestamp key (round GL): the
// frame's cells too (csv_classic_waits, discover_heuristics_net_xes_frame).
bool classic_custom_clock(const HeuristicMinerOptions& options) {
  return options.variant == HeuristicMinerVariant::kClassic &&
         options.decoration == HeuristicNetDecoration::kPerformance &&
         !standard_timestamp_attribute(options.timestamp_attribute) &&
         !(options.start_timestamp_attribute && options.start_timestamp_attribute->empty());
}

// pm4py's classic performance DFG on an XES upload's log shape, the hybrid
// legacy log whose events hold only the keys they carry, each value as its
// XES importer types it (round GK; every kind since round GL, observed
// black-box): a date a datetime (its wall clock in UTC), a string or id a
// str, a valueless string or id and a <list> without children None, any
// other <list> (and a scalar with children) a dict, ints, floats (NaN
// included) and booleans as such; a <container> or an unparsable date is
// not stored. Each directly-follows pair, trace by trace, reads
// start(later) and then complete(earlier) (complete first with business
// hours), a key the event lacks raising KeyError. The start key defaults to
// time:timestamp whatever the timestamp key (round GL). Without business
// hours start - complete: two datetimes give the wait, two numbers an int
// or float without total_seconds (AttributeError), anything else
// TypeError; with business hours both values' replace(tzinfo=None) runs
// first (str: TypeError, other non-dates: AttributeError). Returns nullopt
// when a value the first failing pair (or any pair, when none fails) reads
// is not modelled or when an event of a pair lacks its activity (the
// frequency DFG raises first), and when no pair fails under the standard
// timestamp key (Minera's own path measures those waits); `waits` with a
// custom timestamp key, whose own path reads that key as the start.
enum class LegacyKind { kAbsent, kDatetime, kStr, kInt, kFloat, kBool, kNone, kDict, kUnmodelled };

struct LegacyValue {
  LegacyKind kind = LegacyKind::kAbsent;
  std::int64_t micros = 0;
};

const char* legacy_type_name(LegacyKind kind) {
  switch (kind) {
    case LegacyKind::kDatetime: return "datetime.datetime";
    case LegacyKind::kStr: return "str";
    case LegacyKind::kInt: return "int";
    case LegacyKind::kFloat: return "float";
    case LegacyKind::kBool: return "bool";
    case LegacyKind::kNone: return "NoneType";
    case LegacyKind::kDict: return "dict";
    default: return "object";
  }
}

bool legacy_number(LegacyKind kind) {
  return kind == LegacyKind::kInt || kind == LegacyKind::kFloat || kind == LegacyKind::kBool;
}

// An XES attribute as pm4py's iterparse importer stores it in the event dict.
LegacyValue legacy_attribute_value(const EventAttribute& attribute, bool business_hours) {
  LegacyValue out;
  if (attribute.container != AttributeContainerKind::kScalar || attribute.scalar_wrapper ||
      !attribute.children.empty()) {
    if (attribute.xes_public_shape == XesPublicShape::kNone) out.kind = LegacyKind::kNone;
    else if (attribute.xes_public_shape == XesPublicShape::kWrapper || attribute.scalar_wrapper)
      out.kind = LegacyKind::kDict;
    else out.kind = LegacyKind::kUnmodelled;
    return out;
  }
  if (attribute.is_null) {
    out.kind = LegacyKind::kNone;
    return out;
  }
  switch (attribute.kind) {
    case AttributeKind::kDate: {
      if (is_typed_nat(attribute)) {
        out.kind = LegacyKind::kUnmodelled;
        return out;
      }
      const auto micros = business_hours ? detail::local_wall_timestamp_microseconds(attribute.value)
                                         : parse_timestamp_microseconds(attribute.value);
      out.kind = micros ? LegacyKind::kDatetime : LegacyKind::kUnmodelled;
      out.micros = micros.value_or(0);
      return out;
    }
    case AttributeKind::kString:
    case AttributeKind::kId: out.kind = LegacyKind::kStr; return out;
    case AttributeKind::kInt: out.kind = LegacyKind::kInt; return out;
    case AttributeKind::kFloat: out.kind = LegacyKind::kFloat; return out;
    case AttributeKind::kBoolean: out.kind = LegacyKind::kBool; return out;
    default: out.kind = LegacyKind::kUnmodelled; return out;
  }
}

// event[key] of the legacy event dict (kAbsent when the event lacks it).
LegacyValue legacy_event_value(const Event& event, const std::string& key, bool business_hours) {
  const auto key_of = [](const std::string& source, std::string_view standard) {
    return source.empty() ? std::string(standard) : source;
  };
  const auto attribute_value = [&]() {
    for (const auto& attribute : event.attributes)
      if (attribute.key == key) return legacy_attribute_value(attribute, business_hours);
    return LegacyValue{};
  };
  // A first-class key the event holds as no scalar (a <list>) stays among
  // its attributes.
  const auto field = [&](bool present, FirstClassAttributeState state, const std::optional<EventAttribute>& attribute,
                         bool date) {
    LegacyValue out;
    if (!present) return attribute_value();
    if (attribute) return legacy_attribute_value(*attribute, business_hours);
    if (state == FirstClassAttributeState::kNull) {
      out.kind = LegacyKind::kNone;
      return out;
    }
    if (!date) {
      out.kind = LegacyKind::kStr;
      return out;
    }
    const auto micros = business_hours ? detail::local_wall_timestamp_microseconds(event.timestamp)
                                       : parse_timestamp_microseconds(event.timestamp);
    out.kind = micros ? LegacyKind::kDatetime : LegacyKind::kUnmodelled;
    out.micros = micros.value_or(0);
    return out;
  };
  if (key == key_of(event.timestamp_source_key, "time:timestamp"))
    return field(event.has_timestamp(), event.timestamp_state, event.timestamp_attribute, true);
  if (key == key_of(event.activity_source_key, "concept:name"))
    return field(event.has_activity(), event.activity_state, event.activity_attribute, false);
  if (key == key_of(event.resource_source_key, "org:resource"))
    return field(event.has_resource(), event.resource_state, event.resource_attribute, false);
  if (key == key_of(event.lifecycle_source_key, "lifecycle:transition"))
    return field(event.has_lifecycle(), event.lifecycle_state, event.lifecycle_attribute, false);
  return attribute_value();
}

std::optional<std::vector<std::vector<double>>> legacy_classic_waits(const EventLog& log,
                                                                     const HeuristicMinerOptions& options,
                                                                     bool keyed = false) {
  const std::string start_key = options.start_timestamp_attribute && !options.start_timestamp_attribute->empty()
                                    ? *options.start_timestamp_attribute
                                    : "time:timestamp";
  const std::string time_key = standard_timestamp_attribute(options.timestamp_attribute)
                                   ? std::string("time:timestamp")
                                   : options.timestamp_attribute;
  const bool custom_time = time_key != "time:timestamp";
  const bool business_hours = options.use_business_hours;
  // The frequency DFG raises KeyError first on a pair without its activity
  // (legacy_keyed_heuristics checks a custom key's pairs itself).
  if (!keyed)
    for (const auto& trace : log.traces)
      if (trace.events.size() > 1)
        for (const auto& event : trace.events)
          if (!event.has_activity()) return std::nullopt;
  const auto slots = business_hours ? detail::normalize_business_hour_slots(options.business_hour_slots,
                                                                            options.business_hour_slots_explicit,
                                                                            "Heuristics Miner")
                                    : std::vector<BusinessHourSlot>{};
  const auto missing = [](const std::string& key) {
    return HeuristicPandasError("builtins.KeyError", python_str_repr(key));
  };
  std::vector<std::vector<double>> waits(log.traces.size());
  for (std::size_t t = 0; t < log.traces.size(); ++t) {
    const auto& events = log.traces[t].events;
    waits[t].assign(events.size(), 0.0);
    for (std::size_t i = 1; i < events.size(); ++i) {
      double wait = 0.0;
      if (business_hours) {
        const auto earlier = legacy_event_value(events[i - 1], time_key, true);
        if (earlier.kind == LegacyKind::kUnmodelled) return std::nullopt;
        if (earlier.kind == LegacyKind::kAbsent) throw missing(time_key);
        const auto later = legacy_event_value(events[i], start_key, true);
        if (later.kind == LegacyKind::kUnmodelled) return std::nullopt;
        if (later.kind == LegacyKind::kAbsent) throw missing(start_key);
        for (const auto* value : {&earlier, &later}) {
          if (value->kind == LegacyKind::kDatetime) continue;
          if (value->kind == LegacyKind::kStr)
            throw HeuristicPandasError("builtins.TypeError", "str.replace() takes no keyword arguments");
          throw HeuristicPandasError("builtins.AttributeError", std::string("'") + legacy_type_name(value->kind) +
                                                                    "' object has no attribute 'replace'");
        }
        wait = detail::business_elapsed_seconds_microseconds(earlier.micros, later.micros, slots);
      } else {
        const auto later = legacy_event_value(events[i], start_key, false);
        if (later.kind == LegacyKind::kUnmodelled) return std::nullopt;
        if (later.kind == LegacyKind::kAbsent) throw missing(start_key);
        const auto earlier = legacy_event_value(events[i - 1], time_key, false);
        if (earlier.kind == LegacyKind::kUnmodelled) return std::nullopt;
        if (earlier.kind == LegacyKind::kAbsent) throw missing(time_key);
        if (later.kind == LegacyKind::kDatetime && earlier.kind == LegacyKind::kDatetime) {
          wait = detail::epoch_seconds_from_microseconds(later.micros - earlier.micros);
        } else if (legacy_number(later.kind) && legacy_number(earlier.kind)) {
          const bool real = later.kind == LegacyKind::kFloat || earlier.kind == LegacyKind::kFloat;
          throw HeuristicPandasError("builtins.AttributeError", std::string("'") + (real ? "float" : "int") +
                                                                    "' object has no attribute 'total_seconds'");
        } else {
          throw HeuristicPandasError("builtins.TypeError", std::string("unsupported operand type(s) for -: '") +
                                                               legacy_type_name(later.kind) + "' and '" +
                                                               legacy_type_name(earlier.kind) + "'");
        }
      }
      waits[t][i] = std::max(0.0, wait);
    }
  }
  if (!custom_time && !keyed) return std::nullopt;
  return waits;
}

// An XES frame column's cells as Python reads them (xes_frame_typed.hpp).
using ClassicFrameDtype = detail::XesFrameDtype;

std::optional<std::vector<detail::PlusCell>> classic_frame_cells(const EventTable& table, std::size_t column,
                                                                 bool polars, ClassicFrameDtype* dtype_out = nullptr) {
  return detail::xes_frame_cells(table, column, polars, dtype_out);
}

// `selected_keys`: a CSV frame, whose session selects its own case,
// activity and timestamp columns (the frame's copies are those columns).
bool classic_cell_clock(const HeuristicMinerOptions& options, bool selected_keys = false) {
  return options.variant == HeuristicMinerVariant::kClassic &&
         options.decoration == HeuristicNetDecoration::kPerformance &&
         (selected_keys || canonical_timestamp_attribute(options) == "time:timestamp") &&
         (options.use_business_hours ||
          (options.start_timestamp_attribute && !options.start_timestamp_attribute->empty()));
}

HeuristicMinerOptions without_cell_clock(HeuristicMinerOptions options) {
  options.use_business_hours = false;
  options.start_timestamp_attribute.reset();
  return options;
}

IntervalTrace to_intervals(const Trace& trace,
                           const HeuristicMinerOptions& options,
                           bool pre_interval_mode, bool utc_clock,
                           InstanceKeyMode instance_mode = InstanceKeyMode::kPythonLog,
                           const DataFrameColumn* csv_instance_column = nullptr,
                           bool csv_log = false) {
  using LifecycleKey = std::pair<std::string, std::string>;
  struct PendingStart {
    std::string timestamp_text;
  };
  std::map<LifecycleKey, std::deque<PendingStart>> pending_starts;
  std::vector<Interval> intervals;
  std::size_t position = 0;
  for (const auto& event : trace.events) {
    ++position;
    const auto& timestamp_text = required_timestamp_text(event, options);
    const auto& activity = required_activity(event, options);
    if (pre_interval_mode) {
      const auto timestamp_microseconds =
          required_timestamp_microseconds(timestamp_text, "event");
      const auto complete_exact = utc_clock ? parse_timestamp_exact(timestamp_text)
                                            : std::optional<ParsedExactTimestamp>{};
      const double timestamp = interval_seconds(complete_exact, timestamp_microseconds);
      const auto business_complete =
          options.use_business_hours
              ? std::optional<std::int64_t>{
                    required_business_timestamp_microseconds(timestamp_text, "event", utc_clock)}
              : std::optional<std::int64_t>{};
      const auto* value = selected_start_timestamp(event, options, true);
      if (value == nullptr)
        throw std::invalid_argument(
            "Heuristics Miner PLUSPLUS interval input is missing the "
            "configured start timestamp");
      const auto start_microseconds =
          required_timestamp_microseconds(*value, "start");
      const auto start_exact = utc_clock ? parse_timestamp_exact(*value)
                                         : std::optional<ParsedExactTimestamp>{};
      const double start = interval_seconds(start_exact, start_microseconds);
      const auto business_start =
          options.use_business_hours
              ? std::optional<std::int64_t>{
                    required_business_timestamp_microseconds(*value, "start", utc_clock)}
              : std::optional<std::int64_t>{};
      intervals.push_back({activity,
                          start,
                          timestamp,
                          start_microseconds,
                          timestamp_microseconds,
                          business_start,
                          business_complete,
                          start_exact,
                          complete_exact});
      continue;
    }

    // Lifecycle events with an explicit non-start/complete transition are
    // ignored after required field presence has been checked.  An explicitly
    // present empty transition is also ignored; an omitted transition remains
    // the atomic/completion form below.
    // A CSV log's transition is its frame's lifecycle:transition cell (a
    // lifecycle column under another header is an ordinary column there).
    const auto* csv_transition = csv_log ? csv_transition_text(event) : nullptr;
    const bool lifecycle_present = csv_log ? csv_transition != nullptr : event.has_lifecycle();
    const std::string lifecycle =
        ascii_lower(csv_log ? (csv_transition ? *csv_transition : std::string()) : event.lifecycle);
    // A CSV log's instance is the frame's cell, typed by its column: "1" and
    // "1.0" of a float64 column are both 1.0 (round GH).
    const EventAttribute* instance_cell = event_attribute_cell(event, options.lifecycle_instance_attribute);
    std::optional<EventAttribute> typed_instance;
    if (csv_instance_column && instance_cell && !instance_cell->is_null) {
      typed_instance = csv_dataframe_cell(*csv_instance_column, instance_cell->value);
      instance_cell = &*typed_instance;
    }
    const auto instance = lifecycle_instance_key(instance_cell, instance_mode, position);
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
    const auto complete_exact = utc_clock ? parse_timestamp_exact(timestamp_text)
                                          : std::optional<ParsedExactTimestamp>{};
    const double timestamp = interval_seconds(complete_exact, timestamp_microseconds);
    const auto business_complete =
        options.use_business_hours
            ? std::optional<std::int64_t>{
                  required_business_timestamp_microseconds(timestamp_text, "event", utc_clock)}
            : std::optional<std::int64_t>{};
    auto start_microseconds = timestamp_microseconds;
    double start = timestamp;
    auto start_exact = complete_exact;
    auto business_start = business_complete;
    auto pending = pending_starts.find(key);
    if (pending != pending_starts.end() && !pending->second.empty()) {
      start_microseconds = required_timestamp_microseconds(
          pending->second.front().timestamp_text, "start");
      start_exact = utc_clock ? parse_timestamp_exact(pending->second.front().timestamp_text)
                              : std::optional<ParsedExactTimestamp>{};
      start = interval_seconds(start_exact, start_microseconds);
      if (options.use_business_hours) {
        business_start = required_business_timestamp_microseconds(
            pending->second.front().timestamp_text, "start", utc_clock);
      }
      pending->second.pop_front();
    }
    intervals.push_back({activity,
                        start,
                        timestamp,
                        start_microseconds,
                        timestamp_microseconds,
                        business_start,
                        business_complete,
                        start_exact,
                        complete_exact});
  }
  IntervalTrace result;
  result.sorted = intervals;
  std::stable_sort(result.sorted.begin(), result.sorted.end(),
                   [](const Interval& lhs, const Interval& rhs) {
                     return start_instant(lhs) < start_instant(rhs);
                   });
  result.performance_order =
      pre_interval_mode ? std::move(intervals) : result.sorted;
  return result;
}

PlusAbstraction build_plus_abstraction(
    const EventLog& log, const HeuristicMinerOptions& options,
    std::optional<InstanceKeyMode> instance_mode = std::nullopt) {
  PlusAbstraction out;
  std::set<std::string> known;
  const auto business_slots =
      options.use_business_hours
          ? detail::normalize_business_hour_slots(
                options.business_hour_slots,
                options.business_hour_slots_explicit, "Heuristics Miner")
          : std::vector<BusinessHourSlot>{};
  const bool csv_log = !instance_mode && log.dataframe_columns && log.dataframe_columns->csv_text;
  // On a CSV log every event holds every frame column, so a start column
  // keeps the log as it is (to_interval returns it) even where the first
  // event's cell is missing (round GH).
  // A start key naming the resource or lifecycle column reads that column's
  // cells (round GJ: the frame's event holds every column).
  // A start key naming the activity column (its copy concept:name a str
  // column, the column under its header read_csv's dtype) or the case column
  // under a header without `case:` reads those cells too (round GK).
  std::optional<CsvCellReader> csv_start_reader;
  if (csv_log && !log.traces.empty() && !log.traces.front().events.empty()) {
    const std::string start_key = options.start_timestamp_attribute.value_or("start_timestamp");
    bool timestamp_column = standard_timestamp_attribute(start_key);
    for (const auto& column : log.dataframe_columns->columns)
      if (column.key == start_key && column.role == FirstClassAttributeRole::kTimestamp) timestamp_column = true;
    if (!start_key.empty() && !timestamp_column)
      csv_start_reader = csv_log_cell_reader(*log.dataframe_columns, start_key, false,
                                             log.traces.front().events.front().activity_source_key);
  }
  const bool pre_interval_mode =
      csv_start_reader.has_value() ||
      (!log.traces.empty() && !log.traces.front().events.empty() &&
       selected_start_timestamp(log.traces.front().events.front(), options,
                                true) != nullptr);
  const bool utc_clock = log.dataframe_columns && log.dataframe_columns->csv_text;  // round FL
  const bool frame_seconds = log.dataframe_columns != nullptr;  // round FR
  // A CSV log is pm4py's convert_to_event_log of its frame (round GH,
  // observed black-box): every event holds every column, typed by the
  // column, a missing cell as NaN. to_interval lower-cases each event's
  // transition, so a lifecycle column with a missing cell (NaN) or of
  // another dtype raises AttributeError at the first such event, before
  // any interval is formed; the instance is the typed cell.
  const DataFrameColumn* csv_instance_column = nullptr;
  if (csv_log) {
    for (const auto& column : log.dataframe_columns->columns) {
      if (column.key == options.lifecycle_instance_attribute && column.role == FirstClassAttributeRole::kNone)
        csv_instance_column = &column;
      if (column.key != "lifecycle:transition" || pre_interval_mode) continue;
      for (const auto& trace : log.traces)
        for (const auto& event : trace.events) {
          std::string type;
          if (const auto* text = csv_transition_text(event); !text) {
            type = "float";
          } else {
            switch (csv_dataframe_cell(column, *text).kind) {
              case AttributeKind::kInt: type = "int"; break;
              case AttributeKind::kFloat: type = "float"; break;
              case AttributeKind::kBoolean: type = "bool"; break;
              case AttributeKind::kDate: type = "Timestamp"; break;
              default: break;
            }
          }
          if (!type.empty())
            throw HeuristicPandasError("builtins.AttributeError",
                                       "'" + type + "' object has no attribute 'lower'");
        }
    }
  }
  // A CSV start column: the abstraction reads each event's start as the
  // frame's object (a date column Timestamp or NaT, a str column str or
  // NaN, a float64 column float or NaN, an int64 or bool column int or
  // bool) and raises as on an XES upload's frame
  // (heuristic_plus_frame_intervals.hpp).
  if (csv_start_reader) {
    bool modelled = true;
    std::vector<detail::PlusCell> starts, completes;
    std::vector<std::vector<std::size_t>> traces;
    for (const auto& trace : log.traces) {
      if (!modelled) break;
      auto& rows = traces.emplace_back();
      for (const auto& event : trace.events) {
        auto start = (*csv_start_reader)(trace, event);
        const auto utc = event.has_timestamp() ? csv_utc_date_cell("time:timestamp", event.timestamp) : std::nullopt;
        const auto micros = utc ? parse_timestamp_microseconds(utc->value) : std::nullopt;
        if (!start || !micros) {
          modelled = false;
          break;
        }
        detail::PlusCell complete;
        complete.kind = detail::PlusCellKind::kTimestamp;
        complete.micros = *micros;
        rows.push_back(starts.size());
        starts.push_back(std::move(*start));
        completes.push_back(std::move(complete));
      }
    }
    if (modelled) detail::check_plus_interval_cells(traces, starts, completes, options.use_business_hours);
  }
  for (const auto& trace : log.traces) {
    const auto converted =
        to_intervals(trace, options, pre_interval_mode, utc_clock,
                     instance_mode.value_or(csv_log ? csv_instance_mode(*log.dataframe_columns,
                                                                        options.lifecycle_instance_attribute)
                                                    : InstanceKeyMode::kPythonLog),
                     csv_instance_column, csv_log);
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
        if (complete_instant(intervals[i]) <= start_instant(intervals[j])) {
          ++out.eventually_follows[
              {intervals[i].activity, intervals[j].activity}];
          break;
        }
      }
    }

    // Inclusive overlap: intervals touching at one endpoint are concurrent.
    for (std::size_t i = 0; i < intervals.size(); ++i) {
      for (std::size_t j = i + 1; j < intervals.size(); ++j) {
        if (std::max(start_instant(intervals[i]), start_instant(intervals[j])) <=
            std::min(complete_instant(intervals[i]), complete_instant(intervals[j]))) {
          ++out.concurrency[std::minmax(intervals[i].activity,
                                       intervals[j].activity)];
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
      // On an upload's frame the wait is the exact difference floored to the
      // microsecond, in frame_elapsed_seconds (round FR, observed black-box).
      const auto& previous = performance[i - 1];
      const auto& current = performance[i];
      const auto wait_microseconds =
          current.start_exact && previous.complete_exact
              ? floored_elapsed_microseconds(*current.start_exact, *previous.complete_exact)
              : current.start_microseconds - previous.complete_microseconds;
      double wait = frame_seconds ? frame_elapsed_seconds(wait_microseconds)
                                  : detail::epoch_seconds_from_microseconds(wait_microseconds);
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
  return out;
}

using PandasTimestamp = std::optional<ParsedExactTimestamp>;

PandasTimestamp pandas_timestamp(const EventAttribute* value,
                                 std::int64_t units_per_second) {
  if (value == nullptr || value->is_null || is_typed_nat(*value))
    return std::nullopt;
  if (const auto* number = std::get_if<double>(&value->typed_value)) {
    if (std::isnan(*number)) return std::nullopt;
    throw HeuristicPandasError(
        "builtins.AttributeError", "'float' object has no attribute 'timestamp'");
  }
  const auto* date = std::get_if<NativeDate>(&value->typed_value);
  auto parsed = parse_timestamp_exact(date == nullptr ? value->value
                                                      : date->value);
  if (!parsed)
    throw std::invalid_argument(
        "Heuristics Miner PLUSPLUS pandas requires a valid typed timestamp");
  const auto quantum = 1'000'000'000 / units_per_second;
  parsed->nanoseconds = static_cast<std::uint32_t>(
      parsed->nanoseconds / quantum * quantum);
  return parsed;
}

auto pandas_instant(const ParsedExactTimestamp& value) {
  return std::pair{value.seconds, value.nanoseconds};
}

double pandas_concurrency_instant(const ParsedExactTimestamp& value) {
  // Public pandas concurrency controls compare epoch seconds rounded to six
  // decimal places. Format the actual binary value so decimal rounding keeps
  // its epoch-dependent boundary, including sub-microsecond half ties.
  const double epoch = static_cast<double>(value.seconds) +
                       static_cast<double>(value.nanoseconds) / 1e9;
  char text[64];
  const auto formatted = std::to_chars(text, text + sizeof(text), epoch,
                                       std::chars_format::fixed, 6);
  if (formatted.ec != std::errc{})
    throw std::logic_error("Heuristics Miner pandas timestamp rounding failed");
  double rounded = 0.0;
  const auto parsed = std::from_chars(text, formatted.ptr, rounded,
                                     std::chars_format::fixed);
  if (parsed.ec != std::errc{} || parsed.ptr != formatted.ptr)
    throw std::logic_error("Heuristics Miner pandas timestamp rounding failed");
  return rounded;
}

double pandas_elapsed(const ParsedExactTimestamp& end,
                      const ParsedExactTimestamp& begin,
                      std::int64_t units_per_second,
                      bool scalar_wait = false) {
  const auto seconds = end.seconds - begin.seconds;
  const auto limit = std::numeric_limits<std::int64_t>::max();
  if (seconds > limit / units_per_second ||
      seconds < std::numeric_limits<std::int64_t>::min() / units_per_second)
    throw std::overflow_error("Heuristics Miner pandas timedelta overflow");
  const auto quantum = 1'000'000'000 / units_per_second;
  const auto fraction = static_cast<std::int64_t>(end.nanoseconds / quantum) -
                        static_cast<std::int64_t>(begin.nanoseconds / quantum);
  const auto integral = seconds * units_per_second;
  if ((fraction > 0 && integral > limit - fraction) ||
      (fraction < 0 && integral <
                          std::numeric_limits<std::int64_t>::min() - fraction))
    throw std::overflow_error("Heuristics Miner pandas timedelta overflow");
  const auto ticks = integral + fraction;
  if (scalar_wait && ticks <= 0) return 0.0;
  // Public duration and wait controls scale floored whole seconds, then
  // microseconds and remaining nanoseconds. Durations keep signed values;
  // only performance waits clamp nonpositive differences to zero.
  auto whole = ticks / units_per_second;
  auto remainder = ticks % units_per_second;
  if (remainder < 0) {
    --whole;
    remainder += units_per_second;
  }
  double result = static_cast<double>(whole);
  if (units_per_second == 1'000'000'000) {
    result += static_cast<double>(remainder / 1'000) * 1e-6;
    result += static_cast<double>(remainder % 1'000) * 1e-9;
  } else {
    const auto micros = remainder * (1'000'000 / units_per_second);
    result += static_cast<double>(micros) * 1e-6;
  }
  return result;
}

char32_t pandas_string_codepoint(std::string_view text, std::size_t& position) {
  const auto first = static_cast<unsigned char>(text[position++]);
  char32_t ch = first;
  int continuation = 0;
  if (first >= 0xc2 && first <= 0xdf) {
    ch = first & 0x1f; continuation = 1;
  } else if (first >= 0xe0 && first <= 0xef) {
    ch = first & 0x0f; continuation = 2;
  } else if (first >= 0xf0 && first <= 0xf4) {
    ch = first & 0x07; continuation = 3;
  } else if (first >= 0x80) {
    throw std::invalid_argument("heuristics pandas: invalid UTF-8 selector");
  }
  for (int i = 0; i < continuation; ++i) {
    if (position == text.size())
      throw std::invalid_argument("heuristics pandas: invalid UTF-8 selector");
    const auto next = static_cast<unsigned char>(text[position++]);
    if ((next & 0xc0) != 0x80)
      throw std::invalid_argument("heuristics pandas: invalid UTF-8 selector");
    ch = (ch << 6) | (next & 0x3f);
  }
  if ((continuation == 1 && ch < 0x80) ||
      (continuation == 2 && ch < 0x800) ||
      (continuation == 3 && ch < 0x10000) || ch > 0x10ffff)
    throw std::invalid_argument("heuristics pandas: invalid UTF-8 selector");
  return ch;
}

bool pandas_string_has_surrogate(const std::string& value) {
  for (std::size_t position = 0; position < value.size();) {
    const auto ch = pandas_string_codepoint(value, position);
    if (ch >= 0xd800 && ch <= 0xdfff) return true;
  }
  return false;
}

bool pandas_string_column(const EventTable& table, std::size_t column) {
  if ((!table.dtypes.empty() && table.dtypes[column] == "category") ||
      (!table.categorical_dtypes.empty() && table.categorical_dtypes[column]))
    return false;
  return std::all_of(table.rows.begin(), table.rows.end(), [&](const auto& row) {
    return row[column] && !row[column]->is_null &&
           row[column]->kind == AttributeKind::kString;
  });
}

bool pandas_missing_cell(const std::optional<EventAttribute>& cell) {
  if (!cell || cell->is_null || is_typed_nat(*cell)) return true;
  const auto* number = std::get_if<double>(&cell->typed_value);
  return number != nullptr && std::isnan(*number);
}

struct PandasStringGroups {
  explicit PandasStringGroups(bool merge) : enabled(merge) {}
  bool enabled;
  std::optional<std::string> first_surrogate;
  const std::string& group(const std::string& value) {
    if (!enabled || !pandas_string_has_surrogate(value)) return value;
    if (!first_surrogate) first_surrogate = value;
    return *first_surrogate;
  }
};

struct PandasInterval {
  std::string activity;
  PandasTimestamp start;
  PandasTimestamp complete;
  std::optional<double> concurrency_start;
  std::optional<double> concurrency_complete;
  bool missing_activity = false;
};

PlusAbstraction build_pandas_plus_abstraction(
    const EventLog& log, const HeuristicMinerOptions& options,
    std::int64_t start_units, std::int64_t complete_units,
    const EventTable& projection) {
  PlusAbstraction out;
  out.pandas_sojourn_statistics = true;
  std::set<std::string> known;
  std::map<NamePair, std::uint64_t> directional_concurrency;
  const bool string_cases = pandas_string_column(projection, 0);
  const bool string_activities = pandas_string_column(projection, 1);
  PandasStringGroups means(string_activities), efg_sources(string_activities),
      efg_targets(string_activities), concurrency_sources(string_activities),
      concurrency_targets(string_activities), wait_sources(string_activities),
      wait_targets(string_activities);
  struct Mean {
    double sum = 0.0;
    double correction = 0.0;
    std::uint64_t count = 0;
    void add(double value) {
      if (std::isnan(value)) return;
      const double adjusted = value - correction;
      const double next = sum + adjusted;
      correction = (next - sum) - adjusted;
      sum = next;
      ++count;
    }
  };
  std::map<std::string, Mean> sojourn;
  const auto duration_units = std::max(start_units, complete_units);
  // Column statistics retain all rows and their original frequency-tie order,
  // independently of the case groups used by the interval algorithms.
  for (const auto& row : projection.rows) {
    const auto& label = row[1];
    if (pandas_missing_cell(label)) continue;
    const auto& activity = label->value;
    if (known.insert(activity).second) out.activity_order.push_back(activity);
    ++out.occurrences[activity];
    const auto& duration = row[4];
    if (!duration)
      throw std::logic_error("Heuristics Miner pandas duration column missing");
    const auto* number = std::get_if<double>(&duration->typed_value);
    if (number == nullptr)
      throw std::logic_error("Heuristics Miner pandas duration is not a float");
    sojourn[means.group(activity)].add(*number);
  }
  if (!projection.categorical_dtypes.empty() && projection.categorical_dtypes[1]) {
    std::vector<std::string> ordered;
    std::set<std::string> seen;
    for (const auto& category : projection.categorical_dtypes[1]->categories)
      if (category.kind == AttributeKind::kString &&
          seen.insert(category.value).second) {
        out.occurrences.try_emplace(category.value, 0);
        ordered.push_back(category.value);
      }
    for (const auto& activity : out.activity_order)
      if (seen.insert(activity).second) ordered.push_back(activity);
    out.activity_order = std::move(ordered);
  }
  // Boundary grouping uses original rows. Interval case identities remain
  // separate, and each grouped string column has its own first representative.
  if (string_cases) {
    PandasStringGroups cases(true);
    std::map<std::string, NamePair> boundaries;
    for (const auto& row : projection.rows) {
      if (pandas_missing_cell(row[1])) continue;
      const auto& key = cases.group(row[0]->value);
      const auto& activity = row[1]->value;
      auto [found, inserted] = boundaries.try_emplace(key, NamePair{activity, activity});
      if (!inserted) found->second.second = activity;
    }
    for (const auto& [key, boundary] : boundaries) {
      ++out.starts[boundary.first];
      ++out.ends[boundary.second];
    }
  }
  for (const auto& trace : log.traces) {
    if (trace.events.empty()) continue;
    const bool missing_case = trace.case_id_state == FirstClassAttributeState::kNull;
    if (!missing_case && !string_cases) {
      const auto present = [](const auto& event) {
        return event.has_activity() &&
               event.activity_state != FirstClassAttributeState::kNull;
      };
      const auto first = std::find_if(trace.events.begin(), trace.events.end(), present);
      const auto last = std::find_if(trace.events.rbegin(), trace.events.rend(), present);
      if (first != trace.events.end()) {
        ++out.starts[required_activity(*first, options)];
        ++out.ends[required_activity(*last, options)];
      }
    }
    std::vector<PandasInterval> intervals;
    for (const auto& event : trace.events) {
      const bool missing_activity = !event.has_activity() ||
          event.activity_state == FirstClassAttributeState::kNull;
      const auto activity = missing_activity ? std::string{}
                                            : required_activity(event, options);
      const auto start = pandas_timestamp(
          selected_start_attribute(event, *options.start_timestamp_attribute),
          start_units);
      const auto complete = pandas_timestamp(
          selected_start_attribute(event, canonical_timestamp_attribute(options)),
          complete_units);
      intervals.push_back({
          activity, start, complete,
          start ? std::optional{pandas_concurrency_instant(*start)} : std::nullopt,
          complete ? std::optional{pandas_concurrency_instant(*complete)}
                   : std::nullopt, missing_activity});
    }
    std::stable_sort(intervals.begin(), intervals.end(),
                     [](const auto& lhs, const auto& rhs) {
                       if (!lhs.start) return false;
                       if (!rhs.start) return true;
                       const auto first = pandas_instant(*lhs.start);
                       const auto second = pandas_instant(*rhs.start);
                       if (first != second) return first < second;
                       if (!lhs.complete) return false;
                       if (!rhs.complete) return true;
                       return pandas_instant(*lhs.complete) <
                              pandas_instant(*rhs.complete);
                     });
    for (std::size_t i = 0; i < intervals.size(); ++i) {
      const auto& first = intervals[i];
      if (first.missing_activity) continue;
      for (std::size_t j = i + 1; j < intervals.size(); ++j) {
        const auto& second = intervals[j];
        if (second.missing_activity) continue;
        if (first.complete && second.start &&
            pandas_instant(*first.complete) <= pandas_instant(*second.start)) {
          ++out.eventually_follows[{efg_sources.group(first.activity),
                                    efg_targets.group(second.activity)}];
          break;
        }
      }
      for (std::size_t j = i + 1; j < intervals.size(); ++j) {
        const auto& second = intervals[j];
        if (second.missing_activity) continue;
        if (first.concurrency_start && first.concurrency_complete &&
            second.concurrency_start && second.concurrency_complete &&
            std::max(*first.concurrency_start, *second.concurrency_start) <=
                std::min(*first.concurrency_complete,
                         *second.concurrency_complete))
          ++directional_concurrency[{concurrency_sources.group(first.activity),
                                     concurrency_targets.group(second.activity)}];
      }
    }
    // Pandas performance adjacency follows sorted nonmissing starts. A missing
    // completion preserves its row but contributes no outgoing wait sample.
    // Missing activity rows block adjacency and produce no labeled wait pair.
    // Missing case groups still supply intervals, but no performance waits.
    if (missing_case ||
        (string_cases && pandas_string_has_surrogate(trace.case_id))) continue;
    const PandasInterval* previous = nullptr;
    for (const auto& current : intervals) {
      if (!current.start) continue;
      if (previous != nullptr && !previous->missing_activity &&
          !current.missing_activity) {
        const NamePair pair{wait_sources.group(previous->activity),
                            wait_targets.group(current.activity)};
        ++out.direct_adjacency[pair];
        if (previous->complete) {
          const auto wait = std::max(
              0.0, pandas_elapsed(*current.start, *previous->complete,
                                  duration_units, true));
          out.wait_sums[pair] += wait;
          out.wait_samples[pair].push_back(wait);
        }
      }
      previous = &current;
    }
  }
  for (const auto& [activity, mean] : sojourn)
    out.sojourn_means[activity] =
        mean.count == 0 ? std::numeric_limits<double>::quiet_NaN()
                        : mean.sum / static_cast<double>(mean.count);
  for (const auto& [pair, count] : directional_concurrency)
    out.concurrency[std::minmax(pair.first, pair.second)] = count;
  std::stable_sort(out.activity_order.begin(), out.activity_order.end(),
                   [&](const auto& first, const auto& second) {
                     return out.occurrences.at(first) >
                            out.occurrences.at(second);
                   });
  return out;
}

double plus_sojourn_mean(const PlusAbstraction& abstraction,
                         const std::string& activity) {
  const auto pandas = abstraction.sojourn_means.find(activity);
  if (abstraction.pandas_sojourn_statistics)
    return pandas != abstraction.sojourn_means.end()
        ? pandas->second : std::numeric_limits<double>::quiet_NaN();
  return pandas != abstraction.sojourn_means.end()
             ? pandas->second
             : abstraction.sojourn_sums.at(activity) /
                   static_cast<double>(abstraction.occurrences.at(activity));
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

struct PlusCountThreshold {
  std::uint64_t integer;
  std::optional<double> numeric;

  [[nodiscard]] bool accepts(std::uint64_t count) const {
    if (!numeric) return count >= integer;
    const double minimum = *numeric;
    if (std::isnan(minimum)) return false;
    if (minimum <= 0.0) return true;
    if (minimum >= 0x1p64) return false;
    // Compare integer counts without rounding them to double. The checked
    // conversion truncates a finite positive cutoff below 2^64; a fractional
    // cutoff requires the next integer occurrence.
    const auto whole = static_cast<std::uint64_t>(minimum);
    return count > whole ||
           (count == whole && minimum == static_cast<double>(whole));
  }
};

double plus_dependency(const PlusAbstraction& abstraction,
                       const std::string& first,
                       const std::string& second,
                       const PlusCountThreshold& min_frequency) {
  const double forward = static_cast<double>(
      get_count(abstraction.eventually_follows, first, second));
  const auto raw_backward =
      get_count(abstraction.eventually_follows, second, first);
  const double backward = static_cast<double>(
      min_frequency.accepts(raw_backward) ? raw_backward : 0);
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
    bool outgoing, double threshold, const PlusCountThreshold& min_frequency) {
  const std::size_t k = neighbours.size();
  std::vector<std::vector<bool>> adjacent(k, std::vector<bool>(k, false));
  for (std::size_t i = 0; i < k; ++i) {
    for (std::size_t j = i + 1; j < k; ++j) {
      const auto& n1 = net.activities[neighbours[i]];
      const auto& n2 = net.activities[neighbours[j]];
      const auto& middle = net.activities[center];
      const double numerator = static_cast<double>(
          (min_frequency.accepts(
               get_count(abstraction.eventually_follows, n1, n2))
               ? get_count(abstraction.eventually_follows, n1, n2)
               : 0) +
          (min_frequency.accepts(
               get_count(abstraction.eventually_follows, n2, n1))
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

// `frame_clock` (round GB): an XES upload's frame, whose missing
// timestamps are NaT: a wait that reads one is NaT, and pm4py keeps
// max(0, NaN), a zero wait (observed black-box on the pandas frame).
// `waits` (round GK): each directly-follows pair's wait, as
// classic_cell_waits measured it on the frame's cells, in place of the
// event texts' clock.
HeuristicClassicDfgInput build_classic_input(
    const EventLog& log, const HeuristicMinerOptions& options,
    bool frame_clock = false, const std::vector<std::vector<double>>* waits = nullptr,
    bool pandas_frame = false) {
  HeuristicClassicDfgInput input;
  const bool utc_clock = log.dataframe_columns && log.dataframe_columns->csv_text;  // round FL
  // pandas (a CSV upload's log, a frame shape's pandas frame) measures a wait
  // as Timedelta.total_seconds() of the exact difference (round FR); an XES
  // upload's legacy log and Polars' copies hold Python datetimes, whose
  // timedelta.total_seconds() is the microseconds / 1e6 (round GK, observed
  // black-box: 1.118 s is 1.1179999999999999 on pandas, 1.118 there).
  const bool frame_seconds = utc_clock || pandas_frame;
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
  std::size_t trace_index = 0;
  for (const auto& trace : log.traces) {
    const auto* trace_waits = waits ? &(*waits)[trace_index] : nullptr;
    ++trace_index;
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
      if (options.decoration == HeuristicNetDecoration::kPerformance && trace_waits) {
        (*input.performance_dfg)[pair].push_back((*trace_waits)[i]);
        continue;
      }
      if (options.decoration == HeuristicNetDecoration::kPerformance) {
        if (frame_clock && (!trace.events[i - 1].has_timestamp() ||
                            trace.events[i - 1].timestamp_state == FirstClassAttributeState::kNull ||
                            !trace.events[i].has_timestamp() ||
                            trace.events[i].timestamp_state == FirstClassAttributeState::kNull)) {
          (*input.performance_dfg)[pair].push_back(0.0);
          continue;
        }
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
        // A CSV upload's frame holds nanoseconds: pm4py takes the exact
        // difference floored to the microsecond, not the difference of the
        // floored instants, in frame_elapsed_seconds on an upload's frame
        // (round FR, observed black-box).
        const auto elapsed_microseconds =
            frame_seconds ? floored_elapsed_microseconds(
                            start_text, selected_timestamp(trace.events[i - 1], options),
                            start_microseconds - complete_microseconds)
                      : start_microseconds - complete_microseconds;
        double elapsed = frame_seconds ? frame_elapsed_seconds(elapsed_microseconds)
                                       : detail::epoch_seconds_from_microseconds(elapsed_microseconds);
        if (options.use_business_hours) {
          const auto previous_local =
              required_business_timestamp_microseconds(
                  selected_timestamp(trace.events[i - 1], options), "event", utc_clock);
          const auto current_local =
              required_business_timestamp_microseconds(start_text, "start", utc_clock);
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

HeuristicsNet discover_plus_plus_from_abstraction(
    const PlusAbstraction& abstraction, const HeuristicMinerOptions& options) {
  const PlusCountThreshold min_activity{options.min_activity_count,
                                       options.plusplus_min_activity_count};
  const PlusCountThreshold min_frequency{options.min_frequency,
                                        options.plusplus_min_frequency};
  const auto* typed_decoration = options.plusplus_decoration_value
      ? std::get_if<std::string>(&options.plusplus_decoration_value->value)
      : nullptr;
  const HeuristicNetDecoration effective_decoration =
      options.plusplus_decoration_value
          ? (typed_decoration && *typed_decoration == "frequency"
                 ? HeuristicNetDecoration::kFrequency
                 : HeuristicNetDecoration::kPerformance)
          : options.plusplus_decoration_selector
          ? (*options.plusplus_decoration_selector == "frequency"
                 ? HeuristicNetDecoration::kFrequency
                 : HeuristicNetDecoration::kPerformance)
          : options.decoration;
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
    if (!min_activity.accepts(abstraction.occurrences.at(activity)))
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
         plus_sojourn_mean(abstraction, activity)});
    net.activity_statistics.back().is_start_activity = false;
    net.activity_statistics.back().is_end_activity = false;
  }
  // Discovery keeps sojourn data before activity filtering. Preserve every activity
  // and attach an id only when it survived the node filter.
  net.sojourn_times.emplace();
  for (const auto& activity : abstraction.activity_order) {
    const double mean_sojourn = plus_sojourn_mean(abstraction, activity);
    net.raw_activity_statistics.push_back(
        {activity, abstraction.occurrences.at(activity), mean_sojourn});
    if (!abstraction.pandas_sojourn_statistics ||
        abstraction.sojourn_means.contains(activity))
      net.sojourn_times->emplace(activity, mean_sojourn);
  }

  for (const auto& [pair, frequency] : abstraction.eventually_follows) {
    const auto source = ids.find(pair.first);
    const auto target = ids.find(pair.second);
    if (source == ids.end() || target == ids.end() ||
        !min_frequency.accepts(frequency))
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
                        min_frequency);
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
                     options.and_threshold, min_frequency);
    net.input_bundles[activity] =
        plus_bundles(abstraction, net, activity, predecessors[activity], false,
                     options.and_threshold, min_frequency);
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

HeuristicsNet discover_plus_plus(const EventLog& log,
                                 const HeuristicMinerOptions& options,
                                 std::optional<InstanceKeyMode> instance_mode = std::nullopt) {
  return discover_plus_plus_from_abstraction(build_plus_abstraction(log, options, instance_mode),
                                             options);
}

}  // namespace

HeuristicsNet discover_plus_plus_pandas(
    EventTable& data, const HeuristicMinerOptions& options) {
  data.validate();
  auto selected = options;
  selected.variant = HeuristicMinerVariant::kPlusPlus;
  selected.start_timestamp_attribute =
      options.start_timestamp_attribute.value_or("start_timestamp");
  const auto repr = [](const std::string& text) {
    const char quote = text.find('\'') != std::string::npos &&
                               text.find('"') == std::string::npos
                           ? '"' : '\'';
    std::string result(1, quote);
    constexpr char hex[] = "0123456789abcdef";
    for (std::size_t position = 0; position < text.size();) {
      const auto start = position;
      const auto ch = pandas_string_codepoint(text, position);
      // Python strings can contain lone surrogates. Preserve those code points
      // in WTF-8 input long enough to render their explicit Unicode escapes.
      if (ch == quote || ch == '\\') {
        result += '\\';
        result += static_cast<char>(ch);
      } else if (ch == '\n') result += "\\n";
      else if (ch == '\r') result += "\\r";
      else if (ch == '\t') result += "\\t";
      else if (!root_cause_unicode_printable(ch)) {
        const int digits = ch <= 0xff ? 2 : ch <= 0xffff ? 4 : 8;
        result += digits == 2 ? "\\x" : digits == 4 ? "\\u" : "\\U";
        for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4)
          result += hex[(ch >> shift) & 15];
      } else result.append(text, start, position - start);
    }
    result += quote;
    return result;
  };
  const auto find_column = [&](const std::string& key)
      -> std::optional<std::size_t> {
    const auto found = std::find(data.columns.begin(), data.columns.end(), key);
    if (found == data.columns.end()) return std::nullopt;
    return static_cast<std::size_t>(found - data.columns.begin());
  };
  const auto case_column = find_column(selected.case_id_attribute);
  if (!case_column)
    throw HeuristicPandasError("builtins.KeyError", repr(selected.case_id_attribute));
  const auto activity_column = find_column(selected.activity_attribute);
  if (!activity_column)
    throw HeuristicPandasError(
        "builtins.KeyError", repr("Column not found: " + selected.activity_attribute));
  const auto start_key = *selected.start_timestamp_attribute;
  const auto original_start = find_column(start_key);
  const auto completion_column = find_column(selected.timestamp_attribute);
  if (!completion_column) {
    const auto message = original_start
        ? "[" + repr(selected.timestamp_attribute) + "] not in index"
        : selected.timestamp_attribute;
    throw HeuristicPandasError("builtins.KeyError", repr(message));
  }
  const auto completed = *completion_column;
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
  std::size_t started;
  if (!original_start) {
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
    started = *original_start;
  }
  // Text, number and boolean columns (read_csv's dtypes on CSV rows) fail
  // where pm4py compares each completion with its case's next start and then
  // subtracts them, before any .dt: rows sorted by case and start (a missing
  // start last), pairs with both cells present only (round FT, observed
  // black-box). Two numeric columns pass both and reach the .dt below.
  if (!data.dtypes.empty()) {
    const auto& completion_dtype = data.dtypes[completed];
    const auto& start_dtype = data.dtypes[started];
    const auto numeric_name = [](const std::string& dtype) -> const char* {
      if (dtype == "int64" || dtype == "uint64") return "int";
      if (dtype == "float64") return "float";
      return nullptr;
    };
    std::string message;
    if (completion_dtype == "str" && start_dtype == "str")
      message = "unsupported operand type(s) for -: 'str' and 'str'";
    else if (completion_dtype == "bool" && start_dtype == "bool")
      message = "numpy boolean subtract, the `-` operator, is not supported, use the bitwise_xor, "
                "the `^` operator, or the logical_xor function instead.";
    else if (completion_dtype == "str" && numeric_name(start_dtype))
      message = std::string("'<=' not supported between instances of 'str' and '") +
                numeric_name(start_dtype) + "'";
    else if (start_dtype == "str" && numeric_name(completion_dtype))
      message = std::string("'>=' not supported between instances of 'str' and '") +
                numeric_name(completion_dtype) + "'";
    if (!message.empty()) {
      const auto present = [&](const EventTable::Row& row, std::size_t index) {
        return index < row.size() && row[index] && !row[index]->is_null;
      };
      const auto number = [](const EventAttribute& cell) {
        if (const auto* integer = std::get_if<std::int64_t>(&cell.typed_value))
          return static_cast<long double>(*integer);
        if (const auto* unsigned_integer = std::get_if<std::uint64_t>(&cell.typed_value))
          return static_cast<long double>(*unsigned_integer);
        if (const auto* real = std::get_if<double>(&cell.typed_value))
          return static_cast<long double>(*real);
        return static_cast<long double>(std::strtold(cell.value.c_str(), nullptr));
      };
      const bool numeric_start = numeric_name(start_dtype) != nullptr;
      std::map<std::string, std::vector<std::size_t>> cases;
      for (std::size_t row = 0; row < data.rows.size(); ++row)
        if (present(data.rows[row], *case_column))
          cases[data.rows[row][*case_column]->value].push_back(row);
      // Two text or two boolean columns compare fine, so only the pairs the
      // partial order keeps reach the subtraction: a later row (by start,
      // then completion) whose start is no earlier than the completion
      // (text order, False before True). With none, pm4py meets the .dt on
      // an empty frame instead (round FU, observed black-box).
      const bool same_kind = completion_dtype == start_dtype &&
                             (completion_dtype == "str" || completion_dtype == "bool");
      bool paired = false;
      for (auto& [name, rows] : cases) {
        (void)name;
        std::stable_sort(rows.begin(), rows.end(), [&](std::size_t a, std::size_t b) {
          const bool has_a = present(data.rows[a], started);
          const bool has_b = present(data.rows[b], started);
          if (!has_a || !has_b) return has_a && !has_b;
          const auto& x = *data.rows[a][started];
          const auto& y = *data.rows[b][started];
          if (numeric_start) return number(x) < number(y);
          if (x.value != y.value || !same_kind) return x.value < y.value;
          const bool cx = present(data.rows[a], completed), cy = present(data.rows[b], completed);
          if (!cx || !cy) return cx && !cy;
          return data.rows[a][completed]->value < data.rows[b][completed]->value;
        });
        if (same_kind) {
          std::optional<std::string> earliest;
          for (const auto row : rows) {
            if (earliest && present(data.rows[row], started) &&
                *earliest <= data.rows[row][started]->value) {
              paired = true;
              break;
            }
            if (present(data.rows[row], completed) &&
                (!earliest || data.rows[row][completed]->value < *earliest))
              earliest = data.rows[row][completed]->value;
          }
        } else {
          for (std::size_t k = 0; k + 1 < rows.size() && !paired; ++k)
            paired = present(data.rows[rows[k]], completed) && present(data.rows[rows[k + 1]], started);
        }
        if (paired) break;
      }
      if (paired) throw HeuristicPandasError("builtins.TypeError", message);
    }
  }
  validate_datetime(completed);
  if (!data.dtypes.empty() && data.dtypes[started] == "object" &&
      std::string_view(data.dtypes[completed]).starts_with("datetime64["))
    throw HeuristicPandasError(
        "builtins.TypeError", "cannot subtract DatetimeArray from ndarray[object]");
  validate_datetime(started);
  if (selected.activity_attribute == selected.case_id_attribute)
    throw HeuristicPandasError(
        "builtins.KeyError", repr(selected.activity_attribute + "_2"));
  if (!data.categorical_dtypes.empty() && data.categorical_dtypes[*case_column]) {
    for (const auto& category : data.categorical_dtypes[*case_column]->categories) {
      if (category.kind != AttributeKind::kString) continue;
      std::optional<std::size_t> first, last;
      std::string character;
      std::size_t codepoint_index = 0;
      for (std::size_t position = 0; position < category.value.size(); ++codepoint_index) {
        const auto start = position;
        const auto ch = pandas_string_codepoint(category.value, position);
        if (ch >= 0xd800 && ch <= 0xdfff) {
          if (!first) {
            first = codepoint_index;
            character = category.value.substr(start, position - start);
          }
          last = codepoint_index;
        } else if (first) break;
      }
      if (first) {
        const auto detail = first == last
            ? "character " + repr(character) + " in position " + std::to_string(*first)
            : "characters in position " + std::to_string(*first) + "-" + std::to_string(*last);
        throw HeuristicPandasError("builtins.UnicodeEncodeError",
            "'utf-8' codec can't encode " + detail + ": surrogates not allowed");
      }
    }
  }
  const auto unit = [&](std::size_t index) -> std::int64_t {
    if (!data.dtypes.empty()) {
      const std::string_view dtype = data.dtypes[index];
      if (dtype.starts_with("datetime64[ns")) return 1'000'000'000;
      if (dtype.starts_with("datetime64[ms")) return 1'000;
      if (dtype.starts_with("datetime64[s")) return 1;
      return 1'000'000;
    }
    for (const auto& row : data.rows) {
      if (!row[index]) continue;
      const auto* date = std::get_if<NativeDate>(&row[index]->typed_value);
      if (date && date->resolution == NativeDateResolution::kNanosecond)
        return 1'000'000'000;
    }
    return 1'000'000;
  };
  const auto start_units = unit(started);
  const auto complete_units = unit(completed);
  const auto duration_units = std::max(start_units, complete_units);
  const auto any_timestamp = [&](std::size_t index) {
    return std::any_of(data.rows.begin(), data.rows.end(), [&](const auto& row) {
      return row[index] && !row[index]->is_null && !is_typed_nat(*row[index]);
    });
  };
  const auto timezone_aware = [&](std::size_t index) {
    if (!data.dtypes.empty())
      return data.dtypes[index].find(',') != std::string::npos;
    for (const auto& row : data.rows) {
      if (!row[index]) continue;
      const auto* date = std::get_if<NativeDate>(&row[index]->typed_value);
      if (date && date->timezone) return true;
      const auto parsed = parse_timestamp_exact(row[index]->value);
      if (parsed && parsed->timezone_aware) return true;
    }
    return false;
  };
  const bool string_cases = pandas_string_column(data, *case_column);
  // The pairing of consecutive events of a case yields no rows when no case
  // holds two events; timezone-aware columns then fail on the empty object
  // array. Missing (or unencodable) case ids form no case here.
  const auto holds_pair = [&](const auto& counted) {
    std::map<std::optional<std::string>, std::size_t> sizes;
    for (const auto& row : data.rows) {
      if (!counted(row)) continue;
      const auto& cell = row[*case_column];
      const auto key = pandas_missing_cell(cell) ? std::nullopt
                                                 : std::optional<std::string>(cell->value);
      if (++sizes[key] == 2) return true;
    }
    return false;
  };
  if (timezone_aware(completed) && !holds_pair([&](const auto& row) {
        return !pandas_missing_cell(row[*case_column]) &&
               !(string_cases && pandas_string_has_surrogate(row[*case_column]->value));
      }))
    throw HeuristicPandasError(
        "builtins.TypeError", "cannot subtract DatetimeArray from ndarray[object]");
  if (timezone_aware(started) != timezone_aware(completed) ||
      (started != completed && timezone_aware(completed) &&
       !any_timestamp(completed)))
    throw HeuristicPandasError(
        "builtins.TypeError",
        "Cannot subtract tz-naive and tz-aware datetime-like objects.");
  std::vector<double> durations;
  durations.reserve(data.rows.size());
  for (const auto& row : data.rows) {
    const auto& end = row[completed];
    const auto& begin = row[started];
    if (!end || !begin || end->is_null || begin->is_null ||
        is_typed_nat(*end) || is_typed_nat(*begin)) {
      durations.push_back(std::numeric_limits<double>::quiet_NaN());
    } else {
      durations.push_back(pandas_elapsed(
          *pandas_timestamp(&*end, complete_units),
          *pandas_timestamp(&*begin, start_units), duration_units));
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
  const bool any_activity = std::any_of(
      data.rows.begin(), data.rows.end(), [&](const auto& row) {
        return !pandas_missing_cell(row[*activity_column]);
      });
  // Rows without an activity are dropped before pairing; when no case
  // (missing ids grouped together) keeps two events, the empty pair table
  // fails the duration comparison.
  const bool any_pair = holds_pair([&](const auto& row) {
    return !pandas_missing_cell(row[*activity_column]);
  });
  if (started != difference && (!any_timestamp(started) || !any_activity || !any_pair)) {
    const auto suffix = duration_units == 1'000'000'000 ? "ns"
                        : duration_units == 1'000'000 ? "us"
                        : duration_units == 1'000 ? "ms" : "s";
    throw HeuristicPandasError(
        "builtins.TypeError", "Invalid comparison between dtype=timedelta64[" +
                                  std::string(suffix) + "] and int");
  }
  // Mine an internal projection with unambiguous event roles. The caller's
  // literal selectors and cells remain intact in the mutable dataframe.
  EventTable projection;
  projection.columns = {"case:concept:name", "concept:name", "time:timestamp",
                        "start_timestamp", "@@diff"};
  const std::vector<std::size_t> source_columns{
      *case_column, *activity_column, completed, started, difference};
  if (!data.dtypes.empty())
    for (const auto index : source_columns)
      projection.dtypes.push_back(data.dtypes[index]);
  if (!data.categorical_dtypes.empty())
    for (const auto index : source_columns)
      projection.categorical_dtypes.push_back(data.categorical_dtypes[index]);
  projection.rows.reserve(data.rows.size());
  for (const auto& row : data.rows) {
    EventTable::Row projected;
    for (std::size_t i = 0; i < source_columns.size(); ++i) {
      auto cell = row[source_columns[i]];
      // Normalize missing case/activity scalars only in the internal event
      // roles; retain the caller's actual scalar and duration values.
      if (i < 2 && pandas_missing_cell(cell)) {
        cell = EventAttribute{};
        cell->is_null = true;
      }
      if (cell) {
        cell->key = projection.columns[i];
        cell->first_class_role = i == 0 ? FirstClassAttributeRole::kCaseId
                                : i == 1 ? FirstClassAttributeRole::kActivity
                                : i == 2 ? FirstClassAttributeRole::kTimestamp
                                         : FirstClassAttributeRole::kNone;
        cell->first_class_state = i < 3
            ? (cell->is_null ? FirstClassAttributeState::kNull
                             : FirstClassAttributeState::kPresent)
            : FirstClassAttributeState::kMissing;
        cell->first_class_source_key = i < 3 ? cell->key : std::string{};
      }
      projected.push_back(std::move(cell));
    }
    projection.rows.push_back(std::move(projected));
  }
  auto projected_options = selected;
  projected_options.case_id_attribute = "case:concept:name";
  projected_options.activity_attribute = "concept:name";
  projected_options.timestamp_attribute = "time:timestamp";
  projected_options.start_timestamp_attribute = "start_timestamp";
  auto net = discover_plus_plus_from_abstraction(
      build_pandas_plus_abstraction(
          classic_event_log(EventDataView(projection), projected_options),
          projected_options, start_units, complete_units, projection),
      selected);
  net.activity_attribute = selected.activity_attribute;
  net.timestamp_attribute = selected.timestamp_attribute;
  net.activity_ids_are_local = selected.activity_attribute != "concept:name";
  return net;
}

// pm4py's classic heuristics net sorts each node's accepted successors (DFG
// order) as Python sorts its activity values (round GA's order, see
// discover_heuristics_net_xes_frame): rows in the order their source first
// leads a pair of the cleaned DFG, each row's targets in pair order, an
// accepted edge adding its source, then its target; the length-two loop
// edges join later. `less` is Python's < on two activity ids (it raises
// TypeError on values Python does not order). Round GL: activity values of
// other types (an XES upload's legacy log, a CSV upload's typed column).
// Returns the nodes in that order (the Petri conversion visits them so).
std::vector<std::string> check_successor_sort(const HeuristicsNet& net, const EventLog& log,
                                              const std::function<bool(std::size_t, std::size_t)>& less) {
  std::map<std::pair<std::string, std::string>, std::size_t> pair_order;
  std::vector<std::pair<std::string, std::string>> pairs;
  for (const auto& trace : log.traces)
    for (std::size_t e = 1; e < trace.events.size(); ++e)
      if (pair_order.try_emplace({trace.events[e - 1].activity, trace.events[e].activity}, pair_order.size()).second)
        pairs.emplace_back(trace.events[e - 1].activity, trace.events[e].activity);
  std::vector<std::string> rows;
  for (const auto& pair : pairs)
    if (net.cleaned_dfg.contains(pair) && std::find(rows.begin(), rows.end(), pair.first) == rows.end())
      rows.push_back(pair.first);
  const auto id_of = [&](const std::string& name) -> std::optional<std::size_t> {
    const auto found = std::find(net.activities.begin(), net.activities.end(), name);
    if (found == net.activities.end()) return std::nullopt;
    return static_cast<std::size_t>(found - net.activities.begin());
  };
  const auto accepted = [&](const std::pair<std::string, std::string>& pair) {
    const auto source = id_of(pair.first), target = id_of(pair.second);
    if (!source || !target) return false;
    return std::any_of(net.edges.begin(), net.edges.end(), [&](const auto& edge) {
      return edge.source == *source && edge.target == *target && !edge.length_two_loop;
    });
  };
  std::vector<std::string> appearance;
  for (const auto& row : rows)
    for (const auto& pair : pairs) {
      if (pair.first != row || !net.cleaned_dfg.contains(pair) || !accepted(pair)) continue;
      for (const auto* name : {&pair.first, &pair.second})
        if (std::find(appearance.begin(), appearance.end(), *name) == appearance.end()) appearance.push_back(*name);
    }
  for (const auto& name : appearance) {
    const auto id = id_of(name);
    if (!id) continue;
    std::vector<std::size_t> successors;
    for (const auto& edge : net.edges)
      if (edge.source == *id && !edge.length_two_loop) successors.push_back(edge.target);
    if (successors.size() < 2) continue;
    const auto rank = [&](std::size_t target) {
      const auto at = pair_order.find({name, net.activities[target]});
      return at == pair_order.end() ? pair_order.size() : at->second;
    };
    std::stable_sort(successors.begin(), successors.end(),
                     [&](std::size_t a, std::size_t b) { return rank(a) < rank(b); });
    detail::python_list_sort(successors, less);
  }
  return appearance;
}

// pm4py's Petri conversion names a node's join place "pre_" + its value: a
// node whose value is no str raises TypeError at the first such node with an
// accepted incoming edge, in the dependency pass's order (round GL,
// observed black-box). Every route that mines such a net on a log shape
// converts it next, so the miner raises it.
void check_typed_conversion(const HeuristicsNet& net, const std::vector<std::string>& appearance,
                            const std::function<std::string(const std::string&)>& python_type) {
  for (const auto& name : appearance) {
    const auto type = python_type(name);
    if (type == "str") continue;
    const auto found = std::find(net.activities.begin(), net.activities.end(), name);
    if (found == net.activities.end()) continue;
    const auto id = static_cast<std::size_t>(found - net.activities.begin());
    if (std::any_of(net.edges.begin(), net.edges.end(), [&](const auto& edge) { return edge.target == id; }))
      throw HeuristicPandasError("builtins.TypeError", "can only concatenate str (not \"" + type + "\") to str");
  }
}

// pm4py's classic heuristics miner keyed by a custom activity key on an XES
// upload's log shape, the hybrid legacy log whose event dicts hold each
// value as the XES importer types it (round GL, observed black-box). In
// order: a dict value anywhere raises TypeError unhashable (the activity
// counts); a pair's event without the key raises KeyError (a single-event
// trace without it is left out); the performance DFG's errors
// (legacy_classic_waits); the first pair in log order whose earlier value is
// no str raises TypeError "'T' object is not subscriptable"; the net's
// nodes, in their first appearance among the retained edges, sort their
// successors as Python sorts the values ("'<' not supported between
// instances of ..."). Nodes are named str() of the value, values equal in
// Python (1, 1.0, True) one node named after the first seen. nullopt when a
// value is not modelled, or when two nodes would share a name (a str '5'
// beside an int 5, NaN values, each its own node in pm4py).
struct LegacyActivityValue {
  LegacyKind kind = LegacyKind::kAbsent;
  std::string label;
  std::string identity;  // equal in Python <=> equal identity
  long double number = 0.0L;
  bool nan = false;
  std::int64_t micros = 0;
};

std::optional<LegacyActivityValue> legacy_activity_value(const Event& event, const std::string& key) {
  LegacyActivityValue out;
  const auto attribute_of = [&]() -> const EventAttribute* {
    const auto key_of = [](const std::string& source, std::string_view standard) {
      return source.empty() ? std::string(standard) : source;
    };
    const auto first_class = [&](bool present, const std::optional<EventAttribute>& attribute) -> const EventAttribute* {
      return present && attribute ? &*attribute : nullptr;
    };
    if (key == key_of(event.timestamp_source_key, "time:timestamp") && event.has_timestamp())
      return first_class(true, event.timestamp_attribute);
    if (key == key_of(event.activity_source_key, "concept:name") && event.has_activity())
      return first_class(true, event.activity_attribute);
    if (key == key_of(event.resource_source_key, "org:resource") && event.has_resource())
      return first_class(true, event.resource_attribute);
    if (key == key_of(event.lifecycle_source_key, "lifecycle:transition") && event.has_lifecycle())
      return first_class(true, event.lifecycle_attribute);
    for (const auto& attribute : event.attributes)
      if (attribute.key == key) return &attribute;
    return nullptr;
  };
  const auto value = legacy_event_value(event, key, false);
  out.kind = value.kind;
  out.micros = value.micros;
  switch (value.kind) {
    case LegacyKind::kAbsent:
    case LegacyKind::kDict: return out;
    case LegacyKind::kUnmodelled: return std::nullopt;
    case LegacyKind::kNone:
      out.label = "None";
      out.identity = "none";
      return out;
    default: break;
  }
  const auto* attribute = attribute_of();
  const std::string* text = nullptr;
  if (attribute) text = &attribute->value;
  else if (key == "time:timestamp" || key == event.timestamp_source_key) text = &event.timestamp;
  else if (key == "concept:name" || key == event.activity_source_key) text = &event.activity;
  else if (key == "org:resource" || key == event.resource_source_key) text = &event.resource;
  else text = &event.lifecycle;
  switch (value.kind) {
    case LegacyKind::kStr:
      out.label = *text;
      out.identity = "s" + *text;
      return out;
    case LegacyKind::kDatetime:
      out.label = detail::plus_python_datetime_str(*text);
      out.identity = "d" + std::to_string(value.micros);
      return out;
    case LegacyKind::kBool: {
      const bool truth = attribute && std::holds_alternative<bool>(attribute->typed_value)
                             ? std::get<bool>(attribute->typed_value)
                             : *text == "true" || *text == "True";
      out.label = truth ? "True" : "False";
      out.number = truth ? 1.0L : 0.0L;
      out.identity = truth ? "n1" : "n0";
      return out;
    }
    case LegacyKind::kInt: {
      if (!attribute) return std::nullopt;
      if (const auto* signed_value = std::get_if<std::int64_t>(&attribute->typed_value)) {
        out.label = std::to_string(*signed_value);
        out.number = static_cast<long double>(*signed_value);
      } else if (const auto* unsigned_value = std::get_if<std::uint64_t>(&attribute->typed_value)) {
        out.label = std::to_string(*unsigned_value);
        out.number = static_cast<long double>(*unsigned_value);
      } else {
        return std::nullopt;
      }
      out.identity = "n" + out.label;
      return out;
    }
    case LegacyKind::kFloat: {
      if (!attribute || !std::holds_alternative<double>(attribute->typed_value)) return std::nullopt;
      const double real = std::get<double>(attribute->typed_value);
      out.label = python::float_repr(real);
      out.number = real;
      out.nan = std::isnan(real);
      if (out.nan) {
        out.identity = "nan";
      } else if (std::trunc(real) == real && std::fabs(real) < 9.2e18) {
        out.identity = "n" + std::to_string(static_cast<long long>(real));
      } else {
        out.identity = "f" + out.label;
      }
      return out;
    }
    default: return std::nullopt;
  }
}

// Python's a < b on two activity values (TypeError where Python has none).
bool legacy_activity_less(const LegacyActivityValue& a, const LegacyActivityValue& b) {
  const auto number = [](LegacyKind kind) {
    return kind == LegacyKind::kInt || kind == LegacyKind::kFloat || kind == LegacyKind::kBool;
  };
  if (a.kind == LegacyKind::kStr && b.kind == LegacyKind::kStr) return a.label < b.label;
  if (number(a.kind) && number(b.kind)) return !a.nan && !b.nan && a.number < b.number;
  if (a.kind == LegacyKind::kDatetime && b.kind == LegacyKind::kDatetime) return a.micros < b.micros;
  throw HeuristicPandasError("builtins.TypeError", std::string("'<' not supported between instances of '") +
                                                       legacy_type_name(a.kind) + "' and '" +
                                                       legacy_type_name(b.kind) + "'");
}

std::optional<HeuristicsNet> legacy_keyed_heuristics(const EventLog& log, const HeuristicMinerOptions& options) {
  const auto& key = options.activity_attribute;
  std::vector<std::vector<LegacyActivityValue>> values(log.traces.size());
  for (std::size_t t = 0; t < log.traces.size(); ++t)
    for (const auto& event : log.traces[t].events) {
      auto value = legacy_activity_value(event, key);
      if (!value) return std::nullopt;
      values[t].push_back(std::move(*value));
    }
  for (const auto& trace : values)
    for (const auto& value : trace)
      if (value.kind == LegacyKind::kDict)
        throw HeuristicPandasError("builtins.TypeError", "unhashable type: 'dict'");
  for (const auto& trace : values)
    if (trace.size() > 1)
      for (const auto& value : trace)
        if (value.kind == LegacyKind::kAbsent)
          throw HeuristicPandasError("builtins.KeyError", python_str_repr(key));
  if (options.decoration == HeuristicNetDecoration::kPerformance) (void)legacy_classic_waits(log, options, true);
  for (const auto& trace : values)
    for (std::size_t i = 1; i < trace.size(); ++i)
      if (trace[i - 1].kind != LegacyKind::kStr)
        throw HeuristicPandasError("builtins.TypeError", std::string("'") + legacy_type_name(trace[i - 1].kind) +
                                                             "' object is not subscriptable");
  // One node per Python-equal value, named after the first seen.
  std::map<std::string, std::string> label_of;  // identity -> label
  std::map<std::string, std::string> identity_of_label;
  std::map<std::string, const LegacyActivityValue*> value_of_label;
  for (const auto& trace : values)
    for (const auto& value : trace) {
      if (value.kind == LegacyKind::kAbsent) continue;
      if (value.nan) {
        if (label_of.contains("nan")) return std::nullopt;
      }
      const auto [it, inserted] = label_of.emplace(value.identity, value.label);
      if (!inserted) continue;
      const auto [named, fresh] = identity_of_label.emplace(value.label, value.identity);
      if (!fresh && named->second != value.identity) return std::nullopt;
      value_of_label[value.label] = &value;
    }
  EventLog projected = log;
  projected.traces.clear();
  for (std::size_t t = 0; t < log.traces.size(); ++t) {
    if (values[t].size() == 1 && values[t].front().kind == LegacyKind::kAbsent) continue;
    auto trace = log.traces[t];
    for (std::size_t e = 0; e < trace.events.size(); ++e) {
      auto& event = trace.events[e];
      event.activity = label_of.at(values[t][e].identity);
      event.activity_state = FirstClassAttributeState::kPresent;
      event.activity_source_key.clear();
      event.activity_attribute.reset();
    }
    projected.traces.push_back(std::move(trace));
  }
  auto plain = options;
  plain.activity_attribute = "concept:name";
  auto net = discover_heuristics_net(projected, plain);
  const auto appearance = check_successor_sort(net, projected, [&](std::size_t a, std::size_t b) {
    return legacy_activity_less(*value_of_label.at(net.activities[a]), *value_of_label.at(net.activities[b]));
  });
  check_typed_conversion(net, appearance, [&](const std::string& name) {
    const auto found = value_of_label.find(name);
    return found == value_of_label.end() ? std::string("str") : std::string(legacy_type_name(found->second->kind));
  });
  net.activity_attribute = options.activity_attribute;
  net.activity_ids_are_local = true;
  return net;
}

HeuristicsNet discover_heuristics_net(
    const EventLog& log,
    const HeuristicMinerOptions& options) {
  if (options.variant == HeuristicMinerVariant::kPlusPlus)
    return discover_plus_plus(log, options);
  // A custom activity key on an XES upload's log shape reads the legacy
  // event dicts (round GL).
  if (log.dataframe_columns && !log.dataframe_columns->csv_text &&
      !standard_activity_attribute(options.activity_attribute))
    if (auto net = legacy_keyed_heuristics(log, options)) return std::move(*net);
  // pm4py's classic heuristics miner keyed by a typed column of an upload's
  // frame reads each activity as the cell's Python object: it raises
  // TypeError once a directly-follows pair holds one that is no str, and
  // names the nodes str() of the cells otherwise (round BS); a text column
  // reads the same, the case column included.
  if (auto keyed = frame_keyed_activities(log, options.activity_attribute)) {
    const auto error = frame_keyed_heuristics_error(*keyed);
    if (!error.empty()) throw std::invalid_argument("Heuristics Miner: " + error);
    auto projected = options;
    projected.activity_attribute = "concept:name";
    auto net = discover_heuristics_net(keyed->log, projected);
    if (keyed->non_text) {
      // Each label's Python type (one per label: the column's dtype types
      // its cells alike, a gap NaN or NaT).
      std::map<std::string, std::string> type_of;
      for (std::size_t t = 0; t < keyed->types.size(); ++t)
        for (std::size_t e = 0; e < keyed->types[t].size(); ++e)
          type_of.try_emplace(keyed->log.traces[t].events[e].activity, keyed->types[t][e]);
      const auto python_type = [&](const std::string& label) {
        const auto found = type_of.find(label);
        return found == type_of.end() || found->second.empty() ? std::string("str") : found->second;
      };
      const auto number = [](const std::string& type) { return type == "int" || type == "float" || type == "bool"; };
      const auto value = [](const std::string& label) -> long double {
        if (label == "True") return 1.0L;
        if (label == "False") return 0.0L;
        return std::strtold(label.c_str(), nullptr);
      };
      try {
        const auto appearance = check_successor_sort(net, keyed->log, [&](std::size_t a, std::size_t b) {
          const auto& left = net.activities[a];
          const auto& right = net.activities[b];
          const auto ta = python_type(left), tb = python_type(right);
          if (ta == tb && (ta == "str" || ta == "Timestamp")) return left < right;
          if (number(ta) && number(tb)) {
            const auto x = value(left), y = value(right);
            return !std::isnan(x) && !std::isnan(y) && x < y;
          }
          const bool dates = (ta == "Timestamp" || ta == "NaTType") && (tb == "Timestamp" || tb == "NaTType");
          if (dates) return ta == "Timestamp" && tb == "Timestamp" && left < right;
          throw std::invalid_argument("TypeError: '<' not supported between instances of '" + ta + "' and '" + tb + "'");
        });
        check_typed_conversion(net, appearance, python_type);
      } catch (const HeuristicPandasError&) {
        throw;
      } catch (const std::invalid_argument& sorted) {
        throw std::invalid_argument("Heuristics Miner: " + std::string(sorted.what()));
      }
    }
    net.activity_attribute = options.activity_attribute;
    net.activity_ids_are_local = true;
    return net;
  }
  // A CSV log's classic performance view with business hours or a start
  // key reads the frame's cells (round GK).
  if ((classic_cell_clock(options) || classic_custom_clock(options)) && log.dataframe_columns &&
      log.dataframe_columns->csv_text && standard_activity_attribute(options.activity_attribute))
    if (const auto waits = csv_classic_waits(log, *log.dataframe_columns, options, false)) {
      const auto plain = without_cell_clock(options);
      auto net = discover_classic_dfg(build_classic_input(log, plain, false, &*waits), plain);
      net.activity_ids_are_local = false;
      return net;
    }
  // An XES upload's log (the hybrid legacy log; its importer attaches the
  // frame's columns, a frame shape's log has none) reads each pair's cells
  // as its event dicts hold them where Minera's own path would fail on a
  // missing or non-date cell (round GK), and with a custom timestamp key,
  // whose start stays time:timestamp (round GL).
  if (options.decoration == HeuristicNetDecoration::kPerformance && log.dataframe_columns &&
      !log.dataframe_columns->csv_text &&
      standard_activity_attribute(options.activity_attribute))
    if (const auto waits = legacy_classic_waits(log, options)) {
      const auto plain = without_cell_clock(options);
      auto net = discover_classic_dfg(build_classic_input(log, plain, false, &*waits), plain);
      net.activity_ids_are_local = false;
      return net;
    }
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

std::optional<HeuristicsNet> csv_frame_typed_heuristics(const EventDataView& data, const HeuristicMinerOptions& options,
                                                        const DataFrameColumns& columns);

HeuristicsNet discover_heuristics_net_csv_frame(
    const EventDataView& data, const HeuristicMinerOptions& options,
    const DataFrameColumns& columns) {
  if (auto typed = csv_frame_typed_heuristics(data, options, columns)) return std::move(*typed);
  const bool polars = data.kind() == EventDataKind::kLazyEventTable;
  // Round GN (observed black-box): the classic miner reads pm4py's event log
  // of the frame, whose events lack the case columns (moved to the traces)
  // and any column the frame lacks. Keyed by such a column, a directly-follows
  // pair fails (Minera names the key); a log of single-event traces (the
  // rows without a timestamp dropped) leaves a net without activities.
  if (options.variant == HeuristicMinerVariant::kClassic && !standard_activity_attribute(options.activity_attribute) &&
      (data.kind() == EventDataKind::kEventTable || polars)) {
    const auto& key = options.activity_attribute;
    const bool held = key.rfind("case:", 0) != 0 &&
                      std::any_of(columns.columns.begin(), columns.columns.end(),
                                  [&](const DataFrameColumn& column) { return column.key == key; });
    if (!held) {
      const auto log = classic_event_log(data, options);
      if (std::all_of(log.traces.begin(), log.traces.end(), [](const Trace& trace) { return trace.events.size() < 2; })) {
        auto net = discover_classic_dfg(build_classic_input(EventLog{}, options), options);
        net.activity_attribute = key;
        net.activity_ids_are_local = true;
        return net;
      }
    }
  }
  // The classic performance view with business hours or a start key reads
  // the frame's cells (round GK).
  const bool custom_clock = classic_custom_clock(options);
  if ((classic_cell_clock(options, true) || custom_clock) && (data.kind() == EventDataKind::kEventTable || polars)) {
    const auto log = classic_event_log(data, options);
    const auto selected = [&](const std::string& key, std::string_view standard, auto source) {
      return key.empty() || key == standard || log.traces.empty() || log.traces.front().events.empty() ||
             key == source(log.traces.front().events.front());
    };
    const bool keys = selected(options.activity_attribute, "concept:name", [](const Event& e) { return e.activity_source_key; }) &&
                      (custom_clock ||
                       selected(options.timestamp_attribute, "time:timestamp", [](const Event& e) { return e.timestamp_source_key; }));
    if (keys)
      if (const auto waits = csv_classic_waits(log, columns, options, polars)) {
        const auto plain = without_cell_clock(options);
        auto net = discover_classic_dfg(build_classic_input(log, plain, false, &*waits), plain);
        net.activity_ids_are_local = !standard_activity_attribute(plain.activity_attribute);
        return net;
      }
  }
  // pandas' frame measures the classic waits with Timedelta.total_seconds()
  // of the exact difference (round GK).
  if (options.variant == HeuristicMinerVariant::kClassic &&
      options.decoration == HeuristicNetDecoration::kPerformance && data.kind() == EventDataKind::kEventTable) {
    const auto log = classic_event_log(data, options);
    if (!frame_keyed_activities(log, options.activity_attribute)) {
      auto net = discover_classic_dfg(build_classic_input(log, options, false, nullptr, true), options);
      net.activity_ids_are_local = !standard_activity_attribute(options.activity_attribute);
      return net;
    }
  }
  if (options.variant != HeuristicMinerVariant::kPlusPlus || (data.kind() != EventDataKind::kEventTable && !polars))
    return discover_heuristics_net(data, options);
  // The table holds each cell as its text; the frame holds it typed by the
  // column, a pandas NA text or an empty cell as a gap. Polars' copy holds
  // every gap as the one null, so a float column's 1 and 1.0 pair and a
  // missing instance pairs with a missing one (kPythonFrame).
  auto log = classic_event_log(data, options);
  // The frame holds UTC Timestamps (an offset converted), so BusinessHours
  // reads the UTC clock, as on the upload's log (round GK).
  if (options.use_business_hours && !log.dataframe_columns)
    log.dataframe_columns = std::make_shared<const DataFrameColumns>(columns);
  const DataFrameColumn* instance_column = nullptr;
  for (const auto& column : columns.columns)
    if (column.key == options.lifecycle_instance_attribute && column.role == FirstClassAttributeRole::kNone)
      instance_column = &column;
  if (instance_column)
    for (auto& trace : log.traces)
      for (auto& event : trace.events)
        for (auto& cell : event.attributes) {
          if (cell.key != options.lifecycle_instance_attribute || cell.is_null) continue;
          if (cell.value.empty() || is_pandas_csv_na_text(cell.value)) {
            cell.is_null = true;
          } else {
            auto typed = csv_dataframe_cell(*instance_column, cell.value);
            typed.key = cell.key;
            cell = std::move(typed);
          }
        }
  return discover_plus_plus(log, options,
                            polars ? InstanceKeyMode::kPythonFrame
                                   : csv_instance_mode(columns, options.lifecycle_instance_attribute));
}

using detail::xes_frame_value_repr;

// An activity column of an XES upload's frame as pm4py's classic heuristics
// miner reads it (round GM, observed black-box): each cell the Python
// object read_xes's frame holds (classic_frame_cells: text str, a gap NaN, a
// valueless value None in an object column, ints int (float in a float64
// column), floats, booleans, dates Timestamp in a datetime64 column (a gap
// NaT) and datetime.datetime in an object column, <list>s with children
// dicts), or the Polars copy's (an object column str() of each cell, typed
// columns Int64, Float64, Boolean and Datetime as such, a gap or None None).
struct FrameActivityCell {
  std::string type;      // Python type name
  std::string label;     // Python str of the value
  std::string identity;  // equal in Python <=> equal identity
  long double number = 0.0L;
  bool nan = false;
  std::int64_t micros = 0;
  bool missing = false;  // pandas' NaN gap, Polars' None
};

struct FrameTypedActivities {
  std::vector<FrameActivityCell> cells;  // one per row
  std::vector<std::string> names;        // the node of each row: the first label seen of its value
  std::map<std::string, const FrameActivityCell*> value_of;  // node name -> its first value
  bool dict = false;
  std::optional<std::string> missing_name;
};

// nullopt when every cell is text or missing (the frame's own text path) or
// a kind is not modelled.
// `force`: a column of None alone, which the frame's own path leaves out.
std::optional<FrameTypedActivities> frame_typed_activities(const EventTable& table, std::size_t column, bool polars,
                                                           bool force = false) {
  using detail::PlusCellKind;
  auto dtype = ClassicFrameDtype::kObject;
  const auto cells = classic_frame_cells(table, column, false, &dtype);
  if (!cells) return std::nullopt;
  const bool object = dtype == ClassicFrameDtype::kObject || dtype == ClassicFrameDtype::kNoneObject;
  const bool float64 = dtype == ClassicFrameDtype::kFloat || dtype == ClassicFrameDtype::kNaNFloat;
  // The frame's own text path reads text and cells it holds null (a NaN
  // float is a gap here too).
  bool other = force || dtype != ClassicFrameDtype::kString;
  for (std::size_t r = 0; r < cells->size() && !other; ++r) {
    const auto kind = (*cells)[r].kind;
    const auto& row = table.rows[r];
    const bool held = column < row.size() && row[column] && !row[column]->is_null &&
                      row[column]->xes_public_shape != XesPublicShape::kAbsent;
    other = kind != PlusCellKind::kText && (kind != PlusCellKind::kNaN || held);
  }
  if (!other) return std::nullopt;
  FrameTypedActivities out;
  out.cells.reserve(cells->size());
  const auto raw = [&](std::size_t r) -> const EventAttribute& { return *table.rows[r][column]; };
  for (std::size_t r = 0; r < cells->size(); ++r) {
    const auto& cell = (*cells)[r];
    FrameActivityCell value;
    const auto set_number = [&](long double number, const std::string& label) {
      value.number = number;
      value.label = label;
      value.identity = "n" + label;
    };
    switch (cell.kind) {
      case PlusCellKind::kText:
        value.type = "str";
        value.label = cell.text;
        break;
      case PlusCellKind::kNaN: {
        value.type = "float";
        value.label = "nan";
        value.nan = value.missing = true;
        // Python keys NaN by the object (observed black-box): each cell of a
        // float64 column is its own, so is an object column's parsed NaN
        // float, while its gaps share numpy's NaN, as do a str column's.
        const auto& row = table.rows[r];
        const bool held = column < row.size() && row[column] && !row[column]->is_null &&
                          row[column]->xes_public_shape != XesPublicShape::kAbsent;
        value.identity = float64 || (object && held) ? "nan#" + std::to_string(r) : "nan";
        break;
      }
      case PlusCellKind::kNone:
        value.type = "NoneType";
        value.label = "None";
        value.missing = polars;
        break;
      case PlusCellKind::kInt:
        value.type = "int";
        set_number(static_cast<long double>(cell.micros), std::to_string(cell.micros));
        break;
      case PlusCellKind::kFloat: {
        value.type = "float";
        value.number = cell.number;
        value.label = python::float_repr(cell.number);
        value.identity = std::trunc(cell.number) == cell.number && std::fabs(cell.number) < 9.2e18
                             ? "n" + std::to_string(static_cast<long long>(cell.number))
                             : "f" + value.label;
        break;
      }
      case PlusCellKind::kBool:
        value.type = "bool";
        value.number = cell.micros ? 1.0L : 0.0L;
        value.label = cell.micros ? "True" : "False";
        value.identity = cell.micros ? "n1" : "n0";
        break;
      case PlusCellKind::kTimestamp:
      case PlusCellKind::kDatetime:
        value.type = cell.kind == PlusCellKind::kTimestamp ? "Timestamp" : "datetime.datetime";
        value.micros = cell.micros;
        value.label = detail::plus_python_datetime_str(raw(r).value);
        value.identity = "d" + std::to_string(cell.micros);
        break;
      case PlusCellKind::kNaT:
        value.type = "NaTType";
        value.label = "NaT";
        break;
      default:
        if (cell.kind != kDictCell) return std::nullopt;
        {
          const auto repr = xes_frame_value_repr(raw(r));
          if (!repr) return std::nullopt;
          value.type = "dict";
          value.label = *repr;
          out.dict = true;
        }
        break;
    }
    if (polars) {
      // Polars' copy: None for a gap, None or NaT; an object column's cells
      // str() of each; typed columns keep their values (dates datetime).
      if (value.type == "float" && value.nan) value = FrameActivityCell{};
      if (value.type.empty() || value.type == "NoneType" || value.type == "NaTType") {
        value = FrameActivityCell{};
        value.type = "NoneType";
        value.label = "None";
        value.missing = true;
      } else if (object) {
        value.type = "str";
        value.nan = false;
        value.identity.clear();
      } else if (value.type == "Timestamp") {
        value.type = "datetime.datetime";
      }
    }
    if (value.identity.empty()) value.identity = (value.type == "str" ? "s" : value.type + ":") + value.label;
    out.cells.push_back(std::move(value));
  }
  // One node per Python-equal value, named after the first seen in row
  // order (the frame's activity counts).
  std::map<std::string, std::string> label_of;  // identity -> name
  std::map<std::string, std::string> identity_of;  // name -> identity
  out.names.reserve(out.cells.size());
  for (const auto& value : out.cells) {
    auto [at, inserted] = label_of.emplace(value.identity, value.label);
    if (inserted) {
      // Values Python tells apart that print alike (a str '1' beside an int
      // 1) are nodes of one name in pm4py: made unique here.
      while (!identity_of.emplace(at->second, value.identity).second) at->second += '\x1f';
      out.value_of.emplace(at->second, &value);
      if (value.missing && value.nan == !polars && !out.missing_name) out.missing_name = at->second;
    }
    out.names.push_back(at->second);
  }
  return out;
}

// Python's a < b on two activity values (TypeError where Python has none).
bool frame_activity_less(const FrameActivityCell& a, const FrameActivityCell& b) {
  const auto number = [](const std::string& type) { return type == "int" || type == "float" || type == "bool"; };
  if (a.type == "str" && b.type == "str") return a.label < b.label;
  if (number(a.type) && number(b.type)) return !a.nan && !b.nan && a.number < b.number;
  const auto stamp = [](const std::string& type) { return type == "Timestamp" || type == "NaTType"; };
  if (stamp(a.type) && stamp(b.type)) return a.type == "Timestamp" && b.type == "Timestamp" && a.micros < b.micros;
  if (a.type == "datetime.datetime" && b.type == "datetime.datetime") return a.micros < b.micros;
  throw HeuristicPandasError("builtins.TypeError",
                             "'<' not supported between instances of '" + a.type + "' and '" + b.type + "'");
}

// The type pm4py's Petri conversion fails on: the first node in `appearance`
// whose value is no str with an incoming edge (check_typed_conversion).
std::optional<std::string> typed_conversion_type(const HeuristicsNet& net, const std::vector<std::string>& appearance,
                                                 const std::function<std::string(const std::string&)>& python_type) {
  try {
    check_typed_conversion(net, appearance, python_type);
  } catch (const HeuristicPandasError& error) {
    const std::string message = error.what();
    const auto open = message.find('"'), close = message.rfind('"');
    if (open != std::string::npos && close > open) return message.substr(open + 1, close - open - 1);
  }
  return std::nullopt;
}

// A typed activity column's net after mining (round GM): the AND measure
// sorts each node's successors as Python sorts their values, nodes in the
// dependency pass's order; the Petri conversion fails on a node whose value
// is no str with an incoming edge; pandas' NaN node is the missing label.
void finish_typed_frame_net(HeuristicsNet& net, const EventLog& log, const FrameTypedActivities& typed) {
  const auto value_of = [&](const std::string& name) -> const FrameActivityCell& { return *typed.value_of.at(name); };
  const auto appearance = check_successor_sort(net, log, [&](std::size_t a, std::size_t b) {
    return frame_activity_less(value_of(net.activities[a]), value_of(net.activities[b]));
  });
  net.petri_conversion_type = typed_conversion_type(net, appearance, [&](const std::string& name) {
    const auto found = typed.value_of.find(name);
    return found == typed.value_of.end() ? std::string("str") : found->second->type;
  });
  if (typed.missing_name) {
    const auto at = std::find(net.activities.begin(), net.activities.end(), *typed.missing_name);
    if (at != net.activities.end()) net.null_activity = static_cast<std::size_t>(at - net.activities.begin());
  }
}

// A custom activity key naming another column of a CSV upload's frame
// shapes (round GM, observed black-box): pm4py's classic miner reads that
// column of format_dataframe's frame, each cell the Python object read_csv
// typed it as (a gap NaN), or the Polars copy's, as on an XES upload's frame
// (frame_typed_activities); pairs in the frame's case and time order.
std::optional<HeuristicsNet> csv_frame_typed_heuristics(const EventDataView& data, const HeuristicMinerOptions& options,
                                                        const DataFrameColumns& columns) {
  const bool polars = data.kind() == EventDataKind::kLazyEventTable;
  const auto& key = options.activity_attribute;
  if (options.variant != HeuristicMinerVariant::kClassic || standard_activity_attribute(key) ||
      (data.kind() != EventDataKind::kEventTable && !polars) || classic_cell_clock(options, true))
    return std::nullopt;
  const auto column_named = [&](const std::string& name) {
    return std::find_if(columns.columns.begin(), columns.columns.end(),
                        [&](const DataFrameColumn& column) { return column.key == name; });
  };
  // The upload's timestamp column (under its header or as time:timestamp)
  // is the frame's own clock; another timestamp key stays on Minera's path.
  if (classic_custom_clock(options)) {
    const auto clock = column_named(options.timestamp_attribute);
    if (clock == columns.columns.end() || clock->role != FirstClassAttributeRole::kTimestamp) return std::nullopt;
  }
  const auto listed = column_named(key);
  if (listed == columns.columns.end() || listed->role == FirstClassAttributeRole::kCaseId) return std::nullopt;
  auto log = classic_event_log(data, options);
  if (!log.dataframe_columns) log.dataframe_columns = std::make_shared<const DataFrameColumns>(columns);
  const auto framed = dataframe_frame_event_log(log);
  EventTable cells;
  cells.columns = {key};
  for (const auto& trace : framed.traces)
    for (const auto& event : trace.events) {
      auto& row = cells.rows.emplace_back(1);
      for (const auto& attribute : event.attributes) {
        if (attribute.key != key) continue;
        const auto* number = std::get_if<double>(&attribute.typed_value);
        // read_csv's gaps (an empty cell or pandas' NA text): one NaN object
        // in an object column.
        if (!attribute.is_null && !(number && std::isnan(*number)) && !attribute.value.empty() &&
            !is_pandas_csv_na_text(attribute.value))
          row[0] = attribute;
        break;
      }
    }
  if (cells.rows.empty()) return std::nullopt;
  auto typed = frame_typed_activities(cells, 0, polars, true);
  if (!typed) return std::nullopt;
  EventLog projected_log;
  projected_log.attributes = log.attributes;
  std::size_t r = 0;
  for (const auto& trace : log.traces) {
    auto& copy = projected_log.traces.emplace_back(trace);
    for (auto& event : copy.events) {
      event.activity = typed->names[r++];
      event.activity_state = FirstClassAttributeState::kPresent;
      event.activity_source_key.clear();
      event.activity_attribute.reset();
    }
  }
  // The noise cleaning reads each directly-follows pair's first element as
  // text.
  r = 0;
  for (const auto& trace : log.traces) {
    for (std::size_t e = 1; e < trace.events.size(); ++e)
      if (const auto& cell = typed->cells[r + e - 1]; cell.type != "str")
        throw HeuristicPandasError("builtins.TypeError", "'" + cell.type + "' object is not subscriptable");
    r += trace.events.size();
  }
  auto projected = options;
  projected.activity_attribute = "concept:name";
  auto net = options.decoration == HeuristicNetDecoration::kPerformance
                 ? discover_classic_dfg(build_classic_input(projected_log, projected, false, nullptr, !polars), projected)
                 : discover_heuristics_net(projected_log, projected);
  finish_typed_frame_net(net, projected_log, *typed);
  net.activity_attribute = key;
  return net;
}

HeuristicsNet discover_heuristics_net_xes_frame(
    const EventDataView& data,
    const HeuristicMinerOptions& options) {
  const bool polars = data.kind() == EventDataKind::kLazyEventTable;
  const bool plus = options.variant == HeuristicMinerVariant::kPlusPlus;
  const bool performance = !plus && options.decoration == HeuristicNetDecoration::kPerformance;
  // Heuristics++ and the classic performance decoration read the frame's own
  // clock (round GB); another clock (business hours, a start key, another
  // timestamp column) stays on the older path, but Heuristics++ reads any
  // start key and business hours on the frame too (round GJ). An explicitly
  // empty start key is Minera's own fallback.
  // The classic performance view reads business hours and any start key on
  // the frame's cells too (round GK).
  // Another timestamp column likewise (round GL): pandas and Polars
  // subtract its cell from the start key's, time:timestamp by default.
  const bool custom_clock = performance && !standard_timestamp_attribute(options.timestamp_attribute) &&
                            !(options.start_timestamp_attribute && options.start_timestamp_attribute->empty());
  const bool cell_clock = performance && (classic_cell_clock(options) || custom_clock);
  // Heuristics++ reads another timestamp column's cells as well (round GP,
  // observed black-box: start_timestamp as the timestamp key is its own
  // start, so to_interval keeps the log and the abstraction reads it).
  const bool own_clock =
      plus ? !(options.start_timestamp_attribute && options.start_timestamp_attribute->empty())
           : (custom_clock || canonical_timestamp_attribute(options) == "time:timestamp") &&
                 (cell_clock || (!options.use_business_hours && !options.start_timestamp_attribute));
  if ((!plus && options.variant != HeuristicMinerVariant::kClassic) || ((plus || performance) && !own_clock) ||
      (!polars && data.kind() != EventDataKind::kEventTable))
    return discover_heuristics_net(data, options);
  std::optional<EventTable> materialized;
  const EventTable* source = polars ? &materialized.emplace(data.lazy_table()->materialize()) : data.table();
  std::optional<EventTable> storage;
  const auto& table = detail::xes_frame_present_columns(*source, storage);
  auto projected = options;
  projected.activity_attribute = "concept:name";
  const auto finish = [&](HeuristicsNet net) {
    net.activity_attribute = options.activity_attribute;
    return net;
  };
  // pandas' empty frame (no column) converts to an empty log.
  if (table.columns.empty() || table.rows.empty()) return finish(discover_heuristics_net(EventLog{}, projected));
  const auto column_of = [&](const std::string& name) -> std::optional<std::size_t> {
    const auto found = std::find(table.columns.begin(), table.columns.end(), name);
    if (found == table.columns.end()) return std::nullopt;
    return static_cast<std::size_t>(found - table.columns.begin());
  };
  // How the frame's instance cells pair: by the column's text on Polars, by
  // its pandas dtype on pandas (round GG).
  const auto frame_instance_mode = [&] {
    if (polars) return InstanceKeyMode::kText;
    const auto at = column_of(options.lifecycle_instance_attribute);
    if (!at) return InstanceKeyMode::kPythonFrame;
    std::vector<const EventAttribute*> cells;
    for (const auto& row : table.rows) cells.push_back(*at < row.size() && row[*at] ? &*row[*at] : nullptr);
    return pandas_instance_mode(cells);
  }();
  const auto repr = [](const std::string& text) {
    const bool double_quote = text.find('\'') != std::string::npos && text.find('"') == std::string::npos;
    const char quote = double_quote ? '"' : '\'';
    std::string out(1, quote);
    for (const char ch : text) {
      if (ch == '\\' || ch == quote) out += '\\';
      out += ch;
    }
    return out + quote;
  };
  const auto activity_key = canonical_activity_attribute(options);
  const auto timestamp_key = canonical_timestamp_attribute(options);
  const auto case_at = column_of(options.case_id_attribute);
  if (!case_at) throw HeuristicPandasError("builtins.KeyError", repr(options.case_id_attribute));
  const auto activity_at = column_of(activity_key);
  const auto timestamp_at = column_of(timestamp_key);
  // A column of None cells alone (valueless strings or ids, empty <list>s)
  // is a column of the frame too, which the present columns leave out: its
  // index in `source` (round GL).
  const auto none_column = [&](const std::string& name) -> std::optional<std::size_t> {
    if (name.starts_with("case:")) return std::nullopt;
    const auto found = std::find(source->columns.begin(), source->columns.end(), name);
    if (found == source->columns.end()) return std::nullopt;
    const auto at = static_cast<std::size_t>(found - source->columns.begin());
    for (const auto& row : source->rows)
      if (at < row.size() && row[at] && row[at]->is_null) return at;
    return std::nullopt;
  };
  const auto custom_none_at = custom_clock && !timestamp_at ? none_column(timestamp_key) : std::nullopt;
  const auto value = [](const EventTable::Row& row, std::size_t at) -> const std::string* {
    if (at >= row.size() || !row[at] || row[at]->is_null) return nullptr;
    return &row[at]->value;
  };
  // Heuristics++ reads every event's activity and timestamp: their columns
  // are required (round GB). With a start column the abstraction sorts the
  // first trace by its starts before it reads them (round GO, observed
  // black-box): that sort's TypeError comes first.
  if (plus && (!activity_at || !timestamp_at)) {
    const std::string start_key = options.start_timestamp_attribute.value_or("start_timestamp");
    const auto start_column = start_key.starts_with("case:") || start_key == timestamp_key ? std::nullopt
                                                                                            : column_of(start_key);
    if (start_column) {
      if (const auto starts = detail::xes_frame_cells(table, *start_column, polars)) {
        std::vector<std::size_t> first;
        const auto case_key = [&](std::size_t r) -> std::optional<std::string> {
          const auto* id = value(table.rows[r], *case_at);
          return id ? std::optional<std::string>(*id) : std::nullopt;
        };
        for (std::size_t r = 0; r < table.rows.size(); ++r)
          if (case_key(r) == case_key(0)) first.push_back(r);
        detail::python_list_sort(first, [&](std::size_t a, std::size_t b) {
          return detail::plus_compare((*starts)[a], (*starts)[b], false);
        });
      }
    }
    throw HeuristicPandasError("builtins.KeyError", repr(!activity_at ? activity_key : timestamp_key));
  }
  const auto lifecycle_at = column_of("lifecycle:transition");
  // Heuristics++'s interval input (round GC, observed black-box): with a
  // start_timestamp column every event holds the key, so pm4py's
  // to_interval keeps the log as it is (the lifecycle unread) and the
  // abstraction reads each start as the frame's Python object. A configured
  // start key likewise (round GJ): the timestamp column itself, or another
  // event column (the `case:` columns become trace attributes).
  const std::string interval_start_key = options.start_timestamp_attribute.value_or("start_timestamp");
  std::optional<std::size_t> start_at;
  if (plus && !interval_start_key.starts_with("case:"))
    start_at = interval_start_key == timestamp_key ? timestamp_at : column_of(interval_start_key);
  // Round GO: both columns as read_xes types them (xes_frame_typed.hpp), so
  // timestamp columns of other kinds raise pm4py's own errors in the
  // abstraction's sort, eventually-follows comparison, performance
  // subtraction and timestamp() calls (check_plus_interval_cells).
  const auto time_cells = plus && timestamp_at ? detail::xes_frame_cells(table, *timestamp_at, polars)
                                               : std::optional<std::vector<detail::PlusCell>>();
  if (start_at) {
    const auto start_cells = detail::xes_frame_cells(table, *start_at, polars);
    if (!time_cells || !start_cells) return discover_heuristics_net(data, options);
    // Cases in order of their first row, rows in frame order; the rows
    // without a case id are one trace.
    std::map<std::optional<std::string>, std::size_t> trace_index;
    std::vector<std::vector<std::size_t>> interval_traces;
    for (std::size_t row = 0; row < table.rows.size(); ++row) {
      const auto& cells = table.rows[row];
      const bool named = *case_at < cells.size() && cells[*case_at] && !cells[*case_at]->is_null;
      const auto key = named ? std::optional<std::string>(cells[*case_at]->value) : std::nullopt;
      const auto [it, inserted] = trace_index.try_emplace(key, interval_traces.size());
      if (inserted) interval_traces.emplace_back();
      interval_traces[it->second].push_back(row);
    }
    detail::check_plus_interval_cells(interval_traces, *start_cells, *time_cells, options.use_business_hours);
  }
  std::optional<FrameTypedActivities> typed;
  if (!plus) {
    // The classic reads activities, and the performance decoration
    // timestamps, only in directly-follows pairs: a case of two or more
    // events raises KeyError for a missing column (activity first);
    // otherwise a missing activity column leaves no activity at all.
    std::map<std::optional<std::string>, std::size_t> events_of;
    bool repeated = false;
    for (const auto& row : table.rows) {
      const auto* case_id = value(row, *case_at);
      if (++events_of[case_id ? std::optional<std::string>(*case_id) : std::nullopt] > 1) {
        repeated = true;
        break;
      }
    }
    // A column of None alone is a column of pandas' frame and its Polars
    // copy too (round GM).
    const auto activity_none_at = activity_at ? std::nullopt : none_column(activity_key);
    if (repeated && !activity_at && !activity_none_at)
      throw HeuristicPandasError("builtins.KeyError", repr(activity_key));
    // An activity column of other kinds than text reads each cell's Python
    // object (round GM); pandas counts the activities first, where a dict
    // is unhashable.
    if (activity_at || activity_none_at) {
      typed = frame_typed_activities(activity_at ? table : *source, activity_at ? *activity_at : *activity_none_at,
                                     polars, !activity_at);
      if (!typed && !activity_at) return discover_heuristics_net(data, options);
      if (typed && typed->dict && !polars) throw HeuristicPandasError("builtins.TypeError", "unhashable type: 'dict'");
    }
    // Another timestamp column raises at the first pair, after the start
    // key (classic_cell_waits).
    // The pair reads the later event's start before the earlier one's
    // timestamp: a start key naming no event column raises first (round
    // GP, observed black-box).
    if (repeated && performance && !timestamp_at && !custom_clock) {
      const auto& start_key = options.start_timestamp_attribute;
      if (start_key && !start_key->empty() && *start_key != timestamp_key &&
          (start_key->starts_with("case:") || (!column_of(*start_key) && !none_column(*start_key))))
        throw HeuristicPandasError("builtins.KeyError", repr(*start_key));
      throw HeuristicPandasError("builtins.KeyError", repr(timestamp_key));
    }
    if (!activity_at && !typed) return finish(discover_heuristics_net(EventLog{}, projected));
  }
  const auto missing_time = [&](const EventTable::Row& row) {
    if (!timestamp_at || *timestamp_at >= row.size() || !row[*timestamp_at]) return true;
    return row[*timestamp_at]->is_null || is_typed_nat(*row[*timestamp_at]);
  };
  // Heuristics++'s to_interval without a start column (round GE, observed
  // black-box): case by case (Polars: the frame's first-row order, the
  // missing case among them), event by event in row order, it lower-cases
  // the event's transition (a missing one, NaN or None, raises
  // AttributeError); a complete pairs with the first open start of its
  // activity and instance, or with itself, and subtracts the start from the
  // complete. Polars' None raises TypeError there; pandas' NaT subtracts, and
  // the abstraction then reads each interval's timestamp(): NaT raises
  // ValueError, after every transition was read. A start left open and any
  // other transition form no interval, whatever their time. Without a
  // lifecycle column every event is a complete paired with itself.
  std::vector<bool> interval_free(table.rows.size(), false);
  if (plus && !start_at) {
    const auto instance_at = column_of(options.lifecycle_instance_attribute);
    std::vector<std::optional<std::string>> case_order;
    std::map<std::optional<std::string>, std::vector<std::size_t>> rows_of;
    for (std::size_t r = 0; r < table.rows.size(); ++r) {
      const auto* case_id = value(table.rows[r], *case_at);
      const auto key = case_id ? std::optional<std::string>(*case_id) : std::nullopt;
      auto& rows = rows_of[key];
      if (rows.empty()) case_order.push_back(key);
      rows.push_back(r);
    }
    // With business hours pandas' NaT raises at its interval (below), so
    // the transitions are read in order there too.
    const bool business_nat = !polars && options.use_business_hours;
    // Round GO: with the timestamp cells as read_xes types them, a
    // subtraction of an earlier interval can raise first, so the
    // transitions are read in order (the loop below).
    if (lifecycle_at && !polars && !business_nat && !time_cells &&
        std::any_of(table.rows.begin(), table.rows.end(),
                    [&](const auto& row) { return value(row, *lifecycle_at) == nullptr; }))
      throw HeuristicPandasError("builtins.AttributeError", "'float' object has no attribute 'lower'");
    bool nat_interval = false;
    const auto type_of = [](bool missing) { return std::string(missing ? "NoneType" : "datetime.datetime"); };
    for (const auto& key : case_order) {
      using StartKey = std::pair<std::optional<std::string>, std::string>;
      std::map<StartKey, std::deque<std::size_t>> open;
      for (const auto r : rows_of[key]) {
        const auto& row = table.rows[r];
        std::string lifecycle = "complete";
        if (lifecycle_at) {
          const auto* transition = value(row, *lifecycle_at);
          if (!transition)
            throw HeuristicPandasError("builtins.AttributeError", std::string("'") + (polars ? "NoneType" : "float") +
                                                                      "' object has no attribute 'lower'");
          lifecycle = ascii_lower(*transition);
        }
        const auto* activity = value(row, *activity_at);
        const EventAttribute* instance =
            instance_at && *instance_at < row.size() && row[*instance_at] ? &*row[*instance_at] : nullptr;
        const StartKey start_key{activity ? std::optional<std::string>(*activity) : std::nullopt,
                                 lifecycle_instance_key(instance, frame_instance_mode, r)};
        if (lifecycle == "start") {
          open[start_key].push_back(r);
          interval_free[r] = true;
          continue;
        }
        if (lifecycle != "complete") {
          interval_free[r] = true;
          continue;
        }
        std::size_t start = r;
        if (auto found = open.find(start_key); found != open.end() && !found->second.empty()) {
          start = found->second.front();
          found->second.pop_front();
          interval_free[start] = false;
        }
        // Round GO: the complete's cell minus the start's, as read_xes types
        // the timestamp column (plus_subtract_seconds: numbers, text, None
        // and dicts raise; pandas' NaT subtracts).
        if (time_cells) {
          if (detail::plus_subtract_seconds((*time_cells)[r], (*time_cells)[start])) {
            if (business_nat) throw HeuristicPandasError("builtins.ValueError", "cannot convert float NaN to integer");
            nat_interval = true;
          }
          continue;
        }
        const bool complete_missing = missing_time(row), start_missing = missing_time(table.rows[start]);
        if (complete_missing || start_missing) {
          if (polars)
            throw HeuristicPandasError("builtins.TypeError", "unsupported operand type(s) for -: '" +
                                                                 type_of(complete_missing) + "' and '" +
                                                                 type_of(start_missing) + "'");
          // to_interval's BusinessHours of the interval (round GJ).
          if (business_nat) throw HeuristicPandasError("builtins.ValueError", "cannot convert float NaN to integer");
          nat_interval = true;
        }
      }
    }
    if (nat_interval) throw HeuristicPandasError("builtins.ValueError", "NaTType does not support timestamp");
  }
  // Python's str of the missing label, unique among the named activities.
  std::set<std::string> named;
  bool any_missing = false;
  if (!typed)
    for (const auto& row : table.rows) {
      if (const auto* activity = value(row, *activity_at)) named.insert(*activity);
      else any_missing = true;
    }
  std::string null_label = polars ? "None" : "nan";
  while (named.contains(null_label)) null_label += '\x1f';
  const std::string null_type = polars ? "NoneType" : "float";
  const bool timed = plus || performance;
  EventLog log;
  std::vector<std::vector<std::size_t>> trace_rows;
  std::map<std::optional<std::string>, std::size_t> trace_of;
  for (std::size_t r = 0; r < table.rows.size(); ++r) {
    const auto& row = table.rows[r];
    // An open start or another transition without a time forms no interval
    // (above); the log's interval pairing would reject its missing time.
    if (interval_free[r] && missing_time(row)) continue;
    const auto* case_id = value(row, *case_at);
    const auto key = case_id ? std::optional<std::string>(*case_id) : std::nullopt;
    const auto [it, inserted] = trace_of.try_emplace(key, log.traces.size());
    if (inserted) trace_rows.emplace_back();
    trace_rows[it->second].push_back(r);
    if (inserted) {
      Trace trace;
      trace.case_id = key.value_or("");
      trace.case_id_state = key ? FirstClassAttributeState::kPresent : FirstClassAttributeState::kNull;
      trace.case_id_source_key = options.case_id_attribute;
      log.traces.push_back(std::move(trace));
    }
    Event event;
    if (typed) {
      event.activity = typed->names[r];
    } else {
      const auto* activity = value(row, *activity_at);
      event.activity = activity ? *activity : null_label;
    }
    event.activity_state = FirstClassAttributeState::kPresent;
    if (timed && timestamp_at) {
      if (missing_time(row)) {
        event.timestamp_state = FirstClassAttributeState::kNull;
      } else {
        event.timestamp = row[*timestamp_at]->value;
        event.timestamp_state = FirstClassAttributeState::kPresent;
        event.timestamp_attribute = *row[*timestamp_at];
      }
      event.timestamp_source_key = timestamp_key;
    }
    if (plus) {
      if (lifecycle_at && !start_at) {
        event.lifecycle = *value(row, *lifecycle_at);
        event.lifecycle_state = FirstClassAttributeState::kPresent;
      }
      // The lifecycle pairing reads the other attributes (concept:instance).
      for (std::size_t column = 0; column < table.columns.size(); ++column)
        if (column != *case_at && column != *activity_at && column != *timestamp_at && column != lifecycle_at &&
            value(row, column)) {
          auto attribute = *row[column];
          attribute.key = table.columns[column];
          event.attributes.push_back(std::move(attribute));
        }
      // A start key naming the timestamp column (start_timestamp as the
      // timestamp key) reads that same cell: pm4py sees a start column, so
      // the intervals keep frame order (round GP, observed black-box).
      if (start_at && timestamp_at && *start_at == *timestamp_at && value(row, *timestamp_at) &&
          !standard_timestamp_attribute(interval_start_key)) {
        auto attribute = *row[*timestamp_at];
        attribute.key = interval_start_key;
        event.attributes.push_back(std::move(attribute));
      }
    }
    log.traces[it->second].events.push_back(std::move(event));
  }
  if (plus) {
    // Python compares two labels while it keys a concurrency pair (each
    // overlapping pair of a trace's time-sorted intervals, trace by trace)
    // and while it reads each kept eventually-follows pair's concurrency
    // (sources in the order their activity first appears, each source's
    // targets in the order they follow it first): the missing label beside
    // a named one raises TypeError, and on Polars None beside anything
    // (round GB, observed black-box).
    if (any_missing) {
      const auto type_of = [&](const std::string& label) { return label == null_label ? null_type : std::string("str"); };
      const auto less = [&](const std::string& left, const std::string& right) {
        const bool left_null = left == null_label, right_null = right == null_label;
        if (polars ? (left_null || right_null) : left_null != right_null)
          throw HeuristicPandasError("builtins.TypeError", "'<' not supported between instances of '" +
                                                               type_of(left) + "' and '" + type_of(right) + "'");
      };
      std::vector<std::string> order;
      std::set<std::string> known;
      std::map<std::string, std::uint64_t> occurrences;
      std::map<NamePair, std::uint64_t> follows;
      std::map<std::string, std::vector<std::string>> targets;
      for (const auto& trace : log.traces) {
        const auto converted = to_intervals(trace, projected, start_at.has_value(), false, frame_instance_mode);
        const auto& intervals = converted.sorted;
        for (const auto& interval : converted.performance_order) {
          if (known.insert(interval.activity).second) order.push_back(interval.activity);
          ++occurrences[interval.activity];
        }
        for (std::size_t i = 0; i < intervals.size(); ++i)
          for (std::size_t j = i + 1; j < intervals.size(); ++j) {
            if (!(std::max(start_instant(intervals[i]), start_instant(intervals[j])) <=
                  std::min(complete_instant(intervals[i]), complete_instant(intervals[j]))))
              break;
            less(intervals[j].activity, intervals[i].activity);
          }
        for (std::size_t i = 0; i < intervals.size(); ++i)
          for (std::size_t j = i + 1; j < intervals.size(); ++j)
            if (complete_instant(intervals[i]) <= start_instant(intervals[j])) {
              if (follows[{intervals[i].activity, intervals[j].activity}]++ == 0)
                targets[intervals[i].activity].push_back(intervals[j].activity);
              break;
            }
      }
      const PlusCountThreshold min_activity{options.min_activity_count, options.plusplus_min_activity_count};
      const PlusCountThreshold min_frequency{options.min_frequency, options.plusplus_min_frequency};
      for (const auto& from : order) {
        const auto found = targets.find(from);
        if (found == targets.end()) continue;
        for (const auto& to : found->second)
          if (min_activity.accepts(occurrences[from]) && min_activity.accepts(occurrences[to]) &&
              min_frequency.accepts(follows[{from, to}]))
            less(to, from);
      }
    }
    // The frame's instance cells: pandas' Python objects, Polars' column text.
    auto net = finish(discover_plus_plus(log, projected, frame_instance_mode));
    const auto null_at = std::find(net.activities.begin(), net.activities.end(), null_label);
    if (any_missing && null_at != net.activities.end())
      net.null_activity = static_cast<std::size_t>(null_at - net.activities.begin());
    return net;
  }
  // With business hours or a start key the decoration measures each pair on
  // the frame's cells (round GK, classic_cell_waits), before the noise
  // cleaning; a column it does not model keeps the older path.
  std::optional<std::vector<std::vector<double>>> cell_waits;
  // So does the plain performance view when the timestamp column holds
  // other kinds than dates (round GL), and since round GO whatever it holds:
  // a dates column's NaN floats and valueless cells are NaT there (pandas
  // reads a zero wait, Polars' None raises), as read_xes types the column.
  const bool odd_clock =
      performance && !cell_clock && timestamp_at && !options.use_business_hours && !options.start_timestamp_attribute;
  if (cell_clock || odd_clock) {
    // Without the timestamp column no case holds a pair (above).
    const bool complete_missing = custom_clock && !timestamp_at && !custom_none_at;
    auto completes = custom_none_at ? classic_frame_cells(*source, *custom_none_at, polars)
                     : !timestamp_at ? std::optional<std::vector<detail::PlusCell>>(
                                           std::vector<detail::PlusCell>(table.rows.size()))
                                     : classic_frame_cells(table, *timestamp_at, polars);
    if (!completes) return discover_heuristics_net(data, options);
    const std::string key = options.start_timestamp_attribute && !options.start_timestamp_attribute->empty()
                                ? *options.start_timestamp_attribute
                            : custom_clock ? std::string("time:timestamp")
                                           : timestamp_key;
    std::optional<std::vector<detail::PlusCell>> starts;
    if (key == timestamp_key) {
      starts = completes;
    } else if (!key.starts_with("case:")) {
      if (const auto at = column_of(key)) {
        starts = classic_frame_cells(table, *at, polars);
        if (!starts) return discover_heuristics_net(data, options);
      } else if (const auto none_at = none_column(key)) {
        starts = classic_frame_cells(*source, *none_at, polars);
        if (!starts) return discover_heuristics_net(data, options);
      }
    }
    std::vector<std::vector<detail::PlusCell>> start_rows, complete_rows;
    for (const auto& rows : trace_rows) {
      auto& start_row = start_rows.emplace_back();
      auto& complete_row = complete_rows.emplace_back();
      for (const auto r : rows) {
        complete_row.push_back((*completes)[r]);
        start_row.push_back(starts ? (*starts)[r] : detail::PlusCell{});
      }
    }
    const auto slots = options.use_business_hours
                           ? detail::normalize_business_hour_slots(options.business_hour_slots,
                                                                   options.business_hour_slots_explicit,
                                                                   "Heuristics Miner")
                           : std::vector<BusinessHourSlot>{};
    cell_waits = classic_cell_waits(start_rows, complete_rows, options.use_business_hours, slots,
                                    !starts            ? std::optional<std::string>(repr(key))
                                    : complete_missing ? std::optional<std::string>(repr(timestamp_key))
                                                       : std::nullopt,
                                    polars);
  }
  // Polars' performance decoration subtracts each directly-follows pair's
  // timestamps, case by case, before the noise cleaning: None on either side
  // raises TypeError (round GB, observed black-box).
  if (performance && polars && !cell_waits)
    for (const auto& trace : log.traces)
      for (std::size_t e = 1; e < trace.events.size(); ++e) {
        const bool later = trace.events[e].timestamp_state == FirstClassAttributeState::kNull;
        const bool earlier = trace.events[e - 1].timestamp_state == FirstClassAttributeState::kNull;
        if (later || earlier)
          throw HeuristicPandasError("builtins.TypeError",
                                     std::string("unsupported operand type(s) for -: '") +
                                         (later ? "NoneType" : "datetime.datetime") + "' and '" +
                                         (earlier ? "NoneType" : "datetime.datetime") + "'");
      }
  // The noise cleaning reads each DFG pair's first element as text.
  if (any_missing)
    for (const auto& trace : log.traces)
      for (std::size_t e = 1; e < trace.events.size(); ++e)
        if (trace.events[e - 1].activity == null_label)
          throw HeuristicPandasError("builtins.TypeError", "'" + null_type + "' object is not subscriptable");
  // A typed column likewise: the first pair in log order whose first
  // element is no str (round GM).
  if (typed)
    for (const auto& rows : trace_rows)
      for (std::size_t e = 1; e < rows.size(); ++e)
        if (const auto& cell = typed->cells[rows[e - 1]]; cell.type != "str")
          throw HeuristicPandasError("builtins.TypeError", "'" + cell.type + "' object is not subscriptable");
  auto net = finish([&] {
    if (!performance) return discover_heuristics_net(log, projected);
    if (cell_waits) {
      const auto plain = without_cell_clock(projected);
      auto decorated = discover_classic_dfg(build_classic_input(log, plain, true, &*cell_waits), plain);
      decorated.activity_ids_are_local = !standard_activity_attribute(plain.activity_attribute);
      return decorated;
    }
    // A pair with a NaT reads as a zero wait.
    auto decorated = discover_classic_dfg(build_classic_input(log, projected, true, nullptr, !polars), projected);
    decorated.activity_ids_are_local = !standard_activity_attribute(projected.activity_attribute);
    return decorated;
  }());
  if (typed) {
    finish_typed_frame_net(net, log, *typed);
    return net;
  }
  const auto null_at = std::find(net.activities.begin(), net.activities.end(), null_label);
  if (null_at == net.activities.end()) return net;
  const auto null_id = static_cast<std::size_t>(null_at - net.activities.begin());
  // Each node sorts the successors its dependencies accept (DFG order) by
  // name for the AND measure: a missing label beside a named one raises
  // TypeError. The nodes take their turns in the order the dependency pass
  // creates them (round GA): rows in the order their source first leads a
  // pair of the cleaned DFG, each row's targets in pair order, an accepted
  // edge adding its source, then its target.
  std::map<std::pair<std::string, std::string>, std::size_t> pair_order;
  std::vector<std::pair<std::string, std::string>> pairs;
  for (const auto& trace : log.traces)
    for (std::size_t e = 1; e < trace.events.size(); ++e)
      if (pair_order.try_emplace({trace.events[e - 1].activity, trace.events[e].activity}, pair_order.size()).second)
        pairs.emplace_back(trace.events[e - 1].activity, trace.events[e].activity);
  std::vector<std::string> rows;
  for (const auto& pair : pairs)
    if (net.cleaned_dfg.contains(pair) && std::find(rows.begin(), rows.end(), pair.first) == rows.end())
      rows.push_back(pair.first);
  const auto id_of = [&](const std::string& name) -> std::optional<std::size_t> {
    const auto found = std::find(net.activities.begin(), net.activities.end(), name);
    if (found == net.activities.end()) return std::nullopt;
    return static_cast<std::size_t>(found - net.activities.begin());
  };
  const auto accepted = [&](const std::pair<std::string, std::string>& pair) {
    const auto source = id_of(pair.first), target = id_of(pair.second);
    if (!source || !target) return false;
    return std::any_of(net.edges.begin(), net.edges.end(), [&](const auto& edge) {
      return edge.source == *source && edge.target == *target && !edge.length_two_loop;
    });
  };
  std::vector<std::string> appearance;
  for (const auto& row : rows)
    for (const auto& pair : pairs) {
      if (pair.first != row || !net.cleaned_dfg.contains(pair) || !accepted(pair)) continue;
      for (const auto* name : {&pair.first, &pair.second})
        if (std::find(appearance.begin(), appearance.end(), *name) == appearance.end()) appearance.push_back(*name);
    }
  for (const auto& name : appearance) {
    const auto found = std::find(net.activities.begin(), net.activities.end(), name);
    if (found == net.activities.end()) continue;
    const auto id = static_cast<std::size_t>(found - net.activities.begin());
    std::vector<std::size_t> successors;
    bool reaches_null = false;
    // The length-two loop edges join after the AND measures.
    for (const auto& edge : net.edges)
      if (edge.source == id && !edge.length_two_loop) {
        successors.push_back(edge.target);
        reaches_null = reaches_null || edge.target == null_id;
      }
    if (successors.size() < 2 || !reaches_null) continue;
    const auto rank = [&](std::size_t target) {
      const auto at = pair_order.find({name, net.activities[target]});
      return at == pair_order.end() ? pair_order.size() : at->second;
    };
    std::stable_sort(successors.begin(), successors.end(),
                     [&](std::size_t a, std::size_t b) { return rank(a) < rank(b); });
    detail::python_list_sort(successors, [&](std::size_t a, std::size_t b) {
      if ((a == null_id) != (b == null_id))
        throw HeuristicPandasError("builtins.TypeError",
                                   "'<' not supported between instances of '" +
                                       std::string(a == null_id ? null_type : "str") + "' and '" +
                                       std::string(b == null_id ? null_type : "str") + "'");
      return net.activities[a] < net.activities[b];
    });
  }
  net.null_activity = null_id;
  // The Petri conversion fails on the missing label with an incoming edge
  // (round GM, observed black-box).
  if (std::any_of(net.edges.begin(), net.edges.end(), [&](const auto& edge) { return edge.target == null_id; }))
    net.petri_conversion_type = null_type;
  return net;
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
