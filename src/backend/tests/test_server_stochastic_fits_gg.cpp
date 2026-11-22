#undef NDEBUG
// Review GG: the stochastic map's service times, the stochastic fits of
// the Monte Carlo and stochastic playout routes on a session's cached POWL
// or heuristics net, and Heuristics++ lifecycle pairs by instance.
//
// pm4py's get_map_from_log_and_net times a firing from the input token of
// the latest event in replay order, not from the latest time: a join of
// tokens from the first event and a later event at one instant is timed (the
// first event's tokens alone leave a silent firing untimed), and on events
// out of time order the later event's token sets the arrival even when an
// earlier event's time is later. Minera took the latest time with the first
// input winning ties, so both uploads below lost a uniform service time.
//
// The Monte Carlo route and the stochastic playout fit a map of the cached
// net as pm4py fits the net it built: on the session's named replay copy
// (round GF), whose routing ties go by pm4py's names and the POWL net's
// consumer order. They fitted the cached net under Minera's own names, so
// their weights differed from /api/stochastic-map's (which equal pm4py's) on
// the two uploads below; a seeded run now equals the run on that map.
// A firing after an event without a timestamp: that event's token is still
// the arrival, and a sample with a missing time on either side is 0, so the
// exponential and uniform fits below take those zeros (Minera skipped them,
// or took an earlier event's time).
//
// Heuristics++ pairs a lifecycle start with its complete by the instance:
// on an XES upload's log a NaN instance never pairs (each parsed NaN its own
// object; Minera used its frame rule, NaN with NaN); on a pandas frame the
// column's dtype decides (float64: NaN and gaps never pair; str: both pair;
// object: gaps pair, NaN floats never); on Polars a NaN is null.
//
// Expected values are pm4py 2.7.22 black-box on the same uploads, stable over
// PYTHONHASHSEED 0-5 (review-harness round-gg, specref.py, smapsw2.py,
// mccheck.py, pcheck.py).
//
// Usage: minera_server_stochastic_fits_gg_tests <minera_server>

#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
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
// an event "activity@minute" (minutes of 08:00 on 2021-03-01) or a bare
// "activity" without a timestamp, an empty case name for a trace without one.
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
      const auto at = token.find('@');
      out += "<event><string key=\"concept:name\" value=\"" + token.substr(0, at) + "\"/>";
      if (at != std::string::npos) {
        std::string minute = token.substr(at + 1);
        if (minute.size() < 2) minute = "0" + minute;
        out += "<date key=\"time:timestamp\" value=\"2021-03-01T08:" + minute + ":00.000+00:00\"/>";
      }
      out += "</event>";
    }
    out += "</trace>\n";
  }
  return out + "</log>\n";
}

// The JSON object or array that starts at `open`, strings and escapes skipped.
std::string value_at(const std::string& body, std::size_t open) {
  int depth = 0;
  bool in_string = false;
  for (std::size_t i = open; i < body.size(); ++i) {
    const char c = body[i];
    if (in_string) {
      if (c == '\\') ++i;
      else if (c == '"') in_string = false;
    } else if (c == '"') {
      in_string = true;
    } else if (c == '{' || c == '[') {
      ++depth;
    } else if ((c == '}' || c == ']') && --depth == 0) {
      return body.substr(open, i - open + 1);
    }
  }
  return {};
}

// The raw value after `"key":` (optionally a space) in `entry`: a number, or a
// string without its quotes.
std::string field(const std::string& entry, const std::string& key) {
  auto at = entry.find("\"" + key + "\":");
  if (at == std::string::npos) return {};
  at += key.size() + 3;
  while (at < entry.size() && entry[at] == ' ') ++at;
  if (at < entry.size() && entry[at] == '"') return entry.substr(at + 1, entry.find('"', at + 1) - at - 1);
  return entry.substr(at, entry.find_first_of(",}]", at) - at);
}

// The objects of the array that starts at `open`.
std::vector<std::string> objects(const std::string& body, std::size_t open) {
  const auto array = value_at(body, open);
  std::vector<std::string> out;
  for (std::size_t at = array.find('{'); at != std::string::npos; ) {
    out.push_back(value_at(array, at));
    at = array.find('{', at + out.back().size());
  }
  return out;
}

std::string mine(httplib::Client& client, const std::string& log) {
  httplib::MultipartFormDataItems items = {
      {"include_traces", "false", "", ""},
      {"log", log, "gg1.xes", "application/xml"},
  };
  const auto result = client.Post("/api/mine", items);
  return result ? result->body : std::string("no response");
}

// The Heuristics++ dependency measures of an upload mined in `shape` (empty:
// the default log shape) as "source>target=value" by activity name.
std::string plusplus_dependencies(httplib::Client& client, const std::string& log, const std::string& shape) {
  httplib::MultipartFormDataItems items = {
      {"include_traces", "false", "", ""},
      {"heuristic_variant", "plusplus", "", ""},
      {"log", log, "gg2.xes", "application/xml"},
  };
  if (!shape.empty()) items.push_back({"mining_input_shape", shape, "", ""});
  const auto result = client.Post("/api/mine", items);
  if (!result) return "no response";
  const auto& body = result->body;
  const auto net = body.find("\"heuristics_net\":");
  const auto names_at = body.find("\"activities\":");
  if (net == std::string::npos || names_at == std::string::npos) return "no net: " + body.substr(0, 200);
  std::vector<std::string> names;
  for (const auto& activity : objects(body, body.find('[', names_at))) names.push_back(field(activity, "name"));
  const auto measures = body.find("\"dependency_measures\":", net);
  if (measures == std::string::npos) return "no measures";
  std::string out;
  for (const auto& measure : objects(body, body.find('[', measures))) {
    const auto source = std::stoul(field(measure, "source")), target = std::stoul(field(measure, "target"));
    out += (out.empty() ? "" : " ") + (source < names.size() ? names[source] : "?") + ">" +
           (target < names.size() ? names[target] : "?") + "=" + field(measure, "value");
  }
  return out;
}

// One trace c10: b starts at 08:39, e starts and completes at 08:40, b
// completes at 08:45, each event's concept:instance the XES element given
// (empty: none); `extra` adds traces.
std::string lifecycle_xes(const std::string& b_start, const std::string& e_both, const std::string& b_complete,
                          const std::string& extra = "") {
  const auto event = [](const std::string& activity, const std::string& minute, const std::string& transition,
                        const std::string& instance) {
    return "<event><string key=\"concept:name\" value=\"" + activity +
           "\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:" + minute +
           ":00.000+00:00\"/><string key=\"lifecycle:transition\" value=\"" + transition + "\"/>" + instance +
           "</event>";
  };
  return "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
         "<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n"
         "<trace><string key=\"concept:name\" value=\"c10\"/>" +
         event("b", "39", "start", b_start) + event("e", "40", "start", e_both) +
         event("e", "40", "complete", e_both) + event("b", "45", "complete", b_complete) + "</trace>\n" + extra +
         "</log>\n";
}

std::string get(httplib::Client& client, const std::string& path, const httplib::Params& params) {
  const auto result = client.Get(path, params, httplib::Headers{});
  return result ? result->body : std::string("no response");
}

// The map records of a /api/stochastic-map answer (empty on an error).
std::vector<std::string> map_records(const std::string& body) {
  const auto map = body.find("\"map\":{");
  if (map == std::string::npos) return {};
  const auto at = body.find("\"transitions\":[", map);
  if (at == std::string::npos) return {};
  return objects(body, body.find('[', at));
}

// The timed records of the map as "distribution min..max weight", sorted.
std::string timed_records(const std::string& body) {
  const auto records = map_records(body);
  if (records.empty()) return "no map: " + body.substr(0, 200);
  std::vector<std::string> timed;
  for (const auto& record : records)
    if (field(record, "timing") == "timed")
      timed.push_back(field(record, "distribution") + " " + field(record, "parameter") + ".." +
                      field(record, "secondary_parameter") + " w" + field(record, "activation_weight"));
  std::sort(timed.begin(), timed.end());
  std::string out;
  for (const auto& item : timed) out += (out.empty() ? "" : "; ") + item;
  return out;
}

// The map as the Monte Carlo route's provided-map records.
std::string provided_map(const std::vector<std::string>& records) {
  std::string out;
  for (const auto& record : records)
    out += (out.empty() ? "" : ",") + field(record, "transition") + ":" + field(record, "timing") + ":" +
           field(record, "distribution") + ":" + field(record, "parameter") + ":" +
           field(record, "secondary_parameter") + ":" + field(record, "tertiary_parameter") + ":" +
           field(record, "activation_weight") + ":" + field(record, "priority");
  return out;
}

// The map's weights as a dense playout weight list (absent transitions 1.0).
std::string dense_weights(const std::vector<std::string>& records, std::size_t transitions) {
  std::vector<std::string> weights(transitions, "1");
  for (const auto& record : records)
    weights.at(std::stoul(field(record, "transition"))) = field(record, "activation_weight");
  std::string out;
  for (const auto& weight : weights) out += (out.empty() ? "" : ",") + weight;
  return out;
}

// The transition count of the mined net under `key`.
std::size_t net_transitions(const std::string& body, const std::string& key) {
  const auto at = body.find("\"" + key + "\": {");
  if (at == std::string::npos) return 0;
  const auto net = value_at(body, body.find('{', at));
  const auto list = net.find("\"transitions\": [");
  return list == std::string::npos ? 0 : objects(net, net.find('[', list)).size();
}

// A seeded Monte Carlo run fitted by the route equals the run on the
// stochastic map route's map; a seeded stochastic playout fitted from the
// log equals the playout with that map's weights.
void check_fits(httplib::Client& client, const std::string& label, const std::string& spec,
                const std::string& miner, const std::string& net_key) {
  const auto mined = mine(client, xes(spec));
  const auto transitions = net_transitions(mined, net_key);
  if (transitions == 0) {
    check(label + ": mined net", mined.substr(0, 300), net_key);
    return;
  }
  const auto records = map_records(get(client, "/api/stochastic-map", {{"miner", miner}}));
  if (records.empty()) {
    check(label + ": stochastic map", "no map", "a map");
    return;
  }
  const httplib::Params monte_carlo = {{"miner", miner},
                                       {"monte_carlo_numpy_legacy_seed", "7"},
                                       {"monte_carlo_simulations", "25"}};
  auto supplied = monte_carlo;
  supplied.emplace("monte_carlo_provided_stochastic_map", provided_map(records));
  const auto fitted = get(client, "/api/monte-carlo", monte_carlo);
  check(label + ": Monte Carlo fitted from the log equals the run on the stochastic map",
        fitted, get(client, "/api/monte-carlo", supplied));
  check(label + ": Monte Carlo ran", fitted.find("\"error\"") == std::string::npos ? "ok" : fitted.substr(0, 200),
        "ok");
  const httplib::Params playout = {{"miner", miner},
                                   {"mode", "stochastic"},
                                   {"stochastic_seed", "7"},
                                   {"stochastic_traces", "30"}};
  auto from_log = playout;
  from_log.emplace("stochastic_fit_from_log", "true");
  auto weighted = playout;
  weighted.emplace("stochastic_transition_weights", dense_weights(records, transitions));
  const auto played = get(client, "/api/playout", from_log);
  check(label + ": stochastic playout fitted from the log equals the playout on the map's weights", played,
        get(client, "/api/playout", weighted));
  check(label + ": stochastic playout ran", played.rfind("<?xml", 0) == 0 ? "ok" : played.substr(0, 200), "ok");
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

  {  // c10's b (the first event) and d meet at a silent join at 08:08: the
     // join is timed, so c's first firing waits 0 s and its second 120 s.
    mine(client, xes("c10:b@8 d@8 c@8 e@11 c@13|c11:d@33 d@37 b@40 a@41 a@45 a@50 d@54|c12:c@30|c0:c@28"));
    check("stochastic map service times, a join at the first event's instant",
          timed_records(get(client, "/api/stochastic-map", {{"miner", "powl"}})),
          "uniform 0..120 w1; uniform 240..300 w1");
  }
  {  // c16's d joins a (08:41, first) and b (08:38, second): b's token is the
     // later event's, so d waits 180 s although a's time is later.
    mine(client, xes("c10:b@35 a@35|c11:d@18|c12:c@38 c@38|c13:b@17 b@17 b@20|c14:d@27|c15:a@33 c@34 d@34|"
                     "c16:a@41 b@38 d@41|c17:c@13 b@18 c@21 a@23|c18:c@6"));
    check("stochastic map service times, events out of time order",
          timed_records(get(client, "/api/stochastic-map", {{"miner", "powl"}})),
          "uniform 0..180 w0.44444444444444442");
  }

  {  // Events without a timestamp (round GG, rule H).
    mine(client, xes("c10:a@46 a@50 a@54 a a@54 c@56|:b@23"));
    check("stochastic map service times, an event without a timestamp (POWL)",
          timed_records(get(client, "/api/stochastic-map", {{"miner", "powl"}})), "exponential 0.012500000000000001..0 w1");
    mine(client, xes(":d@6 d@7 a c@14 a@19 c@19|c11:d f@31 d@32 f@37 d@37"));
    check("stochastic map service times, an event without a timestamp (heuristics)",
          timed_records(get(client, "/api/stochastic-map", {{"miner", "heuristic"}})),
          "uniform 0..300 w0.40000000000000002; uniform 0..300 w1; uniform 0..60 w1");
  }
  {  // Heuristics++ lifecycle pairs by instance. b paired spans e (no
     // dependency); b unpaired is an instant at 08:45 after e (e>b=1).
    const std::string nan = "<float key=\"concept:instance\" value=\"NaN\"/>";
    const std::string two = "<float key=\"concept:instance\" value=\"2.0\"/>";
    const std::string x = "<string key=\"concept:instance\" value=\"x\"/>";
    const std::string text_trace =
        "<trace><string key=\"concept:name\" value=\"c11\"/><event><string key=\"concept:name\" value=\"a\"/>"
        "<date key=\"time:timestamp\" value=\"2021-03-01T08:30:00.000+00:00\"/>"
        "<string key=\"lifecycle:transition\" value=\"complete\"/>" + x + "</event></trace>\n";
    struct Case {
      std::string label, log, shape, expected;
    };
    const std::vector<Case> cases = {
        {"log, NaN instances never pair", lifecycle_xes(nan, two, nan), "", "e>b=1"},
        {"log, missing instances pair", lifecycle_xes("", two, ""), "", ""},
        {"pandas float64 column, NaN never pairs", lifecycle_xes(nan, two, nan), "dataframe", "e>b=1"},
        {"pandas float64 column, gaps never pair", lifecycle_xes("", two, ""), "dataframe", "e>b=1"},
        {"pandas str column, NaN pairs", lifecycle_xes(nan, x, nan), "dataframe", ""},
        {"pandas str column, NaN pairs with a gap", lifecycle_xes(nan, x, ""), "dataframe", ""},
        {"pandas object column, NaN floats never pair", lifecycle_xes(nan, two, nan, text_trace), "dataframe",
         "e>b=1"},
        {"pandas object column, gaps pair", lifecycle_xes("", two, "", text_trace), "dataframe", ""},
        {"Polars, NaN is null and pairs with a gap", lifecycle_xes(nan, two, ""), "lazyframe", ""},
        {"Polars, object column NaN pairs with a gap", lifecycle_xes(nan, two, "", text_trace), "lazyframe", ""},
    };
    for (const auto& item : cases)
      check("Heuristics++ instances: " + item.label, plusplus_dependencies(client, item.log, item.shape),
            item.expected);
  }

  check_fits(client, "cached POWL net",
             "c10:d@14 d@14 d@14 d@16|c11:c@43 a@43 e@47 c@47 c@51 c@51 e@51|c0:d@25|c13:e@8 c@12 e@13 e@15|"
             "c14:d@11 a@11 a@11 d@11 a@13",
             "powl", "powl_petri_net");
  check_fits(client, "cached heuristics net",
             "c10:b@43 e@46 c@51 c@52 d@57 c@57|c11:c@11 c@13 a@13|:c@29 e@24|c13:e@51 b@48|c14:e@27|:c@39|"
             "c16:b@42 c@47 b@52 e@54 d@54 c@54 d@55 d@57|c17:c@27 d@33 e@32 b@38 d@31 b@38|"
             "c18:b@51 c@54 c@54 c@55 d@55 d@55 d@55|c19:e@16 e@17 e@22 e@26|c20:b@35 c@37 d@41 a@43 d@48 d@51",
             "heuristic", "heuristic_petri_net");

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures != 0) {
    std::cerr << g_failures << " round GG check(s) failed\n";
    return 1;
  }
  std::cout << "server review GG (stochastic fits, Heuristics++ instances): all checks passed\n";
  return 0;
}
