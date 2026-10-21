#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <variant>

#include "minera/event_data.hpp"
#include "minera/token_integer.hpp"

namespace minera {

using PandasAttributeRepetitionScalar =
    std::variant<std::nullptr_t, bool, TokenInteger, double, std::string>;

struct PandasAttributeRepetitionOptions {
  PandasAttributeRepetitionScalar case_id_key = std::string("case:concept:name");
  PandasAttributeRepetitionScalar attribute_key = std::string("concept:name");
  PandasAttributeRepetitionScalar minimum = TokenInteger(2);
  PandasAttributeRepetitionScalar maximum =
      TokenInteger(std::numeric_limits<std::int64_t>::max());
};

class PandasAttributeRepetitionError : public std::invalid_argument {
 public:
  PandasAttributeRepetitionError(std::string type, std::string message)
      : std::invalid_argument(std::move(message)), type(std::move(type)) {}
  const std::string type;
};

// Filter complete source rows by the count of matching values in their case.
// Only cases observed among matching rows are eligible, including at minimum0.
// Dataframe dtypes, categorical declarations, attributes and source index labels
// are retained. Native tables have unique string column labels.
[[nodiscard]] EventTable filter_pandas_attribute_value_repetition(
    const EventTable& source, const EventAttribute& target,
    const PandasAttributeRepetitionOptions& options = {});

}  // namespace minera
