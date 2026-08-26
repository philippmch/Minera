#include "minera/petri_net_reduction.hpp"

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

}  // namespace

int main() {
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
  }
}
