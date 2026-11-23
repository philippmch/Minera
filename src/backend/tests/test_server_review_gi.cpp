#undef NDEBUG
// Review GI: Heuristics++ on CSV uploads' int start columns and frame shapes,
// and the alignment consumers of an upload without traces.
//
// - A CSV start_timestamp column of ints beyond int64 is a uint64 column,
//   one beyond uint64 (or below int64) an object column of Python ints with
//   NaN in its gaps; Heuristics++ on the log shape compares each event's
//   completion with the later starts and raises TypeError on the first such
//   pair (Timestamp and int, or Timestamp and float for a NaN), where Minera
//   reported its own start-timestamp error.
// - On the DataFrame and LazyFrame shapes pm4py reads format_dataframe's
//   frame: a missing lifecycle:transition cell raises AttributeError on
//   float (pandas NaN) or NoneType (Polars), a column of ints, bools or dates
//   on int, bool, Timestamp or datetime.datetime (Minera mined a net); a
//   start column raises as on the log shape, with datetime.datetime and None
//   on Polars (Minera reported its own error).
// - pm4py's log alignments check easy soundness before reading the log, so
//   an upload without traces fails on a net whose final marking is
//   unreachable for a supplied model on /api/align, the Petri alignment
//   workbench and the export's standard alignments too (Minera aligned
//   nothing, fitness 1); the decomposed fitness of a net without
//   transitions raises whatever the log (Minera returned fitness 0 for an
//   empty log).
//
// Expected values are pm4py 2.7.22 black-box on the same inputs
// (review-harness round-gi: ct/refcsv.py, ct/hppcsv3.py, probes/emptyref.py).
//
// Usage: minera_server_review_gi_tests <minera_server> <minera_export>

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


// The Heuristics++ outcome of a CSV upload on `shape` (log, dataframe or
// lazyframe): "error: <message>" or the dependency measures
// "source>target=value" by activity name.
std::string csv_plusplus(httplib::Client& client, const std::string& csv, const std::string& shape,
                         bool performance = false) {
  httplib::MultipartFormDataItems items = {
      {"include_traces", "false", "", ""},
      {"heuristic_variant", "plusplus", "", ""},
      {"case_column", "case:concept:name", "", ""},
      {"activity_column", "concept:name", "", ""},
      {"timestamp_column", "time:timestamp", "", ""},
      {"log", csv, "gi1.csv", "text/csv"},
  };
  if (shape != "log") items.push_back({"mining_input_shape", shape, "", ""});
  if (performance) items.push_back({"heuristic_decoration", "performance", "", ""});
  const auto result = client.Post("/api/mine", items);
  if (!result) return "no response";
  const auto& body = result->body;
  if (shape == "log") {
    if (body.find("\"heuristics_net\":") == std::string::npos) return "error: " + field(body, "error");
    const auto net = body.find("\"heuristics_net\":");
    const auto names_at = body.find("\"activities\":");
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
  const auto families = body.find("\"family_results\":");
  if (families == std::string::npos) return "no family results: " + body.substr(0, 200);
  const auto at = body.find("\"heuristic\":", families);
  if (at == std::string::npos) return "no heuristic family";
  const auto family = value_at(body, body.find('{', at));
  if (field(family, "executed") != "true") {
    const auto error = family.find("\"error\":", family.find("\"result\":"));
    return "error: " + field(family.substr(error), "message");
  }
  const auto result_at = family.find("\"result\":");
  const auto names_at = family.find("\"activities\":", result_at);
  const auto names_array = value_at(family, family.find('[', names_at));
  std::vector<std::string> names;
  for (std::size_t open = names_array.find('"'); open != std::string::npos; ) {
    const auto close = names_array.find('"', open + 1);
    names.push_back(names_array.substr(open + 1, close - open - 1));
    open = names_array.find('"', close + 1);
  }
  std::string out;
  for (const auto& edge : objects(family, family.find('[', family.find("\"edges\":", result_at)))) {
    const auto source = std::stoul(field(edge, "source")), target = std::stoul(field(edge, "target"));
    out += (out.empty() ? "" : " ") + (source < names.size() ? names[source] : "?") + ">" +
           (target < names.size() ? names[target] : "?") + "=" + field(edge, "dependency");
    if (performance) out += "@" + field(edge, "representation");
  }
  return out;
}

std::string outcome(const httplib::Result& result) {
  if (!result) return "no response";
  const auto& body = result->body;
  if (body.rfind("{\"error\":", 0) == 0) return "error: " + field(body, "error");
  return "computed=" + field(body, "computed") + " fitness=" + field(body, "fitness");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: " << argv[0] << " <minera_server> <minera_export>\n";
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

  const std::string head = "case:concept:name,concept:name,time:timestamp,";
  struct Case {
    std::string label, csv, log, dataframe, lazyframe;
  };
  const std::vector<Case> cases = {
      {"a uint64 start column",
       head + "start_timestamp\nc1,a,2021-03-01 08:00:00,9223372036854775808\nc1,b,2021-03-01 08:01:00,3\n",
       "error: '<=' not supported between instances of 'Timestamp' and 'int'",
       "error: '<=' not supported between instances of 'Timestamp' and 'int'",
       "error: '<=' not supported between instances of 'datetime.datetime' and 'int'"},
      {"an object start column of ints beyond uint64 with a gap",
       head + "start_timestamp\nc1,a,2021-03-01 08:00:00,18446744073709551616\nc1,b,2021-03-01 08:01:00,\nc2,a,2021-03-01 08:02:00,2\n",
       "error: '<=' not supported between instances of 'Timestamp' and 'float'",
       "error: '<=' not supported between instances of 'Timestamp' and 'float'", ""},
      {"an object start column below int64",
       head + "start_timestamp\nc1,a,2021-03-01 08:00:00,\nc1,b,2021-03-01 08:01:00,-9223372036854775809\n",
       "error: '<=' not supported between instances of 'Timestamp' and 'int'",
       "error: '<=' not supported between instances of 'Timestamp' and 'int'", ""},
      {"a missing lifecycle cell",
       head + "lifecycle:transition\nc1,a,2021-03-01 08:00:00,start\nc1,a,2021-03-01 08:01:00,\nc1,b,2021-03-01 08:02:00,complete\n",
       "error: 'float' object has no attribute 'lower'", "error: 'float' object has no attribute 'lower'",
       "error: 'NoneType' object has no attribute 'lower'"},
      {"an int lifecycle column",
       head + "lifecycle:transition\nc1,a,2021-03-01 08:00:00,1\nc1,b,2021-03-01 08:01:00,0\n",
       "error: 'int' object has no attribute 'lower'", "error: 'int' object has no attribute 'lower'",
       "error: 'int' object has no attribute 'lower'"},
      {"a date lifecycle column",
       head + "lifecycle:transition\nc1,a,2021-03-01 08:00:00,2021-03-01 08:00:00\nc1,b,2021-03-01 08:01:00,2021-03-01 08:01:00\n",
       "error: 'Timestamp' object has no attribute 'lower'", "error: 'Timestamp' object has no attribute 'lower'",
       "error: 'datetime.datetime' object has no attribute 'lower'"},
      {"a bool lifecycle column",
       head + "lifecycle:transition\nc1,a,2021-03-01 08:00:00,True\nc1,b,2021-03-01 08:01:00,False\n",
       "error: 'bool' object has no attribute 'lower'", "error: 'bool' object has no attribute 'lower'",
       "error: 'bool' object has no attribute 'lower'"},
      {"a date start column with a missing cell",
       head + "start_timestamp\nc1,a,2021-03-01 08:00:00,\nc1,b,2021-03-01 08:01:00,2021-03-01 08:00:30\n",
       "error: NaTType does not support timestamp", "error: NaTType does not support timestamp",
       "error: '<' not supported between instances of 'datetime.datetime' and 'NoneType'"},
      {"an int start column",
       head + "start_timestamp\nc1,a,2021-03-01 08:00:00,5\nc1,b,2021-03-01 08:01:00,6\n",
       "error: '<=' not supported between instances of 'Timestamp' and 'int'",
       "error: '<=' not supported between instances of 'Timestamp' and 'int'",
       "error: '<=' not supported between instances of 'datetime.datetime' and 'int'"},
      {"lifecycle pairs without gaps",
       head + "lifecycle:transition,concept:instance\nc1,a,2021-03-01 08:00:00,start,1\nc1,a,2021-03-01 08:01:00,complete,1\nc1,b,2021-03-01 08:02:00,complete,1\nc2,a,2021-03-01 08:03:00,complete,2\nc2,b,2021-03-01 08:04:00,complete,2\nc3,a,2021-03-01 08:05:00,complete,1\nc3,b,2021-03-01 08:06:00,complete,1\n",
       "a>b=1", "a>b=1.0", "a>b=1.0"},
  };
  for (const auto& item : cases) {
    check("CSV Heuristics++ log shape: " + item.label, csv_plusplus(client, item.csv, "log"), item.log);
    check("CSV Heuristics++ DataFrame: " + item.label, csv_plusplus(client, item.csv, "dataframe"), item.dataframe);
    // Polars' copy of an object column needs pyarrow in pm4py's environment
    // (declared): no LazyFrame expectation.
    if (!item.lazyframe.empty())
      check("CSV Heuristics++ LazyFrame: " + item.label, csv_plusplus(client, item.csv, "lazyframe"), item.lazyframe);
  }

  // The frame's instances pair by its dtype: a float64 column holds each NaN
  // as its own float (pairs with nothing), Polars' copy holds the one null
  // (pairs with a null) and 1 beside 1.0 as the same float.
  const std::string instances = head + "lifecycle:transition,concept:instance\n";
  const std::string nan_csv = instances +
                              "c0,b,2021-03-01 08:14:00,start,NaN\nc0,b,2021-03-01 08:14:00,complete,1\n"
                              "c0,d,2021-03-01 08:16:00,start,nan\nc0,d,2021-03-01 08:18:00,complete,\n";
  const std::string float_csv = instances +
                                "c0,d,2021-03-01 08:23:00,complete,2.0\nc1,b,2021-03-01 08:15:00,start,1.0\n"
                                "c1,b,2021-03-01 08:18:00,complete,\nc1,b,2021-03-01 08:22:00,start,1.5\n"
                                "c1,b,2021-03-01 08:25:00,complete,1\nc1,d,2021-03-01 08:26:00,start,1\n"
                                "c1,d,2021-03-01 08:26:00,complete,1.5\n";
  check("CSV Heuristics++ DataFrame: NaN instances pair with nothing",
        csv_plusplus(client, nan_csv, "dataframe", true), "b>d=1.0@240.0");
  check("CSV Heuristics++ LazyFrame: null instances pair", csv_plusplus(client, nan_csv, "lazyframe", true),
        "b>d=1.0@120.0");
  check("CSV Heuristics++ DataFrame: 1 pairs with 1.0", csv_plusplus(client, float_csv, "dataframe", true),
        "b>d=1.0@480.0");
  check("CSV Heuristics++ LazyFrame: 1 pairs with 1.0", csv_plusplus(client, float_csv, "lazyframe", true),
        "b>d=1.0@480.0");

  // An XES upload without traces, a supplied net without transitions.
  const std::string empty_xes =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
      "<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n</log>\n";
  const std::string bare_pnml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<pnml><net id=\"n\" type=\"http://www.pnml.org/version-2009/grammar/pnmlcoremodel\"><page id=\"p\">\n"
      "<place id=\"source\"><name><text>source</text></name><initialMarking><text>1</text></initialMarking></place>\n"
      "<place id=\"sink\"><name><text>sink</text></name></place>\n"
      "</page><finalmarkings><marking><place idref=\"sink\"><text>1</text></place></marking></finalmarkings></net></pnml>\n";
  {
    httplib::MultipartFormDataItems items = {
        {"include_traces", "false", "", ""},
        {"log", empty_xes, "gi1_empty.xes", "application/xml"},
    };
    const auto mined = client.Post("/api/mine", items);
    check("empty XES upload mines", mined ? field(mined->body, "trace_count") : "no response", "0");
    httplib::MultipartFormDataItems model = {{"model", bare_pnml, "gi1_bare.pnml", "application/xml"}};
    const auto imported = client.Post("/api/check-model", model);
    check("the bare net imports", imported ? std::to_string(imported->status) : "no response", "200");
    check("empty log: /api/align on the supplied net fails",
          outcome(client.Post("/api/align", "miner=imported", "application/x-www-form-urlencoded")),
          "computed=false fitness=null");
    check("empty log: /api/align on the Inductive net aligns nothing",
          outcome(client.Post("/api/align", "miner=inductive", "application/x-www-form-urlencoded")),
          "computed=true fitness=1");
    const auto decomposed = [&](const std::string& miner) {
      return outcome(client.Post("/api/evaluate?miner=" + miner +
                                     "&evaluation_family=replay_fitness&fitness_variant=alignment_based"
                                     "&evaluation_alignment_variant=recompos_maximal",
                                 "", "application/x-www-form-urlencoded"));
    };
    check("empty log: decomposed fitness of the heuristics net raises", decomposed("heuristic"),
          "error: decomposed alignment requires a net with at least one transition");
    check("empty log: decomposed fitness of the supplied net raises", decomposed("imported"),
          "error: decomposed alignment requires a net with at least one transition");
    const auto inductive = client.Post("/api/evaluate?miner=inductive&evaluation_family=replay_fitness"
                                       "&fitness_variant=alignment_based&evaluation_alignment_variant=recompos_maximal",
                                       "", "application/x-www-form-urlencoded");
    check("empty log: decomposed fitness of the Inductive net is 0",
          inductive ? field(inductive->body, "computed") + " " + field(inductive->body, "fitness") : "no response",
          "true 0");
    httplib::MultipartFormDataItems workbench = {
        {"log", empty_xes, "gi1_empty.xes", "application/xml"},
        {"model", bare_pnml, "gi1_bare.pnml", "application/xml"},
    };
    const auto petri = client.Post("/api/align-petri-workbench", workbench);
    const auto result_at = petri ? petri->body.find("\"result\":") : std::string::npos;
    check("empty log: the Petri alignment workbench fails on the bare net",
          result_at == std::string::npos ? "no result"
                                         : "computed=" + field(petri->body.substr(result_at), "computed"),
          "computed=false");
  }
  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);

  // The export's standard alignments of the same upload.
  {
    char log_path[] = "/tmp/minera_gi_emptyXXXXXX";
    char model_path[] = "/tmp/minera_gi_bareXXXXXX";
    char out_path[] = "/tmp/minera_gi_outXXXXXX";
    const int log_fd = ::mkstemp(log_path), model_fd = ::mkstemp(model_path), out_fd = ::mkstemp(out_path);
    if (log_fd < 0 || model_fd < 0 || out_fd < 0) std::abort();
    if (::write(log_fd, empty_xes.data(), empty_xes.size()) != static_cast<ssize_t>(empty_xes.size())) std::abort();
    if (::write(model_fd, bare_pnml.data(), bare_pnml.size()) != static_cast<ssize_t>(bare_pnml.size())) std::abort();
    ::close(log_fd);
    ::close(model_fd);
    ::close(out_fd);
    const std::string xes_path = std::string(log_path) + ".xes";
    ::rename(log_path, xes_path.c_str());
    std::vector<std::string> export_args = {argv[2], xes_path, "--check-pnml", model_path, "-o", out_path};
    std::vector<char*> export_raw;
    for (auto& arg : export_args) export_raw.push_back(arg.data());
    export_raw.push_back(nullptr);
    pid_t exporter = 0;
    if (::posix_spawn(&exporter, argv[2], nullptr, nullptr, export_raw.data(), environ) != 0) {
      std::cerr << "cannot start " << argv[2] << '\n';
      return 2;
    }
    int export_status = 0;
    ::waitpid(exporter, &export_status, 0);
    std::string json;
    if (FILE* file = std::fopen(out_path, "rb")) {
      char buffer[65536];
      for (std::size_t read = 0; (read = std::fread(buffer, 1, sizeof buffer, file)) > 0; ) json.append(buffer, read);
      std::fclose(file);
    }
    ::unlink(xes_path.c_str());
    ::unlink(model_path);
    ::unlink(out_path);
    const auto conformance = json.find("\"conformance\":");
    const auto alignments_of = [&](const std::string& miner) {
      if (conformance == std::string::npos) return std::string("no conformance");
      const auto at = json.find("\"" + miner + "\":", conformance);
      if (at == std::string::npos) return std::string("no model");
      const auto model = value_at(json, json.find('{', at));
      const auto alignments = model.find("\"alignments\":");
      if (alignments == std::string::npos) return std::string("no alignments");
      return "computed=" + field(model.substr(alignments), "computed");
    };
    check("export: empty log, heuristics net alignments fail", alignments_of("heuristic"), "computed=false");
    check("export: empty log, supplied net alignments fail", alignments_of("imported"), "computed=false");
    check("export: empty log, Inductive net aligns nothing", alignments_of("inductive"), "computed=true");
  }

  if (g_failures != 0) {
    std::cerr << g_failures << " round GI check(s) failed\n";
    return 1;
  }
  std::cout << "server review GI (CSV Heuristics++ frames and int starts, empty-log alignments): all checks passed\n";
  return 0;
}
