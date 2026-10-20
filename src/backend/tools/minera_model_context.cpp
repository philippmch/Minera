#include "minera/original_model_context_workbench_json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <string>
#include <string_view>
#include <vector>
namespace {
int run(int argc,char** argv) {
  std::vector<minera::DiscoveryWorkbenchPart> parts;
  std::string error;
  const std::map<std::string,std::string> names={
      {"--model","model_file"},{"--variant","variant"},{"--model-format","model_format"},
      {"--include-header","include_header"},{"--response-header","response_header"},
      {"--options-json","context_options_json"},{"--options-file","context_options_file"},
      {"--initial-marking-json","initial_marking_json"},{"--initial-marking-file","initial_marking_file"},
      {"--final-marking-json","final_marking_json"},{"--final-marking-file","final_marking_file"},
      {"--pnml-import-options-json","pnml_import_options_json"},{"--pnml-import-options-file","pnml_import_options_file"}};
  try {
    for(int i=1;i<argc;++i) {
      const std::string option=argv[i];
      if(option=="--help") {
        std::cerr<<"Usage: minera_model_context --model PATH --variant declare|log_skeleton|temporal_profile|petri_net [--model-format auto|declare_json|log_skeleton_json|temporal_profile_json|native_net|native_document|pnml] [--include-header true|false | --response-header true|false | --options-json JSON | --options-file PATH] [--initial-marking-json JSON | --initial-marking-file PATH] [--final-marking-json JSON | --final-marking-file PATH] [--pnml-import-options-json JSON | --pnml-import-options-file PATH]\n"
          "The four direct formatters describe the supplied original model without rediscovery or output truncation. Full native_net requires both explicit signed arbitrary-width TokenMarking arrays. Header options contain exactly include_header. PNML import options require all five native booleans. stdout is the full byte-safe LLM_CONTEXT original_model envelope. Original/effective states and optional export errors remain separate.\n";
        return 0;
      }
      const auto found=names.find(option);
      if(found==names.end()) throw std::invalid_argument("unknown original model context CLI option: "+option);
      if(i+1>=argc) throw std::invalid_argument(option+" expects a value");
      const std::string value=argv[++i];
      const bool file=found->second.ends_with("_file");
      parts.push_back({found->second,"",file?std::string{}:value,file});
      if(file) {
        const auto path=std::filesystem::u8path(value);
        const auto filename=path.filename().u8string();
        parts.back().filename=std::string(filename.begin(),filename.end());
        std::ifstream input(path,std::ios::binary);
        if(!input) throw std::invalid_argument("cannot open original model context input File: "+value);
        std::ostringstream bytes;bytes<<input.rdbuf();
        if(input.bad()) throw std::invalid_argument("cannot read original model context input File: "+value);
        parts.back().bytes=bytes.str();
      }
    }
  } catch(const std::exception& e) {error=e.what();}
  const auto result=minera::execute_original_model_context(parts,{},"provided_order",error);
  std::cout<<result.json<<'\n';
  return result.status==200?0:1;
}
} // namespace
#ifdef _WIN32
// Preserve UTF16 lone surrogates as WTF8, just as NativeString JSON does.
int wmain(int argc,wchar_t** argv) {
  std::vector<std::string> values;
  const auto append=[](std::string& out,unsigned code) {
    if(code<0x80)out+=static_cast<char>(code);
    else if(code<0x800){out+=static_cast<char>(0xc0|(code>>6));out+=static_cast<char>(0x80|(code&63));}
    else if(code<0x10000){out+=static_cast<char>(0xe0|(code>>12));out+=static_cast<char>(0x80|((code>>6)&63));out+=static_cast<char>(0x80|(code&63));}
    else{out+=static_cast<char>(0xf0|(code>>18));out+=static_cast<char>(0x80|((code>>12)&63));out+=static_cast<char>(0x80|((code>>6)&63));out+=static_cast<char>(0x80|(code&63));}
  };
  for(int i=0;i<argc;++i){std::string value;for(std::size_t j=0;argv[i][j];++j){unsigned code=static_cast<unsigned short>(argv[i][j]);if(code>=0xd800&&code<=0xdbff){const unsigned low=static_cast<unsigned short>(argv[i][j+1]);if(low>=0xdc00&&low<=0xdfff){code=0x10000+((code-0xd800)<<10)+low-0xdc00;++j;}}append(value,code);}values.push_back(std::move(value));}
  std::vector<char*> pointers;for(auto& value:values)pointers.push_back(value.data());return run(argc,pointers.data());
}
#else
int main(int argc,char** argv){return run(argc,argv);}
#endif
