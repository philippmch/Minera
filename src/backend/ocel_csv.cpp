#include "minera/ocel_csv.hpp"

#include "minera/timestamp.hpp"

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

bool blank_row(const Row& row) {
  return std::all_of(row.begin(), row.end(), [](const std::string& cell) {
    return cell.empty();
  });
}

Table parse_table(std::string_view input, char delimiter, bool trim_fields) {
  if (delimiter == '"' || delimiter == '\r' || delimiter == '\n' ||
      delimiter == '\0') {
    throw std::invalid_argument("invalid CSV delimiter");
  }
  if (input.size() >= 3 && static_cast<unsigned char>(input[0]) == 0xef &&
      static_cast<unsigned char>(input[1]) == 0xbb &&
      static_cast<unsigned char>(input[2]) == 0xbf) {
    input.remove_prefix(3);
  }

  Table rows;
  Row row;
  std::string field;
  bool quoted = false;
  bool quote_closed = false;
  bool saw_any = false;
  const auto finish_field = [&]() {
    row.push_back(trim_fields ? trim(std::move(field)) : std::move(field));
    field.clear();
    quote_closed = false;
  };
  const auto finish_row = [&]() {
    finish_field();
    rows.push_back(std::move(row));
    row = Row{};
    saw_any = false;
  };

  for (std::size_t i = 0; i < input.size(); ++i) {
    const char ch = input[i];
    if (quoted) {
      if (ch == '"') {
        if (i + 1 < input.size() && input[i + 1] == '"') {
          field.push_back('"');
          ++i;
        } else {
          quoted = false;
          quote_closed = true;
        }
      } else {
        field.push_back(ch);
      }
      saw_any = true;
      continue;
    }

    if (ch == '"') {
      if (!field.empty() || quote_closed) {
        throw std::invalid_argument("CSV quote must start a field");
      }
      quoted = true;
      saw_any = true;
    } else if (ch == delimiter) {
      finish_field();
      saw_any = true;
    } else if (ch == '\n' || ch == '\r') {
      if (ch == '\r' && i + 1 < input.size() && input[i + 1] == '\n') ++i;
      finish_row();
    } else {
      if (quote_closed && ch != ' ' && ch != '\t') {
        throw std::invalid_argument("CSV data follows a closed quoted field");
      }
      field.push_back(ch);
      saw_any = true;
    }
  }
  if (quoted) throw std::invalid_argument("CSV contains an unterminated quote");
  if (saw_any || !field.empty() || !row.empty()) finish_row();
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
  Header header{table.front(), {}};
  for (std::size_t i = 0; i < header.names.size(); ++i) {
    if (header.names[i].empty()) {
      throw std::invalid_argument(std::string(label) + " CSV has an empty header");
    }
    if (!header.indexes.emplace(header.names[i], i).second) {
      throw std::invalid_argument(std::string(label) +
                                  " CSV has duplicate column: " + header.names[i]);
    }
  }
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

std::string cell(const Row& row, std::size_t index, std::size_t line,
                 const char* label) {
  if (row.size() <= index) {
    throw std::invalid_argument(std::string(label) + " CSV row " +
                                std::to_string(line) + " has too few fields");
  }
  return row[index];
}

void validate_row_width(const Row& row, std::size_t width, std::size_t line,
                        const char* label) {
  if (row.size() != width) {
    throw std::invalid_argument(std::string(label) + " CSV row " +
                                std::to_string(line) + " has " +
                                std::to_string(row.size()) + " fields; expected " +
                                std::to_string(width));
  }
}

OcelValue infer_value(std::string_view text) {
  if (text == "true" || text == "TRUE" || text == "True") return true;
  if (text == "false" || text == "FALSE" || text == "False") return false;

  std::int64_t integer = 0;
  const auto integer_result = std::from_chars(
      text.data(), text.data() + text.size(), integer);
  if (integer_result.ec == std::errc() &&
      integer_result.ptr == text.data() + text.size()) {
    return integer;
  }

  std::string copy(text);
  char* end = nullptr;
  const double real = std::strtod(copy.c_str(), &end);
  if (end == copy.c_str() + copy.size() && std::isfinite(real)) return real;
  return copy;
}

std::string unescape_list_string(std::string_view text, std::size_t& pos,
                                 char quote) {
  std::string value;
  ++pos;
  while (pos < text.size()) {
    const char ch = text[pos++];
    if (ch == quote) return value;
    if (ch != '\\') {
      value.push_back(ch);
      continue;
    }
    if (pos >= text.size()) throw std::invalid_argument("unterminated object id escape");
    const char escaped = text[pos++];
    switch (escaped) {
      case 'n': value.push_back('\n'); break;
      case 'r': value.push_back('\r'); break;
      case 't': value.push_back('\t'); break;
      case '\\': value.push_back('\\'); break;
      case '\'': value.push_back('\''); break;
      case '"': value.push_back('"'); break;
      default: value.push_back(escaped); break;
    }
  }
  throw std::invalid_argument("unterminated quoted object id");
}

std::vector<std::string> parse_object_list(std::string_view text) {
  const std::string owned = trim(std::string(text));
  text = owned;
  if (text.empty()) return {};
  if (text.front() != '[' || text.back() != ']') {
    throw std::invalid_argument("object reference cell must be a bracketed list");
  }

  std::vector<std::string> values;
  std::unordered_set<std::string> seen;
  std::size_t pos = 1;
  bool expect_value = true;
  while (true) {
    while (pos < text.size() &&
           (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\r' ||
            text[pos] == '\n')) {
      ++pos;
    }
    if (pos >= text.size()) throw std::invalid_argument("malformed object reference list");
    if (text[pos] == ']') {
      if (expect_value && !values.empty()) {
        throw std::invalid_argument("object reference list has a trailing comma");
      }
      ++pos;
      break;
    }

    std::string value;
    if (text[pos] == '\'' || text[pos] == '"') {
      value = unescape_list_string(text, pos, text[pos]);
    } else {
      const std::size_t begin = pos;
      while (pos < text.size() && text[pos] != ',' && text[pos] != ']') ++pos;
      value = trim(std::string(text.substr(begin, pos - begin)));
      if (value.empty()) throw std::invalid_argument("empty object id in list");
    }
    if (value.empty()) throw std::invalid_argument("empty object id in list");
    if (!seen.insert(value).second) {
      throw std::invalid_argument("duplicate object id in one event row: " + value);
    }
    values.push_back(std::move(value));
    expect_value = false;

    while (pos < text.size() &&
           (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\r' ||
            text[pos] == '\n')) {
      ++pos;
    }
    if (pos >= text.size()) throw std::invalid_argument("malformed object reference list");
    if (text[pos] == ']') {
      ++pos;
      break;
    }
    if (text[pos] != ',') {
      throw std::invalid_argument("object reference list expects a comma");
    }
    ++pos;
    expect_value = true;
  }
  while (pos < text.size() &&
         (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\r' ||
          text[pos] == '\n')) {
    ++pos;
  }
  if (pos != text.size()) throw std::invalid_argument("trailing object list data");
  return values;
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

std::string value_text(const OcelValue& value) {
  if (std::holds_alternative<std::nullptr_t>(value)) return {};
  if (const auto* boolean = std::get_if<bool>(&value)) return *boolean ? "true" : "false";
  if (const auto* integer = std::get_if<std::int64_t>(&value)) return std::to_string(*integer);
  if (const auto* real = std::get_if<double>(&value)) {
    if (!std::isfinite(*real)) throw std::invalid_argument("OCEL CSV cannot encode non-finite values");
    std::ostringstream out;
    out << std::setprecision(std::numeric_limits<double>::max_digits10) << *real;
    return out.str();
  }
  return std::get<std::string>(value);
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

std::string list_text(const std::vector<std::string>& ids) {
  std::ostringstream out;
  out << '[';
  for (std::size_t i = 0; i < ids.size(); ++i) {
    if (i != 0) out << ',';
    out << '"';
    for (const char ch : ids[i]) {
      switch (ch) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default: out << ch; break;
      }
    }
    out << '"';
  }
  out << ']';
  return out.str();
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

  std::set<std::string> event_attributes;
  for (const auto& [id, event] : events) {
    std::set<std::string> seen;
    for (const auto& attr : event->attributes) {
      if (!seen.insert(attr.key).second) {
        throw std::invalid_argument("duplicate event attribute: " + attr.key);
      }
      event_attributes.insert(attr.key);
    }
  }

  Row header{options.event_id_column, options.activity_column,
             options.timestamp_column};
  for (const auto& type : objects.types) {
    const std::string name = options.object_type_prefix + type;
    if (name.empty() || std::find(header.begin(), header.end(), name) != header.end()) {
      throw std::invalid_argument("OCEL CSV object type column collides with a required column");
    }
    header.push_back(name);
  }
  for (const auto& key : event_attributes) {
    if (key.empty() || key == options.event_id_column || key == options.activity_column ||
        key == options.timestamp_column ||
        key.rfind(options.object_type_prefix, 0) == 0) {
      throw std::invalid_argument("event attribute collides with an OCEL CSV column: " + key);
    }
    header.push_back(key);
  }
  std::set<std::string> header_seen;
  for (const auto& name : header) {
    if (!header_seen.insert(name).second) {
      throw std::invalid_argument("duplicate OCEL CSV output column: " + name);
    }
  }
  write_row(out, header, options.delimiter);

  std::map<std::string, std::map<std::string, std::vector<std::string>>> refs;
  std::set<std::pair<std::string, std::string>> seen_relations;
  for (const auto& relation : log.relations) {
    const auto event_it = events.find(relation.event_id);
    const auto object_it = objects.by_id.find(relation.object_id);
    if (event_it == events.end() || object_it == objects.by_id.end()) {
      throw std::invalid_argument("OCEL relation references an unknown id");
    }
    if (!seen_relations.emplace(relation.event_id, relation.object_id).second) {
      throw std::invalid_argument("duplicate OCEL event-object relation");
    }
    refs[relation.event_id][object_it->second->type].push_back(relation.object_id);
  }
  for (auto& [event_id, by_type] : refs) {
    (void)event_id;
    for (auto& [type, ids] : by_type) {
      (void)type;
      std::sort(ids.begin(), ids.end());
    }
  }

  for (const auto& [id, event] : events) {
    Row row{id, event->activity, event->timestamp};
    for (const auto& type : objects.types) {
      const auto event_it = refs.find(id);
      if (event_it == refs.end()) {
        row.emplace_back("[]");
      } else {
        const auto type_it = event_it->second.find(type);
        row.push_back(type_it == event_it->second.end()
                          ? "[]"
                          : list_text(type_it->second));
      }
    }
    for (const auto& key : event_attributes) {
      const auto* attr = find_attribute(event->attributes, key);
      row.push_back(attr == nullptr ? std::string{} : value_text(attr->value));
    }
    write_row(out, row, options.delimiter);
  }
}

void write_object_table(std::ostream& out, const Ocel& log,
                        const OcelCsvExportOptions& options) {
  if (options.delimiter == '"' || options.delimiter == '\r' ||
      options.delimiter == '\n' || options.delimiter == '\0') {
    throw std::invalid_argument("invalid CSV delimiter");
  }
  const auto objects = validate_objects(log);
  check_lossy_fields(log, options, true);
  std::set<std::string> attributes;
  for (const auto& [id, object] : objects.by_id) {
    (void)id;
    std::set<std::string> seen;
    for (const auto& attr : object->attributes) {
      if (!seen.insert(attr.key).second) {
        throw std::invalid_argument("duplicate object attribute: " + attr.key);
      }
      attributes.insert(attr.key);
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
  for (const auto& [id, object] : objects.by_id) {
    Row row{id, object->type};
    for (const auto& key : attributes) {
      const auto* attr = find_attribute(object->attributes, key);
      row.push_back(attr == nullptr ? std::string{} : value_text(attr->value));
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
  std::set<std::string> reserved{options.event_id_column, options.activity_column,
                                 options.timestamp_column};
  for (std::size_t i = 0; i < event_header.names.size(); ++i) {
    const auto& name = event_header.names[i];
    if (name.rfind(options.object_type_prefix, 0) == 0) {
      const std::string type = name.substr(options.object_type_prefix.size());
      if (type.empty()) throw std::invalid_argument("event object column has an empty type");
      object_columns.emplace_back(type, i);
      reserved.insert(name);
    } else if (reserved.count(name) == 0 && name.rfind("ocel:", 0) == 0) {
      throw std::invalid_argument("unknown reserved OCEL CSV column: " + name);
    }
  }
  std::map<std::string, OcelObject> object_map;
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
      if (blank_row(row)) continue;
      validate_row_width(row, object_header.names.size(), row_index + 1, "object");
      const std::string id = cell(row, object_id, row_index + 1, "object");
      const std::string type = cell(row, object_type, row_index + 1, "object");
      if (id.empty() || type.empty()) throw std::invalid_argument("object CSV requires non-empty id and type");
      OcelObject object{id, type, {}};
      for (std::size_t i = 0; i < object_header.names.size(); ++i) {
        if (i == object_id || i == object_type) continue;
        const std::string value = row[i];
        if (!value.empty()) object.attributes.push_back({object_header.names[i], infer_value(value)});
      }
      if (!object_map.emplace(id, std::move(object)).second) {
        throw std::invalid_argument("duplicate object CSV id: " + id);
      }
    }
  }
  Ocel log;
  std::unordered_set<std::string> event_ids;
  std::vector<const Row*> event_rows;
  for (std::size_t row_index = 1; row_index < event_table.size(); ++row_index) {
    const auto& row = event_table[row_index];
    if (blank_row(row)) continue;
    validate_row_width(row, event_header.names.size(), row_index + 1, "event");
    OcelEvent event{cell(row, event_id, row_index + 1, "event"),
                    cell(row, activity, row_index + 1, "event"),
                    cell(row, timestamp, row_index + 1, "event"), {}};
    if (event.id.empty() || event.activity.empty() || event.timestamp.empty()) {
      throw std::invalid_argument("event CSV requires non-empty id, activity, and timestamp");
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
                      [i](const auto& column) { return column.second == i; })) {
        continue;
      }
      const std::string value = row[i];
      if (!value.empty()) event.attributes.push_back({event_header.names[i], infer_value(value)});
    }
    log.events.push_back(std::move(event));
    event_rows.push_back(&row);
  }
  std::map<std::string, std::string> object_types;
  for (const auto& [id, object] : object_map) object_types.emplace(id, object.type);
  for (std::size_t event_index = 0; event_index < log.events.size(); ++event_index) {
    const auto& row = *event_rows[event_index];
    const auto& event = log.events[event_index];
    for (const auto& [type, index] : object_columns) {
      for (const auto& object_id : parse_object_list(row[index])) {
        const auto [it, inserted] = object_types.emplace(object_id, type);
        if (!inserted && it->second != type) {
          throw std::invalid_argument("object id appears under multiple types: " + object_id);
        }
        if (object_map.find(object_id) == object_map.end()) {
          object_map.emplace(object_id, OcelObject{object_id, type, {}});
        }
        log.relations.push_back({event.id, object_id, ""});
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

  for (const auto& [id, object] : object_map) log.objects.push_back(object);
  return log;
}

}  // namespace

Ocel parse_ocel_csv(std::string_view event_table,
                    const OcelCsvImportOptions& options) {
  const auto events = parse_table(event_table, options.delimiter, options.trim_fields);
  return parse_tables(events, nullptr, options);
}

Ocel parse_ocel_csv(std::string_view event_table, std::string_view object_table,
                    const OcelCsvImportOptions& options) {
  const auto events = parse_table(event_table, options.delimiter, options.trim_fields);
  const auto objects = parse_table(object_table, options.delimiter, options.trim_fields);
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
  write_event_table(events, log, options, true);
  if (options.emit_objects) {
    std::ofstream objects(object_path, std::ios::binary);
    if (!objects) throw std::runtime_error("Unable to create OCEL object CSV: " + object_path.string());
    write_object_table(objects, log, options);
  }
}

}  // namespace minera
