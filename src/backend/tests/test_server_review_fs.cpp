#undef NDEBUG
// Review FS: routes on CSV uploads against pm4py 2.7.22's DataFrame paths on
// the same upload (format_dataframe over read_csv, its Polars copy):
//  - /api/mine's flat footprints read the upload's frame, whose case:st
//    column keeps every row's own cell (the log keeps one per case), so the
//    Polars variant sorts by them;
//  - /api/dfg-dataframe's Polars partial order pairs the rows by the frame's
//    own "@@index" and measures whole seconds;
//  - /api/analyze-behavior's trace-based correlation reads a projected log
//    that keeps only concept:name and time:timestamp: another completion key
//    or a start key fails there (KeyError, or TypeError for concept:name).
// Every expected value was observed black-box with pm4py on the same uploads
// (review-harness round-fs, rt/rt.py).
//
// Usage: minera_server_review_fs_tests <minera_server>

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
#include <regex>
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

// case:st varies within a case (round FS fp/fsA f129.csv).
const std::string kFootprintsCsv =
    "case,act,ts,case:st,st\n"
    "c1,a,2020-01-01 00:11:01Z,2020-01-01 00:11:01.5Z,2020-01-01 00:11:01Z\n"
    "c1,b,2020-01-01 00:11:00Z,2020-01-01 00:11:00.123456789Z,2020-01-01 00:11:00.999999999Z\n"
    "c1,c,2020-01-01 00:11:01Z,2020-01-01 00:10:31.000001Z,2020-01-01 00:11:00Z\n"
    "c1,b,2020-01-01 00:11:31Z,2020-01-01 00:11:30Z,2020-01-01 00:11:31.000001Z\n"
    "c0,c,2020-01-01 00:42:00Z,2020-01-01 00:41:30.000000500Z,2020-01-01 00:42:00.5Z\n"
    "c0,c,2020-01-01 00:42:30Z,2020-01-01 00:42:30Z,2020-01-01 00:42:29Z\n";

// b starts before a, both complete at 00:22:00 (round FS po/poB p104.csv).
const std::string kPartialOrderCsv =
    "case,act,ts,case:st,st\n"
    "c1,a,2020-01-01 00:22:00,2020-01-01 00:22:00,2020-01-01 00:22:00\n"
    "c1,b,2020-01-01 00:22:00,2020-01-01 00:22:00,2020-01-01 00:21:59\n"
    "c0,c,2020-01-01 00:02:00,2020-01-01 00:02:00.000001,2020-01-01 00:02:00\n"
    "c0,c,2020-01-01 00:12:00,2020-01-01 00:02:00.000001,2020-01-01 00:11:40\n"
    "c1,d,2020-01-01 00:22:30,2020-01-01 00:22:00,2020-01-01 00:22:30\n";

// The JSON array that follows `key` at or after `from` (brackets balanced).
std::string array_after(const std::string& body, const std::string& key, std::size_t from = 0) {
  const auto at = body.find(key, from);
  if (at == std::string::npos) return "<missing " + key + ">";
  const auto begin = body.find('[', at);
  int depth = 0;
  for (auto end = begin; end < body.size(); ++end) {
    if (body[end] == '[') ++depth;
    else if (body[end] == ']' && --depth == 0) return body.substr(begin, end - begin + 1);
  }
  return "<unbalanced " + key + ">";
}

// The footprint's directly-follows pairs as sorted "a>b:count" labels, named
// by the response's top-level activities.
std::string footprint_dfg(const std::string& body, const std::string& variant) {
  std::vector<std::string> names;
  const std::regex name_pattern("\"name\": \"([^\"]*)\"");
  const auto activities = array_after(body, "\"activities\": [");
  for (std::sregex_iterator it(activities.begin(), activities.end(), name_pattern), end; it != end; ++it)
    names.push_back((*it)[1]);
  const auto dfg = array_after(body, "\"dfg\"", body.find("\"" + variant + "\""));
  const std::regex pair_pattern("\"source\": (\\d+), \"target\": (\\d+), \"count\": (\\d+)");
  std::vector<std::string> pairs;
  for (std::sregex_iterator it(dfg.begin(), dfg.end(), pair_pattern), end; it != end; ++it) {
    const auto source = std::stoul((*it)[1]), target = std::stoul((*it)[2]);
    if (source >= names.size() || target >= names.size()) return "<bad index>";
    pairs.push_back(names[source] + ">" + names[target] + ":" + std::string((*it)[3]));
  }
  std::sort(pairs.begin(), pairs.end());
  std::string out;
  for (const auto& pair : pairs) out += (out.empty() ? "" : ",") + pair;
  return out;
}

// The partial order's pairs as sorted "source>target:flow" labels of the
// index values.
std::string partial_order(const std::string& body) {
  const std::regex pattern(
      "\"source_index_value\":\"([^\"]*)\",\"target_index_value\":\"([^\"]*)\",\"flow_time_seconds\":([-0-9.e]+)");
  std::vector<std::string> pairs;
  for (std::sregex_iterator it(body.begin(), body.end(), pattern), end; it != end; ++it)
    pairs.push_back(std::string((*it)[1]) + ">" + std::string((*it)[2]) + ":" + std::string((*it)[3]));
  std::sort(pairs.begin(), pairs.end());
  std::string out;
  for (const auto& pair : pairs) out += (out.empty() ? "" : ",") + pair;
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
  const auto mine = [&](const std::string& csv, const std::string& name,
                        const std::vector<std::pair<std::string, std::string>>& extra = {}) {
    httplib::MultipartFormDataItems items = {{"log", csv, name, "text/csv"},
                                             {"include_traces", "false", "", ""},
                                             {"case_column", "case", "", ""},
                                             {"activity_column", "act", "", ""},
                                             {"timestamp_column", "ts", "", ""}};
    for (const auto& [key, value] : extra) items.push_back({key, value, "", ""});
    auto result = client.Post("/api/mine", items);
    return result && result->status == 200 ? result->body : std::string("mine failed");
  };

  // Footprints: pm4py's Polars variant sorts by the rows' case:st cells.
  for (const std::string start : {"case:st", "st"}) {
    const auto body = mine(kFootprintsCsv, "fs1.csv",
                           {{"footprints_discovery_variants", "polars_lazyframes,entire_dataframe"},
                            {"footprints_start_timestamp_attribute", start}});
    check("mine footprints polars_lazyframes start " + start, footprint_dfg(body, "polars_lazyframes"),
          "a>b:1,b>a:1,c>b:1,c>c:1");
    check("mine footprints entire_dataframe start " + start, footprint_dfg(body, "entire_dataframe"),
          "a>c:1,b>a:1,c>b:1,c>c:1");
  }

  // Trace-based correlation on the projected log.
  mine(kFootprintsCsv, "fs1.csv");
  const auto schema = client.Get("/api/log-query/schema");
  const std::regex source_pattern("\"source_id\":\"([^\"]*)\"");
  std::smatch source_match;
  const std::string schema_body = schema ? schema->body : std::string();
  const std::string source_id =
      std::regex_search(schema_body, source_match, source_pattern) ? std::string(source_match[1]) : "";
  const auto correlation = [&](const std::string& adapter, const std::string& key, const std::string& value) {
    httplib::Params params = {{"family", "correlation"}, {"variant", "trace_based"},
                              {"adapter", adapter}, {"source_id", source_id}};
    if (!key.empty()) params.emplace(key, value);
    auto result = client.Post("/api/analyze-behavior", params);
    if (!result) return std::string("no response");
    return result->status == 200 ? std::string("ok") : result->body;
  };
  for (const std::string adapter : {"dataframe", "log", "stream", "lazyframe"}) {
    check("trace-based start st " + adapter,
          correlation(adapter, "correlation_start_timestamp_attribute", "st"), "{\"error\": \"KeyError: 'st'\"}");
    check("trace-based completion ts " + adapter,
          correlation(adapter, "correlation_timestamp_attribute", "ts"), "{\"error\": \"KeyError: 'ts'\"}");
    check("trace-based start concept:name " + adapter,
          correlation(adapter, "correlation_start_timestamp_attribute", "concept:name"),
          "{\"error\": \"TypeError: '<' not supported between instances of 'float' and 'str'\"}");
    check("trace-based start time:timestamp " + adapter,
          correlation(adapter, "correlation_start_timestamp_attribute", "time:timestamp"), "ok");
    check("trace-based default " + adapter, correlation(adapter, "", ""), "ok");
  }

  // Polars partial order: the frame's @@index, whole seconds.
  mine(kPartialOrderCsv, "fs2.csv");
  auto polars = client.Get("/api/dfg-dataframe?dfg_backend=polars&dfg_operation=partial_order&dfg_start_timestamp_attribute=st");
  check("polars partial order start st", polars ? partial_order(polars->body) : "no response",
        "0>1:580,2>4:30,3>4:30");

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures != 0) {
    std::cerr << g_failures << " review FS route check(s) failed\n";
    return 1;
  }
  std::cout << "review FS: upload footprints, Polars partial order and trace-based correlation keys passed\n";
  return 0;
}
