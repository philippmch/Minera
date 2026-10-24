#pragma once

#include <stdexcept>
#include <utility>

#include "minera/ocel.hpp"
#include "minera/ocdfg_edge_performance_options.hpp"
#include "minera/timestamp.hpp"

namespace minera {
// Separate datetime, missing/NaT and literal text cells. A native OCEL's
// primary timestamp has datetime semantics; direct typed callers can retain
// the original table cell kind without silently parsing ordinary text.
// Generic timestamp selectors use the event type's datetime schema; other
// columns retain literal strings, including ISO-looking text and "NaT".
using OcdfgPerformanceTimestamp =
    std::variant<std::monostate, ParsedExactTimestamp, std::string>;
struct OcdfgPerformanceEvent {
  std::string id;
  OcdfgPerformanceTimestamp timestamp;
};
class OcdfgPerformanceError : public std::invalid_argument {
 public:
  OcdfgPerformanceError(std::string type, std::string message)
      : std::invalid_argument(message), type(std::move(type)) {}
  const std::string type;
};
// Signed elapsed seconds between two instants, floored to microseconds and
// converted the way a timedelta's day/second/microsecond components sum:
// whole seconds first, then the microsecond fraction. A negative interval
// therefore keeps the cancellation the public results show, e.g. -0.519789 s
// is -1 + 0.480211, not the correctly rounded quotient.
[[nodiscard]] double ocdfg_elapsed_seconds(const ParsedExactTimestamp& from,
                                           const ParsedExactTimestamp& to);
[[nodiscard]] std::vector<double> calculate_ocdfg_edge_performance(
    const std::vector<OcdfgPerformanceEvent>& events,
    const std::vector<std::pair<std::string, std::string>>& event_couples,
    const OcdfgEdgePerformanceOptions& = {});
[[nodiscard]] std::vector<double> calculate_ocdfg_edge_performance(
    const Ocel&,
    const std::vector<std::pair<std::string, std::string>>& event_couples,
    const OcdfgEdgePerformanceOptions& = {});
}  // namespace minera
