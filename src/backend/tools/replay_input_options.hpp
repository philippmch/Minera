#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "minera/conformance.hpp"
#include "minera/request_params.hpp"

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

template <typename Options>
[[nodiscard]] Options default_evaluation_input_options(
    const EventLog& log, Options options, bool activity_selector_supplied,
    std::string_view model_activity = "concept:name") {
  if (activity_selector_supplied) return options;
  TokenReplayOptions replay;
  replay.activity_attribute = options.activity_attribute;
  if (replay.activity_attribute == "concept:name" && !model_activity.empty())
    replay.activity_attribute = model_activity;
  options.activity_attribute =
      default_replay_input_options(log, std::move(replay)).activity_attribute;
  return options;
}

[[nodiscard]] inline EvaluationParams default_evaluation_input_params(
    const EventLog& log, EvaluationParams params,
    bool activity_selector_supplied = false,
    std::string_view model_activity = "concept:name") {
  params.replay_fitness_options = default_evaluation_input_options(
      log, std::move(params.replay_fitness_options), activity_selector_supplied,
      model_activity);
  params.precision_options = default_evaluation_input_options(
      log, std::move(params.precision_options), activity_selector_supplied,
      model_activity);
  params.generalization_options = default_evaluation_input_options(
      log, std::move(params.generalization_options), activity_selector_supplied,
      model_activity);
  params.composite_options = default_evaluation_input_options(
      log, std::move(params.composite_options), activity_selector_supplied,
      model_activity);
  return params;
}

// Alignment evaluators also consult named attributes. Expose a consumed CSV
// activity column only in their input copy, without replacing existing cells
// or treating another selector as an alias of the imported source column.
[[nodiscard]] inline std::optional<EventLog> evaluation_input_log(
    const EventLog& log, std::string_view activity_attribute) {
  if (activity_attribute == "concept:name" || activity_attribute == "activity")
    return std::nullopt;
  std::optional<EventLog> projected;
  for (std::size_t t = 0; t < log.traces.size(); ++t) {
    for (std::size_t e = 0; e < log.traces[t].events.size(); ++e) {
      const auto& event = log.traces[t].events[e];
      if (event.activity_source_key != activity_attribute) continue;
      if (event.activity_state == FirstClassAttributeState::kMissing &&
          event.activity.empty() && !event.activity_attribute)
        continue;
      bool exists = false;
      for (const auto& field : event.attributes) {
        if (field.key == activity_attribute ||
            field.first_class_source_key == activity_attribute) {
          exists = true;
          break;
        }
      }
      if (exists) continue;
      if (!projected) projected = log;
      EventAttribute field = event.activity_attribute.value_or(
          EventAttribute{std::string(activity_attribute), event.activity});
      field.key = activity_attribute;
      field.first_class_role = FirstClassAttributeRole::kActivity;
      field.first_class_source_key = event.activity_source_key;
      field.first_class_state = event.activity_state;
      field.is_null = field.is_null ||
                      event.activity_state == FirstClassAttributeState::kNull;
      projected->traces[t].events[e].attributes.push_back(std::move(field));
    }
  }
  return projected;
}

}  // namespace minera::tools
