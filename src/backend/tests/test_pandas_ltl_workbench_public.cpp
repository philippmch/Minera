#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <thread>
#include <vector>
#include "httplib.h"
#include "../tools/ltl_filter_route.hpp"
#include "minera/literal_controls_json.hpp"

// Reuse the original complete oracle without changing a byte of its source.
// Its renamed main is never invoked: falling through a renamed int function
// would not have main's implicit return. retained_cases() runs its assertions.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4715)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#endif
#define main pandas_oracle_unused_entrypoint
#include "test_pandas_ltl_filter.cpp"
#undef main
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace {
#include "pandas_ltl_workbench_public_vectors.inc"

class PandasOriginalCallHttpServer {
 public:
  PandasOriginalCallHttpServer() {
    server_.Post("/api/reference-ltl-filter", handle_reference_ltl_filter);
    port_ = server_.bind_to_any_port("127.0.0.1");
    if (port_ <= 0) throw std::runtime_error("pandas original-call owned HTTP bind failed");
    worker_ = std::thread([this] { listened_ = server_.listen_after_bind(); finished_ = true; });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!server_.is_running() && !finished_ && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!server_.is_running()) { stop(); throw std::runtime_error("pandas owned HTTP did not start"); }
  }
  ~PandasOriginalCallHttpServer() { stop(); }
  void stop() { server_.stop(); if (worker_.joinable()) worker_.join(); }
  int port() const { return port_; }
  bool listened() const { return listened_; }
 private:
  httplib::Server server_;
  int port_ = -1;
  std::thread worker_;
  std::atomic<bool> finished_ = false, listened_ = false;
};
DiscoveryWorkbenchResponse pandas_original_call_http(httplib::Client& client,
    const std::vector<DiscoveryWorkbenchPart>& parts) {
  const std::string boundary = "minera-pandas-original-call-retained-public-boundary";
  std::string body;
  for (const auto& part : parts) {
    body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + part.name + '"';
    if (part.file) body += "; filename=\"" + part.filename + '"';
    body += "\r\nContent-Type: application/octet-stream\r\n\r\n" + part.bytes + "\r\n";
  }
  body += "--" + boundary + "--\r\n";
  const auto response = client.Post("/api/reference-ltl-filter", body, "multipart/form-data; boundary=" + boundary);
  if (!response || !response->get_header_value("Content-Type").starts_with("application/json"))
    throw std::runtime_error("pandas original-call owned HTTP response missing");
  return {response->status, response->body};
}
}

int main() {
  try {
    retained_cases();
    retained_pandas_ltl_workbench_cases([](const auto& parts) { return execute_ltl_filter_workbench(parts); });
    PandasOriginalCallHttpServer owned;
    httplib::Client client("127.0.0.1", owned.port());
    client.set_connection_timeout(5, 0); client.set_read_timeout(5, 0);
    retained_pandas_ltl_workbench_cases([&](const auto& parts) { return pandas_original_call_http(client, parts); });
    owned.stop();
    if (!owned.listened()) throw std::runtime_error("pandas original-call owned HTTP listener failed");
    std::cout << "All 429 original pandas calls passed direct workbench and owned HTTP transport\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
