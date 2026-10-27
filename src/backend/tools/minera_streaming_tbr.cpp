#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "minera/csv_event_log_importer.hpp"
#include "minera/pnml.hpp"
#include "minera/python_csv_stream.hpp"
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
    // The forward variant reads the document's markings as the reference
    // PNML importer does; backwards keeps the workflow source and sink.
    minera::PnmlImportOptions pnml_options;
    if (options.variant == minera::StreamingTbrVariant::kForward)
      pnml_options.public_marking_semantics = true;
    const auto document = minera::import_pnml_document(argv[1], pnml_options);
    if (options.variant == minera::StreamingTbrVariant::kForward) {
      for (const auto& tokens : document.initial_marking)
        options.initial_marking.push_back(static_cast<int>(tokens));
      for (const auto& tokens : document.final_marking)
        options.final_marking.push_back(static_cast<int>(tokens));
      if (document.arc_occurrences) {
        options.input_place_order.resize(document.net.transitions().size());
        for (const auto& arc : *document.arc_occurrences) {
          auto& order = options.input_place_order[arc.transition];
          if (arc.input && std::find(order.begin(), order.end(), arc.place) == order.end())
            order.push_back(arc.place);
        }
      }
    }
    minera::StreamingTokenReplay replay(document.net, options);
    // Events arrive in file order as the reference CSV event stream reads
    // them; one whose case is missing or None is skipped.
    const auto records = minera::read_python_csv_event_stream(argv[2]);
    for (std::size_t i = 0; i < records.size(); ++i) {
      const auto* case_id = records[i].find(import_options.case_id_column);
      if (!case_id || !case_id->has_value()) continue;
      (void)replay.receive(minera::python_csv_record_event(records[i], i), **case_id);
    }
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
