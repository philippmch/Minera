#include "minera/streaming_footprints.hpp"

#include "minera/streaming_dfg.hpp"

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

}  // namespace

StreamingFootprintsConformance::StreamingFootprintsConformance(
    Footprint model, StreamingFootprintsOptions options)
    : model_(std::move(model)), options_(std::move(options)) {
  if (options_.activity_attribute.empty() || options_.case_id_attribute.empty())
    throw std::invalid_argument(
        "streaming footprints attribute names must not be empty");
  allowed_pairs_ = model_.sequence;
  allowed_pairs_.insert(model_.parallel.begin(), model_.parallel.end());
}

StreamingFootprintsEventResult StreamingFootprintsConformance::receive(
    std::string_view case_id, std::string_view activity) {
  // As the reference monitor: empty identifiers are ordinary values. An
  // activity outside the model is a deviation of its case when the case is
  // open (its last activity stays), and opens no case otherwise.
  std::lock_guard<std::mutex> lock(mutex_);
  const std::string case_key(case_id);
  const std::string activity_key(activity);
  ++total_events_processed_;
  auto it = cases_.find(case_key);
  if (!model_.activities.contains(activity_key)) {
    ++total_deviations_;
    if (it == cases_.end()) return {case_key, activity_key, total_events_processed_, 1, false};
    ++it->second.events;
    ++it->second.deviations;
    return {case_key, activity_key, total_events_processed_, 1, false};
  }
  std::size_t deviations = 0;
  if (it == cases_.end()) {
    it = cases_.try_emplace(case_key).first;
    it->second.first_activity = activity_key;
    if (model_.start_activities_present &&
        !model_.start_activities.contains(activity_key)) {
      ++deviations;
    }
  } else if (!allowed_pairs_.contains({it->second.last_activity, activity_key})) {
    ++deviations;
  }
  CaseState& state = it->second;
  ++state.events;
  state.last_activity = activity_key;
  state.deviations += deviations;
  total_deviations_ += deviations;
  return {case_key, activity_key, total_events_processed_, deviations,
          state.deviations == 0};
}
StreamingFootprintsEventResult StreamingFootprintsConformance::receive(
    const Event& event, std::string_view case_id) {
  // The reference skips an event whose activity is missing or None.
  const auto activity = streaming_event_present_value(event, options_.activity_attribute);
  if (!activity) return {};
  return receive(case_id, *activity);
}
bool StreamingFootprintsConformance::terminate_case(
    const std::string& case_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = cases_.find(case_id);
  if (it == cases_.end()) return false;
  if (model_.end_activities_present &&
      !model_.end_activities.contains(it->second.last_activity)) {
    ++it->second.deviations;
    ++total_deviations_;
  }
  const bool fit = it->second.deviations == 0;
  cases_.erase(it);
  return fit;
}

void StreamingFootprintsConformance::terminate_all() {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = cases_.begin(); it != cases_.end();) {
    if (model_.end_activities_present &&
        !model_.end_activities.contains(it->second.last_activity)) {
      ++it->second.deviations;
      ++total_deviations_;
    }
    it = cases_.erase(it);
  }
}

void StreamingFootprintsConformance::reset_case(const std::string& case_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  cases_.erase(case_id);
}

void StreamingFootprintsConformance::reset_all() {
  std::lock_guard<std::mutex> lock(mutex_);
  cases_.clear();
  total_events_processed_ = 0;
  total_deviations_ = 0;
}

StreamingFootprintsSnapshot StreamingFootprintsConformance::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  StreamingFootprintsSnapshot result;
  result.total_events_processed = total_events_processed_;
  result.total_deviations = total_deviations_;
  for (const auto& [case_id, state] : cases_) {
    result.cases.emplace(
        case_id,
        StreamingFootprintsCaseSnapshot{state.events, state.deviations,
                                        state.deviations == 0,
                                        state.first_activity,
                                        state.last_activity});
  }
  return result;
}

std::string serialize_streaming_footprints_json(
    const StreamingFootprintsSnapshot& snapshot) {
  std::ostringstream out;
  out << "{\"family\":\"STREAMING_FOOTPRINTS\",\"total_events_processed\":"
      << snapshot.total_events_processed
      << ",\"total_deviations\":" << snapshot.total_deviations
      << ",\"cases\":[";
  bool first = true;
  for (const auto& [case_id, state] : snapshot.cases) {
    if (!first) out << ',';
    first = false;
    out << "{\"case_id\":";
    write_json_string(out, case_id);
    out << ",\"events\":" << state.events
        << ",\"deviations\":" << state.deviations
        << ",\"is_fit\":" << (state.is_fit ? "true" : "false")
        << ",\"first_activity\":";
    write_json_string(out, state.first_activity);
    out << ",\"last_activity\":";
    write_json_string(out, state.last_activity);
    out << '}';
  }
  out << "]}";
  return out.str();
}

}  // namespace minera
