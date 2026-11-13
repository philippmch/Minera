#undef NDEBUG
// Drives a real minera_server process over loopback HTTP and checks the
// parameter contract of the /api routes: strict values, repeated controls,
// unknown /api/mine fields, JSON 404/405 bodies, valid UTF-8 error JSON,
// defaults (miner, playout traces), live-session release and the mining
// rollback window (cancel after the response was written restores the
// previous source until an ack, a request on the new source or the window's
// end releases it).
//
// Usage: minera_server_parameter_contract_tests <minera_server> <log.csv>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/time.h>
#include <signal.h>
#include <spawn.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "httplib.h"
#if defined(MINERA_TEST_HAS_ZLIB)
#include <zlib.h>
#endif

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
  if (::bind(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0)
    std::abort();
  socklen_t length = sizeof address;
  ::getsockname(socket_fd, reinterpret_cast<sockaddr*>(&address), &length);
  const int port = ntohs(address.sin_port);
  ::close(socket_fd);
  return port;
}

// A body is valid JSON text only when it is UTF-8 without raw control bytes.
bool utf8_without_controls(const std::string& text) {
  for (std::size_t i = 0; i < text.size();) {
    const auto byte = static_cast<unsigned char>(text[i]);
    if (byte < 0x20) return false;
    std::size_t length = byte < 0x80 ? 1 : (byte >> 5) == 6 ? 2 : (byte >> 4) == 14 ? 3
                       : (byte >> 3) == 30 ? 4 : 0;
    if (length == 0 || i + length > text.size()) return false;
    for (std::size_t k = 1; k < length; ++k)
      if ((static_cast<unsigned char>(text[i + k]) & 0xC0) != 0x80) return false;
    i += length;
  }
  return true;
}

// Well-formed UTF-8 (JSON whitespace allowed), checked by Python's strict
// decoder rules: no overlong forms, surrogates or code points above U+10FFFF.
bool valid_utf8(const std::string& text) {
  for (std::size_t i = 0; i < text.size();) {
    const auto lead = static_cast<unsigned char>(text[i]);
    if (lead < 0x80) { ++i; continue; }
    std::size_t length = lead >= 0xc2 && lead <= 0xdf ? 2 : lead >= 0xe0 && lead <= 0xef ? 3
                       : lead >= 0xf0 && lead <= 0xf4 ? 4 : 0;
    if (length == 0 || i + length > text.size()) return false;
    for (std::size_t k = 1; k < length; ++k)
      if ((static_cast<unsigned char>(text[i + k]) & 0xC0) != 0x80) return false;
    const auto second = static_cast<unsigned char>(text[i + 1]);
    if ((lead == 0xe0 && second < 0xa0) || (lead == 0xed && second > 0x9f) ||
        (lead == 0xf0 && second < 0x90) || (lead == 0xf4 && second > 0x8f))
      return false;
    i += length;
  }
  return true;
}

// A small XML 1.0 well-formedness check (enough for the exports): valid UTF-8,
// no characters XML cannot carry, one root element, start and end tags that
// nest, quoted attributes, and only the predefined or numeric entities.
bool well_formed_xml(const std::string& text) {
  if (!valid_utf8(text)) return false;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const auto byte = static_cast<unsigned char>(text[i]);
    if (byte < 0x20 && byte != '\t' && byte != '\n' && byte != '\r') return false;
    if (byte == 0xef && i + 2 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0xbf &&
        static_cast<unsigned char>(text[i + 2]) >= 0xbe)
      return false;
  }
  const auto entity_ok = [&](std::size_t at) {
    const auto end = text.find(';', at);
    if (end == std::string::npos) return false;
    const auto name = text.substr(at + 1, end - at - 1);
    if (name == "lt" || name == "gt" || name == "amp" || name == "quot" || name == "apos") return true;
    if (name.size() < 2 || name[0] != '#') return false;
    const bool hex = name[1] == 'x';
    if (name.size() == (hex ? 2u : 1u)) return false;
    for (std::size_t k = hex ? 2 : 1; k < name.size(); ++k)
      if (!(hex ? std::isxdigit(static_cast<unsigned char>(name[k])) : std::isdigit(static_cast<unsigned char>(name[k]))))
        return false;
    return true;
  };
  std::vector<std::string> open;
  int roots = 0;
  for (std::size_t i = 0; i < text.size();) {
    if (text[i] == '&') {
      if (!entity_ok(i)) return false;
      ++i;
      continue;
    }
    if (text[i] != '<') {
      if (open.empty() && !std::isspace(static_cast<unsigned char>(text[i]))) return false;
      ++i;
      continue;
    }
    if (text.compare(i, 4, "<!--") == 0) {
      const auto end = text.find("-->", i + 4);
      if (end == std::string::npos) return false;
      i = end + 3;
      continue;
    }
    if (text.compare(i, 9, "<![CDATA[") == 0) {
      const auto end = text.find("]]>", i + 9);
      if (end == std::string::npos || open.empty()) return false;
      i = end + 3;
      continue;
    }
    if (text.compare(i, 2, "<?") == 0) {
      const auto end = text.find("?>", i + 2);
      if (end == std::string::npos) return false;
      i = end + 2;
      continue;
    }
    if (text.compare(i, 2, "<!") == 0) return false;  // no DTDs in the exports
    // A start or end tag; quoted attribute values may hold '>'.
    std::size_t at = i + 1;
    const bool closing = at < text.size() && text[at] == '/';
    if (closing) ++at;
    const auto name_end = text.find_first_of(" \t\r\n/>", at);
    if (name_end == std::string::npos || name_end == at) return false;
    const auto name = text.substr(at, name_end - at);
    at = name_end;
    bool self_closing = false;
    for (;;) {
      while (at < text.size() && std::isspace(static_cast<unsigned char>(text[at]))) ++at;
      if (at >= text.size()) return false;
      if (text[at] == '>') { ++at; break; }
      if (text[at] == '/' && !closing && at + 1 < text.size() && text[at + 1] == '>') {
        self_closing = true;
        at += 2;
        break;
      }
      if (closing) return false;
      const auto eq = text.find('=', at);
      if (eq == std::string::npos) return false;
      at = eq + 1;
      while (at < text.size() && std::isspace(static_cast<unsigned char>(text[at]))) ++at;
      if (at >= text.size() || (text[at] != '"' && text[at] != '\'')) return false;
      const auto quote = text[at];
      const auto value_end = text.find(quote, at + 1);
      if (value_end == std::string::npos) return false;
      for (auto k = at + 1; k < value_end; ++k) {
        if (text[k] == '<') return false;
        if (text[k] == '&' && !entity_ok(k)) return false;
      }
      at = value_end + 1;
    }
    if (closing) {
      if (open.empty() || open.back() != name) return false;
      open.pop_back();
    } else {
      if (open.empty() && roots++ > 0) return false;
      if (!self_closing) open.push_back(name);
    }
    i = at;
  }
  return open.empty() && roots == 1;
}

bool contains(const httplib::Result& result, const std::string& needle) {
  return result && result->body.find(needle) != std::string::npos;
}

// The first "source_id" string value of a body, or "" when there is none.
std::string source_id_of(const httplib::Result& result) {
  if (!result) return "";
  const std::string key = "\"source_id\":\"";
  const auto at = result->body.find(key);
  if (at == std::string::npos) return "";
  const auto end = result->body.find('"', at + key.size());
  return end == std::string::npos ? "" : result->body.substr(at + key.size(), end - at - key.size());
}

std::size_t count(const std::string& text, const std::string& needle) {
  std::size_t total = 0;
  for (auto at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1))
    ++total;
  return total;
}


// A minimal JSON text checker (RFC 8259 syntax; strings are checked for
// escapes and raw control bytes, UTF-8 separately by valid_utf8).
class JsonChecker {
 public:
  explicit JsonChecker(const std::string& text) : text_(text) {}
  bool valid() {
    space();
    if (!value(0)) return false;
    space();
    return at_ == text_.size();
  }

 private:
  const std::string& text_;
  std::size_t at_ = 0;
  bool more() const { return at_ < text_.size(); }
  char peek() const { return more() ? text_[at_] : '\0'; }
  void space() {
    while (more() && (peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r')) ++at_;
  }
  bool literal(const char* word) {
    const std::size_t length = std::strlen(word);
    if (text_.compare(at_, length, word) != 0) return false;
    at_ += length;
    return true;
  }
  bool digits() {
    const auto begin = at_;
    while (more() && peek() >= '0' && peek() <= '9') ++at_;
    return at_ > begin;
  }
  bool number() {
    if (peek() == '-') ++at_;
    if (peek() == '0') ++at_;
    else if (!digits()) return false;
    if (peek() == '.') { ++at_; if (!digits()) return false; }
    if (peek() == 'e' || peek() == 'E') {
      ++at_;
      if (peek() == '+' || peek() == '-') ++at_;
      if (!digits()) return false;
    }
    return true;
  }
  bool string() {
    if (peek() != '"') return false;
    ++at_;
    while (more()) {
      const auto c = static_cast<unsigned char>(text_[at_++]);
      if (c == '"') return true;
      if (c < 0x20) return false;
      if (c != '\\') continue;
      if (!more()) return false;
      const char escape = text_[at_++];
      if (escape == 'u') {
        for (int k = 0; k < 4; ++k, ++at_)
          if (!more() || !std::isxdigit(static_cast<unsigned char>(peek()))) return false;
      } else if (std::strchr("\"\\/bfnrt", escape) == nullptr || escape == '\0') {
        return false;
      }
    }
    return false;
  }
  bool value(int depth) {
    if (depth > 512) return false;
    space();
    const char c = peek();
    if (c == '{' || c == '[') {
      const char close = c == '{' ? '}' : ']';
      ++at_;
      space();
      if (peek() == close) { ++at_; return true; }
      for (;;) {
        space();
        if (c == '{') {
          if (!string()) return false;
          space();
          if (peek() != ':') return false;
          ++at_;
        }
        if (!value(depth + 1)) return false;
        space();
        if (peek() == ',') { ++at_; continue; }
        if (peek() == close) { ++at_; return true; }
        return false;
      }
    }
    if (c == '"') return string();
    if (c == 't') return literal("true");
    if (c == 'f') return literal("false");
    if (c == 'n') return literal("null");
    return number();
  }
};

bool valid_json(const std::string& text) { return valid_utf8(text) && JsonChecker(text).valid(); }

// One HTTP/1.1 exchange on a fresh loopback connection, read to EOF, so the
// body is what the server wrote, not what Content-Length promised.
struct RawResponse {
  int status = 0;
  std::string head, body;
  bool complete = false;
  std::string header(const std::string& name) const {
    std::string lower_head = head;
    for (auto& c : lower_head) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    std::string key = "\r\n" + name + ":";
    for (auto& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    const auto at = lower_head.find(key);
    if (at == std::string::npos) return {};
    auto begin = at + key.size();
    while (begin < head.size() && head[begin] == ' ') ++begin;
    return head.substr(begin, head.find("\r\n", begin) - begin);
  }
};

int connect_loopback(int port) {
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(static_cast<uint16_t>(port));
  if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0) {
    ::close(fd);
    return -1;
  }
  timeval timeout{60, 0};
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
  return fd;
}

// macOS has no MSG_NOSIGNAL; main ignores SIGPIPE there instead.
#if !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif

bool send_all(int fd, const std::string& data) {
  for (std::size_t sent = 0; sent < data.size();) {
    const auto n = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
    if (n <= 0) return false;
    sent += static_cast<std::size_t>(n);
  }
  return true;
}

RawResponse read_raw_response(int fd) {
  std::string raw;
  char buffer[65536];
  for (ssize_t n; (n = ::recv(fd, buffer, sizeof buffer, 0)) > 0;) raw.append(buffer, static_cast<std::size_t>(n));
  ::close(fd);
  RawResponse response;
  const auto end = raw.find("\r\n\r\n");
  if (end == std::string::npos || raw.compare(0, 9, "HTTP/1.1 ") != 0) return response;
  response.head = raw.substr(0, end);
  response.body = raw.substr(end + 4);
  response.status = std::atoi(raw.c_str() + 9);
  response.complete = true;
  return response;
}

std::string host_header(int port) { return "Host: 127.0.0.1:" + std::to_string(port) + "\r\n"; }

RawResponse raw_exchange(int port, const std::string& method, const std::string& target,
                         const std::string& extra_headers = "", const std::string& body = "",
                         const std::string& content_type = "application/x-www-form-urlencoded") {
  const int fd = connect_loopback(port);
  if (fd < 0) return {};
  std::string request = method + " " + target + " HTTP/1.1\r\n" + host_header(port) + "Connection: close\r\n" +
                        extra_headers;
  if (method == "POST")
    request += "Content-Type: " + content_type + "\r\nContent-Length: " +
               std::to_string(body.size()) + "\r\n";
  request += "\r\n" + body;
  if (!send_all(fd, request)) { ::close(fd); return {}; }
  return read_raw_response(fd);
}

#if defined(MINERA_TEST_HAS_ZLIB)
// Inflates one complete gzip member; nullopt for anything else (a truncated
// or trailing-garbage body included).
std::optional<std::string> gunzip(const std::string& bytes) {
  z_stream stream{};
  if (inflateInit2(&stream, 15 + 16) != Z_OK) return std::nullopt;
  stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(bytes.data()));
  stream.avail_in = static_cast<uInt>(bytes.size());
  std::string out;
  char buffer[65536];
  int status = Z_OK;
  while (status == Z_OK) {
    stream.next_out = reinterpret_cast<Bytef*>(buffer);
    stream.avail_out = sizeof buffer;
    status = inflate(&stream, Z_NO_FLUSH);
    out.append(buffer, sizeof buffer - stream.avail_out);
  }
  const bool whole = status == Z_STREAM_END && stream.avail_in == 0;
  inflateEnd(&stream);
  if (!whole) return std::nullopt;
  return out;
}

std::string gzip_bytes(const std::string& bytes) {
  z_stream stream{};
  if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) return {};
  std::string out(deflateBound(&stream, bytes.size()), '\0');
  stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(bytes.data()));
  stream.avail_in = static_cast<uInt>(bytes.size());
  stream.next_out = reinterpret_cast<Bytef*>(out.data());
  stream.avail_out = static_cast<uInt>(out.size());
  const bool done = deflate(&stream, Z_FINISH) == Z_STREAM_END;
  out.resize(stream.total_out);
  deflateEnd(&stream);
  return done ? out : std::string{};
}
#endif

// A mining body with its source ids blanked, so two mines of one log compare.
std::string without_source_ids(std::string body) {
  const std::string key = "\"source_id\":\"";
  for (std::size_t at = 0; (at = body.find(key, at)) != std::string::npos;) {
    at += key.size();
    const auto end = body.find('"', at);
    if (end == std::string::npos) break;
    body.erase(at, end - at);
  }
  return body;
}

std::string describe(const RawResponse& response) {
  if (!response.complete) return "no response";
  return std::to_string(response.status) + " Content-Length " + response.header("Content-Length") +
         " body " + std::to_string(response.body.size()) + " " + response.body.substr(0, 200);
}

}  // namespace

// The vendored client (the server's runtime calls) reads a response whose
// Content-Length repeats one value as a list ("5, 5", RFC 9112 6.3) and
// refuses a list of different values or an empty one.
void client_content_length_list_contract() {
  const auto answer = [](const std::string& length) {
    const int listener = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (::bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0 ||
        ::listen(listener, 1) != 0)
      std::abort();
    socklen_t size = sizeof address;
    ::getsockname(listener, reinterpret_cast<sockaddr*>(&address), &size);
    const int port = ntohs(address.sin_port);
    std::thread peer([listener, length] {
      const int fd = ::accept(listener, nullptr, nullptr);
      if (fd >= 0) {
        char buffer[4096];
        (void)::recv(fd, buffer, sizeof buffer, 0);
        send_all(fd, "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/plain\r\nContent-Length: " +
                         length + "\r\n\r\nhello");
        ::shutdown(fd, SHUT_RDWR);
        ::close(fd);
      }
      ::close(listener);
    });
    httplib::Client client("127.0.0.1", port);
    client.set_read_timeout(5, 0);
    auto result = client.Get("/");
    peer.join();
    return result;
  };
  for (const std::string list : {"5, 5", "5,5", "5 ,\t5"}) {
    const auto result = answer(list);
    check(result && result->status == 200 && result->body == "hello",
          "the client reads a response with Content-Length \"" + list + "\": " + describe(result));
  }
  for (const std::string list : {"5, 6", "", "5,"}) {
    const auto result = answer(list);
    check(!result, "the client refuses a response with Content-Length \"" + list + "\": " + describe(result));
  }
}

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: " << argv[0] << " <minera_server> <log.csv>\n";
    return 2;
  }
  // A send to a server that already closed the connection must fail, not
  // end the test (MSG_NOSIGNAL is Linux-only).
  ::signal(SIGPIPE, SIG_IGN);
  client_content_length_list_contract();
  std::ifstream log_file(argv[2], std::ios::binary);
  std::stringstream log_stream;
  log_stream << log_file.rdbuf();
  const std::string log = log_stream.str();
  if (log.empty()) {
    std::cerr << "cannot read " << argv[2] << '\n';
    return 2;
  }

  // A short rollback window keeps the expiry check fast.
  constexpr int rollback_window_ms = 1500;
  ::setenv("MINERA_MINING_ROLLBACK_MS", std::to_string(rollback_window_ms).c_str(), 1);
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
  client.set_read_timeout(60, 0);
  bool ready = false;
  for (int attempt = 0; attempt < 200 && !ready; ++attempt) {
    if (auto result = client.Get("/api/version"); result && result->status == 200)
      ready = true;
    else
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  check(ready, "server answers /api/version");

  if (ready) {
    const auto mine = [&](const std::string& path,
                          std::vector<std::pair<std::string, std::string>> fields) {
      httplib::MultipartFormDataItems items = {{"log", log, "log.csv", "text/csv"}};
      for (auto& [name, value] : fields) items.push_back({name, value, "", ""});
      return client.Post(path, items);
    };

    // OCPN discovery policy defaults (fuzz review H3). One item object with
    // d, é, a, a: pm4py.discover_oc_petri_net mines it without fall-throughs
    // and gives ->(d, é, *(tau, a)), so a is optional. Omitted flags must
    // keep the native defaults (both true) rather than the IM defaults,
    // which give ->(d, é, *(a, tau)).
    {
      const std::string ocel =
          "{\"ocel:global-event\":{\"ocel:activity\":\"__INVALID__\"},"
          "\"ocel:global-object\":{\"ocel:type\":\"__INVALID__\"},"
          "\"ocel:global-log\":{\"ocel:attribute-names\":[],\"ocel:object-types\":[\"item\"],"
          "\"ocel:version\":\"1.0\",\"ocel:ordering\":\"timestamp\"},\"ocel:events\":{"
          "\"e0\":{\"ocel:activity\":\"d\",\"ocel:timestamp\":\"2020-01-01T00:00:00\",\"ocel:omap\":[\"o2\"],\"ocel:vmap\":{}},"
          "\"e1\":{\"ocel:activity\":\"\xc3\xa9\",\"ocel:timestamp\":\"2020-01-02T00:00:00\",\"ocel:omap\":[\"o2\"],\"ocel:vmap\":{}},"
          "\"e2\":{\"ocel:activity\":\"a\",\"ocel:timestamp\":\"2020-01-03T00:00:00\",\"ocel:omap\":[\"o2\"],\"ocel:vmap\":{}},"
          "\"e3\":{\"ocel:activity\":\"a\",\"ocel:timestamp\":\"2020-01-03T01:00:00\",\"ocel:omap\":[\"o2\"],\"ocel:vmap\":{}}},"
          "\"ocel:objects\":{\"o2\":{\"ocel:type\":\"item\",\"ocel:ovmap\":{}}}}";
      const auto discover = [&](const std::string& query) {
        return client.Post("/api/ocel/discover?variant=ocpn" + query, ocel, "application/json");
      };
      const auto omitted = discover("");
      const auto enabled = discover("&ocpn_disable_strict_sequence_cut=true&ocpn_disable_fallthroughs=true");
      const auto disabled = discover("&ocpn_disable_strict_sequence_cut=false&ocpn_disable_fallthroughs=false");
      check(omitted && omitted->status == 200 && enabled && enabled->status == 200 &&
                omitted->body == enabled->body,
            "OCPN discovery defaults both policy flags to true: " + describe(omitted));
      // Activities are sorted (a, d, é): é feeds place 3, a silent step moves
      // to place 4, a loops back to 3 and a silent step reaches the sink.
      check(contains(omitted, "{\"activity\":2,\"inputs\":[2],\"outputs\":[3]},"
                              "{\"activity\":null,\"inputs\":[3],\"outputs\":[4]},"
                              "{\"activity\":0,\"inputs\":[4],\"outputs\":[3]},"
                              "{\"activity\":null,\"inputs\":[4],\"outputs\":[1]}"),
            "OCPN default net is ->(d, \xc3\xa9, *(tau, a)): " + describe(omitted));
      check(disabled && disabled->status == 200 && disabled->body != omitted->body,
            "explicit false OCPN policy flags still select the IM defaults: " + describe(disabled));
      const auto playout = client.Post(
          "/api/ocel/playout?model=ocpn&variant=extensive&max_traces=4&include_model=true", ocel,
          "application/json");
      const auto playout_enabled = client.Post(
          "/api/ocel/playout?model=ocpn&variant=extensive&max_traces=4&include_model=true"
          "&ocpn_disable_strict_sequence_cut=true&ocpn_disable_fallthroughs=true",
          ocel, "application/json");
      const auto playout_disabled = client.Post(
          "/api/ocel/playout?model=ocpn&variant=extensive&max_traces=4&include_model=true"
          "&ocpn_disable_strict_sequence_cut=false&ocpn_disable_fallthroughs=false",
          ocel, "application/json");
      check(playout && playout_enabled && playout_disabled && playout->status == 200 &&
                playout_enabled->status == 200 && playout_disabled->status == 200 &&
                playout->body == playout_enabled->body && playout->body != playout_disabled->body,
            "OCPN playout discovery defaults both policy flags to true: " + describe(playout));
    }

    // Item 8: unknown route and wrong method carry JSON bodies.
    auto result = client.Get("/api/nonexistent");
    check(result && result->status == 404 &&
              contains(result, "\"error\"") &&
              contains(result, "unknown API route: /api/nonexistent"),
          "unknown /api route gives a JSON 404: " + describe(result));
    result = client.Post("/api/version", "", "text/plain");
    check(result && result->status == 405 &&
              result->get_header_value("Allow") == "GET" &&
              contains(result, "\"error\""),
          "wrong method gives a JSON 405 with Allow: " + describe(result));

    // A plain mine still succeeds.
    result = mine("/api/mine", {});
    check(result && result->status == 200, "plain mine: " + describe(result));

    // Item 6: multipart /api/mine rejects query parameters and unknown fields.
    result = mine("/api/mine?dependency_threshold=abc", {});
    check(result && result->status == 400 && contains(result, "query-string parameter"),
          "multipart mine rejects query parameters: " + describe(result));
    result = mine("/api/mine", {{"dependency_treshold", "0.99"}});
    check(result && result->status == 400 &&
              contains(result, "unknown or inapplicable /api/mine form field: dependency_treshold"),
          "multipart mine rejects a misspelled field: " + describe(result));
    result = mine("/api/mine", {{"dependency_threshold", "0.9"}});
    check(result && result->status == 200,
          "multipart mine accepts a known field: " + describe(result));

    // Round BS: a route that takes its controls in the query string rejects
    // any multipart part besides its uploads (a control sent as a form field
    // was dropped silently before).
    {
      const std::string dfg = "1\na\n1\n0x1\n1\n0x1\n";
      httplib::MultipartFormDataItems items = {{"dfg", dfg, "model.dfg", "text/plain"},
                                               {"activity_attribute", "concept:name", "", ""}};
      result = client.Post("/api/dfg-precision", items);
      check(result && result->status == 400 &&
                contains(result, "multipart field activity_attribute is not read by /api/dfg-precision"),
            "DFG precision rejects a control sent as a form field: " + describe(result));
      items.pop_back();
      result = client.Post("/api/dfg-precision?activity_attribute=concept%3Aname", items);
      check(result && result->status == 200,
            "DFG precision reads its control from the query string: " + describe(result));
      httplib::MultipartFormDataItems ocel = {{"log", "{}", "log.jsonocel", "application/json"},
                                              {"zz_unknown", "1", "", ""}};
      result = client.Post("/api/ocel/statistics", ocel);
      check(result && result->status == 400 &&
                contains(result, "multipart field zz_unknown is not read by /api/ocel/statistics"),
            "OCEL statistics rejects an unread multipart part: " + describe(result));
    }

    // Items 2 and 5: strict values and repeated controls.
    for (const auto* bad : {"1junk", "abc", "-1", "", " 1", "0x5"}) {
      result = mine("/api/mine", {{"top_variants", bad}});
      check(result && result->status == 400,
            std::string("top_variants='") + bad + "' is rejected: " + describe(result));
    }
    result = mine("/api/mine", {{"top_variants", "1"}, {"top_variants", "3"}});
    check(result && result->status == 400 && contains(result, "more than once"),
          "a repeated mine field is rejected: " + describe(result));
    result = mine("/api/mine", {{"rework_activity", "A"}, {"rework_min_occurrences", "-5"}});
    check(result && result->status == 400,
          "negative rework_min_occurrences is rejected: " + describe(result));
    result = mine("/api/mine", {{"delimiter", ";x"}});
    check(result && result->status == 400,
          "a multi-character delimiter is rejected: " + describe(result));

    // Item 1: error bodies stay valid UTF-8 JSON whatever the value holds.
    result = mine("/api/mine", {{"path_predecessor", "A"}, {"path_successor", "B"},
                                {"path_min_performance", "1\nx\xff"}});
    check(result && result->status == 400 && utf8_without_controls(result->body) &&
              contains(result, "\\n") && contains(result, "\\ufffd"),
          "error JSON escapes controls and invalid bytes: " + describe(result));

    result = mine("/api/mine", {});
    check(result && result->status == 200, "re-mine: " + describe(result));
    result = client.Get("/api/dfg?noise_threshold=0.1&noise_threshold=abc");
    check(result && result->status == 400 && contains(result, "more than once"),
          "a repeated query parameter is rejected: " + describe(result));

    // Plausible item: model export and alignment default to inductive.
    result = client.Get("/api/pnml");
    check(result && result->status == 200 && contains(result, "<pnml"),
          "/api/pnml defaults to the inductive net: " + describe(result));
    result = client.Post("/api/align", "", "application/x-www-form-urlencoded");
    check(result && result->status == 200,
          "/api/align defaults to the inductive net: " + describe(result));
    result = client.Get("/api/pnml?miner=");
    check(result && result->status == 400,
          "an explicit empty miner is still rejected: " + describe(result));

    // Imported-model binding: each check-model answer carries a fresh id, and
    // a run that echoes a superseded id is refused instead of answering for
    // the newer model.
    {
      const auto pnml = client.Get("/api/pnml?miner=inductive");
      check(pnml && pnml->status == 200, "inductive PNML for import: " + describe(pnml));
      const auto import_model = [&]() -> std::string {
        httplib::MultipartFormDataItems items = {
            {"model", pnml ? pnml->body : std::string{}, "model.pnml", "application/xml"}};
        const auto checked = client.Post("/api/check-model", items);
        const std::string key = "\"imported_model_id\":";
        const auto at = checked ? checked->body.find(key) : std::string::npos;
        check(checked && checked->status == 200 && at == 1,
              "check-model reports its imported model id: " + describe(checked));
        if (at == std::string::npos) return {};
        const auto begin = at + key.size();
        return checked->body.substr(begin, checked->body.find(',', begin) - begin);
      };
      const auto first = import_model(), second = import_model();
      check(!first.empty() && !second.empty() && first != second,
            "each import gets a fresh id: " + first + " vs " + second);
      result = client.Post("/api/align?miner=imported", {{"X-Minera-Imported-Model", second}},
                           "", "application/x-www-form-urlencoded");
      check(result && result->status == 200,
            "a run bound to the current imported model runs: " + describe(result));
      result = client.Post("/api/align?miner=imported", {{"X-Minera-Imported-Model", first}},
                           "", "application/x-www-form-urlencoded");
      check(result && result->status == 409 && contains(result, "check the model again"),
            "a run bound to a replaced imported model is refused: " + describe(result));
      result = client.Get("/api/playout?miner=imported&traces=5",
                          {{"X-Minera-Imported-Model", first}});
      check(result && result->status == 409,
            "a stale bound playout is refused: " + describe(result));
      result = client.Post("/api/align?miner=imported", "", "application/x-www-form-urlencoded");
      check(result && result->status == 200,
            "an unbound run keeps working: " + describe(result));
    }

    // Item 4: basic Petri playout generates pm4py's 1000 traces by default.
    result = client.Get("/api/playout?miner=inductive");
    check(result && result->status == 200 && count(result->body, "<trace>") == 1000,
          "default playout has 1000 traces: " +
              (result ? std::to_string(count(result->body, "<trace>")) : describe(result)));
    // An omitted trace count never fails the 1,000,000-event budget: with a
    // long per-trace cap it shrinks to budget / cap (declared), while an
    // explicit count is still checked as given.
    result = client.Get("/api/playout?miner=inductive&max_steps=5000");
    check(result && result->status == 200 && count(result->body, "<trace>") == 200,
          "long max_steps scales the default trace count: " +
              (result && result->status == 200 ? std::to_string(count(result->body, "<trace>"))
                                               : describe(result)));
    result = client.Get("/api/playout?miner=inductive&max_steps=10000");
    check(result && result->status == 200 && count(result->body, "<trace>") == 100,
          "the longest max_steps still plays out: " +
              (result && result->status == 200 ? std::to_string(count(result->body, "<trace>"))
                                               : describe(result)));
    result = client.Get("/api/playout?miner=inductive&max_steps=1000");
    check(result && result->status == 200 && count(result->body, "<trace>") == 1000,
          "max_steps 1000 keeps 1000 default traces: " + describe(result));
    result = client.Get("/api/playout?miner=inductive&max_steps=5000&traces=1000");
    check(result && result->status == 400 && contains(result, "event budget"),
          "an explicit over-budget trace count is rejected: " + describe(result));
    result = client.Get("/api/playout?miner=stochastic&stochastic_max_steps=5000");
    check(result && result->status == 200 && count(result->body, "<trace>") == 200,
          "stochastic playout scales its default trace count: " +
              (result && result->status == 200 ? std::to_string(count(result->body, "<trace>"))
                                               : describe(result)));
    result = client.Get("/api/playout?miner=stochastic&stochastic_max_steps=5000&stochastic_traces=1000");
    check(result && result->status == 400 && contains(result, "event budget"),
          "an explicit over-budget stochastic count is rejected: " + describe(result));
    result = client.Get("/api/playout?miner=dfg&dfg_playout_variant=performance&dfg_playout_max_steps=5000");
    check(result && result->status == 200,
          "DFG performance playout scales its default trace count: " + describe(result));
    result = client.Get("/api/playout?miner=dfg&dfg_playout_variant=performance&dfg_playout_max_steps=5000&dfg_playout_traces=1000");
    check(result && result->status == 400 && contains(result, "event budget"),
          "an explicit over-budget DFG count is rejected: " + describe(result));
    // EXTENSIVE process-tree playout cut at max_trace_cap says so: the JSON
    // answer in "truncated", the XES download in X-Minera-Truncated.
    result = client.Get("/api/playout?miner=process_tree&variant=EXTENSIVE&max_length=6&max_trace_cap=1&return_set_strings=true");
    check(result && result->status == 200 &&
              result->get_header_value("X-Minera-Truncated") ==
                  (contains(result, "\"truncated\":true") ? "true" : "false"),
          "process-tree playout JSON and its truncation header agree: " + describe(result));
    const bool tree_truncated = result && result->get_header_value("X-Minera-Truncated") == "true";
    result = client.Get("/api/playout?miner=process_tree&variant=EXTENSIVE&max_length=6&max_trace_cap=1");
    check(result && result->status == 200 && count(result->body, "<trace>") <= 1 &&
              result->get_header_value("X-Minera-Truncated") == (tree_truncated ? "true" : "false"),
          "the process-tree playout XES download carries X-Minera-Truncated: " + describe(result));
    result = client.Get("/api/playout?miner=process_tree&variant=EXTENSIVE&max_length=0");
    check(result && result->status == 200 && result->get_header_value("X-Minera-Truncated") == "false",
          "an untruncated process-tree playout says X-Minera-Truncated: false: " + describe(result));
    // Item 9: an empty number is not zero.
    result = client.Get("/api/playout?miner=inductive&start=");
    check(result && result->status == 400, "empty start is rejected: " + describe(result));
    result = client.Get("/api/playout?miner=inductive&case_interval=");
    check(result && result->status == 400,
          "empty case_interval is rejected: " + describe(result));

    // Item 7 and the release fix on live sessions.
    result = client.Post("/api/live-stream/create?method=dfg&session_id=c1&streaming_include_diagnostics=maybe",
                         "", "application/x-www-form-urlencoded");
    check(result && result->status == 400,
          "a non-boolean streaming_include_diagnostics is rejected: " + describe(result));
    result = client.Post("/api/live-stream/create?method=dfg&session_id=c1&source_name=x.pnml",
                         "", "application/x-www-form-urlencoded");
    check(result && result->status == 400 && contains(result, "source_name"),
          "source_name without a model is rejected: " + describe(result));
    result = client.Post("/api/live-stream/create?method=dfg&session_id=c1&streaming_include_diagnostics=true",
                         "", "application/x-www-form-urlencoded");
    check(result && result->status == 200, "live create: " + describe(result));
    {
      httplib::MultipartFormDataItems items = {{"log", log, "log.csv", "text/csv"}};
      result = client.Post("/api/live-stream/append?session_id=c1&expected_revision=0&batch_id=b1&format=csv",
                           items);
      check(result && result->status == 200 && contains(result, "\"state\":\"INACTIVE\""),
            "queued append on an unstarted session: " + describe(result));
    }
    result = client.Post("/api/live-stream/release?session_id=c1&expected_revision=1", "",
                         "application/x-www-form-urlencoded");
    check(result && result->status == 200 && contains(result, "\"released\":true"),
          "an unstarted session with queued events can be released: " + describe(result));
    result = client.Get("/api/live-stream/snapshot?session_id=c1");
    check(result && result->status == 404, "the released session is gone: " + describe(result));

    // Mining rollback: a written response keeps the previous source for a
    // cancel until it is confirmed or the window ends.
    {
      const auto mine_as = [&](const std::string& id) {return mine("/api/mine", {{"mining_request_id", id}});};
      const auto cancel = [&](const std::string& id) {
        return client.Post("/api/cancel-mine?request_id=" + id, "", "application/x-www-form-urlencoded");
      };
      const auto held_after = [&](const httplib::Result& canceled) {
        if (!canceled) return std::string("no response");
        const std::string key = "\"active_source_id\":\"";
        const auto at = canceled->body.find(key);
        if (at == std::string::npos) return canceled->body;
        return canceled->body.substr(at + key.size(), canceled->body.find('"', at + key.size()) - at - key.size());
      };
      const std::string a = "00000000-0000-4000-8000-00000000000a", b = "00000000-0000-4000-8000-00000000000b",
                        c = "00000000-0000-4000-8000-00000000000c", d = "00000000-0000-4000-8000-00000000000d",
                        e = "00000000-0000-4000-8000-00000000000e";
      result = mine_as(a);
      const auto source_a = source_id_of(result);
      check(result && result->status == 200 && !source_a.empty(), "rollback base mine: " + describe(result));
      result = client.Post("/api/mine-ack?request_id=" + a, "", "application/x-www-form-urlencoded");
      check(result && result->status == 200 && contains(result, "\"acknowledged\":true"), "mine-ack: " + describe(result));
      result = client.Post("/api/mine-ack?request_id=" + a + "&x=1", "", "application/x-www-form-urlencoded");
      check(result && result->status == 400, "mine-ack takes only the request id: " + describe(result));

      // The client read the whole response but never confirmed it.
      result = mine_as(b);
      const auto source_b = source_id_of(result);
      check(result && result->status == 200 && !source_b.empty() && source_b != source_a, "rollback mine: " + describe(result));
      auto canceled = cancel(b);
      check(held_after(canceled) == source_a,
            "cancel after the response was written restores the previous source: " + describe(canceled));
      result = client.Get("/api/active-source?source_id=" + source_a);
      check(result && result->status == 200, "the restored source answers: " + describe(result));

      // An explicit ack releases the rollback.
      result = mine_as(c);
      const auto source_c = source_id_of(result);
      result = client.Post("/api/mine-ack?request_id=" + c, "", "application/x-www-form-urlencoded");
      check(result && contains(result, "\"acknowledged\":true"), "ack of a written response: " + describe(result));
      canceled = cancel(c);
      check(!source_c.empty() && held_after(canceled) == source_c, "cancel after an ack keeps the new source: " + describe(canceled));

      // A request bound to the new source id releases it too.
      result = mine_as(d);
      const auto source_d = source_id_of(result);
      result = client.Get("/api/active-source?source_id=" + source_d);
      check(result && result->status == 200, "the new source answers: " + describe(result));
      canceled = cancel(d);
      check(!source_d.empty() && held_after(canceled) == source_d,
            "cancel after a request on the new source keeps it: " + describe(canceled));

      // The window's end releases an unconfirmed rollback.
      result = mine_as(e);
      const auto source_e = source_id_of(result);
      std::this_thread::sleep_for(std::chrono::milliseconds(rollback_window_ms + 1000));
      canceled = cancel(e);
      check(!source_e.empty() && held_after(canceled) == source_e,
            "cancel after the rollback window keeps the new source: " + describe(canceled));
      result = client.Post("/api/mine-ack?request_id=" + e, "", "application/x-www-form-urlencoded");
      check(result && contains(result, "\"acknowledged\":false"), "a canceled request cannot be acknowledged: " + describe(result));
    }

    // The reader snapshot still answers after the lock change.
    {
      httplib::MultipartFormDataItems items = {{"log", log, "log.csv", "text/csv"}};
      result = client.Post("/api/stream-reader/create?session_id=r1&kind=csv_events", items);
      check(result && result->status == 200, "reader create: " + describe(result));
      result = client.Get("/api/stream-reader/snapshot?session_id=r1");
      check(result && result->status == 200 && contains(result, "STREAM_READER_SESSION"),
            "reader snapshot: " + describe(result));
    }

    // Logs whose text XES cannot carry (a C0 control, Latin-1 bytes): Petri
    // playout answers 400 JSON, and routes that embed XES keep their result
    // and report the refusal in the matching *_error field.
    for (const std::string activity : {std::string("A\x01x"), std::string("Pr\xfc" "fung")}) {
      const std::string bad_log =
          "case_id,activity,timestamp\n"
          "c1," + activity + ",2024-01-01T00:00:00Z\nc1,B,2024-01-01T00:01:00Z\n"
          "c2," + activity + ",2024-01-01T00:00:00Z\nc2,B,2024-01-01T00:01:00Z\n";
      const auto label = activity[1] == '\x01' ? std::string("control char") : std::string("Latin-1");
      httplib::MultipartFormDataItems items = {{"log", bad_log, "log.csv", "text/csv"}};
      result = client.Post("/api/mine", items);
      check(result && result->status == 200 && valid_utf8(result->body),
            label + " mine answers valid UTF-8 JSON: " + describe(result));
      const auto source = source_id_of(result);
      check(!source.empty(), label + " mine returns a source id: " + describe(result));
      const auto refused = "XES cannot represent";
      // Every JSON body is valid UTF-8: the Latin-1 byte reaches the result
      // JSON as U+FFFD (one per maximal ill-formed subpart).
      const bool latin1 = label == "Latin-1";
      const auto valid_body = [&](const httplib::Result& r) {
        return valid_utf8(r->body) &&
               (!latin1 || r->body.find("Pr\\ufffdfung") != std::string::npos);
      };

      // The XML exports carry the label as U+FFFD (a C0 control too, which
      // XML 1.0 cannot hold even as a reference), so they stay well-formed.
      for (const char* target : {"/api/pnml", "/api/bpmn"}) {
        result = client.Get(target);
        check(result && result->status == 200 && well_formed_xml(result->body) &&
                  result->body.find(latin1 ? "Pr\xEF\xBF\xBD" "fung" : "A\xEF\xBF\xBDx") != std::string::npos,
              label + " " + target + " is well-formed XML: " + describe(result));
      }

      result = client.Get("/api/playout?miner=inductive&traces=3");
      check(result && result->status == 400 && contains(result, "\"error\"") &&
                contains(result, refused) && utf8_without_controls(result->body),
            label + " Petri playout gives 400 JSON: " + describe(result));

      result = client.Post("/api/log-util?variant=split_train_test&include_source=true&source_id=" +
                               source, "", "application/x-www-form-urlencoded");
      check(result && result->status == 200 && contains(result, "\"xes\":null") &&
                contains(result, std::string("\"xes_error\":\"") + refused) &&
                valid_body(result),
            label + " preparation keeps its result and reports xes_error: " + describe(result));

      result = client.Post("/api/label-splitting?include_source=true&source_id=" + source, "",
                           "application/x-www-form-urlencoded");
      check(result && result->status == 200 && contains(result, "LOG_TRANSFORMATION") &&
                contains(result, "\"transformed_xes\":null") &&
                contains(result, std::string("\"transformed_xes_error\":\"") + refused) &&
                valid_body(result),
            label + " transformation keeps its result and reports transformed_xes_error: " +
                describe(result));

      result = client.Post("/api/anonymize-trace-variants?include_xes=true&tvq_epsilon=100&tvq_k=1&tvq_seed=1&source_id=" +
                               source, "", "application/x-www-form-urlencoded");
      check(result && result->status == 200 && contains(result, "ANONYMIZATION_RELEASE") &&
                contains(result, "\"released_xes\":null") &&
                contains(result, std::string("\"released_xes_error\":\"") + refused) &&
                valid_body(result),
            label + " anonymization keeps its release and reports released_xes_error: " +
                describe(result));

      result = client.Post("/api/monte-carlo?miner=inductive&include_workspace=true&source_id=" +
                               source, "", "application/x-www-form-urlencoded");
      check(result && result->status == 200 && contains(result, "SIMULATION_WORKSPACE") &&
                contains(result, "\"xes\":null") &&
                contains(result, std::string("\"xes_error\":\"") + refused) &&
                valid_body(result),
            label + " simulation workspace keeps its result and reports xes_error: " +
                describe(result));
    }

    // Content-Length after the JSON UTF-8 repair (review H1): a Latin-1 log
    // makes the handlers write ill-formed UTF-8, which is repaired to U+FFFD
    // (one byte becomes three) before httplib frames the response, so each
    // body is exactly Content-Length bytes and parses as JSON. Read over raw
    // sockets to EOF, so a body longer than its Content-Length is seen.
    {
      const std::string latin1_log =
          "case_id,activity,timestamp\n"
          "c1,Pr\xfc" "fung,2024-01-01T00:00:00Z\nc1,B\xe4r,2024-01-01T00:01:00Z\n"
          "c2,Pr\xfc" "fung,2024-01-01T00:00:00Z\nc2,B\xe4r,2024-01-01T00:01:00Z\n";
      httplib::MultipartFormDataItems items = {{"log", latin1_log, "log.csv", "text/csv"}};
      result = client.Post("/api/mine", items);
      check(result && result->status == 200, "Latin-1 sweep mine: " + describe(result));
      const auto source = source_id_of(result);
      struct Route { const char* method; std::string target; };
      const std::vector<Route> routes = {
          {"GET", "/api/dfg"}, {"GET", "/api/trie"}, {"GET", "/api/dfg-dataframe"},
          {"GET", "/api/active-source?source_id=" + source}, {"GET", "/api/stochastic-map"},
          {"GET", "/api/process-tree-petri"}, {"GET", "/api/trie-to-petri"},
          {"GET", "/api/interval-tree"}, {"GET", "/api/playout?miner=dfg&dfg_playout_traces=3"},
          {"POST", "/api/dfg"}, {"POST", "/api/discover-dfg"}, {"POST", "/api/case-statistics"},
          {"POST", "/api/attribute-statistics"}, {"POST", "/api/boundary-activities"},
          {"POST", "/api/align"}, {"POST", "/api/footprints-workbench"}, {"POST", "/api/features"},
          {"POST", "/api/cycle-time"}, {"POST", "/api/log-util?variant=split_train_test&include_source=true&source_id=" + source},
          {"POST", "/api/label-splitting?include_source=true&source_id=" + source},
          // Error bodies that echo the ill-formed target or value.
          {"GET", "/api/nonexistent-Pr\xfc" "fung"},
          {"GET", "/api/dfg?noise_threshold=\xfc"},
          {"POST", "/api/version"},
      };
      int repaired = 0;
      for (const auto& route : routes) {
        const auto response = raw_exchange(port, route.method, route.target);
        const auto what = std::string(route.method) + " " + route.target + ": " + describe(response);
        check(response.complete, "raw response for " + what);
        if (!response.complete) continue;
        check(response.header("Content-Length") == std::to_string(response.body.size()),
              "body length equals Content-Length for " + what);
        if (response.header("Content-Type").starts_with("application/json")) {
          check(valid_json(response.body), "body parses as JSON for " + what);
          if (response.body.find("\\ufffd") != std::string::npos) ++repaired;
        }
      }
      check(repaired >= 8, "the sweep reached repaired bodies (" + std::to_string(repaired) + ")");

      // XML and text exports take the same U+FFFD policy as raw characters,
      // so PNML and BPMN parse as XML and the DFG text is valid UTF-8.
      const std::string replaced = "Pr\xEF\xBF\xBD" "fung";
      for (const char* target : {"/api/pnml", "/api/pnml?miner=alpha", "/api/bpmn", "/api/bpmn?miner=alpha",
                                 "/api/dfg"}) {
        const auto response = raw_exchange(port, "GET", target);
        const auto what = std::string("GET ") + target + ": " + describe(response);
        const bool text = response.header("Content-Type").starts_with("text/");
        check(response.complete && response.status == 200 &&
                  response.header("Content-Length") == std::to_string(response.body.size()) &&
                  valid_utf8(response.body) && response.body.find(replaced) != std::string::npos,
              "Latin-1 labels reach the export as U+FFFD for " + what);
        if (!text)
          check(response.header("Content-Type").starts_with("application/xml") && well_formed_xml(response.body),
                "the export parses as XML for " + what);
      }
      // The same bodies gzipped inflate to the repaired bytes.
#if defined(MINERA_TEST_HAS_ZLIB)
      {
        const auto plain = raw_exchange(port, "GET", "/api/pnml");
        const auto gz = raw_exchange(port, "GET", "/api/pnml", "Accept-Encoding: gzip\r\n");
        const auto inflated = gunzip(gz.body);
        check(gz.header("Content-Encoding") == "gzip" && inflated && *inflated == plain.body,
              "a gzipped PNML export inflates to the repaired body: " + describe(gz));
      }
#endif

      // A Range request slices the repaired body: 206 with exact framing.
      const auto full = raw_exchange(port, "GET", "/api/trie");
      check(full.complete && full.status == 200 && full.body.find("\\ufffd") != std::string::npos,
            "trie for the range check: " + describe(full));
      const auto slice = raw_exchange(port, "GET", "/api/trie", "Range: bytes=0-15\r\n");
      check(slice.status == 206 && slice.body == full.body.substr(0, 16) &&
                slice.header("Content-Length") == "16" &&
                slice.header("Content-Range") == "bytes 0-15/" + std::to_string(full.body.size()),
            "range slice of a repaired body: " + describe(slice));
      const auto tail = raw_exchange(port, "GET", "/api/trie", "Range: bytes=-10\r\n");
      check(tail.status == 206 && full.body.size() >= 10 &&
                tail.body == full.body.substr(full.body.size() - 10) &&
                tail.header("Content-Length") == "10",
            "suffix range of a repaired body: " + describe(tail));

      // gzip on Accept-Encoding: the repaired body, compressed after the
      // repair, inflates to exactly the identity bytes; a Range request
      // stays identity (206 of the JSON's own bytes); no Accept-Encoding,
      // no gzip. Without zlib the server answers identity throughout.
      const auto gz = raw_exchange(port, "GET", "/api/trie", "Accept-Encoding: gzip\r\n");
      check(gz.complete && gz.status == 200 &&
                gz.header("Content-Length") == std::to_string(gz.body.size()),
            "gzip trie framing: " + describe(gz));
#if defined(MINERA_TEST_HAS_ZLIB)
      const auto inflated = gunzip(gz.body);
      check(gz.header("Content-Encoding") == "gzip" && gz.header("Vary") == "Accept-Encoding" &&
                inflated && *inflated == full.body && gz.body.size() < full.body.size(),
            "gzip trie inflates to the identity bytes: " + describe(gz));
      check(full.header("Vary") == "Accept-Encoding" && full.header("Content-Encoding").empty(),
            "the identity answer names Vary and no encoding: " + full.head);
#else
      check(gz.header("Content-Encoding").empty() && gz.body == full.body,
            "without zlib a gzip request gets the identity body: " + describe(gz));
#endif
      const auto gz_slice =
          raw_exchange(port, "GET", "/api/trie", "Accept-Encoding: gzip\r\nRange: bytes=0-15\r\n");
      check(gz_slice.status == 206 && gz_slice.header("Content-Encoding").empty() &&
                gz_slice.body == full.body.substr(0, 16) && gz_slice.header("Content-Length") == "16" &&
                gz_slice.header("Content-Range") == "bytes 0-15/" + std::to_string(full.body.size()),
            "a range with Accept-Encoding: gzip is an identity slice: " + describe(gz_slice));
      const auto gz_head = raw_exchange(port, "GET", "/api/version", "Accept-Encoding: identity\r\n");
      check(gz_head.status == 200 && gz_head.header("Content-Encoding").empty(),
            "Accept-Encoding without gzip gets identity: " + describe(gz_head));
      // A gzip request body is still refused, as without zlib.
      const auto gz_upload = raw_exchange(port, "POST", "/api/version", "Content-Encoding: gzip\r\n", "x");
      check(gz_upload.status == 415, "a gzip request body is refused with 415: " + describe(gz_upload));

      // The streamed /api/mine body: the same bytes as an identity mine of
      // the same log, except its fresh source id.
      const auto mine_target = [&](const char* id) {
        return std::string("/api/mine?mining_request_id=00000000-0000-4000-8000-0000000000") + id;
      };
      const auto plain_mine = raw_exchange(port, "POST", mine_target("f1"), "", log, "text/csv");
      result = client.Post("/api/mine-ack?request_id=00000000-0000-4000-8000-0000000000f1", "", "application/x-www-form-urlencoded");
      const auto gz_mine = raw_exchange(port, "POST", mine_target("f2"), "Accept-Encoding: gzip\r\n", log, "text/csv");
      result = client.Post("/api/mine-ack?request_id=00000000-0000-4000-8000-0000000000f2", "", "application/x-www-form-urlencoded");
      check(plain_mine.status == 200 && plain_mine.header("Content-Encoding").empty() &&
                plain_mine.header("Content-Length") == std::to_string(plain_mine.body.size()),
            "identity mine: " + describe(plain_mine));
      check(gz_mine.status == 200 && gz_mine.header("Content-Length") == std::to_string(gz_mine.body.size()),
            "gzip mine framing: " + describe(gz_mine));
#if defined(MINERA_TEST_HAS_ZLIB)
      const auto mined = gunzip(gz_mine.body);
      check(gz_mine.header("Content-Encoding") == "gzip" && mined &&
                without_source_ids(*mined) == without_source_ids(plain_mine.body) &&
                *mined != plain_mine.body,
            "gzip mine inflates to the identity mine bytes: " + describe(gz_mine));
#else
      check(gz_mine.header("Content-Encoding").empty() &&
                without_source_ids(gz_mine.body) == without_source_ids(plain_mine.body),
            "without zlib the mine is identity: " + describe(gz_mine));
#endif

      // Accept-Encoding is parsed with its q-values (one parser for httplib's
      // compression and the streamed mine): q=0, identity alone or a higher
      // identity preference get the identity body.
      {
        using httplib::detail::accepts_content_coding;
        const std::vector<std::pair<std::string, bool>> cases = {
            {"gzip", true}, {"GZIP", true}, {"x-gzip", true}, {"deflate, gzip;q=0.5", true},
            {"gzip;q=1.000", true}, {"*", true}, {"*;q=0.1", true}, {"gzip ; q=0.001", true},
            {"", false}, {"identity", false}, {"gzip;q=0", false}, {"gzip;q=0.000, identity", false},
            {"gzip;q=0, *", false}, {"*;q=0", false}, {"deflate", false}, {"gzipx", false},
            {"notgzip", false}, {"gzip;q=0.5, identity;q=1", false}, {"gzip;q=1.5", false},
            {"gzip;q=abc", false}, {"gzip;q=0.0001", false}, {"identity;q=0, gzip;q=0.2", true},
            {"br;q=1, gzip;q=0.9", true}, {"*;q=0, gzip", true}};
        for (const auto& [header, expected] : cases)
          check(accepts_content_coding(header, "gzip") == expected,
                "Accept-Encoding \"" + header + "\" " + (expected ? "accepts" : "refuses") + " gzip");
      }
      const auto current_trie = raw_exchange(port, "GET", "/api/trie");  // the mines above replaced the source
      for (const char* refuse : {"gzip;q=0, identity", "identity", "*;q=0", "gzip;q=0.2, identity;q=0.8"}) {
        const auto plain = raw_exchange(port, "GET", "/api/trie", std::string("Accept-Encoding: ") + refuse + "\r\n");
        check(plain.status == 200 && plain.header("Content-Encoding").empty() && plain.body == current_trie.body,
              std::string("GET with Accept-Encoding: ") + refuse + " is identity: " + describe(plain));
      }
      const auto q0_mine = raw_exchange(port, "POST", mine_target("f3"), "Accept-Encoding: gzip;q=0, identity\r\n", log, "text/csv");
      result = client.Post("/api/mine-ack?request_id=00000000-0000-4000-8000-0000000000f3", "", "application/x-www-form-urlencoded");
      check(q0_mine.status == 200 && q0_mine.header("Content-Encoding").empty() &&
                without_source_ids(q0_mine.body) == without_source_ids(plain_mine.body),
            "a mine with gzip;q=0 is identity: " + describe(q0_mine));
      // Range applies to GET only: a POST with Range gets the whole body.
      const auto ranged_mine = raw_exchange(port, "POST", mine_target("f4"), "Range: bytes=0-15\r\n", log, "text/csv");
      result = client.Post("/api/mine-ack?request_id=00000000-0000-4000-8000-0000000000f4", "", "application/x-www-form-urlencoded");
      check(ranged_mine.status == 200 && ranged_mine.header("Content-Range").empty() &&
                without_source_ids(ranged_mine.body) == without_source_ids(plain_mine.body),
            "a POST mine ignores Range: " + describe(ranged_mine));
      const auto ranged_post = raw_exchange(port, "POST", "/api/version", "Range: bytes=0-3\r\n", "");
      const auto unranged_post = raw_exchange(port, "POST", "/api/version", "", "");
      check(ranged_post.status == unranged_post.status && ranged_post.status != 206 &&
                ranged_post.header("Content-Range").empty() && ranged_post.body == unranged_post.body,
            "a POST ignores Range: " + describe(ranged_post));
#if defined(MINERA_TEST_HAS_ZLIB)
      // CORS and compression both vary the response: Vary names both.
      const std::string origin = "Origin: http://127.0.0.1:" + std::to_string(port) + "\r\n";
      const auto cors_gz = raw_exchange(port, "GET", "/api/trie", origin + "Accept-Encoding: gzip\r\n");
      check(cors_gz.status == 200 && cors_gz.header("Content-Encoding") == "gzip" &&
                cors_gz.header("Vary") == "Origin, Accept-Encoding",
            "a gzipped CORS answer varies on Origin and Accept-Encoding: " + cors_gz.head);
      const auto cors_mine = raw_exchange(port, "POST", mine_target("f5"), origin + "Accept-Encoding: gzip\r\n", log, "text/csv");
      result = client.Post("/api/mine-ack?request_id=00000000-0000-4000-8000-0000000000f5", "", "application/x-www-form-urlencoded");
      check(cors_mine.status == 200 && cors_mine.header("Content-Encoding") == "gzip" &&
                cors_mine.header("Vary") == "Origin, Accept-Encoding",
            "a gzipped CORS mine varies on Origin and Accept-Encoding: " + cors_mine.head);
#endif
    }

    // XES size limit (review M1): a request may raise the bound past the old
    // 256 MiB clamp up to the server ceiling (--max-xes-decompressed-bytes,
    // 2 GiB by default); above it the error names the flag.
    {
      const std::string xes =
          "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">"
          "<trace><string key=\"concept:name\" value=\"c1\"/>"
          "<event><string key=\"concept:name\" value=\"A\"/><date key=\"time:timestamp\" value=\"2024-01-01T00:00:00.000+00:00\"/></event>"
          "<event><string key=\"concept:name\" value=\"B\"/><date key=\"time:timestamp\" value=\"2024-01-01T00:01:00.000+00:00\"/></event>"
          "</trace></log>\n";
      const auto mine_xes = [&](const std::string& bytes) {
        httplib::MultipartFormDataItems items = {{"log", xes, "log.xes", "application/xml"},
                                                 {"xes_max_decompressed_bytes", bytes, "", ""}};
        return client.Post("/api/mine", items);
      };
      result = mine_xes("1073741824");
      check(result && result->status == 200, "a 1 GiB XES bound is accepted: " + describe(result));
      result = mine_xes("2147483649");
      check(result && result->status == 400 && contains(result, "--max-xes-decompressed-bytes") &&
                contains(result, "2147483648"),
            "a bound above the ceiling names the flag: " + describe(result));
      result = mine_xes("100");
      check(result && result->status == 400 && contains(result, "XES size limit of 100 bytes") &&
                contains(result, "xes_max_decompressed_bytes"),
            "a too-small bound names the limit: " + describe(result));
    }

    // Reader event estimate (review M2): the estimate is exactly the CSV rows
    // after the header (a trailing newline, blank lines and CRLF add none),
    // so the per-session limit of 1,000,000 events admits 1,000,000 rows.
    // Sources above the limit are refused before parsing, with the estimate
    // in the message; a source within it would be parsed (several GB for a
    // million rows), so the boundary is checked from above.
    {
      const auto rows = [](std::size_t count, const char* newline) {
        std::string csv = std::string("case_id,activity,timestamp") + newline;
        csv.reserve(count * 27 + 32);
        for (std::size_t i = 0; i < count; ++i)
          csv += "c" + std::to_string(i % 1000) + ",A,2024-01-01T00:00:00Z" + newline;
        return csv;
      };
      for (const auto& [label, source] : std::vector<std::pair<std::string, std::string>>{
               {"LF", rows(1000001, "\n")},
               {"CRLF with blank lines", rows(1000001, "\r\n") + "\r\n \r\n\r\n"}}) {
        httplib::MultipartFormDataItems items = {{"log", source, "log.csv", "text/csv"}};
        result = client.Post("/api/stream-reader/create?session_id=big&kind=csv_events", items);
        check(result && result->status == 413 &&
                  contains(result, "holds up to 1000001 events, above the per-session limit of 1000000"),
              "a 1,000,001-row reader (" + label + ") is refused with an exact estimate: " + describe(result));
      }
    }

    // A body sent below the minimum rate gets 408 naming that rate (review L4).
    {
      const int fd = connect_loopback(port);
      check(fd >= 0, "connect for the slow body");
      if (fd >= 0) {
        send_all(fd, "POST /api/mine HTTP/1.1\r\n" + host_header(port) + "Connection: close\r\n"
                     "Content-Type: multipart/form-data; boundary=x\r\nContent-Length: 1000000\r\n\r\n--x\r\n");
        // One byte every 200 ms is far below 64 KiB/s; the server cuts it
        // after the 5 s grace and answers.
        pollfd poll_fd{fd, POLLIN, 0};
        for (int drip = 0; drip < 100 && ::poll(&poll_fd, 1, 200) == 0; ++drip) send_all(fd, "x");
        const auto slow = read_raw_response(fd);
        check(slow.status == 408 && slow.body.find("KiB/s") != std::string::npos &&
                  valid_json(slow.body),
              "a slow body gets 408 naming the minimum rate: " + describe(slow));
      }
    }
  }

  // Idle expiry of live and reader sessions, on a second server with a short
  // MINERA_STREAM_IDLE_MS: a session nobody names for the expiry is freed and
  // its share of the stream budget returned; one that keeps getting requests
  // (snapshots, appends) stays.
  {
    ::setenv("MINERA_STREAM_IDLE_MS", "1500", 1);
    const int idle_port = free_port();
    const std::string idle_port_text = std::to_string(idle_port);
    std::vector<std::string> idle_args = {argv[1], "--port", idle_port_text, "--frontend",
                                          "/nonexistent-minera-frontend"};
    std::vector<char*> idle_raw;
    for (auto& arg : idle_args) idle_raw.push_back(arg.data());
    idle_raw.push_back(nullptr);
    pid_t idle_server = 0;
    const bool spawned = ::posix_spawn(&idle_server, argv[1], nullptr, nullptr, idle_raw.data(), environ) == 0;
    ::unsetenv("MINERA_STREAM_IDLE_MS");
    check(spawned, "start the idle-expiry server");
    if (spawned) {
      httplib::Client idle("127.0.0.1", idle_port);
      idle.set_read_timeout(60, 0);
      bool idle_ready = false;
      for (int attempt = 0; attempt < 200 && !idle_ready; ++attempt) {
        if (auto r = idle.Get("/api/version"); r && r->status == 200) idle_ready = true;
        else std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
      check(idle_ready, "idle-expiry server answers /api/version");
      const auto form = "application/x-www-form-urlencoded";
      auto r = idle.Post("/api/live-stream/create?method=dfg&session_id=idle_live", "", form);
      check(r && r->status == 200, "idle live create: " + describe(r));
      r = idle.Post("/api/live-stream/create?method=dfg&session_id=kept_live", "", form);
      check(r && r->status == 200, "kept live create: " + describe(r));
      {
        httplib::MultipartFormDataItems items = {{"log", log, "log.csv", "text/csv"}};
        r = idle.Post("/api/live-stream/append?session_id=idle_live&expected_revision=0&batch_id=b1&format=csv", items);
        check(r && r->status == 200, "idle live append: " + describe(r));
        r = idle.Post("/api/stream-reader/create?session_id=idle_reader&kind=csv_events", items);
        check(r && r->status == 200, "idle reader create: " + describe(r));
      }
      r = idle.Get("/api/live-stream/list");
      check(r && r->status == 200 && contains(r, "\"stream_budget\":{\"bytes\":") &&
                !contains(r, "\"stream_budget\":{\"bytes\":0,\"events\":0,"),
            "the sessions hold stream budget: " + describe(r));
      // Keep one session in use for twice the expiry: snapshots, then an append.
      int kept_revision = 0;
      for (int step = 0; step < 8; ++step) {
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        if (step == 5) {
          httplib::MultipartFormDataItems items = {{"log", log, "log.csv", "text/csv"}};
          r = idle.Post("/api/live-stream/append?session_id=kept_live&expected_revision=" +
                            std::to_string(kept_revision) + "&batch_id=k1&format=csv", items);
          check(r && r->status == 200, "an append keeps the session: " + describe(r));
          ++kept_revision;
        } else {
          r = idle.Get("/api/live-stream/snapshot?session_id=kept_live&wait_until_drained=false");
          check(r && r->status == 200, "a snapshot keeps the session (step " + std::to_string(step) + "): " + describe(r));
        }
      }
      r = idle.Get("/api/live-stream/snapshot?session_id=idle_live");
      check(r && r->status == 404 && contains(r, "expired after being idle"),
            "an idle live session expires: " + describe(r));
      r = idle.Get("/api/stream-reader/snapshot?session_id=idle_reader");
      check(r && r->status == 404 && contains(r, "expired after being idle"),
            "an idle reader session expires: " + describe(r));
      r = idle.Get("/api/live-stream/snapshot?session_id=kept_live&wait_until_drained=false");
      check(r && r->status == 200, "the session in use stays: " + describe(r));
      r = idle.Get("/api/live-stream/list");
      check(r && r->status == 200 && contains(r, "\"session_id\":\"kept_live\"") &&
                !contains(r, "idle_live"),
            "the live list holds only the session in use: " + describe(r));
      r = idle.Get("/api/stream-reader/list");
      check(r && r->status == 200 && contains(r, "{\"sessions\":[],"),
            "the reader list is empty: " + describe(r));
      // Only the kept session's append (log bytes and events) is still charged.
      const auto kept_events = r ? r->body.find("\"stream_budget\":{\"bytes\":" + std::to_string(log.size()) + ",") : std::string::npos;
      check(kept_events != std::string::npos,
            "the expired sessions returned their budget: " + describe(r));
      ::kill(idle_server, SIGTERM);
      int idle_status = 0;
      ::waitpid(idle_server, &idle_status, 0);
    }
  }

  // Mining admission, on a third server: MINERA_MINE_STAGE_PAUSE_MS slows
  // every stage boundary (250 ms) so the checks can act while a 4-event mine
// runs;
  // --mine-memory-budget 2000000 admits about 122 events of a three-column
  // CSV (16 KiB each) and --max-mine-events 60 fewer.
  {
    ::setenv("MINERA_MINE_STAGE_PAUSE_MS", "250", 1);
    const int limit_port = free_port();
    const std::string limit_port_text = std::to_string(limit_port);
    std::vector<std::string> limit_args = {argv[1], "--port", limit_port_text, "--frontend",
                                           "/nonexistent-minera-frontend", "--mine-memory-budget",
                                           "2000000", "--max-mine-events", "60"};
    std::vector<char*> limit_raw;
    for (auto& arg : limit_args) limit_raw.push_back(arg.data());
    limit_raw.push_back(nullptr);
    pid_t limit_server = 0;
    const bool spawned = ::posix_spawn(&limit_server, argv[1], nullptr, nullptr, limit_raw.data(), environ) == 0;
    ::unsetenv("MINERA_MINE_STAGE_PAUSE_MS");
    check(spawned, "start the mining-admission server");
    if (spawned) {
      httplib::Client limits("127.0.0.1", limit_port);
      limits.set_read_timeout(60, 0);
      bool limit_ready = false;
      for (int attempt = 0; attempt < 200 && !limit_ready; ++attempt) {
        if (auto r = limits.Get("/api/version"); r && r->status == 200) limit_ready = true;
        else std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
      check(limit_ready, "mining-admission server answers /api/version");
      const auto form = "application/x-www-form-urlencoded";
      const auto mine_on = [&](httplib::Client& client_, const std::string& bytes,
                               std::vector<std::pair<std::string, std::string>> fields) {
        httplib::MultipartFormDataItems items = {{"log", bytes, "log.csv", "text/csv"}};
        for (auto& [name, value] : fields) items.push_back({name, value, "", ""});
        return client_.Post("/api/mine", items);
      };
      const auto generated = [](int cases, int per_case) {
        std::string csv = "case_id,activity,timestamp\n";
        for (int c = 0; c < cases; ++c)
          for (int e = 0; e < per_case; ++e)
            csv += "c" + std::to_string(c) + ",A" + std::to_string(e) + ",2024-01-01T00:0" +
                   std::to_string(e) + ":00Z\n";
        return csv;
      };

      // 413 MINE_LIMIT, before anything is parsed (no stage pause applies).
      auto r = mine_on(limits, generated(100, 3), {});
      check(r && r->status == 413 && contains(r, "\"family\":\"MINE_LIMIT\"") &&
                contains(r, "\"limit\":\"mine_memory_budget\"") &&
                contains(r, "\"estimated_events\":300,") &&
                contains(r, "\"estimated_bytes\":3072000,") &&
                contains(r, "\"memory_budget_bytes\":2000000,") &&
                contains(r, "\"max_mine_events\":60,") &&
                contains(r, "--mine-memory-budget") && valid_json(r->body),
            "an over-budget log gets 413 MINE_LIMIT with the estimate: " + describe(r));
      r = mine_on(limits, generated(20, 4), {});
      check(r && r->status == 413 && contains(r, "\"limit\":\"max_mine_events\"") &&
                contains(r, "\"estimated_events\":80,") && contains(r, "--max-mine-events"),
            "a log over --max-mine-events gets 413 MINE_LIMIT: " + describe(r));
      // An XES log is estimated from its <event> tags (and, with zlib, a gzip
      // one as it inflates); three attribute keys mean no extra columns.
      {
        std::string xes = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">";
        for (int c = 0; c < 50; ++c) {
          xes += "<trace><string key=\"concept:name\" value=\"c" + std::to_string(c) + "\"/>";
          for (int e = 0; e < 4; ++e)
            xes += "<event><string key=\"concept:name\" value=\"A" + std::to_string(e) +
                   "\"/><date key=\"time:timestamp\" value=\"2024-01-01T00:0" + std::to_string(e) +
                   ":00.000+00:00\"/></event>";
          xes += "</trace>";
        }
        xes += "</log>\n";
        std::vector<std::pair<std::string, std::string>> uploads = {{"log.xes", xes}};
#if defined(MINERA_TEST_HAS_ZLIB)
        uploads.push_back({"log.xes.gz", gzip_bytes(xes)});
#endif
        for (const auto& [name, bytes] : uploads) {
          httplib::MultipartFormDataItems items = {{"log", bytes, name, "application/octet-stream"}};
          const auto x = limits.Post("/api/mine", items);
          check(x && x->status == 413 && contains(x, "\"limit\":\"mine_memory_budget\"") &&
                    contains(x, "\"estimated_events\":200,") && contains(x, "\"extra_columns\":0,") &&
                    contains(x, "\"estimated_bytes\":2048000,"),
                name + " over the budget gets 413 MINE_LIMIT: " + describe(x));
          // xes_max_traces stops the import after that many traces, so the
          // estimate counts only their events (10 traces: 40 events, admitted).
          items.push_back({"xes_max_traces", "10", "", ""});
          const auto capped = limits.Post("/api/mine", items);
          check(capped && capped->status == 200 && contains(capped, "\"event_count\": 40"),
                name + " with xes_max_traces=10 is estimated on its first 10 traces: " + describe(capped));
          items.back().content = "16";
          const auto over = limits.Post("/api/mine", items);
          check(over && over->status == 413 && contains(over, "\"limit\":\"max_mine_events\"") &&
                    contains(over, "\"estimated_events\":64,"),
                name + " with xes_max_traces=16 (64 events) passes --max-mine-events: " + describe(over));
        }
      }
      // The whole upload is imported, so sampling does not lift the cap.
      r = mine_on(limits, generated(100, 3), {{"sample_cases", "10"}, {"sample_seed", "1"}});
      check(r && r->status == 413 && contains(r, "\"estimated_events\":300,"),
            "a sampled mine is estimated on the whole upload: " + describe(r));
      // A log within both limits is admitted.
      r = mine_on(limits, generated(15, 4), {});
      check(r && r->status == 200 && contains(r, "\"event_count\": 60"),
            "a log at --max-mine-events is admitted: " + describe(r));
      // The estimate tokenizes quotes as the importer does: a stray quote in
      // an unquoted field (5" pipe) is literal and does not open a field
      // that swallows the remaining rows.
      {
        const auto stray = [&](int cases) {
          auto csv = generated(cases, 4);
          const auto first_row = csv.find('\n') + 1;
          csv.replace(csv.find(",A0,", first_row), 4, ",5\" pipe,");
          return csv;
        };
        r = mine_on(limits, stray(15), {});
        check(r && r->status == 200 && contains(r, "\"event_count\": 60") && contains(r, "5\\\" pipe"),
              "a stray quote is literal and the rows after it are imported: " + describe(r));
        r = mine_on(limits, stray(20), {});
        check(r && r->status == 413 && contains(r, "\"limit\":\"max_mine_events\"") &&
                  contains(r, "\"estimated_events\":80,"),
              "a stray quote does not hide rows from the estimate: " + describe(r));
      }
      // Row shapes follow read_csv: a trailing delimiter on every row makes
      // the first field an implicit index (not a column of the frame), so
      // the estimate keeps the header's columns and every row; a later row
      // longer than the first data row is a 400 with read_csv's message.
      {
        const auto trailing = [&](int cases) {
          std::string csv;
          std::istringstream lines(generated(cases, 4));
          std::string line;
          bool first = true;
          while (std::getline(lines, line)) {csv += line + (first ? "" : ",") + "\n"; first = false;}
          return csv;
        };
        r = mine_on(limits, trailing(15), {});
        check(r && r->status == 200 && contains(r, "\"event_count\": 60"),
              "a trailing delimiter on every row is an implicit index, not a refusal: " + describe(r));
        r = mine_on(limits, trailing(20), {});
        check(r && r->status == 413 && contains(r, "\"estimated_events\":80,") &&
                  contains(r, "\"extra_columns\":0,"),
              "the implicit index adds no column to the estimate: " + describe(r));
        auto longer = generated(2, 2);
        longer.insert(longer.size() - 1, ",extra");
        r = mine_on(limits, longer, {});
        check(r && r->status == 400 && contains(r, "Expected 3 fields in line 5, saw 4"),
              "a row longer than the first data row is refused like read_csv: " + describe(r));
      }

      // One mine at a time; a cancel stops the running one at its next stage.
      const std::string slow_id = "00000000-0000-4000-8000-0000000000c1";
      httplib::Result slow;
      const auto slow_start = std::chrono::steady_clock::now();
      auto slow_end = slow_start;
      std::atomic<bool> slow_done = false;
      std::thread runner([&] {
        httplib::Client own("127.0.0.1", limit_port);
        own.set_read_timeout(60, 0);
        slow = mine_on(own, log, {{"mining_request_id", slow_id}});
        slow_end = std::chrono::steady_clock::now();
        slow_done = true;
      });
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      r = mine_on(limits, log, {});
      check(r && r->status == 503 && r->get_header_value("Retry-After") == "5" &&
                contains(r, "\"family\":\"MINE_BUSY\"") &&
                contains(r, "Another log is being mined \\u2014 wait or cancel it.") &&
                contains(r, "\"canceling\":false"),
            "a second mine while one runs gets 503 with Retry-After: " + describe(r));
      r = limits.Post("/api/cancel-mine?request_id=" + slow_id, "", form);
      check(r && r->status == 200 && contains(r, "\"canceled\":true"), "cancel the running mine: " + describe(r));
      const auto canceled_at = std::chrono::steady_clock::now();
      r = mine_on(limits, log, {});
      if (!slow_done || (r && r->status == 503))
        check(r && r->status == 503 && r->get_header_value("Retry-After") == "1" &&
                  contains(r, "\"canceling\":true"),
              "while the canceled mine winds down, 503 says so: " + describe(r));
      runner.join();
      const auto after_cancel = std::chrono::duration_cast<std::chrono::milliseconds>(slow_end - canceled_at).count();
      const auto whole = std::chrono::duration_cast<std::chrono::milliseconds>(slow_end - slow_start).count();
      check(slow && slow->status == 400 && contains(slow, "mining request was canceled or replaced"),
            "the canceled mine answers the canceled response: " + describe(slow));
      // About 20 stage boundaries at 250 ms make an uncanceled run take 5 s.
      check(after_cancel < 1500 && whole < 4000,
            "the canceled mine stops at its next stage (" + std::to_string(after_cancel) + " ms after the cancel, " +
                std::to_string(whole) + " ms in all)");
      r = mine_on(limits, log, {});
      check(r && r->status == 200 && contains(r, "\"source_id\":"),
            "once the slot is free a mine runs to completion: " + describe(r));
      // The stream event ceiling is the budget at 2 KiB an event (976 here):
      // a reader whose source holds more events is refused with 429.
      r = limits.Get("/api/live-stream/list");
      check(r && r->status == 200 && contains(r, "\"max_events\":976}"),
            "the stream event ceiling follows --mine-memory-budget: " + describe(r));
      {
        httplib::MultipartFormDataItems source = {{"log", generated(250, 4), "big.csv", "text/csv"}};
        r = limits.Post("/api/stream-reader/create?session_id=budget1&kind=csv_events", source);
        check(r && r->status == 429 && contains(r, "event budget (976)"),
              "a reader past the stream event ceiling gets 429: " + describe(r));
        httplib::MultipartFormDataItems small = {{"log", generated(10, 4), "small.csv", "text/csv"}};
        r = limits.Post("/api/stream-reader/create?session_id=budget2&kind=csv_events", small);
        check(r && r->status == 200, "a reader within the stream event ceiling is created: " + describe(r));
      }
      ::kill(limit_server, SIGTERM);
      int limit_status = 0;
      ::waitpid(limit_server, &limit_status, 0);
    }
  }

  // Robustness review: malformed or overflowing controls answer 4xx with the
  // error envelope (never 500), and every 200 JSON body is valid JSON.
  if (ready) {
    httplib::MultipartFormDataItems items = {{"log", log, "log.csv", "text/csv"}};
    auto r = client.Post("/api/mine", items);
    check(r && r->status == 200, "robustness: mine the fixture: " + describe(r));
    // A malformed playout marking was an escaped exception (500).
    for (const std::string query : {"initial_marking=", "final_marking=x", "initial_marking=-1"}) {
      r = client.Post("/api/playout?" + query, "", "text/plain");
      check(r && r->status == 400 && contains(r, "\"error\"") && valid_json(r->body),
            "a malformed playout marking (" + query + ") gets 400: " + describe(r));
    }
    // Finite costs whose totals overflow a double printed inf into the JSON.
    r = client.Post("/api/align-decomposed?log_move_cost=1e308", "", "text/plain");
    check(r && r->status == 400 && contains(r, "alignment costs are too large") && valid_json(r->body),
          "decomposed alignment costs that overflow get 400: " + describe(r));
    r = client.Post("/api/align-decomposed?log_move_cost=2", "", "text/plain");
    check(r && r->status == 200 && valid_json(r->body),
          "decomposed alignment with a finite cost total still answers: " + describe(r));
    const auto pnml = client.Get("/api/pnml");
    check(pnml && pnml->status == 200, "robustness: GET /api/pnml: " + describe(pnml));
    if (pnml && pnml->status == 200) {
      httplib::MultipartFormDataItems align = {{"log", log, "log.csv", "text/csv"},
                                               {"model", pnml->body, "model.pnml", "application/xml"},
                                               {"trace_cost", "1e308", "", ""}};
      r = client.Post("/api/align-petri-workbench", align);
      check(r && r->status == 400 && contains(r, "alignment costs are too large") && valid_json(r->body),
            "Petri alignment workbench costs that overflow get 400: " + describe(r));
      align.back().content = "2";
      r = client.Post("/api/align-petri-workbench", align);
      check(r && r->status == 200 && valid_json(r->body),
            "Petri alignment workbench with a finite cost still answers: " + describe(r));
    }
    // Control characters in names were written raw into JSON strings.
    for (const std::string route : {"/api/embeddings", "/api/to-embeddings"}) {
      r = client.Post(route, "embedding_model=a%01b", "application/x-www-form-urlencoded");
      check(r && r->status == 200 && valid_json(r->body) && contains(r, "a\\u0001b"),
            route + " escapes a control character in the model name: " + describe(r));
    }
    items = {{"log", "case_id,activity,timestamp\nc1,A\x01" "B,2024-01-01T00:00:00Z\n", "ctl.csv", "text/csv"}};
    r = client.Post("/api/mine", items);
    check(r && r->status == 200, "robustness: mine a control-character activity: " + describe(r));
    r = client.Get("/api/trie-to-petri");
    check(r && r->status == 200 && valid_json(r->body) && contains(r, "A\\u0001B"),
          "trie-to-petri escapes a control character in an activity: " + describe(r));
    // Framing errors httplib answers itself also get the error envelope: a
    // request line over 8 KiB (414) and a malformed or negative
    // Content-Length (400; a negative one was 413 "larger than 128 MiB").
    const auto long_line = raw_exchange(port, "GET", "/api/traces?source_id=" + std::string(9000, '1'));
    check(long_line.status == 414 && valid_json(long_line.body) &&
              long_line.body.find("request line is longer") != std::string::npos,
          "an over-long /api request line gets 414 with the error envelope: " + describe(long_line));
    for (const std::string length : {"-5", "12abc"}) {
      const int fd = connect_loopback(port);
      check(fd >= 0, "connect for the malformed Content-Length");
      if (fd < 0) continue;
      send_all(fd, "POST /api/mine HTTP/1.1\r\n" + host_header(port) +
                       "Content-Type: text/csv\r\nContent-Length: " + length + "\r\n\r\n");
      const auto bad_length = read_raw_response(fd);
      check(bad_length.status == 400 && valid_json(bad_length.body) &&
                bad_length.body.find("Content-Length header must be") != std::string::npos,
            "Content-Length " + length + " gets 400 with the error envelope: " + describe(bad_length));
    }
    // Ambiguous framing (RFC 9112): an empty Content-Length, Content-Length
    // fields that disagree and Transfer-Encoding next to Content-Length get
    // 400 with the envelope and a closed connection, whatever the method.
    const auto framing = [&](const std::string& request_line, const std::string& headers,
                             const std::string& body) {
      const int fd = connect_loopback(port);
      if (fd < 0) return RawResponse{};
      send_all(fd, request_line + " HTTP/1.1\r\n" + host_header(port) + headers + "\r\n" + body);
      return read_raw_response(fd);
    };
    const struct {
      std::string line, headers, body, message;
    } ambiguous[] = {
        {"POST /api/mine", "Content-Type: text/csv\r\nContent-Length: \r\n", "",
         "Content-Length header must be"},
        {"POST /api/mine", "Content-Type: text/csv\r\nContent-Length: 2\r\nContent-Length: 30\r\n",
         "{}", "Content-Length headers that disagree"},
        {"POST /api/mine", "Content-Type: text/csv\r\nTransfer-Encoding: chunked\r\nContent-Length: 5\r\n",
         "0\r\n\r\n", "both Transfer-Encoding and Content-Length"},
        {"GET /api/version", "Transfer-Encoding: chunked\r\nContent-Length: 30\r\n", "0\r\n\r\n",
         "both Transfer-Encoding and Content-Length"},
        {"GET /api/version", "Content-Length: 1x\r\n", "", "Content-Length header must be"},
    };
    for (const auto& item : ambiguous) {
      const auto refused = framing(item.line, item.headers, item.body);
      check(refused.status == 400 && valid_json(refused.body) &&
                refused.body.find(item.message) != std::string::npos &&
                refused.header("Connection") == "close",
            item.line + " with ambiguous framing gets 400, the envelope and a close: " +
                describe(refused));
    }
    // Repeating one Content-Length value is not ambiguous: the request is
    // framed and routed (POST to a GET route answers 405).
    const auto repeated = framing("POST /api/version",
                                  "Connection: close\r\nContent-Length: 2\r\nContent-Length: 2\r\n", "{}");
    check(repeated.status == 405, "a repeated equal Content-Length is accepted: " + describe(repeated));
  }

  // Address-space limits (ulimit -v, prlimit --as). The default mining memory
  // budget honours one: under 2 GiB it is at most 1 GiB, so a log of 110,000
  // events (about 1.1 GB estimated) gets 413 MINE_LIMIT before it is parsed
  // instead of running the server out of memory. With the budget raised past
  // a 1.5 GiB limit, a mine of 400,000 events that runs out of memory gets
  // 503 naming it (it was 400 "std::bad_alloc") and the server lives on.
  // Linux only: macOS does not enforce RLIMIT_AS (on its CI runner the
  // server saw no lowered limit), so neither check can be observed there.
#if defined(__linux__)
  for (const bool raised : {false, true}) {
    const int as_port = free_port();
    const std::string as_port_text = std::to_string(as_port);
    std::vector<std::string> as_args = {argv[1], "--port", as_port_text, "--frontend",
                                        "/nonexistent-minera-frontend"};
    if (raised) as_args.insert(as_args.end(), {"--mine-memory-budget", "100G"});
    std::vector<char*> as_raw;
    for (auto& arg : as_args) as_raw.push_back(arg.data());
    as_raw.push_back(nullptr);
    const rlim_t address_space = raised ? rlim_t{1536} << 20 : rlim_t{2} << 30;
    const pid_t as_server = ::fork();
    if (as_server == 0) {
      const rlimit limit{address_space, address_space};
      ::setrlimit(RLIMIT_AS, &limit);
      ::execv(argv[1], as_raw.data());
      ::_exit(127);
    }
    check(as_server > 0, "start the address-space-limited server");
    if (as_server <= 0) continue;
    httplib::Client limited("127.0.0.1", as_port);
    limited.set_read_timeout(120, 0);
    bool as_ready = false;
    for (int attempt = 0; attempt < 200 && !as_ready; ++attempt) {
      if (auto r = limited.Get("/api/version"); r && r->status == 200) as_ready = true;
      else std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    check(as_ready, "address-space-limited server answers /api/version");
    // The stream event ceiling follows the same budget: about 2 KB an event,
    // so at most 524,288 events under 2 GiB (it was always 2,000,000).
    if (!raised) {
      const auto list = limited.Get("/api/live-stream/list");
      std::uint64_t max_events = 0;
      if (list) {
        const auto at = list->body.find("\"max_events\":");
        if (at != std::string::npos) max_events = std::strtoull(list->body.c_str() + at + 13, nullptr, 10);
      }
      check(list && list->status == 200 && max_events > 0 && max_events <= 524288,
            "under a 2 GiB address-space limit the stream event ceiling is at most 524288: " + describe(list));
    }
    std::string csv = "case_id,activity,timestamp\n";
    for (int e = 0; e < (raised ? 400000 : 110000); ++e)
      csv += "c" + std::to_string(e / 10) + ",A" + std::to_string(e % 10) + ",2024-01-01T00:00:00Z\n";
    httplib::MultipartFormDataItems big = {{"log", csv, "big.csv", "text/csv"}};
    const auto r = limited.Post("/api/mine", big);
    if (!raised) {
      std::uint64_t budget = 0;
      if (r) {
        const auto at = r->body.find("\"memory_budget_bytes\":");
        if (at != std::string::npos) budget = std::strtoull(r->body.c_str() + at + 22, nullptr, 10);
      }
      check(r && r->status == 413 && contains(r, "\"family\":\"MINE_LIMIT\"") && budget > 0 &&
                budget <= (std::uint64_t{1} << 30),
            "under a 2 GiB address-space limit the default budget is at most 1 GiB: " + describe(r));
    } else {
      check(r && r->status == 503 && contains(r, "ran out of memory") && valid_json(r->body),
            "a mine that runs out of memory gets 503 with the error envelope: " + describe(r));
      const auto alive = limited.Get("/api/version");
      check(alive && alive->status == 200, "the server answers after running out of memory");
    }
    ::kill(as_server, SIGTERM);
    int as_status = 0;
    ::waitpid(as_server, &as_status, 0);
  }
#endif

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures != 0) {
    std::cerr << g_failures << " server contract check(s) failed\n";
    return 1;
  }
  std::cout << "server parameter contract checks passed\n";
  return 0;
}
