#include "minera/original_log_query_workbench_json.hpp"
#include "minera/exact_log_query_json.hpp"
#include "minera/exact_filter_discovery_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/quality_workbench_json.hpp"
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

std::vector<DiscoveryWorkbenchPart> op(std::string name) {
  return {{"operation","",std::move(name),false}};
}
void file(std::vector<DiscoveryWorkbenchPart>& parts,std::string name,std::string bytes) {
  parts.push_back({std::move(name),"original.json",std::move(bytes),true});
}
EventLog source_fixture() {
  EventLog log;
  for (int t=0;t<2;++t) {
    Trace trace;trace.case_id=t?"second":"first";
    for (int i=0;i<2;++i) {
      Event event;event.case_id=trace.case_id;event.activity=i?" B ":" A ";
      event.activity_state=FirstClassAttributeState::kPresent;
      event.input_order=static_cast<std::size_t>(t*2+i);
      trace.events.push_back(event);
    }
    log.traces.push_back(trace);
  }
  Trace empty;empty.case_id="empty";log.traces.push_back(empty);
  return log;
}
EventAttribute stored_attribute() {
  EventAttribute a;a.key=odd();a.value=odd();a.kind=static_cast<AttributeKind>(-13);
  a.container=static_cast<AttributeContainerKind>(-19);a.is_null=true;
  a.scalar_wrapper=true;a.scalar_children_list=true;
  a.first_class_role=static_cast<FirstClassAttributeRole>(-23);
  a.first_class_state=static_cast<FirstClassAttributeState>(-29);
  a.first_class_source_key=odd();a.typed_value=NativeDate{odd(),static_cast<NativeDateResolution>(-31),odd()};
  for (int i=0;i<7;++i) {
    EventAttribute child;child.key="duplicate";child.value=odd();
    switch (i) {
      case 0:break;
      case 1:child.typed_value=std::numeric_limits<std::int64_t>::min();break;
      case 2:child.typed_value=-0.0;break;
      case 3:child.typed_value=true;break;
      case 4:child.typed_value=NativeDate{odd(),NativeDateResolution::kNanosecond,std::string("")};break;
      case 5:child.typed_value=std::numeric_limits<std::uint64_t>::max();break;
      case 6:child.typed_value=NativeNA{};break;
    }
    a.children.push_back(child);
  }
  return a;
}
LogQueryTable stored_table() {
  LogQueryTable result;result.event_index_column=odd();auto& t=result.table;
  t.columns={odd(),odd(),""};
  auto a=stored_attribute();t.rows={{},{std::nullopt,a},{a,a,std::nullopt,a}};t.attributes={a,a};
  t.dtypes={odd()};t.metadata.extensions={{odd(),odd(),odd()}};
  t.metadata.classifiers={{odd(),{odd(),odd()}}};t.metadata.trace_globals={a};t.metadata.event_globals={a};
  t.metadata.properties={a};t.metadata.globals={{odd(),{a}},{odd(),{}}};
  t.metadata.compatibility_snapshot_valid=true;t.metadata.compatibility_globals_snapshot={{odd(),{a}}};
  t.metadata.compatibility_trace_globals_snapshot={a};t.metadata.compatibility_event_globals_snapshot={a};
  EventTableIndex index;index.kind="";index.dtype=odd();index.names={odd(),std::nullopt};
  EventTableIndexLabel label;label.value=a;
  for (int i=0;i<80;++i) { EventTableIndexLabel tuple;tuple.value=std::vector<EventTableIndexLabel>{label};label=std::move(tuple); }
  index.labels={label};t.index=index;
  t.categorical_dtypes={std::nullopt,EventTableCategoricalDtype{odd(),{a,a},true}};
  return result;
}
void full_constructor_codecs_without_semantic_validation() {
  const auto table=stored_table();const auto original=serialize_original_log_query_table_json(table);
  const auto restored=parse_original_log_query_table_json(original);
  assert(serialize_original_log_query_table_json(restored)==original);
  assert(restored.table.rows[1].size()==2&&restored.table.rows[2].size()==4);
  assert(restored.table.columns[0]==restored.table.columns[1]&&restored.table.columns[0]==odd());
  assert(std::signbit(std::get<double>(restored.table.attributes[0].children[2].typed_value)));
  assert(std::get<NativeDate>(restored.table.attributes[0].typed_value).timezone==std::optional<std::string>(odd()));
  assert(serialize_original_log_query_table_json(parse_original_log_query_table_json(
      "{\"type\":\"LogQueryTable\",\"value\":"+original+'}'))==original);
  // The full storage writer does not omit compatibility defaults or route
  // valid Unicode data through a semantically validating projection.
  LogQueryTable empty;const auto bare=serialize_original_log_query_table_json(empty);
  has(bare,"\"native_schema\":\"event-table-raw-v1\"");has(bare,"\"compatibility_snapshot_valid\":false");
  assert(serialize_original_log_query_table_json(parse_original_log_query_table_json(bare))==bare);
  rejects([&]{(void)parse_original_log_query_table_json("{\"table\":{},\"event_index_column\":\"\"}");});
  rejects([&]{(void)parse_original_log_query_table_json(original.substr(0,original.size()-1)+",\"unknown\":0}");});
  rejects([&]{(void)parse_original_log_query_table_json("{\"table\":null,\"table\":null,\"event_index_column\":\"\"}");});
}
LogQueryResult stored_result() {
  LogQueryResult r;r.engine=odd();r.sql=odd();r.event_index_column=odd();r.columns={"dup","dup",odd()};
  r.rows={{std::monostate{},std::numeric_limits<std::int64_t>::min(),std::numeric_limits<std::int64_t>::max(),
      -0.0,std::numeric_limits<double>::denorm_min(),std::numeric_limits<double>::max(),
      std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),
      -std::numeric_limits<double>::infinity(),odd(),QueryBlob{odd()},QueryBlob{""}}, {}, {std::string("ragged")}};
  r.output_truncated=true;r.can_filter=true;r.selected_event_indices={std::numeric_limits<std::size_t>::max(),2,2,0};
  return r;
}
void full_result_and_size_domains() {
  const auto result=stored_result();const auto text=serialize_exact_log_query_result_json(result);
  const auto restored=parse_original_log_query_result_json(text);
  assert(serialize_exact_log_query_result_json(restored)==text);
  assert(std::signbit(std::get<double>(restored.rows[0][3])));
  assert(std::get<double>(restored.rows[0][4])==std::numeric_limits<double>::denorm_min());
  assert(std::get<double>(restored.rows[0][5])==std::numeric_limits<double>::max());
  assert(std::get<QueryBlob>(restored.rows[0][10]).hex==odd());
  assert(parse_original_log_query_indices_json("[\"0000\",1,1,\"18446744073709551615\"]")==
      (std::vector<std::size_t>{0,1,1,std::numeric_limits<std::size_t>::max()}));
  for (const auto token : {"[-0]","[-1]","[true]","[null]","[1.0]","[1e0]","[18446744073709551616]","[\"+1\"]"})
    rejects([&] { (void)parse_original_log_query_indices_json(token); });
  for (const auto token : {"true","null","{}","1e999"}) {
    auto wrong=text;const auto place=wrong.find("-0");assert(place!=std::string::npos);
    wrong.replace(place,2,token);rejects([&] { (void)parse_original_log_query_result_json(wrong); });
  }
  assert(serialize_exact_log_query_result_json(parse_original_log_query_result_json(
      "{\"type\":\"LogQueryResult\",\"value\":"+text+'}'))==text);
  auto parts=op("log_query_json");file(parts,"result_file",text);
  const auto response=execute_original_log_query_workbench(parts);assert(response.status==200);
  has(response.json,"\"input_result\":true");has(response.json,"\"type\":\"NativeString\"");
  has(response.json,"\"native_json\":{\"bytes\":"+q(log_query_json(result))+",\"error\":null}");
  assert(artifact(response.json,"query_result_json")==text);
  assert(artifact(response.json,"selected_event_indices_json")==serialize_original_log_query_indices_json(result.selected_event_indices));
}
void actual_public_calls_defaults_and_byte_handoffs() {
  const auto source=source_fixture();const auto raw=serialize_embedding_workbench_raw_log_json(source);
  auto make=op("log_query_table");file(make,"source_file",raw);
  const auto constructed=execute_original_log_query_workbench(make);assert(constructed.status==200);
  const auto table_bytes=artifact(constructed.json,"query_table_json");
  const auto direct=log_query_table(source);
  assert(table_bytes==serialize_original_log_query_table_json(direct));
  has(constructed.json,"\"source_before\":{\"type\":\"EventLog\"");
  has(constructed.json,"\"argument_form\":\"direct\",\"entered\":true,\"returned\":true");
  auto serialize=op("log_query_table_json");file(serialize,"table_file",table_bytes);
  const auto serialized=execute_original_log_query_workbench(serialize);assert(serialized.status==200);
  assert(artifact(serialized.json,"native_json")==log_query_table_json(direct));
  const auto availability=execute_original_log_query_workbench(op("sqlite_log_query_available"));assert(availability.status==200);
  has(availability.json,std::string("\"result\":{\"type\":\"bool\",\"value\":")+(sqlite_log_query_available()?"true":"false")+'}');
  if (!sqlite_log_query_available()) return;
  auto query=op("query_log_sqlite");file(query,"table_file",table_bytes);
  query.push_back({"sql","","SELECT * FROM dataframe WHERE \"concept:name\"=' B '",false});
  auto queried=execute_original_log_query_workbench(query);assert(queried.status==200);
  has(queried.json,"\"argument_form\":\"native_default\"");
  has(queried.json,"\"options_before\":{\"type\":\"LogQueryOptions\"");
  auto full=LogQueryOptions{};full.table_name=" full \" quoted Ω ";full.parameters={std::string(" B ")};full.max_rows=2;
  query.back().bytes="SELECT * FROM \" full \"\" quoted Ω \" WHERE \"concept:name\"=?";
  query.push_back({"query_options_json","bytes.txt",serialize_exact_log_query_options_json({"sqlite",full}),true});
  queried=execute_original_log_query_workbench(query);assert(queried.status==200);
  has(queried.json,"\"argument_form\":\"explicit_options\"");
  const auto result_bytes=artifact(queried.json,"query_result_json");
  const auto expected=query_log_sqlite(direct,query[2].bytes,full);
  assert(result_bytes==serialize_exact_log_query_result_json(expected));
  const auto indices_bytes=artifact(queried.json,"selected_event_indices_json");
  auto filter=op("filter_query_events");file(filter,"source_file",raw);file(filter,"indices_file",indices_bytes);
  auto filtered=execute_original_log_query_workbench(filter);assert(filtered.status==200);
  assert(artifact(filtered.json,"filtered_log_json")==serialize_embedding_workbench_raw_log_json(filter_query_events(source,expected.selected_event_indices)));
  has(filtered.json,"\"argument_form\":\"native_default\"");
  filter.push_back({"whole_cases","","true",false});
  filtered=execute_original_log_query_workbench(filter);assert(filtered.status==200);
  assert(artifact(filtered.json,"filtered_log_json")==serialize_embedding_workbench_raw_log_json(filter_query_events(source,expected.selected_event_indices,true)));
  filter.back().bytes="false";filtered=execute_original_log_query_workbench(filter);assert(filtered.status==200);
  has(filtered.json,"\"argument_form\":\"explicit_whole_cases\"");
  assert(artifact(filtered.json,"filtered_log_json")==serialize_embedding_workbench_raw_log_json(filter_query_events(source,expected.selected_event_indices,false)));
  serialize=op("log_query_json");file(serialize,"result_file",result_bytes);
  assert(execute_original_log_query_workbench(serialize).status==200);
}
void native_errors_full_states_and_transport_boundaries() {
  // Ragged original tables are accepted by the serializer without validate.
  LogQueryTable ragged;ragged.event_index_column="absent";ragged.table.columns={"same","same"};
  ragged.table.rows={{},{EventAttribute{"a","one"}}};
  const auto bytes=serialize_original_log_query_table_json(ragged);
  auto parts=op("log_query_table_json");file(parts,"table_file",bytes);
  auto response=execute_original_log_query_workbench(parts);assert(response.status==200);
  assert(artifact(response.json,"native_json")==log_query_table_json(ragged));
  has(response.json,"\"table\":true");
  if (sqlite_log_query_available()) {
    parts[0].bytes="query_log_sqlite";parts.push_back({"sql","","",false});
    response=execute_original_log_query_workbench(parts);assert(response.status==400);
    has(response.json,"\"phase\":\"native\"");has(response.json,"\"entered\":true,\"returned\":false");
    // Native table validation wins over empty SQL; no adapter prevalidation.
    has(response.json,"event table: duplicate column name");
    assert(artifact(response.json,"query_table_json")==bytes);
  }
  auto source=source_fixture();auto a=stored_attribute();source.attributes={a,a};source.metadata.compatibility_snapshot_valid=true;
  source.metadata.compatibility_globals_snapshot={{odd(),{a}}};
  source.metadata.compatibility_trace_globals_snapshot={a};source.metadata.compatibility_event_globals_snapshot={a};
  source.traces[0].properties={a};source.traces[0].events[0].activity_attribute=a;
  source.traces[0].events[0].input_order=std::numeric_limits<std::size_t>::max();
  const auto raw=serialize_embedding_workbench_raw_log_json(source);
  auto original=op("filter_query_events");file(original,"source_file",raw);
  original.push_back({"indices_json","","[2,0,2]",false});
  response=execute_original_log_query_workbench(original);assert(response.status==200);
  has(response.json,"\"source\":true");has(response.json,"\"indices\":true");
  assert(artifact(response.json,"indices_json")=="[2,0,2]");
  assert(artifact(response.json,"filtered_log_json")==serialize_embedding_workbench_raw_log_json(filter_query_events(source,{2,0,2})));
  parts=original;parts.back().bytes="[18446744073709551615]";
  response=execute_original_log_query_workbench(parts);assert(response.status==400);
  has(response.json,"selected event index is outside the source log");has(response.json,"\"source\":true");
  const auto fails=[&](const auto& p,const std::string& expected) {
    const auto r=execute_original_log_query_workbench(p);assert(r.status==400);
    has(r.json,expected);has(r.json,"\"entered\":false");return r;
  };
  parts=original;parts[1].file=false;parts[1].filename="";
  response=fails(parts,"requires actual filename presence");
  has(response.json,"\"source\":{\"filename\":null,\"bytes\":"+q(raw));
  parts=original;parts[1].filename="";assert(execute_original_log_query_workbench(parts).status==200);
  parts=original;parts.push_back(parts[1]);response=fails(parts,"duplicate original log query control");has(response.json,"\"source\":null");
  parts=original;file(parts,"indices_file","[]");fails(parts,"exactly one indices_json or indices_file");
  parts=original;parts.push_back({"whole_cases","","0",false});fails(parts,"whole_cases must be true or false");
  parts=original;parts.push_back({"query_options_json","","{}",false});fails(parts,"inapplicable");
  parts=original;parts.push_back({"unknown","",odd(),false});response=fails(parts,"unknown original log query control");has(response.json,q(odd()));
  parts=original;parts[1].bytes="not JSON";response=fails(parts,"Exact filter/discovery JSON:");has(response.json,"\"phase\":\"source_parse\"");
  response=execute_original_log_query_workbench(original,{{"operation","","filter_query_events",false}});
  assert(response.status==400);has(response.json,"\"operation\":null");
  response=execute_original_log_query_workbench({{"operation","","log_query_table",false}},{{"source_file","a.json",raw,true}});
  assert(response.status==400);has(response.json,"File role cannot be a query control");
  response=execute_original_log_query_workbench(original,{},"provided_order","owned transport failure");
  assert(response.status==400);has(response.json,"\"operation\":\"filter_query_events\"");
  has(response.json,"\"phase\":\"transport\"");has(response.json,"\"cli\":null");
  const std::vector<std::string> argv={"C:/native/query.exe","--source","C:/long path/actual.json",odd()};
  response=execute_original_log_query_workbench(original,{},"provided_order",{},&argv);
  assert(response.status==200);has(response.json,"\"cli\":{\"argv\":[");has(response.json,q(argv[2]));has(response.json,q(argv[3]));
}
} // namespace
int main() {
  full_constructor_codecs_without_semantic_validation();
  full_result_and_size_domains();
  actual_public_calls_defaults_and_byte_handoffs();
  native_errors_full_states_and_transport_boundaries();
}
