#include "minera/interleavings_merge.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace minera {
namespace {

using Cell = std::optional<EventAttribute>;

// pandas.Timestamp.min / max, the boundary timestamps of the reference.
constexpr const char* kStartStamp = "1677-09-21T00:12:43.145224193+00:00";
constexpr const char* kEndStamp = "2262-04-11T23:47:16.854775807+00:00";
constexpr std::int64_t kStartInstant = std::numeric_limits<std::int64_t>::min() + 1;
constexpr std::int64_t kEndInstant = std::numeric_limits<std::int64_t>::max();
constexpr const char* kStartActivity = "\xE2\x96\xB6";  // ▶
constexpr const char* kEndActivity = "\xE2\x96\xA0";    // ■
constexpr const char* kNaiveMessage =
    "'values' is not ordered, please explicitly specify the categories order by "
    "passing in a categories argument.";

[[noreturn]] void key_error(const std::string& column) {
  throw InterleavingsMergeError("KeyError", "'" + column + "'");
}

std::size_t column_of(const EventTable& table, const std::string& name) {
  const auto found = std::find(table.columns.begin(), table.columns.end(), name);
  if (found == table.columns.end()) key_error(name);
  return static_cast<std::size_t>(found - table.columns.begin());
}

std::string dtype_of(const EventTable& table, std::size_t column) {
  return column < table.dtypes.size() ? table.dtypes[column] : "object";
}

bool missing(const Cell& cell) {
  if (!cell || cell->is_null || cell->kind == AttributeKind::kNA) return true;
  if (const auto* real = std::get_if<double>(&cell->typed_value)) return std::isnan(*real);
  return false;
}

std::optional<std::int64_t> integer_of(const EventAttribute& cell) {
  if (const auto* value = std::get_if<std::int64_t>(&cell.typed_value)) return *value;
  if (const auto* value = std::get_if<std::uint64_t>(&cell.typed_value))
    return static_cast<std::int64_t>(*value);
  return std::nullopt;
}

// A case key: null, an integer or a string; nulls sort last.
struct Key {
  enum Kind { kInt, kString, kNull } kind = kNull;
  std::int64_t integer = 0;
  std::string text;
  bool operator==(const Key&) const = default;
};

Key key_of(const Cell& cell) {
  Key key;
  if (missing(cell)) return key;
  if (cell->kind == AttributeKind::kInt) {
    if (const auto value = integer_of(*cell)) {
      key.kind = Key::kInt;
      key.integer = *value;
      return key;
    }
  }
  key.kind = Key::kString;
  key.text = cell->value;
  return key;
}

bool key_less(const Key& left, const Key& right) {
  if (left.kind != right.kind) return left.kind < right.kind;
  if (left.kind == Key::kInt) return left.integer < right.integer;
  if (left.kind == Key::kString) return left.text < right.text;
  return false;
}

std::int64_t days_from_civil(std::int64_t year, unsigned month, unsigned day) {
  year -= month <= 2;
  const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
  const auto yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

// Nanoseconds since the epoch of "YYYY-MM-DDTHH:MM:SS[.f][±HH:MM|Z]".
std::optional<std::int64_t> instant_of(const Cell& cell) {
  if (missing(cell) || cell->kind != AttributeKind::kDate) return std::nullopt;
  const auto& text = cell->value;
  if (text.size() < 19) return std::nullopt;
  const auto number = [&](std::size_t at, std::size_t length) {
    return std::stoll(text.substr(at, length));
  };
  const auto days = days_from_civil(number(0, 4), static_cast<unsigned>(number(5, 2)),
                                    static_cast<unsigned>(number(8, 2)));
  std::int64_t seconds = days * 86400 + number(11, 2) * 3600 + number(14, 2) * 60 + number(17, 2);
  std::size_t at = 19;
  std::int64_t fraction = 0;
  if (at < text.size() && text[at] == '.') {
    std::size_t digits = 0;
    for (++at; at < text.size() && text[at] >= '0' && text[at] <= '9'; ++at, ++digits)
      if (digits < 9) fraction = fraction * 10 + (text[at] - '0');
    for (; digits < 9; ++digits) fraction *= 10;
  }
  if (at < text.size() && (text[at] == '+' || text[at] == '-')) {
    const std::int64_t offset = number(at + 1, 2) * 3600 + number(at + 4, 2) * 60;
    seconds -= text[at] == '+' ? offset : -offset;
  }
  return seconds * 1'000'000'000 + fraction;
}

EventAttribute attribute(const std::string& key, std::string value, AttributeKind kind) {
  EventAttribute cell;
  cell.key = key;
  cell.value = std::move(value);
  cell.kind = kind;
  return cell;
}

EventAttribute nan_cell(const std::string& key) {
  auto cell = attribute(key, "NaN", AttributeKind::kFloat);
  cell.typed_value = std::numeric_limits<double>::quiet_NaN();
  return cell;
}

EventAttribute int_cell(const std::string& key, std::int64_t value) {
  auto cell = attribute(key, std::to_string(value), AttributeKind::kInt);
  cell.typed_value = value;
  return cell;
}

EventAttribute string_cell(const std::string& key, const std::string& value) {
  return attribute(key, value, AttributeKind::kString);
}

EventAttribute boundary_stamp(const std::string& key, const char* text) {
  auto cell = attribute(key, text, AttributeKind::kDate);
  cell.typed_value = NativeDate{text, NativeDateResolution::kNanosecond};
  return cell;
}

std::string promoted(const std::string& dtype) {
  if (dtype == "int64") return "float64";
  if (dtype == "bool") return "object";
  return dtype;
}

// The cell as a column of `dtype` named `key` holds it.
Cell projected(const Cell& source, const std::string& key, const std::string& dtype) {
  if (missing(source)) {
    if (source && source->kind == AttributeKind::kDate) {
      auto cell = *source;
      cell.key = key;
      return cell;
    }
    return nan_cell(key);
  }
  auto cell = *source;
  cell.key = key;
  if (dtype == "float64" && cell.kind == AttributeKind::kInt) {
    const auto value = integer_of(cell).value_or(0);
    cell = attribute(key, std::to_string(value) + ".0", AttributeKind::kFloat);
    cell.typed_value = static_cast<double>(value);
  }
  if (auto* date = std::get_if<NativeDate>(&cell.typed_value)) {
    if (dtype.starts_with("datetime64[") && dtype.find(", ") != std::string::npos)
      date->timezone = dtype.substr(dtype.find(", ") + 2, dtype.size() - dtype.find(", ") - 3);
    else
      date->timezone = std::nullopt;
  }
  return cell;
}

EventTableIndexLabel index_label(std::int64_t value) { return {int_cell("", value)}; }

struct Row {
  std::vector<Cell> cells;  // aligned with the output columns
  Key case_key;
  std::optional<std::int64_t> instant;
};

bool numeric_dtype(const std::string& dtype) {
  return dtype == "int64" || dtype == "float64" || dtype == "uint64" || dtype == "bool";
}

// (left row, right row) pairs in pandas' inner-merge order: left rows with
// right rows ascending, except when there is exactly one pair per left row.
// Then the key-grouped pairs land at each left row's position in a stable
// sort by key code (codes in first appearance over the left keys, preceded by
// the right keys when both are numeric; numeric unique monotonic keys keep
// the plain order).
std::vector<std::pair<std::size_t, std::size_t>> inner_merge(
    const std::vector<Key>& left, const std::vector<Key>& right, bool numeric) {
  std::vector<std::pair<std::size_t, std::size_t>> pairs;
  for (std::size_t i = 0; i < left.size(); ++i)
    for (std::size_t j = 0; j < right.size(); ++j)
      if (left[i].kind != Key::kNull && left[i] == right[j]) pairs.emplace_back(i, j);
  if (pairs.size() != left.size() || pairs.empty()) return pairs;
  std::vector<Key> codes;
  const auto code_of = [&](const Key& key) {
    const auto found = std::find(codes.begin(), codes.end(), key);
    return static_cast<std::size_t>(found - codes.begin());
  };
  const auto add = [&](const std::vector<Key>& keys) {
    for (const auto& key : keys)
      if (key.kind != Key::kNull && code_of(key) == codes.size()) codes.push_back(key);
  };
  if (numeric) add(right);
  add(left);
  std::vector<std::size_t> left_codes;
  for (const auto& key : left)
    left_codes.push_back(key.kind == Key::kNull ? codes.size() : code_of(key));
  auto sorted_codes = left_codes;
  std::sort(sorted_codes.begin(), sorted_codes.end());
  const bool unique = std::adjacent_find(sorted_codes.begin(), sorted_codes.end()) == sorted_codes.end();
  const auto monotonic = [](const std::vector<Key>& keys) {
    for (std::size_t i = 0; i < keys.size(); ++i) {
      if (keys[i].kind == Key::kNull) return false;
      if (i && key_less(keys[i], keys[i - 1])) return false;
    }
    return true;
  };
  if (numeric && unique && monotonic(left) && monotonic(right)) return pairs;
  std::vector<std::size_t> sorter(left.size());
  for (std::size_t i = 0; i < sorter.size(); ++i) sorter[i] = i;
  std::stable_sort(sorter.begin(), sorter.end(),
                   [&](std::size_t a, std::size_t b) { return left_codes[a] < left_codes[b]; });
  std::vector<std::vector<std::pair<std::size_t, std::size_t>>> by_row(left.size());
  for (const auto& pair : pairs) by_row[pair.first].push_back(pair);
  std::vector<std::pair<std::size_t, std::size_t>> grouped;
  for (const auto row : sorter) grouped.insert(grouped.end(), by_row[row].begin(), by_row[row].end());
  std::vector<std::pair<std::size_t, std::size_t>> ordered(pairs.size());
  for (std::size_t position = 0; position < sorter.size(); ++position)
    ordered[sorter[position]] = grouped[position];
  return ordered;
}

}  // namespace

EventTable interleavings_directly_follows_dataframe(const EventTable& dataframe,
                                                    const InterleavingsMergeParameters& parameters) {
  dataframe.validate();
  const auto& case_key = parameters.case_id_key;
  const auto& activity_key = parameters.activity_key;
  const auto& timestamp_key = parameters.timestamp_key;
  const auto& index_key = parameters.index_key;
  const auto case_column = column_of(dataframe, case_key);
  const auto activity_column = column_of(dataframe, activity_key);
  const auto timestamp_column = column_of(dataframe, timestamp_key);
  const auto timestamp_dtype = dtype_of(dataframe, timestamp_column);
  const auto comma = timestamp_dtype.find(", ");
  const auto zone = comma == std::string::npos
                        ? std::string{}
                        : timestamp_dtype.substr(comma + 2, timestamp_dtype.size() - comma - 3);

  bool any_stamp = false, any_case = false;
  for (const auto& row : dataframe.rows) {
    any_stamp = any_stamp || !missing(row[timestamp_column]);
    any_case = any_case || !missing(row[case_column]);
  }
  if (zone.empty() && any_stamp && any_case) throw InterleavingsMergeError("TypeError", kNaiveMessage);

  // Output columns and dtypes.
  std::vector<std::size_t> others;
  for (std::size_t column = 0; column < dataframe.columns.size(); ++column)
    if (column != case_column && column != activity_column && column != timestamp_column)
      others.push_back(column);
  std::vector<std::string> columns{case_key, timestamp_key, activity_key};
  std::vector<std::string> dtypes{dtype_of(dataframe, case_column),
                                  zone == "UTC" ? "datetime64[ns, UTC]" : "object", "str"};
  std::optional<std::size_t> index_position;
  for (const auto column : others) {
    if (dataframe.columns[column] == index_key) index_position = columns.size();
    columns.push_back(dataframe.columns[column]);
    dtypes.push_back(promoted(dtype_of(dataframe, column)));
  }
  if (!index_position) {
    index_position = columns.size();
    columns.push_back(index_key);
    dtypes.emplace_back();
  }
  dtypes[*index_position] = "int64";
  const auto width = columns.size();

  // Events sorted by (case, timestamp), between the boundary rows.
  const auto sort_rows = [](std::vector<Row>& rows) {
    std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
      if (key_less(a.case_key, b.case_key)) return true;
      if (key_less(b.case_key, a.case_key)) return false;
      if (!a.instant || !b.instant) return a.instant.has_value() && !b.instant.has_value();
      return *a.instant < *b.instant;
    });
  };
  std::vector<Row> events;
  for (const auto& source : dataframe.rows) {
    Row row;
    row.cells.assign(width, std::nullopt);
    row.cells[0] = source[case_column];
    row.cells[1] = source[timestamp_column];
    row.cells[2] = source[activity_column];
    for (std::size_t k = 0; k < others.size(); ++k) row.cells[3 + k] = source[others[k]];
    row.case_key = key_of(source[case_column]);
    row.instant = instant_of(source[timestamp_column]);
    events.push_back(std::move(row));
  }
  sort_rows(events);
  std::vector<Key> cases;
  std::vector<Cell> case_cells;
  for (const auto& source : dataframe.rows) {
    const auto key = key_of(source[case_column]);
    if (key.kind != Key::kNull && std::find(cases.begin(), cases.end(), key) == cases.end()) {
      cases.push_back(key);
      case_cells.push_back(source[case_column]);
    }
  }
  std::vector<std::size_t> case_order(cases.size());
  for (std::size_t i = 0; i < case_order.size(); ++i) case_order[i] = i;
  std::sort(case_order.begin(), case_order.end(),
            [&](std::size_t a, std::size_t b) { return key_less(cases[a], cases[b]); });
  const auto boundary = [&](std::size_t which, bool start) {
    Row row;
    row.cells.assign(width, std::nullopt);
    row.cells[0] = case_cells[which];
    row.cells[1] = boundary_stamp(timestamp_key, start ? kStartStamp : kEndStamp);
    row.cells[2] = string_cell(activity_key, start ? kStartActivity : kEndActivity);
    row.case_key = cases[which];
    row.instant = start ? kStartInstant : kEndInstant;
    return row;
  };
  std::vector<Row> rows;
  for (const auto which : case_order) rows.push_back(boundary(which, true));
  for (auto& row : events) rows.push_back(std::move(row));
  for (const auto which : case_order) rows.push_back(boundary(which, false));
  for (std::size_t position = 0; position < rows.size(); ++position)
    rows[position].cells[*index_position] = int_cell(index_key, static_cast<std::int64_t>(position));
  sort_rows(rows);
  bool identity = true;
  for (std::size_t position = 0; position < rows.size(); ++position)
    identity = identity && integer_of(*rows[position].cells[*index_position]) ==
                               static_cast<std::int64_t>(position);

  EventTable result;
  result.columns = columns;
  for (const auto& column : columns) result.columns.push_back(column + "_2");
  result.dtypes = dtypes;
  for (const auto& dtype : dtypes) result.dtypes.push_back(dataframe.rows.empty() ? dtype : promoted(dtype));
  EventTableIndex index;
  for (std::size_t position = 0; position < rows.size(); ++position) {
    const auto& row = rows[position];
    if (row.case_key.kind == Key::kNull || position + 1 == rows.size() ||
        !(rows[position + 1].case_key == row.case_key))
      continue;
    const auto& next = rows[position + 1];
    EventTable::Row out;
    for (std::size_t k = 0; k < width; ++k) out.push_back(projected(row.cells[k], result.columns[k], result.dtypes[k]));
    for (std::size_t k = 0; k < width; ++k)
      out.push_back(projected(next.cells[k], result.columns[width + k], result.dtypes[width + k]));
    result.rows.push_back(std::move(out));
    index.labels.push_back(index_label(*integer_of(*row.cells[*index_position])));
  }
  bool range = identity;
  for (std::size_t position = 0; position < index.labels.size(); ++position)
    range = range && std::get<EventAttribute>(index.labels[position].value).value == std::to_string(position);
  index.kind = range ? "RangeIndex" : "Index";
  result.index = std::move(index);
  return result;
}

EventTable interleavings_merge_dataframes(const EventTable& left, const EventTable& right,
                                          const EventTable& relations,
                                          const InterleavingsMergeParameters& parameters) {
  const auto left_df = interleavings_directly_follows_dataframe(left, parameters);
  const auto right_df = interleavings_directly_follows_dataframe(right, parameters);
  relations.validate();
  const auto left_relation = column_of(relations, parameters.case_id_key + parameters.left_suffix);
  const auto right_relation = column_of(relations, parameters.case_id_key + parameters.right_suffix);
  const auto left_case = column_of(left_df, parameters.case_id_key);
  const auto right_case = column_of(right_df, parameters.case_id_key);

  const auto in = [](const EventTable& table, const std::string& name) {
    return std::find(table.columns.begin(), table.columns.end(), name) != table.columns.end();
  };
  EventTable result;
  for (std::size_t k = 0; k < left_df.columns.size(); ++k) {
    const auto& name = left_df.columns[k];
    result.columns.push_back(in(right_df, name) ? name + parameters.left_suffix : name);
    result.dtypes.push_back(left_df.dtypes[k]);
  }
  std::vector<std::size_t> relation_others;
  for (std::size_t k = 0; k < relations.columns.size(); ++k) {
    if (k == left_relation || k == right_relation) continue;
    relation_others.push_back(k);
    result.columns.push_back(relations.columns[k]);
    result.dtypes.push_back(dtype_of(relations, k));
  }
  for (std::size_t k = 0; k < right_df.columns.size(); ++k) {
    const auto& name = right_df.columns[k];
    result.columns.push_back(in(left_df, name) ? name + parameters.right_suffix : name);
    result.dtypes.push_back(right_df.dtypes[k]);
  }

  const auto keys = [](const EventTable& table, std::size_t column) {
    std::vector<Key> out;
    for (const auto& row : table.rows) out.push_back(key_of(row[column]));
    return out;
  };
  const auto left_keys = keys(left_df, left_case);
  const auto relation_left_keys = keys(relations, left_relation);
  const auto relation_right_keys = keys(relations, right_relation);
  const auto first = inner_merge(
      left_keys, relation_left_keys,
      numeric_dtype(left_df.dtypes[left_case]) && numeric_dtype(dtype_of(relations, left_relation)));
  std::vector<Key> joined_keys;
  for (const auto& [row, relation] : first) joined_keys.push_back(relation_right_keys[relation]);
  const auto second = inner_merge(
      joined_keys, keys(right_df, right_case),
      numeric_dtype(dtype_of(relations, right_relation)) && numeric_dtype(right_df.dtypes[right_case]));

  EventTableIndex index;
  index.kind = "RangeIndex";
  for (const auto& [joined, right_row] : second) {
    const auto [left_row, relation] = first[joined];
    EventTable::Row out;
    std::size_t column = 0;
    for (const auto& cell : left_df.rows[left_row]) {
      auto copy = cell;
      if (copy) copy->key = result.columns[column];
      out.push_back(std::move(copy));
      ++column;
    }
    for (const auto k : relation_others) {
      auto copy = relations.rows[relation][k];
      if (copy) copy->key = result.columns[column];
      out.push_back(std::move(copy));
      ++column;
    }
    for (const auto& cell : right_df.rows[right_row]) {
      auto copy = cell;
      if (copy) copy->key = result.columns[column];
      out.push_back(std::move(copy));
      ++column;
    }
    index.labels.push_back(index_label(static_cast<std::int64_t>(result.rows.size())));
    result.rows.push_back(std::move(out));
  }
  result.index = std::move(index);
  return result;
}

}  // namespace minera
