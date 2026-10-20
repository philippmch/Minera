#include "minera/log_trace_filter.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace minera {
namespace {
enum class Kind { literal, repeated_terminated, one_terminated, repeated_prefixed };
struct Part { Kind kind; std::string literal; };
struct Pattern { bool universal = false; std::vector<Part> parts; };

Pattern compile(const std::vector<std::string>& input) {
  Pattern result;
  for (std::size_t i = 0; i < input.size(); ++i) {
    const bool last = i + 1 == input.size();
    if (input[i] == "...") {
      if (last) {
        // The observed unanchored lone-wildcard expression matches an empty
        // suffix of every serialized trace when searched.
        if (i == 0) { result.universal = true; return result; }
        auto& previous = result.parts.back();
        if (previous.kind == Kind::literal) previous.literal.pop_back();
        else previous.kind = Kind::one_terminated;
        result.parts.push_back({Kind::repeated_prefixed, {}});
      } else result.parts.push_back({Kind::repeated_terminated, {}});
    } else result.parts.push_back({Kind::literal, input[i] + (last ? "" : ",")});
  }
  return result;
}

bool matches(std::string_view text, const Pattern& pattern) {
  if (pattern.universal) return true;
  std::vector<unsigned char> previous(text.size() + 1), current(text.size() + 1);
  previous[0] = 1;
  for (const auto& part : pattern.parts) {
    std::fill(current.begin(), current.end(), 0);
    if (part.kind == Kind::literal) {
      if (part.literal.empty()) current = previous;
      else {
        // KMP makes all literal transitions iterative and linear in the
        // serialized trace plus literal length, including repeated commas.
        std::vector<std::size_t> prefix(part.literal.size());
        for (std::size_t i = 1, j = 0; i < part.literal.size(); ++i) {
          while (j && part.literal[i] != part.literal[j]) j = prefix[j - 1];
          if (part.literal[i] == part.literal[j]) ++j;
          prefix[i] = j;
        }
        for (std::size_t i = 0, j = 0; i < text.size(); ++i) {
          while (j && text[i] != part.literal[j]) j = prefix[j - 1];
          if (text[i] == part.literal[j]) ++j;
          if (j == part.literal.size()) {
            if (previous[i + 1 - j]) current[i + 1] = 1;
            j = prefix[j - 1];
          }
        }
      }
    } else if (part.kind == Kind::repeated_terminated) {
      bool seen = false;
      for (std::size_t i = 0; i <= text.size(); ++i) {
        seen = seen || previous[i];
        current[i] = previous[i] || (seen && i && text[i - 1] == ',');
      }
    } else if (part.kind == Kind::one_terminated) {
      bool active = false;
      for (std::size_t i = 0; i < text.size(); ++i) {
        active = active || previous[i];
        if (text[i] == ',') { current[i + 1] = active; active = false; }
      }
    } else {
      bool active = false;
      current[0] = previous[0];
      for (std::size_t i = 0; i < text.size(); ++i) {
        active = active || (previous[i] && text[i] == ',');
        current[i + 1] = previous[i + 1] || active;
      }
    }
    previous.swap(current);
  }
  // Python's '$' also matches immediately before one final LF; no general
  // whitespace trimming or newline normalization occurs.
  return previous.back() || (!text.empty() && text.back() == '\n' &&
                             previous[text.size() - 1]);
}

const std::string& string_attribute(const EventAttribute& attribute,
                                     std::string_view key) {
  if (attribute.kind != AttributeKind::kString || attribute.is_null ||
      attribute.container != AttributeContainerKind::kScalar ||
      attribute.scalar_wrapper || attribute.scalar_children_list ||
      !attribute.children.empty() ||
      !std::holds_alternative<std::monostate>(attribute.typed_value))
    throw std::invalid_argument("trace activity attribute must be a string: " + std::string(key));
  return attribute.value;
}

const std::string& activity(const Event& event, std::string_view key) {
  for (const auto& attribute : event.attributes)
    if (attribute.key == key) return string_attribute(attribute, key);
  const auto projected = [&](std::string_view default_key, const std::string& source_key,
                             const std::optional<EventAttribute>& attribute,
                             FirstClassAttributeState state, const std::string& value)
      -> const std::string* {
    const auto actual = !source_key.empty() ? std::string_view(source_key)
        : attribute ? std::string_view(attribute->key) : default_key;
    if (actual != key || (state == FirstClassAttributeState::kMissing && value.empty() && !attribute))
      return nullptr;
    if (state == FirstClassAttributeState::kNull)
      throw std::invalid_argument("trace activity attribute must be a string: " + std::string(key));
    return attribute ? &string_attribute(*attribute, key) : &value;
  };
  if (const auto* value = projected("concept:name", event.activity_source_key,
          event.activity_attribute, event.activity_state, event.activity)) return *value;
  if (const auto* value = projected("time:timestamp", event.timestamp_source_key,
          event.timestamp_attribute, event.timestamp_state, event.timestamp)) return *value;
  if (const auto* value = projected("org:resource", event.resource_source_key,
          event.resource_attribute, event.resource_state, event.resource)) return *value;
  if (const auto* value = projected("lifecycle:transition", event.lifecycle_source_key,
          event.lifecycle_attribute, event.lifecycle_state, event.lifecycle)) return *value;
  throw std::out_of_range("trace activity attribute is missing: " + std::string(key));
}
}  // namespace

EventLog filter_log_traces(const EventLog& source,
    const std::vector<std::vector<std::string>>& admitted_traces,
    const LogTraceFilterOptions& options) {
  std::vector<Pattern> patterns;
  for (const auto& admitted : admitted_traces) patterns.push_back(compile(admitted));
  EventLog result;
  copy_event_log_context(source, result);
  for (const auto& trace : source.traces) {
    std::string serialized;
    for (std::size_t i = 0; i < trace.events.size(); ++i) {
      if (i) serialized += ',';
      serialized += activity(trace.events[i], options.activity_key);
    }
    const bool matched = patterns.empty() || std::any_of(patterns.begin(), patterns.end(),
        [&](const auto& pattern) { return matches(serialized, pattern); });
    if (matched == options.positive) result.traces.push_back(trace);
  }
  return result;
}
}  // namespace minera
