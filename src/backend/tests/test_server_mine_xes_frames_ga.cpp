#undef NDEBUG
// Review GA1: /api/mine on the frame shapes of an XES upload with gaps.
// The heuristics' AND measure visits the nodes in the order the dependency
// pass creates them (rows in the order their source first leads a DFG pair,
// an accepted edge adding its source, then its target), so the first node
// whose accepted successors mix a missing label with a named one decides the
// TypeError's operand order. Alpha+ converts the frame to an event log (one
// trace per case value in first-row order, the rows without a case id one
// trace, events in row order, a missing activity the label NaN on pandas and
// None on Polars; NaN is one key of the directly-follows counts but never
// equal to itself, so never a short loop); a frame without rows raises
// IndexError, a missing case column, then a missing activity column,
// KeyError. The log skeleton's conformance lists the cases of the groupby
// its discovery reads (pandas: sorted case ids, the rows without one left
// out; Polars: first-row order, the rows without one a case of their own,
// traces sharing a name one case), keeps a Polars missing activity as None,
// and raises TypeError when the activity-frequency deviations of a case name
// None beside a named activity.
// Expected values are pm4py 2.7.22 black-box on the same uploads
// (review-harness round-ga, ct/mk.py and ct/ref.py).
//
// Usage: minera_server_mine_xes_frames_ga_tests <minera_server>

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

std::size_t count_of(const std::string& text, const std::string& needle) {
  std::size_t count = 0;
  for (auto at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1)) ++count;
  return count;
}

// Every value of `"key":` in the entry, comma separated, in order.
std::string values_of(const std::string& entry, const std::string& key) {
  std::string out;
  const std::string needle = "\"" + key + "\":";
  for (auto at = entry.find(needle); at != std::string::npos; at = entry.find(needle, at + 1)) {
    const auto from = at + needle.size();
    const auto to = entry.find_first_of(",}", from);
    out += (out.empty() ? "" : ",") + entry.substr(from, to - from);
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
      {"log", log, "ga1.xes", "application/xml"},
  };
  for (const auto& [name, value] : fields) items.push_back({name, value, "", ""});
  const auto result = client.Post("/api/mine", items);
  return result ? result->body : std::string("no response");
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
  // d is created (by a's accepted a->d) before e, which appears first: d's
  // successors [d, missing] sort first.
  const std::string node_order =
      log(trace("c1", event("a", "01") + event("e", "02") + event("a", "03") + event("d", "04") + event("d", "05") +
                          event("", "06")) +
          trace("c2", event("e", "07") + event("", "08")) +
          trace("c3", event("e", "09") + event("b", "10") + event("", "11")));
  const std::string skel_cases = log(trace("c2", event("a", "01") + event("b", "02")) + trace("", event("a", "03")) +
                                     trace("c1", event("a", "04")) + trace("c2", event("b", "05")) +
                                     trace("", event("c", "06")));
  const std::string skel_null = log(trace("c1", event("a", "01") + event("", "02")) + trace("c2", event("a", "03")) +
                                    trace("c3", event("", "04") + event("", "05") + event("b", "06")));
  const std::string aplus_gaps =
      log(trace("c1", event("a", "01") + event("", "02") + event("b", "03")) +
          trace("", event("a", "04") + event("c", "")) + trace("c2", event("a", "05") + event("b", "06")));
  const std::string aplus_nocase = log(trace("", event("a", "01") + event("b", "02")));
  const std::string aplus_noact = log(trace("c1", event("", "01") + event("", "02")));
  const std::string aplus_empty = log(trace("c1", ""));
  // pandas' NaN label repeats in the trace but nan == nan is False: no
  // length-one loop, no "a b a" closed by it. Polars' None is equal to itself.
  const std::string aplus_loop = log(trace("c1", event("a", "01") + event("", "02") + event("", "03") + event("b", "04")) +
                                     trace("c2", event("a", "05") + event("b", "06")));
  const std::string aplus_two =
      log(trace("c1", event("a", "01") + event("", "02") + event("b", "03") + event("", "04") + event("b", "05") +
                          event("c", "06")) +
          trace("c2", event("a", "07") + event("b", "08") + event("", "09") + event("b", "10") + event("", "11") +
                          event("c", "12")));
  std::string sort_traces, sort2_traces;
  for (int i = 1; i <= 4; ++i) {
    const auto pair = trace("c" + std::to_string(i), event("a", "0" + std::to_string(i)) + event("", "1" + std::to_string(i)));
    sort_traces += pair;
    sort2_traces += pair;
  }
  const std::string skel_sort =
      log(sort_traces + trace("c5", event("a", "20") + event("a", "21") + event("", "22") + event("", "23")));
  const std::string skel_sort2 =
      log(sort2_traces + trace("c5", event("", "20") + event("", "21") + event("a", "22") + event("a", "23")));

  const std::vector<std::pair<std::string, std::string>> plus = {{"alpha_variant", "plus"}};
  const auto noise = [](const std::string& value) {
    return std::vector<std::pair<std::string, std::string>>{{"log_skeleton_noise_threshold", value}};
  };

  {  // The heuristics' AND measure in node-creation order.
    check("node order, heuristic (dataframe shape)", outcome(family(mine(client, node_order, "dataframe"), "heuristic")),
          "'<' not supported between instances of 'float' and 'str'");
    check("node order, heuristic (lazyframe shape)", outcome(family(mine(client, node_order, "lazyframe"), "heuristic")),
          "'<' not supported between instances of 'NoneType' and 'str'");
  }

  {  // Alpha+ on the frame's event log.
    for (const std::string shape : {"dataframe", "lazyframe"}) {
      const std::string at = " (" + shape + " shape)";
      auto entry = family(mine(client, aplus_gaps, shape, plus), "alpha");
      check("Alpha+ with gaps" + at, outcome(entry), "OK");
      // Four transitions (a, b, c and the missing label: the text nan on
      // pandas, None (null) on Polars) and five places.
      check("Alpha+ with gaps, places" + at, std::to_string(count_of(entry, "{\"id\":") - 4), "5");
      check("Alpha+ with gaps, null labels" + at, std::to_string(count_of(entry, "\"label\":null")),
            shape == "dataframe" ? "0" : "1");
      check("Alpha+ with gaps, nan label" + at, std::to_string(count_of(entry, "\"label\":\"nan\"")),
            shape == "dataframe" ? "1" : "0");
      check("Alpha+ without case column" + at, outcome(family(mine(client, aplus_nocase, shape, plus), "alpha")),
            "KeyError: 'case:concept:name'");
      check("Alpha+ without activity column" + at, outcome(family(mine(client, aplus_noact, shape, plus), "alpha")),
            "KeyError: 'concept:name'");
      check("Alpha+ without rows" + at, outcome(family(mine(client, aplus_empty, shape, plus), "alpha")),
            "IndexError: list index out of range");
      check("Alpha+ on the node-order upload" + at, outcome(family(mine(client, node_order, shape, plus), "alpha")), "OK");
      // Three transitions; pandas: no loop place for NaN (3 places), Polars:
      // None's loop place (4).
      entry = family(mine(client, aplus_loop, shape, plus), "alpha");
      check("Alpha+ with a repeated missing label" + at, outcome(entry), "OK");
      check("Alpha+ with a repeated missing label, places" + at, std::to_string(count_of(entry, "{\"id\":") - 3),
            shape == "dataframe" ? "3" : "4");
      // Four transitions; pandas: "b nan b" alone is no length-two loop (6
      // places), Polars: b and None loop both ways (8).
      entry = family(mine(client, aplus_two, shape, plus), "alpha");
      check("Alpha+ with a missing label in a b-loop" + at, outcome(entry), "OK");
      check("Alpha+ with a missing label in a b-loop, places" + at, std::to_string(count_of(entry, "{\"id\":") - 4),
            shape == "dataframe" ? "6" : "8");
    }
  }

  {  // Log skeleton conformance over the groupby's cases.
    auto entry = family(mine(client, skel_cases, "dataframe"), "log_skeleton_conformance");
    check("conformance cases (dataframe shape)", outcome(entry), "OK");
    check("conformance cases, totals (dataframe shape)", values_of(entry, "no_constr_total"), "2,4");
    entry = family(mine(client, skel_cases, "lazyframe"), "log_skeleton_conformance");
    check("conformance cases (lazyframe shape)", outcome(entry), "OK");
    check("conformance cases, totals (lazyframe shape)", values_of(entry, "no_constr_total"), "3,5,1");
    entry = family(mine(client, skel_cases, "lazyframe", noise("0.4")), "log_skeleton_conformance");
    check("conformance cases at noise 0.4, totals (lazyframe shape)", values_of(entry, "no_constr_total"), "5,7,3");
    check("conformance cases at noise 0.4, deviations (lazyframe shape)", values_of(entry, "no_dev_total"), "1,1,0");
    entry = family(mine(client, skel_cases, "dataframe", noise("0.4")), "log_skeleton_conformance");
    check("conformance cases at noise 0.4, totals (dataframe shape)", values_of(entry, "no_constr_total"), "3,5");

    entry = family(mine(client, skel_null, "lazyframe"), "log_skeleton_conformance");
    check("conformance with a missing activity (lazyframe shape)", outcome(entry), "OK");
    check("conformance with a missing activity, totals (lazyframe shape)", values_of(entry, "no_constr_total"),
          "3,2,4");
    entry = family(mine(client, skel_null, "lazyframe", noise("0.4")), "log_skeleton_conformance");
    check("conformance with a missing activity at noise 0.4, totals (lazyframe shape)",
          values_of(entry, "no_constr_total"), "5,2,6");
    check("conformance with a missing activity at noise 0.4, null pair (lazyframe shape)",
          holds(entry, "\"constraint\":\"never_together\",\"relation\":{\"source\":null,\"target\":\"a\"}"), "found");
    check("conformance with a missing activity (dataframe shape)",
          outcome(family(mine(client, skel_null, "dataframe"), "log_skeleton")), "KeyError: nan");

    check("conformance sort, None first in the deviations (lazyframe shape)",
          outcome(family(mine(client, skel_sort, "lazyframe", noise("0.8")), "log_skeleton_conformance")),
          "TypeError: '<' not supported between instances of 'NoneType' and 'str'");
    check("conformance sort, a named activity first (lazyframe shape)",
          outcome(family(mine(client, skel_sort2, "lazyframe", noise("0.8")), "log_skeleton_conformance")),
          "TypeError: '<' not supported between instances of 'str' and 'NoneType'");
    check("conformance sort without deviations (lazyframe shape)",
          outcome(family(mine(client, skel_sort, "lazyframe"), "log_skeleton_conformance")), "OK");
  }

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures != 0) {
    std::cerr << g_failures << " mine XES frame check(s) failed\n";
    return 1;
  }
  std::cout << "review GA1: heuristics node order, Alpha+ and log skeleton conformance on XES upload frames passed\n";
  return 0;
}
