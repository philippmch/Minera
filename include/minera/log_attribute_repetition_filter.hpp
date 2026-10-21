#pragma once

#include <limits>
#include <string>
#include <variant>

#include "minera/event_log.hpp"
#include "minera/token_integer.hpp"

namespace minera {

// Null/string bounds retain the public lazy comparison errors. Integer tokens
// compare exactly without conversion through binary64.
using LogAttributeRepetitionBound =
    std::variant<std::nullptr_t, bool, TokenInteger, double, std::string, NativeNA>;
// Native event keys are strings. Supplied nonstring selectors therefore find
// no stored cell; they remain distinct from a literal empty string selector.
using LogAttributeRepetitionKey =
    std::variant<std::string, std::nullptr_t, bool, TokenInteger, double>;

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
