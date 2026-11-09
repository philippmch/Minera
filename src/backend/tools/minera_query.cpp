#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include "minera/log_query.hpp"
#include "minera/exact_log_query_json.hpp"
#include <set>
#include <optional>
#include "minera/csv_event_log_importer.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "minera/xes_export.hpp"
#include "query_runtime.hpp"
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

int run_query_legacy(int argc,char** argv){
 try{
  if(argc<2){std::cerr<<"Usage: minera_query LOG.csv|LOG.xes --sql SQL [--engine sqlite|duckdb --runtime-url URL] [--limit N] [--filter-output LOG.xes --mode cases|events]\n";return 2;}
  const auto path=std::filesystem::u8path(argv[1]);std::string sql,engine="sqlite",runtime,output,mode="cases";minera::LogQueryOptions options;
  // A repeated option is an error here too, as in the exact-report mode.
  std::set<std::string> seen;
  for(int i=2;i<argc;++i){const std::string key=argv[i];if(i+1>=argc)throw std::invalid_argument("missing query option value");const std::string value=argv[++i];
   if(!seen.insert(key).second)throw std::invalid_argument("duplicate query option: "+key);
   if(key=="--sql")sql=value;else if(key=="--sql-file"){std::ifstream file(std::filesystem::u8path(value),std::ios::binary);if(!file)throw std::invalid_argument("could not open SQL file");sql.assign(std::istreambuf_iterator<char>(file),{});}
   else if(key=="--engine")engine=value;else if(key=="--runtime-url")runtime=value;else if(key=="--filter-output")output=value;else if(key=="--mode")mode=value;
   else if(key=="--limit"){auto parsed=std::from_chars(value.data(),value.data()+value.size(),options.max_rows);if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size())throw std::invalid_argument("invalid query limit");}
   else throw std::invalid_argument("unknown query option: "+key);
  }
  if(engine!="sqlite"&&engine!="duckdb")throw std::invalid_argument("query engine must be sqlite or duckdb");if(mode!="cases"&&mode!="events")throw std::invalid_argument("filter mode must be cases or events");
  // An XES input is pm4py's read_xes frame: the query table holds its rows
  // (each event in file order with its own trace's cells) and a filtered
  // export its cases (merge_dataframe_cases).
  const bool xes=path.extension()==".xes"||path.extension()==".gz";
  const auto log=xes?minera::import_xes_event_log(path):minera::import_csv_event_log(path);const auto table=minera::log_query_table(log);
  std::vector<std::size_t> indices;
  if(engine=="sqlite"){
   const auto result=minera::query_log_sqlite(table,sql,options);std::cout<<minera::log_query_json(result)<<'\n';
   if(!output.empty()){minera::LogQueryOptions complete;complete.max_rows=1000000;complete.max_output_bytes=256u*1024u*1024u;const auto selection=minera::query_log_sqlite(table,sql,complete);if(!selection.can_filter||selection.output_truncated)throw std::invalid_argument("filter requires complete valid event indices");indices=selection.selected_event_indices;}
  }else{
   if(runtime.empty())throw std::invalid_argument("DuckDB requires --runtime-url");
   const auto quote=minera::embedding_runtime::detail::escape_json;
   const auto body="{\"sql\":"+quote(sql)+",\"max_rows\":"+std::to_string(options.max_rows)+",\"data\":"+minera::log_query_table_json(table)+'}';
   const auto result=minera::query_runtime::request(runtime,"/query",&body);if(result->status!=200)throw std::runtime_error("query runtime returned HTTP "+std::to_string(result->status));std::cout<<result->body<<'\n';
   if(!output.empty()){const auto complete="{\"sql\":"+quote(sql)+",\"max_rows\":1000000,\"data\":"+minera::log_query_table_json(table)+'}';const auto selection=minera::query_runtime::request(runtime,"/select",&complete);if(selection->status!=200)throw std::runtime_error("query selection failed");indices=minera::query_runtime::parse_indices(selection->body,log.event_count());}
  }
  if(!output.empty()){
   minera::XesExportOptions export_options;export_options.preserve_nulls=true;
   if(!xes){minera::export_xes_event_log(std::filesystem::u8path(output),minera::filter_query_events(log,indices,mode=="cases"),export_options);return 0;}
   auto cases=log;std::vector<std::vector<std::size_t>> rows;minera::merge_dataframe_cases(cases,&rows);
   // The selected frame rows as positions among the cases' events.
   std::vector<std::size_t> position(table.table.rows.size(),static_cast<std::size_t>(-1));std::size_t next=0;
   for(const auto& trace:rows)for(const auto row:trace){if(row<position.size())position[row]=next;++next;}
   std::vector<std::size_t> selected;for(const auto index:indices)if(index<position.size()&&position[index]!=static_cast<std::size_t>(-1))selected.push_back(position[index]);
   std::sort(selected.begin(),selected.end());
   minera::export_xes_event_log(std::filesystem::u8path(output),minera::filter_query_events(cases,selected,mode=="cases"),export_options);
  }
  return 0;
 }catch(const std::exception& error){std::cerr<<"error: "<<error.what()<<'\n';return 1;}
}

namespace {
std::string query_file_bytes(const std::string& path){
  std::ifstream file(std::filesystem::u8path(path),std::ios::binary);
  if(!file)throw std::invalid_argument("could not open query input: "+path);
  return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
std::string query_basename(const std::string& path){const auto value=std::filesystem::u8path(path).filename().u8string();return {value.begin(),value.end()};}
int run_exact_query(int argc,char** argv){
#ifdef _WIN32
  _setmode(_fileno(stdout),_O_BINARY);_setmode(_fileno(stderr),_O_BINARY);
#endif

  minera::ExactLogQueryReport report;const auto q=minera::exact_query_string;
  std::vector<minera::DiscoveryWorkbenchPart> parts;
  std::string engine="sqlite",runtime,report_path,export_path,mode="cases",stdout_bytes,stderr_bytes,phase="control";
  std::optional<minera::EventLog> log;std::optional<minera::LogQueryTable> table;
  bool export_written=false;int status=0;
  // Discover the report destination before reading any supplied File. A later
  // input/open/parse error must still produce the requested owned sidecar.
  for(int i=2;i+1<argc;i+=2){const std::string key=argv[i];if(key=="--exact-query-report"&&report_path.empty())report_path=argv[i+1];if(key=="--filter-output"&&export_path.empty())export_path=argv[i+1];}

  try{
    if(argc<2)throw std::invalid_argument("supply the original CSV or XES log");
    std::set<std::string> seen;
    for(int i=2;i<argc;++i){
      const std::string key=argv[i];if(i+1>=argc)throw std::invalid_argument("missing query option value");
      const std::string value=argv[++i];if(!seen.insert(key).second)throw std::invalid_argument("duplicate query option: "+key);
      if(key=="--engine"){engine=value;parts.push_back({"engine","",value,false});}
      else if(key=="--runtime-url")runtime=value;
      else if(key=="--exact-query-report")report_path=value;
      else if(key=="--filter-output")export_path=value;
      else if(key=="--mode")mode=value;
      else if(key=="--sql")parts.push_back({"sql","",value,false});
      else if(key=="--sql-file")parts.push_back({"sql_file",query_basename(value),query_file_bytes(value),true});
      else if(key=="--query-options-json")parts.push_back({"query_options_json","",value,false});
      else if(key=="--query-options-file")parts.push_back({"query_options_file",query_basename(value),query_file_bytes(value),true});
      else if(key=="--limit")parts.push_back({"limit","",value,false});
      else throw std::invalid_argument("unknown query option: "+key);
    }
    report.operation=export_path.empty()?"query":"export";
    if(mode!="cases"&&mode!="events")throw std::invalid_argument("filter mode must be cases or events");
    if(!export_path.empty())parts.push_back({"mode","",mode,false});
    auto request=minera::prepare_exact_log_query(report,parts,{},false);
    phase="source";const auto original=query_file_bytes(argv[1]);std::istringstream source(original);
    const auto path=std::filesystem::u8path(argv[1]);
    log=path.extension()==".xes"||path.extension()==".gz"?minera::import_xes_event_log(source):minera::import_csv_event_log(source);
    table=minera::log_query_table(*log);
    report.source_context="{\"id\":"+q("cli")+",\"dataset\":"+q(query_basename(argv[1]))+",\"original_file\":{\"name\":"+q(query_basename(argv[1]))+",\"format\":"+q(path.extension()==".xes"||path.extension()==".gz"?"xes":"csv")+",\"bytes\":"+q(original)+"},\"original_file_unavailable_reason\":null}";
    report.source_id="cli";minera::record_exact_query_source(report,*table);minera::record_exact_query_log(report,*log);
    std::vector<std::size_t> indices;bool can_filter=false,truncated=false;phase="query";
    if(engine=="sqlite"){
      const auto value=minera::execute_exact_sqlite_query(report,*table,request.sql,request.options);
      indices=value.selected_event_indices;can_filter=value.can_filter;truncated=value.output_truncated;
    }else{
      phase="runtime";if(runtime.empty())throw std::invalid_argument("DuckDB requires --runtime-url");
      const auto body=minera::exact_duck_query_body(*table,request.sql,request.options,"cli");report.query_call.known=false;
      try{
        const auto reply=minera::query_runtime::request(runtime,"/query",&body);minera::record_exact_duck_query(report,reply->body,reply->status);
        const auto selected=minera::parse_exact_duck_query_selection(reply->body,table->table.rows.size());indices=selected.indices;can_filter=selected.can_filter;truncated=selected.output_truncated;
      }catch(const std::exception& e){if(report.query_call.error=="null")report.query_call.error=minera::exact_query_error("runtime_error",e.what());throw;}
    }
    // Keep the pre-existing stdout format. The sidecar, independent of JSON
    // projection, retains exact canonical native outcomes and raw stdout bytes.
    if(report.artifacts["legacy_result_json"].bytes)stdout_bytes=*report.artifacts["legacy_result_json"].bytes+'\n';
    minera::record_exact_query_source(report,*table,true);minera::record_exact_query_log(report,*log,true);
    if(!export_path.empty()){
      phase="selection";if(!can_filter||truncated)throw std::invalid_argument("filter requires complete valid event indices");
      std::string selected="[";for(auto i:indices){if(selected.size()>1)selected+=',';selected+=std::to_string(i);}report.selection="{\"indices\":"+selected+"],\"mode\":"+q(mode)+",\"complete\":true}";
      phase="filter";report.filter_call.entered=true;
      try{
        const auto filtered=minera::filter_query_events(*log,indices,mode=="cases");report.filter_call.returned=true;minera::record_exact_query_filtered(report,filtered);
        phase="export";report.export_call.entered=true;
        try{
          minera::XesExportOptions options;options.preserve_nulls=true;
          const auto actual=minera::export_xes_event_log(std::filesystem::u8path(export_path),filtered,options);export_written=true;
          const auto u8=actual.u8string();const std::string actual_path(u8.begin(),u8.end());
          const auto bytes=query_file_bytes(actual_path);minera::record_exact_query_output(report,"xes",actual.extension()==".gz"?"application/gzip":"application/xml",bytes);report.export_call.returned=true;
        }catch(const std::exception& e){report.export_call.error=minera::exact_query_error(minera::exact_query_exception_type(e),e.what());throw;}
      }catch(const std::exception& e){if(!report.filter_call.returned)report.filter_call.error=minera::exact_query_error(minera::exact_query_exception_type(e),e.what());throw;}
    }
  }catch(const std::exception& e){status=1;stderr_bytes="error: "+std::string(e.what())+'\n';report.fail(phase,e.what(),minera::exact_query_exception_type(e));}
  if(table)minera::record_exact_query_source(report,*table,true);if(log)minera::record_exact_query_log(report,*log,true);
  if(report.original_inputs.empty())report.original_inputs=parts;
  std::string arguments="[";for(int i=0;i<argc;++i){if(i)arguments+=',';arguments+=q(argv[i]);}arguments+=']';
  report.cli="{\"argv\":"+arguments+",\"report_path\":"+q(report_path)+",\"export_requested_path\":"+q(export_path)+",\"export_written\":"+(export_written?"true":"false")+",\"stdout\":"+q(stdout_bytes)+",\"stderr\":"+q(stderr_bytes)+'}';
  std::cout.write(stdout_bytes.data(),static_cast<std::streamsize>(stdout_bytes.size()));std::cerr.write(stderr_bytes.data(),static_cast<std::streamsize>(stderr_bytes.size()));
  if(!report_path.empty()){
    try{std::ofstream file(std::filesystem::u8path(report_path),std::ios::binary);const auto bytes=report.json();if(!file)throw std::runtime_error("could not open exact query report");file.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));if(!file)throw std::runtime_error("could not write exact query report");}
    catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}
  }
  return status;
}
} // namespace
int run_query(int argc,char** argv){
  bool exact=false;
  // Scan option positions, not literal SQL/options values.
  for(int i=2;i<argc;i+=2){const std::string key=argv[i];if(key=="--query-options-json"||key=="--query-options-file"||key=="--exact-query-report")exact=true;}
  if(argc==2&&std::string(argv[1])=="--help"){
    std::cout<<"Usage: minera_query LOG.csv|LOG.xes --sql SQL|--sql-file PATH [--engine sqlite|duckdb] [--runtime-url URL] [--limit N] [--query-options-json RAW|--query-options-file PATH] [--exact-query-report PATH] [--filter-output PATH --mode cases|events]\n--runtime-url URL: DuckDB runtime endpoint (required with --engine duckdb). --limit N: maximum result rows. Each option may be given once.\nFull SQLite options: table_name,max_rows,max_vm_steps,parameters. DuckDB: table_name,max_rows,parameters; max_vm_steps is SQLite-only. Full options conflict with --limit. Exact report preserves typed outcomes independently from legacy stdout.\n";return 0;
  }
  return exact?run_exact_query(argc,argv):run_query_legacy(argc,argv);
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){
 std::vector<std::string> values;values.reserve(argc);for(int i=0;i<argc;++i){const auto size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,nullptr,0,nullptr,nullptr);if(size<=0)return 1;std::string value(size,'\0');if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,value.data(),size,nullptr,nullptr))return 1;value.pop_back();values.push_back(std::move(value));}
 std::vector<char*> arguments;for(auto& value:values)arguments.push_back(value.data());return run_query(argc,arguments.data());
}
#else
int main(int argc,char** argv){return run_query(argc,argv);}
#endif
