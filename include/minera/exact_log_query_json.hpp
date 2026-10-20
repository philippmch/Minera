#pragma once
#include "minera/log_query.hpp"
#include "minera/discovery_workbench_json.hpp"
#include <map>
#include <exception>
#include <optional>

namespace minera {
struct ExactLogQueryOptions {
  std::string engine="sqlite";
  LogQueryOptions value;
};
struct ExactQueryCall {
  bool entered=false,returned=false,known=true;
  std::string error="null";
};
struct ExactQueryArtifact {
  std::optional<std::string> bytes,error;
};
struct ExactLogQueryReport {
  explicit ExactLogQueryReport(std::string operation="query");
  int status=200;
  std::string operation,engine,source_id,input_order="provided_order";
  std::vector<DiscoveryWorkbenchPart> original_inputs,query_inputs;
  std::string sql_context="null",options_context="null",source_context="null";
  std::string configured_options="null",result="null",selection="null",filtered="null",output="null",cli="null",error="null";
  std::map<std::string,std::string> snapshots,optional_errors;
  std::map<std::string,ExactQueryArtifact> artifacts;
  ExactQueryCall query_call,filter_call,export_call;
  void fail(std::string_view phase,std::string_view message,std::string_view type="invalid_argument");
  [[nodiscard]] std::string json() const;
};
struct ExactLogQueryRequest {
  ExactLogQueryOptions options;
  std::string sql,source_id,mode="cases",format="xes";
};
[[nodiscard]] std::string exact_query_string(std::string_view);
[[nodiscard]] std::string exact_query_exception_type(const std::exception&);
[[nodiscard]] std::string exact_query_error(std::string_view type,std::string_view message);
[[nodiscard]] ExactLogQueryOptions parse_exact_log_query_options_json(std::string_view,std::string_view engine="sqlite");
[[nodiscard]] std::string serialize_exact_log_query_options_json(const ExactLogQueryOptions&);
[[nodiscard]] std::string serialize_exact_log_query_value_json(const QueryValue&);
[[nodiscard]] std::string serialize_exact_log_query_result_json(const LogQueryResult&);
[[nodiscard]] std::string serialize_exact_log_query_table_json(const LogQueryTable&);
[[nodiscard]] ExactLogQueryRequest prepare_exact_log_query(ExactLogQueryReport&,
    const std::vector<DiscoveryWorkbenchPart>&,const std::vector<DiscoveryWorkbenchPart>& = {},
    bool require_source_id=true,std::string_view transport_error={});
void record_exact_query_options(ExactLogQueryReport&,const ExactLogQueryOptions&,bool after=false);
void record_exact_query_source(ExactLogQueryReport&,const LogQueryTable&,bool after=false);
void record_exact_query_log(ExactLogQueryReport&,const EventLog&,bool after=false);
[[nodiscard]] LogQueryResult execute_exact_sqlite_query(ExactLogQueryReport&,const LogQueryTable&,std::string_view,const ExactLogQueryOptions&);
struct ExactQuerySelection { std::vector<std::size_t> indices; bool can_filter=false,output_truncated=false; };
[[nodiscard]] ExactQuerySelection parse_exact_duck_query_selection(std::string_view,std::size_t);
void record_exact_duck_query(ExactLogQueryReport&,std::string_view body,int status);
[[nodiscard]] std::string exact_duck_query_body(const LogQueryTable&,std::string_view sql,const ExactLogQueryOptions&,std::string_view source_id);
void record_exact_query_filtered(ExactLogQueryReport&,const EventLog&);
void record_exact_query_filtered(ExactLogQueryReport&,const EventTable&);
void record_exact_query_output(ExactLogQueryReport&,std::string_view format,std::string_view mime,std::string_view bytes);
} // namespace minera
