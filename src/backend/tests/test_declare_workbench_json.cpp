#include "minera/declare_workbench_json.hpp"
#include "minera/log_conversion.hpp"

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
  try {action();} catch (const std::invalid_argument&) {rejected = true;}
  assert(rejected);
}
void contains(const std::string& text, std::string_view expected) {
  if (text.find(expected) == std::string::npos)
    std::cerr << "Missing: " << expected << "\nActual: " << text << std::endl;
  assert(text.find(expected) != std::string::npos);
}

DeclareModel one_rule(std::string family, DeclareConstraint rule) {
  DeclareModel result; result.rules.clear(); result.rules[std::move(family)].push_back(std::move(rule)); return result;
}

void exact_groups_records_and_all_eighteen_native_predicates() {
  const auto empty = parse_declare_workbench_model(R"({"rules":{}})");
  assert(empty.rules.empty() && empty.size() == 0);
  assert(serialize_declare_workbench_model_json(empty) == R"({"rules":{}})");
  DeclareModel defaults;
  assert(defaults.rules.size() == 18);
  assert(parse_declare_workbench_model(serialize_declare_workbench_model_json(defaults)).rules == defaults.rules);
  const auto raw = parse_declare_workbench_model(
      R"({"rules":{"":[],"unknown \ud800":[{"first":"A\u0000東京","second":"unused \udfff","binary":false,"support":-2147483648,"confidence":2147483647},{"first":"A\u0000東京","second":"unused \udfff","binary":false,"support":-2147483648,"confidence":2147483647}],"existence":[]}})");
  assert(raw.rules.size() == 3 && raw.rules.at("").empty() && raw.rules.at("existence").empty());
  const auto& duplicate = raw.rules.at(std::string("unknown \xed\xa0\x80"));
  assert(duplicate.size() == 2 && duplicate[0] == duplicate[1]);
  assert(duplicate[0].support == std::numeric_limits<int>::min() && duplicate[0].confidence == std::numeric_limits<int>::max());
  assert(duplicate[0].first == std::string("A\0東京", 8));
  const auto raw_json = serialize_declare_workbench_model_json(raw);
  assert(parse_declare_workbench_model(raw_json).rules == raw.rules);
  contains(raw_json, R"("second":"unused \udfff","binary":false)");

  struct Case {const char* family; bool binary; std::vector<std::string> fit, bad;};
  const Case cases[] = {
    {kDeclareExistence,false,{"A"},{}}, {kDeclareExactlyOne,false,{"A"},{"A","A"}},
    {kDeclareInit,false,{"A"},{"B"}}, {kDeclareAbsence,false,{"B"},{"A"}},
    {kDeclareRespondedExistence,true,{"A","B"},{"A"}}, {kDeclareResponse,true,{"A","B"},{"A"}},
    {kDeclarePrecedence,true,{"A","B"},{"B"}}, {kDeclareSuccession,true,{"A","B"},{"A"}},
    {kDeclareAltResponse,true,{"A","B"},{"A","A"}}, {kDeclareAltPrecedence,true,{"A","B"},{"B","B"}},
    {kDeclareAltSuccession,true,{"A","B"},{"A"}}, {kDeclareChainResponse,true,{"A","B"},{"A","C"}},
    {kDeclareChainPrecedence,true,{"A","B"},{"C","B"}}, {kDeclareChainSuccession,true,{"A","B"},{"A","C"}},
    {kDeclareCoexistence,true,{"A","B"},{"A"}}, {kDeclareNonCoexistence,true,{"A"},{"A","B"}},
    {kDeclareNonSuccession,true,{"B","A"},{"A","B"}}, {kDeclareNonChainSuccession,true,{"A","C","B"},{"A","B"}}
  };
  for (const auto& item : cases) {
    const auto model = one_rule(item.family, {"A", item.binary ? "B" : "unused", item.binary, -7, 99});
    const auto transported = parse_declare_workbench_model(serialize_declare_workbench_model_json(model));
    assert(transported.rules == model.rules);
    const auto fit = check_declare_conformance(item.fit, transported);
    const auto bad = check_declare_conformance(item.bad, transported);
    assert(fit.is_fit && fit.no_constr_total == 1 && fit.no_dev_total == 0);
    assert(!bad.is_fit && bad.no_constr_total == 1 && bad.no_dev_total == 1 && bad.deviations.size() == 1);
    assert(bad.deviations[0].template_name == item.family);
  }
}

void offline_arity_unknown_denominator_and_duplicates_are_not_streaming_validation() {
  const auto model = parse_declare_workbench_model(
      R"({"rules":{"existence":[{"first":"A","second":"retained unused","binary":true,"support":-1,"confidence":-2},{"first":"A","second":"retained unused","binary":true,"support":3,"confidence":4}],"unknown":[{"first":"X","second":"Y","binary":false,"support":0,"confidence":0}]}})");
  const auto result = check_declare_conformance(std::vector<std::string>{}, model);
  assert(result.no_constr_total == 3 && result.no_dev_total == 2 && result.deviations.size() == 2);
  assert(std::abs(result.dev_fitness - 1.0 / 3.0) < 1e-12);
  assert(result.deviations[0].binary && result.deviations[0].second == "retained unused");
  auto zero = parse_declare_playout_workbench_options(R"({"n_traces":0,"seed":0})");
  rejects([&] {(void)declare_playout(model, zero);});
  const auto bad_binary = one_rule(kDeclareResponse, {"A","B",false,0,0});
  assert(!check_declare_conformance(std::vector<std::string>{"A"}, bad_binary).is_fit);
  rejects([&] {(void)declare_playout(bad_binary, zero);});
}

void exact_options_defaults_presence_and_transport_domains() {
  std::set<std::string> supplied{"stale"};
  const auto conf = parse_declare_conformance_workbench_options("{}", &supplied);
  assert(supplied.empty() && conf.activity_key == "concept:name" && conf.case_id_key == "case:concept:name");
  const auto diagnostics = parse_declare_diagnostics_workbench_options("{}", &supplied);
  assert(supplied.empty() && diagnostics.case_id_key == "case:concept:name");
  const auto defaults = parse_declare_playout_workbench_options("{}", &supplied);
  assert(supplied.empty() && defaults.n_traces == 1000 && defaults.min_length == 3 && defaults.max_length == 15 && !defaults.seed);
  const auto full = parse_declare_playout_workbench_options(
      R"({"n_traces":18446744073709551615,"min_length":0,"max_length":18446744073709551615,"seed":18446744073709551615})", &supplied);
  assert(supplied.size() == 4 && *full.seed == std::numeric_limits<std::uint64_t>::max());
  assert(full.n_traces == std::numeric_limits<std::size_t>::max());
  assert(full.max_length == std::numeric_limits<std::size_t>::max());
  const auto encoded = serialize_declare_workbench_options_json(full);
  assert(serialize_declare_workbench_options_json(parse_declare_playout_workbench_options(encoded)) == encoded);
  const auto explicit_null = parse_declare_playout_workbench_options(R"({"seed":null})", &supplied);
  assert(!explicit_null.seed && supplied == std::set<std::string>{"seed"});
  assert(parse_declare_playout_workbench_options(R"({"seed":0})").seed == 0);
  const auto literal = parse_declare_conformance_workbench_options(R"({"activity_key":" a,\ud800\u0000 ","case_id_key":""})");
  assert(literal.case_id_key.empty());
  const auto literal_json = serialize_declare_workbench_options_json(literal);
  assert(serialize_declare_workbench_options_json(parse_declare_conformance_workbench_options(literal_json)) == literal_json);
  supplied = {"untouched"};
  rejects([&] {(void)parse_declare_playout_workbench_options(R"({"seed":false})", &supplied);});
  assert(supplied == std::set<std::string>{"untouched"});
  const char* invalid[] = {"null","[]","{}x",R"({"seed":1,"se\u0065d":2})",R"({"seed":18446744073709551616})",R"({"seed":-0})",R"({"seed":1.0})",R"({"seed":"1"})",R"({"n_traces":true})",R"({"n_traces":1e0})",R"({"unknown":0})"};
  for (const auto* input : invalid) rejects([&] {(void)parse_declare_playout_workbench_options(input);});
  for (const auto* input : {R"({"activity_key":null})",R"({"activity_key":false})",R"({"case_id_key":[]})",R"({"activity_key":"A","activity_key":"B"})"})
    rejects([&] {(void)parse_declare_conformance_workbench_options(input);});
  rejects([] {(void)parse_declare_diagnostics_workbench_options(R"({"activity_key":"A"})");});
  rejects([] {(void)parse_declare_conformance_workbench_options(std::string("{\"activity_key\":\"")+"\xed\xa0\x80"+"\"}");});
}

void full_raw_trace_constructor_round_trips_every_public_storage_field() {
  const auto trace = parse_declare_workbench_trace_json(
      R"({"case_id":" outer\u0000\ud800 ","case_id_state_index":2147483647,"case_id_source_key":" retained source ","case_id_attribute":{"key":"id","value":"projection mismatch","kind_index":-8,"container_index":-9,"null":true,"scalar_wrapper":true,"scalar_children_list":true,"first_class_role_index":-10,"first_class_state_index":-11,"first_class_source_key":"role-none-but-retained","typed_value":{"index":5,"value":18446744073709551615},"children":[{"key":"dup","typed_value":{"index":1,"value":-9223372036854775808}},{"key":"dup","typed_value":{"index":1,"value":9223372036854775807}}]},"attributes":[{"key":"none","typed_value":{"index":0,"value":null}},{"key":"signed","typed_value":{"index":1,"value":9007199254740993}},{"key":"nan","typed_value":{"index":2,"value":"NaN"}},{"key":"negativezero","typed_value":{"index":2,"value":-0.0}},{"key":"subnormal","typed_value":{"index":2,"value":4.9406564584124654e-324}},{"key":"infinity","typed_value":{"index":2,"value":"-INF"}},{"key":"bool","typed_value":{"index":3,"value":false}},{"key":"date","typed_value":{"index":4,"value":{"value":"raw instant\udfff","resolution_index":-2147483648,"timezone":""}}},{"key":"na","kind_index":0,"typed_value":{"index":6,"value":"NA"}}],"properties":[{"key":"p","value":" a,b ","children":[{"key":"","value":"child"}]}],"events":[{"case_id":"independent","activity":"A","timestamp":"not normalized","resource":"who","lifecycle":"phase","input_order":18446744073709551615,"attributes":[{"key":"x","value":"native string"}],"case_id_state_index":-7,"activity_state_index":2,"timestamp_state_index":1,"resource_state_index":2147483647,"lifecycle_state_index":-2147483648,"case_id_source_key":"case-source","activity_source_key":"act-source","timestamp_source_key":"time-source","resource_source_key":"resource-source","lifecycle_source_key":"life-source","activity_attribute":{"key":"selected","typed_value":{"index":5,"value":18446744073709551615}},"timestamp_attribute":{"typed_value":{"index":4,"value":{"value":"date","resolution_index":1,"timezone":null}}},"resource_attribute":{"typed_value":{"index":3,"value":true}},"lifecycle_attribute":{"value":"opaque"}},{"activity":"B"}]})");
  assert(trace.events.size() == 2 && trace.events[1].case_id.empty());
  assert(trace.events[1].input_order == 0 && trace.events[1].activity_state == FirstClassAttributeState::kMissing);
  assert(trace.events[0].input_order == std::numeric_limits<std::size_t>::max());
  assert(static_cast<int>(trace.events[0].lifecycle_state) == std::numeric_limits<int>::min());
  assert(trace.case_id_attribute->children.size() == 2);
  assert(std::get<std::uint64_t>(trace.case_id_attribute->typed_value) == std::numeric_limits<std::uint64_t>::max());
  assert(std::get<std::int64_t>(trace.attributes[1].typed_value) == INT64_C(9007199254740993));
  assert(std::isnan(std::get<double>(trace.attributes[2].typed_value)));
  assert(std::signbit(std::get<double>(trace.attributes[3].typed_value)));
  assert(std::get<double>(trace.attributes[4].typed_value) == std::numeric_limits<double>::denorm_min());
  const auto& date = std::get<NativeDate>(trace.attributes[7].typed_value);
  assert(static_cast<int>(date.resolution) == std::numeric_limits<int>::min() && date.timezone && date.timezone->empty());
  assert(std::holds_alternative<NativeNA>(trace.attributes[8].typed_value));
  const auto json = serialize_declare_workbench_trace_json(trace);
  assert(serialize_declare_workbench_trace_json(parse_declare_workbench_trace_json(json)) == json);
  contains(json,R"("typed_value":{"index":1,"value":9007199254740993})");
  contains(json,R"("typed_value":{"index":2,"value":-0.0})");
  contains(json,R"("container_index":-9)");
  contains(json,R"("first_class_source_key":"role-none-but-retained")");
  const auto empty = parse_declare_workbench_trace_json("{}");
  assert(empty.events.empty() && empty.attributes.empty() && !empty.case_id_attribute);
}

void raw_trace_types_and_model_types_are_strict_without_semantic_normalization() {
  const char* bad_models[] = {"{}","null",R"({"rules":[]})",R"({"rules":{},"extra":1})",R"({"rules":{},"rules":{}})",R"({"rules":{"a":null}})",R"({"rules":{"a":[{}]}})",R"({"rules":{"a":[{"first":"A","second":"B","binary":1,"support":0,"confidence":0}]}})",R"({"rules":{"a":[{"first":"A","second":"B","binary":false,"support":2147483648,"confidence":0}]}})",R"({"rules":{"a":[{"first":"A","second":"B","binary":false,"support":-2147483649,"confidence":0}]}})",R"({"rules":{"a":[{"first":"A","second":"B","binary":false,"support":1.0,"confidence":0}]}})"};
  for (const auto* input : bad_models) rejects([&] {(void)parse_declare_workbench_model(input);});
  const char* bad_traces[] = {"[]",R"({"case_id":null})",R"({"events":[{"input_order":-1}]})",R"({"events":[{"activity_state_index":2147483648}]})",R"({"attributes":null})",R"({"properties":[{"extra":1}]})",R"({"case_id_attribute":[]})",R"({"attributes":[{"typed_value":{"index":7,"value":0}}]})",R"({"attributes":[{"typed_value":{"index":0,"value":0}}]})",R"({"attributes":[{"typed_value":{"index":1,"value":9223372036854775808}}]})",R"({"attributes":[{"typed_value":{"index":1,"value":1e0}}]})",R"({"attributes":[{"typed_value":{"index":2,"value":1e9999}}]})",R"({"attributes":[{"typed_value":{"index":3,"value":0}}]})",R"({"attributes":[{"typed_value":{"index":4,"value":{"timezone":false}}}]})",R"({"attributes":[{"typed_value":{"index":5,"value":-1}}]})",R"({"attributes":[{"typed_value":{"index":6,"value":"<NA>"}}]})",R"({"events":[{"activity":"A","activity":"B"}]})"};
  for (const auto* input : bad_traces) rejects([&] {(void)parse_declare_workbench_trace_json(input);});
}

void actual_trace_all_native_shapes_and_diagnostics_remain_separate_calls() {
  const auto model = one_rule(kDeclareResponse,{"A","B",true,0,0});
  const auto trace = parse_declare_workbench_trace_json(R"({"case_id":"original","properties":[{"key":"p","value":"retained"}],"events":[{"activity":"ignored","attributes":[{"key":" literal task ","value":"A"}]},{"activity":"ignored","attributes":[{"key":" literal task ","value":"B"}]}]})");
  const auto before = serialize_declare_workbench_trace_json(trace);
  const auto options = parse_declare_conformance_workbench_options(R"({"activity_key":" literal task ","case_id_key":"ignored-in-grouped-trace"})");
  assert(check_declare_conformance(trace, model, options).is_fit);
  assert(serialize_declare_workbench_trace_json(trace) == before);
  EventLog log;
  for (const auto& labels : {std::vector<std::string>{"A","B"},std::vector<std::string>{"A"}}) {
    Trace row; row.case_id = "case_" + std::to_string(log.traces.size());
    for (const auto& activity : labels) row.events.push_back(Event{row.case_id,activity});
    log.traces.push_back(row);
  }
  const auto results = check_declare_conformance(log, model);
  assert(results.size() == 2 && results[0].is_fit && !results[1].is_fit);
  const auto expected = serialize_declare_workbench_conformance_json(results);
  const auto stream = event_log_to_event_stream(log);
  const auto table = event_log_to_table(log);
  const auto lazy = event_log_to_lazy_table(log);
  assert(serialize_declare_workbench_conformance_json(check_declare_conformance(stream,model)) == expected);
  assert(serialize_declare_workbench_conformance_json(check_declare_conformance(table,model)) == expected);
  assert(serialize_declare_workbench_conformance_json(check_declare_conformance(lazy,model)) == expected);
  for (const auto& view : {EventDataView(log),EventDataView(stream),EventDataView(table),EventDataView(lazy)})
    assert(serialize_declare_workbench_conformance_json(check_declare_conformance(view,model)) == expected);
  const auto diagnostics = declare_conformance_diagnostics(log,results);
  assert(diagnostics.rows.size() == 2 && diagnostics.columns == std::vector<std::string>({"case_id","no_dev_total","no_constr_total","dev_fitness"}));
  assert(std::holds_alternative<std::int64_t>(diagnostics.rows[0][1]->typed_value));
  assert(std::holds_alternative<double>(diagnostics.rows[0][3]->typed_value));
  rejects([&] {(void)declare_conformance_diagnostics(log,results,parse_declare_diagnostics_workbench_options(R"({"case_id_key":"missing"})"));});
  assert(serialize_declare_workbench_conformance_json(results) == expected);
  auto nonfinite = results; nonfinite[0].dev_fitness = std::numeric_limits<double>::quiet_NaN();
  rejects([&] {(void)declare_conformance_diagnostics(log,nonfinite);});
  rejects([&] {(void)declare_conformance_diagnostics(log,std::vector<DeclareConformance>{});});
  auto typed_log = log;
  typed_log.traces[0].case_id = "18446744073709551615";
  typed_log.traces[0].case_id_state = FirstClassAttributeState::kPresent;
  typed_log.traces[0].case_id_attribute = EventAttribute{"case:concept:name","18446744073709551615",AttributeKind::kInt};
  typed_log.traces[0].case_id_attribute->typed_value = std::numeric_limits<std::uint64_t>::max();
  const auto typed_diagnostics = declare_conformance_diagnostics(typed_log,results);
  assert(std::get<std::uint64_t>(typed_diagnostics.rows[0][0]->typed_value) == std::numeric_limits<std::uint64_t>::max());
  assert(typed_diagnostics.rows[0][0]->first_class_role == FirstClassAttributeRole::kCaseId);
}

void full_conformance_storage_and_prefix_only_playout_are_visible() {
  DeclareConformance stored;
  stored.deviations = {{std::string("\xed\xa0\x80",3)," first ",std::string("x\0y",3),false}};
  stored.no_constr_total = std::numeric_limits<int>::min(); stored.no_dev_total = std::numeric_limits<int>::max();
  stored.dev_fitness = -0.0; stored.is_fit = true;
  const auto json = serialize_declare_workbench_conformance_json(stored);
  contains(json,R"("second":"x\u0000y","binary":false)");
  contains(json,R"("no_constr_total":-2147483648,"no_dev_total":2147483647,"dev_fitness":-0.0,"is_fit":true)");
  stored.dev_fitness = std::numeric_limits<double>::quiet_NaN();
  contains(serialize_declare_workbench_conformance_json(std::vector<DeclareConformance>{stored}),R"("dev_fitness":"NaN")");
  DeclareModel empty; empty.rules.clear();
  const auto options = parse_declare_playout_workbench_options(R"({"n_traces":2,"min_length":3,"max_length":3,"seed":18446744073709551615})");
  const auto empty_log = declare_playout(empty,options);
  assert(empty_log.traces.size() == 2 && empty_log.traces[0].case_id == "0" && empty_log.traces[1].case_id == "1" && empty_log.traces[0].events.empty());
  const auto unknown = one_rule("unknown",{"X","unused",false,-1,-2});
  const auto generated = declare_playout(unknown,options);
  const auto repeated = declare_playout(unknown,options);
  assert(generated.traces.size() == 2);
  for (std::size_t i = 0; i < generated.traces.size(); ++i) {
    assert(generated.traces[i].events.size() == 3);
    for (std::size_t j = 0; j < generated.traces[i].events.size(); ++j) {
      const auto& event = generated.traces[i].events[j];
      assert(event.activity == "X" && event.timestamp.empty() && event.resource.empty() && event.input_order == j);
      assert(event.activity == repeated.traces[i].events[j].activity);
    }
  }
  const auto forbidden = one_rule(kDeclareAbsence,{"A","unused",false,0,0});
  for (const auto& trace : declare_playout(forbidden,options).traces) assert(trace.events.empty());
  auto pending = one_rule(kDeclareResponse,{"A","B",true,0,0});
  pending.rules[kDeclareAbsence].push_back({"B","",false,0,0});
  auto single = options; single.n_traces = 1; single.min_length = single.max_length = 1;
  const auto prefixes = declare_playout(pending,single);
  assert(prefixes.traces[0].events.size() == 1 && prefixes.traces[0].events[0].activity == "A");
  assert(!check_declare_conformance(prefixes, pending)[0].is_fit);
  for (const auto* input : {R"({"n_traces":0,"min_length":2,"max_length":1})",R"({"n_traces":100001})",R"({"max_length":10001})"}) {
    const auto invalid = parse_declare_playout_workbench_options(input);
    rejects([&] {(void)declare_playout(empty,invalid);});
  }
}
}  // namespace

int main() {
  const auto run = [](const char* name, auto action) {
    std::cerr << "Checking " << name << std::endl;
    try {action();} catch (const std::exception& error) {std::cerr << name << ": " << error.what() << std::endl; throw;}
  };
  run("exact groups and all eighteen predicates",exact_groups_records_and_all_eighteen_native_predicates);
  run("offline arity and duplicate denominator",offline_arity_unknown_denominator_and_duplicates_are_not_streaming_validation);
  run("option defaults and exact presence",exact_options_defaults_presence_and_transport_domains);
  run("complete raw Trace constructor",full_raw_trace_constructor_round_trips_every_public_storage_field);
  run("strict model and raw Trace transport",raw_trace_types_and_model_types_are_strict_without_semantic_normalization);
  run("actual Trace, four data shapes and diagnostics",actual_trace_all_native_shapes_and_diagnostics_remain_separate_calls);
  run("complete raw conformance and prefix-safe playout",full_conformance_storage_and_prefix_only_playout_are_visible);
}
