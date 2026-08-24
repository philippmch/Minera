#include "minera/ocel_sqlite.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(MINERA_HAS_SQLITE)
#if defined(_WIN32)
#include <winsqlite/winsqlite3.h>
#else
#include <sqlite3.h>
#endif
#endif

namespace minera {
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
    const auto name = path.string();
    const auto status = sqlite3_open_v2(name.c_str(), &db_, flags, nullptr);
    if (status != SQLITE_OK) {
      const std::string message = db_ == nullptr
                                      ? "unable to open database"
                                      : sqlite3_errmsg(db_);
      if (db_ != nullptr) sqlite3_close(db_);
      db_ = nullptr;
      throw std::runtime_error("SQLite open failed: " + message);
    }
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
      throw std::runtime_error("SQLite statement failed: " + message);
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

void step_done(sqlite3* db, sqlite3_stmt* statement) {
  const auto status = sqlite3_step(statement);
  if (status != SQLITE_DONE) {
    throw std::runtime_error("SQLite step failed: " +
                             std::string(sqlite3_errmsg(db)));
  }
  sqlite3_reset(statement);
  sqlite3_clear_bindings(statement);
}

std::string column_string(sqlite3_stmt* statement, int column) {
  const auto* value = sqlite3_column_text(statement, column);
  return value == nullptr
             ? std::string{}
             : std::string(reinterpret_cast<const char*>(value));
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

bool table_exists(sqlite3* db, std::string_view table) {
  Statement statement(
      db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1 LIMIT 1");
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

std::string attribute_declared_type(
    const std::vector<const OcelValue*>& values) {
  bool has_bool = false;
  bool has_integer = false;
  bool has_real = false;
  bool has_string = false;
  for (const auto* value : values) {
    if (std::holds_alternative<bool>(*value)) has_bool = true;
    else if (std::holds_alternative<std::int64_t>(*value)) has_integer = true;
    else if (std::holds_alternative<double>(*value)) has_real = true;
    else if (std::holds_alternative<std::string>(*value)) has_string = true;
  }
  if (has_string || (has_bool && (has_integer || has_real))) return "TEXT";
  if (has_real) return "REAL";
  if (has_integer) return "INTEGER";
  if (has_bool) return "BOOLEAN";
  return "TEXT";
}

using TypeColumns = std::vector<std::pair<std::string, std::string>>;

TypeColumns collect_type_columns(
    const std::vector<const std::vector<OcelAttribute>*>& rows) {
  std::map<std::string, std::vector<const OcelValue*>> values;
  for (const auto* row : rows) {
    for (const auto& attribute : *row) {
      if (!reserved_type_column(attribute.key)) {
        values[attribute.key].push_back(&attribute.value);
      }
    }
  }
  TypeColumns result;
  for (const auto& [key, entries] : values) {
    result.emplace_back(key, attribute_declared_type(entries));
  }
  return result;
}

std::string declared_sql_type(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](const unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  if (value.find("bool") != std::string::npos) return "BOOLEAN";
  if (value.find("int") != std::string::npos ||
      value.find("long") != std::string::npos)
    return "INTEGER";
  if (value.find("float") != std::string::npos ||
      value.find("double") != std::string::npos ||
      value == "real" || value == "number")
    return "REAL";
  return "TEXT";
}

TypeColumns merge_declared_type_columns(
    TypeColumns observed, const OcelTypeSchema& declared) {
  std::map<std::string, std::string> columns;
  for (const auto& [name, type] : declared) {
    if (!reserved_type_column(name)) columns.emplace(name, declared_sql_type(type));
  }
  for (const auto& [name, type] : observed) {
    if (!reserved_type_column(name)) columns.emplace(name, type);
  }
  TypeColumns result;
  result.reserve(columns.size());
  for (const auto& column : columns) result.push_back(column);
  return result;
}

TypeColumns collect_type_columns(
    const std::vector<const std::vector<OcelAttribute>*>& rows,
    const std::vector<const OcelObjectChange*>& changes) {
  std::map<std::string, std::vector<const OcelValue*>> values;
  for (const auto* row : rows) {
    for (const auto& attribute : *row) {
      if (!reserved_type_column(attribute.key)) {
        values[attribute.key].push_back(&attribute.value);
      }
    }
  }
  for (const auto* change : changes) {
    if (!reserved_type_column(change->field)) {
      values[change->field].push_back(&change->value);
    }
  }
  TypeColumns result;
  for (const auto& [key, entries] : values) {
    result.emplace_back(key, attribute_declared_type(entries));
  }
  return result;
}

std::map<std::string, const OcelValue*> attribute_values(
    const std::vector<OcelAttribute>& attributes) {
  std::map<std::string, const OcelValue*> result;
  for (const auto& attribute : attributes) {
    if (!reserved_type_column(attribute.key)) result[attribute.key] = &attribute.value;
  }
  return result;
}

std::string type_table_schema(std::string_view table,
                              const TypeColumns& columns,
                              bool object_table, bool has_changes) {
  std::string sql = "CREATE TABLE " + quote_identifier(table) +
                    " (ocel_id TEXT, ocel_time TIMESTAMP";
  for (const auto& [key, type] : columns) {
    sql += "," + quote_identifier(key) + " " + type;
  }
  if (object_table && has_changes) {
    sql += ",ocel_changed_field TEXT," + quote_identifier("@@cumcount") +
           " INTEGER, PRIMARY KEY(ocel_id," + quote_identifier("@@cumcount") + ")";
  } else {
    sql += ", PRIMARY KEY(ocel_id)";
  }
  sql += ");";
  return sql;
}

std::string type_table_insert_sql(std::string_view table,
                                  const TypeColumns& columns,
                                  bool object_table, bool has_changes) {
  std::vector<std::string> names{"ocel_id", "ocel_time"};
  for (const auto& [key, type] : columns) names.push_back(key);
  if (object_table && has_changes) {
    names.push_back("ocel_changed_field");
    names.push_back("@@cumcount");
  }
  std::string sql = "INSERT INTO " + quote_identifier(table) + "(";
  for (std::size_t index = 0; index < names.size(); ++index) {
    if (index != 0) sql += ",";
    sql += quote_identifier(names[index]);
  }
  sql += ") VALUES(";
  for (std::size_t index = 0; index < names.size(); ++index) {
    if (index != 0) sql += ",";
    sql += "?" + std::to_string(index + 1);
  }
  sql += ")";
  return sql;
}

void bind_type_attributes(sqlite3_stmt* statement, int start_index,
                          const TypeColumns& columns,
                          const std::vector<OcelAttribute>& attributes) {
  const auto values = attribute_values(attributes);
  for (std::size_t index = 0; index < columns.size(); ++index) {
    const auto value = values.find(columns[index].first);
    if (value == values.end()) bind_null(statement, start_index + static_cast<int>(index));
    else bind_scalar(statement, start_index + static_cast<int>(index), *value->second);
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
};

std::vector<TableColumn> table_columns(sqlite3* db, std::string_view table) {
  std::vector<TableColumn> result;
  if (!table_exists(db, table)) return result;
  Statement statement(db, "PRAGMA table_info(" + quote_identifier(table) + ")");
  while (true) {
    const auto status = sqlite3_step(statement.get());
    if (status == SQLITE_DONE) break;
    if (status != SQLITE_ROW)
      throw std::runtime_error("SQLite table-info query failed");
    result.push_back({column_string(statement.get(), 1),
                      column_string(statement.get(), 2)});
  }
  return result;
}

OcelValue read_sqlite_cell(sqlite3_stmt* statement, int column,
                           std::string_view declared_type = {}) {
  std::string type(declared_type);
  std::transform(type.begin(), type.end(), type.begin(),
                 [](const unsigned char value) {
                   return static_cast<char>(std::toupper(value));
                 });
  const bool boolean_type = type.find("BOOL") != std::string::npos;
  const auto sqlite_type = sqlite3_column_type(statement, column);
  if (boolean_type && sqlite_type == SQLITE_INTEGER) {
    return sqlite3_column_int(statement, column) != 0;
  }
  if (boolean_type && sqlite_type == SQLITE_TEXT) {
    auto text = column_string(statement, column);
    std::transform(text.begin(), text.end(), text.begin(),
                   [](const unsigned char value) {
                     return static_cast<char>(std::tolower(value));
                   });
    if (text == "true" || text == "1") return true;
    if (text == "false" || text == "0") return false;
    throw std::invalid_argument("OCEL SQLite boolean attribute is invalid");
  }
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

std::vector<OcelAttribute> read_type_table_attributes(
    sqlite3* db, std::string_view table, const std::string& id) {
  const auto columns = table_columns(db, table);
  if (columns.empty()) return {};
  const auto changed = std::find_if(
      columns.begin(), columns.end(), [](const auto& column) {
        return column.name == "ocel_changed_field";
      });
  const auto cumulative = std::find_if(
      columns.begin(), columns.end(), [](const auto& column) {
        return column.name == "@@cumcount";
      });
  std::string query = "SELECT * FROM " + quote_identifier(table) +
                      " WHERE ocel_id=?1";
  if (changed == columns.end()) {
    query += " ORDER BY rowid LIMIT 1";
  } else {
    query += " ORDER BY CASE WHEN ocel_changed_field IS NULL THEN 0";
    if (cumulative != columns.end()) {
      query += " WHEN " + quote_identifier("@@cumcount") + "=0 THEN 1";
    }
    query += " ELSE 2 END, rowid LIMIT 1";
  }
  Statement statement(db, query);
  bind_text(statement.get(), 1, id);
  const auto status = sqlite3_step(statement.get());
  if (status == SQLITE_DONE) return {};
  if (status != SQLITE_ROW)
    throw std::runtime_error("SQLite type-table attribute query failed");
  std::vector<OcelAttribute> result;
  for (std::size_t index = 0; index < columns.size(); ++index) {
    const auto& column = columns[index];
    if (column.name == "ocel_id" || column.name == "ocel_time" ||
        column.name == "ocel_changed_field" || column.name == "@@cumcount")
      continue;
    if (sqlite3_column_type(statement.get(), static_cast<int>(index)) == SQLITE_NULL)
      continue;
    result.push_back({column.name,
                      read_sqlite_cell(statement.get(), static_cast<int>(index),
                                       column.declared_type)});
  }
  return result;
}

void read_type_table_changes(sqlite3* db, std::string_view table,
                             std::string_view object_type, Ocel& log,
                             const std::set<std::string>& object_ids,
                             bool validate_references) {
  const auto columns = table_columns(db, table);
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
  if (changed == columns.end() || timestamp == columns.end() ||
      id == columns.end()) return;
  bool has_null_change_marker = false;
  {
    Statement base(db, "SELECT 1 FROM " + quote_identifier(table) +
                           " WHERE ocel_changed_field IS NULL LIMIT 1");
    const auto status = sqlite3_step(base.get());
    if (status == SQLITE_ROW) {
      has_null_change_marker = true;
    } else if (status != SQLITE_DONE) {
      throw std::runtime_error("SQLite object base-row query failed");
    }
  }
  Statement statement(db, "SELECT * FROM " + quote_identifier(table) +
                             " ORDER BY rowid");
  const auto id_index = static_cast<int>(id - columns.begin());
  const auto changed_index = static_cast<int>(changed - columns.begin());
  const auto timestamp_index = static_cast<int>(timestamp - columns.begin());
  const auto cumulative = std::find_if(
      columns.begin(), columns.end(), [](const auto& column) {
        return column.name == "@@cumcount";
      });
  const auto cumulative_index = cumulative == columns.end()
                                    ? -1
                                    : static_cast<int>(cumulative - columns.begin());
  while (true) {
    const auto status = sqlite3_step(statement.get());
    if (status == SQLITE_DONE) break;
    if (status != SQLITE_ROW)
      throw std::runtime_error("SQLite object-change query failed");
    if (sqlite3_column_type(statement.get(), changed_index) == SQLITE_NULL)
      continue;
    if (!has_null_change_marker && cumulative_index >= 0 &&
        sqlite3_column_int64(statement.get(), cumulative_index) == 0)
      continue;
    const auto field = column_string(statement.get(), changed_index);
    if (field.empty()) continue;
    const auto object_id = column_string(statement.get(), id_index);
    if (validate_references && !object_ids.contains(object_id))
      throw std::invalid_argument("OCEL SQLite object change references an unknown id");
    const auto value_column = std::find_if(
        columns.begin(), columns.end(), [&](const auto& column) {
          return column.name == field;
        });
    if (value_column == columns.end())
      throw std::invalid_argument("OCEL SQLite object change value column is missing");
    OcelObjectChange change;
    change.object_id = object_id;
    change.object_type = std::string(object_type);
    change.timestamp = column_string(statement.get(), timestamp_index);
    change.field = field;
    change.value = read_sqlite_cell(
        statement.get(), static_cast<int>(value_column - columns.begin()),
        value_column->declared_type);
    log.object_changes.push_back(std::move(change));
  }
}

std::string table_timestamp(sqlite3* db, std::string_view table,
                            const std::string& id) {
  Statement statement(db, "SELECT ocel_time FROM " + quote_identifier(table) +
                             " WHERE ocel_id=?1 LIMIT 1");
  bind_text(statement.get(), 1, id);
  const auto status = sqlite3_step(statement.get());
  if (status == SQLITE_ROW) {
    const auto result = column_string(statement.get(), 0);
    sqlite3_reset(statement.get());
    sqlite3_clear_bindings(statement.get());
    return result;
  }
  if (status == SQLITE_DONE) return {};
  throw std::runtime_error("SQLite timestamp query failed: " +
                           std::string(sqlite3_errmsg(db)));
}

std::map<std::string, std::string> read_type_map(sqlite3* db,
                                                  std::string_view table) {
  std::map<std::string, std::string> result;
  if (!table_exists(db, table)) return result;
  Statement statement(db, "SELECT ocel_type, ocel_type_map FROM " +
                             quote_identifier(table) + " ORDER BY ocel_type");
  while (true) {
    const auto status = sqlite3_step(statement.get());
    if (status == SQLITE_DONE) break;
    if (status != SQLITE_ROW)
      throw std::runtime_error("SQLite type map query failed: " +
                               std::string(sqlite3_errmsg(db)));
    result.emplace(column_string(statement.get(), 0),
                   column_string(statement.get(), 1));
  }
  return result;
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
      schema[column.name] =
          column.declared_type.empty() ? "string"
                                       : declared_sql_type(column.declared_type);
      if (schema[column.name] == "INTEGER") schema[column.name] = "int";
      else if (schema[column.name] == "REAL") schema[column.name] = "float";
      else if (schema[column.name] == "BOOLEAN") schema[column.name] = "boolean";
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

void read_relations(sqlite3* db, Ocel& log, bool validate_references) {
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
  if (table_exists(db, "event_object")) {
    Statement statement(db, "SELECT ocel_event_id, ocel_object_id, "
                           "ocel_qualifier FROM event_object ORDER BY rowid");
    while (true) {
      const auto status = sqlite3_step(statement.get());
      if (status == SQLITE_DONE) break;
      if (status != SQLITE_ROW)
        throw std::runtime_error("SQLite event-object query failed");
      OcelRelation relation{column_string(statement.get(), 0),
                            column_string(statement.get(), 1),
                            column_string(statement.get(), 2)};
      if (validate_references &&
          (!event_ids.contains(relation.event_id) ||
           !object_ids.contains(relation.object_id)))
        throw std::invalid_argument("OCEL SQLite event-object relation references an unknown id");
      log.relations.push_back(std::move(relation));
    }
  }
  if (table_exists(db, "object_object")) {
    Statement statement(db, "SELECT ocel_source_id, ocel_target_id, "
                           "ocel_qualifier FROM object_object ORDER BY rowid");
    while (true) {
      const auto status = sqlite3_step(statement.get());
      if (status == SQLITE_DONE) break;
      if (status != SQLITE_ROW)
        throw std::runtime_error("SQLite object-object query failed");
      OcelO2oRelation relation{column_string(statement.get(), 0),
                               column_string(statement.get(), 1),
                               column_string(statement.get(), 2)};
      if (validate_references &&
          (!object_ids.contains(relation.source_id) ||
           !object_ids.contains(relation.target_id)))
        throw std::invalid_argument("OCEL SQLite object-object relation references an unknown id");
      log.o2o.push_back(std::move(relation));
    }
  }
  if (table_exists(db, "event_event")) {
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
  if (!table_exists(db, "event") || !table_exists(db, "object"))
    throw std::invalid_argument("OCEL SQLite database requires event and object tables");

  Ocel log;
  std::map<std::string, std::string> event_types;
  std::map<std::string, std::string> object_types;
  {
    Statement statement(db, "SELECT ocel_id, ocel_type FROM event ORDER BY rowid");
    while (true) {
      const auto status = sqlite3_step(statement.get());
      if (status == SQLITE_DONE) break;
      if (status != SQLITE_ROW)
        throw std::runtime_error("SQLite event query failed");
      const auto id = column_string(statement.get(), 0);
      const auto activity = column_string(statement.get(), 1);
      if (id.empty() || !event_types.emplace(id, activity).second)
        throw std::invalid_argument("OCEL SQLite event table contains duplicate or empty ids");
    }
  }
  {
    Statement statement(db, "SELECT ocel_id, ocel_type FROM object ORDER BY rowid");
    while (true) {
      const auto status = sqlite3_step(statement.get());
      if (status == SQLITE_DONE) break;
      if (status != SQLITE_ROW)
        throw std::runtime_error("SQLite object query failed");
      const auto id = column_string(statement.get(), 0);
      const auto type = column_string(statement.get(), 1);
      if (id.empty() || !object_types.emplace(id, type).second)
        throw std::invalid_argument("OCEL SQLite object table contains duplicate or empty ids");
    }
  }
  const auto event_map = read_type_map(db, "event_map_type");
  const auto object_map = read_type_map(db, "object_map_type");
  read_type_catalog(db, event_map, "event_", log.event_type_attributes);
  read_type_catalog(db, object_map, "object_", log.object_type_attributes);
  read_declared_type_catalog(db, "event_type_attributes",
                             log.event_type_attributes);
  read_declared_type_catalog(db, "object_type_attributes",
                             log.object_type_attributes);
  std::map<std::string, std::vector<OcelAttribute>> event_attributes;
  std::map<std::string, std::vector<OcelAttribute>> object_attributes;
  read_attributes(db, "event_attributes", event_attributes);
  read_attributes(db, "object_attributes", object_attributes);

  for (const auto& [id, activity] : event_types) {
    auto mapped = event_map.find(activity);
    const auto table = mapped == event_map.end()
                           ? "event_" + type_table_token(activity, options)
                           : "event_" + mapped->second;
    OcelEvent event;
    event.id = id;
    event.activity = activity;
    if (table_exists(db, table)) event.timestamp = table_timestamp(db, table, id);
    if (const auto attributes = event_attributes.find(id);
        attributes != event_attributes.end()) {
      event.attributes = attributes->second;
    } else if (table_exists(db, table)) {
      event.attributes = read_type_table_attributes(db, table, id);
    }
    log.events.push_back(std::move(event));
    log.event_type_attributes.try_emplace(activity);
  }
  for (const auto& [id, type] : object_types) {
    OcelObject object;
    object.id = id;
    object.type = type;
    const auto mapped = object_map.find(type);
    const auto table = mapped == object_map.end()
                           ? type_table_token(type, options)
                           : "object_" + mapped->second;
    if (const auto attributes = object_attributes.find(id);
        attributes != object_attributes.end()) {
      object.attributes = attributes->second;
    } else if (table_exists(db, table)) {
      object.attributes = read_type_table_attributes(db, table, id);
    }
    log.objects.push_back(std::move(object));
    log.object_type_attributes.try_emplace(type);
  }
  read_relations(db, log, options.validate_references);

  if (table_exists(db, "object_changes")) {
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
      if (options.validate_references && !object_types.contains(change.object_id))
        throw std::invalid_argument("OCEL SQLite object change references an unknown id");
      log.object_changes.push_back(std::move(change));
    }
  } else {
    std::set<std::string> visited_tables;
    std::set<std::string> object_ids;
    for (const auto& [id, type] : object_types) object_ids.insert(id);
    for (const auto& [id, type] : object_types) {
      const auto mapped = object_map.find(type);
      const auto table = mapped == object_map.end()
                             ? "object_" + type_table_token(type, options)
                             : "object_" + mapped->second;
      if (visited_tables.insert(table).second && table_exists(db, table)) {
        read_type_table_changes(db, table, type, log, object_ids,
                                options.validate_references);
      }
    }
  }
  if (table_exists(db, "ocel_globals")) {
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
  return log;
#endif
}

void export_ocel_sqlite(const Ocel& log, const std::filesystem::path& path,
                        const OcelSqliteOptions& options) {
#if !defined(MINERA_HAS_SQLITE)
  (void)log;
  (void)path;
  (void)options;
  throw std::runtime_error("SQLite support is not available in this build");
#else
  validate_log(log, options.validate_references);
  std::error_code error;
  std::filesystem::remove(path, error);
  if (error) throw std::runtime_error("cannot replace SQLite output: " + error.message());
  Database database(path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                             SQLITE_OPEN_FULLMUTEX);
  sqlite3* db = database.get();
  database.exec("PRAGMA foreign_keys=OFF; BEGIN IMMEDIATE;");
  try {
    database.exec(
        "CREATE TABLE event (ocel_id TEXT PRIMARY KEY, ocel_type TEXT NOT NULL);"
        "CREATE TABLE object (ocel_id TEXT PRIMARY KEY, ocel_type TEXT NOT NULL);"
        "CREATE TABLE event_map_type (ocel_type TEXT PRIMARY KEY, ocel_type_map TEXT NOT NULL);"
        "CREATE TABLE object_map_type (ocel_type TEXT PRIMARY KEY, ocel_type_map TEXT NOT NULL);"
        "CREATE TABLE event_type_attributes (ocel_type TEXT, attribute_name TEXT, attribute_type TEXT, PRIMARY KEY(ocel_type, attribute_name));"
        "CREATE TABLE object_type_attributes (ocel_type TEXT, attribute_name TEXT, attribute_type TEXT, PRIMARY KEY(ocel_type, attribute_name));"
        "CREATE TABLE event_object (ocel_event_id TEXT, ocel_object_id TEXT, ocel_qualifier TEXT);"
        "CREATE TABLE object_object (ocel_source_id TEXT, ocel_target_id TEXT, ocel_qualifier TEXT);"
        "CREATE TABLE event_event (source_event_id TEXT, target_event_id TEXT, qualifier TEXT);"
        "CREATE TABLE event_attributes (ocel_id TEXT, attribute_name TEXT, value_type TEXT, text_value TEXT, integer_value INTEGER, real_value REAL, bool_value INTEGER, ordinal INTEGER);"
        "CREATE TABLE object_attributes (ocel_id TEXT, attribute_name TEXT, value_type TEXT, text_value TEXT, integer_value INTEGER, real_value REAL, bool_value INTEGER, ordinal INTEGER);"
        "CREATE TABLE object_changes (object_id TEXT, object_type TEXT, ocel_time TEXT, changed_field TEXT, value_type TEXT, text_value TEXT, integer_value INTEGER, real_value REAL, bool_value INTEGER);"
        "CREATE TABLE ocel_globals (attribute_name TEXT PRIMARY KEY, value_type TEXT, text_value TEXT, integer_value INTEGER, real_value REAL, bool_value INTEGER);");

    std::vector<const OcelEvent*> events;
    for (const auto& event : log.events) events.push_back(&event);
    std::sort(events.begin(), events.end(), [](const auto* left, const auto* right) {
      return left->id < right->id;
    });
    std::vector<const OcelObject*> objects;
    for (const auto& object : log.objects) objects.push_back(&object);
    std::sort(objects.begin(), objects.end(), [](const auto* left, const auto* right) {
      return left->id < right->id;
    });
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
    for (const auto* event : events) activities.insert(event->activity);
    for (const auto* object : objects) object_types.insert(object->type);
    for (const auto& [activity, schema] : log.event_type_attributes) {
      (void)schema;
      activities.insert(activity);
    }
    for (const auto& [type, schema] : log.object_type_attributes) {
      (void)schema;
      object_types.insert(type);
    }
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
    for (const auto& activity : activities) {
      const auto table = "event_" + type_table_token(activity, options);
      std::vector<const std::vector<OcelAttribute>*> rows;
      for (const auto* event : events) {
        if (event->activity == activity) rows.push_back(&event->attributes);
      }
      const auto columns = merge_declared_type_columns(
          collect_type_columns(rows), log.event_type_attributes.contains(activity)
                                           ? log.event_type_attributes.at(activity)
                                           : OcelTypeSchema{});
      database.exec(type_table_schema(table, columns, false, false));
      Statement statement(db, type_table_insert_sql(table, columns, false, false));
      for (const auto* event : events) {
        if (event->activity != activity) continue;
        bind_text(statement.get(), 1, event->id);
        bind_text(statement.get(), 2, event->timestamp);
        bind_type_attributes(statement.get(), 3, columns, event->attributes);
        step_done(db, statement.get());
      }
    }
    std::map<std::string, std::string> object_id_types;
    for (const auto* object : objects) object_id_types[object->id] = object->type;
    for (const auto& type : object_types) {
      const auto table = "object_" + type_table_token(type, options);
      std::vector<const std::vector<OcelAttribute>*> rows;
      for (const auto* object : objects) {
        if (object->type == type) rows.push_back(&object->attributes);
      }
      std::vector<const OcelObjectChange*> changes;
      for (const auto& change : log.object_changes) {
        if (reserved_type_column(change.field)) continue;
        std::string resolved_type = change.object_type;
        if (resolved_type.empty()) {
          const auto object_type = object_id_types.find(change.object_id);
          if (object_type == object_id_types.end()) continue;
          resolved_type = object_type->second;
        }
        if (resolved_type == type) changes.push_back(&change);
      }
      std::stable_sort(changes.begin(), changes.end(),
                       [](const auto* left, const auto* right) {
                         return std::tie(left->object_id, left->timestamp, left->field) <
                                std::tie(right->object_id, right->timestamp, right->field);
                       });
      const bool has_changes = !changes.empty();
      const auto columns = merge_declared_type_columns(
          collect_type_columns(rows, changes),
          log.object_type_attributes.contains(type)
              ? log.object_type_attributes.at(type)
              : OcelTypeSchema{});
      database.exec(type_table_schema(table, columns, true, has_changes));
      Statement statement(db, type_table_insert_sql(table, columns, true, has_changes));
      for (const auto* object : objects) {
        if (object->type != type) continue;
        bind_text(statement.get(), 1, object->id);
        bind_null(statement.get(), 2);
        bind_type_attributes(statement.get(), 3, columns, object->attributes);
        if (has_changes) {
          bind_null(statement.get(), 3 + static_cast<int>(columns.size()));
          bind_integer(statement.get(), 4 + static_cast<int>(columns.size()), 0);
        }
        step_done(db, statement.get());
      }
      std::map<std::string, std::int64_t> change_counts;
      for (const auto* change : changes) {
        bind_text(statement.get(), 1, change->object_id);
        bind_text(statement.get(), 2, change->timestamp);
        bind_type_attributes(statement.get(), 3, columns,
                             std::vector<OcelAttribute>{{change->field, change->value}});
        bind_text(statement.get(), 3 + static_cast<int>(columns.size()), change->field);
        bind_integer(statement.get(), 4 + static_cast<int>(columns.size()),
                     ++change_counts[change->object_id]);
        step_done(db, statement.get());
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
      std::sort(relations.begin(), relations.end(), [](const auto* left, const auto* right) {
        return std::tie(left->event_id, left->object_id, left->qualifier) <
               std::tie(right->event_id, right->object_id, right->qualifier);
      });
      for (const auto* relation : relations) {
        bind_text(statement.get(), 1, relation->event_id);
        bind_text(statement.get(), 2, relation->object_id);
        bind_text(statement.get(), 3, relation->qualifier);
        step_done(db, statement.get());
      }
    }
    {
      Statement statement(db, "INSERT INTO object_object(ocel_source_id, ocel_target_id, ocel_qualifier) VALUES(?1,?2,?3)");
      std::vector<const OcelO2oRelation*> relations;
      for (const auto& relation : log.o2o) relations.push_back(&relation);
      std::sort(relations.begin(), relations.end(), [](const auto* left, const auto* right) {
        return std::tie(left->source_id, left->target_id, left->qualifier) <
               std::tie(right->source_id, right->target_id, right->qualifier);
      });
      for (const auto* relation : relations) {
        bind_text(statement.get(), 1, relation->source_id);
        bind_text(statement.get(), 2, relation->target_id);
        bind_text(statement.get(), 3, relation->qualifier);
        step_done(db, statement.get());
      }
    }
    {
      Statement statement(db, "INSERT INTO event_event(source_event_id, target_event_id, qualifier) VALUES(?1,?2,?3)");
      std::vector<const OcelE2eRelation*> relations;
      for (const auto& relation : log.e2e) relations.push_back(&relation);
      std::sort(relations.begin(), relations.end(), [](const auto* left, const auto* right) {
        return std::tie(left->source_id, left->target_id, left->qualifier) <
               std::tie(right->source_id, right->target_id, right->qualifier);
      });
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
