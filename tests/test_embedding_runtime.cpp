#include "embedding_runtime.hpp"

#include <cassert>
#include <chrono>
#include <string>
#include <thread>

int main() {
  using minera::embedding_runtime::detail::parse_response;
  using minera::embedding_runtime::detail::validate_url;

  validate_url("http://127.0.0.1:18080");
  validate_url("http://[::1]:18080");
  for (const auto* invalid : {"https://127.0.0.1:1", "http://", "http://host/path",
                              "http://host:0", "http://host:65536"}) {
    bool rejected = false;
    try {
      validate_url(invalid);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }

  const auto values = parse_response(R"({"embedding":[1,-2.5,3e-1]})", 3);
  assert(values.size() == 3 && values[0] == 1.0 && values[1] == -2.5 &&
         values[2] == 0.3);
  for (const auto* invalid : {R"({"embedding":[]})",
                              R"({"embedding":[1,2]})",
                              R"({"embedding":[1,2,3,4]})",
                              R"({"embedding":[1,NaN,3]})",
                              R"({"embedding":[1,2,3],"extra":1})"}) {
    bool rejected = false;
    try {
      (void)parse_response(invalid, 3);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }

  httplib::Server server;
  std::string received;
  server.Post("/embed", [&](const httplib::Request& request,
                             httplib::Response& response) {
    received = request.body;
    response.set_content(R"({"embedding":[0.25,-0.5]})", "application/json");
    server.stop();
  });
  const auto port = server.bind_to_any_port("127.0.0.1");
  assert(port > 0);
  std::thread worker([&] { assert(server.listen_after_bind()); });
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::seconds(1);
  while (!server.is_running() &&
         std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  if (!server.is_running()) {
    server.stop();
    worker.join();
    return 1;
  }
  const auto callback = minera::embedding_runtime::make_http_provider(
      "http://127.0.0.1:" + std::to_string(port));
  const auto text = std::string("quote \" and newline\nutf8 ") + "\xC3\xA4";
  const auto result = callback(text, "model\\name", 2);
  worker.join();
  assert(result.size() == 2 && result[0] == 0.25 && result[1] == -0.5);
  assert(received.find("\"text\":\"quote \\\" and newline\\nutf8 ") !=
         std::string::npos);
  assert(received.find(R"("model":"model\\name")") != std::string::npos);
  return 0;
}
