#include "minera/batches.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
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
#include <unordered_map>
#include <utility>
#include <variant>

#include "minera/timestamp.hpp"
#include "python_scalar.hpp"

namespace minera {
namespace {

enum class SelectorRole { kCase, kActivity, kTimestamp, kResource, kGeneric };

// Scalars are the Python objects of the reference's tuples and group keys:
// bool, int and float compare and hash by exact value (1, 1.0 and True are
// equal, 2**53 and 2**53+1 are not), text is another kind, and comparing the
// two kinds raises. See python_scalar.hpp.
using ScalarValue = python::Scalar;

ScalarValue text_scalar(std::string text) { return python::text(std::move(text)); }

// Python's three-way comparison with the module's error prefix.
int compare_scalars(const ScalarValue& a, const ScalarValue& b) {
  try {
    return python::compare(a, b);
  } catch (const python::TypeError& error) {
    throw std::invalid_argument(std::string("batches: ") + error.what());
  }
}

struct IndexedEvent {
  BatchEvent event;
  std::string activity;
  std::string resource;
  // The selected event id, as a typed scalar (None for a null cell) when a
  // table adapter was given an event-id selector.
  std::optional<ScalarValue> event_id;
  // A pandas NaN case id is a float in the reference's event tuples.
  bool case_nan = false;
  // A NaN in a "str" column is pandas' one np.nan object, the same tuple
  // member in every row; any other NaN is a float of its own row.
  bool case_shared_nan = false;
  bool event_id_shared_nan = false;
  ScalarValue case_value;
  ScalarValue activity_value;
  ScalarValue resource_value;
  // The Polars adapter groups a null resource as its own key.
  bool resource_is_null = false;
};

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

// The reference's (start, end, events) interval tuples: events is a set of
// (start, complete, case[, id]) tuples, so a repeated event is one member and
// a union keeps the left set's member of two equal ones. `identities` holds
// the members' interned tuple identities (equal tuples share one id; see
// event_identity), so set equality and inclusion are those of the tuples.
struct Interval {
  double start = 0.0;
  double end = 0.0;
  std::vector<std::size_t> members;
  std::set<std::uint32_t> identities;
};

// Python's tuple comparison: start, then end, then the sets, where a set is
// "less" only when it is a proper subset (equal sets compare equal).
bool interval_less(const Interval& left, const Interval& right) {
  if (left.start != right.start) return left.start < right.start;
  if (left.end != right.end) return left.end < right.end;
  if (left.identities == right.identities) return false;
  if (left.identities.size() >= right.identities.size()) return false;
  return std::includes(right.identities.begin(), right.identities.end(),
                       left.identities.begin(), left.identities.end());
}

// The union of a pair the merge removes: `left` is consumed rather than
// copied, so growing one batch costs only the members it gains.
Interval merged_interval(Interval&& left, const Interval& right,
                         const std::vector<std::uint32_t>& identities) {
  Interval merged{std::min(left.start, right.start),
                  std::max(left.end, right.end), std::move(left.members),
                  std::move(left.identities)};
  for (const auto member : right.members)
    if (merged.identities.insert(identities[member]).second) merged.members.push_back(member);
  return merged;
}

// The interval list the reference rewrites in place. Intervals stay where
// they were created and the list orders handles to them, so removing a pair,
// appending a union and re-sorting move small handles, never the sets.
struct IntervalList {
  std::vector<Interval> pool;
  std::vector<std::uint32_t> order;
  const Interval& operator[](std::size_t position) const { return pool[order[position]]; }
  bool less(std::uint32_t left, std::uint32_t right) const {
    return interval_less(pool[left], pool[right]);
  }
  // Removes the pair at `index` and appends its union, as the reference does.
  void merge_pair(std::size_t index, const std::vector<std::uint32_t>& identities) {
    const auto left = order[index], right = order[index + 1];
    auto merged = merged_interval(std::move(pool[left]), pool[right], identities);
    pool[right] = Interval{};
    order.erase(order.begin() + static_cast<std::ptrdiff_t>(index),
                order.begin() + static_cast<std::ptrdiff_t>(index + 2));
    order.push_back(static_cast<std::uint32_t>(pool.size()));
    pool.push_back(std::move(merged));
  }
};

void sift_down(IntervalList& heap, std::size_t start, std::size_t position) {
  auto& order = heap.order;
  const auto item = order[position];
  while (position > start) {
    const auto parent = (position - 1) / 2;
    if (!heap.less(item, order[parent])) break;
    order[position] = order[parent];
    position = parent;
  }
  order[position] = item;
}

void sift_up(IntervalList& heap, std::size_t position) {
  auto& order = heap.order;
  const auto end = order.size();
  const auto start = position;
  const auto item = order[position];
  auto child = 2 * position + 1;
  while (child < end) {
    const auto right = child + 1;
    if (right < end && !heap.less(order[child], order[right])) child = right;
    order[position] = order[child];
    position = child;
    child = 2 * position + 1;
  }
  order[position] = item;
  sift_down(heap, start, position);
}

// The reference builds one interval per event in input order and heapifies
// the list (Python heapq order) before merging.
IntervalList heapified_intervals(const std::vector<IndexedEvent>& values,
                                 const std::vector<std::size_t>& rows,
                                 const std::vector<std::uint32_t>& identities) {
  if (rows.size() > std::numeric_limits<std::uint32_t>::max() / 2)
    throw std::invalid_argument("batches: too many events in one activity and resource group");
  IntervalList intervals;
  intervals.pool.reserve(rows.size());
  intervals.order.reserve(rows.size());
  for (const auto row : rows) {
    intervals.order.push_back(static_cast<std::uint32_t>(intervals.pool.size()));
    intervals.pool.push_back({values[row].event.start, values[row].event.complete, {row}, {identities[row]}});
  }
  for (auto index = intervals.order.size() / 2; index > 0; --index)
    sift_up(intervals, index - 1);
  return intervals;
}

// When the next interval starts strictly before the current end, the pair
// is removed, their union is appended, the list is sorted again (Python's
// sort, which compares the event sets on tied bounds) and the scan starts
// over, until nothing merges. Appending (rather than replacing the pair in
// place) and restarting decide which of two equal events, such as case ids
// 0 and 0.0, a batch keeps: a union keeps its left set's.
void merge_overlapping(IntervalList& intervals,
                       const std::vector<std::uint32_t>& identities) {
  bool changed = true;
  while (changed) {
    changed = false;
    std::size_t index = 0;
    while (index + 1 < intervals.order.size()) {
      if (!(intervals[index + 1].start < intervals[index].end)) {
        ++index;
        continue;
      }
      intervals.merge_pair(index, identities);
      python::sort(intervals.order, [&](std::uint32_t left, std::uint32_t right) {
        return intervals.less(left, right);
      });
      changed = true;
      index = 0;
    }
  }
}

// The reference merges near intervals by scanning adjacent pairs, removing a
// merged pair and heap-pushing the union back into the list, and repeating
// the scan until nothing merges. The list is not re-sorted, so later scans
// see heap order and can join intervals farther apart than the distance.
void merge_near(IntervalList& intervals, double distance,
                const std::vector<std::uint32_t>& identities) {
  bool changed = true;
  while (changed) {
    changed = false;
    std::size_t index = 0;
    while (index + 1 < intervals.order.size()) {
      if (intervals[index + 1].start - intervals[index].end > distance) {
        ++index;
        continue;
      }
      intervals.merge_pair(index, identities);
      sift_down(intervals, 0, intervals.order.size() - 1);
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

// One tuple member's identity, length-prefixed so that no member's text can
// run into the next. A NaN is a float object of its own row, equal to no
// other row's NaN (the reference boxes each float64 cell separately), unless
// it is the np.nan a "str" column holds for every missing cell.
std::string tuple_member(const ScalarValue& value, std::size_t row, bool shared_nan = false) {
  const auto id = value.kind == ScalarValue::Kind::kFloat && std::isnan(value.real)
                      ? (shared_nan ? std::string("Fnan#shared") : "Fnan#" + std::to_string(row))
                      : python::identity(value);
  return std::to_string(id.size()) + ":" + id;
}

// The identity of an event's (start, complete, case[, id]) tuple: equal
// tuples are one set member.
std::string event_identity(const IndexedEvent& value, std::size_t row, bool include_event_id) {
  std::string key = tuple_member(python::real(value.event.start), row) +
                    tuple_member(python::real(value.event.complete), row) +
                    tuple_member(value.case_value, row, value.case_shared_nan);
  if (include_event_id)
    key += tuple_member(value.event_id.value_or(python::none()), row, value.event_id_shared_nan);
  return key;
}

// Whether two tuple members are the same (Python's `is`, then ==): a NaN is
// the float of its own row, so two rows' NaNs differ (and order neither way),
// except the shared np.nan of "str" columns.
bool same_member(const ScalarValue& a, std::size_t row_a, bool shared_a,
                 const ScalarValue& b, std::size_t row_b, bool shared_b) {
  if (row_a == row_b) return true;
  const auto nan = [](const ScalarValue& v) { return v.kind == ScalarValue::Kind::kFloat && std::isnan(v.real); };
  if (nan(a) && nan(b)) return shared_a && shared_b;
  if (nan(a) || nan(b)) return false;
  return python::equal(a, b);
}

// Python's ordering of two event tuples: the first member pair that is not
// the same decides with `<`, and a pair that cannot be ordered raises.
int compare_events(const IndexedEvent& a, std::size_t row_a, const IndexedEvent& b, std::size_t row_b,
                   bool include_event_id) {
  if (a.event.start != b.event.start) return a.event.start < b.event.start ? -1 : 1;
  if (a.event.complete != b.event.complete) return a.event.complete < b.event.complete ? -1 : 1;
  if (!same_member(a.case_value, row_a, a.case_shared_nan, b.case_value, row_b, b.case_shared_nan))
    return compare_scalars(a.case_value, b.case_value);
  if (!include_event_id) return 0;
  const auto id_a = a.event_id.value_or(python::none());
  const auto id_b = b.event_id.value_or(python::none());
  if (same_member(id_a, row_a, a.event_id_shared_nan, id_b, row_b, b.event_id_shared_nan)) return 0;
  return compare_scalars(id_a, id_b);
}

BatchDiscovery detect_grouped(std::vector<IndexedEvent> values,
                              BatchDiscoveryOptions options,
                              bool include_event_id) {
  validate_options(options);

  // Group by (activity, resource) as pandas does (python::groupby): labels
  // are the first equal value in each column, groups are ordered by key, and
  // int labels in a level with floats can become floats.
  std::vector<std::vector<ScalarValue>> keys;
  keys.reserve(values.size());
  bool nulls = false;
  for (const auto& value : values) {
    keys.push_back({value.activity_value, value.resource_is_null ? python::none() : value.resource_value});
    nulls = nulls || value.resource_is_null;
  }
  std::vector<python::Group> groups;
  try {
    groups = python::groupby(keys, nulls);
  } catch (const python::TypeError& error) {
    throw std::invalid_argument(std::string("batches: ") + error.what());
  }

  // Each event tuple's identity, interned: equal tuples share one id.
  std::vector<std::uint32_t> identities;
  identities.reserve(values.size());
  {
    std::unordered_map<std::string, std::uint32_t> interned;
    interned.reserve(values.size());
    for (std::size_t row = 0; row < values.size(); ++row) {
      const auto next = static_cast<std::uint32_t>(interned.size());
      identities.push_back(interned.try_emplace(event_identity(values[row], row, include_event_id), next).first->second);
    }
  }

  struct KeyedGroup {
    BatchGroup group;
    ScalarValue activity;
    ScalarValue resource;
  };
  std::vector<KeyedGroup> result;
  for (const auto& group : groups) {
    auto intervals = heapified_intervals(values, group.rows, identities);
    merge_overlapping(intervals, identities);
    merge_near(intervals, options.merge_distance_seconds, identities);

    std::vector<Batch> batches;
    for (const auto handle : intervals.order) {
      const auto& interval = intervals.pool[handle];
      // An interval's events are a set, so repeated events count once and
      // the minimum size applies to the distinct events.
      if (interval.members.size() < options.min_batch_size) continue;
      // The reference sorts a kept batch's event tuples; a number and a text
      // case (or id) with equal bounds cannot be ordered.
      auto ordered = interval.members;
      python::sort(ordered, [&](std::size_t left, std::size_t right) {
        return compare_events(values[left], left, values[right], right, include_event_id) < 0;
      });
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
    BatchGroup out;
    const bool null_resource = group.labels[1].kind == ScalarValue::Kind::kNone;
    out.activity = python::str(group.labels[0]);
    out.resource = null_resource ? std::string{} : python::str(group.labels[1]);
    out.resource_is_null = null_resource;
    out.batch_count = batches.size();
    for (const auto& batch : batches) out.event_count += batch.events.size();
    out.batches = std::move(batches);
    result.push_back({std::move(out), group.labels[0], group.labels[1]});
  }
  // sorted(..., key=(batch count, (activity, resource)), reverse=True): a
  // number and a text, or a null resource and a text one, cannot be compared.
  python::sort_reverse(result, [](const KeyedGroup& left, const KeyedGroup& right) {
    if (left.group.batch_count != right.group.batch_count)
      return left.group.batch_count < right.group.batch_count;
    if (!python::equal(left.activity, right.activity)) return compare_scalars(left.activity, right.activity) < 0;
    if (python::equal(left.resource, right.resource)) return false;
    return compare_scalars(left.resource, right.resource) < 0;
  });
  std::vector<BatchGroup> groups_out;
  groups_out.reserve(result.size());
  for (auto& entry : result) groups_out.push_back(std::move(entry.group));
  return BatchDiscovery{options, options.variant, std::move(groups_out)};
}

void append_log_events(
    const EventLog& log, const BatchDiscoveryOptions& options,
    std::vector<IndexedEvent>& values) {
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
      // An event that starts after it completes is taken as it is (round BN,
      // observed black-box).
      const auto& case_id = required_case_value(trace, event,
                                                options.case_id_attribute);
      BatchEvent batch_event{start, *complete, case_id, trace_index, event_index,
                             {}};
      values.push_back(IndexedEvent{std::move(batch_event), activity, resource, std::nullopt, false, false, false,
                                    text_scalar(case_id), text_scalar(activity), text_scalar(resource), false});
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

// A table cell's Python object: typed and lexical bool, int and float cells
// are numbers with their exact value; anything else is its text.
ScalarValue scalar_value(const EventAttribute& cell) {
  auto scalar = python::from_attribute(cell);
  if (scalar.kind == ScalarValue::Kind::kText) scalar.text = attribute_text(cell);
  return scalar;
}

std::vector<IndexedEvent> table_events(const EventTable& table,
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
  // Without an event-id selector the table adapters read a frame's
  // "@@index" column (format_dataframe's) as the event id when it has one, so
  // two rows alike in every other value stay two events (round BN, observed
  // black-box).
  const bool default_event_id = options.event_id_attribute.empty() && exact_column(table, "@@index");
  const auto event_id_selector = default_event_id
                                     ? TableSelector{"@@index", exact_column(table, "@@index"), SelectorRole::kGeneric}
                                     : options.event_id_attribute.empty()
                                     ? TableSelector{}
                                     : resolve_selector(
                                           table, options.event_id_attribute,
                                           SelectorRole::kGeneric, "event id", true);
  const bool event_id_selected = (default_event_id || !options.event_id_attribute.empty()) &&
                                 event_id_selector.column.has_value();
  // pandas' "str" columns hold one np.nan object for every missing cell.
  const auto str_column = [&](const TableSelector& selector) {
    return pandas && selector.column && *selector.column < table.dtypes.size() &&
           table.dtypes[*selector.column] == "str";
  };
  const bool case_str = str_column(case_selector);
  const bool event_id_str = event_id_selected && str_column(event_id_selector);

  // Polars requires a string (or categorical) case column, even when empty.
  // A table typed by the pandas CSV adapter (a mined CSV source) reaches the
  // Polars variant as the frame pl.from_pandas makes of it: its "str",
  // "string" and "object" text columns are String and "category" is
  // Categorical. Numeric cells of an object column are refused per row below.
  if (polars && case_selector.column && *case_selector.column < table.dtypes.size()) {
    const std::string_view dtype = table.dtypes[*case_selector.column];
    const bool text = dtype.starts_with("String") || dtype.starts_with("Utf8") ||
                      dtype.starts_with("Categorical") || dtype.starts_with("Enum") ||
                      dtype == "str" || dtype == "string" || dtype == "object" ||
                      dtype == "category";
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
  std::vector<IndexedEvent> values;
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
    // A pd.NA case id stays pd.NA (str "<NA>"; comparing it raises), except
    // in a "str" column, which holds it as np.nan.
    const bool case_na = !case_str && case_cell && std::holds_alternative<NativeNA>(case_cell->typed_value);
    // A "str" column holds every missing cell (None included) as np.nan.
    const bool pandas_case_nan =
        pandas && !case_na && (missing(case_cell) || (case_str && case_cell->is_null));
    // In an object column a missing case is None, not NaN: it orders against
    // nothing but None, so comparing it with another case id raises. Its
    // label stays the declared "nan".
    const bool case_none = pandas_case_nan && !case_cell && case_selector.column &&
                           (*case_selector.column >= table.dtypes.size() ||
                            table.dtypes[*case_selector.column] == "object");
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
    // str() of the case id: a bool is True/False, a float keeps Python's
    // shortest spelling, a pandas NaN is "nan".
    const auto case_id =
        pandas_case_nan ? std::string("nan") : python::str(scalar_value(*case_cell));
    const auto complete =
        timestamp_cell ? seconds_of(attribute_text(*timestamp_cell))
                       : std::optional<double>{};
    if (timestamp_cell && (!complete || !std::isfinite(*complete)))
      throw std::invalid_argument(
          "batches: table row has an invalid completion timestamp");

    std::optional<double> start = complete;
    if (!options.start_timestamp_attribute.empty()) {
      const auto* start_cell = row_cell(row, table, start_selector);
      // pandas reads a missing start (a blank CSV cell is NaT in the frame)
      // as the smallest datetime (round BN, observed black-box).
      if ((!start_cell || attribute_text(*start_cell).empty()) && pandas) {
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
    // An event that starts after it completes is taken as it is (round BN,
    // observed black-box).
    const double completion =
        complete ? rescale(*complete, completion_scale) : kNaT;
    const double start_time =
        options.start_timestamp_attribute.empty()
            ? completion
            : (start ? rescale(*start, start_scale) : kNaT);

    // The selected event id stays a typed scalar (a null cell is None) so
    // equal ids of different types are one event and incomparable ids raise.
    std::optional<ScalarValue> event_id;
    bool event_id_shared_nan = false;
    if (event_id_selected) {
      const auto& event_id_cell = *event_id_selector.column < row.size() ? row[*event_id_selector.column] : std::optional<EventAttribute>{};
      event_id = event_id_cell ? scalar_value(*event_id_cell) : python::none();
      if (event_id_str && (!event_id_cell || event_id_cell->is_null || missing(&*event_id_cell))) {
        event_id = python::real(std::numeric_limits<double>::quiet_NaN());
        event_id_shared_nan = true;
      }
    }
    BatchEvent batch_event{start_time, completion, case_id, 0, row_index,
                           event_id ? python::str(*event_id) : std::string{}};
    auto case_value = case_none         ? python::none()
                      : pandas_case_nan ? python::real(std::numeric_limits<double>::quiet_NaN())
                                        : scalar_value(*case_cell);
    auto activity_value = scalar_value(*activity_cell);
    auto resource_value = null_resource ? python::none() : scalar_value(*resource_cell);
    values.push_back(IndexedEvent{std::move(batch_event), activity, resource, std::move(event_id), pandas_case_nan,
                                  pandas_case_nan && case_str, event_id_shared_nan,
                                  std::move(case_value), std::move(activity_value), std::move(resource_value),
                                  null_resource});
  }
  return values;
}

BatchDiscovery discover_table(const EventTable& table,
                              BatchDiscoveryOptions options,
                              BatchDiscoveryVariant variant) {
  options.variant = variant;
  validate_options(options);
  auto values = table_events(table, options);
  return detect_grouped(std::move(values), std::move(options), true);
}

}  // namespace

BatchDiscovery discover_batches(const EventLog& log,
                                BatchDiscoveryOptions options) {
  validate_options(options);
  std::vector<IndexedEvent> values;
  append_log_events(log, options, values);
  return detect_grouped(std::move(values), std::move(options), false);
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
