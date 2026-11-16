#include "minera/log_to_target.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <set>
#include <unordered_map>

#include "minera/dataframe_columns.hpp"
#include "minera/frame_keys.hpp"
#include "minera/timestamp.hpp"
#include "frame_cells.hpp"
#include "frame_times.hpp"
#include "python_scalar.hpp"
#include "reference_activity_selector.hpp"

namespace minera {
namespace {

bool is_known_variant(LogToTargetVariant variant) {
  return variant == LogToTargetVariant::NEXT_ACTIVITY ||
         variant == LogToTargetVariant::NEXT_TIME ||
         variant == LogToTargetVariant::REMAINING_TIME;
}

[[noreturn]] void key_error(const std::string& key) {
  throw std::invalid_argument("KeyError: '" + key + "'");
}

// One activity value as the reference's log holds it: a Python scalar, NaT
// (pandas' missing date, one object) or a NaN. A NaN is its own object in a
// float64 column (`nan_cell` tells the cells apart) and pandas' one np.nan in
// a text or object column (round BO, observed black-box).
struct Value {
  python::Scalar scalar;
  bool nat = false;
  bool nan = false;
  std::size_t nan_cell = 0;  // the cell's ordinal; 0 for a shared np.nan
};

// `a == b` as a list lookup or a set sees it: the same object first, then
// Python equality (a NaN equals only itself, NaT only itself).
bool same_value(const Value& a, const Value& b) {
  if (a.nat || b.nat) return a.nat && b.nat;
  if (a.nan || b.nan) return a.nan && b.nan && a.nan_cell == b.nan_cell;
  return python::equal(a.scalar, b.scalar);
}

std::string value_str(const Value& value) {
  if (value.nat) return "NaT";
  if (value.nan) return "nan";
  return python::str(value.scalar);
}

// Python's `a < b` inside sorted(): NaN and NaT order with nothing of their
// kind (False), unorderable types raise TypeError.
bool value_less(const Value& a, const Value& b) {
  if (a.nat || b.nat) {
    const auto& other = a.nat ? b : a;
    if (other.nat || (!other.nan && other.scalar.kind == python::Scalar::Kind::kDate)) return false;
    const std::string other_type = other.nan ? "float" : python::type_name(other.scalar);
    throw python::TypeError("'<' not supported between instances of '" +
                            (a.nat ? std::string("NaTType") : other_type) + "' and '" +
                            (a.nat ? other_type : std::string("NaTType")) + "'");
  }
  if (a.nan || b.nan) {
    const auto& other = a.nan ? b : a;
    if (other.nan || python::is_number(other.scalar)) return false;
    const auto other_type = python::type_name(other.scalar);
    throw python::TypeError("'<' not supported between instances of '" +
                            (a.nan ? std::string("float") : other_type) + "' and '" +
                            (a.nan ? other_type : std::string("float")) + "'");
  }
  return python::compare(a.scalar, b.scalar) < 0;
}

// How the reference reads the events of the log it converts (round CU,
// observed black-box with log_to_target on CSV and XES uploads): an upload
// is read through its frame's log (convert_to_event_log), whose events hold
// every frame column with its typed cell, the case columns excepted; a
// native log through its events' fields and attributes.
class EventReader {
 public:
  explicit EventReader(const EventLog& log) : log_(log) {
    if (!log.dataframe_columns) return;
    frame_ = dataframe_frame_event_log(log);
    if (frame_->traces.size() != log.traces.size()) {
      frame_.reset();
      return;
    }
    for (std::size_t t = 0; t < log.traces.size(); ++t)
      if (frame_->traces[t].events.size() != log.traces[t].events.size()) {
        frame_.reset();
        return;
      }
    keys_ = frame_keys(*frame_, FrameKeyScope::kEventLog);
  }

  Value activity(std::size_t t, std::size_t e, const std::string& requested) {
    if (!frame_) {
      const auto& event = log_.traces[t].events[e];
      if (requested.empty() || requested == "activity" || requested == "concept:name")
        return {python::text(event.activity)};
      const auto* value = reference_selected_field(event, requested);
      if (!value) key_error(requested);
      return {python::text(*value)};
    }
    const auto key = frame_times::activity_column(requested);
    if (!keys_.contains(key)) key_error(key);
    const auto& trace = frame_->traces[t];
    if (auto cell = frame_scalar(*frame_, trace, trace.events[e], key, FrameKeyScope::kEventLog))
      return {std::move(*cell)};
    Value missing;
    const auto type = frame_column_type(*frame_, key);
    if (type == DataFrameColumnType::kDate) {
      missing.nat = true;
    } else {
      missing.nan = true;
      if (type == DataFrameColumnType::kFloat64) missing.nan_cell = ++nan_cells_;
    }
    return missing;
  }

  // Timestamp.timestamp() of the event's cell; Python's error otherwise.
  double instant(std::size_t t, std::size_t e, const std::string& requested) {
    if (!frame_) {
      const auto& event = log_.traces[t].events[e];
      const std::string* text = nullptr;
      if (requested == "time:timestamp" || requested == "timestamp") text = &event.timestamp;
      else if (!(text = event.attribute(requested))) key_error(requested);
      const auto parsed = parse_timestamp(*text);
      if (!parsed || !std::isfinite(*parsed))
        throw std::invalid_argument("log_to_target: temporal variant requires valid timestamps");
      return *parsed;
    }
    const auto key = frame_times::timestamp_column(requested);
    if (!keys_.contains(key)) key_error(key);
    const auto& trace = frame_->traces[t];
    const auto cell = frame_scalar(*frame_, trace, trace.events[e], key, FrameKeyScope::kEventLog);
    if (cell && cell->kind == python::Scalar::Kind::kDate) return frame_times::epoch_seconds(cell->date);
    if (!cell) {
      if (frame_column_type(*frame_, key) == DataFrameColumnType::kDate)
        throw std::invalid_argument("ValueError: NaTType does not support timestamp");
      throw std::invalid_argument("AttributeError: 'float' object has no attribute 'timestamp'");
    }
    throw std::invalid_argument("AttributeError: '" + python::type_name(*cell) +
                                "' object has no attribute 'timestamp'");
  }

 private:
  const EventLog& log_;
  std::optional<EventLog> frame_;
  std::set<std::string> keys_;
  std::size_t nan_cells_ = 0;
};

}  // namespace

LogToTargetResult log_to_target(const EventLog& log,
                                const LogToTargetOptions& options) {
  if (!is_known_variant(options.variant)) {
    throw std::invalid_argument("log_to_target: unknown variant");
  }
  if (options.variant == LogToTargetVariant::NEXT_ACTIVITY &&
      options.activity_key.empty()) {
    throw std::invalid_argument("log_to_target: activity_key must not be empty");
  }
  if (options.variant != LogToTargetVariant::NEXT_ACTIVITY &&
      options.timestamp_key.empty()) {
    throw std::invalid_argument("log_to_target: timestamp_key must not be empty");
  }

  LogToTargetResult result;
  result.variant = options.variant;
  result.trace_count = log.traces.size();
  result.padded = options.enable_padding;
  const bool explicit_classes =
      options.explicit_activities || !options.activities.empty();
  EventReader reader(log);

  // NEXT_ACTIVITY reads every event's activity first (round CU, observed
  // black-box): a key the events lack raises KeyError even beside an explicit
  // class list, and the derived classes are sorted(set(...)) of the values.
  std::vector<std::vector<Value>> activities;
  std::vector<Value> classes;
  if (options.variant == LogToTargetVariant::NEXT_ACTIVITY) {
    activities.resize(log.traces.size());
    for (std::size_t t = 0; t < log.traces.size(); ++t)
      for (std::size_t e = 0; e < log.traces[t].events.size(); ++e)
        activities[t].push_back(reader.activity(t, e, options.activity_key));
    // The set keeps the first of equal values. A float column's NaNs are
    // distinct objects that CPython hashes by address, so their places (and
    // the numbers around them) in sorted() vary from run to run; Minera puts
    // them last in cell order, where they sort to (declared). The sort runs
    // beside an explicit class list too, so its TypeError still raises.
    std::vector<Value> nans;
    for (const auto& trace : activities)
      for (const auto& value : trace) {
        auto& bucket = value.nan ? nans : classes;
        if (std::none_of(bucket.begin(), bucket.end(),
                         [&](const Value& seen) { return same_value(seen, value); }))
          bucket.push_back(value);
      }
    classes.insert(classes.end(), nans.begin(), nans.end());
    try {
      python::sort(classes, value_less);
    } catch (const python::TypeError& error) {
      throw std::invalid_argument(std::string("TypeError: ") + error.what());
    }
    if (explicit_classes) {
      classes.clear();
      for (const auto& activity : options.activities)
        classes.push_back({python::text(activity)});
    }
    for (const auto& value : classes) result.classes.push_back(value_str(value));
  } else if (options.variant == LogToTargetVariant::NEXT_TIME) {
    result.classes = {"@@next_time"};
  } else {
    result.classes = {"@@remaining_time"};
  }
  result.target_width = options.variant == LogToTargetVariant::NEXT_ACTIVITY
                            ? result.classes.size()
                            : 1;

  // The time targets read every event's Timestamp.timestamp(), in log order.
  std::vector<std::vector<double>> instants;
  if (options.variant != LogToTargetVariant::NEXT_ACTIVITY) {
    instants.resize(log.traces.size());
    for (std::size_t t = 0; t < log.traces.size(); ++t)
      for (std::size_t e = 0; e < log.traces[t].events.size(); ++e)
        instants[t].push_back(reader.instant(t, e, options.timestamp_key));
  }

  // The default pad size is max() of the trace lengths, taken whatever the
  // options: an empty log raises there.
  if (log.traces.empty())
    throw std::invalid_argument("ValueError: max() arg is an empty sequence");
  std::size_t longest = 0;
  for (const auto& trace : log.traces) longest = std::max(longest, trace.events.size());
  const std::size_t pad_rows = !options.enable_padding ? 0
                               : options.explicit_pad_size || options.pad_size != 0 ? options.pad_size
                                                                                    : longest;

  result.rows.reserve(log.traces.size());
  for (std::size_t t = 0; t < log.traces.size(); ++t) {
    const auto size = log.traces[t].events.size();
    std::vector<std::vector<double>> rows;
    rows.reserve(std::max(size, std::min<std::size_t>(pad_rows, 100000)));
    for (std::size_t i = 0; i < size; ++i) {
      if (options.variant == LogToTargetVariant::NEXT_ACTIVITY) {
        std::vector<double> row(classes.size(), 0.0);
        if (i + 1 < size) {
          const auto& next = activities[t][i + 1];
          const auto found = std::find_if(classes.begin(), classes.end(),
                                          [&](const Value& c) { return same_value(c, next); });
          if (found != classes.end()) row[static_cast<std::size_t>(found - classes.begin())] = 1.0;
        }
        rows.push_back(std::move(row));
        continue;
      }
      // The instants subtract as given: an event before its predecessor
      // gives a negative target.
      double value = 0.0;
      if (i + 1 < size)
        value = options.variant == LogToTargetVariant::NEXT_TIME
                    ? instants[t][i + 1] - instants[t][i]
                    : instants[t].back() - instants[t][i];
      rows.push_back({value});
    }
    while (rows.size() < pad_rows)
      rows.push_back(options.variant == LogToTargetVariant::NEXT_ACTIVITY
                         ? std::vector<double>(classes.size(), 0.0)
                         : std::vector<double>{0.0});
    result.rows.push_back(std::move(rows));
  }
  return result;
}

}  // namespace minera
