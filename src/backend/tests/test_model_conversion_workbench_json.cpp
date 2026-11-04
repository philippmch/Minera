#undef NDEBUG

#include "minera/model_conversion_workbench_json.hpp"
#include "minera/graph_alignment_workbench_json.hpp"
#include "minera/inductive_input_json.hpp"
#include "minera/powl_conversion.hpp"
#include "minera/workflow_input_result_json.hpp"

#include <cassert>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace minera;
using Values = std::map<std::string, std::string, std::less<>>;

PetriNetReductionOptions parsed(const Values& values, bool use_presence = true) {
  const FieldLookup lookup = [&](std::string_view name) {
    const auto found = values.find(name);
    return found == values.end() ? std::string{} : found->second;
  };
  const FieldPresence presence = [&](std::string_view name) { return values.contains(name); };
  return parse_model_reduction_workbench_params(lookup, use_presence ? presence : FieldPresence{});
}

template <class Error = std::invalid_argument, class Action>
void rejects(Action action) {
  bool threw = false;
  try { action(); } catch (const Error&) { threw = true; }
  assert(threw);
}

void contains(const std::string& json, std::string_view expected) {
  assert(json.find(expected) != std::string::npos);
}

std::size_t occurrences(const std::string& text, std::string_view token) {
  std::size_t count = 0, position = 0;
  while ((position = text.find(token, position)) != std::string::npos) {
    ++count; position += token.size();
  }
  return count;
}

void scoped_markings_and_booleans_preserve_full_native_options() {
  const auto defaults = parsed({});
  assert(defaults.initial_marking.empty() && defaults.final_marking.empty());
  assert(!defaults.enable_parallel_place_fusion && !defaults.enable_abstraction && !defaults.preserve_join_bridges);
  assert(serialize_model_reduction_options_json(defaults) ==
         "{\"initial_marking\":[],\"final_marking\":[],\"enable_parallel_place_fusion\":false,\"enable_abstraction\":false,\"preserve_join_bridges\":false}");
  const auto options = parsed({
      {"petri_reduction_initial_marking_json", "[-9223372036854775808,0,9223372036854775807]"},
      {"petri_reduction_final_marking", " -9223372036854775808, -1, 9223372036854775807 "},
      {"petri_reduction_parallel_place_fusion", "true"},
      {"petri_reduction_abstraction", "1"},
      {"petri_reduction_preserve_join_bridges", "true"}});
  assert((options.initial_marking == Marking{std::numeric_limits<std::int64_t>::min(), 0, std::numeric_limits<std::int64_t>::max()}));
  assert((options.final_marking == Marking{std::numeric_limits<std::int64_t>::min(), -1, std::numeric_limits<std::int64_t>::max()}));
  assert(options.enable_parallel_place_fusion && options.enable_abstraction && options.preserve_join_bridges);
  const auto json = serialize_model_reduction_options_json(options);
  contains(json, "\"initial_marking\":[-9223372036854775808,0,9223372036854775807]");
  contains(json, "\"final_marking\":[-9223372036854775808,-1,9223372036854775807]");
  contains(json, "\"enable_parallel_place_fusion\":true,\"enable_abstraction\":true,\"preserve_join_bridges\":true");
  assert(parsed({{"petri_reduction_initial_marking_json", " \t[]\r\n"}}).initial_marking.empty());
  assert((parsed({{"petri_reduction_initial_marking_json", "[0,-0,0]"}}).initial_marking == Marking{0, 0, 0}));
  assert((parsed({{"petri_reduction_initial_marking", "01,-00,-1"}}).initial_marking == Marking{1, 0, -1}));
  assert(parsed({{"petri_reduction_initial_marking_json", "[]"}}, false).initial_marking.empty());
  for (const auto& key : {"petri_reduction_parallel_place_fusion", "petri_reduction_abstraction", "petri_reduction_preserve_join_bridges"}) {
    for (const auto& value : {"true", "false", "1", "0"}) {
      const auto actual = parsed({{key, value}});
      const bool expected = std::string_view(value) == "true" || std::string_view(value) == "1";
      if (std::string_view(key) == "petri_reduction_parallel_place_fusion") assert(actual.enable_parallel_place_fusion == expected);
      if (std::string_view(key) == "petri_reduction_abstraction") assert(actual.enable_abstraction == expected);
      if (std::string_view(key) == "petri_reduction_preserve_join_bridges") assert(actual.preserve_join_bridges == expected);
    }
    for (const auto& value : {"", "TRUE", "True", "yes", " true", "2"}) rejects([&] { (void)parsed({{key, value}}); });
  }
}

void marking_carriers_reject_ambiguity_and_invalid_integer_grammar() {
  for (const auto& prefix : {"petri_reduction_initial_marking", "petri_reduction_final_marking"}) {
    const auto key = std::string(prefix), json_key = key + "_json";
    for (const auto& legacy : {"", "0", "-1"})
      for (const auto& json : {"", "[]", "[0]"})
        rejects([&] { (void)parsed({{key, legacy}, {json_key, json}}); });
    for (const auto& value : {"", " ", "0", "null", "{}", "[true]", "[null]", "[\"1\"]", "[[1]]", "[{\"x\":1,\"x\":2}]",
         "[1.0]", "[1e0]", "[+1]", "[01]", "[-01]", "[NaN]", "[Infinity]", "[1,]", "[,1]", "[1]x",
         "[9223372036854775808]", "[-9223372036854775809]"})
      rejects([&] { (void)parsed({{json_key, value}}); });
    for (const auto& value : {"", " ", ",", "1,", ",1", "1,,2", "+1", "1.0", "1e0", "9223372036854775808", "-9223372036854775809"})
      rejects([&] { (void)parsed({{key, value}}); });
  }
  rejects([] { (void)parsed({{"petri_reduction_initial_marking_json", std::string("[1]") + '\0'}}); });
  rejects([] { (void)parsed({{"petri_reduction_initial_marking_json", std::string("[") + "\xed\xa0\x80" + "]"}}); });
}

void input_limits_do_not_become_output_limits() {
  rejects([] { (void)parsed({{"petri_reduction_initial_marking_json", std::string(32u * 1024u * 1024u, ' ') + "[]"}}); });
  rejects([] { (void)parsed({{"petri_reduction_initial_marking", std::string(32u * 1024u * 1024u, ' ') + "0"}}); });
  std::string million = "[0";
  for (std::size_t i = 1; i < 1000000; ++i) million += ",0";
  million += ']';
  rejects([&] { (void)parsed({{"petri_reduction_final_marking_json", million}}); });
  rejects([] { (void)parsed({{"petri_reduction_initial_marking_json", std::string(65, '[') + "0" + std::string(65, ']')}}); });

  auto deep = PowlNode::transition("deep");
  for (std::size_t i = 0; i < 80; ++i) deep = PowlNode::loop(std::move(deep), PowlNode::silent());
  const auto deep_json = serialize_model_conversion_powl_json(deep);
  assert(occurrences(deep_json, "\"kind\":\"LOOP\"") == 80);
  assert(occurrences(deep_json, "\"kind\":\"SILENT\"") == 80);
  contains(deep_json, "\"label\":\"deep\"");
  const auto large = PowlNode::transition(std::string(32u * 1024u * 1024u + 1u, 'a'));
  assert(serialize_model_conversion_powl_json(large).size() > 32u * 1024u * 1024u);
}

void powl_snapshots_preserve_native_getters_text_and_order() {
  const auto empty = PowlNode::transition("");
  const auto silent = PowlNode::silent();
  assert(serialize_model_conversion_powl_json(empty) ==
         "{\"kind\":\"TRANSITION\",\"kind_index\":0,\"label\":\"\",\"frequency\":{\"minimum\":1,\"maximum\":1},\"children\":[],\"order\":[]}");
  assert(serialize_model_conversion_powl_json(silent) ==
         "{\"kind\":\"SILENT\",\"kind_index\":1,\"label\":\"\",\"frequency\":{\"minimum\":1,\"maximum\":1},\"children\":[],\"order\":[]}");
  std::string label = " quote \" slash \\ colon : comma , arrow --> at @9 \n";
  label += '\0'; label += "\xe2\x82\xac"; label += "\xed\xa0\x80";
  const auto optional = PowlNode::transition(label, PowlFrequency{0, std::nullopt});
  const auto canonical = optional.serialize();
  assert(PowlNode::parse(canonical) == optional);
  const auto quote_array = serialize_alignment_trace_words_json({label});
  const auto optional_json = serialize_model_conversion_powl_json(optional);
  contains(optional_json, "\"label\":" + quote_array.substr(1, quote_array.size() - 2));
  contains(optional_json, "\"frequency\":{\"minimum\":0,\"maximum\":null}");
  assert(optional.serialize() == canonical);
  const auto po = PowlNode::strict_partial_order(
      {PowlNode::transition("a"), PowlNode::transition("a"), empty}, {{1, 2}, {0, 1}});
  assert(po.order().size() == 3 && po.has_edge(0, 2));
  std::string stored_order = "\"order\":[";
  for (std::size_t i = 0; i < po.order().size(); ++i) {
    if (i) stored_order += ',';
    stored_order += '[' + std::to_string(po.order()[i].first) + ',' + std::to_string(po.order()[i].second) + ']';
  }
  stored_order += ']';
  const auto po_json = serialize_model_conversion_powl_json(po);
  contains(po_json, stored_order);
  assert(occurrences(po_json, "\"label\":\"a\"") == 2);
  assert(PowlNode::parse(po.serialize()) == po);
  const auto compound = PowlNode::xor_node({po, PowlNode::loop(optional, silent)});
  const auto json = serialize_model_conversion_powl_json(compound);
  contains(json, "\"kind\":\"XOR\",\"kind_index\":2");
  contains(json, "\"kind\":\"LOOP\",\"kind_index\":3");
  contains(json, "\"kind\":\"STRICT_PARTIAL_ORDER\",\"kind_index\":4");
  contains(json, serialize_model_conversion_powl_json(optional));
  contains(serialize_model_conversion_powl_json(PowlNode::strict_partial_order({}, {})), "\"children\":[],\"order\":[]");
  // Native relation text can interpret numeric labels as indices on reparse.
  // The complete snapshot and canonical text remain independently available.
  const auto numeric = PowlNode::sequence({PowlNode::transition("9"), PowlNode::transition("1")});
  contains(serialize_model_conversion_powl_json(numeric), "\"label\":\"9\"");
  assert(!numeric.serialize().empty());
  // pm4py builds FrequentTransition("a", 2, 1) as a plain "a" (neither skippable
  // nor a self loop), so a minimum above the maximum is accepted.
  (void)PowlNode::transition("a", PowlFrequency{2, 1});
}

void real_native_conversion_results_cover_all_five_overloads() {
  const ProcessTree tree({"A", "B"}, ProcessTreeNode{ProcessTreeOperator::kSequence, 0,
      {ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}});
  const auto from_tree = to_powl(tree);
  assert(from_tree.kind() == PowlNodeKind::kStrictPartialOrder);
  assert(from_tree.children().size() == 2 && from_tree.has_edge(0, 1));
  contains(serialize_model_conversion_powl_json(from_tree), "\"order\":[[0,1]]");
  const auto net = to_petri_net(from_tree);
  assert(net.has_source() && net.has_sink());
  assert((net.activities() == std::vector<std::string>{"A", "B"}));
  const auto from_net = to_powl(net);
  contains(serialize_model_conversion_powl_json(from_net), "\"label\":\"A\"");
  contains(serialize_model_conversion_powl_json(from_net), "\"label\":\"B\"");
  const auto from_powl_tree = to_process_tree(from_tree);
  assert((from_powl_tree.activities() == std::vector<std::string>{"A", "B"}));
  assert(from_powl_tree.root().op == ProcessTreeOperator::kSequence);
  const auto from_net_tree = to_process_tree(net);
  assert((from_net_tree.activities() == std::vector<std::string>{"A", "B"}));
  assert(from_net_tree.root().op == ProcessTreeOperator::kSequence);
  contains(serialize_inductive_process_tree_json(from_powl_tree), "\"activities\":[\"A\",\"B\"]");
  contains(serialize_inductive_process_tree_json(from_net_tree), "\"activity\":0");
  const auto empty_visible_net = to_petri_net(PowlNode::transition(""));
  assert((empty_visible_net.activities() == std::vector<std::string>{""}));
  assert(empty_visible_net.transitions()[0].activity.has_value());
  rejects<std::out_of_range>([] { (void)to_powl(ProcessTree({"a"}, ProcessTreeNode::leaf(1))); });
}

PetriNet silent_chain() {
  PetriNet net; net.set_activities({"a"});
  for (std::size_t i = 0; i < 5; ++i) net.add_place();
  net.set_source(0); net.set_sink(4);
  net.add_transition(std::nullopt, {0}, {1});
  net.add_transition(std::nullopt, {1}, {2});
  net.add_transition(std::nullopt, {2}, {3});
  net.add_transition(0, {3}, {4});
  return net;
}

void real_reduction_outputs_retain_mappings_markings_and_bridge_choices() {
  const auto net = silent_chain();
  const auto result = reduce_petri_net(net);
  assert(result.changed && result.net.place_count() == 2 && result.net.transitions().size() == 1);
  assert(result.markings.initial.empty() && result.markings.final.empty());
  assert(result.old_to_new_place.size() == net.place_count());
  assert(result.old_to_new_transition.size() == net.transitions().size());
  const auto json = serialize_model_reduction_result_json(result);
  contains(json, "\"net\":" + workflow_input_petri_json(result.net));
  contains(json, "\"markings\":{\"initial\":[],\"final\":[]}");
  contains(json, std::to_string(kRemovedTransition));
  contains(json, "\"changed\":true");
  const auto zeros = parsed({{"petri_reduction_initial_marking_json", "[0,0,0,0,0]"},
                             {"petri_reduction_final_marking_json", "[0,0,0,0,0]"}});
  const auto zero_result = reduce_petri_net(net, zeros);
  assert(zero_result.markings.initial.size() == zero_result.net.place_count());
  assert(zero_result.markings.final.size() == zero_result.net.place_count());
  contains(serialize_model_reduction_result_json(zero_result), "\"markings\":{\"initial\":[0,0],\"final\":[0,0]}");
  rejects([&] { (void)reduce_petri_net(net, parsed({{"petri_reduction_initial_marking_json", "[0]"}})); });

  PetriNet bridge; bridge.set_activities({"a", "b", "c", "d", "e"});
  for (std::size_t i = 0; i < 4; ++i) bridge.add_place();
  bridge.set_source(0); bridge.set_sink(3);
  bridge.add_transition(0, {0}, {1}); bridge.add_transition(1, {0}, {1});
  bridge.add_transition(std::nullopt, {1}, {2});
  bridge.add_transition(2, {2}, {3}); bridge.add_transition(3, {2}, {3});
  bridge.add_transition(4, {0}, {2});
  const auto contracted = reduce_petri_net(bridge);
  const auto guarded = reduce_petri_net(bridge, parsed({{"petri_reduction_preserve_join_bridges", "1"}}));
  assert(contracted.changed && !guarded.changed);
  assert(contracted.old_to_new_transition[2] == kRemovedTransition);
  assert(guarded.old_to_new_transition[2] == 2);
  contains(serialize_model_reduction_result_json(guarded), "\"old_to_new_transition\":[0,1,2,3,4,5]");

  PetriNet signed_net; signed_net.set_activities({"a"});
  signed_net.set_source(signed_net.add_place()); signed_net.set_sink(signed_net.add_place());
  signed_net.add_transition(0, {0}, {1});
  const auto signed_options = parsed({{"petri_reduction_initial_marking_json", "[-9223372036854775808,0]"},
                                      {"petri_reduction_final_marking_json", "[0,9223372036854775807]"}});
  const auto signed_result = reduce_petri_net(signed_net, signed_options);
  assert(signed_result.markings.initial == signed_options.initial_marking);
  assert(signed_result.markings.final == signed_options.final_marking);
  contains(serialize_model_reduction_result_json(signed_result), "\"initial\":[-9223372036854775808,0]");

  PetriNet parallel; parallel.set_activities({"a", "b"});
  for (std::size_t i = 0; i < 4; ++i) parallel.add_place();
  parallel.set_source(0); parallel.set_sink(3);
  parallel.add_transition(0, {0}, {1, 2});
  parallel.add_transition(1, {1, 2}, {3});
  const auto overflow = parsed({
      {"petri_reduction_initial_marking_json", "[0,9223372036854775807,9223372036854775807,0]"},
      {"petri_reduction_parallel_place_fusion", "true"}});
  rejects<std::overflow_error>([&] { (void)reduce_petri_net(parallel, overflow); });

  // changed describes structural rule application, not metadata identity:
  // even a valid unchanged path reconstructs the native net.
  PetriNet raw; raw.set_activities({"a"}); raw.add_place(); raw.add_place();
  raw.add_transition(0, {0}, {1}); raw.set_transition_names({std::string("named")});
  const auto raw_json = workflow_input_petri_json(raw);
  const auto rebuilt = reduce_petri_net(raw);
  assert(!rebuilt.changed && !raw.has_source() && !raw.has_sink());
  assert(rebuilt.net.has_source() && rebuilt.net.has_sink());
  assert(raw.transition_names().has_value() && !rebuilt.net.transition_names().has_value());
  contains(serialize_model_reduction_result_json(rebuilt), "\"net\":" + workflow_input_petri_json(rebuilt.net));
  assert(workflow_input_petri_json(raw) == raw_json);
}

void complete_result_writer_retains_raw_native_state_and_deep_metadata() {
  PetriNetReductionResult result;
  result.net.set_activities({"", "\xed\xbf\xbf"});
  result.net.add_place();
  result.net.add_transition(std::nullopt, {}, {});
  result.net.set_transition_names({std::string("")});
  HeuristicStructuredValue name{std::string("leaf")};
  for (std::size_t i = 0; i < 80; ++i)
    name = HeuristicStructuredValue{HeuristicStructuredValue::Array{std::move(name)}};
  PetriDiscoveryMarkings discovery;
  discovery.detached_places.push_back({std::numeric_limits<std::size_t>::max(), std::move(name)});
  result.net.set_discovery_markings(std::move(discovery));
  result.markings.initial = {std::numeric_limits<std::int64_t>::min(), 0};
  result.markings.final = {std::numeric_limits<std::int64_t>::max()};
  result.old_to_new_place = {kRemovedPlace, 0};
  result.old_to_new_transition = {kRemovedTransition};
  const auto json = serialize_model_reduction_result_json(result);
  contains(json, "\"net\":" + workflow_input_petri_json(result.net));
  contains(json, "\"old_to_new_place\":[" + std::to_string(kRemovedPlace) + ",0]");
  contains(json, "\"old_to_new_transition\":[" + std::to_string(kRemovedTransition) + ']');
  contains(json, "\"changed\":false");
  contains(json, "\"discovery_markings\":");
  const std::string escaped_surrogate = R"(\udfff)";
  contains(json, escaped_surrogate);
  assert(json.find("\xed\xbf\xbf") == std::string::npos);
}

}  // namespace

int main() {
  scoped_markings_and_booleans_preserve_full_native_options();
  marking_carriers_reject_ambiguity_and_invalid_integer_grammar();
  input_limits_do_not_become_output_limits();
  powl_snapshots_preserve_native_getters_text_and_order();
  real_native_conversion_results_cover_all_five_overloads();
  real_reduction_outputs_retain_mappings_markings_and_bridge_choices();
  complete_result_writer_retains_raw_native_state_and_deep_metadata();
}
