#undef NDEBUG
// Review GB1: /api/mine options and the workbench routes on the frame shapes
// of an XES upload with gaps. Heuristics++ converts the frame to an event
// log and raises on a missing timestamp (pandas' NaT has no timestamp(),
// Polars subtracts None) and where Python compares a missing label with a
// named one; the classic performance view waits 0 over a pandas NaT and
// fails on a Polars None. An explicit start key the frame holds is read as
// given: Alpha sorts a text start as text, the temporal profile compares it
// with the timestamp and raises, a start key the frame lacks raises pandas'
// KeyError, and Polars selects the timestamp given as the start key once,
// without the start alias. The routes that re-discover on an XES session
// (discover-constraints' log skeleton and its conformance, discover-workflow,
// discover-alpha-plus without an upload, analyze-behavior's temporal
// profile) read the frame as /api/mine does.
// Expected values are pm4py 2.7.22 black-box on the same uploads
// (review-harness round-gb, ct/mk.py and ct/cmp.py).
//
// Usage: minera_server_mine_xes_frames_gb_tests <minera_server>

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

std::string event(const std::string& activity, const std::string& minute, const std::string& resource = "") {
  std::string out = "<event>";
  if (!activity.empty()) out += "<string key=\"concept:name\" value=\"" + activity + "\"/>";
  if (!minute.empty()) out += "<date key=\"time:timestamp\" value=\"2021-03-01T08:" + minute + ":00.000+00:00\"/>";
  if (!resource.empty()) out += "<string key=\"org:resource\" value=\"" + resource + "\"/>";
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
      {"log", log, "gb1.xes", "application/xml"},
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
  const std::string hpp_nat = log(trace("c1", event("a", "01") + event("b", "")) + trace("c2", event("a", "02") + event("b", "03")));
  const std::string hpp_cmp = log(trace("c1", event("a", "01") + event("", "01") + event("b", "02")) +
                                  trace("c2", event("a", "03") + event("b", "04")));
  const std::string perf_gap = log(trace("c1", event("a", "01") + event("b", "") + event("c", "05")) +
                                   trace("c2", event("a", "02") + event("b", "04") + event("c", "09")));
  const std::string start_text = log(trace("c1", event("a", "01", "y") + event("b", "02", "x")) +
                                     trace("c2", event("a", "03", "x") + event("c", "04", "y")));
  const std::string no_case = log(trace("", event("a", "01") + event("b", "02")) + trace("", event("a", "03") + event("c", "04")));
  const std::string skel_gaps = log(trace("c2", event("a", "01") + event("b", "02")) + trace("", event("a", "03")) +
                                    trace("c1", event("a", "04") + event("", "05")) + trace("c2", event("b", "06")) +
                                    trace("", event("c", "07")));
  const std::string wf_gaps = log(trace("c1", event("a", "01") + event("", "02") + event("b", "03")) +
                                  trace("", event("a", "04") + event("c", "05")) +
                                  trace("c2", event("a", "06") + event("b", "07")));
  using Fields = std::vector<std::pair<std::string, std::string>>;
  const Fields plusplus = {{"heuristic_variant", "plusplus"}};

  {  // Heuristics++ on the frame's event log.
    check("Heuristics++ with a missing timestamp (dataframe shape)",
          outcome(family(mine(client, hpp_nat, "dataframe", plusplus), "heuristic")), "NaTType does not support timestamp");
    check("Heuristics++ with a missing timestamp (lazyframe shape)",
          outcome(family(mine(client, hpp_nat, "lazyframe", plusplus), "heuristic")),
          "unsupported operand type(s) for -: 'NoneType' and 'NoneType'");
    check("Heuristics++ comparing a missing label (dataframe shape)",
          outcome(family(mine(client, hpp_cmp, "dataframe", plusplus), "heuristic")),
          "'<' not supported between instances of 'float' and 'str'");
    check("Heuristics++ comparing a missing label (lazyframe shape)",
          outcome(family(mine(client, hpp_cmp, "lazyframe", plusplus), "heuristic")),
          "'<' not supported between instances of 'NoneType' and 'str'");
  }

  {  // The classic performance view: a pandas NaT waits 0, a Polars None fails.
    const Fields performance = {{"heuristic_decoration", "performance"}};
    const auto entry = family(mine(client, perf_gap, "dataframe", performance), "heuristic");
    check("performance view with a missing timestamp (dataframe shape)", outcome(entry), "OK");
    check("performance view with a missing timestamp, waits (dataframe shape)", values_of(entry, "representation"),
          "60.0,150.0,60.0,150.0");
    check("performance view with a missing timestamp (lazyframe shape)",
          outcome(family(mine(client, perf_gap, "lazyframe", performance), "heuristic")),
          "unsupported operand type(s) for -: 'NoneType' and 'datetime.datetime'");
  }

  {  // Explicit start keys.
    const Fields alpha_text = {{"alpha_start_timestamp_attribute", "org:resource"}};
    for (const std::string shape : {"dataframe", "lazyframe"}) {
      const std::string at = " (" + shape + " shape)";
      const auto entry = family(mine(client, start_text, shape, alpha_text), "alpha");
      check("Alpha with a text start key" + at, outcome(entry), "OK");
      // c1 sorts b (x) before a (y): b leads to a.
      check("Alpha with a text start key, places" + at, values_of(entry, "place_count"), "4");
      check("Alpha with a text start key, transitions" + at, values_of(entry, "activity_name"), "\"a\",\"b\",\"c\"");
    }
    const Fields alpha_missing = {{"alpha_start_timestamp_attribute", "zzz"}};
    check("Alpha with a start key the frame lacks (dataframe shape)",
          outcome(family(mine(client, start_text, "dataframe", alpha_missing), "alpha")), "KeyError: \"['zzz'] not in index\"");
    check("Alpha with a start key the frame lacks (lazyframe shape)",
          outcome(family(mine(client, start_text, "lazyframe", alpha_missing), "alpha")), "OK");
    const Fields tp_text = {{"temporal_target_timestamp_attribute", "org:resource"}};
    check("temporal profile with a text start key (dataframe shape)",
          outcome(family(mine(client, start_text, "dataframe", tp_text), "temporal_profile")),
          "TypeError: Invalid comparison between dtype=datetime64[us, UTC] and StringArray");
    const auto polars_text = outcome(family(mine(client, start_text, "lazyframe", tp_text), "temporal_profile"));
    check("temporal profile with a text start key (lazyframe shape)",
          holds(polars_text, "cannot compare 'date/datetime/time' to a string value"), "found");
    check("temporal profile with a text start key, plan (lazyframe shape)",
          holds(polars_text, "FILTER col(\"time:timestamp\") <= col(\"org:resource_2\")"), "found");
    const Fields tp_timestamp = {{"temporal_target_timestamp_attribute", "time:timestamp"}};
    const auto polars_case = outcome(family(mine(client, no_case, "lazyframe", tp_timestamp), "temporal_profile"));
    check("temporal profile with the timestamp as start key (lazyframe shape)",
          holds(polars_case, "valid columns: [\"concept:name\", \"time:timestamp\"]\n"), "found");
    check("temporal profile with the timestamp as start key, plan (lazyframe shape)",
          holds(polars_case, "SELECT [col(\"concept:name\"), col(\"time:timestamp\")]\n  DF"), "found");
  }

  {  // The workbench routes on an XES session.
    const auto skeleton = route(client, skel_gaps, "lazyframe", "/api/discover-constraints", {{"family", "log_skeleton"}});
    check("discover-constraints log skeleton (lazyframe shape)", route_outcome(skeleton), "OK");
    const auto native = skeleton.find("\"native_result\":");
    const auto conformance = native == std::string::npos ? std::string() : object_at(skeleton, skeleton.find('{', native));
    // pm4py's groupby cases: c2 (both slices), the rows without a case id, c1.
    check("discover-constraints log skeleton, cases (lazyframe shape)", std::to_string(count_of(conformance, "\"no_dev_total\"")), "3");
    check("discover-constraints log skeleton, constraints (lazyframe shape)", values_of(conformance, "no_constr_total"), "4,6,6");
    check("discover-constraints log skeleton (dataframe shape)",
          route_outcome(route(client, skel_gaps, "dataframe", "/api/discover-constraints", {{"family", "log_skeleton"}})),
          "KeyError: nan");
    const auto workflow = route(client, wf_gaps, "lazyframe", "/api/discover-workflow", {{"family", "alpha"}});
    check("discover-workflow alpha (lazyframe shape)", route_outcome(workflow), "OK");
    const auto net = workflow.find("\"petri_net\":", workflow.find("\"native_result\":"));
    check("discover-workflow alpha, transitions (lazyframe shape)",
          net == std::string::npos ? std::string() : values_of(object_at(workflow, workflow.find('{', net)), "activity_name"),
          "\"a\",\"b\",\"c\"");
    check("discover-workflow heuristic (dataframe shape)",
          route_outcome(route(client, wf_gaps, "dataframe", "/api/discover-workflow", {{"family", "heuristic"}})),
          "'float' object is not subscriptable");
    check("discover-workflow heuristic (lazyframe shape)",
          route_outcome(route(client, wf_gaps, "lazyframe", "/api/discover-workflow", {{"family", "heuristic"}})),
          "'NoneType' object is not subscriptable");
    for (const std::string shape : {"dataframe", "lazyframe"}) {
      const std::string at = " (" + shape + " shape)";
      const auto plus = route(client, wf_gaps, shape, "/api/discover-alpha-plus", {}, false);
      check("discover-alpha-plus on the session" + at, route_outcome(plus), "OK");
      check("discover-alpha-plus on the session, labels" + at, values_of(plus, "label"),
            shape == "dataframe" ? "\"a\",\"nan\",\"b\",\"c\"" : "\"a\",null,\"b\",\"c\"");
    }
    check("analyze-behavior temporal profile without a case column (dataframe shape)",
          route_outcome(route(client, no_case, "dataframe", "/api/analyze-behavior",
                              {{"family", "temporal_profile"}, {"variant", "dataframe"}})),
          "KeyError: \"['case:concept:name'] not in index\"");
    check("analyze-behavior temporal profile without a case column (lazyframe shape)",
          holds(route_outcome(route(client, no_case, "lazyframe", "/api/analyze-behavior",
                                    {{"family", "temporal_profile"}, {"variant", "lazyframe"}})),
                "unable to find column \"case:concept:name\""),
          "found");
  }

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures != 0) {
    std::cerr << g_failures << " XES frame check(s) failed\n";
    return 1;
  }
  std::cout << "review GB1: Heuristics++, performance view, explicit start keys and workbench routes on XES upload frames passed\n";
  return 0;
}
