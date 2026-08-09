#pragma once

#include <cstdint>
#include <limits>
#include <string>

#include "minera/event_log.hpp"

namespace minera {

struct AttributeValueRepetitionOptions {
  // Empty, "activity", and "concept:name" select Event::activity. The
  // resource, lifecycle, and timestamp names select their first-class fields;
  // every other key is looked up in Event::attributes.
  std::string attribute_key;
  std::string value;
  std::uint64_t min_repetitions = 2;
  std::uint64_t max_repetitions = std::numeric_limits<std::uint64_t>::max();
};

[[nodiscard]] EventLog filter_attribute_value_repetition(
    const EventLog& event_log,
    const AttributeValueRepetitionOptions& options = {});

[[nodiscard]] EventLog filter_attr_value_repetition(
    const EventLog& event_log,
    const AttributeValueRepetitionOptions& options = {});

[[nodiscard]] EventLog filter_event_log_attr_value_repetition(
    const EventLog& event_log,
    const AttributeValueRepetitionOptions& options = {});

}  // namespace minera
