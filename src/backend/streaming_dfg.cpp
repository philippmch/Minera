#include "minera/streaming_dfg.hpp"

#include "reference_activity_selector.hpp"

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace minera {
namespace {

void write_json_string(std::ostream& out, std::string_view value) {
  out << '"';
  for (const unsigned char character : value) {
    switch (character) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (character < 0x20U) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
              << static_cast<unsigned int>(character) << std::dec
              << std::setfill(' ');
        } else {
          out << static_cast<char>(character);
        }
    }
  }
  out << '"';
}

void write_counts(std::ostream& out,
                  const std::map<std::string, std::uint64_t>& values) {
  out << '{';
  bool first = true;
  for (const auto& [key, value] : values) {
    if (!first) out << ',';
    first = false;
    write_json_string(out, key);
    out << ':' << value;
  }
  out << '}';
}

void write_open_cases(std::ostream& out,
                      const std::map<std::string, std::string>& values) {
  out << '{';
  bool first = true;
  for (const auto& [key, value] : values) {
    if (!first) out << ',';
    first = false;
    write_json_string(out, key);
    out << ':';
    write_json_string(out, value);
  }
  out << '}';
}

}  // namespace

StreamingDfgDiscovery::StreamingDfgDiscovery(StreamingDfgOptions options)
    : options_(std::move(options)) {
  if (options_.activity_attribute.empty() || options_.case_id_attribute.empty())
    throw std::invalid_argument(
        "streaming DFG attribute names must not be empty");
}

bool StreamingDfgDiscovery::receive(std::string_view case_id,
                                    std::string_view activity) {
  std::lock_guard<std::mutex> lock(mutex_);
  const std::string case_key(case_id);
  const std::string activity_key(activity);
  auto previous = last_by_case_.find(case_key);
  if (previous == last_by_case_.end()) {
    ++start_activities_[activity_key];
  } else {
    ++graph_[{previous->second, activity_key}];
  }
  ++activities_[activity_key];
  previous = last_by_case_.insert_or_assign(case_key, activity_key).first;
  (void)previous;
  ++events_;
  return true;
}

bool StreamingDfgDiscovery::receive(const Event& event,
                                    std::string_view case_id) {
  const auto activity = streaming_event_activity(event, options_.activity_attribute);
  return activity && receive(case_id, *activity);
}

std::optional<std::string> streaming_event_activity(const Event& event,
                                                    std::string_view selector) {
  // The reference stream skips an event without the selected key and
  // stringifies a present null as "None".
  for (const auto& attribute : event.attributes)
    if (attribute.key == selector) return attribute.is_null ? std::string("None") : attribute.value;
  try {
    return reference_selected_activity(event, selector, "streaming DFG");
  } catch (const std::invalid_argument&) {
    return std::nullopt;
  }
}

StreamingDfgSnapshot StreamingDfgDiscovery::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  StreamingDfgSnapshot result;
  result.graph = graph_;
  result.activities = activities_;
  result.start_activities = start_activities_;
  result.events = events_;
  for (const auto& [case_id, activity] : last_by_case_) {
    result.open_cases[case_id] = activity;
    ++result.end_activities[activity];
  }
  return result;
}

void StreamingDfgDiscovery::reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  graph_.clear();
  activities_.clear();
  start_activities_.clear();
  last_by_case_.clear();
  events_ = 0;
}

std::string serialize_streaming_dfg_json(
    const StreamingDfgSnapshot& snapshot) {
  std::ostringstream out;
  out << "{\"family\":\"STREAMING_DFG\",\"events\":"
      << snapshot.events << ",\"graph\":[";
  bool first = true;
  for (const auto& [edge, count] : snapshot.graph) {
    if (!first) out << ',';
    first = false;
    out << "{\"source\":";
    write_json_string(out, edge.first);
    out << ",\"target\":";
    write_json_string(out, edge.second);
    out << ",\"count\":" << count << '}';
  }
  out << "],\"activities\":";
  write_counts(out, snapshot.activities);
  out << ",\"start_activities\":";
  write_counts(out, snapshot.start_activities);
  out << ",\"end_activities\":";
  write_counts(out, snapshot.end_activities);
  out << ",\"open_cases\":";
  write_open_cases(out, snapshot.open_cases);
  out << '}';
  return out.str();
}

}  // namespace minera
