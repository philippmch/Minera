#include "minera/ocel_csv.hpp"

#include "minera/csv_event_log_importer.hpp"

#include "minera/timestamp.hpp"
#include "ocel_csv_object_cell.hpp"
#include "ocel_frame_types.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace minera {
namespace {

using Row = std::vector<std::string>;
using Table = std::vector<Row>;

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

// Both tables are read as pandas.read_csv(..., index_col=False) reads them,
// the reference OCEL CSV path. Quoting follows its default C tokenizer: a
// quote opens a quoted field only as a field's first character (anywhere
// else it is literal), "" inside a quoted field is one quote, text after the
// closing quote is appended to the field ("ab"cd is abcd), a quoted field may
// span lines, and an unterminated one is an error. Records that are empty or
// hold only spaces and tabs are skipped but still count as lines; the first
// remaining record is the header. Row shapes: the first data row fixes the
// field count (the header's, or more when that row is longer); a later row
// with more fields is refused with read_csv's message, fields past the header
// are dropped (index_col=False keeps the header's columns), and a short row
// reads its missing trailing cells as missing values (empty here). After a
// blank record ended by a lone "\r", a delimiter is dropped as pandas drops
// it (see skip_after_lone_cr_blank).
Table parse_table(std::string_view input, char delimiter, bool trim_fields,
                  const char* label) {
  if (delimiter == '"' || delimiter == '\r' || delimiter == '\n' ||
      delimiter == '\0') {
    throw std::invalid_argument("invalid CSV delimiter");
  }
  if (input.size() >= 3 && static_cast<unsigned char>(input[0]) == 0xef &&
      static_cast<unsigned char>(input[1]) == 0xbb &&
      static_cast<unsigned char>(input[2]) == 0xbf) {
    input.remove_prefix(3);
  }

  enum class State { kFieldStart, kUnquoted, kQuoted, kQuoteInQuoted };
  Table rows;
  Row row;
  std::string field;
  auto state = State::kFieldStart;
  bool blank = true;  // the record so far holds only spaces and tabs
  std::size_t line = 0;
  std::size_t expected = 0;
  const auto finish_field = [&]() {
    row.push_back(trim_fields ? trim(std::move(field)) : std::move(field));
    field.clear();
    state = State::kFieldStart;
  };
  const auto finish_record = [&]() {
    ++line;
    if (blank) {
      row.clear();
      field.clear();
      state = State::kFieldStart;
      return;
    }
    finish_field();
    if (!rows.empty()) {
      const auto width = rows.front().size();
      if (rows.size() == 1) expected = std::max(width, row.size());
      if (row.size() > expected) {
        const auto fields = [](std::size_t n) {
          return std::to_string(n) + (n == 1 ? " field" : " fields");
        };
        throw std::invalid_argument(
            std::string(label) + " CSV line " + std::to_string(line) + " has " +
            fields(row.size()) + ", but rows of this file have at most " +
            fields(expected) +
            "; pandas.read_csv refuses the file: Error tokenizing data. C error: Expected " +
            std::to_string(expected) + " fields in line " + std::to_string(line) +
            ", saw " + std::to_string(row.size()));
      }
      row.resize(width);
    }
    rows.push_back(std::move(row));
    row = Row{};
    blank = true;
  };

  // pandas' tokenizer quirk after a skipped blank record that a lone "\r"
  // ends (observed black-box): a delimiter right after it is dropped, so
  // "\r\r,5,6" reads as the row 5,6. A record that then starts with spaces or
  // tabs before other text is misread by pandas (rows of garbage), so it is
  // refused here. `i` is the position of the "\r".
  const auto skip_after_lone_cr_blank = [&](std::size_t& i) {
    std::size_t next = i + 1;
    if (next < input.size() && input[next] == delimiter) i = next++;
    if (next >= input.size() || (input[next] != ' ' && input[next] != '\t')) return;
    while (next < input.size() && (input[next] == ' ' || input[next] == '\t')) ++next;
    if (next < input.size() && input[next] != '\n' && input[next] != '\r') {
      throw std::invalid_argument(
          std::string(label) + " CSV line " + std::to_string(line + 1) +
          " starts with spaces or tabs after a blank line ended by a lone carriage return, "
          "which pandas.read_csv misreads; this reader refuses it");
    }
  };
  for (std::size_t i = 0; i < input.size(); ++i) {
    const char ch = input[i];
    if (state == State::kQuoted) {
      if (ch == '"') state = State::kQuoteInQuoted;
      else field.push_back(ch);
    } else if (state == State::kQuoteInQuoted && ch == '"') {
      field.push_back('"');
      state = State::kQuoted;
    } else if (ch == delimiter) {
      blank = false;
      finish_field();
    } else if (ch == '\n' || ch == '\r') {
      const bool lone_cr = ch == '\r' && !(i + 1 < input.size() && input[i + 1] == '\n');
      if (ch == '\r' && !lone_cr) ++i;
      const bool skipped = blank;
      finish_record();
      if (skipped && lone_cr) skip_after_lone_cr_blank(i);
    } else if (state == State::kFieldStart && ch == '"') {
      blank = false;
      state = State::kQuoted;
    } else {
      if (ch != ' ' && ch != '\t') blank = false;
      field.push_back(ch);
      state = State::kUnquoted;
    }
  }
  if (state == State::kQuoted) {
    throw std::invalid_argument(std::string(label) +
                                " CSV contains an unterminated quote");
  }
  finish_record();
  return rows;
}

struct Header {
  std::vector<std::string> names;
  std::unordered_map<std::string, std::size_t> indexes;
};

Header read_header(const Table& table, const OcelCsvImportOptions& options,
                   const char* label) {
  if (!options.has_header) {
    throw std::invalid_argument("OCEL CSV requires a header row");
  }
  if (table.empty()) throw std::invalid_argument(std::string(label) + " CSV is empty");
  // Column names as read_csv gives them: an empty name (a header starting or
  // ending with the delimiter) is "Unnamed: i" and a repeated one "name.1",
  // so ",ocel:eid,..." has an "Unnamed: 0" attribute column and a second
  // "ocel:type:order" column lists objects of type "order.1".
  Header header{reference_csv_header_names(table.front()), {}};
  for (std::size_t i = 0; i < header.names.size(); ++i)
    header.indexes.emplace(header.names[i], i);
  return header;
}

std::size_t required_column(const Header& header, const std::string& name,
                            const char* label) {
  const auto it = header.indexes.find(name);
  if (it == header.indexes.end()) {
    throw std::invalid_argument(std::string(label) + " CSV is missing column: " +
                                name);
  }
  return it->second;
}

const OcelAttribute* find_attribute(const std::vector<OcelAttribute>& attrs,
                                    std::string_view key) {
  const OcelAttribute* found = nullptr;
  for (const auto& attr : attrs) {
    if (attr.key != key) continue;
    if (found != nullptr) {
      throw std::invalid_argument("duplicate OCEL attribute: " + std::string(key));
    }
    found = &attr;
  }
  return found;
}

// A cell as pandas to_csv prints it: Python's str, floats as their repr,
// booleans as True/False; an integer in a float64 frame column (one with a
// float or a missing cell) prints as a float.
std::string value_text(const OcelValue& value, bool float_column = false) {
  if (std::holds_alternative<std::nullptr_t>(value)) return {};
  if (const auto* real = std::get_if<double>(&value)) {
    if (!std::isfinite(*real)) throw std::invalid_argument("OCEL CSV cannot encode non-finite values");
  }
  if (const auto* integer = std::get_if<std::int64_t>(&value); integer && float_column)
    return ocel_frame::float_repr(static_cast<double>(*integer));
  return ocel_frame::python_str(value);
}

// An attribute cell: a value of an attribute the owner's catalog declares a
// date is printed as to_csv prints the reference's datetime64[us, UTC]
// column ("2024-02-01 08:30:00+00:00", value by value).
std::string csv_attribute_text(const OcelAttribute& attribute, const OcelTypeCatalog& catalog,
                               const std::string& owner, ocel_frame::Dtype dtype) {
  if (const auto* text = ocel_frame::declared_date(catalog, owner, attribute.key)
                             ? ocel_frame::date_value_text(attribute.value)
                             : nullptr)
    return ocel_frame::utc_timestamp_text(*text, ' ', "+00:00");
  return value_text(attribute.value, dtype == ocel_frame::Dtype::kFloat64);
}

std::string escape_csv(std::string_view value, char delimiter) {
  bool quote = false;
  if (value.find_first_of(std::string("\"\r\n") + delimiter) !=
      std::string_view::npos) {
    quote = true;
  }
  if (!value.empty() && (value.front() == ' ' || value.front() == '\t' ||
                         value.back() == ' ' || value.back() == '\t')) {
    quote = true;
  }
  if (!quote) return std::string(value);
  std::string out;
  out.reserve(value.size() + 2);
  out.push_back('"');
  for (const char ch : value) {
    if (ch == '"') out.push_back('"');
    out.push_back(ch);
  }
  out.push_back('"');
  return out;
}

void write_row(std::ostream& out, const Row& row, char delimiter) {
  for (std::size_t i = 0; i < row.size(); ++i) {
    if (i != 0) out << delimiter;
    out << escape_csv(row[i], delimiter);
  }
  out << '\n';
}

// An object-id cell as the reference writes it: the Python list repr.
std::string list_text(const std::vector<std::string>& ids) {
  std::string out = "[";
  for (std::size_t i = 0; i < ids.size(); ++i) {
    if (i != 0) out += ", ";
    out += ocel_frame::python_str_repr(ids[i]);
  }
  out += ']';
  return out;
}

struct ObjectIndex {
  std::map<std::string, const OcelObject*> by_id;
  std::set<std::string> types;
};

ObjectIndex validate_objects(const Ocel& log) {
  ObjectIndex result;
  for (const auto& object : log.objects) {
    if (object.id.empty() || object.type.empty()) {
      throw std::invalid_argument("OCEL CSV requires non-empty object ids and types");
    }
    if (!result.by_id.emplace(object.id, &object).second) {
      throw std::invalid_argument("duplicate OCEL object id: " + object.id);
    }
    result.types.insert(object.type);
  }
  return result;
}

std::map<std::string, const OcelEvent*> validate_events(const Ocel& log) {
  std::map<std::string, const OcelEvent*> result;
  for (const auto& event : log.events) {
    if (event.id.empty() || event.activity.empty() || event.timestamp.empty()) {
      throw std::invalid_argument("OCEL CSV requires non-empty event id, activity, and timestamp");
    }
    if (!result.emplace(event.id, &event).second) {
      throw std::invalid_argument("duplicate OCEL event id: " + event.id);
    }
  }
  return result;
}

void check_lossy_fields(const Ocel& log, const OcelCsvExportOptions& options,
                        bool has_object_table) {
  if (options.allow_lossy) return;
  if (!log.o2o.empty() || !log.e2e.empty() || !log.object_changes.empty() ||
      !log.globals.empty()) {
    throw std::invalid_argument(
        "OCEL extended CSV cannot represent O2O/E2E links, object changes, or globals");
  }
  for (const auto& relation : log.relations) {
    if (!relation.qualifier.empty()) {
      throw std::invalid_argument(
          "OCEL extended CSV cannot represent qualified event-object relations");
    }
  }
  if (!has_object_table) {
    for (const auto& object : log.objects) {
      if (!object.attributes.empty()) {
        throw std::invalid_argument(
            "object attributes require the separate OCEL object CSV table");
      }
    }
  }
}

// The event table as the reference writes it: id, timestamp and activity
// (the column order of a frame read from OCEL JSON), the event attributes in
// first-appearance order, then one object list per object type in
// first-appearance order; rows in log order. A cell without objects of a
// type stays empty.
void write_event_table(std::ostream& out, const Ocel& log,
                       const OcelCsvExportOptions& options,
                       bool has_object_table) {
  if (options.delimiter == '"' || options.delimiter == '\r' ||
      options.delimiter == '\n' || options.delimiter == '\0') {
    throw std::invalid_argument("invalid CSV delimiter");
  }
  const auto objects = validate_objects(log);
  const auto events = validate_events(log);
  check_lossy_fields(log, options, has_object_table);

  std::vector<std::string> event_attributes;
  std::unordered_set<std::string> known_attributes;
  for (const auto& event : log.events) {
    std::unordered_set<std::string> seen;
    for (const auto& attr : event.attributes) {
      if (!seen.insert(attr.key).second) {
        throw std::invalid_argument("duplicate event attribute: " + attr.key);
      }
      if (std::holds_alternative<std::nullptr_t>(attr.value)) continue;
      if (known_attributes.insert(attr.key).second) event_attributes.push_back(attr.key);
    }
  }

  // Every relation row is written, a repeated event-object pair as a
  // repeated list entry, as the reference's get_extended_table aggregates
  // the relation rows per event and type into lists.
  std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::string>>> refs;
  std::vector<std::string> types;
  std::unordered_set<std::string> known_types;
  for (const auto& relation : log.relations) {
    const auto event_it = events.find(relation.event_id);
    const auto object_it = objects.by_id.find(relation.object_id);
    if (event_it == events.end() || object_it == objects.by_id.end()) {
      throw std::invalid_argument("OCEL relation references an unknown id");
    }
    const auto& type = object_it->second->type;
    if (known_types.insert(type).second) types.push_back(type);
    refs[relation.event_id][type].push_back(relation.object_id);
  }
  for (const auto& object : log.objects)
    if (known_types.insert(object.type).second) types.push_back(object.type);

  Row header{options.event_id_column, options.timestamp_column,
             options.activity_column};
  for (const auto& key : event_attributes) {
    if (key.empty() || key == options.event_id_column || key == options.activity_column ||
        key == options.timestamp_column ||
        key.rfind(options.object_type_prefix, 0) == 0) {
      throw std::invalid_argument("event attribute collides with an OCEL CSV column: " + key);
    }
    header.push_back(key);
  }
  for (const auto& type : types) {
    const std::string name = options.object_type_prefix + type;
    if (name.empty() || std::find(header.begin(), header.end(), name) != header.end()) {
      throw std::invalid_argument("OCEL CSV object type column collides with a required column");
    }
    header.push_back(name);
  }
  std::set<std::string> header_seen;
  for (const auto& name : header) {
    if (!header_seen.insert(name).second) {
      throw std::invalid_argument("duplicate OCEL CSV output column: " + name);
    }
  }
  write_row(out, header, options.delimiter);

  ocel_frame::Columns frame;
  for (const auto& event : log.events) frame.observe_row(event.attributes);
  std::vector<std::string_view> timestamp_texts;
  timestamp_texts.reserve(log.events.size());
  for (const auto& event : log.events) timestamp_texts.emplace_back(event.timestamp);
  auto timestamps = ocel_frame::pandas_timestamp_column_text(timestamp_texts);
  for (std::size_t index = 0; index < log.events.size(); ++index) {
    const auto& event = log.events[index];
    Row row{event.id, std::move(timestamps[index]), event.activity};
    for (const auto& key : event_attributes) {
      const auto* attr = find_attribute(event.attributes, key);
      row.push_back(attr == nullptr ? std::string{}
                                    : csv_attribute_text(*attr, log.event_type_attributes,
                                                         event.activity, frame.dtype(key)));
    }
    const auto event_it = refs.find(event.id);
    for (const auto& type : types) {
      if (event_it == refs.end()) {
        row.emplace_back();
        continue;
      }
      const auto type_it = event_it->second.find(type);
      row.push_back(type_it == event_it->second.end() ? std::string{}
                                                      : list_text(type_it->second));
    }
    write_row(out, row, options.delimiter);
  }
}

// The object table: id, type and the attributes in first-appearance order,
// rows in log order.
void write_object_table(std::ostream& out, const Ocel& log,
                        const OcelCsvExportOptions& options) {
  if (options.delimiter == '"' || options.delimiter == '\r' ||
      options.delimiter == '\n' || options.delimiter == '\0') {
    throw std::invalid_argument("invalid CSV delimiter");
  }
  (void)validate_objects(log);
  check_lossy_fields(log, options, true);
  std::vector<std::string> attributes;
  std::unordered_set<std::string> known;
  for (const auto& object : log.objects) {
    std::unordered_set<std::string> seen;
    for (const auto& attr : object.attributes) {
      if (!seen.insert(attr.key).second) {
        throw std::invalid_argument("duplicate object attribute: " + attr.key);
      }
      if (std::holds_alternative<std::nullptr_t>(attr.value)) continue;
      if (known.insert(attr.key).second) attributes.push_back(attr.key);
    }
  }
  Row header{options.object_id_column, options.object_type_column};
  for (const auto& key : attributes) {
    if (key.empty() || key == options.object_id_column || key == options.object_type_column) {
      throw std::invalid_argument("object attribute collides with an OCEL CSV column: " + key);
    }
    header.push_back(key);
  }
  write_row(out, header, options.delimiter);
  ocel_frame::Columns frame;
  for (const auto& object : log.objects) frame.observe_row(object.attributes);
  for (const auto& object : log.objects) {
    Row row{object.id, object.type};
    for (const auto& key : attributes) {
      const auto* attr = find_attribute(object.attributes, key);
      row.push_back(attr == nullptr ? std::string{}
                                    : csv_attribute_text(*attr, log.object_type_attributes,
                                                         object.type, frame.dtype(key)));
    }
    write_row(out, row, options.delimiter);
  }
}

Ocel parse_tables(const Table& event_table, const Table* object_table,
                  const OcelCsvImportOptions& options) {
  const auto event_header = read_header(event_table, options, "event");
  const auto event_id = required_column(event_header, options.event_id_column, "event");
  const auto activity = required_column(event_header, options.activity_column, "event");
  const auto timestamp = required_column(event_header, options.timestamp_column, "event");

  std::vector<std::pair<std::string, std::size_t>> object_columns;
  // Columns as pm4py.read_ocel_csv selects them (observed black-box): every
  // prefixed column lists objects of the type its name ends with, except a
  // bare prefix (no type), which lists none and is no attribute either; any
  // other column, an unselected "ocel:" one included (a repeated
  // "ocel:eid.1", "ocel:foo"), is an event attribute.
  std::vector<std::size_t> ignored_columns;
  for (std::size_t i = 0; i < event_header.names.size(); ++i) {
    const auto& name = event_header.names[i];
    if (i == event_id || i == activity || i == timestamp) continue;
    if (name.rfind(options.object_type_prefix, 0) == 0) {
      const std::string type = name.substr(options.object_type_prefix.size());
      if (type.empty()) ignored_columns.push_back(i);
      else object_columns.emplace_back(type, i);
    }
  }
  // Objects in object-table order, then objects only the event table names
  // in first-appearance order.
  std::vector<OcelObject> object_list;
  std::unordered_map<std::string, std::size_t> object_index;
  if (object_table != nullptr) {
    const auto object_header = read_header(*object_table, options, "object");
    const auto object_id = required_column(object_header, options.object_id_column, "object");
    const auto object_type = required_column(object_header, options.object_type_column, "object");
    if (options.reject_unsupported) {
      for (const auto& name : object_header.names) {
        if (name == "ocel:qualifier" || name == "ocel:source" ||
            name == "ocel:target" || name == "ocel:timestamp") {
          throw std::invalid_argument("unsupported reserved OCEL object CSV column: " + name);
        }
      }
    }
    for (std::size_t row_index = 1; row_index < object_table->size(); ++row_index) {
      const auto& row = (*object_table)[row_index];
      // A missing id or type (an empty cell or a short row) is NaN in the
      // reference frame, which drops the row.
      const std::string& id = row[object_id];
      const std::string& type = row[object_type];
      if (id.empty() || type.empty()) continue;
      OcelObject object{id, type, {}};
      for (std::size_t i = 0; i < object_header.names.size(); ++i) {
        if (i == object_id || i == object_type) continue;
        const std::string value = row[i];
        if (!value.empty()) object.attributes.push_back({object_header.names[i], value});
      }
      if (!object_index.emplace(id, object_list.size()).second) {
        throw std::invalid_argument("duplicate object CSV id: " + id +
                                    " (pm4py keeps a row for each listing, which a native OCEL "
                                    "cannot hold)");
      }
      object_list.push_back(std::move(object));
    }
  }
  // Attribute cells stay text, as the reference CSV reader keeps them (it
  // types only the timestamp); empty cells are missing values.
  Ocel log;
  std::unordered_set<std::string> event_ids;
  // Every data row in file order, and whether it became an event.
  std::vector<std::pair<const Row*, bool>> event_rows;
  for (std::size_t row_index = 1; row_index < event_table.size(); ++row_index) {
    const auto& row = event_table[row_index];
    // A missing id or activity (an empty cell or a short row) is NaN in the
    // reference frame, which drops the event; without an object table its
    // objects are still listed, as the reference collects them from every
    // row. A missing timestamp is NaT there, which a native event cannot hold.
    // The reference parses the timestamp column before it drops rows, so a
    // dropped row's unreadable timestamp still refuses the file.
    if (row[event_id].empty() || row[activity].empty()) {
      if (options.sort_events_by_timestamp && !row[timestamp].empty() &&
          !parse_timestamp(row[timestamp])) {
        throw std::invalid_argument("event CSV timestamp is not ISO-8601: " + row[timestamp]);
      }
      event_rows.emplace_back(&row, false);
      continue;
    }
    OcelEvent event{row[event_id], row[activity], row[timestamp], {}};
    if (event.timestamp.empty()) {
      throw std::invalid_argument("event CSV event " + event.id +
                                  " has no timestamp (the reference reads NaT)");
    }
    if (!event_ids.insert(event.id).second) {
      throw std::invalid_argument("duplicate event CSV id: " + event.id);
    }
    if (options.sort_events_by_timestamp) {
      const auto parsed = parse_timestamp(event.timestamp);
      if (!parsed) throw std::invalid_argument("event CSV timestamp is not ISO-8601: " + event.timestamp);
    }
    for (std::size_t i = 0; i < event_header.names.size(); ++i) {
      if (i == event_id || i == activity || i == timestamp ||
          std::any_of(object_columns.begin(), object_columns.end(),
                      [i](const auto& column) { return column.second == i; }) ||
          std::find(ignored_columns.begin(), ignored_columns.end(), i) != ignored_columns.end()) {
        continue;
      }
      const std::string value = row[i];
      if (!value.empty()) event.attributes.push_back({event_header.names[i], value});
    }
    log.events.push_back(std::move(event));
    event_rows.emplace_back(&row, true);
  }
  std::unordered_map<std::string, std::string> object_types;
  for (const auto& object : object_list) object_types.emplace(object.id, object.type);
  // Every object cell of every row (dropped rows too) is read first: pandas
  // types the relations' object ids from all their elements together (see
  // infer_id_dtype), so [1] next to [None] relates "1.0".
  std::vector<std::vector<std::optional<std::vector<ocel_csv_cell::Value>>>> cells;
  cells.reserve(event_rows.size());
  std::vector<const ocel_csv_cell::Value*> elements;
  for (const auto& [row_pointer, kept] : event_rows) {
    (void)kept;
    auto& row_cells = cells.emplace_back();
    for (const auto& column : object_columns)
      row_cells.push_back(ocel_csv_cell::parse_object_cell((*row_pointer)[column.second]));
  }
  for (const auto& row_cells : cells)
    for (const auto& cell : row_cells)
      if (cell)
        for (const auto& value : *cell) elements.push_back(&value);
  const auto dtype = ocel_csv_cell::infer_id_dtype(elements);
  // Without an object table the reference lists one object per value Python
  // compares equal (its first occurrence; the distinct values typed together
  // again), while each relation names the id its own entry got in the
  // relations' column. Where the two disagree (1 and 1.0 in one file, 1 and
  // '1', or [1] next to [None] beside [0] and [-0.0]), the reference relates
  // ids it lists no object for, or lists one id twice, which a native OCEL
  // cannot hold, so such a file is refused.
  std::unordered_map<std::string, std::string> object_text_by_key;
  if (object_table == nullptr) {
    std::unordered_map<std::string, const ocel_csv_cell::Value*> first_by_key;
    std::vector<const ocel_csv_cell::Value*> distinct;
    for (const auto* value : elements) {
      const auto id = ocel_csv_cell::element_id(*value, dtype, false);  // refuses containers
      const auto key = id ? id->raw.key : value->kind == ocel_csv_cell::Value::Kind::kNone
                                              ? std::string("n")
                                              : "s" + value->text;
      if (first_by_key.emplace(key, value).second) distinct.push_back(value);
    }
    const auto object_dtype = ocel_csv_cell::infer_id_dtype(distinct);
    std::unordered_map<std::string, std::string> key_by_object_text;
    for (const auto* value : distinct) {
      const auto id = ocel_csv_cell::element_id(*value, object_dtype, false);
      if (!id) continue;
      object_text_by_key.emplace(id->raw.key, id->relation.text);
      const auto [it, inserted] = key_by_object_text.emplace(id->relation.text, id->raw.key);
      if (!inserted && it->second != id->raw.key)
        throw std::invalid_argument(
            "OCEL CSV object references of different Python types print as one object id (" +
            id->relation.text +
            ") in pm4py.read_ocel_csv, which lists that object twice; this reader does not "
            "support them");
    }
  }
  const auto check_reference = [&](const ocel_csv_cell::ElementId& id) {
    const auto found = object_text_by_key.find(id.raw.key);
    if (found != object_text_by_key.end() && found->second == id.relation.text) return;
    throw std::invalid_argument(
        "OCEL CSV object reference " + id.raw.text + " relates the id " + id.relation.text +
        " in pm4py.read_ocel_csv, which lists its object as " +
        (found == object_text_by_key.end() ? std::string("nothing") : found->second) +
        " (values of different Python types, or pandas typing the ids as numbers); this "
        "reader does not support them");
  };
  std::size_t event_index = 0;
  for (std::size_t row_index = 0; row_index < event_rows.size(); ++row_index) {
    const bool kept = event_rows[row_index].second;
    const OcelEvent* event = kept ? &log.events[event_index++] : nullptr;
    for (std::size_t column = 0; column < object_columns.size(); ++column) {
      const auto& cell = cells[row_index][column];
      if (!cell) continue;
      const auto& type = object_columns[column].first;
      for (const auto& value : *cell) {
        const auto id = ocel_csv_cell::element_id(value, dtype, object_table != nullptr);
        if (!id || (!kept && object_table != nullptr)) continue;
        if (object_table == nullptr) check_reference(*id);
        const auto& object_id = id->relation.text;
        const auto [it, inserted] = object_types.emplace(object_id, type);
        // pm4py.read_ocel_csv lists such an id once per type, each relation
        // keeping its column's type, and its statistics, OC-DFG and
        // flattening count it under both (observed black-box); a native
        // OCEL holds one object per id (declared).
        if (!inserted && it->second != type) {
          throw std::invalid_argument("object id appears under multiple types: " + object_id +
                                      " (" + it->second + " and " + type +
                                      "); pm4py lists it as one object per type, which a native "
                                      "OCEL cannot hold");
        }
        if (object_index.emplace(object_id, object_list.size()).second) {
          object_list.push_back(OcelObject{object_id, type, {}});
        }
        if (event) log.relations.push_back({event->id, object_id, ""});
      }
    }
  }
  if (options.sort_events_by_timestamp) {
    std::stable_sort(log.events.begin(), log.events.end(),
                     [](const OcelEvent& left, const OcelEvent& right) {
                       const auto l = parse_timestamp(left.timestamp);
                       const auto r = parse_timestamp(right.timestamp);
                       return l.value() < r.value();
                     });
  }

  log.objects = std::move(object_list);
  return log;
}

}  // namespace

Ocel parse_ocel_csv(std::string_view event_table,
                    const OcelCsvImportOptions& options) {
  const auto events = parse_table(event_table, options.delimiter, options.trim_fields, "event");
  return parse_tables(events, nullptr, options);
}

Ocel parse_ocel_csv(std::string_view event_table, std::string_view object_table,
                    const OcelCsvImportOptions& options) {
  const auto events = parse_table(event_table, options.delimiter, options.trim_fields, "event");
  const auto objects = parse_table(object_table, options.delimiter, options.trim_fields, "object");
  return parse_tables(events, &objects, options);
}

Ocel import_ocel_csv(std::istream& event_input,
                     const OcelCsvImportOptions& options) {
  std::ostringstream events;
  events << event_input.rdbuf();
  return parse_ocel_csv(events.str(), options);
}

Ocel import_ocel_csv(std::istream& event_input, std::istream& object_input,
                     const OcelCsvImportOptions& options) {
  std::ostringstream events;
  events << event_input.rdbuf();
  std::ostringstream objects;
  objects << object_input.rdbuf();
  return parse_ocel_csv(events.str(), objects.str(), options);
}

Ocel import_ocel_csv(const std::filesystem::path& event_path,
                     const OcelCsvImportOptions& options) {
  std::ifstream input(event_path, std::ios::binary);
  if (!input) throw std::runtime_error("Unable to open OCEL event CSV: " + event_path.string());
  return import_ocel_csv(input, options);
}

Ocel import_ocel_csv(const std::filesystem::path& event_path,
                     const std::filesystem::path& object_path,
                     const OcelCsvImportOptions& options) {
  std::ifstream events(event_path, std::ios::binary);
  if (!events) throw std::runtime_error("Unable to open OCEL event CSV: " + event_path.string());
  std::ifstream objects(object_path, std::ios::binary);
  if (!objects) throw std::runtime_error("Unable to open OCEL object CSV: " + object_path.string());
  return import_ocel_csv(events, objects, options);
}

void write_ocel_csv(std::ostream& out, const Ocel& log,
                    const OcelCsvExportOptions& options) {
  write_event_table(out, log, options, false);
}

void write_ocel_objects_csv(std::ostream& out, const Ocel& log,
                            const OcelCsvExportOptions& options) {
  write_object_table(out, log, options);
}

std::string serialize_ocel_csv(const Ocel& log,
                               const OcelCsvExportOptions& options) {
  std::ostringstream out;
  write_ocel_csv(out, log, options);
  return out.str();
}

std::string serialize_ocel_objects_csv(const Ocel& log,
                                       const OcelCsvExportOptions& options) {
  std::ostringstream out;
  write_ocel_objects_csv(out, log, options);
  return out.str();
}

void export_ocel_csv(const Ocel& log, const std::filesystem::path& event_path,
                     const OcelCsvExportOptions& options) {
  std::ofstream events(event_path, std::ios::binary);
  if (!events) throw std::runtime_error("Unable to create OCEL event CSV: " + event_path.string());
  write_event_table(events, log, options, false);
}

void export_ocel_csv(const Ocel& log, const std::filesystem::path& event_path,
                     const std::filesystem::path& object_path,
                     const OcelCsvExportOptions& options) {
  std::ofstream events(event_path, std::ios::binary);
  if (!events) throw std::runtime_error("Unable to create OCEL event CSV: " + event_path.string());
  // When the object table is suppressed, object attributes have no lossless
  // representation.  Let the normal lossiness validation reject that case
  // instead of silently discarding them from a two-file export.
  write_event_table(events, log, options, options.emit_objects);
  if (options.emit_objects) {
    std::ofstream objects(object_path, std::ios::binary);
    if (!objects) throw std::runtime_error("Unable to create OCEL object CSV: " + object_path.string());
    write_object_table(objects, log, options);
  }
}

}  // namespace minera
