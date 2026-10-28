#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "minera/csv_event_log_importer.hpp"
#include "minera/python_csv_stream.hpp"
#include "minera/streaming_temporal.hpp"
#include "../full_double.hpp"

namespace {

std::vector<std::string> split(std::string_view value, char separator) {
  std::vector<std::string> result;
  std::size_t begin = 0;
  while (begin <= value.size()) {
    const auto end = value.find(separator, begin);
    const auto token = value.substr(
        begin, end == std::string_view::npos ? value.size() - begin
                                               : end - begin);
    if (token.empty()) throw std::invalid_argument("empty list item");
    result.emplace_back(token);
    if (end == std::string_view::npos) break;
    begin = end + 1;
  }
  return result;
}

// One complete number token; subnormal values such as 5e-324 are numbers.
double parse_number(std::string_view value, const char* name) {
  const auto parsed = minera::detail::parse_full_double(value);
  if (!parsed) throw std::invalid_argument(std::string(name) + " must be numeric");
  return *parsed;
}

void parse_profile(minera::TemporalProfile& profile, std::string_view value) {
  for (const auto& item : split(value, ',')) {
    const auto arrow = item.find("->");
    const auto first_colon = item.find(':');
    const auto second_colon = item.find(':', first_colon == std::string::npos
                                                  ? 0
                                                  : first_colon + 1);
    if (arrow == std::string::npos || first_colon == std::string::npos ||
        second_colon == std::string::npos || arrow == 0 ||
        first_colon <= arrow + 2 || second_colon <= first_colon + 1 ||
        second_colon + 1 >= item.size()) {
      throw std::invalid_argument(
          "profile entries must use source->target:mean:stddev");
    }
    minera::TemporalProfileEntry entry;
    entry.source = item.substr(0, arrow);
    entry.target = item.substr(arrow + 2, first_colon - arrow - 2);
    entry.mean_seconds = parse_number(
        std::string_view(item).substr(first_colon + 1,
                                      second_colon - first_colon - 1),
        "profile mean");
    entry.standard_deviation_seconds = parse_number(
        std::string_view(item).substr(second_colon + 1), "profile stddev");
    entry.count = 1;
    profile.entries.push_back(std::move(entry));
  }
}

int usage(const char* program) {
  std::cerr << "Usage: " << program
            << " INPUT.csv [OUTPUT.json] --profile A->B:10:2"
               " [--zeta N] [--case-id-column NAME]"
               " [--activity-column NAME] [--timestamp-column NAME]"
               " [--start-timestamp-column NAME]\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 4) return usage(argv[0]);
    minera::CsvImportOptions import_options;
    minera::StreamingTemporalOptions options;
    minera::TemporalProfile profile;
    std::string output_path;
    bool have_profile = false;
    for (int i = 2; i < argc; ++i) {
      const std::string_view option(argv[i]);
      auto value_for = [&](const char* name) -> std::string_view {
        if (option != name || ++i >= argc || std::string_view(argv[i]).empty())
          throw std::invalid_argument(std::string("missing value for ") + name);
        return argv[i];
      };
      if (option == "--profile") {
        parse_profile(profile, value_for("--profile"));
        have_profile = true;
      } else if (option == "--zeta") {
        options.zeta = parse_number(value_for("--zeta"), "zeta");
      } else if (option == "--case-id-column") {
        import_options.case_id_column = std::string(value_for("--case-id-column"));
      } else if (option == "--activity-column") {
        import_options.activity_column = std::string(value_for("--activity-column"));
      } else if (option == "--timestamp-column") {
        import_options.timestamp_column = std::string(value_for("--timestamp-column"));
      } else if (option == "--start-timestamp-column") {
        options.start_timestamp_attribute =
            std::string(value_for("--start-timestamp-column"));
      } else if (output_path.empty() && !option.empty() && option.front() != '-') {
        output_path = argv[i];
      } else {
        return usage(argv[0]);
      }
    }
    if (!have_profile) return usage(argv[0]);
    options.case_id_attribute = import_options.case_id_column;
    options.activity_attribute = import_options.activity_column;
    if (options.timestamp_attribute.empty())
      options.timestamp_attribute = import_options.timestamp_column;
    minera::StreamingTemporalConformance monitor(std::move(profile), options);
    // Events arrive in file order as the reference CSV event stream reads
    // them, with timestamp cells parsed; a missing or None case skips one.
    const auto records = minera::read_python_csv_event_stream(argv[1]);
    for (std::size_t i = 0; i < records.size(); ++i) {
      const auto* case_id = records[i].find(import_options.case_id_column);
      if (!case_id || !case_id->has_value()) continue;
      (void)monitor.receive(minera::python_csv_record_event(records[i], i), **case_id);
    }
    const auto payload =
        minera::serialize_streaming_temporal_json(monitor.snapshot()) + '\n';
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
