#include "minera/log_attribute_repetition_filter.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/declare_workbench_json.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace minera;
namespace {
EventAttribute null_value(std::string key){EventAttribute a{std::move(key),""};a.is_null=true;return a;}
template<class Integer> EventAttribute integer_value(std::string key,Integer value){EventAttribute a{std::move(key),std::to_string(value),AttributeKind::kInt};a.typed_value=value;return a;}
EventAttribute float_value(std::string key,double value){EventAttribute a{std::move(key),std::to_string(value),AttributeKind::kFloat};a.typed_value=value;return a;}
EventAttribute boolean_value(std::string key,bool value){EventAttribute a{std::move(key),value?"true":"false",AttributeKind::kBoolean};a.typed_value=value;return a;}
EventAttribute date_value(std::string key,std::string value){EventAttribute a{std::move(key),value,AttributeKind::kDate};a.typed_value=NativeDate{value};return a;}
EventAttribute container_value(std::string key,AttributeContainerKind kind,std::vector<EventAttribute> children){EventAttribute a{std::move(key),""};a.container=kind;a.children=std::move(children);return a;}
Event event_for(std::vector<EventAttribute> attributes){
 Event event;event.attributes=std::move(attributes);
 for(const auto& a:event.attributes){
  if(a.key=="concept:name"){event.activity=a.value;event.activity_attribute=a;event.activity_state=a.is_null?FirstClassAttributeState::kNull:FirstClassAttributeState::kPresent;event.activity_source_key=a.key;}
  if(a.key=="time:timestamp"){event.timestamp=a.value;event.timestamp_attribute=a;event.timestamp_state=a.is_null?FirstClassAttributeState::kNull:FirstClassAttributeState::kPresent;event.timestamp_source_key=a.key;}
 }
 return event;
}

EventLog snapshot_0(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x30",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{0LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x30",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{1LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x5a",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x32",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{2LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x32",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x5a",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x33",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{3LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x33",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x5a",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x32\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=2;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x34",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{4LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x34",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x32\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=2;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x35",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{5LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x35",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("",0)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("",0)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x36",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{6LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x36",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x20\x41\x20",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x20\x41\x20",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x37",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{7LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x37",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x2c\x26\x3d\x2b\x7c",6)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x2c\x26\x3d\x2b\x7c",6)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x38",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{8LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x38",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x0a\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x0a\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x39",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{9LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x39",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x00\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x00\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x30",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{10LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x30",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\xce\xa9",2)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\xce\xa9",2)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_1(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x33",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{3LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x33",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x5a",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x32\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=2;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x34",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{4LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x34",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x32\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=2;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_2(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{1LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x5a",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x32",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{2LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x32",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x5a",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x33",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{3LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x33",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x5a",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x32\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=2;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x34",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{4LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x34",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x32\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=2;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x35",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{5LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x35",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("",0)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("",0)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x36",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{6LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x36",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x20\x41\x20",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x20\x41\x20",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x37",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{7LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x37",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x2c\x26\x3d\x2b\x7c",6)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x2c\x26\x3d\x2b\x7c",6)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x38",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{8LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x38",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x0a\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x0a\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x39",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{9LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x39",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x00\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x00\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x30",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{10LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x30",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\xce\xa9",2)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\xce\xa9",2)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_3(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x30",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{0LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x30",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{1LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x5a",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x32",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{2LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x32",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x5a",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x35",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{5LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x35",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("",0)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("",0)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x36",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{6LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x36",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x20\x41\x20",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x20\x41\x20",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x37",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{7LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x37",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x2c\x26\x3d\x2b\x7c",6)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x2c\x26\x3d\x2b\x7c",6)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x38",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{8LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x38",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x0a\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x0a\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x39",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{9LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x39",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x00\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x00\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x30",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{10LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x30",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\xce\xa9",2)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\xce\xa9",2)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_4(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x32",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{2LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x32",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x5a",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x33",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{3LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x33",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x5a",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x32\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=2;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x34",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{4LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x34",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x32\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=2;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_5(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x34",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{4LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x34",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x32\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=2;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_6(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
return log;}
EventLog snapshot_7(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x33",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{3LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x33",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x5a",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x32\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=2;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_8(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x30",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{0LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x30",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{1LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x5a",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x35",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{5LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x35",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("",0)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("",0)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x36",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{6LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x36",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x20\x41\x20",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x20\x41\x20",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x37",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{7LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x37",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x2c\x26\x3d\x2b\x7c",6)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x2c\x26\x3d\x2b\x7c",6)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x38",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{8LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x38",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x0a\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x0a\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x39",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{9LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x39",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x00\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x00\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x30",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{10LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x30",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\xce\xa9",2)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\xce\xa9",2)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_9(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x32",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{2LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x32",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x5a",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_10(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x35",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{5LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x35",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("",0)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("",0)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_11(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x36",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{6LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x36",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x20\x41\x20",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x20\x41\x20",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_12(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x37",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{7LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x37",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x2c\x26\x3d\x2b\x7c",6)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x2c\x26\x3d\x2b\x7c",6)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_13(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x38",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{8LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x38",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x0a\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x0a\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_14(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x39",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{9LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x39",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x00\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x00\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_15(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x30",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{10LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x30",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\xce\xa9",2)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\xce\xa9",2)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_16(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{1LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x5a",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x32",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{2LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x32",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x5a",1)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x35",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{5LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x35",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("",0)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("",0)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x36",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{6LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x36",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x20\x41\x20",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x20\x41\x20",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x37",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{7LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x37",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x2c\x26\x3d\x2b\x7c",6)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x2c\x26\x3d\x2b\x7c",6)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x38",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{8LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x38",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x0a\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x0a\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x39",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{9LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x39",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x00\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x00\x42",3)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x30",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{10LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x30",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\xce\xa9",2)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x77\x65\x62",3)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\xce\xa9",2)},EventAttribute{std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7),std::string("\x6d\x61\x69\x6c",4)},EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)},integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL}),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_17(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x30",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{0LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x30",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_18(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x30",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{0LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x30",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x6f\x74\x68\x65\x72",5),std::string("\x41",1)}});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_19(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x30",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{0LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x30",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x6f\x74\x68\x65\x72",5),std::string("\x41",1)}});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)}});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({null_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12))});event.input_order=2;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{1LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x32",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{2LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x32",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)}});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)}});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_20(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x30",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{0LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x30",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},null_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},null_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{1LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},boolean_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),true)});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},boolean_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),true)});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x32",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{2LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x32",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},boolean_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),false)});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},boolean_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),false)});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x33",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{3LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x33",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},integer_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::int64_t{0LL})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},integer_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::int64_t{0LL})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x34",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{4LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x34",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},integer_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::int64_t{1LL})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},integer_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::int64_t{1LL})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x35",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{5LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x35",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),0x1.0000000000000p+0)});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),0x1.0000000000000p+0)});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x36",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{6LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x36",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),0x1.8000000000000p+0)});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),0x1.8000000000000p+0)});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x37",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{7LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x37",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},integer_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::int64_t{9007199254740992LL})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},integer_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::int64_t{9007199254740992LL})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x38",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{8LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x38",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},integer_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::int64_t{9007199254740993LL})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},integer_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::int64_t{9007199254740993LL})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x39",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{9LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x39",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),0x1.0000000000000p+53)});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),0x1.0000000000000p+53)});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x30",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{10LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x30",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x31",1)}});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x31",1)}});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x31",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{11LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x31",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x4e\x61\x4e",3)}});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x4e\x61\x4e",3)}});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x32",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{12LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x32",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::numeric_limits<double>::quiet_NaN())});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::numeric_limits<double>::quiet_NaN())});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x33",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{13LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x33",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::numeric_limits<double>::infinity())});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::numeric_limits<double>::infinity())});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x34",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{14LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x34",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),-std::numeric_limits<double>::infinity())});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),-std::numeric_limits<double>::infinity())});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x35",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{15LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x35",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},date_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x36\x2e\x31\x32\x33\x34\x35\x36\x2b\x30\x30\x3a\x30\x30",32))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},date_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x36\x2e\x31\x32\x33\x34\x35\x36\x2b\x30\x30\x3a\x30\x30",32))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x36",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{16LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x36",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x36\x2e\x31\x32\x33\x34\x35\x36\x2b\x30\x30\x3a\x30\x30",32)}});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x36\x2e\x31\x32\x33\x34\x35\x36\x2b\x30\x30\x3a\x30\x30",32)}});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x37",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{17LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x37",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kList,{EventAttribute{std::string("\x30",1),std::string("\x61",1)},integer_value(std::string("\x31",1),std::int64_t{1LL})})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kList,{EventAttribute{std::string("\x30",1),std::string("\x61",1)},integer_value(std::string("\x31",1),std::int64_t{1LL})})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x38",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{18LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x38",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kTuple,{EventAttribute{std::string("\x30",1),std::string("\x61",1)},integer_value(std::string("\x31",1),std::int64_t{1LL})})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kTuple,{EventAttribute{std::string("\x30",1),std::string("\x61",1)},integer_value(std::string("\x31",1),std::int64_t{1LL})})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x39",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{19LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x39",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kContainer,{integer_value(std::string("\x78",1),std::int64_t{1LL})})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kContainer,{integer_value(std::string("\x78",1),std::int64_t{1LL})})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x32\x30",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{20LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x32\x30",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kSet,{integer_value(std::string("\x30",1),std::int64_t{1LL}),EventAttribute{std::string("\x31",1),std::string("\x61",1)}})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kSet,{integer_value(std::string("\x30",1),std::int64_t{1LL}),EventAttribute{std::string("\x31",1),std::string("\x61",1)}})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x32\x31",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{21LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x32\x31",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kFrozenSet,{integer_value(std::string("\x30",1),std::int64_t{1LL}),EventAttribute{std::string("\x31",1),std::string("\x61",1)}})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kFrozenSet,{integer_value(std::string("\x30",1),std::int64_t{1LL}),EventAttribute{std::string("\x31",1),std::string("\x61",1)}})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_21(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x30",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{0LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x30",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},null_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},null_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_22(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{1LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},boolean_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),true)});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},boolean_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),true)});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x34",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{4LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x34",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},integer_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::int64_t{1LL})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},integer_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::int64_t{1LL})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x35",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{5LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x35",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),0x1.0000000000000p+0)});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),0x1.0000000000000p+0)});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_23(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x32",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{2LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x32",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},boolean_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),false)});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},boolean_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),false)});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x33",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{3LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x33",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},integer_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::int64_t{0LL})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},integer_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::int64_t{0LL})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_24(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x36",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{6LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x36",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),0x1.8000000000000p+0)});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),0x1.8000000000000p+0)});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_25(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x37",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{7LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x37",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},integer_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::int64_t{9007199254740992LL})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},integer_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::int64_t{9007199254740992LL})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x39",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{9LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x39",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),0x1.0000000000000p+53)});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),0x1.0000000000000p+53)});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_26(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x38",6)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{8LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x38",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},integer_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::int64_t{9007199254740993LL})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},integer_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::int64_t{9007199254740993LL})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_27(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x30",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{10LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x30",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x31",1)}});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x31",1)}});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_28(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x31",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{11LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x31",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x4e\x61\x4e",3)}});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x4e\x61\x4e",3)}});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_29(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x33",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{13LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x33",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::numeric_limits<double>::infinity())});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::numeric_limits<double>::infinity())});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_30(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x34",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{14LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x34",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),-std::numeric_limits<double>::infinity())});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),-std::numeric_limits<double>::infinity())});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_31(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x35",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{15LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x35",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},date_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x36\x2e\x31\x32\x33\x34\x35\x36\x2b\x30\x30\x3a\x30\x30",32))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},date_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x36\x2e\x31\x32\x33\x34\x35\x36\x2b\x30\x30\x3a\x30\x30",32))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_32(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x36",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{16LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x36",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x36\x2e\x31\x32\x33\x34\x35\x36\x2b\x30\x30\x3a\x30\x30",32)}});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x36\x2e\x31\x32\x33\x34\x35\x36\x2b\x30\x30\x3a\x30\x30",32)}});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_33(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x37",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{17LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x37",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kList,{EventAttribute{std::string("\x30",1),std::string("\x61",1)},integer_value(std::string("\x31",1),std::int64_t{1LL})})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kList,{EventAttribute{std::string("\x30",1),std::string("\x61",1)},integer_value(std::string("\x31",1),std::int64_t{1LL})})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_34(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x38",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{18LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x38",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kTuple,{EventAttribute{std::string("\x30",1),std::string("\x61",1)},integer_value(std::string("\x31",1),std::int64_t{1LL})})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kTuple,{EventAttribute{std::string("\x30",1),std::string("\x61",1)},integer_value(std::string("\x31",1),std::int64_t{1LL})})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_35(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x39",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{19LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x39",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kContainer,{integer_value(std::string("\x78",1),std::int64_t{1LL})})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kContainer,{integer_value(std::string("\x78",1),std::int64_t{1LL})})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_36(){EventLog log;
log.attributes={EventAttribute{std::string("\x6c\x6f\x67\x20\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)},integer_value(std::string("\x77\x69\x64\x65\x20\x69\x6e\x74\x65\x67\x65\x72",12),std::int64_t{9007199254740993LL})};
log.metadata.properties={EventAttribute{std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x20\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x72\x65\x74\x61\x69\x6e\x65\x64",8)}};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x74\x72\x61\x63\x65",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
log.metadata.event_globals={EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{EventAttribute{std::string("\x67\x6c\x6f\x62\x61\x6c\x20\x65\x76\x65\x6e\x74",12),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}}});
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x32\x30",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{20LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x32\x30",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kSet,{integer_value(std::string("\x30",1),std::int64_t{1LL}),EventAttribute{std::string("\x31",1),std::string("\x61",1)}})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kSet,{integer_value(std::string("\x30",1),std::int64_t{1LL}),EventAttribute{std::string("\x31",1),std::string("\x61",1)}})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x32\x31",7)},integer_value(std::string("\x70\x6f\x73\x69\x74\x69\x6f\x6e",8),std::int64_t{21LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x20\x66\x6c\x61\x67",10),false)};trace.properties={EventAttribute{std::string("\x74\x72\x61\x63\x65\x20\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)}};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x32\x31",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kFrozenSet,{integer_value(std::string("\x30",1),std::int64_t{1LL}),EventAttribute{std::string("\x31",1),std::string("\x61",1)}})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x20\x65\x76\x65\x6e\x74",11)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kFrozenSet,{integer_value(std::string("\x30",1),std::int64_t{1LL}),EventAttribute{std::string("\x31",1),std::string("\x61",1)}})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_37(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6e\x61\x74\x69\x76\x65\x2d\x6e\x61\x2d\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e\x2d\x32\x30\x32\x36\x31\x30\x32\x31",31)}};
log.metadata.properties={};
return log;}
EventLog snapshot_38(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6e\x61\x74\x69\x76\x65\x2d\x6e\x61\x2d\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e\x2d\x32\x30\x32\x36\x31\x30\x32\x31",31)}};
log.metadata.properties={};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x4e\x41\x2d\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e\x2d\x30",17)}};trace.properties={};
trace.case_id=std::string("\x4e\x41\x2d\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e\x2d\x30",17);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_39(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6e\x61\x74\x69\x76\x65\x2d\x6e\x61\x2d\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e\x2d\x32\x30\x32\x36\x31\x30\x32\x31",31)}};
log.metadata.properties={};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x4e\x41\x2d\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e\x2d\x30",17)}};trace.properties={};
trace.case_id=std::string("\x4e\x41\x2d\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e\x2d\x30",17);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x6f\x74\x68\x65\x72",5),std::string("\x4e\x41\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",15)}});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_40(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6e\x61\x74\x69\x76\x65\x2d\x6e\x61\x2d\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e\x2d\x32\x30\x32\x36\x31\x30\x32\x31",31)}};
log.metadata.properties={};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x4e\x41\x2d\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e\x2d\x30",17)}};trace.properties={};
trace.case_id=std::string("\x4e\x41\x2d\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e\x2d\x30",17);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({make_native_na_attribute(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_41(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6e\x61\x74\x69\x76\x65\x2d\x6e\x61\x2d\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e\x2d\x32\x30\x32\x36\x31\x30\x32\x31",31)}};
log.metadata.properties={};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x4e\x41\x2d\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e\x2d\x30",17)}};trace.properties={};
trace.case_id=std::string("\x4e\x41\x2d\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e\x2d\x30",17);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x4e\x41\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",15)}});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_42(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6e\x61\x74\x69\x76\x65\x2d\x6e\x61\x2d\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e\x2d\x32\x30\x32\x36\x31\x30\x32\x31",31)}};
log.metadata.properties={};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x4e\x41\x2d\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e\x2d\x30",17)}};trace.properties={};
trace.case_id=std::string("\x4e\x41\x2d\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e\x2d\x30",17);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({null_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_43(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6e\x61\x74\x69\x76\x65\x2d\x6e\x61\x2d\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e\x2d\x32\x30\x32\x36\x31\x30\x32\x31",31)}};
log.metadata.properties={};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x4e\x41\x2d\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e\x2d\x30",17)}};trace.properties={};
trace.case_id=std::string("\x4e\x41\x2d\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e\x2d\x30",17);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kList,{make_native_na_attribute(std::string("\x30",1))})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
void retained_public_repetition_cases(){std::size_t matched=0;
{ // default-omitted
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: default-omitted");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // default-none
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: default-none");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // default-empty-parameters
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: default-empty-parameters");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // enum-explicit-range
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x77\x65\x62",3)};
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7);
options.minimum=TokenInteger(std::string("\x31",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_2();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: enum-explicit-range");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // enum-string-precedence
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=TokenInteger(std::string("\x31",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_3();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: enum-string-precedence");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // ignored-controls
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: ignored-controls");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lower-0
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x2d\x31",2));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_0();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: lower-0");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lower-1
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_0();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: lower-1");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lower-2
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x31",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_4();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: lower-2");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lower-3
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: lower-3");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lower-4
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x33",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_5();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: lower-4");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lower-5
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x34",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: lower-5");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lower-6
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=0x1.8000000000000p+0;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: lower-6");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lower-7
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=0x1.0000000000001p+1;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_5();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: lower-7");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lower-8
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=std::numeric_limits<double>::quiet_NaN();
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: lower-8");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lower-9
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=std::numeric_limits<double>::infinity();
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: lower-9");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lower-10
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=-std::numeric_limits<double>::infinity();
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_0();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: lower-10");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lower-11
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x39\x30\x30\x37\x31\x39\x39\x32\x35\x34\x37\x34\x30\x39\x39\x33",16));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: lower-11");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lower-12
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x31\x38\x34\x34\x36\x37\x34\x34\x30\x37\x33\x37\x30\x39\x35\x35\x31\x36\x31\x35",20));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: lower-12");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lower-13
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=false;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_0();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: lower-13");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lower-14
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=true;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_4();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: lower-14");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lower-15
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=nullptr;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for lower-15");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x4e\x6f\x6e\x65\x54\x79\x70\x65\x27\x20\x61\x6e\x64\x20\x27\x69\x6e\x74\x27",60))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lower-16
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=std::string("\x32",1);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for lower-16");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x73\x74\x72\x27\x20\x61\x6e\x64\x20\x27\x69\x6e\x74\x27",55))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // upper-0
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.maximum=TokenInteger(std::string("\x2d\x31",2));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: upper-0");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // upper-1
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.maximum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: upper-1");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // upper-2
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.maximum=TokenInteger(std::string("\x31",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: upper-2");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // upper-3
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_7();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: upper-3");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // upper-4
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.maximum=TokenInteger(std::string("\x33",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: upper-4");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // upper-5
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.maximum=TokenInteger(std::string("\x34",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: upper-5");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // upper-6
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.maximum=0x1.8000000000000p+0;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: upper-6");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // upper-7
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.maximum=std::numeric_limits<double>::quiet_NaN();
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: upper-7");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // upper-8
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.maximum=std::numeric_limits<double>::infinity();
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: upper-8");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // upper-9
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.maximum=-std::numeric_limits<double>::infinity();
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: upper-9");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // upper-10
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.maximum=TokenInteger(std::string("\x39\x30\x30\x37\x31\x39\x39\x32\x35\x34\x37\x34\x30\x39\x39\x33",16));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: upper-10");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // upper-11
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.maximum=false;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: upper-11");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // upper-12
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.maximum=true;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: upper-12");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // upper-13
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.maximum=nullptr;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for upper-13");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x4e\x6f\x6e\x65\x54\x79\x70\x65\x27",60))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // upper-14
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.maximum=std::string("\x32",1);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for upper-14");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x73\x74\x72\x27",55))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // range-0
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_8();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: range-0");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // range-1
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x31",1));
options.maximum=TokenInteger(std::string("\x31",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_9();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: range-1");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // range-2
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_7();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: range-2");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // range-3
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x33",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: range-3");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // range-4
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x2d\x31",2));
options.maximum=TokenInteger(std::string("\x2d\x31",2));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: range-4");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // range-5
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=nullptr;
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for range-5");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x4e\x6f\x6e\x65\x54\x79\x70\x65\x27\x20\x61\x6e\x64\x20\x27\x69\x6e\x74\x27",60))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // range-6
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=nullptr;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for range-6");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x4e\x6f\x6e\x65\x54\x79\x70\x65\x27",60))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // range-7
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=std::numeric_limits<double>::quiet_NaN();
options.maximum=nullptr;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: range-7");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // range-8
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=std::numeric_limits<double>::infinity();
options.maximum=std::numeric_limits<double>::infinity();
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: range-8");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // range-9
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=false;
options.maximum=true;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_3();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: range-9");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // selector-empty
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("",0)};
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12);
options.minimum=TokenInteger(std::string("\x31",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_10();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: selector-empty");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // selector-space
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x20\x41\x20",3)};
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12);
options.minimum=TokenInteger(std::string("\x31",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_11();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: selector-space");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // selector-punctuation
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41\x2c\x26\x3d\x2b\x7c",6)};
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12);
options.minimum=TokenInteger(std::string("\x31",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_12();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: selector-punctuation");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // selector-newline
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41\x0a\x42",3)};
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12);
options.minimum=TokenInteger(std::string("\x31",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_13();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: selector-newline");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // selector-NUL
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41\x00\x42",3)};
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12);
options.minimum=TokenInteger(std::string("\x31",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_14();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: selector-NUL");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // selector-unicode
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\xce\xa9",2)};
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12);
options.minimum=TokenInteger(std::string("\x31",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_15();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: selector-unicode");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // selector-generic-channel
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x77\x65\x62",3)};
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x63\x68\x61\x6e\x6e\x65\x6c",7);
options.minimum=TokenInteger(std::string("\x31",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_2();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: selector-generic-channel");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // selector-empty-key
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x6c\x69\x74\x65\x72\x61\x6c\x20\x65\x6d\x70\x74\x79",13)};
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("",0);
options.minimum=TokenInteger(std::string("\x31",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_16();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: selector-empty-key");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // selector-missing-key
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79",10);
options.minimum=TokenInteger(std::string("\x31",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: selector-missing-key");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // selector-alias-key
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x61\x63\x74\x69\x76\x69\x74\x79",8);
options.minimum=TokenInteger(std::string("\x31",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: selector-alias-key");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // shape-empty-log
const auto source=snapshot_6();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: shape-empty-log");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lazy-empty-log-min_rep
const auto source=snapshot_6();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=nullptr;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: lazy-empty-log-min_rep");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lazy-empty-log-max_rep
const auto source=snapshot_6();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.maximum=nullptr;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: lazy-empty-log-max_rep");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // shape-empty-trace
const auto source=snapshot_17();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_17();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: shape-empty-trace");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lazy-empty-trace-min_rep
const auto source=snapshot_17();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=nullptr;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for lazy-empty-trace-min_rep");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x4e\x6f\x6e\x65\x54\x79\x70\x65\x27\x20\x61\x6e\x64\x20\x27\x69\x6e\x74\x27",60))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lazy-empty-trace-max_rep
const auto source=snapshot_17();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.maximum=nullptr;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: lazy-empty-trace-max_rep");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // shape-missing
const auto source=snapshot_18();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_18();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: shape-missing");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lazy-missing-min_rep
const auto source=snapshot_18();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=nullptr;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for lazy-missing-min_rep");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x4e\x6f\x6e\x65\x54\x79\x70\x65\x27\x20\x61\x6e\x64\x20\x27\x69\x6e\x74\x27",60))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lazy-missing-max_rep
const auto source=snapshot_18();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.maximum=nullptr;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: lazy-missing-max_rep");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // shape-mixed-missing
const auto source=snapshot_19();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_19();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: shape-mixed-missing");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lazy-mixed-missing-min_rep
const auto source=snapshot_19();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=nullptr;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for lazy-mixed-missing-min_rep");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x4e\x6f\x6e\x65\x54\x79\x70\x65\x27\x20\x61\x6e\x64\x20\x27\x69\x6e\x74\x27",60))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // lazy-mixed-missing-max_rep
const auto source=snapshot_19();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.maximum=nullptr;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for lazy-mixed-missing-max_rep");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x4e\x6f\x6e\x65\x54\x79\x70\x65\x27",60))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // typed-target-0
const auto source=snapshot_20();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=null_value(std::string("",0));
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_21();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: typed-target-0");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // typed-target-1
const auto source=snapshot_20();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=boolean_value(std::string("",0),true);
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_22();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: typed-target-1");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // typed-target-2
const auto source=snapshot_20();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=boolean_value(std::string("",0),false);
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_23();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: typed-target-2");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // typed-target-3
const auto source=snapshot_20();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=integer_value(std::string("",0),std::int64_t{0LL});
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_23();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: typed-target-3");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // typed-target-4
const auto source=snapshot_20();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=integer_value(std::string("",0),std::int64_t{1LL});
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_22();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: typed-target-4");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // typed-target-5
const auto source=snapshot_20();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=float_value(std::string("",0),0x1.0000000000000p+0);
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_22();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: typed-target-5");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // typed-target-6
const auto source=snapshot_20();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=float_value(std::string("",0),0x1.8000000000000p+0);
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_24();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: typed-target-6");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // typed-target-7
const auto source=snapshot_20();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=integer_value(std::string("",0),std::int64_t{9007199254740992LL});
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_25();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: typed-target-7");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // typed-target-8
const auto source=snapshot_20();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=integer_value(std::string("",0),std::int64_t{9007199254740993LL});
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_26();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: typed-target-8");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // typed-target-9
const auto source=snapshot_20();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=float_value(std::string("",0),0x1.0000000000000p+53);
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_25();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: typed-target-9");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // typed-target-10
const auto source=snapshot_20();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x31",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_27();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: typed-target-10");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // typed-target-11
const auto source=snapshot_20();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x4e\x61\x4e",3)};
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_28();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: typed-target-11");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // typed-target-12
const auto source=snapshot_20();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=float_value(std::string("",0),std::numeric_limits<double>::quiet_NaN());
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: typed-target-12");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // typed-target-13
const auto source=snapshot_20();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=float_value(std::string("",0),std::numeric_limits<double>::infinity());
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_29();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: typed-target-13");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // typed-target-14
const auto source=snapshot_20();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=float_value(std::string("",0),-std::numeric_limits<double>::infinity());
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_30();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: typed-target-14");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // typed-target-15
const auto source=snapshot_20();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=date_value(std::string("",0),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x36\x2e\x31\x32\x33\x34\x35\x36\x2b\x30\x30\x3a\x30\x30",32));
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_31();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: typed-target-15");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // typed-target-16
const auto source=snapshot_20();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x32\x30\x32\x36\x2d\x30\x33\x2d\x30\x33\x54\x30\x34\x3a\x30\x35\x3a\x30\x36\x2e\x31\x32\x33\x34\x35\x36\x2b\x30\x30\x3a\x30\x30",32)};
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_32();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: typed-target-16");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // typed-target-17
const auto source=snapshot_20();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=container_value(std::string("",0),AttributeContainerKind::kList,{EventAttribute{std::string("\x30",1),std::string("\x61",1)},integer_value(std::string("\x31",1),std::int64_t{1LL})});
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_33();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: typed-target-17");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // typed-target-18
const auto source=snapshot_20();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=container_value(std::string("",0),AttributeContainerKind::kTuple,{EventAttribute{std::string("\x30",1),std::string("\x61",1)},integer_value(std::string("\x31",1),std::int64_t{1LL})});
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_34();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: typed-target-18");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // typed-target-19
const auto source=snapshot_20();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=container_value(std::string("",0),AttributeContainerKind::kContainer,{integer_value(std::string("\x78",1),std::int64_t{1LL})});
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_35();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: typed-target-19");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // typed-target-20
const auto source=snapshot_20();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=container_value(std::string("",0),AttributeContainerKind::kSet,{integer_value(std::string("\x30",1),std::int64_t{1LL}),EventAttribute{std::string("\x31",1),std::string("\x61",1)}});
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_36();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: typed-target-20");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // typed-target-21
const auto source=snapshot_20();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=container_value(std::string("",0),AttributeContainerKind::kFrozenSet,{integer_value(std::string("\x30",1),std::int64_t{1LL}),EventAttribute{std::string("\x31",1),std::string("\x61",1)}});
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=TokenInteger(std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_36();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: typed-target-21");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // upper-short-circuit
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x39\x39",2));
options.maximum=nullptr;
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: upper-short-circuit");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // null-attribute-selector
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=nullptr;
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_0();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: null-attribute-selector");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // numeric-attribute-selector
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=TokenInteger(std::string("\x37",1));
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_0();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: numeric-attribute-selector");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RNA-B1-target-na-empty-log
const auto source=snapshot_37();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=make_native_na_attribute(std::string("",0));
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_37();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RNA-B1-target-na-empty-log");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RNA-B1-target-na-empty-trace
const auto source=snapshot_38();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=make_native_na_attribute(std::string("",0));
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_37();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RNA-B1-target-na-empty-trace");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RNA-B1-target-na-missing
const auto source=snapshot_39();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=make_native_na_attribute(std::string("",0));
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_37();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RNA-B1-target-na-missing");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RNA-B1-target-na-na-cell
const auto source=snapshot_40();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=make_native_na_attribute(std::string("",0));
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RNA-B1-target-na-na-cell");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x62\x6f\x6f\x6c\x65\x61\x6e\x20\x76\x61\x6c\x75\x65\x20\x6f\x66\x20\x4e\x41\x20\x69\x73\x20\x61\x6d\x62\x69\x67\x75\x6f\x75\x73",32))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RNA-B1-target-na-ordinary-cell
const auto source=snapshot_41();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=make_native_na_attribute(std::string("",0));
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RNA-B1-target-na-ordinary-cell");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x62\x6f\x6f\x6c\x65\x61\x6e\x20\x76\x61\x6c\x75\x65\x20\x6f\x66\x20\x4e\x41\x20\x69\x73\x20\x61\x6d\x62\x69\x67\x75\x6f\x75\x73",32))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RNA-B1-target-na-null-cell
const auto source=snapshot_42();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=make_native_na_attribute(std::string("",0));
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_37();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RNA-B1-target-na-null-cell");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RNA-B1-na-cell-string
const auto source=snapshot_40();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x4e\x41\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",15)};
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RNA-B1-na-cell-string");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x62\x6f\x6f\x6c\x65\x61\x6e\x20\x76\x61\x6c\x75\x65\x20\x6f\x66\x20\x4e\x41\x20\x69\x73\x20\x61\x6d\x62\x69\x67\x75\x6f\x75\x73",32))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RNA-B1-na-cell-null
const auto source=snapshot_40();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=null_value(std::string("",0));
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_37();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RNA-B1-na-cell-null");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RNA-B1-na-cell-integer
const auto source=snapshot_40();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=integer_value(std::string("",0),std::int64_t{1LL});
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RNA-B1-na-cell-integer");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x62\x6f\x6f\x6c\x65\x61\x6e\x20\x76\x61\x6c\x75\x65\x20\x6f\x66\x20\x4e\x41\x20\x69\x73\x20\x61\x6d\x62\x69\x67\x75\x6f\x75\x73",32))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RNA-B1-nested-na
const auto source=snapshot_43();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=container_value(std::string("",0),AttributeContainerKind::kList,{make_native_na_attribute(std::string("\x30",1))});
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_37();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RNA-B1-nested-na");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RNA-B1-lower-na-empty
const auto source=snapshot_37();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x4e\x41\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",15)};
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=NativeNA{};
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_37();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RNA-B1-lower-na-empty");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RNA-B1-lower-na-present
const auto source=snapshot_41();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x4e\x41\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",15)};
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=NativeNA{};
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RNA-B1-lower-na-present");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x62\x6f\x6f\x6c\x65\x61\x6e\x20\x76\x61\x6c\x75\x65\x20\x6f\x66\x20\x4e\x41\x20\x69\x73\x20\x61\x6d\x62\x69\x67\x75\x6f\x75\x73",32))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RNA-B1-upper-na-short-circuit
const auto source=snapshot_41();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x4e\x41\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",15)};
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x32",1));
options.maximum=NativeNA{};
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_37();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RNA-B1-upper-na-short-circuit");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RNA-B1-upper-na-present
const auto source=snapshot_41();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x4e\x41\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",15)};
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=NativeNA{};
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RNA-B1-upper-na-present");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x62\x6f\x6f\x6c\x65\x61\x6e\x20\x76\x61\x6c\x75\x65\x20\x6f\x66\x20\x4e\x41\x20\x69\x73\x20\x61\x6d\x62\x69\x67\x75\x6f\x75\x73",32))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
assert(matched==109);std::cout<<matched<<"/109 retained full repetition outcomes/errors passed\n";}
void native_storage_controls(){
 auto source=snapshot_0();source.traces={source.traces.at(3)};
 source.metadata.compatibility_snapshot_valid=true;
 source.metadata.compatibility_globals_snapshot={{"raw",{{"raw",std::string("\xff\0",2)}}}};
 source.traces.front().events.front().input_order=42;
 const auto before=serialize_embedding_workbench_raw_log_json(source);
 auto result=filter_log_attribute_value_repetition(source,EventAttribute{"","A"});
 assert(serialize_embedding_workbench_raw_log_json(result)==before);
 assert(result.metadata.compatibility_snapshot_valid&&result.metadata.compatibility_globals_snapshot==source.metadata.compatibility_globals_snapshot);
 result.traces.front().events.clear();assert(source.traces.front().events.size()==3);
 EventAttribute nan=float_value("",std::numeric_limits<double>::quiet_NaN());
 source.traces.front().events={event_for({nan}),event_for({nan})};
 LogAttributeRepetitionOptions options;options.attribute_key=std::string("");options.minimum=TokenInteger(0);options.maximum=TokenInteger(0);
 assert(filter_log_attribute_value_repetition(source,nan,options).trace_count()==1);
 options.attribute_key=std::string("missing");options.minimum=TokenInteger(99);options.maximum=nullptr;
 assert(filter_log_attribute_value_repetition(source,EventAttribute{"","A"},options).traces.empty());
}
void native_first_class_and_exact_dates(){
 EventLog source;Trace trace;Event event;event.activity="A";
 event.activity_attribute=EventAttribute{"concept:name","A"};
 trace.events={event,event};source.traces={trace};
 assert(filter_log_attribute_value_repetition(source,EventAttribute{"","A"}).trace_count()==1);
 for(auto& value:source.traces.front().events){value.activity="";value.activity_attribute=EventAttribute{"concept:name",""};}
 assert(filter_log_attribute_value_repetition(source,EventAttribute{"",""}).trace_count()==1);
 for(auto& value:source.traces.front().events){value.activity_attribute=null_value("concept:name");}
 assert(filter_log_attribute_value_repetition(source,null_value("")).trace_count()==1);
 for(auto& value:source.traces.front().events){value.activity="A";value.activity_attribute=EventAttribute{"concept:name","A"};value.activity_attribute->first_class_role=FirstClassAttributeRole::kActivity;value.activity_attribute->first_class_state=FirstClassAttributeState::kMissing;}
 assert(filter_log_attribute_value_repetition(source,EventAttribute{"","A"}).traces.empty());
 auto first=date_value("date","2026-03-03T04:05:06.000000001Z");
 auto second=date_value("","2026-03-03T04:05:06.000000002Z");
 first.typed_value=NativeDate{first.value,NativeDateResolution::kNanosecond};
 second.typed_value=NativeDate{second.value,NativeDateResolution::kNanosecond};
 source.traces.front().events={event_for({first}),event_for({first})};
 LogAttributeRepetitionOptions options;options.attribute_key=std::string("date");
 assert(filter_log_attribute_value_repetition(source,second,options).traces.empty());
 assert(filter_log_attribute_value_repetition(source,date_value("","2026-03-03T05:05:06.000000001+01:00"),options).trace_count()==1);
 assert(filter_log_attribute_value_repetition(source,date_value("","2026-03-03T04:05:06.000000001"),options).traces.empty());
}
} // namespace
int main(){try{retained_public_repetition_cases();native_storage_controls();native_first_class_and_exact_dates();}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
