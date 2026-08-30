#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace minera {

// The XES attribute type an event attribute round-trips as. CSV columns and
// unknown XES elements read as strings; the value itself always stays text.
enum class AttributeKind { kString, kDate, kInt, kFloat, kBoolean, kId };

// One generic event attribute beyond the four first-class fields: any extra
// CSV column, or any XES event attribute other than the activity, timestamp,
// and resource keys.
struct EventAttribute {
  std::string key;
  std::string value;
  AttributeKind kind = AttributeKind::kString;
  // A present null differs from a missing attribute and from an empty string.
  // Text formats that cannot represent null values omit such attributes.
  bool is_null = false;

  bool operator==(const EventAttribute&) const = default;
};

struct LogExtension {
  std::string name;
  std::string prefix;
  std::string uri;

  bool operator==(const LogExtension&) const = default;
};

struct LogClassifier {
  std::string name;
  std::vector<std::string> keys;

  bool operator==(const LogClassifier&) const = default;
};

// Metadata shared by trace logs, flat streams, and tabular event data.
struct EventDataMetadata {
  std::vector<LogExtension> extensions;
  std::vector<LogClassifier> classifiers;
  std::vector<EventAttribute> trace_globals;
  std::vector<EventAttribute> event_globals;
  std::vector<EventAttribute> properties;

  bool operator==(const EventDataMetadata&) const = default;
};

struct Event {
  std::string case_id;
  std::string activity;
  std::string timestamp;
  std::string resource;  // who performed the activity; empty when unknown
  std::size_t input_order = 0;
  std::vector<EventAttribute> attributes;  // in input order
  // XES lifecycle:transition ("start", "complete", …); empty when the log
  // records atomic events. Kept last so aggregate initializers without it
  // stay valid.
  std::string lifecycle;

  // The value of the named generic attribute, or nullptr when it is missing
  // or explicitly null. Linear, but events carry only a handful of attributes.
  [[nodiscard]] const std::string* attribute(std::string_view key) const {
    for (const auto& entry : attributes) {
      if (entry.key == key) return entry.is_null ? nullptr : &entry.value;
    }
    return nullptr;
  }
};

struct Trace {
  std::string case_id;
  std::vector<Event> events;
  // Case-level attributes beyond the id: any other scalar XES trace attribute,
  // or a `case:`-prefixed CSV column (constant per case, first value wins).
  std::vector<EventAttribute> attributes;
  std::vector<EventAttribute> properties;

  // The value of the named case attribute, or nullptr when it is missing or
  // explicitly null. Linear, but traces carry only a handful of attributes.
  [[nodiscard]] const std::string* attribute(std::string_view key) const {
    for (const auto& entry : attributes) {
      if (entry.key == key) return entry.is_null ? nullptr : &entry.value;
    }
    return nullptr;
  }
};

// Orders a trace's events by their parsed timestamp instant (normalized to UTC
// via parse_timestamp), so events recorded with different timezone offsets or
// formats still sort by the moment they actually occurred. Document order
// (input_order) breaks ties. When any event's timestamp is not a recognizable
// instant, the function falls back to a stable lexicographic order so the
// comparison stays a consistent strict weak ordering.
void order_events_by_time(std::vector<Event>& events);

struct EventLog {
  std::vector<Trace> traces;
  // Scalar log-level XES attributes and derived-model projection markers.
  // Kept after traces so existing aggregate initializers that provide only
  // the trace vector remain source-compatible.
  std::vector<EventAttribute> attributes;
  EventDataMetadata metadata;

  [[nodiscard]] const std::string* attribute(std::string_view key) const {
    for (const auto& entry : attributes) {
      if (entry.key == key) return entry.is_null ? nullptr : &entry.value;
    }
    return nullptr;
  }

  [[nodiscard]] std::size_t trace_count() const noexcept {
    return traces.size();
  }

  [[nodiscard]] std::size_t event_count() const noexcept {
    std::size_t count = 0;
    for (const auto& trace : traces) {
      count += trace.events.size();
    }
    return count;
  }
};

inline void copy_event_log_context(const EventLog& source, EventLog& target) {
  target.attributes = source.attributes;
  target.metadata = source.metadata;
}

}  // namespace minera
