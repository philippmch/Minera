#undef NDEBUG
#include "minera/genetic_matrix_workbench_json.hpp"
#include "minera/pnml.hpp"
#include "minera/workflow_input_result_json.hpp"
#include "minera/object_centric_playout_workbench_json.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace minera;
template<class Action> void rejects(Action action) {
  bool rejected = false;
  try { action(); } catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
}
void contains(const std::string& text, std::string_view expected) {
  if (text.find(expected) == std::string::npos) std::cerr << "Missing " << expected << " in " << text << '\n';
  assert(text.find(expected) != std::string::npos);
}
GeneticCausalMatrix sequence() { return {{"A", "B"}, {{}, {{0}}}, {{{1}}, {}}}; }
// Search only tiny authored fixtures, preserving native enabled/fire semantics.
// This verifies actual conversion language including silent routes/boundaries.
bool accepts(const GeneticMatrixConversionResult& result, const std::vector<std::string>& word) {
  using State = std::pair<std::size_t, Marking>;
  std::vector<State> pending = {{0, result.markings.initial}};
  std::set<State> seen;
  for (std::size_t next = 0; next < pending.size(); ++next) {
    assert(pending.size() < 1024);
    const auto [position, marking] = pending[next];
    if (!seen.emplace(position, marking).second) continue;
    if (position == word.size() && marking == result.markings.final) return true;
    for (const auto& transition : result.net.transitions()) {
      if (!petri_transition_enabled(transition, marking)) continue;
      auto following_position = position;
      if (transition.activity) {
        if (position == word.size() || result.net.activities().at(*transition.activity) != word[position]) continue;
        ++following_position;
      }
      auto following = marking;
      fire_petri_transition(transition, following);
      if (!seen.contains({following_position, following})) pending.emplace_back(following_position, std::move(following));
    }
  }
  return false;
}

void exact_input_storage_and_transactional_presence() {
  const auto raw = R"({"activities":["","A","A","\ud800\u0000東京","\udfff","\ud83d\ude80"],"input_map":[[[2,1,2],[1,2],[]],[],[[18446744073709551615]]],"output_map":[[],[[0]],[],[]]})";
  std::set<std::string> members = {"stale"};
  const auto model = parse_genetic_matrix_workbench_model_json(raw, &members);
  assert(model.activities.size() == 6 && model.activities[1] == model.activities[2]);
  assert(model.input_map[0] == std::vector<GeneticMatrixPartition>({{2,1,2},{1,2},{}}));
  assert(model.input_map[2][0][0] == std::numeric_limits<std::size_t>::max());
  assert(model.input_map.size() == 3 && model.output_map.size() == 4);
  assert(members == std::set<std::string>({"activities","input_map","output_map"}));
  const auto encoded = serialize_genetic_matrix_workbench_model_json(model);
  assert(serialize_genetic_matrix_workbench_model_json(parse_genetic_matrix_workbench_model_json(encoded)) == encoded);
  contains(encoded, R"(\ud800\u0000東京)"); contains(encoded, "18446744073709551615");
  assert(serialize_genetic_matrix_workbench_model_json(parse_genetic_matrix_workbench_model_json("{}")) ==
      R"({"activities":[],"input_map":[],"output_map":[]})");
  for (const auto* bad : {R"({"activities":null})", R"({"activities":[1]})", R"({"input_map":[null]})",
      R"({"input_map":[[[1.0]]]})", R"({"output_map":[[[1e0]]]})", R"({"input_map":[[[-1]]]})",
      R"({"input_map":[[[-0]]]})", R"({"input_map":[[[18446744073709551616]]]})", R"({"extra":[]})",
      R"({"activities":[],"activit\u0069es":[]})", "[]", "{} trailing"}) {
    members = {"sentinel"}; rejects([&] { (void)parse_genetic_matrix_workbench_model_json(bad, &members); });
    assert(members == std::set<std::string>{"sentinel"});
  }
  rejects([&] { (void)parse_genetic_matrix_workbench_model_json(std::string("{\"activities\":[\"") + char(0xc0) + char(0x80) + "\"]}"); });
  rejects([&] { (void)parse_genetic_matrix_workbench_model_json(std::string("{\"activities\":[\"") + "\xed\xa0\x80\xed\xb0\x80" + "\"]}"); });
}

void one_real_variant_and_exact_integer_storage() {
  const auto defaults = parse_genetic_matrix_workbench_options_json("{}");
  assert(static_cast<int>(defaults.variant) == 0);
  assert(serialize_genetic_matrix_workbench_options_json(defaults) == R"({"variant":{"name":"TO_PETRI_NET","index":0}})");
  std::set<std::string> members;
  for (const auto* alias : {"to_petri_net", "TO_PETRI_NET", "petri_net"}) {
    const auto options = parse_genetic_matrix_workbench_options_json("{\"variant\":\"" + std::string(alias) + "\"}", &members);
    assert(static_cast<int>(options.variant) == 0 && members == std::set<std::string>{"variant"});
    assert(accepts(convert_genetic_matrix(sequence(), options.variant), {"A","B"}));
  }
  for (const auto* token : {"-2147483648", "-1", "1", "2147483647"}) {
    const auto options = parse_genetic_matrix_workbench_options_json("{\"variant_index\":" + std::string(token) + "}", &members);
    contains(serialize_genetic_matrix_workbench_options_json(options), "\"name\":null");
    contains(serialize_genetic_matrix_workbench_options_json(options), "\"index\":" + std::string(token));
    assert(members == std::set<std::string>{"variant_index"});
    rejects([&] { (void)convert_genetic_matrix(sequence(), options.variant); });
  }
  for (const auto* bad : {R"({"variant":" to_petri_net"})", R"({"variant":"To_Petri_Net"})",
      R"({"variant":"to_petri_net "})", R"({"variant":0})", R"({"variant":null})",
      R"({"variant_index":null})", R"({"variant_index":0.0})", R"({"variant_index":0e0})",
      R"({"variant_index":true})", R"({"variant_index":"0"})", R"({"variant_index":2147483648})",
      R"({"variant_index":-2147483649})", R"({"variant":"to_petri_net","variant_index":0})",
      R"({"variant_index":0,"variant_\u0069ndex":0})", R"({"population_size":4})", "null"}) {
    members = {"sentinel"}; rejects([&] { (void)parse_genetic_matrix_workbench_options_json(bad, &members); });
    assert(members == std::set<std::string>{"sentinel"});
  }
}

void normalization_is_native_and_original_is_separate() {
  auto model = sequence(); model.input_map[1] = {{0,0},{0}}; model.output_map[0] = {{1,1},{1}};
  const auto before = serialize_genetic_matrix_workbench_model_json(model);
  const auto result = convert_genetic_matrix(model);
  assert(serialize_genetic_matrix_workbench_model_json(model) == before);
  assert(result.matrix.input_map[1] == std::vector<GeneticMatrixPartition>{{0}});
  assert(result.matrix.output_map[0] == std::vector<GeneticMatrixPartition>{{1}});
  assert(result.net.place_count() == 3 && result.net.transitions().size() == 2);
  assert(result.markings.initial == Marking({1,0,0}) && result.markings.final == Marking({0,1,0}));
  assert(accepts(result, {"A","B"}) && !accepts(result, {"A"}) && !accepts(result, {"B","A"}));
  const auto full = serialize_genetic_matrix_workbench_result_json(result);
  contains(full, "\"matrix\":" + serialize_genetic_matrix_workbench_model_json(result.matrix));
  contains(full, R"("markings":{"initial":[1,0,0],"final":[0,1,0]})");
  contains(full, R"("source_storage":0,"sink_storage":1,"transition_names":null)");
  const auto pnml = to_pnml(result.net, result.markings.initial, result.markings.final);
  const auto imported = parse_pnml_document(pnml);
  assert(imported.net.activities() == result.net.activities());
  assert(imported.net.transitions().size() == result.net.transitions().size());
  for (std::size_t i = 0; i < result.net.transitions().size(); ++i) {
    assert(imported.net.transitions()[i].inputs == result.net.transitions()[i].inputs);
    assert(imported.net.transitions()[i].outputs == result.net.transitions()[i].outputs);
  }
  contains(genetic_matrix_conversion_json(result), "\"family\":\"GENETIC_MATRIX_CONVERSION\"");
}

void alternative_groups_and_independent_groups_have_different_languages() {
  const GeneticCausalMatrix choice{{"A","B","C","D"},{{},{{0}},{{0}},{{1,2}}},{{{1,2}},{{3}},{{3}},{}}};
  const GeneticCausalMatrix parallel{{"A","B","C","D"},{{},{{0}},{{0}},{{1},{2}}},{{{1},{2}},{{3}},{{3}},{}}};
  const auto x = convert_genetic_matrix(choice), a = convert_genetic_matrix(parallel);
  assert(accepts(x,{"A","B","D"}) && accepts(x,{"A","C","D"}));
  assert(!accepts(x,{"A","B","C","D"}));
  assert(accepts(a,{"A","B","C","D"}) && accepts(a,{"A","C","B","D"}));
  assert(!accepts(a,{"A","B","D"}));
  const auto loop = convert_genetic_matrix({{"A","B"},{{{1}},{{0}}},{{{1}},{{0}}}});
  assert(accepts(loop,{"A","B"}) && accepts(loop,{"A","B","A","B"}));
  assert(std::any_of(loop.net.transitions().begin(),loop.net.transitions().end(),[](const auto& t){return t.silent();}));
  const auto disconnected = convert_genetic_matrix({{"A","B"},{{},{}},{{},{}}});
  assert(accepts(disconnected,{"A"}) && accepts(disconnected,{"B"}) && !accepts(disconnected,{"A","B"}));
  const auto empty = convert_genetic_matrix({}); assert(empty.net.place_count()==2 && empty.net.transitions().empty());
  const auto literal = convert_genetic_matrix(parse_genetic_matrix_workbench_model_json(R"({"activities":["","","\ud800"],"input_map":[[],[],[]],"output_map":[[],[],[]]})"));
  assert(literal.net.activities() == literal.matrix.activities && literal.net.activities().size()==3);
}

void native_errors_are_not_input_codec_semantic_constraints() {
  for (const auto* raw : {R"({"activities":["A"],"input_map":[],"output_map":[[]]})",
      R"({"activities":["A"],"input_map":[[[]]],"output_map":[[]]})",
      R"({"activities":["A"],"input_map":[[[1]]],"output_map":[[]]})",
      R"({"activities":["A"],"input_map":[[[18446744073709551615]]],"output_map":[[]]})"}) {
    const auto matrix = parse_genetic_matrix_workbench_model_json(raw);
    const auto before = serialize_genetic_matrix_workbench_model_json(matrix);
    rejects([&] { (void)convert_genetic_matrix(matrix); });
    assert(serialize_genetic_matrix_workbench_model_json(matrix) == before);
  }
}

void all_public_net_storage_and_signed_markings_are_complete() {
  PetriNet empty;
  const auto absent = serialize_genetic_matrix_workbench_net_json(empty);
  contains(absent, R"("has_source":false,"has_sink":false,"source":null,"sink":null)");
  contains(absent, R"("source_storage":0,"sink_storage":0,"transition_names":null)");
  empty.set_transition_names({});
  const auto engaged = serialize_genetic_matrix_workbench_net_json(empty);
  contains(engaged, R"("transition_names":[])"); assert(absent != engaged);
  PetriNet net;
  for (int i=0;i<4;++i) net.add_place();
  net.set_source(1); net.set_sink(0); net.set_activities({"same","same",""});
  const auto t=net.add_transition(0,{0,0},{1}); net.add_transition(std::nullopt,{},{});
  net.set_transition_names({std::nullopt,std::string("")});
  const TokenInteger huge("1234567890123456789012345678901234567890");
  net.set_ordinary_arc_weights(t,{{huge,true},{0,false}},{{2,false}});
  net.add_reset_arc(t,2,huge,true);net.add_inhibitor_arc(t,3,0,false);
  HeuristicStructuredValue::Array names = {
      HeuristicStructuredValue{std::int64_t{-9223372036854775807LL-1}},
      HeuristicStructuredValue{std::uint64_t{18446744073709551615ULL}},
      HeuristicStructuredValue{-0.0},HeuristicStructuredValue{std::numeric_limits<double>::denorm_min()},
      HeuristicStructuredValue{std::numeric_limits<double>::infinity()},
      HeuristicStructuredValue{-std::numeric_limits<double>::infinity()},
      HeuristicStructuredValue{std::numeric_limits<double>::quiet_NaN()},
      HeuristicStructuredValue{std::string("\xed\xa0\x80")}};
  PetriDiscoveryMarkings sparse;
  sparse.detached_places={{7,HeuristicStructuredValue{names}}};
  sparse.initial={{{PetriMarkingPlaceKind::kRoot,0},0},{{PetriMarkingPlaceKind::kDetached,7},-huge}};
  sparse.final={{{PetriMarkingPlaceKind::kDetached,7},huge}};net.set_discovery_markings(sparse);
  const auto json=serialize_genetic_matrix_workbench_net_json(net);
  contains(json,R"("input_weights":[{"negative":true,"magnitude":1234567890123456789012345678901234567890},{"negative":false,"magnitude":0}])");
  contains(json,R"("reset_weights":[{"negative":true,"magnitude":1234567890123456789012345678901234567890}])");
  contains(json,R"("inhibitor_weights":[{"negative":false,"magnitude":0}])");
  contains(json,R"("namespace":"root","place_id":0,"tokens":0)");
  contains(json,R"("namespace":"detached","place_id":7,"tokens":-1234567890123456789012345678901234567890)");
  contains(json,R"({"type":"int64","value":-9223372036854775808})");
  contains(json,R"({"type":"uint64","value":18446744073709551615})");
  contains(json,R"({"type":"float","value":-0})");
  contains(json,R"({"type":"float","value":4.9406564584124654e-324})");
  contains(json,R"({"type":"float","value":"INF"})");
  contains(json,R"({"type":"float","value":"-INF"})");
  contains(json,R"({"type":"float","value":"NaN"})");
  contains(json,R"({"type":"string","value":"\ud800"})");
  contains(json,R"("source_storage":1,"sink_storage":0,"transition_names":[null,""])");
  GeneticMatrixConversionResult arbitrary;
  arbitrary.variant=static_cast<GeneticMatrixConversionVariant>(-2147483647-1);
  arbitrary.net=net; arbitrary.matrix=sequence();
  arbitrary.markings={{std::numeric_limits<std::int64_t>::min(),-1,0},
                      {std::numeric_limits<std::int64_t>::max()}};
  const auto result=serialize_genetic_matrix_workbench_result_json(arbitrary);
  contains(result,R"("variant":{"name":null,"index":-2147483648})");
  contains(result,R"("markings":{"initial":[-9223372036854775808,-1,0],"final":[9223372036854775807]})");
  // The exact result serializer retains caller-owned arbitrary result state.
  // Narrow optional native legacy export validates independently and rejects it.
  rejects([&]{(void)genetic_matrix_conversion_json(arbitrary);});
}


void all_native_byte_domains_and_old_wire_compatibility() {
  const std::string high="\xed\xa0\x80", low="\xed\xb0\x80", pair=high+low, scalar="\xf0\x90\x80\x80";
  GeneticCausalMatrix matrix;
  matrix.activities={"", "ordinary", std::string("A\0B",3), high, low, pair, scalar,
      std::string("\xff\0",2),std::string("\xc0\x80",2),std::string("\xf0\x9f",2),high+"x"+low};
  matrix.input_map.resize(matrix.activities.size()); matrix.output_map.resize(matrix.activities.size());
  matrix.input_map[1]={{0,0},{0},{}}; // Literal storage, not converter-valid groups.
  const auto before=matrix;
  const auto encoded=serialize_genetic_matrix_workbench_model_json(matrix);
  const auto restored=parse_genetic_matrix_workbench_model_json(encoded);
  assert(restored.activities==matrix.activities && restored.input_map==matrix.input_map && restored.output_map==matrix.output_map);
  assert(matrix.activities==before.activities && matrix.input_map==before.input_map);
  assert(restored.activities[5]==pair&&restored.activities[6]==scalar&&restored.activities[5]!=restored.activities[6]);
  contains(encoded,R"({"native_bytes":"hex-v1","data":"eda080edb080"})");
  contains(encoded,R"("\ud800")"); contains(encoded,R"("\udc00")"); contains(encoded,scalar);
  contains(encoded,R"({"native_bytes":"hex-v1","data":"ff00"})");
  contains(encoded,R"("A\u0000B")");
  const auto old=parse_genetic_matrix_workbench_model_json(R"({"activities":["ordinary","\ud800","\udc00","\u0000"],"input_map":[],"output_map":[]})");
  const std::string expected_old=R"({"activities":["ordinary","\ud800","\udc00","\u0000"],"input_map":[],"output_map":[]})";
  assert(serialize_genetic_matrix_workbench_model_json(old)==expected_old);
  // Valid byte-tag input can expand beyond the request JSON limit when its
  // canonical scalar escapes controls. Serialization has no request-size cap.
  { GeneticCausalMatrix expanded;expanded.activities={std::string(6u*1024u*1024u,'\0')};const auto output=serialize_genetic_matrix_workbench_model_json(expanded);assert(output.size()>32u*1024u*1024u);assert(output.starts_with("{\"activities\":[\"\\u0000"));assert(output.find("native_bytes")==std::string::npos); }
  // Every octet, not only a selected malformed-UTF8 example, is reversible.
  for(unsigned byte=0;byte<256;++byte){GeneticCausalMatrix one;one.activities={std::string(1,static_cast<char>(byte))};const auto text=serialize_genetic_matrix_workbench_model_json(one);assert(parse_genetic_matrix_workbench_model_json(text).activities==one.activities);}
  std::set<std::string> supplied;
  const auto hex=parse_genetic_matrix_workbench_model_json(R"({"activities":[{"native_bytes":"hex-v1","data":"41"},{"native_bytes":"hex-v1","data":"FF00"},{"native_bytes":"hex-v1","data":""}],"output_map":[[],[],[]]})",&supplied);
  assert(hex.activities==std::vector<std::string>({"A",std::string("\xff\0",2),""}));
  assert(supplied==std::set<std::string>({"activities","output_map"}));
  contains(serialize_genetic_matrix_workbench_model_json(hex),R"("activities":["A",{"native_bytes":"hex-v1","data":"ff00"},""])");
  for(const auto* bad:{R"({"activities":[{}]})",R"({"activities":[{"native_bytes":"hex-v2","data":"ff"}]})",R"({"activities":[{"native_bytes":"hex-v1"}]})",R"({"activities":[{"data":"ff"}]})",R"({"activities":[{"native_bytes":"hex-v1","data":"f"}]})",R"({"activities":[{"native_bytes":"hex-v1","data":"gg"}]})",R"({"activities":[{"native_bytes":"hex-v1","data":null}]})",R"({"activities":[{"native_bytes":"hex-v1","data":"ff","extra":0}]})",R"({"activities":[{"native_bytes":"hex-v1","data":"ff","d\u0061ta":"00"}]})"}){
    supplied={"sentinel"}; rejects([&]{(void)parse_genetic_matrix_workbench_model_json(bad,&supplied);});assert(supplied==std::set<std::string>{"sentinel"});
  }
  // Actual conversion keeps byte labels and distinct scalar/WTF8 owners. Its
  // semantic normalization is independent from constructor serialization.
  auto executable=matrix;executable.input_map.assign(matrix.activities.size(),{});executable.output_map.assign(matrix.activities.size(),{});
  const auto converted=convert_genetic_matrix(executable);
  assert(converted.matrix.activities==executable.activities&&converted.net.activities()==executable.activities);
  const auto net_json=serialize_genetic_matrix_workbench_net_json(converted.net);
  contains(net_json,"\"activities\":");assert(net_json.find("activity_names")==std::string::npos);
  const auto net_copy=parse_oc_playout_workbench_petri_net_json(net_json);
  assert(net_copy.activities()==converted.net.activities());
}

void full_literal_net_byte_fields_and_inactive_boundaries() {
  const std::string bytes("\xff\0",2), pair="\xed\xa0\x80\xed\xb0\x80";
  // Existing inactive getter storage already survives. Prove nonzero maxima
  // while retaining the ordinary rich getter view and its null presentation.
  PetriNetLiteralStorage inactive;inactive.source_storage=std::numeric_limits<std::size_t>::max();inactive.sink_storage=std::numeric_limits<std::size_t>::max()-1;
  auto net=PetriNet::from_literal_storage(inactive);
  auto encoded=serialize_genetic_matrix_workbench_net_json(net);
  contains(encoded,R"("has_source":false,"has_sink":false,"source":null,"sink":null)");
  contains(encoded,R"("source_storage":18446744073709551615,"sink_storage":18446744073709551614)");
  auto exact=serialize_oc_playout_workbench_petri_net_json(net);auto copy=parse_oc_playout_workbench_petri_net_json(exact);
  assert(!copy.has_source()&&!copy.has_sink()&&copy.source()==inactive.source_storage&&copy.sink()==inactive.sink_storage);
  inactive.has_source=true;inactive.has_sink=true;net=PetriNet::from_literal_storage(inactive);
  encoded=serialize_genetic_matrix_workbench_net_json(net);assert(encoded==serialize_oc_playout_workbench_petri_net_json(net));
  copy=parse_oc_playout_workbench_petri_net_json(encoded);assert(copy.has_source()&&copy.has_sink()&&copy.source()==inactive.source_storage);
  const auto roundtrip=[](const PetriNet& value){const auto json=serialize_genetic_matrix_workbench_net_json(value);assert(json.find("activity_names")==std::string::npos);assert(json==serialize_oc_playout_workbench_petri_net_json(value));const auto parsed=parse_oc_playout_workbench_petri_net_json(json);assert(serialize_oc_playout_workbench_petri_net_json(parsed)==serialize_oc_playout_workbench_petri_net_json(value));return parsed;};
  // Each native string location alone must trigger the lossless literal view.
  auto named=convert_genetic_matrix(sequence()).net;named.set_transition_names({bytes,std::nullopt});auto named_copy=roundtrip(named);assert(named_copy.transition_names()->at(0)==bytes);
  auto keyed=convert_genetic_matrix(sequence()).net;
  PetriDiscoveryMarkings keyed_marking;keyed_marking.detached_places={{7,HeuristicStructuredValue{HeuristicStructuredValue::Object{{bytes,HeuristicStructuredValue{std::string("ordinary")}}}}}};
  keyed_marking.initial={{{PetriMarkingPlaceKind::kDetached,7},0}};keyed.set_discovery_markings(keyed_marking);auto keyed_copy=roundtrip(keyed);
  assert(std::get<HeuristicStructuredValue::Object>(keyed_copy.discovery_markings()->detached_places[0].name.value).begin()->first==bytes);
  auto valued=convert_genetic_matrix(sequence()).net;keyed_marking.detached_places[0].name=HeuristicStructuredValue{HeuristicStructuredValue::Array{HeuristicStructuredValue{pair}}};valued.set_discovery_markings(keyed_marking);auto valued_copy=roundtrip(valued);
  assert(std::get<std::string>(std::get<HeuristicStructuredValue::Array>(valued_copy.discovery_markings()->detached_places[0].name.value)[0].value)==pair);
  // The literal constructor permits inactive, inconsistent and unknown stored
  // fields. A complete serializer must preserve all of them without invoking
  // workflow/export validators or manufacturing an executable replacement.
  PetriNetLiteralStorage state;state.place_count=3;state.source_storage=std::numeric_limits<std::size_t>::max();state.sink_storage=2;state.has_source=false;state.has_sink=true;state.activities={bytes,pair,""};
  PetriTransition transition;transition.activity=std::numeric_limits<std::size_t>::max();transition.inputs={0,0,std::numeric_limits<std::size_t>::max()};transition.outputs={99};transition.reset_inputs={2,2};transition.inhibitor_inputs={1};
  const TokenInteger huge("1234567890123456789012345678901234567890");
  transition.input_weights={{-huge,true}};transition.output_weights={{0,false},{huge,true}};transition.reset_weights={{huge,false},{0,true}};transition.inhibitor_weights={{-huge,false}};state.transitions={transition};
  state.transition_names=PetriTransitionNameList{std::nullopt,bytes,std::string("")};
  HeuristicStructuredValue::Object nested{{bytes,HeuristicStructuredValue{HeuristicStructuredValue::Array{HeuristicStructuredValue{bytes},HeuristicStructuredValue{pair},HeuristicStructuredValue{std::numeric_limits<std::int64_t>::min()},HeuristicStructuredValue{std::numeric_limits<std::uint64_t>::max()},HeuristicStructuredValue{-0.0},HeuristicStructuredValue{std::numeric_limits<double>::infinity()},HeuristicStructuredValue{-std::numeric_limits<double>::infinity()},HeuristicStructuredValue{std::numeric_limits<double>::quiet_NaN()}}}}};
  PetriDiscoveryMarkings sparse;sparse.detached_places={{7,HeuristicStructuredValue{nested}},{7,HeuristicStructuredValue{std::string("duplicate identity")}}};
  const auto unknown=static_cast<PetriMarkingPlaceKind>(-2147483647-1);
  sparse.initial={{{unknown,std::numeric_limits<std::size_t>::max()},-huge},{{PetriMarkingPlaceKind::kRoot,99},0},{{PetriMarkingPlaceKind::kRoot,99},huge}};
  sparse.final={{{PetriMarkingPlaceKind::kDetached,8},huge}};state.discovery_markings=sparse;
  const auto arbitrary=PetriNet::from_literal_storage(state);copy=roundtrip(arbitrary);
  assert(copy.source()==state.source_storage&&!copy.has_source()&&copy.sink()==2&&copy.has_sink());
  assert(copy.activities()==state.activities&&copy.transition_names()==state.transition_names);
  assert(copy.transitions().front().inputs==transition.inputs&&copy.transitions().front().input_weights.front().magnitude==-huge);
  assert(copy.discovery_markings()->initial[0].place.kind==unknown&&copy.discovery_markings()->initial.size()==3&&copy.discovery_markings()->detached_places.size()==2);
  // Full-result serialization preserves arbitrary variant/matrix/marking
  // fields too; this is a direct public codec check, never a fake HTTP upload.
  GeneticMatrixConversionResult result;result.variant=static_cast<GeneticMatrixConversionVariant>(-2147483647-1);result.net=arbitrary;result.matrix.activities={bytes,pair};result.matrix.input_map={{{std::numeric_limits<std::size_t>::max(),0,0}}};result.matrix.output_map={};result.markings={{std::numeric_limits<std::int64_t>::min(),-1,0},{std::numeric_limits<std::int64_t>::max()}};
  const auto full=serialize_genetic_matrix_workbench_result_json(result);
  contains(full,"\"net\":"+serialize_oc_playout_workbench_petri_net_json(arbitrary));contains(full,"\"matrix\":"+serialize_genetic_matrix_workbench_model_json(result.matrix));contains(full,R"("variant":{"name":null,"index":-2147483648})");contains(full,R"("markings":{"initial":[-9223372036854775808,-1,0],"final":[9223372036854775807]})");
  rejects([&]{(void)genetic_matrix_conversion_json(result);});
}

template<class Action> void run(const char* name,Action action) {
  std::cerr << "[genetic_matrix_workbench_json] " << name << std::endl;
  try { action(); }
  catch (const std::exception& error) { std::cerr << name << ": " << error.what() << std::endl; throw; }
}
}  // namespace

int main() {
  run("exact constructor storage and transactional presence",exact_input_storage_and_transactional_presence);
  run("one real variant and exact int32",one_real_variant_and_exact_integer_storage);
  run("native normalization and unchanged original",normalization_is_native_and_original_is_separate);
  run("XOR AND loop and disconnected executable languages",alternative_groups_and_independent_groups_have_different_languages);
  run("native semantic validation remains native",native_errors_are_not_input_codec_semantic_constraints);
  run("full Petri getters signed marking and IEEE nested metadata",all_public_net_storage_and_signed_markings_are_complete);
  run("all native byte domains and old wire compatibility",all_native_byte_domains_and_old_wire_compatibility);
  run("literal net byte fields inactive maxima and invalid storage",full_literal_net_byte_fields_and_inactive_boundaries);
}
