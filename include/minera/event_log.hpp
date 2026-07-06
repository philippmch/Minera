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
};

struct Event {
  std::string case_id;
  std::string activity;
  std::string timestamp;
  std::string resource;  // who performed the activity; empty when unknown
  std::size_t input_order = 0;
  std::vector<EventAttribute> attributes;  // in input order

  // The value of the named generic attribute, or nullptr when the event does
  // not carry it. Linear, but events carry only a handful of attributes.
  [[nodiscard]] const std::string* attribute(std::string_view key) const {
    for (const auto& entry : attributes) {
      if (entry.key == key) return &entry.value;
    }
    return nullptr;
  }
};

struct Trace {
  std::string case_id;
  std::vector<Event> events;
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

}  // namespace minera
