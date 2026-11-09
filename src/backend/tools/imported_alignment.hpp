#pragma once

// Alignments of a supplied (imported) model as the reference's public
// alignment dispatcher reports them; see minera::reference_log_alignment.

#include "minera/conformance.hpp"
#include "minera/event_log.hpp"
#include "minera/petri_net.hpp"
#include "minera/petri_trace_alignment.hpp"

namespace minera::tools {

[[nodiscard]] inline bool reference_imported_alignment_applies(
    const AlignmentOptions& options) {
  return reference_log_alignment_applies(options);
}

[[nodiscard]] inline AlignmentResult imported_model_alignment(
    const EventLog& log, const PetriNet& net, const AlignmentOptions& options) {
  return reference_log_alignment(log, net, options);
}

}  // namespace minera::tools
