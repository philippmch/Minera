#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "minera/csv_event_log_importer.hpp"
#include "minera/dfg_io.hpp"
#include "minera/dfg_precision.hpp"

int main(int argc, char** argv) {
  try {
    if (argc < 3 || argc > 6) {
      std::cerr << "Usage: " << argv[0]
                << " INPUT.csv MODEL.dfg [OUTPUT.json]"
                   " [--activity-column NAME]\n";
      return 2;
    }
    minera::CsvImportOptions import_options;
    std::string output_path;
    for (int i = 3; i < argc; ++i) {
      const std::string_view argument(argv[i]);
      if (argument == "--activity-column") {
        if (++i >= argc || std::string_view(argv[i]).empty())
          throw std::invalid_argument("--activity-column requires a name");
        import_options.activity_column = argv[i];
      } else if (output_path.empty() && !argument.empty() &&
                 argument.front() != '-') {
        output_path = argv[i];
      } else {
        throw std::invalid_argument("unknown or duplicate argument: " +
                                    std::string(argument));
      }
    }
    minera::DfgPrecisionOptions options;
    options.activity_attribute = import_options.activity_column;
    const auto log = minera::import_csv_event_log(argv[1], import_options);
    const auto model = minera::import_dfg(argv[2]);
    const auto payload =
        minera::serialize_dfg_precision_json(
            minera::evaluate_dfg_precision(log, model, options)) + '\n';
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
