#include "minera/log_trace_filter.hpp"
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
    {std::string("A\0B",3)}, {std::string("\xce\xa9",2)}, {"A","A"}, {"B","A"}, {"X","A"}};
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
struct CapturedCase {
  std::vector<std::vector<std::string>> patterns;
  bool positive;
  std::string activity_key;
  std::vector<std::size_t> selected;
  int error_kind; // 0 return, 1 missing key, 2 non-string value
  int source_variant = 0; // 0 original, 1 zero traces, 2 two empty traces
  int parameter_mode = 0; // 1 omitted, 2 None/default, 3 empty dictionary/default
};
void log_trace_filter_module_acceptance() {
  // All 50 literal outcomes from actual public trace_filter.apply calls.
  const std::vector<CapturedCase> cases = {
    {{},true,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17},false}, // 1 none
    {{},false,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{},false}, // 2 none
    {{{}},true,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{0,1},false}, // 3 empty-pattern
    {{{}},false,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17},false}, // 4 empty-pattern
    {{{std::string("\x2e\x2e\x2e", 3)}},true,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17},false}, // 5 wildcard-only
    {{{std::string("\x2e\x2e\x2e", 3)}},false,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{},false}, // 6 wildcard-only
    {{{std::string("\x41", 1)}},true,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{2},false}, // 7 exact-A
    {{{std::string("\x41", 1)}},false,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{0,1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17},false}, // 8 exact-A
    {{{std::string("\x41", 1),std::string("\x42", 1)}},true,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{3,7,11},false}, // 9 exact-AB
    {{{std::string("\x41", 1),std::string("\x42", 1)}},false,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{0,1,2,4,5,6,8,9,10,12,13,14,15,16,17},false}, // 10 exact-AB
    {{{std::string("\x41", 1),std::string("\x2e\x2e\x2e", 3)}},true,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{2,3,5,7,11,15},false}, // 11 prefix
    {{{std::string("\x41", 1),std::string("\x2e\x2e\x2e", 3)}},false,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{0,1,4,6,8,9,10,12,13,14,16,17},false}, // 12 prefix
    {{{std::string("\x2e\x2e\x2e", 3),std::string("\x42", 1)}},true,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{3,5,7,11},false}, // 13 suffix
    {{{std::string("\x2e\x2e\x2e", 3),std::string("\x42", 1)}},false,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{0,1,2,4,6,8,9,10,12,13,14,15,16,17},false}, // 14 suffix
    {{{std::string("\x2e\x2e\x2e", 3),std::string("\x41", 1),std::string("\x42", 1),std::string("\x2e\x2e\x2e", 3)}},true,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{3,4,7,11},false}, // 15 infix
    {{{std::string("\x2e\x2e\x2e", 3),std::string("\x41", 1),std::string("\x42", 1),std::string("\x2e\x2e\x2e", 3)}},false,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{0,1,2,5,6,8,9,10,12,13,14,15,16,17},false}, // 16 infix
    {{{std::string("\x41", 1),std::string("\x2e\x2e\x2e", 3),std::string("\x42", 1)}},true,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{3,5,7,11},false}, // 17 middle
    {{{std::string("\x41", 1),std::string("\x2e\x2e\x2e", 3),std::string("\x42", 1)}},false,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{0,1,2,4,6,8,9,10,12,13,14,15,16,17},false}, // 18 middle
    {{{std::string("\x2e\x2e\x2e", 3),std::string("\x2e\x2e\x2e", 3)}},true,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{},false}, // 19 repeated-wildcards
    {{{std::string("\x2e\x2e\x2e", 3),std::string("\x2e\x2e\x2e", 3)}},false,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17},false}, // 20 repeated-wildcards
    {{{std::string("", 0)}},true,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{0,1},false}, // 21 empty-label
    {{{std::string("", 0)}},false,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17},false}, // 22 empty-label
    {{{std::string("\x41\x2c\x42", 3)}},true,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{3,7,11},false}, // 23 comma-label
    {{{std::string("\x41\x2c\x42", 3)}},false,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{0,1,2,4,5,6,8,9,10,12,13,14,15,16,17},false}, // 24 comma-label
    {{{std::string("\x20\x41\x20", 3)}},true,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{8},false}, // 25 significant-spaces
    {{{std::string("\x20\x41\x20", 3)}},false,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{0,1,2,3,4,5,6,7,9,10,11,12,13,14,15,16,17},false}, // 26 significant-spaces
    {{{std::string("\x61\x2e\x62", 3)}},true,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{9},false}, // 27 dot-label
    {{{std::string("\x61\x2e\x62", 3)}},false,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{0,1,2,3,4,5,6,7,8,10,11,12,13,14,15,16,17},false}, // 28 dot-label
    {{{std::string("\x41\x0a\x42", 3)}},true,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{10},false}, // 29 newline-label
    {{{std::string("\x41\x0a\x42", 3)}},false,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{0,1,2,3,4,5,6,7,8,9,11,12,13,14,15,16,17},false}, // 30 newline-label
    {{{std::string("\x41\x00\x42", 3)}},true,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{13},false}, // 31 NUL-label
    {{{std::string("\x41\x00\x42", 3)}},false,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{0,1,2,3,4,5,6,7,8,9,10,11,12,14,15,16,17},false}, // 32 NUL-label
    {{{std::string("\xce\xa9", 2)},{std::string("\x61\x2b\x62", 3)}},true,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{12,14},false}, // 33 union-literals
    {{{std::string("\xce\xa9", 2)},{std::string("\x61\x2b\x62", 3)}},false,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{0,1,2,3,4,5,6,7,8,9,10,11,13,15,16,17},false}, // 34 union-literals
    {{{std::string("\x43\x3a\x41", 3)}},true,std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79", 15),{2},false}, // 35 custom-exact
    {{{std::string("\x43\x3a\x41", 3)}},false,std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79", 15),{0,1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17},false}, // 36 custom-exact
    {{{std::string("\x2e\x2e\x2e", 3),std::string("\x43\x3a\x41", 3),std::string("\x2e\x2e\x2e", 3)}},true,std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79", 15),{2,3,4,5,7,11,15,16,17},false}, // 37 custom-infix
    {{{std::string("\x2e\x2e\x2e", 3),std::string("\x43\x3a\x41", 3),std::string("\x2e\x2e\x2e", 3)}},false,std::string("\x63\x75\x73\x74\x6f\x6d\x3a\x61\x63\x74\x69\x76\x69\x74\x79", 15),{0,1,6,8,9,10,12,13,14},false}, // 38 custom-infix
    {{{std::string("\x41", 1)}},true,std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79", 10),{},true}, // 39 missing-key
    {{{std::string("\x41", 1)}},false,std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79", 10),{},true}, // 40 missing-key
    {{{std::string("\x41", 1)}},true,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{2},0,0,1}, // supplement default-omitted
    {{{std::string("\x41", 1)}},true,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{2},0,0,2}, // supplement default-None
    {{{std::string("\x41", 1)}},true,std::string("\x63\x6f\x6e\x63\x65\x70\x74\x3a\x6e\x61\x6d\x65", 12),{2},0,0,3}, // supplement default-empty-dict
    {{},true,std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79", 10),{},1,0,0}, // supplement no-admissions-missing-positive
    {{},false,std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79", 10),{},1,0,0}, // supplement no-admissions-missing-negative
    {{{std::string("\x41", 1)}},true,std::string("\x63\x6f\x75\x6e\x74", 5),{},2,0,0}, // supplement numeric-selected-key
    {{{std::string("\x41", 1)}},true,std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79", 10),{},0,1,0}, // supplement zero-traces-missing-positive
    {{{std::string("\x41", 1)}},false,std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79", 10),{},0,1,0}, // supplement zero-traces-missing-negative
    {{{std::string("\x41", 1)}},true,std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79", 10),{},0,2,0}, // supplement empty-traces-missing-positive
    {{{std::string("\x41", 1)}},false,std::string("\x61\x62\x73\x65\x6e\x74\x3a\x6b\x65\x79", 10),{0,1},0,2,0}, // supplement empty-traces-missing-negative
  };
  const auto original = fixture();
  for (const auto& captured : cases) {
    auto source = original;
    if (captured.source_variant) source.traces.clear();
    if (captured.source_variant == 2) {
      for (int i = 0; i < 2; ++i) {
        Trace trace; trace.case_id = "empty-"+std::to_string(i);
        trace.case_id_state=FirstClassAttributeState::kPresent;
        trace.case_id_source_key="concept:name";
        trace.case_id_attribute=string_value("concept:name",trace.case_id);
        trace.attributes={string_value("concept:name",trace.case_id),integer_value("trace_int",i)};
        trace.properties={string_value("trace_property","empty"+std::to_string(i))};
        source.traces.push_back(trace);
      }
    }
    const auto before = serialize_embedding_workbench_raw_log_json(source);
    const auto patterns_before = captured.patterns;
    LogTraceFilterOptions options{captured.positive, captured.activity_key};
    if (captured.error_kind == 1) {
      bool rejected = false;
      try { (void)filter_log_traces(source, captured.patterns, options); }
      catch (const std::out_of_range& e) {
        rejected = true; assert(std::string(e.what()).find("absent:key") != std::string::npos);
      }
      assert(rejected);
    } else if (captured.error_kind == 2) {
      bool rejected=false;
      try { (void)filter_log_traces(source,captured.patterns,options); }
      catch(const std::invalid_argument& e) {
        // pm4py joins the values: the TypeError names the item, not the key (round BV).
        rejected=true; assert(std::string(e.what()).find("expected str instance, int found")!=std::string::npos);
      }
      assert(rejected);
    } else {
      const auto actual = captured.parameter_mode == 1
          ? filter_log_traces(source,captured.patterns)
          : captured.parameter_mode == 2 || captured.parameter_mode == 3
          ? filter_log_traces(source,captured.patterns,{})
          : filter_log_traces(source, captured.patterns, options);
      EventLog expected=source; expected.traces.clear();
      for (auto index : captured.selected) expected.traces.push_back(source.traces.at(index));
      // Full native source/selected state comparison, not only case IDs.
      assert(serialize_embedding_workbench_raw_log_json(actual) ==
             serialize_embedding_workbench_raw_log_json(expected));
    }
    assert(serialize_embedding_workbench_raw_log_json(source) == before);
    assert(captured.patterns == patterns_before && options.positive == captured.positive &&
           options.activity_key == captured.activity_key);
  }
}
void native_stored_state_and_long_literal() {
  auto source = fixture();
  source.metadata.compatibility_snapshot_valid = true;
  source.metadata.compatibility_globals_snapshot = {{"custom",{string_value("shadow","old")}}};
  source.metadata.compatibility_trace_globals_snapshot = {string_value("trace_shadow","old")};
  source.metadata.compatibility_event_globals_snapshot = {string_value("event_shadow","old")};
  auto a = make_bytes_attribute("bytes",std::string("\0\xff",2));
  source.traces.at(2).events.at(0).attributes.push_back(a);
  source.traces.at(2).events.at(0).attributes.push_back(make_native_na_attribute("NA"));
  const auto before = serialize_embedding_workbench_raw_log_json(source);
  const auto actual = filter_log_traces(source, {{"A"}});
  EventLog expected=source; expected.traces={source.traces.at(2)};
  assert(serialize_embedding_workbench_raw_log_json(actual)==serialize_embedding_workbench_raw_log_json(expected));
  assert(serialize_embedding_workbench_raw_log_json(source)==before);
  const std::string literal(150000,'x');
  Event event; event.activity=literal; event.activity_state=FirstClassAttributeState::kPresent;
  EventLog large; Trace trace; trace.events={event}; large.traces={trace};
  assert(filter_log_traces(large,{{"...",literal,"..."}}).trace_count()==1);
  bool rejected=false;
  try { (void)filter_log_traces(source,{{"A"}}, {true,"count"}); }
  catch(const std::invalid_argument&) {rejected=true;}
  assert(rejected);
}
}  // namespace
int main() {
  log_trace_filter_module_acceptance();
  native_stored_state_and_long_literal();
}
