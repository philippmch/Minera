#include "minera/windows_event_log.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "xml_pull_parser.hpp"

namespace minera {
namespace {

constexpr std::size_t kMaxEvents = 10'000'000;

std::string local_name(std::string_view name) {
  const auto separator = name.find(':');
  return std::string(separator == std::string_view::npos
                         ? name
                         : name.substr(separator + 1));
}

const std::string* attribute(const detail::XmlPullParser::Token& token,
                             std::string_view name) {
  for (const auto& [key, value] : token.attributes)
    if (local_name(key) == name) return &value;
  return nullptr;
}

std::string trim(std::string value) {
  const auto begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) return {};
  const auto end = value.find_last_not_of(" \t\r\n");
  return value.substr(begin, end - begin + 1);
}

std::string escape_json(std::string_view value) {
  std::string out;
  for (const char c : value) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c; break;
    }
  }
  return out;
}

EventAttribute attr(std::string key, std::string value,
                    AttributeKind kind = AttributeKind::kString) {
  return {std::move(key), std::move(value), kind, false};
}

struct Record {
  std::string provider;
  std::string event_id;
  std::string computer;
  std::string timestamp;
  std::string user;
  std::string channel;
  std::string level;
  std::string task;
  std::string opcode;
  std::string keywords;
  std::string record_id;
  std::string message;
  std::vector<EventAttribute> data;
};

Event event_from_record(Record record, std::size_t input_order) {
  if (record.computer.empty()) record.computer = "UNKNOWN";
  if (record.provider.empty()) record.provider = "UNKNOWN";
  if (record.event_id.empty()) record.event_id = "UNKNOWN";
  if (record.timestamp.empty())
    throw std::invalid_argument("Windows event log: event timestamp is missing");
  Event event;
  event.case_id = record.computer;
  event.activity = record.provider + " " + record.event_id;
  event.timestamp = record.timestamp;
  event.resource = record.user;
  event.input_order = input_order;
  event.attributes = {
      attr("provider", std::move(record.provider)),
      attr("event_id", std::move(record.event_id)),
      attr("computer", record.computer),
      attr("channel", std::move(record.channel)),
      attr("level", std::move(record.level)),
      attr("task", std::move(record.task)),
      attr("opcode", std::move(record.opcode)),
      attr("keywords", std::move(record.keywords)),
      attr("record_id", std::move(record.record_id)),
      attr("message", std::move(record.message))};
  event.attributes.insert(event.attributes.end(),
                          std::make_move_iterator(record.data.begin()),
                          std::make_move_iterator(record.data.end()));
  return event;
}

}  // namespace

EventLog import_windows_event_log(const WindowsEventLogOptions& options) {
  if (options.path.empty())
    throw std::invalid_argument("Windows event log: path is required");
  if (options.max_events == 0 || options.max_events > kMaxEvents)
    throw std::invalid_argument(
        "Windows event log: max_events must be in [1,10000000]");
  std::ifstream input(options.path, std::ios::binary);
  if (!input)
    throw std::invalid_argument("Windows event log: input file does not exist");
  const std::string document((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>());
  detail::XmlPullParser parser(document, true);
  EventLog log;
  log.attributes.push_back(attr("source", "windows_events"));
  std::map<std::string, std::size_t> trace_indexes;
  std::size_t event_count = 0;
  bool in_event = false;
  bool in_event_data = false;
  bool in_rendering = false;
  std::string active_text_element;
  std::string text_value;
  std::string data_name;
  Record record;

  for (;;) {
    const auto token = parser.next();
    if (token.kind == detail::XmlPullParser::Kind::EndOfInput) break;
    const auto name = local_name(token.name);
    if (token.kind == detail::XmlPullParser::Kind::StartElement) {
      if (name == "Event") {
        if (in_event)
          throw std::invalid_argument("Windows event log: nested Event element");
        in_event = true;
        record = {};
      } else if (!in_event) {
        continue;
      } else if (name == "Provider") {
        if (const auto* value = attribute(token, "Name")) record.provider = *value;
      } else if (name == "TimeCreated") {
        if (const auto* value = attribute(token, "SystemTime")) record.timestamp = *value;
      } else if (name == "Security") {
        if (const auto* value = attribute(token, "UserID")) record.user = *value;
      } else if (name == "EventData") {
        in_event_data = true;
      } else if (name == "RenderingInfo") {
        in_rendering = true;
      } else if (name == "Data" && in_event_data) {
        data_name.clear();
        if (const auto* value = attribute(token, "Name")) data_name = *value;
        active_text_element = "Data";
        text_value.clear();
      } else if (name == "EventID" || name == "Computer" ||
                 name == "Channel" || name == "Level" || name == "Task" ||
                 name == "Opcode" || name == "Keywords" ||
                 name == "EventRecordID" || (name == "Message" && in_rendering)) {
        active_text_element = name;
        text_value.clear();
      }
      if (token.self_closing && name == "Event")
        throw std::invalid_argument("Windows event log: empty Event element");
    } else if (token.kind == detail::XmlPullParser::Kind::CharacterData &&
               !active_text_element.empty()) {
      text_value += token.text;
    } else if (token.kind == detail::XmlPullParser::Kind::EndElement) {
      if (name == active_text_element) {
        auto value = trim(std::move(text_value));
        if (name == "EventID") record.event_id = std::move(value);
        else if (name == "Computer") record.computer = std::move(value);
        else if (name == "Channel") record.channel = std::move(value);
        else if (name == "Level") record.level = std::move(value);
        else if (name == "Task") record.task = std::move(value);
        else if (name == "Opcode") record.opcode = std::move(value);
        else if (name == "Keywords") record.keywords = std::move(value);
        else if (name == "EventRecordID") record.record_id = std::move(value);
        else if (name == "Message") record.message = std::move(value);
        else if (name == "Data") {
          const auto key = data_name.empty()
                               ? "data_" + std::to_string(record.data.size())
                               : "data:" + data_name;
          record.data.push_back(attr(key, std::move(value)));
        }
        active_text_element.clear();
        text_value.clear();
      }
      if (name == "EventData") in_event_data = false;
      else if (name == "RenderingInfo") in_rendering = false;
      else if (name == "Event") {
        if (!in_event)
          throw std::invalid_argument("Windows event log: unmatched Event element");
        if (event_count >= options.max_events)
          throw std::invalid_argument("Windows event log: max_events exceeded");
        auto event = event_from_record(std::move(record), event_count++);
        const auto case_id = event.case_id;
        const auto [it, inserted] =
            trace_indexes.emplace(case_id, log.traces.size());
        if (inserted) log.traces.push_back({case_id, {}});
        log.traces[it->second].events.push_back(std::move(event));
        in_event = false;
      }
    }
  }
  if (in_event)
    throw std::invalid_argument("Windows event log: unclosed Event element");
  for (auto& trace : log.traces) order_events_by_time(trace.events);
  return log;
}

std::string windows_event_log_json(const EventLog& log) {
  std::ostringstream out;
  out << "{\"type\":\"WINDOWS_EVENTS\",\"variant\":\"windows_events\","
      << "\"trace_count\":" << log.trace_count()
      << ",\"event_count\":" << log.event_count() << ",\"traces\":[";
  for (std::size_t i = 0; i < log.traces.size(); ++i) {
    if (i) out << ',';
    const auto& trace = log.traces[i];
    out << "{\"case_id\":\"" << escape_json(trace.case_id)
        << "\",\"events\":[";
    for (std::size_t j = 0; j < trace.events.size(); ++j) {
      if (j) out << ',';
      const auto& event = trace.events[j];
      out << "{\"activity\":\"" << escape_json(event.activity)
          << "\",\"timestamp\":\"" << escape_json(event.timestamp)
          << "\",\"resource\":\"" << escape_json(event.resource)
          << "\",\"attributes\":{";
      for (std::size_t k = 0; k < event.attributes.size(); ++k) {
        if (k) out << ',';
        out << '"' << escape_json(event.attributes[k].key) << "\":\""
            << escape_json(event.attributes[k].value) << '"';
      }
      out << "}}";
    }
    out << "]}";
  }
  out << "]}";
  return out.str();
}

}  // namespace minera
