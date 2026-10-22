#include <atomic>
#include <chrono>
#include <functional>
#include <filesystem>
#include <fstream>
#include <optional>
#include <thread>
#include <vector>
#include "httplib.h"
#include "../tools/log_variants_filter_route.hpp"
#include "minera/literal_controls_json.hpp"
#include "minera/xes_export.hpp"

// Reuse the original complete oracle without changing a byte of its source.
// Its renamed main is never invoked: falling through a renamed int function
// would not have main's implicit return. retained_public_variants_filter_cases() runs its assertions.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4715)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#endif
#define main log_variants_oracle_unused_entrypoint
#include "test_log_variants_filter.cpp"
#undef main
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace {
#include "log_variants_workbench_public_vectors.inc"

class LogVariantsOriginalCallHttpServer {
 public:
  LogVariantsOriginalCallHttpServer() {
    server_.Post("/api/reference-log-variants", handle_log_variants_filter);
    port_ = server_.bind_to_any_port("127.0.0.1");
    if (port_ <= 0) throw std::runtime_error("LOG variants original-call owned HTTP bind failed");
    worker_ = std::thread([this] { listened_ = server_.listen_after_bind(); finished_ = true; });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!server_.is_running() && !finished_ && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!server_.is_running()) { stop(); throw std::runtime_error("LOG owned HTTP did not start"); }
  }
  ~LogVariantsOriginalCallHttpServer() { stop(); }
  void stop() { server_.stop(); if (worker_.joinable()) worker_.join(); }
  int port() const { return port_; }
  bool listened() const { return listened_; }
 private:
  httplib::Server server_;
  int port_ = -1;
  std::thread worker_;
  std::atomic<bool> finished_ = false, listened_ = false;
};
DiscoveryWorkbenchResponse log_variants_original_call_http(httplib::Client& client,
    const std::vector<DiscoveryWorkbenchPart>& parts) {
  const std::string boundary = "minera-log-variants-original-call-retained-public-boundary";
  std::string body;
  for (const auto& part : parts) {
    body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + part.name + '"';
    if (part.file) body += "; filename=\"" + part.filename + '"';
    body += "\r\nContent-Type: application/octet-stream\r\n\r\n" + part.bytes + "\r\n";
  }
  body += "--" + boundary + "--\r\n";
  const auto response = client.Post("/api/reference-log-variants", body, "multipart/form-data; boundary=" + boundary);
  if (!response || !response->get_header_value("Content-Type").starts_with("application/json"))
    throw std::runtime_error("LOG variants original-call owned HTTP response missing");
  return {response->status, response->body};
}
}

int main(int argc, char** argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--emit-native-source") {
      const auto root = std::filesystem::canonical(std::filesystem::current_path());
      if (!std::filesystem::is_regular_file(root / "CMakeLists.txt") ||
          !std::filesystem::is_regular_file(root / "include/minera/log_variants_filter.hpp"))
        throw std::runtime_error("Native source emission requires repository-root cwd");
      const auto parent = root / "build/parity-closures";
      if (std::filesystem::canonical(parent) != parent)
        throw std::runtime_error("Native source output parent must be canonical repository path");
      const auto folder = parent / "log-variants-original-call-browser-input-1";
      if (!std::filesystem::create_directory(folder))
        throw std::runtime_error("Preserve existing native-source emission output");
      const auto output = folder / "source.native.json";
      if (std::filesystem::exists(output)) throw std::runtime_error("Native source output exists");
      const auto bytes = serialize_embedding_workbench_raw_log_json(snapshot_0());
      std::ofstream stream(output, std::ios::binary);
      stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));stream.flush();
      if (!stream) throw std::runtime_error("Native source emission failed");
      stream.close();
      std::cout << "Emitted create-once original snapshot_0 native source; no filters executed\n";
      return 0;
    }
    if (argc != 1) throw std::runtime_error("Unknown LOG variants test utility arguments");
    retained_public_variants_filter_cases();
    native_variants_filter_storage_controls();
    retained_log_variants_workbench_cases([](const auto& parts) { return execute_log_variants_filter(parts); });
    LogVariantsOriginalCallHttpServer owned;
    httplib::Client client("127.0.0.1", owned.port());
    client.set_connection_timeout(5, 0); client.set_read_timeout(5, 0);
    retained_log_variants_workbench_cases([&](const auto& parts) { return log_variants_original_call_http(client, parts); });
    owned.stop();
    if (!owned.listened()) throw std::runtime_error("LOG variants original-call owned HTTP listener failed");
    std::cout << "All 380 original LOG variants calls passed direct workbench and owned HTTP transport\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
