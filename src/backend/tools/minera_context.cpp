#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include "minera/alpha_miner.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/powl.hpp"
#include "minera/powl_discovery.hpp"
#include "minera/powl_conversion.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "llm_context_options.hpp"
#ifdef _WIN32
#include <windows.h>
#endif
int run_context(int argc,char** argv){
 try{
  if(argc<2){std::cerr<<"Usage: minera_context INPUT.csv|INPUT.xes|INPUT.json|MODEL.pnml --variant CONTEXT [--max-len N --case-index N --miner inductive|alpha|heuristic|powl --object-type TYPE --include-header true|false ...]\n";return 2;}
  std::map<std::string,std::string> values;for(int i=2;i<argc;++i){std::string key=argv[i];if(!key.starts_with("--")||i+1>=argc)throw std::invalid_argument("context options require a value");key=key.substr(2);std::replace(key.begin(),key.end(),'-','_');if(!minera::context_tools::fields.contains(key)||values.contains(key))throw std::invalid_argument("unknown or duplicate context option: "+key);values[key]=argv[++i];}
  const auto options=minera::context_tools::options([&](auto key){return values.at(std::string(key));},[&](auto key){return values.contains(std::string(key));});const auto path=std::filesystem::u8path(argv[1]);minera::LlmContextResult result;
  if(minera::llm_context_is_ocel(options.variant)){std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("could not open OCEL source");std::string body(std::istreambuf_iterator<char>(file),{});result=minera::build_llm_context(minera::parse_ocel_json(body),options);}
  else if(path.extension()==".pnml"){if(options.variant!="petri_net")throw std::invalid_argument("PNML source requires variant=petri_net");minera::PnmlImportOptions import;import.require_workflow_boundaries=false;const auto document=minera::import_pnml_document(path,import);result=minera::build_llm_context(minera::EventLog{},options,&document.net,&document);}
  else{const auto log=path.extension()==".xes"||path.extension()==".gz"?minera::import_xes_dataframe_log(path):minera::import_csv_event_log(path);std::optional<minera::PetriNet> net;
   if(options.variant=="petri_net"){const auto miner=values.contains("miner")?values["miner"]:"inductive";if(miner=="inductive")net=minera::to_petri_net(minera::discover_process_tree(log));else if(miner=="alpha")net=minera::discover_petri_net(log);else if(miner=="heuristic")net=minera::to_petri_net(minera::discover_heuristics_net(log));else if(miner=="powl")net=minera::to_petri_net(minera::discover_powl(log));else throw std::invalid_argument("unknown context miner");}
   result=minera::build_llm_context(log,options,net?&*net:nullptr);
  }
  std::cout<<minera::llm_context_json(result)<<'\n';return 0;
 }catch(const std::exception& error){std::cerr<<"error: "<<error.what()<<'\n';return 1;}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){std::vector<std::string> values;for(int i=0;i<argc;++i){const auto size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,nullptr,0,nullptr,nullptr);if(size<=0)return 1;std::string value(size,'\0');if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,value.data(),size,nullptr,nullptr))return 1;value.pop_back();values.push_back(std::move(value));}std::vector<char*> arguments;for(auto& value:values)arguments.push_back(value.data());return run_context(argc,arguments.data());}
#else
int main(int argc,char** argv){return run_context(argc,argv);}
#endif
