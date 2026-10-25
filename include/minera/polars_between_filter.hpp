#pragma once

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/event_data.hpp"

namespace minera {

struct PolarsBetweenOptions {
  EventAttribute case_id_key{"", "case:concept:name"};
  EventAttribute activity_key{"", "concept:name"};
  EventAttribute subcase_concat_str{"", "##@@"};
};

class PolarsBetweenError : public std::invalid_argument {
 public:
  PolarsBetweenError(std::string type, std::string phase, std::string message)
      : std::invalid_argument(message), type(std::move(type)), phase(std::move(phase)),
        message(std::move(message)) {}
  const std::string type;
  const std::string phase;
  // Diagnostic plans can contain literal NUL labels; this retains the length.
  const std::string message;
};

// Typed native frame companion for the retained public Polars between domain.
// The public operation requires the canonical time:timestamp column and sorts
// by case and timestamp before extracting intervals. Frame semantics and
// native support boundaries are documented with the retained observations.
// Category label dictionaries and Enum order use categorical_dtypes. Selected
// Enum sort columns require that descriptor; named context identity and
// physical category representation are not modeled by the native carrier.
// A between_filter parameters dictionary: a Parameters member is read before
// the plain string equal to its value; other keys are ignored.
struct PolarsBetweenParameter { std::string key; EventAttribute value; bool enum_key = false; };
[[nodiscard]] PolarsBetweenOptions resolve_polars_between_parameters(const std::vector<PolarsBetweenParameter>&);
// A parameters argument as one value: None or a mapping, or any other
// carrier, which fails as the reference reads it (see check_parameter_carrier).
[[nodiscard]] PolarsBetweenOptions resolve_polars_between_parameter_value(const EventAttribute& parameters);

[[nodiscard]] EventTable filter_polars_between(
    const EventTable& source, const EventAttribute& act1, const EventAttribute& act2,
    const PolarsBetweenOptions& = {});

[[nodiscard]] LazyEventTable filter_polars_between(
    const LazyEventTable& source, const EventAttribute& act1, const EventAttribute& act2,
    const PolarsBetweenOptions& = {});

}  // namespace minera
