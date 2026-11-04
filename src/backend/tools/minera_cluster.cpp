#include <filesystem>
#include <fstream>
#include <iostream>
#include "minera/csv_event_log_importer.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "minera/xes_export.hpp"
#include "log_cluster_runtime.hpp"
#ifdef _WIN32
#include <windows.h>
#endif

int run_cluster(int argc,char** argv){
 try{
  if(argc<2){std::cerr<<"Usage: minera_cluster LOG.csv|LOG.xes --regex PATTERN --runtime-url URL [--name NAME --activity-attribute KEY --filter-output LOG.xes --table-output TABLE.json]\n";return 2;}
  std::string pattern,runtime,name,activity,output,table_output;bool has_pattern=false;
  for(int i=2;i<argc;++i){const std::string key=argv[i];if(i+1>=argc)throw std::invalid_argument("missing cluster option value");const std::string value=argv[++i];
   if(key=="--regex"){pattern=value;has_pattern=true;}else if(key=="--runtime-url")runtime=value;else if(key=="--name")name=value;else if(key=="--activity-attribute")activity=value;else if(key=="--filter-output")output=value;else if(key=="--table-output")table_output=value;else throw std::invalid_argument("unknown cluster option: "+key);
  }
  if(!has_pattern||runtime.empty())throw std::invalid_argument("--regex and --runtime-url are required");
  const auto path=std::filesystem::u8path(argv[1]);const auto log=path.extension()==".xes"||path.extension()==".gz"?minera::import_xes_event_log(path):minera::import_csv_event_log(path);
  const auto indices=minera::log_cluster_runtime::select(runtime,log,pattern,activity);const auto filtered=minera::filter_query_events(log,indices,true);
  if(!output.empty()){minera::XesExportOptions options;options.preserve_nulls=true;minera::export_xes_event_log(std::filesystem::u8path(output),filtered,options);}
  if(!table_output.empty()){std::ofstream file(std::filesystem::u8path(table_output),std::ios::binary);if(!file)throw std::runtime_error("could not open cluster table output");file<<minera::log_cluster_runtime::table_json(log,indices);if(!file)throw std::runtime_error("could not write cluster table output");}
  std::cout<<minera::log_cluster_runtime::summary(filtered,name,pattern,indices)<<'\n';return 0;
 }catch(const std::exception& error){std::cerr<<"error: "<<error.what()<<'\n';return 1;}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){
 std::vector<std::string> values;for(int i=0;i<argc;++i){const auto size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,nullptr,0,nullptr,nullptr);if(size<=0)return 1;std::string value(size,'\0');if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,value.data(),size,nullptr,nullptr))return 1;value.pop_back();values.push_back(std::move(value));}
 std::vector<char*> arguments;for(auto& value:values)arguments.push_back(value.data());return run_cluster(argc,arguments.data());
}
#else
int main(int argc,char** argv){return run_cluster(argc,argv);}
#endif
