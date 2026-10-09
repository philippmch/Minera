#include "minera/source_acquisition.hpp"
#include "minera/xes_export.hpp"
#include "repository_history_runtime.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv) {
  try {
    if(argc<2){std::cerr<<"Usage: minera_acquire_source CONNECTOR [PATH] [--param key=value] [--token-env NAME] [--output-xes PATH]\n";return 2;}
    minera::SourceAcquisitionInput input;input.connector=argv[1];std::string output;
    for(int i=2;i<argc;++i){const std::string key=argv[i];if(!key.starts_with("--")){if(!input.server_path.empty())throw std::invalid_argument("only one source path is accepted");input.server_path=key;continue;}if(++i>=argc)throw std::invalid_argument("missing option value");const std::string value=argv[i];
      if(key=="--param"){const auto separator=value.find('=');if(separator==std::string::npos||separator==0||!input.parameters.emplace(value.substr(0,separator),value.substr(separator+1)).second)throw std::invalid_argument("native parameters require unique key=value entries");}
      else if(key=="--token-env"){const auto* token=std::getenv(value.c_str());if(!token||!*token)throw std::invalid_argument("the named token environment variable is empty");input.auth_token=token;}
      else if(key=="--output-xes")output=value;else throw std::invalid_argument("unknown option: "+key);
    }
    input.repository_transport=minera::tools::repository_history_http_transport();const auto result=minera::acquire_source(input);
    if(!output.empty()){minera::XesExportOptions options;options.preserve_nulls=true;(void)minera::export_xes_event_log(std::filesystem::u8path(output),result.log,options);}
    std::cout<<minera::source_acquisition_json(result)<<'\n';
  }catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}return 0;
}
