#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <vector>
#include "minera/event_data.hpp"

namespace minera {
struct QueryBlob { std::string hex; };
using QueryValue = std::variant<std::monostate,std::int64_t,double,std::string,QueryBlob>;
struct LogQueryOptions {
  std::string table_name="dataframe";
  std::size_t max_rows=1000;
  std::size_t max_vm_steps=50000000;
  std::vector<QueryValue> parameters;
};
struct LogQueryTable {
  EventTable table;
  std::string event_index_column;
};
struct LogQueryResult {
  std::string engine="sqlite";
  std::string sql;
  std::string event_index_column;
  std::vector<std::string> columns;
  std::vector<std::vector<QueryValue>> rows;
  bool output_truncated=false;
  bool can_filter=false;
  std::vector<std::size_t> selected_event_indices;
};
[[nodiscard]] LogQueryTable log_query_table(const EventLog& log);
[[nodiscard]] LogQueryResult query_log_sqlite(const LogQueryTable& table,std::string_view sql,const LogQueryOptions& options={});
[[nodiscard]] EventLog filter_query_events(const EventLog& log,const std::vector<std::size_t>& indices,bool whole_cases=false);
[[nodiscard]] std::string log_query_json(const LogQueryResult& result);
[[nodiscard]] std::string log_query_table_json(const LogQueryTable& table);
[[nodiscard]] bool sqlite_log_query_available();
} // namespace minera
