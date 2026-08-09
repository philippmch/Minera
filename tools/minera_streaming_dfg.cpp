#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "minera/csv_event_log_importer.hpp"
#include "minera/streaming_dfg.hpp"

namespace {

int usage(const char* program) {
  std::cerr << "Usage: " << program
            << " INPUT.csv [OUTPUT.json]"
               " [--case-id-column NAME] [--activity-column NAME]\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 2 || argc > 7) return usage(argv[0]);
    minera::CsvImportOptions import_options;
    std::string output_path;
    for (int i = 2; i < argc; ++i) {
      const std::string_view option(argv[i]);
      if (option == "--case-id-column" || option == "--activity-column") {
        if (++i >= argc || std::string_view(argv[i]).empty())
          return usage(argv[0]);
        if (option == "--case-id-column")
          import_options.case_id_column = argv[i];
        else
          import_options.activity_column = argv[i];
      } else if (output_path.empty() && !option.empty() &&
                 option.front() != '-') {
        output_path = argv[i];
      } else {
        return usage(argv[0]);
      }
    }
    const auto log = minera::import_csv_event_log(argv[1], import_options);
    minera::StreamingDfgDiscovery stream(
        {import_options.activity_column, import_options.case_id_column});
    for (const auto& trace : log.traces)
      for (const auto& event : trace.events)
        stream.receive(event, trace.case_id);
    const auto payload =
        minera::serialize_streaming_dfg_json(stream.snapshot()) + '\n';
    if (output_path.empty()) {
      std::cout << payload;
    } else {
      std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
      if (!output) throw std::runtime_error("cannot open output file");
      output << payload;
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
