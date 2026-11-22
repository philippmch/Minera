#undef NDEBUG
// Review GD: token replay of a session's heuristics and Alpha models and
// the heuristics model's footprints on XES upload frames. pm4py's
// token_replay.apply replays the frame's variants: each case's activities in
// the frame's row order, pandas' cases in its sorted groupby without a
// missing case, Polars' in first-row order with the null case as one more.
// The variants sort by count and activity tuple, a TypeError where a missing
// label meets a name; the net's transitions sort by name first, a TypeError
// beside the missing label's own transition. A pandas NaN matches no
// transition of the heuristics net but Alpha's NaN transition; Polars' None
// fires a silent one. Silent routing ties go by pm4py's place names. The
// footprints of the cached heuristics net need no POWL model. Expected
// values are pm4py 2.7.22 black-box on the same uploads (review-harness
// round-gd, ct/mk.py and ct/mka.py).
//
// Usage: minera_server_mine_xes_frames_gd_tests <minera_server>

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

const std::string kHead =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
    "<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n";

std::string event(const std::string& activity, const std::string& minute, const std::string& extra = "") {
  std::string out = "<event>";
  if (!activity.empty()) out += "<string key=\"concept:name\" value=\"" + activity + "\"/>";
  if (!minute.empty()) out += "<date key=\"time:timestamp\" value=\"2021-03-01T08:" + minute + ":00.000+00:00\"/>";
  return out + extra + "</event>";
}

std::string trace(const std::string& name, const std::string& events) {
  return "<trace>" + (name.empty() ? std::string() : "<string key=\"concept:name\" value=\"" + name + "\"/>") + events +
         "</trace>\n";
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

// The family's entry in family_results, or "" when the response has none.
std::string family(const std::string& body, const std::string& name) {
  const auto results = body.find("\"family_results\":{");
  if (results == std::string::npos) return {};
  const auto at = body.find("\"" + name + "\":{", results);
  if (at == std::string::npos) return {};
  return object_at(body, body.find('{', at));
}

// The family's error message (JSON escapes decoded), or "OK" when it ran.
std::string outcome(const std::string& entry) {
  if (entry.empty()) return "no family entry";
  if (entry.find("\"executed\":true") != std::string::npos) return "OK";
  const auto message = entry.find("\"message\":\"");
  if (message == std::string::npos) return "no message: " + entry.substr(0, 300);
  std::string out;
  for (std::size_t i = message + 11; i < entry.size() && entry[i] != '"'; ++i) {
    if (entry[i] != '\\') {
      out += entry[i];
      continue;
    }
    const char escaped = entry[++i];
    out += escaped == 'n' ? '\n' : escaped == 't' ? '\t' : escaped;
  }
  return out;
}

// "found" when `entry` holds `needle`, else the start of the entry.
std::string holds(const std::string& entry, const std::string& needle) {
  return entry.find(needle) != std::string::npos ? "found" : entry.substr(0, 600);
}

std::string mine(httplib::Client& client, const std::string& log, const std::string& shape,
                 const std::vector<std::pair<std::string, std::string>>& fields = {}) {
  httplib::MultipartFormDataItems items = {
      {"include_traces", "false", "", ""},
      {"mining_input_shape", shape, "", ""},
      {"log", log, "gd1.xes", "application/xml"},
  };
  for (const auto& [name, value] : fields) items.push_back({name, value, "", ""});
  const auto result = client.Post("/api/mine", items);
  return result ? result->body : std::string("no response");
}

// The session's source id after a mine.
std::string source_id(httplib::Client& client) {
  const auto result = client.Get("/api/log-query/schema");
  if (!result) return "0";
  const auto at = result->body.find("\"source_id\":\"");
  if (at == std::string::npos) return "0";
  const auto from = at + 13;
  return result->body.substr(from, result->body.find('"', from) - from);
}

// A workbench route on the session `log` mines in `shape`.
std::string route(httplib::Client& client, const std::string& log, const std::string& shape, const std::string& path,
                  httplib::Params params, bool with_source = true) {
  mine(client, log, shape);
  if (with_source) params.emplace("source_id", source_id(client));
  const auto result = client.Post(path, params);
  return result ? result->body : std::string("no response");
}

// The route's error message (JSON escapes decoded), or "OK" when it has none.
std::string route_outcome(const std::string& body) {
  if (body.rfind("{\"error\":", 0) != 0) return "OK";
  auto from = body.find('"', 9);
  return from == std::string::npos ? body : outcome("{\"message\":" + body.substr(from));
}

// The JSON object after `"key": {` (from `from` on), or "" when it has none.
std::string object_after(const std::string& body, const std::string& key, std::size_t from = 0) {
  const auto at = body.find("\"" + key + "\": {", from);
  if (at == std::string::npos) return {};
  return object_at(body, body.find('{', at));
}

// The totals of the heuristics model's token replay in the mine's analysis.
std::string replay_totals(const std::string& body) {
  // analysis.conformance.heuristic (the footprints' "conformance" comes first).
  const auto at = body.find("\"heuristic\": {\"sound\": ");
  if (at == std::string::npos) return "no heuristic conformance";
  const auto heuristic = object_at(body, body.find('{', at));
  const auto replay = object_after(heuristic, "token_replay");
  std::string out;
  for (const std::string key : {"produced", "consumed", "missing", "remaining", "fitting_traces", "trace_count"}) {
    const std::string needle = "\"" + key + "\": ";
    const auto at = replay.find(needle);
    const auto from = at == std::string::npos ? replay.size() : at + needle.size();
    out += (out.empty() ? "" : ",") + replay.substr(from, replay.find_first_of(",}", from) - from);
  }
  return out;
}

// The model footprint of /api/discover-graph's footprints of the session's
// heuristics net, or the route's error.
std::string heuristic_footprint(httplib::Client& client, const std::string& log, const std::string& shape) {
  const auto body = route(client, log, shape, "/api/discover-graph",
                          {{"family", "footprints"}, {"footprints_model", "heuristic"}});
  if (route_outcome(body) != "OK") return route_outcome(body);
  const auto at = body.find("\"model_footprint\":");
  if (at == std::string::npos) return "no model_footprint: " + body.substr(0, 300);
  return object_at(body, body.find('{', at));
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

  const auto log = [](const std::string& traces) { return kHead + traces + "</log>\n"; };
  // Events (activity, minute) of one case; an empty activity has no concept:name.
  using Events = std::vector<std::pair<std::string, int>>;
  const auto events = [](const Events& list) {
    std::string out;
    for (const auto& [activity, minute] : list)
      out += event(activity, (minute < 10 ? "0" : "") + std::to_string(minute));
    return out;
  };
  // An unnamed trace: pandas drops its rows, Polars replays them as one more case.
  const std::string null_case = log(trace("c1", events({{"a", 1}, {"b", 2}})) + trace("", events({{"b", 3}, {"a", 4}, {"b", 5}})) +
                                    trace("c2", events({{"a", 6}, {"b", 7}})) + trace("c3", events({{"a", 8}, {"c", 9}})));
  // c3's rows are not in time order: the replay reads them as they are.
  const std::string row_order = log(trace("c1", events({{"a", 1}, {"b", 2}, {"c", 3}})) + trace("c2", events({{"a", 4}, {"b", 5}, {"c", 6}})) +
                                    trace("c3", events({{"c", 12}, {"a", 10}, {"b", 11}})));
  // A variant of one missing label ties in count with a name's: the variant sort raises.
  const std::string variant_sort = log(trace("c10", events({{"b", 49}})) + trace("c11", "") + trace("c12", events({{"c", 44}, {"a", 48}})) +
                                       trace("", events({{"a", 17}})) + trace("c14", "") + trace("c15", events({{"", 10}})) +
                                       trace("c16", events({{"d", 53}})) + trace("", events({{"b", 46}})));
  // Missing labels alone in their cases: no transition for NaN, a silent one for None.
  const std::string none_silent = log(trace("c1", events({{"b", 1}, {"a", 2}})) + trace("c2", events({{"", 3}})) +
                                      trace("c3", events({{"a", 4}, {"b", 5}, {"c", 6}})) + trace("c4", events({{"", 7}})) +
                                      trace("c5", events({{"a", 8}, {"c", 9}})) + trace("c6", events({{"", 10}})) +
                                      trace("c7", events({{"", 11}})) + trace("c8", events({{"b", 12}, {"a", 13}, {"a", 14}})) +
                                      trace("c9", events({{"a", 15}, {"a", 16}, {"a", 17}})));
  // The missing label's own transition is named NaN / None: the name sort raises.
  const std::string null_transition = log(trace("c2", events({{"", 47}})) + trace("c12", events({{"c", 45}})));
  const std::string null_transition_late = log(trace("c1", events({{"a", 1}})) + trace("c2", events({{"", 3}})) + trace("c3", events({{"b", 1}})));
  // Silent routing ties that pm4py breaks by its place names.
  const std::string routing_tie = log(trace("c0", events({{"b", 18}, {"a", 22}, {"c", 43}, {"a", 44}, {"b", 58}})) +
                                      trace("c1", events({{"b", 8}, {"b", 23}})) +
                                      trace("c2", events({{"c", 1}, {"a", 7}, {"a", 33}, {"b", 58}})) +
                                      trace("c3", events({{"b", 5}, {"c", 11}, {"a", 13}, {"b", 14}, {"c", 43}, {"c", 46}})));
  const std::string routing_tie2 = log(trace("c0", events({{"f", 19}, {"f", 21}, {"c", 22}, {"c", 29}, {"e", 49}})) +
                                       trace("c1", events({{"f", 4}, {"e", 5}, {"a", 20}, {"d", 40}, {"a", 44}, {"c", 53}})) +
                                       trace("c2", events({{"e", 10}, {"f", 11}, {"b", 23}, {"a", 29}, {"f", 32}, {"c", 33}, {"b", 38}})));
  // Alpha's net: the missing label's transition alone (NaN matches it on pandas), or beside a name.
  const std::string alpha_nan_only = log(trace("", events({{"c", 36}})) + trace("c11", events({{"", 9}, {"", 10}})) +
                                         trace("c12", events({{"", 20}})));
  const std::string alpha_null_sort = log(trace("c1", events({{"a", 1}, {"b", 2}})) + trace("c2", events({{"", 3}})) +
                                          trace("c3", events({{"a", 4}, {"b", 5}})));
  // No events: POWL fails, the heuristics net is empty.
  const std::string empty_events = log(trace("c1", ""));

  {  // Totals of the token replay: produced, consumed, missing, remaining, fitting traces, traces.
    const auto totals = [&](const std::string& name, const std::string& upload, const std::string& shape,
                            const std::string& expected) {
      check("token replay totals, " + name + " (" + shape + " shape)", replay_totals(mine(client, upload, shape)), expected);
    };
    totals("an unnamed trace", null_case, "dataframe", "7,7,2,2,1,3");
    totals("an unnamed trace", null_case, "lazyframe", "9,9,3,3,1,4");
    totals("rows out of time order", row_order, "dataframe", "21,21,0,0,3,3");
    totals("rows out of time order", row_order, "lazyframe", "21,21,0,0,3,3");
    totals("missing labels alone in their cases", none_silent, "dataframe", "31,33,8,6,1,9");
    totals("missing labels alone in their cases", none_silent, "lazyframe", "35,37,8,6,1,9");
    for (const std::string shape : {"dataframe", "lazyframe"}) {
      totals("a silent routing tie", routing_tie, shape, "39,39,3,3,1,4");
      totals("a second silent routing tie", routing_tie2, shape, "37,37,3,3,1,3");
    }
  }

  {  // The variant sort and the transition name sort.
    const auto replay = [&](const std::string& upload, const std::string& shape) {
      return outcome(family(mine(client, upload, shape), "heuristic_conformance"));
    };
    check("token replay, a missing label's variant tied with a name's (dataframe shape)", replay(variant_sort, "dataframe"),
          "TypeError: '<' not supported between instances of 'float' and 'str'");
    check("token replay, a missing label's variant tied with a name's (lazyframe shape)", replay(variant_sort, "lazyframe"),
          "TypeError: '<' not supported between instances of 'NoneType' and 'str'");
    check("token replay, the missing label's transition first (dataframe shape)", replay(null_transition, "dataframe"),
          "TypeError: '<' not supported between instances of 'str' and 'float'");
    check("token replay, the missing label's transition first (lazyframe shape)", replay(null_transition, "lazyframe"),
          "TypeError: '<' not supported between instances of 'str' and 'NoneType'");
    check("token replay, the missing label's transition later (dataframe shape)", replay(null_transition_late, "dataframe"),
          "TypeError: '<' not supported between instances of 'float' and 'str'");
    check("token replay, the missing label's transition later (lazyframe shape)", replay(null_transition_late, "lazyframe"),
          "TypeError: '<' not supported between instances of 'NoneType' and 'str'");
  }

  {  // Alpha's net replays the frame too, without the Alpha prepared log (a missing case).
    const auto alpha = [&](const std::string& upload, const std::string& shape) {
      const auto body = mine(client, upload, shape);
      const auto entry = family(body, "alpha_conformance");
      if (outcome(entry) != "OK") return outcome(entry);
      const auto at = body.find("\"alpha\": {\"sound\": ");
      if (at == std::string::npos) return std::string("no alpha conformance");
      const auto replay = object_after(object_at(body, body.find('{', at)), "token_replay");
      std::string out;
      for (const std::string key : {"produced", "consumed", "missing", "remaining", "fitting_traces", "trace_count"}) {
        const std::string needle = "\"" + key + "\": ";
        const auto from = replay.find(needle) == std::string::npos ? replay.size() : replay.find(needle) + needle.size();
        out += (out.empty() ? "" : ",") + replay.substr(from, replay.find_first_of(",}", from) - from);
      }
      return out;
    };
    check("Alpha token replay, an unnamed trace (dataframe shape)", alpha(null_case, "dataframe"), "9,9,0,0,3,3");
    check("Alpha token replay, an unnamed trace (lazyframe shape)", alpha(null_case, "lazyframe"), "13,17,5,1,1,4");
    for (const std::string shape : {"dataframe", "lazyframe"})
      check("Alpha token replay, rows out of time order (" + shape + " shape)", alpha(row_order, shape), "15,15,4,4,0,3");
    check("Alpha token replay, NaN fires its own transition (dataframe shape)", alpha(alpha_nan_only, "dataframe"),
          "5,5,1,1,1,2");
    // The operands' order follows a Python set (the hash seed): only the types are checked.
    const std::string type_error = "TypeError: '<' not supported between instances of ";
    const auto sort_error = [&](const std::string& seen, const std::string& missing) {
      return seen.rfind(type_error, 0) == 0 && holds(seen, "'" + missing + "'") == "found" && holds(seen, "'str'") == "found"
                 ? std::string("found")
                 : seen;
    };
    check("Alpha token replay, the missing label's transition beside a name (dataframe shape)",
          sort_error(alpha(alpha_null_sort, "dataframe"), "float"), "found");
    check("Alpha token replay, the missing label's transition beside a name (lazyframe shape)",
          sort_error(alpha(alpha_null_sort, "lazyframe"), "NoneType"), "found");
    check("Alpha token replay, None's transition beside a name (lazyframe shape)",
          sort_error(alpha(alpha_nan_only, "lazyframe"), "NoneType"), "found");
  }

  {  // The footprints of the cached heuristics net, with or without a POWL model.
    for (const std::string shape : {"dataframe", "lazyframe"}) {
      const std::string at = " (" + shape + " shape)";
      check("heuristics net footprints without events" + at, heuristic_footprint(client, empty_events, shape),
            "{\"sequence\": [], \"parallel\": [], \"atomic_interleaving\": [], \"start_activities\": [], \"activities\": []}");
      // Activities a, b, c: c only after a.
      check("heuristics net footprints of an unnamed trace's upload" + at, heuristic_footprint(client, null_case, shape),
            "{\"sequence\": [{\"source\": 0, \"target\": 2}], \"parallel\": [], \"atomic_interleaving\": [], "
            "\"start_activities\": [0], \"activities\": [0, 2]}");
      // Activities b, a, c.
      check("heuristics net footprints of a routing tie" + at, heuristic_footprint(client, routing_tie, shape),
            "{\"sequence\": [{\"source\": 0, \"target\": 2}], \"parallel\": [{\"source\": 0, \"target\": 0}, "
            "{\"source\": 1, \"target\": 1}, {\"source\": 1, \"target\": 2}, {\"source\": 2, \"target\": 1}, "
            "{\"source\": 2, \"target\": 2}], \"atomic_interleaving\": [], \"start_activities\": [0, 2], "
            "\"activities\": [0, 1, 2]}");
    }
  }

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures != 0) {
    std::cerr << g_failures << " XES frame check(s) failed\n";
    return 1;
  }
  std::cout << "review GD: token replay of the heuristics and Alpha models and the heuristics footprints on XES upload frames passed\n";
  return 0;
}
