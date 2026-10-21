#include "httplib.h"
#include "../tools/table_attribute_repetition_route.hpp"
#include "table_attribute_repetition_test_support.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

namespace {
using namespace table_repetition_test;
constexpr auto route="/api/reference-table-repetition";
class OwnedServer {
 public:
  OwnedServer(){server.Post(route,handle_table_attribute_repetition);port=server.bind_to_any_port("127.0.0.1");require(port>0,"could not bind owned server");worker=std::thread([this]{listened=server.listen_after_bind();finished=true;});
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(!server.is_running()&&!finished&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if(!server.is_running()){stop();throw std::runtime_error("owned HTTP server failed to start");}}
  ~OwnedServer(){stop();}void stop(){server.stop();if(worker.joinable())worker.join();}
  httplib::Server server;int port=-1;std::thread worker;std::atomic<bool> finished=false,listened=false;
};
httplib::MultipartFormDataItems multipart(const std::vector<DiscoveryWorkbenchPart>& parts) {
  httplib::MultipartFormDataItems result;for(const auto& part:parts)result.push_back({part.name,part.bytes,part.file?part.filename:"",part.file?"application/json":"text/plain"});return result;
}
DiscoveryWorkbenchResponse received(const httplib::Result& response){require(static_cast<bool>(response),"owned multipart request failed");require(response->get_header_value("Content-Type").starts_with("application/json"),"HTTP response type differs");return {response->status,response->body};}
void actual_table_http_workflows(httplib::Client& client) {
  for(const std::string engine:{"pandas","polars"}) {
    const auto source=source_table(engine),expected=expected_table(source);
    const auto result=received(client.Post(route,multipart(request(engine,source))));success(result,source,expected,engine);
    // Reupload the exact complete returned artifact, including opaque bytes and
    // positive signed/unsigned carriers. It remains a table throughout.
    auto follow=request(engine,source);follow[2].bytes=*text(result.json,"filtered_table_json");
    success(received(client.Post(route,multipart(follow))),expected,expected,engine);
    auto failed=received(client.Post(route,multipart(request(engine,source,{field("attribute_key","missing")}))));require(failed.status==400,"missing HTTP column succeeded");contains(failed,engine=="polars"?"\"phase\":\"collect\"":"\"phase\":\"apply\"");
    failed=received(client.Post(std::string(route)+"?engine="+engine,multipart(request(engine,source))));require(failed.status==400,"duplicate query/multipart engine accepted");contains(failed,"\"phase\":\"control\"");
  }
  const auto failed=received(client.Post(route,"not multipart","text/plain"));require(failed.status==400,"nonmultipart accepted");contains(failed,"multipart native EventTable JSON File");
}
}  // namespace
int main(){try{OwnedServer owned;httplib::Client client("127.0.0.1",owned.port);client.set_connection_timeout(3,0);client.set_read_timeout(3,0);actual_table_http_workflows(client);owned.stop();require(owned.listened,"owned server listen failed");std::cout<<"pandas/Polars actual ephemeral HTTP table workflows passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
