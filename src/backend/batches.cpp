#include "minera/batches.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

enum class SelectorRole { kCase, kActivity, kTimestamp, kResource, kGeneric };

// A scalar as the reference's tuples and group keys hold it: bool, int and
// float are one kind compared and hashed by value (1, 1.0 and True are
// equal), text another; comparing the two kinds raises.
struct ScalarValue {
  bool number = false;
  double value = 0.0;
  std::string text;
  const char* type = "str";
};

ScalarValue text_scalar(std::string text) { return {false, 0.0, std::move(text), "str"}; }

// Equal scalars share one identity key.
std::string scalar_key(const ScalarValue& v) {
  if (!v.number) return "S" + v.text;
  if (std::isnan(v.value)) return "Nnan";
  char buffer[64];
  const auto r = std::to_chars(buffer, buffer + sizeof(buffer), v.value == 0 ? 0.0 : v.value);
  return "N" + std::string(buffer, r.ptr);
}

[[noreturn]] void unorderable(const ScalarValue& a, const ScalarValue& b) {
  const auto& number = a.number ? a : b;
  const auto& other = a.number ? b : a;
  throw std::invalid_argument(std::string("batches: '<' not supported between instances of '") + number.type +
                              "' and '" + other.type + "'");
}

// Python's three-way comparison of two scalars; NaN orders with nothing.
int compare_scalars(const ScalarValue& a, const ScalarValue& b) {
  if (a.number != b.number) unorderable(a, b);
  if (a.number) return a.value < b.value ? -1 : b.value < a.value ? 1 : 0;
  return a.text < b.text ? -1 : b.text < a.text ? 1 : 0;
}

struct IndexedEvent {
  BatchEvent event;
  std::string activity;
  std::string resource;
  // A numeric event id orders numerically, as the reference's event tuples
  // compare it.
  std::optional<double> event_id_number;
  // A pandas NaN case id is a float in the reference's event tuples.
  bool case_nan = false;
  ScalarValue case_value;
  ScalarValue activity_value;
  ScalarValue resource_value;
};

// Groups are keyed by the identities of activity and resource; a null
// resource (Polars only) is its own key. A group shows the values of its
// first event.
struct GroupKey {
  std::string activity;
  std::string resource;
  bool resource_is_null = false;
  bool operator<(const GroupKey& other) const {
    return std::tie(activity, resource_is_null, resource) <
           std::tie(other.activity, other.resource_is_null, other.resource);
  }
};
using GroupedEvents = std::map<GroupKey, std::vector<IndexedEvent>>;

struct TableSelector {
  std::string name;
  std::optional<std::size_t> column;
  SelectorRole role = SelectorRole::kGeneric;
};

bool is_alias(std::string_view value,
              std::initializer_list<std::string_view> aliases) {
  return std::find(aliases.begin(), aliases.end(), value) != aliases.end();
}

bool is_alias(std::string_view value,
              const std::vector<std::string_view>& aliases) {
  return std::find(aliases.begin(), aliases.end(), value) != aliases.end();
}

std::optional<SelectorRole> selector_role(std::string_view value) {
  if (is_alias(value, {"case:concept:name", "case_id", "case"}))
    return SelectorRole::kCase;
  if (is_alias(value, {"concept:name", "activity"}))
    return SelectorRole::kActivity;
  if (is_alias(value, {"time:timestamp", "timestamp"}))
    return SelectorRole::kTimestamp;
  if (is_alias(value, {"org:resource", "resource"}))
    return SelectorRole::kResource;
  return std::nullopt;
}

FirstClassAttributeRole first_class_role(SelectorRole role) {
  switch (role) {
    case SelectorRole::kCase: return FirstClassAttributeRole::kCaseId;
    case SelectorRole::kActivity: return FirstClassAttributeRole::kActivity;
    case SelectorRole::kTimestamp: return FirstClassAttributeRole::kTimestamp;
    case SelectorRole::kResource: return FirstClassAttributeRole::kResource;
    case SelectorRole::kGeneric: return FirstClassAttributeRole::kNone;
  }
  return FirstClassAttributeRole::kNone;
}

std::vector<std::string_view> aliases_for(SelectorRole role) {
  switch (role) {
    case SelectorRole::kCase:
      return {"case:concept:name", "case_id", "case"};
    case SelectorRole::kActivity:
      return {"concept:name", "activity"};
    case SelectorRole::kTimestamp:
      return {"time:timestamp", "timestamp"};
    case SelectorRole::kResource:
      return {"org:resource", "resource"};
    case SelectorRole::kGeneric:
      return {};
  }
  return {};
}

std::optional<std::size_t> exact_column(const EventTable& table,
                                        std::string_view name) {
  const auto found = std::find(table.columns.begin(), table.columns.end(), name);
  if (found == table.columns.end()) return std::nullopt;
  return static_cast<std::size_t>(found - table.columns.begin());
}

const EventAttribute* row_cell(const EventTable::Row& row,
                               const EventTable& table,
                               const TableSelector& selector) {
  if (selector.column) {
    if (*selector.column >= row.size() || !row[*selector.column]) return nullptr;
    const auto& cell = *row[*selector.column];
    return cell.is_null ? nullptr : &cell;
  }

  const auto role = first_class_role(selector.role);
  const bool allow_role_projection = selector.name.empty() ||
                                     selector_role(selector.name).has_value();
  for (std::size_t index = 0; index < row.size(); ++index) {
    if (!row[index] || row[index]->is_null) continue;
    const auto& cell = *row[index];
    const auto column_name = index < table.columns.size()
                                 ? std::string_view(table.columns[index])
                                 : std::string_view{};
    if ((allow_role_projection && role != FirstClassAttributeRole::kNone &&
         (cell.first_class_role == role ||
          (cell.first_class_role == FirstClassAttributeRole::kNone &&
           is_alias(column_name, aliases_for(selector.role))))) ||
        cell.key == selector.name ||
        cell.first_class_source_key == selector.name ||
        column_name == selector.name) {
      return &cell;
    }
  }
  return nullptr;
}

bool any_selected_cell(const EventTable& table, const TableSelector& selector) {
  for (const auto& row : table.rows)
    if (row_cell(row, table, selector) != nullptr) return true;
  return false;
}

// A log-to-table conversion keeps the original spelling of a first-class
// selector in transport metadata while using a canonical table column for
// the projected value.  Resolve that source spelling only when the metadata
// carries the role requested by the caller; a source key belonging to an
// activity, resource, or timestamp must not silently become a case column.
std::optional<std::size_t> source_projection_column(
    const EventTable& table, std::string_view name, SelectorRole requested_role) {
  const auto expected_role = first_class_role(requested_role);
  if (expected_role == FirstClassAttributeRole::kNone || name.empty())
    return std::nullopt;
  for (const auto& row : table.rows) {
    for (std::size_t index = 0; index < row.size(); ++index) {
      if (!row[index] || row[index]->is_null) continue;
      const auto& cell = *row[index];
      if (cell.first_class_role == expected_role &&
          cell.first_class_source_key == name)
        return index;
    }
  }
  return std::nullopt;
}

TableSelector resolve_selector(const EventTable& table, std::string name,
                               SelectorRole requested_role,
                               std::string_view description,
                               bool required) {
  const auto semantic_role = selector_role(name);
  TableSelector selector{std::move(name), std::nullopt,
                         semantic_role.value_or(requested_role)};
  if (const auto found = exact_column(table, selector.name)) {
    selector.column = found;
    return selector;
  }
  if (semantic_role) {
    for (const auto alias : aliases_for(*semantic_role)) {
      if (const auto found = exact_column(table, alias)) {
        selector.column = found;
        return selector;
      }
    }
  }
  // Flat log adapters retain non-standard first-class source names in cell
  // metadata even when the visible table column is canonical.  Resolve that
  // metadata only with a matching role so an unrelated source key cannot be
  // selected by accident.
  if (const auto found =
          source_projection_column(table, selector.name, requested_role)) {
    selector.column = found;
    return selector;
  }
  // A non-standard selector otherwise names a concrete table column.  Do not
  // infer it from an unrelated first-class role or stale cell metadata after
  // a column rename.
  if (semantic_role && any_selected_cell(table, selector)) return selector;
  if (required) {
    throw std::invalid_argument("batches: missing " + std::string(description) +
                                " column '" + selector.name + "'");
  }
  return selector;
}

std::string attribute_text(const EventAttribute& cell) {
  if (!cell.value.empty()) return cell.value;
  if (const auto* date = std::get_if<NativeDate>(&cell.typed_value))
    return date->value;
  if (const auto* integer = std::get_if<std::int64_t>(&cell.typed_value))
    return std::to_string(*integer);
  if (const auto* unsigned_integer =
          std::get_if<std::uint64_t>(&cell.typed_value))
    return std::to_string(*unsigned_integer);
  if (const auto* floating = std::get_if<double>(&cell.typed_value))
    return std::to_string(*floating);
  if (const auto* boolean = std::get_if<bool>(&cell.typed_value))
    return *boolean ? "true" : "false";
  return {};
}

const std::string* event_value(const Event& event, std::string_view key,
                               std::string_view kind) {
  if (key.empty()) {
    if (kind == "activity") return &event.activity;
    if (kind == "resource") return &event.resource;
    if (kind == "case") return &event.case_id;
    return &event.timestamp;
  }
  if (kind == "activity" &&
      (key == "activity" || key == "concept:name" ||
       key == event.activity_source_key)) {
    return &event.activity;
  }
  if (kind == "resource" &&
      (key == "resource" || key == "org:resource" ||
       key == event.resource_source_key)) {
    return &event.resource;
  }
  if (kind == "timestamp" &&
      (key == "timestamp" || key == "time:timestamp" ||
       key == event.timestamp_source_key)) {
    return &event.timestamp;
  }
  if (kind == "case" &&
      (key == "case" || key == "case_id" || key == "case:id" ||
       key == "case:concept:name" || key == "concept:name" ||
       key == event.case_id_source_key)) {
    return &event.case_id;
  }
  return event.attribute(key);
}

const std::string* trace_value(const Trace& trace, std::string_view key) {
  if (key.empty() || key == "case_id" || key == "case:id" ||
      key == "case:concept:name" || key == "concept:name" || key == "case") {
    return &trace.case_id;
  }
  return trace.attribute(key);
}

const std::string* log_case_value(const Trace& trace, const Event& event,
                                  std::string_view key) {
  const bool canonical =
      key.empty() || key == "case_id" || key == "case:id" ||
      key == "case:concept:name" || key == "concept:name" || key == "case";
  if (canonical) return trace_value(trace, key);

  // A custom case selector is an event selector.  A trace-level value remains
  // a useful fallback for logs whose case metadata was imported separately.
  if (const auto* value = event_value(event, key, "case")) return value;
  return trace.attribute(key);
}

const std::string& required_event_value(const Event& event,
                                        std::string_view key,
                                        std::string_view kind) {
  const auto* value = event_value(event, key, kind);
  if (value == nullptr) {
    throw std::invalid_argument("batches: event is missing " +
                                std::string(kind) + " attribute '" +
                                std::string(key) + "'");
  }
  return *value;
}

const std::string& required_case_value(const Trace& trace, const Event& event,
                                       std::string_view key) {
  const auto* value = log_case_value(trace, event, key);
  if (value == nullptr) {
    throw std::invalid_argument("batches: event is missing case-id attribute '" +
                                std::string(key) + "'");
  }
  return *value;
}

void validate_options(const BatchDiscoveryOptions& options) {
  if (options.variant != BatchDiscoveryVariant::kLog &&
      options.variant != BatchDiscoveryVariant::kPandas &&
      options.variant != BatchDiscoveryVariant::kPolars) {
    throw std::invalid_argument("batches: unknown discovery variant");
  }
  // The reference accepts any merge distance and minimum size: a negative
  // distance merges nothing that does not overlap, and a minimum of zero or
  // one keeps every interval.
  if (!std::isfinite(options.merge_distance_seconds)) {
    throw std::invalid_argument("batches: merge distance must be finite");
  }
  if (options.activity_attribute.empty() || options.resource_attribute.empty() ||
      options.timestamp_attribute.empty() || options.case_id_attribute.empty()) {
    throw std::invalid_argument(
        "batches: activity, resource, timestamp, and case-id attributes must not be empty");
  }
}

struct EventIdentity {
  double start = 0.0;
  double complete = 0.0;
  std::string case_id;
  std::optional<double> event_id_number;
  std::string event_id;

  bool operator<(const EventIdentity& other) const {
    return std::tie(start, complete, case_id, event_id_number, event_id) <
           std::tie(other.start, other.complete, other.case_id,
                    other.event_id_number, other.event_id);
  }
};

struct Interval {
  double start = 0.0;
  double end = 0.0;
  std::vector<std::size_t> members;
};

// Interval lists compare as the reference's (start, end, events) tuples; the
// event sets are disjoint, so ties never order.
bool interval_less(const Interval& left, const Interval& right) {
  return left.start < right.start ||
         (left.start == right.start && left.end < right.end);
}

Interval merged_interval(const Interval& left, const Interval& right) {
  Interval merged{std::min(left.start, right.start),
                  std::max(left.end, right.end), left.members};
  merged.members.insert(merged.members.end(), right.members.begin(),
                        right.members.end());
  return merged;
}

void sift_down(std::vector<Interval>& heap, std::size_t start,
               std::size_t position) {
  auto item = std::move(heap[position]);
  while (position > start) {
    const auto parent = (position - 1) / 2;
    if (!interval_less(item, heap[parent])) break;
    heap[position] = std::move(heap[parent]);
    position = parent;
  }
  heap[position] = std::move(item);
}

void sift_up(std::vector<Interval>& heap, std::size_t position) {
  const auto end = heap.size();
  const auto start = position;
  auto item = std::move(heap[position]);
  auto child = 2 * position + 1;
  while (child < end) {
    const auto right = child + 1;
    if (right < end && !interval_less(heap[child], heap[right])) child = right;
    heap[position] = std::move(heap[child]);
    position = child;
    child = 2 * position + 1;
  }
  heap[position] = std::move(item);
  sift_down(heap, start, position);
}

// The reference builds one interval per event in input order and heapifies
// the list (Python heapq order) before merging.
std::vector<Interval> heapified_intervals(const std::vector<IndexedEvent>& values) {
  std::vector<Interval> intervals;
  intervals.reserve(values.size());
  for (std::size_t index = 0; index < values.size(); ++index)
    intervals.push_back(
        {values[index].event.start, values[index].event.complete, {index}});
  for (auto index = intervals.size() / 2; index > 0; --index)
    sift_up(intervals, index - 1);
  return intervals;
}

// Adjacent intervals merge in place while the next starts strictly before
// the current end; the list is stably sorted after a merge (after the first
// one it stays sorted) and the scan repeats until nothing merges.
void merge_overlapping(std::vector<Interval>& intervals) {
  bool changed = true;
  bool sorted = false;
  while (changed) {
    changed = false;
    std::size_t index = 0;
    while (index + 1 < intervals.size()) {
      if (!(intervals[index + 1].start < intervals[index].end)) {
        ++index;
        continue;
      }
      intervals[index] =
          merged_interval(intervals[index], intervals[index + 1]);
      intervals.erase(intervals.begin() +
                      static_cast<std::ptrdiff_t>(index + 1));
      changed = true;
      if (!sorted) {
        std::stable_sort(intervals.begin(), intervals.end(), interval_less);
        sorted = true;
      }
    }
  }
}

// The reference merges near intervals by scanning adjacent pairs, removing a
// merged pair and heap-pushing the union back into the list, and repeating
// the scan until nothing merges. The list is not re-sorted, so later scans
// see heap order and can join intervals farther apart than the distance.
void merge_near(std::vector<Interval>& intervals, double distance) {
  bool changed = true;
  while (changed) {
    changed = false;
    std::size_t index = 0;
    while (index + 1 < intervals.size()) {
      if (intervals[index + 1].start - intervals[index].end > distance) {
        ++index;
        continue;
      }
      auto merged = merged_interval(intervals[index], intervals[index + 1]);
      intervals.erase(intervals.begin() + static_cast<std::ptrdiff_t>(index),
                      intervals.begin() + static_cast<std::ptrdiff_t>(index + 2));
      intervals.push_back(std::move(merged));
      sift_down(intervals, 0, intervals.size() - 1);
      changed = true;
    }
  }
}

BatchType classify(const std::vector<BatchEvent>& events) {
  if (events.empty()) return BatchType::kConcurrent;
  const double first_start = events.front().start;
  const double first_complete = events.front().complete;
  bool equal_starts = true;
  bool equal_ends = true;
  bool simultaneous = true;
  for (const auto& event : events) {
    equal_starts = equal_starts && event.start == first_start;
    equal_ends = equal_ends && event.complete == first_complete;
    simultaneous = simultaneous && event.start == first_start &&
                   event.complete == first_complete;
  }
  if (simultaneous) return BatchType::kSimultaneous;
  if (equal_starts) return BatchType::kBatchingOnStart;
  if (equal_ends) return BatchType::kBatchingOnEnd;

  bool sequential = true;
  for (std::size_t i = 1; i < events.size(); ++i) {
    if (events[i - 1].complete != events[i].start) {
      sequential = false;
      break;
    }
  }
  return sequential ? BatchType::kSequential : BatchType::kConcurrent;
}

BatchDiscovery detect_grouped(GroupedEvents grouped,
                              BatchDiscoveryOptions options,
                              bool include_event_id) {
  validate_options(options);

  struct KeyedGroup {
    GroupKey key;
    BatchGroup group;
    ScalarValue activity;
    ScalarValue resource;
  };
  std::vector<KeyedGroup> result;
  for (auto& [key, values] : grouped) {
    auto intervals = heapified_intervals(values);
    merge_overlapping(intervals);
    merge_near(intervals, options.merge_distance_seconds);

    // An interval's events are a set of (start, complete, case[, id]) tuples,
    // so repeated events count once; merging unions the sets left first, so
    // the first member of equal events is kept (1 before an equal 1.0).
    std::vector<Batch> batches;
    for (auto& interval : intervals) {
      std::set<std::tuple<double, double, std::string, std::optional<double>, std::string>> identities;
      std::vector<std::size_t> kept;
      for (const auto member : interval.members) {
        const auto& value = values[member];
        if (identities
                .emplace(value.event.start, value.event.complete, scalar_key(value.case_value),
                         include_event_id ? value.event_id_number : std::nullopt,
                         include_event_id ? value.event.event_id : std::string{})
                .second)
          kept.push_back(member);
      }
      if (kept.size() < options.min_batch_size) continue;
      // The reference sorts a kept batch's event tuples: the case ids of
      // events with equal start and completion are compared, so a number
      // and a text case there cannot be ordered.
      std::map<std::pair<double, double>, std::size_t> first_at;
      for (const auto member : kept) {
        const auto& value = values[member];
        const auto [it, added] = first_at.emplace(std::make_pair(value.event.start, value.event.complete), member);
        if (!added && values[it->second].case_value.number != value.case_value.number)
          unorderable(values[it->second].case_value, value.case_value);
      }
      const auto less = [&](std::size_t left, std::size_t right) {
        const auto& a = values[left];
        const auto& b = values[right];
        if (a.event.start != b.event.start) return a.event.start < b.event.start;
        if (a.event.complete != b.event.complete) return a.event.complete < b.event.complete;
        if (const int c = compare_scalars(a.case_value, b.case_value)) return c < 0;
        if (!include_event_id) return false;
        return std::tie(a.event_id_number, a.event.event_id) < std::tie(b.event_id_number, b.event.event_id);
      };
      // Binary insertion keeps the sort stable and safe with NaN case ids,
      // which order with nothing.
      std::vector<std::size_t> ordered;
      for (const auto member : kept) {
        std::size_t low = 0, high = ordered.size();
        while (low < high) {
          const auto mid = (low + high) / 2;
          if (less(member, ordered[mid])) high = mid;
          else low = mid + 1;
        }
        ordered.insert(ordered.begin() + static_cast<std::ptrdiff_t>(low), member);
      }
      Batch batch;
      for (const auto member : ordered) batch.events.push_back(values[member].event);
      batch.start = interval.start;
      batch.complete = interval.end;
      batch.type = classify(batch.events);
      batches.push_back(std::move(batch));
    }
    if (batches.empty()) continue;
    // Batches are listed by start; the reference lists them per type in its
    // final interval order.
    std::stable_sort(batches.begin(), batches.end(),
                     [](const Batch& left, const Batch& right) {
                       return left.start < right.start;
                     });
    BatchGroup group;
    group.activity = values.front().activity;
    group.resource = values.front().resource;
    group.resource_is_null = key.resource_is_null;
    group.batch_count = batches.size();
    for (const auto& batch : batches) group.event_count += batch.events.size();
    group.batches = std::move(batches);
    result.push_back({key, std::move(group), values.front().activity_value, values.front().resource_value});
  }
  // Groups sort by batch count, then (activity, resource), both descending,
  // comparing the values as the reference's key tuples do: a number and a
  // text, or a null resource and a text one, cannot be compared.
  const auto compare_keys = [](const KeyedGroup& left, const KeyedGroup& right) {
    if (const int c = compare_scalars(left.activity, right.activity)) return c;
    if (left.group.resource_is_null != right.group.resource_is_null)
      throw std::invalid_argument(
          "batches: '<' not supported between instances of 'str' and "
          "'NoneType'");
    if (left.group.resource_is_null) return 0;
    return compare_scalars(left.resource, right.resource);
  };
  for (std::size_t i = 0; i < result.size(); ++i)
    for (std::size_t j = i + 1; j < result.size(); ++j)
      if (result[i].group.batch_count == result[j].group.batch_count) (void)compare_keys(result[i], result[j]);
  std::sort(result.begin(), result.end(), [&](const KeyedGroup& left,
                                              const KeyedGroup& right) {
    if (left.group.batch_count != right.group.batch_count)
      return left.group.batch_count > right.group.batch_count;
    return compare_keys(left, right) > 0;
  });
  std::vector<BatchGroup> groups;
  groups.reserve(result.size());
  for (auto& entry : result) groups.push_back(std::move(entry.group));
  return BatchDiscovery{options, options.variant, std::move(groups)};
}

void append_log_events(
    const EventLog& log, const BatchDiscoveryOptions& options,
    GroupedEvents& grouped) {
  for (std::size_t trace_index = 0; trace_index < log.traces.size();
       ++trace_index) {
    const auto& trace = log.traces[trace_index];
    for (std::size_t event_index = 0; event_index < trace.events.size();
         ++event_index) {
      const auto& event = trace.events[event_index];
      const auto& activity =
          required_event_value(event, options.activity_attribute, "activity");
      const auto& resource =
          required_event_value(event, options.resource_attribute, "resource");
      const auto& completion_text =
          required_event_value(event, options.timestamp_attribute, "timestamp");
      const auto complete = parse_timestamp(completion_text);
      if (!complete || !std::isfinite(*complete))
        throw std::invalid_argument(
            "batches: event has an invalid completion timestamp");
      double start = *complete;
      if (!options.start_timestamp_attribute.empty()) {
        const auto* value = event_value(
            event, options.start_timestamp_attribute, "timestamp");
        if (value == nullptr || value->empty())
          throw std::invalid_argument(
              "batches: configured start timestamp is missing");
        const auto parsed = parse_timestamp(*value);
        if (!parsed || !std::isfinite(*parsed))
          throw std::invalid_argument(
              "batches: configured start timestamp is invalid");
        start = *parsed;
      }
      if (!std::isfinite(start))
        throw std::invalid_argument("batches: non-finite start timestamp");
      if (start > *complete)
        throw std::invalid_argument(
            "batches: start timestamp is after completion");
      const auto& case_id = required_case_value(trace, event,
                                                options.case_id_attribute);
      BatchEvent batch_event{start, *complete, case_id, trace_index, event_index,
                             {}};
      grouped[{scalar_key(text_scalar(activity)), scalar_key(text_scalar(resource)), false}].push_back(
          IndexedEvent{std::move(batch_event), activity, resource, {}, false, text_scalar(case_id),
                       text_scalar(activity), text_scalar(resource)});
    }
  }
}

std::optional<double> numeric_value(const EventAttribute& cell) {
  if (const auto* integer = std::get_if<std::int64_t>(&cell.typed_value))
    return static_cast<double>(*integer);
  if (const auto* unsigned_integer =
          std::get_if<std::uint64_t>(&cell.typed_value))
    return static_cast<double>(*unsigned_integer);
  if (const auto* floating = std::get_if<double>(&cell.typed_value))
    return *floating;
  return std::nullopt;
}

// A table cell's scalar: typed and lexical bool, int and float cells are
// numbers; anything else is compared by its text.
ScalarValue scalar_value(const EventAttribute& cell) {
  if (const auto* b = std::get_if<bool>(&cell.typed_value)) return {true, *b ? 1.0 : 0.0, {}, "bool"};
  if (const auto* i = std::get_if<std::int64_t>(&cell.typed_value)) return {true, static_cast<double>(*i), {}, "int"};
  if (const auto* u = std::get_if<std::uint64_t>(&cell.typed_value)) return {true, static_cast<double>(*u), {}, "int"};
  if (const auto* d = std::get_if<double>(&cell.typed_value)) return {true, *d, {}, "float"};
  if (cell.typed_value.index() == 0) {
    if (cell.kind == AttributeKind::kBoolean && (cell.value == "True" || cell.value == "true" ||
                                                 cell.value == "False" || cell.value == "false"))
      return {true, cell.value == "True" || cell.value == "true" ? 1.0 : 0.0, {}, "bool"};
    if (cell.kind == AttributeKind::kInt || cell.kind == AttributeKind::kFloat) {
      char* end = nullptr;
      const double parsed = std::strtod(cell.value.c_str(), &end);
      if (!cell.value.empty() && end && *end == '\0')
        return {true, parsed, {}, cell.kind == AttributeKind::kInt ? "int" : "float"};
    }
  }
  return text_scalar(attribute_text(cell));
}

GroupedEvents table_events(const EventTable& table,
                           const BatchDiscoveryOptions& options) {
  const bool polars = options.variant == BatchDiscoveryVariant::kPolars;
  const bool pandas = options.variant == BatchDiscoveryVariant::kPandas;
  table.validate();
  const auto case_selector = resolve_selector(
      table, options.case_id_attribute, SelectorRole::kCase, "case-id", true);
  const auto activity_selector = resolve_selector(
      table, options.activity_attribute, SelectorRole::kActivity, "activity", true);
  const auto resource_selector = resolve_selector(
      table, options.resource_attribute, SelectorRole::kResource, "resource", true);
  const auto timestamp_selector = resolve_selector(
      table, options.timestamp_attribute, SelectorRole::kTimestamp, "timestamp", true);
  const auto start_selector = options.start_timestamp_attribute.empty()
                                  ? TableSelector{}
                                  : resolve_selector(
                                        table, options.start_timestamp_attribute,
                                        SelectorRole::kTimestamp,
                                        "start timestamp", true);
  const auto event_id_selector = options.event_id_attribute.empty()
                                     ? TableSelector{}
                                     : resolve_selector(
                                           table, options.event_id_attribute,
                                           SelectorRole::kGeneric, "event id", true);
  const bool event_id_selected = !options.event_id_attribute.empty() &&
                                 event_id_selector.column.has_value();

  // Polars requires a string (or categorical) case column, even when empty.
  if (polars && case_selector.column && *case_selector.column < table.dtypes.size()) {
    const std::string_view dtype = table.dtypes[*case_selector.column];
    const bool text = dtype.starts_with("String") || dtype.starts_with("Utf8") ||
                      dtype.starts_with("Categorical") || dtype.starts_with("Enum");
    if (!text)
      throw std::invalid_argument(
          "batches: the case ID column should be of type string.");
  }
  // The pandas adapter reads a datetime column as its integer units divided
  // by 1e9, so a microsecond, millisecond or second column gives seconds
  // scaled down by 1e3, 1e6 or 1e9.
  const auto pandas_scale = [&](const TableSelector& selector)
      -> std::optional<double> {
    if (options.variant != BatchDiscoveryVariant::kPandas || !selector.column ||
        *selector.column >= table.dtypes.size())
      return std::nullopt;
    const std::string_view dtype = table.dtypes[*selector.column];
    if (dtype.starts_with("datetime64[us")) return 1e6;
    if (dtype.starts_with("datetime64[ms")) return 1e3;
    if (dtype.starts_with("datetime64[s")) return 1.0;
    return std::nullopt;
  };
  const auto rescale = [](double seconds, std::optional<double> units) {
    if (!units) return seconds;
    return static_cast<double>(std::llround(seconds * *units)) / 1e9;
  };
  const auto completion_scale = pandas_scale(timestamp_selector);
  const auto start_scale = options.start_timestamp_attribute.empty()
                               ? std::nullopt
                               : pandas_scale(start_selector);
  // The Polars adapter reads a timestamp as its epoch nanoseconds times
  // 1e-9, so second 15 is 15.000000000000002 and batch boundaries follow
  // that value.
  const auto seconds_of = [&](const std::string& text) -> std::optional<double> {
    if (!polars) return parse_timestamp(text);
    const auto exact = parse_timestamp_exact(text);
    if (!exact) return std::nullopt;
    constexpr std::int64_t kBillion = 1000000000;
    if (exact->seconds > std::numeric_limits<std::int64_t>::max() / kBillion - 1 ||
        exact->seconds < std::numeric_limits<std::int64_t>::min() / kBillion + 1)
      return static_cast<double>(exact->seconds) + 1e-9 * static_cast<double>(exact->nanoseconds);
    return static_cast<double>(exact->seconds * kBillion + exact->nanoseconds) * 1e-9;
  };
  GroupedEvents grouped;
  for (std::size_t row_index = 0; row_index < table.rows.size(); ++row_index) {
    const auto& row = table.rows[row_index];
    const auto* activity_cell = row_cell(row, table, activity_selector);
    const auto* resource_cell = row_cell(row, table, resource_selector);
    const auto* timestamp_cell = row_cell(row, table, timestamp_selector);
    const auto* case_cell = row_cell(row, table, case_selector);
    // pandas drops rows whose activity or resource is missing (NaN), keeps a
    // missing case id as nan and reads NaT as the minimum int64 over 1e9.
    const auto missing = [](const EventAttribute* cell) {
      if (!cell || std::holds_alternative<NativeNA>(cell->typed_value))
        return true;
      const auto number = numeric_value(*cell);
      return number && std::isnan(*number);
    };
    constexpr double kNaT =
        static_cast<double>(std::numeric_limits<std::int64_t>::min()) / 1e9;
    if (pandas && (missing(activity_cell) || missing(resource_cell))) continue;
    const bool pandas_case_nan = pandas && missing(case_cell);
    // Polars groups a null resource as its own key.
    const bool null_resource = polars && !resource_cell;
    if (!activity_cell || (!resource_cell && !null_resource) ||
        (!timestamp_cell && !pandas) || (!case_cell && !pandas_case_nan)) {
      throw std::invalid_argument("batches: a required table cell is missing");
    }
    if (polars && numeric_value(*case_cell))
      throw std::invalid_argument(
          "batches: the case ID column should be of type string.");
    const auto activity = attribute_text(*activity_cell);
    const auto resource = null_resource ? std::string{}
                                        : attribute_text(*resource_cell);
    const auto case_id =
        pandas_case_nan ? std::string("nan") : attribute_text(*case_cell);
    const auto complete =
        timestamp_cell ? seconds_of(attribute_text(*timestamp_cell))
                       : std::optional<double>{};
    if (timestamp_cell && (!complete || !std::isfinite(*complete)))
      throw std::invalid_argument(
          "batches: table row has an invalid completion timestamp");

    std::optional<double> start = complete;
    if (!options.start_timestamp_attribute.empty()) {
      const auto* start_cell = row_cell(row, table, start_selector);
      if (!start_cell && pandas) {
        start.reset();
      } else {
        if (!start_cell || attribute_text(*start_cell).empty())
          throw std::invalid_argument(
              "batches: configured start timestamp is missing");
        const auto parsed = seconds_of(attribute_text(*start_cell));
        if (!parsed || !std::isfinite(*parsed))
          throw std::invalid_argument(
              "batches: configured start timestamp is invalid");
        start = parsed;
      }
    }
    if (start && complete && *start > *complete)
      throw std::invalid_argument(
          "batches: start timestamp is after completion");
    const double completion =
        complete ? rescale(*complete, completion_scale) : kNaT;
    const double start_time =
        options.start_timestamp_attribute.empty()
            ? completion
            : (start ? rescale(*start, start_scale) : kNaT);

    std::string event_id;
    std::optional<double> event_id_number;
    if (event_id_selected) {
      if (const auto* event_id_cell = row_cell(row, table, event_id_selector)) {
        event_id = attribute_text(*event_id_cell);
        event_id_number = numeric_value(*event_id_cell);
      }
    }
    BatchEvent batch_event{start_time, completion, case_id, 0, row_index, event_id};
    auto case_value = pandas_case_nan ? ScalarValue{true, std::numeric_limits<double>::quiet_NaN(), {}, "float"}
                                      : scalar_value(*case_cell);
    auto activity_value = scalar_value(*activity_cell);
    auto resource_value = null_resource ? text_scalar({}) : scalar_value(*resource_cell);
    grouped[{scalar_key(activity_value), null_resource ? std::string{} : scalar_key(resource_value), null_resource}]
        .push_back(IndexedEvent{std::move(batch_event), activity, resource, event_id_number, pandas_case_nan,
                                std::move(case_value), std::move(activity_value), std::move(resource_value)});
  }
  return grouped;
}

BatchDiscovery discover_table(const EventTable& table,
                              BatchDiscoveryOptions options,
                              BatchDiscoveryVariant variant) {
  options.variant = variant;
  validate_options(options);
  auto grouped = table_events(table, options);
  return detect_grouped(std::move(grouped), std::move(options), true);
}

}  // namespace

BatchDiscovery discover_batches(const EventLog& log,
                                BatchDiscoveryOptions options) {
  validate_options(options);
  GroupedEvents grouped;
  append_log_events(log, options, grouped);
  return detect_grouped(std::move(grouped), std::move(options), false);
}

BatchDiscovery discover_batches(const EventTable& table,
                                BatchDiscoveryOptions options) {
  return discover_table(table, std::move(options), BatchDiscoveryVariant::kPandas);
}

BatchDiscovery discover_batches(const LazyEventTable& table,
                                BatchDiscoveryOptions options) {
  return discover_table(table.materialize(), std::move(options),
                        BatchDiscoveryVariant::kPolars);
}

BatchDiscovery BatchLogDiscovery::apply(const EventLog& log,
                                         BatchDiscoveryOptions options) {
  options.variant = BatchDiscoveryVariant::kLog;
  return discover_batches(log, std::move(options));
}

BatchDiscovery BatchPandasDiscovery::apply(const EventTable& table,
                                            BatchDiscoveryOptions options) {
  return discover_table(table, std::move(options), BatchDiscoveryVariant::kPandas);
}

BatchDiscovery BatchPolarsDiscovery::apply(const LazyEventTable& table,
                                            BatchDiscoveryOptions options) {
  return discover_table(table.materialize(), std::move(options),
                        BatchDiscoveryVariant::kPolars);
}

}  // namespace minera
