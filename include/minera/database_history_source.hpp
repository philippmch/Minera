#pragma once
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "minera/event_log.hpp"
namespace minera {
struct DatabaseHistoryOptions {
  std::string connector,driver="sqlite",prefix,dialect;
  std::filesystem::path path;
  std::string connection_string;  // Used for the request only, never serialized.
  std::size_t max_rows=1000;
  unsigned timeout_seconds=30;
};
struct DatabaseHistoryTable {
  std::vector<std::string> columns,column_types;
  std::vector<std::vector<std::optional<std::string>>> rows;
  bool limited=false;
  bool query_timeout_applied=true;
};
using DatabaseHistoryReader=std::function<DatabaseHistoryTable(const DatabaseHistoryOptions&,const std::string&)>;
struct DatabaseHistoryCapture {
  std::string connector,driver,prefix,dialect,query,reference_query;
  DatabaseHistoryTable table;
  EventLog log;
  std::string raw_table_json,formatted_table_json;
};
[[nodiscard]] std::string database_history_query(const std::string& connector,const std::string& prefix={},bool sqlite_dialect=false);
[[nodiscard]] DatabaseHistoryCapture capture_database_history(const DatabaseHistoryOptions& options,DatabaseHistoryReader reader={});
[[nodiscard]] std::string database_history_capture_json(const DatabaseHistoryCapture& capture);
}  // namespace minera
