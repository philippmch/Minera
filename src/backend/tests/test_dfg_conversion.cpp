#include "minera/dfg_conversion.hpp"
#include "minera/pnml.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

namespace {

minera::DfgCleanAnalysis sequence_dfg() {
  minera::DfgCleanAnalysis dfg;
  dfg.graph[{"A", "B"}] = 3;
  dfg.start_activities["A"] = 3;
  dfg.end_activities["B"] = 3;
  return dfg;
}

void activity_places_variant_uses_edge_transitions() {
  const auto net = minera::convert_dfg_to_petri_net(sequence_dfg());
  assert(net.activities() == std::vector<std::string>({"A", "B"}));
  assert(net.place_count() == 4);  // source, sink, A, B
  assert(net.source() == 0 && net.sink() == 1);
  assert(net.transitions().size() == 3);
  assert(net.transitions()[0].activity && *net.transitions()[0].activity == 0);
  assert(net.transitions()[1].silent());
  assert(net.transitions()[2].activity && *net.transitions()[2].activity == 1);
  assert(net.transitions()[0].inputs == std::vector<std::size_t>({0}));
  assert(net.transitions()[0].outputs == std::vector<std::size_t>({2}));
  assert(net.transitions()[1].inputs == std::vector<std::size_t>({3}));
  assert(net.transitions()[1].outputs == std::vector<std::size_t>({1}));
  assert(net.transitions()[2].inputs == std::vector<std::size_t>({2}));
  assert(net.transitions()[2].outputs == std::vector<std::size_t>({3}));
}

void invisible_variant_has_one_visible_transition_per_activity() {
  minera::DfgPetriConversionOptions options;
  options.variant = minera::DfgPetriConversionVariant::kInvisiblesNoDuplicates;
  const auto net = minera::convert_dfg_to_petri_net(sequence_dfg(), options);
  assert(net.activities() == std::vector<std::string>({"A", "B"}));
  assert(net.place_count() == 8);  // left/right places for A, B, and boundaries
  assert(net.source() == 4 && net.sink() == 7);
  assert(net.transitions().size() == 7);
  std::size_t visible = 0;
  std::size_t silent = 0;
  for (const auto& transition : net.transitions()) {
    if (transition.activity) ++visible;
    else ++silent;
  }
  assert(visible == 2 && silent == 5);
}

// pm4py 2.7.22 convert_to_petri_net(dfg, sa, ea) passes both maps, so an
// empty map is not inferred (captured black box 2026-11-05).
void empty_boundaries_are_explicit() {
  minera::DfgCleanAnalysis dfg;
  dfg.graph[{"A", "B"}] = 1;
  dfg.graph[{"B", "B"}] = 2;
  const auto net = minera::convert_dfg_to_petri_net(dfg);
  assert(net.place_count() == 4);  // source, sink, A, B
  assert(net.transitions().size() == 2);  // the two edges, no boundaries
  for (const auto& transition : net.transitions()) {
    assert(transition.activity && net.activities()[*transition.activity] == "B");
    assert(transition.inputs.front() != net.source());
    assert(transition.outputs.front() != net.sink());
  }
  minera::DfgPetriConversionOptions options;
  options.variant = minera::DfgPetriConversionVariant::kInvisiblesNoDuplicates;
  for (const bool empty_start : {true, false}) {
    auto partial = dfg;
    if (empty_start) partial.end_activities["B"] = 1;
    else partial.start_activities["A"] = 1;
    bool raised = false;
    try {
      (void)minera::convert_dfg_to_petri_net(partial, options);
    } catch (const std::invalid_argument&) {
      raised = true;
    }
    assert(raised);
  }
}

// Activity-defines-place keeps only edge activities: pm4py drops a boundary
// activity without edges (the DFG of the log [<a>] is a bare source and
// sink). Zero frequencies still count as keys. Invisibles-no-duplicates
// keeps boundary activities.
void boundary_activities_outside_edges() {
  minera::DfgCleanAnalysis single;
  single.start_activities["a"] = 1;
  single.end_activities["a"] = 1;
  const auto bare = minera::convert_dfg_to_petri_net(single);
  assert(bare.place_count() == 2 && bare.transitions().empty());
  // The bare net survives a PNML round trip, as pm4py's write_pnml and
  // read_pnml keep it (model exchange of the DFG of [<a>] converts it).
  const auto document = minera::parse_pnml_document(minera::to_pnml(bare));
  assert(document.net.place_count() == 2 && document.net.transitions().empty());
  assert(document.initial_marking[document.net.source()] == 1);
  assert(document.final_marking[document.net.sink()] == 1);
  assert(document.net.source() != document.net.sink());

  minera::DfgCleanAnalysis dfg;
  dfg.graph[{"b", "c"}] = 0;
  dfg.start_activities["a"] = 1;
  dfg.start_activities["b"] = 0;
  dfg.end_activities["a"] = 1;
  dfg.end_activities["c"] = 1;
  const auto net = minera::convert_dfg_to_petri_net(dfg);
  // places b, c, source, sink; transitions start b, end c, edge b->c
  assert(net.place_count() == 4 && net.transitions().size() == 3);
  minera::DfgPetriConversionOptions options;
  options.variant = minera::DfgPetriConversionVariant::kInvisiblesNoDuplicates;
  const auto invisible = minera::convert_dfg_to_petri_net(dfg, options);
  // a, b, c and both artificial activities: 10 places, 5 own transitions,
  // 2 start, 1 edge and 2 end silent transitions.
  assert(invisible.place_count() == 10 && invisible.transitions().size() == 10);
}

void graph_overload_and_json_are_deterministic() {
  minera::EventLog log;
  log.traces.push_back({"case", {minera::Event{"case", "A"},
                                  minera::Event{"case", "B"}}, {}});
  const auto graph = minera::discover_directly_follows_graph(log);
  minera::DfgPetriConversionOptions options;
  options.variant = minera::DfgPetriConversionVariant::kInvisiblesNoDuplicates;
  const auto net = minera::convert_dfg_to_petri_net(graph, options);
  const auto json = minera::serialize_dfg_petri_net_json(
      net, options.variant);
  assert(json.find("\"family\":\"DFG_CONVERSION\"") != std::string::npos);
  assert(json.find("\"variant\":\"INVISIBLES_NO_DUPLICATES\"") !=
         std::string::npos);
  assert(json.find("\"activities\":[\"A\",\"B\"]") !=
         std::string::npos);
}

void invalid_options_are_rejected() {
  auto options = minera::DfgPetriConversionOptions{};
  options.variant = minera::DfgPetriConversionVariant::kInvisiblesNoDuplicates;
  options.artificial_start_activity = "A";
  bool collision = false;
  try {
    (void)minera::convert_dfg_to_petri_net(sequence_dfg(), options);
  } catch (const std::invalid_argument&) {
    collision = true;
  }
  assert(collision);
  bool unknown = false;
  try {
    (void)minera::parse_dfg_petri_conversion_variant("unknown");
  } catch (const std::invalid_argument&) {
    unknown = true;
  }
  assert(unknown);
}

}  // namespace

int main() {
  activity_places_variant_uses_edge_transitions();
  invisible_variant_has_one_visible_transition_per_activity();
  empty_boundaries_are_explicit();
  boundary_activities_outside_edges();
  graph_overload_and_json_are_deterministic();
  invalid_options_are_rejected();
}
