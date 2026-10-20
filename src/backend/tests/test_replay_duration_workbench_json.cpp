#include "minera/replay_duration_workbench_json.hpp"
#include "minera/token_replay_duration_error.hpp"
#include "minera/token_replay_error.hpp"
#include "minera/timestamp.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace minera;
namespace {
void require(bool v,const char* why){if(!v)throw std::runtime_error(why);}
template<class E=std::invalid_argument,class F>void rejects(F f){bool rejected=false;try{f();}catch(const E&){rejected=true;}require(rejected,"expected contract rejection");}
Trace timed(double seconds,std::string label="b") {Trace t;t.case_id="duplicate";Event a,b;a.activity=label;b.activity=label;a.timestamp="1970-01-01T00:00:00Z";b.timestamp=format_timestamp(seconds);t.events={a,b};return t;}
EventLog fixture(){EventLog log;log.traces={timed(10),timed(30),timed(80),timed(100)};return log;}
void parsing(){
 const std::string raw=" \n{\"mode\":\"cohorts\",\"activity_attribute\":{\"native_bytes\":\"hex-v1\",\"data\":\"2000ff20\"},\"timestamp_attribute\":\" Ω \",\"transition_cohorts\":[{\"transition_index\":\"9007199254740993\",\"activity\":null,\"problem_trace_indices\":[2,2,0]}],\"unknown_activity_cohorts\":[{\"activity\":\"\",\"activity_is_null\":false,\"problem_trace_indices\":[]}]} \n";
 const auto r=parse_replay_duration_workbench_json(raw);require(r.raw_json==raw,"verbatim whitespace/payload");require(*r.activity_attribute==std::string(" \0\xff ",4),"exact bytes selectors");require(*r.timestamp_attribute==" Ω ","Unicode whitespace selector");
 require(r.transition_cohorts[0].transition_index==9007199254740993ULL&&!r.transition_cohorts[0].activity,"integer tokens never double rounded/null label");
 require(r.transition_cohorts[0].problem_trace_indices==std::vector<std::size_t>({2,2,0}),"parser preserves weights/order");require(!r.unknown_activity_cohorts[0].activity_is_null&&r.unknown_activity_cohorts[0].activity.empty(),"empty label distinct null");
 for(const auto& bad:{"{}","{\"mode\":\"other\"}","{\"mode\":\"case_duration\",\"trace_index\":1.0}","{\"mode\":\"case_duration\",\"trace_index\":-1}","{\"mode\":\"case_duration\",\"trace_index\":\" 1\"}","{\"mode\":\"case_duration\",\"trace_index\":18446744073709551616}","{\"mode\":\"cohorts\",\"mode\":\"cohorts\"}","{\"mode\":\"cohorts\",\"timestamp_attribute\":{\"native_bytes\":\"hex-v1\",\"data\":\"f\"}}","{\"mode\":\"median_duration\"}"})rejects([&]{(void)parse_replay_duration_workbench_json(bad);});
}
void surrogate_legacy(){
 const std::string raw=R"({"mode":"saved_diagnostics","activity_attribute":"\ud800","saved_diagnostics":[{"trace_index":0,"case_id":"\udfff","event_diagnostics":[],"unknown_event_indices":[],"opaque":{"name":"\ud800"}}]})";
 const auto r=parse_replay_duration_workbench_json(raw);require(r.raw_json==raw,"escaped legacy surrogate saved payload retained");require(r.activity_attribute==std::optional<std::string>(std::string("\xed\xa0\x80",3)),"legacy surrogate selector WTF8 bytes");
 auto result=TokenReplayDurationDiagnostics{};result.timestamp_attribute=*r.activity_attribute;require(serialize_replay_duration_diagnostics_json(result).find("eda080")!=std::string::npos,"surrogate selector NativeString output");
}
void cohorts(){
 auto log=fixture();auto input=parse_replay_duration_workbench_json(R"({"mode":"cohorts","transition_cohorts":[{"transition_index":7,"activity":"b","problem_trace_indices":[2,2]}]})");
 const auto text=serialize_replay_duration_workbench_result_json(log,nullptr,input,{});require(text.find("\"fit_median_seconds\":30")!=std::string::npos&&text.find("\"problem_median_seconds\":80")!=std::string::npos,"native cohort counterexample");require(text.find("\"problem_trace_indices\":[2]")!=std::string::npos,"native dedup not parser");
 input.transition_cohorts[0].problem_trace_indices={0,2};auto direct=token_replay_transition_duration_diagnostics(log,input.transition_cohorts);require(direct[0].fit_median_seconds==65&&direct[0].problem_median_seconds==45,"edited cohort changes question");require(serialize_replay_duration_workbench_result_json(log,nullptr,input,{}).find("\"fit_median_seconds\":65")!=std::string::npos,"edited helper wired");
 log.traces[2].events.back().activity="X";log.traces[3].events.back().activity="Y";
 input.transition_cohorts.clear();input.unknown_activity_cohorts={{"X",false,{2}},{"Y",false,{}}};auto values=token_replay_unknown_activity_duration_diagnostics(log,input.unknown_activity_cohorts);require(values[0].fit_median_seconds==20,"empty key excludes its baseline");require(serialize_replay_duration_workbench_result_json(log,nullptr,input,{}).find("\"fit_median_seconds\":20")!=std::string::npos,"unknown whole bank wired");
 input.unknown_activity_cohorts.pop_back();values=token_replay_unknown_activity_duration_diagnostics(log,input.unknown_activity_cohorts);require(values[0].fit_median_seconds==30,"removing empty key changes baseline");
 input.unknown_activity_cohorts={{std::string("\xff\0",2),true,{2}}};auto encoded=serialize_replay_duration_workbench_result_json(log,nullptr,input,{});require(encoded.find("ff00")!=std::string::npos&&encoded.find("\"activity_is_null\":true")!=std::string::npos,"full byte/null output");
 input.unknown_activity_cohorts.clear();input.transition_cohorts={{7,"b",{9}}};rejects<std::out_of_range>([&]{(void)serialize_replay_duration_workbench_result_json(log,nullptr,input,{});});
}
void scalar(){
 auto log=fixture();auto r=parse_replay_duration_workbench_json(R"({"mode":"median_duration","trace_indices":[0,0,2]})");require(serialize_replay_duration_workbench_result_json(log,nullptr,r,{}).find("\"median_duration_seconds\":10")!=std::string::npos,"weighted indices retained");r.trace_indices={0,2};require(serialize_replay_duration_workbench_result_json(log,nullptr,r,{}).find("\"median_duration_seconds\":45")!=std::string::npos,"unweighted witness differs");
 r.trace_indices.clear();rejects<TokenReplayDurationError>([&]{(void)serialize_replay_duration_workbench_result_json(log,nullptr,r,{});});
 r=parse_replay_duration_workbench_json(R"({"mode":"case_duration","trace_index":"3"})");require(serialize_replay_duration_workbench_result_json(log,nullptr,r,{}).find("\"case_duration_seconds\":100")!=std::string::npos,"case helper");
 log.traces[3]=timed(-10);require(serialize_replay_duration_workbench_result_json(log,nullptr,r,{}).find("\"case_duration_seconds\":-10")!=std::string::npos,"negative duration preserved");
 r.timestamp_attribute="missing";rejects<TokenReplayKeyError>([&]{(void)serialize_replay_duration_workbench_result_json(log,nullptr,r,{});});r.timestamp_attribute.reset();log.traces[3].events[1].timestamp_state=FirstClassAttributeState::kNull;rejects<TokenReplayDurationError>([&]{(void)serialize_replay_duration_workbench_result_json(log,nullptr,r,{});});
}
void saved(){
 auto log=fixture();PetriNet net;net.set_activities({"b"});net.add_transition(0,{},{});
 const std::string rows=R"([{"trace_index":2,"event_diagnostics":[{"event_index":1,"transitions_with_problems":[0]}],"unknown_event_indices":[],"transitions_with_problems":[],"missing":999999999999999999999999999999999999,"reached_marking":[0,-999999999999999999999999],"marking_domain":{"opaque":"full payload"}}])";
 for(const std::string shape:{"native_rows","native_result","workbench","report"}){
  const auto saved=shape=="native_rows"?rows:shape=="native_result"?"{\"traces\":"+rows+"}":shape=="workbench"?"{\"diagnostics\":{\"traces\":"+rows+"}}":"{\"native_result\":{\"diagnostics\":{\"traces\":"+rows+"}}}";
  const std::string raw="{\"mode\":\"saved_diagnostics\",\"saved_input_shape\":\""+shape+"\",\"saved_diagnostics\":"+saved+"}";auto r=parse_replay_duration_workbench_json(raw);require(r.raw_json==raw,"full saved payload not reconstructed");require(r.saved_diagnostics[0].transitions_with_problems.empty()&&r.saved_diagnostics[0].event_diagnostics[0].transitions_with_problems==std::vector<std::size_t>({0}),"event-level consumption only");
  require(serialize_replay_duration_workbench_result_json(log,&net,r,{}).find("\"fit_median_seconds\":30")!=std::string::npos,"saved events drive comparison");
  r.saved_diagnostics[0].event_diagnostics[0].transitions_with_problems.clear();require(serialize_replay_duration_workbench_result_json(log,&net,r,{}).find("\"transitions\":[]")!=std::string::npos,"editing saved events no replay");
  r.saved_diagnostics[0].event_diagnostics[0].event_index=99;rejects<std::out_of_range>([&]{(void)serialize_replay_duration_workbench_result_json(log,&net,r,{});});
  r.saved_diagnostics[0].event_diagnostics.clear();r.saved_diagnostics[0].unknown_event_indices={99};rejects<std::out_of_range>([&]{(void)serialize_replay_duration_workbench_result_json(log,&net,r,{});});
  r.saved_diagnostics[0].unknown_event_indices.clear();r.saved_diagnostics[0].trace_index=99;rejects<std::out_of_range>([&]{(void)serialize_replay_duration_workbench_result_json(log,&net,r,{});});
 }
 auto file_request=parse_replay_duration_workbench_json(R"({"mode":"saved_diagnostics","saved_input_shape":"native_rows"})");
 require(!file_request.has_saved_diagnostics&&file_request.raw_json.find("saved_diagnostics\":")==std::string::npos,"file controls do not synthesize inline payload");
 file_request.saved_diagnostics=parse_saved_replay_duration_diagnostics_json(rows,"native_rows");file_request.has_saved_diagnostics=true;
 require(serialize_replay_duration_workbench_result_json(log,&net,file_request,{}).find("\"fit_median_seconds\":30")!=std::string::npos,"uploaded full file same native helper");
 auto empty=parse_replay_duration_workbench_json(R"({"mode":"saved_diagnostics","saved_diagnostics":[]})");require(empty.has_saved_diagnostics,"explicit empty diagnostic input present");
 require(serialize_replay_duration_workbench_result_json(log,&net,empty,{}).find("\"transitions\":[]")!=std::string::npos,"empty saved input accepted natively");
 rejects([&]{(void)parse_replay_duration_workbench_json(R"({"mode":"saved_diagnostics","saved_diagnostics":{"type":"list","values":[]},"saved_input_shape":"native_result"})");});
}
}
int main(){try{parsing();surrogate_legacy();cohorts();scalar();saved();std::cout<<"replay duration workbench contracts passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
