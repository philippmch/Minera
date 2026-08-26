#include "minera/dfg_conversion.hpp"

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

void empty_boundaries_are_inferred() {
  minera::DfgCleanAnalysis dfg;
  dfg.graph[{"A", "B"}] = 1;
  const auto net = minera::convert_dfg_to_petri_net(dfg);
  assert(net.activities().size() == 2);
  assert(net.transitions().size() == 3);
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
  empty_boundaries_are_inferred();
  graph_overload_and_json_are_deterministic();
  invalid_options_are_rejected();
}
