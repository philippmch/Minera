#include "minera/temporal_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/quality_workbench_json.hpp"
#include "minera/footprints_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include "minera/active_event_source.hpp"
#include "minera/json_export.hpp"
#include "minera/log_conversion.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
using namespace minera;
namespace {
// An independent fragment slicer, not the implementation's bounded parser.
// Canonical outputs may be deeper/larger than accepted input documents.
std::map<std::string,std::string> fields(const std::string& text){
 assert(text.size()>=2&&text.front()=='{'&&text.back()=='}');std::map<std::string,std::string> out;std::size_t at=1;
 while(at+1<text.size()){
  assert(text[at++]=='"');const auto begin=at;while(text[at]!='"')++at;const auto name=text.substr(begin,at-begin);assert(text[++at]==':');++at;
  const auto start=at;int depth=0;bool quoted=false;
  for(;at<text.size();++at){const auto c=text[at];if(quoted){if(c=='\\')++at;else if(c=='"')quoted=false;continue;}if(c=='"')quoted=true;else if(c=='{'||c=='[')++depth;else if(c=='}'||c==']'){if(!depth)break;--depth;}else if(c==','&&!depth)break;}
  assert(out.emplace(name,text.substr(start,at-start)).second);if(text[at]==',')++at;
 }
 return out;
}
std::vector<std::string> elements(const std::string& text){
 assert(text.size()>=2&&text.front()=='['&&text.back()==']');std::vector<std::string> out;std::size_t at=1;
 while(at+1<text.size()){const auto start=at;int depth=0;bool quoted=false;for(;at<text.size();++at){const auto c=text[at];if(quoted){if(c=='\\')++at;else if(c=='"')quoted=false;continue;}if(c=='"')quoted=true;else if(c=='{'||c=='[')++depth;else if(c=='}'||c==']'){if(!depth)break;--depth;}else if(c==','&&!depth)break;}out.push_back(text.substr(start,at-start));if(text[at]==',')++at;}
 return out;
}
std::string q(const std::string& value){return serialize_embedding_workbench_string_json(value);}
std::string text(const std::string& value){return parse_literal_string_vector_json("["+value+"]").front();}
std::string bytes(const std::string& carrier){
 const auto c=fields(carrier);const auto data=text(c.at("data"));std::string value;
 if(c.at("encoding")=="\"utf8\"")value=data;
 else{assert(c.at("encoding")=="\"hex\""&&data.size()%2==0);for(std::size_t i=0;i<data.size();i+=2)value+=static_cast<char>(std::stoul(data.substr(i,2),nullptr,16));}
 assert(c.at("bytes")==std::to_string(value.size())&&c.at("sha256")==q(footprints_workbench_sha256(value)));return value;
}
std::string raw(const EventLog& log){return serialize_embedding_workbench_raw_log_json(log);}
std::string all_bytes(){std::string value;for(int i=0;i<256;++i)value+=static_cast<char>(i);return value;}
struct Direct {bool returned=false;std::string value,message,type;};
Direct native(const std::function<std::string()>& function){
 try{return {true,function(),{}, {}};}catch(const std::invalid_argument& e){return {false,{},e.what(),"invalid_argument"};}catch(const std::out_of_range& e){return {false,{},e.what(),"out_of_range"};}catch(const std::length_error& e){return {false,{},e.what(),"length_error"};}catch(const std::runtime_error& e){return {false,{},e.what(),"runtime_error"};}
}
template<class F>void rejects(F function){bool rejected=false;try{function();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}
EventLog log_sample(){
 EventLog log;log.attributes={{"log-only","retained"}};log.metadata.extensions={{"ext","prefix","urn:source"}};log.metadata.classifiers={{"classifier",{"concept:name","extra"}}};
 log.metadata.trace_globals={{"global-trace","retained"}};log.metadata.event_globals={{"global-event","retained"}};log.metadata.properties={{"property","retained"}};log.metadata.globals={{"event",{{"ordered","one"}}},{"event",{{"ordered","two"}}}};
 log.metadata.compatibility_snapshot_valid=true;log.metadata.compatibility_globals_snapshot={{"old",{{"snapshot","global"}}}};log.metadata.compatibility_trace_globals_snapshot={{"snapshot","trace"}};log.metadata.compatibility_event_globals_snapshot={{"snapshot","event"}};
 for(std::size_t i=0;i<6;++i){Trace t;t.case_id=i<2?"repeated":"case-"+std::to_string(i);t.attributes={{"group",i%2?"left":"right"},{"number",std::to_string(i),AttributeKind::kInt}};t.properties={{"trace-property","kept"}};
  const auto sequence=i<3?std::vector<std::string>{"A","B"}:std::vector<std::string>{"B","A","C"};for(std::size_t e=0;e<sequence.size();++e){Event v;v.case_id="event-id-not-trace-id";v.activity=sequence[e];v.timestamp=std::to_string(e);v.resource="resource-"+std::to_string(i%2);v.lifecycle=e%2?"complete":"start";v.input_order=e;v.attributes={{"alternate",sequence[e]},{"amount",std::to_string(i+e),AttributeKind::kFloat},{"choice",i%2?"L":"R"}};t.events.push_back(std::move(v));}log.traces.push_back(std::move(t));}
 return log;
}
EventLog full_log(){
 auto log=log_sample();for(auto& t:log.traces)for(std::size_t i=0;i<t.events.size();++i){auto& e=t.events[i];e.timestamp=std::array<std::string,3>{"2026-10-19T08:00:00Z","2026-10-19T09:00:00Z","2026-10-19T10:00:00Z"}[i];e.attributes.push_back({"org:group",i%2?"G2":"G1"});e.attributes.push_back({"case:custom","joined"});e.attributes.push_back({"resource-x",e.resource});e.attributes.push_back({"activity-x",e.activity});e.attributes.push_back({"stamp-x",e.timestamp});e.attributes.push_back({"start-x","2026-10-19T07:00:00Z"});}
 const auto octets=all_bytes();log.attributes.push_back({octets,octets});auto& a=log.attributes.back();a.kind=static_cast<AttributeKind>(-11);a.container=static_cast<AttributeContainerKind>(-7);a.scalar_wrapper=true;a.scalar_children_list=true;a.first_class_role=static_cast<FirstClassAttributeRole>(-9);a.first_class_state=static_cast<FirstClassAttributeState>(-3);a.first_class_source_key=octets;a.children={{"i","inactive",AttributeKind::kInt},{"u","inactive",AttributeKind::kInt},{"nan","NaN",AttributeKind::kFloat},{"zero","-0",AttributeKind::kFloat},{"date",octets,AttributeKind::kDate},{"bool","false",AttributeKind::kBoolean}};
 a.children[0].typed_value=std::numeric_limits<std::int64_t>::max();a.children[1].typed_value=std::numeric_limits<std::uint64_t>::max();a.children[2].typed_value=std::numeric_limits<double>::quiet_NaN();a.children[3].typed_value=-0.0;a.children[4].typed_value=NativeDate{octets,static_cast<NativeDateResolution>(-4),octets};a.children[5].typed_value=false;
 log.metadata.compatibility_globals_snapshot.push_back({octets,{{octets,octets}}});log.metadata.compatibility_trace_globals_snapshot.push_back(a);log.metadata.compatibility_event_globals_snapshot.push_back(a);
 const auto raw=serialize_embedding_workbench_raw_log_json(log);assert(raw.starts_with("{\"native_byte_strings\":\"hex-v1\""));const auto parsed=parse_embedding_workbench_raw_log_json(raw);assert(serialize_embedding_workbench_raw_log_json(parsed)==raw);
 const auto metadata=fields(fields(fields(raw).at("value")).at("metadata"));assert(metadata.size()==10&&metadata.at("compatibility_snapshot_valid")=="true");
 assert(elements(metadata.at("compatibility_globals_snapshot")).size()==2&&elements(metadata.at("compatibility_trace_globals_snapshot")).size()==2&&elements(metadata.at("compatibility_event_globals_snapshot")).size()==2);
 return log;
}
const std::array<std::string_view,21> operations={"discover_temporal_profile","discover_temporal_profile_log","discover_temporal_profile_dataframe","discover_temporal_profile","discover_temporal_profile","discover_temporal_profile","discover_temporal_profile","check_temporal_profile_conformance","conform_temporal_profile_log","conform_temporal_profile_dataframe","conform_temporal_profile","check_temporal_profile_conformance","conformance_temporal_profile","parse_temporal_profile_json","temporal_profile_json","temporal_conformance_json","serialize_active_source_temporal_profile_json","serialize_active_source_temporal_conformance_json","TemporalProfileEntry::stddev","TemporalDeviation::distance","TemporalProfile::find"};
const std::array<std::string_view,21> signatures={"discover_temporal_profile(const EventLog&,const TemporalProfileOptions&)","discover_temporal_profile_log(const EventLog&,const TemporalProfileOptions&)","discover_temporal_profile_dataframe(const EventTable&,const TemporalProfileOptions&)","discover_temporal_profile(const EventStream&,const TemporalProfileOptions&)","discover_temporal_profile(const EventTable&,const TemporalProfileOptions&)","discover_temporal_profile(const LazyEventTable&,const TemporalProfileOptions&)","discover_temporal_profile(const EventDataView&,const TemporalProfileOptions&)","check_temporal_profile_conformance(const EventLog&,const TemporalProfile&,const TemporalProfileOptions&)","conform_temporal_profile_log(const EventLog&,const TemporalProfile&,const TemporalProfileOptions&)","conform_temporal_profile_dataframe(const EventTable&,const TemporalProfile&,const TemporalProfileOptions&)","conform_temporal_profile(const EventDataView&,const TemporalProfile&,const TemporalProfileOptions&)","check_temporal_profile_conformance(const EventLog&,const TemporalProfile&,double)","conformance_temporal_profile(const EventLog&,const TemporalProfile&,const TemporalProfileOptions&)","parse_temporal_profile_json(std::string_view)","temporal_profile_json(const TemporalProfile&)","temporal_conformance_json(const TemporalConformanceResult&)","serialize_active_source_temporal_profile_json(const TemporalProfile&)","serialize_active_source_temporal_conformance_json(const TemporalConformanceResult&)","TemporalProfileEntry::stddev() const noexcept","TemporalDeviation::distance() const noexcept","TemporalProfile::find(const std::string&,const std::string&) const noexcept"};
const std::array<std::string_view,21> input_types={"EventLog","EventLog","EventTable","EventStream","EventTable","LazyEventTable","EventDataView","EventLog","EventLog","EventTable","EventDataView","EventLog","EventLog","std::string_view","TemporalProfile","TemporalConformanceResult","TemporalProfile","TemporalConformanceResult","TemporalProfileEntry","TemporalDeviation","TemporalProfile"};
const std::array<std::string_view,21> result_types={"TemporalProfile","TemporalProfile","TemporalProfile","TemporalProfile","TemporalProfile","TemporalProfile","TemporalProfile","TemporalDeviationVector","TemporalConformanceResult","TemporalConformanceResult","TemporalConformanceResult","TemporalDeviationVector","TemporalDeviationVector","TemporalProfile","NativeString","NativeString","NativeString","NativeString","NativeFloat64","NativeFloat64","TemporalProfileEntryPointer"};
const std::array<std::string_view,21> source_kinds={"log","log","table","stream","table","lazy","view","log","log","table","view","log","log","","","","","","","",""};

using Parts=std::vector<DiscoveryWorkbenchPart>;
std::set<std::tuple<int,std::string,std::string>> observed;
const std::array<std::string,18> artifact_names={"source_input_json","source_after_json","profile_input_json","profile_after_json","receiver_input_json","receiver_after_json","conformance_input_json","conformance_after_json","configured_options_json","zeta_input_json","find_source_json","find_target_json","text_input_bytes","text_input_json","full_result_json","found_entry_json","native_output_bytes","lazy_scans_json"};
struct Fixture {
 EventLog log;EventStream stream;EventTable table;LazyEventTable lazy;std::shared_ptr<std::size_t> scans=std::make_shared<std::size_t>(0);
 explicit Fixture(EventLog supplied):log(std::move(supplied)),stream(event_log_to_event_stream(log)),table(event_log_to_table(log)){
  // Constructor-owned latent metadata is deliberate test input, including all
  // four compatibility fields; no older lossy inspection writer builds it.
  stream.metadata=log.metadata;table.metadata=log.metadata;
  auto owned=std::make_shared<const EventTable>(table);const auto count=scans;lazy=LazyEventTable(table.columns,[owned,count](const LazyEventTable::RowConsumer& consumer){++*count;for(const auto& row:owned->rows)consumer(row);},table.attributes,table.metadata,table.rows.size(),table.dtypes,table.index,table.categorical_dtypes);
 }
 EventDataView view(const std::string& kind)const{if(kind=="log")return EventDataView(log);if(kind=="stream")return EventDataView(stream);if(kind=="table")return EventDataView(table);return EventDataView(lazy);}
 std::string encoded(const std::string& kind)const{if(kind=="log")return raw(log);if(kind=="stream")return serialize_quality_workbench_stream_json(stream);if(kind=="table")return serialize_quality_workbench_table_json(table);return "{\"table\":"+serialize_quality_workbench_table_json(table)+",\"valid\":true,\"row_count\":"+std::to_string(table.rows.size())+'}';}
};
std::string pointer_value(const TemporalProfile& profile,const std::string& source,const std::string& target){const auto* pointer=profile.find(source,target);return "{\"index\":"+(pointer?std::to_string(pointer-profile.entries.data()):"null")+",\"entry\":"+(pointer?serialize_temporal_workbench_result_json(*pointer):"null")+'}';}
Direct direct(int index,Fixture& data,const std::string& view,const TemporalProfileOptions& options,bool defaults,const TemporalProfile& profile,const TemporalConformanceResult& deviations,const TemporalProfileEntry& entry,const TemporalDeviation& deviation,double zeta,const std::string& input_text,const std::string& from,const std::string& to){return native([&]()->std::string{
 switch(index){
 case 0:return serialize_temporal_workbench_result_json(defaults?discover_temporal_profile(data.log):discover_temporal_profile(data.log,options));
 case 1:return serialize_temporal_workbench_result_json(defaults?discover_temporal_profile_log(data.log):discover_temporal_profile_log(data.log,options));
 case 2:return serialize_temporal_workbench_result_json(defaults?discover_temporal_profile_dataframe(data.table):discover_temporal_profile_dataframe(data.table,options));
 case 3:return serialize_temporal_workbench_result_json(defaults?discover_temporal_profile(data.stream):discover_temporal_profile(data.stream,options));
 case 4:return serialize_temporal_workbench_result_json(defaults?discover_temporal_profile(data.table):discover_temporal_profile(data.table,options));
 case 5:return serialize_temporal_workbench_result_json(defaults?discover_temporal_profile(data.lazy):discover_temporal_profile(data.lazy,options));
 case 6:return serialize_temporal_workbench_result_json(defaults?discover_temporal_profile(data.view(view)):discover_temporal_profile(data.view(view),options));
 case 7:return serialize_temporal_workbench_result_json(defaults?check_temporal_profile_conformance(data.log,profile):check_temporal_profile_conformance(data.log,profile,options));
 case 8:return serialize_temporal_workbench_result_json(defaults?conform_temporal_profile_log(data.log,profile):conform_temporal_profile_log(data.log,profile,options));
 case 9:return serialize_temporal_workbench_result_json(defaults?conform_temporal_profile_dataframe(data.table,profile):conform_temporal_profile_dataframe(data.table,profile,options));
 case 10:return serialize_temporal_workbench_result_json(defaults?conform_temporal_profile(data.view(view),profile):conform_temporal_profile(data.view(view),profile,options));
 case 11:return serialize_temporal_workbench_result_json(check_temporal_profile_conformance(data.log,profile,zeta));
 case 12:return serialize_temporal_workbench_result_json(defaults?conformance_temporal_profile(data.log,profile):conformance_temporal_profile(data.log,profile,options));
 case 13:return serialize_temporal_workbench_result_json(parse_temporal_profile_json(std::string_view(input_text.data(),input_text.size())));
 case 14:return q(temporal_profile_json(profile));
 case 15:return q(temporal_conformance_json(deviations));
 case 16:return q(serialize_active_source_temporal_profile_json(profile));
 case 17:return q(serialize_active_source_temporal_conformance_json(deviations));
 case 18:return serialize_temporal_workbench_scalar_json(entry.stddev());
 case 19:return serialize_temporal_workbench_scalar_json(deviation.distance());
 case 20:return pointer_value(profile,from,to);
 default:throw std::logic_error("driver declaration");
 }
});}
Parts request(int index,const Fixture& data,const std::string& view,const std::string& form,const TemporalProfileOptions& options,const TemporalProfile& profile,const TemporalConformanceResult& conformance,const TemporalProfileEntry& entry,const TemporalDeviation& deviation,double zeta,const std::string& input_text,const std::string& from,const std::string& to,bool omit_options=false){
 Parts p{{"operation","",std::string(operations[static_cast<std::size_t>(index)]),false},{"call_form","",form,false}};
 const auto kind=std::string(source_kinds[static_cast<std::size_t>(index)]);if(!kind.empty()){p.push_back({"source_kind","",kind,false});if(kind=="view")p.push_back({"view_kind","",view,false});p.push_back({"source_file","source.json",data.encoded(kind=="view"?view:kind),true});}
 if(index>=7&&index<=12||index==14||index==16||index==20)p.push_back({"profile_file","profile.json",serialize_temporal_workbench_result_json(profile),true});
 if(index==15||index==17)p.push_back({"conformance_file","conformance.json",serialize_temporal_workbench_result_json(conformance),true});
 if(index==18||index==19)p.push_back({"receiver_file","receiver.json",index==18?serialize_temporal_workbench_result_json(entry):serialize_temporal_workbench_result_json(deviation),true});
 if(index==13)p.push_back({"text_file","moments.bin",input_text,true});
 if(form=="with_options"&&!omit_options)p.push_back({"options_json","",serialize_temporal_workbench_options_json(options),false});
 if(form=="with_zeta")p.push_back({"zeta_json","",serialize_temporal_workbench_scalar_json(zeta),false});
 if(index==20){p.push_back({"find_source_json","",q(from),false});p.push_back({"find_target_json","",q(to),false});}return p;
}
std::map<std::string,std::string> check(const TemporalWorkbenchResponse& response,const Direct& expected,int index,const std::string& form,const std::string& view){
 observed.emplace(index,form,view);const auto r=fields(response.json);assert(r.size()==27&&r.at("family")==q("ORIGINAL_TEMPORAL_WORKBENCH")&&r.at("version")==q("original-temporal-workbench-v1"));
 assert(r.at("native_entered")=="true"&&r.at("native_returned")== (expected.returned?"true":"false")&&r.at("executed")==r.at("native_returned"));
 std::size_t primaries=0;for(const auto& item:elements(r.at("native_calls"))){const auto c=fields(item);assert(c.size()==7&&c.at("entered")=="true");if(c.at("stage")==q("primary")){++primaries;assert(c.at("name")==q(std::string(signatures[static_cast<std::size_t>(index)]))&&c.at("input_kind")==q(std::string(input_types[static_cast<std::size_t>(index)]))&&c.at("default_arguments_used")== (form=="default_options"?"true":"false")&&c.at("returned")==r.at("native_returned")&&c.at("error")== (expected.returned?"null":q(expected.message)));}}
 assert(primaries==1);
 if(expected.returned){assert(response.status==200&&r.at("error")=="null");const auto result=fields(r.at("result"));assert(result.at("type")==q(std::string(result_types[static_cast<std::size_t>(index)]))&&result.at("value")==expected.value);}
 else{assert(response.status==400&&r.at("result")=="null"&&r.at("native_output")=="null");const auto error=fields(r.at("error"));assert(error.at("phase")==q("native_error")&&error.at("type")==q(expected.type)&&error.at("message")==q(expected.message));}
 const auto context=fields(r.at("input_context")),input=fields(r.at("input"));assert(context.size()==22&&context.at("size_t_bytes")==std::to_string(sizeof(std::size_t))&&context.at("int_bytes")==std::to_string(sizeof(int))&&input.at("before")==input.at("after"));
 assert(context.at("input_type")==q(std::string(input_types[static_cast<std::size_t>(index)])));
 const auto before=fields(input.at("before")),a=fields(r.at("artifact_bytes"));assert(a.size()==18);
 for(const auto& name:artifact_names){assert(a.contains(name));if(a.at(name)!="null"){const bool binary=name=="text_input_bytes"||name=="native_output_bytes";assert(fields(a.at(name)).at("dataset")==q(name+(binary?".bin":".json")));(void)bytes(a.at(name));}}
 for(const auto* name:{"profile","conformance","text","zeta"})if(before.at(name)!="null"){const auto key=std::string(name)+(std::string(name)=="text"?"_input_json":"_input_json");assert(bytes(a.at(key))==before.at(name));assert(context.at(std::string(name)+"_unchanged")=="true");}
 if(before.at("source")!="null"){const auto source=fields(before.at("source"));const auto value=source.at("kind")==q("lazy")?fields(source.at("value")).at("binding"):source.at("value");assert(bytes(a.at("source_input_json"))==value&&bytes(a.at("source_after_json"))==value&&context.at("source_unchanged")=="true");}
 if(before.at("receiver")!="null"){assert(bytes(a.at("receiver_input_json"))==fields(before.at("receiver")).at("value")&&bytes(a.at("receiver_after_json"))==fields(before.at("receiver")).at("value")&&context.at("receiver_unchanged")=="true");}
 if(context.at("options_prepared")=="true"){assert(bytes(a.at("configured_options_json"))==r.at("configured_options")&&context.at("options_unchanged")== (form=="default_options"?"null":"true"));}else assert(r.at("configured_options")=="null"&&a.at("configured_options_json")=="null"&&context.at("options_unchanged")=="null");
 if(index==20){assert(context.at("find_unchanged")=="true");const auto value=fields(fields(r.at("result")).at("value")),observation=fields(r.at("pointer_observation"));assert(observation.at("entry_index")==value.at("index")&&observation.at("is_null")== (value.at("entry")=="null"?"true":"false"));if(value.at("entry")!="null")assert(bytes(a.at("found_entry_json"))==value.at("entry"));}
 else assert(r.at("pointer_observation")=="null");
 if(expected.returned){assert(bytes(a.at("full_result_json"))==expected.value);if(index>=14&&index<=17){assert(bytes(a.at("native_output_bytes"))==text(expected.value)&&bytes(r.at("native_output"))==text(expected.value));}else assert(r.at("native_output")=="null"&&a.at("native_output_bytes")=="null");}
 return r;
}
std::map<std::string,std::string> compare(int index,Fixture& data,const TemporalProfileOptions& options={},const TemporalProfile& profile={},const std::string& form="with_options",const std::string& view="",const TemporalConformanceResult& conformance={},const TemporalProfileEntry& entry={},const TemporalDeviation& deviation={},double zeta=6,const std::string& input_text="[]",const std::string& from="A",const std::string& to="B",bool omit_options=false){
 const auto expected=direct(index,data,view,options,form=="default_options",profile,conformance,entry,deviation,zeta,input_text,from,to);
 const auto response=execute_temporal_workbench(request(index,data,view,form,options,profile,conformance,entry,deviation,zeta,input_text,from,to,omit_options));return check(response,expected,index,form,view);
}
void host_error(const TemporalWorkbenchResponse& response,const std::string& phase){const auto r=fields(response.json);assert(response.status==400&&r.at("native_entered")=="false"&&r.at("native_returned")=="false"&&r.at("result")=="null"&&fields(r.at("error")).at("phase")==q(phase));for(const auto& c:elements(r.at("native_calls")))assert(fields(c).at("stage")!=q("primary"));assert(fields(r.at("artifact_bytes")).size()==18);}
void all_forms(){Fixture data(full_log());const auto profile=discover_temporal_profile(data.log);
 for(int index=0;index<21;++index){if(index==11){compare(index,data,{},profile,"with_zeta");continue;}if(index>=13){compare(index,data,{},profile,"direct");continue;}
  const auto kinds=index==6||index==10?std::vector<std::string>{"log","stream","table","lazy"}:std::vector<std::string>{""};for(const auto& kind:kinds){compare(index,data,{},profile,"with_options",kind);compare(index,data,{},profile,"default_options",kind);}}
 assert(observed.size()==45);
 // One direct native source scanner call for each actual lazy discovery/view
 // conformance expression. No additional scan is needed for native snapshots.
 const auto lazy_run=compare(5,data);const auto scans=elements(fields(lazy_run.at("input_context")).at("lazy_scans"));assert(scans.size()==1);const auto scan=fields(scans.front());assert(scan.at("invocation")=="0"&&scan.at("completed")=="true"&&scan.at("error")=="null"&&elements(scan.at("yielded_rows")).size()==data.table.rows.size());
 const auto native_state=fields(fields(fields(fields(lazy_run.at("input")).at("before")).at("source")).at("value"));const auto getters=fields(native_state.at("native"));assert(getters.size()==9&&getters.at("native_byte_strings")==q("hex-v1")&&fields(getters.at("metadata")).size()==10);
 const auto table_metadata=fields(fields(fields(serialize_quality_workbench_table_json(data.table)).at("value")).at("metadata"));const auto getter_metadata=fields(getters.at("metadata"));
 for(const auto* field:{"compatibility_snapshot_valid","compatibility_globals_snapshot","compatibility_trace_globals_snapshot","compatibility_event_globals_snapshot"})assert(getter_metadata.at(field)==table_metadata.at(field));
}
void stored_domains(){Fixture data(full_log());const auto octets=all_bytes();TemporalProfile profile;profile.entries={{octets,std::string("\0",1),0,-0.0,-0.0},{octets,std::string("\0",1),std::numeric_limits<std::uint64_t>::max(),-1,std::numeric_limits<double>::infinity()}};
 const auto encoded=serialize_temporal_workbench_result_json(profile);assert(serialize_temporal_workbench_result_json(parse_temporal_workbench_profile_json(encoded))==encoded&&fields(elements(fields(encoded).at("entries")).back()).at("count")=="18446744073709551615");
 compare(20,data,{},profile,"direct","",{}, {},{},6,"[]",octets,std::string("\0",1));compare(20,data,{},profile,"direct","",{}, {},{},6,"[]","absent","");compare(14,data,{},profile,"direct");compare(16,data,{},profile,"direct");
 TemporalProfileEntry entry{std::string("\0",1),octets,std::numeric_limits<std::uint64_t>::max(),-1,std::numeric_limits<double>::quiet_NaN()};TemporalDeviation deviation{octets,octets,std::string("\0",1),-0.0,-std::numeric_limits<double>::infinity()};
 assert(serialize_temporal_workbench_result_json(parse_temporal_workbench_entry_json(serialize_temporal_workbench_result_json(entry)))==serialize_temporal_workbench_result_json(entry));assert(serialize_temporal_workbench_result_json(parse_temporal_workbench_deviation_json(serialize_temporal_workbench_result_json(deviation)))==serialize_temporal_workbench_result_json(deviation));
 for(const double value:{-0.0,-1.0,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()}){entry.standard_deviation_seconds=value;deviation.standard_deviation_distance=value;compare(18,data,{}, {},"direct","",{},entry);compare(19,data,{}, {},"direct","",{}, {},deviation);}
 TemporalConformanceResult c{{},{deviation},{deviation,deviation},{}};const auto cbytes=serialize_temporal_workbench_result_json(c);assert(serialize_temporal_workbench_result_json(parse_temporal_workbench_conformance_json(cbytes))==cbytes);compare(15,data,{}, {},"direct","",c);compare(17,data,{}, {},"direct","",c);
 profile.entries={{octets,octets,0,0,0},{octets,octets,std::numeric_limits<std::uint64_t>::max(),-0.0,-0.0}};compare(14,data,{},profile,"direct");compare(16,data,{},profile,"direct");
 const std::string rawmoments="[[\""+std::string(1,static_cast<char>(0xff))+"\",\"\\u0000\",-0,1]]";
 for(const auto& input:{std::string("[]"),std::string("[[\"A\",\"B\",1,0],[\"A\",\"B\",2,3]]"),rawmoments,std::string("[[\"A\",\"B\",1e999,0]]"),std::string("[[\"A\",\"B\",1e-999,0]]"),std::string("[[\"\\ud800\",\"B\",1,0]]"),std::string("[]\0x",4),std::string("[[\"A\",\"B\",-1,0]]")})compare(13,data,{}, {},"direct","",{}, {},{},6,input);
}
void option_domains(){Fixture data(full_log());auto profile=discover_temporal_profile(data.log);TemporalProfileOptions options;
 options.activity_attribute="activity-x";options.case_id_attribute="ignored-for-log";options.source_timestamp_attribute="stamp-x";options.target_timestamp_attribute="stamp-x";options.source_completion_timestamp_attribute="absent-alias";options.target_start_timestamp_attribute="absent-alias";options.use_business_hours=true;options.business_hour_slots={{9*3600,12*3600},{7*3600,9*3600}};options.business_hour_slots_explicit=true;options.strict_attributes=false;options.zeta=-0.0;
 const auto encoded=serialize_temporal_workbench_options_json(options);std::set<std::string> supplied;assert(fields(encoded).size()==11&&serialize_temporal_workbench_options_json(parse_temporal_workbench_options_json(encoded,&supplied))==encoded&&supplied.size()==11);compare(0,data,options);compare(7,data,options,profile);
 options.source_timestamp_attribute.clear();options.source_completion_timestamp_attribute="stamp-x";options.target_timestamp_attribute.clear();options.target_start_timestamp_attribute="stamp-x";compare(0,data,options);
 options.business_hour_slots.clear();compare(0,data,options);options.business_hour_slots_explicit=false;compare(0,data,options);
 options.business_hour_slots={{std::numeric_limits<double>::quiet_NaN(),-1}};options.use_business_hours=false;compare(0,data,options);options.use_business_hours=true;compare(0,data,options);
 options={};for(const double zeta:{-1.0,-0.0,0.0,6.0,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()}){options.zeta=zeta;compare(7,data,options,profile);compare(11,data,{},profile,"with_zeta","",{}, {},{},zeta);}
 options.activity_attribute.clear();compare(0,data,options);compare(7,data,options,TemporalProfile{});options={};options.case_id_attribute.clear();compare(0,data,options);compare(4,data,options);
 compare(0,data,{}, {},"with_options","",{}, {},{},6,"[]","A","B",true);
 auto empty=Fixture(EventLog{});TemporalProfile invalid;invalid.entries={{"A","B",0,0,0}};compare(7,empty,{},invalid);compare(9,empty,{},invalid);invalid.entries={{"A","B",1,0,0},{"A","B",1,0,0}};compare(7,empty,{},invalid);
 auto sparse=data.log;sparse.traces[0].events[1].timestamp="broken";Fixture sparse_data(sparse);options={};compare(0,sparse_data,options);options.strict_attributes=false;compare(0,sparse_data,options);compare(9,sparse_data,options,profile);
}
void transport_and_artifacts(){Fixture data(full_log());const auto profile=discover_temporal_profile(data.log);auto p=request(0,data,"","with_options",{},profile,{}, {},{},6,"[]","A","B");
 host_error(execute_temporal_workbench({}),"configuration_error");auto bad=p;bad.push_back({"operation","","discover_temporal_profile",false});host_error(execute_temporal_workbench(bad),"configuration_error");bad=p;bad.push_back({"extra","","",false});host_error(execute_temporal_workbench(bad),"configuration_error");bad=p;bad.push_back({"view_kind","","log",false});host_error(execute_temporal_workbench(bad),"configuration_error");
 bad=p;for(auto& part:bad)if(part.name=="source_kind")part.bytes="bogus";const auto unknown_kind=execute_temporal_workbench(bad);host_error(unknown_kind,"configuration_error");assert(fields(unknown_kind.json).at("source_kind")=="null");
 bad=p;for(auto& part:bad)if(part.name=="source_kind")part.bytes="view";bad.push_back({"view_kind","","view",false});const auto invalid_view=execute_temporal_workbench(bad);host_error(invalid_view,"configuration_error");assert(fields(invalid_view.json).at("source_kind")==q("view")&&fields(invalid_view.json).at("view_kind")=="null");
 bad=p;for(auto& part:bad)if(part.name=="options_json")part.bytes="{\"zeta\":1,\"zeta\":2}";host_error(execute_temporal_workbench(bad),"configuration_error");bad=p;for(auto& part:bad)if(part.name=="options_json")part.bytes="{\"unknown\":1}";host_error(execute_temporal_workbench(bad),"configuration_error");
 const auto baseline=compare(0,data);const auto a=fields(baseline.at("artifact_bytes"));Parts reopened{{"operation","","discover_temporal_profile",false},{"source_kind","","log",false},{"source_file","",bytes(a.at("source_input_json")),false},{"options_json","",bytes(a.at("configured_options_json")),true}};check(execute_temporal_workbench(reopened),direct(0,data,"",{},false,{}, {},{}, {},6,"[]","A","B"),0,"with_options","");
 Parts parsed{{"operation","","parse_temporal_profile_json",false},{"text_file","",bytes(fields(compare(14,data,{},profile,"direct").at("artifact_bytes")).at("native_output_bytes")),true}};check(execute_temporal_workbench(parsed),native([&]{return serialize_temporal_workbench_result_json(parse_temporal_profile_json(temporal_profile_json(profile)));}),13,"direct","");
 const auto find=compare(20,data,{},profile,"direct");const auto found=fields(find.at("artifact_bytes"));Parts member{{"operation","","TemporalProfileEntry::stddev",false},{"receiver_file","",bytes(found.at("found_entry_json")),true}};check(execute_temporal_workbench(member),native([&]{return serialize_temporal_workbench_scalar_json(profile.find("A","B")->stddev());}),18,"direct","");
 // Original field order is separate from canonical complete option order.
 auto ordered=p;for(auto& part:ordered)if(part.name=="options_json")part.bytes="{\"zeta\":-0,\"strict_attributes\":false,\"activity_attribute\":\"concept:name\"}";const auto ord=fields(execute_temporal_workbench(ordered).json);assert(ord.at("supplied_options")=="[\"zeta\",\"strict_attributes\",\"activity_attribute\"]");
 std::string body;for(const auto& part:p){body+="--boundary\r\nContent-Disposition: form-data; name=\""+part.name+"\"";if(part.file)body+="; filename=\""+part.filename+"\"";body+="\r\nContent-Type: application/x-arbitrary\r\n\r\n"+part.bytes+"\r\n";}body+="--boundary--\r\n";
 const auto multipart=execute_temporal_workbench_multipart("multipart/form-data; boundary=boundary",body);assert(multipart.json==execute_temporal_workbench(p).json);host_error(execute_temporal_workbench_multipart("text/plain",body),"transport_error");host_error(execute_temporal_workbench_multipart("multipart/form-data; boundary=boundary",body,{},false),"transport_error");
 Parts query{{"operation","","discover_temporal_profile",false},{"source_kind","","log",false}};Parts only{{"source_file","",data.encoded("log"),false}};assert(execute_temporal_workbench(only,query).status==200);host_error(execute_temporal_workbench({},{{"operation","","discover_temporal_profile",false},{"source_kind","","log",false},{"source_file","",data.encoded("log"),false}}),"configuration_error");
 // Query scalar bytes are original evidence too. Typed enum normalization
 // never erases invalid submitted query values, including NUL/invalid UTF8.
 std::string invalid_kind="bogus";invalid_kind.push_back(static_cast<char>(0));invalid_kind.push_back(static_cast<char>(0xff));
 Parts query_invalid{{"operation","","discover_temporal_profile",false},{"source_kind","ignored.bin",invalid_kind,true}};
 const auto query_error=execute_temporal_workbench(only,query_invalid);host_error(query_error,"configuration_error");const auto query_error_root=fields(query_error.json);assert(query_error_root.at("source_kind")=="null");
 const auto query_originals=elements(query_error_root.at("originals"));assert(query_originals.size()==3);
 for(std::size_t i=0;i<query_invalid.size();++i){const auto original=fields(query_originals[i]);assert(original.at("field")==q(query_invalid[i].name)&&original.at("filename")=="null"&&original.at("file")=="false"&&bytes(original.at("original"))==query_invalid[i].bytes);}
 assert(fields(query_originals.back()).at("field")==q("source_file"));
 Parts query_view{{"operation","","discover_temporal_profile",false},{"source_kind","","view",false},{"view_kind","","view",false}};
 const auto query_view_error=execute_temporal_workbench(only,query_view);host_error(query_view_error,"configuration_error");const auto query_view_root=fields(query_view_error.json);assert(query_view_root.at("source_kind")==q("view")&&query_view_root.at("view_kind")=="null");const auto view_originals=elements(query_view_root.at("originals"));assert(view_originals.size()==4&&bytes(fields(view_originals[2]).at("original"))=="view");
 // Lazy input errors retain completed preparation, and static snapshot never
 // causes a second scan to mask a native row-width/index validation failure.
 const auto table=serialize_quality_workbench_table_json(data.table);Parts lazy{{"operation","","discover_temporal_profile",false},{"source_kind","","lazy",false},{"source_file","","{\"table\":"+table+",\"valid\":false}",true}};host_error(execute_temporal_workbench(lazy),"input_error");
 lazy.back().bytes="{\"table\":{\"native_byte_strings\":\"hex-v1\",\"native_schema\":\"event-table-raw-v1\",\"value\":{}} ,\"valid\":false}";const auto invalid_lazy=fields(execute_temporal_workbench(lazy).json);assert(fields(invalid_lazy.at("error")).at("phase")==q("native_error")&&invalid_lazy.at("native_entered")=="true"&&elements(fields(invalid_lazy.at("input_context")).at("lazy_scans")).empty());
 for(const auto malformed:{std::string("{\"entries\":[{\"count\":18446744073709551616}]}"),std::string("{\"entries\":[{\"count\":1.0}]}"),std::string("{\"entries\":[{\"source\":{\"native_bytes\":\"hex-v1\",\"data\":\"f\"}}]}"),std::string("{\"entries\":[{\"mean_seconds\":1e999}]}")}){Parts original{{"operation","","temporal_profile_json",false},{"profile_file","",malformed,true}};host_error(execute_temporal_workbench(original),"input_error");}
}
} // namespace

void named_source_presence_contract(){
 using namespace minera;
 const std::string log="{\"traces\":[]}";
 const std::vector<DiscoveryWorkbenchPart> omitted={{"operation","","discover_temporal_profile_log",false},{"source_file","",log,false}};
 assert(execute_temporal_workbench(omitted).status==200);
 auto explicit_empty=omitted;explicit_empty.push_back({"source_kind","","",false});
 auto response=execute_temporal_workbench(explicit_empty);assert(response.status==400&&response.json.find("\"native_entered\":false")!=std::string::npos);
 response=execute_temporal_workbench(omitted,{{"source_kind","","",false}});assert(response.status==400&&response.json.find("\"native_entered\":false")!=std::string::npos);
}
int main(){
 named_source_presence_contract();all_forms();stored_domains();option_domains();transport_and_artifacts();assert(observed.size()>=45);return 0;}
