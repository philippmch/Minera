#undef NDEBUG
// Drives a real minera_server process over loopback HTTP: a long route whose
// client disconnects stops computing within a second (the server process
// goes idle), and the server keeps answering afterwards. Routes covered, on
// a mined 50,000-event log: edit-distance alignment of the log against
// itself, and mining the log again (early in the run, and during the model
// decorations). CPU time is read from /proc, so the timing checks run on
// Linux only; elsewhere the test only checks that the routes answer.
//
// Usage: minera_server_cancellation_tests <minera_server>

#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
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

// 5,000 cases of 10 events over 14 activities in random order: every case
// is its own variant, so the models and alignments are expensive.
std::string noisy_log() {
  std::mt19937 rng(42);
  std::uniform_int_distribution<int> activity(0, 13);
  std::ostringstream csv;
  csv << "case_id,activity,timestamp\n";
  for (int c = 0; c < 5000; ++c) {
    for (int e = 0; e < 10; ++e) {
      csv << 'c' << c << ",a" << activity(rng) << ",2024-01-01T00:"
          << (e < 10 ? "0" : "") << e << ":00Z\n";
    }
  }
  return csv.str();
}

// CPU seconds (user + system) the process has used, or a negative value
// where /proc is unavailable.
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

// Sends `target` as a POST of `body`, lets the server work for `work_ms`,
// closes the socket, and checks that the server stops spending CPU within a
// second.
void disconnect_stops(pid_t server, int port, const std::string& name, const std::string& target,
                      const std::string& body = "", const std::string& content_type = "",
                      int work_ms = 300) {
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(static_cast<std::uint16_t>(port));
  if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0) {
    check(false, name + ": cannot connect");
    ::close(fd);
    return;
  }
  // The server answers only its own host:port.
  std::string request = "POST " + target + " HTTP/1.1\r\nHost: 127.0.0.1:" + std::to_string(port) +
                        "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n";
  if (!content_type.empty()) request += "Content-Type: " + content_type + "\r\n";
  request += "Connection: close\r\n\r\n" + body;
  std::size_t sent = 0;
  while (sent < request.size()) {
    const auto n = ::send(fd, request.data() + sent, request.size() - sent, 0);
    if (n <= 0) break;
    sent += static_cast<std::size_t>(n);
  }
  check(sent == request.size(), name + ": request sent");
  sleep_ms(work_ms);
  const double busy_from = cpu_seconds(server);
  sleep_ms(400);
  const double busy_to = cpu_seconds(server);
  ::close(fd);  // the client gives up
  sleep_ms(1000);
  const double idle_from = cpu_seconds(server);
  sleep_ms(500);
  const double idle_to = cpu_seconds(server);
  if (busy_from < 0.0) {
    std::cout << name << ": no /proc CPU counters; timing not checked\n";
    return;
  }
  std::cout << name << ": " << (busy_to - busy_from) << " s CPU in the 0.4 s before the disconnect, "
            << (idle_to - idle_from) << " s in 0.5 s from 1 s after it\n";
  check(busy_to - busy_from > 0.2, name + ": the route was still computing before the disconnect");
  check(idle_to - idle_from < 0.1, name + ": the server stopped within a second of the disconnect");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " <minera_server>\n";
    return 2;
  }
  const int port = free_port();
  const std::string port_text = std::to_string(port);
  std::vector<std::string> args = {argv[1], "--port", port_text, "--frontend",
                                   "/nonexistent-minera-frontend"};
  std::vector<char*> raw;
  for (auto& arg : args) raw.push_back(arg.data());
  raw.push_back(nullptr);
  pid_t server = 0;
  if (::posix_spawn(&server, argv[1], nullptr, nullptr, raw.data(), environ) != 0) {
    std::cerr << "cannot start " << argv[1] << '\n';
    return 2;
  }
  httplib::Client client("127.0.0.1", port);
  client.set_read_timeout(300, 0);
  bool ready = false;
  for (int attempt = 0; attempt < 200 && !ready; ++attempt) {
    if (auto result = client.Get("/api/version"); result && result->status == 200) ready = true;
    else sleep_ms(50);
  }
  check(ready, "server answers /api/version");

  if (ready) {
    const auto log = noisy_log();
    httplib::MultipartFormDataItems items = {{"log", log, "noisy.csv", "text/csv"},
                                             {"include_traces", "false", "", ""}};
    const auto mine_started = std::chrono::steady_clock::now();
    const auto mined = client.Post("/api/mine", items);
    // The decorations run from about a sixth of a mine's time to its end.
    const auto mine_seconds_to_decorations = static_cast<int>(
        std::chrono::duration<double>(std::chrono::steady_clock::now() - mine_started).count() / 2.0);
    check(mined && mined->status == 200, "mine the 50,000-event log: " + describe(mined));
    if (mined && mined->status == 200) {
      // A short request still answers in full.
      auto r = client.Post("/api/concept-drift?drift_num_permutations=2", "", "text/plain");
      check(r && r->status == 200, "concept drift answers: " + describe(r));
      // Every variant against every reference variant: tens of seconds.
      const std::string boundary = "minera-cancellation-boundary";
      const auto multipart_type = "multipart/form-data; boundary=" + boundary;
      const std::string reference_body =
          "--" + boundary + "\r\nContent-Disposition: form-data; name=\"reference_log\"; "
          "filename=\"noisy.csv\"\r\nContent-Type: text/csv\r\n\r\n" + log + "\r\n--" +
          boundary + "--\r\n";
      disconnect_stops(server, port, "edit-distance alignment",
                       "/api/align-edit-distance?edit_activity_attribute=activity", reference_body,
                       multipart_type);
      // Mining the log again: given up early, and during the model
      // decorations (their token replays take most of a mine's time).
      const std::string mine_body =
          "--" + boundary + "\r\nContent-Disposition: form-data; name=\"include_traces\"\r\n\r\nfalse\r\n"
          "--" + boundary + "\r\nContent-Disposition: form-data; name=\"log\"; filename=\"noisy.csv\"\r\n"
          "Content-Type: text/csv\r\n\r\n" + log + "\r\n--" + boundary + "--\r\n";
      disconnect_stops(server, port, "mining, early", "/api/mine", mine_body, multipart_type, 1000);
      disconnect_stops(server, port, "mining, decorations", "/api/mine", mine_body, multipart_type,
                       std::max(1, mine_seconds_to_decorations) * 1000);
      r = client.Get("/api/version");
      check(r && r->status == 200, "the server answers after the disconnects: " + describe(r));
      // The first mine's session still serves.
      r = client.Post("/api/concept-drift?drift_num_permutations=2", "", "text/plain");
      check(r && r->status == 200, "the mined session serves after the given-up mines: " + describe(r));
    }
  }

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures != 0) {
    std::cerr << g_failures << " cancellation check(s) failed\n";
    return 1;
  }
  std::cout << "server cancellation checks passed\n";
  return 0;
}
