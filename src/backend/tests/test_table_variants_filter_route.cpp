#include "httplib.h"
#include "../tools/table_variants_filter_route.hpp"
#include "table_variants_filter_test_support.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
namespace {
using namespace table_variants_test;
class OwnedServer {
 public:
  OwnedServer() {server.Post("/api/reference-table-variants", handle_table_variants_filter); port = server.bind_to_any_port("127.0.0.1"); require(port > 0, "owned bind failed");
    worker = std::thread([this] {server.listen_after_bind(); finished = true;}); const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!server.is_running() && !finished && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!server.is_running()) {stop(); throw std::runtime_error("owned HTTP server failed");}}
  ~OwnedServer() {stop();} void stop() {server.stop(); if (worker.joinable()) worker.join();}
  httplib::Server server; int port; std::thread worker; std::atomic<bool> finished = false;
};
}
int main() {try {OwnedServer server; httplib::Client client("127.0.0.1", server.port); client.set_read_timeout(5, 0);
  variants_workflows([&](const auto& parts) {httplib::MultipartFormDataItems body; for (const auto& part : parts) body.push_back({part.name, part.bytes, part.file ? part.filename : "", part.file ? "application/json" : "text/plain"});
    const auto response = client.Post("/api/reference-table-variants", body); require(static_cast<bool>(response), "owned request failed"); return DiscoveryWorkbenchResponse{response->status, response->body};});
  const auto response = client.Post("/api/reference-table-variants", "plain", "text/plain"); require(response && response->status == 400, "ordinary body accepted");
  server.stop(); std::cout << "owned table variants HTTP workflows passed\n";
} catch (const std::exception& error) {std::cerr << error.what() << '\n'; return 1;}}
