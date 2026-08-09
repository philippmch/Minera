#pragma once

#include <string>
#include <string_view>

#include "minera/dfg_variants.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/petri_net.hpp"

namespace minera {

// The two classic DFG-to-workflow-net constructions.  The first gives each
// activity its own place and makes each directly-follows edge a visible
// transition.  The second gives each activity one visible transition and
// joins activities with silent edge transitions, so no activity label is
// duplicated on the visible transitions.
enum class DfgPetriConversionVariant {
  kActivityDefinesPlace,
  kInvisiblesNoDuplicates,

  ACTIVITY_DEFINES_PLACE = kActivityDefinesPlace,
  INVISIBLES_NO_DUPLICATES = kInvisiblesNoDuplicates,
};

struct DfgPetriConversionOptions {
  DfgPetriConversionVariant variant =
      DfgPetriConversionVariant::kActivityDefinesPlace;
  std::string artificial_start_activity = "@@start@@";
  std::string artificial_end_activity = "@@end@@";
};

[[nodiscard]] PetriNet convert_dfg_to_petri_net(
    const DfgCleanAnalysis& dfg,
    const DfgPetriConversionOptions& options = {});

[[nodiscard]] PetriNet convert_dfg_to_petri_net(
    const DirectlyFollowsGraph& graph,
    const DfgPetriConversionOptions& options = {});

[[nodiscard]] inline PetriNet to_petri_net(
    const DfgCleanAnalysis& dfg,
    const DfgPetriConversionOptions& options = {}) {
  return convert_dfg_to_petri_net(dfg, options);
}

[[nodiscard]] inline PetriNet to_petri_net(
    const DirectlyFollowsGraph& graph,
    const DfgPetriConversionOptions& options = {}) {
  return convert_dfg_to_petri_net(graph, options);
}

// Standalone transport for callers that do not have an analysis envelope.
// The payload contains the selected variant, activity labels, source/sink
// place ids, and every transition's optional activity plus input/output arcs.
[[nodiscard]] std::string serialize_dfg_petri_net_json(
    const PetriNet& net, DfgPetriConversionVariant variant);

[[nodiscard]] inline std::string dfg_petri_net_json(
    const PetriNet& net, DfgPetriConversionVariant variant) {
  return serialize_dfg_petri_net_json(net, variant);
}

[[nodiscard]] DfgPetriConversionVariant parse_dfg_petri_conversion_variant(
    std::string_view name);

}  // namespace minera
