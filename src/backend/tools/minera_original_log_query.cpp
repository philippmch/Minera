#include "minera/original_log_query_workbench_json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

namespace {
std::string read_file(const std::string& path) {
  std::ifstream input(std::filesystem::u8path(path),std::ios::binary);
  if (!input) throw std::runtime_error("could not read original input File");
  return std::string(std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>());
}
std::string basename(const std::string& path) {
  const auto bytes=std::filesystem::u8path(path).filename().u8string();
  return std::string(reinterpret_cast<const char*>(bytes.data()),bytes.size());
}
void usage() {
  std::cout<<"minera_original_log_query --operation OP [original controls]\n"
      "Operations: log_query_table, query_log_sqlite, filter_query_events, "
      "log_query_json, log_query_table_json, sqlite_log_query_available\n"
      "--source FILE  --table FILE  --result FILE\n"
      "--sql TEXT | --sql-file FILE\n"
      "--query-options-json JSON | --query-options-file FILE\n"
      "--indices-json JSON | --indices-file FILE  --whole-cases true|false\n"
      "Omitted query options/filter whole-cases call the actual native defaults.\n"
      "stdout is one byte-safe complete report; exit1 preserves owned errors.\n";
}
}
int main(int argc,char** argv) {
#if defined(_WIN32)
  _setmode(_fileno(stdout),_O_BINARY);
  _setmode(_fileno(stderr),_O_BINARY);
#endif
  std::vector<std::string> physical_argv;
  for (int i=0;i<argc;++i) physical_argv.emplace_back(argv[i]);
  if (argc==2&&std::string(argv[1])=="--help") { usage();return 0; }
  static const std::map<std::string,std::string> controls={
      {"--operation","operation"},{"--source","source_file"},{"--table","table_file"},
      {"--result","result_file"},{"--sql","sql"},{"--sql-file","sql_file"},
      {"--query-options-json","query_options_json"},{"--query-options-file","query_options_file"},
      {"--indices-json","indices_json"},{"--indices-file","indices_file"},{"--whole-cases","whole_cases"}};
  struct Pending { std::string name,value; bool file; };
  std::vector<Pending> pending;
  std::vector<minera::DiscoveryWorkbenchPart> parts;
  std::string transport_error;
  const auto fail=[&](std::string message) {
    if (transport_error.empty()) transport_error=std::move(message);
  };
  // Collect every option position before any file I/O. A failed File read
  // must not hide a later unique operation or duplicate operation control.
  for (int i=1;i<argc;i+=2) {
    const auto found=controls.find(argv[i]);
    if (found==controls.end()) { fail("unknown original log query CLI flag");continue; }
    if (i+1==argc) { fail("original log query CLI flag requires a value");break; }
    pending.push_back({found->second,argv[i+1],found->second.ends_with("_file")});
  }
  for (const auto& control : pending) {
    if (!control.file) { parts.push_back({control.name,"",control.value,false});continue; }
    try {
      auto bytes=read_file(control.value);
      parts.push_back({control.name,basename(control.value),std::move(bytes),true});
    } catch (const std::exception& error) {
      // No actual File bytes are available: do not fabricate an empty File
      // part. The full physical path remains authoritative in cli.argv.
      fail(error.what());
    }
  }
  const auto response=minera::execute_original_log_query_workbench(parts,{},
      "provided_order",transport_error,&physical_argv);
  std::cout.write(response.json.data(),static_cast<std::streamsize>(response.json.size()));
  std::cout.put('\n');
  return response.status==200?0:1;
}
