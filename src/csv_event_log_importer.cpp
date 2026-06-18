#include "minera/csv_event_log_importer.hpp"

#include <algorithm>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace minera {
namespace {

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return {};
  }

  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::vector<std::string> parse_csv_row(
    std::string_view row,
    char delimiter,
    bool trim_fields) {
  std::vector<std::string> fields;
  std::string field;
  bool in_quotes = false;

  for (std::size_t i = 0; i < row.size(); ++i) {
    const char ch = row[i];
    if (ch == '"') {
      if (in_quotes && i + 1 < row.size() && row[i + 1] == '"') {
        field.push_back('"');
        ++i;
      } else {
        in_quotes = !in_quotes;
      }
    } else if (ch == delimiter && !in_quotes) {
      fields.push_back(trim_fields ? trim(std::move(field)) : std::move(field));
      field.clear();
    } else {
      field.push_back(ch);
    }
  }

  if (in_quotes) {
    throw std::runtime_error("CSV row contains an unterminated quoted field");
  }

  fields.push_back(trim_fields ? trim(std::move(field)) : std::move(field));
  return fields;
}

std::unordered_map<std::string, std::size_t> header_index(
    const std::vector<std::string>& header) {
  std::unordered_map<std::string, std::size_t> index;
  for (std::size_t i = 0; i < header.size(); ++i) {
    index.emplace(header[i], i);
  }
  return index;
}

std::size_t required_column(
    const std::unordered_map<std::string, std::size_t>& columns,
    const std::string& name) {
  const auto it = columns.find(name);
  if (it == columns.end()) {
    throw std::invalid_argument("CSV column not found: " + name);
  }
  return it->second;
}

std::optional<std::size_t> optional_column(
    const std::unordered_map<std::string, std::size_t>& columns,
    const std::string& name) {
  const auto it = columns.find(name);
  if (it == columns.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::string field_at(
    const std::vector<std::string>& fields,
    std::size_t index,
    std::size_t line_number) {
  if (index >= fields.size()) {
    throw std::runtime_error(
        "CSV row " + std::to_string(line_number) + " has too few fields");
  }
  return fields[index];
}

}  // namespace

EventLog import_csv_event_log(
    std::istream& input,
    const CsvImportOptions& options) {
  if (!options.has_header) {
    throw std::invalid_argument("CSV import currently requires a header row");
  }
  if (options.delimiter == '"') {
    throw std::invalid_argument("CSV delimiter cannot be a quote character");
  }

  std::string line;
  if (!std::getline(input, line)) {
    return {};
  }

  // Strip a leading UTF-8 byte-order mark so the first column name still
  // matches; spreadsheet exports routinely prepend one.
  if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
      static_cast<unsigned char>(line[1]) == 0xBB &&
      static_cast<unsigned char>(line[2]) == 0xBF) {
    line.erase(0, 3);
  }

  auto columns = header_index(
      parse_csv_row(line, options.delimiter, options.trim_fields));
  const auto case_index = required_column(columns, options.case_id_column);
  const auto activity_index = required_column(columns, options.activity_column);
  auto timestamp_index = optional_column(columns, options.timestamp_column);
  if (!timestamp_index && options.require_timestamp_column) {
    throw std::invalid_argument(
        "CSV column not found: " + options.timestamp_column);
  }

  std::unordered_map<std::string, std::size_t> trace_ids;
  EventLog log;

  std::size_t line_number = 1;
  std::size_t input_order = 0;
  while (std::getline(input, line)) {
    ++line_number;
    if (line.empty()) {
      continue;
    }

    auto fields = parse_csv_row(line, options.delimiter, options.trim_fields);
    auto case_id = field_at(fields, case_index, line_number);
    auto activity = field_at(fields, activity_index, line_number);
    auto timestamp = timestamp_index
                         ? field_at(fields, *timestamp_index, line_number)
                         : std::string{};

    if (case_id.empty()) {
      throw std::runtime_error(
          "CSV row " + std::to_string(line_number) + " has an empty case id");
    }
    if (activity.empty()) {
      throw std::runtime_error(
          "CSV row " + std::to_string(line_number) + " has an empty activity");
    }

    auto [it, inserted] = trace_ids.emplace(case_id, log.traces.size());
    if (inserted) {
      log.traces.push_back(Trace{case_id, {}});
    }

    log.traces[it->second].events.push_back(
        Event{case_id, activity, timestamp, input_order++});
  }

  if (timestamp_index) {
    for (auto& trace : log.traces) {
      std::stable_sort(
          trace.events.begin(),
          trace.events.end(),
          [](const Event& lhs, const Event& rhs) {
            if (lhs.timestamp == rhs.timestamp) {
              return lhs.input_order < rhs.input_order;
            }
            return lhs.timestamp < rhs.timestamp;
          });
    }
  }

  return log;
}

EventLog import_csv_event_log(
    const std::filesystem::path& path,
    const CsvImportOptions& options) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("Unable to open CSV event log: " + path.string());
  }
  return import_csv_event_log(input, options);
}

}  // namespace minera
