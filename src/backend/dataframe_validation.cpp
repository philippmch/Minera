#include "minera/dataframe_validation.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace minera {
namespace {
bool missing(const std::optional<EventAttribute>& cell) {
  if (!cell || cell->is_null || cell->first_class_state == FirstClassAttributeState::kNull)
    return true;
  const auto* number = std::get_if<double>(&cell->typed_value);
  return number && std::isnan(*number);
}

std::string inferred_dtype(const EventTable& table, std::size_t column) {
  if (!table.dtypes.empty()) return table.dtypes[column];
  bool seen = false, dates = true, integers = true, numbers = true, booleans = true;
  for (const auto& row : table.rows) {
    if (missing(row[column])) continue;
    seen = true;
    const auto& cell = *row[column];
    dates &= cell.kind == AttributeKind::kDate || std::holds_alternative<NativeDate>(cell.typed_value);
    integers &= cell.kind == AttributeKind::kInt;
    numbers &= cell.kind == AttributeKind::kInt || cell.kind == AttributeKind::kFloat;
    booleans &= cell.kind == AttributeKind::kBoolean;
  }
  if (seen && dates) return "datetime64[us]";
  if (seen && integers) return "int64";
  if (seen && numbers) return "float64";
  if (seen && booleans) return "bool";
  return "object";
}
bool string_dtype(std::string_view value) {
  return value == "object" || value == "str" || value == "string" ||
         value.starts_with("string[") || value.starts_with("str[");
}
bool date_dtype(std::string_view value) { return value.starts_with("datetime64["); }

std::string available_columns(std::vector<std::string> columns) {
  std::sort(columns.begin(), columns.end());
  std::ostringstream output;
  output << '[';
  for (std::size_t i = 0; i < columns.size(); ++i) {
    if (i) output << ", ";
    const char quote = columns[i].find('\'') != std::string::npos &&
                       columns[i].find('"') == std::string::npos ? '"' : '\'';
    output << quote;
    for (const unsigned char character : columns[i]) {
      if (character == quote || character == '\\') output << '\\' << character;
      else if (character == '\n') output << "\\n";
      else if (character == '\r') output << "\\r";
      else if (character == '\t') output << "\\t";
      else if (character < 32 || character == 127)
        output << "\\x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(character) << std::dec;
      else output << character;
    }
    output << quote;
  }
  output << ']';
  return output.str();
}
}  // namespace

void check_dataframe_columns(const EventTable& table,
                             const DataFrameColumnOptions& options) {
  table.validate();
  if (table.columns.size() < 3)
    throw std::invalid_argument("the dataframe should (at least) contain a column for the case identifier, a column for the activity and a column for the timestamp.");
  std::vector<std::string> dtypes;
  std::size_t strings = 0, dates = 0;
  for (std::size_t i = 0; i < table.columns.size(); ++i) {
    dtypes.push_back(inferred_dtype(table, i));
    strings += string_dtype(dtypes.back());
    dates += date_dtype(dtypes.back());
  }
  if (strings < 2)
    throw std::invalid_argument("the dataframe should (at least) contain a column of type string for the case identifier and a column of type string for the activity.");
  if (!dates) throw std::invalid_argument("the dataframe should (at least) contain a column of type date");
  const auto check = [&](const std::optional<std::string>& selected, std::string_view role, bool date) {
    if (!selected) return;
    const auto found = std::find(table.columns.begin(), table.columns.end(), *selected);
    if (found == table.columns.end())
      throw std::invalid_argument("the specified " + std::string(role) +
          " column is not contained in the dataframe. Available columns: " + available_columns(table.columns));
    const auto column = static_cast<std::size_t>(found - table.columns.begin());
    if (date ? !date_dtype(dtypes[column]) : !string_dtype(dtypes[column]))
      throw std::invalid_argument("the " + std::string(role) + " column should be of " +
          (date ? "time datetime. Use the function pandas.to_datetime" : "type string."));
    for (const auto& row : table.rows)
      if (missing(row[column]))
        throw std::invalid_argument("the " + std::string(role) + " column should not contain any empty value.");
  };
  check(options.case_id_key, "case ID", false);
  check(options.activity_key, "activity", false);
  check(options.timestamp_key, "timestamp", true);
  check(options.start_timestamp_key, "start timestamp", true);
}
}  // namespace minera
