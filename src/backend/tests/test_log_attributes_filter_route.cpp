#include "httplib.h"
#include "../tools/log_attributes_filter_route.hpp"
#include "log_attributes_filter_test_support.hpp"
#include <chrono>
#include <iostream>
#include <thread>
int main(){httplib::Server server;std::thread worker;try{
  server.Post("/api/reference-log-attributes",handle_log_attributes_filter);const auto port=server.bind_to_any_port("127.0.0.1");log_attributes_test::require(port>0,"owned bind failed");
  worker=std::thread([&]{server.listen_after_bind();});const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
  while(!server.is_running() && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  log_attributes_test::require(server.is_running(),"owned server start failed");httplib::Client client("127.0.0.1",port);client.set_read_timeout(10,0);
  log_attributes_test::workflows([&](const auto& parts){
    // A raw multipart body retains genuine filename="" File presence.
    const std::string boundary="minera-attributes-route-boundary";std::string body;
    for(const auto& part:parts){body+="--"+boundary+"\r\nContent-Disposition: form-data; name=\""+part.name+"\"";
      if(part.file)body+="; filename=\""+part.filename+"\"";body+="\r\n\r\n"+part.bytes+"\r\n";
    }body+="--"+boundary+"--\r\n";
    const auto response=client.Post("/api/reference-log-attributes",body,"multipart/form-data; boundary="+boundary);log_attributes_test::require(static_cast<bool>(response),"owned request failed");return minera::DiscoveryWorkbenchResponse{response->status,response->body};
  });
  const auto ordinary=client.Post("/api/reference-log-attributes","plain","text/plain");log_attributes_test::require(ordinary && ordinary->status==400,"ordinary body accepted");
  server.stop();worker.join();std::cout<<"owned log attributes HTTP workflows passed\n";
}catch(const std::exception& e){server.stop();if(worker.joinable())worker.join();std::cerr<<e.what()<<'\n';return 1;}}
