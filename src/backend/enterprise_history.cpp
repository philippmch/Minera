#include "minera/enterprise_history.hpp"

#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace minera {
namespace {
EventAttribute attr(std::string key, std::string value) {
  return {std::move(key), std::move(value), AttributeKind::kString, false};
}
std::vector<std::vector<std::string>> csv_table(std::string_view input) {
  if (input.size() > 256 * 1024 * 1024)
    throw std::invalid_argument("Enterprise history: CSV exceeds 256 MiB");
  std::vector<std::vector<std::string>> rows;
  std::vector<std::string> row; std::string field; bool quoted = false;
  for (std::size_t i = 0; i <= input.size(); ++i) {
    const char c = i == input.size() ? '\n' : input[i];
    if (quoted) {
      if (c == '"' && i + 1 < input.size() && input[i + 1] == '"') { field.push_back('"'); ++i; }
      else if (c == '"') quoted = false;
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
  if (quoted) throw std::invalid_argument("Enterprise history: unterminated CSV quote");
  return rows;
}
std::string escape_json(std::string_view value) {
  std::string out;
  for (const unsigned char c : value) {
    if (c == '"') out += "\\\""; else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n"; else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t"; else out += static_cast<char>(c);
  }
  return out;
}
std::string_view name(EnterpriseHistoryVariant variant) {
  return variant == EnterpriseHistoryVariant::kOrderToCash ? "order_to_cash" : "accounting";
}
}  // namespace

EventLog import_enterprise_history_csv(std::string_view csv,
                                       const EnterpriseHistoryOptions& options) {
  if (options.max_events == 0 || options.max_events > 10'000'000)
    throw std::invalid_argument("Enterprise history: max_events must be in [1,10000000]");
  const auto table = csv_table(csv);
  if (table.empty()) throw std::invalid_argument("Enterprise history: CSV header is required");
  std::map<std::string, std::size_t, std::less<>> columns;
  for (std::size_t i = 0; i < table[0].size(); ++i)
    if (!columns.emplace(table[0][i], i).second)
      throw std::invalid_argument("Enterprise history: duplicate CSV column");
  const auto required = [&](std::string_view column) {
    const auto it = columns.find(column);
    if (it == columns.end()) throw std::invalid_argument("Enterprise history: missing CSV column " + std::string(column));
    return it->second;
  };
  const auto case_column = required("case_id");
  const auto activity_column = required("activity");
  const auto timestamp_column = required("timestamp");
  const auto resource_column = required("resource");
  const auto document_type = columns.find("document_type");
  EventLog log; log.attributes.push_back(attr("source", "enterprise_history"));
  log.attributes.push_back(attr("variant", std::string(name(options.variant))));
  std::map<std::string, std::size_t> traces;
  for (std::size_t row_index = 1; row_index < table.size(); ++row_index) {
    if (row_index > options.max_events)
      throw std::invalid_argument("Enterprise history: max_events exceeded");
    const auto& row = table[row_index];
    if (row.size() != table[0].size()) throw std::invalid_argument("Enterprise history: inconsistent CSV row width");
    if (row[case_column].empty() || row[activity_column].empty() || row[timestamp_column].empty())
      throw std::invalid_argument("Enterprise history: case, activity, and timestamp are required");
    const auto [it, inserted] = traces.emplace(row[case_column], log.traces.size());
    if (inserted) log.traces.push_back({row[case_column], {}});
    Event event; event.case_id = row[case_column]; event.activity = row[activity_column];
    event.timestamp = row[timestamp_column]; event.resource = row[resource_column]; event.input_order = row_index - 1;
    if (options.variant == EnterpriseHistoryVariant::kAccounting && document_type != columns.end() &&
        !row[document_type->second].empty())
      event.attributes.push_back(attr("document_type", row[document_type->second]));
    log.traces[it->second].events.push_back(std::move(event));
  }
  for (auto& trace : log.traces) order_events_by_time(trace.events);
  return log;
}

std::string enterprise_history_json(const EventLog& log,
                                    EnterpriseHistoryVariant variant) {
  std::ostringstream out;
  out << "{\"type\":\"ENTERPRISE_HISTORY\",\"variant\":\"" << name(variant)
      << "\",\"trace_count\":" << log.trace_count() << ",\"event_count\":" << log.event_count() << ",\"traces\":[";
  for (std::size_t i = 0; i < log.traces.size(); ++i) {
    if (i) out << ','; const auto& trace = log.traces[i];
    out << "{\"case_id\":\"" << escape_json(trace.case_id) << "\",\"events\":[";
    for (std::size_t j = 0; j < trace.events.size(); ++j) {
      if (j) out << ','; const auto& event = trace.events[j];
      out << "{\"activity\":\"" << escape_json(event.activity) << "\",\"timestamp\":\""
          << escape_json(event.timestamp) << "\",\"resource\":\"" << escape_json(event.resource) << "\",\"attributes\":{";
      for (std::size_t k = 0; k < event.attributes.size(); ++k) {
        if (k) out << ','; out << '"' << escape_json(event.attributes[k].key) << "\":\"" << escape_json(event.attributes[k].value) << '"';
      }
      out << "}}";
    }
    out << "]}";
  }
  out << "]}"; return out.str();
}
}  // namespace minera
