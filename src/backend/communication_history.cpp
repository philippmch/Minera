#include "minera/communication_history.hpp"

#include <chrono>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "minera/timestamp.hpp"

namespace minera {
namespace {
EventAttribute attr(std::string key, std::string value) { return {std::move(key), std::move(value), AttributeKind::kString, false}; }
std::vector<std::vector<std::string>> table(std::string_view input) {
  if (input.size() > 256 * 1024 * 1024) throw std::invalid_argument("Communication history: CSV exceeds 256 MiB");
  std::vector<std::vector<std::string>> rows; std::vector<std::string> row; std::string field; bool quoted = false;
  for (std::size_t i = 0; i <= input.size(); ++i) {
    const char c = i == input.size() ? '\n' : input[i];
    if (quoted) {
      if (c == '"' && i + 1 < input.size() && input[i + 1] == '"') { field.push_back('"'); ++i; }
      else if (c == '"') quoted = false; else field.push_back(c);
    } else if (c == '"' && field.empty()) quoted = true;
    else if (c == ',') { row.push_back(std::move(field)); field.clear(); }
    else if (c == '\n') { if (!field.empty() || !row.empty()) { if (!field.empty() && field.back() == '\r') field.pop_back(); row.push_back(std::move(field)); field.clear(); rows.push_back(std::move(row)); row.clear(); } }
    else field.push_back(c);
  }
  if (quoted) throw std::invalid_argument("Communication history: unterminated CSV quote");
  return rows;
}
std::string escape_json(std::string_view value) { constexpr char hex[]="0123456789abcdef"; std::string out; for (const unsigned char c : value) { if (c == '"') out += "\\\""; else if (c == '\\') out += "\\\\"; else if (c == '\n') out += "\\n"; else if (c == '\r') out += "\\r"; else if (c == '\t') out += "\\t"; else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];} else out += static_cast<char>(c); } return out; }
std::string_view variant_name(CommunicationHistoryVariant variant) { return variant == CommunicationHistoryVariant::kMailbox ? "mailbox" : "calendar"; }
const std::map<std::string, std::string, std::less<>> kClasses = {
    {"26","Appointment"},{"40","Contact"},{"41","Document"},{"42","Journal"},{"43","Mail"},{"44","Note"},{"45","Post"},{"46","Delivery Report"},{"47","Remote"},{"48","Task"},{"49","Task Request"},{"50","Task Request Update"},{"51","Task Request Acceptance"},{"52","Task Request Declination"},{"53","Meeting Request"},{"54","Meeting Cancellation"},{"55","Meeting Declination"},{"56","Meeting Acceptance"},{"57","Meeting Tentatively Accepted"},{"69","Distribution List"},{"104","Sharing"},{"113","Storage"},{"181","Meeting Forward Notification"}};
}  // namespace

EventLog import_communication_history_csv(std::string_view csv,
                                          const CommunicationHistoryOptions& options) {
  if (options.max_items == 0 || options.max_items > 1'000'000 || options.max_events == 0 || options.max_events > 10'000'000)
    throw std::invalid_argument("Communication history: invalid safety limit");
  const auto rows = table(csv);
  if (rows.empty()) throw std::invalid_argument("Communication history: CSV header is required");
  std::map<std::string, std::size_t, std::less<>> columns;
  for (std::size_t i = 0; i < rows[0].size(); ++i) if (!columns.emplace(rows[0][i], i).second) throw std::invalid_argument("Communication history: duplicate CSV column");
  const auto required = [&](std::string_view key) { const auto it = columns.find(key); if (it == columns.end()) throw std::invalid_argument("Communication history: missing CSV column " + std::string(key)); return it->second; };
  const auto conversation = required("conversation_id");
  EventLog log; log.attributes = {attr("source", "communication_history"), attr("variant", std::string(variant_name(options.variant)))};
  std::map<std::string, std::size_t> traces; std::size_t event_count = 0;
  auto add = [&](const std::vector<std::string>& row, std::string activity, std::string timestamp, std::string resource, std::vector<EventAttribute> attributes) {
    if (row[conversation].empty() || activity.empty() || timestamp.empty()) throw std::invalid_argument("Communication history: case, activity, and timestamp are required");
    if (++event_count > options.max_events) throw std::invalid_argument("Communication history: max_events exceeded");
    const auto [it, inserted] = traces.emplace(row[conversation], log.traces.size()); if (inserted) log.traces.push_back({row[conversation], {}});
    Event event; event.case_id = row[conversation]; event.activity = std::move(activity); event.timestamp = std::move(timestamp); event.resource = std::move(resource); event.input_order = event_count - 1; event.attributes = std::move(attributes); log.traces[it->second].events.push_back(std::move(event));
  };
  if (rows.size() - 1 > options.max_items) throw std::invalid_argument("Communication history: max_items exceeded");
  if (options.variant == CommunicationHistoryVariant::kMailbox) {
    const auto direction = required("direction"), item_class = required("item_class"), created = required("created_at"), sender = required("sender"), recipients = required("recipients"), topic = required("topic"), subject = required("subject");
    for (std::size_t i = 1; i < rows.size(); ++i) {
      if (rows[i].size() != rows[0].size()) throw std::invalid_argument("Communication history: inconsistent CSV row width");
      const auto class_it = kClasses.find(rows[i][item_class]); if (class_it == kClasses.end()) continue;
      if (rows[i][direction] != "Sent" && rows[i][direction] != "Received") throw std::invalid_argument("Communication history: direction must be Sent or Received");
      add(rows[i], rows[i][direction] + " " + class_it->second, rows[i][created], rows[i][sender], {attr("recipients", rows[i][recipients]), attr("topic", rows[i][topic]), attr("subject", rows[i][subject])});
    }
  } else {
    const auto subject = required("subject"), created = required("created_at"), changed = required("last_changed_at"), start = required("start_at"), end = required("end_at"), cancelled = required("cancelled"), reminder = required("reminder_at");
    std::optional<double> now;
    if (!options.current_time.empty()) now = parse_timestamp(options.current_time);
    for (std::size_t i = 1; i < rows.size(); ++i) {
      if (rows[i].size() != rows[0].size()) throw std::invalid_argument("Communication history: inconsistent CSV row width");
      const auto metadata = [&] { return std::vector<EventAttribute>{attr("subject", rows[i][subject])}; };
      add(rows[i], "Meeting Created", rows[i][created], "", metadata());
      if (rows[i][changed] != rows[i][created]) add(rows[i], "Last Change of Meeting", rows[i][changed], "", metadata());
      add(rows[i], "Meeting Started", rows[i][start], "", metadata()); add(rows[i], "Meeting Completed", rows[i][end], "", metadata());
      if (rows[i][cancelled] == "true") add(rows[i], "Meeting Cancelled", rows[i][changed], "", metadata());
      else if (rows[i][cancelled] != "false") throw std::invalid_argument("Communication history: cancelled must be true or false");
      if (options.include_reminders && !rows[i][reminder].empty()) {
        const auto reminder_time = parse_timestamp(rows[i][reminder]);
        if (!reminder_time) throw std::invalid_argument("Communication history: invalid reminder timestamp");
        if (!now || *reminder_time <= *now)
          add(rows[i], "Meeting Reminder Fired", rows[i][reminder], "", metadata());
      }
    }
  }
  for (auto& trace : log.traces) order_events_by_time(trace.events); return log;
}

std::string communication_history_json(const EventLog& log, CommunicationHistoryVariant variant) {
  std::ostringstream out; out << "{\"type\":\"COMMUNICATION_HISTORY\",\"variant\":\"" << variant_name(variant) << "\",\"trace_count\":" << log.trace_count() << ",\"event_count\":" << log.event_count() << ",\"traces\":[";
  for (std::size_t i = 0; i < log.traces.size(); ++i) { if (i) out << ','; const auto& trace = log.traces[i]; out << "{\"case_id\":\"" << escape_json(trace.case_id) << "\",\"events\":["; for (std::size_t j = 0; j < trace.events.size(); ++j) { if (j) out << ','; const auto& event = trace.events[j]; out << "{\"activity\":\"" << escape_json(event.activity) << "\",\"timestamp\":\"" << escape_json(event.timestamp) << "\",\"resource\":\"" << escape_json(event.resource) << "\",\"attributes\":{"; for (std::size_t k = 0; k < event.attributes.size(); ++k) { if (k) out << ','; out << '"' << escape_json(event.attributes[k].key) << "\":\"" << escape_json(event.attributes[k].value) << '"'; } out << "}}"; } out << "]}"; } out << "]}"; return out.str();
}
}  // namespace minera
