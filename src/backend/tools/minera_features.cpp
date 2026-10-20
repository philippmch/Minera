#include "minera/original_features_workbench_json.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int run(int argc,char** argv) {
  std::vector<minera::DiscoveryWorkbenchPart> parts;
  std::string error;
  bool operation_supplied=false;
  const std::map<std::string,std::string> names={
      {"--operation","operation"},{"--source","source_file"},
      {"--source-format","source_format"},{"--result","result_file"},
      {"--options-json","features_options_json"},
      {"--features-options-json","features_options_json"},
      {"--options-file","features_options_file"},
      {"--features-options-file","features_options_file"},
      {"--variant","features_variant"}};
  try {
    for (int i=1;i<argc;++i) {
      const std::string option=argv[i];
      if (option=="--help") {
        std::cout<<
            "Usage: minera_features --source PATH [--variant event_based|trace_based|temporal|INT32 | --options-json FULL56 | --options-file PATH]\n"
            "       minera_features --operation serialize_result --result PATH\n"
            "The original source is complete native EventLog JSON, not a CSV/XES import.\n"
            "Omitting all options invokes the native default argument. --variant changes only the native default variant.\n"
            "Full options require all56 stored fields, including inactive fields and supplied bits.\n"
            "--features-options-json/file are aliases of --options-json/file; duplicate aliases conflict.\n"
            "--source-format native_log is optional. The original result is the bare complete six-field canonical record.\n"
            "stdout is the safe ORIGINAL_LOG_TO_FEATURES envelope with actual File bytes, complete snapshots,\n"
            "native result and independent canonical/legacy artifacts or owned errors. Exit0 success, exit1 owned error.\n";
        return 0;
      }
      const auto found=names.find(option);
      if (found==names.end()) throw std::invalid_argument("unknown original features CLI option: "+option);
      if (i+1>=argc) throw std::invalid_argument(option+" expects a value");
      const std::string value=argv[++i];
      operation_supplied=operation_supplied || found->second=="operation";
      const bool file=found->second.ends_with("_file");
      parts.push_back({found->second,"",file?std::string{}:value,file});
      if (file) {
        const auto path=std::filesystem::u8path(value);
        const auto filename=path.filename().u8string();
        parts.back().filename=std::string(filename.begin(),filename.end());
        std::ifstream input(path,std::ios::binary);
        if (!input) throw std::invalid_argument("cannot open original features File: "+value);
        std::ostringstream bytes;bytes<<input.rdbuf();
        if (input.bad()) throw std::invalid_argument("cannot read original features File: "+value);
        parts.back().bytes=bytes.str();
      }
    }
  } catch (const std::exception& exception) { error=exception.what(); }
  if (!operation_supplied) parts.push_back({"operation","","extract_features",false});
  const auto result=minera::execute_original_features_workbench(parts,{},"provided_order",error);
  std::cout<<result.json<<'\n';
  return result.status==200?0:1;
}
} // namespace

#ifdef _WIN32
// Retain UTF16 lone surrogates as WTF8 instead of using an ACP conversion.
int wmain(int argc,wchar_t** argv) {
  std::vector<std::string> values;
  const auto append=[](std::string& out,unsigned code) {
    if (code<0x80) out+=static_cast<char>(code);
    else if (code<0x800) {
      out+=static_cast<char>(0xc0|(code>>6));out+=static_cast<char>(0x80|(code&63));
    } else if (code<0x10000) {
      out+=static_cast<char>(0xe0|(code>>12));out+=static_cast<char>(0x80|((code>>6)&63));
      out+=static_cast<char>(0x80|(code&63));
    } else {
      out+=static_cast<char>(0xf0|(code>>18));out+=static_cast<char>(0x80|((code>>12)&63));
      out+=static_cast<char>(0x80|((code>>6)&63));out+=static_cast<char>(0x80|(code&63));
    }
  };
  for (int i=0;i<argc;++i) {
    std::string value;
    for (std::size_t j=0;argv[i][j];++j) {
      unsigned code=static_cast<unsigned short>(argv[i][j]);
      if (code>=0xd800 && code<=0xdbff) {
        const unsigned low=static_cast<unsigned short>(argv[i][j+1]);
        if (low>=0xdc00 && low<=0xdfff) { code=0x10000+((code-0xd800)<<10)+low-0xdc00;++j; }
      }
      append(value,code);
    }
    values.push_back(std::move(value));
  }
  std::vector<char*> pointers;
  for (auto& value:values) pointers.push_back(value.data());
  return run(argc,pointers.data());
}
#else
int main(int argc,char** argv) { return run(argc,argv); }
#endif
