#pragma once

// Shared helpers for the OCEL writers that reproduce what the reference
// writers emit from their pandas frames: the column dtype a frame built from
// the OCEL's typed values would get, Python's float and str reprs, and the
// text pandas prints for a UTC timestamp.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "minera/filtering_common.hpp"
#include "minera/ocel.hpp"
#include "minera/timestamp.hpp"

namespace minera::ocel_frame {

// The dtype pandas gives a column of a frame built from records: an integer
// column with a missing cell becomes float64, a boolean one object. A column
// of datetimes alone (missing cells are NaT) is datetime64, naive or aware;
// next to any other value, or naive next to aware, it is object (round EK).
enum class Dtype { kEmpty, kInt64, kFloat64, kBool, kObject, kDatetime };

// Whether `text` (a timestamp text) is a naive one.
inline bool naive_timestamp_text(std::string_view text) {
  const auto parsed = parse_timestamp_exact(text);
  return parsed && !parsed->timezone_aware;
}

struct ColumnStats {
  bool has_bool = false;
  bool has_int = false;
  bool has_float = false;
  bool has_string = false;
  // Round EK: date values (a date-declared text the reference reads as a
  // datetime), naive and aware apart.
  bool has_naive_date = false;
  bool has_aware_date = false;
  // Round EO: an all-NULL type table column was concatenated in (an object
  // column then, see kOcelHintNone).
  bool has_none = false;
  std::size_t present = 0;

  void observe(const OcelValue& value) { observe(value, false); }
  // `date`: the value is a datetime in the reference frame.
  void observe(const OcelValue& value, bool date) {
    if (std::holds_alternative<std::nullptr_t>(value)) return;
    ++present;
    if (std::holds_alternative<bool>(value)) has_bool = true;
    else if (std::holds_alternative<std::int64_t>(value)) has_int = true;
    else if (std::holds_alternative<double>(value)) has_float = true;
    else if (date) (naive_timestamp_text(std::get<std::string>(value)) ? has_naive_date : has_aware_date) = true;
    else has_string = true;
  }
  void merge(const ColumnStats& other) {
    has_bool = has_bool || other.has_bool;
    has_int = has_int || other.has_int;
    has_float = has_float || other.has_float;
    has_string = has_string || other.has_string;
    has_naive_date = has_naive_date || other.has_naive_date;
    has_aware_date = has_aware_date || other.has_aware_date;
    has_none = has_none || other.has_none;
    present += other.present;
  }
  [[nodiscard]] bool has_date() const { return has_naive_date || has_aware_date; }

  [[nodiscard]] Dtype dtype(std::size_t rows) const {
    if (present == 0) return Dtype::kEmpty;
    if (has_none) return Dtype::kObject;
    const bool missing = present < rows;
    if (has_date())
      return has_string || has_bool || has_int || has_float || (has_naive_date && has_aware_date)
                 ? Dtype::kObject
                 : Dtype::kDatetime;
    if (has_string) return Dtype::kObject;
    if (has_bool)
      return has_int || has_float || missing ? Dtype::kObject : Dtype::kBool;
    if (has_float || missing) return Dtype::kFloat64;
    return Dtype::kInt64;
  }
};

// The dtype of two frames' column concatenated (pandas concat): equal dtypes
// stay, an empty side (the column missing there, so NaN cells) keeps the
// other's dtype save that ints and booleans then hold NaN, anything else is
// object; ints and floats make float64.
inline Dtype concat_dtype(Dtype left, Dtype right) {
  if (left == right) return left;
  if (left == Dtype::kEmpty) std::swap(left, right);
  if (right == Dtype::kEmpty)
    return left == Dtype::kInt64 ? Dtype::kFloat64 : left == Dtype::kBool ? Dtype::kObject : left;
  if ((left == Dtype::kInt64 && right == Dtype::kFloat64) || (left == Dtype::kFloat64 && right == Dtype::kInt64))
    return Dtype::kFloat64;
  return Dtype::kObject;
}

// Whether a catalog declares `key` of `owner` a date ("date", "time",
// "timestamp", ... in any case), as the reference OCEL 2.0 readers convert.
inline bool declared_date(const OcelTypeCatalog& catalog, const std::string& owner,
                          const std::string& key);
// A date attribute's string value as a timestamp text, or nullptr when the
// value is no string or the text is no timestamp (kept as written).
inline const std::string* date_value_text(const OcelValue& value);
// Whether `value` of `owner`'s `key` is a datetime in the reference frame: a
// text the catalog declares a date and that reads as a timestamp (the native
// readers keep the texts the reference keeps as texts unread).
inline bool reference_date_value(const OcelTypeCatalog& catalog, const std::string& owner,
                                 const std::string& key, const OcelValue& value) {
  return date_value_text(value) != nullptr && declared_date(catalog, owner, key);
}

// Columns in first-appearance order with their value statistics.
class Columns {
 public:
  void observe(const std::string& key, const OcelValue& value) {
    const auto slot = register_name(key);
    stats_[slot].observe(value);
    kept_[slot].observe(value);
  }
  void observe(const std::string& key, const OcelValue& value, bool date) {
    const auto slot = register_name(key);
    stats_[slot].observe(value, date);
    kept_[slot].observe(value, date);
  }
  void observe_row(const std::vector<OcelAttribute>& attributes) {
    ++rows_;
    for (const auto& attribute : attributes) observe(attribute.key, attribute.value);
  }
  // Round EK: a row of `owner`, its date values (by `catalog`) as datetimes.
  void observe_row(const std::vector<OcelAttribute>& attributes, const OcelTypeCatalog& catalog,
                   const std::string& owner) {
    ++rows_;
    for (const auto& attribute : attributes)
      observe(attribute.key, attribute.value,
              reference_date_value(catalog, owner, attribute.key, attribute.value));
  }
  void add_rows(std::size_t rows) { rows_ += rows; }
  // Round EN: a change row of `owner` (counts no row): its field's value and
  // its other cells (OcelObjectChange::cells), date values by the catalog.
  void observe_change(const OcelObjectChange& change, const OcelTypeCatalog& catalog, const std::string& owner) {
    observe(change.field, change.value, reference_date_value(catalog, owner, change.field, change.value));
    for (const auto& cell : change.cells)
      observe(cell.key, cell.value, reference_date_value(catalog, owner, cell.key, cell.value));
  }
  void observe_change(const OcelObjectChange& change) {
    observe(change.field, change.value);
    for (const auto& cell : change.cells) observe(cell.key, cell.value);
  }

  // Round EM: the frame as the reference built it (see OcelFrameHint): its
  // column order first, then the dropped rows' values and row count. Call
  // before observing the kept rows.
  void seed(const OcelFrameHint& hint) {
    for (const auto& name : hint.columns) register_name(name);
    for (const auto& column : hint.dropped) {
      auto& stats = stats_[register_name(column.key)];
      stats.has_bool = stats.has_bool || (column.kinds & kOcelHintBool);
      stats.has_int = stats.has_int || (column.kinds & kOcelHintInt);
      stats.has_float = stats.has_float || (column.kinds & kOcelHintFloat);
      stats.has_string = stats.has_string || (column.kinds & kOcelHintString);
      stats.has_naive_date = stats.has_naive_date || (column.kinds & kOcelHintNaiveDate);
      stats.has_aware_date = stats.has_aware_date || (column.kinds & kOcelHintAwareDate);
      stats.has_none = stats.has_none || (column.kinds & kOcelHintNone);
      stats.present += column.present;
    }
    rows_ += hint.dropped_rows;
    seeded_rows_ += hint.dropped_rows;
  }

  [[nodiscard]] const std::vector<std::string>& names() const { return names_; }
  // The column's position in first-appearance order (the end for no column).
  [[nodiscard]] std::size_t position(const std::string& key) const {
    const auto found = index_.find(key);
    return found == index_.end() ? names_.size() : found->second;
  }
  [[nodiscard]] std::size_t rows() const { return rows_; }
  [[nodiscard]] const ColumnStats* stats(const std::string& key) const {
    const auto found = index_.find(key);
    return found == index_.end() ? nullptr : &stats_[found->second];
  }
  [[nodiscard]] Dtype dtype(const std::string& key) const {
    const auto* found = stats(key);
    return found == nullptr ? Dtype::kEmpty : found->dtype(rows_);
  }
  // Round EO: the dtype write_ocel_json, write_ocel2_json and write_ocel_xml
  // print an events or objects frame column's cells as: an object column
  // whose observed cells (not the seeded dropped rows) are numbers alone,
  // ints and floats without a boolean, prints as float64 when one of those
  // rows lacks a cell or holds a float, as int64 otherwise (pandas'
  // infer_objects; observed black-box: such a column is object only through
  // rows the reader dropped, a text or datetime there). The type
  // declarations, write_ocel2_xml, write_ocel_csv and the changes frame keep
  // the object column.
  [[nodiscard]] Dtype printed_dtype(const std::string& key) const {
    const auto found = index_.find(key);
    if (found == index_.end()) return Dtype::kEmpty;
    const auto column = stats_[found->second].dtype(rows_);
    const auto& kept = kept_[found->second];
    if (column != Dtype::kObject || kept.present == 0 || kept.has_bool || kept.has_string || kept.has_date())
      return column;
    return kept.has_float || kept.present < rows_ - seeded_rows_ ? Dtype::kFloat64 : Dtype::kInt64;
  }

 private:
  std::size_t register_name(const std::string& key) {
    auto [slot, added] = index_.try_emplace(key, names_.size());
    if (added) {
      names_.push_back(key);
      stats_.emplace_back();
      kept_.emplace_back();
    }
    return slot->second;
  }

  std::vector<std::string> names_;
  std::vector<ColumnStats> stats_;
  // The observed cells alone (printed_dtype).
  std::vector<ColumnStats> kept_;
  std::unordered_map<std::string, std::size_t> index_;
  std::size_t rows_ = 0;
  std::size_t seeded_rows_ = 0;
};

// Round EN: the changes frame's column order with its "ocel:field" and
// "ocel:timestamp" columns placed among `names` (the frame's attribute
// columns, seeded from `hint`): where the hint puts them
// (OcelFrameHint::field_at, time_at), else after the attribute columns,
// before "@@cumcount" (pm4py's JSON and XML readers: write_ocel2_sqlite
// writes them after the other columns, observed black-box).
inline constexpr const char* kChangeFieldColumn = "ocel:field";
inline constexpr const char* kChangeTimeColumn = "ocel:timestamp";
inline std::vector<std::string> change_frame_order(const OcelFrameHint& hint,
                                                   const std::vector<std::string>& names) {
  const std::size_t total = names.size() + 2;
  std::vector<std::string> order;
  order.reserve(total);
  if (hint.field_at && hint.time_at && *hint.field_at < total && *hint.time_at < total &&
      *hint.field_at != *hint.time_at) {
    std::size_t next = 0;
    for (std::size_t at = 0; at < total; ++at) {
      if (at == *hint.field_at) order.emplace_back(kChangeFieldColumn);
      else if (at == *hint.time_at) order.emplace_back(kChangeTimeColumn);
      else order.push_back(names[next++]);
    }
    return order;
  }
  bool placed = false;
  for (const auto& name : names) {
    if (!placed && name == kOcelCumcountColumn) {
      order.emplace_back(kChangeFieldColumn);
      order.emplace_back(kChangeTimeColumn);
      placed = true;
    }
    order.push_back(name);
  }
  if (!placed) {
    order.emplace_back(kChangeFieldColumn);
    order.emplace_back(kChangeTimeColumn);
  }
  return order;
}

// Round EN: a frame hint's build order `columns` is redundant when the
// frame's first appearance over the kept rows gives it: no dropped rows, no
// field or time places, and the same names in the same order.
inline void trim_frame_hint(OcelFrameHint& hint, const std::vector<std::string>& natural) {
  if (!hint.dropped.empty() || hint.dropped_rows != 0 || hint.field_at || hint.time_at ||
      !hint.reserved.empty())
    return;
  if (hint.columns == natural) hint.columns.clear();
}

// Round EP: a frame's columns in pandas' order (OcelFrameHint::reserved):
// the attribute columns `attributes` in the writer's order with the frame's
// own columns among them where the hint places them; `defaults` (those
// columns in their default order) the hint does not place go first.
inline std::vector<std::string> frame_column_order(const OcelFrameHint& hint,
                                                   const std::vector<std::string>& defaults,
                                                   const std::vector<std::string>& attributes) {
  std::vector<std::string> order;
  const auto placed = [&](const std::string& name) {
    return std::any_of(hint.reserved.begin(), hint.reserved.end(),
                       [&](const auto& entry) { return entry.first == name; });
  };
  for (const auto& name : defaults)
    if (!placed(name)) order.push_back(name);
  if (hint.reserved.empty()) {
    order.insert(order.end(), attributes.begin(), attributes.end());
    return order;
  }
  std::unordered_map<std::string_view, std::size_t> position;
  for (std::size_t i = 0; i < attributes.size(); ++i) position.emplace(attributes[i], i);
  // Each placed column's anchor: the writer's attribute it stands before
  // (attributes.size(): after every one).
  std::vector<std::size_t> anchors;
  for (const auto& [name, at] : hint.reserved) {
    std::size_t anchor = attributes.size();
    if (at == 0) {
      anchor = 0;
    } else if (at != kOcelHintEnd) {
      for (std::size_t k = at; k < hint.columns.size(); ++k)
        if (const auto found = position.find(hint.columns[k]); found != position.end()) {
          anchor = found->second;
          break;
        }
    }
    anchors.push_back(anchor);
  }
  for (std::size_t i = 0; i <= attributes.size(); ++i) {
    for (std::size_t r = 0; r < hint.reserved.size(); ++r)
      if (anchors[r] == i) order.push_back(hint.reserved[r].first);
    if (i < attributes.size()) order.push_back(attributes[i]);
  }
  return order;
}

// Round EP: the hint's `reserved` for a frame whose columns stand in
// `order` (its own columns `own` among the attribute columns; an own column
// `order` lacks is last), none when the own columns lead in `defaults`
// order. Places count in the frame's attribute columns (hint.columns, or
// `natural` when the hint has none: then they become hint.columns).
inline void hint_reserved_order(OcelFrameHint& hint, const std::vector<std::string>& order,
                                const std::vector<std::string>& own,
                                const std::vector<std::string>& defaults,
                                const std::vector<std::string>& natural) {
  const auto& columns = hint.columns.empty() ? natural : hint.columns;
  const auto is_own = [&](const std::string& name) { return std::find(own.begin(), own.end(), name) != own.end(); };
  std::vector<std::pair<std::string, std::size_t>> reserved;
  for (std::size_t i = 0; i < order.size(); ++i) {
    if (!is_own(order[i])) continue;
    std::size_t at = kOcelHintEnd;
    for (std::size_t k = i + 1; k < order.size() && at == kOcelHintEnd; ++k) {
      if (is_own(order[k])) continue;
      if (const auto found = std::find(columns.begin(), columns.end(), order[k]); found != columns.end())
        at = static_cast<std::size_t>(found - columns.begin());
    }
    reserved.emplace_back(order[i], at);
  }
  for (const auto& name : own)
    if (std::none_of(reserved.begin(), reserved.end(), [&](const auto& entry) { return entry.first == name; }))
      reserved.emplace_back(name, kOcelHintEnd);
  bool leading = reserved.size() == defaults.size();
  for (std::size_t i = 0; leading && i < reserved.size(); ++i)
    leading = reserved[i].first == defaults[i] && (reserved[i].second == 0 || columns.empty());
  if (leading) return;
  for (auto& entry : reserved)
    if (entry.second == kOcelHintEnd && columns.empty()) entry.second = 0;
  if (hint.columns.empty()) hint.columns = natural;
  hint.reserved = std::move(reserved);
}

// Round EM: a frame of the log's events, objects or changes as the
// reference holds it: seeded with the rows a reader or writer dropped
// (OcelFrameHint), then the kept rows. `dated`: date values by the catalog.
inline Columns event_frame(const Ocel& log, bool dated = false) {
  Columns frame;
  frame.seed(log.event_frame_hint);
  for (const auto& event : log.events) {
    if (dated) frame.observe_row(event.attributes, log.event_type_attributes, event.activity);
    else frame.observe_row(event.attributes);
  }
  return frame;
}
inline Columns object_frame(const Ocel& log, bool dated = false) {
  Columns frame;
  frame.seed(log.object_frame_hint);
  for (const auto& object : log.objects) {
    if (dated) frame.observe_row(object.attributes, log.object_type_attributes, object.type);
    else frame.observe_row(object.attributes);
  }
  return frame;
}

// Round EO: the names write_ocel_xml and write_ocel_json list as the log's
// "attribute-names": the events and objects frames' columns (the hints'
// dropped rows included), without the "ocel:" ones, sorted.
inline std::set<std::string> frame_attribute_names(const Ocel& log) {
  std::set<std::string> names;
  const auto events = event_frame(log);
  const auto objects = object_frame(log);
  for (const auto& name : events.names())
    if (!name.starts_with("ocel:")) names.insert(name);
  for (const auto& name : objects.names())
    if (!name.starts_with("ocel:")) names.insert(name);
  return names;
}

// Round EM: rows a native log never held but the reference frames were built
// from (an OCEL 2.0 SQLite type table's rows that are no event or object of
// their own: change rows, rows without an id, repeated ids), as an owner and
// its non-null cells.
struct HintRow {
  std::string owner;
  std::vector<OcelAttribute> cells;
};

// Round EM: adds `extra_events` and `extra_objects` to the log's event and
// object frame hints as dropped rows, and `extra_change_values` (the cells of
// change rows outside their field) to the change frame hint as values
// without rows (the change rows are the log's own).
// Round EO: `extra_change_rows` are rows of the changes frame the reader
// dropped (an OCEL 2.0 SQLite type table's rows without a changed field).
inline void hint_extra_rows(Ocel& log, const std::vector<HintRow>& extra_events,
                            const std::vector<HintRow>& extra_objects,
                            const std::vector<HintRow>& extra_change_values,
                            const std::vector<HintRow>& extra_change_rows = {}) {
  const auto kinds_of = [](const ColumnStats& stats) {
    return static_cast<unsigned char>((stats.has_bool ? kOcelHintBool : 0) | (stats.has_int ? kOcelHintInt : 0) |
                                      (stats.has_float ? kOcelHintFloat : 0) |
                                      (stats.has_string ? kOcelHintString : 0) |
                                      (stats.has_naive_date ? kOcelHintNaiveDate : 0) |
                                      (stats.has_aware_date ? kOcelHintAwareDate : 0) |
                                      (stats.has_none ? kOcelHintNone : 0));
  };
  const auto fold = [&](OcelFrameHint& hint, const OcelTypeCatalog& catalog, const std::vector<HintRow>& extra,
                        bool rows, const auto& kept_rows) {
    if (extra.empty()) return;
    Columns all, dropped;
    all.seed(hint);
    dropped.seed(OcelFrameHint{{}, hint.dropped, hint.dropped_rows});
    kept_rows(all);
    for (const auto& row : extra) {
      if (rows) {
        all.add_rows(1);
        dropped.add_rows(1);
      }
      for (const auto& cell : row.cells) {
        const bool date = reference_date_value(catalog, row.owner, cell.key, cell.value);
        all.observe(cell.key, cell.value, date);
        dropped.observe(cell.key, cell.value, date);
      }
    }
    hint.columns = all.names();
    hint.dropped.clear();
    for (const auto& name : dropped.names()) {
      const auto* stats = dropped.stats(name);
      if (stats == nullptr || (stats->present == 0 && !stats->has_none)) continue;
      hint.dropped.push_back({name, kinds_of(*stats), stats->present});
    }
    hint.dropped_rows = dropped.rows();
  };
  fold(log.event_frame_hint, log.event_type_attributes, extra_events, true, [&](Columns& all) {
    for (const auto& event : log.events) all.observe_row(event.attributes);
  });
  fold(log.object_frame_hint, log.object_type_attributes, extra_objects, true, [&](Columns& all) {
    for (const auto& object : log.objects) all.observe_row(object.attributes);
  });
  fold(log.change_frame_hint, log.object_type_attributes, extra_change_values, false, [&](Columns& all) {
    for (const auto& change : log.object_changes) all.observe_change(change);
  });
  fold(log.change_frame_hint, log.object_type_attributes, extra_change_rows, true, [&](Columns& all) {
    for (const auto& change : log.object_changes) all.observe_change(change);
  });
}

// Round EO: marks `keys` in `hint` as columns a type table holds only NULLs
// in (kOcelHintNone).
inline void hint_none_columns(OcelFrameHint& hint, const std::vector<std::string>& keys) {
  for (const auto& key : keys) {
    const auto found = std::find_if(hint.dropped.begin(), hint.dropped.end(),
                                    [&](const OcelFrameHintColumn& column) { return column.key == key; });
    if (found != hint.dropped.end()) found->kinds = static_cast<unsigned char>(found->kinds | kOcelHintNone);
    else hint.dropped.push_back({key, kOcelHintNone, 0});
  }
}

// Round EM: records in the log's frame hints the rows about to be dropped:
// the events, objects and changes `drop_event`, `drop_object` and
// `drop_change` name (the frames' column order over every row, then the
// dropped rows' value kinds; see OcelFrameHint).
template <class DropEvent, class DropObject, class DropChange>
void hint_dropped_rows(Ocel& log, DropEvent&& drop_event, DropObject&& drop_object, DropChange&& drop_change) {
  const auto kinds_of = [](const ColumnStats& stats) {
    return static_cast<unsigned char>((stats.has_bool ? kOcelHintBool : 0) | (stats.has_int ? kOcelHintInt : 0) |
                                      (stats.has_float ? kOcelHintFloat : 0) |
                                      (stats.has_string ? kOcelHintString : 0) |
                                      (stats.has_naive_date ? kOcelHintNaiveDate : 0) |
                                      (stats.has_aware_date ? kOcelHintAwareDate : 0) |
                                      (stats.has_none ? kOcelHintNone : 0));
  };
  // all: the frame over every row (its column order); dropped: the hint's
  // dropped rows and the new ones.
  const auto record = [&](OcelFrameHint& hint, const Columns& all, Columns& dropped, std::size_t rows) {
    if (rows == 0) return;
    hint.columns = all.names();
    hint.dropped.clear();
    for (const auto& name : dropped.names()) {
      const auto* stats = dropped.stats(name);
      if (stats == nullptr || (stats->present == 0 && !stats->has_none)) continue;
      hint.dropped.push_back({name, kinds_of(*stats), stats->present});
    }
    hint.dropped_rows = dropped.rows();
  };
  {
    Columns all, dropped;
    all.seed(log.event_frame_hint);
    dropped.seed(OcelFrameHint{{}, log.event_frame_hint.dropped, log.event_frame_hint.dropped_rows});
    std::size_t rows = 0;
    for (const auto& event : log.events) {
      all.observe_row(event.attributes, log.event_type_attributes, event.activity);
      if (!drop_event(event)) continue;
      ++rows;
      dropped.observe_row(event.attributes, log.event_type_attributes, event.activity);
    }
    record(log.event_frame_hint, all, dropped, rows);
  }
  std::unordered_map<std::string_view, std::string_view> type_of;
  {
    Columns all, dropped;
    all.seed(log.object_frame_hint);
    dropped.seed(OcelFrameHint{{}, log.object_frame_hint.dropped, log.object_frame_hint.dropped_rows});
    std::size_t rows = 0;
    for (const auto& object : log.objects) {
      type_of.emplace(object.id, object.type);
      all.observe_row(object.attributes, log.object_type_attributes, object.type);
      if (!drop_object(object)) continue;
      ++rows;
      dropped.observe_row(object.attributes, log.object_type_attributes, object.type);
    }
    record(log.object_frame_hint, all, dropped, rows);
  }
  {
    Columns all, dropped;
    all.seed(log.change_frame_hint);
    dropped.seed(OcelFrameHint{{}, log.change_frame_hint.dropped, log.change_frame_hint.dropped_rows});
    std::size_t rows = 0;
    for (const auto& change : log.object_changes) {
      std::string owner = change.object_type;
      if (owner.empty())
        if (const auto found = type_of.find(change.object_id); found != type_of.end()) owner = found->second;
      all.add_rows(1);
      all.observe_change(change, log.object_type_attributes, owner);
      if (!drop_change(change)) continue;
      ++rows;
      dropped.add_rows(1);
      dropped.observe_change(change, log.object_type_attributes, owner);
    }
    record(log.change_frame_hint, all, dropped, rows);
  }
}

// Round EK: a row's attributes as the reference writers print a frame row:
// in the frame's column order (first appearance over the rows), null cells
// left out unless `keep_null`; a repeated key keeps its last value.
inline std::vector<const OcelAttribute*> frame_row(const std::vector<OcelAttribute>& attributes,
                                                   const Columns& frame, bool keep_null = false) {
  std::vector<const OcelAttribute*> out;
  std::unordered_map<std::string_view, std::size_t> seen;
  for (const auto& attribute : attributes) {
    const auto [slot, added] = seen.try_emplace(attribute.key, out.size());
    if (added) out.push_back(&attribute);
    else out[slot->second] = &attribute;
  }
  if (!keep_null)
    std::erase_if(out, [](const OcelAttribute* attribute) {
      return std::holds_alternative<std::nullptr_t>(attribute->value);
    });
  std::stable_sort(out.begin(), out.end(), [&](const OcelAttribute* left, const OcelAttribute* right) {
    return frame.position(left->key) < frame.position(right->key);
  });
  return out;
}

// One owner's attribute declarations in output order, per owner.
using TypeDeclarations =
    std::map<std::string, std::vector<std::pair<std::string, std::string>>>;

// The reference OCEL 2.0 writers (write_ocel2_xml, write_ocel2_json) and
// write_ocel_json's type catalogs declare what each type's rows hold
// (observed black-box, round EK): "float" for a float64 frame column (an
// integer column with a missing cell is one), "date" for a datetime64 one;
// in an object column, by the type's own values: "date" when all are
// datetimes (naive next to aware too: round EO, x52_0046), "float" when
// floats with no text or datetime (ints and booleans next to them),
// otherwise "string" (also integer and boolean columns). Each
// type lists its attributes with a value in global column order (changed
// fields after the static ones, typed by the changes frame when the type
// holds no static value); declared-only catalog entries follow with their
// declared type, so empty types and attributes survive.
inline std::string reference_declared_type(Dtype column, const ColumnStats* owner) {
  if (column == Dtype::kFloat64) return "float";
  if (column == Dtype::kDatetime) return "date";
  if (column != Dtype::kObject || owner == nullptr) return "string";
  if (owner->has_string) return "string";
  if (owner->has_date()) return owner->has_bool || owner->has_int || owner->has_float ? "string" : "date";
  return owner->has_float ? "float" : "string";
}

// Round EN: `with_catalog` adds the declared types and attributes without
// rows or values (the transport keeps them); pm4py's writers declare only
// what the frames hold (observed black-box, probe en o/osw.py).
inline void reference_type_declarations(const Ocel& log, TypeDeclarations& object_types,
                                        TypeDeclarations& event_types, bool with_catalog = false) {
  Columns object_columns, change_columns, event_columns;
  // Round EM: the columns as the reference built them (OcelFrameHint).
  object_columns.seed(log.object_frame_hint);
  change_columns.seed(log.change_frame_hint);
  event_columns.seed(log.event_frame_hint);
  // Per owner: the stats of its own rows (objects frame and changes frame).
  std::map<std::string, Columns> object_owner, change_owner, event_owner;
  std::unordered_map<std::string_view, std::string_view> object_type_of;
  for (const auto& object : log.objects) {
    object_type_of.emplace(object.id, object.type);
    object_columns.observe_row(object.attributes, log.object_type_attributes, object.type);
    object_owner[object.type].observe_row(object.attributes, log.object_type_attributes, object.type);
  }
  for (const auto& change : log.object_changes) {
    std::string owner = change.object_type;
    if (owner.empty())
      if (const auto found = object_type_of.find(change.object_id); found != object_type_of.end())
        owner = std::string(found->second);
    change_columns.add_rows(1);
    change_columns.observe_change(change, log.object_type_attributes, owner);
    auto& own = change_owner[owner];
    own.add_rows(1);
    own.observe_change(change, log.object_type_attributes, owner);
  }
  for (const auto& event : log.events) {
    event_columns.observe_row(event.attributes, log.event_type_attributes, event.activity);
    event_owner[event.activity].observe_row(event.attributes, log.event_type_attributes, event.activity);
  }
  const auto owner_stats = [](const std::map<std::string, Columns>& owners, const std::string& owner,
                              const std::string& key) -> const ColumnStats* {
    const auto found = owners.find(owner);
    if (found == owners.end()) return nullptr;
    const auto* stats = found->second.stats(key);
    return stats == nullptr || stats->present == 0 ? nullptr : stats;
  };
  using Present = std::map<std::string, std::unordered_set<std::string>>;
  const auto build = [&](const OcelTypeCatalog& catalog, const Present& present,
                         const std::vector<const Columns*>& order,
                         const std::vector<const std::map<std::string, Columns>*>& owners,
                         TypeDeclarations& table) {
    for (const auto& [owner, keys] : present) {
      auto& declarations = table[owner];
      std::unordered_set<std::string> written;
      for (const auto* columns : order)
        for (const auto& name : columns->names()) {
          if (!keys.contains(name) || !written.insert(name).second) continue;
          // The static frame's column types a type holding a static value,
          // the changes frame's one holding only changes.
          std::size_t frame = 0;
          const ColumnStats* own = owner_stats(*owners[0], owner, name);
          if (own == nullptr && order.size() > 1) {
            frame = 1;
            own = owner_stats(*owners[1], owner, name);
          }
          auto dtype = order[frame]->dtype(name);
          if (frame == 0 && dtype == Dtype::kEmpty && order.size() > 1) dtype = order[1]->dtype(name);
          declarations.emplace_back(name, reference_declared_type(dtype, own));
        }
    }
    if (!with_catalog) return;
    for (const auto& [owner, schema] : catalog) {
      auto& declarations = table[owner];
      for (const auto& [name, type] : schema) {
        const bool listed = std::any_of(declarations.begin(), declarations.end(),
                                        [&](const auto& entry) { return entry.first == name; });
        if (!listed) declarations.emplace_back(name, type.empty() ? "string" : type);
      }
    }
  };
  Present object_present, event_present;
  for (const auto& object : log.objects)
    for (const auto& attribute : object.attributes)
      if (!std::holds_alternative<std::nullptr_t>(attribute.value))
        object_present[object.type].insert(attribute.key);
  for (const auto& change : log.object_changes) {
    if (!std::holds_alternative<std::nullptr_t>(change.value))
      object_present[change.object_type].insert(change.field);
    // Round EN: a change row's other cells too (read_ocel2_sqlite's
    // "@@cumcount" is declared a string attribute, observed black-box).
    for (const auto& cell : change.cells)
      if (!std::holds_alternative<std::nullptr_t>(cell.value)) object_present[change.object_type].insert(cell.key);
  }
  for (const auto& event : log.events)
    for (const auto& attribute : event.attributes)
      if (!std::holds_alternative<std::nullptr_t>(attribute.value))
        event_present[event.activity].insert(attribute.key);
  build(log.object_type_attributes, object_present, {&object_columns, &change_columns},
        {&object_owner, &change_owner}, object_types);
  build(log.event_type_attributes, event_present, {&event_columns}, {&event_owner}, event_types);
  for (auto* table : {&object_types, &event_types})
    for (auto& [owner, declarations] : *table)
      std::erase_if(declarations, [](const auto& entry) { return entry.first.starts_with("ocel:"); });
  for (const auto& object : log.objects) object_types[object.type];
  for (const auto& event : log.events) event_types[event.activity];
}

// pandas to_sql's column type: from the dtype, or for an object column from
// the kinds of its non-null values in the written table. Round EK: ints with
// a missing cell in the table (`rows` its rows) are REAL there (observed
// black-box: the table's column is float64, an int stored as a float).
inline std::string sql_type(Dtype dtype, const ColumnStats& table_values, std::size_t rows = 0) {
  switch (dtype) {
    case Dtype::kInt64:
    case Dtype::kBool:
      return "INTEGER";
    case Dtype::kFloat64:
      return "REAL";
    case Dtype::kDatetime:
      return "TIMESTAMP";
    default:
      break;
  }
  if (table_values.has_string || table_values.has_date()) return "TEXT";
  if (table_values.has_bool)
    return table_values.has_int || table_values.has_float ? "TEXT" : "INTEGER";
  if (table_values.has_float) return "REAL";
  if (table_values.has_int) return table_values.present < rows ? "REAL" : "INTEGER";
  return "TEXT";
}

// Python's repr of a float; "nan", "inf" and "-inf" when not finite.
inline std::string float_repr(double value) {
  if (std::isnan(value)) return "nan";
  if (std::isinf(value)) return value > 0 ? "inf" : "-inf";
  return filtering_float_repr(value);
}

// Python's str of a scalar: True/False, the int, the float repr, the text.
inline std::string python_str(const OcelValue& value) {
  if (std::holds_alternative<std::nullptr_t>(value)) return "None";
  if (const auto* flag = std::get_if<bool>(&value)) return *flag ? "True" : "False";
  if (const auto* integer = std::get_if<std::int64_t>(&value)) return std::to_string(*integer);
  if (const auto* real = std::get_if<double>(&value)) return float_repr(*real);
  return std::get<std::string>(value);
}

// Python's repr of a str: single quotes unless the text holds a single quote
// and no double quote; backslash, the quote and control characters escaped.
// Bytes from 0x80 up are kept (non-ASCII text is printable).
inline std::string python_str_repr(std::string_view text) {
  const bool double_quote = text.find('\'') != std::string_view::npos &&
                            text.find('"') == std::string_view::npos;
  const char quote = double_quote ? '"' : '\'';
  std::string out(1, quote);
  for (const char ch : text) {
    const auto byte = static_cast<unsigned char>(ch);
    if (ch == '\\') out += "\\\\";
    else if (ch == quote) { out += '\\'; out += ch; }
    else if (ch == '\n') out += "\\n";
    else if (ch == '\r') out += "\\r";
    else if (ch == '\t') out += "\\t";
    else if (byte < 0x20 || byte == 0x7f) {
      static constexpr char digits[] = "0123456789abcdef";
      out += "\\x";
      out += digits[byte >> 4];
      out += digits[byte & 15];
    } else {
      out += ch;
    }
  }
  out += quote;
  return out;
}

// A parsed timestamp's civil date and time of day (UTC for an aware one,
// the wall clock for a naive one); nullopt outside years 1..9999.
struct CivilTimestamp {
  long long year, month, day, hour, minute, second;
  std::uint32_t nanoseconds;
  bool timezone_aware;
};

inline std::optional<CivilTimestamp> civil_timestamp(const ParsedExactTimestamp& parsed) {
  std::int64_t days = parsed.seconds / 86400;
  std::int64_t rest = parsed.seconds % 86400;
  if (rest < 0) {
    rest += 86400;
    --days;
  }
  // Civil date from days since 1970-01-01 (Howard Hinnant's algorithm).
  const std::int64_t z = days + 719468;
  const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const std::int64_t doe = z - era * 146097;
  const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  std::int64_t year = yoe + era * 400;
  const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const std::int64_t mp = (5 * doy + 2) / 153;
  const std::int64_t day = doy - (153 * mp + 2) / 5 + 1;
  const std::int64_t month = mp < 10 ? mp + 3 : mp - 9;
  if (month <= 2) ++year;
  if (year < 1 || year > 9999) return std::nullopt;
  return CivilTimestamp{static_cast<long long>(year), static_cast<long long>(month),
                        static_cast<long long>(day), static_cast<long long>(rest / 3600),
                        static_cast<long long>(rest / 60 % 60), static_cast<long long>(rest % 60),
                        parsed.nanoseconds, parsed.timezone_aware};
}

// "Y-MM-DD<separator>HH:MM:SS" plus `digits` fraction digits (0, 3, 6 or 9),
// the year zero-padded to four digits when `pad_year`.
inline std::string civil_timestamp_text(const CivilTimestamp& time, char separator, int digits,
                                        bool pad_year) {
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, pad_year ? "%04lld-%02lld-%02lld%c%02lld:%02lld:%02lld"
                                                : "%lld-%02lld-%02lld%c%02lld:%02lld:%02lld",
                time.year, time.month, time.day, separator, time.hour, time.minute, time.second);
  std::string out(buffer);
  if (digits > 0) {
    unsigned fraction = time.nanoseconds;
    for (int drop = 9 - digits; drop > 0; --drop) fraction /= 10;
    std::snprintf(buffer, sizeof buffer, ".%0*u", digits, fraction);
    out += buffer;
  }
  return out;
}

// An event time pandas holds as NaT: the empty text native keeps for an
// OCEL 2.0 SQLite '' or NULL time, sent on in transports (round EG). pandas
// sorts it after every time and prints it "NaT".
inline bool nat_time_text(std::string_view text) { return text.empty(); }
// Round EK: a change time pandas reads as NaT from a text (its NaT strings:
// "NaT", "nat", "NAT", "nan", "NaN", "NAN"; observed black-box in
// read_ocel2_json's change frame), or the empty text.
inline bool pandas_nat_time_text(std::string_view text) {
  return text.empty() || text == "NaT" || text == "nat" || text == "NAT" || text == "nan" ||
         text == "NaN" || text == "NAN";
}
// The error pm4py raises taking .timestamp() of a missing event time: NaT's,
// or None's for a log whose time column holds None (Ocel::none_event_times,
// round EH).
inline std::string nat_timestamp_error(const Ocel& log) {
  return log.none_event_times ? "AttributeError: 'NoneType' object has no attribute 'timestamp'"
                              : "ValueError: NaTType does not support timestamp";
}

// The text pandas prints for a timestamp the reference holds as UTC:
// "YYYY-MM-DD HH:MM:SS[.ffffff]+00:00" (separator ' ', suffix "+00:00"), or
// with separator 'T' and suffix "Z" for the OCEL 1.0 XML date. A naive text
// gets no suffix; text that is no timestamp is returned as is.
// With `microseconds`, nanosecond digits are cut first, as a value passed
// through a Python datetime is (pm4py.write_ocel2_sqlite, round BW).
inline std::string utc_timestamp_text(std::string_view text, char separator,
                                      std::string_view suffix, bool microseconds = false) {
  auto parsed = parse_timestamp_exact(text);
  if (parsed && microseconds) parsed->nanoseconds -= parsed->nanoseconds % 1000;
  const auto time = parsed ? civil_timestamp(*parsed) : std::nullopt;
  if (!time) return std::string(text);
  const int digits = time->nanoseconds == 0 ? 0 : time->nanoseconds % 1000 == 0 ? 6 : 9;
  auto out = civil_timestamp_text(*time, separator, digits, true);
  if (time->timezone_aware) out += suffix;
  return out;
}

// Round EK: a datetime cell as the reference JSON and OCEL 1.0 XML writers
// print a datetime64 column ("%Y-%m-%dT%H:%M:%SZ", a naive one too), the
// fraction kept (declared: pm4py drops it); text that is no timestamp stays.
inline std::string datetime64_json_text(std::string_view text) {
  const auto parsed = parse_timestamp_exact(text);
  const auto time = parsed ? civil_timestamp(*parsed) : std::nullopt;
  if (!time) return std::string(text);
  const int digits = time->nanoseconds == 0 ? 0 : time->nanoseconds % 1000 == 0 ? 6 : 9;
  return civil_timestamp_text(*time, 'T', digits, true) + "Z";
}
// Round EK: a datetime cell of an object column (dates next to other values)
// as every reference writer prints it: its isoformat ("+00:00", a naive one
// without offset).
inline std::string object_datetime_text(std::string_view text) {
  return utc_timestamp_text(text, 'T', "+00:00");
}

// The texts DataFrame.to_csv writes for a timestamp column holding `texts`.
// A column of naive timestamps is datetime64 and formatted as a whole: dates
// alone ("2024-01-01", the year unpadded) when every value is a midnight,
// otherwise "Y-MM-DD HH:MM:SS" with as many fraction digits as the most
// precise value needs (3, 6 or 9) on every value. A column of aware
// timestamps is written value by value as str() of each: in the offset the
// values share (pm4py.read_ocel_csv keeps a fixed "+02:00"; the other native
// importers store UTC), and in UTC when they do not share one. A column
// mixing kinds or holding text that is no timestamp is written value by value
// too, as utc_timestamp_text with suffix "+00:00" (observed black-box on
// pm4py.write_ocel_csv).
inline std::vector<std::string> pandas_timestamp_column_text(
    const std::vector<std::string_view>& texts) {
  // Round EH: a NaT time (an empty text) is an empty cell, and the column's
  // format follows its other values, as pandas' to_csv writes it (observed
  // black-box).
  if (std::any_of(texts.begin(), texts.end(), [](std::string_view text) { return nat_time_text(text); })) {
    std::vector<std::string_view> known;
    for (const auto text : texts)
      if (!nat_time_text(text)) known.push_back(text);
    auto printed = pandas_timestamp_column_text(known);
    std::vector<std::string> out;
    out.reserve(texts.size());
    std::size_t next = 0;
    for (const auto text : texts) out.push_back(nat_time_text(text) ? std::string() : std::move(printed[next++]));
    return out;
  }
  std::vector<CivilTimestamp> times;
  times.reserve(texts.size());
  bool naive_column = true, aware_column = true;
  std::optional<std::int32_t> offset;  // minutes, when every aware value shares it
  bool one_offset = true;
  std::vector<ParsedExactTimestamp> instants;
  instants.reserve(texts.size());
  for (const auto text : texts) {
    const auto parsed = parse_timestamp_exact(text);
    const auto time = parsed ? civil_timestamp(*parsed) : std::nullopt;
    if (!time) {
      naive_column = aware_column = false;
      break;
    }
    (time->timezone_aware ? naive_column : aware_column) = false;
    if (time->timezone_aware) {
      if (offset && *offset != parsed->offset_minutes) one_offset = false;
      offset = parsed->offset_minutes;
    }
    times.push_back(*time);
    instants.push_back(*parsed);
  }
  std::vector<std::string> out;
  out.reserve(texts.size());
  if (aware_column && one_offset && offset && *offset != 0) {
    char suffix[16];
    const auto minutes = *offset < 0 ? -*offset : *offset;
    std::snprintf(suffix, sizeof suffix, "%c%02d:%02d", *offset < 0 ? '-' : '+',
                  static_cast<int>(minutes / 60), static_cast<int>(minutes % 60));
    for (std::size_t index = 0; index < texts.size(); ++index) {
      auto local = instants[index];
      local.seconds += static_cast<std::int64_t>(*offset) * 60;
      const auto time = civil_timestamp(local);
      if (!time) {
        out.push_back(utc_timestamp_text(texts[index], ' ', "+00:00"));
        continue;
      }
      const int digits = time->nanoseconds == 0 ? 0 : time->nanoseconds % 1000 == 0 ? 6 : 9;
      out.push_back(civil_timestamp_text(*time, ' ', digits, true) + suffix);
    }
    return out;
  }
  if (!naive_column || texts.empty()) {
    for (const auto text : texts) out.push_back(utc_timestamp_text(text, ' ', "+00:00"));
    return out;
  }
  bool dates_only = true;
  int digits = 0;
  for (const auto& time : times) {
    if (time.hour != 0 || time.minute != 0 || time.second != 0 || time.nanoseconds != 0)
      dates_only = false;
    if (time.nanoseconds % 1000 != 0) digits = 9;
    else if (time.nanoseconds % 1000000 != 0) digits = std::max(digits, 6);
    else if (time.nanoseconds != 0) digits = std::max(digits, 3);
  }
  for (const auto& time : times) {
    if (dates_only) {
      char buffer[32];
      std::snprintf(buffer, sizeof buffer, "%lld-%02lld-%02lld", time.year, time.month, time.day);
      out.emplace_back(buffer);
    } else {
      out.push_back(civil_timestamp_text(time, ' ', digits, false));
    }
  }
  return out;
}

// The offset (minutes) that every value of an aware timestamp column shares,
// when it is not zero. pm4py.read_ocel_csv keeps such a column with its
// fixed offset (datetime64[us, UTC+02:00]; mixed offsets raise there), and
// the reference writers print each value's wall clock in it (observed
// black-box: OCEL 2.0 XML "2024-01-01T10:00:00+02:00", OCEL 1.0 XML
// "2024-01-01T10:00:00Z", SQLite "2024-01-01 10:00:00+02:00"). The other
// native importers store UTC, which gives nullopt.
inline std::optional<std::int32_t> shared_nonzero_offset(
    const std::vector<std::string_view>& texts) {
  std::optional<std::int32_t> offset;
  for (const auto text : texts) {
    // Round EH: a NaT time leaves the column's offset as it is.
    if (nat_time_text(text)) continue;
    const auto parsed = parse_timestamp_exact(text);
    if (!parsed || !parsed->timezone_aware) return std::nullopt;
    if (offset && *offset != parsed->offset_minutes) return std::nullopt;
    offset = parsed->offset_minutes;
  }
  if (!offset || *offset == 0) return std::nullopt;
  return offset;
}

// `text` as utc_timestamp_text prints it, but in the wall clock of
// `offset_minutes` (nullopt: UTC) and followed by `suffix`, or by the offset
// itself ("+02:00") when `suffix` is nullopt.
inline std::string offset_timestamp_text(std::string_view text, char separator,
                                         std::optional<std::int32_t> offset_minutes,
                                         std::optional<std::string_view> suffix,
                                         bool microseconds = false) {
  if (!offset_minutes) return utc_timestamp_text(text, separator, suffix.value_or("+00:00"), microseconds);
  auto parsed = parse_timestamp_exact(text);
  if (!parsed || !parsed->timezone_aware)
    return utc_timestamp_text(text, separator, suffix.value_or("+00:00"), microseconds);
  if (microseconds) parsed->nanoseconds -= parsed->nanoseconds % 1000;
  parsed->seconds += static_cast<std::int64_t>(*offset_minutes) * 60;
  const auto time = civil_timestamp(*parsed);
  if (!time) return utc_timestamp_text(text, separator, suffix.value_or("+00:00"), microseconds);
  const int digits = time->nanoseconds == 0 ? 0 : time->nanoseconds % 1000 == 0 ? 6 : 9;
  auto out = civil_timestamp_text(*time, separator, digits, true);
  if (suffix) return out.append(*suffix);
  char buffer[16];
  const auto minutes = *offset_minutes < 0 ? -*offset_minutes : *offset_minutes;
  std::snprintf(buffer, sizeof buffer, "%c%02d:%02d", *offset_minutes < 0 ? '-' : '+',
                static_cast<int>(minutes / 60), static_cast<int>(minutes % 60));
  return out.append(buffer);
}

// Whether a catalog declares `key` of `owner` a date ("date", "time",
// "timestamp", ... in any case), as the reference OCEL 2.0 readers convert.
inline bool declared_date(const OcelTypeCatalog& catalog, const std::string& owner,
                          const std::string& key) {
  const auto schema = catalog.find(owner);
  if (schema == catalog.end()) return false;
  const auto found = schema->second.find(key);
  if (found == schema->second.end()) return false;
  std::string lowered = found->second;
  for (auto& ch : lowered) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return lowered.find("date") != std::string::npos || lowered.find("time") != std::string::npos;
}

// A date attribute's string value as a timestamp text, or nullptr when the
// value is no string or the text is no timestamp (kept as written).
inline const std::string* date_value_text(const OcelValue& value) {
  const auto* text = std::get_if<std::string>(&value);
  if (text == nullptr || !parse_timestamp_exact(*text)) return nullptr;
  return text;
}

}  // namespace minera::ocel_frame
