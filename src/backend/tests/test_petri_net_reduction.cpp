#include "minera/petri_net_reduction.hpp"
#include "minera/json_export.hpp"

#include <cassert>
#include <cstdint>
#include <optional>
#include <vector>

using namespace minera;

namespace {

PetriNet silent_chain() {
  PetriNet net;
  net.set_activities({"a"});
  const auto source = net.add_place();
  const auto p1 = net.add_place();
  const auto p2 = net.add_place();
  const auto p3 = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(std::nullopt, {source}, {p1});
  net.add_transition(std::nullopt, {p1}, {p2});
  net.add_transition(std::nullopt, {p2}, {p3});
  net.add_transition(0, {p3}, {sink});
  return net;
}

bool same_net(const PetriNet& lhs, const PetriNet& rhs) {
  if (lhs.place_count() != rhs.place_count() || lhs.source() != rhs.source() ||
      lhs.sink() != rhs.sink() || lhs.activities() != rhs.activities() ||
      lhs.transitions().size() != rhs.transitions().size()) {
    return false;
  }
  for (std::size_t i = 0; i < lhs.transitions().size(); ++i) {
    const auto& a = lhs.transitions()[i];
    const auto& b = rhs.transitions()[i];
    if (a.activity != b.activity || a.inputs != b.inputs ||
        a.outputs != b.outputs || a.reset_inputs != b.reset_inputs ||
        a.inhibitor_inputs != b.inhibitor_inputs) {
      return false;
    }
  }
  return true;
}

void join_bridge_preservation_is_opt_in_and_requires_external_producer() {
  for (const bool external_producer : {false, true}) {
    PetriNet net;
    net.set_activities({"a", "b", "c", "d", "e"});
    const auto source = net.add_place();
    const auto bridge_input = net.add_place();
    const auto join = net.add_place();
    const auto sink = net.add_place();
    net.set_source(source);
    net.set_sink(sink);
    net.add_transition(0, {source}, {bridge_input});
    net.add_transition(1, {source}, {bridge_input});
    net.add_transition(std::nullopt, {bridge_input}, {join});
    net.add_transition(2, {join}, {sink});
    net.add_transition(3, {join}, {sink});
    if (external_producer) net.add_transition(4, {source}, {join});

    PetriNetReductionOptions defaults;
    assert(!defaults.preserve_join_bridges);
    defaults.initial_marking = {1, 0, 0, 0};
    defaults.final_marking = {0, 0, 0, 1};
    const auto contracted = reduce_petri_net(net, defaults);
    assert(contracted.changed);
    assert(contracted.net.place_count() == 3);
    assert(contracted.net.transitions().size() ==
           net.transitions().size() - 1);
    assert((contracted.old_to_new_place ==
            std::vector<std::size_t>{0, kRemovedPlace, 1, 2}));
    assert(contracted.net.transitions()[0].outputs ==
           std::vector<std::size_t>{1});
    assert(contracted.net.transitions()[1].outputs ==
           std::vector<std::size_t>{1});

    auto preserve = defaults;
    preserve.preserve_join_bridges = true;
    const auto guarded = reduce_petri_net(net, preserve);
    if (external_producer) {
      assert(!guarded.changed);
      assert(same_net(guarded.net, net));
      assert((guarded.old_to_new_place ==
              std::vector<std::size_t>{0, 1, 2, 3}));
      assert(guarded.markings.initial == preserve.initial_marking);
      assert(guarded.markings.final == preserve.final_marking);
    } else {
      // Several producers alone do not inhibit ordinary series-place fusion.
      assert(guarded.changed);
      assert(same_net(guarded.net, contracted.net));
      assert(guarded.old_to_new_place == contracted.old_to_new_place);
      assert(guarded.markings.initial == contracted.markings.initial);
      assert(guarded.markings.final == contracted.markings.final);
    }
  }
}

}  // namespace

int main() {
  join_bridge_preservation_is_opt_in_and_requires_external_producer();
  {
    const auto result = reduce_petri_net(silent_chain());
    assert(result.changed);
    assert(result.net.place_count() == 2);
    assert(result.net.transitions().size() == 1);
    assert(result.net.source() == 0 && result.net.sink() == 1);
    assert(result.net.transitions()[0].activity == std::optional<std::size_t>(0));
    assert(result.net.transitions()[0].inputs == std::vector<std::size_t>{0});
    assert(result.net.transitions()[0].outputs == std::vector<std::size_t>{1});
    assert((result.old_to_new_place ==
            std::vector<std::size_t>{0, kRemovedPlace, kRemovedPlace,
                                     kRemovedPlace, 1}));

    PetriNetReductionOptions options;
    options.initial_marking = {1, 0, 0, 0, 0};
    options.final_marking = {0, 0, 0, 0, 1};
    const auto with_markings = reduce_petri_net(silent_chain(), options);
    assert((with_markings.markings.initial ==
            std::vector<std::int64_t>{1, 0}));
    assert((with_markings.markings.final == std::vector<std::int64_t>{0, 1}));
  }

  {
    // Boundary places and marked places stay distinct, including through a
    // later fixed-point pass.
    PetriNet net;
    net.set_activities({"a"});
    const auto source = net.add_place();
    const auto entry = net.add_place();
    const auto marked = net.add_place();
    const auto exit = net.add_place();
    const auto sink = net.add_place();
    net.set_source(source);
    net.set_sink(sink);
    net.add_transition(std::nullopt, {source}, {entry});
    net.add_transition(std::nullopt, {entry}, {marked});
    net.add_transition(std::nullopt, {marked}, {exit});
    net.add_transition(0, {exit}, {sink});
    PetriNetReductionOptions options;
    options.initial_marking = {1, 0, 2, 0, 0};
    options.final_marking = {0, 0, 0, 0, 1};
    const auto result = reduce_petri_net(net, options);
    assert(result.changed);
    assert(result.net.place_count() == 3);
    assert(result.net.transitions().size() == 2);
    assert((result.old_to_new_place ==
            std::vector<std::size_t>{0, kRemovedPlace, 1, kRemovedPlace,
                                     2}));
    assert((result.markings.initial ==
            std::vector<std::int64_t>{1, 2, 0}));
    assert((result.markings.final == std::vector<std::int64_t>{0, 0, 1}));
  }

  {
    // A single-entry silent transition may fan out; only its private entry
    // place is removed and every former output arc is spliced onto its
    // producer.
    PetriNet net;
    net.set_activities({"a", "b", "c"});
    const auto source = net.add_place();
    const auto entry = net.add_place();
    const auto left = net.add_place();
    const auto right = net.add_place();
    const auto sink = net.add_place();
    net.set_source(source);
    net.set_sink(sink);
    net.add_transition(0, {source}, {entry});
    net.add_transition(std::nullopt, {entry}, {left, right});
    net.add_transition(1, {left}, {sink});
    net.add_transition(2, {right}, {sink});
    const auto result = reduce_petri_net(net);
    assert(result.changed);
    assert(result.net.transitions().size() == 3);
    assert((result.net.transitions()[0].inputs ==
            std::vector<std::size_t>{0}));
    assert(result.net.transitions()[0].outputs ==
           std::vector<std::size_t>({1, 2}));
    assert((result.net.transitions()[1].inputs ==
            std::vector<std::size_t>{1}));
    assert((result.net.transitions()[2].inputs ==
            std::vector<std::size_t>{2}));
  }

  {
    // The dual single-exit rule may merge a silent transition fed by several
    // places into one consumer while preserving all input multiplicities.
    PetriNet net;
    net.set_activities({"a", "b", "c"});
    const auto source = net.add_place();
    const auto left = net.add_place();
    const auto right = net.add_place();
    const auto exit = net.add_place();
    const auto sink = net.add_place();
    net.set_source(source);
    net.set_sink(sink);
    net.add_transition(0, {source}, {left});
    net.add_transition(1, {source}, {right});
    net.add_transition(std::nullopt, {left, right}, {exit});
    net.add_transition(2, {exit}, {sink});
    const auto result = reduce_petri_net(net);
    assert(result.changed);
    assert(result.net.transitions().size() == 3);
    assert(result.net.transitions()[2].inputs ==
           std::vector<std::size_t>({1, 2}));
    assert((result.net.transitions()[2].outputs ==
            std::vector<std::size_t>{3}));
  }

  {
    // Shared entry/exit places are not private silent chains and must remain
    // untouched; otherwise an exclusive choice could be changed into a
    // direct bypass.
    PetriNet net;
    net.set_activities({"a", "b", "c"});
    const auto source = net.add_place();
    const auto entry = net.add_place();
    const auto exit = net.add_place();
    const auto sink = net.add_place();
    net.set_source(source);
    net.set_sink(sink);
    net.add_transition(0, {source}, {entry});
    net.add_transition(std::nullopt, {entry}, {exit});
    net.add_transition(1, {entry}, {sink});
    net.add_transition(2, {source}, {exit});
    net.add_transition(1, {exit}, {sink});
    const auto result = reduce_petri_net(net);
    assert(!result.changed);
    assert(same_net(result.net, net));
  }

  {
    // Duplicate ordinary arcs survive a fusion without being collapsed.
    PetriNet net;
    const auto source = net.add_place();
    const auto entry = net.add_place();
    const auto exit = net.add_place();
    const auto sink = net.add_place();
    net.set_source(source);
    net.set_sink(sink);
    net.add_transition(std::nullopt, {entry}, {exit});
    net.add_transition(0, {source}, {entry, entry});
    net.add_transition(1, {exit, exit}, {sink});
    const auto result = reduce_petri_net(net);
    assert(result.changed);
    assert(result.net.transitions().size() == 2);
    assert((result.net.transitions()[0].inputs ==
            std::vector<std::size_t>{0}));
    assert((result.net.transitions()[0].outputs ==
            std::vector<std::size_t>{1}));
    assert((result.net.transitions()[1].inputs ==
            std::vector<std::size_t>{1, 1}));
    assert((result.net.transitions()[1].outputs ==
            std::vector<std::size_t>{2}));
  }

  {
    // Extended arcs are deliberately outside this conservative reduction.
    PetriNet net;
    const auto source = net.add_place();
    const auto entry = net.add_place();
    const auto exit = net.add_place();
    const auto sink = net.add_place();
    net.set_source(source);
    net.set_sink(sink);
    net.add_transition(std::nullopt, {entry}, {exit});
    const auto visible = net.add_transition(0, {source}, {sink});
    net.add_reset_arc(visible, entry);
    PetriNetReductionOptions options;
    options.initial_marking = {1, 0, 0, 0};
    const auto result = reduce_petri_net(net, options);
    assert(!result.changed);
    assert(same_net(result.net, net));
    assert((result.old_to_new_place ==
            std::vector<std::size_t>{0, 1, 2, 3}));
    assert(result.markings.initial == options.initial_marking);
  }

  {
    // An unrelated extended guard must survive while an ordinary silent chain
    // is reduced.  The guarded place is retained and remapped deterministically.
    PetriNet net;
    net.set_activities({"a"});
    const auto source = net.add_place();
    const auto entry = net.add_place();
    const auto exit = net.add_place();
    const auto sink = net.add_place();
    const auto guard = net.add_place();
    net.set_source(source);
    net.set_sink(sink);
    net.add_transition(0, {source}, {entry});
    net.add_transition(std::nullopt, {entry}, {exit});
    const auto visible = net.add_transition(0, {exit}, {sink});
    net.add_inhibitor_arc(visible, guard);
    PetriNetReductionOptions options;
    options.initial_marking = {1, 0, 0, 0, 0};
    options.final_marking = {0, 0, 0, 1, 0};
    const auto result = reduce_petri_net(net, options);
    assert(result.changed);
    assert(result.net.place_count() == 4);
    assert(result.net.transitions().size() == 2);
    assert((result.old_to_new_place ==
            std::vector<std::size_t>{0, kRemovedPlace, 1, 2, 3}));
    assert(result.net.transitions()[1].inhibitor_inputs ==
           std::vector<std::size_t>{3});
    assert((result.markings.initial == std::vector<std::int64_t>{1, 0, 0, 0}));
    assert((result.markings.final == std::vector<std::int64_t>{0, 0, 1, 0}));
  }

  {
    // Series-place fusion handles a private silent place with multiple
    // producers and a shared output place with multiple consumers.
    PetriNet net;
    net.set_activities({"a", "b", "c", "d"});
    const auto source = net.add_place();
    const auto private_place = net.add_place();
    const auto shared = net.add_place();
    const auto sink = net.add_place();
    net.set_source(source);
    net.set_sink(sink);
    net.add_transition(0, {source}, {private_place});
    net.add_transition(1, {source}, {private_place});
    net.add_transition(std::nullopt, {private_place}, {shared});
    net.add_transition(2, {shared}, {sink});
    net.add_transition(3, {shared}, {sink});
    const auto result = reduce_petri_net(net);
    assert(result.changed);
    assert(result.net.place_count() == 3);
    assert(result.net.transitions().size() == 4);
    assert((result.old_to_new_place ==
            std::vector<std::size_t>{0, kRemovedPlace, 1, 2}));
    assert(result.net.transitions()[0].outputs ==
           std::vector<std::size_t>{1});
    assert(result.net.transitions()[1].outputs ==
           std::vector<std::size_t>{1});
    assert(result.net.transitions()[2].inputs ==
           std::vector<std::size_t>{1});
    assert(result.net.transitions()[3].inputs ==
           std::vector<std::size_t>{1});
  }

  {
    // Duplicate silent transitions are behaviorally identical and can be
    // fused even when their boundary place is protected.
    PetriNet net;
    net.set_activities({"a", "b"});
    const auto source = net.add_place();
    const auto middle = net.add_place();
    const auto sink = net.add_place();
    net.set_source(source);
    net.set_sink(sink);
    net.add_transition(std::nullopt, {source}, {middle});
    net.add_transition(std::nullopt, {source}, {middle});
    net.add_transition(0, {middle}, {sink});
    net.add_transition(1, {middle}, {sink});
    const auto result = reduce_petri_net(net);
    assert(result.changed);
    assert(result.net.place_count() == 3);
    assert(result.net.transitions().size() == 3);
    assert(result.net.transitions()[0].activity == std::nullopt);
    assert(result.net.transitions()[1].activity ==
           std::optional<std::size_t>(0));
    assert(result.net.transitions()[2].activity ==
           std::optional<std::size_t>(1));
  }

  {
    // An ordinary silent self-loop has no observable effect once another
    // transition produces the loop place.
    PetriNet net;
    net.set_activities({"a", "b"});
    const auto source = net.add_place();
    const auto middle = net.add_place();
    const auto sink = net.add_place();
    net.set_source(source);
    net.set_sink(sink);
    net.add_transition(0, {source}, {middle});
    net.add_transition(std::nullopt, {middle}, {middle});
    net.add_transition(1, {middle}, {sink});
    const auto result = reduce_petri_net(net);
    assert(result.changed);
    assert(result.net.place_count() == 3);
    assert(result.net.transitions().size() == 2);
    assert(result.net.transitions()[0].activity ==
           std::optional<std::size_t>(0));
    assert(result.net.transitions()[1].activity ==
           std::optional<std::size_t>(1));
  }

  {
    // A positively marked self-loop place is an invariant token and can be
    // eliminated together with its ordinary input/output arcs.
    PetriNet net;
    net.set_activities({"a"});
    const auto source = net.add_place();
    const auto invariant = net.add_place();
    const auto sink = net.add_place();
    net.set_source(source);
    net.set_sink(sink);
    net.add_transition(0, {source, invariant}, {invariant, sink});
    PetriNetReductionOptions options;
    options.initial_marking = {1, 1, 0};
    options.final_marking = {0, 0, 1};
    const auto result = reduce_petri_net(net, options);
    assert(result.changed);
    assert(result.net.place_count() == 2);
    assert(result.net.transitions().size() == 1);
    assert(result.net.transitions()[0].inputs ==
           std::vector<std::size_t>{0});
    assert(result.net.transitions()[0].outputs ==
           std::vector<std::size_t>{1});
    assert((result.old_to_new_place ==
            std::vector<std::size_t>{0, kRemovedPlace, 1}));
    assert(result.markings.initial == std::vector<std::int64_t>({1, 0}));
    assert(result.markings.final == std::vector<std::int64_t>({0, 1}));
  }

  {
    // The invariant-place rule requires a positive supplied marking and a
    // closed producer/consumer set; an unmarked or externally consumed place
    // must remain observable.
    PetriNet net;
    net.set_activities({"a", "b"});
    const auto source = net.add_place();
    const auto invariant = net.add_place();
    const auto branch = net.add_place();
    const auto sink = net.add_place();
    net.set_source(source);
    net.set_sink(sink);
    net.add_transition(0, {source, invariant}, {invariant, branch});
    net.add_transition(1, {invariant}, {sink});
    const auto unmarked = reduce_petri_net(net);
    assert(!unmarked.changed);
    PetriNetReductionOptions options;
    options.initial_marking = {1, 1, 0, 0};
    const auto open = reduce_petri_net(net, options);
    assert(!open.changed);
    assert(open.net.place_count() == net.place_count());
    assert(open.net.transitions().size() == net.transitions().size());
  }

  {
    // A shared producer output is not a legal series-transition splice: the
    // silent bridge must not duplicate or reorder a token on that place.
    PetriNet net;
    net.set_activities({"a", "b"});
    const auto source = net.add_place();
    const auto private_place = net.add_place();
    const auto shared = net.add_place();
    const auto branch = net.add_place();
    const auto sink = net.add_place();
    net.set_source(source);
    net.set_sink(sink);
    net.add_transition(0, {source}, {private_place, shared});
    net.add_transition(std::nullopt, {private_place}, {shared, branch});
    net.add_transition(1, {shared}, {sink});
    const auto result = reduce_petri_net(net);
    assert(!result.changed);
    assert(same_net(result.net, net));
  }

  {
    // Parallel-place fusion is opt-in and preserves ordinary arc weights by
    // redirecting both equivalent place occurrences to one representative.
    PetriNet net;
    net.set_activities({"a"});
    const auto source = net.add_place();
    const auto left = net.add_place();
    const auto right = net.add_place();
    const auto sink = net.add_place();
    net.set_source(source);
    net.set_sink(sink);
    net.add_transition(std::nullopt, {source}, {left, right});
    net.add_transition(0, {left, right}, {left, right});
    net.add_transition(std::nullopt, {left, right}, {sink});

    const auto conservative = reduce_petri_net(net);
    assert(!conservative.changed);
    PetriNetReductionOptions options;
    options.enable_parallel_place_fusion = true;
    const auto result = reduce_petri_net(net, options);
    assert(result.changed);
    assert(result.net.place_count() == 3);
    assert(result.net.transitions().size() == 3);
    assert((result.old_to_new_place ==
            std::vector<std::size_t>{0, 1, 1, 2}));
    assert(result.net.transitions()[0].outputs ==
           std::vector<std::size_t>({1, 1}));
    assert(result.net.transitions()[1].inputs ==
           std::vector<std::size_t>({1, 1}));
    assert(result.net.transitions()[1].outputs ==
           std::vector<std::size_t>({1, 1}));
    assert(result.net.transitions()[2].inputs ==
           std::vector<std::size_t>({1, 1}));

    options.initial_marking = {1, 1, 0, 0};
    const auto mismatched = reduce_petri_net(net, options);
    assert(!mismatched.changed);
    assert(same_net(mismatched.net, net));

    options.initial_marking = {1, 2, 2, 0};
    const auto equal_marking = reduce_petri_net(net, options);
    assert(equal_marking.changed);
    assert(equal_marking.markings.initial ==
           std::vector<std::int64_t>({1, 4, 0}));
  }

  {
    // Abstraction removes a silent fan-out bridge with multiple producers and
    // reconnects every producer to each target in deterministic order.
    PetriNet net;
    net.set_activities({"a", "b", "c", "d"});
    const auto source = net.add_place();
    const auto private_place = net.add_place();
    const auto left = net.add_place();
    const auto right = net.add_place();
    const auto sink = net.add_place();
    net.set_source(source);
    net.set_sink(sink);
    net.add_transition(0, {source}, {private_place});
    net.add_transition(1, {source}, {private_place});
    net.add_transition(std::nullopt, {private_place}, {left, right});
    net.add_transition(2, {left}, {sink});
    net.add_transition(3, {right}, {sink});

    const auto conservative = reduce_petri_net(net);
    assert(!conservative.changed);
    PetriNetReductionOptions options;
    options.enable_abstraction = true;
    const auto result = reduce_petri_net(net, options);
    assert(result.changed);
    assert(result.net.place_count() == 4);
    assert(result.net.transitions().size() == 4);
    assert((result.old_to_new_place ==
            std::vector<std::size_t>{0, kRemovedPlace, 1, 2, 3}));
    assert(result.net.transitions()[0].outputs ==
           std::vector<std::size_t>({1, 2}));
    assert(result.net.transitions()[1].outputs ==
           std::vector<std::size_t>({1, 2}));

    options.initial_marking = {1, 1, 0, 0, 0};
    const auto marked = reduce_petri_net(net, options);
    assert(!marked.changed);
    assert(same_net(marked.net, net));
  }

  {
    bool threw = false;
    try {
      PetriNetReductionOptions options;
      options.final_marking = {0};
      (void)reduce_petri_net(silent_chain(), options);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }

  {
    const auto first = reduce_petri_net(silent_chain());
    const auto second = reduce_petri_net(silent_chain());
    assert(same_net(first.net, second.net));
    assert(first.old_to_new_place == second.old_to_new_place);
    const auto json = petri_net_reduction_json(first);
    assert(json.find("\"family\":\"PETRI_NET_REDUCTION\"") !=
           std::string::npos);
    assert(json.find("\"variant\":\"complete\"") !=
           std::string::npos);
    assert(json.find("\"changed\":true") != std::string::npos);
    assert(json.find("\"old_to_new_place\":[") != std::string::npos);
    assert(json.find("\"pnml\":\"<?xml") != std::string::npos);

  }
}
