#define MINERA_PANDAS_VARIANTS_ORIGINAL_ROUTE_TEST
#include "test_pandas_variants_original_call_workbench.cpp"
#include "httplib.h"
#include "../tools/table_variants_filter_route.hpp"
#include <atomic>
#include <chrono>
#include <thread>
int main(){try{
  using namespace pandas_variants_original_test;httplib::Server server;server.Post("/api/reference-table-variants",handle_table_variants_filter);const auto port=server.bind_to_any_port("127.0.0.1");require(port>0,"owned server bind failed");std::atomic<bool> finished=false;std::thread worker([&]{server.listen_after_bind();finished=true;});
  struct Stop{httplib::Server& server;std::thread& worker;~Stop(){server.stop();if(worker.joinable())worker.join();}}stop{server,worker};
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(!server.is_running()&&!finished&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));require(server.is_running(),"owned server startup failed");httplib::Client client("127.0.0.1",port);client.set_read_timeout(10,0);
  workflows([&](const Parts& parts){httplib::MultipartFormDataItems body;for(const auto& p:parts)body.push_back({p.name,p.bytes,p.file?p.filename:"",p.file?"application/json":"text/plain"});const auto r=client.Post("/api/reference-table-variants",body);require(static_cast<bool>(r),"owned original variants HTTP request failed");return DiscoveryWorkbenchResponse{r->status,r->body};});
  const auto plain=client.Post("/api/reference-table-variants","plain","text/plain");require(plain&&plain->status==400,"nonmultipart accepted");
  auto parts=request(between_source("pandas"),"membership",call(R"({"type":"list","values":[]})"));httplib::MultipartFormDataItems body;for(const auto& p:parts)body.push_back({p.name,p.bytes,p.file?p.filename:"",p.file?"application/json":"text/plain"});const auto dup=client.Post("/api/reference-table-variants?engine=pandas",body);require(dup&&dup->status==400,"duplicate query/multipart accepted");
  server.stop();worker.join();std::cout<<"owned original pandas variants HTTP controls passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
