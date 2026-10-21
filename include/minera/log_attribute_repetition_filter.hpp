#pragma once

#include <limits>
#include <string>
#include <variant>

#include "minera/event_log.hpp"
#include "minera/token_integer.hpp"

namespace minera {

// Ordinary date/bytes/container bounds retain lazy public comparison errors.
// Integer tokens compare exactly without conversion through binary64. The
// EventAttribute alternative is appended to preserve existing variant indexes.
using LogAttributeRepetitionBound =
    std::variant<std::nullptr_t, bool, TokenInteger, double, std::string, NativeNA,
                 EventAttribute>;
// Native event keys are strings. Hashable nonstring selectors find no stored
// cell. Mutable selectors raise only when event lookup is actually reached.
using LogAttributeRepetitionKey =
    std::variant<std::string, std::nullptr_t, bool, TokenInteger, double,
                 EventAttribute>;

struct LogAttributeRepetitionOptions {
  LogAttributeRepetitionKey attribute_key = std::string("concept:name");
  LogAttributeRepetitionBound minimum = TokenInteger(2);
  LogAttributeRepetitionBound maximum =
      TokenInteger(std::numeric_limits<std::int64_t>::max());
};

// Count present typed-equal cells in every trace, then evaluate the chained
// minimum <= count <= maximum condition in that order. Missing cells do not
// count; zero-count and empty traces may qualify. Full stored context survives.
[[nodiscard]] EventLog filter_log_attribute_value_repetition(
    const EventLog& source, const EventAttribute& target,
    const LogAttributeRepetitionOptions& options = {});

}  // namespace minera
