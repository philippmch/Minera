#include "minera/original_label_splitting_workbench_json.hpp"

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
      {"--options-json","label_splitting_options_json"},
      {"--label-splitting-options-json","label_splitting_options_json"},
      {"--options-file","label_splitting_options_file"},
      {"--label-splitting-options-file","label_splitting_options_file"}};
  try {
    for (int i=1;i<argc;++i) {
      const std::string option=argv[i];
      if (option=="--help") {
        std::cout<<
            "Usage: minera_label_splitting --source PATH [--operation split_activity_labels|contextual_label_splitting] [--options-json FULL8 | --options-file PATH]\n"
            "       minera_label_splitting --operation serialize_result --result PATH\n"
            "Original source is complete native EventLog JSON; no cached source or import conversion.\n"
            "Omitting options invokes the actual native default argument; complete options require all eight stored fields.\n"
            "--label-splitting-options-json/file alias --options-json/file; duplicate aliases conflict.\n"
            "--source-format native_log is optional. Result File is bare {variant,log}, with the complete native log.\n"
            "stdout is the safe ORIGINAL_LABEL_SPLITTING envelope with actual File bytes, snapshots and five artifacts,\n"
            "including bare result_log_json for another original-source handoff. Exit0 success; exit1 owned error.\n";
        return 0;
      }
      const auto found=names.find(option);
      if (found==names.end()) throw std::invalid_argument("unknown original label splitting CLI option: "+option);
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
        if (!input) throw std::invalid_argument("cannot open original label splitting File: "+value);
        std::ostringstream bytes;bytes<<input.rdbuf();
        if (input.bad()) throw std::invalid_argument("cannot read original label splitting File: "+value);
        parts.back().bytes=bytes.str();
      }
    }
  } catch (const std::exception& exception) { error=exception.what(); }
  if (!operation_supplied) parts.push_back({"operation","","split_activity_labels",false});
  const auto result=minera::execute_original_label_splitting_workbench(parts,{},"provided_order",error);
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
