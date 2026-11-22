#undef NDEBUG
// Review FX1: /api/footprints-workbench discovery on the frame shapes of an XES
// upload follows pm4py's footprints variants on read_xes's frame
// (entire_dataframe) and its Polars copy (polars_lazyframes). read_xes's frame
// has no column for an attribute no event carries, so the pandas variant's
// column selection raises KeyError and the Polars variant names the missing
// columns; a frame without events has no column at all. An unnamed trace is a
// NaN case: pandas groups leave it out (its activities still count), Polars
// keeps it as one null group. Expected values are pm4py 2.7.22 black-box on
// the same uploads (review-harness round-fx, fpcmp.py on st/*.xes).
//
// Usage: minera_server_footprints_frames_tests <minera_server>

#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "httplib.h"

extern char** environ;

namespace {

int g_failures = 0;

void check(const std::string& label, const std::string& seen, const std::string& expected) {
  if (seen == expected) return;
  ++g_failures;
  std::cerr << "FAIL: " << label << "\n  seen     " << seen << "\n  expected " << expected << '\n';
}

int free_port() {
  if (const char* pinned = std::getenv("MINERA_TEST_SERVER_PORT"); pinned && *pinned)
    return std::atoi(pinned);
  const int socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;
  if (::bind(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0) std::abort();
  socklen_t length = sizeof address;
  ::getsockname(socket_fd, reinterpret_cast<sockaddr*>(&address), &length);
  const int port = ntohs(address.sin_port);
  ::close(socket_fd);
  return port;
}

const std::string kHead =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
    "<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n";

std::string event(const std::string& activity, const std::string& minute) {
  std::string out = "<event>";
  if (!activity.empty()) out += "<string key=\"concept:name\" value=\"" + activity + "\"/>";
  if (!minute.empty()) out += "<date key=\"time:timestamp\" value=\"2021-03-01T08:" + minute + ":00.000+00:00\"/>";
  return out + "</event>";
}

std::string trace(const std::string& name, const std::string& events) {
  return "<trace>" + (name.empty() ? std::string() : "<string key=\"concept:name\" value=\"" + name + "\"/>") + events +
         "</trace>\n";
}

const std::string kModel =
    "{\"sequence\":[{\"source\":\"A\",\"target\":\"B\"}],\"activities\":[\"A\",\"B\"],\"start_activities\":[\"A\"],"
    "\"end_activities\":[\"B\"],\"min_trace_length\":2,\"activities_always_happening\":[\"A\",\"B\"]}";

// The discovery call's error message (JSON text), or the observed footprint's
// fields after "value":{ when discovery returned.
std::string discovery(httplib::Client& client, const std::string& log, const std::string& variant,
                      const std::string& shape) {
  httplib::MultipartFormDataItems items = {
      {"operation", "log_extensive", "", ""},
      {"discovery_options_json", "{}", "", ""},
      {"observed_mode", "source", "", ""},
      {"discovery_variant", variant, "", ""},
      {"mining_input_shape", shape, "", ""},
      {"include_metrics", "false", "", ""},
      {"source_format", "import", "", ""},
      {"mining_input_format", "auto", "", ""},
      {"mining_selectors_json", "{}", "", ""},
      {"model_file", kModel, "model.fp.json", "application/json"},
      {"source_file", log, "fx1.xes", "application/xml"},
  };
  const auto result = client.Post("/api/footprints-workbench", items);
  if (!result) return "no response";
  const std::string& body = result->body;
  const auto call = body.find("\"name\":\"discover_footprints(");
  if (call == std::string::npos) return "no discovery call: " + body.substr(0, 300);
  const auto end_of_call = body.find("},{\"name\"", call);
  const std::string call_text = body.substr(call, end_of_call == std::string::npos ? 600 : end_of_call - call);
  if (const auto message = call_text.find("\"message\":\""); message != std::string::npos) {
    const auto start = message + 11;
    const auto stop = call_text.find("\",\"exception_type\"", start);
    return "ERR " + call_text.substr(start, stop - start);
  }
  const auto value = body.find("\"type\":\"footprint\",\"value\":{");
  if (value == std::string::npos) return "no observed value: " + body.substr(0, 300);
  const auto start = value + 28;
  return body.substr(start, body.find("}}", start) - start);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " <minera_server>\n";
    return 2;
  }
  const int port = free_port();
  const std::string port_text = std::to_string(port);
  std::vector<std::string> args = {argv[1], "--port", port_text, "--frontend", "/nonexistent-minera-frontend"};
  std::vector<char*> raw;
  for (auto& arg : args) raw.push_back(arg.data());
  raw.push_back(nullptr);
  pid_t server = 0;
  if (::posix_spawn(&server, argv[1], nullptr, nullptr, raw.data(), environ) != 0) {
    std::cerr << "cannot start " << argv[1] << '\n';
    return 2;
  }
  httplib::Client client("127.0.0.1", port);
  client.set_read_timeout(120, 0);
  bool ready = false;
  for (int attempt = 0; attempt < 200 && !ready; ++attempt) {
    if (auto result = client.Get("/api/version"); result && result->status == 200) ready = true;
    else std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  if (!ready) {
    std::cerr << "FAIL: server answers /api/version\n";
    ::kill(server, SIGTERM);
    return 1;
  }

  const std::string no_timestamps =
      kHead + trace("c1", event("a", "") + event("b", "")) + trace("c2", event("b", "")) + "</log>\n";
  const std::string no_case =
      kHead + trace("", event("a", "01") + event("b", "02")) + trace("", event("b", "03")) + "</log>\n";
  const std::string no_events = kHead + trace("c1", "") + "</log>\n";
  const std::string gaps = kHead + trace("", event("a", "01") + event("b", "02")) +
                           trace("c2", event("c", "03") + event("", "04") + event("d", "05")) + "</log>\n";

  for (const std::string shape : {"dataframe", "lazyframe"}) {
    const std::string at = " (" + shape + " shape)";
    check("pandas, no event has a timestamp" + at, discovery(client, no_timestamps, "entire_dataframe", shape),
          "ERR KeyError: \\\"['time:timestamp'] not in index\\\"");
    check("Polars, no event has a timestamp" + at, discovery(client, no_timestamps, "polars_lazyframes", shape),
          "ERR The provided Polars LazyFrame does not contain the following required columns: time:timestamp");
    check("pandas, no trace has a name" + at, discovery(client, no_case, "entire_dataframe", shape),
          "ERR KeyError: \\\"['case:concept:name'] not in index\\\"");
    check("Polars, no trace has a name" + at, discovery(client, no_case, "polars_lazyframes", shape),
          "ERR The provided Polars LazyFrame does not contain the following required columns: case:concept:name");
    check("pandas, no events" + at, discovery(client, no_events, "entire_dataframe", shape),
          "ERR KeyError: \\\"None of [Index(['case:concept:name', 'concept:name', 'time:timestamp'], "
          "dtype='str')] are in the [columns]\\\"");
    check("Polars, no events" + at, discovery(client, no_events, "polars_lazyframes", shape),
          "ERR The provided Polars LazyFrame does not contain the following required columns: case:concept:name, "
          "concept:name, time:timestamp");
    check("pandas, an unnamed trace and an unnamed event" + at, discovery(client, gaps, "entire_dataframe", shape),
          "\"dfg\":[],\"dfg_present\":true,\"sequence\":[],\"parallel\":[],\"atomic_interleaving\":[],"
          "\"start_activities\":[\"c\"],\"end_activities\":[\"d\"],\"start_activities_present\":true,"
          "\"end_activities_present\":true,\"activities\":[\"a\",\"b\",\"c\",\"d\"],\"skippable\":null,"
          "\"activities_always_happening\":null,\"min_trace_length\":3,\"min_trace_length_present\":true,"
          "\"max_trace_length_without_loops\":null,\"trace\":null,\"activities_include_null\":true");
    check("Polars, an unnamed trace and an unnamed event" + at, discovery(client, gaps, "polars_lazyframes", shape),
          "\"dfg\":[],\"dfg_present\":true,\"sequence\":[],\"parallel\":[],\"atomic_interleaving\":[],"
          "\"start_activities\":[\"a\",\"c\"],\"end_activities\":[\"b\",\"d\"],\"start_activities_present\":true,"
          "\"end_activities_present\":true,\"activities\":[\"a\",\"b\",\"c\",\"d\"],\"skippable\":null,"
          "\"activities_always_happening\":null,\"min_trace_length\":2,\"min_trace_length_present\":true,"
          "\"max_trace_length_without_loops\":null,\"trace\":null,\"activities_include_null\":true");
  }

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures != 0) {
    std::cerr << g_failures << " footprints frame check(s) failed\n";
    return 1;
  }
  std::cout << "review FX1: footprints discovery on XES upload frames passed\n";
  return 0;
}
