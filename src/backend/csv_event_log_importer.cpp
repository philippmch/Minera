#include "minera/csv_event_log_importer.hpp"

#include <algorithm>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>
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

// A quoted field can span physical lines. Keep its exact CR/LF bytes; only an
// unquoted record terminator is consumed. Escaped quotes retain their spelling
// for the existing field parser to decode.
bool read_csv_record(std::istream& input, std::string& record) {
  record.clear();
  bool in_quotes = false;
  bool read_any = false;
  char ch;
  while (input.get(ch)) {
    read_any = true;
    if (ch == '"') {
      record.push_back(ch);
      if (in_quotes && input.peek() == '"') {
        input.get(ch);
        record.push_back(ch);
      } else in_quotes = !in_quotes;
    } else if (!in_quotes && (ch == '\r' || ch == '\n')) {
      if (ch == '\r' && input.peek() == '\n') input.get(ch);
      return true;
    } else record.push_back(ch);
  }
  return read_any;
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

namespace {

CsvEventData import_csv_event_data_impl(
    std::istream& input,
    const CsvImportOptions& options,
    bool preserve_table) {
  if (!options.has_header) {
    throw std::invalid_argument("CSV import currently requires a header row");
  }
  if (options.delimiter == '"') {
    throw std::invalid_argument("CSV delimiter cannot be a quote character");
  }

  std::string line;
  if (!read_csv_record(input, line)) {
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
  std::optional<std::size_t> case_alias_index;
  if (options.allow_case_id_aliases && !columns.contains(options.case_id_column)) {
    const auto alias = options.case_id_column == "case_id" ? "case:concept:name"
        : options.case_id_column == "case:concept:name" ? "case_id" : "";
    if (*alias != '\0') case_alias_index = optional_selector_column(columns, alias);
  }
  const auto case_index = case_alias_index.has_value()
      ? *case_alias_index : required_column(columns, options.case_id_column);
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
  CsvEventData result;
  EventLog& log = result.log;
  EventTable& table = result.table;
  if (preserve_table) {
    table.columns = header;
    // CSV keeps every source column as a string. Timestamp-role cells use a
    // date kind marker for first-class selection, but no datetime value is
    // inferred from the textual CSV payload.
    table.dtypes.assign(header.size(), "str");
  }

  std::size_t line_number = 1;
  std::size_t input_order = 0;
  while (read_csv_record(input, line)) {
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
    if (preserve_table) {
      EventTable::Row table_row;
      table_row.reserve(header.size());
      for (std::size_t index = 0; index < header.size(); ++index) {
        if (index >= fields.size()) {
          table_row.push_back(std::nullopt);
          continue;
        }
        const bool is_timestamp = timestamp_index && index == *timestamp_index;
        EventAttribute cell{header[index], fields[index],
                            is_timestamp ? AttributeKind::kDate
                                         : AttributeKind::kString};
        const auto set_role = [&](FirstClassAttributeRole role,
                                  FirstClassAttributeState state) {
          cell.first_class_role = role;
          cell.first_class_state = state;
          cell.first_class_source_key = header[index];
        };
        if (index == case_index) {
          set_role(FirstClassAttributeRole::kCaseId,
                   FirstClassAttributeState::kPresent);
        } else if (index == activity_index) {
          set_role(FirstClassAttributeRole::kActivity,
                   FirstClassAttributeState::kPresent);
        } else if (timestamp_index && index == *timestamp_index) {
          set_role(FirstClassAttributeRole::kTimestamp,
                   fields[index].empty() ? FirstClassAttributeState::kMissing
                                         : FirstClassAttributeState::kPresent);
        } else if (resource_index && index == *resource_index) {
          set_role(FirstClassAttributeRole::kResource,
                   fields[index].empty() ? FirstClassAttributeState::kMissing
                                         : FirstClassAttributeState::kPresent);
        } else if (lifecycle_index && index == *lifecycle_index) {
          set_role(FirstClassAttributeRole::kLifecycle,
                   fields[index].empty() ? FirstClassAttributeState::kMissing
                                         : FirstClassAttributeState::kPresent);
        }
        table_row.push_back(std::move(cell));
      }
      table.rows.push_back(std::move(table_row));
    }
    log.traces[it->second].events.push_back(std::move(event));
  }

  if (timestamp_index) {
    for (auto& trace : log.traces) {
      order_events_by_time(trace.events);
    }
  }

  if (preserve_table) table.validate();
  return result;
}

}  // namespace

CsvEventData import_csv_event_data(
    std::istream& input,
    const CsvImportOptions& options) {
  return import_csv_event_data_impl(input, options, true);
}

CsvEventData import_csv_event_data(
    const std::filesystem::path& path,
    const CsvImportOptions& options) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("Unable to open CSV event log: " + path.string());
  }
  return import_csv_event_data(input, options);
}

EventTable project_csv_event_table(const EventTable& source,
                                   const EventLog& filtered) {
  source.validate();

  struct Candidate {
    std::size_t source_row = 0;
    std::size_t encounter = 0;
    const Event* event = nullptr;
    const Trace* trace = nullptr;
  };

  std::vector<Candidate> candidates;
  candidates.reserve(filtered.event_count());
  std::size_t encounter = 0;
  for (const auto& trace : filtered.traces) {
    for (const auto& event : trace.events) {
      if (event.input_order >= source.rows.size()) {
        throw std::invalid_argument(
            "CSV table projection event provenance is outside the source "
            "table");
      }
      candidates.push_back({event.input_order, encounter++, &event, &trace});
    }
  }
  // Importer-created traces are timestamp ordered, while flat correlation
  // semantics use source row order. Stable ordering also gives deterministic
  // placement to the rare transformed event that shares an input_order.
  std::stable_sort(candidates.begin(), candidates.end(),
                   [](const Candidate& left, const Candidate& right) {
                     if (left.source_row != right.source_row)
                       return left.source_row < right.source_row;
                     return left.encounter < right.encounter;
                   });

  EventTable result = source;
  result.rows.clear();
  result.rows.reserve(candidates.size());
  bool identity_rows = candidates.size() == source.rows.size();
  for (std::size_t index = 0; identity_rows && index < candidates.size();
       ++index)
    identity_rows = candidates[index].source_row == index;
  if (!result.index && !identity_rows) {
    // A filtered or reordered projection of an implicit RangeIndex must keep
    // the original source labels. Materialize those labels instead of
    // silently renumbering the result from zero.
    EventTableIndex index;
    index.kind = "Index";
    index.dtype = "int64";
    index.names = {std::nullopt};
    result.index = std::move(index);
  } else if (result.index) {
    // A projection changes row order and cardinality. Keep the explicit
    // index declaration, but select labels by the same source-row mapping as
    // the copied cells so validation and downstream joins see aligned rows.
    if (!identity_rows && result.index->kind == "RangeIndex")
      result.index->kind = "Index";
    result.index->labels.clear();
    result.index->labels.reserve(candidates.size());
  }

  const auto first_class_value = [](const Event& event, const Trace& trace,
                                    FirstClassAttributeRole role)
      -> std::tuple<std::string, FirstClassAttributeState,
                    const std::optional<EventAttribute>*> {
    switch (role) {
      case FirstClassAttributeRole::kCaseId:
        return {event.case_id, event.case_id_state, &trace.case_id_attribute};
      case FirstClassAttributeRole::kActivity:
        return {event.activity, event.activity_state,
                &event.activity_attribute};
      case FirstClassAttributeRole::kTimestamp:
        return {event.timestamp, event.timestamp_state,
                &event.timestamp_attribute};
      case FirstClassAttributeRole::kResource:
        return {event.resource, event.resource_state,
                &event.resource_attribute};
      case FirstClassAttributeRole::kLifecycle:
        return {event.lifecycle, event.lifecycle_state,
                &event.lifecycle_attribute};
      case FirstClassAttributeRole::kNone:
        break;
    }
    return {{}, FirstClassAttributeState::kMissing, nullptr};
  };

  for (const auto& candidate : candidates) {
    EventTable::Row row = source.rows[candidate.source_row];
    const Event& event = *candidate.event;
    for (std::size_t index = 0; index < source.columns.size(); ++index) {
      const auto source_cell = row[index];
      const auto role = source_cell
                            ? source_cell->first_class_role
                            : FirstClassAttributeRole::kNone;
      if (role != FirstClassAttributeRole::kNone) {
        const auto [value, state, metadata] = first_class_value(event, *candidate.trace, role);
        if (!source_cell && state == FirstClassAttributeState::kMissing)
          continue;

        EventAttribute replacement = source_cell.value_or(EventAttribute{
            source.columns[index], value,
            role == FirstClassAttributeRole::kTimestamp
                ? AttributeKind::kDate
                : AttributeKind::kString});
        replacement.key = source.columns[index];
        replacement.value = value;
        replacement.is_null = state == FirstClassAttributeState::kNull;
        replacement.first_class_role = role;
        replacement.first_class_state = state;
        if (replacement.first_class_source_key.empty())
          replacement.first_class_source_key = source.columns[index];
        if (metadata && *metadata &&
            (*metadata)->value == replacement.value &&
            (*metadata)->is_null == replacement.is_null) {
          replacement.kind = (*metadata)->kind;
          replacement.typed_value = (*metadata)->typed_value;
          replacement.container = (*metadata)->container;
          replacement.children = (*metadata)->children;
          replacement.scalar_wrapper = (*metadata)->scalar_wrapper;
          replacement.scalar_children_list = (*metadata)->scalar_children_list;
        } else if (!source_cell || source_cell->value != replacement.value ||
                   source_cell->is_null != replacement.is_null) {
          // A transformed projection without matching typed metadata is a new
          // scalar string; the previous cell's payload no longer describes it.
          replacement.kind = AttributeKind::kString;
          replacement.typed_value = std::monostate{};
          replacement.container = AttributeContainerKind::kScalar;
          replacement.children.clear();
          replacement.scalar_wrapper = false;
          replacement.scalar_children_list = false;
        }
        row[index] = std::move(replacement);
        continue;
      }

      const auto found = std::find_if(
          event.attributes.begin(), event.attributes.end(),
          [&](const auto& attribute) {
            return attribute.key == source.columns[index];
          });
      if (found != event.attributes.end()) {
        auto replacement = *found;
        replacement.key = source.columns[index];
        row[index] = std::move(replacement);
      }
    }
    result.rows.push_back(std::move(row));
    if (result.index && source.index)
      result.index->labels.push_back(source.index->labels[candidate.source_row]);
    else if (result.index) {
      EventAttribute label{"", std::to_string(candidate.source_row),
                           AttributeKind::kInt};
      label.typed_value = static_cast<std::int64_t>(candidate.source_row);
      result.index->labels.push_back({std::move(label)});
    }
  }
  result.validate();
  return result;
}

EventLog import_csv_event_log(
    std::istream& input,
    const CsvImportOptions& options) {
  return import_csv_event_data_impl(input, options, false).log;
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
