#include "minera/decision_workbench_json.hpp"
#include "minera/workflow_input_result_json.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace minera;

template <class Action>
void rejects(Action action) {
  bool rejected = false;
  try { action(); } catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
}

void contains(const std::string& actual, std::string_view expected) {
  if (actual.find(expected) == std::string::npos)
    std::cerr << "Missing JSON fragment: " << expected << "\nActual: " << actual << std::endl;
  assert(actual.find(expected) != std::string::npos);
}

template <class Options, class Parse>
void round_trip(const Options& source, Parse parse, std::size_t count) {
  const auto text = serialize_decision_workbench_options_json(source);
  std::set<std::string> supplied{"stale"};
  const auto parsed = parse(text, &supplied);
  assert(supplied.size() == count);
  assert(serialize_decision_workbench_options_json(parsed) == text);
  (void)parse("{}", &supplied);
  assert(supplied.empty());
}

PetriNet decision_net() {
  PetriNet net;
  const auto source = net.add_place(), after_a = net.add_place();
  const auto choice = net.add_place(), sink = net.add_place();
  net.set_source(source); net.set_sink(sink);
  net.set_activities({"A", "Prepare", "B", "C"});
  net.add_transition(0, {source}, {after_a});
  net.add_transition(1, {after_a}, {choice});
  net.add_transition(2, {choice}, {sink});
  net.add_transition(3, {choice}, {sink});
  return net;
}

EventLog decision_log() {
  EventLog log;
  const std::vector<int> amounts{10, 20, 90, 100};
  for (std::size_t i = 0; i < amounts.size(); ++i) {
    Trace trace;
    trace.case_id = "case_" + std::to_string(i);
    trace.attributes = {{"customer", "vip"}};
    Event first, before, branch;
    first.activity = "A"; first.attributes = {{"amount", std::to_string(amounts[i] - 1)}};
    before.activity = "Prepare"; before.attributes = {{"amount", std::to_string(amounts[i])}};
    branch.activity = i < 2 ? "B" : "C"; branch.attributes = {{"amount", "999"}};
    trace.events = {first, before, branch};
    log.traces.push_back(trace);
  }
  return log;
}

void native_defaults_and_transactional_nested_presence() {
  const auto mining = parse_decision_workbench_options("{}");
  assert(mining.max_depth == 4 && mining.min_split == 10 && mining.min_leaf == 5);
  assert(mining.max_categories == 16 && mining.max_thresholds == 32 && mining.min_gain == 1e-9);
  assert(mining.include_trace_attributes && mining.attributes.empty() && mining.activity_attribute == "concept:name");
  const auto table = parse_decision_table_workbench_options("{}");
  assert(table.mining.include_trace_attributes && !table.include_trace_attributes);
  assert(table.mining.attributes.empty() && table.attributes.empty() && table.lookback == 1);
  const auto guarded = parse_data_petri_workbench_options("{}");
  assert(guarded.include_decision_diagnostics && guarded.decision_options.include_trace_attributes);
  assert(guarded.decision_table_lookback == 1);
  round_trip(mining, parse_decision_workbench_options, 9);
  round_trip(table, parse_decision_table_workbench_options, 13);
  round_trip(guarded, parse_data_petri_workbench_options, 12);
  std::set<std::string> supplied;
  (void)parse_decision_table_workbench_options(R"({"mining":{},"attributes":[],"include_trace_attributes":false})", &supplied);
  assert(supplied == std::set<std::string>({"mining", "attributes", "include_trace_attributes"}));
  const auto selected = parse_decision_table_workbench_options(
      R"({"mining":{"attributes":["inner"],"include_trace_attributes":false},"attributes":["outer"],"include_trace_attributes":true})", &supplied);
  assert(selected.mining.attributes == std::vector<std::string>{"inner"});
  assert(selected.attributes == std::vector<std::string>{"outer"});
  assert(!selected.mining.include_trace_attributes && selected.include_trace_attributes);
  assert(supplied == std::set<std::string>({"mining", "mining.attributes", "mining.include_trace_attributes", "attributes", "include_trace_attributes"}));
  (void)parse_data_petri_workbench_options(R"({"decision_options":{"max_depth":0}})", &supplied);
  assert(supplied == std::set<std::string>({"decision_options", "decision_options.max_depth"}));
  supplied = {"unchanged"};
  rejects([&] { (void)parse_decision_table_workbench_options(R"({"mining":{"min_leaf":0},"lookback":false})", &supplied); });
  assert(supplied == std::set<std::string>{"unchanged"});
  rejects([&] { (void)parse_data_petri_workbench_options(R"({"decision_options":{"unexpected":0}})", &supplied); });
  assert(supplied == std::set<std::string>{"unchanged"});
}

void exact_storage_domains_and_literal_attributes() {
  DecisionMiningOptions source;
  const auto max = std::numeric_limits<std::size_t>::max();
  source.max_depth = max; source.min_split = max - 1; source.min_leaf = max - 2;
  source.max_categories = max - 3; source.max_thresholds = max - 4;
  source.min_gain = -0.0; source.include_trace_attributes = false;
  source.attributes = {"", " a,b ", "duplicate", "duplicate", std::string("x\0y", 3), std::string("\xed\xa0\x80", 3)};
  source.activity_attribute = std::string("\xed\xb0\x80", 3);
  round_trip(source, parse_decision_workbench_options, 9);
  const auto parsed = parse_decision_workbench_options(serialize_decision_workbench_options_json(source));
  assert(parsed.max_depth == max && parsed.min_split == max - 1 && parsed.min_leaf == max - 2);
  assert(parsed.max_categories == max - 3 && parsed.max_thresholds == max - 4);
  assert(parsed.attributes == source.attributes && parsed.activity_attribute == source.activity_attribute);
  assert(std::signbit(parsed.min_gain));
  DecisionTableOptions table;
  table.mining = source; table.include_trace_attributes = true;
  table.lookback = max; table.attributes = {"outer", "", " ", "x,y"};
  round_trip(table, parse_decision_table_workbench_options, 13);
  DataPetriNetOptions guarded;
  guarded.include_decision_diagnostics = false; guarded.decision_options = source; guarded.decision_table_lookback = max;
  round_trip(guarded, parse_data_petri_workbench_options, 12);
  const auto literal = parse_decision_workbench_options(R"({"activity_attribute":"\ud83d\ude00","attributes":["\ud800","\udc00","\u0000\n\t","", " a,b "]})");
  assert(literal.activity_attribute == std::string("\xf0\x9f\x98\x80", 4));
  assert(literal.attributes[0] == std::string("\xed\xa0\x80", 3));
  assert(literal.attributes[1] == std::string("\xed\xb0\x80", 3));
  assert(literal.attributes[2] == std::string("\0\n\t", 3));
  for (const auto* tag : {"NaN", "INF", "-INF"}) {
    const auto text = std::string("{\"min_gain\":\"") + tag + "\"}";
    const auto ieee = parse_decision_workbench_options(text);
    contains(serialize_decision_workbench_options_json(ieee), std::string("\"min_gain\":\"") + tag + '"');
  }
  assert(parse_decision_workbench_options(R"({"min_gain":4.9406564584124654e-324})").min_gain == std::numeric_limits<double>::denorm_min());
  assert(std::signbit(parse_decision_workbench_options(R"({"min_gain":-1e-999})").min_gain));
  // Transport retains native-invalid values so the actual algorithm reports
  // its own error instead of a clamped/defaulted request.
  rejects([&] { (void)mine_decisions(decision_log(), decision_net(), parsed); });
}

void strict_json_does_not_coerce_or_accept_unknown_fields() {
  for (const auto* text : {"[]", "null", "{}x", R"({"max_depth":1,"max_depth":2})",
      R"({"max_depth":1,"\u006dax_depth":2})", R"({"max_depth":18446744073709551616})",
      R"({"max_depth":1.0})", R"({"max_depth":1e2})", R"({"max_depth":-1})",
      R"({"max_depth":-0})", R"({"max_depth":"4"})", R"({"max_depth":null})",
      R"({"min_gain":1e999})", R"({"min_gain":"nan"})", R"({"min_gain":false})",
      R"({"include_trace_attributes":1})", R"({"attributes":null})", R"({"attributes":[0]})",
      R"({"activity_attribute":false})", R"({"unused":0})", R"({"activity_attribute":"\uZZZZ"})"})
    rejects([&] { (void)parse_decision_workbench_options(text); });
  for (const auto* text : {R"({"mining":null})", R"({"mining":[]})", R"({"mining":{"unknown":0}})",
      R"({"mining":{"max_depth":1,"max_depth":2}})", R"({"lookback":true})", R"({"attributes":{}})",
      R"({"min_gain":0})"})
    rejects([&] { (void)parse_decision_table_workbench_options(text); });
  for (const auto* text : {R"({"decision_options":null})", R"({"decision_table_lookback":1.1})",
      R"({"include_decision_diagnostics":"false"})", R"({"mining":{}})"})
    rejects([&] { (void)parse_data_petri_workbench_options(text); });
  rejects([] { (void)parse_decision_workbench_options(std::string("{\"activity_attribute\":\"") + char(0xff) + "\"}"); });
  rejects([] { (void)parse_decision_workbench_options(std::string("{\"activity_attribute\":\"") + std::string("\xed\xa0\x80", 3) + "\"}"); });
}

void native_decisions_tables_and_guards_preserve_independent_controls() {
  const auto net = decision_net();
  const auto original_net = workflow_input_petri_json(net);
  const auto log = decision_log();
  const auto mining = parse_decision_workbench_options(R"({"min_split":2,"min_leaf":1,"attributes":["amount"],"include_trace_attributes":false})");
  const auto decisions = mine_decisions(log, net, mining);
  assert(decisions.miner.empty() && decisions.observed_traces == 4 && decisions.replayed_traces == 4);
  assert(decisions.aligned_traces == 0 && decisions.alignment_failures == 0 && decisions.trace_count == 4);
  assert(decisions.decision_points.size() == 1 && decisions.decision_points[0].nodes.size() == 3);
  assert(decisions.decision_points[0].nodes[0].split.key == "amount");
  assert(decisions.decision_points[0].accuracy == 1);
  contains(serialize_decision_workbench_result_json(decisions), R"("miner":"")");
  const auto table_options = parse_decision_table_workbench_options(R"({"mining":{"attributes":["customer"],"include_trace_attributes":true},"include_trace_attributes":false,"attributes":["amount"],"lookback":2})");
  const auto tables = mine_decision_tables(log, net, table_options);
  assert(tables.tables.size() == 1 && tables.tables[0].rows.size() == 8);
  assert(tables.tables[0].feature_names == std::vector<std::string>{"amount"});
  for (const auto& row : tables.tables[0].rows) {
    assert(row.features.size() == 1 && row.features[0]);
    assert(*row.features[0] != 999);
  }
  const auto case_options = parse_decision_table_workbench_options(R"({"mining":{"attributes":["amount"],"include_trace_attributes":false},"include_trace_attributes":true,"attributes":["customer"]})");
  const auto case_table = mine_decision_tables(log, net, case_options);
  assert(case_table.tables[0].feature_names == std::vector<std::string>{"customer=vip"});
  const auto empty_outer = parse_decision_table_workbench_options(R"({"attributes":[""]})");
  const auto no_columns = mine_decision_tables(log, net, empty_outer);
  assert(no_columns.tables[0].feature_names.empty() && no_columns.tables[0].rows.size() == 4);
  auto sparse_log = log;
  sparse_log.traces[0].events[1].attributes.clear();
  const auto sparse_options = parse_decision_table_workbench_options(R"({"attributes":["amount"]})");
  const auto sparse = mine_decision_tables(sparse_log, net, sparse_options);
  assert(sparse.tables[0].rows[0].features.size() == 1 && !sparse.tables[0].rows[0].features[0]);
  const auto invalid_inner = parse_decision_table_workbench_options(R"({"mining":{"attributes":[""]},"attributes":["amount"]})");
  rejects([&] { (void)mine_decision_tables(log, net, invalid_inner); });
  const auto empty_activity = parse_decision_workbench_options(R"({"activity_attribute":""})");
  const auto canonical_activity = mine_decisions(log, net, empty_activity);
  assert(canonical_activity.activity_attribute == "concept:name" && canonical_activity.observed_traces == 4);
  const auto guarded_options = parse_data_petri_workbench_options(R"({"include_decision_diagnostics":false,"decision_options":{"min_split":2,"min_leaf":1,"attributes":["amount"],"include_trace_attributes":false},"decision_table_lookback":2})");
  const auto guarded = mine_data_petri_net(log, net, guarded_options);
  assert(guarded.decisions.decision_points.empty() && guarded.decisions.observed_traces == 4);
  assert(guarded.decision_tables.tables[0].rows.size() == 8 && guarded.guarded_transitions.size() == 2);
  for (const auto& guard : guarded.guarded_transitions) {
    assert(!guard.guard.empty() && guard.read_variables == std::vector<std::string>{"amount"});
    assert(guard.write_variables.empty());
  }
  assert(workflow_input_petri_json(guarded.net) == original_net && workflow_input_petri_json(net) == original_net);
  contains(serialize_decision_workbench_result_json(guarded), "\"net\":" + original_net);
  contains(serialize_decision_workbench_result_json(guarded), "\"decision_tables\":" + serialize_decision_workbench_result_json(guarded.decision_tables));
  const auto bad_lookback = parse_decision_table_workbench_options(R"({"lookback":0})");
  rejects([&] { (void)mine_decision_tables(log, net, bad_lookback); });
  const auto bad_gain = parse_decision_workbench_options(R"({"min_gain":"NaN"})");
  rejects([&] { (void)mine_decisions(log, net, bad_gain); });
}

void complete_native_result_storage_and_raw_net() {
  const auto max = std::numeric_limits<std::uint64_t>::max();
  DecisionMiningResult result;
  result.miner = std::string("\xed\xa0\x80", 3); result.observed_traces = max;
  result.replayed_traces = max - 1; result.aligned_traces = 3;
  result.alignment_failures = 4; result.trace_count = 5; result.activity_attribute = "";
  DecisionPoint point;
  point.place = std::numeric_limits<std::size_t>::max(); point.branches = {"", "dup", "dup"};
  point.observations = max; point.accuracy = -0.0; point.majority_share = std::numeric_limits<double>::infinity();
  DecisionNode node;
  node.branch_counts = {max, 0, 9007199254740993ull};
  node.split = {std::string("k\0x", 3), false, " unused value ", std::numeric_limits<double>::quiet_NaN()};
  node.then_child = std::numeric_limits<int>::min(); node.else_child = std::numeric_limits<int>::max();
  point.nodes = {node, DecisionNode{}}; result.decision_points = {point};
  const auto json = serialize_decision_workbench_result_json(result);
  contains(json, R"("observed_traces":18446744073709551615,"replayed_traces":18446744073709551614)");
  contains(json, R"("branch_counts":[18446744073709551615,0,9007199254740993])");
  contains(json, R"("numeric":false,"value":" unused value ","threshold":"NaN")");
  contains(json, R"("then_child":-2147483648,"else_child":2147483647)");
  contains(json, R"("then_child":-1,"else_child":-1)");
  contains(json, R"("accuracy":-0.0,"majority_share":"INF")");
  contains(json, R"("activity_attribute":"")");
  DecisionTablesResult tables;
  tables.observed_traces = max; tables.replayed_traces = 1; tables.aligned_traces = 2;
  tables.alignment_failures = 3; tables.trace_count = 4; tables.activity_attribute = " a,b ";
  EncodedDecisionTable table;
  table.place = std::numeric_limits<std::size_t>::max(); table.classes = {"dup", "dup", ""};
  table.feature_names = {"", " f ", "x,y", std::string("a\0b", 3)};
  table.rows = {{{std::nullopt, -0.0, std::numeric_limits<double>::quiet_NaN(),
      -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::denorm_min()}, std::numeric_limits<std::size_t>::max()}, {{}, 0}};
  tables.tables = {table};
  const auto table_json = serialize_decision_workbench_result_json(tables);
  contains(table_json, R"("features":[null,-0.0,"NaN","-INF",)");
  contains(table_json, R"({"features":[],"target":0})");
  contains(table_json, "\"target\":" + std::to_string(std::numeric_limits<std::size_t>::max()));
  DataPetriNet guarded;
  guarded.variant = ""; guarded.net = decision_net();
  guarded.net.set_transition_names({std::nullopt, "", "literal", std::string("\xed\xb0\x80", 3)});
  guarded.net.set_ordinary_arc_weights(0, {{TokenInteger(9007199254740993ull), true}}, {{0, false}});
  guarded.decisions = result; guarded.decision_tables = tables;
  guarded.guarded_transitions = {{std::numeric_limits<std::size_t>::max(), std::string("g\0x", 3), {"dup", "dup", ""}, {"write", "write"}}, {0, "", {}, {}}};
  const auto guarded_json = serialize_decision_workbench_result_json(guarded);
  contains(guarded_json, R"("variant":"")");
  contains(guarded_json, "\"net\":" + workflow_input_petri_json(guarded.net));
  contains(guarded_json, "\"decisions\":" + json);
  contains(guarded_json, "\"decision_tables\":" + table_json);
  contains(guarded_json, R"("read_variables":["dup","dup",""],"write_variables":["write","write"])");
  contains(guarded_json, R"({"transition_index":0,"guard":"","read_variables":[],"write_variables":[]})");
}

}  // namespace

int main() {
  const auto run = [](const char* name, auto action) {
    std::cerr << "Checking " << name << std::endl;
    try { action(); }
    catch (const std::exception& error) {std::cerr << name << ": " << error.what() << std::endl; throw;}
  };
  run("native defaults and nested presence", native_defaults_and_transactional_nested_presence);
  run("exact storage and literal attributes", exact_storage_domains_and_literal_attributes);
  run("strict JSON transport", strict_json_does_not_coerce_or_accept_unknown_fields);
  run("native decisions, tables and guards", native_decisions_tables_and_guards_preserve_independent_controls);
  run("complete result storage and raw net", complete_native_result_storage_and_raw_net);
}
