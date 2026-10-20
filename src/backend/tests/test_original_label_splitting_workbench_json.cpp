#include "minera/original_label_splitting_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/exact_filter_discovery_json.hpp"
#include "minera/json_export.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <set>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

using namespace minera;
namespace {
const auto q=serialize_embedding_workbench_string_json;
void has(const std::string& text,const std::string& fragment,
         std::source_location where=std::source_location::current()) {
  if (text.find(fragment)==std::string::npos) {
    std::cerr<<"Missing fragment at line "<<where.line()<<": "<<fragment.substr(0,1024)
        <<"\nActual: "<<text.substr(0,2048)<<'\n';
    assert(false);
  }
}
template<class Make> void rejects(Make make) {
  bool caught=false;
  try { make(); } catch (const std::invalid_argument&) { caught=true; }
  assert(caught);
}
std::string odd() { return std::string("k\0\xff\xed\xa0\x80",6); }
std::string replace_scalar(std::string text,const std::string& name,const std::string& value) {
  const auto key=text.find(q(name)+':');assert(key!=std::string::npos);
  const auto start=key+name.size()+3;
  const auto end=text.find_first_of(",}",start);
  text.replace(start,end-start,value);
  return text;
}
// Independent extraction of the JSON string that holds an actual native JSON
// artifact. The artifact itself is scalar UTF8 JSON (unsafe leaves are carriers).
std::string artifact(const std::string& body,const std::string& name) {
  const auto marker=q(name)+":{\"bytes\":";
  auto position=body.find(marker);assert(position!=std::string::npos);
  position+=marker.size();assert(body[position++]=='"');std::string result;
  while (position<body.size()) {
    char c=body[position++];if(c=='"') return result;
    if(c!='\\') { result+=c;continue; }
    c=body[position++];
    switch(c) {
      case '"':case '\\':case '/':result+=c;break;
      case 'b':result+='\b';break;case 'f':result+='\f';break;
      case 'n':result+='\n';break;case 'r':result+='\r';break;case 't':result+='\t';break;
      case 'u': {
        unsigned code=0;
        for(int i=0;i<4;++i) {
          const char digit=body[position++];
          code=code*16+(digit>='0'&&digit<='9'?digit-'0':digit>='a'&&digit<='f'?digit-'a'+10:digit-'A'+10);
        }
        if(code<0x80)result+=static_cast<char>(code);
        else if(code<0x800) {result+=static_cast<char>(0xc0|(code>>6));result+=static_cast<char>(0x80|(code&63));}
        else {result+=static_cast<char>(0xe0|(code>>12));result+=static_cast<char>(0x80|((code>>6)&63));result+=static_cast<char>(0x80|(code&63));}
        break;
      }
      default:assert(false);
    }
  }
  assert(false);return {};
}
EventLog fixture(const std::string& target=" A ") {
  EventLog log;
  for(std::size_t i=0;i<2;++i) {
    Trace trace;trace.case_id="case"+std::to_string(i);
    trace.events={Event{trace.case_id,i?"X":"L","","",i*3},
        Event{trace.case_id,target,"","",i*3+1},Event{trace.case_id,i?"Y":"R","","",i*3+2}};
    EventAttribute previous{"split","7",AttributeKind::kInt};previous.typed_value=std::int64_t{7};
    trace.events[1].attributes={previous,{"untouched","literal"}};
    log.traces.push_back(trace);
  }
  return log;
}
LabelSplittingOptions explicit_options(const std::string& target=" A ") {
  LabelSplittingOptions result;result.target_key="split";result.activities_suffix="";
  result.prefix_length=1;result.suffix_length=1;result.min_edge_weight=2.0;
  result.target_activities={target};return result;
}
std::vector<DiscoveryWorkbenchPart> split(const EventLog& log,std::string operation="split_activity_labels") {
  return {{"operation","",std::move(operation),false},
      {"source_file","original.json",serialize_embedding_workbench_raw_log_json(log),true}};
}
void with_options(std::vector<DiscoveryWorkbenchPart>& parts,const LabelSplittingOptions& options) {
  parts.push_back({"label_splitting_options_json","",serialize_original_label_splitting_options_json(options),false});
}
void complete_option_domain_and_structural_errors() {
  auto options=explicit_options();options.variant=static_cast<LabelSplittingVariant>(-2147483647);
  options.activity_key=odd();options.target_key="";options.activities_suffix=odd();
  options.prefix_length=std::numeric_limits<std::size_t>::max();options.suffix_length=0;
  options.min_edge_weight=-0.0;options.target_activities={"",odd()," A ",odd()};
  const auto text=serialize_original_label_splitting_options_json(options);
  const auto restored=parse_original_label_splitting_options_json(text);
  assert(serialize_original_label_splitting_options_json(restored)==text);
  assert(std::signbit(restored.min_edge_weight));assert(restored.prefix_length==options.prefix_length);
  assert(restored.target_activities==options.target_activities);
  for(const auto token:{"5e-324","1.7976931348623157e308","-2.0","\"NaN\"","\"INF\"","\"-INF\""}) {
    const auto value=parse_original_label_splitting_options_json(replace_scalar(text,"min_edge_weight",token));
    assert(value.activity_key==odd());
    if(std::string(token)=="5e-324")assert(value.min_edge_weight==std::numeric_limits<double>::denorm_min());
    if(std::string(token)=="1.7976931348623157e308")assert(value.min_edge_weight==std::numeric_limits<double>::max());
  }
  auto ordinary=serialize_original_label_splitting_options_json(LabelSplittingOptions{});
  for(const auto token:{"\"0000\"","\"4097\""})
    (void)parse_original_label_splitting_options_json(replace_scalar(ordinary,"prefix_length",token));
  auto enum_string=ordinary;
  const auto enum_start=enum_string.find("{\"name\":\"contextual\",\"index\":0}");assert(enum_start!=std::string::npos);
  enum_string.replace(enum_start,std::string("{\"name\":\"contextual\",\"index\":0}").size(),"\"-0003\"");
  assert(static_cast<int>(parse_original_label_splitting_options_json(enum_string).variant)==-3);
  for (const auto token : {"-2147483648","2147483647","\"-0000\""}) {
    auto raw=ordinary;const auto begin=raw.find("{\"name\":\"contextual\",\"index\":0}");
    raw.replace(begin,std::string("{\"name\":\"contextual\",\"index\":0}").size(),token);
    (void)parse_original_label_splitting_options_json(raw);
  }
  for (const auto token : {"2147483648","-2147483649","1.0","1e0","\"+1\""}) {
    auto raw=ordinary;const auto begin=raw.find("{\"name\":\"contextual\",\"index\":0}");
    raw.replace(begin,std::string("{\"name\":\"contextual\",\"index\":0}").size(),token);
    rejects([&]{(void)parse_original_label_splitting_options_json(raw);});
  }
  const auto raw_key=ordinary.find("\"activity_key\":\"activity\"");assert(raw_key!=std::string::npos);
  ordinary.replace(raw_key,std::string("\"activity_key\":\"activity\"").size(),std::string("\"activity_key\":\"")+"\xed\xa0\x80"+'"');
  assert(parse_original_label_splitting_options_json(ordinary).activity_key==std::string("\xed\xa0\x80",3));
  rejects([]{(void)parse_original_label_splitting_options_json("{}");});
  for(const auto field:{"\"activity_key\":\"duplicate\",","\"activity_\\u006bey\":\"duplicate\",","\"unknown\":0,"}) {
    auto wrong=text;wrong.insert(1,field);rejects([&]{(void)parse_original_label_splitting_options_json(wrong);});
  }
  const auto normal=serialize_original_label_splitting_options_json(LabelSplittingOptions{});
  for(const auto token:{"-0","\"+1\"","\"-1\"","1.0","1e0","18446744073709551616","true","null"})
    rejects([&]{(void)parse_original_label_splitting_options_json(replace_scalar(normal,"prefix_length",token));});
  for(const auto token:{"true","null","\"unknown\"","1e999"})
    rejects([&]{(void)parse_original_label_splitting_options_json(replace_scalar(normal,"min_edge_weight",token));});
  auto mismatched=normal;mismatched.replace(mismatched.find("\"contextual\""),12,"null");
  rejects([&]{(void)parse_original_label_splitting_options_json(mismatched);});
}
void both_public_functions_and_actual_artifact_handoffs() {
  const auto log=fixture();const auto options=explicit_options();
  const auto direct=split_activity_labels(log,options);
  std::set<std::string> replacements;
  for(const auto& trace:direct.log.traces) {
    assert(trace.events[1].activity==" A ");assert(trace.events[1].attribute("split"));
    replacements.insert(*trace.events[1].attribute("split"));
    assert(trace.events[1].attributes[0].kind==AttributeKind::kString);
    assert(trace.events[1].attributes[0].typed_value.index()==0);
    assert(*trace.events[1].attribute("untouched")=="literal");
  }
  assert((replacements==std::set<std::string>{" A 0"," A 1"}));
  for(const auto* operation:{"split_activity_labels","contextual_label_splitting"}) {
    auto parts=split(log,operation);with_options(parts,options);
    auto response=execute_original_label_splitting_workbench(parts);assert(response.status==200);
    has(response.json,"\"function\":"+q(operation));has(response.json,"\"argument_form\":\"explicit_options\"");
    has(response.json,"\"result\":{\"type\":\"LabelSplittingResult\",\"value\":"+serialize_original_label_splitting_result_json(direct));
    has(response.json,"\"source_before\":"+exact_source_snapshot(EventDataView{log}));
    has(response.json,"\"source\":true");has(response.json,"\"options\":true");
    assert(artifact(response.json,"legacy_result_json")==label_splitting_json(direct));
    const auto bare=artifact(response.json,"result_log_json");
    assert(bare==serialize_embedding_workbench_raw_log_json(direct.log));
    auto handed=parts;handed[0].bytes="contextual_label_splitting";handed[1]={"source_file","handed.json",bare,true};
    const auto next=execute_original_label_splitting_workbench(handed);assert(next.status==200);
    has(next.json,"\"source_before\":"+exact_source_snapshot(EventDataView{direct.log}));
    const auto full=artifact(response.json,"canonical_result_json");
    const auto saved=parse_original_label_splitting_result_json(full);
    assert(serialize_original_label_splitting_result_json(saved)==full);
    const auto serialized=execute_original_label_splitting_workbench({{"operation","","serialize_result",false},{"result_file","result.json",full,true}});
    assert(serialized.status==200);has(serialized.json,"\"input_result\":true");
    assert(artifact(serialized.json,"legacy_result_json")==label_splitting_json(direct));
    assert(artifact(serialized.json,"result_log_json")==bare);
    const auto defaults=execute_original_label_splitting_workbench(split(log,operation));assert(defaults.status==200);
    has(defaults.json,"\"argument_form\":\"native_default\"");
    has(defaults.json,"\"mode\":\"native_default\"");
    assert(artifact(defaults.json,"canonical_result_json")==serialize_original_label_splitting_result_json(split_activity_labels(log)));
  }
  auto empty=split(fixture(""));with_options(empty,explicit_options(""));
  auto response=execute_original_label_splitting_workbench(empty);assert(response.status==200);
  const auto result=parse_original_label_splitting_result_json(artifact(response.json,"canonical_result_json"));
  replacements.clear();for(const auto& trace:result.log.traces)replacements.insert(*trace.events[1].attribute("split"));
  assert((replacements==std::set<std::string>{"0","1"}));
}
void all_stored_log_state_and_optional_legacy_failures() {
  auto log=fixture(odd());
  log.metadata.compatibility_snapshot_valid=true;log.metadata.compatibility_globals_snapshot={{odd(),{}}};
  log.metadata.compatibility_trace_globals_snapshot={{odd(),odd()}};log.metadata.compatibility_event_globals_snapshot={{odd(),odd()}};
  log.metadata.extensions={{odd(),odd(),odd()}};log.metadata.classifiers={{odd(),{odd(),odd()}}};
  log.metadata.trace_globals={{odd(),odd()}};log.metadata.event_globals={{odd(),odd()}};
  log.metadata.properties={{odd(),odd()}};log.metadata.globals={{odd(),{}}};
  EventAttribute raw;raw.key=odd();raw.value=odd();raw.kind=static_cast<AttributeKind>(-1);
  raw.container=static_cast<AttributeContainerKind>(-2);raw.is_null=true;raw.scalar_wrapper=true;raw.scalar_children_list=true;
  raw.first_class_role=static_cast<FirstClassAttributeRole>(-3);raw.first_class_state=static_cast<FirstClassAttributeState>(-4);
  raw.first_class_source_key=odd();raw.typed_value=NativeDate{odd(),static_cast<NativeDateResolution>(-7),odd()};
  for(int i=0;i<7;++i) {
    EventAttribute child;child.key="duplicate";child.value=odd();
    switch(i) {
      case 0:break;case 1:child.typed_value=std::numeric_limits<std::int64_t>::min();break;
      case 2:child.typed_value=std::numeric_limits<double>::quiet_NaN();break;
      case 3:child.typed_value=true;break;case 4:child.typed_value=NativeDate{odd(),NativeDateResolution::kNanosecond,std::string("")};break;
      case 5:child.typed_value=std::numeric_limits<std::uint64_t>::max();break;case 6:child.typed_value=NativeNA{};break;
    }
    raw.children.push_back(child);
  }
  log.attributes={raw,raw};
  auto& trace=log.traces[0];trace.case_id_source_key=odd();trace.case_id_state=static_cast<FirstClassAttributeState>(-9);
  trace.case_id_attribute=raw;trace.attributes={raw};trace.properties={raw};
  auto& event=trace.events[0];event.input_order=std::numeric_limits<std::size_t>::max();event.case_id=odd();event.timestamp=odd();event.resource=odd();event.lifecycle=odd();
  event.case_id_source_key=odd();event.activity_source_key=odd();event.timestamp_source_key=odd();event.resource_source_key=odd();event.lifecycle_source_key=odd();
  event.case_id_state=FirstClassAttributeState::kNull;event.activity_state=FirstClassAttributeState::kPresent;
  event.timestamp_state=static_cast<FirstClassAttributeState>(-10);event.resource_state=FirstClassAttributeState::kNull;event.lifecycle_state=FirstClassAttributeState::kPresent;
  event.activity_attribute=raw;event.timestamp_attribute=raw;event.resource_attribute=raw;event.lifecycle_attribute=raw;
  EventAttribute nested;for(int i=0;i<80;++i){EventAttribute parent;parent.children={nested};nested=parent;}event.attributes={nested,raw};
  const auto options=explicit_options(odd());auto parts=split(log);with_options(parts,options);
  const auto response=execute_original_label_splitting_workbench(parts);assert(response.status==200);
  has(response.json,"\"entered\":true,\"returned\":true");has(response.json,"\"source_before\":"+exact_source_snapshot(EventDataView{log}));
  has(response.json,"\"legacy_result_json\":{\"bytes\":null,\"error\":");
  const auto expected=split_activity_labels(log,options);
  assert(artifact(response.json,"canonical_result_json")==serialize_original_label_splitting_result_json(expected));
  assert(artifact(response.json,"result_log_json")==serialize_embedding_workbench_raw_log_json(expected.log));
  LabelSplittingResult supplied;supplied.variant=static_cast<LabelSplittingVariant>(-5);supplied.log=log;
  const auto raw_result=serialize_original_label_splitting_result_json(supplied);
  assert(serialize_original_label_splitting_result_json(parse_original_label_splitting_result_json(raw_result))==raw_result);
  const auto failed=execute_original_label_splitting_workbench({{"operation","","serialize_result",false},{"result_file","",raw_result,true}});
  assert(failed.status==400);has(failed.json,"\"entered\":true,\"returned\":false");has(failed.json,"\"input_result\":true");
  assert(artifact(failed.json,"canonical_result_json")==raw_result);
  assert(artifact(failed.json,"result_log_json")==serialize_embedding_workbench_raw_log_json(log));
  supplied.log=fixture();const auto plain=serialize_original_label_splitting_result_json(supplied);
  const auto success=execute_original_label_splitting_workbench({{"operation","","serialize_result",false},{"result_file","result.json",plain,true}});
  assert(success.status==200);has(success.json,"\"name\":null,\"index\":-5");
  assert(artifact(success.json,"legacy_result_json")==label_splitting_json(supplied));
  has(artifact(success.json,"legacy_result_json"),"\"variant\":\"contextual\"");
  // Raw nonUnicode data can survive the primary kernel while legacy bytes are
  // independently diagnosed, rather than silently replacement-decoded.
  auto byte_log=fixture(odd());parts=split(byte_log);with_options(parts,explicit_options(odd()));
  const auto byte_response=execute_original_label_splitting_workbench(parts);assert(byte_response.status==200);
  has(byte_response.json,"\"legacy_transport\":\"non_scalar_utf8\"");
}
void native_validation_precedence_and_transport_errors() {
  const auto original=split(fixture());
  const auto native_failure=[&](const LabelSplittingOptions& options,const std::string& expected) {
    auto parts=original;with_options(parts,options);const auto response=execute_original_label_splitting_workbench(parts);
    assert(response.status==400);has(response.json,"\"phase\":\"native\"");has(response.json,"\"entered\":true,\"returned\":false");
    has(response.json,expected);has(response.json,"\"source\":true");has(response.json,"\"options\":true");
    assert(artifact(response.json,"canonical_options_json")==serialize_original_label_splitting_options_json(options));
  };
  auto options=explicit_options();options.variant=static_cast<LabelSplittingVariant>(-1);options.activity_key="";options.min_edge_weight=std::numeric_limits<double>::infinity();options.prefix_length=4097;options.target_activities={"x","x"};
  native_failure(options,"label_splitting: unknown variant");options.variant=LabelSplittingVariant::CONTEXTUAL;
  native_failure(options,"label_splitting: keys must not be empty");options.activity_key="activity";
  native_failure(options,"label_splitting: min_edge_weight must be finite");options.min_edge_weight=2.0;
  native_failure(options,"label_splitting: context lengths are too large");options.prefix_length=1;
  native_failure(options,"label_splitting: duplicate target activity");
  for(double weight:{-2.0,std::numeric_limits<double>::max()}) {
    options=explicit_options();options.min_edge_weight=weight;auto parts=original;with_options(parts,options);
    assert(execute_original_label_splitting_workbench(parts).status==200);
  }
  const auto fail=[&](auto parts,const std::string& expected) {
    auto response=execute_original_label_splitting_workbench(parts);assert(response.status==400);
    has(response.json,expected);has(response.json,"\"entered\":false");return response;
  };
  auto parts=original;parts[1].file=false;parts[1].filename="";
  auto response=fail(parts,"requires actual filename presence");
  has(response.json,"\"source\":{\"filename\":null,\"bytes\":"+q(parts[1].bytes));
  parts=original;parts[1].filename="";assert(execute_original_label_splitting_workbench(parts).status==200);
  parts=original;parts.push_back(parts[1]);response=fail(parts,"duplicate original label splitting control");has(response.json,"\"source\":null");
  parts=original;parts.push_back({"label_splitting_options_json","","",false});parts.push_back({"label_splitting_options_file","","",true});
  fail(parts,"conflict by presence");
  parts=original;parts.push_back({"unknown","",odd(),false});response=fail(parts,"unknown original label splitting control");
  has(response.json,"\"operation\":\"split_activity_labels\"");has(response.json,"\"bytes\":"+q(odd()));
  response=execute_original_label_splitting_workbench(original,{{"operation","","split_activity_labels",false}});
  assert(response.status==400);has(response.json,"\"operation\":null");
  response=execute_original_label_splitting_workbench({{"operation","","split_activity_labels",false}},{{"source_file","original.json",original[1].bytes,true}});
  assert(response.status==400);has(response.json,"File role cannot be a query control");
  parts=original;parts[1].bytes="not JSON";with_options(parts,explicit_options());response=fail(parts,"Exact filter/discovery JSON:");
  has(response.json,"\"options_before\":{");has(response.json,"\"phase\":\"source_parse\"");
  parts=original;parts.push_back({"label_splitting_options_json","inline-bytes.txt","{}",true});response=fail(parts,"missing field");
  has(response.json,"\"options_before\":null");has(response.json,"\"filename\":\"inline-bytes.txt\"");
  response=execute_original_label_splitting_workbench(original,{},"provided_order","owned transport failure");
  assert(response.status==400);has(response.json,"\"phase\":\"transport\"");has(response.json,"\"operation\":\"split_activity_labels\"");
  for(const auto* name:{"canonical_source_json","canonical_options_json","canonical_result_json","legacy_result_json","result_log_json"})has(response.json,q(name)+":{\"bytes\":null,\"error\":null}");
}
} // namespace
int main() {
  complete_option_domain_and_structural_errors();
  both_public_functions_and_actual_artifact_handoffs();
  all_stored_log_state_and_optional_legacy_failures();
  native_validation_precedence_and_transport_errors();
}
