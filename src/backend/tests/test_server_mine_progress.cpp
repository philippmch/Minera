#undef NDEBUG
// Drives a real minera_server process over loopback HTTP: GET
// /api/mine-progress reports the running mine's stage, stage count and
// elapsed time while another connection mines (and while a further mine is
// refused with MINE_BUSY), and POST /api/cancel-mine stops the mine inside a
// long stage (the inductive net's token replay), not only at the next
// stage boundary: the server's CPU use stops within about a second while the
// mining connection stays open, the mine answers with the cancel's error, and
// the progress record clears. CPU time is read from /proc, so that check runs
// on Linux only.
//
// Usage: minera_server_mine_progress_tests <minera_server>

#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "httplib.h"

extern char** environ;

namespace {

int g_failures = 0;

void check(bool condition, const std::string& what) {
  if (!condition) {
    ++g_failures;
    std::cerr << "FAIL: " << what << '\n';
  }
}

std::string describe(const httplib::Result& result) {
  if (!result) return "no response";
  return std::to_string(result->status) + " " + result->body.substr(0, 300);
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

// The stage the cancel lands in: the longest stage of the mine below.
const std::string kLongStage = "Inductive net: token replay";

// 5,000 cases of 20 to 60 events over 40 activities in random order: the
// inductive miner answers with a parallel of 40 optional loops, and replaying
// 5,000 distinct traces on that net takes seconds (about 3 s, after about
// 7 s of earlier stages, on an unloaded four-core machine). Its reachability
// check is instant since the net reduction of review AA2, and its footprints
// since the packed footprint exploration of the same review (the footprints
// stage was the long one before). Review AD2 spread the replay's precision
// searches over up to four threads, which cut 2,000 cases to about 1 s, so
// the log grew to keep this stage the longest one (a cancel that lands while
// those threads run stops them too; that was checked by hand).
std::string random_log() {
  std::mt19937 rng(7);
  std::uniform_int_distribution<int> activity(0, 39), length(20, 60);
  std::ostringstream csv;
  csv << "case_id,activity,timestamp\n";
  for (int c = 0; c < 5000; ++c) {
    const int n = length(rng);
    for (int e = 0; e < n; ++e)
      csv << 'c' << c << ",a" << activity(rng) << ",2024-01-01T" << (e / 60 < 10 ? "0" : "") << e / 60
          << ':' << (e % 60 < 10 ? "0" : "") << e % 60 << ":00Z\n";
  }
  return csv.str();
}

double cpu_seconds(pid_t pid) {
#if defined(__linux__)
  std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
  std::string text((std::istreambuf_iterator<char>(stat)), std::istreambuf_iterator<char>());
  const auto close = text.rfind(')');
  if (close == std::string::npos) return -1.0;
  std::istringstream fields(text.substr(close + 2));
  std::string field;
  unsigned long long utime = 0, stime = 0;
  for (int index = 3; fields >> field; ++index) {
    if (index == 14) utime = std::stoull(field);
    if (index == 15) { stime = std::stoull(field); break; }
  }
  return static_cast<double>(utime + stime) / static_cast<double>(::sysconf(_SC_CLK_TCK));
#else
  (void)pid;
  return -1.0;
#endif
}

void sleep_ms(int milliseconds) {
  std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

bool contains(const std::string& text, const std::string& part) {
  return text.find(part) != std::string::npos;
}

// The raw value of a top-level JSON field of a flat object ("" when absent).
std::string field(const std::string& body, const std::string& name) {
  const auto key = "\"" + name + "\":";
  const auto at = body.find(key);
  if (at == std::string::npos) return {};
  auto start = at + key.size();
  if (body[start] == '"') return body.substr(start + 1, body.find('"', start + 1) - start - 1);
  const auto end = body.find_first_of(",}", start);
  return body.substr(start, end - start);
}

struct Progress {
  bool running = false, matches = false;
  std::string stage;
  long index = 0, done = 0, total = 0;
  double elapsed = 0.0;
};

std::optional<Progress> poll(httplib::Client& client, const std::string& id) {
  const auto result = client.Get("/api/mine-progress?request_id=" + id);
  if (!result || result->status != 200 || field(result->body, "family") != "MINE_PROGRESS") return std::nullopt;
  Progress progress;
  progress.running = field(result->body, "running") == "true";
  progress.matches = field(result->body, "request_matches") == "true";
  progress.stage = field(result->body, "stage");
  if (progress.running) {
    progress.index = std::atol(field(result->body, "stage_index").c_str());
    progress.done = std::atol(field(result->body, "stages_done").c_str());
    progress.total = std::atol(field(result->body, "stages_total").c_str());
    progress.elapsed = std::atof(field(result->body, "elapsed_seconds").c_str());
  }
  return progress;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " <minera_server>\n";
    return 2;
  }
  const int port = free_port();
  const std::string port_text = std::to_string(port);
  // The large budget lets the 600,000-row read below start; it is canceled
  // while reading.
  std::vector<std::string> args = {argv[1], "--port", port_text, "--frontend",
                                   "/nonexistent-minera-frontend", "--mine-memory-budget", "64G"};
  std::vector<char*> raw;
  for (auto& arg : args) raw.push_back(arg.data());
  raw.push_back(nullptr);
  pid_t server = 0;
  if (::posix_spawn(&server, argv[1], nullptr, nullptr, raw.data(), environ) != 0) {
    std::cerr << "cannot start " << argv[1] << '\n';
    return 2;
  }
  httplib::Client client("127.0.0.1", port);
  client.set_read_timeout(30, 0);
  bool ready = false;
  for (int attempt = 0; attempt < 200 && !ready; ++attempt) {
    if (auto result = client.Get("/api/version"); result && result->status == 200) ready = true;
    else sleep_ms(50);
  }
  check(ready, "server answers /api/version");

  if (ready) {
    // Idle: nothing is running, for any request id; other controls are refused.
    auto r = client.Get("/api/mine-progress");
    check(r && r->status == 200 && r->body == "{\"family\":\"MINE_PROGRESS\",\"running\":false}",
          "idle progress: " + describe(r));
    r = client.Get("/api/mine-progress?request_id=00000000-0000-0000-0000-000000000000");
    check(r && r->status == 200 && field(r->body, "running") == "false", "idle progress for an id: " + describe(r));
    r = client.Get("/api/mine-progress?stage=1");
    check(r && r->status == 400, "an unknown control is refused: " + describe(r));
    r = client.Post("/api/mine-progress", "", "text/plain");
    check(r && r->status == 405, "progress is GET only: " + describe(r));

    const std::string id = "3f6c1a52-9b0e-4c7d-8a21-5e4f0b9d7c13";
    const auto log = random_log();
    std::optional<httplib::Result> mined;
    std::thread miner([&] {
      httplib::Client mining("127.0.0.1", port);
      mining.set_read_timeout(300, 0);
      httplib::MultipartFormDataItems items = {{"log", log, "random.csv", "text/csv"},
                                               {"include_traces", "false", "", ""},
                                               {"mining_request_id", id, "", ""}};
      mined = mining.Post("/api/mine", items);
    });

    // Poll from another connection until the mine is in the long stage.
    std::vector<Progress> seen;
    const auto started = std::chrono::steady_clock::now();
    double slowest_poll = 0.0;
    bool in_long_stage = false;
    while (std::chrono::steady_clock::now() - started < std::chrono::seconds(120)) {
      const auto asked = std::chrono::steady_clock::now();
      const auto progress = poll(client, id);
      slowest_poll = std::max(slowest_poll, std::chrono::duration<double>(std::chrono::steady_clock::now() - asked).count());
      if (progress && progress->running && progress->matches) {
        seen.push_back(*progress);
        if (progress->stage == kLongStage) { in_long_stage = true; break; }
      }
      if (mined) break;  // finished (or failed) before the stage was seen
      sleep_ms(50);
    }
    check(in_long_stage, "the mine reached the " + kLongStage + " stage");
    check(!seen.empty() && seen.front().total == 26, "26 stages in all");
    bool ordered = true;
    for (std::size_t i = 1; i < seen.size(); ++i)
      ordered = ordered && seen[i].index >= seen[i - 1].index && seen[i].elapsed >= seen[i - 1].elapsed;
    for (const auto& progress : seen) ordered = ordered && progress.done == progress.index - 1 && progress.index <= progress.total;
    check(ordered, "stage indexes and elapsed times only grow");
    check(seen.size() >= 2 && seen.back().index > seen.front().index, "progress moved through several stages");
    check(slowest_poll < 1.0, "a poll answers at once while the mine runs (slowest " + std::to_string(slowest_poll) + " s)");
    // Another request id sees the mine as someone else's.
    r = client.Get("/api/mine-progress?request_id=00000000-0000-0000-0000-000000000000");
    check(r && field(r->body, "running") == "true" && field(r->body, "request_matches") == "false",
          "another id's progress: " + describe(r));
    // A second mine is refused while this one runs; progress still answers.
    httplib::MultipartFormDataItems other = {{"log", "case_id,activity,timestamp\nc,a,2024-01-01\n", "x.csv", "text/csv"}};
    r = client.Post("/api/mine", other);
    check(r && r->status == 503 && contains(r->body, "MINE_BUSY"), "a second mine is busy: " + describe(r));

    if (in_long_stage) {
      sleep_ms(200);
      const double busy_from = cpu_seconds(server);
      sleep_ms(400);
      const double busy_to = cpu_seconds(server);
      r = client.Post("/api/cancel-mine?request_id=" + id, "", "text/plain");
      check(r && r->status == 200 && contains(r->body, "\"canceled\":true"), "cancel-mine: " + describe(r));
      const auto progress = poll(client, id);
      check(progress && progress->stage == kLongStage,
            "the cancel arrived inside the " + kLongStage + " stage");
      sleep_ms(1000);
      const double idle_from = cpu_seconds(server);
      sleep_ms(500);
      const double idle_to = cpu_seconds(server);
      if (busy_from >= 0.0) {
        std::cout << "cancel: " << (busy_to - busy_from) << " s CPU in the 0.4 s before it, "
                  << (idle_to - idle_from) << " s in 0.5 s from 1 s after it\n";
        check(busy_to - busy_from > 0.2, "the mine was computing before the cancel");
        check(idle_to - idle_from < 0.1, "the server stopped within a second of the cancel");
      }
    }
    miner.join();
    check(mined && *mined && (*mined)->status == 400 &&
              contains((*mined)->body, "mining request was canceled or replaced by a newer source request"),
          "the canceled mine answers with the cancel's error: " + (mined ? describe(*mined) : std::string("none")));
    bool cleared = false;
    for (int attempt = 0; attempt < 40 && !cleared; ++attempt) {
      const auto progress = poll(client, id);
      cleared = progress && !progress->running;
      if (!cleared) sleep_ms(50);
    }
    check(cleared, "progress clears once the mine stops");
    // The server mines again afterwards.
    httplib::MultipartFormDataItems small = {{"log", "case_id,activity,timestamp\nc1,a,2024-01-01T00:00:00Z\nc1,b,2024-01-01T00:01:00Z\n", "small.csv", "text/csv"},
                                             {"include_traces", "false", "", ""}};
    r = client.Post("/api/mine", small);
    check(r && r->status == 200, "a later mine succeeds: " + describe(r));
    r = client.Get("/api/mine-progress");
    check(r && field(r->body, "running") == "false", "idle again after it: " + describe(r));

    // A long read: 600,000 rows take seconds to import.
    std::string big = "case_id,activity,timestamp\n";
    big.reserve(600000 * 36);
    for (int row = 0; row < 600000; ++row) {
      char line[64];
      std::snprintf(line, sizeof line, "c%d,a%d,2024-01-01T%02d:%02d:%02dZ\n", row / 12, row % 9,
                    (row % 12) * 2, row % 60, (row / 60) % 60);
      big += line;
    }
    // An unknown field is refused before the log is read.
    {
      const auto asked = std::chrono::steady_clock::now();
      httplib::MultipartFormDataItems items = {{"log", big, "big.csv", "text/csv"}, {"request_id", "x", "", ""}};
      r = client.Post("/api/mine", items);
      const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - asked).count();
      check(r && r->status == 400 && contains(r->body, "unknown or inapplicable /api/mine form field: request_id"),
            "an unknown field is refused: " + describe(r));
      std::cout << "unknown field refused in " << took << " s\n";
      check(took < 1.0, "an unknown field is refused before the log is read (" + std::to_string(took) + " s)");
    }
    // A cancel during "Reading the event log" stops the read part-way.
    {
      const std::string read_id = "8d1e2f30-4a5b-4c6d-9e7f-0a1b2c3d4e5f";
      std::optional<httplib::Result> read_mined;
      std::thread reader([&] {
        httplib::Client mining("127.0.0.1", port);
        mining.set_read_timeout(300, 0);
        httplib::MultipartFormDataItems items = {{"log", big, "big.csv", "text/csv"},
                                                 {"include_traces", "false", "", ""},
                                                 {"mining_request_id", read_id, "", ""}};
        read_mined = mining.Post("/api/mine", items);
      });
      bool reading = false;
      for (int attempt = 0; attempt < 400 && !reading && !read_mined; ++attempt) {
        const auto progress = poll(client, read_id);
        reading = progress && progress->running && progress->matches && progress->stage == "Reading the event log";
        if (!reading) sleep_ms(10);
      }
      check(reading, "the mine reached the reading stage");
      sleep_ms(50);
      const auto canceled_at = std::chrono::steady_clock::now();
      r = client.Post("/api/cancel-mine?request_id=" + read_id, "", "text/plain");
      check(r && r->status == 200 && contains(r->body, "\"canceled\":true"), "cancel-mine while reading: " + describe(r));
      const auto still = poll(client, read_id);
      check(still && still->stage == "Reading the event log", "the cancel arrived while reading");
      bool stopped = false;
      while (!stopped && std::chrono::steady_clock::now() - canceled_at < std::chrono::seconds(30)) {
        const auto progress = poll(client, read_id);
        stopped = progress && !progress->running;
        if (!stopped) sleep_ms(10);
      }
      const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - canceled_at).count();
      std::cout << "read stopped " << took << " s after the cancel\n";
      check(stopped && took < 0.5, "the read stopped within half a second of the cancel (" + std::to_string(took) + " s)");
      reader.join();
      check(read_mined && *read_mined && (*read_mined)->status == 400 &&
                contains((*read_mined)->body, "mining request was canceled or replaced by a newer source request"),
            "the canceled read answers with the cancel's error: " + (read_mined ? describe(*read_mined) : std::string("none")));
    }
    // cancel-mine says whether it undid anything: a request whose response
    // was acknowledged cannot be canceled; an unknown id is recorded.
    {
      const std::string done_id = "1b2c3d4e-5f60-4718-8a9b-0c1d2e3f4a5b";
      httplib::MultipartFormDataItems items = {{"log", "case_id,activity,timestamp\nc1,a,2024-01-01T00:00:00Z\n", "one.csv", "text/csv"},
                                               {"include_traces", "false", "", ""},
                                               {"mining_request_id", done_id, "", ""}};
      r = client.Post("/api/mine", items);
      check(r && r->status == 200, "a tracked mine: " + describe(r));
      r = client.Post("/api/mine-ack?request_id=" + done_id, "", "text/plain");
      check(r && contains(r->body, "\"acknowledged\":true"), "ack: " + describe(r));
      r = client.Post("/api/cancel-mine?request_id=" + done_id, "", "text/plain");
      check(r && r->status == 200 && contains(r->body, "\"canceled\":false"), "cancel after an ack: " + describe(r));
      r = client.Post("/api/cancel-mine?request_id=2c3d4e5f-6071-4829-9bac-1d2e3f4a5b6c", "", "text/plain");
      check(r && r->status == 200 && contains(r->body, "\"canceled\":true"), "cancel of a request not yet seen: " + describe(r));
    }
  }

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures != 0) {
    std::cerr << g_failures << " mine progress check(s) failed\n";
    return 1;
  }
  std::cout << "server mine progress checks passed\n";
  return 0;
}
