#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "minera/csv_event_log_importer.hpp"
#include "minera/python_csv_stream.hpp"
#include "minera/streaming_dfg.hpp"
#include "minera/streaming_store.hpp"

namespace {

int usage(const char* program) {
  std::cerr << "Usage: " << program
            << " INPUT.csv [OUTPUT.json]"
               " [--case-id-column NAME] [--activity-column NAME]"
               " [--state STATE.csv]\n"
               "  --case-id-column defaults to case_id and --activity-column to activity;\n"
               "  the reference streaming DFG reads case:concept:name and concept:name, so\n"
               "  pass those names for a pm4py-style CSV.\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 2 || argc > 9) return usage(argv[0]);
    minera::CsvImportOptions import_options;
    std::string output_path;
    std::string state_path;
    for (int i = 2; i < argc; ++i) {
      const std::string_view option(argv[i]);
      if (option == "--case-id-column" || option == "--activity-column" ||
          option == "--state") {
        if (++i >= argc || std::string_view(argv[i]).empty())
          return usage(argv[0]);
        if (option == "--case-id-column")
          import_options.case_id_column = argv[i];
        else if (option == "--activity-column")
          import_options.activity_column = argv[i];
        else
          state_path = argv[i];
      } else if (output_path.empty() && !option.empty() &&
                 option.front() != '-') {
        output_path = argv[i];
      } else {
        return usage(argv[0]);
      }
    }
    // Events arrive in file order as the reference CSV event stream reads
    // them; one without the case column is skipped.
    const auto records = minera::read_python_csv_event_stream(argv[1]);
    minera::StreamingDfgDiscovery stream(
        {import_options.activity_column, import_options.case_id_column});
    std::optional<minera::PersistentStreamingDfg> persistent;
    if (!state_path.empty()) {
      persistent.emplace(
          state_path,
          minera::StreamingDfgOptions{import_options.activity_column,
                                      import_options.case_id_column});
    }
    for (std::size_t i = 0; i < records.size(); ++i) {
      const auto case_id = minera::python_csv_case_key(records[i], import_options.case_id_column);
      if (!case_id) continue;
      const auto event = minera::python_csv_record_event(records[i], i);
      if (persistent)
        persistent->receive(event, *case_id);
      else
        stream.receive(event, *case_id);
    }
    const auto snapshot = persistent ? persistent->snapshot() : stream.snapshot();
    const auto payload =
        minera::serialize_streaming_dfg_json(snapshot) + '\n';
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
