#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/performance.hpp"
#include "minera/petri_net.hpp"

namespace minera {

// Frequency and performance annotations for one workflow net, measured by
// token-replaying the log on it: how often each transition actually fires,
// and how long tokens rest in each place between being produced and being
// consumed — the model-level counterpart of the process map's edge
// frequencies and waiting times. Only traces that replay cleanly contribute
// (a forced replay's firings would not be a real path through the model), so
// on a model that does not fit the whole log the annotations describe the
// fitting part and `decorated_traces` says how much that is.
struct ModelDecoration {
  // Per transition, in PetriNet::transitions() order: firings across all
  // cleanly replaying traces, silent routing included.
  std::vector<std::uint64_t> transition_counts;
  // Per place id: tokens consumed from the place across those traces.
  std::vector<std::uint64_t> place_counts;
  // Per place id: waiting time between a token's production (the instant of
  // the event whose firing produced it; the case's first instant for the
  // initial source token) and its consumption. Only tokens with parseable
  // instants on both ends contribute, so an untimed log yields empty stats
  // (count 0) while the counts above still tally.
  std::vector<DurationStats> place_waits;
  std::uint64_t decorated_traces = 0;  // traces whose replay was clean
  std::uint64_t trace_count = 0;
};

// Decorates `net` with the replay of `log`. Tokens are matched
// first-in-first-out per place, and a silent transition fires at the instant
// of the event whose consumption triggered it (the routing to the sink at the
// case's last instant).
[[nodiscard]] ModelDecoration decorate_model(const EventLog& log,
                                             const PetriNet& net,
                                             std::string_view activity_attribute =
                                                 "concept:name",
                                             std::string_view timestamp_attribute =
                                                 "time:timestamp");

}  // namespace minera
