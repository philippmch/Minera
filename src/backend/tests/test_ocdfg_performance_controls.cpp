#include "httplib.h"
#include "minera/ocdfg_edge_performance.hpp"
#include "minera/ocdfg_performance_controls.hpp"
#include "../tools/ocdfg_performance_route.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <map>
#include <thread>

namespace {
using namespace minera;

std::optional<OcdfgEdgePerformanceOptions> controls(
    const std::map<std::string, std::string>& input) {
  return parse_ocdfg_performance_controls(
      [&](std::string_view name) { return input.at(std::string(name)); },
      [&](std::string_view name) { return input.contains(std::string(name)); });
}

template <class Action>
void rejects(Action action) {
  bool rejected = false;
  try { action(); } catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
}

void exact_slot_and_presence_controls() {
  assert(!controls({}));
  const auto default_options = controls({{"ocdfg_business_hours", "true"}});
  assert(default_options && default_options->business_hours);
  assert(std::holds_alternative<std::monostate>(default_options->business_hour_slots));
  const auto slots = std::get<std::vector<BusinessHourSlot>>(
      parse_ocdfg_business_hour_slots_json("[[-0,0.25],[20,10],[-1,604801],[1e2,2E2]]"));
  assert(slots.size() == 4 && std::signbit(slots[0].start_seconds));
  assert(slots[0].end_seconds == 0.25 && slots[1].start_seconds == 20 && slots[1].end_seconds == 10);
  assert(slots[2].start_seconds == -1 && slots[2].end_seconds == 604801);
  assert(slots[3].start_seconds == 100 && slots[3].end_seconds == 200);
  assert(std::get<std::vector<BusinessHourSlot>>(parse_ocdfg_business_hour_slots_json(" [ ] ")).empty());
  assert(std::holds_alternative<std::nullptr_t>(parse_ocdfg_business_hour_slots_json(" null\n")));
  const auto literal = controls({{"ocdfg_event_id_attribute", ""},
      {"ocdfg_timestamp_attribute", " ,:\n "}, {"ocdfg_business_hours", "0"}});
  assert(literal && !literal->business_hours && literal->event_id_attribute == "");
  assert(literal->timestamp_attribute == " ,:\n ");
  for (const auto value : {"null", "false", "\"inert calendar\"", "{\"holidays\":[\"2027-02-05\"]}", "[]", "1.25"}) {
    const auto option = controls({{"ocdfg_workcalendar_json", value}});
    assert(option && option->workcalendar_json == value);
  }
  for (const auto value : {"", "[]x", "[1,2]", "[[1]]", "[[1,2,3]]", "[[01,2]]",
                           "[[1.,2]]", "[[+1,2]]", "[[true,2]]", "[[1e999,2]]", "[[1,2],]"})
    rejects([&] { (void)parse_ocdfg_business_hour_slots_json(value); });
  for (const auto value : {"", "null,\"injected\":1", "{]", "false false", "NaN", "\"unterminated"})
    rejects([&] { (void)controls({{"ocdfg_workcalendar_json", value}}); });
  rejects([&] { (void)controls({{"ocdfg_business_hours", "yes"}}); });
}

struct ServerGuard {
  httplib::Server& server;
  std::thread& worker;
  ~ServerGuard() { server.stop(); if (worker.joinable()) worker.join(); }
};

void actual_working_time_http_controls() {
  Ocel source;
  source.events = {{"e1", "depart", "2027-02-05T16:30:00+01:00", {{"", std::string("2027-02-05T16:30:00+01:00")}}},
                   {"e2", "arrive", "2027-02-08T08:30:00+01:00", {{"", std::string("2027-02-08T08:30:00+01:00")}}}};
  source.event_type_attributes["depart"][""] = "datetime";
  source.event_type_attributes["arrive"][""] = "datetime";
  source.objects = {{"o", "Order", {}}};
  source.relations = {{"e1", "o", ""}, {"e2", "o", ""}};
  const auto before = serialize_ocel_json(source);
  auto input = before;
  input.insert(input.size() - 1, ",\"minera:literalNames\":true");
  httplib::Server server;
  server.Post("/api/ocel/discover", [&](const httplib::Request& request, httplib::Response& response) {
    try {
      OcdfgDiscoveryOptions options;
      options.edge_performance_options = ocdfg_performance_options_from_request(request);
      const auto log = parse_ocel_json(request.body);
      response.set_content(serialize_ocdfg_json(discover_ocdfg_classic_with_options(log, options)), "application/json");
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
  const auto post = [&](const httplib::Params& values) {
    std::string query = "/api/ocel/discover?variant=ocdfg&";
    for (const auto& [key, value] : values)
      query += httplib::detail::encode_url(key) + '=' + httplib::detail::encode_url(value) + '&';
    return client.Post(query, input, "application/json");
  };
  const auto check = [&](const httplib::Params& values, double expected) {
    const auto response = post(values);
    if (!response || response->status != 200)
      throw std::runtime_error(response ? response->body : "OCDFG test HTTP request failed");
    const auto graph = parse_ocdfg_json(response->body);
    const auto& edge = graph.edges.at("Order").at({"depart", "arrive"});
    assert(edge.performance == std::vector<double>{expected});
    assert(edge.performance_event_couples == edge.performance && edge.performance_total_objects == edge.performance);
  };
  check({}, 230400);
  check({{"ocdfg_business_hours", "false"}}, 230400);
  check({{"ocdfg_business_hours", "true"}}, 7200);
  check({{"ocdfg_business_hours", "true"}, {"ocdfg_timestamp_attribute", ""}}, 7200);
  check({{"ocdfg_business_hours", "true"}, {"ocdfg_workcalendar_json", "false"}}, 7200);
  check({{"ocdfg_business_hours", "true"}, {"ocdfg_business_hour_slots_json", "[]"}}, 0);
  check({{"ocdfg_business_hours", "false"}, {"ocdfg_business_hour_slots_json", "null"}}, 230400);
  check({{"ocdfg_business_hours", "true"}, {"ocdfg_business_hour_slots_json", "[[0,604800]]"}}, 230400);
  for (const auto& values : std::vector<httplib::Params>{
      {{"ocdfg_business_hours", "true"}, {"ocdfg_business_hour_slots_json", "null"}},
      {{"ocdfg_business_hours", "true"}, {"ocdfg_timestamp_attribute", "missing"}},
      {{"ocdfg_business_hours", "true"}, {"ocdfg_event_id_attribute", ""}},
      {{"ocdfg_business_hour_slots_json", "[[1]]"}},
      {{"ocdfg_workcalendar_json", "{bad}"}}, {{"ocdfg_business_hours", "yes"}}}) {
    const auto response = post(values);
    assert(response && response->status == 400);
  }
  assert(serialize_ocel_json(source) == before);
}
}  // namespace

int main() {
  exact_slot_and_presence_controls();
  actual_working_time_http_controls();
  std::cout << "OCDFG exact slot/presence controls and owned ephemeral HTTP working-time acceptance passed\n";
}
