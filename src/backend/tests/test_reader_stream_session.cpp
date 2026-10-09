#include "minera/reader_stream_session.hpp"
#include "minera/streaming_session.hpp"
#include "minera/log_conversion.hpp"
#include "minera/xes_export.hpp"
#include <cassert>
#include <sstream>
#include <iostream>
#include <algorithm>
using namespace minera;
#undef assert
#define assert(condition) do {if(!(condition)){std::cerr<<"Assertion failed at line "<<__LINE__<<": "<<#condition<<'\n';throw std::runtime_error(#condition);}}while(false)
template<class F> void rejects(F operation){bool rejected=false;try{operation();}catch(const std::exception&){rejected=true;}assert(rejected);}
void owned_source_can_reset_after_the_input_stream_is_gone() {
  std::unique_ptr<CsvEventStreamReader> reader;
  {std::istringstream input("case_id,activity\nc1,A\nc2,X\nc1,B\n");StreamingEventReaderOptions options;options.transform=[](Event e){e.activity+='!';return e;};options.acceptance=[](const Event& e){return e.activity!="X!";};reader=std::make_unique<CsvEventStreamReader>(input,CsvImportOptions{},options);}
  assert(reader->read_event()->activity=="A!");assert(reader->read_event()->activity=="B!");assert(reader->exhausted());reader->reset();assert(reader->read_event()->activity=="A!");
}
void queue_read_reset_and_detached_capture_preserve_raw_items() {
  ReaderStreamOptions options;options.source="case_id,activity\nc1,A\nc2,X\nc1,B\n";ReaderStreamSession session(options);
  session.read(1,"first",0);assert(session.selected_log(false).event_count()==1);assert(session.selected_log(true).event_count()==0);
  rejects([&]{session.action(ReaderStreamAction::kDrain,1);});rejects([&]{session.read(1,"first",1);});rejects([&]{session.read(0,"bad",1);});
  session.action(ReaderStreamAction::kStart,1);session.action(ReaderStreamAction::kDeregister,2);session.read(1,"detached",3);session.action(ReaderStreamAction::kDrain,4);assert(session.selected_log(true).event_count()==1);
  session.action(ReaderStreamAction::kRegister,5);session.read(10,"remaining",6);assert(session.selected_log(true).event_count()==2);assert(session.selected_log(false).event_count()==3);
  session.action(ReaderStreamAction::kResetReader,7);session.read(1,"reset",8);assert(session.selected_log(true).event_count()==3);assert(session.snapshot_json().find("\"reader_generation\":1")!=std::string::npos);
  session.action(ReaderStreamAction::kFinish,9);rejects([&]{session.read(1,"finished",10);});rejects([&]{session.action(ReaderStreamAction::kStart,10);});
}
const std::string typed_xes="<log><string key=\"dataset\" value=\"東京\"/><trace><string key=\"concept:name\" value=\"same\"/><int key=\"cost\" value=\"9223372036854775807\"/><event><string key=\"concept:name\" value=\"A\"/><boolean key=\"ok\" value=\"false\"/></event></trace><trace><string key=\"concept:name\" value=\"same\"/></trace></log>";
void whole_traces_retain_empty_occurrences_typed_attributes_and_metadata() {
  ReaderStreamOptions options;options.kind=ReaderStreamKind::kXesTraces;options.source=typed_xes;ReaderStreamSession session(options);
  session.read(10,"whole",0);session.action(ReaderStreamAction::kStart,1);auto selected=session.selected_log(true);
  assert(selected.trace_count()==2&&selected.event_count()==1);assert(selected.traces[0].case_id==selected.traces[1].case_id&&selected.traces[1].events.empty());
  assert(selected.attribute("dataset")&&*selected.attribute("dataset")=="東京");assert(std::any_of(selected.traces[0].attributes.begin(),selected.traces[0].attributes.end(),[](const auto& value){return value.key=="cost"&&value.kind==AttributeKind::kInt;}));
  assert(std::any_of(selected.traces[0].events[0].attributes.begin(),selected.traces[0].events[0].attributes.end(),[](const auto& value){return value.key=="ok"&&value.kind==AttributeKind::kBoolean&&value.value=="false";}));
  const auto json=session.snapshot_json();assert(json.find("9223372036854775807")!=std::string::npos);
  XesExportOptions compressed;compressed.compress=true;auto bytes=serialize_xes(selected,compressed);std::istringstream input(bytes);XesTraceStreamReader again(input);
  assert(again.read_trace()->events.size()==1);assert(again.read_trace()->events.empty());assert(again.exhausted());
}
void transform_precedes_acceptance_and_reset_repeats_the_native_pipeline() {
  ReaderStreamOptions options;options.kind=ReaderStreamKind::kXesTraces;options.source=typed_xes;
  options.traces.transform=[](Trace trace){for(auto& event:trace.events){auto original=event;set_event_activity_context(original,"selected,"+original.activity,event);}return trace;};
  options.traces.acceptance=[](const Trace& trace){return !trace.events.empty()&&trace.events[0].activity=="selected,A";};ReaderStreamSession session(options);
  session.read(10,"first",0);session.action(ReaderStreamAction::kStart,1);assert(session.selected_log(true).traces[0].events[0].activity=="selected,A");
  session.action(ReaderStreamAction::kResetReader,2);session.read(10,"again",3);assert(session.selected_log(true).trace_count()==2);
}
void event_export_does_not_infer_new_types_or_remove_original_whitespace() {
  ReaderStreamOptions options;options.source="Case| Act | Time \n c1 | A | t \n";options.csv.case_id_column="Case";options.csv.activity_column=" Act ";options.csv.timestamp_column=" Time ";options.csv.trim_fields=false;options.csv.delimiter='|';ReaderStreamSession session(options);
  session.action(ReaderStreamAction::kStart,0);session.read(1,"first",1);const auto log=session.selected_log(true);const auto& event=log.traces[0].events[0];
  assert(event.activity==" A "&&event.timestamp==" t "&&!event.timestamp_attribute);const auto json=serialize_xes(log);assert(json.find(" t ")!=std::string::npos);std::istringstream input(json);XesImportOptions xes;xes.activity_attribute=" Act ";xes.timestamp_attribute=" Time ";const auto imported=import_xes_event_log(input,xes);assert(imported.traces[0].events[0].activity==" A "&&imported.traces[0].events[0].timestamp==" t ");
}
int main(){try{owned_source_can_reset_after_the_input_stream_is_gone();queue_read_reset_and_detached_capture_preserve_raw_items();whole_traces_retain_empty_occurrences_typed_attributes_and_metadata();transform_precedes_acceptance_and_reset_repeats_the_native_pipeline();event_export_does_not_infer_new_types_or_remove_original_whitespace();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
