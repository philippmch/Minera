#undef NDEBUG

#include "minera/petri_anti_multi_workbench_json.hpp"
#include "minera/workflow_input_result_json.hpp"

#include <cassert>
#include <cmath>
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
using Values = std::map<std::string, std::string, std::less<>>;

template <class Parse>
auto parsed(Parse parse, const Values& values) {
  const FieldLookup lookup = [&](std::string_view name) {
    const auto found = values.find(name); return found == values.end() ? std::string{} : found->second;
  };
  const FieldPresence presence = [&](std::string_view name) { return values.contains(name); };
  return parse(lookup, presence);
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

double scalar(const std::string& json, std::string_view field) {
  const auto key = "\"" + std::string(field) + "\":";
  const auto begin = json.find(key); assert(begin != std::string::npos);
  const auto start = begin + key.size(), end = json.find_first_of(",}", start);
  const auto token = json.substr(start, end - start); char* parsed_end = nullptr;
  const auto value = std::strtod(token.c_str(), &parsed_end);
  assert(parsed_end == token.c_str() + token.size());
  return value;
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

PetriNet choice_net() {
  PetriNet net; net.set_activities({"a", "b"});
  net.set_source(net.add_place()); net.set_sink(net.add_place());
  net.add_transition(0, {0}, {1}); net.add_transition(1, {0}, {1});
  return net;
}

void scoped_options_preserve_defaults_exact_numbers_and_deferred_domains() {
  const auto anti = parsed(parse_petri_anti_workbench_params, {});
  assert(serialize_petri_anti_workbench_options_json(anti) == R"({"variant":"VERSION_DISCOUNTED_A_STAR","variant_index":0,"exponent":2.0,"epsilon":0.0050000000000000001,"marking_limit":100000,"max_states":0,"initial_marking":[],"final_marking":[],"activity_attribute":"concept:name"})");
  assert(serialize_petri_multi_workbench_options_json(parsed(parse_petri_multi_workbench_params, {})) == R"({"variant":"VERSION_DISCOUNTED_A_STAR","variant_index":0,"exponent":2.0,"marking_limit":100000,"max_states":0,"initial_marking":[],"final_marking":[],"activity_attribute":"concept:name"})");
  for (const auto alias : {"version_discounted_a_star", "VERSION_DISCOUNTED_A_STAR", "discounted_a_star"}) {
    assert(parsed(parse_petri_anti_workbench_params, {{"variant", alias}}).variant == PetriAlignmentVariant::kDiscountedAStar);
    assert(parsed(parse_petri_multi_workbench_params, {{"variant", alias}}).variant == PetriAlignmentVariant::kDiscountedAStar);
  }
  const auto native_max = std::to_string(std::numeric_limits<std::size_t>::max());
  const auto custom = parsed(parse_petri_anti_workbench_params, {
      {"exponent", "0x1.0000000000001p+1"}, {"epsilon", "0x0.0000000000001p-1022"},
      {"marking_limit", "0"}, {"max_states", native_max}, {"initial_marking_json", "[]"},
      {"final_marking_json", "[0," + native_max + ",0]"}, {"activity_attribute", " phase \t"}});
  assert(custom.exponent == std::nextafter(2.0, 3.0) && custom.epsilon == std::numeric_limits<double>::denorm_min());
  assert(custom.marking_limit == 0 && custom.max_states == std::numeric_limits<std::size_t>::max());
  assert(custom.initial_marking.empty() && custom.final_marking == std::vector<std::size_t>({0, std::numeric_limits<std::size_t>::max(), 0}));
  assert(custom.activity_attribute == " phase \t");
  const auto negative_zero = parsed(parse_petri_anti_workbench_params, {{"epsilon", "-0.0"}, {"activity_attribute", ""}});
  assert(negative_zero.epsilon == 0.0 && std::signbit(negative_zero.epsilon) && negative_zero.activity_attribute.empty());
  assert(parsed(parse_petri_anti_workbench_params, {{"epsilon", "1e-9999"}}).epsilon == 0.0);
  const auto comma = parsed(parse_petri_multi_workbench_params, {{"initial_marking", " 0 , " + native_max + " ,0\t"}, {"final_marking", "1,0"}});
  assert(comma.initial_marking == custom.final_marking && comma.final_marking == std::vector<std::size_t>({1, 0}));

  const auto net = choice_net();
  for (const auto& fields : std::vector<Values>{{{"exponent", "1"}}, {{"exponent", "-2"}}, {{"epsilon", "-1"}},
      {{"marking_limit", "0"}, {"max_states", "0"}}, {{"activity_attribute", ""}}, {{"activity_attribute", " \t"}}}) {
    const auto options = parsed(parse_petri_anti_workbench_params, fields);
    rejects([&] { (void)anti_align_petri(net, log_of({{"a"}}), options); });
  }
  for (const auto& fields : std::vector<Values>{{{"exponent", "1"}}, {{"marking_limit", "0"}, {"max_states", "0"}}, {{"activity_attribute", ""}}}) {
    const auto options = parsed(parse_petri_multi_workbench_params, fields);
    rejects([&] { (void)multi_align_petri(net, log_of({{"a"}}), options); });
  }
  // Scoped changes do not broaden either cached parser's old acceptance.
  rejects([&] {
    const auto legacy = static_cast<PetriMultiAlignmentOptions (*)(const FieldLookup&, const FieldPresence&)>(parse_petri_multi_alignment_params);
    (void)parsed(legacy, {{"marking_limit", "0"}, {"max_states", "1"}});
  });
}

void strict_transport_rejects_wrong_carriers_conflicts_and_nonfinite_inputs() {
  for (const auto& fields : std::vector<Values>{{{"variant", "UNKNOWN"}}, {{"variant", " VERSION_DISCOUNTED_A_STAR"}},
      {{"exponent", ""}}, {{"exponent", "NaN"}}, {{"exponent", "INF"}}, {{"exponent", "1e9999"}},
      {{"exponent", "2junk"}}, {{"exponent", std::string("2\0x", 3)}}, {{"epsilon", "-Infinity"}},
      {{"marking_limit", "-0"}}, {{"max_states", "-1"}}, {{"marking_limit", "18446744073709551616"}},
      {{"initial_marking", ""}}, {{"initial_marking", "1,"}}, {{"final_marking", "1.0"}},
      {{"initial_marking", ""}, {"initial_marking_json", "[]"}}, {{"final_marking", "0"}, {"final_marking_json", "[]"}}}) {
    rejects([&] { (void)parsed(parse_petri_anti_workbench_params, fields); });
  }
  const std::vector<std::string> invalid_json{"", "null", "{}", "0", "true", "[true]", "[null]", "[\"1\"]", "[[0]]",
      "[-0]", "[-1]", "[1.0]", "[1e0]", "[01]", "[+1]", "[18446744073709551616]", "[NaN]", "[Infinity]",
      "[0,]", "[0]false", R"({"x":0,"\u0078":1})", "[\"\xed\xa0\x80\"]", "[\"\xc0\x80\"]"};
  for (const auto& value : invalid_json) {
    rejects([&] { (void)parsed(parse_petri_anti_workbench_params, {{"initial_marking_json", value}}); });
    rejects([&] { (void)parsed(parse_petri_multi_workbench_params, {{"final_marking_json", value}}); });
  }
  const auto repeated = parsed(parse_petri_multi_workbench_params, {{"initial_marking_json", "[0,0,0]"}});
  assert(repeated.initial_marking == std::vector<std::size_t>({0, 0, 0}));
}

void real_native_baselines_marking_defaults_and_exact_selected_source() {
  const auto net = choice_net();
  const auto anti_options = parsed(parse_petri_anti_workbench_params, {{"epsilon", "0"}, {"marking_limit", "0"}, {"max_states", "1"}, {"initial_marking_json", "[]"}, {"final_marking_json", "[]"}});
  const auto anti = anti_align_petri(net, log_of({{"a"}}), anti_options);
  assert(anti.activities == std::vector<std::string>({"b"}) && anti.transitions == std::vector<std::size_t>({1}));
  assert(anti.cost == -1.5 && anti.objective == -1.5 && anti.precision == 0.0);
  const auto anti_json = serialize_petri_anti_workbench_result_json(anti);
  contains(anti_json, R"("activities":["b"],"transitions":[1],"cost":-1.5,"objective":-1.5,"precision":0.0)");
  const auto multi_options = parsed(parse_petri_multi_workbench_params, {{"marking_limit", "0"}, {"max_states", "1"}, {"initial_marking_json", "[]"}, {"final_marking_json", "[]"}});
  const auto multi = multi_align_petri(net, log_of({{"a"}, {"b"}, {"a"}}), multi_options);
  assert(multi.activities == std::vector<std::string>({"a"}) && multi.transitions == std::vector<std::size_t>({0}));
  assert(multi.cost == 1.5 && multi.objective == 1.5 && multi.max_distance_to_log == 2.0);
  const auto multi_json = serialize_petri_multi_workbench_result_json(multi);
  contains(multi_json, R"("activities":["a"],"transitions":[0],"cost":1.5,"objective":1.5,"max_distance_to_log":2.0)");
  contains(multi_json, "\"visited_states\":" + std::to_string(multi.diagnostics.visited_states));
  contains(multi_json, "\"queued_states\":" + std::to_string(multi.diagnostics.queued_states));
  contains(multi_json, "\"traversed_arcs\":" + std::to_string(multi.diagnostics.traversed_arcs));
  assert(multi_json.find("computed") == std::string::npos && multi_json.find("variant") == std::string::npos);
  auto zero = multi_options; zero.initial_marking = zero.final_marking = {0, 0};
  const auto no_run = multi_align_petri(net, log_of({{"a"}}), zero);
  assert(no_run.activities.empty() && no_run.transitions.empty());
  auto wrong_width = multi_options; wrong_width.initial_marking = {0};
  rejects([&] { (void)multi_align_petri(net, log_of({{"a"}}), wrong_width); });

  EventLog selected = log_of({{"ignored"}}); selected.traces[0].events[0].attributes.push_back({" phase ", "a"});
  auto literal = multi_options; literal.activity_attribute = " phase ";
  assert(multi_align_petri(net, selected, literal).cost == 0.0);
  selected.traces[0].events[0].attributes[0].is_null = true;
  rejects([&] { (void)multi_align_petri(net, selected, literal); });
  PetriNet blank; blank.set_activities({""}); blank.set_source(blank.add_place()); blank.set_sink(blank.add_place()); blank.add_transition(0, {0}, {1});
  const auto present_blank = multi_align_petri(blank, log_of({{""}}), multi_options);
  assert(present_blank.activities == std::vector<std::string>({""}) && present_blank.cost == 0.0);
  const auto empty_trace = multi_align_petri(blank, log_of({{}}), multi_options);
  assert(empty_trace.transitions == std::vector<std::size_t>({0}));
  rejects([&] { (void)multi_align_petri(net, EventLog{}, multi_options); });
}

void output_preserves_all_stored_fields_precision_and_unknown_options() {
  PetriAntiAlignmentResult anti; anti.activities = {"", " a ", "\xed\xa0\x80", std::string("n\0", 2)};
  anti.transitions = {std::numeric_limits<std::size_t>::max(), 0};
  anti.diagnostics = {std::numeric_limits<std::size_t>::max(), 9007199254740993ull, 17, true};
  for (const auto value : {std::nextafter(0.1, 1.0), std::numeric_limits<double>::max(), std::numeric_limits<double>::denorm_min(), -0.0}) {
    anti.cost = anti.objective = anti.precision = value;
    const auto output = serialize_petri_anti_workbench_result_json(anti);
    for (const auto field : {"cost", "objective", "precision"}) {
      const auto roundtrip = scalar(output, field);
      assert(roundtrip == value && std::signbit(roundtrip) == std::signbit(value));
    }
    const std::string escaped_words = R"("activities":[""," a ","\ud800","n\u0000"])";
    contains(output, escaped_words);
    contains(output, "\"transitions\":[" + std::to_string(std::numeric_limits<std::size_t>::max()) + ",0]");
    contains(output, R"("queued_states":9007199254740993,"traversed_arcs":17,"exhausted":true)");
  }
  anti.cost = std::numeric_limits<double>::quiet_NaN(); anti.objective = std::numeric_limits<double>::infinity(); anti.precision = -std::numeric_limits<double>::infinity();
  contains(serialize_petri_anti_workbench_result_json(anti), R"("cost":"NaN","objective":"INF","precision":"-INF")");
  PetriMultiAlignmentResult multi; multi.activities = anti.activities; multi.transitions = anti.transitions;
  multi.cost = -0.0; multi.objective = std::numeric_limits<double>::quiet_NaN(); multi.max_distance_to_log = std::numeric_limits<double>::infinity();
  multi.diagnostics = {1, 2, 3, true};
  contains(serialize_petri_multi_workbench_result_json(multi), R"("cost":-0.0,"objective":"NaN","max_distance_to_log":"INF","diagnostics":{"visited_states":1,"queued_states":2,"traversed_arcs":3,"exhausted":true})");
  PetriAntiAlignmentOptions options; options.variant = static_cast<PetriAlignmentVariant>(-7);
  options.exponent = std::numeric_limits<double>::quiet_NaN(); options.epsilon = -0.0;
  options.initial_marking = {std::numeric_limits<std::size_t>::max()}; options.final_marking = {0};
  options.max_states = std::numeric_limits<std::size_t>::max(); options.marking_limit = 0; options.activity_attribute = "\xed\xbf\xbf";
  const auto output = serialize_petri_anti_workbench_options_json(options);
  contains(output, R"("variant":"UNKNOWN","variant_index":-7,"exponent":"NaN","epsilon":-0.0,"marking_limit":0)");
  const std::string literal_selector = R"("activity_attribute":"\udfff")";
  contains(output, literal_selector);
}

void complete_document_preserves_arc_order_metadata_weights_and_stochastic_storage() {
  PnmlDocument document; document.net.set_activities({"A", "", "\xed\xa0\x80"});
  for (int i = 0; i < 4; ++i) document.net.add_place();
  document.net.set_source(0); document.net.set_sink(1);
  const auto transition = document.net.add_transition(0, {0, 0}, {1});
  const TokenInteger huge("184467440737095516160000000000000000000");
  document.net.set_ordinary_arc_weights(transition, {{TokenInteger(0), false}, {huge, true}}, {{TokenInteger(7), false}});
  document.net.add_reset_arc(transition, 2, huge, true); document.net.add_inhibitor_arc(transition, 3, TokenInteger(0));
  document.net.set_transition_names({std::string("\xed\xbf\xbf")});
  document.initial_marking = {huge, 0, 1, 0}; document.final_marking = {0, 2, 0, 0};
  document.place_ids = {"p0", " p1 ", "p2", "p3"}; document.transition_ids = {"t0"};
  document.initial_marking_keys = {true, true, true, false}; document.final_marking_keys = {true, true, false, true};
  document.arc_occurrences = std::vector<PnmlArcOccurrence>{
      {0, transition, true, PetriArcType::kNormal, 0, "second", false},
      {0, transition, true, PetriArcType::kNormal, huge, "first", true},
      {2, transition, true, PetriArcType::kReset, huge, " reset ", true},
      {1, transition, false, PetriArcType::kNormal, 7, "out", false},
      {3, transition, true, PetriArcType::kInhibitor, 0, std::string("n\0", 2), false}};
  StochasticMapTransition record; record.transition = transition; record.timing = StochasticMapTiming::kImmediate;
  record.distribution = StochasticMapDistribution::kNormal; record.parameter = std::nextafter(0.1, 1.0);
  record.secondary_parameter = -0.0; record.tertiary_parameter = std::numeric_limits<double>::infinity();
  record.activation_weight = std::numeric_limits<double>::quiet_NaN(); record.priority = std::numeric_limits<int>::min();
  document.stochastic_map = StochasticMap{{record}};
  const auto output = serialize_petri_workbench_document_json(document);
  contains(output, "\"net\":" + workflow_input_petri_json(document.net));
  contains(output, R"("raw_source":0,"raw_sink":1,"initial_marking":[184467440737095516160000000000000000000,0,1,0],"final_marking":[0,2,0,0])");
  contains(output, R"("place_ids":["p0"," p1 ","p2","p3"],"transition_ids":["t0"],"initial_marking_keys":[true,true,true,false],"final_marking_keys":[true,true,false,true])");
  contains(output, R"("arc_occurrences":[{"place":0,"transition":0,"input":true,"type":"NORMAL","type_index":0,"weight":0,"negative_weight":false,"id":"second"},{"place":0,"transition":0,"input":true,"type":"NORMAL","type_index":0,"weight":184467440737095516160000000000000000000,"negative_weight":true,"id":"first"})");
  contains(output, R"("type":"RESET","type_index":1,"weight":184467440737095516160000000000000000000,"negative_weight":true,"id":" reset ")");
  contains(output, R"("type":"INHIBITOR","type_index":2,"weight":0,"negative_weight":false)");
  contains(output, R"("timing":"immediate","timing_index":1,"distribution":"normal","distribution_index":4)");
  assert(scalar(output, "parameter") == record.parameter);
  contains(output, R"("secondary_parameter":-0.0,"tertiary_parameter":"INF","activation_weight":"NaN")");
  contains(output, "\"priority\":" + std::to_string(std::numeric_limits<int>::min()));
  assert(document.arc_occurrences->at(0).id == "second" && document.initial_marking[0] == huge);
  rejects<std::overflow_error>([&] { (void)static_cast<std::size_t>(huge); });

  // A complete by-value carrier also retains unknown native enum storage.
  document.arc_occurrences->push_back({std::numeric_limits<std::size_t>::max(), 31, false, static_cast<PetriArcType>(-9), TokenInteger(-5), "", true});
  document.stochastic_map->transitions[0].timing = static_cast<StochasticMapTiming>(-17);
  document.stochastic_map->transitions[0].distribution = static_cast<StochasticMapDistribution>(99);
  const auto unknown = serialize_petri_workbench_document_json(document);
  contains(unknown, R"("type":"UNKNOWN","type_index":-9,"weight":-5,"negative_weight":true,"id":"")");
  contains(unknown, R"("timing":"UNKNOWN","timing_index":-17,"distribution":"UNKNOWN","distribution_index":99)");
}

void real_pnml_import_and_optional_empty_identity_are_retained() {
  const auto xml = R"(<pnml><net id="n"><page id="page">
    <place id="p0"><initialMarking><text>184467440737095516160</text></initialMarking></place>
    <place id="p1"/><transition id="t0"><name><text>A</text></name>
      <toolspecific tool="StochasticPetriNet" version="0.2">
        <property key="distributionType">EXPONENTIAL</property><property key="distributionParameters">2.5</property>
        <property key="priority">4</property><property key="weight">3.5</property>
      </toolspecific></transition>
    <arc id="z" source="p0" target="t0"><inscription><text>0</text></inscription></arc>
    <arc id="a" source="p0" target="t0"><inscription><text>184467440737095516160</text></inscription></arc>
    <arc id="out" source="t0" target="p1"/>
    </page><finalmarkings><marking><place idref="p1"><text>0</text></place></marking></finalmarkings>
    </net></pnml>)";
  PnmlImportOptions options; options.infer_initial_marking = false; options.infer_final_marking = false;
  options.require_workflow_boundaries = false; options.import_stochastic_map = true;
  const auto document = parse_pnml_document(xml, options);
  assert(document.arc_occurrences && document.arc_occurrences->size() == 3);
  assert(document.arc_occurrences->at(0).id == "z" && document.arc_occurrences->at(1).id == "a");
  const auto output = serialize_petri_workbench_document_json(document);
  contains(output, R"("initial_marking":[184467440737095516160,0],"final_marking":[0,0])");
  contains(output, R"("weight":184467440737095516160,"negative_weight":false,"id":"a")");
  contains(output, R"("distribution":"exponential","distribution_index":1,"parameter":2.5)");
  assert(document.final_marking_keys[1]);
  const auto reread = parse_pnml_document(to_pnml(document), options);
  assert(reread.initial_marking == document.initial_marking && reread.final_marking_keys == document.final_marking_keys);
  assert(reread.arc_occurrences && reread.arc_occurrences->at(0).id == "z" && reread.arc_occurrences->at(1).id == "a");

  PnmlDocument empty;
  const auto omitted = serialize_petri_workbench_document_json(empty);
  contains(omitted, R"("has_source":false,"has_sink":false,"source":null,"sink":null)");
  contains(omitted, R"("raw_source":0,"raw_sink":0,"initial_marking":[],"final_marking":[],"place_ids":[],"transition_ids":[],"initial_marking_keys":[],"final_marking_keys":[],"arc_occurrences":null,"stochastic_map":null)");
  empty.arc_occurrences.emplace(); empty.stochastic_map.emplace();
  contains(serialize_petri_workbench_document_json(empty), R"("arc_occurrences":[],"stochastic_map":{"transitions":[]})");
}

void bounded_input_and_unbounded_output_have_separate_contracts() {
  std::string nested(65, '['); nested += '0'; nested.append(65, ']');
  rejects([&] { (void)parsed(parse_petri_multi_workbench_params, {{"initial_marking_json", nested}}); });
  std::string million = "[0";
  for (std::size_t i = 1; i < 1000000; ++i) million += ",0";
  million += ']';
  rejects([&] { (void)parsed(parse_petri_multi_workbench_params, {{"initial_marking_json", million}}); });
  auto large = std::string(32u * 1024u * 1024u + 1, 'a');
  rejects([&] { (void)parsed(parse_petri_multi_workbench_params, {{"activity_attribute", large}}); });
  PetriMultiAlignmentOptions options; options.activity_attribute = large;
  assert(serialize_petri_multi_workbench_options_json(options).size() > 32u * 1024u * 1024u);
  PetriAntiAlignmentResult result; result.activities = {std::move(large)};
  assert(serialize_petri_anti_workbench_result_json(result).size() > 32u * 1024u * 1024u);
  PnmlDocument document;
  HeuristicStructuredValue deep{std::string("leaf")};
  for (int i = 0; i < 100; ++i) deep = HeuristicStructuredValue{HeuristicStructuredValue::Array{std::move(deep)}};
  PetriDiscoveryMarkings markings; markings.detached_places.push_back({1, std::move(deep)});
  document.net.set_discovery_markings(std::move(markings));
  contains(serialize_petri_workbench_document_json(document), R"("type":"string","value":"leaf")");
}
}  // namespace

int main() {
  scoped_options_preserve_defaults_exact_numbers_and_deferred_domains();
  strict_transport_rejects_wrong_carriers_conflicts_and_nonfinite_inputs();
  real_native_baselines_marking_defaults_and_exact_selected_source();
  output_preserves_all_stored_fields_precision_and_unknown_options();
  complete_document_preserves_arc_order_metadata_weights_and_stochastic_storage();
  real_pnml_import_and_optional_empty_identity_are_retained();
  bounded_input_and_unbounded_output_have_separate_contracts();
}
