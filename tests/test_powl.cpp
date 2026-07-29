#include "minera/powl.hpp"

#include <cassert>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>

using minera::PowlFrequency;
using minera::PowlNode;

namespace {
template <typename F>
void throws_invalid(F&& fn) {
  bool thrown = false;
  try { fn(); } catch (const std::invalid_argument&) { thrown = true; }
  assert(thrown);
}
PowlNode a() { return PowlNode::transition("A"); }
PowlNode b() { return PowlNode::transition("B"); }
}

int main() {
  // Leaves, including empty labels, tau, and frequency metadata.
  const auto empty = PowlNode::transition("");
  assert(empty.kind() == minera::PowlNodeKind::kTransition);
  assert(empty.label().empty());
  assert(PowlNode::silent().kind() == minera::PowlNodeKind::kSilent);
  const auto optional = PowlNode::transition("A", PowlFrequency{0, std::nullopt});
  assert(optional.skippable() && optional.self_loop());
  assert(PowlNode::parse(optional.serialize()) == optional);
  throws_invalid([] { PowlNode::transition("A", PowlFrequency{2, 1}); });

  // Arity and loop ordering.
  throws_invalid([] { PowlNode::xor_node({a()}); });
  const auto loop_ab = PowlNode::loop(a(), b());
  const auto loop_ba = PowlNode::loop(b(), a());
  assert(loop_ab != loop_ba);
  throws_invalid([] { PowlNode::parse("*(tau)"); });
  throws_invalid([&] { (void)loop_ab.has_edge(0, 1); });
  throws_invalid([&] { (void)loop_ab.start_nodes(); });
  throws_invalid([&] { (void)loop_ab.end_nodes(); });
  throws_invalid([&] { (void)loop_ab.transitive_reduction(); });

  // Strict PO closure, start/end nodes, and transitive reduction.
  const auto po = PowlNode::strict_partial_order({a(), b(), empty}, {{0, 1}, {1, 2}});
  assert(po.has_edge(0, 1) && po.has_edge(1, 2) && po.has_edge(0, 2));
  throws_invalid([&] { (void)po.has_edge(0, 3); });
  assert((po.start_nodes() == std::vector<std::size_t>{0}));
  assert((po.end_nodes() == std::vector<std::size_t>{2}));
  assert((po.transitive_reduction() == std::vector<std::pair<std::size_t, std::size_t>>{{0, 1}, {1, 2}}));
  throws_invalid([] { PowlNode::strict_partial_order({a(), b()}, {{0, 2}}); });
  throws_invalid([] { PowlNode::strict_partial_order({a(), b()}, {{0, 0}}); });
  throws_invalid([] { PowlNode::strict_partial_order({a(), b()}, {{0, 1}, {1, 0}}); });

  const auto seq = PowlNode::sequence({a(), b(), empty});
  assert(seq.has_edge(0, 2) && seq.order().size() == 3);
  const auto conc = PowlNode::concurrency({a(), b()});
  assert(conc.order().empty());
  assert((conc.start_nodes() == std::vector<std::size_t>{0, 1}));

  // Deterministic canonical text and parser round-trip (nested and escaped).
  const auto complex = PowlNode::xor_node({
      PowlNode::transition("quote \" slash \\ newline\n"),
      PowlNode::loop(PowlNode::silent(), optional),
      seq,
  });
  const std::string text = complex.serialize();
  assert(text == complex.serialize());
  assert(PowlNode::parse(text) == complex);
  assert(PowlNode::parse("X(T(\"A\"),tau)") == PowlNode::xor_node({a(), PowlNode::silent()}));
  const auto abc = PowlNode::strict_partial_order({a(), b(), PowlNode::transition("C")}, {{0, 1}, {1, 2}});
  assert(PowlNode::parse(
             "PO=(nodes={ NODE1, NODE2, NODE3 }, "
             "order={ NODE1-->NODE2, NODE2-->NODE3 })") ==
         PowlNode::strict_partial_order(
             {PowlNode::transition("NODE1"), PowlNode::transition("NODE2"),
              PowlNode::transition("NODE3")},
             {{0, 1}, {1, 2}}));
  assert(PowlNode::parse(abc.serialize()) == abc);
  assert(abc.serialize().find("A-->B") != std::string::npos);
  assert(PowlNode::parse("PO=(nodes={},order={})").children().empty());
  const auto nested_po = PowlNode::strict_partial_order(
      {PowlNode::xor_node({a(), b()}), PowlNode::transition("C")}, {{0, 1}});
  assert(PowlNode::parse(nested_po.serialize()) == nested_po);
  const auto duplicate_po =
      PowlNode::strict_partial_order({a(), a()}, {{0, 1}});
  assert(duplicate_po.serialize().find("@0-->@1") != std::string::npos);
  assert(PowlNode::parse(duplicate_po.serialize()) == duplicate_po);
  for (const auto& whitespace_label :
       {std::string(" A"), std::string("A "), std::string("   ")}) {
    const auto whitespace_po = PowlNode::strict_partial_order(
        {PowlNode::transition(whitespace_label), b()}, {{0, 1}});
    assert(PowlNode::parse(whitespace_po.serialize()) == whitespace_po);
  }

  // Raw labels consume their full grammar item and operator-like
  // prefixes are recognized only when followed by their delimiters.
  assert(PowlNode::parse("Receive Invoice") ==
         PowlNode::transition("Receive Invoice"));
  assert(PowlNode::parse("'Receive Invoice'") ==
         PowlNode::transition("Receive Invoice"));
  assert(PowlNode::parse("'it\\'s ready'") ==
         PowlNode::transition("it's ready"));
  assert(PowlNode::parse("Xray") == PowlNode::transition("Xray"));
  assert(PowlNode::parse("Power") == PowlNode::transition("Power"));
  assert(PowlNode::parse("tau details") ==
         PowlNode::transition("tau details"));

  // Malformed syntax must fail instead of accepting a partial object.
  for (const auto& malformed : {
           std::string(""), std::string("X(A)"), std::string("*(A)"),
           std::string("T(\"unterminated)"), std::string("PO=(nodes={A},order={0<1})"),
           std::string("PO=(nodes={A,B},order={0<1,1<0})"),
           std::string("PO=(nodes={A,B},order={0<0})"),
           std::string("T(\"A\",4294967296,1)"),
           std::string("T(\"A\",1,4294967296)"),
           std::string("X(A,B) trailing")}) {
    throws_invalid([&] { PowlNode::parse(malformed); });
  }

  // Value copies are independent deep copies and preserve structural identity.
  auto copy = complex;
  assert(copy == complex);
  assert(copy.children().size() == complex.children().size());
  std::cout << "POWL tests passed\n";
}
