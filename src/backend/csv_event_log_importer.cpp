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

std::vector<std::string_view> selector_aliases(std::string_view name) {
  if (name == "activity" || name == "concept:name")
    return {"activity", "concept:name"};
  if (name == "resource" || name == "org:resource")
    return {"resource", "org:resource"};
  if (name == "timestamp" || name == "time:timestamp")
    return {"timestamp", "time:timestamp"};
  if (name == "lifecycle" || name == "lifecycle:transition")
    return {"lifecycle", "lifecycle:transition"};
  return {};
}

std::size_t required_selector_column(
    const std::unordered_map<std::string, std::size_t>& columns,
    const std::string& name) {
  if (name.find_first_not_of(" \t\r\n") == std::string::npos) {
    throw std::invalid_argument("CSV activity column must not be blank");
  }
  if (const auto it = columns.find(name); it != columns.end()) return it->second;
  for (const auto alias : selector_aliases(name)) {
    if (const auto it = columns.find(std::string(alias)); it != columns.end())
      return it->second;
  }
  throw std::invalid_argument("CSV column not found: " + name);
}

std::optional<std::size_t> optional_selector_column(
    const std::unordered_map<std::string, std::size_t>& columns,
    const std::string& name) {
  if (const auto it = columns.find(name); it != columns.end()) return it->second;
  for (const auto alias : selector_aliases(name)) {
    if (const auto it = columns.find(std::string(alias)); it != columns.end())
      return it->second;
  }
  return std::nullopt;
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

  const auto header =
      parse_csv_row(line, options.delimiter, options.trim_fields);
  auto columns = header_index(header);
  const auto case_index = required_column(columns, options.case_id_column);
  const auto activity_index =
      required_selector_column(columns, options.activity_column);
  auto timestamp_index =
      optional_selector_column(columns, options.timestamp_column);
  if (!timestamp_index && options.require_timestamp_column) {
    throw std::invalid_argument(
        "CSV column not found: " + options.timestamp_column);
  }
  auto resource_index =
      optional_selector_column(columns, options.resource_column);
  auto lifecycle_index =
      optional_selector_column(columns, options.lifecycle_column);

  // Every remaining named column becomes a generic event attribute, keyed by
  // its header, so domain data (channel, amount, region, …) survives import —
  // except columns prefixed `case:`, which are case attributes: constant per
  // case (a repeated flat-file export of e.g. a requested amount), stored once
  // on the trace under the unprefixed key.
  constexpr std::string_view kCasePrefix = "case:";
  std::vector<std::size_t> attribute_indices;
  std::vector<std::size_t> case_attribute_indices;
  for (std::size_t i = 0; i < header.size(); ++i) {
    if (i == case_index || i == activity_index) continue;
    if (timestamp_index && i == *timestamp_index) continue;
    if (resource_index && i == *resource_index) continue;
    if (lifecycle_index && i == *lifecycle_index) continue;
    if (header[i].empty()) continue;
    if (header[i].size() > kCasePrefix.size() &&
        std::string_view(header[i]).substr(0, kCasePrefix.size()) ==
            kCasePrefix) {
      case_attribute_indices.push_back(i);
    } else {
      attribute_indices.push_back(i);
    }
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
    auto resource = resource_index
                        ? field_at(fields, *resource_index, line_number)
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
      log.traces.push_back(Trace{case_id, {}, {}});
      auto& inserted_trace = log.traces.back();
      inserted_trace.case_id_state = FirstClassAttributeState::kPresent;
      inserted_trace.case_id_source_key = header[case_index];
    }

    auto& trace = log.traces[it->second];
    for (const auto index : case_attribute_indices) {
      // The first row carrying a value sets it; later rows of the same case
      // repeat it in a flat file, so they are ignored (as is a conflict).
      if (index >= fields.size() || fields[index].empty()) continue;
      const std::string key = header[index].substr(kCasePrefix.size());
      if (trace.attribute(key) != nullptr) continue;
      trace.attributes.push_back(
          EventAttribute{key, fields[index], AttributeKind::kString});
    }

    Event event{case_id, activity, timestamp, resource, input_order++};
    event.case_id_state = FirstClassAttributeState::kPresent;
    event.activity_state = FirstClassAttributeState::kPresent;
    event.case_id_source_key = header[case_index];
    event.activity_source_key = header[activity_index];
    if (timestamp_index) {
      event.timestamp_state = event.timestamp.empty()
                                  ? FirstClassAttributeState::kMissing
                                  : FirstClassAttributeState::kPresent;
      event.timestamp_source_key = header[*timestamp_index];
    }
    if (resource_index) {
      event.resource_state = event.resource.empty()
                                 ? FirstClassAttributeState::kMissing
                                 : FirstClassAttributeState::kPresent;
      event.resource_source_key = header[*resource_index];
    }
    if (lifecycle_index) {
      event.lifecycle = field_at(fields, *lifecycle_index, line_number);
      event.lifecycle_state = event.lifecycle.empty()
                                  ? FirstClassAttributeState::kMissing
                                  : FirstClassAttributeState::kPresent;
      event.lifecycle_source_key = header[*lifecycle_index];
    }
    for (const auto index : attribute_indices) {
      // A blank cell means the event simply has no such attribute; a short row
      // reads the same way rather than erroring, since these columns are extra.
      if (index >= fields.size() || fields[index].empty()) continue;
      event.attributes.push_back(
          EventAttribute{header[index], fields[index], AttributeKind::kString});
    }
    log.traces[it->second].events.push_back(std::move(event));
  }

  if (timestamp_index) {
    for (auto& trace : log.traces) {
      order_events_by_time(trace.events);
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
