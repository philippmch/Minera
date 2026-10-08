#include "minera/declare_streaming_io.hpp"
#include "minera/streaming_session.hpp"
#include <cassert>
#include <string>
#include <vector>
#include <iostream>
using namespace minera;
#undef assert
#define assert(condition) do { if (!(condition)) { std::cerr << "Assertion failed at line " << __LINE__ << ": " << #condition << '\n'; throw std::runtime_error(#condition); } } while(false)
template<class F> void rejects(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}assert(rejected);}
void model_roundtrip_retains_labels_duplicates_unknown_rules_and_native_order() {
  const auto model=parse_declare_stream_model_json(R"({"rules":[{"template":"response","first":"A,->:\n東京","second":"B\\β","binary":true,"support":-2,"confidence":2147483647},{"template":"response","first":"A,->:\n東京","second":"B\\β","binary":true},{"template":"custom,rule","first":"","second":"","binary":false}]})");
  assert(model.size()==3);assert(model.rules.at("response")[0].first=="A,->:\n東京");assert(model.rules.at("response")[0].support==-2);
  const auto canonical=serialize_declare_stream_model_json(model);assert(serialize_declare_stream_model_json(parse_declare_stream_model_json(canonical))==canonical);
  const auto legacy=parse_declare_stream_model_json(R"({"activities":[{"id":2,"name":"A"},{"id":8,"name":"B"}],"declare":{"response":[{"source":2,"target":8,"support":1,"confidence":2}],"existence":[{"activity":2}]}})");
  assert(legacy.size()==2&&legacy.rules.at("response")[0].second=="B");
}
void malformed_models_fail_before_execution() {
  for(const auto& raw:std::vector<std::string>{R"({"rules":[{"template":"response","first":"A","binary":false}]})",R"({"rules":[{"template":"init","first":"A","binary":1}]})",R"({"rules":[{"template":"init","first":"A","binary":false,"support":1.5}]})",R"({"rules":[{"template":"init","first":"A","binary":false,"confidence":2147483648}]})",R"({"rules":[],"rules":[]})",R"({"rules":[{"template":"init","first":"\ud800","binary":false}]})",R"({"activities":["A"],"declare":{"response":[{"source":0,"target":1}]}})"})rejects([&]{(void)parse_declare_stream_model_json(raw);});
}
void prefix_state_violation_identity_and_time_history_survive_batches() {
  const auto model=parse_declare_stream_model_json(R"({"rules":[{"template":"response","first":"A","second":"B","binary":true},{"template":"chainresponse","first":"A","second":"B","binary":true},{"template":"absence","first":"X","binary":false}]})");
  DeclareStreamingConformance monitor(model);const auto first=monitor.receive("case","A","");assert(first.deviations==0&&first.timestamp=="1");const auto next=monitor.receive("case","X","same");assert(next.deviations==2&&next.violated_constraints.size()==2);const auto last=monitor.receive("case","B","same");assert(last.deviations==0);
  auto snapshot=monitor.snapshot();assert(snapshot.deviations_per_time.size()==3&&snapshot.deviations_per_time[1].timestamp=="same"&&snapshot.deviations_per_time[2].timestamp=="same");
  const auto json=serialize_declare_streaming_json(snapshot,model);assert(json.find("\"state\":\"violated\"")!=std::string::npos);assert(serialize_declare_stream_event_json(next).find("\"violated_constraints\":[{")!=std::string::npos);
  StreamingSessionOptions options;options.method=StreamingSessionMethod::kDeclare;options.declare_model=model;StreamingSession session(options);
  session.append({Event{"case","A","","",0,{},""}},"first",0);session.action(StreamingSessionAction::kStart,1);session.append({Event{"case","X","same","",0,{},""},Event{"case","B","same","",1,{},""}},"second",2);
  auto live=session.snapshot_json();assert(live.find("\"total_deviations\":2")!=std::string::npos&&live.find("\"native_event\":{")!=std::string::npos);session.action(StreamingSessionAction::kFinish,3);rejects([&]{session.action(StreamingSessionAction::kTerminateAll,4);});session.action(StreamingSessionAction::kResetCase,4,"case");assert(session.snapshot_json().find("\"cases\":{}")!=std::string::npos);
}
void empty_models_and_unknown_templates_preserve_native_prefix_behavior() {
  DeclareModel model;model.add("unknown",{"","",false,0,0});DeclareStreamingConformance monitor(model);const auto r=monitor.receive("","","");assert(r.case_id=="undefined_case"&&r.event_index==1&&r.deviations==0);assert(monitor.snapshot().cases.at("undefined_case").constraint_states[0].state=="init");
  const auto empty=parse_declare_stream_model_json("{\"rules\":[]}");DeclareStreamingConformance no_rules(empty);assert(no_rules.receive("c","A").deviations==0);
}
int main(){try{model_roundtrip_retains_labels_duplicates_unknown_rules_and_native_order();malformed_models_fail_before_execution();prefix_state_violation_identity_and_time_history_survive_batches();empty_models_and_unknown_templates_preserve_native_prefix_behavior();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
