#undef NDEBUG
// Review GF: routes replaying a session's cached POWL or heuristics net, and
// Heuristics++'s lifecycle pairing by concept:instance. pm4py's converted
// nets name their places and silent transitions (POWL: p_N, <kind>_N;
// heuristics: source0, sink0, pre_X, intplace_X, splace_in_T_S, hid_N) and
// its token replay breaks routing ties by those names: the default log
// shape's heuristics replay, /api/evaluate's token-based fitness, precision
// and generalization, and /api/compare-petri replay the cached net with
// them. Heuristics++ keys an open start by its activity and instance as
// Python values: an int 2 pairs with a float 2.0 but not with a string '2'
// on a log and a pandas frame; a Polars frame holds the column as text.
// Expected values are pm4py 2.7.22 black-box on the same uploads
// (review-harness round-gf, ct/logtot.py, ct/evprobe.py, ct/cpms.py and
// ct/inst_ct.py).
//
// Usage: minera_server_cached_net_names_gf_tests <minera_server>

#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <map>
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

std::string mine(httplib::Client& client, const std::string& log,
                 const std::vector<std::pair<std::string, std::string>>& fields = {}) {
  httplib::MultipartFormDataItems items = {
      {"include_traces", "false", "", ""},
      {"log", log, "gf1.xes", "application/xml"},
  };
  for (const auto& [name, value] : fields) items.push_back({name, value, "", ""});
  const auto result = client.Post("/api/mine", items);
  return result ? result->body : std::string("no response");
}

// The number after `"key":` (optionally a space) in `entry`, or "" if absent.
std::string number_after(const std::string& entry, const std::string& key, std::size_t from = 0) {
  auto at = entry.find("\"" + key + "\":", from);
  if (at == std::string::npos) return {};
  at += key.size() + 3;
  while (at < entry.size() && entry[at] == ' ') ++at;
  return entry.substr(at, entry.find_first_of(",}]", at) - at);
}

// The model's token replay totals in a default-shape mine: produced,
// consumed, missing, remaining, fitting traces, traces.
std::string replay_totals(const std::string& body, const std::string& model) {
  const auto at = body.find("\"" + model + "\": {\"sound\": ");
  if (at == std::string::npos) return "no " + model + " conformance: " + body.substr(0, 200);
  const auto entry = object_at(body, body.find('{', at));
  const auto replay_at = entry.find("\"token_replay\": {");
  if (replay_at == std::string::npos) return "no token_replay";
  const auto replay = object_at(entry, entry.find('{', replay_at));
  std::string out;
  for (const std::string key : {"produced", "consumed", "missing", "remaining", "fitting_traces", "trace_count"})
    out += (out.empty() ? "" : ",") + number_after(replay, key);
  return out;
}

// A double read back from the response compared exactly with the reference.
void check_number(const std::string& label, const std::string& seen, double expected) {
  char* end = nullptr;
  const double value = seen.empty() ? 0.0 : std::strtod(seen.c_str(), &end);
  if (!seen.empty() && end && *end == '\0' && value == expected) return;
  ++g_failures;
  std::cerr.precision(17);
  std::cerr << "FAIL: " << label << "\n  seen     " << seen << "\n  expected " << expected << '\n';
}

std::string evaluate(httplib::Client& client, const std::string& query) {
  const auto result = client.Post("/api/evaluate?" + query, "", "application/x-www-form-urlencoded");
  return result ? result->body : std::string("no response");
}

// The (log1, log2) usage multiset of one array of a compare-petri answer,
// as "log1/log2xcount" in ascending order.
std::string usage_multiset(const std::string& body, const std::string& array) {
  const auto at = body.find("\"" + array + "\":[");
  if (at == std::string::npos) return "no " + array + ": " + body.substr(0, 200);
  std::map<std::pair<long, long>, int> counts;
  std::size_t open = body.find('{', at);
  const auto close = body.find(']', at);
  while (open != std::string::npos && open < close) {
    const auto entry = object_at(body, open);
    ++counts[{std::stol(number_after(entry, "log1_occurrences")), std::stol(number_after(entry, "log2_occurrences"))}];
    open = body.find('{', open + entry.size());
  }
  std::string out;
  for (const auto& [usage, count] : counts)
    out += (out.empty() ? "" : " ") + std::to_string(usage.first) + "/" + std::to_string(usage.second) + "x" +
           std::to_string(count);
  return out;
}

// A lifecycle event with a typed concept:instance (no activity when `activity`
// is empty, no instance when `type` is empty).
std::string typed_event(const std::string& activity, int minute, const std::string& lifecycle,
                        const std::string& type = "", const std::string& instance = "") {
  std::string out = "<event>";
  if (!activity.empty()) out += "<string key=\"concept:name\" value=\"" + activity + "\"/>";
  out += "<date key=\"time:timestamp\" value=\"2021-03-01T08:" + std::string(minute < 10 ? "0" : "") +
         std::to_string(minute) + ":00.000+00:00\"/>";
  out += "<string key=\"lifecycle:transition\" value=\"" + lifecycle + "\"/>";
  if (!type.empty()) out += "<" + type + " key=\"concept:instance\" value=\"" + instance + "\"/>";
  return out + "</event>";
}

std::string typed_log(const std::vector<std::pair<std::string, std::string>>& traces) {
  std::string out =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
      "<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n";
  for (const auto& [name, events] : traces)
    out += "<trace><string key=\"concept:name\" value=\"" + name + "\"/>" + events + "</trace>\n";
  return out + "</log>\n";
}

// The single dependency measure's value of the mined heuristics net.
std::string single_dependency(const std::string& body) {
  const auto at = body.find("\"dependency_measures\": [");
  if (at == std::string::npos) return "no dependency_measures: " + family_outcome(body, "heuristic");
  const auto end = body.find(']', at);
  const auto first = body.find('{', at);
  if (first == std::string::npos || first > end) return "none";
  if (body.find('{', first + 1) < end) return "several: " + body.substr(at, end - at);
  return number_after(body, "value", first);
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

  // An event of each case at minute N (no lifecycle: completes).
  const std::string powl_log =
      "c10:a@4 e@8 l@10|c11:g@53 k@52|c12:h@32 g@34|c13:f@26 f@31 c@35 k@37 k@42 e@43 k@43 l@43 g@47 l@47 j@48 b@53|"
      "c14:k@53 c i@58 b@59|c15:k@32 l@35 c j@40 a@40 c@41 a@42 h@45 h@50 g@50 j@54 g@54 d|"
      "c16:b@5 i@9 a@10 e@13 l@13 i@14 k@14 b@14 d@14 h@17 k@19 i@19 a@22 f@25|c17:h@14 g@16 a@19 i@24 k@29 e@33 f@33 j@33";
  const std::string heuristics_log =
      "c10:e@38 c@39/start a@39 a@39 e@40 e@45 d@50 c@51|c11:g@38 a@43 e@45/start a@49 e@49 d@49 b@51 c@51/complete|:|"
      "c2:a@21/complete";

  {  // The default log shape's heuristics replay uses pm4py's net names (an
     // empty trace dropped, the unnamed case apart).
    struct Case {
      std::string name, spec, totals;
    };
    const std::vector<Case> cases = {
        {"lifecycle rows and an empty trace", heuristics_log, "33,34,1,0,2,3"},
        {"unnamed cases",
         ":e@26 e@28 a@28 c@28|c11:d@45 e@49|c12:a@3 a@3 d@3 b@3|c13:a@35 b@38|"
         "c14:d@44 c@37 c@44 d@48 e@48 d@48 d@40 a@40|c15:b@31|:a@7 a@12 a@12 b@13 b@14 e@14 d@14 d@15 d@16|"
         "c17:b@23 e@25 b@28 c@30 e@30 c@31 e@31 e@31",
         "88,88,4,4,4,8"},
        {"a case name used twice",
         "c0:c@17|c11:b@34 d@35 d@35 a@39 d@43 d@47 c@49 c@54 c@56|c12:c@38 c@41|c0:c@9 a@9 a@14 e@19 e@24 e@24 b@25|"
         "c14:c@36 a@40 c@41 c@46 c@50 c@55|c15:b@25|c16:a@15 b@15 b@20 b@20 c@25 b@27 a@32 c@34 c@39|"
         "c17:d@35 d@39 c@39 d@43",
         "91,91,1,1,6,7"},
    };
    for (const auto& item : cases)
      check("heuristics token replay totals, " + item.name + " (log shape)",
            replay_totals(mine(client, xes(item.spec)), "heuristic"), item.totals);
  }

  {  // /api/evaluate replays the session's cached net with pm4py's names.
    mine(client, xes(powl_log));
    check_number("POWL token-based fitness",
                 number_after(evaluate(client, "miner=powl&evaluation_family=replay_fitness&fitness_variant=token_based"),
                              "log_fitness"),
                 0.9829735689910502);
    const auto precision =
        evaluate(client, "miner=powl&evaluation_family=precision&precision_variant=etconformance_token");
    const auto precision_at = precision.find("\"precision\":");
    check_number("POWL token-based precision",
                 precision_at == std::string::npos
                     ? precision.substr(0, 200)
                     : number_after(object_at(precision, precision.find('{', precision_at)), "precision"),
                 0.20844327176781008);
    check_number("POWL token-based generalization",
                 number_after(evaluate(client, "miner=powl&evaluation_family=generalization"), "score"),
                 0.6567465307858212);
    mine(client, xes(heuristics_log));
    check_number("heuristics token-based fitness",
                 number_after(evaluate(client, "miner=heuristic&evaluation_family=replay_fitness&fitness_variant=token_based"),
                              "log_fitness"),
                 0.9852941176470589);
    check_number("heuristics token-based generalization",
                 number_after(evaluate(client, "miner=heuristic&evaluation_family=generalization"), "score"),
                 0.15854116926548356);
  }

  {  // /api/compare-petri replays both logs on the cached POWL net with its names.
    mine(client, xes(powl_log));
    httplib::MultipartFormDataItems items = {
        {"reference_log", xes(heuristics_log), "gf2.xes", "application/xml"},
        {"miner", "powl", "", ""},
    };
    const auto result = client.Post("/api/compare-petri", items);
    const std::string body = result ? result->body : std::string("no response");
    check("POWL element usage, transitions", usage_multiset(body, "transitions"),
          "2/2x1 3/0x1 4/0x2 4/1x2 4/3x1 4/5x1 5/0x3 6/1x1 6/5x1 8/3x2 9/0x1 10/2x1 10/4x1 11/3x1 11/5x1 12/6x1 "
          "13/4x1 13/6x1 13/7x1 14/5x2 14/6x1 14/7x2 15/5x1 16/7x1 17/6x2 17/7x2 19/6x2 19/7x1 20/7x1 37/13x1");
    check("POWL element usage, places", usage_multiset(body, "places"),
          "8/3x2 14/7x3 15/5x2 16/7x1 17/6x2 17/7x5 18/6x1 19/6x7 19/7x2 20/7x1 37/13x1 38/13x1");
  }

  {  // Heuristics++ pairs a start with its complete by the instance's Python value.
    const std::string start = typed_event("a", 10, "start", "int", "2"), between = typed_event("b", 12, "complete");
    const std::string typed = typed_log({
        {"c1", start + between + typed_event("a", 20, "complete", "string", "2")},
        {"c2", start + between + typed_event("a", 20, "complete", "string", "2")},
        {"c3", start + between + typed_event("a", 20, "complete", "float", "2.0")},
    });
    const std::vector<std::pair<std::string, std::string>> plusplus = {{"heuristic_variant", "plusplus"}};
    // pandas and the log: only c3's a overlaps b, so b precedes a twice.
    check_number("Heuristics++ b->a dependency, typed instances (log shape)",
                 single_dependency(mine(client, typed, plusplus)), 2.0 / 3.0);
    auto with_shape = [&](const std::string& shape) {
      auto fields = plusplus;
      fields.push_back({"mining_input_shape", shape});
      return fields;
    };
    check_number("Heuristics++ b->a dependency, typed instances (dataframe shape)",
                 single_dependency(mine(client, typed, with_shape("dataframe"))), 2.0 / 3.0);
    // Polars holds the column as text: '2.0' differs from '2'.
    check_number("Heuristics++ b->a dependency, typed instances (lazyframe shape)",
                 single_dependency(mine(client, typed, with_shape("lazyframe"))), 1.0 / 3.0);
    // The missing label's int 2 start stays open beside its '2' complete:
    // the trace's intervals are e then the missing label, both at 26.
    const std::string missing = typed_log({{"c1", typed_event("", 14, "start", "int", "2") +
                                                        typed_event("e", 26, "complete", "string", "2") +
                                                        typed_event("", 26, "complete", "string", "2")}});
    check("Heuristics++, a missing label's typed instances (dataframe shape)",
          family_outcome(mine(client, missing, with_shape("dataframe")), "heuristic"),
          "TypeError: '<' not supported between instances of 'float' and 'str'");
    check("Heuristics++, a missing label's typed instances (lazyframe shape)",
          family_outcome(mine(client, missing, with_shape("lazyframe")), "heuristic"),
          "TypeError: '<' not supported between instances of 'str' and 'NoneType'");
  }

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures != 0) {
    std::cerr << g_failures << " cached-net check(s) failed\n";
    return 1;
  }
  std::cout << "review GF: cached POWL and heuristics nets replayed with pm4py's names, typed instances passed\n";
  return 0;
}
