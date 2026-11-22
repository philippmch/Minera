#undef NDEBUG
// Review GE: token replay of a session's POWL model and Heuristics++ on XES
// upload frames. pm4py's POWL net names its places p_N (source and sink
// first) and its silent transitions <kind>_N, and its token replay breaks
// routing ties by those names; its shortest silent paths follow the arcs in
// the order the converter made them. Heuristics++ pairs a frame's lifecycle
// events into intervals case by case (Polars' cases in first-row order, the
// null case included): a complete takes the first open start of its
// activity, a timeless start or other transition forms no interval, and the
// first pair with a missing time raises (Polars' subtraction TypeError,
// pandas' NaT ValueError after the scan); a missing lifecycle raises its
// AttributeError when the scan reaches it (pandas: before the scan).
// Expected values are pm4py 2.7.22 black-box on the same uploads
// (review-harness round-ge, ct/totals.py and ct/hpp.py).
//
// Usage: minera_server_mine_xes_frames_ge_tests <minera_server>

#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
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

// An XES upload from a compact spec: cases split by '|', each "case:events",
// an event "activity[@minute][/lifecycle]" ("_" for no activity), an empty
// case name for a trace without one.
std::string xes(const std::string& spec) {
  std::string out =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
      "<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n";
  std::stringstream cases(spec);
  std::string item;
  while (std::getline(cases, item, '|')) {
    const auto colon = item.find(':');
    const auto name = item.substr(0, colon);
    out += "<trace>";
    if (!name.empty()) out += "<string key=\"concept:name\" value=\"" + name + "\"/>";
    std::stringstream events(item.substr(colon + 1));
    std::string token;
    while (events >> token) {
      std::string lifecycle, minute;
      if (const auto slash = token.find('/'); slash != std::string::npos) {
        lifecycle = token.substr(slash + 1);
        token.resize(slash);
      }
      if (const auto at = token.find('@'); at != std::string::npos) {
        minute = token.substr(at + 1);
        if (minute.size() < 2) minute = "0" + minute;
        token.resize(at);
      }
      out += "<event>";
      if (token != "_") out += "<string key=\"concept:name\" value=\"" + token + "\"/>";
      if (!minute.empty()) out += "<date key=\"time:timestamp\" value=\"2021-03-01T08:" + minute + ":00.000+00:00\"/>";
      if (!lifecycle.empty()) out += "<string key=\"lifecycle:transition\" value=\"" + lifecycle + "\"/>";
      out += "</event>";
    }
    out += "</trace>\n";
  }
  return out + "</log>\n";
}

// The JSON object that starts at `open` (a '{'), strings and escapes skipped.
std::string object_at(const std::string& body, std::size_t open) {
  int depth = 0;
  bool in_string = false;
  for (std::size_t i = open; i < body.size(); ++i) {
    const char c = body[i];
    if (in_string) {
      if (c == '\\') ++i;
      else if (c == '"') in_string = false;
    } else if (c == '"') {
      in_string = true;
    } else if (c == '{') {
      ++depth;
    } else if (c == '}' && --depth == 0) {
      return body.substr(open, i - open + 1);
    }
  }
  return {};
}

// The JSON string value after `"key":"` in `entry` (escapes decoded).
std::string string_after(const std::string& entry, const std::string& key) {
  const auto at = entry.find("\"" + key + "\":\"");
  if (at == std::string::npos) return {};
  std::string out;
  for (std::size_t i = at + key.size() + 4; i < entry.size() && entry[i] != '"'; ++i) {
    if (entry[i] != '\\') {
      out += entry[i];
      continue;
    }
    const char escaped = entry[++i];
    out += escaped == 'n' ? '\n' : escaped == 't' ? '\t' : escaped;
  }
  return out;
}

// The family's outcome: "OK" when it ran, else its error as "Type: message".
std::string family_outcome(const std::string& body, const std::string& name) {
  const auto results = body.find("\"family_results\":{");
  if (results == std::string::npos) return "no family_results: " + body.substr(0, 300);
  const auto at = body.find("\"" + name + "\":{", results);
  if (at == std::string::npos) return "no family entry";
  const auto entry = object_at(body, body.find('{', at));
  if (entry.find("\"executed\":true") != std::string::npos) return "OK";
  auto type = string_after(entry, "type");
  if (const auto dot = type.rfind('.'); dot != std::string::npos) type = type.substr(dot + 1);
  return type + ": " + string_after(entry, "message");
}

std::string mine(httplib::Client& client, const std::string& log, const std::string& shape,
                 const std::vector<std::pair<std::string, std::string>>& fields = {}) {
  httplib::MultipartFormDataItems items = {
      {"include_traces", "false", "", ""},
      {"mining_input_shape", shape, "", ""},
      {"log", log, "ge1.xes", "application/xml"},
  };
  for (const auto& [name, value] : fields) items.push_back({name, value, "", ""});
  const auto result = client.Post("/api/mine", items);
  return result ? result->body : std::string("no response");
}

// The totals of the POWL model's token replay in the mine's analysis:
// produced, consumed, missing, remaining, fitting traces, traces.
std::string powl_totals(const std::string& body) {
  const auto at = body.find("\"powl\": {\"sound\": ");
  if (at == std::string::npos) return "no powl conformance: " + family_outcome(body, "powl_conformance");
  const auto model = object_at(body, body.find('{', at));
  const auto replay_at = model.find("\"token_replay\": {");
  if (replay_at == std::string::npos) return "no token_replay";
  const auto replay = object_at(model, model.find('{', replay_at));
  std::string out;
  for (const std::string key : {"produced", "consumed", "missing", "remaining", "fitting_traces", "trace_count"}) {
    const std::string needle = "\"" + key + "\": ";
    const auto found = replay.find(needle);
    const auto from = found == std::string::npos ? replay.size() : found + needle.size();
    out += (out.empty() ? "" : ",") + replay.substr(from, replay.find_first_of(",}", from) - from);
  }
  return out;
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

  {  // POWL token replay totals: ties broken by pm4py's place names and arc order.
    struct Case {
      std::string name, spec, dataframe, lazyframe;
    };
    const std::vector<Case> cases = {
        // pm4py's place names break the silent routing tie.
        {"lifecycle rows of one case",
         "c10:e/start e@47/suspend e@47/COMPLETE d@39/COMPLETE e@44/suspend a@37/COMPLETE d@32/complete e/start",
         "15,15,1,1,0,1", "20,20,1,1,0,1"},
        // A case name used twice, its last row timeless.
        {"a repeated case name", "c10:|c1:a@14 b@19 a@20|c1:b", "8,8,0,0,1,1", "7,7,2,2,0,1"},
        {"timeless rows", "c1:b@5 d b@9 b e@9 c|c11:e@42|c12:c|c13:d@37|c14:b e@54 b a@59|c15:d|c16:e",
         "91,91,1,1,6,7", "90,89,2,3,5,7"},
        // Two shortest silent paths of equal length: pm4py follows the arcs as made.
        {"rows out of time order",
         "c10:h@21|c11:c@27 d@29 c@33 g@36 d@37 i@40 e@45 g@47|c12:i@23 d@19 g@21|c13:g@2|c14:|c15:g@6|"
         "c16:c@9 g@0 h@4 g@10 a@9 e@9 b@2 i@3 i@5 b@3 h@12 k@9 a@8 k@6|c17:|c18:|c19:d@41|"
         "c20:e@24 j@21 c@42 d@32 c@26 e@37 f@31",
         "362,358,6,10,6,8", "257,255,5,7,6,8"},
    };
    for (const auto& item : cases) {
      check("POWL token replay totals, " + item.name + " (dataframe shape)",
            powl_totals(mine(client, xes(item.spec), "dataframe")), item.dataframe);
      check("POWL token replay totals, " + item.name + " (lazyframe shape)",
            powl_totals(mine(client, xes(item.spec), "lazyframe")), item.lazyframe);
    }
  }

  {  // Heuristics++'s intervals on frames with lifecycle and time gaps.
    struct Case {
      std::string name, spec, dataframe, lazyframe;
    };
    const std::string nat = "ValueError: NaTType does not support timestamp";
    const std::string pandas_lower = "AttributeError: 'float' object has no attribute 'lower'";
    const std::string none_minus_none = "TypeError: unsupported operand type(s) for -: 'NoneType' and 'NoneType'";
    const std::vector<Case> cases = {
        // An open start or another transition forms no interval: its time is never read.
        {"a timeless start without its complete", "c1:a/start b@2/complete", "OK", "OK"},
        {"a timeless suspend", "c1:a/suspend b@2/complete", "OK", "OK"},
        {"a timeless complete after its start", "c1:a@1/start a/complete", nat,
         "TypeError: unsupported operand type(s) for -: 'NoneType' and 'datetime.datetime'"},
        {"a timeless start before its complete", "c1:a/start a@2/complete", nat,
         "TypeError: unsupported operand type(s) for -: 'datetime.datetime' and 'NoneType'"},
        // A complete takes the first open start (FIFO).
        {"two starts, the first complete timeless", "c1:a@1/start a@2/start a/complete a@4/complete", nat,
         "TypeError: unsupported operand type(s) for -: 'NoneType' and 'datetime.datetime'"},
        // Polars scans the cases in first-row order: the first problem raises.
        {"a missing lifecycle in the first case", "c2:b@2|c1:a/complete", pandas_lower,
         "AttributeError: 'NoneType' object has no attribute 'lower'"},
        {"a timeless complete in the first case", "c2:a/complete|c1:b@2", pandas_lower, none_minus_none},
        {"a timeless complete before a missing lifecycle", "c1:a/complete b@2", pandas_lower, none_minus_none},
    };
    for (const auto& item : cases) {
      const std::vector<std::pair<std::string, std::string>> plusplus = {{"heuristic_variant", "plusplus"}};
      check("Heuristics++, " + item.name + " (dataframe shape)",
            family_outcome(mine(client, xes(item.spec), "dataframe", plusplus), "heuristic"), item.dataframe);
      check("Heuristics++, " + item.name + " (lazyframe shape)",
            family_outcome(mine(client, xes(item.spec), "lazyframe", plusplus), "heuristic"), item.lazyframe);
    }
  }

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures != 0) {
    std::cerr << g_failures << " XES frame check(s) failed\n";
    return 1;
  }
  std::cout << "review GE: POWL token replay and Heuristics++ intervals on XES upload frames passed\n";
  return 0;
}
