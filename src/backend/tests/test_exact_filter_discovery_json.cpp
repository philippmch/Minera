#include "minera/exact_filter_discovery_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/workflow_input_result_json.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>
using namespace minera;
namespace {
void has(std::string_view text,std::string_view expected,std::source_location where=std::source_location::current()){
 if(text.find(expected)==std::string_view::npos){std::cerr<<where.line()<<": missing "<<expected.substr(0,1024)<<"\nactual "<<text.substr(0,2048)<<'\n';assert(false);}
}
template<class Call>void rejects(Call call,std::string_view expected){try{call();assert(false);}catch(const std::invalid_argument& e){has(e.what(),expected);}}
std::string replace(std::string text,std::string_view from,std::string_view to){const auto at=text.find(from);assert(at!=std::string::npos);text.replace(at,from.size(),to);return text;}
EventLog log(){EventLog r;Trace t;t.case_id="case";for(const auto* name:{"A,B"," B ","A,B"}){Event e;e.case_id=t.case_id;e.activity=name;t.events.push_back(e);}r.traces.push_back(t);return r;}
void values_and_options(){
 const auto values=parse_exact_ltl_values_json(R"(["A,B"," B ","","A,B",{"native_bytes":"hex-v1","data":"00ffeda080"},"\ud800"])");
 assert(values.size()==6&&values[0]=="A,B"&&values[1]==" B "&&values[2].empty()&&values[3]==values[0]);assert(values[4]==std::string("\0\xff\xed\xa0\x80",5));assert(values[5]==std::string("\xed\xa0\x80",3));
 LtlFilterOptions o;o.variant=static_cast<LtlFilterVariant>(-7);o.attribute_key=values[4];o.resource_key=values[5];o.attribute_values=values;o.timestamp_diff_boundaries={{-0.0,std::numeric_limits<double>::denorm_min()},{-std::numeric_limits<double>::max(),std::numeric_limits<double>::max()},{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()},{-std::numeric_limits<double>::infinity(),-1}};
 const auto json=serialize_exact_ltl_options_json(o);const auto parsed=parse_exact_ltl_options_json(json);assert(serialize_exact_ltl_options_json(parsed)==json);assert(std::signbit(parsed.timestamp_diff_boundaries[0].first));assert(parsed.timestamp_diff_boundaries[0].second==std::numeric_limits<double>::denorm_min());
 rejects([&]{parse_exact_ltl_options_json("{}");},"seven");rejects([&]{parse_exact_ltl_options_json(replace(json,"\"positive\":true","\"positive\":null"));},"boolean");
 rejects([]{parse_exact_ltl_values_json("[null]");},"NativeString");rejects([]{parse_exact_ltl_values_json(R"([{"native_bytes":"hex-v1","data":"f"}])");},"carrier");
 const auto pairs=parse_exact_heuristic_pairs_json(R"([[" A ","B"],[" A ","B"],[{"native_bytes":"hex-v1","data":"ff"},""],["a->b","c,d"]])");assert(pairs.size()==3&&pairs.contains({" A ","B"})&&pairs.contains({std::string("\xff",1),""}));
 rejects([]{parse_exact_heuristic_pairs_json("[[true,1]]");},"NativeString");rejects([]{parse_exact_heuristic_pairs_json("[[\"A\"]]");},"exactly two");
}
void full_heuristic(){
 HeuristicMinerOptions o;o.variant=static_cast<HeuristicMinerVariant>(-3);o.decoration=static_cast<HeuristicNetDecoration>(8);o.performance_aggregation=static_cast<HeuristicPerformanceAggregation>(-2);o.activity_attribute=std::string("a\0\xff",3);o.case_id_attribute=" case ";o.min_frequency=std::numeric_limits<std::uint64_t>::max();o.min_activity_count=0;o.dependency_threshold=std::numeric_limits<double>::quiet_NaN();o.plusplus_min_activity_count=-0.0;o.plusplus_min_frequency=std::numeric_limits<double>::infinity();o.start_timestamp_attribute="";o.plusplus_decoration_selector=std::string("\xed\xa0\x80",3);o.business_hour_slots={{-1,std::numeric_limits<double>::infinity()},{-0.0,std::numeric_limits<double>::denorm_min()}};o.business_hour_slots_explicit=true;o.most_common_paths={{o.activity_attribute,""}};
 HeuristicStructuredValue value{HeuristicStructuredValue::Object{{o.activity_attribute,HeuristicStructuredValue{std::uint64_t{18446744073709551615ull}}},{"null",HeuristicStructuredValue{nullptr}},{"signed",HeuristicStructuredValue{std::int64_t{-9223372036854775807ll-1}}}}};
 for(int i=0;i<80;++i)value=HeuristicStructuredValue{HeuristicStructuredValue::Array{value}};
 o.plusplus_decoration_value=value;
 const auto json=serialize_exact_heuristic_options_json(o);const auto parsed=parse_exact_heuristic_options_json(json);assert(serialize_exact_heuristic_options_json(parsed)==json);assert(static_cast<int>(parsed.variant)==-3&&parsed.case_id_attribute==" case ");
 rejects([]{parse_exact_heuristic_options_json("{}");},"missing field");
 rejects([&]{parse_exact_heuristic_options_json(replace(json,"\"min_frequency\":18446744073709551615","\"min_frequency\":18446744073709551616"));},"uint64");
 rejects([&]{parse_exact_heuristic_options_json(replace(json,"\"most_common_paths\":[","\"unexpected\":1,\"most_common_paths\":["));},"unknown native record field");
}
void dfg_and_primary_call(){
 const auto byte=std::string("\xff",1);HeuristicClassicDfgInput d;d.dfg={{{" A ","B"},1},{{" A ","X"},100},{{"Y","B"},100},{{byte,"B"},1}};d.activities=std::vector<std::string>{" A ","B","X","Y",byte,byte};d.activity_counts=std::map<std::string,std::uint64_t>{{byte,1}};d.start_counts=std::map<std::string,std::uint64_t>{};d.end_counts=std::nullopt;d.window_two_dfg={{{byte,"B"},2}};d.frequency_triples={{{byte," A ","B"},3}};d.performance_dfg=HeuristicPerformanceDfg{{{byte,"B"},{-0.0,std::numeric_limits<double>::denorm_min(),std::numeric_limits<double>::infinity()}}};d.start_activities=HeuristicBoundaryActivities{std::vector<std::string>{byte,byte}};d.end_activities=HeuristicBoundaryActivities{std::map<std::string,std::uint64_t>{{byte,0}}};d.performance_values=HeuristicDecoratedDfg{{{byte,"B"},HeuristicStructuredValue{HeuristicStructuredValue::Object{{byte,HeuristicStructuredValue{true}}}}}};d.dfg_order=std::vector<std::pair<std::string,std::string>>{{byte,"B"},{byte,"B"}};d.dfg_window_2_supplied=false;d.freq_triples_supplied=true;
 const auto json=serialize_exact_heuristic_dfg_json(d);const auto parsed=parse_exact_heuristic_dfg_json(json);assert(serialize_exact_heuristic_dfg_json(parsed)==json);assert(parsed.activities->size()==6&&parsed.dfg_order->size()==2);assert(!parsed.dfg_window_2_supplied&&parsed.freq_triples_supplied);
 rejects([&]{parse_exact_heuristic_dfg_json(replace(json,"\"dfg\":[","\"dfg\":[{\"key\":[\" A \",\"B\"],\"value\":0},"));},"duplicate decoded native map key");
 const auto partial=parse_exact_heuristic_dfg_json(R"({"dfg":[{"source":{"native_bytes":"hex-v1","data":"ff"},"target":"B","count":1}]})");assert(partial.dfg.contains({byte,"B"}));
 HeuristicClassicDfgInput witness;witness.dfg={{{" A ","B"},1},{{" A ","X"},100},{{"Y","B"},100}};HeuristicMinerOptions options;options.dfg_pre_cleaning_noise_threshold=.5;options.dependency_threshold=0;
 const auto unprotected=discover_heuristics_net(witness,options);assert(!unprotected.cleaned_dfg.contains({" A ","B"}));options.most_common_paths={{" A ","B"}};const auto protected_net=discover_heuristics_net(witness,options);assert(protected_net.cleaned_dfg.contains({" A ","B"}));
 witness.dfg={{{byte,"B"},1},{{byte,"X"},100},{{"Y","B"},100}};options.most_common_paths={{byte,"B"}};
 ExactFilterDiscoveryContext c;ExactFilterDiscoveryScope scope(c);const auto native=observe_exact_heuristic_call(witness,options,[&]{return discover_heuristics_net(witness,options);});assert(native.cleaned_dfg.contains({byte,"B"})&&c.heuristic_entered&&c.heuristic_returned);
 try{workflow_input_heuristics_json(native);assert(false);}catch(const std::invalid_argument& e){record_exact_optional_artifact("legacy_heuristics_json",e.what(),true);}
 const auto response=exact_filter_discovery_response(c,"discover_workflow_input",200,"{}");has(response,"\"returned\":true");has(response,"\"native_bytes\":\"hex-v1\"");has(response,"legacy_heuristics_json_error");assert(!response.starts_with("{,"));
 const auto original=serialize_exact_heuristic_dfg_json(witness);assert(c.call_source_before==c.call_source_after&&original==serialize_exact_heuristic_dfg_json(witness));
}
void legacy_constructor_boundary_label(){
 const auto legacy=parse_exact_heuristic_dfg_json(R"({"dfg":[{"source":"index","target":"B","count":1}],"activities":["index","B"],"activity_counts":{"index":1,"B":1},"start_counts":{"index":1},"end_counts":{"B":1},"window_two_dfg":[],"frequency_triples":[],"performance_dfg":[],"start_activities":{"index":1},"end_activities":{"B":1},"performance_values":[],"dfg_window_2_supplied":false,"freq_triples_supplied":false,"dfg_order":[{"source":"index","target":"B"}]})");
 assert(legacy.dfg.contains({"index","B"}));assert(legacy.start_activities&&legacy.start_activities->index()==1);
 assert((std::get<std::map<std::string,std::uint64_t>>(*legacy.start_activities).at("index")==1));
 assert(legacy.activities->at(0)=="index"&&legacy.dfg_order->at(0).first=="index");
}
void envelope_and_parser(){
 ExactFilterDiscoveryContext c;ExactFilterDiscoveryScope scope(c);MineParams p;p.heuristic.activity_attribute="before";record_exact_parsed_options(p);p.heuristic.activity_attribute="after";record_exact_effective_options(p);has(c.parsed_heuristic,"before");has(c.configured_heuristic,"after");
 record_exact_heuristic_begin("null",p.heuristic);HeuristicsNet n;record_exact_heuristic_end("null",p.heuristic,&n);const auto safe=exact_filter_discovery_response(c,"mine",200,"{}");has(safe,"\"source_type\":null");assert(safe.starts_with("{\"exact_controls\":"));
 const auto fallback=exact_filter_discovery_response(c,"mine",400,std::string("{\"bad\":\"\xff\"}"));has(fallback,"EXACT_FILTER_DISCOVERY");has(fallback,"\"json\":null");
 std::map<std::string,std::string> fields{{"ltl_attribute_values_json",R"(["A,B"," B ","A,B"])"}};const auto get=[&](std::string_view k){const auto i=fields.find(std::string(k));return i==fields.end()?std::string{}:i->second;};const auto present=[&](std::string_view k){return fields.contains(std::string(k));};const auto opts=parse_mine_params(get,present);assert(opts.ltl->attribute_values.size()==3&&opts.ltl->attribute_values[1]==" B ");fields["ltl_attribute_values"]="";rejects([&]{parse_mine_params(get,present);},"conflicts by presence");fields.erase("ltl_attribute_values");fields["ltl_attribute_values_file"]="[]";rejects([&]{parse_mine_params(get,present);},"JSON and File conflict");
 fields={{"heuristic_options_json",serialize_exact_heuristic_options_json(HeuristicMinerOptions{})},{"dependency_threshold",""}};rejects([&]{parse_mine_params(get,present);},"conflicts by presence");
}
void standalone(){
 auto source=log();source.attributes.push_back({std::string("\0\xff",2),"raw",AttributeKind::kString});source.metadata.compatibility_snapshot_valid=true;source.metadata.compatibility_globals_snapshot.push_back({"trace",{{"key","snapshot",AttributeKind::kString}}});
 const auto source_json=serialize_embedding_workbench_raw_log_json(source);const auto parsed=parse_exact_ltl_log_json(source_json);assert(serialize_embedding_workbench_raw_log_json(parsed)==source_json);
 LtlFilterOptions o;o.attribute_values={"A,B"," B ","A,B"};const auto options_json=serialize_exact_ltl_options_json(o);
 std::vector<DiscoveryWorkbenchPart> parts={{"source_file","",source_json,true},{"ltl_options_file","options.json",options_json,true}};
 auto result=execute_exact_ltl_filter(parts);assert(result.status==200);has(result.json,"\"native_returned\":true");has(result.json,"\"source_unchanged\":true");has(result.json,"compatibility_globals_snapshot");has(result.json,"\"filename\":\"\"");
 parts[1].bytes=replace(options_json,"\"variant_index\":0","\"variant_index\":99");result=execute_exact_ltl_filter(parts);assert(result.status==400);has(result.json,"\"native_entered\":true");has(result.json,"unknown variant");has(result.json,"\"variant_index\":99");
 parts[0].file=false;result=execute_exact_ltl_filter(parts);assert(result.status==400);has(result.json,"actual File");has(result.json,"\"native_entered\":false");
 parts[0].file=true;parts[1].bytes=options_json;parts.push_back(parts[1]);result=execute_exact_ltl_filter(parts);assert(result.status==400);has(result.json,"duplicate original LTL control");parts.pop_back();
 parts={{"source_file","input.csv","case_id,activity,timestamp\n1,\"A,B\",\n1, B ,\n1,\"A,B\",\n",true},{"source_format","","import",false},{"ltl_options_json","",options_json,false}};
 result=execute_exact_ltl_filter(parts);assert(result.status==200);has(result.json,"\"detected_format\":\"csv\"");
}
}
int main(){values_and_options();full_heuristic();dfg_and_primary_call();legacy_constructor_boundary_label();envelope_and_parser();standalone();}
