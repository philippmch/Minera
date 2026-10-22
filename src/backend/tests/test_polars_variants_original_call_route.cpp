#define MINERA_POLARS_VARIANTS_ORIGINAL_ROUTE_TEST
#include "test_polars_variants_original_call_workbench.cpp"
#include "httplib.h"
#include "../tools/table_variants_filter_route.hpp"
#include <atomic>
#include <chrono>
#include <thread>
int main(){try{
  using namespace polars_variants_original_test;httplib::Server server;server.Post("/api/reference-table-variants",handle_table_variants_filter);const auto port=server.bind_to_any_port("127.0.0.1");require(port>0,"owned Polars HTTP server bind failed");std::atomic<bool> finished=false;std::thread worker([&]{server.listen_after_bind();finished=true;});
  struct Stop{httplib::Server& server;std::thread& worker;~Stop(){server.stop();if(worker.joinable())worker.join();}}stop{server,worker};
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(!server.is_running()&&!finished&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));require(server.is_running(),"owned Polars HTTP server startup failed");httplib::Client client("127.0.0.1",port);client.set_read_timeout(10,0);
  workflows([&](const Parts& parts){httplib::MultipartFormDataItems body;for(const auto& part:parts)body.push_back({part.name,part.bytes,part.file?part.filename:"",part.file?"application/json":"text/plain"});const auto response=client.Post("/api/reference-table-variants",body);require(static_cast<bool>(response),"owned Polars original HTTP request failed");return DiscoveryWorkbenchResponse{response->status,response->body};});
  const auto nonmultipart=client.Post("/api/reference-table-variants","plain","text/plain");require(nonmultipart&&nonmultipart->status==400,"nonmultipart accepted");
  const auto parts=request(between_source("polars"),"membership",call(keywords("admitted_variants",admitted)));httplib::MultipartFormDataItems body;for(const auto& part:parts)body.push_back({part.name,part.bytes,part.file?part.filename:"",part.file?"application/json":"text/plain"});const auto duplicated=client.Post("/api/reference-table-variants?engine=polars",body);require(duplicated&&duplicated->status==400,"duplicate query/multipart engine accepted");
  server.stop();worker.join();std::cout<<"owned original Polars variants HTTP controls passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}return 0;}
