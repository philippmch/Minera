#include "minera/log_segments_filter.hpp"
#include "minera/embedding_workbench_json.hpp"

#include <cassert>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using namespace minera;
EventAttribute string_value(std::string key, std::string value) {
  return {std::move(key), std::move(value), AttributeKind::kString};
}
EventAttribute integer_value(std::string key, std::int64_t value) {
  EventAttribute a{std::move(key), std::to_string(value), AttributeKind::kInt};
  a.typed_value = value; return a;
}
EventAttribute boolean_value(std::string key, bool value) {
  EventAttribute a{std::move(key), value ? "true" : "false", AttributeKind::kBoolean};
  a.typed_value = value; return a;
}
EventLog fixture() {
  const std::vector<std::vector<std::string>> variants = {
    {}, {""}, {"A"}, {"A","B"}, {"X","A","B","Y"}, {"A","X","B"},
    {"..."}, {"A,B"}, {" A "}, {"a.b"}, {"A\nB"}, {"A","B\n"}, {"a+b"},
    {std::string("A\0B",3)}, {std::string("\xce\xa9",2)}, {"A","A"}, {"B","A"}, {"X","A"}, {"X","A","B","A","Y"}, {"A","X","A"}};
  EventLog result;
  result.attributes = {string_value("log_attribute","original"), integer_value("number",9007199254740993LL)};
  result.metadata.extensions = {{"Concept","concept","urn:concept"}};
  result.metadata.classifiers = {{"Activity",{"concept:name"}}};
  result.metadata.trace_globals = {string_value("global_trace","t")};
  result.metadata.event_globals = {string_value("global_event","e")};
  result.metadata.globals = {{"trace",result.metadata.trace_globals},{"event",result.metadata.event_globals}};
  result.metadata.properties = {string_value("original_property","keep")};
  for (std::size_t i = 0; i < variants.size(); ++i) {
    Trace trace; trace.case_id = "case-" + std::to_string(i);
    trace.case_id_state = FirstClassAttributeState::kPresent;
    trace.case_id_source_key = "concept:name";
    trace.case_id_attribute = string_value("concept:name",trace.case_id);
    trace.attributes = {string_value("concept:name",trace.case_id), integer_value("trace_int",i),boolean_value("trace_flag",false)};
    trace.properties = {string_value("trace_property","p"+std::to_string(i))};
    for (std::size_t j = 0; j < variants[i].size(); ++j) {
      Event event; event.case_id = trace.case_id;
      event.case_id_state = FirstClassAttributeState::kPresent;
      event.activity = variants[i][j]; event.activity_state = FirstClassAttributeState::kPresent;
      event.activity_source_key = "concept:name";
      event.activity_attribute = string_value("concept:name",event.activity);
      event.timestamp = "2026-01-02T03:04:0"+std::to_string(j)+"+00:00";
      event.timestamp_state = FirstClassAttributeState::kPresent;
      event.timestamp_source_key = "time:timestamp";
      event.resource = " r "; event.resource_state = FirstClassAttributeState::kPresent;
      event.resource_source_key = "org:resource";
      event.input_order = j;
      EventAttribute cost{"cost","1.25",AttributeKind::kFloat}; cost.typed_value = 1.25;
      EventAttribute null{"nullable","",AttributeKind::kString}; null.is_null = true;
      EventAttribute date{"time:timestamp",event.timestamp,AttributeKind::kDate};
      date.typed_value = NativeDate{event.timestamp}; event.timestamp_attribute = date;
      event.resource_attribute = string_value("org:resource",event.resource);
      event.attributes = {string_value("concept:name",event.activity),string_value("custom:activity","C:"+event.activity),
          integer_value("count",9007199254740993LL+j),cost,boolean_value("flag",true),null,date,
          string_value("org:resource",event.resource)};
      trace.events.push_back(event);
    }
    result.traces.push_back(trace);
  }
  return result;
}
EventLog mixed_fixture() {
  auto source=fixture();source.traces.clear();
  const std::vector<std::vector<int>> values={{-1,1,2,1},{2,-1,1,-1,1},{3,1,-1,1},{1,4,2,-1,1}};
  for(std::size_t i=0;i<values.size();++i) {
    Trace trace;trace.case_id="mixed-"+std::to_string(i);
    trace.attributes={string_value("concept:name",trace.case_id),integer_value("trace_int",i)};
    trace.properties={string_value("trace_property","mixed"+std::to_string(i))};
    for(std::size_t j=0;j<values[i].size();++j) {
      Event event;event.activity="position-"+std::to_string(j);
      event.activity_state=FirstClassAttributeState::kPresent;event.activity_source_key="concept:name";
      event.activity_attribute=string_value("concept:name",event.activity);
      event.attributes={string_value("concept:name",event.activity),integer_value("count",j)};
      const auto value=values[i][j];
      if(value==1 || value==2)event.attributes.push_back(string_value("mixed:key",value==1?"A":"X"));
      if(value==3)event.attributes.push_back(integer_value("mixed:key",17));
      if(value==4)event.attributes.push_back(boolean_value("mixed:key",false));
      trace.events.push_back(event);
    }
    source.traces.push_back(trace);
  }
  return source;
}
struct CapturedCase {
  std::string activity, key;
  bool strict, last;
  std::vector<std::pair<std::size_t,std::vector<std::size_t>>> cuts;
  int source_variant;
  bool default_parameters;
};
EventLog source_for(int variant) {
  auto source=fixture();
  if (variant) source.traces.clear();
  if (variant==2) for(int i=0;i<2;++i) {
    Trace t; t.case_id="empty-"+std::to_string(i);
    t.case_id_state=FirstClassAttributeState::kPresent;t.case_id_source_key="concept:name";
    t.case_id_attribute=string_value("concept:name",t.case_id);
    t.attributes={string_value("concept:name",t.case_id),integer_value("trace_int",i)};
    t.properties={string_value("trace_property","empty"+std::to_string(i))};source.traces.push_back(t);
  }
  return source;
}
EventLog expected_for(const EventLog& source,const CapturedCase& captured) {
  auto expected=source;expected.traces.clear();
  for (const auto& [index,events] : captured.cuts) {
    auto cut=source.traces.at(index);cut.events.clear();
    for(auto event:events) cut.events.push_back(source.traces.at(index).events.at(event));
    expected.traces.push_back(cut);
  }
  return expected;
}
void log_prefix_filter_module_acceptance() {
  const std::vector<CapturedCase> cases = {
    {std::string("\x41", 1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{{2,{}},{3,{}},{4,{0}},{5,{}},{11,{}},{15,{}},{16,{0}},{17,{0}},{18,{0}},{19,{}}},0,true}, // 1 default-omitted
    {std::string("\x41", 1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{{2,{}},{3,{}},{4,{0}},{5,{}},{11,{}},{15,{}},{16,{0}},{17,{0}},{18,{0}},{19,{}}},0,true}, // 2 default-None
    {std::string("\x41", 1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{{2,{}},{3,{}},{4,{0}},{5,{}},{11,{}},{15,{}},{16,{0}},{17,{0}},{18,{0}},{19,{}}},0,true}, // 3 default-empty-dict
    {std::string("\x41", 1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{{2,{}},{3,{}},{4,{0}},{5,{}},{11,{}},{15,{}},{16,{0}},{17,{0}},{18,{0}},{19,{}}},0,false}, // 4 anchor
    {std::string("\x41", 1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,true,{{2,{}},{3,{}},{4,{0}},{5,{}},{11,{}},{15,{0}},{16,{0}},{17,{0}},{18,{0,1,2}},{19,{0,1}}},0,false}, // 5 anchor
    {std::string("\x41", 1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),false,false,{{2,{0}},{3,{0}},{4,{0,1}},{5,{0}},{11,{0}},{15,{0}},{16,{0,1}},{17,{0,1}},{18,{0,1}},{19,{0}}},0,false}, // 6 anchor
    {std::string("\x41", 1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),false,true,{{2,{0}},{3,{0}},{4,{0,1}},{5,{0}},{11,{0}},{15,{0,1}},{16,{0,1}},{17,{0,1}},{18,{0,1,2,3}},{19,{0,1,2}}},0,false}, // 7 anchor
    {std::string("", 0),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{{1,{}}},0,false}, // 8 empty-anchor
    {std::string("", 0),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,true,{{1,{}}},0,false}, // 9 empty-anchor
    {std::string("", 0),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),false,false,{{1,{0}}},0,false}, // 10 empty-anchor
    {std::string("", 0),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),false,true,{{1,{0}}},0,false}, // 11 empty-anchor
    {std::string("\x43\x3a\x41", 3),std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79", 15),true,false,{{2,{}},{3,{}},{4,{0}},{5,{}},{11,{}},{15,{}},{16,{0}},{17,{0}},{18,{0}},{19,{}}},0,false}, // 12 custom-anchor
    {std::string("\x43\x3a\x41", 3),std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79", 15),true,true,{{2,{}},{3,{}},{4,{0}},{5,{}},{11,{}},{15,{0}},{16,{0}},{17,{0}},{18,{0,1,2}},{19,{0,1}}},0,false}, // 13 custom-anchor
    {std::string("\x43\x3a\x41", 3),std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79", 15),false,false,{{2,{0}},{3,{0}},{4,{0,1}},{5,{0}},{11,{0}},{15,{0}},{16,{0,1}},{17,{0,1}},{18,{0,1}},{19,{0}}},0,false}, // 14 custom-anchor
    {std::string("\x43\x3a\x41", 3),std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79", 15),false,true,{{2,{0}},{3,{0}},{4,{0,1}},{5,{0}},{11,{0}},{15,{0,1}},{16,{0,1}},{17,{0,1}},{18,{0,1,2,3}},{19,{0,1,2}}},0,false}, // 15 custom-anchor
    {std::string("\x41", 1),std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79", 10),true,false,{},0,false}, // 16 missing-key
    {std::string("\x39\x30\x30\x37\x31\x39\x39\x32\x35\x34\x37\x34\x30\x39\x39\x33", 16),std::string("\x63\x6f\x75\x6e\x74", 5),true,false,{},0,false}, // 17 numeric-key-string-target
    {std::string("\x20\x41\x20", 3),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{{8,{}}},0,false}, // 18 spaces-anchor
    {std::string("\x41\x42\x53\x45\x4e\x54", 6),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{},0,false}, // 19 absent-anchor
    {std::string("\x41\x00\x42", 3),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{{13,{}}},0,false}, // 20 NUL-anchor
    {std::string("\x41\x2c\x42", 3),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{{7,{}}},0,false}, // 21 comma-anchor
    {std::string("\xce\xa9", 2),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{{14,{}}},0,false}, // 22 unicode-anchor
    {std::string("\x41", 1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{},1,false}, // 23 zero-log-default
    {std::string("\x41", 1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{},2,false}, // 24 empty-traces-default
    {std::string("\x41", 1),std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79", 10),true,false,{},1,false}, // 25 zero-log-missing-key
    {std::string("\x41", 1),std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79", 10),true,false,{},2,false}, // 26 empty-traces-missing-key
  };
  for (const auto& captured:cases) {
    const auto source=source_for(captured.source_variant);
    const auto before=serialize_embedding_workbench_raw_log_json(source);
    const LogSegmentFilterOptions options{captured.key,captured.strict,
      captured.last?LogSegmentOccurrence::last:LogSegmentOccurrence::first};
    const auto actual=captured.default_parameters ? filter_log_prefix(source,captured.activity) : filter_log_prefix(source,captured.activity,options);
    assert(serialize_embedding_workbench_raw_log_json(actual)==
           serialize_embedding_workbench_raw_log_json(expected_for(source,captured)));
    assert(serialize_embedding_workbench_raw_log_json(source)==before);
    assert(options.activity_key==captured.key && options.strict==captured.strict);
  }
  const std::vector<CapturedCase> mixed_cases = {
    {"A","mixed:key",true,false,{{0,{0}},{1,{0,1}},{2,{0}},{3,{}}},0,false},
    {"A","mixed:key",true,true,{{0,{0,1,2}},{1,{0,1,2,3}},{2,{0,1,2}},{3,{0,1,2,3}}},0,false},
    {"A","mixed:key",false,false,{{0,{0,1}},{1,{0,1,2}},{2,{0,1}},{3,{0}}},0,false},
    {"A","mixed:key",false,true,{{0,{0,1,2,3}},{1,{0,1,2,3,4}},{2,{0,1,2,3}},{3,{0,1,2,3,4}}},0,false},
  };
  for(const auto& captured:mixed_cases) {
    const auto source=mixed_fixture();
    const auto before=serialize_embedding_workbench_raw_log_json(source);
    const LogSegmentFilterOptions options{captured.key,captured.strict,
      captured.last?LogSegmentOccurrence::last:LogSegmentOccurrence::first};
    const auto actual=filter_log_prefix(source,captured.activity,options);
    assert(serialize_embedding_workbench_raw_log_json(actual)==
           serialize_embedding_workbench_raw_log_json(expected_for(source,captured)));
    assert(serialize_embedding_workbench_raw_log_json(source)==before);
  }

}
void log_suffix_filter_module_acceptance() {
  const std::vector<CapturedCase> cases = {
    {std::string("\x41", 1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{{2,{}},{3,{1}},{4,{2,3}},{5,{1,2}},{11,{1}},{15,{1}},{16,{}},{17,{}},{18,{2,3,4}},{19,{1,2}}},0,true}, // 27 default-omitted
    {std::string("\x41", 1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{{2,{}},{3,{1}},{4,{2,3}},{5,{1,2}},{11,{1}},{15,{1}},{16,{}},{17,{}},{18,{2,3,4}},{19,{1,2}}},0,true}, // 28 default-None
    {std::string("\x41", 1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{{2,{}},{3,{1}},{4,{2,3}},{5,{1,2}},{11,{1}},{15,{1}},{16,{}},{17,{}},{18,{2,3,4}},{19,{1,2}}},0,true}, // 29 default-empty-dict
    {std::string("\x41", 1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{{2,{}},{3,{1}},{4,{2,3}},{5,{1,2}},{11,{1}},{15,{1}},{16,{}},{17,{}},{18,{2,3,4}},{19,{1,2}}},0,false}, // 30 anchor
    {std::string("\x41", 1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,true,{{2,{}},{3,{1}},{4,{2,3}},{5,{1,2}},{11,{1}},{15,{}},{16,{}},{17,{}},{18,{4}},{19,{}}},0,false}, // 31 anchor
    {std::string("\x41", 1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),false,false,{{2,{0}},{3,{0,1}},{4,{1,2,3}},{5,{0,1,2}},{11,{0,1}},{15,{0,1}},{16,{1}},{17,{1}},{18,{1,2,3,4}},{19,{0,1,2}}},0,false}, // 32 anchor
    {std::string("\x41", 1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),false,true,{{2,{0}},{3,{0,1}},{4,{1,2,3}},{5,{0,1,2}},{11,{0,1}},{15,{1}},{16,{1}},{17,{1}},{18,{3,4}},{19,{2}}},0,false}, // 33 anchor
    {std::string("", 0),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{{1,{}}},0,false}, // 34 empty-anchor
    {std::string("", 0),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,true,{{1,{}}},0,false}, // 35 empty-anchor
    {std::string("", 0),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),false,false,{{1,{0}}},0,false}, // 36 empty-anchor
    {std::string("", 0),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),false,true,{{1,{0}}},0,false}, // 37 empty-anchor
    {std::string("\x43\x3a\x41", 3),std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79", 15),true,false,{{2,{}},{3,{1}},{4,{2,3}},{5,{1,2}},{11,{1}},{15,{1}},{16,{}},{17,{}},{18,{2,3,4}},{19,{1,2}}},0,false}, // 38 custom-anchor
    {std::string("\x43\x3a\x41", 3),std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79", 15),true,true,{{2,{}},{3,{1}},{4,{2,3}},{5,{1,2}},{11,{1}},{15,{}},{16,{}},{17,{}},{18,{4}},{19,{}}},0,false}, // 39 custom-anchor
    {std::string("\x43\x3a\x41", 3),std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79", 15),false,false,{{2,{0}},{3,{0,1}},{4,{1,2,3}},{5,{0,1,2}},{11,{0,1}},{15,{0,1}},{16,{1}},{17,{1}},{18,{1,2,3,4}},{19,{0,1,2}}},0,false}, // 40 custom-anchor
    {std::string("\x43\x3a\x41", 3),std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79", 15),false,true,{{2,{0}},{3,{0,1}},{4,{1,2,3}},{5,{0,1,2}},{11,{0,1}},{15,{1}},{16,{1}},{17,{1}},{18,{3,4}},{19,{2}}},0,false}, // 41 custom-anchor
    {std::string("\x41", 1),std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79", 10),true,false,{},0,false}, // 42 missing-key
    {std::string("\x39\x30\x30\x37\x31\x39\x39\x32\x35\x34\x37\x34\x30\x39\x39\x33", 16),std::string("\x63\x6f\x75\x6e\x74", 5),true,false,{},0,false}, // 43 numeric-key-string-target
    {std::string("\x20\x41\x20", 3),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{{8,{}}},0,false}, // 44 spaces-anchor
    {std::string("\x41\x42\x53\x45\x4e\x54", 6),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{},0,false}, // 45 absent-anchor
    {std::string("\x41\x00\x42", 3),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{{13,{}}},0,false}, // 46 NUL-anchor
    {std::string("\x41\x2c\x42", 3),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{{7,{}}},0,false}, // 47 comma-anchor
    {std::string("\xce\xa9", 2),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{{14,{}}},0,false}, // 48 unicode-anchor
    {std::string("\x41", 1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{},1,false}, // 49 zero-log-default
    {std::string("\x41", 1),std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),true,false,{},2,false}, // 50 empty-traces-default
    {std::string("\x41", 1),std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79", 10),true,false,{},1,false}, // 51 zero-log-missing-key
    {std::string("\x41", 1),std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79", 10),true,false,{},2,false}, // 52 empty-traces-missing-key
  };
  for (const auto& captured:cases) {
    const auto source=source_for(captured.source_variant);
    const auto before=serialize_embedding_workbench_raw_log_json(source);
    const LogSegmentFilterOptions options{captured.key,captured.strict,
      captured.last?LogSegmentOccurrence::last:LogSegmentOccurrence::first};
    const auto actual=captured.default_parameters ? filter_log_suffix(source,captured.activity) : filter_log_suffix(source,captured.activity,options);
    assert(serialize_embedding_workbench_raw_log_json(actual)==
           serialize_embedding_workbench_raw_log_json(expected_for(source,captured)));
    assert(serialize_embedding_workbench_raw_log_json(source)==before);
    assert(options.activity_key==captured.key && options.strict==captured.strict);
  }
  const std::vector<CapturedCase> mixed_cases = {
    {"A","mixed:key",true,false,{{0,{2,3}},{1,{3,4}},{2,{2,3}},{3,{1,2,3,4}}},0,false},
    {"A","mixed:key",true,true,{{0,{}},{1,{}},{2,{}},{3,{}}},0,false},
    {"A","mixed:key",false,false,{{0,{1,2,3}},{1,{2,3,4}},{2,{1,2,3}},{3,{0,1,2,3,4}}},0,false},
    {"A","mixed:key",false,true,{{0,{3}},{1,{4}},{2,{3}},{3,{4}}},0,false},
  };
  for(const auto& captured:mixed_cases) {
    const auto source=mixed_fixture();
    const auto before=serialize_embedding_workbench_raw_log_json(source);
    const LogSegmentFilterOptions options{captured.key,captured.strict,
      captured.last?LogSegmentOccurrence::last:LogSegmentOccurrence::first};
    const auto actual=filter_log_suffix(source,captured.activity,options);
    assert(serialize_embedding_workbench_raw_log_json(actual)==
           serialize_embedding_workbench_raw_log_json(expected_for(source,captured)));
    assert(serialize_embedding_workbench_raw_log_json(source)==before);
  }

}
void log_segment_native_state_acceptance() {
  auto source=fixture();
  source.metadata.compatibility_snapshot_valid=true;
  source.metadata.compatibility_globals_snapshot={{"inactive",{string_value("raw",std::string("\xff\0",2))}}};
  source.metadata.compatibility_trace_globals_snapshot={string_value("stored","trace")};
  source.metadata.compatibility_event_globals_snapshot={string_value("stored","event")};
  source.traces.resize(1);
  Trace& trace=source.traces.front();
  for(const auto& value:{std::string("X"),std::string("\xed\xa0\x80\0\xff",5),std::string("Y")}) {
    Event event;event.activity=value;event.activity_state=FirstClassAttributeState::kPresent;
    event.activity_source_key=std::string("\xff\0",2);
    event.activity_attribute=string_value(event.activity_source_key,value);
    event.input_order=123;
    trace.events.push_back(event);
  }
  const auto before=serialize_embedding_workbench_raw_log_json(source);
  const LogSegmentFilterOptions options{std::string("\xff\0",2),false,LogSegmentOccurrence::first};
  auto prefix_expected=source;prefix_expected.traces.front().events.resize(2);
  auto suffix_expected=source;suffix_expected.traces.front().events.erase(suffix_expected.traces.front().events.begin());
  assert(serialize_embedding_workbench_raw_log_json(filter_log_prefix(source,trace.events[1].activity,options))==
         serialize_embedding_workbench_raw_log_json(prefix_expected));
  assert(serialize_embedding_workbench_raw_log_json(filter_log_suffix(source,trace.events[1].activity,options))==
         serialize_embedding_workbench_raw_log_json(suffix_expected));
  assert(serialize_embedding_workbench_raw_log_json(source)==before);
}
}  // namespace
int main() {
  log_prefix_filter_module_acceptance();
  log_suffix_filter_module_acceptance();
  log_segment_native_state_acceptance();
}
