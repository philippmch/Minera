#pragma once

// algo.evaluation.precision.utils of the PM4Py 2.7.22 reference: the log
// prefixes used by the ETConformance and alignment-based precision variants,
// the fake log built from those prefixes, and the module's search for the
// markings that a synchronous product reaches once a prefix is consumed.

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/event_data.hpp"
#include "minera/event_log.hpp"

namespace minera {

class PrecisionUtilsError : public std::invalid_argument {
 public:
  PrecisionUtilsError(std::string type, const std::string& message)
      : std::invalid_argument(message), type(std::move(type)) {}
  std::string type;
};

// get_log_prefixes: every proper non-empty prefix of a trace, joined by ",",
// mapped to the activities that follow it (sorted here; a set in the
// reference), and the number of times it is followed by an activity. Both
// lists are in first-appearance order.
struct PrecisionLogPrefixes {
  std::vector<std::pair<std::string, std::vector<std::optional<std::string>>>> prefixes;
  std::vector<std::pair<std::string, std::size_t>> counts;
};

// Every event must carry the activity key (KeyError otherwise); a null
// activity inside a joined prefix raises the reference's TypeError.
[[nodiscard]] PrecisionLogPrefixes precision_log_prefixes(const EventLog& log,
                                                          const std::string& activity_key = "concept:name");
// A table is read as a log: cases in sorted case-id order (numeric for
// numeric ids), the events of a case in row order.
[[nodiscard]] PrecisionLogPrefixes precision_log_prefixes(const EventTable& table,
                                                          const std::string& activity_key = "concept:name",
                                                          const std::string& case_id_key = "case:concept:name");

// form_fake_log: one trace per prefix, one event per ","-separated part
// (empty parts included) carrying only the activity key.
[[nodiscard]] EventLog precision_fake_log(const std::vector<std::string>& prefixes,
                                          const std::string& activity_key = "concept:name");

// A synchronous product: each transition label is a (trace, model) pair in
// which nullopt is None and "skip" marks a move on one side only.
using PrecisionMarking = std::vector<std::pair<std::size_t, std::size_t>>;  // sorted (place, tokens)

struct PrecisionSyncTransition {
  std::optional<std::string> trace_label, model_label;
  double cost = 0;
  PrecisionMarking in, out;  // (place, weight)
};

struct PrecisionSyncNet {
  std::size_t places = 0;
  std::vector<PrecisionSyncTransition> transitions;
};

// The module's __search: a cheapest-first exploration from the initial
// marking that fires only synchronous moves and invisible model moves
// (skip, None). A marking is expanded at most once, when it leaves the
// queue; one that covers `stop` is recorded instead of expanded, and the
// search ends at the first queued cost above the cheapest recorded one.
// Returns every recorded marking (sorted), or nullopt when none is reached.
[[nodiscard]] std::optional<std::vector<PrecisionMarking>> precision_sync_search(
    const PrecisionSyncNet& net, const PrecisionMarking& initial, const PrecisionMarking& stop,
    const std::string& skip = ">>");

}  // namespace minera
