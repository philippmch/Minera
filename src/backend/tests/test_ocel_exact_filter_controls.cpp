#include "httplib.h"
#include "minera/ocel_exact_filter_controls.hpp"
#include "minera/ocel.hpp"
#include "../tools/ocel_exact_filter_route.hpp"
#include <cassert>
#include <chrono>
#include <thread>
#include <iostream>

using namespace minera;
namespace {
template<class Action> auto checked(const char* label,Action action){
  try{return action();}catch(const std::exception& error){throw std::runtime_error(std::string(label)+": "+error.what());}
}
void parser_and_native_controls(){
  const auto values=checked("values",[&]{return parse_ocel_event_filter_values_json(R"([{"type":"integer","value":9007199254740993},{"type":"decimal","value":"NaN"},{"type":"timestamp","value":"2026-01-01T00:00:00.000000001Z"},{"type":"string","value":" ,:\n "},{"type":"null"},{"type":"boolean","value":true}])");});
  assert(std::get<std::int64_t>(values[0])==9007199254740993LL);
  assert(std::isnan(std::get<double>(values[1])));
  assert(std::get<ParsedExactTimestamp>(values[2]).nanoseconds==1);
  assert(std::get<std::string>(values[3])==" ,:\n "&&std::holds_alternative<std::nullptr_t>(values[4])&&std::get<bool>(values[5]));
  const auto missing=checked("missing",[&]{return parse_ocel_event_filter_values_json(R"([{"type":"nat"},{"type":"nat","value":null}])");});
  assert(missing.size()==2&&std::holds_alternative<OcelAttributeNaT>(missing[0])&&std::holds_alternative<OcelAttributeNaT>(missing[1]));
  const auto counts=checked("counts",[&]{return parse_ocel_exact_minimum_counts_json(R"({"":-999999999999999999999999999999999,"U":1.5,"V":"NaN","W":true})");});
  assert(std::get<TokenInteger>(counts.at("")).decimal()=="-999999999999999999999999999999999");
  assert(std::get<double>(counts.at("U"))==1.5&&std::isnan(std::get<double>(counts.at("V"))));
  assert(std::get<TokenInteger>(counts.at("W"))==TokenInteger(1));
  const auto bound=checked("bound",[&]{return parse_ocel_event_timestamp_bound_json(R"({"type":"timestamp","value":"2026-01-01T01:00:00.000000001+01:00"})");});
  assert(std::get<ParsedExactTimestamp>(bound).offset_minutes==60);
  assert(std::get<std::string>(parse_ocel_event_timestamp_bound_json(R"({"type":"string","value":"2026-01-01 00:00:00"})"))=="2026-01-01 00:00:00");
  for(const auto bad:{"[1]","[{}]","[{\"type\":\"nat\",\"value\":\"NaT\"}]","[{\"type\":\"integer\",\"value\":9223372036854775808}]","[{\"type\":\"null\",\"value\":1}]","[true,]","[]x","[{\"type\":\"string\",\"value\":\"a\",\"value\":\"b\"}]"}){
    bool threw=false;try{(void)parse_ocel_event_filter_values_json(bad);}catch(const std::invalid_argument&){threw=true;}assert(threw);
  }
  for(const auto bad:{"[]","{\"A\":null}","{\"A\":\"2\"}","{\"A\":01}"}){
    bool threw=false;try{(void)parse_ocel_exact_minimum_counts_json(bad);}catch(const std::invalid_argument&){threw=true;}assert(threw);
  }
}
struct ServerGuard {
  httplib::Server& server;std::thread& worker;
  ~ServerGuard(){server.stop();if(worker.joinable())worker.join();}
};
void literal_name_interchange(){
  Ocel log;log.events={{"","","2026-01-01T00:00:00Z",{{"",std::int64_t{7}}}}};
  log.objects={{"","",{{"",true}}}};log.relations={{"","",""}};
  for(const auto format:{OcelJsonFormat::kClassic,OcelJsonFormat::kStandard}){
    const auto ordinary=serialize_ocel_json(log,format);
    bool rejected=false;try{(void)parse_ocel_json(ordinary);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    const auto exact=serialize_ocel_exact_filter_json(log,format);
    const auto reopened=parse_ocel_json(exact);
    assert(reopened.events.at(0).id.empty()&&reopened.events.at(0).attributes.at(0).key.empty());
    assert(reopened.objects.at(0).id.empty()&&reopened.objects.at(0).type.empty());
    assert(reopened.objects.at(0).attributes.at(0).key.empty()&&reopened.relations.size()==1);
    assert(serialize_ocel_exact_filter_json(reopened,format)==exact);
  }
}
void actual_http_controls(){
  Ocel source;
  source.events={{" e1 ","A","2026-01-01T00:00:00.000000001Z",{{"",std::int64_t{9007199254740993LL}}}},
                 {"e2","B","2026-01-01T00:00:00.000000002Z",{{"",std::int64_t{9007199254740992LL}}}}};
  source.objects={{"o","T",{}}};source.relations={{" e1 ","o","q"},{" e1 ","o","duplicate"},{"e2","o","q"}};
  source.e2e={{" e1 ","e2","q"}};source.globals={{"literal","original"}};
  const auto before=serialize_ocel_json(source);
  // Interchange import retains the declared A/B catalogs inferred by export,
  // including B after filtering. Build the expected selection from that actual
  // imported source rather than dropping its declarations with a fresh export.
  auto expected=parse_ocel_json_literal_names(before);
  expected.events.pop_back();expected.relations.pop_back();expected.e2e.clear();
  assert(expected.event_type_attributes.contains("B"));
  httplib::Server server;
  server.Post("/api/ocel/filter",[&](const httplib::Request& request,httplib::Response& response){
    try{
      const auto parsed=parse_ocel_json_literal_names(request.body);
      const auto result=dispatch_ocel_exact_filter(parsed,request,request.get_param_value("variant"),true);
      if(!result)throw std::invalid_argument("unsupported exact variant");
      response.set_content(serialize_ocel_exact_filter_json(result->log),"application/json");
    }catch(const std::exception& e){response.status=400;response.set_content(e.what(),"text/plain");}
  });
  const auto port=server.bind_to_any_port("127.0.0.1");assert(port>0);
  std::thread worker([&]{server.listen_after_bind();});ServerGuard guard{server,worker};
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
  while(!server.is_running()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(server.is_running());httplib::Client client("127.0.0.1",port);client.set_read_timeout(3,0);
  const auto post=[&](const httplib::Params& controls){
    std::string query="/api/ocel/filter?";for(const auto& [key,value]:controls)query+=httplib::detail::encode_url(key)+'='+httplib::detail::encode_url(value)+'&';
    return client.Post(query,before,"application/json");
  };
  for(const auto& controls:std::vector<httplib::Params>{
      {{"variant","event_attribute_exact"},{"attribute_key",""},{"attribute_values_json",R"([{"type":"integer","value":9007199254740993}])"}},
      {{"variant","timestamp_exact"},{"from_json",R"({"type":"timestamp","value":"2026-01-01T00:00:00.000000001Z"})"},{"to_json",R"({"type":"timestamp","value":"2026-01-01T00:00:00.000000001Z"})"}},
      {{"variant","object_count_exact"},{"minimum_counts_json",R"({"T":1.5})"}}}){
    const auto response=post(controls);
    if(!response||response->status!=200)throw std::runtime_error(response?response->body:"HTTP request failed");
    const auto actual=serialize_ocel_json(parse_ocel_json(response->body));
    if(actual!=serialize_ocel_json(expected))throw std::runtime_error("Actual: "+actual+"\nExpected: "+serialize_ocel_json(expected));
  }
  const auto huge=post({{"variant","object_count_exact"},{"minimum_counts_json","{\"T\":"+std::string(100,'9')+'}'}});
  assert(huge&&huge->status==200&&parse_ocel_json(huge->body).events.empty());
  const auto bad=post({{"variant","timestamp_exact"},{"from_json",R"({"type":"timestamp","value":"2026-01-01Z"})"}});
  assert(bad&&bad->status==400);
  assert(serialize_ocel_json(source)==before);
 }
void typed_nat_native_controls(){
  Ocel source;
  source.events={{"e1","A","NaT",{}},{"e2","A","2026-01-01T00:00:00Z",{}}};
  source.objects={{"o1","T",{{"when",std::string("NaT")}}},{"o2","T",{{"when",std::string("2026-01-01T00:00:00Z")}}}};
  source.relations={{"e1","o1","q"},{"e2","o2","q"}};
  source.object_type_attributes["T"]["when"]="date";
  const auto nat=checked("nat",[&]{return parse_ocel_event_filter_values_json(R"([{"type":"nat"}])");});
  const auto ordinary=checked("ordinary",[&]{return parse_ocel_event_filter_values_json(R"([{"type":"null"},{"type":"decimal","value":"NaN"}])");});
  assert(filter_ocel_event_attribute_exact(source,"ocel:timestamp",nat).events.at(0).id=="e1");
  assert(filter_ocel_event_attribute_exact(source,"ocel:timestamp",ordinary).events.empty());
  assert(filter_ocel_object_attribute_exact(source,"when",nat).objects.at(0).id=="o1");
  assert(filter_ocel_object_attribute_exact(source,"when",ordinary).objects.empty());
  assert(filter_ocel_object_attribute_exact(source,"when",nat,true,OcelEventAttributeColumnKind::kScalar).objects.empty());
  assert(filter_ocel_object_attribute_exact(source,"when",nat,false).objects.at(0).id=="o2");
}
void actual_http_object_controls(){
  Ocel source;
  source.events={{"e1","A","2026-01-01T00:00:00Z",{}},{"e2","B","2026-01-01T00:00:01Z",{}}};
  source.objects={{"","T",{{"",std::int64_t{9007199254740993LL}}}},
                  {"o2","U",{{"",std::int64_t{9007199254740992LL}}}},
                  {"unused","T",{{"",std::int64_t{9007199254740993LL}}}}};
  source.relations={{"e1","","first"},{"e1","","duplicate"},{"e2","o2","q"}};
  source.o2o={{"","unused","retained"},{"","o2","removed"}};
  source.e2e={{"e1","e1","retained"},{"e1","e2","removed"}};
  source.globals={{"source","original"}};
  const auto before=serialize_ocel_json(source);
  auto expected=parse_ocel_json_literal_names(before);
  std::erase_if(expected.objects,[](const auto& value){return value.id=="o2";});
  std::erase_if(expected.events,[](const auto& value){return value.id=="e2";});
  std::erase_if(expected.relations,[](const auto& value){return value.object_id=="o2";});
  std::erase_if(expected.o2o,[](const auto& value){return value.target_id=="o2";});
  std::erase_if(expected.e2e,[](const auto& value){return value.target_id=="e2";});
  httplib::Server server;
  server.Post("/api/ocel/filter",[&](const httplib::Request& request,httplib::Response& response){
    try{
      const auto parsed=parse_ocel_json_literal_names(request.body);
      const auto result=dispatch_ocel_exact_filter(parsed,request,request.get_param_value("variant"),request.get_param_value("positive")!="false");
      if(!result)throw std::invalid_argument("unsupported exact variant");
      assert(result->variant=="OBJECT_ATTRIBUTE_EXACT");
      response.set_content(serialize_ocel_exact_filter_json(result->log),"application/json");
    }catch(const std::exception& e){response.status=400;response.set_content(e.what(),"text/plain");}
  });
  const auto port=server.bind_to_any_port("127.0.0.1");assert(port>0);
  std::thread worker([&]{server.listen_after_bind();});ServerGuard guard{server,worker};
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
  while(!server.is_running()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(server.is_running());httplib::Client client("127.0.0.1",port);client.set_read_timeout(3,0);
  const auto post=[&](std::string key,std::string values,bool positive=true){
    const auto query="/api/ocel/filter?variant=object_attribute_exact&attribute_key="+httplib::detail::encode_url(key)+"&attribute_values_json="+httplib::detail::encode_url(values)+"&positive="+(positive?"true":"false");
    return client.Post(query,before,"application/json");
  };
  for(const auto& controls:std::vector<std::pair<std::string,std::string>>{
      {"",R"([{"type":"integer","value":9007199254740993}])"},
      {"ocel:type",R"([{"type":"string","value":"T"}])"}}){
    const auto response=post(controls.first,controls.second);
    if(!response||response->status!=200)throw std::runtime_error(response?response->body:"Object HTTP request failed");
    const auto reopened=parse_ocel_json(response->body);
    if(serialize_ocel_json(reopened)!=serialize_ocel_json(expected))throw std::runtime_error("Object HTTP actual: "+serialize_ocel_json(reopened)+"\nExpected: "+serialize_ocel_json(expected));
    assert(reopened.objects.at(1).id=="unused"&&reopened.relations.size()==2);
    const auto negative=post(controls.first,controls.second,false);
    assert(negative&&negative->status==200);
    const auto removed=parse_ocel_json(negative->body);
    assert(removed.objects.size()==1&&removed.objects.at(0).id=="o2");
    assert(removed.events.size()==1&&removed.events.at(0).id=="e2");
  }
  const auto id=post("ocel:oid",R"([{"type":"string","value":""}])");
  assert(id&&id->status==200&&parse_ocel_json(id->body).objects.at(0).id.empty());
  const auto bad=post("missing","[]");assert(bad&&bad->status==400);
  assert(serialize_ocel_json(source)==before);
}
}
int main(){const char* stage="parser_and_native_controls";try{
  parser_and_native_controls();
  stage="typed_nat_native_controls";typed_nat_native_controls();
  stage="literal_name_interchange";literal_name_interchange();
  stage="actual_http_controls";actual_http_controls();
  stage="actual_http_object_controls";actual_http_object_controls();
  std::cout<<"Exact OCEL JSON controls and real HTTP dispatch passed\n";
}catch(const std::exception& error){std::cerr<<stage<<": "<<error.what()<<'\n';return 1;}}
