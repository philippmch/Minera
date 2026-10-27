#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <set>
#include <utility>
#include <vector>

#include "minera/csv_event_log_importer.hpp"
#include "minera/python_csv_stream.hpp"
#include "minera/streaming_footprints.hpp"

namespace {

std::vector<std::string> split_csv(std::string_view value) {
  std::vector<std::string> result;
  std::size_t begin = 0;
  while (begin <= value.size()) {
    const auto end = value.find(',', begin);
    const auto token = value.substr(begin, end == std::string_view::npos
                                             ? value.size() - begin
                                             : end - begin);
    if (token.empty()) throw std::invalid_argument("empty list item");
    result.emplace_back(token);
    if (end == std::string_view::npos) break;
    begin = end + 1;
  }
  return result;
}

void add_pairs(std::set<minera::FootprintPair>& target,
               std::string_view value) {
  for (const auto& item : split_csv(value)) {
    const auto separator = item.find("->");
    if (separator == std::string::npos || separator == 0 ||
        separator + 2 >= item.size() ||
        item.find("->", separator + 2) != std::string::npos) {
      throw std::invalid_argument("relations must use source->target");
    }
    target.emplace(item.substr(0, separator), item.substr(separator + 2));
  }
}

int usage(const char* program) {
  std::cerr << "Usage: " << program
            << " INPUT.csv [OUTPUT.json] --model-activities A,B"
               " [--model-start A,B] [--model-end A,B]"
               " [--model-sequence A->B,C->D]"
               " [--model-parallel A->B,C->D]"
               " [--case-id-column NAME] [--activity-column NAME]"
               " [--terminate-all]\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 4) return usage(argv[0]);
    minera::CsvImportOptions import_options;
    minera::Footprint model;
    model.start_activities_present = false;
    model.end_activities_present = false;
    std::string output_path;
    bool terminate_all = false;
    bool have_activities = false;
    for (int i = 2; i < argc; ++i) {
      const std::string_view option(argv[i]);
      auto value_for = [&](const char* name) -> std::string_view {
        if (option != name || ++i >= argc || std::string_view(argv[i]).empty())
          throw std::invalid_argument(std::string("missing value for ") + name);
        return argv[i];
      };
      if (option == "--model-activities") {
        for (const auto& activity : split_csv(value_for("--model-activities")))
          model.activities.insert(activity);
        have_activities = true;
      } else if (option == "--model-start") {
        model.start_activities_present = true;
        for (const auto& activity : split_csv(value_for("--model-start")))
          model.start_activities.insert(activity);
      } else if (option == "--model-end") {
        model.end_activities_present = true;
        for (const auto& activity : split_csv(value_for("--model-end")))
          model.end_activities.insert(activity);
      } else if (option == "--model-sequence") {
        add_pairs(model.sequence, value_for("--model-sequence"));
      } else if (option == "--model-parallel") {
        add_pairs(model.parallel, value_for("--model-parallel"));
      } else if (option == "--case-id-column") {
        import_options.case_id_column = std::string(value_for("--case-id-column"));
      } else if (option == "--activity-column") {
        import_options.activity_column = std::string(value_for("--activity-column"));
      } else if (option == "--terminate-all") {
        terminate_all = true;
      } else if (output_path.empty() && !option.empty() && option.front() != '-') {
        output_path = argv[i];
      } else {
        return usage(argv[0]);
      }
    }
    if (!have_activities) return usage(argv[0]);
    for (const auto& [source, target] : model.sequence)
      model.activities.insert(source), model.activities.insert(target);
    for (const auto& [source, target] : model.parallel)
      model.activities.insert(source), model.activities.insert(target);
    for (const auto& activity : model.start_activities)
      model.activities.insert(activity);
    for (const auto& activity : model.end_activities)
      model.activities.insert(activity);

    // Events arrive in file order as the reference CSV event stream reads
    // them; one whose case is missing or None is skipped.
    const auto records = minera::read_python_csv_event_stream(argv[1]);
    minera::StreamingFootprintsConformance monitor(
        std::move(model),
        {import_options.activity_column, import_options.case_id_column});
    for (std::size_t i = 0; i < records.size(); ++i) {
      const auto* case_id = records[i].find(import_options.case_id_column);
      if (!case_id || !case_id->has_value()) continue;
      (void)monitor.receive(minera::python_csv_record_event(records[i], i), **case_id);
    }
    if (terminate_all) monitor.terminate_all();
    const auto payload =
        minera::serialize_streaming_footprints_json(monitor.snapshot()) + '\n';
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
