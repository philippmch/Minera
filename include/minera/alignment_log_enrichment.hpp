#pragma once

#include <optional>
#include <string>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/petri_trace_alignment.hpp"

namespace minera {

// Public algo.conformance.alignments.petri_net.utils.log_enrichment: every
// event aligned by a synchronous move receives the name of its model
// transition under target_attribute. Alignment entry i belongs to trace i;
// log moves only advance through the trace's events, model moves touch none.
struct AlignmentLogEnrichmentOptions {
  std::string target_attribute = "@@transition_id";
};

// One entry per aligned trace; nullopt (or an alignment that was not
// computed) stands for a public None entry. transition_names holds the model
// transition names by native index (PnmlDocument::transition_ids for an
// imported net). Errors follow the public order of operations: a None entry
// raises std::invalid_argument ("'NoneType' object is not subscriptable");
// a synchronous move raises std::out_of_range ("list index out of range")
// when its entry lies beyond the log or its log-side position beyond the
// trace's events. Log and model moves alone never raise. Fewer entries than traces
// leave the remaining traces unchanged. The in-place form keeps the
// enrichment made before an error, like the public call without deepcopy; the
// copying form (the ENABLE_DEEPCOPY parameter) leaves its input untouched.
// "concept:name", "org:resource" and "lifecycle:transition" set the
// first-class field; "time:timestamp" is rejected, because a native timestamp
// cannot hold a transition name.
void enrich_log_with_alignments_in_place(
    EventLog& log, const std::vector<std::optional<PetriTraceAlignment>>& aligned_traces,
    const std::vector<std::string>& transition_names,
    const AlignmentLogEnrichmentOptions& options = {});

[[nodiscard]] EventLog enrich_log_with_alignments(
    const EventLog& log, const std::vector<std::optional<PetriTraceAlignment>>& aligned_traces,
    const std::vector<std::string>& transition_names,
    const AlignmentLogEnrichmentOptions& options = {});

}  // namespace minera
