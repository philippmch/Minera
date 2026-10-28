#include "minera/streaming_temporal.hpp"

#include "minera/streaming_dfg.hpp"
#include "reference_activity_selector.hpp"

#include <cmath>
#include <iomanip>
#include <charconv>
#include <limits>
#include <optional>
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

// The selected value: a literal attribute wins (a null reads as missing, as
// the reference skips None), then the first-class field the selector names.
std::optional<std::string> event_value(const Event& event, std::string_view key,
                                       bool activity) {
  // The activity follows the shared selector rule: a missing literal key or
  // a projected null is skipped, never read as the empty activity.
  if (activity) return streaming_event_present_value(event, key);
  if (key.empty()) {
    if (event.timestamp.empty()) return std::nullopt;
    return event.timestamp;
  }
  // An explicit timestamp key reads the literal attribute, else the
  // projection read from exactly that field; a canonical key is not read
  // from a timestamp imported from another field. A stream event is a plain
  // dictionary, so a recorded source key is the only field the projection
  // stands for (no adapter copies it under the canonical key).
  for (const auto& attribute : event.attributes)
    if (attribute.key == key) {
      if (attribute.is_null || attribute.value.empty()) return std::nullopt;
      return attribute.value;
    }
  const std::string_view source = !event.timestamp_source_key.empty() ? std::string_view(event.timestamp_source_key)
                                  : event.timestamp_attribute ? std::string_view(event.timestamp_attribute->key)
                                                              : std::string_view();
  const bool names = source.empty() ? (key == "time:timestamp" || key == "timestamp") : source == key;
  if (!names) return std::nullopt;
  const auto* value = detail::present_projection(event.timestamp, event.timestamp_state);
  if (!value || value->empty()) return std::nullopt;
  return *value;
}

}  // namespace

StreamingTemporalConformance::StreamingTemporalConformance(
    TemporalProfile profile, StreamingTemporalOptions options)
    : profile_(std::move(profile)), options_(std::move(options)) {
  if (options_.case_id_attribute.empty())
    throw std::invalid_argument(
        "streaming temporal case-id attribute must not be empty");
  // Any finite zeta is accepted, as in the reference: zero or a negative
  // value reports every observed pair whose distance exceeds it.
  if (!std::isfinite(options_.zeta))
    throw std::invalid_argument("streaming temporal zeta must be finite");
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
  // Empty case ids and activities are ordinary values, as in the reference;
  // an unparseable timestamp skips the event (the reference fails on it).
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
    bool sentinel = false;
    if (entry.standard_deviation_seconds == 0.0) {
      if (difference == 0.0) continue;
      // The reference reports the integer sys.maxsize.
      distance = 9223372036854775807.0;
      sentinel = true;
    } else {
      distance = difference / entry.standard_deviation_seconds;
      if (distance <= options_.zeta) continue;
    }
    new_deviations.push_back(
        {case_key, previous.activity, activity_key, elapsed, distance, sentinel});
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
  const auto activity = event_value(event, options_.activity_attribute, true);
  // The start timestamp defaults to the timestamp key, as in the reference.
  const auto start = event_value(event,
      options_.start_timestamp_attribute.empty() ? options_.timestamp_attribute
                                                 : options_.start_timestamp_attribute,
      false);
  const auto timestamp = event_value(event, options_.timestamp_attribute, false);
  if (!activity || !start || !timestamp) return {};
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
      // Shortest round-trip text keeps every digit of the values.
      const auto number = [&out](double value) {
        char buffer[32];
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
        out.write(buffer, result.ptr - buffer);
      };
      out << ",\"elapsed_seconds\":";
      number(deviation.elapsed_seconds);
      out << ",\"standard_deviation_distance\":";
      if (deviation.zero_variance_sentinel) out << "9223372036854775807";
      else number(deviation.standard_deviation_distance);
      out << '}';
    }
    out << ']';
  }
  out << "}}";
  return out.str();
}

}  // namespace minera
