#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include "minera/log_query.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "minera/xes_export.hpp"
#include "query_runtime.hpp"
#ifdef _WIN32
#include <windows.h>
#endif

int run_query(int argc,char** argv){
 try{
  if(argc<2){std::cerr<<"Usage: minera_query LOG.csv|LOG.xes --sql SQL [--engine sqlite|duckdb --runtime-url URL] [--limit N] [--filter-output LOG.xes --mode cases|events]\n";return 2;}
  const auto path=std::filesystem::u8path(argv[1]);std::string sql,engine="sqlite",runtime,output,mode="cases";minera::LogQueryOptions options;
  for(int i=2;i<argc;++i){const std::string key=argv[i];if(i+1>=argc)throw std::invalid_argument("missing query option value");const std::string value=argv[++i];
   if(key=="--sql")sql=value;else if(key=="--sql-file"){std::ifstream file(std::filesystem::u8path(value),std::ios::binary);if(!file)throw std::invalid_argument("could not open SQL file");sql.assign(std::istreambuf_iterator<char>(file),{});}
   else if(key=="--engine")engine=value;else if(key=="--runtime-url")runtime=value;else if(key=="--filter-output")output=value;else if(key=="--mode")mode=value;
   else if(key=="--limit"){auto parsed=std::from_chars(value.data(),value.data()+value.size(),options.max_rows);if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size())throw std::invalid_argument("invalid query limit");}
   else throw std::invalid_argument("unknown query option: "+key);
  }
  if(engine!="sqlite"&&engine!="duckdb")throw std::invalid_argument("query engine must be sqlite or duckdb");if(mode!="cases"&&mode!="events")throw std::invalid_argument("filter mode must be cases or events");
  const auto log=path.extension()==".xes"||path.extension()==".gz"?minera::import_xes_event_log(path):minera::import_csv_event_log(path);const auto table=minera::log_query_table(log);
  std::vector<std::size_t> indices;
  if(engine=="sqlite"){
   const auto result=minera::query_log_sqlite(table,sql,options);std::cout<<minera::log_query_json(result)<<'\n';
   if(!output.empty()){minera::LogQueryOptions complete;complete.max_rows=1000000;const auto selection=minera::query_log_sqlite(table,sql,complete);if(!selection.can_filter||selection.output_truncated)throw std::invalid_argument("filter requires complete valid event indices");indices=selection.selected_event_indices;}
  }else{
   if(runtime.empty())throw std::invalid_argument("DuckDB requires --runtime-url");
   const auto quote=minera::embedding_runtime::detail::escape_json;
   const auto body="{\"sql\":"+quote(sql)+",\"max_rows\":"+std::to_string(options.max_rows)+",\"data\":"+minera::log_query_table_json(table)+'}';
   const auto result=minera::query_runtime::request(runtime,"/query",&body);if(result->status!=200)throw std::runtime_error("query runtime returned HTTP "+std::to_string(result->status));std::cout<<result->body<<'\n';
   if(!output.empty()){const auto complete="{\"sql\":"+quote(sql)+",\"max_rows\":1000000,\"data\":"+minera::log_query_table_json(table)+'}';const auto selection=minera::query_runtime::request(runtime,"/select",&complete);if(selection->status!=200)throw std::runtime_error("query selection failed");indices=minera::query_runtime::parse_indices(selection->body,log.event_count());}
  }
  if(!output.empty()){minera::XesExportOptions export_options;export_options.preserve_nulls=true;minera::export_xes_event_log(std::filesystem::u8path(output),minera::filter_query_events(log,indices,mode=="cases"),export_options);}
  return 0;
 }catch(const std::exception& error){std::cerr<<"error: "<<error.what()<<'\n';return 1;}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){
 std::vector<std::string> values;values.reserve(argc);for(int i=0;i<argc;++i){const auto size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,nullptr,0,nullptr,nullptr);if(size<=0)return 1;std::string value(size,'\0');if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,value.data(),size,nullptr,nullptr))return 1;value.pop_back();values.push_back(std::move(value));}
 std::vector<char*> arguments;for(auto& value:values)arguments.push_back(value.data());return run_query(argc,arguments.data());
}
#else
int main(int argc,char** argv){return run_query(argc,argv);}
#endif
