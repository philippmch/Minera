#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

#include "minera/event_data.hpp"
#include "minera/token_integer.hpp"

namespace minera {

using PolarsAttributeRepetitionScalar =
    std::variant<std::nullptr_t, bool, TokenInteger, double, std::string>;

struct PolarsAttributeRepetitionOptions {
  PolarsAttributeRepetitionScalar case_id_key = std::string("case:concept:name");
  PolarsAttributeRepetitionScalar attribute_key = std::string("concept:name");
  PolarsAttributeRepetitionScalar minimum = TokenInteger(2);
  PolarsAttributeRepetitionScalar maximum =
      TokenInteger(std::numeric_limits<std::int64_t>::max());
};

class PolarsAttributeRepetitionError : public std::invalid_argument {
 public:
  PolarsAttributeRepetitionError(std::string type, std::string phase, std::string message)
      : std::invalid_argument(message), type(std::move(type)), phase(std::move(phase)), message(std::move(message)) {}
  const std::string type;
  const std::string phase;
  // Public diagnostic plans can include literal NUL column names. what() has
  // no length carrier; use message when transporting the complete bytes.
  const std::string message;
};

// Visible Polars frame companion over String, Int8-64, UInt8-64, Float32/64,
// Boolean, Null, Categorical, Enum, Date and ms/us/ns Datetime columns with
// optional zones. The literal model follows Python value conversion (integer
// class, fixed offsets as UTC, named zones kept, nanoseconds floored to
// microseconds); comparisons, coercion errors and their rendered expressions
// follow the column x literal matrix, including the optimizer folding integer
// bounds with minimum above maximum to an empty count. Polars has no index;
// native indexed tables reject instead of silently discarding their index.
[[nodiscard]] EventTable filter_polars_attribute_value_repetition(
    const EventTable& source, const EventAttribute& target,
    const PolarsAttributeRepetitionOptions& = {});

// Selector construction errors occur at this call. Schema/coercion errors and
// source scanning are deferred until collection/materialization. The returned
// producer is restartable and retains source metadata and complete rows.
[[nodiscard]] LazyEventTable filter_polars_attribute_value_repetition(
    const LazyEventTable& source, const EventAttribute& target,
    const PolarsAttributeRepetitionOptions& = {});

}  // namespace minera
