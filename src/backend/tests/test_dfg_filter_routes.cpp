#undef NDEBUG
// Review W2: GET /api/dfg and minera_dfg filter a DFG through the reference
// port (algo.filtering.dfg.dfg_filtering), not the earlier native filters.
// The server passes the mined log's real activity counts, as a reference user
// passes discover_dfg(log) and get_event_attribute_values(log, activity); the
// CLI reads a .dfg file without counts and derives them like the reference's
// public filter_dfg_activities_percentage / filter_dfg_paths_percentage
// wrappers (max of the incoming and outgoing edge sums). Every expected body
// is the pinned reference's result on the same input, written in the .dfg
// layout (activities sorted, then starts, ends and edges).
//
// Usage: minera_dfg_filter_route_tests <minera_server> <minera_dfg> [port]

#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "httplib.h"
#include "minera/dfg_filtering.hpp"
#include "minera/dfg_io.hpp"

extern char** environ;

namespace {

int g_failures = 0;

void check(bool condition, const std::string& what) {
  if (!condition) {
    ++g_failures;
    std::cerr << "FAIL: " << what << '\n';
  }
}

int free_port() {
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

// One case per word, one event per letter, a minute apart.
std::string csv_log(const std::vector<std::string>& traces) {
  std::ostringstream csv;
  csv << "case_id,activity,timestamp\n";
  int minute = 0;
  for (std::size_t c = 0; c < traces.size(); ++c) {
    for (const char activity : traces[c]) {
      csv << 'c' << c << ',' << activity << ",2024-01-01T" << (minute / 60 < 10 ? "0" : "") << minute / 60
          << ':' << (minute % 60 < 10 ? "0" : "") << minute % 60 << ":00Z\n";
      ++minute;
    }
  }
  return csv.str();
}

struct ServerCase {
  std::vector<std::string> traces;
  std::string query;
  int status;
  std::string body;  // the exact .dfg body, or a substring of the error
};

struct CliCase {
  std::string name;
  std::string dfg;
  std::vector<std::string> args;
  int exit_code;
  std::string output;  // the exact .dfg on stdout, or a substring of stderr
};

std::string shell_quote(const std::string& text) {
  std::string out = "'";
  for (const char c : text) {
    if (c == '\'') out += "'\\''";
    else out += c;
  }
  return out + "'";
}

std::pair<int, std::string> run(const std::string& command) {
  FILE* pipe = ::popen(command.c_str(), "r");
  if (!pipe) return {-1, {}};
  std::string out;
  char buffer[4096];
  while (const auto n = std::fread(buffer, 1, sizeof buffer, pipe)) out.append(buffer, n);
  const int status = ::pclose(pipe);
  return {WIFEXITED(status) ? WEXITSTATUS(status) : -1, out};
}

void library_checks() {
  // The .dfg count rule: max(incoming, outgoing) edge sums, edge activities
  // only, in first occurrence over the edges.
  const auto dfg = minera::parse_dfg("4\na\nb\nc\nd\n2\n1x5\n3x2\n1\n0x5\n2>0x5\n1>0x5\n3>3x9\n0>2x1\n");
  const auto data = minera::dfg_filtering_data_from_dfg(dfg);
  const minera::DfgFilteringCounts expected = {{"a", 10}, {"c", 5}, {"b", 5}, {"d", 9}};
  check(data.activities == expected, "count rule for a .dfg input");
  check(data.start_activities == minera::DfgFilteringCounts({{"b", 5}, {"d", 2}}), "start activities kept");
  check(data.end_activities == minera::DfgFilteringCounts({{"a", 5}}), "end activities kept");
  // No filter: the payload round-trips.
  check(minera::to_dfg(minera::to_dfg_clean_analysis(minera::apply_dfg_filtering_request(data, {}))) ==
            minera::to_dfg(dfg),
        "an empty request leaves the DFG unchanged");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3 && argc != 4) {
    std::cerr << "usage: " << argv[0] << " <minera_server> <minera_dfg> [port]\n";
    return 2;
  }
  library_checks();

  const std::vector<ServerCase> server_cases = {
      // contain_activity keeps only the start activities that reach it.
      {{"ab", "b"}, "contain_activity=a", 200, "2\na\nb\n1\n0x1\n1\n1x2\n0>1x1\n"},
      // paths_percentage breaks count ties by edge, descending, the boundary
      // arcs included.
      {{"abc", "ac", "ab"}, "paths_percentage=0.5", 200, "3\na\nb\nc\n1\n0x3\n1\n2x2\n0>1x2\n1>2x1\n"},
      {{"c", "bebcb", "c", "aeceebc"},
       "paths_percentage=0.5",
       200,
       "3\nb\nc\ne\n1\n1x2\n1\n1x3\n0>1x2\n1>2x1\n2>0x2\n2>1x1\n2>2x1\n"},
      {{"c", "bebcb", "c", "aeceebc"},
       "paths_percentage=0.5&keep_all_activities=true",
       200,
       "4\na\nb\nc\ne\n2\n0x1\n2x2\n1\n2x3\n0>3x1\n1>2x2\n2>3x1\n3>1x2\n3>2x1\n3>3x1\n"},
      // Chained: the path filter keeps b, so to_activity=b succeeds (the
      // native filters dropped b and refused the target).
      {{"aba"}, "paths_percentage=0&to_activity=b", 200, "2\na\nb\n1\n0x1\n1\n1x1\n0>1x1\n"},
      // Chained where the reference raises: to_activity=b drops a, and
      // contain_activity=a then fails with KeyError.
      {{"ca", "cb"}, "to_activity=b&contain_activity=a", 400, "KeyError: 'a'"},
  };

  const int port = argc == 4 ? std::atoi(argv[3]) : free_port();
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
  client.set_read_timeout(60, 0);
  bool ready = false;
  for (int attempt = 0; attempt < 200 && !ready; ++attempt) {
    if (auto result = client.Get("/api/version"); result && result->status == 200) ready = true;
    else std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  check(ready, "server answers /api/version");
  if (ready) {
    for (const auto& item : server_cases) {
      httplib::MultipartFormDataItems form = {{"log", csv_log(item.traces), "log.csv", "text/csv"},
                                              {"include_traces", "false", "", ""}};
      const auto mined = client.Post("/api/mine", form);
      check(mined && mined->status == 200, "mine for " + item.query);
      const auto result = client.Get("/api/dfg?" + item.query);
      const bool ok = result && result->status == item.status &&
                      (item.status == 200 ? result->body == item.body
                                          : result->body.find(item.body) != std::string::npos);
      check(ok, "GET /api/dfg?" + item.query + ": " +
                    (result ? std::to_string(result->status) + " " + result->body : std::string("no response")));
    }
  }
  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);

  const std::vector<CliCase> cli_cases = {
      {"paths", "3\na\nb\nc\n1\n0x3\n2\n1x1\n2x2\n0>1x2\n0>2x1\n1>2x1\n", {"--paths-percentage", "0.5"}, 0,
       "3\na\nb\nc\n1\n0x3\n1\n2x2\n0>1x2\n1>2x1\n"},
      {"contain", "2\na\nb\n2\n0x1\n1x1\n1\n1x2\n0>1x1\n", {"--contain-activity", "a"}, 0,
       "2\na\nb\n1\n0x1\n1\n1x2\n0>1x1\n"},
      // Ranked by max(incoming, outgoing): a (10) ahead of d (9), so a stays
      // with the c branch that keeps it connected.
      {"activities", "4\na\nb\nc\nd\n3\n1x5\n2x5\n3x3\n2\n0x5\n3x3\n1>0x5\n2>0x5\n3>3x9\n",
       {"--activities-percentage", "0"}, 0, "2\na\nc\n1\n1x5\n1\n0x5\n1>0x5\n"},
      {"chained_error", "3\na\nb\nc\n1\n2x2\n2\n0x1\n1x1\n2>0x1\n2>1x1\n",
       {"--to-activity", "b", "--contain-activity", "a"}, 1, "KeyError: 'a'"},
  };
  const auto dir = std::filesystem::temp_directory_path() /
                   ("minera_dfg_filter_route_tests_" + std::to_string(::getpid()));
  std::filesystem::create_directories(dir);
  for (const auto& item : cli_cases) {
    const auto input = dir / (item.name + ".dfg");
    std::ofstream(input, std::ios::binary) << item.dfg;
    std::string command = shell_quote(argv[2]);
    for (const auto& arg : item.args) command += ' ' + shell_quote(arg);
    command += ' ' + shell_quote(input.string());
    const auto [stdout_code, stdout_text] = run(command + " 2>/dev/null");
    const auto [stderr_code, stderr_text] = run(command + " 2>&1 >/dev/null");
    const bool ok = item.exit_code == 0 ? stdout_code == 0 && stdout_text == item.output
                                        : stderr_code == item.exit_code &&
                                              stderr_text.find(item.output) != std::string::npos;
    check(ok, "minera_dfg " + item.name + ": exit " + std::to_string(stdout_code) + " out " + stdout_text +
                  " err " + stderr_text);
  }
  std::filesystem::remove_all(dir);

  if (g_failures != 0) {
    std::cerr << g_failures << " DFG filter route check(s) failed\n";
    return 1;
  }
  std::cout << "DFG filter route checks passed\n";
  return 0;
}
