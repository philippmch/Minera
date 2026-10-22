#include "httplib.h"
#include "../tools/ltl_filter_route.hpp"
#define MINERA_LTL_ROUTE_TEST
#include "test_ltl_filter_workbench.cpp"
#undef MINERA_LTL_ROUTE_TEST
#include <atomic>
#include <chrono>
#include <thread>
namespace {
using namespace ltl_workbench_test;
constexpr auto route="/api/reference-ltl-filter";
class OwnedServer {
 public:
 OwnedServer(){server.Post(route,handle_reference_ltl_filter);port=server.bind_to_any_port("127.0.0.1");require(port>0,"owned relationship bind failed");worker=std::thread([this]{listened=server.listen_after_bind();finished=true;});
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(!server.is_running()&&!finished&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  if(!server.is_running()){stop();throw std::runtime_error("owned relationship server did not start");}}
 ~OwnedServer(){stop();}void stop(){server.stop();if(worker.joinable())worker.join();}
 httplib::Server server;int port=-1;std::thread worker;std::atomic<bool> finished=false,listened=false;
};
DiscoveryWorkbenchResponse received(const httplib::Result& response){require(static_cast<bool>(response),"owned relationship HTTP request failed");require(response->get_header_value("Content-Type").starts_with("application/json"),"relationship HTTP lost JSON media type");return {response->status,response->body};}
DiscoveryWorkbenchResponse send(httplib::Client& client,const Parts& parts,std::string path=route){
 // Explicit filename presence is semantically distinct from an empty filename.
 const std::string boundary="minera-relationships-owned-boundary";std::string body;
 for(const auto& part:parts){body+="--"+boundary+"\r\nContent-Disposition: form-data; name=\""+part.name+'"';if(part.file)body+="; filename=\""+part.filename+'"';body+="\r\nContent-Type: ";body+=part.file?"application/json":"text/plain";body+="\r\n\r\n"+part.bytes+"\r\n";}
 body+="--"+boundary+"--\r\n";return received(client.Post(path,body,"multipart/form-data; boundary="+boundary));
}
}
int main(){try{
 OwnedServer owned;httplib::Client client("127.0.0.1",owned.port);client.set_connection_timeout(5,0);client.set_read_timeout(5,0);
 workflows([&](const auto& parts){return send(client,parts);});
 for(const std::string engine:{"log","pandas","polars"}){
  const auto source=engine=="log"?serialize_embedding_workbench_raw_log_json(sample_log()):serialize_embedding_workbench_table_json(sample_table(engine));
  auto parts=ltl_workbench_test::request(engine,source);parts[2].filename="";const auto answer=send(client,parts);require(answer.status==200,"empty filename relationship File rejected");
  const auto filtered=text(answer.json,"result_json");require(filtered.has_value(),"filtered relationship artifact missing");parts[2].bytes=*filtered;
  const auto reopened=send(client,parts);require(reopened.status==200 && text(reopened.json,"result_json")==filtered,"actual relationship result cannot be reuploaded");
  parts[2].file=false;require(send(client,parts).status==400,"plain text relationship source accepted");
  require(send(client,ltl_workbench_test::request(engine,source),std::string(route)+"?engine="+engine).status==400,"duplicate relationship query accepted");
 }
 const auto polars_source=serialize_embedding_workbench_table_json(sample_table("polars"));
 const auto duplicate_kind=send(client,ltl_workbench_test::request("polars",polars_source,"eventually",{field("input_kind","DataFrame")}),std::string(route)+"?input_kind=LazyFrame");
 require(duplicate_kind.status==400 && !text(duplicate_kind.json,"input_kind"),"duplicate HTTP query/multipart input kind accepted");contains(duplicate_kind,"\"phase\":\"control\"");
 const auto ordinary=received(client.Post(route,"not multipart","text/plain"));require(ordinary.status==400,"nonmultipart relationship request accepted");
 const auto malformed=client.Post(route,"missing delimiters","multipart/form-data; boundary=broken");require(malformed && malformed->status==400,"malformed relationship multipart accepted");
 owned.stop();require(owned.listened,"owned relationship server listen failed");std::cout<<"owned relationship HTTP workflows and strict actual Files passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
