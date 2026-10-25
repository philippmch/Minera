#include "minera/alignment_log_enrichment.hpp"

#include <algorithm>
#include <stdexcept>

namespace minera {
namespace {

void set_target(Event& event, const std::string& key, const std::string& value) {
  if (key == "time:timestamp" || key == "timestamp")
    throw std::invalid_argument("alignment enrichment cannot store a transition name as the timestamp");
  const auto first_class = [&](std::string& field, FirstClassAttributeState& state,
                               std::optional<EventAttribute>& selected) {
    field = value;
    state = FirstClassAttributeState::kPresent;
    selected.reset();
  };
  if (key == "concept:name") return first_class(event.activity, event.activity_state, event.activity_attribute);
  if (key == "org:resource") return first_class(event.resource, event.resource_state, event.resource_attribute);
  if (key == "lifecycle:transition")
    return first_class(event.lifecycle, event.lifecycle_state, event.lifecycle_attribute);
  const auto existing = std::find_if(event.attributes.begin(), event.attributes.end(),
                                     [&](const auto& attribute) { return attribute.key == key; });
  EventAttribute attribute;
  attribute.key = key;
  attribute.value = value;
  if (existing == event.attributes.end()) event.attributes.push_back(std::move(attribute));
  else *existing = std::move(attribute);
}

}  // namespace

void enrich_log_with_alignments_in_place(
    EventLog& log, const std::vector<std::optional<PetriTraceAlignment>>& aligned_traces,
    const std::vector<std::string>& transition_names,
    const AlignmentLogEnrichmentOptions& options) {
  for (std::size_t index = 0; index < aligned_traces.size(); ++index) {
    const auto& aligned = aligned_traces[index];
    if (!aligned || !aligned->computed)
      throw std::invalid_argument("'NoneType' object is not subscriptable");
    // The trace and its event are looked up only for a synchronous move, so
    // an entry beyond the log, or log moves past the trace's end, raise
    // nothing on their own.
    std::size_t position = 0;
    for (const auto& move : aligned->alignment) {
      if (move.log == ">>") continue;
      if (move.transition) {
        if (index >= log.traces.size()) throw std::out_of_range("list index out of range");
        auto& events = log.traces[index].events;
        if (position >= events.size()) throw std::out_of_range("list index out of range");
        if (*move.transition >= transition_names.size())
          throw std::invalid_argument("alignment move names an unknown transition");
        set_target(events[position], options.target_attribute, transition_names[*move.transition]);
      }
      ++position;
    }
  }
}

EventLog enrich_log_with_alignments(
    const EventLog& log, const std::vector<std::optional<PetriTraceAlignment>>& aligned_traces,
    const std::vector<std::string>& transition_names,
    const AlignmentLogEnrichmentOptions& options) {
  auto copy = log;
  enrich_log_with_alignments_in_place(copy, aligned_traces, transition_names, options);
  return copy;
}

}  // namespace minera
