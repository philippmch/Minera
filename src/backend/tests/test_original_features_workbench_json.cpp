#include "minera/original_features_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/exact_filter_discovery_json.hpp"
#include "minera/literal_controls_json.hpp"
#include "minera/json_export.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
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
std::string scalar(std::string text,const std::string& name,const std::string& value) {
  const auto key=text.find(q(name)+':');
  assert(key!=std::string::npos);
  const auto start=key+name.size()+3;
  const auto end=text.find_first_of(",}",start);
  text.replace(start,end-start,value);
  return text;
}
EventLog fixture() {
  EventLog log;
  Trace trace;trace.case_id="original";
  trace.events={Event{"original","A","2024-01-01T00:00:00Z","r",0},
                Event{"original","B","2024-01-01T01:00:00Z","r",1}};
  log.traces.push_back(trace);
  return log;
}
std::vector<DiscoveryWorkbenchPart> extract(const EventLog& log) {
  return {{"operation","","extract_features",false},
      {"source_file","source.json",serialize_embedding_workbench_raw_log_json(log),true}};
}
std::vector<DiscoveryWorkbenchPart> serialize(const LogToFeaturesResult& result) {
  return {{"operation","","serialize_result",false},
      {"result_file","result.json",serialize_literal_controls_result_json(result),true}};
}
void all_options_and_exact_storage() {
  LogToFeaturesOptions options;
  options.variant=static_cast<LogToFeaturesVariant>(-2147483647);
  options.activity_key=odd();options.timestamp_key="";options.start_timestamp_key=" start ";
  options.resource_key="a,b";options.window_seconds=-0.0;options.grouper_freq=odd();
  options.event_string_attributes={""," A ",odd(),odd()};
  options.event_numeric_attributes={odd()};options.case_string_attributes={"x"};
  options.case_numeric_attributes={""};options.event_attributes={"z,z"};
  options.case_attributes={"=>"};options.event_successor_attributes={"",""};
  options.feature_names={"",odd(),odd()};options.case_id_key=odd();
  options.arrival_rate=odd();options.finish_rate="";options.service_time=odd();
  options.waiting_time=" wait ";options.sojourn_time="a,b";options.diff_start_end=odd();
  options.default_not_present=std::numeric_limits<double>::infinity();
  options.epsilon=std::numeric_limits<double>::denorm_min();
  options.min_num_different_string_values=0;
  options.max_num_different_string_values=std::numeric_limits<std::size_t>::max();
  options.event_string_attributes_supplied=false;
  options.event_numeric_attributes_supplied=true;options.feature_names_supplied=false;
  options.enable_activity_def_representation=false;options.enable_succ_def_representation=false;
  options.enable_all_extra_features=true;options.enable_case_duration=false;
  options.enable_case_duration_supplied=true;options.enable_times_from_first_occurrence=true;
  options.enable_times_from_first_occurrence_supplied=false;
  options.enable_times_from_last_occurrence=false;options.enable_times_from_last_occurrence_supplied=true;
  options.enable_direct_paths_times_last_occ=true;options.enable_direct_paths_times_last_occ_supplied=false;
  options.enable_indirect_paths_times_last_occ=false;options.enable_indirect_paths_times_last_occ_supplied=true;
  options.enable_work_in_progress=true;options.enable_work_in_progress_supplied=false;
  options.enable_resource_workload=false;options.enable_resource_workload_supplied=true;
  options.enable_first_last_activity_index=true;options.enable_first_last_activity_index_supplied=false;
  options.enable_max_concurrent_events=false;options.enable_max_concurrent_events_supplied=true;
  options.enable_max_concurrent_events_per_activity=true;
  options.enable_max_concurrent_events_per_activity_supplied=false;
  const auto text=serialize_literal_controls_options_json(options);
  const auto parsed=parse_original_features_options_json(text);
  assert(serialize_literal_controls_options_json(parsed)==text);
  assert(std::signbit(parsed.window_seconds));
  assert(parsed.epsilon==std::numeric_limits<double>::denorm_min());
  assert(parsed.event_string_attributes.size()==4 && !parsed.event_string_attributes_supplied);
  for (const auto token : {"-0.0","5e-324","1.7976931348623157e308","\"NaN\"","\"INF\"","\"-INF\""}) {
    const auto value=parse_original_features_options_json(scalar(text,"epsilon",token));
    assert(value.activity_key==odd());
    if (std::string(token)=="-0.0") assert(value.epsilon==0 && std::signbit(value.epsilon));
  }
  auto raw=serialize_literal_controls_options_json(LogToFeaturesOptions{});
  const auto key=raw.find("\"activity_key\":\"concept:name\"");
  raw.replace(key,std::string("\"activity_key\":\"concept:name\"").size(),
      std::string("\"activity_key\":\"")+"\xed\xa0\x80"+"\"");
  assert(parse_original_features_options_json(raw).activity_key==std::string("\xed\xa0\x80",3));
  rejects([&]{(void)parse_original_features_options_json("{}");});
  rejects([&]{(void)parse_original_features_options_json(scalar(text,"epsilon","true"));});
  rejects([&]{(void)parse_original_features_options_json(scalar(text,"epsilon","1e999"));});
  rejects([&]{(void)parse_original_features_options_json(scalar(text,"epsilon","\"Infinity\""));});
  rejects([&]{(void)parse_original_features_options_json(scalar(text,"min_num_different_string_values","-1"));});
  rejects([&]{(void)parse_original_features_options_json(scalar(text,"min_num_different_string_values","18446744073709551616"));});
  auto duplicate=text;duplicate.insert(1,"\"activity_key\":\"duplicate\",");
  rejects([&]{(void)parse_original_features_options_json(duplicate);});
  auto escaped=text;escaped.insert(1,"\"activity_\\u006bey\":\"duplicate\",");
  rejects([&]{(void)parse_original_features_options_json(escaped);});
  auto unknown=text;unknown.insert(1,"\"unknown\":0,");
  rejects([&]{(void)parse_original_features_options_json(unknown);});
}
void complete_result_inverse_and_semantic_ownership() {
  LogToFeaturesResult result;
  result.variant=static_cast<LogToFeaturesVariant>(-3);
  result.feature_names={"",odd(),odd()};result.case_ids={odd(),""};
  result.event_rows={{{-0.0,std::numeric_limits<double>::denorm_min(),
      std::numeric_limits<double>::quiet_NaN()}},{}};
  EventAttribute attr;attr.key=odd();attr.value=odd();attr.kind=AttributeKind::kBytes;
  attr.container=static_cast<AttributeContainerKind>(-2);
  attr.first_class_role=static_cast<FirstClassAttributeRole>(-3);
  attr.first_class_state=static_cast<FirstClassAttributeState>(-4);
  attr.first_class_source_key=odd();attr.scalar_wrapper=true;attr.scalar_children_list=true;
  attr.typed_value=NativeDate{odd(),static_cast<NativeDateResolution>(-1),odd()};
  EventAttribute child;child.key="child";child.value=odd();
  child.typed_value=std::numeric_limits<std::uint64_t>::max();attr.children={child};
  result.trace_rows={{attr,-std::numeric_limits<double>::infinity()},{}};
  result.temporal_rows={{-0.0,{std::string(""),odd(),std::numeric_limits<double>::infinity()},odd()}};
  const auto text=serialize_literal_controls_result_json(result);
  const auto parsed=parse_original_features_result_json(text);
  assert(serialize_literal_controls_result_json(parsed)==text);
  const auto& actual=std::get<EventAttribute>(parsed.trace_rows[0][0]);
  assert(actual.key==odd() && actual.children[0].typed_value.index()==5);
  assert(std::get<NativeDate>(actual.typed_value).timezone==std::optional<std::string>(odd()));
  auto response=execute_original_features_workbench(serialize(result));
  assert(response.status==400);
  has(response.json,"\"entered\":true,\"returned\":false");
  has(response.json,"\"input_result\":true");
  has(response.json,"\"canonical_result_json\":{\"bytes\":"+q(text));
  has(response.json,"\"legacy_result_json\":{\"bytes\":null,\"error\":");
  for (auto variant : {LogToFeaturesVariant::EVENT_BASED,LogToFeaturesVariant::TRACE_BASED,LogToFeaturesVariant::TEMPORAL}) {
    result.variant=variant;
    // Mixed inactive fields remain parseable for every variant; only native exporter validates them.
    (void)parse_original_features_result_json(serialize_literal_controls_result_json(result));
    response=execute_original_features_workbench(serialize(result));
    assert(response.status==400);has(response.json,"\"entered\":true,\"returned\":false");
  }
  LogToFeaturesResult valid;valid.feature_names={"x"};valid.trace_rows={{2.0}};
  const auto legacy=log_to_features_json(valid);
  response=execute_original_features_workbench(serialize(valid));assert(response.status==200);
  has(response.json,"\"type\":\"string\",\"value\":"+q(legacy));
  has(response.json,"\"legacy_result_json\":{\"bytes\":"+q(legacy));
  rejects([&]{(void)parse_original_features_result_json("{}");});
  auto wrong=serialize_literal_controls_result_json(valid);
  const auto cell=wrong.find("\"index\":0");wrong.replace(cell,9,"\"index\":2");
  rejects([&]{(void)parse_original_features_result_json(wrong);});
}
void original_log_defaults_variants_and_optional_export_errors() {
  auto log=fixture();
  log.metadata.compatibility_snapshot_valid=true;
  log.metadata.compatibility_globals_snapshot={{odd(),{}}};
  log.metadata.compatibility_trace_globals_snapshot={{odd(),odd()}};
  log.metadata.compatibility_event_globals_snapshot={{odd(),odd()}};
  log.metadata.extensions={{odd(),odd(),odd()}};
  log.metadata.classifiers={{odd(),{odd(),odd()}}};
  log.metadata.trace_globals={{odd(),odd()}};log.metadata.event_globals={{odd(),odd()}};
  log.metadata.properties={{odd(),odd()}};log.metadata.globals={{odd(),{}}};
  EventAttribute nested; nested.key=odd();nested.value=odd();
  for (int i=0;i<80;++i) { EventAttribute parent;parent.key="nested";parent.children={nested};nested=parent; }
  log.attributes={nested,nested};
  auto parts=extract(log);
  auto response=execute_original_features_workbench(parts);
  assert(response.status==200);
  has(response.json,"\"argument_form\":\"native_default\"");
  has(response.json,"\"result\":{\"type\":\"LogToFeaturesResult\",\"value\":"+
      serialize_literal_controls_result_json(log_to_features(log)));
  has(response.json,"\"source\":true");has(response.json,"\"options\":true");
  has(response.json,"\"source_before\":"+exact_source_snapshot(EventDataView{log}));
  for (const auto name : {"event_based","trace_based","temporal"}) {
    auto selected=parts;selected.push_back({"features_variant","",name,false});
    response=execute_original_features_workbench(selected);
    assert(response.status==200);has(response.json,"\"mode\":\"variant_default\"");
    has(response.json,"\"argument_form\":\"explicit_options\"");
  }
  auto invalid=parts;invalid.push_back({"features_variant","","-9",false});
  response=execute_original_features_workbench(invalid);assert(response.status==400);
  has(response.json,"\"entered\":true,\"returned\":false");
  has(response.json,"\"name\":null,\"index\":-9");
  LogToFeaturesOptions options;options.feature_names={odd(),odd()};options.feature_names_supplied=true;
  options.event_string_attributes_supplied=true; // Explicit empty selection retains the supplied byte schema.
  parts.push_back({"features_options_file","",serialize_literal_controls_options_json(options),true});
  response=execute_original_features_workbench(parts);assert(response.status==200);
  has(response.json,"\"legacy_transport\":\"non_scalar_utf8\"");
  auto identifier_log=fixture();identifier_log.traces[0].attributes={{"identifier","not_boolean",AttributeKind::kBoolean}};
  options=LogToFeaturesOptions{};options.case_id_key="identifier";
  options.add_case_identifier_column=true;options.event_string_attributes_supplied=true;
  auto identifier_parts=extract(identifier_log);
  identifier_parts.push_back({"features_options_json","inline.txt",serialize_literal_controls_options_json(options),true});
  response=execute_original_features_workbench(identifier_parts);assert(response.status==200);
  has(response.json,"\"entered\":true,\"returned\":true");
  has(response.json,"\"legacy_result_json\":{\"bytes\":null,\"error\":");
  has(response.json,"\"result\":{\"type\":\"LogToFeaturesResult\",\"value\":{");
}
void transport_presence_original_bytes_and_errors() {
  const auto log=fixture();const auto base=extract(log);
  const auto fail=[&](auto parts,const std::string& message) {
    const auto response=execute_original_features_workbench(parts);
    assert(response.status==400);has(response.json,message);has(response.json,"\"entered\":false");
    return response;
  };
  auto parts=base;parts[1].file=false;parts[1].filename="";
  auto response=fail(parts,"requires actual filename presence");
  has(response.json,"\"source\":{\"filename\":null,\"bytes\":"+q(parts[1].bytes));
  parts=base;parts[1].filename="";response=execute_original_features_workbench(parts);assert(response.status==200);
  parts=base;parts.push_back({"features_options_json","","",false});parts.push_back({"features_variant","","",false});
  fail(parts,"conflict by presence");
  parts=base;parts.push_back(parts[1]);response=fail(parts,"duplicate original features control");
  has(response.json,"\"source\":null");
  response=execute_original_features_workbench(base,{{"operation","","extract_features",false}});
  assert(response.status==400);has(response.json,"\"operation\":null");
  parts=base;parts.push_back({"unknown","",odd(),false});response=fail(parts,"unknown original features control");
  has(response.json,"\"operation\":\"extract_features\"");has(response.json,"\"bytes\":"+q(odd()));
  parts=base;parts[1].bytes=odd();response=fail(parts,"Exact filter/discovery JSON:");
  has(response.json,"\"bytes\":"+q(odd()));
  parts=base;parts.push_back({"features_options_json","","{}",false});response=fail(parts,"missing field");
  has(response.json,"\"options_before\":null");has(response.json,"\"options_after\":null");
  parts=base;parts.push_back({"features_variant","","not_a_variant",false});response=fail(parts,"features_variant");
  has(response.json,"\"options_before\":null");
  response=execute_original_features_workbench(base,{},"provided_order","manual CLI read failure");
  assert(response.status==400);has(response.json,"\"phase\":\"transport\"");
  has(response.json,"\"operation\":\"extract_features\"");
}
} // namespace

int main() {
  all_options_and_exact_storage();
  complete_result_inverse_and_semantic_ownership();
  original_log_defaults_variants_and_optional_export_errors();
  transport_presence_original_bytes_and_errors();
}
