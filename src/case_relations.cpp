#include "minera/case_relations.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::vector<std::string> parse_csv_row(std::string_view line) {
  std::vector<std::string> fields;
  std::string field;
  bool in_quotes = false;
  bool closed_quote = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (in_quotes) {
      if (c == '"') {
        if (i + 1 < line.size() && line[i + 1] == '"') {
          field.push_back('"');
          ++i;
        } else {
          in_quotes = false;
          closed_quote = true;
        }
      } else {
        field.push_back(c);
      }
    } else if (closed_quote) {
      if (c != ',')
        throw std::invalid_argument(
            "case relations: characters after a quoted CSV field");
      fields.push_back(trim(std::move(field)));
      field.clear();
      closed_quote = false;
    } else if (c == ',') {
      fields.push_back(trim(std::move(field)));
      field.clear();
    } else if (c == '"') {
      if (!field.empty())
        throw std::invalid_argument(
            "case relations: quote inside an unquoted CSV field");
      in_quotes = true;
    } else {
      field.push_back(c);
    }
  }
  if (in_quotes)
    throw std::invalid_argument("case relations: unterminated CSV quote");
  fields.push_back(trim(std::move(field)));
  return fields;
}

const std::string* timestamp_text(const Event& event,
                                   const std::string& key) {
  if (key == "time:timestamp" || key == "timestamp") return &event.timestamp;
  return event.attribute(key);
}

double event_time(const Event& event, const std::string& key) {
  const auto* text = timestamp_text(event, key);
  if (!text) throw std::invalid_argument("case relations: missing timestamp");
  const auto parsed = parse_timestamp(*text);
  if (!parsed) throw std::invalid_argument("case relations: invalid timestamp");
  return *parsed;
}

struct TaggedEvent {
  Event event;
  std::size_t sequence = 0;
};

}  // namespace

std::vector<CaseRelation> parse_case_relations_csv(std::istream& input,
                                                    std::string left_column,
                                                    std::string right_column) {
  if (left_column.empty() || right_column.empty() ||
      left_column == right_column)
    throw std::invalid_argument("case relations: relation columns must differ");

  std::string line;
  std::vector<std::string> header;
  while (std::getline(input, line)) {
    if (!trim(line).empty()) {
      header = parse_csv_row(line);
      break;
    }
  }
  if (header.empty()) throw std::invalid_argument("case relations: missing header");
  std::set<std::string> header_names;
  for (const auto& name : header) {
    if (!header_names.insert(name).second)
      throw std::invalid_argument("case relations: duplicate header " + name);
  }
  const auto find_column = [&](const std::string& name) {
    const auto it = std::find(header.begin(), header.end(), name);
    if (it == header.end())
      throw std::invalid_argument("case relations: missing column " + name);
    return static_cast<std::size_t>(it - header.begin());
  };
  const std::size_t left_index = find_column(left_column);
  const std::size_t right_index = find_column(right_column);

  std::vector<CaseRelation> relations;
  while (std::getline(input, line)) {
    if (trim(line).empty()) continue;
    const auto fields = parse_csv_row(line);
    if (fields.size() != header.size())
      throw std::invalid_argument("case relations: row has wrong column count");
    if (fields[left_index].empty() || fields[right_index].empty())
      throw std::invalid_argument("case relations: case ids must not be empty");
    relations.push_back({fields[left_index], fields[right_index]});
  }
  return relations;
}

EventLog merge_case_relations(const EventLog& base, const EventLog& incoming,
                              const std::vector<CaseRelation>& relations,
                              const CaseRelationsOptions& options) {
  if (options.variant != CaseRelationsVariant::kPandas)
    throw std::invalid_argument("case relations variant must be pandas");
  if (options.timestamp_key.empty())
    throw std::invalid_argument("case relations: timestamp key must not be empty");

  std::unordered_map<std::string, std::vector<std::string>> targets;
  for (const auto& relation : relations) {
    if (relation.left_case_id.empty() || relation.right_case_id.empty())
      throw std::invalid_argument("case relations: case ids must not be empty");
    targets[relation.right_case_id].push_back(relation.left_case_id);
  }

  struct OutputTrace {
    Trace trace;
    std::vector<TaggedEvent> events;
  };
  std::map<std::string, OutputTrace> grouped;
  std::size_t sequence = 0;
  const auto add_base = [&](const Trace& source) {
    auto& out = grouped[source.case_id];
    if (out.trace.case_id.empty()) {
      out.trace.case_id = source.case_id;
      out.trace.attributes = source.attributes;
    }
    for (const auto& source_event : source.events) {
      Event event = source_event;
      event.case_id = source.case_id;
      out.events.push_back({std::move(event), sequence++});
    }
  };
  for (const auto& trace : base.traces) add_base(trace);

  for (const auto& source : incoming.traces) {
    const auto target_it = targets.find(source.case_id);
    if (target_it == targets.end()) continue;
    for (const auto& left_case_id : target_it->second) {
      auto& out = grouped[left_case_id];
      if (out.trace.case_id.empty()) out.trace.case_id = left_case_id;
      for (const auto& source_event : source.events) {
        Event event = source_event;
        event.case_id = left_case_id;
        out.events.push_back({std::move(event), sequence++});
      }
    }
  }

  EventLog result;
  result.attributes = base.attributes;
  result.traces.reserve(grouped.size());
  for (auto& [case_id, output] : grouped) {
    for (const auto& tagged : output.events)
      (void)event_time(tagged.event, options.timestamp_key);
    std::stable_sort(output.events.begin(), output.events.end(),
                     [&](const TaggedEvent& lhs, const TaggedEvent& rhs) {
      const double left_time = event_time(lhs.event, options.timestamp_key);
      const double right_time = event_time(rhs.event, options.timestamp_key);
      if (left_time != right_time) return left_time < right_time;
      return lhs.sequence < rhs.sequence;
    });
    output.trace.events.reserve(output.events.size());
    for (auto& tagged : output.events)
      output.trace.events.push_back(std::move(tagged.event));
    result.traces.push_back(std::move(output.trace));
  }
  return result;
}

}  // namespace minera
