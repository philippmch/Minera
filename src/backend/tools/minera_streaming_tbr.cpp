#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "minera/csv_event_log_importer.hpp"
#include "minera/pnml.hpp"
#include "minera/streaming_tbr.hpp"

int usage(const char* program) {
  std::cerr << "Usage: " << program
            << " MODEL.pnml INPUT.csv [OUTPUT.json]"
               " [--variant forward|backwards]"
               " [--case-id-column NAME] [--activity-column NAME]"
               " [--max-invisible N] [--stop-immediately]\n";
  return 2;
}

int main(int argc, char** argv) {
  try {
    if (argc < 3) return usage(argv[0]);
    minera::CsvImportOptions import_options;
    minera::StreamingTbrOptions options;
    std::string output_path;
    for (int i = 3; i < argc; ++i) {
      const std::string_view argument(argv[i]);
      auto value_for = [&](const char* name) -> std::string_view {
        if (argument != name || ++i >= argc || std::string_view(argv[i]).empty())
          throw std::invalid_argument(std::string("missing value for ") + name);
        return argv[i];
      };
      if (argument == "--variant") {
        const auto value = value_for("--variant");
        if (value == "forward")
          options.variant = minera::StreamingTbrVariant::kForward;
        else if (value == "backwards")
          options.variant = minera::StreamingTbrVariant::kBackwards;
        else
          throw std::invalid_argument("variant must be forward or backwards");
      } else if (argument == "--case-id-column") {
        import_options.case_id_column =
            std::string(value_for("--case-id-column"));
      } else if (argument == "--activity-column") {
        import_options.activity_column =
            std::string(value_for("--activity-column"));
      } else if (argument == "--max-invisible") {
        const auto value = value_for("--max-invisible");
        std::size_t used = 0;
        const std::string text(value);
        const auto parsed = std::stoull(text, &used);
        if (used != text.size())
          throw std::invalid_argument("max-invisible must be an integer");
        options.maximum_iterations_invisibles =
            static_cast<std::size_t>(parsed);
      } else if (argument == "--stop-immediately") {
        options.stop_immediately_unfit = true;
      } else if (output_path.empty() && !argument.empty() &&
                 argument.front() != '-') {
        output_path = argv[i];
      } else {
        return usage(argv[0]);
      }
    }
    options.activity_attribute = import_options.activity_column;
    options.case_id_attribute = import_options.case_id_column;
    const auto net = minera::import_pnml(argv[1]);
    const auto log = minera::import_csv_event_log(argv[2], import_options);
    minera::StreamingTokenReplay replay(net, options);
    for (const auto& trace : log.traces)
      for (const auto& event : trace.events)
        (void)replay.receive(event, trace.case_id);
    const auto payload = minera::serialize_streaming_tbr_json(replay.snapshot()) +
                         '\n';
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
