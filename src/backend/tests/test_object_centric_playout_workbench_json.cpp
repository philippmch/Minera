#include "minera/object_centric_playout_workbench_json.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace minera;
namespace {
template<class Exception=std::invalid_argument,class Fn> void rejects(Fn make) {
  bool caught=false;try{make();}catch(const Exception&){caught=true;}assert(caught);
}
bool same_double(double a,double b) {
  if(std::isnan(a))return std::isnan(b);
  std::uint64_t x=0,y=0;static_assert(sizeof(x)==sizeof(a));
  std::memcpy(&x,&a,sizeof(a));std::memcpy(&y,&b,sizeof(b));return x==y;
}
std::string odd_bytes(){return std::string("\xff\xed\xa0\x80\xed\xb0\x80\0z",9);}
PetriNet sequence_net(bool duplicate=false) {
  PetriNetLiteralStorage value;value.place_count=3;value.source_storage=0;value.sink_storage=2;value.has_source=true;value.has_sink=true;value.activities={"Create","Ship"};
  PetriTransition a;a.activity=0;a.inputs={0};a.outputs={1};value.transitions.push_back(a);
  if(duplicate)value.transitions.push_back(a);
  PetriTransition b;b.activity=1;b.inputs={1};b.outputs={2};value.transitions.push_back(b);
  return PetriNet::from_literal_storage(std::move(value));
}
OcpnGraph graph(bool shared=false,bool duplicate=false) {
  OcpnGraph value;value.activities={"Create","Ship"};value.object_types={"order"};value.petri_nets.emplace("order",sequence_net(duplicate));
  if(shared){value.object_types.push_back("item");value.petri_nets.emplace("item",sequence_net());value.activity_object_types["Create"]={"order","item"};value.activity_object_types["Ship"]={"order","item"};}
  return value;
}
OcCausalMarker marker(std::string related,std::string type,std::size_t minimum=1,std::size_t maximum=1,std::int64_t key=0) {
  return {std::move(related),std::move(type),minimum,maximum,key};
}
OcCausalMarkerGroup group(std::initializer_list<OcCausalMarker> markers) {
  return {std::vector<OcCausalMarker>(markers),kOcCausalInfiniteCardinality};
}
OcCausalNet causal_chain() {
  OcCausalNet value;value.object_types={"order"};
  value.activities={{"Create",{group({marker("START_order","order")})},{group({marker("Ship","order")})}},
                    {"Ship",{group({marker("Create","order")})},{group({marker("END_order","order")})}}};
  return value;
}
OcpnPlayoutOptions options() {
  OcpnPlayoutOptions value;value.max_bindings_per_activity=1;value.max_traces=8;value.max_states=1000;value.return_sequences=true;value.return_traces=true;value.objects["order"]={"o1"};return value;
}
void defaults_options_and_exact_numbers() {
  const auto empty=parse_oc_playout_workbench_ocpn_json("{}");
  assert(empty.activities.empty()&&empty.petri_nets.empty()&&empty.annotated&&empty.annotations.performance_computed);
  const auto causal=parse_oc_playout_workbench_causal_net_json("{}");assert(causal.activities.empty()&&causal.workflow_projection.annotated);
  const auto initial=parse_oc_playout_workbench_options_json("{}");
  assert(initial.max_bindings_per_activity==2&&initial.max_traces==100&&initial.max_states==200000&&initial.seed==1);
  assert(initial.start==1767225600.0&&initial.case_interval==60&&initial.event_interval==1);
  assert(initial.objects.empty()&&initial.initial_markings.empty()&&initial.final_markings.empty());
  assert(!initial.objects_unique_per_trace&&!initial.exists_trace&&!initial.return_sequences&&!initial.return_traces&&initial.event_binding==OcpnEventBinding::kIndependent);
  auto literal=initial;literal.max_bindings_per_activity=std::numeric_limits<std::size_t>::max();literal.max_traces=0;literal.max_states=std::numeric_limits<std::size_t>::max();literal.seed=std::numeric_limits<std::uint64_t>::max();
  literal.objects[odd_bytes()]={"",odd_bytes(),odd_bytes()};literal.initial_markings[odd_bytes()]={0,std::numeric_limits<std::uint32_t>::max(),0};literal.final_markings[""]={};
  literal.branching_factor_transitions=-0.0;literal.branching_factor_bindings=std::numeric_limits<double>::denorm_min();literal.branching_factor_activities=std::numeric_limits<double>::quiet_NaN();
  literal.start=-std::numeric_limits<double>::infinity();literal.case_interval=std::numeric_limits<double>::max();literal.event_interval=std::numeric_limits<double>::infinity();
  literal.event_binding=static_cast<OcpnEventBinding>(std::numeric_limits<std::int32_t>::min());literal.objects_unique_per_trace=true;literal.exists_trace=true;literal.return_sequences=true;literal.return_traces=true;
  const auto json=serialize_oc_playout_workbench_options_json(literal);std::set<std::string> supplied;
  const auto copy=parse_oc_playout_workbench_options_json(json,&supplied);assert(supplied.size()==18&&serialize_oc_playout_workbench_options_json(copy)==json);
  assert(copy.seed==std::numeric_limits<std::uint64_t>::max()&&copy.initial_markings==literal.initial_markings&&copy.objects==literal.objects);
  assert(std::signbit(copy.branching_factor_transitions)&&same_double(copy.branching_factor_bindings,literal.branching_factor_bindings)&&std::isnan(copy.branching_factor_activities));
  assert(static_cast<std::int32_t>(copy.event_binding)==std::numeric_limits<std::int32_t>::min());
  assert(json.find("\"name\":null")!=std::string::npos&&json.find("native_bytes")!=std::string::npos);
  for(const auto* name:{"independent","shared_activity","synchronized"}){
    const auto parsed=parse_oc_playout_workbench_options_json("{\"event_binding\":\""+std::string(name)+"\"}");
    assert(std::string(ocpn_event_binding_name(parsed.event_binding))==name);
  }
  std::set<std::string> sentinel={"untouched"};
  rejects([&]{(void)parse_oc_playout_workbench_options_json(R"({"seed":18446744073709551616})",&sentinel);});assert(sentinel==std::set<std::string>{"untouched"});
  const auto partial=parse_oc_playout_workbench_options_json(R"({"objects":[{"key":"order","value":[]}]})",&supplied);assert(supplied==std::set<std::string>{"objects"}&&partial.objects.at("order").empty());
}
void literal_petri_storage() {
  const auto huge=TokenInteger("123456789012345678901234567890123456789012345678901234567890");
  PetriNetLiteralStorage state;state.place_count=std::numeric_limits<std::size_t>::max();state.source_storage=7;state.sink_storage=9;state.has_source=false;state.has_sink=false;state.activities={"",odd_bytes(),"same","same"};
  PetriTransition transition;transition.activity=std::numeric_limits<std::size_t>::max();transition.inputs={7,7};transition.outputs={8,8};transition.reset_inputs={9,9};transition.inhibitor_inputs={10,10};
  transition.input_weights={{0,true},{huge,true}};transition.output_weights={{-huge,false}};transition.reset_weights={};transition.inhibitor_weights={{0,true},{huge,false}};
  state.transitions={transition,PetriTransition{}};state.transition_names=PetriTransitionNameList{std::nullopt,"",odd_bytes()};
  PetriDiscoveryMarkings markings;HeuristicStructuredValue::Object nested;nested[odd_bytes()]={HeuristicStructuredValue::Array{{std::int64_t{-7}},{std::numeric_limits<std::uint64_t>::max()},{-0.0},{odd_bytes()},{nullptr},{true}}};
  markings.detached_places={{7,HeuristicStructuredValue{nested}},{7,HeuristicStructuredValue{odd_bytes()}}};
  markings.initial={{{PetriMarkingPlaceKind::kRoot,7},0},{{static_cast<PetriMarkingPlaceKind>(-2147483647),9},-huge},{{PetriMarkingPlaceKind::kRoot,7},huge}};
  markings.final={{{PetriMarkingPlaceKind::kDetached,7},-huge}};state.discovery_markings=markings;
  const auto net=PetriNet::from_literal_storage(state);const auto json=serialize_oc_playout_workbench_petri_net_json(net);const auto copy=parse_oc_playout_workbench_petri_net_json(json);
  assert(serialize_oc_playout_workbench_petri_net_json(copy)==json&&copy.place_count()==std::numeric_limits<std::size_t>::max());
  assert(!copy.has_source()&&!copy.has_sink()&&copy.source()==7&&copy.sink()==9&&copy.activities()==state.activities);
  assert(copy.transitions()[0].input_weights[0].negative&&copy.transitions()[0].input_weights[0].magnitude==0);
  assert(copy.transitions()[0].output_weights[0].magnitude==-huge&&copy.transitions()[0].reset_weights.empty());
  assert(copy.transition_names()->size()==3&&(*copy.transition_names())[2]==odd_bytes());assert(copy.discovery_markings()->initial.size()==3&&copy.discovery_markings()->detached_places.size()==2);
  assert(copy.discovery_markings()->initial[1].tokens==-huge&&static_cast<std::int32_t>(copy.discovery_markings()->initial[1].place.kind)==-2147483647);
  assert(std::get<HeuristicStructuredValue::Object>(copy.discovery_markings()->detached_places[0].name.value).contains(odd_bytes()));
  const auto empty=parse_oc_playout_workbench_petri_net_json(R"({"transition_names":[],"discovery_markings":{}})");assert(empty.transition_names()&&empty.transition_names()->empty()&&empty.discovery_markings());
  assert(!parse_oc_playout_workbench_petri_net_json("{}").transition_names());
}
std::size_t top_level_keys(const std::string& json) {
  std::size_t keys=0,depth=0;bool quoted=false,escaped=false,key=true;
  for(const char c:json){
    if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;continue;}
    if(c=='"'){quoted=true;if(depth==1&&key){++keys;key=false;}}
    else if(c=='{'||c=='['){++depth;}else if(c=='}'||c==']'){--depth;}
    else if(c==','&&depth==1){key=true;}
  }
  return keys;
}
// The workbench carrier keeps a named net's place names (token replay breaks
// ties by place name; tree-converted and PNML nets carry them) as a tenth
// field; a net without names keeps the nine-field shape.
void carrier_keeps_native_place_names() {
  PetriNetLiteralStorage state;state.place_count=2;state.activities={"a"};
  PetriTransition transition;transition.activity=0;transition.inputs={0};transition.outputs={1};state.transitions={transition};
  const auto plain=serialize_oc_playout_workbench_petri_net_json(PetriNet::from_literal_storage(state));
  assert(plain.find("place_names")==std::string::npos&&top_level_keys(plain)==9);
  assert(!parse_oc_playout_workbench_petri_net_json(plain).place_names());
  state.place_names=std::vector<std::string>{"source","sink"};
  const auto net=PetriNet::from_literal_storage(state);assert(net.place_names());
  const auto json=serialize_oc_playout_workbench_petri_net_json(net);
  assert(top_level_keys(json)==10);
  assert(json.size()>plain.size()&&json.compare(0,plain.size()-1,plain,0,plain.size()-1)==0);
  assert(json.ends_with(R"(,"place_names":["source","sink"]})"));
  const auto copy=parse_oc_playout_workbench_petri_net_json(json);
  assert(copy.place_names()&&*copy.place_names()==*net.place_names()&&serialize_oc_playout_workbench_petri_net_json(copy)==json);
  // A carrier written before place names were kept still reads, without names.
  assert(!parse_oc_playout_workbench_petri_net_json(plain).place_names());
}
void complete_graph_annotations_and_causal_storage() {
  auto model=graph();model.activities={odd_bytes(),"",odd_bytes()};model.object_types={"dup","dup",""};model.annotated=false;
  model.activity_object_types[odd_bytes()]={"dup","dup",""};model.activity_variable_object_types[""]={odd_bytes(),odd_bytes()};model.double_arcs_on_activity[odd_bytes()][""]=false;
  OcdfgActivityMetric activity;activity.events={"e","e",odd_bytes()};activity.unique_objects={"o","o"};activity.event_objects={{"e","o"}};activity.total_objects={};
  model.annotations.activities={odd_bytes(),odd_bytes()};model.annotations.object_types={"",""};model.annotations.activities_indep[odd_bytes()]=activity;model.annotations.activities_ot["x"]["y"]=activity;
  model.annotations.start_activities[""][odd_bytes()]=activity;model.annotations.end_activities[odd_bytes()][""]=activity;model.annotations.performance_computed=false;
  OcdfgEdgeMetric edge;edge.event_couples={{"a","b"},{"a","b"}};edge.unique_objects={"o","o"};edge.event_objects={{"a","b",odd_bytes()}};edge.total_objects={};
  edge.performance={-0.0,std::numeric_limits<double>::denorm_min()};edge.performance_event_couples={std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()};edge.performance_total_objects={-std::numeric_limits<double>::infinity()};
  model.annotations.edges[odd_bytes()][{"a->b","c"}]=edge;model.annotations.edges[odd_bytes()][{"a","b->c"}]=edge;
  OcpnTokenReplayDiagnostics replay;replay.enabled=false;replay.trace_count=std::numeric_limits<std::uint64_t>::max();replay.fitting_traces=9007199254740993ULL;replay.places={{-TokenInteger("99999999999999999999999999999999999"),0,1,2}};replay.transition_activations={0,std::numeric_limits<std::uint64_t>::max()};model.tbr_results[odd_bytes()]=replay;
  const auto json=serialize_oc_playout_workbench_ocpn_json(model);const auto copy=parse_oc_playout_workbench_ocpn_json(json);assert(serialize_oc_playout_workbench_ocpn_json(copy)==json);
  assert(!copy.annotated&&!copy.tbr_results.at(odd_bytes()).enabled&&copy.tbr_results.at(odd_bytes()).fitting_traces==9007199254740993ULL);
  assert(copy.annotations.activities_indep.at(odd_bytes()).total_objects.empty()&&copy.annotations.activities_indep.at(odd_bytes()).event_objects.size()==1);
  assert(copy.annotations.edges.at(odd_bytes()).size()==2&&copy.activity_object_types==model.activity_object_types);
  auto causal=causal_chain();causal.workflow_projection=model;causal.object_types={odd_bytes(),"",odd_bytes()};causal.activities.push_back({"",{OcCausalMarkerGroup{}},{}});
  causal.activities[0].input_marker_groups[0].support_count=0;
  causal.activities[0].input_marker_groups[0].markers.push_back({odd_bytes(),"",std::numeric_limits<std::size_t>::max(),0,std::numeric_limits<std::int64_t>::min()});
  causal.activities[0].output_marker_groups[0].markers[0].max_cardinality=kOcCausalInfiniteCardinality;causal.activities[0].output_marker_groups[0].markers[0].marker_key=std::numeric_limits<std::int64_t>::max();
  const auto encoded=serialize_oc_playout_workbench_causal_net_json(causal);const auto restored=parse_oc_playout_workbench_causal_net_json(encoded);assert(serialize_oc_playout_workbench_causal_net_json(restored)==encoded);
  assert(restored.activities.back().input_marker_groups.size()==1&&restored.activities.back().input_marker_groups[0].markers.empty());
  assert(restored.activities[0].input_marker_groups[0].markers.back().marker_key==std::numeric_limits<std::int64_t>::min());
}
void full_raw_ocel_and_inactive_result() {
  const std::vector<OcelAttribute> attributes={{"same",nullptr},{"same",false},{"same",std::numeric_limits<std::int64_t>::min()},{"same",std::int64_t{0}},{"same",-0.0},{odd_bytes(),odd_bytes()},{"inf",std::numeric_limits<double>::infinity()},{"nan",std::numeric_limits<double>::quiet_NaN()}};
  Ocel log;log.events={{odd_bytes(),"",odd_bytes(),attributes},{"duplicate","A","",attributes},{"duplicate","B","",{}}};log.objects={{odd_bytes(),"",attributes},{odd_bytes(),odd_bytes(),{}}};
  log.relations={{odd_bytes(),odd_bytes(),odd_bytes()},{odd_bytes(),odd_bytes(),odd_bytes()}};log.o2o={{odd_bytes(),"",odd_bytes()}};log.e2e={{"",odd_bytes(),odd_bytes()}};
  log.object_changes={{odd_bytes(),"",odd_bytes(),odd_bytes(),std::numeric_limits<std::int64_t>::max()},{"","","","",-std::numeric_limits<double>::infinity()}};
  log.globals[odd_bytes()]=odd_bytes();log.globals["outside-legacy-prefix"]=std::int64_t{9};log.event_type_attributes[odd_bytes()][""]="";log.object_type_attributes[""][odd_bytes()]=odd_bytes();
  log.table_columns.events=std::vector<std::string>{};log.table_columns.objects=std::vector<std::string>{odd_bytes(),odd_bytes()};
  const auto json=serialize_oc_playout_workbench_ocel_json(log);const auto copy=parse_oc_playout_workbench_ocel_json(json);assert(serialize_oc_playout_workbench_ocel_json(copy)==json);
  assert(copy.events[0].id==odd_bytes()&&copy.events[0].attributes.size()==8&&copy.events[0].attributes[3].value.index()==2&&copy.events[0].attributes[4].value.index()==3);
  assert(same_double(std::get<double>(copy.events[0].attributes[4].value),-0.0)&&copy.globals.contains("outside-legacy-prefix"));
  assert(copy.table_columns.events&&copy.table_columns.events->empty()&&!copy.table_columns.relations&&copy.object_type_attributes.at("").at(odd_bytes())==odd_bytes());
  OcpnPlayoutResult result;result.log=log;result.sequences={{{"",{{odd_bytes(),""},{odd_bytes(),""}}}}};result.traces={{{odd_bytes(),{{odd_bytes(),odd_bytes()}},{{odd_bytes(),odd_bytes(),std::numeric_limits<std::size_t>::max()}}}}};
  result.trace_exists=true;
  const auto result_json=serialize_oc_playout_workbench_result_json(result);const auto restored=parse_oc_playout_workbench_result_json(result_json);assert(serialize_oc_playout_workbench_result_json(restored)==result_json&&!restored.return_sequences_requested&&!restored.return_traces_requested&&restored.trace_exists);
  rejects([&]{(void)serialize_ocpn_playout_json(restored,OcpnEventBinding::kIndependent);});
}
void strict_parser_and_byte_identity() {
  for(const auto* text:{R"({"max_traces":1.0})",R"({"max_states":-1})",R"({"seed":1e0})",R"({"event_binding":2147483648})",R"({"event_binding":{"index":0,"name":"synchronized"}})",R"({"event_binding":{"index":999,"name":"UNKNOWN"}})",R"({"exists_trace":1})",R"({"objects":[{"key":"x","value":[]},{"key":{"native_bytes":"hex-v1","data":"78"},"value":[]}]})",R"({"initial_markings":[{"key":"x","value":[4294967296]}]})",R"({"max_traces":0,"max_traces":1})",R"({"extra":0})",R"({"objects":{}})",R"({"start":null})"})rejects([&]{(void)parse_oc_playout_workbench_options_json(text);});
  for(const auto* text:{R"({"events":[{"attributes":[{"key":"x","value":{"index":5,"value":null}}]}]})",R"({"objects":[{"id":{"native_bytes":"hex-v1","data":"f"}}]})",R"({"globals":[{"key":"x","value":{"index":2,"value":9223372036854775808}}]})"})rejects([&]{(void)parse_oc_playout_workbench_ocel_json(text);});
  const auto parsed=parse_oc_playout_workbench_ocpn_json(R"({"activities":["\ud800",{"native_bytes":"hex-v1","data":"eda080edb080"},"\ud800\udc00","\u0000","日本語"]})");
  assert(parsed.activities[0]==std::string("\xed\xa0\x80",3)&&parsed.activities[1]==std::string("\xed\xa0\x80\xed\xb0\x80",6)&&parsed.activities[2]==std::string("\xf0\x90\x80\x80",4)&&parsed.activities[3]==std::string(1,'\0'));
  const auto copy=parse_oc_playout_workbench_ocpn_json(serialize_oc_playout_workbench_ocpn_json(parsed));assert(copy.activities==parsed.activities);
  std::string invalid="{\"activities\":[\"";invalid+='\xff';invalid+="\"]}";rejects([&]{(void)parse_oc_playout_workbench_ocpn_json(invalid);});
  rejects([]{(void)parse_oc_playout_workbench_petri_net_json(R"({"discovery_markings":{"detached_places":[{"name":{"index":7,"value":[{"key":"x","value":{"index":0,"value":null}},{"key":"x","value":{"index":0,"value":null}}]}}]}})");});
}
void actual_ocpn_surfaces_and_reachability() {
  auto model=graph(true);auto configured=options();configured.objects["item"]={"i1"};
  const auto model_before=serialize_oc_playout_workbench_ocpn_json(model);
  for(const auto binding:{OcpnEventBinding::kIndependent,OcpnEventBinding::kSharedActivity,OcpnEventBinding::kSynchronized}){
    configured.event_binding=binding;configured.objects_unique_per_trace=true;
    const auto options_before=serialize_oc_playout_workbench_options_json(configured);
    const auto result=ocpn_extensive_playout_result(parse_oc_playout_workbench_ocpn_json(model_before),configured);
    const auto scalar=ocpn_extensive_playout(model,configured);
    assert(result.trace_exists&&result.sequences.size()==1&&result.traces.size()==1&&serialize_oc_playout_workbench_ocel_json(result.log)==serialize_oc_playout_workbench_ocel_json(scalar));
    assert(serialize_oc_playout_workbench_options_json(configured)==options_before);
    assert(result.log.events.size()==(binding==OcpnEventBinding::kIndependent?4:2));
    assert(result.log.objects[0].id.find("_1")!=std::string::npos&&result.sequences[0][0].objects[0].second.find("_1")==std::string::npos);
    const auto legacy=serialize_ocpn_playout_json(result,binding);assert(legacy.find("traces")!=std::string::npos);
    assert(serialize_ocpn_playout_json(scalar).find("\"event_binding\":\"independent\"")!=std::string::npos);
    assert(!serialize_ocpn_playout_json(scalar,binding).empty());
    configured.exists_trace=true;configured.max_traces=0;const auto exists=ocpn_extensive_playout_result(model,configured);
    assert(exists.trace_exists&&exists.log.events.empty()&&exists.exists_trace_requested&&exists.return_sequences_requested&&exists.return_traces_requested);
    assert(binding==OcpnEventBinding::kSynchronized?exists.sequences.empty():!exists.sequences.empty());
    configured.exists_trace=false;configured.max_traces=8;
  }
  assert(serialize_oc_playout_workbench_ocpn_json(model)==model_before);
  configured=options();
  configured.initial_markings["order"]={0,1,0};configured.final_markings["order"]={0,0,1};
  auto one=ocpn_extensive_playout_result(graph(),configured);assert(one.log.events.size()==1&&one.log.events[0].activity=="Ship");
  configured.initial_markings["missing"]={};rejects([&]{(void)ocpn_extensive_playout_result(graph(),configured);});
  configured=options();configured.branching_factor_activities=std::numeric_limits<double>::quiet_NaN();assert(ocpn_extensive_playout_result(graph(),configured).trace_exists);
  configured.event_binding=static_cast<OcpnEventBinding>(-2147483647);rejects([&]{(void)ocpn_extensive_playout_result(graph(),configured);});
}
void concrete_witnesses_and_seeded_caps() {
  auto configured=options();auto duplicate=graph(false,true);const auto concrete=ocpn_extensive_playout_result(duplicate,configured);
  assert(concrete.traces.size()==2&&concrete.traces[0][0].transitions[0].transition_index==0&&concrete.traces[1][0].transitions[0].transition_index==1);
  configured.return_traces=false;const auto words=ocpn_extensive_playout_result(duplicate,configured);assert(words.log.events.size()==2&&words.traces.empty());
  PetriNetLiteralStorage state;state.place_count=2;state.source_storage=0;state.sink_storage=1;state.has_source=true;state.has_sink=true;state.activities={"A","B"};
  PetriTransition a;a.activity=0;a.inputs={0};a.outputs={1};auto b=a;b.activity=1;state.transitions={a,b};OcpnGraph branch;branch.petri_nets["order"]=PetriNet::from_literal_storage(state);
  configured=options();configured.seed=std::numeric_limits<std::uint64_t>::max();configured.branching_factor_transitions=1.5;configured.branching_factor_bindings=1.5;
  for(const auto binding:{OcpnEventBinding::kIndependent,OcpnEventBinding::kSharedActivity,OcpnEventBinding::kSynchronized}){configured.event_binding=binding;const auto first=ocpn_extensive_playout_result(branch,configured);const auto repeat=ocpn_extensive_playout_result(branch,configured);assert(serialize_oc_playout_workbench_result_json(first)==serialize_oc_playout_workbench_result_json(repeat));assert(first.traces.size()<=2);}
  configured.branching_factor_transitions=0;configured.branching_factor_bindings=0;assert(ocpn_extensive_playout_result(branch,configured).traces.size()==2);
}
void actual_causal_surfaces_marker_defaults_and_fallback() {
  auto model=causal_chain();auto configured=options();
  const auto before=serialize_oc_playout_workbench_causal_net_json(model);
  for(const auto binding:{OcpnEventBinding::kIndependent,OcpnEventBinding::kSharedActivity,OcpnEventBinding::kSynchronized}){
    configured.event_binding=binding;const auto result=oc_causal_net_extensive_playout_result(parse_oc_playout_workbench_causal_net_json(before),configured);const auto scalar=oc_causal_net_extensive_playout(model,configured);
    assert(result.trace_exists&&result.log.events.size()==2&&result.traces[0][0].transitions[0].transition_index==0&&result.traces[0][1].transitions[0].transition_index==1);
    assert(serialize_oc_playout_workbench_ocel_json(result.log)==serialize_oc_playout_workbench_ocel_json(scalar));
    assert(!serialize_oc_causal_net_playout_json(result,binding).empty()&&!serialize_oc_causal_net_playout_json(scalar,binding).empty()&&!serialize_oc_causal_net_playout_json(scalar).empty());
    configured.exists_trace=true;configured.max_traces=0;const auto exists=oc_causal_net_extensive_playout_result(model,configured);assert(exists.trace_exists&&exists.sequences.empty()&&exists.traces.empty()&&exists.log.events.empty());configured.exists_trace=false;configured.max_traces=8;
  }
  assert(serialize_oc_playout_workbench_causal_net_json(model)==before);
  configured=options();configured.initial_markings["not-in-model"]={std::numeric_limits<std::uint32_t>::max()};configured.final_markings[""]={};model.activities[0].input_marker_groups[0].support_count=0;
  assert(oc_causal_net_extensive_playout_result(model,configured).trace_exists);
  configured.max_states=0;rejects([&]{(void)oc_causal_net_extensive_playout_result(model,configured);});
  OcCausalNet fallback;fallback.workflow_projection=graph();fallback.object_types={"ignored"};fallback.activities={{"",{},{}},{"",{},{}}};configured=options();
  assert(serialize_oc_playout_workbench_result_json(oc_causal_net_extensive_playout_result(fallback,configured))==serialize_oc_playout_workbench_result_json(ocpn_extensive_playout_result(fallback.workflow_projection,configured)));
  configured.branching_factor_activities=std::numeric_limits<double>::infinity();assert(oc_causal_net_extensive_playout_result(fallback,configured).trace_exists);
  fallback.activities[0].input_marker_groups.push_back({});rejects([&]{(void)oc_causal_net_extensive_playout_result(fallback,configured);});
  auto invalid=model;invalid.activities[0].input_marker_groups[0].markers[0].min_cardinality=2;invalid.activities[0].input_marker_groups[0].markers[0].max_cardinality=1;rejects([&]{(void)oc_causal_net_extensive_playout_result(invalid,options());});
}
void actual_causal_binding_cardinality_and_activity_cap() {
  OcCausalNet both;both.object_types={"item","order"};both.activities={{"Close",{group({marker("START_item","item",1,1,1),marker("START_order","order",1,1,2)})},{group({marker("END_item","item",1,1,1),marker("END_order","order",1,1,2)})}}};
  auto configured=options();configured.objects["item"]={"i1"};
  for(const auto binding:{OcpnEventBinding::kIndependent,OcpnEventBinding::kSharedActivity,OcpnEventBinding::kSynchronized}){configured.event_binding=binding;const auto result=oc_causal_net_extensive_playout_result(both,configured);assert(result.trace_exists&&result.log.events.size()==(binding==OcpnEventBinding::kIndependent?2:1));}
  OcCausalNet batch;batch.object_types={"item"};batch.activities={{"Batch",{group({marker("START_item","item",2,kOcCausalInfiniteCardinality,std::numeric_limits<std::int64_t>::min())})},{group({marker("END_item","item",2,kOcCausalInfiniteCardinality,std::numeric_limits<std::int64_t>::max())})}}};configured.objects.clear();configured.objects["item"]={"i1","i2"};assert(oc_causal_net_extensive_playout_result(batch,configured).trace_exists);
  OcCausalNet alternatives;alternatives.object_types={"item"};alternatives.activities={{"A",{group({marker("START_item","item")})},{}},{"B",{group({marker("START_item","item")})},{}}};configured.objects["item"]={"i1"};configured.branching_factor_activities=1;configured.seed=17;
  const auto capped=oc_causal_net_extensive_playout_result(alternatives,configured);assert(capped.sequences.size()==1);
  configured.branching_factor_activities=1.5;const auto first=oc_causal_net_extensive_playout_result(alternatives,configured);assert(serialize_oc_playout_workbench_result_json(first)==serialize_oc_playout_workbench_result_json(oc_causal_net_extensive_playout_result(alternatives,configured)));
  configured.branching_factor_activities=0;assert(oc_causal_net_extensive_playout_result(alternatives,configured).sequences.size()==2);
  configured.branching_factor_activities=std::numeric_limits<double>::quiet_NaN();rejects([&]{(void)oc_causal_net_extensive_playout_result(alternatives,configured);});
  OcCausalNet keyed;keyed.object_types={"item"};
  keyed.activities={
      {"Seed",{group({marker("START_item","item",1,1,1)})},{group({marker("A","item",1,1,2),marker("B","item",1,1,3)})}},
      {"A",{group({marker("Seed","item",1,1,4)})},{group({marker("Join","item",1,1,5)})}},
      {"B",{group({marker("Seed","item",1,1,6)})},{group({marker("Join","item",1,1,7)})}},
      {"Join",{group({marker("A","item",1,1,7),marker("B","item",1,1,7)})},{group({marker("END_item","item",1,1,7)})}}};
  configured=options();configured.objects.clear();configured.objects["item"]={"i1"};
  auto decoded=parse_oc_playout_workbench_causal_net_json(serialize_oc_playout_workbench_causal_net_json(keyed));
  assert(!oc_causal_net_extensive_playout_result(decoded,configured).trace_exists);
  decoded.activities[3].input_marker_groups[0].markers[1].marker_key=8;assert(oc_causal_net_extensive_playout_result(decoded,configured).trace_exists);
  decoded.activities[3].input_marker_groups[0].markers[0].marker_key=0;decoded.activities[3].input_marker_groups[0].markers[1].marker_key=0;assert(oc_causal_net_extensive_playout_result(decoded,configured).trace_exists);
}
void zero_bounds_empty_objects_and_negative_epoch() {
  auto configured=options();configured.max_states=0;assert(!ocpn_extensive_playout_result(graph(),configured).trace_exists);
  configured=options();configured.max_traces=0;assert(!ocpn_extensive_playout_result(graph(),configured).trace_exists);
  configured=options();configured.objects["order"]={};assert(!ocpn_extensive_playout_result(graph(),configured).trace_exists);
  const auto causal=oc_causal_net_extensive_playout_result(causal_chain(),configured);assert(causal.trace_exists&&causal.log.events.empty()&&causal.sequences.size()==1&&causal.sequences[0].empty());
  configured=options();configured.objects.clear();const auto synthesized=ocpn_extensive_playout_result(graph(),configured);assert(synthesized.trace_exists&&synthesized.log.objects[0].id=="order_1");
  configured.start=-1;configured.case_interval=-0.0;configured.event_interval=std::numeric_limits<double>::denorm_min();const auto negative=oc_causal_net_extensive_playout_result(causal_chain(),configured);assert(negative.trace_exists&&negative.log.events[0].timestamp.find("1969-")==0);
}
void actual_reset_bounds_and_legacy_export_separation() {
  PetriTransition valid;valid.inputs={0};valid.outputs={1};valid.reset_inputs={2};std::vector<std::uint32_t> marking={1,0,7};fire_petri_transition(valid,marking);assert((marking==std::vector<std::uint32_t>{0,1,0}));
  auto invalid=valid;invalid.reset_inputs={3};marking={1,0,7};assert(petri_transition_enabled(invalid,marking));rejects<std::out_of_range>([&]{fire_petri_transition(invalid,marking);});
  PetriNetLiteralStorage state;state.place_count=3;state.source_storage=0;state.sink_storage=1;state.has_source=true;state.has_sink=true;state.activities={"A"};invalid.activity=0;state.transitions={invalid};OcpnGraph bad;bad.petri_nets["order"]=PetriNet::from_literal_storage(state);auto configured=options();
  for(const auto binding:{OcpnEventBinding::kIndependent,OcpnEventBinding::kSharedActivity,OcpnEventBinding::kSynchronized}){configured.event_binding=binding;rejects<std::out_of_range>([&]{(void)ocpn_extensive_playout_result(bad,configured);});}
  auto inactive=causal_chain();inactive.workflow_projection=bad;assert(oc_causal_net_extensive_playout_result(inactive,options()).trace_exists);
  state.transitions[0].reset_inputs.clear();state.activities={""};auto empty_label=bad;empty_label.petri_nets["order"]=PetriNet::from_literal_storage(state);const auto result=ocpn_extensive_playout_result(empty_label,options());assert(result.trace_exists&&result.log.events[0].activity.empty());
  assert(!serialize_oc_playout_workbench_result_json(result).empty());rejects([&]{(void)serialize_ocpn_playout_json(result,OcpnEventBinding::kIndependent);});
}
}  // namespace
int main() {
  try {
    std::cerr<<"defaults/options/exact integers\n";defaults_options_and_exact_numbers();
    std::cerr<<"literal Petri storage without normalization\n";literal_petri_storage();
    std::cerr<<"carrier keeps native place names only when the net has them\n";carrier_keeps_native_place_names();
    std::cerr<<"complete graph/annotation/causal state\n";complete_graph_annotations_and_causal_storage();
    std::cerr<<"complete raw OCEL and inactive result\n";full_raw_ocel_and_inactive_result();
    std::cerr<<"strict JSON and byte identity\n";strict_parser_and_byte_identity();
    std::cerr<<"actual OCPN log/result/reachability\n";actual_ocpn_surfaces_and_reachability();
    std::cerr<<"actual witnesses and seeded caps\n";concrete_witnesses_and_seeded_caps();
    std::cerr<<"actual causal surfaces/fallback\n";actual_causal_surfaces_marker_defaults_and_fallback();
    std::cerr<<"actual causal binding/cardinality/caps\n";actual_causal_binding_cardinality_and_activity_cap();
    std::cerr<<"zero bounds/empty objects/negative epoch\n";zero_bounds_empty_objects_and_negative_epoch();
    std::cerr<<"native reset bounds and independent legacy errors\n";actual_reset_bounds_and_legacy_export_separation();
  }catch(const std::exception& error){std::cerr<<"Object-centric playout workbench test exception: "<<error.what()<<'\n';return 1;}
}
