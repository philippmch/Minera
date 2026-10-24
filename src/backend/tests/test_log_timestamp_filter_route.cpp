#include "httplib.h"
#include "../tools/log_timestamp_filter_route.hpp"
#define main log_timestamp_workbench_direct_main_not_invoked
#include "test_log_timestamp_filter_workbench.cpp"
#undef main
#include <chrono>
#include <thread>

int main() {
  httplib::Server server; std::thread worker;
  try {
    using namespace log_timestamp_workbench_test;
    server.Post("/api/reference-log-timestamp", handle_log_timestamp_filter);
    const auto port = server.bind_to_any_port("127.0.0.1"); require(port > 0, "owned bind failed");
    worker = std::thread([&] { server.listen_after_bind(); });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!server.is_running() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    require(server.is_running(), "owned listener failed");
    httplib::Client client("127.0.0.1", port); client.set_read_timeout(10, 0);
    workflows([&](const Parts& parts) {
      const std::string boundary = "owned-log-timestamp-original-boundary"; std::string body;
      for (const auto& part : parts) {
        body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + part.name + "\"";
        if (part.file) body += "; filename=\"" + part.filename + "\"";
        body += "\r\n\r\n" + part.bytes + "\r\n";
      }
      body += "--" + boundary + "--\r\n";
      const auto response = client.Post("/api/reference-log-timestamp", body, "multipart/form-data; boundary=" + boundary);
      require(static_cast<bool>(response), "owned HTTP request failed");
      return DiscoveryWorkbenchResponse{response->status, response->body};
    });
    const auto plain = client.Post("/api/reference-log-timestamp", "plain", "text/plain"); require(plain && plain->status == 400, "ordinary body accepted");
    const auto query = client.Post("/api/reference-log-timestamp?operation=apply", "plain", "text/plain"); require(query && query->status == 400, "query accepted");
    server.stop(); worker.join(); std::cout << "owned LOG timestamp HTTP workflow passed\n";
  } catch (const std::exception& error) { server.stop(); if (worker.joinable()) worker.join(); std::cerr << error.what() << '\n'; return 1; }
}
