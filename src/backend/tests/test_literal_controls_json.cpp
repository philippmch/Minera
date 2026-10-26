#include "minera/literal_controls_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/json_export.hpp"
#include "minera/log_conversion.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>

using namespace minera;
namespace {
using Fields=std::map<std::string,std::string>;
FieldLookup lookup(const Fields& fields){return [&fields](std::string_view name){const auto i=fields.find(std::string(name));return i==fields.end()?std::string{}:i->second;};}
FieldPresence present(const Fields& fields){return [&fields](std::string_view name){return fields.contains(std::string(name));};}
template<class Exception=std::invalid_argument,class Fn>void rejects(Fn make){bool caught=false;try{make();}catch(const Exception&){caught=true;}assert(caught);}
std::string odd(){return std::string("\xff\xed\xa0\x80\xed\xb0\x80\0z",9);}
EventLog log_of(std::vector<std::string> words={"A","B"}){
 EventLog log;Trace trace;trace.case_id="case1";std::size_t order=0;
 for(const auto& word:words){Event event;event.case_id=trace.case_id;event.activity=word;event.timestamp=order?"2024-01-01T01:00:00Z":"2024-01-01T00:00:00Z";event.input_order=order++;event.attributes.push_back({" amount ","2.5",AttributeKind::kFloat});event.attributes.push_back({" kind "," x ",AttributeKind::kString});trace.events.push_back(std::move(event));}
 log.traces.push_back(std::move(trace));return log;
}
void endpoint_applicable_opt_in(){
 const std::vector<std::pair<std::string,std::vector<std::string>>> groups={
  {"target",{"target_activities_json"}},
  {"features",{"features_event_string_attributes_json","features_event_numeric_attributes_json","features_case_string_attributes_json","features_case_numeric_attributes_json","features_event_attributes_json","features_case_attributes_json","features_event_successor_attributes_json","features_feature_names_json"}},
  {"ilp",{"ilp_options_json"}},{"dfg_precision",{"dfg_precision_options_json"}}};
 for(const auto& [owner,names]:groups)for(const auto& name:names){
  // Empty named multipart content is still present. Only its endpoint opts in;
  // every other endpoint keeps its original query/file handling path.
  const Fields files={{name,""}};
  for(const auto& [endpoint,unused]:groups){(void)unused;assert(literal_controls_requested(endpoint,present(files))==(endpoint==owner));}
 }
 assert(!literal_controls_requested("features",{}));
 Fields wrong_group={{"ilp_options_json","{}"}};
 assert(!literal_controls_requested("target",present(wrong_group)));
 assert(parse_log_to_target_params(lookup(wrong_group),present(wrong_group)).activities.empty());
 wrong_group["target_activities_json"]="[]";
 assert(literal_controls_requested("target",present(wrong_group)));
}
void successful_control_parse_prefix(){
 std::vector<std::string> parsed;
 Fields fields={{"features_event_string_attributes_json",R"([" first "])"},{"features_event_numeric_attributes_json","["},{"features_case_string_attributes_json",R"(["unreached"])"}};
 rejects([&]{(void)parse_log_to_features_literal_params(lookup(fields),present(fields),&parsed);});
 assert(parsed==std::vector<std::string>{"features_event_string_attributes_json"});
 parsed.clear();fields["features_event_string_attributes_json"]=R"(["whole vector fails",1])";
 rejects([&]{(void)parse_log_to_features_literal_params(lookup(fields),present(fields),&parsed);});assert(parsed.empty());
 fields={{"features_variant","unknown"},{"features_event_string_attributes_json","[]"}};
 rejects([&]{(void)parse_log_to_features_literal_params(lookup(fields),present(fields),&parsed);});assert(parsed.empty());
 fields={{"features_event_string_attributes_json","[]"},{"features_event_string_attributes","legacy"}};
 rejects([&]{(void)parse_log_to_features_literal_params(lookup(fields),present(fields),&parsed);});assert(parsed.empty());
 fields={{"features_event_string_attributes_json","[]"},{"features_event_numeric_attributes_json","[]"},{"features_feature_names_json","[]"}};
 const auto result=parse_log_to_features_literal_params(lookup(fields),present(fields),&parsed);
 assert((parsed==std::vector<std::string>{"features_event_string_attributes_json","features_event_numeric_attributes_json","features_feature_names_json"}));
 assert(result.event_string_attributes_supplied&&result.event_numeric_attributes_supplied&&result.feature_names_supplied);
 // Each object's member metadata is published only after complete parsing.
 std::set<std::string> members={"previous"};
 rejects([&]{(void)parse_literal_ilp_options_json(R"({"alpha":1,"max_nodes":false})",&members);});assert(members==std::set<std::string>{"previous"});
 rejects([&]{(void)parse_literal_dfg_precision_options_json(R"({"activity_attribute":"A","max_rows_to_materialize":false})",&members);});assert(members==std::set<std::string>{"previous"});
 (void)parse_literal_dfg_precision_options_json(R"({"max_rows_to_materialize":null})",&members);assert(members==std::set<std::string>{"max_rows_to_materialize"});
}
void literal_vectors_and_structural_errors(){
 const auto values=parse_literal_string_vector_json(R"(["", " A ", "a,b", "x=>y", "\\", "\u0000", "日本語", {"native_bytes":"hex-v1","data":"ffeda080edb080007a"}," A "])");
 assert(values.size()==9&&values[0].empty()&&values[1]==" A "&&values[2]=="a,b"&&values[3]=="x=>y"&&values[4]=="\\"&&values[5]==std::string(1,'\0')&&values[7]==odd()&&values[8]==values[1]);
 assert(parse_literal_string_vector_json("[]").empty()&&parse_literal_string_vector_json(R"([""])").size()==1);
 for(const auto* text:{"null","{}","[1]",R"([{"native_bytes":"unknown","data":"ff"}])",R"([{"native_bytes":"hex-v1","data":"f"}])",R"([{"native_bytes":"hex-v1","data":"gg"}])",R"([{"native_bytes":"hex-v1","data":"ff","extra":0}])"})rejects([&]{(void)parse_literal_string_vector_json(text);});
 std::string malformed="[\"";malformed+='\xff';malformed+="\"]";rejects([&]{(void)parse_literal_string_vector_json(malformed);});
}
void target_exact_and_legacy_are_independent(){
 Fields fields={{"target_activities_json",R"([" B "," A "] )"}};auto options=parse_log_to_target_literal_params(lookup(fields),present(fields));assert((options.activities==std::vector<std::string>{" B "," A "}));
 const auto result=log_to_target(log_of({" A "," B "}),options);assert(result.classes==options.activities&&result.rows[0][0][0]==1&&result.rows[0][0][1]==0);
 fields["target_activities_json"]="[]";options=parse_log_to_target_literal_params(lookup(fields),present(fields));assert(options.activities.empty()&&log_to_target(log_of(),options).classes.size()==2);
 fields["target_activities_json"]=R"([""])";options=parse_log_to_target_literal_params(lookup(fields),present(fields));assert(options.activities.size()==1&&options.activities[0].empty());rejects([&]{(void)log_to_target(log_of(),options);});
 fields["target_activities_json"]=R"(["A","A"])";options=parse_log_to_target_literal_params(lookup(fields),present(fields));assert(options.activities.size()==2);rejects([&]{(void)log_to_target(log_of(),options);});
 fields["target_activities"]="legacy";rejects([&]{(void)parse_log_to_target_literal_params(lookup(fields),present(fields));});
 fields={{"target_activities","  A  , B "}};assert(parse_log_to_target_literal_params(lookup(fields),present(fields)).activities==parse_log_to_target_params(lookup(fields),present(fields)).activities);
 fields["target_activities"]="";rejects([&]{(void)parse_log_to_target_literal_params(lookup(fields),present(fields));});
 fields={{"target_activities_json",R"([{"native_bytes":"hex-v1","data":"ffeda080edb080007a"}])"}};options=parse_log_to_target_literal_params(lookup(fields),present(fields));const auto raw=log_to_target(log_of({odd(),odd()}),options);
 assert(raw.classes[0]==odd()&&literal_controls_transport_error(log_to_target_json(raw))==std::optional<std::string>{"non_scalar_utf8"});assert(!literal_controls_transport_error(serialize_literal_controls_result_json(raw)));
}
void all_eight_feature_vectors_and_six_presence_flags(){
 const std::vector<std::string> suffixes={"event_string_attributes","event_numeric_attributes","case_string_attributes","case_numeric_attributes","event_attributes","case_attributes","event_successor_attributes","feature_names"};
 Fields fields;for(const auto& suffix:suffixes)fields["features_"+suffix+"_json"]=R"(["", " name ", "a,b", {"native_bytes":"hex-v1","data":"ffeda080edb080007a"}, " name "])";
 const auto options=parse_log_to_features_literal_params(lookup(fields),present(fields));
 const auto expected=parse_literal_string_vector_json(fields.begin()->second);
 assert(options.event_string_attributes==expected&&options.event_numeric_attributes==expected&&options.case_string_attributes==expected&&options.case_numeric_attributes==expected&&options.event_attributes==expected&&options.case_attributes==expected&&options.event_successor_attributes==expected&&options.feature_names==expected);
 assert(options.event_string_attributes_supplied&&options.event_numeric_attributes_supplied&&options.case_string_attributes_supplied&&options.case_numeric_attributes_supplied&&options.event_successor_attributes_supplied&&options.feature_names_supplied);
 const auto json=serialize_literal_controls_options_json(options);assert(!literal_controls_transport_error(json)&&json.find("native_bytes")!=std::string::npos&&json.find("\"case_id_key\":null")!=std::string::npos);
 fields.clear();for(const auto& suffix:suffixes)fields["features_"+suffix+"_json"]="[]";
 const auto empty=parse_log_to_features_literal_params(lookup(fields),present(fields));assert(empty.event_attributes.empty()&&empty.case_attributes.empty()&&empty.event_string_attributes_supplied&&empty.feature_names_supplied);
 fields={{"features_event_attributes_json","[]"}};const auto generic=parse_log_to_features_literal_params(lookup(fields),present(fields));assert(!generic.event_string_attributes_supplied&&!generic.case_string_attributes_supplied&&!generic.feature_names_supplied);
 fields={{"features_event_string_attributes_json","[]"},{"feature_event_string_attributes","legacy"}};rejects([&]{(void)parse_log_to_features_literal_params(lookup(fields),present(fields));});
 fields={{"features_event_string_attributes_json",R"(["literal"] )"},{"feature_case_numeric_attributes","amount"}};const auto mixed=parse_log_to_features_literal_params(lookup(fields),present(fields));assert(mixed.event_string_attributes[0]=="literal"&&mixed.case_numeric_attributes[0]=="amount");
 fields={{"features_event_string_attributes","plural"},{"feature_event_string_attributes","singular"}};assert(parse_log_to_features_literal_params(lookup(fields),present(fields)).event_string_attributes==std::vector<std::string>{"plural"});
 fields={{"features_feature_names",R"( left ,\e,comma\,name)"}};const auto legacy=parse_log_to_features_literal_params(lookup(fields),present(fields));assert((legacy.feature_names==std::vector<std::string>{" left ","","comma,name"}));
}
void actual_feature_projections_and_native_errors(){
 const auto log=log_of();
 Fields fields={{"features_variant","event"},{"features_event_numeric_attributes_json",R"([" amount "])"},{"features_event_string_attributes_json","[]"},{"features_case_string_attributes_json","[]"},{"features_case_numeric_attributes_json","[]"}};
 auto options=parse_log_to_features_literal_params(lookup(fields),present(fields));auto result=log_to_features(log,options);assert(std::find(result.feature_names.begin(),result.feature_names.end(),"event: amount ")!=result.feature_names.end());
 fields["features_variant"]="trace";options=parse_log_to_features_literal_params(lookup(fields),present(fields));result=log_to_features(log,options);assert(result.variant==LogToFeaturesVariant::TRACE_BASED&&result.trace_rows.size()==1);
 fields={{"features_variant","event"},{"features_event_attributes_json",R"([""])"}};options=parse_log_to_features_literal_params(lookup(fields),present(fields));assert(options.event_attributes[0].empty());rejects([&]{(void)log_to_features(log,options);});
 auto temporal_log=log;for(auto& trace:temporal_log.traces)for(auto& event:trace.events){event.resource="worker";event.attributes.push_back({"org:resource","worker",AttributeKind::kString});}
 fields={{"features_variant","temporal"},{"features_window_seconds","3600"},{"features_feature_names_json",R"([""])"}};options=parse_log_to_features_literal_params(lookup(fields),present(fields));assert(options.grouper_freq.empty()&&options.feature_names_supplied&&options.feature_names[0].empty());bool rejected_empty_schema=false;try{(void)log_to_features(temporal_log,options);}catch(const std::invalid_argument& error){rejected_empty_schema=true;assert(std::string(error.what())=="log_to_features: feature_names contains empty name");}assert(rejected_empty_schema);
 fields={{"features_variant","temporal"},{"features_feature_names_json",R"([""])"}};options=parse_log_to_features_literal_params(lookup(fields),present(fields));result=log_to_features(temporal_log,options);assert(result.variant==LogToFeaturesVariant::TEMPORAL&&!result.temporal_rows.empty()&&options.feature_names==std::vector<std::string>{""}&&std::find(result.feature_names.begin(),result.feature_names.end(),"unique_resources")!=result.feature_names.end());
 fields={{"features_variant","event"},{"features_feature_names_json",R"(["",{"native_bytes":"hex-v1","data":"ffeda080edb080007a"}])"}};options=parse_log_to_features_literal_params(lookup(fields),present(fields));result=log_to_features(log,options);assert(result.feature_names==options.feature_names);assert(!literal_controls_transport_error(serialize_literal_controls_result_json(result)));assert(literal_controls_transport_error(log_to_features_json(result))==std::optional<std::string>{"non_scalar_utf8"});
}
void ilp_native_defaults_order_bytes_and_full_limit(){
 const auto defaults=parse_literal_ilp_options_json("{}");assert(defaults.activity_key.empty()&&defaults.artificial_start_activity=="▶"&&defaults.artificial_end_activity=="■"&&defaults.alpha==1&&defaults.noise_threshold==0&&defaults.max_nodes==100000&&defaults.causal_relation.empty());
 std::set<std::string> supplied;const auto exact=parse_literal_ilp_options_json("{\"activity_key\":\"\",\"artificial_start_activity\":\"\",\"artificial_end_activity\":\"\",\"causal_relation\":[[\"A,=>\",\" B \"],[\"A,=>\",\" B \"],[\"\",{\"native_bytes\":\"hex-v1\",\"data\":\"ffeda080edb080007a\"}]],\"max_nodes\":"+std::to_string(std::numeric_limits<std::size_t>::max())+"}",&supplied);
 assert(exact.artificial_start_activity.empty()&&exact.artificial_end_activity.empty()&&exact.causal_relation.size()==3&&exact.causal_relation[0]==exact.causal_relation[1]&&exact.causal_relation[2].second==odd()&&exact.max_nodes==std::numeric_limits<std::size_t>::max()&&supplied.size()==5);
 const auto tiny=parse_literal_ilp_options_json("{\"artificial_start_activity\":\"\",\"artificial_end_activity\":\"\",\"causal_relation\":[[\"A\",\"B\"],[\"A\",\"B\"]],\"max_nodes\":"+std::to_string(std::numeric_limits<std::size_t>::max())+"}");
 // Empty boundary labels are outside the reference port's domain (the
 // reference itself raises for this causal relation), so discovery rejects.
 rejects([&]{(void)mine_ilp(log_of(),tiny);});
 auto labelled=tiny;labelled.artificial_start_activity="▶";labelled.artificial_end_activity="■";labelled.causal_relation={{"▶","A"},{"A","B"},{"B","■"}};
 const auto result=mine_ilp(log_of(),labelled);assert(result.diagnostics.computed&&result.net.activities()==std::vector<std::string>({"A","B"})&&result.markings.initial.size()==result.net.place_count());assert(!literal_controls_transport_error(serialize_literal_controls_result_json(result)));
 auto invalid=parse_literal_ilp_options_json(R"({"alpha":"NaN","noise_threshold":-0.0,"max_nodes":0})");assert(std::isnan(invalid.alpha)&&std::signbit(invalid.noise_threshold)&&invalid.max_nodes==0);rejects([&]{(void)mine_ilp(log_of(),invalid);});
 Fields fields={{"ilp_options_json","{}"},{"ilp_alpha","1"}};rejects([&]{(void)parse_ilp_literal_params(lookup(fields),present(fields));});
 fields={{"ilp_artificial_start_activity",""}};rejects([&]{(void)parse_ilp_literal_params(lookup(fields),present(fields));});
 for(const auto* text:{R"({"max_nodes":1.0})",R"({"max_nodes":-1})",R"({"max_nodes":18446744073709551616})",R"({"alpha":true})",R"({"alpha":1,"alpha":0})",R"({"extra":0})",R"({"causal_relation":[["A"]]})"}){std::set<std::string> sentinel={"kept"};rejects([&]{(void)parse_literal_ilp_options_json(text,&sentinel);});assert(sentinel==std::set<std::string>{"kept"});}
}
void dfg_default_presence_caps_and_actual_flat_views(){
 std::set<std::string> supplied;auto defaults=parse_literal_dfg_precision_options_json("{}",&supplied);assert(defaults.activity_attribute=="concept:name"&&defaults.case_id_attribute=="case:concept:name"&&!defaults.max_rows_to_materialize&&supplied.empty());
 const auto explicit_null=parse_literal_dfg_precision_options_json(R"({"max_rows_to_materialize":null})",&supplied);assert(!explicit_null.max_rows_to_materialize&&supplied==std::set<std::string>{"max_rows_to_materialize"});
 const auto cap=parse_literal_dfg_precision_options_json("{\"activity_attribute\":\" label \",\"case_id_attribute\":\" case \",\"max_rows_to_materialize\":"+std::to_string(std::numeric_limits<std::size_t>::max())+"}");assert(cap.activity_attribute==" label "&&cap.case_id_attribute==" case "&&*cap.max_rows_to_materialize==std::numeric_limits<std::size_t>::max());
 DfgCleanAnalysis model;model.graph[{"A","B"}]=1;model.start_activities["A"]=1;model.end_activities["B"]=1;
 EventStream stream;EventTable table;table.columns={" case "," label "};for(const auto* name:{"A","B"}){EventRecord event;event.fields={{" case ","c",AttributeKind::kString},{" label ",name,AttributeKind::kString}};stream.events.push_back(event);EventTable::Row row;row.emplace_back(event.fields[0]);row.emplace_back(event.fields[1]);table.rows.push_back(std::move(row));}
 LazyEventTable lazy(table.columns,[rows=table.rows](const LazyEventTable::RowConsumer& consume){for(const auto& row:rows)consume(row);},{},{},table.rows.size());
 for(const auto view:{EventDataView(stream),EventDataView(table),EventDataView(lazy)}){const auto result=evaluate_dfg_precision(view,model,cap);assert(result.computed&&result.precision==1&&result.escaping_transitions==0);auto zero=cap;zero.max_rows_to_materialize=0;rejects<std::length_error>([&]{(void)evaluate_dfg_precision(view,model,zero);});}
 defaults.max_rows_to_materialize=2;assert(evaluate_dfg_precision(EventDataView(log_of()),model,defaults).precision==1);defaults.max_rows_to_materialize=0;rejects<std::length_error>([&]{(void)evaluate_dfg_precision(EventDataView(log_of()),model,defaults);});
 const auto empty=parse_literal_dfg_precision_options_json(R"({"activity_attribute":"","case_id_attribute":""})");assert(empty.activity_attribute.empty()&&empty.case_id_attribute.empty());rejects([&]{(void)evaluate_dfg_precision(EventDataView(stream),model,empty);});
 for(const auto* bad:{R"({"max_rows_to_materialize":false})",R"({"case_id_attribute":null})",R"({"extra":0})",R"({"activity_attribute":"A","activity_attribute":"B"})"})rejects([&]{(void)parse_literal_dfg_precision_options_json(bad);});
}
void full_result_inspection_and_legacy_export_errors(){
 LogToFeaturesResult result;result.feature_names={"",""};result.case_ids={odd()};result.event_rows={{{-0.0,std::numeric_limits<double>::quiet_NaN()}}};
 EventAttribute attribute;attribute.key=odd();attribute.value=odd();attribute.kind=AttributeKind::kString;attribute.children={{odd(),odd()}};attribute.typed_value=std::numeric_limits<std::uint64_t>::max();
 result.trace_rows={{attribute,std::numeric_limits<double>::infinity()}};result.temporal_rows={{-0.0,{std::string{},-std::numeric_limits<double>::infinity()},odd()}};
 const auto json=serialize_literal_controls_result_json(result);assert(!literal_controls_transport_error(json)&&json.find("event-attribute-via-raw-log-v1")!=std::string::npos&&json.find("native_byte_strings")!=std::string::npos&&json.find("18446744073709551615")!=std::string::npos);
 rejects([&]{(void)log_to_features_json(result);});
 ILPMinerResult ilp;ilp.diagnostics.message=odd();ilp.diagnostics.visited_nodes=std::numeric_limits<std::size_t>::max();ilp.diagnostics.lp_solved=std::numeric_limits<std::size_t>::max();const auto ilp_json=serialize_literal_controls_result_json(ilp);assert(!literal_controls_transport_error(ilp_json)&&ilp_json.find("source_storage")!=std::string::npos);
 DfgPrecisionResult dfg;dfg.computed=false;dfg.precision=std::numeric_limits<double>::quiet_NaN();dfg.prefix_occurrences=std::numeric_limits<std::uint64_t>::max();dfg.error=odd();assert(!literal_controls_transport_error(serialize_literal_controls_result_json(dfg)));rejects([&]{(void)serialize_dfg_precision_json(dfg);});
 assert(!literal_controls_transport_error(R"({"family":"OK","value":-0.0})"));assert(literal_controls_transport_error("{\"value\":nan}")==std::optional<std::string>{"invalid_json"});
 assert(literal_controls_transport_error("[]")==std::optional<std::string>{"invalid_json"});assert(literal_controls_transport_error(std::string("{\"x\":\"")+odd()+"\"}")==std::optional<std::string>{"non_scalar_utf8"});
}
}
int main(){
 try{
  std::cerr<<"endpoint-applicable opt-in\n";endpoint_applicable_opt_in();
  std::cerr<<"successful control parse prefix\n";successful_control_parse_prefix();
  std::cerr<<"literal vectors and structural errors\n";literal_vectors_and_structural_errors();
  std::cerr<<"target exact and legacy semantics\n";target_exact_and_legacy_are_independent();
  std::cerr<<"eight feature vectors/six presence flags\n";all_eight_feature_vectors_and_six_presence_flags();
  std::cerr<<"actual feature projections/native errors\n";actual_feature_projections_and_native_errors();
  std::cerr<<"ILP native defaults/order/full limit\n";ilp_native_defaults_order_bytes_and_full_limit();
  std::cerr<<"DFG native defaults/caps/flat views\n";dfg_default_presence_caps_and_actual_flat_views();
  std::cerr<<"full result and independent legacy errors\n";full_result_inspection_and_legacy_export_errors();
 }catch(const std::exception& error){std::cerr<<"Literal control test exception: "<<error.what()<<'\n';return 1;}
}
