#undef NDEBUG

#include "minera/process_tree_alignment_workbench_json.hpp"
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

template <class Action>
void rejects(Action action) {
  bool threw = false;
  try { action(); } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

std::string input_with_node(std::string node, std::string activities = "[]") {
  return "{\"activities\":" + activities + ",\"root\":" + node + '}';
}

void equal_node(const ProcessTreeNode& left, const ProcessTreeNode& right) {
  assert(left.op == right.op && left.activity == right.activity && left.children.size() == right.children.size());
  for (std::size_t i = 0; i < left.children.size(); ++i) equal_node(left.children[i], right.children[i]);
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

ProcessTree sequence() {
  return ProcessTree({"a", "b"}, {ProcessTreeOperator::kSequence, 987,
      {ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}});
}

void complete_constructor_state_round_trips_without_semantic_narrowing() {
  ProcessTreeNode root{ProcessTreeOperator::kLoop, std::numeric_limits<std::size_t>::max(), {}};
  for (int i = 0; i < 8; ++i)
    root.children.push_back({static_cast<ProcessTreeOperator>(i), 100 + static_cast<std::size_t>(i), {ProcessTreeNode::tau()}});
  root.children.push_back({static_cast<ProcessTreeOperator>(std::numeric_limits<int>::min()), 17, {}});
  root.children.push_back({static_cast<ProcessTreeOperator>(std::numeric_limits<int>::max()), 0, {}});
  ProcessTree original({"", " a\t\n ", "same", "same", std::string("n\0", 2), "\xed\xa0\x80", "\xed\xbf\xbf", "\xf0\x9f\x98\x80"}, std::move(root));
  const auto encoded = serialize_process_tree_alignment_input_json(original);
  assert(encoded == serialize_inductive_process_tree_json(original));
  const auto parsed = parse_process_tree_alignment_input_json(encoded);
  assert(parsed.activities() == original.activities()); equal_node(parsed.root(), original.root());
  const std::string escaped_high = R"(\ud800)";
  const std::string escaped_low = R"(\udfff)";
  contains(encoded, escaped_high); contains(encoded, escaped_low);
  contains(encoded, "\"op\":\"UNKNOWN\",\"op_index\":" + std::to_string(std::numeric_limits<int>::min()));
  assert(encoded.find("\xed\xa0\x80") == std::string::npos);
  const auto empty = parse_process_tree_alignment_input_json(serialize_process_tree_alignment_input_json(ProcessTree{}));
  assert(empty.activities().empty() && empty.root().op == ProcessTreeOperator::kTau && empty.root().children.empty());

  // The constructor accepts invalid leaves and empty operators; native
  // alignment remains responsible for rejecting its own semantic constraints.
  const auto invalid_leaf = parse_process_tree_alignment_input_json(input_with_node(
      R"({"op":"ACTIVITY","op_index":0,"activity":99,"children":[]})", R"(["a"])") );
  assert(invalid_leaf.root().activity == 99);
  rejects([&] { (void)align_process_tree(log_of({{"a"}}), invalid_leaf); });
  const auto empty_sequence = parse_process_tree_alignment_input_json(input_with_node(
      R"({"op":"SEQUENCE","op_index":2,"activity":123,"children":[]})"));
  assert(empty_sequence.root().activity == 123 && empty_sequence.root().children.empty());
  rejects([&] { (void)align_process_tree(log_of({{}}), empty_sequence); });
  ProcessTreeNode nary_loop{ProcessTreeOperator::kLoop, 31,
      {ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(0)}};
  const auto nary = parse_process_tree_alignment_input_json(serialize_process_tree_alignment_input_json(ProcessTree({"a"}, nary_loop)));
  ProcessTreeAlignmentOptions milp; milp.variant = ProcessTreeAlignmentVariant::kMilp;
  rejects([&] { (void)align_process_tree(log_of({{"a"}}), nary, milp); });
  ProcessTreeNode interleaving{ProcessTreeOperator::kInterleaving, 0, std::vector<ProcessTreeNode>(9, ProcessTreeNode::leaf(0))};
  const auto wide = parse_process_tree_alignment_input_json(serialize_process_tree_alignment_input_json(ProcessTree({"a"}, std::move(interleaving))));
  assert(wide.root().children.size() == 9);
  rejects([&] { (void)align_process_tree(log_of({{"a"}}), wide); });
}

void strict_schema_numbers_duplicate_keys_and_unicode() {
  const std::vector<std::string> invalid_documents{
      "", "null", "[]", "0", "{}", R"({"activities":[]})",
      R"({"activities":null,"root":{"op":"TAU","op_index":1,"activity":0,"children":[]}})",
      R"({"activities":[null],"root":{"op":"TAU","op_index":1,"activity":0,"children":[]}})",
      R"({"activities":[],"root":null})",
      R"({"activities":[],"root":{},"extra":0})",
      R"({"activities":[],"activit\u0069es":[],"root":{}})"};
  for (const auto& input : invalid_documents) rejects([&] { (void)parse_process_tree_alignment_input_json(input); });
  const std::vector<std::string> invalid_nodes{
      "{}", "[]", "0", "null", R"({"op":"TAU","op_index":1,"activity":0})",
      R"({"op":"TAU","op_index":1,"activity":0,"children":[],"extra":0})",
      R"({"op":"tau","op_index":1,"activity":0,"children":[]})",
      R"({"op":"UNKNOWN","op_index":1,"activity":0,"children":[]})",
      R"({"op":"LOOP","op_index":-1,"activity":0,"children":[]})",
      R"({"op":1,"op_index":1,"activity":0,"children":[]})",
      R"({"op":"TAU","op_index":1.0,"activity":0,"children":[]})",
      R"({"op":"TAU","op_index":1e0,"activity":0,"children":[]})",
      R"({"op":"TAU","op_index":"1","activity":0,"children":[]})",
      R"({"op":"TAU","op_index":true,"activity":0,"children":[]})",
      R"({"op":"UNKNOWN","op_index":2147483648,"activity":0,"children":[]})",
      R"({"op":"UNKNOWN","op_index":-2147483649,"activity":0,"children":[]})",
      R"({"op":"TAU","op_index":NaN,"activity":0,"children":[]})",
      R"({"op":"TAU","op_index":01,"activity":0,"children":[]})",
      R"({"op":"TAU","op_index":+1,"activity":0,"children":[]})",
      R"({"op":"TAU","op_index":1,"activity":-0,"children":[]})",
      R"({"op":"TAU","op_index":1,"activity":0.0,"children":[]})",
      R"({"op":"TAU","op_index":1,"activity":18446744073709551616,"children":[]})",
      R"({"op":"TAU","op_index":1,"activity":"0","children":[]})",
      R"({"op":"TAU","op_index":1,"activity":null,"children":[]})",
      R"({"op":"TAU","op_index":1,"activity":0,"children":null})",
      R"({"op":"TAU","op_index":1,"activity":0,"children":[null]})",
      R"({"op":"TAU","op_index":1,"activity":0,"children":[],"op_\u0069ndex":1})"};
  for (const auto& node : invalid_nodes) rejects([&] { (void)parse_process_tree_alignment_input_json(input_with_node(node)); });
  const auto valid = serialize_process_tree_alignment_input_json(ProcessTree{});
  rejects([&] { (void)parse_process_tree_alignment_input_json(valid + "false"); });
  rejects([&] { (void)parse_process_tree_alignment_input_json(valid + ','); });
  const auto tau = R"({"op":"TAU","op_index":1,"activity":0,"children":[]})";
  for (const auto& word : {std::string("\xed\xa0\x80"), std::string("\xc0\x80"), std::string("\xf4\x90\x80\x80"), std::string("x\n")})
    rejects([&] { (void)parse_process_tree_alignment_input_json(input_with_node(tau, "[\"" + word + "\"]")); });
  const auto escaped = parse_process_tree_alignment_input_json(input_with_node(tau,
      R"(["\ud800","\udfff","\ud83d\ude00","x\u0000"," a:b,=\\\t "])") );
  assert(escaped.activities() == std::vector<std::string>({"\xed\xa0\x80", "\xed\xbf\xbf", "\xf0\x9f\x98\x80", std::string("x\0", 2), " a:b,=\\\t "}));
}

void real_native_all_variants_deviations_blank_and_null_sources() {
  for (const auto variant : {ProcessTreeAlignmentVariant::kSearchGraphPt, ProcessTreeAlignmentVariant::kDynamicProgramming,
                            ProcessTreeAlignmentVariant::kMilp, ProcessTreeAlignmentVariant::kApproximatedOriginal}) {
    ProcessTreeAlignmentOptions options; options.variant = variant;
    const auto tree = parse_process_tree_alignment_input_json(serialize_process_tree_alignment_input_json(sequence()));
    const auto result = align_process_tree(log_of({{"a", "b"}, {"a", "x", "b"}, {"b"}}), tree, options);
    assert(result.computed && result.completed && result.traces.size() == 3 && result.total_cost == 2);
    const auto encoded = serialize_process_tree_alignment_result_json(result);
    contains(encoded, "\"variant_index\":" + std::to_string(static_cast<int>(variant)));
    contains(encoded, R"("total_cost":2)"); contains(encoded, R"("trace":["a","x","b"])");
    contains(encoded, R"("type":"sync","type_index":0,"log_activity":"a","model_activity":"a","activity":"a")");
    contains(encoded, R"("type":"log","type_index":1,"log_activity":"x","model_activity":"","activity":"x")");
    contains(encoded, R"("type":"model","type_index":2,"log_activity":"","model_activity":"a","activity":"a")");
    assert(result.diagnostics.traces_aligned == 3);
    contains(encoded, "\"visited_states\":" + std::to_string(result.diagnostics.visited_states));
    contains(encoded, "\"queued_states\":" + std::to_string(result.diagnostics.queued_states));
    contains(encoded, "\"traversed_arcs\":" + std::to_string(result.diagnostics.traversed_arcs));
    contains(encoded, "\"lp_solved\":" + std::to_string(result.diagnostics.lp_solved));
    contains(encoded, R"("traces_aligned":3)");
    const auto blank = align_process_tree(log_of({{""}}), ProcessTree({""}, ProcessTreeNode::leaf(0)), options);
    assert(blank.computed && blank.total_cost == 0 && blank.traces.front().trace.front().empty());
    contains(serialize_process_tree_alignment_result_json(blank), R"("log_activity":"","model_activity":"","activity":"")");
    auto null_source = log_of({{"a"}}); null_source.traces[0].events[0].activity_state = FirstClassAttributeState::kNull;
    rejects([&] { (void)align_process_tree(null_source, tree, options); });
    auto selected = log_of({{"ignored"}}); selected.traces[0].events[0].attributes.push_back({" phase ", "a"});
    options.activity_attribute = " phase ";
    const auto literal = align_process_tree(selected, ProcessTree({"a"}, ProcessTreeNode::leaf(0)), options);
    assert(literal.total_cost == 0 && literal.traces.front().trace == std::vector<std::string>({"a"}));
    selected.traces[0].events[0].attributes.front().is_null = true;
    rejects([&] { (void)align_process_tree(selected, tree, options); });
    options.activity_attribute = ""; rejects([&] { (void)align_process_tree(selected, tree, options); });
  }
  ProcessTreeAlignmentOptions cap; cap.max_states = 1;
  const auto incomplete = align_process_tree(log_of({{"a", "x", "b"}}), sequence(), cap);
  assert(incomplete.exhausted && !incomplete.completed && !incomplete.traces.front().completed);
  const auto encoded = serialize_process_tree_alignment_result_json(incomplete);
  contains(encoded, R"("completed":false)"); contains(encoded, R"("exhausted":true)");
  contains(encoded, R"("trace":["a","x","b"])"); contains(encoded, R"("moves":[])");
}

void every_stored_result_field_nonfinite_and_large_integer_is_retained() {
  ProcessTreeAlignmentResult result; result.computed = false; result.completed = false;
  result.timed_out = true; result.exhausted = true; result.variant = static_cast<ProcessTreeAlignmentVariant>(-19);
  result.total_cost = std::numeric_limits<std::uint64_t>::max(); result.fitness = std::numeric_limits<double>::quiet_NaN();
  result.diagnostics = {std::numeric_limits<std::size_t>::max(), 9007199254740993ull, 31, 43, 59, std::string("n\0", 2) + "\xed\xa0\x80"};
  ProcessTreeTraceAlignment trace; trace.trace = {"", " a ", "\xed\xbf\xbf"};
  trace.moves = {{static_cast<ProcessTreeAlignmentMoveType>(std::numeric_limits<int>::min()), "", "\xed\xa0\x80", std::string("z\0", 2)}};
  trace.cost = std::numeric_limits<std::uint64_t>::max(); trace.fitness = -0.0; trace.completed = false; trace.timed_out = true;
  result.traces.push_back(trace); trace.fitness = std::numeric_limits<double>::infinity(); result.traces.push_back(trace);
  trace.fitness = -std::numeric_limits<double>::infinity(); result.traces.push_back(trace);
  const auto encoded = serialize_process_tree_alignment_result_json(result);
  contains(encoded, R"("computed":false,"completed":false,"timed_out":true,"exhausted":true,"variant":"UNKNOWN","variant_index":-19)");
  contains(encoded, R"("total_cost":18446744073709551615,"fitness":"NaN")");
  contains(encoded, "\"visited_states\":" + std::to_string(std::numeric_limits<std::size_t>::max()));
  contains(encoded, R"("queued_states":9007199254740993,"traversed_arcs":31,"lp_solved":43,"traces_aligned":59)");
  const std::string message = R"("message":"n\u0000\ud800")";
  const std::string move = R"("log_activity":"","model_activity":"\ud800","activity":"z\u0000")";
  contains(encoded, message); contains(encoded, move);
  contains(encoded, "\"type\":\"UNKNOWN\",\"type_index\":" + std::to_string(std::numeric_limits<int>::min()));
  contains(encoded, R"("cost":18446744073709551615,"fitness":-0.0,"completed":false,"timed_out":true)");
  contains(encoded, R"("fitness":"INF")"); contains(encoded, R"("fitness":"-INF")");
  assert(encoded.find("\xed\xa0\x80") == std::string::npos);
}

void all_options_and_exact_double_storage() {
  const auto defaults = serialize_process_tree_alignment_options_json({});
  assert(defaults == R"({"variant":"SEARCH_GRAPH_PT","variant_index":0,"max_states":0,"max_time_seconds":0.0,"max_total_time_seconds":0.0,"max_trace_length":1,"max_process_tree_height":1,"max_trace_time_seconds":0.0,"max_per_trace_time_seconds":0.0,"use_global_cache":false,"batch_size":1000,"clear_cache":true,"activity_attribute":"concept:name"})");
  const std::vector<std::string> names{"SEARCH_GRAPH_PT", "DYNAMIC_PROGRAMMING", "MILP", "APPROXIMATED_ORIGINAL"};
  for (int i = 0; i < 4; ++i) {
    ProcessTreeAlignmentOptions options; options.variant = static_cast<ProcessTreeAlignmentVariant>(i);
    contains(serialize_process_tree_alignment_options_json(options), "\"variant\":\"" + names[i] + "\",\"variant_index\":" + std::to_string(i));
  }
  ProcessTreeAlignmentOptions options; options.variant = static_cast<ProcessTreeAlignmentVariant>(-7);
  options.max_states = std::numeric_limits<std::size_t>::max(); options.max_trace_length = 9007199254740993ull;
  options.max_process_tree_height = 0; options.batch_size = std::numeric_limits<std::size_t>::max();
  options.use_global_cache = true; options.clear_cache = false; options.activity_attribute = " \t\xed\xa0\x80";
  options.max_total_time_seconds = std::numeric_limits<double>::infinity();
  options.max_trace_time_seconds = -std::numeric_limits<double>::infinity();
  options.max_per_trace_time_seconds = std::numeric_limits<double>::quiet_NaN();
  for (const auto value : {std::nextafter(0.1, 1.0), std::numeric_limits<double>::max(), std::numeric_limits<double>::denorm_min(), -0.0}) {
    options.max_time_seconds = value;
    const auto encoded = serialize_process_tree_alignment_options_json(options);
    const auto start = encoded.find("\"max_time_seconds\":") + std::string("\"max_time_seconds\":").size();
    const auto token = encoded.substr(start, encoded.find(',', start) - start); char* end = nullptr;
    const auto parsed = std::strtod(token.c_str(), &end);
    assert(end == token.c_str() + token.size() && parsed == value && std::signbit(parsed) == std::signbit(value));
    contains(encoded, R"("variant":"UNKNOWN","variant_index":-7)");
    contains(encoded, "\"max_states\":" + std::to_string(std::numeric_limits<std::size_t>::max()));
    contains(encoded, R"("max_total_time_seconds":"INF","max_trace_length":9007199254740993,"max_process_tree_height":0,"max_trace_time_seconds":"-INF","max_per_trace_time_seconds":"NaN","use_global_cache":true)");
    contains(encoded, "\"batch_size\":" + std::to_string(std::numeric_limits<std::size_t>::max()));
    const std::string literal = R"("clear_cache":false,"activity_attribute":" \t\ud800")";
    contains(encoded, literal);
  }
  options.activity_attribute.clear(); contains(serialize_process_tree_alignment_options_json(options), R"("activity_attribute":"")");
}

void only_inputs_have_transport_limits() {
  ProcessTreeNode node = ProcessTreeNode::tau();
  for (int i = 0; i < 100; ++i) node = {ProcessTreeOperator::kSequence, static_cast<std::size_t>(i), {std::move(node)}};
  const auto deep_output = serialize_process_tree_alignment_input_json(ProcessTree({}, std::move(node)));
  contains(deep_output, R"("activity":99)");
  rejects([&] { (void)parse_process_tree_alignment_input_json(deep_output); });
  auto large_label = std::string(32u * 1024u * 1024u, 'a');
  const auto large_output = serialize_process_tree_alignment_input_json(ProcessTree({large_label}, ProcessTreeNode::leaf(0)));
  assert(large_output.size() > 32u * 1024u * 1024u);
  rejects([&] { (void)parse_process_tree_alignment_input_json(large_output); });
  ProcessTreeAlignmentResult result; result.diagnostics.message = std::move(large_label);
  assert(serialize_process_tree_alignment_result_json(result).size() > 32u * 1024u * 1024u);
  std::string million = "[\"\"";
  for (std::size_t i = 1; i < 1000000; ++i) million += ",\"\"";
  million += ']';
  const auto excessive_values = input_with_node(R"({"op":"TAU","op_index":1,"activity":0,"children":[]})", std::move(million));
  rejects([&] { (void)parse_process_tree_alignment_input_json(excessive_values); });
}
}  // namespace

int main() {
  complete_constructor_state_round_trips_without_semantic_narrowing();
  strict_schema_numbers_duplicate_keys_and_unicode();
  real_native_all_variants_deviations_blank_and_null_sources();
  every_stored_result_field_nonfinite_and_large_integer_is_retained();
  all_options_and_exact_double_storage();
  only_inputs_have_transport_limits();
}
