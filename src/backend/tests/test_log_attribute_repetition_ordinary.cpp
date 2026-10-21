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
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x73\x2d\x42\x32",25)},EventAttribute{std::string("\x6c\x69\x74\x65\x72\x61\x6c",7),std::string("\x20\xce\xa9\x0a\x00\x20",6)}};
log.metadata.properties={make_bytes_attribute(std::string("\x6f\x70\x61\x71\x75\x65\x20\x62\x79\x74\x65\x73",12),std::string("\x00\xff",2))};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x30",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x30",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x31",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x31",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x5a",1)}});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x32",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x32",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)}});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x42",1)}});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x33",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x33",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)}});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x41",1)}});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_1(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x73\x2d\x42\x32",25)},EventAttribute{std::string("\x6c\x69\x74\x65\x72\x61\x6c",7),std::string("\x20\xce\xa9\x0a\x00\x20",6)}};
log.metadata.properties={make_bytes_attribute(std::string("\x6f\x70\x61\x71\x75\x65\x20\x62\x79\x74\x65\x73",12),std::string("\x00\xff",2))};
return log;}
EventLog snapshot_2(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x73\x2d\x42\x32",25)},EventAttribute{std::string("\x6c\x69\x74\x65\x72\x61\x6c",7),std::string("\x20\xce\xa9\x0a\x00\x20",6)}};
log.metadata.properties={make_bytes_attribute(std::string("\x6f\x70\x61\x71\x75\x65\x20\x62\x79\x74\x65\x73",12),std::string("\x00\xff",2))};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x30",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x30",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_3(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x73\x2d\x42\x32",25)},EventAttribute{std::string("\x6c\x69\x74\x65\x72\x61\x6c",7),std::string("\x20\xce\xa9\x0a\x00\x20",6)}};
log.metadata.properties={make_bytes_attribute(std::string("\x6f\x70\x61\x71\x75\x65\x20\x62\x79\x74\x65\x73",12),std::string("\x00\xff",2))};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x30",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x30",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_4(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x73\x2d\x42\x32",25)},EventAttribute{std::string("\x6c\x69\x74\x65\x72\x61\x6c",7),std::string("\x20\xce\xa9\x0a\x00\x20",6)}};
log.metadata.properties={make_bytes_attribute(std::string("\x6f\x70\x61\x71\x75\x65\x20\x62\x79\x74\x65\x73",12),std::string("\x00\xff",2))};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x30",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x30",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},make_bytes_attribute(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x41",1))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},make_bytes_attribute(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x41",1))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x31",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x31",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x41",1)}});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x41",1)}});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x32",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x32",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},make_bytes_attribute(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("",0))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},make_bytes_attribute(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("",0))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x33",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x33",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("",0)}});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("",0)}});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x34",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x34",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},make_bytes_attribute(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x00\x41\xff",3))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},make_bytes_attribute(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x00\x41\xff",3))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x35",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x35",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},make_bytes_attribute(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\xce\xa9",2))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},make_bytes_attribute(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\xce\xa9",2))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x36",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x36",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\xce\xa9",2)}});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\xce\xa9",2)}});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x37",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x37",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},make_bytes_attribute(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x6e\x61\x6e",3))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},make_bytes_attribute(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x6e\x61\x6e",3))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x38",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x38",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::numeric_limits<double>::quiet_NaN())});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},float_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::numeric_limits<double>::quiet_NaN())});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_5(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x73\x2d\x42\x32",25)},EventAttribute{std::string("\x6c\x69\x74\x65\x72\x61\x6c",7),std::string("\x20\xce\xa9\x0a\x00\x20",6)}};
log.metadata.properties={make_bytes_attribute(std::string("\x6f\x70\x61\x71\x75\x65\x20\x62\x79\x74\x65\x73",12),std::string("\x00\xff",2))};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x30",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x30",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},make_bytes_attribute(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x41",1))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},make_bytes_attribute(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x41",1))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_6(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x73\x2d\x42\x32",25)},EventAttribute{std::string("\x6c\x69\x74\x65\x72\x61\x6c",7),std::string("\x20\xce\xa9\x0a\x00\x20",6)}};
log.metadata.properties={make_bytes_attribute(std::string("\x6f\x70\x61\x71\x75\x65\x20\x62\x79\x74\x65\x73",12),std::string("\x00\xff",2))};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x31",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x31",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x41",1)}});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x41",1)}});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_7(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x73\x2d\x42\x32",25)},EventAttribute{std::string("\x6c\x69\x74\x65\x72\x61\x6c",7),std::string("\x20\xce\xa9\x0a\x00\x20",6)}};
log.metadata.properties={make_bytes_attribute(std::string("\x6f\x70\x61\x71\x75\x65\x20\x62\x79\x74\x65\x73",12),std::string("\x00\xff",2))};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x32",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x32",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},make_bytes_attribute(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("",0))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},make_bytes_attribute(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("",0))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_8(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x73\x2d\x42\x32",25)},EventAttribute{std::string("\x6c\x69\x74\x65\x72\x61\x6c",7),std::string("\x20\xce\xa9\x0a\x00\x20",6)}};
log.metadata.properties={make_bytes_attribute(std::string("\x6f\x70\x61\x71\x75\x65\x20\x62\x79\x74\x65\x73",12),std::string("\x00\xff",2))};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x33",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x33",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("",0)}});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("",0)}});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_9(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x73\x2d\x42\x32",25)},EventAttribute{std::string("\x6c\x69\x74\x65\x72\x61\x6c",7),std::string("\x20\xce\xa9\x0a\x00\x20",6)}};
log.metadata.properties={make_bytes_attribute(std::string("\x6f\x70\x61\x71\x75\x65\x20\x62\x79\x74\x65\x73",12),std::string("\x00\xff",2))};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x34",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x34",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},make_bytes_attribute(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x00\x41\xff",3))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},make_bytes_attribute(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x00\x41\xff",3))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_10(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x73\x2d\x42\x32",25)},EventAttribute{std::string("\x6c\x69\x74\x65\x72\x61\x6c",7),std::string("\x20\xce\xa9\x0a\x00\x20",6)}};
log.metadata.properties={make_bytes_attribute(std::string("\x6f\x70\x61\x71\x75\x65\x20\x62\x79\x74\x65\x73",12),std::string("\x00\xff",2))};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x35",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x35",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},make_bytes_attribute(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\xce\xa9",2))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},make_bytes_attribute(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\xce\xa9",2))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_11(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x73\x2d\x42\x32",25)},EventAttribute{std::string("\x6c\x69\x74\x65\x72\x61\x6c",7),std::string("\x20\xce\xa9\x0a\x00\x20",6)}};
log.metadata.properties={make_bytes_attribute(std::string("\x6f\x70\x61\x71\x75\x65\x20\x62\x79\x74\x65\x73",12),std::string("\x00\xff",2))};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x36",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x36",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\xce\xa9",2)}});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},EventAttribute{std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\xce\xa9",2)}});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_12(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x73\x2d\x42\x32",25)},EventAttribute{std::string("\x6c\x69\x74\x65\x72\x61\x6c",7),std::string("\x20\xce\xa9\x0a\x00\x20",6)}};
log.metadata.properties={make_bytes_attribute(std::string("\x6f\x70\x61\x71\x75\x65\x20\x62\x79\x74\x65\x73",12),std::string("\x00\xff",2))};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x37",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x37",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},make_bytes_attribute(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x6e\x61\x6e",3))});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",10)},make_bytes_attribute(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),std::string("\x6e\x61\x6e",3))});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_13(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x73\x2d\x42\x32",25)},EventAttribute{std::string("\x6c\x69\x74\x65\x72\x61\x6c",7),std::string("\x20\xce\xa9\x0a\x00\x20",6)}};
log.metadata.properties={make_bytes_attribute(std::string("\x6f\x70\x61\x71\x75\x65\x20\x62\x79\x74\x65\x73",12),std::string("\x00\xff",2))};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x30",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x30",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6e\x65\x73\x74\x65\x64\x20\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",17)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kList,{make_bytes_attribute(std::string("\x30",1),std::string("\x00\x41\xff",3))})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6e\x65\x73\x74\x65\x64\x20\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",17)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kList,{make_bytes_attribute(std::string("\x30",1),std::string("\x00\x41\xff",3))})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x31",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x31",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6e\x65\x73\x74\x65\x64\x20\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",17)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kTuple,{make_bytes_attribute(std::string("\x30",1),std::string("\x00\x41\xff",3))})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6e\x65\x73\x74\x65\x64\x20\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",17)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kTuple,{make_bytes_attribute(std::string("\x30",1),std::string("\x00\x41\xff",3))})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x32",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x32",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6e\x65\x73\x74\x65\x64\x20\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",17)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kContainer,{make_bytes_attribute(std::string("\x78",1),std::string("\x00\x41\xff",3))})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6e\x65\x73\x74\x65\x64\x20\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",17)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kContainer,{make_bytes_attribute(std::string("\x78",1),std::string("\x00\x41\xff",3))})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x33",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x33",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6e\x65\x73\x74\x65\x64\x20\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",17)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kSet,{make_bytes_attribute(std::string("\x30",1),std::string("\x00\x41\xff",3))})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6e\x65\x73\x74\x65\x64\x20\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",17)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kSet,{make_bytes_attribute(std::string("\x30",1),std::string("\x00\x41\xff",3))})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x34",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x34",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6e\x65\x73\x74\x65\x64\x20\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",17)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kFrozenSet,{make_bytes_attribute(std::string("\x30",1),std::string("\x00\x41\xff",3))})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6e\x65\x73\x74\x65\x64\x20\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",17)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kFrozenSet,{make_bytes_attribute(std::string("\x30",1),std::string("\x00\x41\xff",3))})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_14(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x73\x2d\x42\x32",25)},EventAttribute{std::string("\x6c\x69\x74\x65\x72\x61\x6c",7),std::string("\x20\xce\xa9\x0a\x00\x20",6)}};
log.metadata.properties={make_bytes_attribute(std::string("\x6f\x70\x61\x71\x75\x65\x20\x62\x79\x74\x65\x73",12),std::string("\x00\xff",2))};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x30",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x30",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6e\x65\x73\x74\x65\x64\x20\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",17)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kList,{make_bytes_attribute(std::string("\x30",1),std::string("\x00\x41\xff",3))})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6e\x65\x73\x74\x65\x64\x20\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",17)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kList,{make_bytes_attribute(std::string("\x30",1),std::string("\x00\x41\xff",3))})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_15(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x73\x2d\x42\x32",25)},EventAttribute{std::string("\x6c\x69\x74\x65\x72\x61\x6c",7),std::string("\x20\xce\xa9\x0a\x00\x20",6)}};
log.metadata.properties={make_bytes_attribute(std::string("\x6f\x70\x61\x71\x75\x65\x20\x62\x79\x74\x65\x73",12),std::string("\x00\xff",2))};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x31",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x31",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6e\x65\x73\x74\x65\x64\x20\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",17)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kTuple,{make_bytes_attribute(std::string("\x30",1),std::string("\x00\x41\xff",3))})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6e\x65\x73\x74\x65\x64\x20\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",17)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kTuple,{make_bytes_attribute(std::string("\x30",1),std::string("\x00\x41\xff",3))})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_16(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x73\x2d\x42\x32",25)},EventAttribute{std::string("\x6c\x69\x74\x65\x72\x61\x6c",7),std::string("\x20\xce\xa9\x0a\x00\x20",6)}};
log.metadata.properties={make_bytes_attribute(std::string("\x6f\x70\x61\x71\x75\x65\x20\x62\x79\x74\x65\x73",12),std::string("\x00\xff",2))};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x32",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x32",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6e\x65\x73\x74\x65\x64\x20\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",17)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kContainer,{make_bytes_attribute(std::string("\x78",1),std::string("\x00\x41\xff",3))})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6e\x65\x73\x74\x65\x64\x20\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",17)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kContainer,{make_bytes_attribute(std::string("\x78",1),std::string("\x00\x41\xff",3))})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
EventLog snapshot_17(){EventLog log;
log.attributes={EventAttribute{std::string("\x62\x61\x6e\x6b",4),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x73\x2d\x42\x32",25)},EventAttribute{std::string("\x6c\x69\x74\x65\x72\x61\x6c",7),std::string("\x20\xce\xa9\x0a\x00\x20",6)}};
log.metadata.properties={make_bytes_attribute(std::string("\x6f\x70\x61\x71\x75\x65\x20\x62\x79\x74\x65\x73",12),std::string("\x00\xff",2))};
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x33",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x33",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6e\x65\x73\x74\x65\x64\x20\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",17)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kSet,{make_bytes_attribute(std::string("\x30",1),std::string("\x00\x41\xff",3))})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6e\x65\x73\x74\x65\x64\x20\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",17)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kSet,{make_bytes_attribute(std::string("\x30",1),std::string("\x00\x41\xff",3))})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
{Trace trace;trace.attributes={EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x34",19)},boolean_value(std::string("\x72\x65\x74\x61\x69\x6e\x65\x64\x20\x66\x6c\x61\x67",13),false)};trace.properties={EventAttribute{std::string("\x73\x6f\x75\x72\x63\x65",6),std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x20\x6e\x61\x74\x69\x76\x65\x2d\x73\x68\x61\x70\x65\x20\x63\x6f\x6e\x74\x69\x6e\x75\x61\x74\x69\x6f\x6e",34)}};
trace.case_id=std::string("\x6f\x72\x64\x69\x6e\x61\x72\x79\x2d\x73\x68\x61\x70\x65\x2d\x42\x32\x2d\x34",19);trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6e\x65\x73\x74\x65\x64\x20\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",17)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kFrozenSet,{make_bytes_attribute(std::string("\x30",1),std::string("\x00\x41\xff",3))})});event.input_order=0;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
{auto event=event_for({EventAttribute{std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12),std::string("\x6e\x65\x73\x74\x65\x64\x20\x62\x79\x74\x65\x20\x65\x76\x65\x6e\x74",17)},container_value(std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8),AttributeContainerKind::kFrozenSet,{make_bytes_attribute(std::string("\x30",1),std::string("\x00\x41\xff",3))})});event.input_order=1;event.case_id=trace.case_id;event.case_id_state=FirstClassAttributeState::kPresent;trace.events.push_back(std::move(event));}
log.traces.push_back(std::move(trace));}
return log;}
void retained_public_repetition_cases(){std::size_t matched=0;
{ // RLO-B2-lower-list
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=container_value(std::string("",0),AttributeContainerKind::kList,{});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-lower-list");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x6c\x69\x73\x74\x27\x20\x61\x6e\x64\x20\x27\x69\x6e\x74\x27",56))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-upper-list
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=container_value(std::string("",0),AttributeContainerKind::kList,{});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-upper-list");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x6c\x69\x73\x74\x27",56))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-upper-skipped-list
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x39\x39",2));
options.maximum=container_value(std::string("",0),AttributeContainerKind::kList,{});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-upper-skipped-list");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-empty-log-bound-list
const auto source=snapshot_1();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=container_value(std::string("",0),AttributeContainerKind::kList,{});
options.maximum=container_value(std::string("",0),AttributeContainerKind::kList,{});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-empty-log-bound-list");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-empty-trace-upper-list
const auto source=snapshot_2();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=container_value(std::string("",0),AttributeContainerKind::kList,{});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-empty-trace-upper-list");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x6c\x69\x73\x74\x27",56))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-lower-tuple
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=container_value(std::string("",0),AttributeContainerKind::kTuple,{integer_value(std::string("\x30",1),std::int64_t{1LL})});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-lower-tuple");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x74\x75\x70\x6c\x65\x27\x20\x61\x6e\x64\x20\x27\x69\x6e\x74\x27",57))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-upper-tuple
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=container_value(std::string("",0),AttributeContainerKind::kTuple,{integer_value(std::string("\x30",1),std::int64_t{1LL})});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-upper-tuple");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x74\x75\x70\x6c\x65\x27",57))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-upper-skipped-tuple
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x39\x39",2));
options.maximum=container_value(std::string("",0),AttributeContainerKind::kTuple,{integer_value(std::string("\x30",1),std::int64_t{1LL})});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-upper-skipped-tuple");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-empty-log-bound-tuple
const auto source=snapshot_1();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=container_value(std::string("",0),AttributeContainerKind::kTuple,{integer_value(std::string("\x30",1),std::int64_t{1LL})});
options.maximum=container_value(std::string("",0),AttributeContainerKind::kTuple,{integer_value(std::string("\x30",1),std::int64_t{1LL})});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-empty-log-bound-tuple");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-empty-trace-upper-tuple
const auto source=snapshot_2();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=container_value(std::string("",0),AttributeContainerKind::kTuple,{integer_value(std::string("\x30",1),std::int64_t{1LL})});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-empty-trace-upper-tuple");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x74\x75\x70\x6c\x65\x27",57))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-lower-dict
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=container_value(std::string("",0),AttributeContainerKind::kContainer,{integer_value(std::string("\x78",1),std::int64_t{1LL})});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-lower-dict");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x64\x69\x63\x74\x27\x20\x61\x6e\x64\x20\x27\x69\x6e\x74\x27",56))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-upper-dict
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=container_value(std::string("",0),AttributeContainerKind::kContainer,{integer_value(std::string("\x78",1),std::int64_t{1LL})});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-upper-dict");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x64\x69\x63\x74\x27",56))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-upper-skipped-dict
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x39\x39",2));
options.maximum=container_value(std::string("",0),AttributeContainerKind::kContainer,{integer_value(std::string("\x78",1),std::int64_t{1LL})});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-upper-skipped-dict");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-empty-log-bound-dict
const auto source=snapshot_1();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=container_value(std::string("",0),AttributeContainerKind::kContainer,{integer_value(std::string("\x78",1),std::int64_t{1LL})});
options.maximum=container_value(std::string("",0),AttributeContainerKind::kContainer,{integer_value(std::string("\x78",1),std::int64_t{1LL})});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-empty-log-bound-dict");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-empty-trace-upper-dict
const auto source=snapshot_2();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=container_value(std::string("",0),AttributeContainerKind::kContainer,{integer_value(std::string("\x78",1),std::int64_t{1LL})});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-empty-trace-upper-dict");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x64\x69\x63\x74\x27",56))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-lower-set
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=container_value(std::string("",0),AttributeContainerKind::kSet,{integer_value(std::string("\x30",1),std::int64_t{1LL})});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-lower-set");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x73\x65\x74\x27\x20\x61\x6e\x64\x20\x27\x69\x6e\x74\x27",55))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-upper-set
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=container_value(std::string("",0),AttributeContainerKind::kSet,{integer_value(std::string("\x30",1),std::int64_t{1LL})});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-upper-set");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x73\x65\x74\x27",55))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-upper-skipped-set
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x39\x39",2));
options.maximum=container_value(std::string("",0),AttributeContainerKind::kSet,{integer_value(std::string("\x30",1),std::int64_t{1LL})});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-upper-skipped-set");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-empty-log-bound-set
const auto source=snapshot_1();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=container_value(std::string("",0),AttributeContainerKind::kSet,{integer_value(std::string("\x30",1),std::int64_t{1LL})});
options.maximum=container_value(std::string("",0),AttributeContainerKind::kSet,{integer_value(std::string("\x30",1),std::int64_t{1LL})});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-empty-log-bound-set");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-empty-trace-upper-set
const auto source=snapshot_2();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=container_value(std::string("",0),AttributeContainerKind::kSet,{integer_value(std::string("\x30",1),std::int64_t{1LL})});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-empty-trace-upper-set");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x73\x65\x74\x27",55))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-lower-frozenset
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=container_value(std::string("",0),AttributeContainerKind::kFrozenSet,{integer_value(std::string("\x30",1),std::int64_t{1LL})});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-lower-frozenset");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x66\x72\x6f\x7a\x65\x6e\x73\x65\x74\x27\x20\x61\x6e\x64\x20\x27\x69\x6e\x74\x27",61))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-upper-frozenset
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=container_value(std::string("",0),AttributeContainerKind::kFrozenSet,{integer_value(std::string("\x30",1),std::int64_t{1LL})});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-upper-frozenset");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x66\x72\x6f\x7a\x65\x6e\x73\x65\x74\x27",61))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-upper-skipped-frozenset
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x39\x39",2));
options.maximum=container_value(std::string("",0),AttributeContainerKind::kFrozenSet,{integer_value(std::string("\x30",1),std::int64_t{1LL})});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-upper-skipped-frozenset");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-empty-log-bound-frozenset
const auto source=snapshot_1();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=container_value(std::string("",0),AttributeContainerKind::kFrozenSet,{integer_value(std::string("\x30",1),std::int64_t{1LL})});
options.maximum=container_value(std::string("",0),AttributeContainerKind::kFrozenSet,{integer_value(std::string("\x30",1),std::int64_t{1LL})});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-empty-log-bound-frozenset");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-empty-trace-upper-frozenset
const auto source=snapshot_2();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=container_value(std::string("",0),AttributeContainerKind::kFrozenSet,{integer_value(std::string("\x30",1),std::int64_t{1LL})});
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-empty-trace-upper-frozenset");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x66\x72\x6f\x7a\x65\x6e\x73\x65\x74\x27",61))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-lower-aware-date
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=date_value(std::string("",0),std::string("\x32\x30\x32\x36\x2d\x30\x35\x2d\x30\x35\x54\x31\x32\x3a\x31\x33\x3a\x31\x34\x2e\x31\x32\x33\x34\x35\x36\x2b\x30\x30\x3a\x30\x30",32));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-lower-aware-date");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x64\x61\x74\x65\x74\x69\x6d\x65\x2e\x64\x61\x74\x65\x74\x69\x6d\x65\x27\x20\x61\x6e\x64\x20\x27\x69\x6e\x74\x27",69))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-upper-aware-date
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=date_value(std::string("",0),std::string("\x32\x30\x32\x36\x2d\x30\x35\x2d\x30\x35\x54\x31\x32\x3a\x31\x33\x3a\x31\x34\x2e\x31\x32\x33\x34\x35\x36\x2b\x30\x30\x3a\x30\x30",32));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-upper-aware-date");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x64\x61\x74\x65\x74\x69\x6d\x65\x2e\x64\x61\x74\x65\x74\x69\x6d\x65\x27",69))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-upper-skipped-aware-date
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x39\x39",2));
options.maximum=date_value(std::string("",0),std::string("\x32\x30\x32\x36\x2d\x30\x35\x2d\x30\x35\x54\x31\x32\x3a\x31\x33\x3a\x31\x34\x2e\x31\x32\x33\x34\x35\x36\x2b\x30\x30\x3a\x30\x30",32));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-upper-skipped-aware-date");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-empty-log-bound-aware-date
const auto source=snapshot_1();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=date_value(std::string("",0),std::string("\x32\x30\x32\x36\x2d\x30\x35\x2d\x30\x35\x54\x31\x32\x3a\x31\x33\x3a\x31\x34\x2e\x31\x32\x33\x34\x35\x36\x2b\x30\x30\x3a\x30\x30",32));
options.maximum=date_value(std::string("",0),std::string("\x32\x30\x32\x36\x2d\x30\x35\x2d\x30\x35\x54\x31\x32\x3a\x31\x33\x3a\x31\x34\x2e\x31\x32\x33\x34\x35\x36\x2b\x30\x30\x3a\x30\x30",32));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-empty-log-bound-aware-date");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-empty-trace-upper-aware-date
const auto source=snapshot_2();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=date_value(std::string("",0),std::string("\x32\x30\x32\x36\x2d\x30\x35\x2d\x30\x35\x54\x31\x32\x3a\x31\x33\x3a\x31\x34\x2e\x31\x32\x33\x34\x35\x36\x2b\x30\x30\x3a\x30\x30",32));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-empty-trace-upper-aware-date");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x64\x61\x74\x65\x74\x69\x6d\x65\x2e\x64\x61\x74\x65\x74\x69\x6d\x65\x27",69))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-lower-naive-date
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=date_value(std::string("",0),std::string("\x32\x30\x32\x36\x2d\x30\x35\x2d\x30\x35\x54\x31\x32\x3a\x31\x33\x3a\x31\x34\x2e\x31\x32\x33\x34\x35\x36",26));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-lower-naive-date");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x64\x61\x74\x65\x74\x69\x6d\x65\x2e\x64\x61\x74\x65\x74\x69\x6d\x65\x27\x20\x61\x6e\x64\x20\x27\x69\x6e\x74\x27",69))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-upper-naive-date
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=date_value(std::string("",0),std::string("\x32\x30\x32\x36\x2d\x30\x35\x2d\x30\x35\x54\x31\x32\x3a\x31\x33\x3a\x31\x34\x2e\x31\x32\x33\x34\x35\x36",26));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-upper-naive-date");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x64\x61\x74\x65\x74\x69\x6d\x65\x2e\x64\x61\x74\x65\x74\x69\x6d\x65\x27",69))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-upper-skipped-naive-date
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x39\x39",2));
options.maximum=date_value(std::string("",0),std::string("\x32\x30\x32\x36\x2d\x30\x35\x2d\x30\x35\x54\x31\x32\x3a\x31\x33\x3a\x31\x34\x2e\x31\x32\x33\x34\x35\x36",26));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-upper-skipped-naive-date");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-empty-log-bound-naive-date
const auto source=snapshot_1();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=date_value(std::string("",0),std::string("\x32\x30\x32\x36\x2d\x30\x35\x2d\x30\x35\x54\x31\x32\x3a\x31\x33\x3a\x31\x34\x2e\x31\x32\x33\x34\x35\x36",26));
options.maximum=date_value(std::string("",0),std::string("\x32\x30\x32\x36\x2d\x30\x35\x2d\x30\x35\x54\x31\x32\x3a\x31\x33\x3a\x31\x34\x2e\x31\x32\x33\x34\x35\x36",26));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-empty-log-bound-naive-date");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-empty-trace-upper-naive-date
const auto source=snapshot_2();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=date_value(std::string("",0),std::string("\x32\x30\x32\x36\x2d\x30\x35\x2d\x30\x35\x54\x31\x32\x3a\x31\x33\x3a\x31\x34\x2e\x31\x32\x33\x34\x35\x36",26));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-empty-trace-upper-naive-date");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x64\x61\x74\x65\x74\x69\x6d\x65\x2e\x64\x61\x74\x65\x74\x69\x6d\x65\x27",69))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-lower-bytes
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=make_bytes_attribute(std::string("",0),std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-lower-bytes");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x62\x79\x74\x65\x73\x27\x20\x61\x6e\x64\x20\x27\x69\x6e\x74\x27",57))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-upper-bytes
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=make_bytes_attribute(std::string("",0),std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-upper-bytes");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x62\x79\x74\x65\x73\x27",57))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-upper-skipped-bytes
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x39\x39",2));
options.maximum=make_bytes_attribute(std::string("",0),std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-upper-skipped-bytes");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-empty-log-bound-bytes
const auto source=snapshot_1();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=make_bytes_attribute(std::string("",0),std::string("\x32",1));
options.maximum=make_bytes_attribute(std::string("",0),std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-empty-log-bound-bytes");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-empty-trace-upper-bytes
const auto source=snapshot_2();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.minimum=TokenInteger(std::string("\x30",1));
options.maximum=make_bytes_attribute(std::string("",0),std::string("\x32",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-empty-trace-upper-bytes");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x27\x3c\x3d\x27\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64\x20\x62\x65\x74\x77\x65\x65\x6e\x20\x69\x6e\x73\x74\x61\x6e\x63\x65\x73\x20\x6f\x66\x20\x27\x69\x6e\x74\x27\x20\x61\x6e\x64\x20\x27\x62\x79\x74\x65\x73\x27",57))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-list-empty-log
const auto source=snapshot_1();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=container_value(std::string("",0),AttributeContainerKind::kList,{});
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-list-empty-log");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-list-empty-trace
const auto source=snapshot_2();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=container_value(std::string("",0),AttributeContainerKind::kList,{});
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_2();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-list-empty-trace");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-list-empty-event
const auto source=snapshot_3();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=container_value(std::string("",0),AttributeContainerKind::kList,{});
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-selector-list-empty-event");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x75\x6e\x68\x61\x73\x68\x61\x62\x6c\x65\x20\x74\x79\x70\x65\x3a\x20\x27\x6c\x69\x73\x74\x27",23))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-list-ordinary
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=container_value(std::string("",0),AttributeContainerKind::kList,{});
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-selector-list-ordinary");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x75\x6e\x68\x61\x73\x68\x61\x62\x6c\x65\x20\x74\x79\x70\x65\x3a\x20\x27\x6c\x69\x73\x74\x27",23))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-dict-empty-log
const auto source=snapshot_1();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=container_value(std::string("",0),AttributeContainerKind::kContainer,{integer_value(std::string("\x78",1),std::int64_t{1LL})});
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-dict-empty-log");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-dict-empty-trace
const auto source=snapshot_2();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=container_value(std::string("",0),AttributeContainerKind::kContainer,{integer_value(std::string("\x78",1),std::int64_t{1LL})});
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_2();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-dict-empty-trace");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-dict-empty-event
const auto source=snapshot_3();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=container_value(std::string("",0),AttributeContainerKind::kContainer,{integer_value(std::string("\x78",1),std::int64_t{1LL})});
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-selector-dict-empty-event");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x75\x6e\x68\x61\x73\x68\x61\x62\x6c\x65\x20\x74\x79\x70\x65\x3a\x20\x27\x64\x69\x63\x74\x27",23))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-dict-ordinary
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=container_value(std::string("",0),AttributeContainerKind::kContainer,{integer_value(std::string("\x78",1),std::int64_t{1LL})});
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-selector-dict-ordinary");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x75\x6e\x68\x61\x73\x68\x61\x62\x6c\x65\x20\x74\x79\x70\x65\x3a\x20\x27\x64\x69\x63\x74\x27",23))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-set-empty-log
const auto source=snapshot_1();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=container_value(std::string("",0),AttributeContainerKind::kSet,{integer_value(std::string("\x30",1),std::int64_t{1LL})});
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-set-empty-log");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-set-empty-trace
const auto source=snapshot_2();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=container_value(std::string("",0),AttributeContainerKind::kSet,{integer_value(std::string("\x30",1),std::int64_t{1LL})});
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_2();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-set-empty-trace");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-set-empty-event
const auto source=snapshot_3();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=container_value(std::string("",0),AttributeContainerKind::kSet,{integer_value(std::string("\x30",1),std::int64_t{1LL})});
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-selector-set-empty-event");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x75\x6e\x68\x61\x73\x68\x61\x62\x6c\x65\x20\x74\x79\x70\x65\x3a\x20\x27\x73\x65\x74\x27",22))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-set-ordinary
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=container_value(std::string("",0),AttributeContainerKind::kSet,{integer_value(std::string("\x30",1),std::int64_t{1LL})});
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
throw std::runtime_error("Expected captured error for RLO-B2-selector-set-ordinary");
}catch(const std::invalid_argument& error){if(std::string(error.what())!=std::string("\x75\x6e\x68\x61\x73\x68\x61\x62\x6c\x65\x20\x74\x79\x70\x65\x3a\x20\x27\x73\x65\x74\x27",22))throw std::runtime_error(std::string("Wrong error: ")+error.what());}
assert(!returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-tuple-empty-log
const auto source=snapshot_1();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=container_value(std::string("",0),AttributeContainerKind::kTuple,{EventAttribute{std::string("\x30",1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-tuple-empty-log");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-tuple-empty-trace
const auto source=snapshot_2();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=container_value(std::string("",0),AttributeContainerKind::kTuple,{EventAttribute{std::string("\x30",1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_2();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-tuple-empty-trace");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-tuple-empty-event
const auto source=snapshot_3();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=container_value(std::string("",0),AttributeContainerKind::kTuple,{EventAttribute{std::string("\x30",1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_3();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-tuple-empty-event");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-tuple-ordinary
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=container_value(std::string("",0),AttributeContainerKind::kTuple,{EventAttribute{std::string("\x30",1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_0();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-tuple-ordinary");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-frozenset-empty-log
const auto source=snapshot_1();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=container_value(std::string("",0),AttributeContainerKind::kFrozenSet,{EventAttribute{std::string("\x30",1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-frozenset-empty-log");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-frozenset-empty-trace
const auto source=snapshot_2();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=container_value(std::string("",0),AttributeContainerKind::kFrozenSet,{EventAttribute{std::string("\x30",1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_2();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-frozenset-empty-trace");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-frozenset-empty-event
const auto source=snapshot_3();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=container_value(std::string("",0),AttributeContainerKind::kFrozenSet,{EventAttribute{std::string("\x30",1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_3();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-frozenset-empty-event");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-frozenset-ordinary
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=container_value(std::string("",0),AttributeContainerKind::kFrozenSet,{EventAttribute{std::string("\x30",1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12)}});
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_0();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-frozenset-ordinary");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-bytes-empty-log
const auto source=snapshot_1();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=make_bytes_attribute(std::string("",0),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12));
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-bytes-empty-log");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-bytes-empty-trace
const auto source=snapshot_2();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=make_bytes_attribute(std::string("",0),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12));
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_2();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-bytes-empty-trace");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-bytes-empty-event
const auto source=snapshot_3();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=make_bytes_attribute(std::string("",0),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12));
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_3();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-bytes-empty-event");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-bytes-ordinary
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=make_bytes_attribute(std::string("",0),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65",12));
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_0();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-bytes-ordinary");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-date-empty-log
const auto source=snapshot_1();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=date_value(std::string("",0),std::string("\x32\x30\x32\x36\x2d\x30\x35\x2d\x30\x35\x54\x31\x32\x3a\x31\x33\x3a\x31\x34\x2e\x31\x32\x33\x34\x35\x36\x2b\x30\x30\x3a\x30\x30",32));
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-date-empty-log");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-date-empty-trace
const auto source=snapshot_2();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=date_value(std::string("",0),std::string("\x32\x30\x32\x36\x2d\x30\x35\x2d\x30\x35\x54\x31\x32\x3a\x31\x33\x3a\x31\x34\x2e\x31\x32\x33\x34\x35\x36\x2b\x30\x30\x3a\x30\x30",32));
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_2();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-date-empty-trace");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-date-empty-event
const auto source=snapshot_3();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=date_value(std::string("",0),std::string("\x32\x30\x32\x36\x2d\x30\x35\x2d\x30\x35\x54\x31\x32\x3a\x31\x33\x3a\x31\x34\x2e\x31\x32\x33\x34\x35\x36\x2b\x30\x30\x3a\x30\x30",32));
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_3();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-date-empty-event");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-date-ordinary
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=date_value(std::string("",0),std::string("\x32\x30\x32\x36\x2d\x30\x35\x2d\x30\x35\x54\x31\x32\x3a\x31\x33\x3a\x31\x34\x2e\x31\x32\x33\x34\x35\x36\x2b\x30\x30\x3a\x30\x30",32));
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_0();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-date-ordinary");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-native-na-empty-log
const auto source=snapshot_1();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=make_native_na_attribute(std::string("",0));
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-native-na-empty-log");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-native-na-empty-trace
const auto source=snapshot_2();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=make_native_na_attribute(std::string("",0));
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_2();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-native-na-empty-trace");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-native-na-empty-event
const auto source=snapshot_3();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=make_native_na_attribute(std::string("",0));
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_3();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-native-na-empty-event");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-selector-native-na-ordinary
const auto source=snapshot_0();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=make_native_na_attribute(std::string("",0));
options.minimum=TokenInteger(std::string("\x30",1));
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_0();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-selector-native-na-ordinary");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-bytes-target-0
const auto source=snapshot_4();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=make_bytes_attribute(std::string("",0),std::string("\x41",1));
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_5();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-bytes-target-0");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-bytes-target-1
const auto source=snapshot_4();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\x41",1)};
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_6();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-bytes-target-1");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-bytes-target-2
const auto source=snapshot_4();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=make_bytes_attribute(std::string("",0),std::string("",0));
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_7();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-bytes-target-2");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-bytes-target-3
const auto source=snapshot_4();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("",0)};
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_8();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-bytes-target-3");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-bytes-target-4
const auto source=snapshot_4();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=make_bytes_attribute(std::string("",0),std::string("\x00\x41\xff",3));
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_9();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-bytes-target-4");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-bytes-target-5
const auto source=snapshot_4();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=make_bytes_attribute(std::string("",0),std::string("\xce\xa9",2));
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_10();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-bytes-target-5");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-bytes-target-6
const auto source=snapshot_4();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=EventAttribute{std::string("",0),std::string("\xce\xa9",2)};
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_11();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-bytes-target-6");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-bytes-target-7
const auto source=snapshot_4();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=make_bytes_attribute(std::string("",0),std::string("\x6e\x61\x6e",3));
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_12();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-bytes-target-7");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-bytes-target-8
const auto source=snapshot_4();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=float_value(std::string("",0),std::numeric_limits<double>::quiet_NaN());
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_1();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-bytes-target-8");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-nested-bytes-list
const auto source=snapshot_13();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=container_value(std::string("",0),AttributeContainerKind::kList,{make_bytes_attribute(std::string("\x30",1),std::string("\x00\x41\xff",3))});
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_14();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-nested-bytes-list");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-nested-bytes-tuple
const auto source=snapshot_13();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=container_value(std::string("",0),AttributeContainerKind::kTuple,{make_bytes_attribute(std::string("\x30",1),std::string("\x00\x41\xff",3))});
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_15();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-nested-bytes-tuple");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-nested-bytes-dict
const auto source=snapshot_13();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=container_value(std::string("",0),AttributeContainerKind::kContainer,{make_bytes_attribute(std::string("\x78",1),std::string("\x00\x41\xff",3))});
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_16();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-nested-bytes-dict");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-nested-bytes-set
const auto source=snapshot_13();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=container_value(std::string("",0),AttributeContainerKind::kSet,{make_bytes_attribute(std::string("\x30",1),std::string("\x00\x41\xff",3))});
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_17();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-nested-bytes-set");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
{ // RLO-B2-nested-bytes-frozenset
const auto source=snapshot_13();const auto before=serialize_embedding_workbench_raw_log_json(source);
const auto target=container_value(std::string("",0),AttributeContainerKind::kFrozenSet,{make_bytes_attribute(std::string("\x30",1),std::string("\x00\x41\xff",3))});
LogAttributeRepetitionOptions options;
options.attribute_key=std::string("\x73\x65\x6c\x65\x63\x74\x65\x64",8);
bool returned=false;try{const auto result=filter_log_attribute_value_repetition(source,target,options);returned=true;
const auto expected=snapshot_17();
if(serialize_embedding_workbench_raw_log_json(result)!=serialize_embedding_workbench_raw_log_json(expected))throw std::runtime_error("Full returned log mismatch: RLO-B2-nested-bytes-frozenset");
}catch(...){throw;}assert(returned);
assert(serialize_embedding_workbench_raw_log_json(source)==before);++matched;}
assert(matched==86);std::cout<<matched<<"/86 retained ordinary full repetition outcomes/errors passed\n";}
} // namespace
int main(){try{retained_public_repetition_cases();}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}}