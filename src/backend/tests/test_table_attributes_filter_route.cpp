#include "httplib.h"
#include "../tools/table_attributes_filter_route.hpp"
#define MINERA_ATTRIBUTES_ROUTE_TEST
#include "test_table_attributes_filter_workbench.cpp"
#undef MINERA_ATTRIBUTES_ROUTE_TEST
#include <atomic>
#include <chrono>
#include <thread>
namespace {
using namespace table_attributes_test;
constexpr auto route="/api/reference-table-attributes";
class OwnedServer {
 public:
 OwnedServer(){server.Post(route,handle_table_attributes_filter);port=server.bind_to_any_port("127.0.0.1");require(port>0,"owned attributes bind failed");worker=std::thread([this]{listened=server.listen_after_bind();finished=true;});
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(!server.is_running()&&!finished&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  if(!server.is_running()){stop();throw std::runtime_error("owned attributes server did not start");}}
 ~OwnedServer(){stop();}void stop(){server.stop();if(worker.joinable())worker.join();}
 httplib::Server server;int port=-1;std::thread worker;std::atomic<bool> finished=false,listened=false;
};
DiscoveryWorkbenchResponse received(const httplib::Result& response){require(static_cast<bool>(response),"owned attributes HTTP request failed");require(response->get_header_value("Content-Type").starts_with("application/json"),"attributes HTTP lost JSON media type");return {response->status,response->body};}
DiscoveryWorkbenchResponse send(httplib::Client& client,const Parts& parts,std::string path=route){
 // Explicit filename presence is semantically distinct from an empty filename.
 const std::string boundary="minera-attributes-owned-boundary";std::string body;
 for(const auto& part:parts){body+="--"+boundary+"\r\nContent-Disposition: form-data; name=\""+part.name+'"';if(part.file)body+="; filename=\""+part.filename+'"';body+="\r\nContent-Type: ";body+=part.file?"application/json":"text/plain";body+="\r\n\r\n"+part.bytes+"\r\n";}
 body+="--"+boundary+"--\r\n";return received(client.Post(path,body,"multipart/form-data; boundary="+boundary));
}
}
int main(){try{
 OwnedServer owned;httplib::Client client("127.0.0.1",owned.port);client.set_connection_timeout(5,0);client.set_read_timeout(5,0);
 workflows([&](const auto& parts){return send(client,parts);});
 for(const std::string engine:{"pandas","polars"}){
  const auto source=source_table(engine),expected=subset(source,{0,2});
  auto request=attributes_request(engine,source);for(auto& part:request)if(part.name=="source_file")part.filename="";
  const auto first=send(client,request);table_success(first,source,expected,engine);
  for(auto& part:request)if(part.name=="source_file")part.bytes=*text(first.json,"filtered_table_json");
  table_success(send(client,request),expected,expected,engine);
  auto not_file=attributes_request(engine,source);for(auto& part:not_file)if(part.name=="source_file")part.file=false;
  const auto scalar=send(client,not_file);require(scalar.status==400,"plain multipart source content accepted as File");contains(scalar,"\"phase\":\"control\"");
  const auto duplicate=send(client,attributes_request(engine,source),std::string(route)+"?engine="+engine);require(duplicate.status==400,"query/multipart duplicate accepted");contains(duplicate,"\"phase\":\"control\"");
  const auto unknown=send(client,attributes_request(engine,source,"events",{field("unexpected","x")}));require(unknown.status==400,"unknown multipart control accepted");
 }
 const auto ordinary=received(client.Post(route,"not a File","text/plain"));require(ordinary.status==400,"nonmultipart source accepted");contains(ordinary,"multipart");
 const auto malformed=client.Post(route,"missing multipart delimiters","multipart/form-data; boundary=broken");require(malformed&&malformed->status==400,"malformed multipart accepted");
 owned.stop();require(owned.listened,"owned attributes server listen failed");std::cout<<"owned attributes HTTP workflows, strict multipart and empty-filename File passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
