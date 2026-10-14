#undef NDEBUG

#include "minera/inductive_input_json.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace minera;

void contains(const std::string& json, std::string_view expected) {
  assert(json.find(expected) != std::string::npos);
}

template <class Parse>
void rejects(Parse parse, const std::vector<std::string>& cases) {
  for (const auto& input : cases) {
    bool threw = false;
    try { (void)parse(input); } catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
  }
}

void equal_variants(const InductiveMinerVariantLog& left, const InductiveMinerVariantLog& right) {
  assert(left.size() == right.size());
  for (std::size_t i = 0; i < left.size(); ++i)
    assert(left[i].activities == right[i].activities && left[i].count == right[i].count);
}

EventLog log_of(const std::vector<std::vector<std::string>>& words) {
  EventLog result;
  for (const auto& word : words) {
    Trace trace;
    for (const auto& label : word) {
      Event event{"", label}; event.activity_state = FirstClassAttributeState::kPresent;
      trace.events.push_back(std::move(event));
    }
    result.traces.push_back(std::move(trace));
  }
  return result;
}

void weighted_variants_preserve_order_repetitions_and_exact_counts() {
  const auto input = parse_inductive_variant_log_json(
      R"([{"activities":[""," a\t\n ","a:b,=\\","n\u0000","\ud800","\udfff","\ud83d\ude00"],"count":18446744073709551615},{"activities":[],"count":0},{"activities":["A","A"],"count":9007199254740993},{"activities":["A","A"],"count":7}])");
  assert(input.size() == 4 && input[0].count == std::numeric_limits<std::uint64_t>::max());
  assert(input[0].activities.size() == 7 && input[0].activities[0].empty());
  assert(input[0].activities[1] == " a\t\n " && input[0].activities[2] == "a:b,=\\");
  assert(input[0].activities[3] == std::string("n\0", 2));
  assert(input[0].activities[4] == "\xed\xa0\x80" && input[0].activities[5] == "\xed\xbf\xbf");
  assert(input[0].activities[6] == "\xf0\x9f\x98\x80");
  assert(input[1].activities.empty() && input[1].count == 0);
  assert(input[2].count == 9007199254740993ull && input[3].count == 7 && input[2].activities == input[3].activities);
  const auto output = serialize_inductive_variant_log_json(input);
  contains(output, R"(\ud800)"); contains(output, R"("count":18446744073709551615)");
  assert(output.find("\xed\xa0\x80") == std::string::npos);
  equal_variants(input, parse_inductive_variant_log_json(output));
  assert(parse_inductive_variant_log_json("[]").empty());
  assert(serialize_inductive_variant_log_json({}) == "[]");
}

void strict_weighted_schema_integer_grammar_and_duplicate_keys() {
  rejects(parse_inductive_variant_log_json, {
      "", "null", "{}", "0", "[null]", "[[]]", "[{}]", R"([{"activities":[]}])",
      R"([{"count":1}])", R"([{"activities":[],"count":1,"extra":0}])",
      R"([{"activities":null,"count":0}])", R"([{"activities":[1],"count":0}])",
      R"([{"activities":[null],"count":0}])", R"([{"activities":[["a"]],"count":0}])",
      R"([{"activities":[],"count":-0}])", R"([{"activities":[],"count":-1}])",
      R"([{"activities":[],"count":1.0}])", R"([{"activities":[],"count":1e0}])",
      R"([{"activities":[],"count":"1"}])", R"([{"activities":[],"count":true}])",
      R"([{"activities":[],"count":18446744073709551616}])",
      R"([{"activities":[],"count":01}])", R"([{"activities":[],"count":+1}])",
      R"([{"activities":[],"count":NaN}])", R"([{"activities":[],"count":Infinity}])",
      R"([{"activities":[],"count":1,"co\u0075nt":2}])",
      R"([{"activities":[],"activit\u0069es":[],"count":0}])",
      R"([{"activities":[],"count":0},])", R"([{"activities":[],"count":0}]false)",
      "[{\"activities\":[\"\xed\xa0\x80\"],\"count\":1}]",
      "[{\"activities\":[\"\xc0\x80\"],\"count\":1}]",
      "[{\"activities\":[\"x\n\"],\"count\":1}]"});
}

void constructor_graph_preserves_every_declared_field_and_zero_edge() {
  const auto input = parse_inductive_dfg_input_json(
      R"({"activities":[""," a ","\ud800"],"edges":[{"source":2,"target":0,"count":18446744073709551615},{"source":0,"target":1,"count":0}],"start_counts":[0,18446744073709551615,7],"end_counts":[9007199254740993,0,8]})");
  assert(input.activities == std::vector<std::string>({"", " a ", "\xed\xa0\x80"}));
  assert(input.edges.size() == 2 && input.edges[0].source == 2 && input.edges[0].target == 0);
  assert(input.edges[0].count == std::numeric_limits<std::uint64_t>::max() && input.edges[1].count == 0);
  assert(input.start_counts[1] == std::numeric_limits<std::uint64_t>::max() && input.end_counts[0] == 9007199254740993ull);
  const auto encoded = serialize_inductive_dfg_input_json(input);
  const auto repeated = parse_inductive_dfg_input_json(encoded);
  assert(repeated.activities == input.activities && repeated.start_counts == input.start_counts && repeated.end_counts == input.end_counts);
  assert(repeated.edges[0].source == 2 && repeated.edges[1].source == 0 && repeated.edges[1].count == 0);
  const auto graph = DirectlyFollowsGraph::from_frequencies(input.activities, input.edges, input.start_counts, input.end_counts);
  const auto snapshot = serialize_inductive_native_dfg_json(graph);
  contains(snapshot, R"("activity_counts":[0,0,0],"rework_case_counts":[0,0,0],"variants":[])");
  contains(snapshot, R"("start_counts":[0,18446744073709551615,7],"end_counts":[9007199254740993,0,8])");
  contains(snapshot, R"("edges":[{"source":2,"target":0,"count":18446744073709551615},{"source":0,"target":1,"count":0}])");
  assert(serialize_inductive_dfg_input_json({}) == R"({"activities":[],"edges":[],"start_counts":[],"end_counts":[]})");
  contains(serialize_inductive_native_dfg_json(DirectlyFollowsGraph{}), R"("activities":[],"edges":[],"start_counts":[],"end_counts":[],"activity_counts":[],"rework_case_counts":[],"variants":[])");
}

void strict_graph_schema_and_native_constructor_validation() {
  rejects(parse_inductive_dfg_input_json, {
      "[]", "{}", "null", R"({"activities":[],"edges":[],"start_counts":[]})",
      R"({"activities":[],"edges":[],"start_counts":[],"end_counts":[],"activity_counts":[]})",
      R"({"activities":["a","\u0061"],"edges":[],"start_counts":[0,0],"end_counts":[0,0]})",
      R"({"activities":["a"],"edges":[],"start_counts":[],"end_counts":[0]})",
      R"({"activities":["a"],"edges":[],"start_counts":[0],"end_counts":[0,1]})",
      R"({"activities":[],"edges":[{"source":0,"target":0,"count":0}],"start_counts":[],"end_counts":[]})",
      R"({"activities":["a"],"edges":[{"source":1,"target":0,"count":1}],"start_counts":[0],"end_counts":[0]})",
      R"({"activities":["a"],"edges":[{"source":0,"target":0,"count":0},{"source":0,"target":0,"count":1}],"start_counts":[0],"end_counts":[0]})",
      R"({"activities":["a"],"edges":[{"source":0,"target":0,"count":1,"source":0}],"start_counts":[0],"end_counts":[0]})",
      R"({"activities":["a"],"edges":[{"source":0,"target":0,"count":1,"co\u0075nt":2}],"start_counts":[0],"end_counts":[0]})",
      R"({"activities":["a"],"edges":[{"source":0,"target":0,"count":1,"extra":0}],"start_counts":[0],"end_counts":[0]})",
      R"({"activities":["a"],"edges":[{"source":18446744073709551616,"target":0,"count":1}],"start_counts":[0],"end_counts":[0]})",
      R"({"activities":["a"],"edges":[{"source":0.0,"target":0,"count":1}],"start_counts":[0],"end_counts":[0]})",
      R"({"activities":["a"],"edges":[],"start_counts":[-1],"end_counts":[0]})",
      R"({"activities":["a"],"edges":[],"start_counts":[18446744073709551616],"end_counts":[0]})",
      R"({"activities":["a"],"edges":[],"start_counts":[0],"end_counts":["0"]})"});
  InductiveDfgInput invalid; invalid.activities = {"a"};
  bool threw = false;
  try { (void)serialize_inductive_dfg_input_json(invalid); } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

void actual_graph_observations_are_not_reconstructed_from_frequencies() {
  auto graph = discover_directly_follows_graph(log_of({{"b", "a", "b"}, {"b"}, {"b", "a", "b"}, {}}));
  graph.ensure_activity("unused");
  const auto encoded = serialize_inductive_native_dfg_json(graph);
  contains(encoded, R"("activities":["b","a","unused"])");
  contains(encoded, R"("edges":[{"source":0,"target":1,"count":2},{"source":1,"target":0,"count":2}])");
  contains(encoded, R"("start_counts":[3,0,0],"end_counts":[3,0,0],"activity_counts":[5,2,0],"rework_case_counts":[2,0,0])");
  contains(encoded, R"("variants":[{"activities":["b","a","b"],"count":2},{"activities":["b"],"count":1}])");
  assert(graph.activities().size() == 3 && graph.variants().size() == 2 && graph.rework_case_count("b") == 2);
}

void real_native_all_selectors_weighted_and_graph_inputs() {
  const auto weighted = parse_inductive_variant_log_json(
      R"([{"activities":["a","b"],"count":18446744073709551615},{"activities":["unused"],"count":0}])");
  const auto constructor = parse_inductive_dfg_input_json(
      R"({"activities":["b","a"],"edges":[{"source":1,"target":0,"count":18446744073709551615}],"start_counts":[0,18446744073709551615],"end_counts":[18446744073709551615,0]})");
  const auto graph = DirectlyFollowsGraph::from_frequencies(constructor.activities, constructor.edges, constructor.start_counts, constructor.end_counts);
  std::string graph_tree;
  for (const auto selector : {InductiveMinerVariant::IM, InductiveMinerVariant::IMF, InductiveMinerVariant::IMD}) {
    InductiveMinerOptions options; options.variant = selector;
    const auto tree = discover_process_tree(weighted, options);
    const auto encoded = serialize_inductive_process_tree_json(tree);
    assert(tree.activities() == std::vector<std::string>({"a", "b", "unused"}));
    contains(encoded, R"("op":"SEQUENCE","op_index":2)");
    contains(encoded, R"("op":"ACTIVITY","op_index":0,"activity":0,"children":[])");
    contains(encoded, R"("op":"ACTIVITY","op_index":0,"activity":1,"children":[])");
    const auto direct = serialize_inductive_process_tree_json(discover_process_tree(graph, options));
    if (graph_tree.empty()) graph_tree = direct; else assert(graph_tree == direct);
    contains(direct, R"("activities":["b","a"])");
    const auto empty = serialize_inductive_process_tree_json(discover_process_tree(InductiveMinerVariantLog{{{}, 0}}, options));
    contains(empty, R"("op":"TAU","op_index":1)");
  }
  equal_variants(weighted, parse_inductive_variant_log_json(serialize_inductive_variant_log_json(weighted)));
}

void complete_tree_preserves_every_node_and_registry_field() {
  ProcessTreeNode root{ProcessTreeOperator::kLoop, std::numeric_limits<std::size_t>::max(), {}};
  const std::vector<ProcessTreeOperator> operators{ProcessTreeOperator::kActivity, ProcessTreeOperator::kTau,
      ProcessTreeOperator::kSequence, ProcessTreeOperator::kExclusive, ProcessTreeOperator::kInclusiveChoice,
      ProcessTreeOperator::kParallel, ProcessTreeOperator::kInterleaving, ProcessTreeOperator::kLoop};
  const std::vector<std::string> names{"ACTIVITY", "TAU", "SEQUENCE", "EXCLUSIVE", "INCLUSIVE_CHOICE", "PARALLEL", "INTERLEAVING", "LOOP"};
  for (std::size_t i = 0; i < operators.size(); ++i) root.children.push_back({operators[i], 100 + i, {ProcessTreeNode::tau()}});
  root.children.push_back({static_cast<ProcessTreeOperator>(-9), 999, {}});
  ProcessTree tree({"", " duplicate ", " duplicate ", "\xed\xa0\x80", std::string("n\0", 2)}, std::move(root));
  const auto encoded = serialize_inductive_process_tree_json(tree);
  contains(encoded, R"("activities":[""," duplicate "," duplicate ","\ud800","n\u0000"])");
  contains(encoded, "\"root\":{\"op\":\"LOOP\",\"op_index\":7,\"activity\":" + std::to_string(std::numeric_limits<std::size_t>::max()));
  std::size_t previous = 0;
  for (std::size_t i = 0; i < names.size(); ++i) {
    const auto identity = "\"op\":\"" + names[i] + "\",\"op_index\":" + std::to_string(i) + ",\"activity\":" + std::to_string(100 + i);
    const auto position = encoded.find(identity); assert(position != std::string::npos && position >= previous); previous = position;
  }
  contains(encoded, R"("op":"UNKNOWN","op_index":-9,"activity":999,"children":[])");
  const auto default_tree = serialize_inductive_process_tree_json(ProcessTree{});
  assert(default_tree == R"({"activities":[],"root":{"op":"TAU","op_index":1,"activity":0,"children":[]}})");
}

void complete_options_retain_native_storage_and_precision() {
  const auto defaults = serialize_inductive_options_json({});
  assert(defaults == R"({"noise_threshold":0.0,"variant":"IM","variant_index":0,"activity_key":"concept:name","timestamp_key":"time:timestamp","case_id_key":"case:concept:name","sort_events_by_timestamp":false,"disable_strict_sequence_cut":false,"multiprocessing":false,"disable_fallthroughs":false})");
  for (int i = 0; i < 3; ++i) {
    InductiveMinerOptions options; options.variant = static_cast<InductiveMinerVariant>(i);
    contains(serialize_inductive_options_json(options), "\"variant_index\":" + std::to_string(i));
    contains(serialize_inductive_options_json(options), "\"variant\":\"" + std::string(i == 0 ? "IM" : i == 1 ? "IMF" : "IMD") + '"');
  }
  InductiveMinerOptions options; options.variant = static_cast<InductiveMinerVariant>(-7);
  options.activity_key = " phase "; options.timestamp_key = "\xed\xbf\xbf"; options.case_id_key = std::string("n\0", 2);
  options.sort_events_by_timestamp = true; options.disable_strict_sequence_cut = true;
  options.multiprocessing = true; options.disable_fallthroughs = true;
  for (const auto value : {std::nextafter(0.1, 1.0), std::numeric_limits<double>::max(), std::numeric_limits<double>::denorm_min(), -0.0}) {
    options.noise_threshold = value;
    const auto encoded = serialize_inductive_options_json(options);
    const auto begin = encoded.find(':') + 1, end = encoded.find(',', begin);
    const auto token = encoded.substr(begin, end - begin); char* parsed_end = nullptr;
    const auto parsed = std::strtod(token.c_str(), &parsed_end);
    assert(parsed_end == token.c_str() + token.size() && parsed == value && std::signbit(parsed) == std::signbit(value));
    contains(encoded, R"("variant":"UNKNOWN","variant_index":-7)");
    contains(encoded, R"("activity_key":" phase ","timestamp_key":"\udfff","case_id_key":"n\u0000")");
    contains(encoded, R"("sort_events_by_timestamp":true,"disable_strict_sequence_cut":true,"multiprocessing":true,"disable_fallthroughs":true)");
  }
  for (const auto& [value, tag] : std::vector<std::pair<double, std::string>>{
      {std::numeric_limits<double>::quiet_NaN(), "NaN"}, {std::numeric_limits<double>::infinity(), "INF"}, {-std::numeric_limits<double>::infinity(), "-INF"}}) {
    options.noise_threshold = value; contains(serialize_inductive_options_json(options), "\"noise_threshold\":\"" + tag + '"');
  }
}

void bounded_inputs_and_unbounded_native_outputs() {
  const InductiveMinerVariantLog huge{{{std::string(32u * 1024u * 1024u, 'x')}, 0}};
  const auto large = serialize_inductive_variant_log_json(huge);
  assert(large.size() > 32u * 1024u * 1024u);
  rejects(parse_inductive_variant_log_json, {large});
  std::string many = R"([{"activities":[)";
  for (std::size_t i = 0; i < 1000000; ++i) { if (i) many += ','; many += "\"\""; }
  many += R"(],"count":0}])";
  rejects(parse_inductive_variant_log_json, {many});
  const auto deep_input = std::string(66, '[') + "0" + std::string(66, ']');
  rejects(parse_inductive_variant_log_json, {deep_input});
  ProcessTreeNode node = ProcessTreeNode::leaf(0);
  for (int i = 0; i < 100; ++i) { ProcessTreeNode parent{ProcessTreeOperator::kSequence, 0, {}}; parent.children.push_back(std::move(node)); node = std::move(parent); }
  const auto output = serialize_inductive_process_tree_json(ProcessTree({"\xed\xa0\x80"}, std::move(node)));
  contains(output, R"("op":"ACTIVITY","op_index":0,"activity":0,"children":[])");
  contains(output, R"(\ud800)");
  std::size_t count = 0, at = 0;
  while ((at = output.find("\"op\":\"SEQUENCE\"", at)) != std::string::npos) { ++count; ++at; }
  assert(count == 100);
}
}  // namespace

int main() {
  weighted_variants_preserve_order_repetitions_and_exact_counts();
  strict_weighted_schema_integer_grammar_and_duplicate_keys();
  constructor_graph_preserves_every_declared_field_and_zero_edge();
  strict_graph_schema_and_native_constructor_validation();
  actual_graph_observations_are_not_reconstructed_from_frequencies();
  real_native_all_selectors_weighted_and_graph_inputs();
  complete_tree_preserves_every_node_and_registry_field();
  complete_options_retain_native_storage_and_precision();
  bounded_inputs_and_unbounded_native_outputs();
}
