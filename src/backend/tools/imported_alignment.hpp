#pragma once

// Alignments of a supplied (imported) model as the reference's public
// alignment dispatcher reports them; see minera::reference_log_alignment.

#include "minera/conformance.hpp"
#include "minera/event_log.hpp"
#include "minera/petri_net.hpp"
#include "minera/petri_reference_arcs.hpp"
#include "minera/petri_trace_alignment.hpp"

#include <stdexcept>

namespace minera::tools {

[[nodiscard]] inline bool reference_imported_alignment_applies(
    const AlignmentOptions& options) {
  return reference_log_alignment_applies(options);
}

[[nodiscard]] inline AlignmentResult imported_model_alignment(
    const EventLog& log, const PetriNet& net, const AlignmentOptions& options) {
  return reference_log_alignment(log, net, options);
}

// The reference's log alignments check easy soundness before reading the
// log, so a log without traces fails on a net whose final marking is
// unreachable as a non-empty log does (rounds GH and GI, observed
// black-box: "trying to apply alignments on a Petri net that is not a easy
// sound net!!"). The core alignment keeps an empty log trivially computed;
// the routes and the export that mirror the reference's log alignments
// apply this check to its result. A net with weighted or parallel arcs is
// checked by the reference's own rules for them (reference_easy_sound).
inline void fail_empty_log_alignment_on_unsound_net(AlignmentResult& result, const PetriNet& net,
                                                    const AlignmentOptions& options) {
  if (!result.computed || result.trace_count != 0) return;
  if (petri_net_has_weighted_arcs(net)) {
    PetriTraceAlignmentOptions check;
    check.initial_marking = options.initial_marking;
    check.final_marking = options.final_marking;
    check.max_states = options.max_states;
    check.max_time_seconds = options.max_time_seconds;
    check.cancelled = options.cancelled;
    try {
      if (reference_easy_sound(net, check)) return;
    } catch (const std::invalid_argument&) {
      result.exhausted = true;
    }
    result.computed = false;
    return;
  }
  EventLog probe;
  probe.traces.push_back(Trace{});
  const auto soundness = check_alignment(probe, net, options);
  if (soundness.computed) return;
  result.computed = false;
  result.exhausted = soundness.exhausted;
  result.time_limit_reached = soundness.time_limit_reached;
}

}  // namespace minera::tools
