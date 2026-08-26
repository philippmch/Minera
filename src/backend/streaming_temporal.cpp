#include "minera/streaming_temporal.hpp"

#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "minera/timestamp.hpp"

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

const std::string* event_value(const Event& event, std::string_view key,
                               bool activity) {
  if (key.empty() || key == (activity ? "activity" : "timestamp")) {
    if (activity) return &event.activity;
    return &event.timestamp;
  }
  if (const auto* value = event.attribute(key)) return value;
  // CSV import maps the configured activity/timestamp columns onto the
  // canonical Event fields, so a custom column name may not remain in the
  // generic attribute list.
  if (activity) return &event.activity;
  return &event.timestamp;
}

}  // namespace

StreamingTemporalConformance::StreamingTemporalConformance(
    TemporalProfile profile, StreamingTemporalOptions options)
    : profile_(std::move(profile)), options_(std::move(options)) {
  if (options_.case_id_attribute.empty())
    throw std::invalid_argument(
        "streaming temporal case-id attribute must not be empty");
  if (!std::isfinite(options_.zeta) || options_.zeta <= 0.0)
    throw std::invalid_argument(
        "streaming temporal zeta must be finite and greater than zero");
  for (const auto& entry : profile_.entries) {
    if (entry.count == 0 || !std::isfinite(entry.mean_seconds) ||
        entry.mean_seconds < 0.0 ||
        !std::isfinite(entry.standard_deviation_seconds) ||
        entry.standard_deviation_seconds < 0.0) {
      throw std::invalid_argument(
          "streaming temporal profile entries must be finite and non-negative");
    }
    if (!profile_lookup_
             .emplace(std::make_pair(entry.source, entry.target), &entry)
             .second) {
      throw std::invalid_argument(
          "streaming temporal profile contains duplicate activity pairs");
    }
  }
}

StreamingTemporalEventResult StreamingTemporalConformance::receive(
    std::string_view case_id, std::string_view activity,
    std::string_view start_timestamp, std::string_view timestamp) {
  if (case_id.empty() || activity.empty() || start_timestamp.empty() ||
      timestamp.empty())
    return {};
  const auto start = parse_timestamp(start_timestamp);
  const auto end = parse_timestamp(timestamp);
  if (!start || !end) return {};

  std::lock_guard<std::mutex> lock(mutex_);
  const std::string case_key(case_id);
  const std::string activity_key(activity);
  auto& state = cases_[case_key];
  std::vector<TemporalDeviation> new_deviations;
  for (const auto& previous : state.events_in_order) {
    if (*start < previous.end) continue;
    const auto profile_it = profile_lookup_.find(
        {previous.activity, activity_key});
    if (profile_it == profile_lookup_.end()) continue;
    const auto& entry = *profile_it->second;
    const double elapsed = *start - previous.end;
    const double difference = std::abs(elapsed - entry.mean_seconds);
    double distance = 0.0;
    if (entry.standard_deviation_seconds == 0.0) {
      if (difference == 0.0) continue;
      distance = std::numeric_limits<double>::max();
    } else {
      distance = difference / entry.standard_deviation_seconds;
      if (distance <= options_.zeta) continue;
    }
    new_deviations.push_back(
        {case_key, previous.activity, activity_key, elapsed, distance});
  }
  ++state.events;
  state.events_in_order.push_back({*start, *end, activity_key});
  state.deviations += new_deviations.size();
  total_deviations_ += new_deviations.size();
  ++total_events_processed_;
  if (!new_deviations.empty()) {
    auto& stored = deviations_[case_key];
    stored.insert(stored.end(), new_deviations.begin(), new_deviations.end());
  }
  return {case_key, activity_key, total_events_processed_,
          std::move(new_deviations)};
}

StreamingTemporalEventResult StreamingTemporalConformance::receive(
    const Event& event, std::string_view case_id) {
  const std::string resolved_case =
      case_id.empty() ? event.case_id : std::string(case_id);
  const auto* activity = event_value(event, options_.activity_attribute, true);
  const auto* start = event_value(event, options_.start_timestamp_attribute,
                                  false);
  const auto* timestamp = event_value(event, options_.timestamp_attribute,
                                      false);
  if (activity == nullptr || start == nullptr || timestamp == nullptr)
    return {};
  return receive(resolved_case, *activity, *start, *timestamp);
}

void StreamingTemporalConformance::reset_case(const std::string& case_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  cases_.erase(case_id);
  deviations_.erase(case_id);
}

void StreamingTemporalConformance::reset_all() {
  std::lock_guard<std::mutex> lock(mutex_);
  cases_.clear();
  deviations_.clear();
  total_events_processed_ = 0;
  total_deviations_ = 0;
}

StreamingTemporalSnapshot StreamingTemporalConformance::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  StreamingTemporalSnapshot result;
  result.total_events_processed = total_events_processed_;
  result.total_deviations = total_deviations_;
  result.deviations = deviations_;
  for (const auto& [case_id, state] : cases_) {
    result.cases.emplace(
        case_id, StreamingTemporalCaseSnapshot{state.events, state.deviations});
  }
  return result;
}

std::string serialize_streaming_temporal_json(
    const StreamingTemporalSnapshot& snapshot) {
  std::ostringstream out;
  out << "{\"family\":\"STREAMING_TEMPORAL\",\"total_events_processed\":"
      << snapshot.total_events_processed
      << ",\"total_deviations\":" << snapshot.total_deviations
      << ",\"cases\":{";
  bool first_case = true;
  for (const auto& [case_id, state] : snapshot.cases) {
    if (!first_case) out << ',';
    first_case = false;
    write_json_string(out, case_id);
    out << ":{\"events\":" << state.events
        << ",\"deviations\":" << state.deviations << '}';
  }
  out << "},\"deviations\":{";
  bool first_deviation_case = true;
  for (const auto& [case_id, deviations] : snapshot.deviations) {
    if (!first_deviation_case) out << ',';
    first_deviation_case = false;
    write_json_string(out, case_id);
    out << ":[";
    for (std::size_t i = 0; i < deviations.size(); ++i) {
      if (i != 0) out << ',';
      const auto& deviation = deviations[i];
      out << "{\"source\":";
      write_json_string(out, deviation.source);
      out << ",\"target\":";
      write_json_string(out, deviation.target);
      out << ",\"elapsed_seconds\":" << deviation.elapsed_seconds
          << ",\"standard_deviation_distance\":"
          << deviation.standard_deviation_distance << '}';
    }
    out << ']';
  }
  out << "}}";
  return out.str();
}

}  // namespace minera
