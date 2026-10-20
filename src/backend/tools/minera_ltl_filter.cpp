#include "minera/exact_filter_discovery_json.hpp"
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
  const std::map<std::string,std::string> names={{"--ltl-options-json","ltl_options_json"},{"--ltl-options-file","ltl_options_file"},{"--ltl-attribute-values-json","ltl_attribute_values_json"},{"--ltl-attribute-values-file","ltl_attribute_values_file"},{"--ltl-attribute-values","ltl_attribute_values"},{"--source","source_file"},{"--source-format","source_format"},{"--values-json","ltl_attribute_values_json"},{"--values-file","ltl_attribute_values_file"},{"--options-json","ltl_options_json"},{"--options-file","ltl_options_file"},{"--ltl-variant","ltl_variant"},{"--ltl-attribute-key","ltl_attribute_key"},{"--ltl-positive","ltl_positive"},{"--ltl-enable-timestamp","ltl_enable_timestamp"},{"--ltl-timestamp-diff-boundaries","ltl_timestamp_diff_boundaries"},{"--ltl-resource-key","ltl_resource_key"},{"--mining-input-format","mining_input_format"},{"--mining-input-shape","mining_input_shape"},{"--mining-selectors-json","mining_selectors_json"},{"--case-column","case_column"},{"--activity-column","activity_column"},{"--timestamp-column","timestamp_column"},{"--resource-column","resource_column"},{"--lifecycle-column","lifecycle_column"},{"--delimiter","delimiter"},{"--xes-variant","xes_variant"},{"--xes-case-attribute","xes_case_attribute"},{"--xes-activity-attribute","xes_activity_attribute"},{"--xes-timestamp-attribute","xes_timestamp_attribute"},{"--xes-resource-attribute","xes_resource_attribute"},{"--xes-lifecycle-attribute","xes_lifecycle_attribute"},{"--xes-require-timestamp","xes_require_timestamp"},{"--xes-timestamp-sort","xes_timestamp_sort"},{"--xes-reverse-sort","xes_reverse_sort"},{"--xes-max-traces","xes_max_traces"},{"--xes-attributes-to-read","xes_attributes_to_read"},{"--xes-max-decompressed-bytes","xes_max_decompressed_bytes"}};
  try {
    for(int i=1;i<argc;++i) {
      const std::string option=argv[i];
      if(option=="--help") {
        std::cerr<<"Usage: minera_ltl_filter --source PATH [--source-format native_log|import] [--options-json JSON|--options-file PATH|--values-json JSON|--values-file PATH] [--ltl-variant eventually_follows|a_next_b_next_c|four_eyes|different_persons] [--ltl-attribute-key TEXT] [--ltl-resource-key TEXT] [--ltl-positive BOOL] [--ltl-enable-timestamp BOOL] [--ltl-timestamp-diff-boundaries TEXT]\nLong exact flags --ltl-options-json/file and --ltl-attribute-values-json/file are equivalent aliases. Complete seven-field options and NativeString Files preserve all native states. Native semantic errors return full ORIGINAL_LTL_FILTER stdout envelope and exit1. Import controls use the existing mining/XES flag names.\n";
        return 0;
      }
      const auto found=names.find(option);
      if(found==names.end()) throw std::invalid_argument("unknown original LTL CLI option: "+option);
      if(i+1>=argc) throw std::invalid_argument(option+" expects a value");
      const std::string value=argv[++i];
      const bool file=found->second.ends_with("_file");
      parts.push_back({found->second,"",file?std::string{}:value,file});
      if(file) {
        const auto path=std::filesystem::u8path(value);
        const auto filename=path.filename().u8string();
        parts.back().filename=std::string(filename.begin(),filename.end());
        std::ifstream input(path,std::ios::binary);
        if(!input) throw std::invalid_argument("cannot open original LTL input File: "+value);
        std::ostringstream bytes;bytes<<input.rdbuf();
        if(input.bad()) throw std::invalid_argument("cannot read original LTL input File: "+value);
        parts.back().bytes=bytes.str();
      }
    }
  } catch(const std::exception& e) {error=e.what();}
  const auto result=minera::execute_exact_ltl_filter(parts,{},"provided_order",error);
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
