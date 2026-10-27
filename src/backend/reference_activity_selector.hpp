#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

#include "minera/event_log.hpp"

namespace minera {

// The activity a reference-style `activity_key` names on one event.
// A literal attribute with the selector's exact spelling wins over every
// alias, so a field that is literally called "activity" is read even when
// the canonical concept:name differs. The canonical name must be present:
// a missing concept:name is an error (the reference raises KeyError), while
// a present empty label stays a valid activity. "activity" without such a
// literal field still names the canonical projection, as the flat adapters
// have always used it.
inline const std::string& reference_selected_activity(const Event& event, std::string_view selector,
                                                      std::string_view context) {
  const auto missing = [&](std::string_view key) {
    return std::invalid_argument(std::string(context) + ": event is missing activity attribute '" +
                                 std::string(key) + "'");
  };
  const bool canonical = selector.empty() || selector == "concept:name";
  if (!canonical) {
    for (const auto& attribute : event.attributes) {
      if (attribute.key != selector) continue;
      if (attribute.is_null) throw missing(selector);
      return attribute.value;
    }
    const bool projected = (!event.activity_source_key.empty() && event.activity_source_key == selector) ||
                           (event.activity_attribute && event.activity_attribute->key == selector);
    if (!projected && selector != "activity") throw missing(selector);
  }
  if (!event.has_activity()) throw missing(canonical ? std::string_view("concept:name") : selector);
  return event.activity;
}

// As above, but "org:resource" and "resource" without a literal field of
// that name read the event's resource projection, which must be present.
inline const std::string& reference_selected_label(const Event& event, std::string_view selector,
                                                   std::string_view context) {
  if (selector == "org:resource" || selector == "resource") {
    for (const auto& attribute : event.attributes) {
      if (attribute.key != selector) continue;
      if (attribute.is_null) break;
      return attribute.value;
    }
    if (!event.has_resource())
      throw std::invalid_argument(std::string(context) + ": event is missing activity attribute '" +
                                  std::string(selector) + "'");
    return event.resource;
  }
  return reference_selected_activity(event, selector, context);
}

}  // namespace minera
