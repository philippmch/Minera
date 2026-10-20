#include "minera/log_rework_filter.hpp"
#include "minera/embedding_workbench_json.hpp"
#include <cassert>
#include <limits>
#include <stdexcept>
#include <iostream>
using namespace minera;
namespace {
EventAttribute string_value(std::string key,std::string value){return {std::move(key),std::move(value)};}
EventAttribute null_value(std::string key){EventAttribute a{std::move(key),""};a.is_null=true;return a;}
EventAttribute integer_value(std::string key,std::int64_t value){EventAttribute a{std::move(key),std::to_string(value),AttributeKind::kInt};a.typed_value=value;return a;}
EventAttribute float_value(std::string key,double value){EventAttribute a{std::move(key),std::to_string(value),AttributeKind::kFloat};a.typed_value=value;return a;}
EventAttribute boolean_value(std::string key,bool value){EventAttribute a{std::move(key),value?"true":"false",AttributeKind::kBoolean};a.typed_value=value;return a;}
EventAttribute date_value(std::string key,std::string value){EventAttribute a{std::move(key),value,AttributeKind::kDate};a.typed_value=NativeDate{value};return a;}
Event event_for(std::vector<EventAttribute> attributes) {
 Event event;event.attributes=std::move(attributes);
 for(const auto& a:event.attributes) {
  if(a.key=="concept:name"){event.activity=a.value;event.activity_attribute=a;event.activity_state=a.is_null?FirstClassAttributeState::kNull:FirstClassAttributeState::kPresent;event.activity_source_key=a.key;}
  if(a.key=="time:timestamp"){event.timestamp=a.value;event.timestamp_attribute=a;event.timestamp_state=FirstClassAttributeState::kPresent;event.timestamp_source_key=a.key;}
  if(a.key=="org:resource"){event.resource=a.value;event.resource_attribute=a;event.resource_state=FirstClassAttributeState::kPresent;event.resource_source_key=a.key;}
 }
 return event;
}

EventLog source_fixture(){ EventLog log;
log.attributes={string_value(std::string("\x6c\x6f\x67\x5f\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)),integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL})};
log.metadata.properties={string_value(std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x6b\x65\x65\x70",4))};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x74\x72\x61\x63\x65",12),std::string("\x74",1))};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x74\x72\x61\x63\x65",12),std::string("\x74",1))}});
log.metadata.event_globals={string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x65\x76\x65\x6e\x74",12),std::string("\x65",1))};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x65\x76\x65\x6e\x74",12),std::string("\x65",1))}});
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x30",6)),integer_value(std::string("\x74\x72\x61\x63\x65\x5f\x69\x6e\x74",9),std::int64_t{0LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x5f\x66\x6c\x61\x67",10),false)};trace.properties={string_value(std::string("\x74\x72\x61\x63\x65\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x70\x30",2))};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x30",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31",6)),integer_value(std::string("\x74\x72\x61\x63\x65\x5f\x69\x6e\x74",9),std::int64_t{1LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x5f\x66\x6c\x61\x67",10),false)};trace.properties={string_value(std::string("\x74\x72\x61\x63\x65\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x70\x31",2))};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("",0)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a",2)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740993LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x32",6)),integer_value(std::string("\x74\x72\x61\x63\x65\x5f\x69\x6e\x74",9),std::int64_t{2LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x5f\x66\x6c\x61\x67",10),false)};trace.properties={string_value(std::string("\x74\x72\x61\x63\x65\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x70\x32",2))};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x32",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("",0)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a",2)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740993LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("",0)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a",2)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740994LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x33",6)),integer_value(std::string("\x74\x72\x61\x63\x65\x5f\x69\x6e\x74",9),std::int64_t{3LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x5f\x66\x6c\x61\x67",10),false)};trace.properties={string_value(std::string("\x74\x72\x61\x63\x65\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x70\x33",2))};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x33",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\x41",3)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740993LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x34",6)),integer_value(std::string("\x74\x72\x61\x63\x65\x5f\x69\x6e\x74",9),std::int64_t{4LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x5f\x66\x6c\x61\x67",10),false)};trace.properties={string_value(std::string("\x74\x72\x61\x63\x65\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x70\x34",2))};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x34",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\x41",3)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740993LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x42",1)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\x42",3)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740994LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x35",6)),integer_value(std::string("\x74\x72\x61\x63\x65\x5f\x69\x6e\x74",9),std::int64_t{5LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x5f\x66\x6c\x61\x67",10),false)};trace.properties={string_value(std::string("\x74\x72\x61\x63\x65\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x70\x35",2))};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x35",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\x41",3)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740993LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\x41",3)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740994LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x36",6)),integer_value(std::string("\x74\x72\x61\x63\x65\x5f\x69\x6e\x74",9),std::int64_t{6LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x5f\x66\x6c\x61\x67",10),false)};trace.properties={string_value(std::string("\x74\x72\x61\x63\x65\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x70\x36",2))};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x36",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\x41",3)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740993LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x42",1)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\x42",3)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740994LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\x41",3)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740995LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x32\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=2;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x37",6)),integer_value(std::string("\x74\x72\x61\x63\x65\x5f\x69\x6e\x74",9),std::int64_t{7LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x5f\x66\x6c\x61\x67",10),false)};trace.properties={string_value(std::string("\x74\x72\x61\x63\x65\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x70\x37",2))};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x37",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\x41",3)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740993LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\x41",3)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740994LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\x41",3)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740995LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x32\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=2;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x38",6)),integer_value(std::string("\x74\x72\x61\x63\x65\x5f\x69\x6e\x74",9),std::int64_t{8LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x5f\x66\x6c\x61\x67",10),false)};trace.properties={string_value(std::string("\x74\x72\x61\x63\x65\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x70\x38",2))};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x38",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x42",1)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\x42",3)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740993LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x39",6)),integer_value(std::string("\x74\x72\x61\x63\x65\x5f\x69\x6e\x74",9),std::int64_t{9LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x5f\x66\x6c\x61\x67",10),false)};trace.properties={string_value(std::string("\x74\x72\x61\x63\x65\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x70\x39",2))};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x39",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x2e\x2e\x2e",3)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\x2e\x2e\x2e",5)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740993LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x2e\x2e\x2e",3)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\x2e\x2e\x2e",5)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740994LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x30",7)),integer_value(std::string("\x74\x72\x61\x63\x65\x5f\x69\x6e\x74",9),std::int64_t{10LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x5f\x66\x6c\x61\x67",10),false)};trace.properties={string_value(std::string("\x74\x72\x61\x63\x65\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x70\x31\x30",3))};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x30",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x2c\x42",3)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\x41\x2c\x42",5)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740993LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x2c\x42",3)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\x41\x2c\x42",5)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740994LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x31",7)),integer_value(std::string("\x74\x72\x61\x63\x65\x5f\x69\x6e\x74",9),std::int64_t{11LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x5f\x66\x6c\x61\x67",10),false)};trace.properties={string_value(std::string("\x74\x72\x61\x63\x65\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x70\x31\x31",3))};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x31",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x20\x41\x20",3)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\x20\x41\x20",5)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740993LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x20\x41\x20",3)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\x20\x41\x20",5)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740994LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x32",7)),integer_value(std::string("\x74\x72\x61\x63\x65\x5f\x69\x6e\x74",9),std::int64_t{12LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x5f\x66\x6c\x61\x67",10),false)};trace.properties={string_value(std::string("\x74\x72\x61\x63\x65\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x70\x31\x32",3))};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x32",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x0a\x42",3)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\x41\x0a\x42",5)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740993LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x0a\x42",3)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\x41\x0a\x42",5)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740994LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x33",7)),integer_value(std::string("\x74\x72\x61\x63\x65\x5f\x69\x6e\x74",9),std::int64_t{13LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x5f\x66\x6c\x61\x67",10),false)};trace.properties={string_value(std::string("\x74\x72\x61\x63\x65\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x70\x31\x33",3))};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x33",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x00\x42",3)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\x41\x00\x42",5)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740993LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41\x00\x42",3)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\x41\x00\x42",5)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740994LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x31\x34",7)),integer_value(std::string("\x74\x72\x61\x63\x65\x5f\x69\x6e\x74",9),std::int64_t{14LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x5f\x66\x6c\x61\x67",10),false)};trace.properties={string_value(std::string("\x74\x72\x61\x63\x65\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x70\x31\x34",3))};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x31\x34",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\xce\xa9",2)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\xce\xa9",4)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740993LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x30\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\xce\xa9",2)),string_value(std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::string("\x43\x3a\xce\xa9",4)),integer_value(std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{9007199254740994LL}),float_value(std::string("\x63\x6f\x73\x74",4),0x1.4000000000000p+0),boolean_value(std::string("\x66\x6c\x61\x67",4),true),null_value(std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8)),date_value(std::string("\x74\x69\x6d\x65\x3a\x74\x69\x6d\x65\x73\x74\x61\x6d\x70",14),std::string("\x32\x30\x32\x36\x2d\x30\x31\x2d\x30\x32\x54\x30\x33\x3a\x30\x34\x3a\x30\x31\x2b\x30\x30\x3a\x30\x30",25)),string_value(std::string("\x6f\x72\x67\x3a\x72\x65\x73\x6f\x75\x72\x63\x65",12),std::string("\x20\x72\x20",3))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
return log; }
EventLog source_mixed(){ EventLog log;
log.attributes={string_value(std::string("\x6c\x6f\x67\x5f\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)),integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL})};
log.metadata.properties={string_value(std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x6b\x65\x65\x70",4))};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x74\x72\x61\x63\x65",12),std::string("\x74",1))};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x74\x72\x61\x63\x65",12),std::string("\x74",1))}});
log.metadata.event_globals={string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x65\x76\x65\x6e\x74",12),std::string("\x65",1))};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x65\x76\x65\x6e\x74",12),std::string("\x65",1))}});
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6d\x69\x78\x65\x64\x2d\x30",7))};trace.properties={string_value(std::string("\x74\x72\x61\x63\x65\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x6d\x69\x78\x65\x64",5))};
trace.case_id=std::string("\x6d\x69\x78\x65\x64\x2d\x30",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({string_value(std::string("\x6f\x74\x68\x65\x72",5),std::string("\x41",1))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1))});event.input_order=2;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6d\x69\x78\x65\x64\x2d\x31",7))};trace.properties={};
trace.case_id=std::string("\x6d\x69\x78\x65\x64\x2d\x31",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x42",1))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({null_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({integer_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{3LL})});event.input_order=2;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({boolean_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),true)});event.input_order=3;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
return log; }
EventLog source_missing(){ EventLog log;
log.attributes={string_value(std::string("\x6c\x6f\x67\x5f\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)),integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL})};
log.metadata.properties={string_value(std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x6b\x65\x65\x70",4))};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x74\x72\x61\x63\x65",12),std::string("\x74",1))};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x74\x72\x61\x63\x65",12),std::string("\x74",1))}});
log.metadata.event_globals={string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x65\x76\x65\x6e\x74",12),std::string("\x65",1))};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x65\x76\x65\x6e\x74",12),std::string("\x65",1))}});
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6d\x69\x73\x73\x69\x6e\x67",7))};trace.properties={};
trace.case_id=std::string("\x6d\x69\x73\x73\x69\x6e\x67",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({string_value(std::string("\x6f\x74\x68\x65\x72",5),std::string("\x41",1))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
return log; }
EventLog source_zero(){ EventLog log;
log.attributes={string_value(std::string("\x6c\x6f\x67\x5f\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)),integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL})};
log.metadata.properties={string_value(std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x6b\x65\x65\x70",4))};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x74\x72\x61\x63\x65",12),std::string("\x74",1))};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x74\x72\x61\x63\x65",12),std::string("\x74",1))}});
log.metadata.event_globals={string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x65\x76\x65\x6e\x74",12),std::string("\x65",1))};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x65\x76\x65\x6e\x74",12),std::string("\x65",1))}});
return log; }
EventLog source_empty(){ EventLog log;
log.attributes={string_value(std::string("\x6c\x6f\x67\x5f\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)),integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL})};
log.metadata.properties={string_value(std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x6b\x65\x65\x70",4))};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x74\x72\x61\x63\x65",12),std::string("\x74",1))};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x74\x72\x61\x63\x65",12),std::string("\x74",1))}});
log.metadata.event_globals={string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x65\x76\x65\x6e\x74",12),std::string("\x65",1))};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x65\x76\x65\x6e\x74",12),std::string("\x65",1))}});
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x63\x61\x73\x65\x2d\x30",6)),integer_value(std::string("\x74\x72\x61\x63\x65\x5f\x69\x6e\x74",9),std::int64_t{0LL}),boolean_value(std::string("\x74\x72\x61\x63\x65\x5f\x66\x6c\x61\x67",10),false)};trace.properties={string_value(std::string("\x74\x72\x61\x63\x65\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",14),std::string("\x70\x30",2))};
trace.case_id=std::string("\x63\x61\x73\x65\x2d\x30",6);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
log.traces.push_back(trace); }
return log; }
EventLog source_typed(){ EventLog log;
log.attributes={string_value(std::string("\x6c\x6f\x67\x5f\x61\x74\x74\x72\x69\x62\x75\x74\x65",13),std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c",8)),integer_value(std::string("\x6e\x75\x6d\x62\x65\x72",6),std::int64_t{9007199254740993LL})};
log.metadata.properties={string_value(std::string("\x6f\x72\x69\x67\x69\x6e\x61\x6c\x5f\x70\x72\x6f\x70\x65\x72\x74\x79",17),std::string("\x6b\x65\x65\x70",4))};
log.metadata.extensions.push_back({std::string("\x43\x6f\x6e\x63\x65\x70\x74",7),std::string("\x63\x6f\x6e\x63\x65\x70\x74",7),std::string("\x75\x72\x6e\x3a\x63\x6f\x6e\x63\x65\x70\x74",11)});
log.metadata.classifiers.push_back({std::string("\x41\x63\x74\x69\x76\x69\x74\x79",8),{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
log.metadata.trace_globals={string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x74\x72\x61\x63\x65",12),std::string("\x74",1))};
log.metadata.globals.push_back({std::string("\x74\x72\x61\x63\x65",5),{string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x74\x72\x61\x63\x65",12),std::string("\x74",1))}});
log.metadata.event_globals={string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x65\x76\x65\x6e\x74",12),std::string("\x65",1))};
log.metadata.globals.push_back({std::string("\x65\x76\x65\x6e\x74",5),{string_value(std::string("\x67\x6c\x6f\x62\x61\x6c\x5f\x65\x76\x65\x6e\x74",12),std::string("\x65",1))}});
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x2d\x30",7))};trace.properties={};
trace.case_id=std::string("\x74\x79\x70\x65\x64\x2d\x30",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({integer_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{3LL})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({integer_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{3LL})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x2d\x31",7))};trace.properties={};
trace.case_id=std::string("\x74\x79\x70\x65\x64\x2d\x31",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({float_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),0x1.8000000000000p+1)});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({float_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),0x1.8000000000000p+1)});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x2d\x32",7))};trace.properties={};
trace.case_id=std::string("\x74\x79\x70\x65\x64\x2d\x32",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({boolean_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),true)});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({boolean_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),true)});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x2d\x33",7))};trace.properties={};
trace.case_id=std::string("\x74\x79\x70\x65\x64\x2d\x33",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({integer_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{1LL})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({integer_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{1LL})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x2d\x34",7))};trace.properties={};
trace.case_id=std::string("\x74\x79\x70\x65\x64\x2d\x34",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({null_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({null_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x2d\x35",7))};trace.properties={};
trace.case_id=std::string("\x74\x79\x70\x65\x64\x2d\x35",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x33",1))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x33",1))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x2d\x36",7))};trace.properties={};
trace.case_id=std::string("\x74\x79\x70\x65\x64\x2d\x36",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x54\x72\x75\x65",4))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x54\x72\x75\x65",4))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x2d\x37",7))};trace.properties={};
trace.case_id=std::string("\x74\x79\x70\x65\x64\x2d\x37",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("",0))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("",0))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x2d\x38",7))};trace.properties={};
trace.case_id=std::string("\x74\x79\x70\x65\x64\x2d\x38",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({boolean_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),false)});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({boolean_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),false)});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x2d\x39",7))};trace.properties={};
trace.case_id=std::string("\x74\x79\x70\x65\x64\x2d\x39",7);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({integer_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{0LL})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({integer_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{0LL})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
{ Trace trace;trace.attributes={string_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x74\x79\x70\x65\x64\x2d\x31\x30",8))};trace.properties={};
trace.case_id=std::string("\x74\x79\x70\x65\x64\x2d\x31\x30",8);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=string_value("concept:name",trace.case_id);
{auto event=event_for({float_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),0x0.0p+0)});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
{auto event=event_for({float_value(std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),0x0.0p+0)});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(event);}
log.traces.push_back(trace); }
return log; }
void log_rework_filter_module_acceptance(){
{ // 1 default-omitted
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,std::string("\x41",1));returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 2 default-None
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,std::string("\x41",1));returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 3 default-empty-dict
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,std::string("\x41",1));returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 4 enum-controls
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x43\x3a\x41",3)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 5 minimum-negative-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{-1LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 6 minimum-negative-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{-1LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 7 minimum-zero-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{0LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 8 minimum-zero-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{0LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 9 minimum-one-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{1LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 10 minimum-one-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{1LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 11 minimum-two-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 12 minimum-two-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 13 minimum-three-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{3LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(7));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 14 minimum-three-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{3LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 15 minimum-four-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{4LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 16 minimum-four-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{4LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 17 minimum-fraction-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),0x1.8000000000000p+0,true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 18 minimum-fraction-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),0x1.8000000000000p+0,false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 19 minimum-tiny-positive-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),0x0.0000000000001p-1022,true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 20 minimum-tiny-positive-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),0x0.0000000000001p-1022,false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 21 minimum-fraction-above-two-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),0x1.0000000000001p+1,true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(7));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 22 minimum-fraction-above-two-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),0x1.0000000000001p+1,false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 23 minimum-NaN-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::numeric_limits<double>::quiet_NaN(),true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 24 minimum-NaN-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::numeric_limits<double>::quiet_NaN(),false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 25 minimum-positive-INF-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::numeric_limits<double>::infinity(),true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 26 minimum-positive-INF-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::numeric_limits<double>::infinity(),false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 27 minimum-negative-INF-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),-std::numeric_limits<double>::infinity(),true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 28 minimum-negative-INF-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),-std::numeric_limits<double>::infinity(),false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 29 minimum-large-exact-integer-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{9007199254740993LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 30 minimum-large-exact-integer-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{9007199254740993LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 31 minimum-uint64-max-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::uint64_t{18446744073709551615ULL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 32 minimum-uint64-max-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::uint64_t{18446744073709551615ULL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 33 minimum-False-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),false,true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 34 minimum-False-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),false,false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 35 minimum-True-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),true,true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 36 minimum-True-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),true,false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 37 minimum-None-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::monostate{},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
assert(false && "expected captured error");
}catch(const std::invalid_argument& e){assert(std::string(e.what())==std::string("\x27\x3e\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x4e\x6f\x6e\x65\x54\x79\x70\x65\x27",60));}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 38 minimum-None-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::monostate{},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
assert(false && "expected captured error");
}catch(const std::invalid_argument& e){assert(std::string(e.what())==std::string("\x27\x3c\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x4e\x6f\x6e\x65\x54\x79\x70\x65\x27",59));}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 39 minimum-string-two-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x32",1),true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
assert(false && "expected captured error");
}catch(const std::invalid_argument& e){assert(std::string(e.what())==std::string("\x27\x3e\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x73\x74\x72\x27",55));}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 40 minimum-string-two-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x32",1),false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
assert(false && "expected captured error");
}catch(const std::invalid_argument& e){assert(std::string(e.what())==std::string("\x27\x3c\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x73\x74\x72\x27",54));}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 41 empty-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("",0)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(2));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 42 empty-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("",0)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 43 custom-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x43\x3a\x41",3)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 44 custom-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79",15),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x43\x3a\x41",3)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 45 missing-key-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79",10),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
assert(false && "expected captured error");
}catch(const std::out_of_range& e){assert(std::string(e.what())==std::string("\x27\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79\x27",12));}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 46 missing-key-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79",10),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
assert(false && "expected captured error");
}catch(const std::out_of_range& e){assert(std::string(e.what())==std::string("\x27\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79\x27",12));}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 47 numeric-key-string-target-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x39\x30\x30\x37\x31\x39\x39\x32\x35\x34\x37\x34\x30\x39\x39\x33",16)),options);returned=true;
auto expected=source;expected.traces.clear();
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 48 numeric-key-string-target-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x39\x30\x30\x37\x31\x39\x39\x32\x35\x34\x37\x34\x30\x39\x39\x33",16)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 49 null-key-string-target-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x4e\x6f\x6e\x65",4)),options);returned=true;
auto expected=source;expected.traces.clear();
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 50 null-key-string-target-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x4e\x6f\x6e\x65",4)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 51 numeric-key-numeric-target-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,integer_value(std::string("",0),std::int64_t{9007199254740993LL}),options);returned=true;
auto expected=source;expected.traces.clear();
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 52 numeric-key-numeric-target-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x75\x6e\x74",5),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,integer_value(std::string("",0),std::int64_t{9007199254740993LL}),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 53 null-key-null-target-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,null_value(std::string("",0)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 54 null-key-null-target-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x6e\x75\x6c\x6c\x61\x62\x6c\x65",8),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,null_value(std::string("",0)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(8));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 55 spaces-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x20\x41\x20",3)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(11));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 56 spaces-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x20\x41\x20",3)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 57 NUL-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41\x00\x42",3)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(13));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 58 NUL-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41\x00\x42",3)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 59 comma-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41\x2c\x42",3)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(10));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 60 comma-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41\x2c\x42",3)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 61 newline-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41\x0a\x42",3)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(12));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 62 newline-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41\x0a\x42",3)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(13));
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 63 unicode-True
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\xce\xa9",2)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(14));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 64 unicode-False
const auto source=source_fixture();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\xce\xa9",2)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
expected.traces.push_back(source.traces.at(11));
expected.traces.push_back(source.traces.at(12));
expected.traces.push_back(source.traces.at(13));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 65 mixed-missing-True
const auto source=source_mixed();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
assert(false && "expected captured error");
}catch(const std::out_of_range& e){assert(std::string(e.what())==std::string("\x27\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65\x27",14));}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 66 mixed-missing-False
const auto source=source_mixed();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
assert(false && "expected captured error");
}catch(const std::out_of_range& e){assert(std::string(e.what())==std::string("\x27\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65\x27",14));}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 67 only-missing-True
const auto source=source_missing();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
assert(false && "expected captured error");
}catch(const std::out_of_range& e){assert(std::string(e.what())==std::string("\x27\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65\x27",14));}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 68 only-missing-False
const auto source=source_missing();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
assert(false && "expected captured error");
}catch(const std::out_of_range& e){assert(std::string(e.what())==std::string("\x27\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65\x27",14));}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 69 zero-True-0
const auto source=source_zero();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79",10),std::int64_t{0LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 70 zero-True-2
const auto source=source_zero();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79",10),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 71 zero-False-0
const auto source=source_zero();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79",10),std::int64_t{0LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 72 zero-False-2
const auto source=source_zero();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79",10),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 73 empty-True-0
const auto source=source_empty();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79",10),std::int64_t{0LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 74 empty-True-2
const auto source=source_empty();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79",10),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 75 empty-False-0
const auto source=source_empty();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79",10),std::int64_t{0LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 76 empty-False-2
const auto source=source_empty();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79",10),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x41",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 77 typed-target-int-True
const auto source=source_typed();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,integer_value(std::string("",0),std::int64_t{3LL}),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 78 typed-target-int-False
const auto source=source_typed();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,integer_value(std::string("",0),std::int64_t{3LL}),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 79 typed-target-float-True
const auto source=source_typed();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,float_value(std::string("",0),0x1.8000000000000p+1),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 80 typed-target-float-False
const auto source=source_typed();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,float_value(std::string("",0),0x1.8000000000000p+1),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 81 typed-target-bool-True
const auto source=source_typed();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,boolean_value(std::string("",0),true),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 82 typed-target-bool-False
const auto source=source_typed();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,boolean_value(std::string("",0),true),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 83 typed-target-null-True
const auto source=source_typed();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,null_value(std::string("",0)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(4));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 84 typed-target-null-False
const auto source=source_typed();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,null_value(std::string("",0)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 85 typed-target-string-True
const auto source=source_typed();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x33",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(5));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 86 typed-target-string-False
const auto source=source_typed();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,string_value(std::string("",0),std::string("\x33",1)),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 87 typed-target-zero-True
const auto source=source_typed();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},true};
bool returned=false;try {
const auto result=filter_log_rework(source,integer_value(std::string("",0),std::int64_t{0LL}),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(8));
expected.traces.push_back(source.traces.at(9));
expected.traces.push_back(source.traces.at(10));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
{ // 88 typed-target-zero-False
const auto source=source_typed();const auto before=serialize_embedding_workbench_raw_log_json(source);
LogReworkFilterOptions options{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::int64_t{2LL},false};
bool returned=false;try {
const auto result=filter_log_rework(source,integer_value(std::string("",0),std::int64_t{0LL}),options);returned=true;
auto expected=source;expected.traces.clear();
expected.traces.push_back(source.traces.at(0));
expected.traces.push_back(source.traces.at(1));
expected.traces.push_back(source.traces.at(2));
expected.traces.push_back(source.traces.at(3));
expected.traces.push_back(source.traces.at(4));
expected.traces.push_back(source.traces.at(5));
expected.traces.push_back(source.traces.at(6));
expected.traces.push_back(source.traces.at(7));
assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(expected));
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
}
void native_state_acceptance(){
 auto source=source_fixture();source.traces={source.traces.at(5)};
 source.metadata.compatibility_snapshot_valid=true;
 source.metadata.compatibility_globals_snapshot={{"raw",{string_value("raw",std::string("\xff\0",2))}}};
 auto& trace=source.traces.front();trace.events.front().input_order=42;
 for(auto& e:trace.events){e.attributes.clear();e.activity_attribute.reset();e.activity="";e.activity_state=FirstClassAttributeState::kPresent;e.activity_source_key="exact:key";}
 LogReworkFilterOptions options;options.activity_key="exact:key";
 auto result=filter_log_rework(source,"",options);
 assert(serialize_embedding_workbench_raw_log_json(result)==serialize_embedding_workbench_raw_log_json(source));
 assert(result.metadata.compatibility_snapshot_valid);
 assert(result.metadata.compatibility_globals_snapshot==source.metadata.compatibility_globals_snapshot);
 assert(result.traces.front().events.front().input_order==42);
 result.traces.front().events.front().activity="changed";assert(source.traces.front().events.front().activity.empty());
 // Present null first-class data and attribute-only numeric values stay typed.
 for(auto& e:source.traces.front().events)e.activity_state=FirstClassAttributeState::kNull;
 assert(filter_log_rework(source,null_value(""),options).trace_count()==1);
 assert(filter_log_rework(source,"",options).trace_count()==0);
}
void native_numeric_acceptance(){
 const auto matches=[](const EventAttribute& value,const EventAttribute& target){
   EventLog source;Trace trace;Event event;event.attributes={value};
   trace.events={event,event};source.traces={trace};
   return filter_log_rework(source,target).trace_count()==1;
 };
 EventAttribute lexical{"concept:name","1.25",AttributeKind::kFloat};
 assert(matches(lexical,float_value("",1.25)));
 lexical.value=" +1.25 ";assert(matches(lexical,float_value("",1.25)));
 lexical.value="4.9406564584124654e-324";
 assert(matches(lexical,float_value("",std::numeric_limits<double>::denorm_min())));
 assert(!matches(lexical,integer_value("",0)));
 lexical.value="INF";assert(matches(lexical,float_value("",std::numeric_limits<double>::infinity())));
 lexical.value="-Infinity";assert(matches(lexical,float_value("",-std::numeric_limits<double>::infinity())));
 lexical.value="NaN";assert(!matches(lexical,float_value("",std::numeric_limits<double>::quiet_NaN())));
 lexical.value="-0.0";assert(matches(lexical,boolean_value("",false)));
 assert(!matches(integer_value("concept:name",9007199254740993LL),float_value("",0x1p53)));
 assert(matches(integer_value("concept:name",9007199254740992LL),float_value("",0x1p53)));
 EventAttribute wide{"concept:name","18446744073709551615",AttributeKind::kInt};
 wide.typed_value=std::numeric_limits<std::uint64_t>::max();
 assert(!matches(wide,float_value("",0x1p64)));
 wide.typed_value=std::uint64_t{1}<<63;
 assert(matches(wide,float_value("",0x1p63)));
 assert(matches(integer_value("concept:name",std::numeric_limits<std::int64_t>::min()),float_value("",-0x1p63)));
 auto absent=source_fixture();absent.traces={absent.traces.at(8)};
 LogReworkFilterOptions options;options.min_occurrences=std::monostate{};
 assert(filter_log_rework(absent,"A",options).trace_count()==0);
 options.positive=false;assert(filter_log_rework(absent,"A",options).trace_count()==1);
}
} // namespace
int main(){log_rework_filter_module_acceptance();native_state_acceptance();native_numeric_acceptance();std::cout<<"88 captured rework cases and native state checks passed\n";}
