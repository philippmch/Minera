#pragma once

#include <stdexcept>
#include <string>

#include "minera/event_data.hpp"

namespace minera {

struct PandasBetweenOptions {
  EventAttribute case_id_key{"", "case:concept:name"};
  EventAttribute activity_key{"", "concept:name"};
  EventAttribute subcase_concat_str{"", "##@@"};
};

class PandasBetweenError : public std::invalid_argument {
 public:
  PandasBetweenError(std::string type, std::string message)
      : std::invalid_argument(message), type(std::move(type)), message(std::move(message)) {}
  const std::string type;
  const std::string message;
};

// Public-shaped pandas between behavior on native tables. Case blocks use
// each ordered case value's first position and total count, including for
// interleaved source rows. Completed blocks mark inclusive positional slices;
// a later mark replaces an earlier mark on an overlapping boundary row.
[[nodiscard]] EventTable filter_pandas_between(
    const EventTable& source, const EventAttribute& act1,
    const EventAttribute& act2, const PandasBetweenOptions& options = {});

// Ordinary public parameter carriers: None/empty list use defaults, dictionary
// entries bind the three public string keys, and invalid carriers retain the
// observed diagnostics. Callers resolve enum-key precedence before transport.
[[nodiscard]] EventTable filter_pandas_between(
    const EventTable& source, const EventAttribute& act1,
    const EventAttribute& act2, const EventAttribute& parameters);

}  // namespace minera
