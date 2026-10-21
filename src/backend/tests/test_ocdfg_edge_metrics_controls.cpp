#include "httplib.h"
#include "minera/ocdfg_edge_metrics_json.hpp"
#include "../tools/ocdfg_edge_metrics_route.hpp"

#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

namespace {
using namespace minera;

void exact_report_serialization() {
  const OcdfgEdgeAssociations source = {{"Order", {{{"A", "B"}, {
      {"e0", "e1", "o1"}, {"e0", "e1", "o0"}, {"e0", "e1", "o1"}}}}}};
  const auto before = source;
  assert(serialize_ocdfg_edge_metrics_json(source) ==
      R"({"object_types":[{"object_type":"Order","edges":[{"source_activity":"A","target_activity":"B","associations":[["e0","e1","o1"],["e0","e1","o0"],["e0","e1","o1"]],"event_couples":[["e0","e1"]],"total_objects":[["e0","e1","o0"],["e0","e1","o1"]],"unique_objects":["o0","o1"]}]}]})");
  assert(source == before);
  assert(serialize_ocdfg_edge_metrics_json({}) == R"({"object_types":[]})");
  const OcdfgEdgeAssociations literal = {{"", {{{"a->b", "c\n\""}, {{"", "\\", "o"}}}}}};
  const auto json = serialize_ocdfg_edge_metrics_json(literal);
  const std::string expected_literal = R"({"object_types":[{"object_type":"","edges":[{"source_activity":"a->b","target_activity":"c\n\"","associations":[["","\\","o"]],"event_couples":[["","\\"]],"total_objects":[["","\\","o"]],"unique_objects":["o"]}]}]})";
  assert(json == expected_literal);
}

struct ServerGuard {
  httplib::Server& server;
  std::thread& worker;
  ~ServerGuard() { server.stop(); if (worker.joinable()) worker.join(); }
};

void actual_edge_metrics_http_controls() {
  Ocel source;
  source.events = {{"e0", "A", "", {{"", std::string("x0")}, {"alternate label", std::string("start")}}},
                   {"e1", "B", "", {{"", std::string("x1")}, {"alternate label", std::string("finish")}}}};
  source.objects = {{"o", "Order", {{"object key", std::string("alternate object")},
                                      {"type key", std::string("alternate type")}}}};
  source.relations = {{"e0", "o", ""}, {"e1", "o", ""}};
  const auto before = serialize_ocel_json(source);
  auto input = before;
  input.insert(input.size() - 1, ",\"minera:literalNames\":true");
  httplib::Server server;
  server.Post("/edge-metrics-test", [&](const httplib::Request& request, httplib::Response& response) {
    try {
      response.set_content(ocdfg_edge_metrics_report_from_request(parse_ocel_json(request.body), request), "application/json");
    } catch (const std::exception& error) {
      response.status = 400;
      response.set_content(error.what(), "text/plain");
    }
  });
  const auto port = server.bind_to_any_port("127.0.0.1");
  assert(port > 0);
  std::thread worker([&] { server.listen_after_bind(); });
  ServerGuard guard{server, worker};
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (!server.is_running() && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(server.is_running());
  httplib::Client client("127.0.0.1", port);
  client.set_read_timeout(3, 0);
  const auto post = [&](const httplib::Params& params) {
    std::string query = "/edge-metrics-test?";
    for (const auto& [key, value] : params)
      query += httplib::detail::encode_url(key) + '=' + httplib::detail::encode_url(value) + '&';
    return client.Post(query, input, "application/json");
  };
  const auto ordinary = post({});
  assert(ordinary && ordinary->status == 200);
  assert(ordinary->body == R"({"object_types":[{"object_type":"Order","edges":[{"source_activity":"A","target_activity":"B","associations":[["e0","e1","o"]],"event_couples":[["e0","e1"]],"total_objects":[["e0","e1","o"]],"unique_objects":["o"]}]}]})");
  const auto selected = post({{"ocdfg_edge_metrics_event_id_key", ""},
      {"ocdfg_edge_metrics_object_id_key", "object key"},
      {"ocdfg_edge_metrics_object_type_key", "type key"},
      {"ocdfg_edge_metrics_activity_key", "alternate label"},
      {"ocdfg_edge_metrics_timestamp_key", "missing and inactive"}});
  assert(selected && selected->status == 200);
  assert(selected->body == R"({"object_types":[{"object_type":"alternate type","edges":[{"source_activity":"start","target_activity":"finish","associations":[["x0","x1","alternate object"]],"event_couples":[["x0","x1"]],"total_objects":[["x0","x1","alternate object"]],"unique_objects":["alternate object"]}]}]})");
  const auto missing = post({{"ocdfg_edge_metrics_event_id_key", "missing"}});
  assert(missing && missing->status == 400 && missing->body == "'missing'");
  assert(serialize_ocel_json(source) == before);
}

}  // namespace

int main() {
  exact_report_serialization();
  actual_edge_metrics_http_controls();
  std::cout << "OCDFG edge metrics: literal-safe report JSON and owned HTTP selector controls passed\n";
}
