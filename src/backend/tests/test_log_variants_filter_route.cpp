#include "httplib.h"
#include "../tools/log_variants_filter_route.hpp"
#define main log_variants_workbench_direct_main_not_invoked
#include "test_log_variants_filter_workbench.cpp"
#undef main
#include <chrono>
#include <thread>
int main(){httplib::Server server;std::thread worker;try{
  using namespace log_variants_workbench_test;
  server.Post("/api/reference-log-variants",handle_log_variants_filter);const auto port=server.bind_to_any_port("127.0.0.1");require(port>0,"owned bind failed");
  worker=std::thread([&]{server.listen_after_bind();});const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(!server.is_running()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));require(server.is_running(),"owned listener failed");
  httplib::Client client("127.0.0.1",port);client.set_read_timeout(10,0);
  workflows([&](const Parts& parts){const std::string boundary="owned-log-variants-original-boundary";std::string body;for(const auto& part:parts){body+="--"+boundary+"\r\nContent-Disposition: form-data; name=\""+part.name+"\"";if(part.file)body+="; filename=\""+part.filename+"\"";body+="\r\n\r\n"+part.bytes+"\r\n";}body+="--"+boundary+"--\r\n";const auto response=client.Post("/api/reference-log-variants",body,"multipart/form-data; boundary="+boundary);require(static_cast<bool>(response),"owned LOG variants HTTP request failed");return minera::DiscoveryWorkbenchResponse{response->status,response->body};});
  const auto body=client.Post("/api/reference-log-variants","plain","text/plain");require(body&&body->status==400,"ordinary body accepted");
  const auto queried=client.Post("/api/reference-log-variants?operation=apply","plain","text/plain");require(queried&&queried->status==400,"query body accepted");
  server.stop();worker.join();std::cout<<"owned original LOG variants HTTP workflow passed\n";
}catch(const std::exception& e){server.stop();if(worker.joinable())worker.join();std::cerr<<e.what()<<'\n';return 1;}}
