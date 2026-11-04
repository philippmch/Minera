#undef NDEBUG
// Starts two minera_server processes on one loopback port: the second must
// not share the port with the first (which would let the kernel hand it half
// of the first server's connections). It exits non-zero, says the port is
// already in use, and the first server keeps answering.
//
// Usage: minera_server_port_exclusive_tests <minera_server>

#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
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

void sleep_ms(int milliseconds) {
  std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

// Starts the server on `port`, its stderr going to `stderr_path`.
pid_t start_server(const char* binary, int port, const std::string& stderr_path) {
  const std::string port_text = std::to_string(port);
  std::vector<std::string> args = {binary, "--port", port_text, "--frontend",
                                   "/nonexistent-minera-frontend"};
  std::vector<char*> raw;
  for (auto& arg : args) raw.push_back(arg.data());
  raw.push_back(nullptr);
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, stderr_path.c_str(),
                                   O_WRONLY | O_CREAT | O_TRUNC, 0600);
  pid_t pid = 0;
  const int spawned = ::posix_spawn(&pid, binary, &actions, nullptr, raw.data(), environ);
  posix_spawn_file_actions_destroy(&actions);
  return spawned == 0 ? pid : 0;
}

// Waits up to `seconds` for `pid` to exit; returns its wait status, or -1
// when it is still running.
int wait_exit(pid_t pid, int seconds) {
  for (int attempt = 0; attempt < seconds * 20; ++attempt) {
    int status = 0;
    if (::waitpid(pid, &status, WNOHANG) == pid) return status;
    sleep_ms(50);
  }
  return -1;
}

std::string read_file(const std::string& path) {
  std::ifstream in(path);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " <minera_server>\n";
    return 2;
  }
  char directory_template[] = "/tmp/minera-port-test-XXXXXX";
  const char* directory = ::mkdtemp(directory_template);
  if (directory == nullptr) {
    std::cerr << "cannot create a temporary directory\n";
    return 2;
  }
  const std::string first_log = std::string(directory) + "/first.err";
  const std::string second_log = std::string(directory) + "/second.err";
  const int port = free_port();
  const pid_t first = start_server(argv[1], port, first_log);
  if (first == 0) {
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
  check(ready, "the first server answers /api/version");

  if (ready) {
    const pid_t second = start_server(argv[1], port, second_log);
    check(second != 0, "the second server starts");
    if (second != 0) {
      const int status = wait_exit(second, 20);
      if (status == -1) {
        check(false, "the second server exits instead of listening on the busy port");
        ::kill(second, SIGTERM);
        ::waitpid(second, nullptr, 0);
      } else {
        check(WIFEXITED(status) && WEXITSTATUS(status) != 0,
              "the second server exits with a non-zero status");
        const auto message = read_file(second_log);
        check(message.find("port " + std::to_string(port) + " is already in use") !=
                  std::string::npos,
              "the second server says the port is already in use: " + message);
      }
    }
    // Every connection still reaches the first server.
    for (int request = 0; request < 20; ++request) {
      httplib::Client fresh("127.0.0.1", port);
      fresh.set_keep_alive(false);
      const auto result = fresh.Get("/api/version");
      check(result && result->status == 200, "the first server keeps answering");
    }
  }

  ::kill(first, SIGTERM);
  ::waitpid(first, nullptr, 0);
  std::remove(first_log.c_str());
  std::remove(second_log.c_str());
  ::rmdir(directory);
  if (g_failures != 0) {
    std::cerr << g_failures << " port check(s) failed\n";
    return 1;
  }
  std::cout << "server port checks passed\n";
  return 0;
}
