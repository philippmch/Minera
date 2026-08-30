#include "minera/workflow_history.hpp"

#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace minera {
namespace {
EventAttribute attr(std::string key, std::string value) {
  return {std::move(key), std::move(value), AttributeKind::kString, false};
}
std::string escape_json(std::string_view value) {
  std::string out;
  for (const unsigned char c : value) {
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else if (c < 0x20) {
      constexpr char hex[] = "0123456789abcdef";
      out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15];
    } else out += static_cast<char>(c);
  }
  return out;
}
std::vector<std::vector<std::string>> parse_csv(std::string_view input) {
  std::vector<std::vector<std::string>> rows;
  std::vector<std::string> row;
  std::string field;
  bool quoted = false;
  for (std::size_t i = 0; i <= input.size(); ++i) {
    const char c = i == input.size() ? '\n' : input[i];
    if (quoted) {
      if (c == '"' && i + 1 < input.size() && input[i + 1] == '"') {
        field.push_back('"'); ++i;
      } else if (c == '"') quoted = false;
      else field.push_back(c);
    } else if (c == '"' && field.empty()) quoted = true;
    else if (c == ',') { row.push_back(std::move(field)); field.clear(); }
    else if (c == '\n') {
      if (!field.empty() || !row.empty()) {
        if (!field.empty() && field.back() == '\r') field.pop_back();
        row.push_back(std::move(field)); field.clear(); rows.push_back(std::move(row)); row.clear();
      }
    } else field.push_back(c);
  }
  if (quoted) throw std::invalid_argument("Workflow history: unterminated CSV quote");
  return rows;
}
}  // namespace

EventLog import_workflow_history_csv(std::string_view csv,
                                     const WorkflowHistoryOptions& options) {
  if (csv.size() > 256 * 1024 * 1024)
    throw std::invalid_argument("Workflow history: CSV exceeds 256 MiB");
  const auto table = parse_csv(csv);
  if (table.empty()) throw std::invalid_argument("Workflow history: CSV header is required");
  std::map<std::string, std::size_t, std::less<>> columns;
  for (std::size_t i = 0; i < table.front().size(); ++i)
    if (!columns.emplace(table.front()[i], i).second)
      throw std::invalid_argument("Workflow history: duplicate CSV column");
  const auto required = [&](std::string_view name) {
    const auto it = columns.find(name);
    if (it == columns.end()) throw std::invalid_argument("Workflow history: missing CSV column " + std::string(name));
    return it->second;
  };
  const auto process = required("process_id");
  const auto case_id = required("case_id");
  const auto activity = required("activity");
  const auto timestamp = required("timestamp");
  const auto resource = required("resource");
  std::vector<WorkflowHistoryRow> rows;
  rows.reserve(table.size() - 1);
  for (std::size_t i = 1; i < table.size(); ++i) {
    if (table[i].size() != table.front().size())
      throw std::invalid_argument("Workflow history: inconsistent CSV row width");
    rows.push_back({table[i][process], table[i][case_id], table[i][activity],
                    table[i][timestamp], table[i][resource]});
  }
  return import_workflow_history(rows, options);
}

EventLog import_workflow_history(const std::vector<WorkflowHistoryRow>& rows,
                                 const WorkflowHistoryOptions& options) {
  if (options.max_events == 0 || options.max_events > 10'000'000)
    throw std::invalid_argument("Workflow history: max_events must be in [1,10000000]");
  if (rows.size() > options.max_events)
    throw std::invalid_argument("Workflow history: max_events exceeded");
  EventLog log;
  log.attributes.push_back(attr("source", "workflow_history"));
  std::map<std::string, std::size_t> trace_indexes;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const auto& row = rows[i];
    if (row.case_id.empty()) throw std::invalid_argument("Workflow history: case id is required");
    if (row.activity.empty()) throw std::invalid_argument("Workflow history: activity is required");
    if (row.timestamp.empty()) throw std::invalid_argument("Workflow history: timestamp is required");
    const auto [it, inserted] = trace_indexes.emplace(row.case_id, log.traces.size());
    if (inserted) {
      Trace trace;
      trace.case_id = row.case_id;
      if (!row.process_id.empty())
        trace.attributes.push_back(attr("case:process_id", row.process_id));
      log.traces.push_back(std::move(trace));
    } else {
      const auto* process = log.traces[it->second].attribute("case:process_id");
      if ((!process && !row.process_id.empty()) ||
          (process && *process != row.process_id))
        throw std::invalid_argument("Workflow history: process id changes within a case");
    }
    Event event;
    event.case_id = row.case_id;
    event.activity = row.activity;
    event.timestamp = row.timestamp;
    event.resource = row.resource;
    event.input_order = i;
    log.traces[it->second].events.push_back(std::move(event));
  }
  for (auto& trace : log.traces) order_events_by_time(trace.events);
  return log;
}

std::string workflow_history_json(const EventLog& log) {
  std::ostringstream out;
  out << "{\"type\":\"WORKFLOW_HISTORY\",\"variant\":\"activity_instances\","
      << "\"trace_count\":" << log.trace_count() << ",\"event_count\":"
      << log.event_count() << ",\"traces\":[";
  for (std::size_t i = 0; i < log.traces.size(); ++i) {
    if (i) out << ',';
    const auto& trace = log.traces[i];
    out << "{\"case_id\":\"" << escape_json(trace.case_id) << "\",\"process_id\":\""
        << escape_json(trace.attribute("case:process_id") ? *trace.attribute("case:process_id") : "")
        << "\",\"events\":[";
    for (std::size_t j = 0; j < trace.events.size(); ++j) {
      if (j) out << ',';
      const auto& event = trace.events[j];
      out << "{\"activity\":\"" << escape_json(event.activity)
          << "\",\"timestamp\":\"" << escape_json(event.timestamp)
          << "\",\"resource\":\"" << escape_json(event.resource) << "\"}";
    }
    out << "]}";
  }
  out << "]}";
  return out.str();
}
}  // namespace minera
