#include "minera/ocel_sqlite.hpp"

#include "ocel_frame_types.hpp"

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

using TypeColumns = std::vector<std::pair<std::string, std::string>>;

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

std::map<std::string, const OcelValue*> attribute_values(
    const std::vector<OcelAttribute>& attributes) {
  std::map<std::string, const OcelValue*> result;
  for (const auto& attribute : attributes) {
    if (!reserved_type_column(attribute.key)) result[attribute.key] = &attribute.value;
  }
  return result;
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

// One pass over a type table: per ocel_id, the timestamp of its first row by
// rowid and the attributes of its base row. The base row is the first by
// rowid, or with object changes the first one without a changed field (then
// the first with @@cumcount 0, then any). Reference-written tables have no
// index on ocel_id, so per-id queries would scan the table once per id.
struct TypeTableRow {
  std::string timestamp;
  std::vector<OcelAttribute> attributes;
  int rank = 0;
};

class TypeTableRows {
 public:
  TypeTableRows(sqlite3* db, std::string_view table) {
    const auto columns = table_columns(db, table);
    if (columns.empty()) return;
    exists_ = true;
    int id_index = -1, time_index = -1, changed_index = -1, cumulative_index = -1;
    for (std::size_t index = 0; index < columns.size(); ++index) {
      const auto& name = columns[index].name;
      if (name == "ocel_id") id_index = static_cast<int>(index);
      else if (name == "ocel_time") time_index = static_cast<int>(index);
      else if (name == "ocel_changed_field") changed_index = static_cast<int>(index);
      else if (name == "@@cumcount") cumulative_index = static_cast<int>(index);
    }
    if (id_index < 0)
      throw std::runtime_error("SQLite prepare failed: no such column: ocel_id");
    has_time_ = time_index >= 0;
    Statement statement(db, "SELECT * FROM " + quote_identifier(table) +
                                " ORDER BY rowid");
    while (true) {
      const auto status = sqlite3_step(statement.get());
      if (status == SQLITE_DONE) break;
      if (status != SQLITE_ROW)
        throw std::runtime_error("SQLite type-table attribute query failed");
      auto* row = statement.get();
      if (sqlite3_column_type(row, id_index) == SQLITE_NULL) continue;
      int rank = 0;
      if (changed_index >= 0 &&
          sqlite3_column_type(row, changed_index) != SQLITE_NULL) {
        const auto cumulative_type =
            cumulative_index < 0 ? SQLITE_NULL
                                 : sqlite3_column_type(row, cumulative_index);
        rank = (cumulative_type == SQLITE_INTEGER ||
                cumulative_type == SQLITE_FLOAT) &&
                       sqlite3_column_double(row, cumulative_index) == 0.0
                   ? 1
                   : 2;
      }
      auto [entry, inserted] = rows_.try_emplace(column_string(row, id_index));
      if (inserted) {
        if (time_index >= 0) entry->second.timestamp = column_string(row, time_index);
      } else if (rank >= entry->second.rank) {
        continue;
      }
      entry->second.rank = rank;
      entry->second.attributes.clear();
      for (std::size_t index = 0; index < columns.size(); ++index) {
        const auto& column = columns[index];
        if (column.name == "ocel_id" || column.name == "ocel_time" ||
            column.name == "ocel_changed_field" || column.name == "@@cumcount")
          continue;
        if (sqlite3_column_type(row, static_cast<int>(index)) == SQLITE_NULL)
          continue;
        entry->second.attributes.push_back(
            {column.name, read_sqlite_cell(row, static_cast<int>(index),
                                           column.declared_type)});
      }
    }
  }

  bool exists() const { return exists_; }
  const TypeTableRow* find(const std::string& id) const {
    const auto found = rows_.find(id);
    return found == rows_.end() ? nullptr : &found->second;
  }
  std::string timestamp(const std::string& id) const {
    if (!has_time_)
      throw std::runtime_error("SQLite prepare failed: no such column: ocel_time");
    const auto* row = find(id);
    return row == nullptr ? std::string{} : row->timestamp;
  }

 private:
  bool exists_ = false;
  bool has_time_ = false;
  std::unordered_map<std::string, TypeTableRow> rows_;
};

class TypeTableCache {
 public:
  explicit TypeTableCache(sqlite3* db) : db_(db) {}
  const TypeTableRows& rows(const std::string& table) {
    auto found = tables_.find(table);
    if (found == tables_.end())
      found = tables_.try_emplace(table, db_, table).first;
    return found->second;
  }

 private:
  sqlite3* db_;
  std::map<std::string, TypeTableRows> tables_;
};

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
  std::vector<std::string> event_order;
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
      event_order.push_back(id);
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

  TypeTableCache type_tables(db);
  // Rows in table order, then ordered by timestamp below like the reference.
  for (const auto& id : event_order) {
    const auto& activity = event_types.at(id);
    auto mapped = event_map.find(activity);
    const auto table = mapped == event_map.end()
                           ? "event_" + type_table_token(activity, options)
                           : "event_" + mapped->second;
    OcelEvent event;
    event.id = id;
    event.activity = activity;
    const auto& rows = type_tables.rows(table);
    if (rows.exists()) event.timestamp = rows.timestamp(id);
    if (const auto attributes = event_attributes.find(id);
        attributes != event_attributes.end()) {
      event.attributes = attributes->second;
    } else if (const auto* row = rows.find(id)) {
      event.attributes = row->attributes;
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
                           ? "object_" + type_table_token(type, options)
                           : "object_" + mapped->second;
    if (const auto attributes = object_attributes.find(id);
        attributes != object_attributes.end()) {
      object.attributes = attributes->second;
    } else if (const auto* row = type_tables.rows(table).find(id)) {
      object.attributes = row->attributes;
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
  sort_ocel_events_by_timestamp(log, true);
  // The reference SQLite reader drops events and objects without relations.
  drop_unrelated_ocel_entries(log);
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
    // Type tables as the reference writes them from its frames (pandas
    // to_sql): one column per attribute with a value in the type, in global
    // column order, typed by the frame column's dtype (an integer column with
    // a missing cell is float64, so REAL; booleans are INTEGER 0/1; an object
    // column by the kinds of its values in the table). Timestamps are written
    // as pandas prints them. The exact typed values and declared schemas live
    // in the attribute and catalog tables above and below.
    const auto table_columns_for = [](const ocel_frame::Columns& frame,
                                      const ocel_frame::Columns& table_values) {
      TypeColumns columns;
      for (const auto& name : frame.names()) {
        if (reserved_type_column(name) || name == "ocel:activity") continue;
        const auto* stats = table_values.stats(name);
        if (stats == nullptr || stats->present == 0) continue;
        columns.emplace_back(name, ocel_frame::sql_type(frame.dtype(name), *stats));
      }
      return columns;
    };
    const auto create_table = [&](const std::string& table,
                                  const std::vector<std::string>& leading,
                                  const TypeColumns& columns,
                                  const std::vector<std::string>& trailing) {
      std::string schema = "CREATE TABLE " + quote_identifier(table) + " (";
      std::string insert = "INSERT INTO " + quote_identifier(table) + " VALUES(";
      std::size_t count = 0;
      const auto add = [&](const std::string& name, const std::string& type) {
        if (count != 0) { schema += ", "; insert += ","; }
        schema += quote_identifier(name) + " " + type;
        insert += "?" + std::to_string(++count);
      };
      for (const auto& name : leading) add(name, name == "ocel_time" ? "TIMESTAMP" : "TEXT");
      for (const auto& [name, type] : columns) add(name, type);
      for (const auto& name : trailing) add(name, name == "ocel_time" ? "TIMESTAMP" : "TEXT");
      database.exec(schema + ");");
      return insert + ")";
    };
    const auto pandas_time = [](const std::string& text) {
      return ocel_frame::utc_timestamp_text(text, ' ', "+00:00");
    };
    ocel_frame::Columns event_frame;
    std::map<std::string, std::vector<const OcelEvent*>> events_by_activity;
    for (const auto* event : events) {
      event_frame.observe_row(event->attributes);
      events_by_activity[event->activity].push_back(event);
    }
    for (const auto& activity : activities) {
      const auto table = "event_" + type_table_token(activity, options);
      const auto& rows = events_by_activity[activity];
      ocel_frame::Columns table_values;
      for (const auto* event : rows) table_values.observe_row(event->attributes);
      const auto columns = table_columns_for(event_frame, table_values);
      Statement statement(db, create_table(table, {"ocel_id", "ocel_time", "ocel:activity"},
                                           columns, {}));
      for (const auto* event : rows) {
        bind_text(statement.get(), 1, event->id);
        bind_text(statement.get(), 2, pandas_time(event->timestamp));
        bind_text(statement.get(), 3, event->activity);
        bind_type_attributes(statement.get(), 4, columns, event->attributes);
        step_done(db, statement.get());
      }
    }
    std::unordered_map<std::string_view, std::string_view> object_id_types;
    ocel_frame::Columns object_frame;
    std::map<std::string, std::vector<const OcelObject*>> objects_by_type;
    for (const auto* object : objects) {
      object_id_types.emplace(object->id, object->type);
      object_frame.observe_row(object->attributes);
      objects_by_type[object->type].push_back(object);
    }
    std::map<std::string, std::vector<const OcelObjectChange*>> changes_by_type;
    for (const auto& change : log.object_changes) {
      if (reserved_type_column(change.field) || change.field == "ocel:activity") continue;
      std::string resolved_type = change.object_type;
      if (resolved_type.empty()) {
        const auto object_type = object_id_types.find(change.object_id);
        if (object_type == object_id_types.end()) continue;
        resolved_type = std::string(object_type->second);
      }
      changes_by_type[resolved_type].push_back(&change);
    }
    for (const auto& type : object_types) {
      const auto table = "object_" + type_table_token(type, options);
      const auto& rows = objects_by_type[type];
      const auto& changes = changes_by_type[type];
      const bool has_changes = !changes.empty();
      // The reference appends the change rows to the type's object rows, so
      // every column of a type with changes has missing cells.
      ocel_frame::Columns frame = object_frame;
      ocel_frame::Columns table_values;
      for (const auto* object : rows) table_values.observe_row(object->attributes);
      frame.add_rows(changes.size());
      for (const auto* change : changes) {
        frame.observe(change->field, change->value);
        table_values.observe(change->field, change->value);
      }
      const auto columns = table_columns_for(frame, table_values);
      const std::vector<std::string> trailing =
          has_changes ? std::vector<std::string>{"ocel_changed_field", "ocel_time"}
                      : std::vector<std::string>{};
      Statement statement(db, create_table(table, {"ocel_id"}, columns, trailing));
      const int after = 2 + static_cast<int>(columns.size());
      for (const auto* object : rows) {
        bind_text(statement.get(), 1, object->id);
        bind_type_attributes(statement.get(), 2, columns, object->attributes);
        if (has_changes) {
          bind_null(statement.get(), after);
          bind_null(statement.get(), after + 1);
        }
        step_done(db, statement.get());
      }
      for (const auto* change : changes) {
        bind_text(statement.get(), 1, change->object_id);
        bind_type_attributes(statement.get(), 2, columns,
                             std::vector<OcelAttribute>{{change->field, change->value}});
        bind_text(statement.get(), after, change->field);
        bind_text(statement.get(), after + 1, pandas_time(change->timestamp));
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
