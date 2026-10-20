#include "minera/saved_dfg_projection_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/graph_alignment_workbench_json.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace minera;
namespace {
void require(bool v,const char* why){if(!v)throw std::runtime_error(why);}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::invalid_argument&){bad=true;}require(bad,"expected invalid argument");}
DfgAlignmentLogResult supplied(){DfgAlignmentResult a,b;a.alignment={{"a","a"},{"b","b"}};b.alignment={{"a","a"},{"b",">>"},{">>","b"}};b.cost=b.internal_cost=20000;return {{a,b},1};}
EventLog source(){Event a,b;a.activity="a";a.timestamp="time-0";b.activity="b";b.timestamp="time-1";Trace t;t.case_id="duplicate";t.events={a,b};EventLog l;l.traces={t,t};return l;}
AlphaDfgDiscoveryInput model(){return parse_saved_dfg_projection_model_json(R"({"kind":"clean","dfg":[{"source":"a","target":"b","count":1}],"start_activities":{"a":1},"end_activities":{"b":1}})");}
void complete_result(){
 auto r=supplied();auto& a=r.alignments[1];a.computed=false;a.cost=-0.0;a.internal_cost=std::numeric_limits<double>::infinity();a.fitness=std::numeric_limits<double>::quiet_NaN();a.best_worst_cost=-std::numeric_limits<double>::infinity();a.visited_states=std::numeric_limits<std::size_t>::max();a.closed=9007199254740993ULL;a.closed_states=4;r.unique_variants=std::numeric_limits<std::size_t>::max();a.alignment.push_back({std::string("\xff\0",2),std::string("\xed\xa0\x80",3)});
 const auto raw=serialize_saved_dfg_alignment_result_json(r);auto back=parse_saved_dfg_alignment_result_json(raw);
 require(raw.find("ff00")!=std::string::npos&&raw.find("eda080")!=std::string::npos,"byte labels and WTF8 safe serialization");require(back.unique_variants==r.unique_variants&&back.alignments[1].closed==9007199254740993ULL&&back.alignments[1].visited_states==a.visited_states,"complete unrounded counters");require(!back.alignments[1].computed&&std::signbit(back.alignments[1].cost)&&std::isnan(back.alignments[1].fitness)&&back.alignments[1].internal_cost==a.internal_cost&&back.alignments[1].best_worst_cost==a.best_worst_cost,"complete nonfinite and false flags");require(back.alignments[1].alignment.back().log==a.alignment.back().log&&back.alignments[1].alignment.back().model==a.alignment.back().model,"complete byte routes");
 const auto escaped=parse_saved_dfg_alignment_result_json(R"({"alignments":[{"computed":true,"alignment":[{"log":"\ud800","model":""}],"cost":-1.5,"internal_cost":"INF","fitness":"NaN","best_worst_cost":"-INF","visited_states":"18446744073709551615","closed":"0002","closed_states":0}],"unique_variants":"9007199254740993"})");
 require(escaped.alignments[0].alignment[0].log==std::string("\xed\xa0\x80",3)&&escaped.alignments[0].alignment[0].model.empty()&&escaped.alignments[0].closed==2,"escaped legacy surrogate, empty labels and exact decimal strings");
 for(const auto shape:{"native_result","workbench","report"}){const auto input=std::string(shape)=="native_result"?raw:std::string(shape)=="workbench"?"{\"result\":"+raw+",\"opaque\":99999999999999999999999999}":"{\"native_result\":{\"result\":"+raw+"},\"request\":{}}";require(serialize_saved_dfg_alignment_result_json(parse_saved_dfg_alignment_result_json(input,shape))==raw,"explicit full envelope extraction");}
 rejects([&]{(void)parse_saved_dfg_alignment_result_json(raw,"infer");});
 for(const auto bad:{"{}","{\"alignments\":[],\"unique_variants\":1.0}","{\"alignments\":[],\"unique_variants\":-1}","{\"alignments\":[],\"unique_variants\":18446744073709551616}","{\"alignments\":[],\"unique_variants\":\" 1\"}","{\"alignments\":[],\"unique_variants\":0,\"unique_variants\":0}","{\"alignments\":[],\"unique_variants\":0,\"other\":0}"})rejects([&]{(void)parse_saved_dfg_alignment_result_json(bad);});
 auto corrupt=raw;auto at=corrupt.find("\"computed\":false");corrupt.replace(at,16,"\"computed\":null");rejects([&]{(void)parse_saved_dfg_alignment_result_json(corrupt);});
 corrupt=raw;at=corrupt.find("\"internal_cost\":\"INF\"");corrupt.replace(at,21,"\"internal_cost\":\"1\"");rejects([&]{(void)parse_saved_dfg_alignment_result_json(corrupt);});
}
void models_and_options(){
 const auto m=model();const auto canonical=serialize_saved_dfg_projection_model_json(m);require(parse_saved_dfg_projection_model_json(canonical).dfg==m.dfg,"canonical model reimport");
 auto bytes=parse_saved_dfg_projection_model_json(R"({"kind":"graph","dfg":[{"source":{"native_bytes":"hex-v1","data":"ff00"},"target":"\udfff","count":"18446744073709551615"}],"start_activities":[],"end_activities":[],"activities":[{"native_bytes":"hex-v1","data":"ff00"},"\udfff"]})");
 require(bytes.start_activities->empty()&&bytes.end_activities->empty()&&bytes.dfg.begin()->first.first==std::string("\xff\0",2)&&bytes.dfg.begin()->second==std::numeric_limits<std::uint64_t>::max(),"explicit empty boundaries and bytes/frequency");require(serialize_saved_dfg_projection_model_json(parse_saved_dfg_projection_model_json(serialize_saved_dfg_projection_model_json(bytes)))==serialize_saved_dfg_projection_model_json(bytes),"full native model roundtrip");
 for(const auto bad:{R"({"kind":"clean","dfg":[],"start_activities":{}})",R"({"kind":"clean","dfg":[],"start_activities":[{"activity":"a","count":1},{"activity":{"native_bytes":"hex-v1","data":"61"},"count":2}],"end_activities":{}})",R"({"kind":"clean","dfg":[{"source":"a","target":"b","count":1},{"source":"a","target":"b","count":2}],"start_activities":{},"end_activities":{}})"})rejects([&]{(void)parse_saved_dfg_projection_model_json(bad);});
 const auto selectors=parse_saved_dfg_projection_selectors_json(R"({"alignment_case_id_attribute":"","dfg_activity_attribute":{"native_bytes":"hex-v1","data":"ff00"},"dfg_timestamp_attribute":" Ω "})");require(selectors.at("alignment_case_id_attribute").empty()&&selectors.at("dfg_activity_attribute")==std::string("\xff\0",2)&&selectors.at("dfg_timestamp_attribute")==" Ω ","exact NativeString selectors");
 rejects([&]{(void)parse_saved_dfg_projection_selectors_json(R"({"dfg_activity_attribute":{"native_bytes":"hex-v1","data":"f"}})");});
 DfgAlignmentOptions o;o.activity_attribute=std::string("\xff\0",2);o.timestamp_attribute="";o.sync_cost_supplied=true;o.model_move_cost_supplied=true;o.log_move_cost_supplied=true;o.internal_log_move_cost_supplied=true;
 const auto options=serialize_saved_dfg_projection_options_json(o);require(options.find("ff00")!=std::string::npos&&options.find("\"sync_cost\":[]")!=std::string::npos&&options.find("\"sync_cost_supplied\":true")!=std::string::npos,"exact empty map presence and selectors retained");
}
void projection_witness(){
 const auto l=source();const auto m=model();const auto r=parse_saved_dfg_alignment_result_json(serialize_saved_dfg_alignment_result_json(supplied()));const DfgAlignmentOptions o;
 const auto projected=project_log_on_dfg(l,m.dfg,*m.start_activities,*m.end_activities,r,o);
 require(projected.traces.size()==2&&projected.traces[0].events[1].timestamp=="time-1"&&projected.traces[1].events[1].timestamp=="time-0","duplicate source traces consume distinct supplied routes");
 const auto metrics=project_alignments_on_dfg(l,m.dfg,*m.start_activities,*m.end_activities,r,o);
 require(metrics.activities.at("a").sync==2&&metrics.activities.at("b").sync==0&&metrics.activities.at("b").lm==1&&metrics.dfg.at({"a","b"}).sync==1&&metrics.dfg.at({"a","b"}).mm==0,"native adjacent-pair metrics retained");
 require(serialize_saved_dfg_projection_metrics_json(metrics)==serialize_dfg_alignment_projection_json(metrics),"conventional-label metric serialization equivalence");
 const auto snapshot=serialize_embedding_workbench_raw_log_json(projected);require(serialize_embedding_workbench_raw_log_json(parse_embedding_workbench_raw_log_json(snapshot))==snapshot,"complete lossless projection metadata snapshot");
 auto bad=r;bad.alignments.pop_back();rejects([&]{(void)project_log_on_dfg(l,m.dfg,*m.start_activities,*m.end_activities,bad,o);});rejects([&]{(void)project_alignments_on_dfg(l,m.dfg,*m.start_activities,*m.end_activities,bad,o);});
 bad=r;bad.alignments[1].computed=false;rejects([&]{(void)project_log_on_dfg(l,m.dfg,*m.start_activities,*m.end_activities,bad,o);});rejects([&]{(void)project_alignments_on_dfg(l,m.dfg,*m.start_activities,*m.end_activities,bad,o);});
 bad=r;bad.alignments[0].alignment.push_back({"b","b"});rejects([&]{(void)project_log_on_dfg(l,m.dfg,*m.start_activities,*m.end_activities,bad,o);});
 EventLog empty;DfgAlignmentLogResult no_rows;const auto empty_model=parse_saved_dfg_projection_model_json(R"({"kind":"clean","dfg":[],"start_activities":{},"end_activities":{}})");require(project_log_on_dfg(empty,empty_model.dfg,*empty_model.start_activities,*empty_model.end_activities,no_rows,o).traces.empty(),"empty log and supplied result native projection");
 auto custom=l;for(auto& t:custom.traces)for(auto& e:t.events){e.attributes.push_back({" act ",e.activity,AttributeKind::kString});e.attributes.push_back({" time ",e.timestamp,AttributeKind::kString});}DfgAlignmentOptions c;c.activity_attribute=" act ";c.timestamp_attribute=" time ";require(project_log_on_dfg(custom,m.dfg,*m.start_activities,*m.end_activities,r,c).traces[1].events[1].timestamp=="time-0","native exact custom selectors");
 c.timestamp_attribute="missing";rejects([&]{(void)project_log_on_dfg(custom,m.dfg,*m.start_activities,*m.end_activities,r,c);});
 auto odd=metrics;odd.activities.emplace(std::string("\xff\0",2),DfgProjectedActivityCounts{1,2,3});require(serialize_saved_dfg_projection_metrics_json(odd).find("ff00")!=std::string::npos,"byte-safe metrics independent of optional legacy presentation");
}
}
int main(){try{complete_result();models_and_options();projection_witness();std::cout<<"saved DFG projection contracts passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
