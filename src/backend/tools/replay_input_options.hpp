#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "minera/conformance.hpp"

namespace minera::tools {

// The application imports CSV activity columns into Event::activity while
// retaining their source spelling. Native replay intentionally selects exact
// fields. An omitted application selector should follow that imported field,
// without weakening explicit selectors or inventing a missing canonical cell.
[[nodiscard]] inline TokenReplayOptions default_replay_input_options(
    const EventLog& log, TokenReplayOptions options,
    bool activity_selector_supplied = false) {
  if (activity_selector_supplied || options.activity_attribute != "concept:name")
    return options;

  std::optional<std::string> source;
  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) {
      for (const auto& attribute : event.attributes) {
        if (attribute.key == options.activity_attribute ||
            attribute.first_class_source_key == options.activity_attribute)
          return options;
      }
      const std::string_view projected_key = !event.activity_source_key.empty()
          ? std::string_view(event.activity_source_key)
          : event.activity_attribute
              ? !event.activity_attribute->first_class_source_key.empty()
                  ? std::string_view(event.activity_attribute->first_class_source_key)
                  : std::string_view(event.activity_attribute->key)
              : std::string_view("concept:name");
      if (projected_key.empty() || projected_key == options.activity_attribute)
        return options;
      if (source && *source != projected_key) return options;
      source = projected_key;
    }
  }
  if (source) options.activity_attribute = *source;
  return options;
}

}  // namespace minera::tools
