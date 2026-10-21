#include "httplib.h"
#include "../tools/table_between_filter_route.hpp"
#include "table_between_filter_test_support.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

namespace {
using namespace table_between_test;
constexpr auto route = "/api/reference-table-between";
class OwnedServer {
 public:
  OwnedServer() {
    server.Post(route, handle_table_between_filter); port = server.bind_to_any_port("127.0.0.1"); require(port > 0, "owned socket bind failed");
    worker = std::thread([this] { listened = server.listen_after_bind(); finished = true; });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!server.is_running() && !finished && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!server.is_running()) { stop(); throw std::runtime_error("owned HTTP server failed to start"); }
  }
  ~OwnedServer() { stop(); }
  void stop() { server.stop(); if (worker.joinable()) worker.join(); }
  httplib::Server server; int port = -1; std::thread worker; std::atomic<bool> finished = false, listened = false;
};
httplib::MultipartFormDataItems multipart(const std::vector<DiscoveryWorkbenchPart>& parts) {
  httplib::MultipartFormDataItems result;
  for (const auto& part : parts) result.push_back({part.name, part.bytes, part.file ? part.filename : "", part.file ? "application/json" : "text/plain"});
  return result;
}
DiscoveryWorkbenchResponse received(const httplib::Result& response) {
  require(static_cast<bool>(response), "owned HTTP request failed");
  require(response->get_header_value("Content-Type").starts_with("application/json"), "response MIME type changed");
  return {response->status, response->body};
}
void actual_between_http(httplib::Client& client) {
  for (const std::string engine : {"pandas", "polars"}) {
    const auto source = between_source(engine), expected = direct_between(engine, source);
    success(received(client.Post(route, multipart(between_request(engine, source)))), source, expected, engine);
    const auto duplicate = received(client.Post(std::string(route) + "?engine=" + engine, multipart(between_request(engine, source))));
    require(duplicate.status == 400, "duplicate engine accepted over HTTP"); contains(duplicate, "\"phase\":\"control\"");
    const auto missing = received(client.Post(route, multipart(between_request(engine, source, {field("activity_key", "missing")}))));
    require(missing.status == 400, "missing HTTP activity accepted"); contains(missing, "\"source_unchanged\":true");
  }
  const auto rejected = received(client.Post(route, "ordinary body", "text/plain"));
  require(rejected.status == 400, "nonmultipart accepted"); contains(rejected, "multipart native table File");
}
}  // namespace
int main() { try { OwnedServer server; httplib::Client client("127.0.0.1", server.port); client.set_connection_timeout(3, 0); client.set_read_timeout(3, 0);
  actual_between_http(client); server.stop(); require(server.listened, "owned server listen failed"); std::cout << "owned between table HTTP workflows passed\n"; }
  catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; } }
