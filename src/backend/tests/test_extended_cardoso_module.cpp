#include "minera/evaluation.hpp"
#include "minera/pnml.hpp"
#include "minera/quality_workbench_json.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace minera;

namespace {
// These are the returned Python integer values, not expectations calculated
// from the native formula. Direct and parent routes agree on all 264 records.
// Retained capture: build/simplicity-structure-public-6f1b09a445884e7389aa1a19512c5caa/public-returned-values.json
// Capture SHA-256: 209213e6191f8985e3d5dffa0c53f72c6da55e313c081d305ff9d6ea6c1e474f
// Fixtures 0..255 enumerate the eight incidence bits on two places/transitions.
constexpr std::array<unsigned, 264> public_cardoso_values = {
    0, 1, 1, 1, 1, 2, 2, 2, 1, 2, 2, 2, 1, 2, 2, 2,
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    0, 1, 1, 1, 1, 2, 2, 2, 1, 2, 2, 2, 1, 2, 2, 2,
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    0, 1, 1, 1, 1, 2, 2, 2, 1, 2, 2, 2, 1, 2, 2, 2,
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    0, 1, 1, 1, 1, 2, 2, 2, 1, 2, 2, 2, 1, 2, 2, 2,
    0, 0, 0, 1, 1, 1, 1, 2,
};

PetriNodeRef place(std::size_t id) { return {PetriNodeKind::kPlace, id}; }
PetriNodeRef transition(std::size_t id) {
  return {PetriNodeKind::kTransition, id};
}

PetriNetPresentation empty_graph(std::size_t places, std::size_t transitions) {
  PetriNetPresentation net;
  for (std::size_t i = 0; i < places; ++i)
    net.places.push_back({i, "p" + std::to_string(i), {}});
  for (std::size_t i = 0; i < transitions; ++i)
    net.transitions.push_back({i, "t" + std::to_string(i),
        i == 0 ? std::optional<std::string>{"A"} : std::nullopt, {}});
  return net;
}

PetriNetPresentation fixture(std::size_t index) {
  if (index < 256) {
    auto net = empty_graph(2, 2);
    for (unsigned bit = 0; bit < 8; ++bit) {
      if (!(index & (std::size_t{1} << bit))) continue;
      const auto source = (bit % 4) / 2;
      const auto target = bit % 2;
      net.arcs.push_back(bit < 4
          ? PetriPresentedArc{place(source), transition(target), 1, {}}
          : PetriPresentedArc{transition(source), place(target), 1, {}});
    }
    return net;
  }
  if (index == 256) return empty_graph(0, 0);
  if (index == 257) return empty_graph(3, 0);
  if (index == 258) return empty_graph(0, 3);
  auto net = empty_graph(index < 261 ? 2 : index == 262 ? 1 : 3,
                         index < 261 ? 1 : 2);
  if (index == 259)
    net.arcs = {{place(0), transition(0), 3, {}},
                {transition(0), place(1), 2, {}}};
  if (index == 260)
    net.arcs = {{place(0), transition(0), 1, {}},
                {place(0), transition(0), 2, {}},
                {transition(0), place(1), 1, {}}};
  if (index >= 261) {
    net.arcs = {{place(0), transition(0), 1, {}},
                {place(0), transition(1), 1, {}}};
    if (index == 261)
      net.arcs.insert(net.arcs.end(), {
          {transition(0), place(1), 1, {}},
          {transition(0), place(2), 2, {}},
          {transition(1), place(1), 3, {}},
          {transition(1), place(2), 1, {}}});
    if (index == 263)
      net.arcs.insert(net.arcs.end(), {
          {transition(0), place(1), 1, {}},
          {transition(1), place(2), 1, {}}});
  }
  return net;
}

void expect(const SimplicityResult& result, unsigned expected) {
  assert(result.computed && !result.exhausted && result.error.empty());
  assert(result.variant == SimplicityVariant::EXTENDED_CARDOSO);
  assert(result.value == expected);
  assert(result.simplicity == expected && result.complexity == expected);
}

void check_graph(const PetriNetPresentation& net, unsigned expected,
                 const SimplicityOptions& options = {}) {
  const auto before = serialize_quality_workbench_presentation_json(net);
  const auto options_before = serialize_quality_workbench_options_json(options);
  const auto result = evaluate_simplicity(
      net, SimplicityVariant::EXTENDED_CARDOSO, options);
  expect(result, expected);
  assert(result.vertices == net.places.size() && result.edges == net.arcs.size());
  assert(serialize_quality_workbench_presentation_json(net) == before);
  assert(serialize_quality_workbench_options_json(options) == options_before);
}

void retained_public_fixture_matrix() {
  for (std::size_t i = 0; i < public_cardoso_values.size(); ++i) {
    auto net = fixture(i);
    check_graph(net, public_cardoso_values[i]);
    // Identity order and arc order are not graph identity. A permutation must
    // not merge choices or make iteration order affect the returned metric.
    std::reverse(net.places.begin(), net.places.end());
    std::reverse(net.transitions.begin(), net.transitions.end());
    std::reverse(net.arcs.begin(), net.arcs.end());
    check_graph(net, public_cardoso_values[i]);
  }
}

void identity_metadata_and_ignored_options() {
  auto net = fixture(261);  // Equal two-place successor sets count once.
  const auto maximum = std::numeric_limits<std::size_t>::max();
  for (auto& p : net.places) {
    p.id = maximum - p.id;
    p.name = std::string("same\0\xff", 6);
    p.properties["ignored"] = HeuristicStructuredValue{true};
  }
  for (auto& t : net.transitions) {
    t.id = maximum - t.id;  // Same numeric IDs in each independent kind scope.
    t.name = std::string("same\0\xff", 6);
    t.label = std::string("\xed\xa0\x80", 3);
    t.properties["ignored"] = HeuristicStructuredValue{std::int64_t{-7}};
  }
  for (auto& arc : net.arcs) {
    arc.source.id = maximum - arc.source.id;
    arc.target.id = maximum - arc.target.id;
    arc.weight = TokenInteger(std::string_view{
        "12345678901234567890123456789012345678901234567890"});
    arc.negative_weight = true;
    arc.properties["ignored"] = HeuristicStructuredValue{std::string("\0\xff", 2)};
  }
  net.name = std::string("net\0\xff", 5);
  net.properties["ignored"] = HeuristicStructuredValue{false};
  net.initial_marking = {{maximum, -1}, {maximum, 0}};
  net.final_marking = {{maximum - 1, 42}};
  check_graph(net, 1);
  // Separate parallel arc objects and arbitrary weights are intentionally not
  // multiplicities of a successor-set choice, on both sides of transitions.
  const auto duplicates = net.arcs;
  net.arcs.insert(net.arcs.end(), duplicates.begin(), duplicates.end());
  check_graph(net, 1);
  net.arcs.front().weight = 0;
  check_graph(net, 1);

  SimplicityOptions options;
  options.initial_marking = {-9, 2};
  options.final_marking = {-1};
  options.max_states = 0;
  options.presentation_initial_marking =
      std::vector<PetriSparseMarkingEntry>{{maximum, -9}};
  for (double k : {0.0, -0.0, -7.0,
       std::numeric_limits<double>::max(),
       std::numeric_limits<double>::infinity(),
       -std::numeric_limits<double>::infinity(),
       std::numeric_limits<double>::quiet_NaN()}) {
    options.arc_degree_k = k;
    check_graph(net, 1, options);
  }
  // Empty successor sets are choices as well. Removing all t1 outputs makes
  // it distinct from t0, even though all names and labels collide.
  net.arcs.erase(std::remove_if(net.arcs.begin(), net.arcs.end(),
      [&](const auto& arc) {
        return arc.source.kind == PetriNodeKind::kTransition &&
               arc.source.id == maximum - 1;
      }), net.arcs.end());
  check_graph(net, 2);
}

void native_invalid_identity_diagnostics() {
  const auto rejected = [](const PetriNetPresentation& net,
                           std::string_view expected_error) {
    const auto before = serialize_quality_workbench_presentation_json(net);
    const auto result = evaluate_simplicity(
        net, SimplicityVariant::EXTENDED_CARDOSO);
    assert(!result.computed && !result.exhausted);
    assert(result.variant == SimplicityVariant::EXTENDED_CARDOSO);
    assert(result.error == expected_error);
    assert(serialize_quality_workbench_presentation_json(net) == before);
  };
  auto net = fixture(263);
  net.places.push_back(net.places.front());
  rejected(net, "duplicate presented place identity");
  net = fixture(263);
  net.transitions.push_back(net.transitions.front());
  rejected(net, "duplicate presented transition identity");
  net = fixture(263);
  net.arcs.front().target.id = 999;
  rejected(net, "presented arc refers to an absent endpoint");
  net = fixture(263);
  net.arcs.front().target = place(0);
  rejected(net, "presented arc endpoints must be a place and a transition");
  net = fixture(263);
  net.arcs.front().source.kind = static_cast<PetriNodeKind>(-1);
  rejected(net, "presented arc endpoints must be a place and a transition");
}

void native_executable_and_pnml_entrypoints() {
  PetriNet net;
  net.set_activities({"A", "B"});
  net.add_place(); net.add_place(); net.add_place();
  net.add_transition(0, {0, 0}, {1, 1, 2});
  net.add_transition(1, {0}, {2, 1});
  expect(evaluate_simplicity(net, SimplicityVariant::EXTENDED_CARDOSO), 1);
  net.add_transition(std::nullopt, {0}, {});
  expect(evaluate_simplicity(net, SimplicityVariant::EXTENDED_CARDOSO), 2);

  const auto document = parse_pnml_document(R"(<pnml><net id="cardoso">
    <place id="p0"/><place id="p1"/>
    <transition id="t0"/><transition id="t1"/>
    <arc id="a" source="p0" target="t0"/>
    <arc id="b" source="p0" target="t0"><inscription><text>9</text></inscription></arc>
    <arc id="c" source="p0" target="t1"/>
    <arc id="d" source="t0" target="p1"/>
    <arc id="e" source="t1" target="p1"/>
  </net></pnml>)");
  const auto before = serialize_quality_workbench_document_json(document);
  expect(evaluate_simplicity(document, SimplicityVariant::EXTENDED_CARDOSO), 1);
  assert(serialize_quality_workbench_document_json(document) == before);
  const auto roundtrip = parse_pnml_document(to_pnml(document));
  expect(evaluate_simplicity(roundtrip, SimplicityVariant::EXTENDED_CARDOSO), 1);
  auto stale = document;
  stale.net.add_arc(0, 0);
  const auto result = evaluate_simplicity(stale, SimplicityVariant::EXTENDED_CARDOSO);
  assert(!result.computed && !result.error.empty());
}
}  // namespace

int main() {
  retained_public_fixture_matrix();
  identity_metadata_and_ignored_options();
  native_invalid_identity_diagnostics();
  native_executable_and_pnml_entrypoints();
  std::cout << "Extended Cardoso: 264 retained public fixtures, their permutations, "
               "identity/arc/metadata/options invariants, native diagnostics and "
               "executable/PNML entrypoints passed.\n";
}
