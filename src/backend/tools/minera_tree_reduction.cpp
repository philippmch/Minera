#include "minera/tree_reduction_workbench_json.hpp"
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
      {"--model","model_file"},{"--trace","trace_file"},
      {"--model-format","model_format"},{"--trace-format","trace_format"},
      {"--options-json","tree_reduction_options_json"},
      {"--options-file","tree_reduction_options_file"},
      {"--variant","tree_reduction_variant"},{"--activity-key","tree_reduction_activity_key"},
      {"--activity-key-file","tree_reduction_activity_key_file"}};
  try {
    for(int i=1;i<argc;++i) {
      const std::string option=argv[i];
      if(option=="--help") {
        std::cerr<<"Usage: minera_tree_reduction --model PATH --trace PATH [--model-format auto|ptml|tree_json] [--trace-format native_trace|trace_words] [--options-json JSON | --options-file PATH | --variant TREE_TR_BASED --activity-key TEXT | --activity-key-file PATH]\n"
          "Complete options require variant_index and activity_key. native_trace uses complete raw Trace state with per-leaf NativeString; trace_words is an explicit convenience array. File bytes and all supplied controls are retained. stdout is the complete MODEL_CONVERSION_WORKBENCH envelope; optional export errors do not change the native result.\n";
        return 0;
      }
      const auto found=names.find(option);
      if(found==names.end()) throw std::invalid_argument("unknown tree reduction CLI option: "+option);
      if(i+1>=argc) throw std::invalid_argument(option+" expects a value");
      const std::string value=argv[++i];
      const bool file=found->second.ends_with("_file");
      parts.push_back({found->second,"",file?std::string{}:value,file});
      if(file) {
        const auto path=std::filesystem::u8path(value);
        const auto filename=path.filename().u8string();
        parts.back().filename=std::string(filename.begin(),filename.end());
        std::ifstream input(path,std::ios::binary);
        if(!input) throw std::invalid_argument("cannot open tree reduction input File: "+value);
        std::ostringstream bytes;bytes<<input.rdbuf();
        if(input.bad()) throw std::invalid_argument("cannot read tree reduction input File: "+value);
        parts.back().bytes=bytes.str();
      }
    }
  } catch(const std::exception& e) {error=e.what();}
  const auto result=minera::execute_tree_reduction_workbench(parts,{},"provided_order",error);
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
