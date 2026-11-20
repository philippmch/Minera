#include "minera/ocel_sqlite.hpp"

#include "ocel_frame_types.hpp"
#include "ocel_reader_index.hpp"
#include "pandas_to_datetime.hpp"
#include "sqlite_real_text.hpp"
#include "minera/timestamp.hpp"

#include <algorithm>
#include <numeric>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#if defined(MINERA_HAS_SQLITE)
#if defined(_WIN32)
#include <winsqlite/winsqlite3.h>
#else
#include <sqlite3.h>
#endif
#include "sqlite_untrusted.hpp"
#endif

namespace minera {
// Defined in ocel.cpp (round EL): the change frame's time column as pm4py's
// read_ocel2_sqlite reads it; round EM: `column` its concatenated dtype.
void sort_ocel2_sqlite_changes(Ocel& log, const std::vector<OcelSqliteTime>& times,
                               OcelSqliteTimeColumn column);
namespace {

#if defined(MINERA_HAS_SQLITE)

class Statement {
 public:
  Statement(sqlite3* db, std::string_view sql) : db_(db) {
    const auto text = std::string(sql);
    const auto status = sqlite3_prepare_v2(db_, text.c_str(), -1, &statement_,
                                           nullptr);
    if (status != SQLITE_OK) {
      throw std::runtime_error("SQLite prepare failed: " +
                               std::string(sqlite3_errmsg(db_)));
    }
  }

  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;

  ~Statement() {
    if (statement_ != nullptr) sqlite3_finalize(statement_);
  }

  sqlite3_stmt* get() const { return statement_; }

 private:
  sqlite3* db_ = nullptr;
  sqlite3_stmt* statement_ = nullptr;
};

class Database {
 public:
  Database(const std::filesystem::path& path, int flags) {
    // SQLite takes UTF-8 names on every platform; path.string() is the ANSI
    // code page on Windows and garbles (or throws on) a non-ASCII temp dir.
    const auto name = path.u8string();
    const auto status =
        sqlite3_open_v2(reinterpret_cast<const char*>(name.c_str()), &db_, flags, nullptr);
    if (status != SQLITE_OK) {
      const std::string message = db_ == nullptr
                                      ? "unable to open database"
                                      : sqlite3_errmsg(db_);
      if (db_ != nullptr) sqlite3_close(db_);
      db_ = nullptr;
      throw std::runtime_error("SQLite open failed: " + message);
    }
    // An imported database is untrusted; an export writes Minera's own schema.
    if ((flags & SQLITE_OPEN_READONLY) != 0) detail::harden_untrusted_sqlite(db_);
    sqlite3_busy_timeout(db_, 5000);
  }

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

  ~Database() {
    if (db_ != nullptr) sqlite3_close(db_);
  }

  sqlite3* get() const { return db_; }

  void exec(std::string_view sql) {
    char* error = nullptr;
    const auto text = std::string(sql);
    const auto status = sqlite3_exec(db_, text.c_str(), nullptr, nullptr,
                                     &error);
    if (status != SQLITE_OK) {
      const std::string message = error == nullptr ? sqlite3_errmsg(db_) : error;
      if (error != nullptr) sqlite3_free(error);
      // Round ER: as pandas' to_sql raises sqlite3's error in
      // write_ocel2_sqlite (a duplicate column name, observed black-box).
      throw std::runtime_error("OperationalError: " + message);
    }
  }

 private:
  sqlite3* db_ = nullptr;
};

void bind_text(sqlite3_stmt* statement, int index, const std::string& value) {
  if (sqlite3_bind_text(statement, index, value.c_str(), -1,
                        SQLITE_TRANSIENT) != SQLITE_OK) {
    throw std::runtime_error("SQLite text bind failed");
  }
}

void bind_integer(sqlite3_stmt* statement, int index, std::int64_t value) {
  if (sqlite3_bind_int64(statement, index, value) != SQLITE_OK) {
    throw std::runtime_error("SQLite integer bind failed");
  }
}

void bind_double(sqlite3_stmt* statement, int index, double value) {
  if (sqlite3_bind_double(statement, index, value) != SQLITE_OK) {
    throw std::runtime_error("SQLite real bind failed");
  }
}

void bind_null(sqlite3_stmt* statement, int index) {
  if (sqlite3_bind_null(statement, index) != SQLITE_OK) {
    throw std::runtime_error("SQLite null bind failed");
  }
}

// Round EW: write_ocel2_sqlite declares a qualifier column as pandas'
// to_sql infers it from the values (observed black-box): INTEGER when every
// qualifier is an int, REAL when every one is a float, TEXT otherwise (a str,
// or ints next to floats, written as their str). A table without rows keeps
// the declaration below (declared: the reference's dtype of an emptied
// column is not carried).
template <class Relation>
const char* qualifier_column_type(const std::vector<Relation>& rows) {
  if (rows.empty()) return "TEXT";
  const auto kind = rows.front().qualifier_kind;
  if (kind == OcelQualifierKind::kStr) return "TEXT";
  for (const auto& row : rows)
    if (row.qualifier_kind != kind) return "TEXT";
  // Round EX: bools only (read_ocel2_json, read_ocel_json) are INTEGER too
  // (probe ex p/wj.py, wj2.py).
  return kind == OcelQualifierKind::kFloat ? "REAL" : "INTEGER";
}

void step_done(sqlite3* db, sqlite3_stmt* statement) {
  const auto status = sqlite3_step(statement);
  if (status != SQLITE_DONE) {
    throw std::runtime_error("SQLite step failed: " +
                             std::string(sqlite3_errmsg(db)));
  }
  sqlite3_reset(statement);
  sqlite3_clear_bindings(statement);
}

void bind_qualifier(sqlite3_stmt* statement, int index, const std::string& text,
                    OcelQualifierKind kind, std::string_view column) {
  // Round EX: a bool binds as the integer 1 or 0, which a TEXT column
  // stores as '1' or '0' (observed black-box, probe ex p/wj.py).
  if (kind == OcelQualifierKind::kBool) {
    if (column == "INTEGER") return bind_integer(statement, index, text == "True" ? 1 : 0);
    return bind_text(statement, index, text == "True" ? "1" : "0");
  }
  if (column == "INTEGER" && kind == OcelQualifierKind::kInt) {
    errno = 0;
    char* end = nullptr;
    const long long value = std::strtoll(text.c_str(), &end, 10);
    if (errno == 0 && end != text.c_str() && *end == '\0') return bind_integer(statement, index, value);
  } else if (column == "REAL" && kind == OcelQualifierKind::kFloat) {
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end != text.c_str() && *end == '\0') return bind_double(statement, index, value);
  } else if (kind == OcelQualifierKind::kFloat) {
    // A float in a TEXT column is SQLite's text of the REAL pandas binds
    // ("1.0e+20", "9.22337203685478e+18"; observed black-box), spelled as
    // SQLite 3.45 does on every platform (see sqlite_legacy_real_text).
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end != text.c_str() && *end == '\0') return bind_text(statement, index, sqlite_legacy_real_text(value));
  }
  bind_text(statement, index, text);
}

// Round EW: one relation table's qualifier cell as SQLite stores it.
struct QualifierCell {
  int type = SQLITE_NULL;
  std::int64_t integer = 0;
  double real = 0;
};

QualifierCell qualifier_cell(sqlite3_stmt* statement, int column) {
  QualifierCell cell;
  if (column < 0) return cell;
  cell.type = sqlite3_column_type(statement, column);
  if (cell.type == SQLITE_INTEGER) cell.integer = sqlite3_column_int64(statement, column);
  if (cell.type == SQLITE_FLOAT) cell.real = sqlite3_column_double(statement, column);
  return cell;
}

std::string column_string(sqlite3_stmt* statement, int column) {
  // A REAL as the "%!.15g" text that SQLite 3.45 gives it, formatted here:
  // newer libraries (Windows' winsqlite3) spell a value that does not
  // round-trip in 15 digits with 17, through sqlite3_snprintf too, so a kept
  // time's text moved with the platform (rounds EC and ED, the Windows CI
  // runs of rounds EB and EC). See sqlite_legacy_real_text.
  if (sqlite3_column_type(statement, column) == SQLITE_FLOAT)
    return sqlite_legacy_real_text(sqlite3_column_double(statement, column));
  const auto* value = sqlite3_column_text(statement, column);
  return value == nullptr
             ? std::string{}
             : std::string(reinterpret_cast<const char*>(value));
}

// Round EW: read_sql types a relation table's qualifier column as pandas
// types its cells: a column of ints is int64, ints next to floats or to
// NULLs are float64 (the ints become floats), and a column with a text
// keeps each number as an int or float; the NULLs then read as empty texts
// (observed black-box, probe ew q/wq.py). The qualifier is then the
// number's Python repr (OcelQualifierKind). An infinite REAL keeps its text.
template <class Relation>
void type_sqlite_qualifiers(std::vector<Relation>& rows, std::size_t first,
                            const std::vector<QualifierCell>& cells) {
  bool text = false, real = false, integer = false, null = false;
  for (const auto& cell : cells) {
    if (cell.type == SQLITE_TEXT || cell.type == SQLITE_BLOB) text = true;
    else if (cell.type == SQLITE_FLOAT) real = true;
    else if (cell.type == SQLITE_INTEGER) integer = true;
    else null = true;
  }
  if (!real && !integer) return;
  const bool float_column = !text && (real || null);
  for (std::size_t i = 0; i < cells.size() && first + i < rows.size(); ++i) {
    auto& row = rows[first + i];
    const auto& cell = cells[i];
    if (cell.type == SQLITE_INTEGER) {
      if (float_column) {
        row.qualifier = ocel_frame::float_repr(static_cast<double>(cell.integer));
        row.qualifier_kind = OcelQualifierKind::kFloat;
      } else {
        row.qualifier = std::to_string(cell.integer);
        row.qualifier_kind = OcelQualifierKind::kInt;
      }
    } else if (cell.type == SQLITE_FLOAT && std::isfinite(cell.real)) {
      row.qualifier = ocel_frame::float_repr(cell.real);
      row.qualifier_kind = OcelQualifierKind::kFloat;
    }
  }
}

std::string quote_identifier(std::string_view identifier) {
  std::string result = "\"";
  for (const char character : identifier) {
    if (character == '"') result += "\"\"";
    else result.push_back(character);
  }
  result.push_back('"');
  return result;
}

// Creates `table` with the columns `bound` (name, SQL type) in `order`
// (their names; empty: bound's order) and returns the INSERT that binds
// them in bound's order.
std::string create_table_sql(Database& database, const std::string& table,
                             const std::vector<std::pair<std::string, std::string>>& bound,
                             const std::vector<std::string>& order) {
  std::string schema = "CREATE TABLE " + quote_identifier(table) + " (";
  std::string names, values;
  for (std::size_t index = 0; index < bound.size(); ++index) {
    if (index != 0) {
      names += ", ";
      values += ",";
    }
    names += quote_identifier(bound[index].first);
    values += "?" + std::to_string(index + 1);
  }
  if (order.empty()) {
    for (std::size_t index = 0; index < bound.size(); ++index)
      schema += (index != 0 ? ", " : "") + quote_identifier(bound[index].first) + " " + bound[index].second;
  } else {
    if (order.size() != bound.size()) throw std::logic_error("SQLite table order does not cover its columns");
    for (std::size_t index = 0; index < order.size(); ++index) {
      const auto found = std::find_if(bound.begin(), bound.end(),
                                      [&](const auto& column) { return column.first == order[index]; });
      if (found == bound.end()) throw std::logic_error("SQLite table order names an unknown column");
      schema += (index != 0 ? ", " : "") + quote_identifier(found->first) + " " + found->second;
    }
  }
  database.exec(schema + ");");
  return "INSERT INTO " + quote_identifier(table) + " (" + names + ") VALUES(" + values + ")";
}

bool table_exists(sqlite3* db, std::string_view table) {
  Statement statement(
      // SQLite resolves identifiers without regard to ASCII case (round EK).
      // Round ER: a view reads like a table ("SELECT * FROM <view>" in the
      // reference; observed black-box, probe er hb1.py).
      db, "SELECT 1 FROM sqlite_master WHERE type IN ('table', 'view') AND name=?1 COLLATE NOCASE LIMIT 1");
  bind_text(statement.get(), 1, std::string(table));
  const auto status = sqlite3_step(statement.get());
  sqlite3_reset(statement.get());
  sqlite3_clear_bindings(statement.get());
  if (status == SQLITE_ROW) return true;
  if (status == SQLITE_DONE) return false;
  throw std::runtime_error("SQLite table lookup failed: " +
                           std::string(sqlite3_errmsg(db)));
}

std::string hex_name(std::string_view value) {
  static constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(value.size() * 2);
  for (const unsigned char character : value) {
    result.push_back(digits[character >> 4]);
    result.push_back(digits[character & 0x0f]);
  }
  return result;
}

std::string type_table_token(std::string_view value,
                             const OcelSqliteOptions& options) {
  std::string result = "t_";
  if (options.readable_type_table_names) {
    for (const unsigned char character : value) {
      if (std::isalnum(character) || character == '_') {
        result.push_back(static_cast<char>(character));
      } else {
        result.push_back('_');
      }
    }
    if (result.size() == 2) result += "empty";
  }
  result += "_";
  result += hex_name(value);
  return result;
}

void validate_log(const Ocel& log, bool validate_references) {
  std::set<std::string> event_ids;
  std::set<std::string> object_ids;
  for (const auto& event : log.events) {
    if (event.id.empty() || !event_ids.insert(event.id).second)
      throw std::invalid_argument("OCEL SQLite export requires unique event ids");
  }
  for (const auto& object : log.objects) {
    if (object.id.empty() || !object_ids.insert(object.id).second)
      throw std::invalid_argument("OCEL SQLite export requires unique object ids");
  }
  if (!validate_references) return;
  for (const auto& relation : log.relations) {
    if (!event_ids.contains(relation.event_id) ||
        !object_ids.contains(relation.object_id))
      throw std::invalid_argument("OCEL SQLite event-object relation references an unknown id");
  }
  for (const auto& relation : log.o2o) {
    if (!object_ids.contains(relation.source_id) ||
        !object_ids.contains(relation.target_id))
      throw std::invalid_argument("OCEL SQLite object-object relation references an unknown id");
  }
  for (const auto& relation : log.e2e) {
    if (!event_ids.contains(relation.source_id) ||
        !event_ids.contains(relation.target_id))
      throw std::invalid_argument("OCEL SQLite event-event relation references an unknown id");
  }
  for (const auto& change : log.object_changes) {
    if (!object_ids.contains(change.object_id))
      throw std::invalid_argument("OCEL SQLite object change references an unknown id");
  }
}

void bind_value(sqlite3_stmt* statement, int index, const OcelValue& value,
                std::string& type) {
  if (std::holds_alternative<std::nullptr_t>(value)) {
    type = "null";
    bind_null(statement, index + 1);
    bind_null(statement, index + 2);
    bind_null(statement, index + 3);
    bind_null(statement, index + 4);
  } else if (const auto* boolean = std::get_if<bool>(&value)) {
    type = "bool";
    bind_null(statement, index + 1);
    bind_null(statement, index + 2);
    bind_null(statement, index + 3);
    bind_integer(statement, index + 4, *boolean ? 1 : 0);
  } else if (const auto* integer = std::get_if<std::int64_t>(&value)) {
    type = "int";
    bind_null(statement, index + 1);
    bind_integer(statement, index + 2, *integer);
    bind_null(statement, index + 3);
    bind_null(statement, index + 4);
  } else if (const auto* real = std::get_if<double>(&value)) {
    type = "double";
    bind_null(statement, index + 1);
    bind_null(statement, index + 2);
    bind_double(statement, index + 3, *real);
    bind_null(statement, index + 4);
  } else {
    type = "string";
    bind_text(statement, index + 1, std::get<std::string>(value));
    bind_null(statement, index + 2);
    bind_null(statement, index + 3);
    bind_null(statement, index + 4);
    bind_null(statement, index + 5);
  }
}

void insert_attribute(sqlite3* db, sqlite3_stmt* statement,
                      const std::string& id, const OcelAttribute& attribute,
                      std::size_t ordinal) {
  bind_text(statement, 1, id);
  bind_text(statement, 2, attribute.key);
  std::string type;
  bind_value(statement, 2, attribute.value, type);
  bind_text(statement, 8, type);
  bind_integer(statement, 9, static_cast<std::int64_t>(ordinal));
  step_done(db, statement);
}

void bind_scalar(sqlite3_stmt* statement, int index, const OcelValue& value) {
  if (std::holds_alternative<std::nullptr_t>(value)) {
    bind_null(statement, index);
  } else if (const auto* boolean = std::get_if<bool>(&value)) {
    bind_integer(statement, index, *boolean ? 1 : 0);
  } else if (const auto* integer = std::get_if<std::int64_t>(&value)) {
    bind_integer(statement, index, *integer);
  } else if (const auto* real = std::get_if<double>(&value)) {
    bind_double(statement, index, *real);
  } else {
    bind_text(statement, index, std::get<std::string>(value));
  }
}

bool reserved_type_column(std::string_view key) {
  return key.empty() || key == "ocel_id" || key == "ocel_time" ||
         key == "ocel_changed_field" || key == "@@cumcount";
}

// Round ES: write_ocel2_sqlite writes the change rows of any field as rows of
// their type table, "@@cumcount", "ocel:activity", ocel_id, ocel_time and
// the empty field included (observed black-box, probes es x4, x7), and
// raises on a change of "ocel_changed_field", ocel_id or ocel_time with a
// value (see reference_type_table_error).

// A cell pandas keeps as a value (dropna drops None and NaN).
bool frame_value(const OcelValue& value) {
  if (std::holds_alternative<std::nullptr_t>(value)) return false;
  const auto* real = std::get_if<double>(&value);
  return real == nullptr || !std::isnan(*real);
}

bool has_frame_value(const std::vector<OcelAttribute>& cells, std::string_view key) {
  return std::any_of(cells.begin(), cells.end(),
                     [&](const OcelAttribute& cell) { return cell.key == key && frame_value(cell.value); });
}

constexpr const char* kAssembleDuplicateKeys = "ValueError: cannot assemble with duplicate keys";
constexpr const char* kFrameHasNoDtype = "AttributeError: 'DataFrame' object has no attribute 'dtype'";
constexpr const char* kReindexNotUnique = "InvalidIndexError: Reindexing only valid with uniquely valued Index objects";

using TypeColumns = std::vector<std::pair<std::string, std::string>>;

std::string declared_sql_type(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](const unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  // Round ER: a BOOLEAN column holds what an INTEGER one holds (its cells
  // read as stored, see read_sqlite_cell).
  if (value.find("bool") != std::string::npos) return "INTEGER";
  if (value.find("int") != std::string::npos ||
      value.find("long") != std::string::npos)
    return "INTEGER";
  if (value.find("float") != std::string::npos ||
      value.find("double") != std::string::npos ||
      value == "real" || value == "number")
    return "REAL";
  return "TEXT";
}

std::map<std::string, const OcelValue*> attribute_values(
    const std::vector<OcelAttribute>& attributes) {
  std::map<std::string, const OcelValue*> result;
  for (const auto& attribute : attributes) {
    if (!attribute.key.empty()) result[attribute.key] = &attribute.value;
  }
  return result;
}

// A TIMESTAMP column (an attribute the catalog declares a date) holds each
// timestamp text as pandas prints the reference's datetime64[us, UTC] value;
// nanosecond digits are cut (the reference writes Python datetimes, round BW).
// Round EK: a datetime of an object column (dates next to other values, a
// TEXT column) is its isoformat, as the reference writes an object column
// (observed black-box); `catalog` and `owner` tell the date values.
void bind_type_value(sqlite3_stmt* statement, int position, const std::string& name,
                     const std::string& sql_type, const OcelValue* value,
                     const OcelTypeCatalog& catalog, const std::string& owner) {
  if (value == nullptr) bind_null(statement, position);
  else if (const auto* text = sql_type == "TIMESTAMP" ? ocel_frame::date_value_text(*value) : nullptr)
    bind_text(statement, position, ocel_frame::utc_timestamp_text(*text, ' ', "+00:00", true));
  else if (ocel_frame::reference_date_value(catalog, owner, name, *value))
    bind_text(statement, position,
              ocel_frame::utc_timestamp_text(std::get<std::string>(*value), 'T', "+00:00", true));
  else bind_scalar(statement, position, *value);
}

void bind_type_attributes(sqlite3_stmt* statement, int start_index,
                          const TypeColumns& columns,
                          const std::vector<OcelAttribute>& attributes,
                          const OcelTypeCatalog& catalog, const std::string& owner) {
  const auto values = attribute_values(attributes);
  for (std::size_t index = 0; index < columns.size(); ++index) {
    const auto value = values.find(columns[index].first);
    bind_type_value(statement, start_index + static_cast<int>(index), columns[index].first,
                    columns[index].second, value == values.end() ? nullptr : value->second, catalog, owner);
  }
}

OcelValue read_value(sqlite3_stmt* statement, int type_column, int text_column,
                     int int_column, int real_column, int bool_column) {
  const auto type = column_string(statement, type_column);
  if (type == "null") return nullptr;
  if (type == "bool") return sqlite3_column_int(statement, bool_column) != 0;
  if (type == "int") return static_cast<std::int64_t>(
      sqlite3_column_int64(statement, int_column));
  if (type == "double") return sqlite3_column_double(statement, real_column);
  if (type == "string") return column_string(statement, text_column);
  throw std::invalid_argument("OCEL SQLite attribute has an unknown value type");
}

void read_attributes(sqlite3* db, std::string_view table,
                     std::map<std::string, std::vector<OcelAttribute>>& target) {
  if (!table_exists(db, table)) return;
  Statement statement(db, "SELECT ocel_id, attribute_name, value_type, "
                         "text_value, integer_value, real_value, bool_value, ordinal "
                         "FROM " + quote_identifier(table) +
                         " ORDER BY ocel_id, ordinal");
  while (true) {
    const auto status = sqlite3_step(statement.get());
    if (status == SQLITE_DONE) break;
    if (status != SQLITE_ROW)
      throw std::runtime_error("SQLite attribute query failed: " +
                               std::string(sqlite3_errmsg(db)));
    const auto id = column_string(statement.get(), 0);
    OcelAttribute attribute;
    attribute.key = column_string(statement.get(), 1);
    attribute.value = read_value(statement.get(), 2, 3, 4, 5, 6);
    target[id].push_back(std::move(attribute));
  }
}

struct TableColumn {
  std::string name;
  std::string declared_type;
  std::string file_name;  // the name in the file (name may be the reference's renamed one)
};

// field_alias: no object table read has an "ocel_changed_field" column but
// one has "ocel:field" (computed where the reader reads the object tables).
std::vector<TableColumn> table_columns(sqlite3* db, std::string_view table, bool field_alias = false) {
  std::vector<TableColumn> result;
  if (!table_exists(db, table)) return result;
  Statement statement(db, "PRAGMA table_info(" + quote_identifier(table) + ")");
  while (true) {
    const auto status = sqlite3_step(statement.get());
    if (status == SQLITE_DONE) break;
    if (status != SQLITE_ROW)
      throw std::runtime_error("SQLite table-info query failed");
    auto name = column_string(statement.get(), 1);
    result.push_back({name, column_string(statement.get(), 2), name});
  }
  // Round ER: the reference renames a type table's columns (ocel_id to
  // ocel:eid or ocel:oid, ocel_time to ocel:timestamp) before it looks them
  // up, so a table that already uses the new names reads the same (observed
  // black-box, probe er hb6.py): here such a column reads under the file
  // name. A table with both spellings is outside the model (declared).
  const bool events = table.rfind("event_", 0) == 0 && table != "event_object" && table != "event_map_type";
  const bool objects = table.rfind("object_", 0) == 0 && table != "object_object" && table != "object_map_type";
  if (events || objects) {
    const auto present = [&](std::string_view name) {
      return std::any_of(result.begin(), result.end(), [&](const TableColumn& column) { return column.name == name; });
    };
    const bool id = present("ocel_id"), time = present("ocel_time");
    // Round ET: the reference renames ocel_changed_field to "ocel:field"
    // after concatenating the object tables, so when none of them has an
    // ocel_changed_field column an "ocel:field" column is their changed
    // field (observed black-box, probe et t1; next to one in any table the
    // split raises, see repeated_time_fault's caller).
    const bool changed = present("ocel_changed_field");
    for (auto& column : result) {
      if (!id && column.name == (events ? "ocel:eid" : "ocel:oid")) column.name = "ocel_id";
      else if (!time && column.name == "ocel:timestamp") column.name = "ocel_time";
      else if (objects && field_alias && !changed && column.name == "ocel:field") column.name = "ocel_changed_field";
    }
  }
  return result;
}

// Round ER: the reference reads every table with "SELECT * FROM <table>"
// and takes its columns by name as pandas names them, exactly (a column
// "OCEL_ID" is no "ocel_id"); a missing one raises the KeyError of the name
// it looks up (observed black-box, probes er hb3.py and hb4.py). The rows
// come in the table's scan order (rowid order; a WITHOUT ROWID table's key
// order; a view's), which a plain "SELECT *" gives here too, where "ORDER BY
// rowid" fails on a table without rowid and follows a column named rowid.
int result_column(sqlite3_stmt* statement, std::string_view name) {
  const int count = sqlite3_column_count(statement);
  for (int index = 0; index < count; ++index)
    if (const char* column = sqlite3_column_name(statement, index); column != nullptr && name == column)
      return index;
  return -1;
}
std::invalid_argument key_error(std::string_view name) {
  return std::invalid_argument("KeyError: " + ocel_frame::python_str_repr(name));
}
// The column of `name`, or of `key` (the name the reference renames it to:
// a table that already uses it reads the same, observed black-box, probe er
// hb6.py), else the KeyError of `key`.
int required_column(sqlite3_stmt* statement, std::string_view name, std::string_view key) {
  int index = result_column(statement, name);
  if (index < 0 && name != key) index = result_column(statement, key);
  if (index < 0) throw key_error(key);
  return index;
}

OcelValue read_sqlite_cell(sqlite3_stmt* statement, int column,
                           std::string_view declared_type = {}) {
  // Round ER: a cell reads as SQLite stores it, whatever the column's
  // declared type (read_sql; a BOOLEAN column's 1 is the integer 1, its
  // "true" a text: observed black-box, probe er h5). Neither writer
  // declares BOOLEAN; the native tables keep booleans (read_value).
  (void)declared_type;
  const auto sqlite_type = sqlite3_column_type(statement, column);
  switch (sqlite_type) {
    case SQLITE_NULL:
      return nullptr;
    case SQLITE_INTEGER:
      return static_cast<std::int64_t>(sqlite3_column_int64(statement, column));
    case SQLITE_FLOAT:
      return sqlite3_column_double(statement, column);
    case SQLITE_TEXT:
      return column_string(statement, column);
    default:
      throw std::invalid_argument("OCEL SQLite attributes cannot contain BLOB values");
  }
}

// One pass over a type table: per ocel_id, the timestamp of its first row by
// rowid and the attributes of its base row (see TypeTableBase). Reference-
// written tables have no index on ocel_id, so per-id queries would scan the
// table once per id.
struct TypeTableRow {
  std::string timestamp;
  // The time cell as SQLite stores it (its text in `timestamp`).
  OcelSqliteTime time;
  std::vector<OcelAttribute> attributes;
  // The row's place in its table (every row counts, NULL ids too): the
  // reference frame keeps it as the row's index.
  std::size_t position = 0;
};

// Round EJ: read_sql types each column of a type table over all of its
// rows, base and change rows alike: a column of integers with a NULL or a
// REAL cell anywhere in the table is float64, so its integers read as floats
// (observed black-box: a change row of another field leaves the column NULL,
// so a pm4py-written object table with changes of two fields reads its
// integer and boolean columns as floats). The names of those columns.
std::unordered_set<std::string> float64_integer_columns(sqlite3* db, std::string_view table,
                                                        const std::vector<TableColumn>& columns) {
  struct Seen {
    bool integer = false, real = false, null = false, other = false;
  };
  std::vector<Seen> seen(columns.size());
  Statement statement(db, "SELECT * FROM " + quote_identifier(table));
  while (true) {
    const auto status = sqlite3_step(statement.get());
    if (status == SQLITE_DONE) break;
    if (status != SQLITE_ROW) throw std::runtime_error("SQLite type-table column query failed");
    const auto count = std::min<std::size_t>(columns.size(),
                                             static_cast<std::size_t>(sqlite3_column_count(statement.get())));
    for (std::size_t index = 0; index < count; ++index) {
      switch (sqlite3_column_type(statement.get(), static_cast<int>(index))) {
        case SQLITE_INTEGER: seen[index].integer = true; break;
        case SQLITE_FLOAT: seen[index].real = true; break;
        case SQLITE_NULL: seen[index].null = true; break;
        default: seen[index].other = true; break;
      }
    }
  }
  std::unordered_set<std::string> result;
  for (std::size_t index = 0; index < columns.size(); ++index)
    if (seen[index].integer && !seen[index].other && (seen[index].null || seen[index].real))
      result.insert(columns[index].name);
  return result;
}

void floatify_integer(OcelValue& value) {
  if (const auto* integer = std::get_if<std::int64_t>(&value)) value = static_cast<double>(*integer);
}

// Which rows of a type table are rows of the event or object itself. An
// event table's rows all are. In object tables the reference reader takes
// the rows without a changed field, or, when no object table has one, each
// id's first row over the object tables in type-name order (the later rows
// are its changes). A "@@cumcount" column in the file plays no part: the
// reader counts the rows itself (observed black-box).
enum class TypeTableBase { kEvents, kNullChangedField, kFirstRow };

class TypeTableRows {
 public:
  TypeTableRows(sqlite3* db, std::string_view table, TypeTableBase base, bool field_alias = false) {
    const auto columns = table_columns(db, table, field_alias);
    if (columns.empty()) return;
    exists_ = true;
    for (const auto& column : columns) column_names_.push_back(column.name);
    int id_index = -1, time_index = -1, changed_index = -1;
    const bool events = base == TypeTableBase::kEvents;
    for (std::size_t index = 0; index < columns.size(); ++index) {
      const auto& name = columns[index].name;
      if (name == "ocel_id") id_index = static_cast<int>(index);
      else if (name == "ocel_time") time_index = static_cast<int>(index);
      else if (name == "ocel_changed_field" && !events) changed_index = static_cast<int>(index);
    }
    // Round ER: an event table's ocel_changed_field and "@@cumcount" are
    // attributes like any other column; an object table's "ocel:type" is
    // replaced by the object's type (observed black-box, probe er hb5.py).
    const auto reserved = [events](const std::string& name) {
      if (name == "ocel_id" || name == "ocel_time") return true;
      if (events) return false;
      return name == "ocel_changed_field" || name == "@@cumcount" || name == "ocel:type";
    };
    if (id_index < 0) throw key_error(base == TypeTableBase::kEvents ? "ocel:eid" : "ocel:oid");
    has_time_ = time_index >= 0;
    Statement statement(db, "SELECT * FROM " + quote_identifier(table));
    bool integers = true, nulls = true, objects = false, texts = false, blobs = false, numbers = false;
    std::vector<bool> valued(columns.size(), false);
    for (std::size_t position = 0;; ++position) {
      const auto status = sqlite3_step(statement.get());
      if (status == SQLITE_DONE) break;
      if (status != SQLITE_ROW)
        throw std::runtime_error("SQLite type-table attribute query failed");
      auto* row = statement.get();
      ++row_count_;
      for (std::size_t index = 0; index < columns.size(); ++index)
        if (!valued[index] && sqlite3_column_type(row, static_cast<int>(index)) != SQLITE_NULL) valued[index] = true;
      // read_sql's dtype of the time column comes from every row.
      if (has_time_) {
        const auto type = sqlite3_column_type(row, time_index);
        if (type != SQLITE_NULL) nulls = false;
        if (type != SQLITE_INTEGER) integers = false;
        if (type == SQLITE_TEXT || type == SQLITE_BLOB) objects = true;
        texts = texts || type == SQLITE_TEXT;
        blobs = blobs || type == SQLITE_BLOB;
        numbers = numbers || type == SQLITE_INTEGER || type == SQLITE_FLOAT;
      }
      // Round EO: read_ocel2_sqlite splits the type tables' union by
      // ocel_changed_field, so a row without a changed field (an object's,
      // a repeated or id-less one) types the changes frame's columns too
      // (observed black-box, s53_0000: an int column another table lacks is
      // float64 there): its cells go to base_rows.
      if (changed_index < 0 || sqlite3_column_type(row, changed_index) == SQLITE_NULL) {
        std::vector<OcelAttribute> cells;
        for (std::size_t index = 0; index < columns.size(); ++index) {
          const auto& column = columns[index];
          if (reserved(column.name) || (events && column.name == "ocel:activity")) continue;
          if (sqlite3_column_type(row, static_cast<int>(index)) == SQLITE_NULL) continue;
          cells.push_back({column.name, read_sqlite_cell(row, static_cast<int>(index), column.declared_type)});
        }
        base_rows_.push_back(std::move(cells));
        base_row_ids_.push_back(sqlite3_column_type(row, id_index) != SQLITE_NULL);
      }
      // Round EM: a row that is no event or object of its own still types
      // the frame's columns (read_sql reads the whole table): its cells go
      // to the frame hint, a change row's cells outside its field to the
      // changes frame's (OcelFrameHint).
      const auto extra = [&](bool change_row) {
        std::vector<OcelAttribute> cells, change_cells;
        const std::string field =
            change_row && changed_index >= 0 && sqlite3_column_type(row, changed_index) != SQLITE_NULL
                ? column_string(row, changed_index)
                : std::string();
        for (std::size_t index = 0; index < columns.size(); ++index) {
          const auto& column = columns[index];
          if (reserved(column.name) || (events && column.name == "ocel:activity")) continue;
          if (sqlite3_column_type(row, static_cast<int>(index)) == SQLITE_NULL) continue;
          OcelAttribute cell{column.name,
                             read_sqlite_cell(row, static_cast<int>(index), column.declared_type)};
          if (change_row && column.name != field) change_cells.push_back(cell);
          cells.push_back(std::move(cell));
        }
        extra_rows_.push_back(std::move(cells));
        if (change_row) extra_change_values_.push_back(std::move(change_cells));
      };
      if (sqlite3_column_type(row, id_index) == SQLITE_NULL) {
        extra(false);
        continue;
      }
      const bool changed = changed_index >= 0 &&
                           sqlite3_column_type(row, changed_index) != SQLITE_NULL;
      if (base == TypeTableBase::kNullChangedField && changed) {
        extra(true);
        continue;
      }
      // An id with two rows is one event or object here: the first row. (For
      // events and base rows the reference keeps a frame row for each,
      // declared; for kFirstRow the later rows are changes.)
      auto [entry, inserted] = rows_.try_emplace(column_string(row, id_index));
      if (!inserted) {
        extra(base == TypeTableBase::kFirstRow);
        continue;
      }
      order_.push_back(entry->first);
      entry->second.position = position;
      // No time column reads as no time (NaT), as the reference reads it.
      if (has_time_) {
        auto& time = entry->second.time;
        switch (sqlite3_column_type(row, time_index)) {
          case SQLITE_INTEGER:
            time.kind = OcelSqliteTime::Kind::kInteger;
            time.integer = sqlite3_column_int64(row, time_index);
            break;
          case SQLITE_FLOAT:
            time.kind = OcelSqliteTime::Kind::kReal;
            time.real = sqlite3_column_double(row, time_index);
            break;
          case SQLITE_TEXT: time.kind = OcelSqliteTime::Kind::kText; break;
          case SQLITE_BLOB: time.kind = OcelSqliteTime::Kind::kBlob; break;
          default: break;
        }
        entry->second.timestamp = column_string(row, time_index);
      }
      for (std::size_t index = 0; index < columns.size(); ++index) {
        const auto& column = columns[index];
        if (reserved(column.name)) continue;
        if (sqlite3_column_type(row, static_cast<int>(index)) == SQLITE_NULL)
          continue;
        entry->second.attributes.push_back(
            {column.name, read_sqlite_cell(row, static_cast<int>(index),
                                           column.declared_type)});
      }
    }
    // Round EO: read_sql reads an all-NULL column as object (None), which
    // keeps the frames' concatenated column an object one (observed
    // black-box: s53_0094, ints next to it print as ints). Round ER: so
    // does every column of a table without rows (a listed type's table
    // still takes part in the concatenation, probe er hc/empty_dtypes.py).
    for (std::size_t index = 0; index < columns.size(); ++index) {
      const auto& name = columns[index].name;
      if (valued[index] || reserved(name) || (events && name == "ocel:activity")) continue;
      null_columns_.push_back(name);
    }
    if (!objects && !nulls)
      time_column_ = integers ? OcelSqliteTimeColumn::kInteger : OcelSqliteTimeColumn::kReal;
    // Round EN: an object column reads its NULLs as None.
    if (has_time_ && (nulls || blobs || (texts && numbers)))
      for (auto& [id, row] : rows_)
        if (row.time.kind == OcelSqliteTime::Kind::kNull) row.time.none = true;
    if (const auto floats = float64_integer_columns(db, table, columns); !floats.empty()) {
      for (auto& [id, row] : rows_)
        for (auto& attribute : row.attributes)
          if (floats.contains(attribute.key)) floatify_integer(attribute.value);
      for (auto* extra : {&extra_rows_, &extra_change_values_, &base_rows_})
        for (auto& cells : *extra)
          for (auto& cell : cells)
            if (floats.contains(cell.key)) floatify_integer(cell.value);
    }
  }

  bool exists() const { return exists_; }
  bool has_time() const { return has_time_; }
  // Every row of the table, NULL ids and repeated ids too.
  std::size_t row_count() const { return row_count_; }
  // read_sql's dtype of the time column (with rows): int64, float64 or object.
  OcelSqliteTimeColumn time_column() const { return time_column_; }
  const TypeTableRow* find(const std::string& id) const {
    const auto found = rows_.find(id);
    return found == rows_.end() ? nullptr : &found->second;
  }
  // The ids in table order.
  const std::vector<std::string>& ids() const { return order_; }
  // Round EM: the cells of the rows that are no event or object of their
  // own, and of change rows outside their field (see the constructor).
  const std::vector<std::vector<OcelAttribute>>& extra_rows() const { return extra_rows_; }
  const std::vector<std::vector<OcelAttribute>>& extra_change_values() const { return extra_change_values_; }
  // Round EO: the cells of the rows without a changed field (see the
  // constructor).
  const std::vector<std::vector<OcelAttribute>>& base_rows() const { return base_rows_; }
  // Whether each of base_rows has an id.
  const std::vector<bool>& base_row_ids() const { return base_row_ids_; }
  // Round EO: the attribute columns without a value in any row (see the
  // constructor).
  const std::vector<std::string>& null_columns() const { return null_columns_; }
  // Round EN: the table's column names in table order.
  const std::vector<std::string>& column_names() const { return column_names_; }

 private:
  std::vector<std::string> column_names_;
  std::vector<std::vector<OcelAttribute>> extra_rows_;
  std::vector<std::vector<OcelAttribute>> extra_change_values_;
  std::vector<std::vector<OcelAttribute>> base_rows_;
  std::vector<bool> base_row_ids_;
  std::vector<std::string> null_columns_;
  bool exists_ = false;
  bool has_time_ = false;
  std::size_t row_count_ = 0;
  OcelSqliteTimeColumn time_column_ = OcelSqliteTimeColumn::kObject;
  std::unordered_map<std::string, TypeTableRow> rows_;
  std::vector<std::string> order_;
};

// True when some object type table has a row without a changed field (a
// table without that column counts: every row of it lacks one).
bool object_tables_have_base_rows(sqlite3* db, const std::vector<std::string>& tables, bool field_alias) {
  for (const auto& table : tables) {
    const auto columns = table_columns(db, table, field_alias);
    if (columns.empty()) continue;
    const auto changed = std::find_if(columns.begin(), columns.end(), [](const auto& column) {
      return column.name == "ocel_changed_field";
    });
    if (changed == columns.end()) {
      Statement any(db, "SELECT 1 FROM " + quote_identifier(table) + " LIMIT 1");
      const auto status = sqlite3_step(any.get());
      if (status == SQLITE_ROW) return true;
      if (status != SQLITE_DONE) throw std::runtime_error("SQLite object base-row query failed");
      continue;
    }
    Statement base(db, "SELECT 1 FROM " + quote_identifier(table) + " WHERE " +
                           quote_identifier(changed->file_name) + " IS NULL LIMIT 1");
    const auto status = sqlite3_step(base.get());
    if (status == SQLITE_ROW) return true;
    if (status != SQLITE_DONE) throw std::runtime_error("SQLite object base-row query failed");
  }
  return false;
}

// The change rows of one object type table. With base rows somewhere
// (has_base_rows), every row with a changed field is a change. Without, a row
// is a change when its id already had a row earlier in the object tables
// (first_rows holds the ids seen so far, over the tables in the order the
// objects are read); the first row is the object's own.
void read_type_table_changes(sqlite3* db, std::string_view table,
                             std::string_view object_type, Ocel& log,
                             const std::set<std::string>& object_ids,
                             bool validate_references, bool has_base_rows,
                             std::unordered_set<std::string>& first_rows,
                             std::size_t type_rank,
                             std::vector<std::pair<std::size_t, std::size_t>>& places,
                             std::vector<OcelSqliteTime>& times,
                             std::vector<OcelSqliteTimeColumn>& table_columns_read,
                             std::unordered_map<std::string, std::int64_t>& id_rows,
                             bool& timed, bool& untimed_changes, bool field_alias) {
  const auto columns = table_columns(db, table, field_alias);
  const auto changed = std::find_if(
      columns.begin(), columns.end(), [](const auto& column) {
        return column.name == "ocel_changed_field";
      });
  const auto timestamp = std::find_if(
      columns.begin(), columns.end(), [](const auto& column) {
        return column.name == "ocel_time";
      });
  const auto id = std::find_if(
      columns.begin(), columns.end(), [](const auto& column) {
        return column.name == "ocel_id";
      });
  if (id == columns.end()) return;
  timed = timed || timestamp != columns.end();
  // Round EN: read_ocel2_sqlite's "@@cumcount" counts each id's rows over
  // the object tables in type order, base rows and tables without changes
  // included (observed black-box, probe en cc.py).
  if (changed == columns.end()) {
    // No change rows: every row is an object row (counted for "@@cumcount").
    const int id_at = static_cast<int>(id - columns.begin());
    Statement rows(db, "SELECT * FROM " + quote_identifier(table));
    while (true) {
      const auto status = sqlite3_step(rows.get());
      if (status == SQLITE_DONE) break;
      if (status != SQLITE_ROW) throw std::runtime_error("SQLite object-change query failed");
      if (sqlite3_column_type(rows.get(), id_at) != SQLITE_NULL) ++id_rows[column_string(rows.get(), id_at)];
    }
    return;
  }
  // Round ER: a table without a time column keeps its change rows with a
  // missing (NaN) time when another object table has one; pandas' concat
  // leaves the table out of the time column's dtype (a float64 column stays
  // float64). With no time column anywhere a change row raises KeyError
  // 'ocel:timestamp' (untimed_changes; observed black-box, probe er h9).
  const bool untimed = timestamp == columns.end();
  Statement statement(db, "SELECT * FROM " + quote_identifier(table));
  const auto id_index = static_cast<int>(id - columns.begin());
  const auto changed_index = static_cast<int>(changed - columns.begin());
  const auto timestamp_index = untimed ? -1 : static_cast<int>(timestamp - columns.begin());
  const auto floats = float64_integer_columns(db, table, columns);
  // Round EL: read_sql's dtype of the table's time column (over every row,
  // base rows too): int64 when every value is an integer, float64 for
  // numbers with NULLs or reals, else object.
  bool integers = true, nulls = true, objects = false, texts = false, blobs = false, numbers = false;
  const auto first_time = times.size();
  for (std::size_t position = 0;; ++position) {
    const auto status = sqlite3_step(statement.get());
    if (status == SQLITE_DONE) break;
    if (status != SQLITE_ROW)
      throw std::runtime_error("SQLite object-change query failed");
    if (!untimed) {
      const auto type = sqlite3_column_type(statement.get(), timestamp_index);
      if (type != SQLITE_NULL) nulls = false;
      if (type != SQLITE_INTEGER) integers = false;
      if (type == SQLITE_TEXT || type == SQLITE_BLOB) objects = true;
      texts = texts || type == SQLITE_TEXT;
      blobs = blobs || type == SQLITE_BLOB;
      numbers = numbers || type == SQLITE_INTEGER || type == SQLITE_FLOAT;
    }
    std::int64_t cumcount = 0;
    if (sqlite3_column_type(statement.get(), id_index) != SQLITE_NULL)
      cumcount = id_rows[column_string(statement.get(), id_index)]++;
    if (has_base_rows) {
      if (sqlite3_column_type(statement.get(), changed_index) == SQLITE_NULL) continue;
    } else {
      // No base row anywhere: an id's first row is the object, a row without
      // an id is neither.
      if (sqlite3_column_type(statement.get(), id_index) == SQLITE_NULL) continue;
      if (first_rows.insert(column_string(statement.get(), id_index)).second) continue;
    }
    // An empty changed field is a change too (its value then NaN).
    const auto field = column_string(statement.get(), changed_index);
    const auto object_id = column_string(statement.get(), id_index);
    if (validate_references && !object_ids.contains(object_id))
      throw std::invalid_argument("OCEL SQLite object change references an unknown id");
    const auto value_column = std::find_if(
        columns.begin(), columns.end(), [&](const auto& column) {
          return column.name == field;
        });
    OcelObjectChange change;
    change.object_id = object_id;
    change.object_type = std::string(object_type);
    if (!untimed) change.timestamp = column_string(statement.get(), timestamp_index);
    untimed_changes = untimed_changes || untimed;
    change.field = field;
    // A field without a column in its table has no value there: the
    // reference keeps the change with a NaN value (its writer leaves out a
    // column that holds only NULLs; observed black-box).
    change.value = value_column == columns.end()
                       ? OcelValue{nullptr}
                       : read_sqlite_cell(statement.get(),
                                          static_cast<int>(value_column - columns.begin()),
                                          value_column->declared_type);
    if (floats.contains(field)) floatify_integer(change.value);
    // Round EN: the row's other cells and its "@@cumcount" stay with the
    // change (see OcelObjectChange::cells).
    for (std::size_t index = 0; index < columns.size(); ++index) {
      const auto& column = columns[index];
      if (column.name == "ocel_id" || column.name == "ocel_time" || column.name == "ocel_changed_field" ||
          column.name == kOcelCumcountColumn || column.name == field || column.name == "ocel:type")
        continue;
      if (sqlite3_column_type(statement.get(), static_cast<int>(index)) == SQLITE_NULL) continue;
      OcelAttribute cell{column.name, read_sqlite_cell(statement.get(), static_cast<int>(index), column.declared_type)};
      if (floats.contains(column.name)) floatify_integer(cell.value);
      change.cells.push_back(std::move(cell));
    }
    // Round EO: NaN for a row without an id (see base_rows).
    if (sqlite3_column_type(statement.get(), id_index) != SQLITE_NULL)
      change.cells.push_back({kOcelCumcountColumn, OcelValue{cumcount}});
    log.object_changes.push_back(std::move(change));
    places.emplace_back(position, type_rank);
    OcelSqliteTime time;
    switch (untimed ? SQLITE_NULL : sqlite3_column_type(statement.get(), timestamp_index)) {
      case SQLITE_INTEGER:
        time.kind = OcelSqliteTime::Kind::kInteger;
        time.integer = sqlite3_column_int64(statement.get(), timestamp_index);
        break;
      case SQLITE_FLOAT:
        time.kind = OcelSqliteTime::Kind::kReal;
        time.real = sqlite3_column_double(statement.get(), timestamp_index);
        break;
      case SQLITE_TEXT: time.kind = OcelSqliteTime::Kind::kText; break;
      case SQLITE_BLOB: time.kind = OcelSqliteTime::Kind::kBlob; break;
      default: break;
    }
    times.push_back(time);
  }
  if (untimed) return;
  // Round EM: the table's dtype joins the concatenated change frame's
  // (a table without rows, or of NULLs only, is object; see
  // sort_ocel2_sqlite_changes).
  table_columns_read.push_back(objects || nulls ? OcelSqliteTimeColumn::kObject
                               : integers       ? OcelSqliteTimeColumn::kInteger
                                                : OcelSqliteTimeColumn::kReal);
  // Round EN: an object column reads its NULLs as None.
  if (nulls || blobs || (texts && numbers))
    for (auto index = first_time; index < times.size(); ++index)
      if (times[index].kind == OcelSqliteTime::Kind::kNull) times[index].none = true;
  // A float64 table holds its integers as floats.
  if (!objects && !nulls && !integers)
    for (auto index = first_time; index < times.size(); ++index)
      if (times[index].kind == OcelSqliteTime::Kind::kInteger) {
        times[index].kind = OcelSqliteTime::Kind::kReal;
        times[index].real = static_cast<double>(times[index].integer);
      }
}

// Round EL: write_ocel2_sqlite converts the event and the change times of
// each type table with pd.to_datetime(errors="coerce") (no utc). A datetime64
// column (every column native reads its times into) comes out as it went
// in; a column pm4py holds as texts (an OCEL 2.0 SQLite time column with a
// text pandas could not read, kept as read; native keeps the texts, one of
// them unreadable: text_time_column) takes, per table, the format pandas
// guesses from the table's first text, a text that does not match it is
// NULL, a table of mixed zones raises (observed black-box, probe el
// sw3.py). One text per time in frame order (nullopt for NULL); nullopt
// outside the model.
bool text_time_column(const std::vector<std::string_view>& times) {
  return std::any_of(times.begin(), times.end(), [](std::string_view text) {
    if (ocel_frame::pandas_nat_time_text(text)) return false;
    const auto parsed = parse_timestamp_exact(text);
    return !parsed || !ocel_frame::civil_timestamp(*parsed).has_value();
  });
}
std::optional<std::vector<std::optional<std::string>>> coerced_time_texts(
    const std::vector<std::string_view>& times) {
  namespace ptd = pandas_to_datetime;
  if (times.empty()) return std::vector<std::optional<std::string>>{};
  const auto read = ptd::to_datetime_coerce(times);
  if (!read) return std::nullopt;
  std::vector<std::optional<std::string>> out;
  out.reserve(times.size());
  for (const auto& value : read->values) {
    if (!value) {
      out.emplace_back();
      continue;
    }
    // A Python datetime's str(): microseconds when not zero, the column's
    // zone as an offset.
    char buffer[64];
    int length = std::snprintf(buffer, sizeof buffer, "%04d-%02d-%02d %02d:%02d:%02d", value->year,
                               value->month, value->day, value->hour, value->minute, value->second);
    std::string text(buffer, static_cast<std::size_t>(length));
    if (const auto micro = value->nanoseconds / 1000; micro != 0) {
      length = std::snprintf(buffer, sizeof buffer, ".%06u", micro);
      text.append(buffer, static_cast<std::size_t>(length));
    }
    if (read->offset_minutes) {
      const int minutes = *read->offset_minutes;
      const int magnitude = minutes < 0 ? -minutes : minutes;
      length = std::snprintf(buffer, sizeof buffer, "%c%02d:%02d", minutes < 0 ? '-' : '+', magnitude / 60,
                             magnitude % 60);
      text.append(buffer, static_cast<std::size_t>(length));
    }
    out.push_back(std::move(text));
  }
  return out;
}

// Round EL: the same conversion of a type table's raw times (OcelRawTime,
// a column pm4py kept as read_sql gave it), observed black-box (probe el
// wo.py): pandas reads element by element when the first value is a number
// (an int or float counts nanoseconds since the epoch, naive), so an aware
// text there raises; when it is a text, the format guessed from it reads
// the texts and every number is NaT. NaN is NULL.
std::vector<std::optional<std::string>> coerced_raw_time_texts(
    const std::vector<std::pair<OcelRawTime, std::string_view>>& times) {
  const auto missing = [](const std::pair<OcelRawTime, std::string_view>& time) {
    return ocel_raw_time_missing(time.first) || time.first == OcelRawTime::kNone ||
           (time.first == OcelRawTime::kStr && ocel_frame::pandas_nat_time_text(time.second));
  };
  const auto first = std::find_if_not(times.begin(), times.end(), missing);
  std::vector<std::optional<std::string>> out(times.size());
  if (first == times.end()) return out;
  if (first->first == OcelRawTime::kStr) {
    std::vector<std::string_view> texts;
    for (const auto& [kind, text] : times) texts.push_back(kind == OcelRawTime::kStr ? text : std::string_view{});
    if (auto coerced = coerced_time_texts(texts)) return std::move(*coerced);
    return out;
  }
  for (const auto& [kind, text] : times) {
    if (kind != OcelRawTime::kStr || ocel_frame::pandas_nat_time_text(text)) continue;
    const auto parsed = parse_timestamp_exact(text);
    if (parsed && parsed->timezone_aware)
      throw std::invalid_argument(
          "ValueError: Mixed timezones detected. Pass utc=True in to_datetime or tz='UTC' in DatetimeIndex to "
          "convert to a common timezone.");
  }
  for (std::size_t i = 0; i < times.size(); ++i) {
    const auto& [kind, text] = times[i];
    if (kind == OcelRawTime::kInt || kind == OcelRawTime::kFloat) {
      std::int64_t nanoseconds = 0;
      if (kind == OcelRawTime::kInt) {
        nanoseconds = std::stoll(std::string(text));
      } else {
        const double value = std::stod(std::string(text));
        if (!(value > -9.2e18 && value < 9.2e18)) continue;
        nanoseconds = static_cast<std::int64_t>(value);
      }
      std::int64_t seconds = nanoseconds / 1000000000;
      std::int64_t rest = nanoseconds % 1000000000;
      if (rest < 0) {
        rest += 1000000000;
        --seconds;
      }
      std::int64_t days = seconds / 86400, clock = seconds % 86400;
      if (clock < 0) {
        clock += 86400;
        --days;
      }
      // Howard Hinnant's civil_from_days.
      const std::int64_t z = days + 719468;
      const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
      const std::int64_t doe = z - era * 146097;
      const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
      const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
      const std::int64_t mp = (5 * doy + 2) / 153;
      const std::int64_t day = doy - (153 * mp + 2) / 5 + 1;
      const std::int64_t month = mp < 10 ? mp + 3 : mp - 9;
      const std::int64_t year = yoe + era * 400 + (month <= 2);
      char buffer[64];
      int length = std::snprintf(buffer, sizeof buffer, "%04lld-%02lld-%02lld %02lld:%02lld:%02lld",
                                 static_cast<long long>(year), static_cast<long long>(month),
                                 static_cast<long long>(day), static_cast<long long>(clock / 3600),
                                 static_cast<long long>(clock / 60 % 60), static_cast<long long>(clock % 60));
      std::string printed(buffer, static_cast<std::size_t>(length));
      if (const auto micro = rest / 1000; micro != 0) {
        length = std::snprintf(buffer, sizeof buffer, ".%06lld", static_cast<long long>(micro));
        printed.append(buffer, static_cast<std::size_t>(length));
      }
      out[i] = std::move(printed);
    } else if (kind == OcelRawTime::kStr && !ocel_frame::pandas_nat_time_text(text)) {
      if (auto one = coerced_time_texts(std::vector<std::string_view>{text}); one && !one->empty())
        out[i] = std::move((*one)[0]);
    }
  }
  return out;
}

// Round EN: a change-time column of read_ocel2_xml's or read_ocel2_json's
// UTC datetimes with the texts they keep (a "1970-01-01T00:00:00..." or "0"
// change time, OcelRawTime::kStr): pd.to_datetime(errors="coerce") takes a
// UTC text, raises on a naive one or another offset next to the datetimes,
// and coerces an unreadable one to NaT (observed black-box). nullopt when
// the table holds no such mix.
std::optional<std::vector<std::optional<std::string>>> coerced_mixed_time_texts(
    const std::vector<std::pair<OcelRawTime, std::string_view>>& times) {
  bool dates = false, texts = false;
  for (const auto& [kind, text] : times) {
    if (kind == OcelRawTime::kNone && !ocel_frame::pandas_nat_time_text(text)) dates = true;
    else if (kind == OcelRawTime::kStr) texts = true;
    else if (kind != OcelRawTime::kNone) return std::nullopt;
  }
  if (!dates || !texts) return std::nullopt;
  std::vector<std::optional<std::string>> out(times.size());
  for (std::size_t i = 0; i < times.size(); ++i) {
    const auto& [kind, text] = times[i];
    if (ocel_frame::pandas_nat_time_text(text)) continue;
    const auto parsed = parse_timestamp_exact(text);
    if (!parsed || !ocel_frame::civil_timestamp(*parsed)) continue;
    if (kind == OcelRawTime::kStr && (!parsed->timezone_aware || parsed->offset_minutes != 0))
      throw std::invalid_argument(
          "ValueError: Mixed timezones detected. Pass utc=True in to_datetime or tz='UTC' in DatetimeIndex to "
          "convert to a common timezone.");
    out[i] = ocel_frame::utc_timestamp_text(std::string(text), ' ', "+00:00", true);
  }
  return out;
}

// Round EK: the reference reader's structural errors are pandas.read_sql's
// and Python's (observed black-box, probe ek s/p9.py): a missing table fails
// its "SELECT * FROM <table>" (a type table's name in double quotes).
std::invalid_argument no_such_table(const std::string& from, const std::string& table) {
  return std::invalid_argument("Execution failed on sql 'SELECT * FROM " + from +
                               "': no such table: " + table);
}
void require_table(sqlite3* db, const std::string& table) {
  if (!table_exists(db, table)) throw no_such_table(table, table);
}

// An event_map_type/object_map_type table as the reference reads it: a dict
// in row order (a later listing of a type wins). pandas reads a text column
// with NULLs as its str dtype (NULL is NaN, matching no type) and a column of
// NULLs only as objects (NULL is None).
struct SqliteTypeMap {
  std::map<std::string, std::optional<std::string>> entries;  // nullopt: NULL value
  bool none_key = false;                                      // every key is NULL
  std::optional<std::string> none_value;
  bool null_value_is_nan = false;                             // NULL value next to a text
};

SqliteTypeMap read_type_map(sqlite3* db, const std::string& table) {
  require_table(db, table);
  SqliteTypeMap result;
  Statement statement(db, "SELECT * FROM " + quote_identifier(table));
  const int key_column = required_column(statement.get(), "ocel_type", "ocel_type");
  const int value_column = required_column(statement.get(), "ocel_type_map", "ocel_type_map");
  bool any_key = false, any_value = false, rows = false;
  while (true) {
    const auto status = sqlite3_step(statement.get());
    if (status == SQLITE_DONE) break;
    if (status != SQLITE_ROW)
      throw std::runtime_error("SQLite type map query failed: " +
                               std::string(sqlite3_errmsg(db)));
    rows = true;
    std::optional<std::string> value;
    if (sqlite3_column_type(statement.get(), value_column) != SQLITE_NULL) {
      value = column_string(statement.get(), value_column);
      any_value = true;
    }
    if (sqlite3_column_type(statement.get(), key_column) == SQLITE_NULL) {
      result.none_value = value;
      continue;
    }
    any_key = true;
    result.entries.insert_or_assign(column_string(statement.get(), key_column), value);
  }
  result.none_key = rows && !any_key;
  result.null_value_is_nan = any_value;
  return result;
}

std::map<std::string, std::string> mapped_types(const SqliteTypeMap& type_map) {
  std::map<std::string, std::string> result;
  for (const auto& [type, value] : type_map.entries)
    if (value) result.emplace(type, *value);
  return result;
}

// The distinct values of the event or object table's ocel_type column in
// first-appearance order, as pandas' unique() gives them: NULL is one NaN next
// to texts, and None when the column holds only NULLs.
struct SqliteTypeColumn {
  std::vector<std::optional<std::string>> distinct;  // nullopt: NULL
  std::unordered_set<std::string> seen;
  bool any_null = false;
  bool any_text = false;
  void add(const std::optional<std::string>& value) {
    if (!value) {
      if (!any_null) distinct.emplace_back(std::nullopt);
      any_null = true;
      return;
    }
    any_text = true;
    if (seen.insert(*value).second) distinct.push_back(value);
  }
  bool only_nulls() const { return any_null && !any_text; }
  // The texts in the order the reference reads their tables (sorted()).
  std::vector<std::string> sorted_texts() const {
    std::vector<std::string> result(seen.begin(), seen.end());
    std::sort(result.begin(), result.end());
    return result;
  }
};

// sorted() of the distinct types raises at its first comparison with the NaN
// ("'<' not supported between instances of ..."), whose operand order is
// CPython 3.11's list.sort: runs found by count_run and lengthened by binary
// insertion to minrun (model checked against sorted() on 20,000 lists, probe
// ek s/tsort.py). A NaN met only by the merges reads with the NaN second.
void check_sortable_types(const SqliteTypeColumn& column) {
  if (!column.any_null || !column.any_text) return;
  std::vector<const std::string*> values;
  for (const auto& value : column.distinct) values.push_back(value ? &*value : nullptr);
  const auto fail = [](const std::string* left, const std::string* right) {
    throw std::invalid_argument(std::string("'<' not supported between instances of '") +
                                (left == nullptr ? "float" : "str") + "' and '" +
                                (right == nullptr ? "float" : "str") + "'");
  };
  const auto less = [&](const std::string* left, const std::string* right) {
    if (left == nullptr || right == nullptr) fail(left, right);
    return *left < *right;
  };
  const std::size_t n = values.size();
  std::size_t minrun = n, carry = 0;
  while (minrun >= 64) {
    carry |= minrun & 1U;
    minrun >>= 1U;
  }
  minrun += carry;
  std::size_t lo = 0;
  while (lo < n) {
    std::size_t run = 1;
    if (lo + 1 < n) {
      const bool descending = less(values[lo + 1], values[lo]);
      std::size_t i = lo + 2;
      if (descending) {
        while (i < n && less(values[i], values[i - 1])) ++i;
      } else {
        while (i < n && !less(values[i], values[i - 1])) ++i;
      }
      run = i - lo;
      if (descending) std::reverse(values.begin() + static_cast<std::ptrdiff_t>(lo),
                                   values.begin() + static_cast<std::ptrdiff_t>(lo + run));
    }
    if (run < minrun) {
      const std::size_t force = std::min(minrun, n - lo);
      for (std::size_t k = lo + run; k < lo + force; ++k) {
        const auto* pivot = values[k];
        std::size_t left = lo, right = k;
        while (left < right) {
          const std::size_t middle = (left + right) / 2;
          if (less(pivot, values[middle])) right = middle;
          else left = middle + 1;
        }
        for (std::size_t m = k; m > left; --m) values[m] = values[m - 1];
        values[left] = pivot;
      }
      run = force;
    }
    lo += run;
  }
  for (const auto* value : values)
    if (value == nullptr) fail(values.front() == nullptr ? values.back() : values.front(), nullptr);
}

// The table the reference reads for each type, in sorted order: the type's
// map entry (a KeyError without one, Python's concatenation error for a NULL
// entry), then "SELECT * FROM "<prefix><entry>"" (observed black-box).
std::map<std::string, std::string> resolve_type_tables(sqlite3* db,
                                                       const SqliteTypeColumn& column,
                                                       const SqliteTypeMap& type_map,
                                                       const std::string& prefix) {
  std::map<std::string, std::string> tables;
  const auto check = [&](const std::optional<std::string>& value) {
    if (!value)
      throw std::invalid_argument(std::string("can only concatenate str (not \"") +
                                  (type_map.null_value_is_nan ? "float" : "NoneType") +
                                  "\") to str");
    const auto table = prefix + *value;
    if (!table_exists(db, table)) throw no_such_table("\"" + table + "\"", table);
    // Round ER: each table's id column, renamed, is looked up before the
    // next type's table is read (observed black-box, probe er hb4.py).
    const auto columns = table_columns(db, table);
    if (std::none_of(columns.begin(), columns.end(), [](const TableColumn& column) { return column.name == "ocel_id"; }))
      throw key_error(prefix == "event_" ? "ocel:eid" : "ocel:oid");
    // Round ES: a table holding its id column under both names (ocel_id and
    // ocel:eid or ocel:oid, which table_columns leaves apart then) raises
    // there too, as the reader takes the renamed id column's strings
    // (observed black-box, probes es x8 p3.py, p4.py).
    const auto alias = prefix == "event_" ? "ocel:eid" : "ocel:oid";
    if (std::any_of(columns.begin(), columns.end(), [&](const TableColumn& column) { return column.name == alias; }))
      throw std::invalid_argument("AttributeError: 'DataFrame' object has no attribute 'str'");
    return table;
  };
  if (column.only_nulls()) {
    // None is a key only of a map whose keys are all NULL; such a log's
    // NULL-typed rows are not read natively (declared).
    if (!type_map.none_key) throw std::invalid_argument("KeyError: None");
    (void)check(type_map.none_value);
    return tables;
  }
  for (const auto& type : column.sorted_texts()) {
    const auto entry = type_map.entries.find(type);
    if (entry == type_map.entries.end())
      throw std::invalid_argument("KeyError: " + ocel_frame::python_str_repr(type));
    tables.emplace(type, check(entry->second));
  }
  return tables;
}

void read_type_catalog(sqlite3* db,
                       const std::map<std::string, std::string>& type_map,
                       std::string_view table_prefix,
                       OcelTypeCatalog& catalog) {
  for (const auto& [type, mapped] : type_map) {
    auto& schema = catalog[type];
    const auto table = std::string(table_prefix) + mapped;
    for (const auto& column : table_columns(db, table)) {
      if (reserved_type_column(column.name)) continue;
      // The reference writer adds the activity to each event type table,
      // and its reader takes the activity from the event table only.
      if (table_prefix == "event_" && column.name == "ocel:activity") continue;
      schema[column.name] =
          column.declared_type.empty() ? "string"
                                       : declared_sql_type(column.declared_type);
      if (schema[column.name] == "INTEGER") schema[column.name] = "int";
      else if (schema[column.name] == "REAL") schema[column.name] = "float";
      else schema[column.name] = "string";
    }
  }
}

void read_declared_type_catalog(sqlite3* db, std::string_view table,
                                OcelTypeCatalog& catalog) {
  if (!table_exists(db, table)) return;
  Statement statement(db, "SELECT ocel_type, attribute_name, attribute_type FROM " +
                             quote_identifier(table) +
                             " ORDER BY ocel_type, attribute_name");
  while (true) {
    const auto status = sqlite3_step(statement.get());
    if (status == SQLITE_DONE) break;
    if (status != SQLITE_ROW)
      throw std::runtime_error("SQLite declared type query failed");
    catalog[column_string(statement.get(), 0)][column_string(statement.get(), 1)] =
        column_string(statement.get(), 2);
  }
}

// Returns the O2O table's missing qualifier column (KeyError
// 'ocel:qualifier'), which the caller raises after the change rows.
bool read_relations(sqlite3* db, Ocel& log, bool validate_references, bool native_tables) {
  const std::set<std::string> event_ids = [&] {
    std::set<std::string> result;
    for (const auto& event : log.events) result.insert(event.id);
    return result;
  }();
  const std::set<std::string> object_ids = [&] {
    std::set<std::string> result;
    for (const auto& object : log.objects) result.insert(object.id);
    return result;
  }();
  // Round ER: each relation table is read whole and its columns looked up
  // (renamed) before the next one is read; a relation table without
  // qualifiers reads them as empty texts, the O2O table raises (observed
  // black-box, probe er hb4.py).
  require_table(db, "event_object");
  {
    Statement statement(db, "SELECT * FROM event_object");
    const int event_column = required_column(statement.get(), "ocel_event_id", "ocel:eid");
    const int object_column = required_column(statement.get(), "ocel_object_id", "ocel:oid");
    int qualifier_column = result_column(statement.get(), "ocel_qualifier");
    if (qualifier_column < 0) qualifier_column = result_column(statement.get(), "ocel:qualifier");
    const auto first_relation = log.relations.size();
    std::vector<QualifierCell> qualifier_cells;
    while (true) {
      const auto status = sqlite3_step(statement.get());
      if (status == SQLITE_DONE) break;
      if (status != SQLITE_ROW)
        throw std::runtime_error("SQLite event-object query failed");
      qualifier_cells.push_back(qualifier_cell(statement.get(), qualifier_column));
      OcelRelation relation{column_string(statement.get(), event_column),
                            column_string(statement.get(), object_column),
                            qualifier_column < 0 ? std::string() : column_string(statement.get(), qualifier_column)};
      if (validate_references &&
          (!event_ids.contains(relation.event_id) ||
           !object_ids.contains(relation.object_id)))
        throw std::invalid_argument("OCEL SQLite event-object relation references an unknown id");
      log.relations.push_back(std::move(relation));
    }
    if (qualifier_column >= 0) type_sqlite_qualifiers(log.relations, first_relation, qualifier_cells);
  }
  require_table(db, "object_object");
  // Round ES: its qualifier column is looked up after the change rows
  // (their KeyError 'ocel:timestamp' first; its ids before them, observed
  // black-box, probe es x11); a missing one reads as empty texts here.
  bool qualifier_missing = false;
  {
    Statement statement(db, "SELECT * FROM object_object");
    // Round ER: the O2O columns are looked up only when the table has rows
    // (observed black-box, probe er hb6.py).
    int source_column = -1, target_column = -1, qualifier_column = -1;
    // Round EO: read_sql of a table without rows gives float64 columns
    // (Ocel::untyped_o2o).
    log.untyped_o2o = true;
    const auto first_link = log.o2o.size();
    std::vector<QualifierCell> qualifier_cells;
    while (true) {
      const auto status = sqlite3_step(statement.get());
      if (status == SQLITE_DONE) break;
      if (status != SQLITE_ROW)
        throw std::runtime_error("SQLite object-object query failed");
      if (log.untyped_o2o) {
        source_column = required_column(statement.get(), "ocel_source_id", "ocel:oid");
        target_column = required_column(statement.get(), "ocel_target_id", "ocel:oid_2");
        qualifier_column = result_column(statement.get(), "ocel_qualifier");
        if (qualifier_column < 0) qualifier_column = result_column(statement.get(), "ocel:qualifier");
        qualifier_missing = qualifier_column < 0;
      }
      log.untyped_o2o = false;
      qualifier_cells.push_back(qualifier_cell(statement.get(), qualifier_missing ? -1 : qualifier_column));
      OcelO2oRelation relation{column_string(statement.get(), source_column),
                               column_string(statement.get(), target_column),
                               qualifier_missing ? std::string() : column_string(statement.get(), qualifier_column)};
      if (validate_references &&
          (!object_ids.contains(relation.source_id) ||
           !object_ids.contains(relation.target_id)))
        throw std::invalid_argument("OCEL SQLite object-object relation references an unknown id");
      log.o2o.push_back(std::move(relation));
    }
    if (!qualifier_missing) type_sqlite_qualifiers(log.o2o, first_link, qualifier_cells);
  }
  if (native_tables && table_exists(db, "event_event")) {
    Statement statement(db, "SELECT source_event_id, target_event_id, "
                           "qualifier FROM event_event ORDER BY rowid");
    while (true) {
      const auto status = sqlite3_step(statement.get());
      if (status == SQLITE_DONE) break;
      if (status != SQLITE_ROW)
        throw std::runtime_error("SQLite event-event query failed");
      OcelE2eRelation relation{column_string(statement.get(), 0),
                               column_string(statement.get(), 1),
                               column_string(statement.get(), 2)};
      if (validate_references &&
          (!event_ids.contains(relation.source_id) ||
           !event_ids.contains(relation.target_id)))
        throw std::invalid_argument("OCEL SQLite event-event relation references an unknown id");
      log.e2e.push_back(std::move(relation));
    }
  }
  return qualifier_missing;
}

#endif  // MINERA_HAS_SQLITE

}  // namespace

Ocel import_ocel_sqlite(const std::filesystem::path& path,
                        const OcelSqliteOptions& options) {
#if !defined(MINERA_HAS_SQLITE)
  (void)path;
  (void)options;
  throw std::runtime_error("SQLite support is not available in this build");
#else
  Database database(path, SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX);
  sqlite3* db = database.get();
  // The reference reads the event table, then the object table (round EK).
  require_table(db, "event");
  require_table(db, "object");

  Ocel log;
  // Each id's type is its last listing; the type tables read are those of
  // every listed type (observed black-box).
  std::map<std::string, std::string> event_types;
  std::set<std::string> activities;
  std::map<std::string, std::string> object_types;
  std::set<std::string> object_type_names;
  SqliteTypeColumn event_type_column;
  SqliteTypeColumn object_type_column;
  // Round ER: both tables are read whole, then each one's id column is
  // looked up, then the event types (sorted), then the object types (sorted)
  // (observed black-box, probe er hb4.py).
  Statement event_rows(db, "SELECT * FROM event");
  Statement object_rows(db, "SELECT * FROM object");
  const int event_id_column = required_column(event_rows.get(), "ocel_id", "ocel_id");
  const int object_id_column = required_column(object_rows.get(), "ocel_id", "ocel_id");
  {
    auto& statement = event_rows;
    const int id_column = event_id_column;
    const int type_column = required_column(statement.get(), "ocel_type", "ocel_type");
    while (true) {
      const auto status = sqlite3_step(statement.get());
      if (status == SQLITE_DONE) break;
      if (status != SQLITE_ROW)
        throw std::runtime_error("SQLite event query failed");
      // The reference reader orders the types it reads, so a NULL type next
      // to a text raises there, and alone it is None, which no map holds
      // (observed black-box; round EK: after both tables and in the
      // reference's order, below).
      if (sqlite3_column_type(statement.get(), type_column) == SQLITE_NULL) {
        if (options.validate_references.value_or(false))
          throw std::invalid_argument("OCEL SQLite event table has a NULL ocel_type");
        event_type_column.add(std::nullopt);
        continue;
      }
      const auto id = column_string(statement.get(), id_column);
      const auto activity = column_string(statement.get(), type_column);
      // An empty type is a type the map must hold (KeyError: '' otherwise);
      // its rows are not read natively (declared).
      event_type_column.add(activity);
      // The reference reader drops an event row without id or type (observed
      // black-box); validate_references refuses it instead. Round EO: it
      // still reads the type table of a row without id (its columns are the
      // frame's, observed black-box: s53_0007).
      if (id.empty() && !activity.empty() && !options.validate_references.value_or(false))
        activities.insert(activity);
      if (id.empty() || activity.empty()) {
        if (!options.validate_references.value_or(false)) continue;
        throw std::invalid_argument("OCEL SQLite event table contains duplicate or empty ids");
      }
      // The reference reader types an id listed twice by its last listing
      // and reads it from the type tables (observed black-box).
      const auto [listed, added] = event_types.insert_or_assign(id, activity);
      if (!added && options.validate_references.value_or(false))
        throw std::invalid_argument("OCEL SQLite event table contains duplicate or empty ids");
      activities.insert(activity);
    }
  }
  check_sortable_types(event_type_column);
  {
    auto& statement = object_rows;
    const int id_column = object_id_column;
    const int type_column = required_column(statement.get(), "ocel_type", "ocel_type");
    while (true) {
      const auto status = sqlite3_step(statement.get());
      if (status == SQLITE_DONE) break;
      if (status != SQLITE_ROW)
        throw std::runtime_error("SQLite object query failed");
      if (sqlite3_column_type(statement.get(), type_column) == SQLITE_NULL) {
        if (options.validate_references.value_or(false))
          throw std::invalid_argument("OCEL SQLite object table has a NULL ocel_type");
        object_type_column.add(std::nullopt);
        continue;
      }
      const auto id = column_string(statement.get(), id_column);
      const auto type = column_string(statement.get(), type_column);
      object_type_column.add(type);
      // As for events: a row without id or type is dropped (its type table
      // still read, round EO).
      if (id.empty() && !type.empty() && !options.validate_references.value_or(false))
        object_type_names.insert(type);
      if (id.empty() || type.empty()) {
        if (!options.validate_references.value_or(false)) continue;
        throw std::invalid_argument("OCEL SQLite object table contains duplicate or empty ids");
      }
      // As for events: an id listed twice has its last listing's type. The
      // reference keeps one object per type-table row, so an id with rows in
      // two type tables is two objects there, which a native OCEL cannot hold
      // (declared): the first table by type name is read.
      if (const auto [it, added] = object_types.insert_or_assign(id, type);
          !added && options.validate_references.value_or(false))
        throw std::invalid_argument(
            "OCEL SQLite object table contains duplicate or empty ids: " + id +
            " is listed twice");
      object_type_names.insert(type);
    }
  }
  // Round EK: the reference sorts the event types, then the object types,
  // reads both map tables, and reads each event type's table, then each
  // object type's, in sorted order (observed black-box, probe ek s/p9.py).
  check_sortable_types(object_type_column);
  // Round ER: both map tables are read before either one's columns are
  // looked up (observed black-box, probe er hb4.py).
  require_table(db, "event_map_type");
  require_table(db, "object_map_type");
  const auto event_type_map = read_type_map(db, "event_map_type");
  const auto object_type_map = read_type_map(db, "object_map_type");
  const auto event_tables = resolve_type_tables(db, event_type_column, event_type_map, "event_");
  const auto event_map = mapped_types(event_type_map);
  const auto object_map = mapped_types(object_type_map);
  read_type_catalog(db, event_map, "event_", log.event_type_attributes);
  read_type_catalog(db, object_map, "object_", log.object_type_attributes);
  // Round EQ: the reference reads a native file through its type tables
  // like any other, so the native tables (declared types, attribute
  // values, changes, globals, event links) are read only on request
  // (observed black-box: native files read back, banks b51, b59 and b61).
  const bool native_tables = options.read_native_tables;
  if (native_tables) {
    read_declared_type_catalog(db, "event_type_attributes",
                               log.event_type_attributes);
    read_declared_type_catalog(db, "object_type_attributes",
                               log.object_type_attributes);
  }
  std::map<std::string, std::vector<OcelAttribute>> event_attributes;
  std::map<std::string, std::vector<OcelAttribute>> object_attributes;
  if (native_tables) {
    read_attributes(db, "event_attributes", event_attributes);
    read_attributes(db, "object_attributes", object_attributes);
  }

  // Every listed type has its table now (resolve_type_tables).
  const auto event_table = [&](const std::string& activity) { return event_tables.at(activity); };
  std::map<std::string, std::string> object_tables_by_type;
  const auto object_table = [&](const std::string& type) { return object_tables_by_type.at(type); };
  // The reference events frame takes its column order from the union of
  // the type tables' columns, one table per event type in name order:
  // activity first when "ocel:activity" comes before "ocel_time" there
  // (observed black-box; round ER: a first table without a time column puts
  // its activity first, probe er h71_0183).
  if (!activities.empty()) {
    for (const auto& type : activities) {
      const auto columns = table_columns(db, event_table(type));
      std::optional<std::size_t> activity_index, time_index;
      for (std::size_t index = 0; index < columns.size(); ++index) {
        if (columns[index].name == "ocel:activity" && !activity_index) activity_index = index;
        if (columns[index].name == "ocel_time" && !time_index) time_index = index;
      }
      if (!activity_index && !time_index) continue;
      log.event_activity_first = activity_index && (!time_index || *activity_index < *time_index);
      break;
    }
  }
  // The reference reader builds its events frame from the type tables, one
  // table per event type in name order, each row keeping its place in its
  // table, and then orders it by time: equal times go by that place, then
  // by type name (observed black-box). An event no table holds a row for is
  // not read; a row in another type's table is the event of its listed type;
  // a missing table raises.
  struct PendingEvent {
    std::size_t position;
    std::size_t type_rank;
    OcelEvent event;
    OcelSqliteTime time;
  };
  // pandas' dtype of the concatenated time column (round ER: a table without
  // rows makes it object, or float64 without a time column); a table with
  // rows but no time column is outside the model.
  std::optional<OcelSqliteTimeColumn> time_column = OcelSqliteTimeColumn::kInteger;
  bool any_rows = false;
  std::vector<PendingEvent> pending_events;
  // Round EM: the type tables' rows that are no event or object of their
  // own (see TypeTableRows::extra_rows).
  std::vector<ocel_frame::HintRow> extra_event_rows, extra_object_rows, extra_change_values, extra_change_rows;
  // Round EO: the type tables' all-NULL columns (TypeTableRows::null_columns).
  std::vector<std::string> null_event_columns, null_object_columns;
  // Round EN: the frames' columns as read_ocel2_sqlite concatenates the type
  // tables of the listed types, in type order: each new column where its
  // first table has it (an all-NULL column too; round ER: a table without
  // rows too), the
  // changes frame over every object table, its field and time columns
  // included (observed black-box, probe en fh.py).
  std::vector<std::string> event_columns, object_columns, change_columns_order;
  // Round EP: the same with the frames' own columns (OcelFrameHint::reserved).
  std::vector<std::string> event_order, object_order;
  const auto own_name = [](const std::string& name, bool events) -> std::string {
    if (name == "ocel_id") return events ? "ocel:eid" : "ocel:oid";
    if (name == "ocel_time" && events) return "ocel:timestamp";
    return name;
  };
  const auto add_column = [](std::vector<std::string>& order, const std::string& name) {
    if (std::find(order.begin(), order.end(), name) == order.end()) order.push_back(name);
  };
  const auto attribute_column = [](const std::string& name) {
    return name != "ocel_id" && name != "ocel_time" && name != "ocel_changed_field" && name != "@@cumcount";
  };
  // A table without a time column reads as NaT; with none in any table
  // the reference has no time column at all and raises when it selects the
  // frame's columns (round ER: after the object tables and their
  // concatenation, observed black-box, probe er hb4.py).
  bool events_untimed = false;
  {
    std::unordered_set<std::string> read;
    std::size_t type_rank = 0;
    bool timed = false;
    for (const auto& type : activities) {
      const auto table = event_table(type);
      const TypeTableRows rows(db, table, TypeTableBase::kEvents);
      if (!rows.exists())
        throw std::invalid_argument("OCEL SQLite event type table is missing: " + table);
      for (const auto& cells : rows.extra_rows()) extra_event_rows.push_back({type, cells});
      // Round ER: a table without rows types the frame like a dropped row
      // holding None in its own columns (null_columns) and lacking the
      // others: an int column it lacks is float64, a boolean one object
      // (pandas 3's concat counts the empty frame, probe er
      // hc/empty_dtypes.py).
      if (rows.row_count() == 0) extra_event_rows.push_back({type, {}});
      for (const auto& name : rows.null_columns())
        if (std::find(null_event_columns.begin(), null_event_columns.end(), name) == null_event_columns.end())
          null_event_columns.push_back(name);
      // Round ER: a listed type's table without rows gives the frame its
      // columns too, each an object one, its time column included (pandas
      // 3 counts the empty frame's dtypes; without a time column it adds
      // the NaN of a missing column), observed black-box (probe er
      // hc/empty_tables.py, hc/empty_dtypes.py; EN1-3's empty tables were
      // tables of types no event lists, which are not read).
      for (const auto& name : rows.column_names()) {
        // Round ER: an event table's ocel_changed_field and "@@cumcount"
        // are attributes (see TypeTableRows).
        if (name != "ocel_id" && name != "ocel_time" && name != "ocel:activity") add_column(event_columns, name);
        add_column(event_order, own_name(name, true));
      }
      timed = timed || rows.has_time();
      if (rows.row_count() > 0) {
        any_rows = true;
        if (!rows.has_time()) time_column.reset();
        else if (time_column) time_column = std::max(*time_column, rows.time_column());
      } else if (time_column) {
        time_column = std::max(*time_column, rows.has_time() ? OcelSqliteTimeColumn::kObject
                                                             : OcelSqliteTimeColumn::kReal);
      }
      for (const auto& id : rows.ids()) {
        const auto listed = event_types.find(id);
        if (listed == event_types.end() || !read.insert(id).second) continue;
        const auto& row = *rows.find(id);
        OcelEvent event;
        event.id = id;
        event.activity = listed->second;
        event.timestamp = row.timestamp;
        if (const auto attributes = event_attributes.find(id);
            attributes != event_attributes.end()) {
          event.attributes = attributes->second;
        } else {
          // pm4py.write_ocel2_sqlite writes an "ocel:activity" column into
          // each event type table; pm4py.read_ocel2_sqlite ignores it (the
          // event table's type is the activity, whatever the column holds),
          // so it is no attribute (observed black-box).
          for (const auto& attribute : row.attributes)
            if (attribute.key != "ocel:activity") event.attributes.push_back(attribute);
        }
        auto time = row.time;
        // A float64 table holds its integers as floats.
        if (rows.time_column() == OcelSqliteTimeColumn::kReal && time.kind == OcelSqliteTime::Kind::kInteger) {
          time.kind = OcelSqliteTime::Kind::kReal;
          time.real = static_cast<double>(time.integer);
        }
        pending_events.push_back({row.position, type_rank, std::move(event), time});
      }
      ++type_rank;
    }
    events_untimed = !activities.empty() && !timed;
  }
  std::stable_sort(pending_events.begin(), pending_events.end(),
                   [](const PendingEvent& left, const PendingEvent& right) {
                     return std::tie(left.position, left.type_rank) <
                            std::tie(right.position, right.type_rank);
                   });
  std::vector<OcelSqliteTime> event_times;
  for (auto& pending : pending_events) {
    log.event_type_attributes.try_emplace(pending.event.activity);
    log.events.push_back(std::move(pending.event));
    event_times.push_back(pending.time);
  }
  if (!any_rows) time_column = OcelSqliteTimeColumn::kObject;
  object_tables_by_type = resolve_type_tables(db, object_type_column, object_type_map, "object_");
  // Objects likewise: one type table per object type in name order, rows in
  // table order, only the rows that are the object's own (not changes); an
  // object without such a row is not read.
  std::vector<std::string> object_tables;
  for (const auto& type : object_type_names) object_tables.push_back(object_table(type));
  // Round ET: an "ocel:field" column is the object tables' changed field
  // when none of them has an ocel_changed_field column (table_columns), so
  // it is no attribute either.
  bool field_alias = false;
  {
    bool field = false, changed_field = false;
    for (const auto& table : object_tables)
      for (const auto& column : table_columns(db, table)) {
        field = field || column.name == "ocel:field";
        changed_field = changed_field || column.name == "ocel_changed_field";
      }
    field_alias = field && !changed_field;
    if (field_alias)
      for (auto& [type, schema] : log.object_type_attributes) schema.erase("ocel:field");
  }
  const bool has_base_rows = object_tables_have_base_rows(db, object_tables, field_alias);
  {
    std::unordered_set<std::string> read;
    for (const auto& type : object_type_names) {
      const auto table = object_table(type);
      const TypeTableRows rows(db, table,
                               has_base_rows ? TypeTableBase::kNullChangedField
                                             : TypeTableBase::kFirstRow,
                               field_alias);
      if (!rows.exists())
        throw std::invalid_argument("OCEL SQLite object type table is missing: " + table);
      for (const auto& cells : rows.extra_rows()) extra_object_rows.push_back({type, cells});
      // Round ER: as for the event tables, in both frames of the split
      // ("@@cumcount" is counted after the concatenation).
      if (rows.row_count() == 0) {
        extra_object_rows.push_back({type, {}});
        extra_change_rows.push_back({type, {{kOcelCumcountColumn, OcelValue{std::int64_t{0}}}}});
      }
      for (const auto& name : rows.null_columns())
        if (std::find(null_object_columns.begin(), null_object_columns.end(), name) == null_object_columns.end())
          null_object_columns.push_back(name);
      // Round EO: the rows without a changed field, as rows of the changes
      // frame its split drops ("@@cumcount", counted per id over the union,
      // is NaN for a row without an id: observed black-box, s53_0050).
      if (has_base_rows)
        for (std::size_t index = 0; index < rows.base_rows().size(); ++index) {
          auto row = rows.base_rows()[index];
          if (rows.base_row_ids()[index]) row.push_back({kOcelCumcountColumn, OcelValue{std::int64_t{0}}});
          extra_change_rows.push_back({type, std::move(row)});
        }
      // Round ER: a table without rows too (see the event tables).
      for (const auto& name : rows.column_names()) {
        if (attribute_column(name) && name != "ocel:type") add_column(object_columns, name);
        if (name != "ocel_id") add_column(change_columns_order, name);
        if ((attribute_column(name) && name != "ocel:type") || name == "ocel_id")
          add_column(object_order, own_name(name, false));
      }
      // Round EN: a change row's other cells stay with its change
      // (OcelObjectChange::cells), so the change hint no longer holds them.
      for (const auto& id : rows.ids()) {
        const auto listed = object_types.find(id);
        if (listed == object_types.end() || !read.insert(id).second) continue;
        OcelObject object;
        object.id = id;
        object.type = listed->second;
        if (const auto attributes = object_attributes.find(id);
            attributes != object_attributes.end()) {
          object.attributes = attributes->second;
        } else {
          object.attributes = rows.find(id)->attributes;
        }
        log.object_type_attributes.try_emplace(object.type);
        log.objects.push_back(std::move(object));
      }
    }
  }
  // Round DT: the reference reader concatenates the type tables of the
  // listed types, so an event or object table without rows fails there
  // (observed black-box, probe dt/oc sq.py); round EK: after every type
  // table is read, before the relation tables (which must exist). A type
  // whose rows all lack an id still has its table read there.
  const bool events_read = !event_tables.empty() || (event_type_column.only_nulls() && event_type_map.none_key);
  const bool objects_read =
      !object_tables_by_type.empty() || (object_type_column.only_nulls() && object_type_map.none_key);
  // Round ES: a table holding its time column under both names (ocel_time
  // and ocel:timestamp) repeats a label in the concatenation: pandas raises
  // InvalidIndexError when the tables' columns differ (a reindex), and
  // reading the time column's dtype after it when every table has the same
  // columns; in the object tables a column named "ocel:field" next to a
  // changed-field column (in any table) repeats the field column's renamed
  // label, and the split into objects and changes raises. The event tables
  // first, each after the object tables' own faults (observed black-box,
  // probes es x8 p3.py, p4.py).
  const auto repeated_time_fault = [&](const std::vector<std::vector<TableColumn>>& tables) -> const char* {
    const auto has = [](const std::vector<TableColumn>& columns, std::string_view name) {
      return std::any_of(columns.begin(), columns.end(), [&](const TableColumn& column) { return column.name == name; });
    };
    if (std::none_of(tables.begin(), tables.end(), [&](const auto& columns) {
          return has(columns, "ocel_time") && has(columns, "ocel:timestamp");
        }))
      return nullptr;
    const auto names = [](const std::vector<TableColumn>& columns) {
      std::vector<std::string> result;
      for (const auto& column : columns) result.push_back(column.name);
      return result;
    };
    for (const auto& columns : tables)
      if (names(columns) != names(tables.front()))
        return "InvalidIndexError: Reindexing only valid with uniquely valued Index objects";
    return "AttributeError: 'DataFrame' object has no attribute 'dtype'";
  };
  const bool validating = options.validate_references.value_or(false);
  if (!events_read && !validating) throw std::invalid_argument("ValueError: No objects to concatenate");
  {
    std::vector<std::vector<TableColumn>> tables;
    for (const auto& [activity, table] : event_tables) tables.push_back(table_columns(db, table));
    if (const auto* fault = repeated_time_fault(tables)) throw std::invalid_argument(fault);
  }
  if (!objects_read && !validating) throw std::invalid_argument("ValueError: No objects to concatenate");
  {
    std::vector<std::vector<TableColumn>> tables;
    for (const auto& [type, table] : object_tables_by_type) tables.push_back(table_columns(db, table));
    const auto* fault = repeated_time_fault(tables);
    if (fault != nullptr && std::string_view(fault).starts_with("InvalidIndexError")) throw std::invalid_argument(fault);
    bool field = false, changed_field = false;
    for (const auto& columns : tables)
      for (const auto& column : columns) {
        field = field || column.name == "ocel:field";
        changed_field = changed_field || column.name == "ocel_changed_field";
      }
    if (field && changed_field)
      throw std::invalid_argument("ValueError: cannot reindex on an axis with duplicate labels");
    if (fault != nullptr) throw std::invalid_argument(fault);
  }
  if (events_untimed) throw key_error("['ocel:timestamp'] not in index");
  const bool o2o_qualifier_missing = read_relations(db, log, options.validate_references.value_or(false), native_tables);

  if (native_tables && table_exists(db, "object_changes")) {
    Statement statement(db, "SELECT object_id, object_type, ocel_time, "
                           "changed_field, value_type, text_value, integer_value, "
                           "real_value, bool_value FROM object_changes ORDER BY rowid");
    while (true) {
      const auto status = sqlite3_step(statement.get());
      if (status == SQLITE_DONE) break;
      if (status != SQLITE_ROW)
        throw std::runtime_error("SQLite object change query failed");
      OcelObjectChange change;
      change.object_id = column_string(statement.get(), 0);
      change.object_type = column_string(statement.get(), 1);
      change.timestamp = column_string(statement.get(), 2);
      change.field = column_string(statement.get(), 3);
      change.value = read_value(statement.get(), 4, 5, 6, 7, 8);
      if (options.validate_references.value_or(false) && !object_types.contains(change.object_id))
        throw std::invalid_argument("OCEL SQLite object change references an unknown id");
      log.object_changes.push_back(std::move(change));
    }
  } else {
    std::set<std::string> visited_tables;
    std::set<std::string> object_ids;
    std::unordered_set<std::string> first_rows;
    std::vector<std::pair<std::size_t, std::size_t>> places;
    std::vector<OcelSqliteTime> change_times;
    std::vector<OcelSqliteTimeColumn> change_columns;
    std::unordered_map<std::string, std::int64_t> id_rows;
    std::size_t type_rank = 0;
    // Round ER: change rows while no object table has a time column raise
    // the changes frame's KeyError, after the relation tables (observed
    // black-box, probe er h6: a table without one beside a timed table
    // gives NaT changes).
    bool timed = false, untimed_changes = false;
    for (const auto& [id, type] : object_types) object_ids.insert(id);
    for (const auto& type : object_type_names) {
      const auto table = object_table(type);
      if (visited_tables.insert(table).second && table_exists(db, table)) {
        read_type_table_changes(db, table, type, log, object_ids,
                                options.validate_references.value_or(false), has_base_rows,
                                first_rows, type_rank, places, change_times, change_columns, id_rows,
                                timed, untimed_changes, field_alias);
      }
      ++type_rank;
    }
    if (untimed_changes && !timed) throw key_error("ocel:timestamp");
    // The reference frame of the changes takes the tables' rows by their
    // place in the table, then the type, and sorts them by time, stably, as
    // it does the events (round BY, observed black-box); round EL: its time
    // column reads as the event column does (sort_ocel2_sqlite_changes).
    std::vector<std::size_t> order(log.object_changes.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right) {
      return places[left] < places[right];
    });
    std::vector<OcelObjectChange> placed_changes;
    std::vector<OcelSqliteTime> placed_times;
    placed_changes.reserve(order.size());
    placed_times.reserve(order.size());
    for (const auto index : order) {
      placed_changes.push_back(std::move(log.object_changes[index]));
      placed_times.push_back(change_times[index]);
    }
    log.object_changes = std::move(placed_changes);
    // Round EM: the change frame concatenates every table read, those
    // without change rows too, so its dtype is numeric only when every one
    // of them is (observed black-box, probe em ct.py).
    auto change_column = change_columns.empty() ? OcelSqliteTimeColumn::kObject : OcelSqliteTimeColumn::kInteger;
    for (const auto column : change_columns) change_column = std::max(change_column, column);
    sort_ocel2_sqlite_changes(log, placed_times, change_column);
    // A change takes its object's type (the id's last listing in the object
    // table), not its table's: pm4py types the change rows by the object
    // table (observed black-box).
    for (auto& change : log.object_changes)
      if (const auto listed = object_types.find(change.object_id); listed != object_types.end())
        change.object_type = listed->second;
  }
  if (o2o_qualifier_missing) throw key_error("ocel:qualifier");
  if (native_tables && table_exists(db, "ocel_globals")) {
    Statement statement(db, "SELECT attribute_name, value_type, text_value, "
                           "integer_value, real_value, bool_value FROM ocel_globals "
                           "ORDER BY attribute_name");
    while (true) {
      const auto status = sqlite3_step(statement.get());
      if (status == SQLITE_DONE) break;
      if (status != SQLITE_ROW)
        throw std::runtime_error("SQLite globals query failed");
      log.globals[column_string(statement.get(), 0)] =
          read_value(statement.get(), 1, 2, 3, 4, 5);
    }
  }
  // Round EN: the frames' build order (see event_columns), the changes
  // frame's "@@cumcount" last unless a table has one, its field and time
  // columns placed when not after the others.
  const bool attribute_tables =
      native_tables && (table_exists(db, "event_attributes") || table_exists(db, "object_attributes"));
  // Round EQ: a native file (with its attribute tables) is read by the
  // reference through its type tables too, so its frames take their
  // columns in the type tables' union order, not in the attribute tables'
  // first appearance (observed black-box: a CSV log with its attribute
  // columns in file order, written by native and read back, bank b62).
  log.event_frame_hint.columns = event_columns;
  log.object_frame_hint.columns = object_columns;
  if (!attribute_tables) {
    if (!log.object_changes.empty() && !(native_tables && table_exists(db, "object_changes"))) {
      auto& hint = log.change_frame_hint;
      hint.columns.clear();
      std::optional<std::size_t> field_at, time_at;
      for (std::size_t at = 0; at < change_columns_order.size(); ++at) {
        const auto& name = change_columns_order[at];
        if (name == "ocel_changed_field") field_at = at;
        else if (name == "ocel_time") time_at = at;
        else hint.columns.push_back(name);
      }
      if (std::find(hint.columns.begin(), hint.columns.end(), kOcelCumcountColumn) == hint.columns.end())
        hint.columns.emplace_back(kOcelCumcountColumn);
      if (field_at && time_at) {
        hint.field_at = field_at;
        hint.time_at = time_at;
        const auto natural = ocel_frame::change_frame_order(OcelFrameHint{hint.columns, {}, 0}, hint.columns);
        const auto placed = ocel_frame::change_frame_order(hint, hint.columns);
        if (natural == placed) {
          hint.field_at.reset();
          hint.time_at.reset();
        }
      }
    }
  }
  if (time_column) sort_ocel2_sqlite_events(log, event_times, *time_column);
  else sort_ocel_events_by_timestamp(log, true);
  // Round EM: the frames' hints of the rows read but not kept, then of the
  // drops below.
  if (!attribute_tables) {
    ocel_frame::hint_extra_rows(log, extra_event_rows, extra_object_rows, extra_change_values,
                                log.object_changes.empty() ? std::vector<ocel_frame::HintRow>{} : extra_change_rows);
    ocel_frame::hint_none_columns(log.event_frame_hint, null_event_columns);
    ocel_frame::hint_none_columns(log.object_frame_hint, null_object_columns);
    if (!log.object_changes.empty()) ocel_frame::hint_none_columns(log.change_frame_hint, null_object_columns);
  }
  // The reference SQLite reader drops what names an empty id or type, then
  // the events and objects without relations. It finds them in the event
  // and object tables, before it reads the type tables: an event whose only
  // object has no row in its type table stays, without that relation, and
  // likewise an object (observed black-box).
  drop_empty_ocel_names(log);
  std::unordered_set<std::string> related_events;
  std::unordered_set<std::string> related_objects;
  for (const auto& relation : log.relations) {
    if (!event_types.contains(relation.event_id) || !object_types.contains(relation.object_id))
      continue;
    related_events.insert(relation.event_id);
    related_objects.insert(relation.object_id);
  }
  // With no event or no object left, the reference reader keeps neither
  // (observed black-box).
  const bool keep_none =
      std::none_of(log.events.begin(), log.events.end(),
                   [&](const OcelEvent& event) { return related_events.contains(event.id); }) ||
      std::none_of(log.objects.begin(), log.objects.end(),
                   [&](const OcelObject& object) { return related_objects.contains(object.id); });
  // Round EM: the frames keep what the dropped rows add (OcelFrameHint).
  ocel_frame::hint_dropped_rows(
      log, [&](const OcelEvent& event) { return keep_none || !related_events.contains(event.id); },
      [&](const OcelObject& object) { return keep_none || !related_objects.contains(object.id); },
      [&](const OcelObjectChange& change) { return keep_none || !related_objects.contains(change.object_id); });
  std::erase_if(log.events, [&](const OcelEvent& event) { return !related_events.contains(event.id); });
  std::erase_if(log.objects, [&](const OcelObject& object) { return !related_objects.contains(object.id); });
  if (log.events.empty() || log.objects.empty()) {
    log.events.clear();
    log.objects.clear();
  }
  std::unordered_set<std::string> kept_events;
  std::unordered_set<std::string> kept_objects;
  for (const auto& event : log.events) kept_events.insert(event.id);
  for (const auto& object : log.objects) kept_objects.insert(object.id);
  std::erase_if(log.relations, [&](const OcelRelation& relation) {
    return !kept_events.contains(relation.event_id) || !kept_objects.contains(relation.object_id);
  });
  std::erase_if(log.o2o, [&](const OcelO2oRelation& relation) {
    return !kept_objects.contains(relation.source_id) || !kept_objects.contains(relation.target_id);
  });
  std::erase_if(log.e2e, [&](const OcelE2eRelation& relation) {
    return !kept_events.contains(relation.source_id) || !kept_events.contains(relation.target_id);
  });
  std::erase_if(log.object_changes, [&](const OcelObjectChange& change) {
    return !kept_objects.contains(change.object_id);
  });
  // Round EN: no hint where the kept rows give the same frame.
  {
    ocel_frame::Columns events, objects, changes;
    for (const auto& event : log.events) events.observe_row(event.attributes);
    for (const auto& object : log.objects) objects.observe_row(object.attributes);
    for (const auto& change : log.object_changes) changes.observe_change(change);
    ocel_frame::trim_frame_hint(log.event_frame_hint, events.names());
    ocel_frame::trim_frame_hint(log.object_frame_hint, objects.names());
    ocel_frame::trim_frame_hint(log.change_frame_hint, changes.names());
    // Round EP: read_ocel2_sqlite's frames hold the type tables' columns in
    // that union order, ocel_id and ocel_time renamed, "ocel:activity" last
    // when no table has it, and ocel:type last in the objects frame;
    // write_ocel_csv and the SQLite writer's type tables follow it
    // (observed black-box, bank b53, probe ep hc/sq.py). A native file's
    // type tables hold its frames' order too.
    {
      ocel_frame::hint_reserved_order(
          log.event_frame_hint, event_order, {"ocel:eid", "ocel:activity", "ocel:timestamp"},
          log.event_activity_first
              ? std::vector<std::string>{"ocel:eid", "ocel:activity", "ocel:timestamp"}
              : std::vector<std::string>{"ocel:eid", "ocel:timestamp", "ocel:activity"},
          events.names());
      ocel_frame::hint_reserved_order(log.object_frame_hint, object_order, {"ocel:oid", "ocel:type"},
                                      {"ocel:oid", "ocel:type"}, objects.names());
    }
  }
  // Round ES: read_ocel2_sqlite loses an event table's "@@index" column (see
  // drop_reader_event_index).
  ocel_frame::drop_reader_event_index(log);
  return log;
#endif
}

void export_ocel_sqlite(const Ocel& input, const std::filesystem::path& path,
                        const OcelSqliteOptions& options) {
  // Round DV: it selects the events frame's id and activity first (an empty
  // playout has no column), and the relations' event and object ids after
  // the objects (a degenerate relations frame, black-box).
  if (ocel_events_frame_columnless(input))
    throw std::invalid_argument(
        "KeyError: \"None of [Index(['ocel:eid', 'ocel:activity'], dtype='str')] are in the [columns]\"");
  // Round DT: the reference writer selects the objects frame's id and type.
  require_ocel_object_columns(input);
  if (ocel_relations_frame_degenerate(input))
    throw std::invalid_argument("KeyError: \"['ocel:eid', 'ocel:oid'] not in index\"");
  // Round EH: without the events and objects that have no relation.
  std::optional<Ocel> written;
  const Ocel& log = ocel_written_entries(input, written);
#if !defined(MINERA_HAS_SQLITE)
  (void)log;
  (void)path;
  (void)options;
  throw std::runtime_error("SQLite support is not available in this build");
#else
  validate_log(log, options.validate_references.value_or(true));
  // Round EG: the reference drops a type table's columns without a value,
  // so an event type whose times are all NaT (an OCEL 2.0 SQLite '' or NULL
  // time) loses its time column and raises (observed black-box; it has
  // written other tables by then, native writes no file).
  {
    // Round EL: a raw NaN counts (a raw text does not), and so does an
    // object type whose written changes all lack a time (observed
    // black-box, probe el wo.py).
    const auto nat = [](OcelRawTime kind, const std::string& text) {
      return ocel_raw_time_missing(kind) || (kind == OcelRawTime::kNone && ocel_frame::nat_time_text(text));
    };
    std::map<std::string_view, bool> all_nat;
    for (const auto& event : log.events) {
      const auto [entry, added] = all_nat.try_emplace(event.activity, true);
      entry->second = entry->second && nat(event.raw_time, event.timestamp);
    }
    // Each type table in turn: a raw table raises its conversion's error
    // (see coerced_raw_time_texts) before a later table's KeyError.
    const auto raw_check = [](const auto& rows, std::string_view owner, const auto& owner_of) {
      std::vector<std::pair<OcelRawTime, std::string_view>> table;
      bool raw = false;
      for (const auto& row : rows)
        if (owner_of(row) == owner) {
          table.emplace_back(row.raw_time, row.timestamp);
          raw = raw || row.raw_time != OcelRawTime::kNone;
        }
      if (raw) (void)coerced_raw_time_texts(table);
    };
    // Round ES: a type table's columns renamed, an attribute named ocel_time
    // or ocel_id with a value in the type repeats its time or id column:
    // pandas raises converting the time column (a frame then), and reading
    // the id column's dtype after it; the change rows' columns raise at the
    // conversion (a change of ocel_time) or at their concatenation to the
    // object rows (a change of ocel_id or "ocel_changed_field", an object
    // attribute ocel_id), an object type without change rows at the dtype
    // of an object attribute ocel_id (event types, then object types, each
    // in sorted order; observed black-box, probes es x5 p4.py, x7).
    for (const auto& [activity, missing] : all_nat) {
      if (missing) throw std::invalid_argument("KeyError: 'ocel_time'");
      bool time_column = false, id_column = false;
      for (const auto& event : log.events) {
        if (event.activity != activity) continue;
        time_column = time_column || has_frame_value(event.attributes, "ocel_time");
        id_column = id_column || has_frame_value(event.attributes, "ocel_id");
      }
      if (time_column) throw std::invalid_argument(kAssembleDuplicateKeys);
      raw_check(log.events, activity, [](const OcelEvent& event) -> std::string_view { return event.activity; });
      if (id_column) throw std::invalid_argument(kFrameHasNoDtype);
    }
    std::unordered_map<std::string_view, std::string_view> type_of;
    for (const auto& object : log.objects) type_of.emplace(object.id, object.type);
    std::map<std::string, bool> all_nat_changes;
    for (const auto& change : log.object_changes) {
      std::string type = change.object_type;
      if (type.empty()) {
        const auto found = type_of.find(change.object_id);
        if (found == type_of.end()) continue;
        type = std::string(found->second);
      }
      const auto [entry, added] = all_nat_changes.try_emplace(type, true);
      entry->second = entry->second && nat(change.raw_time, change.timestamp);
    }
    std::set<std::string> object_types;
    for (const auto& [type, missing] : all_nat_changes) object_types.insert(type);
    for (const auto& object : log.objects) object_types.insert(object.type);
    for (const auto& type : object_types) {
      const bool id_column = std::any_of(log.objects.begin(), log.objects.end(), [&](const OcelObject& object) {
        return object.type == type && has_frame_value(object.attributes, "ocel_id");
      });
      const auto changed = all_nat_changes.find(type);
      if (changed == all_nat_changes.end()) {
        if (id_column) throw std::invalid_argument(kFrameHasNoDtype);
        continue;
      }
      if (changed->second) throw std::invalid_argument("KeyError: 'ocel_time'");
      std::vector<OcelObjectChange> rows;
      for (const auto& change : log.object_changes) {
        std::string_view owner = change.object_type;
        if (owner.empty())
          if (const auto found = type_of.find(change.object_id); found != type_of.end()) owner = found->second;
        if (owner == type) rows.push_back(change);
      }
      const auto changes_field = [&](std::string_view field) {
        return std::any_of(rows.begin(), rows.end(), [&](const OcelObjectChange& change) {
          return change.field == field && frame_value(change.value);
        });
      };
      if (changes_field("ocel_time")) throw std::invalid_argument(kAssembleDuplicateKeys);
      raw_check(rows, std::string_view{}, [](const OcelObjectChange&) -> std::string_view { return {}; });
      if (id_column || changes_field("ocel_id") || changes_field("ocel_changed_field"))
        throw std::invalid_argument(kReindexNotUnique);
    }
  }
  std::error_code error;
  std::filesystem::remove(path, error);
  if (error) throw std::runtime_error("cannot replace SQLite output: " + error.message());
  Database database(path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                             SQLITE_OPEN_FULLMUTEX);
  sqlite3* db = database.get();
  database.exec("PRAGMA foreign_keys=OFF; BEGIN IMMEDIATE;");
  const char* relation_qualifier_type = qualifier_column_type(log.relations);
  const char* o2o_qualifier_type = qualifier_column_type(log.o2o);
  try {
    database.exec(
        "CREATE TABLE event (ocel_id TEXT PRIMARY KEY, ocel_type TEXT NOT NULL);"
        "CREATE TABLE object (ocel_id TEXT PRIMARY KEY, ocel_type TEXT NOT NULL);"
        // Round EO: the map of a frame without rows is float64 there, so
        // write_ocel2_sqlite declares its columns REAL (observed black-box:
        // c56_0010, a log without relations).
        + std::string(log.events.empty()
                          ? "CREATE TABLE event_map_type (ocel_type REAL PRIMARY KEY, ocel_type_map REAL NOT NULL);"
                          : "CREATE TABLE event_map_type (ocel_type TEXT PRIMARY KEY, ocel_type_map TEXT NOT NULL);") +
        (log.objects.empty()
             ? "CREATE TABLE object_map_type (ocel_type REAL PRIMARY KEY, ocel_type_map REAL NOT NULL);"
             : "CREATE TABLE object_map_type (ocel_type TEXT PRIMARY KEY, ocel_type_map TEXT NOT NULL);") +
        "CREATE TABLE event_type_attributes (ocel_type TEXT, attribute_name TEXT, attribute_type TEXT, PRIMARY KEY(ocel_type, attribute_name));"
        "CREATE TABLE object_type_attributes (ocel_type TEXT, attribute_name TEXT, attribute_type TEXT, PRIMARY KEY(ocel_type, attribute_name));"
        + std::string(log.untyped_relations && log.relations.empty()
                          ? "CREATE TABLE event_object (ocel_event_id REAL, ocel_object_id REAL, ocel_qualifier REAL);"
                          : std::string("CREATE TABLE event_object (ocel_event_id TEXT, ocel_object_id TEXT, ocel_qualifier ") +
                                relation_qualifier_type + ");") +
        (log.untyped_o2o && log.o2o.empty()
             ? "CREATE TABLE object_object (ocel_source_id REAL, ocel_target_id REAL, ocel_qualifier REAL);"
             : std::string("CREATE TABLE object_object (ocel_source_id TEXT, ocel_target_id TEXT, ocel_qualifier ") +
                   o2o_qualifier_type + ");") +
        "CREATE TABLE event_event (source_event_id TEXT, target_event_id TEXT, qualifier TEXT);"
        "CREATE TABLE event_attributes (ocel_id TEXT, attribute_name TEXT, value_type TEXT, text_value TEXT, integer_value INTEGER, real_value REAL, bool_value INTEGER, ordinal INTEGER);"
        "CREATE TABLE object_attributes (ocel_id TEXT, attribute_name TEXT, value_type TEXT, text_value TEXT, integer_value INTEGER, real_value REAL, bool_value INTEGER, ordinal INTEGER);"
        "CREATE TABLE object_changes (object_id TEXT, object_type TEXT, ocel_time TEXT, changed_field TEXT, value_type TEXT, text_value TEXT, integer_value INTEGER, real_value REAL, bool_value INTEGER);"
        "CREATE TABLE ocel_globals (attribute_name TEXT PRIMARY KEY, value_type TEXT, text_value TEXT, integer_value INTEGER, real_value REAL, bool_value INTEGER);");

    // Rows in log order, as the reference writes its frames; the importer
    // keeps table order among equal timestamps.
    std::vector<const OcelEvent*> events;
    for (const auto& event : log.events) events.push_back(&event);
    std::vector<const OcelObject*> objects;
    for (const auto& object : log.objects) objects.push_back(&object);
    {
      Statement statement(db, "INSERT INTO event(ocel_id, ocel_type) VALUES(?1,?2)");
      for (const auto* event : events) {
        bind_text(statement.get(), 1, event->id);
        bind_text(statement.get(), 2, event->activity);
        step_done(db, statement.get());
      }
    }
    {
      Statement statement(db, "INSERT INTO object(ocel_id, ocel_type) VALUES(?1,?2)");
      for (const auto* object : objects) {
        bind_text(statement.get(), 1, object->id);
        bind_text(statement.get(), 2, object->type);
        step_done(db, statement.get());
      }
    }
    std::set<std::string> activities;
    std::set<std::string> object_types;
    // Round EN: write_ocel2_sqlite writes the types of the frames' rows
    // alone: a declared type without events or objects (an OCEL 2.0 JSON or
    // XML type list) has no map row and no table (observed black-box, probe
    // en o/osw.py; the catalog tables below keep its declaration).
    for (const auto* event : events) activities.insert(event->activity);
    for (const auto* object : objects) object_types.insert(object->type);
    {
      Statement statement(db, "INSERT INTO event_map_type(ocel_type, ocel_type_map) VALUES(?1,?2)");
      for (const auto& activity : activities) {
        bind_text(statement.get(), 1, activity);
        bind_text(statement.get(), 2, type_table_token(activity, options));
        step_done(db, statement.get());
      }
    }
    {
      Statement statement(db, "INSERT INTO object_map_type(ocel_type, ocel_type_map) VALUES(?1,?2)");
      for (const auto& type : object_types) {
        bind_text(statement.get(), 1, type);
        bind_text(statement.get(), 2, type_table_token(type, options));
        step_done(db, statement.get());
      }
    }
    {
      Statement statement(db, "INSERT INTO event_type_attributes(ocel_type, attribute_name, attribute_type) VALUES(?1,?2,?3)");
      for (const auto& [activity, schema] : log.event_type_attributes) {
        for (const auto& [name, type] : schema) {
          bind_text(statement.get(), 1, activity);
          bind_text(statement.get(), 2, name);
          bind_text(statement.get(), 3, type);
          step_done(db, statement.get());
        }
      }
    }
    {
      Statement statement(db, "INSERT INTO object_type_attributes(ocel_type, attribute_name, attribute_type) VALUES(?1,?2,?3)");
      for (const auto& [type_name, schema] : log.object_type_attributes) {
        for (const auto& [name, type] : schema) {
          bind_text(statement.get(), 1, type_name);
          bind_text(statement.get(), 2, name);
          bind_text(statement.get(), 3, type);
          step_done(db, statement.get());
        }
      }
    }
    // Type tables as the reference writes them from its frames (pandas
    // to_sql): one column per attribute with a value in the type, in global
    // column order, typed by the frame column's dtype (an integer column with
    // a missing cell is float64, so REAL; booleans are INTEGER 0/1; an object
    // column by the kinds of its values in the table). Timestamps are written
    // as pandas prints them. The exact typed values and declared schemas live
    // in the attribute and catalog tables above and below.
    // An attribute the owner's catalog declares a date is a TIMESTAMP
    // column, as the reference writes a datetime64 frame column.
    const auto table_columns_for = [](const ocel_frame::Columns& frame,
                                      const ocel_frame::Columns& table_values,
                                      const OcelTypeCatalog& catalog,
                                      const std::string& owner) {
      TypeColumns columns;
      for (const auto& name : frame.names()) {
        // Round ER: an event attribute named "ocel_changed_field" or
        // "@@cumcount" is a column of its table (observed black-box, probe
        // er h10).
        if (name.empty() || name == "ocel_id" || name == "ocel_time" || name == "ocel:activity") continue;
        const auto* stats = table_values.stats(name);
        if (stats == nullptr || stats->present == 0) continue;
        // Round EK: TIMESTAMP for a datetime64 column (date values alone),
        // TEXT for dates next to other values (sql_type).
        (void)catalog;
        (void)owner;
        columns.emplace_back(name, ocel_frame::sql_type(frame.dtype(name), *stats, table_values.rows()));
      }
      return columns;
    };
    // The INSERT binds leading, columns, trailing in that order; the table
    // holds them in `order` (their names; empty: the same order).
    const auto create_table = [&](const std::string& table,
                                  const std::vector<std::string>& leading,
                                  const TypeColumns& columns,
                                  const std::vector<std::string>& trailing,
                                  const std::vector<std::string>& order = {}) {
      std::vector<std::pair<std::string, std::string>> bound;
      for (const auto& name : leading) bound.emplace_back(name, name == "ocel_time" ? "TIMESTAMP" : "TEXT");
      for (const auto& [name, type] : columns) bound.emplace_back(name, type);
      for (const auto& name : trailing) bound.emplace_back(name, name == "ocel_time" ? "TIMESTAMP" : "TEXT");
      return create_table_sql(database, table, bound, order);
    };
    // pm4py.write_ocel2_sqlite writes each time through a Python datetime:
    // nanosecond digits are cut to the microsecond (round BW, observed
    // black-box on event, change and date attribute times).
    const auto pandas_time = [](const std::string& text) {
      return ocel_frame::utc_timestamp_text(text, ' ', "+00:00", true);
    };
    // A CSV log whose times share an offset is written in that offset, as
    // the reference writes such a column (see shared_nonzero_offset).
    const auto event_offset = [&] {
      std::vector<std::string_view> texts;
      texts.reserve(events.size());
      for (const auto* event : events) texts.emplace_back(event->timestamp);
      return ocel_frame::shared_nonzero_offset(texts);
    }();
    ocel_frame::Columns event_frame;
    event_frame.seed(log.event_frame_hint);  // round EM: see OcelFrameHint
    std::map<std::string, std::vector<const OcelEvent*>> events_by_activity;
    for (const auto* event : events) {
      event_frame.observe_row(event->attributes, log.event_type_attributes, event->activity);
      events_by_activity[event->activity].push_back(event);
    }
    std::unordered_map<const OcelEvent*, std::optional<std::string>> coerced_event_times;
    {
      std::vector<std::string_view> texts;
      texts.reserve(events.size());
      for (const auto* event : events) texts.emplace_back(event->timestamp);
      const bool raw = std::any_of(events.begin(), events.end(),
                                   [](const OcelEvent* event) { return event->raw_time != OcelRawTime::kNone; });
      if (raw)
        for (const auto& [activity, rows] : events_by_activity) {
          std::vector<std::pair<OcelRawTime, std::string_view>> table;
          for (const auto* event : rows) table.emplace_back(event->raw_time, event->timestamp);
          const auto coerced = coerced_raw_time_texts(table);
          for (std::size_t i = 0; i < rows.size(); ++i) coerced_event_times.emplace(rows[i], coerced[i]);
        }
      else if (text_time_column(texts))
        for (const auto& [activity, rows] : events_by_activity) {
          std::vector<std::string_view> table;
          table.reserve(rows.size());
          for (const auto* event : rows) table.emplace_back(event->timestamp);
          if (const auto coerced = coerced_time_texts(table))
            for (std::size_t i = 0; i < rows.size(); ++i) coerced_event_times.emplace(rows[i], (*coerced)[i]);
        }
    }
    for (const auto& activity : activities) {
      const auto table = "event_" + type_table_token(activity, options);
      const auto& rows = events_by_activity[activity];
      ocel_frame::Columns table_values;
      for (const auto* event : rows) table_values.observe_row(event->attributes, log.event_type_attributes, activity);
      const auto columns =
          table_columns_for(event_frame, table_values, log.event_type_attributes, activity);
      // The reference frame's column order (activity first after XML;
      // round EP: the frame's own columns where OcelFrameHint::reserved
      // places them, observed black-box).
      const bool activity_first = log.event_activity_first;
      std::vector<std::string> order;
      if (!log.event_frame_hint.reserved.empty()) {
        std::vector<std::string> names;
        for (const auto& column : columns) names.push_back(column.first);
        for (auto& name : ocel_frame::frame_column_order(
                 log.event_frame_hint,
                 activity_first ? std::vector<std::string>{"ocel:eid", "ocel:activity", "ocel:timestamp"}
                                : std::vector<std::string>{"ocel:eid", "ocel:timestamp", "ocel:activity"},
                 names))
          order.push_back(name == "ocel:eid" ? "ocel_id" : name == "ocel:timestamp" ? "ocel_time" : name);
      }
      Statement statement(db, create_table(table,
                                           activity_first
                                               ? std::vector<std::string>{"ocel_id", "ocel:activity", "ocel_time"}
                                               : std::vector<std::string>{"ocel_id", "ocel_time", "ocel:activity"},
                                           columns, {}, order));
      for (const auto* event : rows) {
        bind_text(statement.get(), 1, event->id);
        // A NaT time is NULL there, as pandas writes a missing cell.
        if (const auto coerced = coerced_event_times.find(event); coerced != coerced_event_times.end()) {
          if (coerced->second) bind_text(statement.get(), activity_first ? 3 : 2, *coerced->second);
          else bind_null(statement.get(), activity_first ? 3 : 2);
        } else if (ocel_frame::nat_time_text(event->timestamp))
          bind_null(statement.get(), activity_first ? 3 : 2);
        else
          bind_text(statement.get(), activity_first ? 3 : 2,
                    event_offset ? ocel_frame::offset_timestamp_text(event->timestamp, ' ',
                                                                     event_offset, std::nullopt, true)
                                 : pandas_time(event->timestamp));
        bind_text(statement.get(), activity_first ? 2 : 3, event->activity);
        bind_type_attributes(statement.get(), 4, columns, event->attributes, log.event_type_attributes, activity);
        step_done(db, statement.get());
      }
    }
    std::unordered_map<std::string_view, std::string_view> object_id_types;
    ocel_frame::Columns object_frame;
    object_frame.seed(log.object_frame_hint);  // round EM: see OcelFrameHint
    std::map<std::string, std::vector<const OcelObject*>> objects_by_type;
    for (const auto* object : objects) {
      object_id_types.emplace(object->id, object->type);
      object_frame.observe_row(object->attributes, log.object_type_attributes, object->type);
      objects_by_type[object->type].push_back(object);
    }
    std::map<std::string, std::vector<const OcelObjectChange*>> changes_by_type;
    for (const auto& change : log.object_changes) {
      // Round EL: a change of the empty field (an OCEL 2.0 SQLite change row
      // with '' there) is written too, with no value (observed black-box).
      std::string resolved_type = change.object_type;
      if (resolved_type.empty()) {
        const auto object_type = object_id_types.find(change.object_id);
        if (object_type == object_id_types.end()) continue;
        resolved_type = std::string(object_type->second);
      }
      changes_by_type[resolved_type].push_back(&change);
    }
    std::unordered_map<const OcelObjectChange*, std::optional<std::string>> coerced_change_times;
    {
      std::vector<std::string_view> texts;
      texts.reserve(log.object_changes.size());
      for (const auto& change : log.object_changes) texts.emplace_back(change.timestamp);
      const bool raw = std::any_of(log.object_changes.begin(), log.object_changes.end(),
                                   [](const OcelObjectChange& change) { return change.raw_time != OcelRawTime::kNone; });
      if (raw)
        for (const auto& [type, rows] : changes_by_type) {
          std::vector<std::pair<OcelRawTime, std::string_view>> table;
          for (const auto* change : rows) table.emplace_back(change->raw_time, change->timestamp);
          // Round EN: a table of datetimes alone (a column read_ocel2_xml
          // left object elsewhere) converts as usual.
          if (std::none_of(rows.begin(), rows.end(),
                           [](const OcelObjectChange* change) { return change->raw_time != OcelRawTime::kNone; }))
            continue;
          auto mixed = coerced_mixed_time_texts(table);
          const auto coerced = mixed ? std::move(*mixed) : coerced_raw_time_texts(table);
          for (std::size_t i = 0; i < rows.size(); ++i) coerced_change_times.emplace(rows[i], coerced[i]);
        }
      else if (text_time_column(texts))
        for (const auto& [type, rows] : changes_by_type) {
          std::vector<std::string_view> table;
          table.reserve(rows.size());
          for (const auto* change : rows) table.emplace_back(change->timestamp);
          if (const auto coerced = coerced_time_texts(table))
            for (std::size_t i = 0; i < rows.size(); ++i) coerced_change_times.emplace(rows[i], (*coerced)[i]);
        }
    }
    // Round EN: pm4py's write_ocel2_sqlite writes a type's table from its
    // object rows and its change rows, each with the columns empty there
    // dropped, concatenated (observed black-box, probes en cc.py, fh.py):
    // the object rows' columns in the objects frame's order, then the change
    // rows' other columns in the changes frame's order (its "ocel:field"
    // and "ocel:timestamp" as ocel_changed_field and ocel_time, a change
    // row's other cells and "@@cumcount" included); a column's dtype is the
    // concatenation of its objects frame and changes frame dtypes (a side
    // without the column adds missing cells), an object column typed by the
    // table's values.
    ocel_frame::Columns change_frame;
    change_frame.seed(log.change_frame_hint);
    {
      std::unordered_map<const OcelObjectChange*, const std::string*> owners;
      for (const auto& [type, rows] : changes_by_type)
        for (const auto* change : rows) owners.emplace(change, &type);
      for (const auto& change : log.object_changes) {
        const auto owner = owners.find(&change);
        if (owner == owners.end()) continue;
        change_frame.add_rows(1);
        change_frame.observe_change(change, log.object_type_attributes, *owner->second);
      }
    }
    const auto change_order = ocel_frame::change_frame_order(log.change_frame_hint, change_frame.names());
    for (const auto& type : object_types) {
      const auto table = "object_" + type_table_token(type, options);
      const auto& rows = objects_by_type[type];
      const auto& changes = changes_by_type[type];
      const bool has_changes = !changes.empty();
      ocel_frame::Columns object_part, change_part;
      for (const auto* object : rows) object_part.observe_row(object->attributes, log.object_type_attributes, type);
      for (const auto* change : changes) {
        change_part.add_rows(1);
        change_part.observe_change(*change, log.object_type_attributes, type);
      }
      const auto present = [](const ocel_frame::Columns& part, const std::string& name) {
        const auto* stats = part.stats(name);
        return stats != nullptr && stats->present != 0;
      };
      // The table's columns: name, SQL type ("" for the change field and
      // time columns).
      std::vector<std::pair<std::string, std::string>> layout;
      const auto add_attribute = [&](const std::string& name) {
        const bool in_objects = present(object_part, name);
        const bool in_changes = present(change_part, name);
        ocel_frame::Dtype dtype = in_objects ? object_frame.dtype(name) : ocel_frame::Dtype::kEmpty;
        if (has_changes)
          dtype = ocel_frame::concat_dtype(dtype, in_changes ? change_frame.dtype(name) : ocel_frame::Dtype::kEmpty);
        ocel_frame::ColumnStats values;
        if (const auto* stats = object_part.stats(name)) values.merge(*stats);
        if (const auto* stats = change_part.stats(name)) values.merge(*stats);
        layout.emplace_back(name, ocel_frame::sql_type(dtype, values, rows.size() + changes.size()));
      };
      std::unordered_set<std::string> placed;
      std::vector<std::string> placed_order;
      // Round ES: an object attribute named "ocel_changed_field" is the
      // table's changed-field column: the object rows hold its values, the
      // change rows their fields, at the attribute's place (an object column
      // with change rows; observed black-box, probe es x5).
      bool shared_field_column = false;
      // Round ES: so is an object attribute named ocel_time the table's time
      // column with change rows: the object rows hold its values, the change
      // rows their times, at the attribute's place (pandas concatenates the
      // change rows' converted times to the object rows' column: datetimes
      // alone stay a TIMESTAMP column, next to other values a TEXT one of
      // their isoformats; observed black-box, probe es x7). Without change
      // rows it is a column as any other.
      bool shared_time_column = false;
      for (const auto& name : object_frame.names()) {
        // Round ER: an object attribute "ocel:activity" is a column (probe er h10).
        // Round ES: so is one named "@@cumcount" (probe es x4) or ocel_time.
        const bool attribute_name = !reserved_type_column(name) || name == kOcelCumcountColumn ||
                                    name == "ocel_changed_field" || name == "ocel_time";
        if (!attribute_name || !present(object_part, name)) continue;
        if (name == "ocel_time" && has_changes) {
          ocel_frame::ColumnStats values;
          if (const auto* stats = object_part.stats(name)) values.merge(*stats);
          values.has_aware_date = true;
          layout.emplace_back(name, ocel_frame::sql_type(
                                        ocel_frame::concat_dtype(object_frame.dtype(name), ocel_frame::Dtype::kDatetime),
                                        values, rows.size() + changes.size()));
          shared_time_column = true;
        } else if (name == "ocel_changed_field" && has_changes) {
          ocel_frame::ColumnStats values;
          if (const auto* stats = object_part.stats(name)) values.merge(*stats);
          values.has_string = true;
          layout.emplace_back(name, ocel_frame::sql_type(
                                        ocel_frame::concat_dtype(object_frame.dtype(name), ocel_frame::Dtype::kObject),
                                        values, rows.size() + changes.size()));
          shared_field_column = true;
        } else {
          add_attribute(name);
        }
        placed.insert(name);
        placed_order.push_back(name);
      }
      if (has_changes)
        for (const auto& name : change_order) {
          if (name == ocel_frame::kChangeFieldColumn) {
            if (!shared_field_column) layout.emplace_back("ocel_changed_field", "");
          } else if (name == ocel_frame::kChangeTimeColumn) {
            if (!shared_time_column) layout.emplace_back("ocel_time", "");
          }
          else if (!placed.count(name) && present(change_part, name) &&
                   (name == kOcelCumcountColumn || !reserved_type_column(name)))
            add_attribute(name);
        }
      std::vector<std::pair<std::string, std::string>> bound{{"ocel_id", "TEXT"}};
      for (const auto& [name, type_name] : layout)
        bound.emplace_back(name, name == "ocel_time" && type_name.empty() ? std::string("TIMESTAMP")
                                 : type_name.empty()                     ? std::string("TEXT")
                                                                         : type_name);
      // Round EP: ocel_id where the objects frame holds ocel:oid among the
      // object rows' columns (OcelFrameHint::reserved; ocel:type is not
      // written), observed black-box.
      std::vector<std::string> order;
      if (!log.object_frame_hint.reserved.empty()) {
        std::vector<std::string> names;
        for (const auto& name : placed_order) names.push_back(name);
        for (auto& name : ocel_frame::frame_column_order(log.object_frame_hint, {"ocel:oid", "ocel:type"}, names))
          if (name != "ocel:type") order.push_back(name == "ocel:oid" ? "ocel_id" : name);
        for (std::size_t index = placed_order.size(); index < layout.size(); ++index)
          order.push_back(layout[index].first);
      }
      Statement statement(db, create_table_sql(database, table, bound, order));
      const auto bind_row = [&](const std::string& id, const std::vector<OcelAttribute>& cells,
                                const OcelObjectChange* change) {
        std::unordered_map<std::string_view, const OcelValue*> values;
        for (const auto& cell : cells) values.try_emplace(cell.key, &cell.value);
        bind_text(statement.get(), 1, id);
        for (std::size_t index = 0; index < layout.size(); ++index) {
          const int position = static_cast<int>(index) + 2;
          const auto& [name, type_name] = layout[index];
          if (type_name.empty() && name == "ocel_changed_field") {
            if (change) bind_text(statement.get(), position, change->field);
            else bind_null(statement.get(), position);
            continue;
          }
          if (change && name == "ocel_changed_field") {  // round ES: the shared column
            bind_text(statement.get(), position, change->field);
            continue;
          }
          const bool shared_time = change && shared_time_column && name == "ocel_time";
          if (type_name.empty() || shared_time) {
            if (!change) {
              bind_null(statement.get(), position);
              continue;
            }
            // Round EK: a missing change time and one pm4py could not read
            // (a text left in its change-time column) are written as NULL
            // (observed black-box, probe ek: write_ocel2_sqlite coerces them
            // to NaT).
            // Round ES: in a TEXT column (see shared_time_column) as its
            // isoformat.
            const char separator = shared_time && type_name != "TIMESTAMP" ? 'T' : ' ';
            const auto parsed = parse_timestamp_exact(change->timestamp);
            if (const auto coerced = coerced_change_times.find(change); coerced != coerced_change_times.end()) {
              if (coerced->second) {
                std::string text = *coerced->second;
                if (text.size() > 10 && text[10] == ' ') text[10] = separator;
                bind_text(statement.get(), position, text);
              } else {
                bind_null(statement.get(), position);
              }
            } else if (!parsed || !ocel_frame::civil_timestamp(*parsed))
              bind_null(statement.get(), position);
            else
              bind_text(statement.get(), position,
                        separator == ' ' ? pandas_time(change->timestamp)
                                         : ocel_frame::utc_timestamp_text(change->timestamp, 'T', "+00:00", true));
            continue;
          }
          const auto value = values.find(name);
          bind_type_value(statement.get(), position, name, type_name,
                          value == values.end() ? nullptr : value->second, log.object_type_attributes, type);
        }
        step_done(db, statement.get());
      };
      for (const auto* object : rows) bind_row(object->id, object->attributes, nullptr);
      for (const auto* change : changes) {
        std::vector<OcelAttribute> cells{{change->field, change->value}};
        for (const auto& cell : change->cells)
          if (cell.key != change->field) cells.push_back(cell);
        bind_row(change->object_id, cells, change);
      }
    }
    {
      Statement statement(db, "INSERT INTO event_attributes(ocel_id, attribute_name, value_type, text_value, integer_value, real_value, bool_value, ordinal) VALUES(?1,?2,?8,?3,?4,?5,?6,?9)");
      for (const auto* event : events) {
        for (std::size_t ordinal = 0; ordinal < event->attributes.size(); ++ordinal)
          insert_attribute(db, statement.get(), event->id,
                           event->attributes[ordinal], ordinal);
      }
    }
    {
      Statement statement(db, "INSERT INTO object_attributes(ocel_id, attribute_name, value_type, text_value, integer_value, real_value, bool_value, ordinal) VALUES(?1,?2,?8,?3,?4,?5,?6,?9)");
      for (const auto* object : objects) {
        for (std::size_t ordinal = 0; ordinal < object->attributes.size(); ++ordinal)
          insert_attribute(db, statement.get(), object->id,
                           object->attributes[ordinal], ordinal);
      }
    }
    {
      Statement statement(db, "INSERT INTO event_object(ocel_event_id, ocel_object_id, ocel_qualifier) VALUES(?1,?2,?3)");
      std::vector<const OcelRelation*> relations;
      for (const auto& relation : log.relations) relations.push_back(&relation);
      for (const auto* relation : relations) {
        bind_text(statement.get(), 1, relation->event_id);
        bind_text(statement.get(), 2, relation->object_id);
        bind_qualifier(statement.get(), 3, relation->qualifier, relation->qualifier_kind,
                       relation_qualifier_type);
        step_done(db, statement.get());
      }
    }
    {
      Statement statement(db, "INSERT INTO object_object(ocel_source_id, ocel_target_id, ocel_qualifier) VALUES(?1,?2,?3)");
      std::vector<const OcelO2oRelation*> relations;
      for (const auto& relation : log.o2o) relations.push_back(&relation);
      for (const auto* relation : relations) {
        bind_text(statement.get(), 1, relation->source_id);
        bind_text(statement.get(), 2, relation->target_id);
        bind_qualifier(statement.get(), 3, relation->qualifier, relation->qualifier_kind,
                       o2o_qualifier_type);
        step_done(db, statement.get());
      }
    }
    {
      Statement statement(db, "INSERT INTO event_event(source_event_id, target_event_id, qualifier) VALUES(?1,?2,?3)");
      std::vector<const OcelE2eRelation*> relations;
      for (const auto& relation : log.e2e) relations.push_back(&relation);
      for (const auto* relation : relations) {
        bind_text(statement.get(), 1, relation->source_id);
        bind_text(statement.get(), 2, relation->target_id);
        bind_text(statement.get(), 3, relation->qualifier);
        step_done(db, statement.get());
      }
    }
    {
      Statement statement(db, "INSERT INTO object_changes(object_id, object_type, ocel_time, changed_field, value_type, text_value, integer_value, real_value, bool_value) VALUES(?1,?2,?3,?4,?9,?5,?6,?7,?8)");
      std::map<std::string, std::string> types;
      for (const auto* object : objects) types.emplace(object->id, object->type);
      for (const auto& change : log.object_changes) {
        bind_text(statement.get(), 1, change.object_id);
        bind_text(statement.get(), 2, change.object_type.empty() ? types[change.object_id] : change.object_type);
        bind_text(statement.get(), 3, change.timestamp);
        bind_text(statement.get(), 4, change.field);
        std::string type;
        bind_value(statement.get(), 4, change.value, type);
        bind_text(statement.get(), 9, type);
        step_done(db, statement.get());
      }
    }
    {
      Statement statement(db, "INSERT INTO ocel_globals(attribute_name, value_type, text_value, integer_value, real_value, bool_value) VALUES(?1,?7,?2,?3,?4,?5)");
      for (const auto& [key, value] : log.globals) {
        // The reference OCEL 1.0 writers' own globals (scope defaults,
        // computed log scope) are left out, as the OCEL 2.0 JSON writer does.
        if (ocel_reference_generated_global(key, value)) continue;
        bind_text(statement.get(), 1, key);
        std::string type;
        bind_value(statement.get(), 1, value, type);
        bind_text(statement.get(), 7, type);
        step_done(db, statement.get());
      }
    }
    database.exec("COMMIT;");
  } catch (...) {
    try { database.exec("ROLLBACK;"); } catch (...) {}
    throw;
  }
#endif
}

}  // namespace minera
