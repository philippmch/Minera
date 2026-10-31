#include "minera/streaming_session.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/xes_event_log_importer.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

using Action=minera::StreamingSessionAction;
using Method=minera::StreamingSessionMethod;
minera::Event event(std::string id,std::string activity,std::size_t order=0,std::string time="") {
  return {std::move(id),std::move(activity),std::move(time),"",order,{},""};
}
template<class F> void rejects(F action) {bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}assert(rejected);}
void drain_waits_for_inflight_observers_and_keeps_stream_active() {
  minera::LiveEventStream stream(1);std::mutex mutex;std::condition_variable condition;bool entered=false,release=false;
  auto id=stream.register_observer([&](const minera::Event&){std::unique_lock lock(mutex);entered=true;condition.notify_all();condition.wait(lock,[&]{return release;});});
  assert(stream.append(event("c","A")));rejects([&]{stream.drain();});stream.start();
  {std::unique_lock lock(mutex);condition.wait(lock,[&]{return entered;});}
  assert(stream.pending()==0);auto drained=std::async(std::launch::async,[&]{stream.drain();});
  assert(drained.wait_for(std::chrono::milliseconds(20))==std::future_status::timeout);
  {std::lock_guard lock(mutex);release=true;}condition.notify_all();drained.get();
  assert(stream.state()==minera::StreamingStreamState::kActive);assert(stream.append(event("c","B")));stream.drain();stream.stop();assert(stream.deregister_observer(id));
  minera::LiveTraceStream traces(1);std::atomic<int> count=0;
  (void)traces.register_observer([&](const minera::Trace&){++count;});minera::Trace trace;trace.case_id="c";assert(traces.append(trace));traces.start();traces.drain();assert(count==1);traces.stop();
}
void dfg_persists_between_batches_and_revisions_guard_mutations() {
  minera::StreamingSession session;assert(session.revision()==0);
  assert(session.append({event("c","A",8)},"first",0)==1);
  auto queued=session.snapshot_json();assert(queued.find("\"state\":\"INACTIVE\"")!=std::string::npos);assert(queued.find("\"events\":0")!=std::string::npos);
  rejects([&]{session.append({event("c","X")},"stale",0);});rejects([&]{session.action(Action::kDrain,1);});
  session.action(Action::kStart,1);assert(session.revision()==2);
  assert(session.append({event("c","B",3),event("other","A",4),event("c","C",5)},"second",2)==3);
  auto snapshot=session.snapshot_json();assert(snapshot.find("\"events\":4")!=std::string::npos);assert(snapshot.find("\"source\":\"A\",\"target\":\"B\",\"count\":1")!=std::string::npos);assert(snapshot.find("\"source\":\"B\",\"target\":\"C\",\"count\":1")!=std::string::npos);assert(snapshot.find("\"source_position\":8")!=std::string::npos);
  rejects([&]{session.append({},"second",3);});assert(session.revision()==3);session.action(Action::kFinish,3);rejects([&]{session.append({},"third",4);});rejects([&]{session.action(Action::kStart,4);});
}
void deregistration_and_reset_keep_delivery_history_and_boundaries() {
  minera::StreamingSession session;session.action(Action::kStart,0);session.append({event("c","A")},"one",1);session.action(Action::kDeregister,2);
  session.append({event("c","X")},"detached",3);session.action(Action::kDrain,4);
  auto s=session.snapshot_json();assert(s.find("\"enqueued\":2,\"delivered\":1")!=std::string::npos);assert(s.find("\"activities\":{\"A\":1}")!=std::string::npos);
  session.action(Action::kRegister,4);session.append({event("c","B")},"two",5);s=session.snapshot_json();assert(s.find("\"source\":\"A\",\"target\":\"B\"")!=std::string::npos);
  session.action(Action::kResetAll,6);s=session.snapshot_json();assert(s.find("\"events\":0")!=std::string::npos);assert(s.find("\"delivered\":2")!=std::string::npos);session.action(Action::kFinish,7);
}
void footprint_prefixes_terminate_after_finish_and_allow_a_new_case_generation() {
  minera::StreamingSessionOptions options;options.method=Method::kFootprints;minera::Footprint model;model.activities={"A","B"};model.sequence={{"A","B"}};model.start_activities={"A"};model.end_activities={"B"};options.footprint_model=model;
  minera::StreamingSession session(options);session.action(Action::kStart,0);session.append({event("c","A")},"one",1);session.action(Action::kFinish,2);session.action(Action::kTerminateCase,3,"c");
  auto s=session.snapshot_json();assert(s.find("\"total_deviations\":1")!=std::string::npos);assert(s.find("\"case_id\":\"c\",\"revision\":4,\"is_fit\":false")!=std::string::npos);rejects([&]{session.action(Action::kTerminateCase,4,"missing");});
  minera::StreamingSession restarted(options);restarted.action(Action::kStart,0);restarted.append({event("c","A")},"one",1);restarted.action(Action::kResetCase,2,"c");restarted.append({event("c","A"),event("c","B")},"two",3);restarted.action(Action::kTerminateAll,4);assert(restarted.snapshot_json().find("\"is_fit\":true")!=std::string::npos);
}
minera::PetriNet net() {minera::PetriNet n;auto a=n.add_place(),b=n.add_place(),c=n.add_place();n.set_source(a);n.set_sink(c);n.set_activities({"A","B"});n.add_transition(0,{a},{b});n.add_transition(1,{b},{c});return n;}
void both_replay_variants_keep_markings_across_appends_and_final_diagnostics() {
  for(auto variant:{minera::StreamingTbrVariant::kForward,minera::StreamingTbrVariant::kBackwards}){
    minera::StreamingSessionOptions options;options.method=Method::kTbr;options.net=net();options.tbr.variant=variant;
    minera::StreamingSession session(options);session.append({event("c","A")},"one",0);session.action(Action::kStart,1);session.append({event("c","B")},"two",2);session.action(Action::kFinish,3);session.action(Action::kTerminateAll,4);
    auto s=session.snapshot_json();assert(s.find("\"is_fit\":true,\"missing\":0,\"remaining\":0,\"marking\":[0,0,1]")!=std::string::npos);
  }
}
void temporal_pairs_cross_batch_boundaries_and_invalid_times_remain_skipped() {
  minera::StreamingSessionOptions options;options.method=Method::kTemporal;minera::TemporalProfile profile;profile.entries.push_back({"A","B",2,5,1});options.temporal_profile=profile;options.temporal.zeta=1;
  minera::StreamingSession session(options);session.action(Action::kStart,0);session.append({event("c","A",0,"2026-10-01T00:00:00Z")},"one",1);session.append({event("c","B",0,"2026-10-01T00:00:20Z"),event("bad","A",1,"bad")},"two",2);
  auto s=session.snapshot_json();assert(s.find("\"total_events_processed\":2")!=std::string::npos);assert(s.find("\"total_deviations\":1")!=std::string::npos);assert(s.find("\"enqueued\":3,\"delivered\":3")!=std::string::npos);assert(s.find("\"accepted\":false")!=std::string::npos);
}
void crlf_records_do_not_change_untrimmed_native_columns_or_quoted_values() {
  minera::CsvImportOptions options;options.delimiter=';';options.trim_fields=false;options.allow_case_id_aliases=true;
  std::istringstream input("case:concept:name;activity\r\nc; A \r\nquoted;\"B\r\"\r\n");
  const auto data=minera::import_csv_event_data(input,options);
  assert(data.table.columns==std::vector<std::string>({"case:concept:name","activity"}));
  assert(data.log.traces[0].events[0].activity==" A ");assert(data.log.traces[1].events[0].activity=="B\r");
  std::istringstream lf("case:concept:name;activity\nc; A \nquoted;\"B\r\"\n");
  const auto other=minera::import_csv_event_log(lf,options);assert(other.traces[1].events[0].activity=="B\r");
}
// The reference DFG, footprints, temporal and TBR stream monitors skip an
// event without a case id. Expected values from PM4Py 2.7.22 on the XES
// event stream of this log (traces 2 and 3 have no concept:name): DFG
// {(A,D):1}, activities {A:1,D:1}; footprints and TBR keep case c only;
// temporal reports one deviation, (c, A, D, 100.0, 90.0).
std::vector<minera::Event> unnamed_trace_events() {
  std::istringstream xes(R"(<log xes.version="1.0">
<trace><string key="concept:name" value="c"/>
<event><string key="concept:name" value="A"/><date key="time:timestamp" value="2024-01-01T00:00:00"/></event>
<event><string key="concept:name" value="D"/><date key="time:timestamp" value="2024-01-01T00:01:40"/></event></trace>
<trace><event><string key="concept:name" value="B"/><date key="time:timestamp" value="2024-01-01T00:00:00"/></event>
<event><string key="concept:name" value="C"/><date key="time:timestamp" value="2024-01-01T00:01:40"/></event></trace>
<trace><event><string key="concept:name" value="A"/><date key="time:timestamp" value="2024-01-01T00:00:00"/></event>
<event><string key="concept:name" value="D"/><date key="time:timestamp" value="2024-01-01T00:01:40"/></event></trace></log>)");
  const auto log=minera::import_xes_event_log(xes);std::vector<minera::Event> events;std::size_t order=0;
  for(const auto& trace:log.traces)for(auto event:trace.events){event.input_order=order++;events.push_back(std::move(event));}
  assert(events.size()==6&&!events[2].has_case_id()&&events[0].case_id=="c");
  return events;
}
void events_without_case_ids_are_skipped() {
  const auto run=[](minera::StreamingSessionOptions options){
    options.dfg.activity_attribute=options.footprints.activity_attribute=options.tbr.activity_attribute="concept:name";
    minera::StreamingSession session(options);session.action(Action::kStart,0);session.append(unnamed_trace_events(),"one",1);return session.snapshot_json();};
  {auto s=run({});
   assert(s.find("\"activities\":{\"A\":1,\"D\":1}")!=std::string::npos);
   assert(s.find("\"source\":\"A\",\"target\":\"D\",\"count\":1")!=std::string::npos);
   assert(s.find("\"source\":\"B\"")==std::string::npos);}
  {minera::StreamingSessionOptions options;options.method=Method::kFootprints;minera::Footprint model;model.activities={"A","B","C","D"};model.sequence={{"A","D"}};model.start_activities={"A"};model.end_activities={"D"};options.footprint_model=model;
   auto s=run(options);assert(s.find("\"total_events_processed\":2")!=std::string::npos);assert(s.find("\"total_deviations\":0")!=std::string::npos);assert(s.find("\"\":{")==std::string::npos);}
  {minera::StreamingSessionOptions options;options.method=Method::kTemporal;minera::TemporalProfile profile;profile.entries.push_back({"A","D",2,10,1});profile.entries.push_back({"B","C",2,10,1});options.temporal_profile=profile;
   auto s=run(options);assert(s.find("\"total_events_processed\":2")!=std::string::npos);assert(s.find("\"total_deviations\":1")!=std::string::npos);assert(s.find("\"\":{")==std::string::npos);}
  {minera::StreamingSessionOptions options;options.method=Method::kTbr;minera::PetriNet n;auto a=n.add_place(),b=n.add_place(),c=n.add_place();n.set_source(a);n.set_sink(c);n.set_activities({"A","D"});n.add_transition(0,{a},{b});n.add_transition(1,{b},{c});options.net=n;
   auto s=run(options);assert(s.find("\"\":{")==std::string::npos);assert(s.find("\"c\":{")!=std::string::npos);}
}
int main(){drain_waits_for_inflight_observers_and_keeps_stream_active();dfg_persists_between_batches_and_revisions_guard_mutations();deregistration_and_reset_keep_delivery_history_and_boundaries();footprint_prefixes_terminate_after_finish_and_allow_a_new_case_generation();both_replay_variants_keep_markings_across_appends_and_final_diagnostics();temporal_pairs_cross_batch_boundaries_and_invalid_times_remain_skipped();crlf_records_do_not_change_untrimmed_native_columns_or_quoted_values();events_without_case_ids_are_skipped();}
