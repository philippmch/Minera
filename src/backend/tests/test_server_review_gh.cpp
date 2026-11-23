#undef NDEBUG
// Review GH: Heuristics++ on a CSV upload's default (log) shape, and the
// alignments of an upload without traces.
//
// The log shape of a CSV upload is pm4py's convert_to_event_log of its
// frame (read_csv + format_dataframe): every event holds every column,
// typed by the column, a missing cell as NaN. Heuristics++ reads it so:
//
// - the lifecycle pairing keys a start by its activity and the instance's
//   frame value, so "1" and "1.0" of a float64 column pair (Minera compared
//   the CSV text) while "01" and "1" of a str column stay apart;
// - to_interval lower-cases each event's lifecycle:transition: a missing
//   cell (NaN) raises AttributeError ('float' object has no attribute
//   'lower'), an int column likewise ('int'); a lifecycle column under
//   another header is an ordinary column, so every event is a complete
//   (Minera paired its starts and skipped gaps);
// - a start_timestamp column keeps the log as it is even where the first
//   event's cell is missing, and the abstraction then raises as on an XES
//   upload's frame: NaT ValueError, a str, int or float column TypeError at
//   the eventually-follows comparison (Minera tested the first event only and
//   reported its own start-timestamp errors).
//
// pm4py's log alignments check easy soundness before reading the log: an
// XES upload without traces has a heuristics net without transitions, whose
// final marking is unreachable, so alignment fitness and /api/align fail
// there as on a non-empty log ("trying to apply alignments on a Petri net
// that is not a easy sound net!!"; Minera returned fitness 0 and an empty
// alignment). The POWL net of the same upload is sound: fitness 0, nothing
// to align.
//
// Expected values are pm4py 2.7.22 black-box on the same files
// (review-harness round-gh, ct/hppcsv.py: apply_heu(PLUSPLUS) on
// convert_to_event_log(format_dataframe(read_csv))).
//
// Usage: minera_server_csv_plusplus_gh_tests <minera_server>

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

// The Heuristics++ dependency measures of a CSV upload's default shape as
// "source>target=value" by activity name, or "error: <message>".
std::string csv_plusplus(httplib::Client& client, const std::string& csv) {
  httplib::MultipartFormDataItems items = {
      {"include_traces", "false", "", ""},
      {"heuristic_variant", "plusplus", "", ""},
      {"case_column", "case:concept:name", "", ""},
      {"activity_column", "concept:name", "", ""},
      {"timestamp_column", "time:timestamp", "", ""},
      {"log", csv, "gh1.csv", "text/csv"},
  };
  const auto result = client.Post("/api/mine", items);
  if (!result) return "no response";
  const auto& body = result->body;
  if (body.find("\"error\":") != std::string::npos && body.find("\"heuristics_net\":") == std::string::npos)
    return "error: " + field(body, "error");
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

  struct Case {
    std::string label, csv, expected;
  };
  const std::vector<Case> cases = {
      {"float64 instances, \"1\" pairs with \"1.0\"",
       "case:concept:name,concept:name,time:timestamp,lifecycle:transition,concept:instance\nc1,a,2021-03-01 08:00:00,complete,2\nc1,b,2021-03-01 08:01:00,start,1\nc1,c,2021-03-01 08:02:00,complete,2.5\nc1,b,2021-03-01 08:03:00,complete,1.0\nc1,d,2021-03-01 08:04:00,complete,2\n",
       "a>b=1 b>d=1 c>d=1"},
      {"int64 instances, 2 and 1 stay apart",
       "case:concept:name,concept:name,time:timestamp,lifecycle:transition,concept:instance\nc1,b,2021-03-01 08:01:00,start,2\nc1,c,2021-03-01 08:02:00,complete,1\nc1,b,2021-03-01 08:03:00,complete,1\nc1,d,2021-03-01 08:04:00,complete,1\n",
       "b>d=1 c>b=1"},
      {"str instances, \"01\" and \"1\" stay apart",
       "case:concept:name,concept:name,time:timestamp,lifecycle:transition,concept:instance\nc1,b,2021-03-01 08:01:00,start,01\nc1,c,2021-03-01 08:02:00,complete,x\nc1,b,2021-03-01 08:03:00,complete,1\nc1,d,2021-03-01 08:04:00,complete,1\n",
       "b>d=1 c>b=1"},
      {"a missing lifecycle cell raises",
       "case:concept:name,concept:name,time:timestamp,lifecycle:transition,concept:instance\nc1,a,2021-03-01 08:00:00,start,1\nc1,a,2021-03-01 08:01:00,,1\nc1,b,2021-03-01 08:02:00,complete,1\n",
       "error: 'float' object has no attribute 'lower'"},
      {"an int lifecycle column raises",
       "case:concept:name,concept:name,time:timestamp,lifecycle:transition\nc1,a,2021-03-01 08:00:00,1\nc1,b,2021-03-01 08:01:00,0\n",
       "error: 'int' object has no attribute 'lower'"},
      {"a lifecycle column under another header is ignored",
       "case:concept:name,concept:name,time:timestamp,lifecycle\nc1,a,2021-03-01 08:00:00,start\nc1,a,2021-03-01 08:01:00,\nc1,b,2021-03-01 08:02:00,complete\n",
       "a>a=0 a>b=1"},
      {"a start column with a missing first cell",
       "case:concept:name,concept:name,time:timestamp,start_timestamp,lifecycle:transition\nc1,a,2021-03-01 08:00:00,,start\nc1,a,2021-03-01 08:01:00,2021-03-01 07:59:00,complete\nc1,b,2021-03-01 08:02:00,2021-03-01 08:01:30,\n",
       "error: NaTType does not support timestamp"},
      {"a str start column",
       "case:concept:name,concept:name,time:timestamp,start_timestamp\nc1,a,2021-03-01 08:00:00,early\nc1,b,2021-03-01 08:01:00,2021-03-01 08:00:30\n",
       "error: '<=' not supported between instances of 'Timestamp' and 'str'"},
      {"an int start column",
       "case:concept:name,concept:name,time:timestamp,start_timestamp\nc1,a,2021-03-01 08:00:00,5\nc1,b,2021-03-01 08:01:00,6\n",
       "error: '<=' not supported between instances of 'Timestamp' and 'int'"},
      {"a float start column",
       "case:concept:name,concept:name,time:timestamp,start_timestamp\nc1,a,2021-03-01 08:00:00,5.5\nc1,b,2021-03-01 08:01:00,\n",
       "error: '<=' not supported between instances of 'Timestamp' and 'float'"},
      {"an all-missing start column",
       "case:concept:name,concept:name,time:timestamp,start_timestamp\nc1,a,2021-03-01 08:00:00,\nc1,b,2021-03-01 08:01:00,\n",
       "error: '<=' not supported between instances of 'Timestamp' and 'float'"},
  };
  for (const auto& item : cases)
    check("CSV Heuristics++: " + item.label, csv_plusplus(client, item.csv), item.expected);

  // An XES upload without traces, then the alignment routes on its cached nets.
  {
    const std::string empty_xes =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
        "<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n</log>\n";
    httplib::MultipartFormDataItems items = {
        {"include_traces", "false", "", ""},
        {"log", empty_xes, "gh1_empty.xes", "application/xml"},
    };
    const auto mined = client.Post("/api/mine", items);
    check("empty XES upload mines", mined ? field(mined->body, "trace_count") : "no response", "0");
    const auto outcome = [](const httplib::Result& result) -> std::string {
      if (!result) return "no response";
      const auto& body = result->body;
      return "computed=" + field(body, "computed") + " error=" + field(body, "error") +
             " fitness=" + field(body, "fitness");
    };
    const auto evaluate = [&](const std::string& miner) {
      return outcome(client.Post("/api/evaluate?miner=" + miner +
                                     "&evaluation_family=replay_fitness&fitness_variant=alignment_based",
                                 "", "application/x-www-form-urlencoded"));
    };
    const auto align = [&](const std::string& miner) {
      return outcome(client.Post("/api/align", "miner=" + miner, "application/x-www-form-urlencoded"));
    };
    check("empty log: heuristics alignment fitness fails", evaluate("heuristic"),
          "computed=false error=final marking is unreachable fitness=0");
    check("empty log: POWL alignment fitness is 0", evaluate("powl"), "computed=true error=null fitness=0");
    check("empty log: /api/align on the heuristics net fails", align("heuristic"),
          "computed=false error=alignment not computed: final marking unreachable or invalid alignment "
          "options fitness=null");
    check("empty log: /api/align on the POWL net aligns nothing", align("powl"),
          "computed=true error= fitness=1");
  }

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures != 0) {
    std::cerr << g_failures << " round GH check(s) failed\n";
    return 1;
  }
  std::cout << "server review GH (Heuristics++ on CSV uploads, empty-log alignments): all checks passed\n";
  return 0;
}
