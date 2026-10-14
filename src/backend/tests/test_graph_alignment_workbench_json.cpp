#undef NDEBUG

#include "minera/graph_alignment_workbench_json.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace minera;

void contains(const std::string& json, std::string_view text) {
  assert(json.find(text) != std::string::npos);
}

void rejects(std::string_view input) {
  bool threw = false;
  try { (void)parse_alignment_trace_words_json(input); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

// Inspect the root scalar token, keeping uint64 literals and nonfinite tags
// independent of JavaScript/double integer conversion.
std::string token(const std::string& json, std::string_view name) {
  const auto key = '"' + std::string(name) + "\":";
  const auto start = json.find(key);
  assert(start != std::string::npos);
  const auto begin = start + key.size();
  const auto end = json.find_first_of(",}", begin);
  assert(end != std::string::npos);
  return json.substr(begin, end - begin);
}

void finite_round_trip(const std::string& json, std::string_view name, double value) {
  const auto raw = token(json, name);
  char* end = nullptr;
  const auto parsed = std::strtod(raw.c_str(), &end);
  assert(end == raw.c_str() + raw.size());
  assert(parsed == value && std::signbit(parsed) == std::signbit(value));
}

EventLog log_of(const std::vector<std::vector<std::string>>& words) {
  EventLog log;
  for (const auto& word : words) {
    Trace trace;
    for (const auto& name : word) trace.events.push_back(Event{"", name});
    log.traces.push_back(std::move(trace));
  }
  return log;
}

PetriNet chain() {
  PetriNet net;
  const auto source = net.add_place(), middle = net.add_place(), sink = net.add_place();
  net.set_source(source); net.set_sink(sink); net.set_activities({"a", "b"});
  net.add_transition(0, {source}, {middle});
  net.add_transition(1, {middle}, {sink});
  return net;
}

void exact_trace_words_and_surrogates() {
  const auto words = parse_alignment_trace_words_json(
      R"( [""," \t\r\n ","a:b,=\\\"/","a\u0000b","\u00e9","\ud83d\ude00","\ud800","\udfff","A","\u0041","\ud801\u0042"] )");
  assert(words.size() == 11 && words[0].empty());
  assert(words[1] == " \t\r\n " && words[2] == "a:b,=\\\"/");
  assert(words[3] == std::string("a\0b", 3));
  assert(words[4] == "\xc3\xa9" && words[5] == "\xf0\x9f\x98\x80");
  assert(words[6] == "\xed\xa0\x80" && words[7] == "\xed\xbf\xbf");
  assert(words[8] == "A" && words[9] == "A");  // repeated decoded labels stay repeated
  assert(words[10] == std::string("\xed\xa0\x81") + "B");
  const auto encoded = serialize_alignment_trace_words_json(words);
  contains(encoded, R"(\ud800)"); contains(encoded, R"(\udfff)");
  assert(encoded.find("\xed\xa0\x80") == std::string::npos);
  assert(parse_alignment_trace_words_json(encoded) == words);
  assert(parse_alignment_trace_words_json("\t []\r\n").empty());
  assert(serialize_alignment_trace_words_json({}) == "[]");
  assert(parse_alignment_trace_words_json("[\"\xc2\x80\xe0\xa0\x80\xf4\x8f\xbf\xbf\"]")[0] ==
         "\xc2\x80\xe0\xa0\x80\xf4\x8f\xbf\xbf");
}

void strict_trace_words_schema_and_unicode() {
  for (const auto& input : std::vector<std::string>{
      "", "null", "{}", "\"x\"", "0", "true", "[1]", "[1.0]", "[null]",
      "[false]", "[{}]", "[[\"x\"]]", "[NaN]", "[Infinity]", "[\"x\",]",
      "[,\"x\"]", "[\"x\" \"y\"]", "[\"x\"]false", "[\"x\"][]", "[\"x\"",
      "[\"\\x\"]", "[\"\\u12\"]", "[\"\\uZZZZ\"]", "[\"\\ud800\\uZZZZ\"]",
      "[\"x\n\"]", "[\"x\t\"]", std::string("[\"x\0y\"]", 7),
      "[\"\xc0\x80\"]", "[\"\xc2\"]", "[\"\x80\"]", "[\"\xe0\x80\x80\"]",
      "[\"\xed\xa0\x80\"]", "[\"\xf0\x80\x80\x80\"]", "[\"\xf4\x90\x80\x80\"]",
      "[\"\xf5\x80\x80\x80\"]", "[\"\xc2x\"]", "\xef\xbb\xbf[]", "\v[]"}) rejects(input);
  rejects(std::string(65, '[') + "\"x\"" + std::string(65, ']'));
}

void input_caps_do_not_limit_native_output() {
  // This result is a legitimate native string carrier beyond the input cap.
  const std::vector<std::string> large{std::string(32u * 1024u * 1024u, 'x')};
  const auto output = serialize_alignment_trace_words_json(large);
  assert(output.size() == large[0].size() + 4);
  rejects(output);
  auto many = std::vector<std::string>(1000000, "");
  const auto many_output = serialize_alignment_trace_words_json(many);
  rejects(many_output);  // array plus one million values exceeds the node cap
  many.pop_back();
  assert(parse_alignment_trace_words_json(serialize_alignment_trace_words_json(many)).size() == 999999);
}

void real_dfg_trace_undefined_fitness_and_uncomputed_result() {
  const DfgFrequency dfg{{{"a", "b"}, 5}};
  const std::map<std::string, std::uint64_t> starts{{"a", 2}}, ends{{"b", 3}};
  const auto result = align_trace_on_dfg({"a", "b"}, dfg, starts, ends);
  assert(result.computed && std::isnan(result.fitness) && std::isnan(result.best_worst_cost));
  const auto encoded = serialize_dfg_alignment_trace_json(result);
  contains(encoded, R"("alignment":[{"log":"a","model":"a"},{"log":"b","model":"b"}])");
  assert(token(encoded, "computed") == "true" && token(encoded, "cost") == "0.0");
  assert(token(encoded, "internal_cost") == "0.0" && token(encoded, "fitness") == "\"NaN\"");
  assert(token(encoded, "best_worst_cost") == "\"NaN\"");
  assert(token(encoded, "visited_states") == std::to_string(result.visited_states));
  assert(token(encoded, "closed") == std::to_string(result.closed));
  assert(token(encoded, "closed_states") == std::to_string(result.closed_states));
  const auto unavailable = align_trace_on_dfg({"a", "b"}, dfg, starts, {{"c", 1}});
  assert(!unavailable.computed);
  const auto missing = serialize_dfg_alignment_trace_json(unavailable);
  assert(token(missing, "computed") == "false");
  assert(token(missing, "fitness") == "\"NaN\"");
  contains(missing, R"("alignment":[])");
  assert(token(missing, "visited_states") == std::to_string(unavailable.visited_states));
  assert(token(missing, "closed_states") == std::to_string(unavailable.closed_states));
  finite_round_trip(missing, "cost", unavailable.cost);
}

void real_dfg_log_same_run_projection_and_empty_containers() {
  const DfgFrequency dfg{{{"a", "b"}, 5}};
  const std::map<std::string, std::uint64_t> starts{{"a", 2}}, ends{{"b", 3}};
  const auto log = log_of({{"a", "b"}, {"a", "b"}, {"a"}});
  const auto result = align_log_on_dfg(log, dfg, starts, ends);
  assert(result.alignments.size() == 3 && result.unique_variants == 2);
  const auto projection = project_alignments_on_dfg(log, dfg, starts, ends, result, {});
  const auto encoded = serialize_dfg_alignment_log_json(result, &projection);
  contains(encoded, "\"projection\":" + serialize_dfg_alignment_projection_json(projection));
  contains(encoded, "\"alignments\":[" + serialize_dfg_alignment_trace_json(result.alignments[0]));
  assert(encoded.find(serialize_dfg_alignment_trace_json(result.alignments[2])) != std::string::npos);
  assert(token(encoded, "unique_variants") == "2");
  contains(serialize_dfg_alignment_log_json(result), R"("projection":null)");
  assert(serialize_dfg_alignment_log_json({}) ==
         R"({"alignments":[],"unique_variants":0,"projection":null})");
  assert(serialize_dfg_alignment_projection_json({}) == R"({"dfg":[],"activities":[]})");
}

void all_dfg_fields_round_trip_numbers_and_exact_counts() {
  DfgAlignmentResult result;
  result.computed = false; result.alignment = {{std::string("a\0", 2), "\xed\xa0\x80"}};
  result.cost = std::nextafter(0.1, 1.0);
  result.internal_cost = -0.0;
  result.fitness = std::numeric_limits<double>::infinity();
  result.best_worst_cost = -std::numeric_limits<double>::infinity();
  result.visited_states = std::numeric_limits<std::size_t>::max();
  result.closed = 9007199254740993ull; result.closed_states = 42;
  auto encoded = serialize_dfg_alignment_trace_json(result);
  finite_round_trip(encoded, "cost", result.cost);
  finite_round_trip(encoded, "internal_cost", result.internal_cost);
  assert(token(encoded, "fitness") == "\"INF\"" && token(encoded, "best_worst_cost") == "\"-INF\"");
  assert(token(encoded, "visited_states") == std::to_string(result.visited_states));
  assert(token(encoded, "closed") == "9007199254740993" && token(encoded, "closed_states") == "42");
  contains(encoded, R"("log":"a\u0000","model":"\ud800")");
  for (const auto value : {std::numeric_limits<double>::max(), std::numeric_limits<double>::denorm_min(),
                          std::nextafter(1.0, 2.0), -0.0}) {
    result.cost = value;
    finite_round_trip(serialize_dfg_alignment_trace_json(result), "cost", value);
  }
  DfgAlignmentProjection projection;
  projection.dfg[{"\xed\xa0\x80", std::string("b\0", 2)}] = {
      std::numeric_limits<std::uint64_t>::max(), 9007199254740993ull};
  projection.activities[""] = {0, 7, std::numeric_limits<std::uint64_t>::max()};
  // Keep the raw JSON literal outside assert's macro arguments: MSVC's
  // traditional preprocessor can reinterpret a raw surrogate escape there.
  const std::string expected_projection =
      R"({"dfg":[{"source":"\ud800","target":"b\u0000","sync":18446744073709551615,"mm":9007199254740993}],"activities":[{"activity":"","sync":0,"mm":7,"lm":18446744073709551615}]})";
  assert(serialize_dfg_alignment_projection_json(projection) == expected_projection);
}

void decomposed_all_fields_optional_labels_and_unsigned_sentinel() {
  DecomposedAlignmentResult result;
  result.complete = false; result.timed_out = true; result.exhausted = true;
  result.fitness = std::numeric_limits<double>::quiet_NaN();
  result.best_worst_cost = std::numeric_limits<double>::infinity();
  result.trace_count = std::numeric_limits<std::uint64_t>::max();
  result.total_cost = -std::numeric_limits<double>::infinity();
  result.diagnostics = {1, 2, 3, 4, 5, 6, 7, 8, 9, std::numeric_limits<std::size_t>::max(),
                        true, std::string("n\0", 2) + "\xed\xbf\xbf"};
  DecomposedAlignmentTrace trace;
  trace.activities = {"", " a ", "\xed\xa0\x80"};
  trace.count = 9007199254740993ull; trace.complete = false; trace.timed_out = true; trace.exhausted = false;
  trace.cost = std::nextafter(0.1, 1.0); trace.fitness = -0.0;
  trace.best_worst_cost = std::numeric_limits<double>::quiet_NaN();
  trace.alignment = {{std::nullopt, std::string(""), std::numeric_limits<std::size_t>::max()},
                     {std::string(">>"), std::nullopt, 9007199254740993ull}};
  result.traces.push_back(trace);
  const auto row = serialize_decomposed_alignment_trace_json(trace);
  finite_round_trip(row, "cost", trace.cost); finite_round_trip(row, "fitness", trace.fitness);
  contains(row, R"("activities":[""," a ","\ud800"])");
  contains(row, R"("count":9007199254740993,"complete":false,"timed_out":true,"exhausted":false)");
  contains(row, "\"alignment\":[{\"log\":null,\"model\":\"\",\"transition\":" +
                    std::to_string(std::numeric_limits<std::size_t>::max()));
  contains(row, R"({"log":">>","model":null,"transition":9007199254740993})");
  const auto encoded = serialize_decomposed_alignment_result_json(result);
  contains(encoded, R"("complete":false,"timed_out":true,"exhausted":true,"fitness":"NaN","best_worst_cost":"INF","trace_count":18446744073709551615,"total_cost":"-INF")");
  contains(encoded, R"("diagnostics":{"decompositions":1,"components":2,"merges":3,"cache_hits":4,"cache_misses":5,"border_disagreements":6,"agreement_iterations":7,"visited_states":8,"queued_states":9,)");
  contains(encoded, "\"traversed_arcs\":" + std::to_string(result.diagnostics.traversed_arcs));
  contains(encoded, R"("whole_net_fallback":true,"note":"n\u0000\udfff")");
  contains(encoded, "\"traces\":[" + row + ']');
}

void real_decomposed_complete_duplicate_and_exhausted_results() {
  const auto net = chain();
  const auto result = align_decomposed(log_of({{"a", "b"}, {"a", "b"}, {}}), net);
  assert(result.complete && result.traces.size() == 3 && result.diagnostics.cache_hits > 0);
  const auto encoded = serialize_decomposed_alignment_result_json(result);
  assert(token(encoded, "trace_count") == "3");
  contains(encoded, R"("complete":true,"timed_out":false,"exhausted":false)");
  contains(encoded, "\"traces\":[" + serialize_decomposed_alignment_trace_json(result.traces[0]));
  for (const auto& trace : result.traces) contains(encoded, serialize_decomposed_alignment_trace_json(trace));
  contains(encoded, R"("transition":0)"); contains(encoded, R"("transition":1)");
  DecomposedAlignmentOptions options; options.max_states = 1;
  const auto exhausted = align_decomposed(log_of({{"a"}, {"b"}}), net, options);
  assert(!exhausted.complete && exhausted.exhausted && exhausted.traces.size() == 2);
  const auto incomplete = serialize_decomposed_alignment_result_json(exhausted);
  contains(incomplete, R"("complete":false,"timed_out":false,"exhausted":true)");
  for (const auto& trace : exhausted.traces) contains(incomplete, serialize_decomposed_alignment_trace_json(trace));
  contains(serialize_decomposed_alignment_result_json(align_decomposed(EventLog{}, net)), R"("traces":[])");
}

void options_defaults_and_supplied_empty_functions_remain_observable() {
  assert(serialize_dfg_alignment_options_json({}) ==
      R"({"variant":"CLASSIC","variant_index":0,"sync_cost":{},"model_move_cost":{},"log_move_cost":{},"internal_log_move_cost":{},"max_visited_states":0,"max_time_ms":0,"activity_attribute":"concept:name","timestamp_attribute":"time:timestamp","sync_cost_supplied":false,"model_move_cost_supplied":false,"log_move_cost_supplied":false,"internal_log_move_cost_supplied":false})");
  assert(serialize_decomposed_alignment_options_json({}) ==
      R"({"variant":"RECOMPOS_MAXIMAL","variant_index":0,"threshold_border_agreement":100000000.0,"max_time_trace":0.0,"max_time_total":0.0,"max_states":200000,"max_total_states":2000000,"use_cache":true,"clear_cache":false,"log_move_cost":10000.0,"model_move_cost":10000.0,"tau_model_cost":1.0,"sync_cost":0.0,"activity_log_costs":{},"activity_model_costs":{},"activity_sync_costs":{},"initial_marking":[],"final_marking":[],"activity_attribute":"concept:name"})");
  DfgAlignmentOptions options;
  options.sync_cost_supplied = true; options.model_move_cost_supplied = true;
  options.log_move_cost_supplied = true; options.internal_log_move_cost_supplied = true;
  const auto encoded = serialize_dfg_alignment_options_json(options);
  contains(encoded, R"("sync_cost":{},"model_move_cost":{},"log_move_cost":{},"internal_log_move_cost":{})");
  contains(encoded, R"("sync_cost_supplied":true,"model_move_cost_supplied":true,"log_move_cost_supplied":true,"internal_log_move_cost_supplied":true)");
  const DfgFrequency dfg{{{"a", "b"}, 1}};
  bool missing_cost = false;
  try { (void)align_trace_on_dfg({"a", "b"}, dfg, {{"a", 1}}, {{"b", 1}}, options); }
  catch (const std::invalid_argument&) { missing_cost = true; }
  assert(missing_cost);  // explicitly empty functions differ from default costs
}

void configured_options_describe_actual_native_alignment() {
  DfgAlignmentOptions dfg_options;
  dfg_options.sync_cost = {{"a", 0.125}, {"b", 0.75}};
  dfg_options.model_move_cost = {{"a", 17}, {"b", 18}};
  dfg_options.log_move_cost = {{"a", 31}, {"b", 32}};
  dfg_options.internal_log_move_cost = {{"a", 41}, {"b", 42}};
  dfg_options.sync_cost_supplied = true; dfg_options.model_move_cost_supplied = true;
  dfg_options.log_move_cost_supplied = true; dfg_options.internal_log_move_cost_supplied = true;
  dfg_options.max_visited_states = 123;
  const auto dfg = align_trace_on_dfg({"a", "b"}, {{{"a", "b"}, 1}}, {{"a", 1}}, {{"b", 1}}, dfg_options);
  assert(dfg.computed && dfg.cost == 0.875);
  const auto dfg_json = serialize_dfg_alignment_options_json(dfg_options);
  contains(dfg_json, R"("sync_cost":{"a":0.125,"b":0.75})");
  contains(dfg_json, R"("model_move_cost":{"a":17.0,"b":18.0})");
  contains(dfg_json, R"("log_move_cost":{"a":31.0,"b":32.0})");
  contains(dfg_json, R"("internal_log_move_cost":{"a":41.0,"b":42.0})");
  assert(token(dfg_json, "max_visited_states") == "123");

  DecomposedAlignmentOptions options;
  options.threshold_border_agreement = 25; options.max_time_trace = 100; options.max_time_total = 200;
  options.max_states = 2000; options.max_total_states = 20000;
  options.use_cache = false; options.clear_cache = true;
  options.log_move_cost = 400; options.model_move_cost = 100; options.tau_model_cost = 3; options.sync_cost = 2;
  options.activity_log_costs = {{"a", 31}, {"b", 32}};
  options.activity_model_costs = {{"a", 17}, {"b", 18}};
  options.activity_sync_costs = {{"a", 0.125}, {"b", 0.75}};
  options.initial_marking = {1, 0, 0}; options.final_marking = {0, 0, 1};
  const auto result = align_decomposed(log_of({{"a", "b"}}), chain(), options);
  assert(result.complete && result.traces[0].cost == 0.875);
  const auto encoded = serialize_decomposed_alignment_options_json(options);
  contains(encoded, R"("threshold_border_agreement":25.0,"max_time_trace":100.0,"max_time_total":200.0,"max_states":2000,"max_total_states":20000,"use_cache":false,"clear_cache":true)");
  contains(encoded, R"("log_move_cost":400.0,"model_move_cost":100.0,"tau_model_cost":3.0,"sync_cost":2.0)");
  contains(encoded, R"("activity_log_costs":{"a":31.0,"b":32.0},"activity_model_costs":{"a":17.0,"b":18.0},"activity_sync_costs":{"a":0.125,"b":0.75})");
  contains(encoded, R"("initial_marking":[1,0,0],"final_marking":[0,0,1])");
  options.max_states = 0; options.max_total_states = 0;
  const auto exhausted = align_decomposed(log_of({{"a"}}), chain(), options);
  assert(!exhausted.complete && !exhausted.exhausted && !exhausted.timed_out && exhausted.traces.empty());
  contains(serialize_decomposed_alignment_options_json(options), R"("max_states":0,"max_total_states":0)");
}

void options_retain_native_numeric_label_and_enum_storage() {
  DfgAlignmentOptions dfg;
  dfg.variant = static_cast<DfgAlignmentVariant>(-7);
  dfg.sync_cost = {{"", -0.0}, {"\xed\xa0\x80", std::numeric_limits<double>::quiet_NaN()}};
  dfg.model_move_cost = {{std::string("a\0b", 3), std::numeric_limits<double>::infinity()}};
  dfg.log_move_cost = {{">>", -std::numeric_limits<double>::infinity()}};
  dfg.internal_log_move_cost = {{" x ", std::nextafter(0.1, 1.0)}};
  dfg.max_visited_states = std::numeric_limits<std::size_t>::max();
  dfg.max_time_ms = std::numeric_limits<std::uint64_t>::max();
  dfg.activity_attribute = " phase "; dfg.timestamp_attribute = std::string("t\0", 2) + "\xed\xbf\xbf";
  const auto encoded = serialize_dfg_alignment_options_json(dfg);
  contains(encoded, R"("variant":"UNKNOWN","variant_index":-7)");
  contains(encoded, R"("sync_cost":{"":-0.0,"\ud800":"NaN"})");
  contains(encoded, R"("model_move_cost":{"a\u0000b":"INF"},"log_move_cost":{">>":"-INF"})");
  contains(encoded, R"("activity_attribute":" phase ","timestamp_attribute":"t\u0000\udfff")");
  assert(token(encoded, "max_visited_states") == std::to_string(dfg.max_visited_states));
  assert(token(encoded, "max_time_ms") == "18446744073709551615");
  // Nonempty maps do not rewrite the original native boolean flag storage.
  assert(token(encoded, "sync_cost_supplied") == "false");

  DecomposedAlignmentOptions options;
  options.variant = static_cast<DecomposedAlignmentVariant>(19);
  options.threshold_border_agreement = std::nextafter(1.0, 2.0);
  options.max_time_trace = -0.0; options.max_time_total = std::numeric_limits<double>::denorm_min();
  options.max_states = 9007199254740993ull; options.max_total_states = std::numeric_limits<std::size_t>::max();
  options.log_move_cost = std::numeric_limits<double>::quiet_NaN();
  options.model_move_cost = std::numeric_limits<double>::infinity();
  options.tau_model_cost = -std::numeric_limits<double>::infinity(); options.sync_cost = -0.0;
  options.activity_log_costs = {{"", -0.0}};
  options.activity_model_costs = {{"\xed\xa0\x80", std::nextafter(0.1, 1.0)}};
  options.activity_sync_costs = {{std::string("k\0", 2), std::numeric_limits<double>::infinity()}};
  options.initial_marking = {0, std::numeric_limits<std::size_t>::max(), 9007199254740993ull};
  options.final_marking = {}; options.activity_attribute = "\xed\xbf\xbf";
  const auto json = serialize_decomposed_alignment_options_json(options);
  contains(json, R"("variant":"UNKNOWN","variant_index":19)");
  finite_round_trip(json, "threshold_border_agreement", options.threshold_border_agreement);
  finite_round_trip(json, "max_time_trace", options.max_time_trace);
  finite_round_trip(json, "max_time_total", options.max_time_total);
  finite_round_trip(json, "sync_cost", options.sync_cost);
  contains(json, R"("log_move_cost":"NaN","model_move_cost":"INF","tau_model_cost":"-INF")");
  contains(json, R"("activity_log_costs":{"":-0.0})");
  contains(json, R"("activity_sync_costs":{"k\u0000":"INF"})");
  contains(json, "\"initial_marking\":[0," + std::to_string(std::numeric_limits<std::size_t>::max()) + ",9007199254740993]");
  contains(json, R"("final_marking":[],"activity_attribute":"\udfff")");
  assert(token(json, "max_states") == "9007199254740993");
  assert(token(json, "max_total_states") == std::to_string(options.max_total_states));
  assert(options.initial_marking.size() == 3 && options.activity_log_costs.contains(""));
}
}  // namespace

int main() {
  exact_trace_words_and_surrogates();
  strict_trace_words_schema_and_unicode();
  input_caps_do_not_limit_native_output();
  real_dfg_trace_undefined_fitness_and_uncomputed_result();
  real_dfg_log_same_run_projection_and_empty_containers();
  all_dfg_fields_round_trip_numbers_and_exact_counts();
  decomposed_all_fields_optional_labels_and_unsigned_sentinel();
  real_decomposed_complete_duplicate_and_exhausted_results();
  options_defaults_and_supplied_empty_functions_remain_observable();
  configured_options_describe_actual_native_alignment();
  options_retain_native_numeric_label_and_enum_storage();
}
