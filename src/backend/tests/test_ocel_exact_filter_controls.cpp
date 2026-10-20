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
void parser_and_native_controls(){
  const auto values=parse_ocel_event_filter_values_json(R"([{"type":"integer","value":9007199254740993},{"type":"decimal","value":"NaN"},{"type":"timestamp","value":"2026-01-01T00:00:00.000000001Z"},{"type":"string","value":" ,:\n "},{"type":"null"},{"type":"boolean","value":true}])");
  assert(std::get<std::int64_t>(values[0])==9007199254740993LL);
  assert(std::isnan(std::get<double>(values[1])));
  assert(std::get<ParsedExactTimestamp>(values[2]).nanoseconds==1);
  assert(std::get<std::string>(values[3])==" ,:\n "&&std::holds_alternative<std::nullptr_t>(values[4])&&std::get<bool>(values[5]));
  const auto counts=parse_ocel_exact_minimum_counts_json(R"({"":-999999999999999999999999999999999,"U":1.5,"V":"NaN","W":true})");
  assert(std::get<TokenInteger>(counts.at("")).decimal()=="-999999999999999999999999999999999");
  assert(std::get<double>(counts.at("U"))==1.5&&std::isnan(std::get<double>(counts.at("V"))));
  assert(std::get<TokenInteger>(counts.at("W"))==TokenInteger(1));
  const auto bound=parse_ocel_event_timestamp_bound_json(R"({"type":"timestamp","value":"2026-01-01T01:00:00.000000001+01:00"})");
  assert(std::get<ParsedExactTimestamp>(bound).offset_minutes==60);
  assert(std::get<std::string>(parse_ocel_event_timestamp_bound_json(R"({"type":"string","value":"2026-01-01 00:00:00"})"))=="2026-01-01 00:00:00");
  for(const auto bad:{"[1]","[{}]","[{\"type\":\"integer\",\"value\":9223372036854775808}]","[{\"type\":\"null\",\"value\":1}]","[true,]","[]x","[{\"type\":\"string\",\"value\":\"a\",\"value\":\"b\"}]"}){
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
}
int main(){try{parser_and_native_controls();literal_name_interchange();actual_http_controls();std::cout<<"Exact OCEL JSON controls and real HTTP dispatch passed\n";}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
