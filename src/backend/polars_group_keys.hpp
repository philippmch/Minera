#pragma once

#include <algorithm>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "minera/event_data.hpp"
#include "minera/event_log.hpp"
#include "polars_lit_model.hpp"

namespace minera::detail::polars_group_keys {

// Round CO: case keys of the join-based Polars table filters (the case,
// attributes and timestamp filters) given as a list, or as a non-str scalar,
// observed black-box against Polars 1.44.1 through pm4py 2.7.22:
// - each list item is one group_by/join key: a str names a column, any
//   other value is a pl.lit literal (a dict a struct, None a Null literal,
//   an int a dyn int that a frame holds as Int32 when it fits, a list a
//   list literal) whose output column is called `literal`;
// - the keys resolve in list order: a missing column fails as a missing
//   str key fails, a second output name fails group_by's DuplicateError;
// - the groups that pass are selected by their keys and joined back. A key
//   list of literals only selects one row whatever passed (a select of
//   literals has one row), so the inner join keeps every row and the anti
//   join none; a Null literal never joins (inner keeps nothing, anti keeps
//   everything); a column cell that is null never joins either;
// - an inner join on keys holding a literal appends the right side's keys:
//   a column as `<name>_right`, a literal as `literal`. Plain column keys
//   append nothing, and neither does the anti join.
struct Item {
  bool literal = false;
  std::size_t column = 0;  // column keys: the source column
  std::string name;        // the output name (the column, or "literal")
  std::string dtype;       // the output dtype as a frame schema prints it
  EventAttribute cell;     // literal keys: the cell the join appends
  bool null = false;       // a Null literal (never joins)
  polars_lit::LitType type;
  polars_lit::Value value;
};

struct Keys {
  std::vector<Item> items;
  bool extras = false;                // the inner join appends the right side's keys
  std::vector<std::string> appended;  // their names
  bool all_literal() const {
    return std::all_of(items.begin(), items.end(), [](const Item& item) { return item.literal; });
  }
  bool null_literal() const {
    return std::any_of(items.begin(), items.end(), [](const Item& item) { return item.literal && item.null; });
  }
  bool any_literal() const {
    return std::any_of(items.begin(), items.end(), [](const Item& item) { return item.literal; });
  }
};

inline bool text_key(const EventAttribute& v) {
  return v.container == AttributeContainerKind::kScalar && !v.is_null && !is_native_na_attribute(v) &&
         v.kind == AttributeKind::kString && std::holds_alternative<std::monostate>(v.typed_value);
}
// Round DB: group_by/sort take a set's items as keys, like a list's.
inline bool sequence_key(const EventAttribute& v) {
  return v.container == AttributeContainerKind::kList || v.container == AttributeContainerKind::kTuple ||
         v.container == AttributeContainerKind::kSet || v.container == AttributeContainerKind::kFrozenSet;
}

// A non-str scalar or a non-empty list/tuple: the keys this model reads.
inline bool modelled(const EventAttribute& key) {
  if (sequence_key(key)) return !key.children.empty();
  if (key.container != AttributeContainerKind::kScalar || key.is_null || is_native_na_attribute(key)) return false;
  return !text_key(key) && (key.kind == AttributeKind::kInt || key.kind == AttributeKind::kFloat || key.kind == AttributeKind::kBoolean ||
                            std::holds_alternative<std::int64_t>(key.typed_value) || std::holds_alternative<std::uint64_t>(key.typed_value) ||
                            std::holds_alternative<double>(key.typed_value) || std::holds_alternative<bool>(key.typed_value));
}

// The dtype and cell of a literal key (a Python value).
inline Item literal_item(const polars_lit::Value& v) {
  namespace lit = polars_lit;
  Item item;
  item.literal = true;
  item.name = "literal";
  // Round DB: pl.lit refuses a set group key.
  lit::set_literal_error(v);
  if (v.kind == lit::Kind::kDict)
    if (const auto error = lit::dict_literal_error(v)) lit::fail("TypeError", *error);
  auto typed = lit::lit_value(v);
  item.null = typed.type.kind == lit::LitType::K::kNull;
  if (v.kind == lit::Kind::kInt) {
    // A dyn int: Int32 when it fits, Int64, then UInt64 (round DG: Int128
    // past both).
    const auto wide = lit::wide_int(v);
    if (lit::huge_int(v)) item.dtype = "Int128";
    else if (!wide && v.integer >= std::numeric_limits<std::int32_t>::min() && v.integer <= std::numeric_limits<std::int32_t>::max()) item.dtype = "Int32";
    else if (!wide) item.dtype = "Int64";
    else item.dtype = "UInt64";
  } else {
    item.dtype = lit::frame_dtype(typed.type);
  }
  item.cell = lit::literal_cell("literal", typed.type, typed.value);
  item.type = typed.type;
  item.value = typed.value;
  return item;
}
// Round DG: pl.lit refuses an int key past the Int128 range; one inside it
// is an Int128 literal (probe big-pltblcj).
inline Item literal_item(const EventAttribute& value) {
  polars_lit::scalar_int_range(value);
  return literal_item(polars_lit::value_of(value, true));
}

// A key as a plan prints it (col("c"), dyn int: 1, dyn float: 2.5, true,
// null, {1,"x"}, [1, 2]).
inline std::string key_text(const Item& item) {
  namespace lit = polars_lit;
  if (!item.literal) return "col(\"" + item.name + "\")";
  if (item.value.kind == lit::Kind::kInt) return "dyn int: " + lit::integer_text(item.value);
  if (item.value.kind == lit::Kind::kFloat) return "dyn float: " + lit::rust_float(item.value.real);
  return lit::lit_display(item.type, item.value);
}
inline std::string keys_text(const Keys& keys) {
  std::string out;
  for (const auto& item : keys.items) out += (out.empty() ? "" : ", ") + key_text(item);
  return out;
}

// The literal keys, built while Python builds the expressions (a dict that
// pl.lit refuses fails here, before anything resolves).
inline std::vector<std::optional<Item>> build(const EventAttribute& key) {
  std::vector<std::optional<Item>> out;
  const auto one = [&](const EventAttribute& value) { out.push_back(text_key(value) ? std::nullopt : std::optional<Item>(literal_item(value))); };
  if (sequence_key(key)) for (const auto& child : key.children) one(child);
  else one(key);
  return out;
}

// The keys in resolution order. `missing(name)` and `duplicate(name)` must
// throw.
template <typename Missing, typename Duplicate>
Keys resolve(const EventAttribute& key, const std::vector<std::string>& columns, const std::vector<std::string>& dtypes, Missing&& missing,
             Duplicate&& duplicate) {
  Keys keys;
  std::vector<const EventAttribute*> items;
  if (sequence_key(key)) for (const auto& child : key.children) items.push_back(&child);
  else items.push_back(&key);
  auto built = build(key);
  std::vector<std::string> names;
  for (std::size_t i = 0; i < items.size(); ++i) {
    const auto* value = items[i];
    Item item;
    if (!built[i]) {
      const auto at = std::find(columns.begin(), columns.end(), value->value);
      if (at == columns.end()) missing(value->value);
      item.column = static_cast<std::size_t>(at - columns.begin());
      item.name = value->value;
      item.dtype = item.column < dtypes.size() ? dtypes[item.column] : std::string();
    } else {
      item = std::move(*built[i]);
    }
    if (std::find(names.begin(), names.end(), item.name) != names.end()) duplicate(item.name);
    names.push_back(item.name);
    keys.items.push_back(std::move(item));
  }
  keys.extras = keys.any_literal();
  if (keys.extras) {
    std::vector<std::string> all = columns;
    for (const auto& item : keys.items) {
      auto name = std::find(columns.begin(), columns.end(), item.name) == columns.end() ? item.name : item.name + "_right";
      if (std::find(all.begin(), all.end(), name) != all.end())
        throw std::invalid_argument("Native Polars join keys outside observed domain: the right key column " + name + " exists");
      all.push_back(name);
      keys.appended.push_back(std::move(name));
    }
  }
  return keys;
}

// Round CP: the same keys given as Python values (the Polars sequence
// filters' parameters): a str item names a column, any other is a literal.
// Round DI: each item is its own pl.lit, which refuses an int past the
// Int128 range (`integer too large`, before a Series would overflow; probes
// big-plseq*).
inline std::vector<std::optional<Item>> build_values(const std::vector<polars_lit::Value>& key) {
  std::vector<std::optional<Item>> out;
  for (const auto& value : key) {
    if (value.kind == polars_lit::Kind::kInt && !value.text.empty())
      polars_lit::scalar_int_range(EventAttribute{"", value.text, AttributeKind::kInt});
    out.push_back(value.kind == polars_lit::Kind::kString ? std::nullopt : std::optional<Item>(literal_item(value)));
  }
  return out;
}
template <typename Missing, typename Duplicate>
Keys resolve_values(const std::vector<polars_lit::Value>& key, const std::vector<std::string>& columns, const std::vector<std::string>& dtypes,
                    Missing&& missing, Duplicate&& duplicate) {
  Keys keys;
  auto built = build_values(key);
  std::vector<std::string> names;
  for (std::size_t i = 0; i < key.size(); ++i) {
    Item item;
    if (!built[i]) {
      const auto at = std::find(columns.begin(), columns.end(), key[i].text);
      if (at == columns.end()) missing(key[i].text);
      item.column = static_cast<std::size_t>(at - columns.begin());
      item.name = key[i].text;
      item.dtype = item.column < dtypes.size() ? dtypes[item.column] : std::string();
    } else {
      item = std::move(*built[i]);
    }
    if (std::find(names.begin(), names.end(), item.name) != names.end()) duplicate(item.name);
    names.push_back(item.name);
    keys.items.push_back(std::move(item));
  }
  keys.extras = keys.any_literal();
  if (keys.extras)
    for (const auto& item : keys.items) {
      auto name = std::find(columns.begin(), columns.end(), item.name) == columns.end() ? item.name : item.name + "_right";
      if (std::find(columns.begin(), columns.end(), name) != columns.end() ||
          std::find(keys.appended.begin(), keys.appended.end(), name) != keys.appended.end())
        throw std::invalid_argument("Native Polars join keys outside observed domain: the right key column " + name + " exists");
      keys.appended.push_back(std::move(name));
    }
  return keys;
}

// The DuplicateError of a group_by ("group_by keys") or a select
// ("projections") on the keys.
inline std::string duplicate_message(const std::string& name, const std::string& node = "group_by keys") {
  return node + " contained duplicate output name '" + name +
         "'. It's possible that multiple expressions are returning the same default column name. If this is the case, try renaming the "
         "columns with `.alias(\"new_name\")` to avoid duplicate column names.";
}

// The join key of a row (nullopt: a null cell or a Null literal, which never
// joins); `cell_key(cell, dtype)` keys one column cell, "n" for null.
inline std::optional<std::string> row_key(const Keys& keys, const EventTable::Row& row,
                                          const std::function<std::string(const std::optional<EventAttribute>&, const std::string&)>& cell_key) {
  if (keys.null_literal()) return std::nullopt;
  std::string out;
  for (const auto& item : keys.items) {
    if (item.literal) continue;
    const auto& cell = row[item.column];
    if (!cell || cell->is_null || is_native_na_attribute(*cell)) return std::nullopt;
    const auto part = cell_key(cell, item.dtype);
    if (part == "n") return std::nullopt;
    out += std::to_string(part.size()) + ':' + part;
  }
  return out;
}

// Round CT: Polars joins a one-row frame with a one-row right side on a
// single float key by comparing the two values, so a NaN key matches
// nothing there (inner and left joins), while the hash join of taller sides,
// several keys and semi or anti joins match NaN with NaN (fresh CP bank
// 2026111712 case 411, probe ct/j1).
inline bool nan_float_cell(const std::optional<EventAttribute>& cell, const std::string& dtype) {
  if (!dtype.starts_with("Float") || !cell || cell->is_null || is_native_na_attribute(*cell)) return false;
  if (const auto* d = std::get_if<double>(&cell->typed_value)) return *d != *d;
  return cell->value == "NaN" || cell->value == "nan";
}
inline bool one_row_nan_join(const EventTable& source, std::size_t column, std::size_t right_rows) {
  return source.rows.size() == 1 && right_rows == 1 && column < source.dtypes.size() &&
         nan_float_cell(source.rows.front()[column], source.dtypes[column]);
}
inline bool one_row_nan_join(const EventTable& source, const Keys& keys, std::size_t right_rows) {
  return keys.items.size() == 1 && !keys.items.front().literal && one_row_nan_join(source, keys.items.front().column, right_rows);
}

// The group of a row as group_by keys it (nulls form their own group).
inline std::string group_of(const Keys& keys, const EventTable::Row& row,
                            const std::function<std::string(const std::optional<EventAttribute>&, const std::string&)>& cell_key) {
  std::string out;
  for (const auto& item : keys.items) {
    if (item.literal) continue;
    const auto part = cell_key(row[item.column], item.dtype);
    out += std::to_string(part.size()) + ':' + part;
  }
  return out;
}

// The schema an inner join on the keys appends.
inline void append_schema(std::vector<std::string>& columns, std::vector<std::string>& dtypes,
                          std::vector<std::optional<EventTableCategoricalDtype>>& categories, const Keys& keys,
                          const std::vector<std::optional<EventTableCategoricalDtype>>& source_categories) {
  if (!keys.extras) return;
  for (std::size_t i = 0; i < keys.items.size(); ++i) {
    const auto& item = keys.items[i];
    columns.push_back(keys.appended[i]);
    if (!dtypes.empty()) dtypes.push_back(item.dtype);
    if (!categories.empty())
      categories.push_back(item.literal || item.column >= source_categories.size() ? std::nullopt : source_categories[item.column]);
  }
}
inline void append_schema(EventTable& table, const Keys& keys, const EventTable& source) {
  append_schema(table.columns, table.dtypes, table.categorical_dtypes, keys, source.categorical_dtypes);
}

// The cells an inner join on the keys appends to a kept row.
inline void append_cells(EventTable::Row& out, const Keys& keys, const EventTable::Row& row) {
  if (!keys.extras) return;
  for (std::size_t i = 0; i < keys.items.size(); ++i) {
    const auto& item = keys.items[i];
    auto cell = item.literal ? std::optional<EventAttribute>(item.cell) : row[item.column];
    if (cell) cell->key = keys.appended[i];
    out.push_back(std::move(cell));
  }
}

}  // namespace minera::detail::polars_group_keys
