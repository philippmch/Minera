#include "minera/ilp_miner.hpp"
#include "minera/ilp_reference.hpp"
#include "minera/json_export.hpp"

#include <algorithm>
#include <cassert>
#include <map>
#include <tuple>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace minera;

// mine_ilp is an adapter over the reference classic ILP port
// (ilp_reference.hpp, covered by the ILP discovery parity tests). These tests
// check that the adapter lays the reference net out faithfully and keeps its
// own validation, limits and native extensions.

namespace {
using Rows = std::vector<std::vector<std::string>>;

EventLog log_of(const Rows& rows) {
  EventLog log;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    Trace trace;
    trace.case_id = std::to_string(i);
    for (std::size_t j = 0; j < rows[i].size(); ++j)
      trace.events.push_back(Event{trace.case_id, rows[i][j], {}, {}, j, {}, {}});
    log.traces.push_back(std::move(trace));
  }
  return log;
}

template <class Fn>
void rejects(Fn make) {
  bool caught = false;
  try {
    make();
  } catch (const std::invalid_argument&) {
    caught = true;
  }
  assert(caught);
}

// Every arc as (transition label or "τ", place index, direction).
using Arc = std::tuple<std::string, std::size_t, bool>;
std::multiset<Arc> arcs_of(const PetriNet& net) {
  std::multiset<Arc> arcs;
  for (const auto& transition : net.transitions()) {
    const auto label = transition.activity ? net.activities().at(*transition.activity) : "τ";
    for (const auto place : transition.inputs) arcs.emplace(label, place, false);
    for (const auto place : transition.outputs) arcs.emplace(label, place, true);
  }
  return arcs;
}

std::multiset<Arc> arcs_of(const ReferenceIlpNet& net) {
  std::map<std::string, std::string> labels;
  for (const auto& transition : net.transitions)
    labels[transition.name] = transition.label.value_or("τ");
  std::multiset<Arc> arcs;
  for (std::size_t p = 0; p < net.places.size(); ++p) {
    for (const auto& name : net.places[p].inputs) arcs.emplace(labels.at(name), p, true);
    for (const auto& name : net.places[p].outputs) arcs.emplace(labels.at(name), p, false);
  }
  return arcs;
}

void matches_reference(const Rows& rows, const ILPMinerOptions& options = {}) {
  const auto result = mine_ilp(log_of(rows), options);
  ReferenceIlpOptions reference;
  reference.alpha = options.alpha;
  reference.artificial_start_activity = options.artificial_start_activity;
  reference.artificial_end_activity = options.artificial_end_activity;
  if (!options.causal_relation.empty()) reference.causal_relation = options.causal_relation;
  const auto expected = reference_ilp_net(rows, reference);
  assert(result.diagnostics.computed);
  assert(result.net.place_count() == expected.places.size());
  assert(result.net.transitions().size() == expected.transitions.size());
  assert(arcs_of(result.net) == arcs_of(expected));
  for (std::size_t p = 0; p < expected.places.size(); ++p) {
    assert((expected.places[p].name == "source") == (p == result.net.source()));
    assert((expected.places[p].name == "sink") == (p == result.net.sink()));
  }
  assert(result.markings.initial.size() == result.net.place_count());
  assert(result.markings.initial[result.net.source()] == 1);
  assert(result.markings.final[result.net.sink()] == 1);
  assert(result.diagnostics.lp_solved == reference_ilp_causal_pairs(rows, reference).size());
}
}  // namespace

int main() {
  matches_reference({{"a", "b", "c"}, {"a", "b", "c"}});
  matches_reference({{"a", "b"}, {"a", "c"}});
  matches_reference({{"a", "b", "c", "d"}, {"a", "c", "b", "d"}});
  matches_reference({{"a", "a", "b"}, {"a", "b"}});
  matches_reference({{"a", "a"}, {"a"}});
  matches_reference({{}, {"a"}});
  matches_reference({{}});

  ILPMinerOptions alpha;
  alpha.alpha = 0.5;
  matches_reference({{"a", "b"}, {"a", "b"}, {"a", "b"}, {"a", "c"}}, alpha);

  ILPMinerOptions boundaries;
  boundaries.artificial_start_activity = "BEGIN";
  boundaries.artificial_end_activity = "FINISH";
  matches_reference({{"a", "b"}, {"a", "b"}}, boundaries);
  const auto bounded = mine_ilp(log_of({{"a", "b"}}), boundaries);
  assert(bounded.net.activities() == std::vector<std::string>({"a", "b"}));

  ILPMinerOptions causal;
  causal.causal_relation = {{"▶", "a"}, {"a", "c"}, {"c", "■"}};
  matches_reference({{"a", "c"}}, causal);

  // The thirteen-activity sample size works; activities are the sorted
  // visible labels.
  Rows wide(1);
  for (int i = 0; i < 13; ++i) wide[0].push_back("activity_" + std::to_string(100 + i));
  matches_reference(wide);
  assert(mine_ilp(log_of(wide)).net.activities().front() == "activity_100");

  // Deterministic across runs.
  const auto first = mine_ilp(log_of({{"a", "b"}, {"b", "a"}}));
  const auto second = mine_ilp(log_of({{"a", "b"}, {"b", "a"}}));
  assert(arcs_of(first.net) == arcs_of(second.net));

  // max_nodes caps the number of causal pairs (LP solves).
  ILPMinerOptions limited;
  limited.max_nodes = 1;
  rejects([&] { (void)mine_ilp(log_of({{"a", "b", "c"}}), limited); });

  // Native noise extension: variants below half of the dominant one go.
  ILPMinerOptions noise;
  noise.noise_threshold = 0.5;
  const auto denoised = mine_ilp(log_of({{"a", "b"}, {"a", "b"}, {"a", "b"}, {"a", "c"}}), noise);
  assert(std::find(denoised.net.activities().begin(), denoised.net.activities().end(), "c") ==
         denoised.net.activities().end());

  // A missing or empty activity-key value falls back to the event activity.
  EventLog keyed = log_of({{"a", "b"}});
  keyed.traces[0].events[0].attributes.push_back({"phase", "x", AttributeKind::kString});
  keyed.traces[0].events[1].attributes.push_back({"phase", "", AttributeKind::kString});
  ILPMinerOptions phase;
  phase.activity_key = "phase";
  assert(mine_ilp(keyed, phase).net.activities() == std::vector<std::string>({"b", "x"}));

  // Validation.
  ILPMinerOptions invalid;
  invalid.noise_threshold = 2.0;
  rejects([&] { (void)mine_ilp(log_of({{"a"}}), invalid); });
  invalid = {};
  invalid.alpha = -0.1;
  rejects([&] { (void)mine_ilp(log_of({{"a"}}), invalid); });
  invalid = {};
  invalid.artificial_start_activity.clear();
  rejects([&] { (void)mine_ilp(log_of({{"a"}}), invalid); });
  invalid = {};
  invalid.artificial_end_activity = invalid.artificial_start_activity;
  rejects([&] { (void)mine_ilp(log_of({{"a"}}), invalid); });
  invalid = {};
  invalid.artificial_start_activity = "a";
  rejects([&] { (void)mine_ilp(log_of({{"a"}}), invalid); });
  invalid = {};
  invalid.max_nodes = 0;
  rejects([&] { (void)mine_ilp(log_of({{"a"}}), invalid); });
  rejects([&] { (void)mine_ilp(EventLog{}); });
  // The reference rejects a causal relation without the boundary activities.
  ILPMinerOptions partial;
  partial.causal_relation = {{"a", "b"}};
  rejects([&] { (void)mine_ilp(log_of({{"a", "b"}}), partial); });

  assert(!ilp_miner_json(first).empty());
}
