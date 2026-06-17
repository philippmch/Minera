// Runs the Minera core on a CSV event log and emits the analysis (event log
// plus discovered directly-follows graph) as JSON for the frontend.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

#include "minera/csv_event_log_importer.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/json_export.hpp"

namespace {

int usage(const char* program) {
  std::cerr << "Usage: " << program
            << " <event_log.csv> [options]\n"
               "  -o <path>                 Write JSON to <path> instead of stdout\n"
               "  --case-column <name>      Case id column (default: case_id)\n"
               "  --activity-column <name>  Activity column (default: activity)\n"
               "  --timestamp-column <name> Timestamp column (default: timestamp)\n"
               "  --delimiter <char>        Field delimiter (default: ,)\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path input;
  std::filesystem::path output;
  minera::CsvImportOptions options;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    const auto next = [&]() -> const char* {
      return i + 1 < argc ? argv[++i] : nullptr;
    };

    if (arg == "-o") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      output = value;
    } else if (arg == "--case-column") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      options.case_id_column = value;
    } else if (arg == "--activity-column") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      options.activity_column = value;
    } else if (arg == "--timestamp-column") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      options.timestamp_column = value;
    } else if (arg == "--delimiter") {
      const auto* value = next();
      if (!value || value[0] == '\0') return usage(argv[0]);
      options.delimiter = value[0];
    } else if (!arg.empty() && arg.front() == '-') {
      return usage(argv[0]);
    } else if (input.empty()) {
      input = arg;
    } else {
      return usage(argv[0]);
    }
  }

  if (input.empty()) {
    return usage(argv[0]);
  }

  try {
    const auto log = minera::import_csv_event_log(input, options);
    const auto graph = minera::discover_directly_follows_graph(log);
    const auto tree = minera::discover_process_tree(log);
    const auto dataset = input.filename().string();

    if (output.empty()) {
      minera::write_analysis_json(std::cout, dataset, log, graph, &tree);
    } else {
      std::ofstream file(output);
      if (!file) {
        std::cerr << "error: cannot open output file " << output << '\n';
        return 1;
      }
      minera::write_analysis_json(file, dataset, log, graph, &tree);
    }
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }

  return 0;
}
