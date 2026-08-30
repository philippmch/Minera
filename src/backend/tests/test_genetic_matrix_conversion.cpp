#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <string>

#include "minera/conformance.hpp"
#include "minera/genetic_matrix_conversion.hpp"
#include "minera/genetic_miner.hpp"
#include "minera/json_export.hpp"

namespace {

minera::GeneticCausalMatrix sequence_matrix() {
  return {{"A", "B"}, {{}, {{0}}}, {{{1}}, {}}};
}

void sequence_uses_one_complete_causal_place() {
  const auto result =
      minera::convert_genetic_matrix_to_petri_net(sequence_matrix());
  assert(result.variant ==
         minera::GeneticMatrixConversionVariant::kToPetriNet);
  assert(result.net.activities() ==
         std::vector<std::string>({"A", "B"}));
  assert(result.net.place_count() == 3);
  assert(result.net.transitions().size() == 2);
  assert(result.net.source() == 0 && result.net.sink() == 1);
  assert(result.net.transitions()[0].inputs ==
         std::vector<std::size_t>({0}));
  assert(result.net.transitions()[0].outputs ==
         std::vector<std::size_t>({2}));
  assert(result.net.transitions()[1].inputs ==
         std::vector<std::size_t>({2}));
  assert(result.net.transitions()[1].outputs ==
         std::vector<std::size_t>({1}));
  assert(result.markings.initial == minera::Marking({1, 0, 0}));
  assert(result.markings.final == minera::Marking({0, 1, 0}));
}

void grouped_split_and_join_preserve_parallel_tokens() {
  minera::GeneticCausalMatrix matrix{
      {"A", "B", "C", "D"},
      {{}, {{0}}, {{0}}, {{1}, {2}}},
      {{{1}, {2}}, {{3}}, {{3}}, {}}};
  const auto result = minera::convert_genetic_matrix(matrix);
  assert(result.net.place_count() == 6);
  assert(result.net.transitions().size() == 4);
  assert(result.net.transitions()[0].outputs ==
         std::vector<std::size_t>({2, 3}));
  assert(result.net.transitions()[1].inputs ==
         std::vector<std::size_t>({2}));
  assert(result.net.transitions()[2].inputs ==
         std::vector<std::size_t>({3}));
  assert(result.net.transitions()[1].outputs ==
         std::vector<std::size_t>({4}));
  assert(result.net.transitions()[2].outputs ==
         std::vector<std::size_t>({5}));
  assert(result.net.transitions()[3].inputs ==
         std::vector<std::size_t>({4, 5}));

  minera::EventLog log;
  log.traces.push_back(
      {"c1", {minera::Event{"c1", "A"}, minera::Event{"c1", "B"},
              minera::Event{"c1", "C"}, minera::Event{"c1", "D"}}, {}});
  log.traces.push_back(
      {"c2", {minera::Event{"c2", "A"}, minera::Event{"c2", "C"},
              minera::Event{"c2", "B"}, minera::Event{"c2", "D"}}, {}});
  const auto replay = minera::check_conformance(log, result.net);
  assert(replay.fitting_traces == 2);
}

void inconsistent_groups_receive_silent_routing() {
  minera::GeneticCausalMatrix matrix{
      {"A", "B", "C"},
      {{}, {{0}}, {{1}}},
      {{{1, 2}}, {{2}}, {}}};
  const auto result = minera::convert_genetic_matrix(matrix);
  assert(std::any_of(result.net.transitions().begin(),
                     result.net.transitions().end(),
                     [](const auto& transition) {
                       return transition.silent();
                     }));
  assert(result.net.place_count() > 3);
}

void normalization_and_transport_are_deterministic() {
  auto matrix = sequence_matrix();
  matrix.input_map[1] = {{0, 0}, {0}};
  matrix.output_map[0] = {{1}, {1, 1}};
  const auto first = minera::convert_genetic_matrix(matrix);
  const auto second = minera::convert_genetic_matrix(matrix);
  assert(first.matrix.input_map[1] ==
         std::vector<minera::GeneticMatrixPartition>({{0}}));
  assert(first.matrix.output_map[0] ==
         std::vector<minera::GeneticMatrixPartition>({{1}}));
  const auto first_json = minera::genetic_matrix_conversion_json(first);
  const auto second_json = minera::genetic_matrix_conversion_json(second);
  assert(first_json == second_json);
  assert(first_json.find("\"family\":\"GENETIC_MATRIX_CONVERSION\"") !=
         std::string::npos);
  assert(first_json.find("\"variant\":\"TO_PETRI_NET\"") !=
         std::string::npos);
  assert(first_json.find("\"input_map\":[[],[[0]]]") !=
         std::string::npos);
}

void mined_matrix_reconstructs_the_returned_net() {
  minera::EventLog log;
  log.traces.push_back(
      {"case", {minera::Event{"case", "A"},
                minera::Event{"case", "B"}}, {}});
  minera::GeneticMinerOptions options;
  options.population_size = 4;
  options.generations = 1;
  options.elitism_min_sample = 1;
  options.max_states = 16;
  const auto mined = minera::mine_genetic(log, options);
  const auto converted =
      minera::convert_genetic_matrix_to_petri_net(mined.matrix);
  assert(converted.net.activities() == mined.net.activities());
  assert(converted.net.place_count() == mined.net.place_count());
  assert(converted.net.transitions().size() == mined.net.transitions().size());
  assert(converted.markings.initial == mined.markings.initial);
  assert(converted.markings.final == mined.markings.final);
  const auto json = minera::genetic_miner_json(mined);
  assert(json.find("\"matrix\":{\"activities\":[\"A\",\"B\"]") !=
         std::string::npos);
  assert(json.find("\"input_map\":") != std::string::npos);
  assert(json.find("\"output_map\":") != std::string::npos);
}

void invalid_models_and_variants_are_rejected() {
  for (const auto& matrix : {
           minera::GeneticCausalMatrix{{"A"}, {}, {{}}},
           minera::GeneticCausalMatrix{{"A"}, {{{}}}, {{}}},
           minera::GeneticCausalMatrix{{"A"}, {{{1}}}, {{}}},
       }) {
    bool threw = false;
    try {
      (void)minera::convert_genetic_matrix(matrix);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  assert(minera::parse_genetic_matrix_conversion_variant("to_petri_net") ==
         minera::GeneticMatrixConversionVariant::kToPetriNet);
  bool variant_threw = false;
  try {
    (void)minera::parse_genetic_matrix_conversion_variant("unknown");
  } catch (const std::invalid_argument&) {
    variant_threw = true;
  }
  assert(variant_threw);
}

}  // namespace

int main() {
  sequence_uses_one_complete_causal_place();
  grouped_split_and_join_preserve_parallel_tokens();
  inconsistent_groups_receive_silent_routing();
  normalization_and_transport_are_deterministic();
  mined_matrix_reconstructs_the_returned_net();
  invalid_models_and_variants_are_rejected();
}
