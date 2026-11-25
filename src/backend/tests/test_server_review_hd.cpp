#undef NDEBUG
// Review HD: Declare's collapsed keys on a CSV upload's DataFrame shape and
// Heuristics++ keyed by a typed or gapped column on its log shape.
//
// - pm4py writes Declare's derived binary families act by act over a Python
//   set of the activities, so a key collapsed onto its act (a False, 0, 0.0
//   or '' partner, or a gap) keeps the partner the set lists last: a
//   True/False column with gaps (NaN after False and True), an int column
//   holding 0, and a date column, whose Timestamps hash as pandas does.
// - Heuristics++ on the log shape keyed by a column read_csv types as
//   numbers, booleans, dates or objects, or with gaps, reads
//   convert_to_event_log's objects as the DataFrame shape reads the frame's
//   cells (equal by Python equality, gaps NaN or NaT), raises the frame's
//   errors, and the token replay of the converted net replays the same
//   labels.
//
// Expected values are pm4py 2.7.22 black-box on the same inputs
// (review-harness round-hd: tc/mkhd.py, ct/dcx.py, ct/csvfam.py).
//
// Usage: minera_server_review_hd_tests <minera_server>
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
#include <functional>
#include <iostream>
#include <map>
#include <memory>
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

// A small JSON reader: enough for /api/mine's family results.
struct Json {
  enum class Kind { kNull, kBool, kNumber, kString, kArray, kObject } kind = Kind::kNull;
  std::string text;  // a string's value, a number's or boolean's literal
  std::vector<Json> items;
  std::vector<std::pair<std::string, Json>> members;
  const Json& operator[](const std::string& key) const {
    static const Json null;
    for (const auto& [name, value] : members)
      if (name == key) return value;
    return null;
  }
};

struct JsonReader {
  const std::string& s;
  std::size_t i = 0;
  void space() {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i;
  }
  std::string string() {
    std::string out;
    for (++i; i < s.size() && s[i] != '"'; ++i) {
      if (s[i] != '\\') {
        out += s[i];
        continue;
      }
      const char c = s[++i];
      if (c == 'n') out += '\n';
      else if (c == 't') out += '\t';
      else if (c == 'r') out += '\r';
      else if (c == 'b') out += '\b';
      else if (c == 'f') out += '\f';
      else if (c == 'u') {
        const auto code = std::stoul(s.substr(i + 1, 4), nullptr, 16);
        i += 4;
        if (code < 0x80) out += static_cast<char>(code);
        else if (code < 0x800) {
          out += static_cast<char>(0xc0 | (code >> 6));
          out += static_cast<char>(0x80 | (code & 0x3f));
        } else {
          out += static_cast<char>(0xe0 | (code >> 12));
          out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
          out += static_cast<char>(0x80 | (code & 0x3f));
        }
      } else out += c;
    }
    ++i;
    return out;
  }
  Json value() {
    space();
    Json out;
    if (i >= s.size()) return out;
    const char c = s[i];
    if (c == '{') {
      out.kind = Json::Kind::kObject;
      ++i;
      space();
      if (s[i] == '}') { ++i; return out; }
      while (true) {
        space();
        auto key = string();
        space();
        ++i;  // ':'
        auto item = value();
        out.members.emplace_back(std::move(key), std::move(item));
        space();
        if (s[i++] == '}') return out;
      }
    }
    if (c == '[') {
      out.kind = Json::Kind::kArray;
      ++i;
      space();
      if (s[i] == ']') { ++i; return out; }
      while (true) {
        out.items.push_back(value());
        space();
        if (s[i++] == ']') return out;
      }
    }
    if (c == '"') {
      out.kind = Json::Kind::kString;
      out.text = string();
      return out;
    }
    const auto end = s.find_first_of(",]} \n\r\t", i);
    out.text = s.substr(i, end - i);
    i = end;
    out.kind = out.text == "null" ? Json::Kind::kNull
               : out.text == "true" || out.text == "false" ? Json::Kind::kBool : Json::Kind::kNumber;
    return out;
  }
};

std::string join(std::vector<std::string> parts, const std::string& by) {
  std::sort(parts.begin(), parts.end());
  std::string out;
  for (std::size_t k = 0; k < parts.size(); ++k) out += (k ? by : "") + parts[k];
  return out;
}

std::string sorted_join(std::vector<std::string> parts, const std::string& by) { return join(std::move(parts), by); }

std::string fixed(const Json& value, int digits) {
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "%.*f", digits, std::strtod(value.text.c_str(), nullptr));
  return buffer;
}

using Fields = std::vector<std::pair<std::string, std::string>>;

std::string plain(const Json& value) {
  if (value.kind == Json::Kind::kNull) return "None";
  std::string out;
  for (const char c : value.text)
    if (c != '\x1f') out += c;
  return out;
}

std::string error_text(const Json& error) {
  if (error.kind == Json::Kind::kString) return error.text;
  const auto& type = error["type"].text;
  return (type.starts_with("builtins.") ? type.substr(9) + ": " : std::string()) + error["message"].text;
}

std::string declare_canon(const std::vector<std::pair<std::string, std::vector<std::string>>>& families) {
  std::vector<std::string> shown;
  for (const auto& [family, rules] : families)
    if (!rules.empty()) shown.push_back(family + "{" + sorted_join(rules, ",") + "}");
  return sorted_join(shown, " ");
}

// One case's outcome in mkhd.py's canonical form.
std::string outcome(httplib::Client& client, const std::string& name, const std::string& content,
                    const std::string& shape, const std::string& kind, const Fields& fields) {
  httplib::MultipartFormDataItems items = {{"log", content, name, "application/octet-stream"},
                                           {"include_traces", "false", "", ""}};
  for (const auto& [key, value] : fields) items.push_back({key, value, "", ""});
  if (shape != "log") items.push_back({"mining_input_shape", shape, "", ""});
  const auto response = client.Post("/api/mine", items);
  if (!response) return "no response";
  JsonReader reader{response->body};
  const auto body = reader.value();
  if (kind == "d") {
    if (body["error"].kind != Json::Kind::kNull) return "mine: " + error_text(body["error"]);
    std::vector<std::pair<std::string, std::vector<std::string>>> families;
    if (body["declare"].kind == Json::Kind::kObject && body["family_results"]["declare"].kind == Json::Kind::kNull) {
      // The CSV log shape's response: activities by index.
      const auto& names = body["activities"].items;
      const auto name_of = [&](const Json& at) {
        if (at.kind == Json::Kind::kNull) return std::string("None");
        const auto i = std::stoul(at.text);
        return i < names.size() ? plain(names[i]["name"]) : "#" + at.text;
      };
      for (const auto& [family, rules] : body["declare"].members) {
        std::vector<std::string> shown;
        for (const auto& x : rules.items) {
          const bool binary = std::any_of(x.members.begin(), x.members.end(),
                                          [](const auto& member) { return member.first == "source"; });
          shown.push_back((binary ? name_of(x["source"]) + ">" + name_of(x["target"]) : name_of(x["activity"])) + ":" +
                          x["support"].text + "/" + x["confidence"].text);
        }
        families.emplace_back(family, std::move(shown));
      }
      return declare_canon(families);
    }
    const auto& x = body["family_results"]["declare"];
    if (x["executed"].text != "true") return "mine: " + error_text(x["error"]);
    for (const auto& [family, rules] : x["result"]["model"]["rules"].members) {
      std::vector<std::string> shown;
      for (const auto& y : rules.items)
        shown.push_back((y["binary"].text == "true" ? plain(y["first"]) + ">" + plain(y["second"]) : plain(y["first"])) +
                        ":" + y["support"].text + "/" + y["confidence"].text);
      families.emplace_back(family, std::move(shown));
    }
    return declare_canon(families);
  }
  if (body["error"].kind != Json::Kind::kNull) {
    // The log shape's response names no error type; a leading one is dropped.
    auto message = error_text(body["error"]);
    for (const char* type : {"TypeError: ", "KeyError: ", "ValueError: ", "AttributeError: ", "IndexError: "})
      if (message.starts_with(type)) message = message.substr(std::string(type).size());
    return "error: " + message;
  }
  const auto& net = body["heuristics_net"];
  std::vector<std::string> activities, edges;
  for (const auto& a : net["activities"].items) activities.push_back(plain(a));
  for (const auto& e : net["edges"].items)
    edges.push_back(activities[std::stoul(e["source"].text)] + ">" + activities[std::stoul(e["target"].text)] + ":" +
                    (e["dependency"].kind == Json::Kind::kNumber ? fixed(e["dependency"], 6) : std::string("nan")) + "/" +
                    e["frequency"].text);
  const auto& replay = body["conformance"]["heuristic"]["token_replay"];
  std::string totals;
  for (const char* key : {"fitting_traces", "produced", "consumed", "missing", "remaining"})
    totals += (totals.empty() ? "" : "/") + replay[key].text;
  return "A[" + sorted_join(activities, ",") + "] E[" + sorted_join(edges, " ") + "] R " + totals;
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
    std::string label, name, content, shape, kind;
    Fields fields;
    std::string expected;
  };
  const std::string f0 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,d,2021-03-01 08:08:00,True,p,1.5,2021-03-01 08:00:00,2,True,b\nc0,b,2021-03-01 08:09:00,False,q,3.0,2021-03-02 09:30:00,,False,1\nc1,c,2021-03-01 08:10:00,True,r,3.0,2021-03-01 08:00:00,3,,a\nc1,a,,True,q,2.5,2021-03-02 09:30:00,1,False,\nc1,a,2021-03-01 08:17:00,True,r,3.0,2021-03-01 08:00:00,3,False,a\nc2,c,2021-03-01 08:18:00,True,r,2.5,,,True,a\nc2,c,2021-03-01 08:21:00,False,,1.5,,1,True,b\nc2,d,2021-03-01 08:23:00,True,p,,,2,True,1\n";
  const std::string f1 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-11 18:00:00,1,p,,2021-03-01 08:00:00,1,True,a\nc0,a,2021-03-12 14:00:00,18446744073709551625,,3.0,2021-03-01 08:00:00,2,True,1\nc0,a,2021-03-13 10:00:00,2,r,2.5,2021-03-01 08:00:00,3,False,a\nc0,c,2021-03-15 02:00:00,0,,1.5,2021-03-02 09:30:00,1,False,1\n";
  const std::string f2 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-11 08:00:00,0,,2.5,,3,True,\n";
  const std::string f3 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:32:00,2021-03-01 08:31:00,p,1.5,,,True,b\nc0,c,2021-03-01 08:35:00,2021-03-01 08:34:00,r,1.5,2021-03-01 08:00:00,1,,1\nc0,a,,2021-03-01 08:33:00,q,2.5,,3,False,a\nc0,b,,2021-03-01 08:34:00,p,1.5,2021-03-01 08:00:00,3,False,1\nc1,a,2021-03-01 08:15:00,2021-03-01 08:14:00,p,3.0,2021-03-01 08:00:00,2,False,\nc2,c,,2021-03-01 08:05:00,p,3.0,,3,False,b\nc2,a,2021-03-01 08:07:00,2021-03-01 08:07:00,r,3.0,,2,True,1\nc2,c,2021-03-01 08:07:00,2021-03-01 08:04:00,r,,2021-03-02 09:30:00,,False,1\nc2,a,2021-03-01 08:11:00,2021-03-01 08:08:00,r,1.5,2021-03-02 09:30:00,3,,1\nc3,a,,2021-03-01 08:28:00,q,1.5,,3,,\nc3,c,2021-03-01 08:30:00,2021-03-01 08:29:00,r,1.5,2021-03-01 08:00:00,1,True,b\nc3,d,2021-03-01 08:31:00,2021-03-01 08:28:00,r,,,2,False,b\nc4,b,,2021-03-01 08:20:00,,3.0,,1,False,b\nc4,a,2021-03-01 08:23:00,2021-03-01 08:22:00,q,2.5,,1,False,\nc4,d,2021-03-01 08:26:00,2021-03-01 08:23:00,,1.5,2021-03-01 08:00:00,3,True,1\nc4,a,2021-03-01 08:30:00,2021-03-01 08:30:00,q,,2021-03-01 08:00:00,1,,1\n";
  const std::string f4 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,d,2021-03-01 08:02:00,2021-03-01 08:00:00,p,2.5,,,,\nc0,d,2021-03-01 08:04:00,2021-03-01 08:04:00,q,1.5,2021-03-01 08:00:00,1,True,a\nc0,b,2021-03-01 08:05:00,2021-03-01 08:05:00,q,2.5,,1,False,a\nc0,a,2021-03-01 08:07:00,2021-03-01 08:07:00,p,,2021-03-01 08:00:00,1,,\nc1,d,2021-03-01 08:30:00,,,2.5,2021-03-01 08:00:00,1,False,a\nc1,d,2021-03-01 08:30:00,,q,,2021-03-01 08:00:00,1,False,a\nc2,b,2021-03-01 08:05:00,2021-03-01 08:05:00,,2.5,2021-03-01 08:00:00,1,False,1\nc2,c,2021-03-01 08:07:00,2021-03-01 08:07:00,,1.5,2021-03-01 08:00:00,2,,1\n";
  const std::string f5 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 08:33:00,4,r,3.0,,3,True,\nc0,d,2021-03-01 08:34:00,18446744073709551620,q,2.5,2021-03-01 08:00:00,3,True,b\nc0,a,2021-03-01 08:36:00,7,r,,,2,False,b\nc0,b,,8,q,,2021-03-01 08:00:00,2,,a\nc1,d,2021-03-01 08:00:00,2,q,2.5,,,True,1\nc1,a,2021-03-01 08:03:00,18446744073709551617,p,1.5,2021-03-01 08:00:00,1,True,\nc1,d,2021-03-01 08:03:00,3,q,2.5,,,False,1\nc1,b,2021-03-01 08:06:00,2,p,1.5,2021-03-02 09:30:00,2,True,a\n";
  const std::string f6 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-11 08:00:00,0,p,3.0,2021-03-02 09:30:00,2,,a\nc0,b,2021-03-12 14:00:00,False,,1.5,2021-03-02 09:30:00,,True,b\nc1,d,,False,r,1.5,2021-03-02 09:30:00,1,,\nc1,a,2021-03-13 10:00:00,0,q,3.0,2021-03-02 09:30:00,1,False,b\nc1,b,2021-03-13 10:00:00,False,r,2.5,2021-03-02 09:30:00,3,,a\nc1,d,2021-03-13 10:00:00,False,q,3.0,2021-03-02 09:30:00,1,True,a\nc2,b,2021-03-13 20:00:00,False,,,2021-03-01 08:00:00,,False,b\nc2,b,2021-03-14 16:00:00,0,q,,,2,True,\nc2,c,2021-03-15 12:00:00,2,q,,,1,True,\nc2,c,2021-03-15 22:00:00,1,,,2021-03-02 09:30:00,2,False,1\nc2,a,2021-03-15 22:00:00,True,r,1.5,2021-03-02 09:30:00,2,True,\nc3,a,2021-03-05 12:00:00,0,r,1.5,,1,,1\nc3,b,2021-03-05 12:00:00,2,r,3.0,,2,False,b\nc3,c,2021-03-06 08:00:00,0,r,2.5,2021-03-01 08:00:00,,,1\nc3,d,2021-03-08 00:00:00,1,r,,2021-03-02 09:30:00,,False,b\nc4,a,2021-03-10 02:00:00,True,r,,2021-03-01 08:00:00,2,True,\nc4,a,2021-03-10 02:00:00,True,r,,2021-03-01 08:00:00,1,False,\nc4,d,2021-03-11 18:00:00,False,,1.5,2021-03-01 08:00:00,1,False,\nc5,c,2021-03-06 18:00:00,0,p,1.5,2021-03-01 08:00:00,1,,\nc5,d,,0,p,1.5,2021-03-01 08:00:00,3,,1\n";
  const std::string f7 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,d,2021-03-01 11:00:00,4,p,-1.0,,7,,\nc1,c,2021-03-01 15:00:00,,r,1.5,2021-03-02 09:30:00,3,,b\nc1,b,,5,p,,2021-03-01 08:00:00,2,,b\nc1,b,2021-03-01 18:00:00,3,p,1.5,2021-03-01 08:00:00,1,True,a\nc2,d,,4,p,,,7,False,2\nc2,b,,,q,,2021-03-03 10:00:00,3,True,1\nc2,b,2021-03-01 23:00:00,2,,1.5,2021-03-01 08:00:00,7,False,\nc2,a,2021-03-02 03:00:00,9,p,1.5,2021-03-02 09:30:00,,,b\nc2,a,2021-03-02 04:00:00,7,p,1.5,2021-03-02 09:30:00,7,,a\nc3,b,2021-03-01 16:00:00,6,q,2.5,2021-03-01 08:00:00,7,True,b\nc3,c,2021-03-01 20:00:00,,p,-1.0,2021-03-01 08:00:00,,True,\nc3,d,,,,,2021-03-03 10:00:00,2,False,\nc3,c,2021-03-02 04:00:00,5,r,-1.0,2021-03-03 10:00:00,,False,b\nc3,b,2021-03-02 05:00:00,3,p,,,2,True,b\nc4,d,2021-03-01 14:00:00,,s,2.5,2021-03-02 09:30:00,,,a\nc4,b,2021-03-01 15:00:00,6,,2.5,,3,False,b\nc4,a,2021-03-01 16:00:00,,r,2.5,2021-03-03 10:00:00,1,,a\n";
  const std::string f8 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 08:21:00,False,q,2.5,2021-03-01 08:00:00,2,,\nc0,c,2021-03-01 08:25:00,True,p,2.5,,,,1\nc0,d,2021-03-01 08:27:00,False,q,1.5,2021-03-01 08:00:00,1,False,1\nc0,a,2021-03-01 08:28:00,False,q,1.5,2021-03-01 08:00:00,2,False,a\nc0,a,2021-03-01 08:28:00,True,p,,,2,False,1\nc1,d,2021-03-01 08:27:00,True,,1.5,,1,,\nc1,b,2021-03-01 08:28:00,False,q,,2021-03-01 08:00:00,2,True,\nc1,d,,False,p,2.5,,2,False,1\nc1,a,2021-03-01 08:36:00,False,q,2.5,2021-03-01 08:00:00,2,False,\nc2,b,,True,q,,2021-03-01 08:00:00,2,False,1\nc2,c,2021-03-01 08:09:00,True,q,,2021-03-01 08:00:00,,,a\nc2,d,2021-03-01 08:09:00,False,,1.5,,,False,a\nc2,c,2021-03-01 08:11:00,True,q,1.5,2021-03-01 08:00:00,2,,\nc2,d,2021-03-01 08:12:00,True,p,1.5,2021-03-01 08:00:00,1,True,a\nc3,b,2021-03-01 08:18:00,True,p,2.5,2021-03-01 08:00:00,1,True,\nc4,a,2021-03-01 08:25:00,True,p,1.5,2021-03-01 08:00:00,2,True,a\nc4,b,2021-03-01 08:29:00,True,q,1.5,2021-03-01 08:00:00,1,True,a\n";
  const std::string f9 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 08:18:00,2021-03-01 08:18:00,p,1.5,,1,,\nc1,d,,2021-03-01 08:15:00,p,2.5,2021-03-01 08:00:00,1,False,a\nc1,a,2021-03-01 08:16:00,,q,,,1,True,1\nc1,c,2021-03-01 08:19:00,2021-03-01 08:19:00,q,1.5,,2,True,1\nc1,b,2021-03-01 08:19:00,2021-03-01 08:19:00,,,2021-03-01 08:00:00,,True,1\nc2,c,2021-03-01 08:22:00,2021-03-01 08:19:00,,1.5,,2,,\nc2,a,,2021-03-01 08:26:00,p,,2021-03-01 08:00:00,2,False,a\nc3,d,2021-03-01 08:26:00,2021-03-01 08:24:00,q,2.5,2021-03-01 08:00:00,2,True,a\nc3,a,2021-03-01 08:27:00,,p,2.5,2021-03-01 08:00:00,1,,1\nc4,b,2021-03-01 08:05:00,2021-03-01 08:04:00,q,2.5,,2,False,1\nc4,b,2021-03-01 08:05:00,2021-03-01 08:05:00,q,1.5,,1,False,1\nc4,c,,,,1.5,,1,False,a\nc5,d,,,p,2.5,2021-03-01 08:00:00,,True,1\nc5,b,2021-03-01 08:29:00,2021-03-01 08:27:00,,2.5,,2,,1\nc5,a,,2021-03-01 08:30:00,p,1.5,2021-03-01 08:00:00,1,True,1\n";
  const std::string f10 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-04 16:00:00,False,r,1.5,2021-03-02 09:30:00,2,,b\nc0,b,2021-03-05 12:00:00,True,p,2.5,,,True,b\nc1,d,,True,,2.5,2021-03-02 09:30:00,2,False,a\nc2,c,2021-03-11 18:00:00,True,r,1.5,2021-03-01 08:00:00,1,,1\nc2,d,2021-03-13 10:00:00,False,,1.5,,1,,a\nc2,b,2021-03-13 20:00:00,True,p,,2021-03-02 09:30:00,2,False,b\nc2,c,2021-03-15 02:00:00,False,q,1.5,2021-03-01 08:00:00,3,False,\nc3,c,2021-03-12 14:00:00,True,p,3.0,2021-03-01 08:00:00,3,,a\nc3,b,2021-03-14 06:00:00,False,,1.5,2021-03-02 09:30:00,,True,b\nc4,d,2021-03-05 02:00:00,True,r,,2021-03-02 09:30:00,3,False,1\nc4,b,2021-03-05 22:00:00,False,r,,2021-03-01 08:00:00,3,True,1\nc4,d,,False,p,2.5,2021-03-01 08:00:00,1,,a\nc5,c,2021-03-04 06:00:00,False,r,,,,False,b\nc5,b,2021-03-05 22:00:00,False,q,2.5,2021-03-01 08:00:00,3,True,1\nc5,d,2021-03-06 08:00:00,True,q,1.5,2021-03-01 08:00:00,2,,b\n";
  const std::string f11 = "case,act,ts,K5\nc0,a,2021-03-01 08:00:00,True\nc1,a,2021-03-01 08:01:00,\nc2,a,2021-03-01 08:02:00,True\nc2,b,2021-03-01 08:03:00,False\nc3,a,2021-03-01 08:04:00,True\nc3,b,2021-03-01 08:05:00,\n";
  const std::string f12 = "case,act,ts,K5\nc0,a,2021-03-01 08:00:00,True\nc1,a,2021-03-01 08:01:00,False\nc2,a,2021-03-01 08:02:00,True\nc2,b,2021-03-01 08:03:00,\nc3,a,2021-03-01 08:04:00,True\nc3,b,2021-03-01 08:05:00,False\n";
  const std::string f13 = "case,act,ts,K5\nc0,a,2021-03-01 08:00:00,1.5\nc1,a,2021-03-01 08:01:00,\nc2,a,2021-03-01 08:02:00,1.5\nc2,b,2021-03-01 08:03:00,2.5\nc3,a,2021-03-01 08:04:00,1.5\nc3,b,2021-03-01 08:05:00,\n";
  const std::string f14 = "case,act,ts,K5\nc0,a,2021-03-01 08:00:00,1.5\nc1,a,2021-03-01 08:01:00,x\nc2,a,2021-03-01 08:02:00,1.5\nc2,b,2021-03-01 08:03:00,\nc3,a,2021-03-01 08:04:00,1.5\nc3,b,2021-03-01 08:05:00,x\n";
  const std::string f15 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,d,2021-03-01 08:08:00,p,1.5,2021-03-01 08:00:00,2,True,b\nc0,b,2021-03-01 08:09:00,q,3.0,2021-03-02 09:30:00,,False,1\nc1,c,2021-03-01 08:10:00,r,3.0,2021-03-01 08:00:00,3,,a\nc1,a,,q,2.5,2021-03-02 09:30:00,1,False,\nc1,a,2021-03-01 08:17:00,r,3.0,2021-03-01 08:00:00,3,False,a\nc2,c,2021-03-01 08:18:00,r,2.5,,,True,a\nc2,c,2021-03-01 08:21:00,,1.5,,1,True,b\nc2,d,2021-03-01 08:23:00,p,,,2,True,1\n";
  const std::string f16 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-11 18:00:00,p,,2021-03-01 08:00:00,1,True,a\nc0,a,2021-03-12 14:00:00,,3.0,2021-03-01 08:00:00,2,True,1\nc0,a,2021-03-13 10:00:00,r,2.5,2021-03-01 08:00:00,3,False,a\nc0,c,2021-03-15 02:00:00,,1.5,2021-03-02 09:30:00,1,False,1\n";
  const std::string f17 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-11 08:00:00,,2.5,,3,True,\n";
  const std::string f18 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 08:05:00,p,1.5,2021-03-02 09:30:00,,True,a\nc0,a,2021-03-01 08:09:00,,3.0,2021-03-02 09:30:00,1,True,b\nc0,d,2021-03-01 08:11:00,p,3.0,,,True,a\n";
  const std::string f19 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,,r,1.5,2021-03-01 08:00:00,7,False,\nc0,a,2021-03-02 04:00:00,s,,2021-03-03 10:00:00,,False,a\nc0,d,2021-03-02 06:00:00,,-1.0,2021-03-02 09:30:00,,True,\nc0,d,2021-03-02 07:00:00,p,-1.0,2021-03-03 10:00:00,1,False,\nc0,a,2021-03-02 10:00:00,s,,2021-03-02 09:30:00,7,False,b\nc1,b,2021-03-02 09:00:00,,2.5,2021-03-02 09:30:00,3,True,a\nc1,b,2021-03-02 13:00:00,,,,,False,1\nc2,d,2021-03-01 13:00:00,,2.5,2021-03-03 10:00:00,7,False,b\nc3,a,2021-03-01 09:00:00,,,,,,\nc3,a,2021-03-01 11:00:00,q,,,,,\nc3,d,,,1.5,2021-03-01 08:00:00,,False,b\nc3,b,2021-03-01 15:00:00,q,1.5,2021-03-03 10:00:00,,False,\nc3,a,2021-03-01 19:00:00,q,1.5,2021-03-02 09:30:00,7,False,\n";
  const std::string f20 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:32:00,p,1.5,,,True,b\nc0,c,2021-03-01 08:35:00,r,1.5,2021-03-01 08:00:00,1,,1\nc0,a,,q,2.5,,3,False,a\nc0,b,,p,1.5,2021-03-01 08:00:00,3,False,1\nc1,a,2021-03-01 08:15:00,p,3.0,2021-03-01 08:00:00,2,False,\nc2,c,,p,3.0,,3,False,b\nc2,a,2021-03-01 08:07:00,r,3.0,,2,True,1\nc2,c,2021-03-01 08:07:00,r,,2021-03-02 09:30:00,,False,1\nc2,a,2021-03-01 08:11:00,r,1.5,2021-03-02 09:30:00,3,,1\nc3,a,,q,1.5,,3,,\nc3,c,2021-03-01 08:30:00,r,1.5,2021-03-01 08:00:00,1,True,b\nc3,d,2021-03-01 08:31:00,r,,,2,False,b\nc4,b,,,3.0,,1,False,b\nc4,a,2021-03-01 08:23:00,q,2.5,,1,False,\nc4,d,2021-03-01 08:26:00,,1.5,2021-03-01 08:00:00,3,True,1\nc4,a,2021-03-01 08:30:00,q,,2021-03-01 08:00:00,1,,1\n";
  const std::string f21 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:21:00,p,2.5,,1,True,a\nc0,b,2021-03-01 08:23:00,p,,2021-03-01 08:00:00,1,False,a\nc0,c,,q,2.5,2021-03-01 08:00:00,2,True,\nc0,c,,p,,2021-03-01 08:00:00,1,True,\nc0,c,2021-03-01 08:27:00,,2.5,2021-03-01 08:00:00,2,False,1\nc1,d,2021-03-01 08:14:00,,,,2,True,\nc2,b,,p,,2021-03-01 08:00:00,2,False,1\n";
  const std::string f22 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,d,2021-03-01 08:02:00,p,2.5,,,,\nc0,d,2021-03-01 08:04:00,q,1.5,2021-03-01 08:00:00,1,True,a\nc0,b,2021-03-01 08:05:00,q,2.5,,1,False,a\nc0,a,2021-03-01 08:07:00,p,,2021-03-01 08:00:00,1,,\nc1,d,2021-03-01 08:30:00,,2.5,2021-03-01 08:00:00,1,False,a\nc1,d,2021-03-01 08:30:00,q,,2021-03-01 08:00:00,1,False,a\nc2,b,2021-03-01 08:05:00,,2.5,2021-03-01 08:00:00,1,False,1\nc2,c,2021-03-01 08:07:00,,1.5,2021-03-01 08:00:00,2,,1\n";
  const std::string f23 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 08:33:00,r,3.0,,3,True,\nc0,d,2021-03-01 08:34:00,q,2.5,2021-03-01 08:00:00,3,True,b\nc0,a,2021-03-01 08:36:00,r,,,2,False,b\nc0,b,,q,,2021-03-01 08:00:00,2,,a\nc1,d,2021-03-01 08:00:00,q,2.5,,,True,1\nc1,a,2021-03-01 08:03:00,p,1.5,2021-03-01 08:00:00,1,True,\nc1,d,2021-03-01 08:03:00,q,2.5,,,False,1\nc1,b,2021-03-01 08:06:00,p,1.5,2021-03-02 09:30:00,2,True,a\n";
  const std::string f24 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,d,,r,,2021-03-02 09:30:00,3,,1\nc0,a,2021-03-01 08:28:00,q,2.5,,2,False,a\nc0,d,2021-03-01 08:31:00,q,3.0,2021-03-01 08:00:00,3,True,a\nc1,d,2021-03-01 08:18:00,q,,2021-03-01 08:00:00,1,False,\nc2,a,,q,,2021-03-02 09:30:00,2,,1\n";
  const std::string f25 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-02 06:00:00,p,1.5,2021-03-01 08:00:00,3,False,a\nc0,a,2021-03-02 07:00:00,r,3.0,2021-03-02 09:30:00,,,1\nc0,c,2021-03-02 08:00:00,r,2.5,2021-03-01 08:00:00,1,False,b\nc1,c,2021-03-01 09:00:00,p,2.5,2021-03-02 09:30:00,1,True,1\nc2,c,2021-03-01 22:00:00,q,2.5,2021-03-01 08:00:00,,,1\n";
  const std::string f26 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 08:05:00,,3.0,2021-03-01 08:00:00,2,True,1\nc1,a,2021-03-01 08:27:00,s,,2021-03-03 10:00:00,2,False,1\n";
  const std::string f27 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-02 07:00:00,,2.5,2021-03-01 08:00:00,2,False,a\nc0,c,2021-03-02 07:00:00,p,,,2,False,a\nc0,d,2021-03-02 07:00:00,q,1.5,2021-03-01 08:00:00,2,True,\nc0,b,,q,1.5,2021-03-01 08:00:00,,False,\nc0,a,2021-03-02 12:00:00,q,2.5,2021-03-01 08:00:00,1,False,1\nc1,d,2021-03-02 10:00:00,p,2.5,,1,False,1\nc2,b,2021-03-01 21:00:00,p,1.5,,1,,1\nc2,a,,,1.5,,1,False,1\nc3,c,2021-03-01 20:00:00,p,1.5,2021-03-01 08:00:00,1,,1\nc3,d,2021-03-01 20:00:00,p,1.5,,2,True,1\nc3,c,2021-03-01 23:00:00,q,2.5,,,,a\nc3,b,2021-03-01 23:00:00,,1.5,,2,False,a\n";
  const std::string f28 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 08:03:00,p,,2021-03-02 09:30:00,,True,1\nc0,c,2021-03-01 08:05:00,,3.0,2021-03-02 09:30:00,1,True,a\nc0,a,2021-03-01 08:08:00,r,,,,False,a\nc1,b,2021-03-01 08:21:00,,1.5,2021-03-02 09:30:00,3,False,1\n";
  const std::string f29 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,b,,q,3.0,2021-03-02 09:30:00,2,False,a\nc0,b,,r,-1.0,,,True,b\nc1,d,2021-03-01 08:11:00,p,3.0,2021-03-02 09:30:00,7,,b\nc2,d,2021-03-01 08:12:00,q,3.0,2021-03-01 08:00:00,2,,2\nc2,d,2021-03-01 08:12:00,p,2.5,,,False,2\nc3,d,2021-03-01 08:05:00,q,-1.0,2021-03-02 09:30:00,,,1\n";
  const std::string f30 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,d,2021-03-01 11:00:00,p,-1.0,,7,,\nc1,c,2021-03-01 15:00:00,r,1.5,2021-03-02 09:30:00,3,,b\nc1,b,,p,,2021-03-01 08:00:00,2,,b\nc1,b,2021-03-01 18:00:00,p,1.5,2021-03-01 08:00:00,1,True,a\nc2,d,,p,,,7,False,2\nc2,b,,q,,2021-03-03 10:00:00,3,True,1\nc2,b,2021-03-01 23:00:00,,1.5,2021-03-01 08:00:00,7,False,\nc2,a,2021-03-02 03:00:00,p,1.5,2021-03-02 09:30:00,,,b\nc2,a,2021-03-02 04:00:00,p,1.5,2021-03-02 09:30:00,7,,a\nc3,b,2021-03-01 16:00:00,q,2.5,2021-03-01 08:00:00,7,True,b\nc3,c,2021-03-01 20:00:00,p,-1.0,2021-03-01 08:00:00,,True,\nc3,d,,,,2021-03-03 10:00:00,2,False,\nc3,c,2021-03-02 04:00:00,r,-1.0,2021-03-03 10:00:00,,False,b\nc3,b,2021-03-02 05:00:00,p,,,2,True,b\nc4,d,2021-03-01 14:00:00,s,2.5,2021-03-02 09:30:00,,,a\nc4,b,2021-03-01 15:00:00,,2.5,,3,False,b\nc4,a,2021-03-01 16:00:00,r,2.5,2021-03-03 10:00:00,1,,a\n";
  const std::string f31 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 16:00:00,,2.5,,3,True,\n";
  const std::string f32 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:14:00,p,1.5,2021-03-01 08:00:00,2,False,a\nc0,a,2021-03-01 08:15:00,p,1.5,2021-03-01 08:00:00,2,True,a\nc0,b,2021-03-01 08:15:00,,,,1,False,\nc0,a,2021-03-01 08:18:00,q,2.5,,2,True,1\nc1,c,,p,2.5,2021-03-01 08:00:00,2,True,1\nc1,d,,p,2.5,2021-03-01 08:00:00,,False,a\nc1,a,2021-03-01 08:24:00,q,,2021-03-01 08:00:00,1,,a\nc2,b,,p,1.5,2021-03-01 08:00:00,,False,1\n";
  const std::string f33 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 08:17:00,p,,2021-03-01 08:00:00,1,True,1\nc0,c,2021-03-01 08:21:00,,1.5,2021-03-01 08:00:00,2,True,\n";
  const std::string f34 = "case,act,ts,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:36:00,q,2.5,,1,True,1\nc1,c,2021-03-01 08:50:00,,3.0,2021-03-01 08:00:00,3,False,\nc2,a,2021-03-01 08:48:00,r,2.5,2021-03-02 09:30:00,3,False,1\n";
  const std::string f35 = "case,act,ts,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 09:45:00,q,3.0,2021-03-01 08:00:00,,True,2\nc1,b,2021-03-01 09:31:00,q,-1.0,,2,,\nc2,b,2021-03-01 08:12:00,q,,2021-03-02 09:30:00,,,2\nc2,b,2021-03-01 09:12:00,r,3.0,,7,True,\n";
  const std::string f36 = "case,act,ts,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 09:26:00,q,-1.0,2021-03-03 10:00:00,3,,1\nc1,c,2021-03-01 09:08:00,p,,2021-03-01 08:00:00,7,,\nc1,c,2021-03-01 10:08:00,r,3.0,2021-03-03 10:00:00,3,True,1\nc1,a,2021-03-01 10:09:00,s,-1.0,,1,False,1\n";
  const std::vector<Case> cases = {
      {"d dataframe K5 s68500.csv", "s68500.csv", f0, "dataframe", "d", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K5"}}, "nonchainsuccession{False>True:3/3,True:3/3,nan>True:3/3} noncoexistence{True:3/3,nan>True:3/3} nonsuccession{False>True:3/3,True:3/3,nan>True:3/3}"},
      {"d dataframe K3 s68500.csv", "s68500.csv", f0, "dataframe", "d", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K3"}}, "nonchainsuccession{2021-03-01 08:00:00+00:00:3/3,NaT>2021-03-01 08:00:00+00:00:3/3} noncoexistence{2021-03-01 08:00:00+00:00:3/3,NaT>2021-03-01 08:00:00+00:00:3/3} nonsuccession{2021-03-01 08:00:00+00:00:3/3,NaT>2021-03-01 08:00:00+00:00:3/3}"},
      {"d dataframe K4 s68500.csv", "s68500.csv", f0, "dataframe", "d", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K4"}}, "absence{1.0:3/3,2.0:3/3,3.0:3/3,nan:3/3,nan:3/3} altprecedence{1.0:3/3,1.0>2.0:3/3,1.0>3.0:3/3,2.0:3/3,2.0>1.0:3/3,2.0>3.0:3/3,3.0:3/3,3.0>1.0:3/3,3.0>2.0:3/3,nan:3/3,nan:3/3,nan:3/3,nan:3/3,nan>1.0:3/3,nan>1.0:3/3,nan>2.0:3/3,nan>2.0:3/3,nan>3.0:3/3,nan>3.0:3/3} altresponse{1.0:3/3,1.0>2.0:3/3,1.0>3.0:3/3,2.0:3/3,2.0>1.0:3/3,2.0>3.0:3/3,3.0:3/3,3.0>1.0:3/3,3.0>2.0:3/3,nan:3/3,nan:3/3,nan:3/3,nan:3/3,nan>1.0:3/3,nan>1.0:3/3,nan>2.0:3/3,nan>2.0:3/3,nan>3.0:3/3,nan>3.0:3/3} altsuccession{1.0:3/3,1.0>2.0:3/3,1.0>3.0:3/3,2.0:3/3,2.0>1.0:3/3,2.0>3.0:3/3,3.0:3/3,3.0>1.0:3/3,3.0>2.0:3/3,nan:3/3,nan:3/3,nan:3/3,nan:3/3,nan>1.0:3/3,nan>1.0:3/3,nan>2.0:3/3,nan>2.0:3/3,nan>3.0:3/3,nan>3.0:3/3} chainprecedence{1.0:3/3,1.0>2.0:3/3,1.0>3.0:3/3,2.0:3/3,2.0>1.0:3/3,2.0>3.0:3/3,3.0:3/3,3.0>1.0:3/3,3.0>2.0:3/3,nan:3/3,nan:3/3,nan:3/3,nan:3/3,nan>1.0:3/3,nan>1.0:3/3,nan>2.0:3/3,nan>2.0:3/3,nan>3.0:3/3,nan>3.0:3/3} chainresponse{1.0:3/3,1.0>2.0:3/3,1.0>3.0:3/3,2.0:3/3,2.0>1.0:3/3,2.0>3.0:3/3,3.0:3/3,3.0>1.0:3/3,3.0>2.0:3/3,nan:3/3,nan:3/3,nan:3/3,nan:3/3,nan>1.0:3/3,nan>1.0:3/3,nan>2.0:3/3,nan>2.0:3/3,nan>3.0:3/3,nan>3.0:3/3} chainsuccession{1.0:3/3,1.0>2.0:3/3,1.0>3.0:3/3,2.0:3/3,2.0>1.0:3/3,2.0>3.0:3/3,3.0:3/3,3.0>1.0:3/3,3.0>2.0:3/3,nan:3/3,nan:3/3,nan:3/3,nan:3/3,nan>1.0:3/3,nan>1.0:3/3,nan>2.0:3/3,nan>2.0:3/3,nan>3.0:3/3,nan>3.0:3/3} coexistence{1.0:3/3,1.0>2.0:3/3,1.0>3.0:3/3,2.0:3/3,2.0>1.0:3/3,2.0>3.0:3/3,3.0:3/3,3.0>1.0:3/3,3.0>2.0:3/3,nan:3/3,nan:3/3,nan:3/3,nan:3/3,nan>1.0:3/3,nan>1.0:3/3,nan>2.0:3/3,nan>2.0:3/3,nan>3.0:3/3,nan>3.0:3/3} exactly_one{1.0:3/3,2.0:3/3,3.0:3/3,nan:3/3,nan:3/3} existence{1.0:3/3,2.0:3/3,3.0:3/3,nan:3/3,nan:3/3} init{1.0:3/3,2.0:3/3,3.0:3/3,nan:3/3,nan:3/3} nonchainsuccession{1.0:3/3,1.0>2.0:3/3,1.0>3.0:3/3,2.0:3/3,2.0>1.0:3/3,2.0>3.0:3/3,3.0:3/3,3.0>1.0:3/3,3.0>2.0:3/3,nan:3/3,nan:3/3,nan:3/3,nan:3/3,nan>1.0:3/3,nan>1.0:3/3,nan>2.0:3/3,nan>2.0:3/3,nan>3.0:3/3,nan>3.0:3/3} noncoexistence{1.0:3/3,1.0>2.0:3/3,1.0>3.0:3/3,2.0:3/3,2.0>1.0:3/3,2.0>3.0:3/3,3.0:3/3,3.0>1.0:3/3,3.0>2.0:3/3,nan:3/3,nan:3/3,nan:3/3,nan:3/3,nan>1.0:3/3,nan>1.0:3/3,nan>2.0:3/3,nan>2.0:3/3,nan>3.0:3/3,nan>3.0:3/3} nonsuccession{1.0:3/3,1.0>2.0:3/3,1.0>3.0:3/3,2.0:3/3,2.0>1.0:3/3,2.0>3.0:3/3,3.0:3/3,3.0>1.0:3/3,3.0>2.0:3/3,nan:3/3,nan:3/3,nan:3/3,nan:3/3,nan>1.0:3/3,nan>1.0:3/3,nan>2.0:3/3,nan>2.0:3/3,nan>3.0:3/3,nan>3.0:3/3} precedence{1.0:3/3,1.0>2.0:3/3,1.0>3.0:3/3,2.0:3/3,2.0>1.0:3/3,2.0>3.0:3/3,3.0:3/3,3.0>1.0:3/3,3.0>2.0:3/3,nan:3/3,nan:3/3,nan:3/3,nan:3/3,nan>1.0:3/3,nan>1.0:3/3,nan>2.0:3/3,nan>2.0:3/3,nan>3.0:3/3,nan>3.0:3/3} responded_existence{1.0:3/3,1.0>2.0:3/3,1.0>3.0:3/3,2.0:3/3,2.0>1.0:3/3,2.0>3.0:3/3,3.0:3/3,3.0>1.0:3/3,3.0>2.0:3/3,nan:3/3,nan:3/3,nan:3/3,nan:3/3,nan>1.0:3/3,nan>1.0:3/3,nan>2.0:3/3,nan>2.0:3/3,nan>3.0:3/3,nan>3.0:3/3} response{1.0:3/3,1.0>2.0:3/3,1.0>3.0:3/3,2.0:3/3,2.0>1.0:3/3,2.0>3.0:3/3,3.0:3/3,3.0>1.0:3/3,3.0>2.0:3/3,nan:3/3,nan:3/3,nan:3/3,nan:3/3,nan>1.0:3/3,nan>1.0:3/3,nan>2.0:3/3,nan>2.0:3/3,nan>3.0:3/3,nan>3.0:3/3} succession{1.0:3/3,1.0>2.0:3/3,1.0>3.0:3/3,2.0:3/3,2.0>1.0:3/3,2.0>3.0:3/3,3.0:3/3,3.0>1.0:3/3,3.0>2.0:3/3,nan:3/3,nan:3/3,nan:3/3,nan:3/3,nan>1.0:3/3,nan>1.0:3/3,nan>2.0:3/3,nan>2.0:3/3,nan>3.0:3/3,nan>3.0:3/3}"},
      {"d dataframe K5 s68501.csv", "s68501.csv", f1, "dataframe", "d", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K5"}}, "coexistence{False>True:1/1,True:1/1} existence{False:1/1,True:1/1} init{True:1/1} nonchainsuccession{False>True:1/1,True:1/1} nonsuccession{False>True:1/1} precedence{True:1/1} responded_existence{False>True:1/1,True:1/1} response{True:1/1} succession{True:1/1}"},
      {"d dataframe K3 s68501.csv", "s68501.csv", f1, "dataframe", "d", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K3"}}, "altprecedence{2021-03-01 08:00:00+00:00>2021-03-02 09:30:00+00:00:1/1} chainprecedence{2021-03-01 08:00:00+00:00>2021-03-02 09:30:00+00:00:1/1} coexistence{2021-03-01 08:00:00+00:00>2021-03-02 09:30:00+00:00:1/1,2021-03-02 09:30:00+00:00>2021-03-01 08:00:00+00:00:1/1} exactly_one{2021-03-02 09:30:00+00:00:1/1} existence{2021-03-01 08:00:00+00:00:1/1,2021-03-02 09:30:00+00:00:1/1} init{2021-03-01 08:00:00+00:00:1/1} nonchainsuccession{2021-03-01 08:00:00+00:00>2021-03-02 09:30:00+00:00:1/1,2021-03-02 09:30:00+00:00>2021-03-01 08:00:00+00:00:1/1} nonsuccession{2021-03-02 09:30:00+00:00>2021-03-01 08:00:00+00:00:1/1} precedence{2021-03-01 08:00:00+00:00>2021-03-02 09:30:00+00:00:1/1} responded_existence{2021-03-01 08:00:00+00:00>2021-03-02 09:30:00+00:00:1/1,2021-03-02 09:30:00+00:00>2021-03-01 08:00:00+00:00:1/1} response{2021-03-01 08:00:00+00:00>2021-03-02 09:30:00+00:00:1/1} succession{2021-03-01 08:00:00+00:00>2021-03-02 09:30:00+00:00:1/1}"},
      {"d dataframe K4 s68501.csv", "s68501.csv", f1, "dataframe", "d", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K4"}}, "altprecedence{1>2.0:1/1,1>3.0:1/1,2>3.0:1/1} altresponse{2>1.0:1/1,2>3.0:1/1,3>1.0:1/1} altsuccession{2>3.0:1/1} chainprecedence{1>2.0:1/1,2>3.0:1/1} chainresponse{2>3.0:1/1,3>1.0:1/1} chainsuccession{2>3.0:1/1} coexistence{1>2.0:1/1,1>3.0:1/1,2>1.0:1/1,2>3.0:1/1,3>1.0:1/1,3>2.0:1/1} exactly_one{2:1/1,3:1/1} existence{1:1/1,2:1/1,3:1/1} init{1:1/1} nonchainsuccession{1>2.0:1/1,1>3.0:1/1,2>1.0:1/1,3>1.0:1/1,3>2.0:1/1} nonsuccession{1>2.0:1/1,1>3.0:1/1,2>1.0:1/1,3>1.0:1/1,3>2.0:1/1} precedence{1>2.0:1/1,1>3.0:1/1,2>3.0:1/1} responded_existence{1>2.0:1/1,1>3.0:1/1,2>1.0:1/1,2>3.0:1/1,3>1.0:1/1,3>2.0:1/1} response{2>1.0:1/1,2>3.0:1/1,3>1.0:1/1} succession{2>3.0:1/1}"},
      {"d dataframe K5 s68502.csv", "s68502.csv", f2, "dataframe", "d", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K5"}}, "exactly_one{True:1/1} existence{True:1/1} init{True:1/1}"},
      {"d dataframe K3 s68502.csv", "s68502.csv", f2, "dataframe", "d", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K3"}}, "coexistence{nan:1/1} exactly_one{nan:1/1} existence{nan:1/1} nonchainsuccession{nan:1/1} nonsuccession{nan:1/1} responded_existence{nan:1/1}"},
      {"d dataframe K4 s68502.csv", "s68502.csv", f2, "dataframe", "d", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K4"}}, "exactly_one{3:1/1} existence{3:1/1} init{3:1/1}"},
      {"d dataframe K3 s68505.csv", "s68505.csv", f3, "dataframe", "d", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K3"}}, "absence{2021-03-02 09:30:00+00:00:5/4} coexistence{NaT:4/4} exactly_one{NaT:5/4} existence{2021-03-01 08:00:00+00:00:5/4,NaT:5/4} nonchainsuccession{2021-03-01 08:00:00+00:00:5/5,2021-03-01 08:00:00+00:00>2021-03-02 09:30:00+00:00:5/5,2021-03-02 09:30:00+00:00:4/4,2021-03-02 09:30:00+00:00>2021-03-01 08:00:00+00:00:5/5,NaT:4/4,NaT>2021-03-01 08:00:00+00:00:5/5,NaT>2021-03-02 09:30:00+00:00:4/4} noncoexistence{2021-03-01 08:00:00+00:00>2021-03-02 09:30:00+00:00:5/5,2021-03-02 09:30:00+00:00>2021-03-01 08:00:00+00:00:5/5} nonsuccession{2021-03-01 08:00:00+00:00:5/4,2021-03-01 08:00:00+00:00>2021-03-02 09:30:00+00:00:5/5,2021-03-02 09:30:00+00:00:4/4,2021-03-02 09:30:00+00:00>2021-03-01 08:00:00+00:00:5/5,NaT:4/4} responded_existence{NaT:4/4}"},
      {"d dataframe K3 s68507.csv", "s68507.csv", f4, "dataframe", "d", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K3"}}, "existence{2021-03-01 08:00:00+00:00:3/3} nonchainsuccession{2021-03-01 08:00:00+00:00:3/3,NaT>2021-03-01 08:00:00+00:00:3/3} nonsuccession{2021-03-01 08:00:00+00:00:3/3}"},
      {"d dataframe K5 s68508.csv", "s68508.csv", f5, "dataframe", "d", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K5"}}, "altprecedence{True:2/2} chainprecedence{True:2/2} coexistence{False>True:2/2,True:2/2} exactly_one{False:2/2} existence{False:2/2,True:2/2} init{True:2/2} nonchainsuccession{False>True:2/2,True:2/2} nonsuccession{False>True:2/2} precedence{True:2/2} responded_existence{False>True:2/2,True:2/2}"},
      {"d dataframe K5 s68519.csv", "s68519.csv", f6, "dataframe", "d", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K5"}}, "nonchainsuccession{False:6/6,False>True:5/5,True:6/6,nan:6/6,nan>True:6/6} nonsuccession{False:6/5,True:6/6,nan:6/5}"},
      {"d dataframe K5 s68524.csv", "s68524.csv", f7, "dataframe", "d", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K5"}}, "coexistence{nan:4/4} existence{nan:5/4} nonchainsuccession{False:5/5,False>True:4/4,True:5/5,nan:4/4,nan>True:5/5} noncoexistence{True:5/4,nan>True:5/4} nonsuccession{False:5/4,False>True:4/4,True:5/5,nan:4/4,nan>True:5/4} responded_existence{nan:4/4}"},
      {"d dataframe K5 s68537.csv", "s68537.csv", f8, "dataframe", "d", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K5"}}, "existence{True:5/4} nonchainsuccession{False>True:5/5,True:5/5,nan>True:5/5} nonsuccession{False>True:5/4,True:5/5}"},
      {"d dataframe K5 s68568.csv", "s68568.csv", f9, "dataframe", "d", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K5"}}, "absence{False:6/5} coexistence{nan:4/4} nonchainsuccession{False:5/5,True:5/5,nan:4/4,nan>True:5/5} noncoexistence{False:5/5,True:5/4,nan:5/5,nan>True:5/4} nonsuccession{False:5/5,True:5/4,nan:4/4,nan>True:5/5} responded_existence{nan:4/4}"},
      {"d dataframe K5 s68589.csv", "s68589.csv", f10, "dataframe", "d", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K5"}}, "coexistence{nan:4/4} exactly_one{True:5/4} existence{True:5/4,nan:5/4} nonchainsuccession{False:5/5,True:5/5,nan:4/4,nan>True:5/5} nonsuccession{False:5/4,True:5/4,nan:4/4} responded_existence{nan:4/4}"},
      {"h log K5 p_bool_a.csv", "p_bool_a.csv", f11, "log", "h", {{"case_column", "case"}, {"activity_column", "act"}, {"timestamp_column", "ts"}, {"heuristic_activity_attribute", "K5"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"bool\") to str"},
      {"h log K5 p_bool_b.csv", "p_bool_b.csv", f12, "log", "h", {{"case_column", "case"}, {"activity_column", "act"}, {"timestamp_column", "ts"}, {"heuristic_activity_attribute", "K5"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"float\") to str"},
      {"h log K5 p_float_a.csv", "p_float_a.csv", f13, "log", "h", {{"case_column", "case"}, {"activity_column", "act"}, {"timestamp_column", "ts"}, {"heuristic_activity_attribute", "K5"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"float\") to str"},
      {"h log K5 p_mixed_b.csv", "p_mixed_b.csv", f14, "log", "h", {{"case_column", "case"}, {"activity_column", "act"}, {"timestamp_column", "ts"}, {"heuristic_activity_attribute", "K5"}, {"heuristic_variant", "plusplus"}}, "error: '<' not supported between instances of 'float' and 'str'"},
      {"h log K1 s68500.csv", "s68500.csv", f15, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}, {"heuristic_variant", "plusplus"}}, "error: '<' not supported between instances of 'float' and 'str'"},
      {"h log K2 s68500.csv", "s68500.csv", f15, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K2"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"float\") to str"},
      {"h log K3 s68500.csv", "s68500.csv", f15, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K3"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"Timestamp\") to str"},
      {"h log K4 s68500.csv", "s68500.csv", f15, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"float\") to str"},
      {"h log K5 s68500.csv", "s68500.csv", f15, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K5"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"bool\") to str"},
      {"h log K6 s68500.csv", "s68500.csv", f15, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K6"}, {"heuristic_variant", "plusplus"}}, "A[1,a,b] E[a>b:1.000000/1 b>1:1.000000/2] R 2/13/13/1/1"},
      {"h log K1 s68501.csv", "s68501.csv", f16, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}, {"heuristic_variant", "plusplus"}}, "error: '<' not supported between instances of 'float' and 'str'"},
      {"h log K2 s68501.csv", "s68501.csv", f16, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K2"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"float\") to str"},
      {"h log K3 s68501.csv", "s68501.csv", f16, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K3"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"Timestamp\") to str"},
      {"h log K4 s68501.csv", "s68501.csv", f16, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"int\") to str"},
      {"h log K5 s68501.csv", "s68501.csv", f16, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K5"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"bool\") to str"},
      {"h log K1 s68502.csv", "s68502.csv", f17, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}, {"heuristic_variant", "plusplus"}}, "A[nan] E[] R 1/2/2/0/0"},
      {"h log K2 s68502.csv", "s68502.csv", f17, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K2"}, {"heuristic_variant", "plusplus"}}, "A[2.5] E[] R 1/2/2/0/0"},
      {"h log K3 s68502.csv", "s68502.csv", f17, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K3"}, {"heuristic_variant", "plusplus"}}, "A[nan] E[] R 1/2/2/0/0"},
      {"h log K4 s68502.csv", "s68502.csv", f17, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}, {"heuristic_variant", "plusplus"}}, "A[3] E[] R 1/2/2/0/0"},
      {"h log K5 s68502.csv", "s68502.csv", f17, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K5"}, {"heuristic_variant", "plusplus"}}, "A[True] E[] R 1/2/2/0/0"},
      {"h log K6 s68502.csv", "s68502.csv", f17, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K6"}, {"heuristic_variant", "plusplus"}}, "A[nan] E[] R 1/2/2/0/0"},
      {"h log K1 s68503.csv", "s68503.csv", f18, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}, {"heuristic_variant", "plusplus"}}, "error: '<' not supported between instances of 'float' and 'str'"},
      {"h log K2 s68503.csv", "s68503.csv", f18, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K2"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"float\") to str"},
      {"h log K3 s68503.csv", "s68503.csv", f18, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K3"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"NaTType\") to str"},
      {"h log K4 s68503.csv", "s68503.csv", f18, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"float\") to str"},
      {"h log K3 s68504.csv", "s68504.csv", f19, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K3"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"Timestamp\") to str"},
      {"h log K4 s68504.csv", "s68504.csv", f19, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"float\") to str"},
      {"h log K6 s68504.csv", "s68504.csv", f19, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K6"}, {"heuristic_variant", "plusplus"}}, "error: '<' not supported between instances of 'float' and 'str'"},
      {"h log K3 s68505.csv", "s68505.csv", f20, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K3"}, {"heuristic_variant", "plusplus"}}, "A[2021-03-01 08:00:00+00:00,2021-03-02 09:30:00+00:00,NaT] E[] R 1/16/14/4/6"},
      {"h log K5 s68505.csv", "s68505.csv", f20, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K5"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"float\") to str"},
      {"h log K6 s68505.csv", "s68505.csv", f20, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K6"}, {"heuristic_variant", "plusplus"}}, "error: '<' not supported between instances of 'str' and 'float'"},
      {"h log K2 s68506.csv", "s68506.csv", f21, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K2"}, {"heuristic_variant", "plusplus"}}, "A[2.5,nan,nan] E[] R 1/6/5/1/2"},
      {"h log K4 s68506.csv", "s68506.csv", f21, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"int\") to str"},
      {"h log K6 s68506.csv", "s68506.csv", f21, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K6"}, {"heuristic_variant", "plusplus"}}, "error: '<' not supported between instances of 'float' and 'str'"},
      {"h log K3 s68507.csv", "s68507.csv", f22, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K3"}, {"heuristic_variant", "plusplus"}}, "A[2021-03-01 08:00:00+00:00,NaT] E[] R 0/11/11/5/5"},
      {"h log K1 s68508.csv", "s68508.csv", f23, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}, {"heuristic_variant", "plusplus"}}, "A[p,q,r] E[] R 0/9/7/3/5"},
      {"h log K4 s68509.csv", "s68509.csv", f24, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"int\") to str"},
      {"h log K5 s68510.csv", "s68510.csv", f25, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K5"}, {"heuristic_variant", "plusplus"}}, "A[False,True,nan] E[] R 2/8/8/2/2"},
      {"h log K2 s68513.csv", "s68513.csv", f26, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K2"}, {"heuristic_variant", "plusplus"}}, "A[3.0,nan] E[] R 2/4/4/0/0"},
      {"h log K2 s68516.csv", "s68516.csv", f27, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K2"}, {"heuristic_variant", "plusplus"}}, "A[1.5,2.5,nan] E[] R 2/14/13/5/6"},
      {"h log K4 s68516.csv", "s68516.csv", f27, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}, {"heuristic_variant", "plusplus"}}, "A[1.0,2.0,nan] E[] R 2/14/13/5/6"},
      {"h log K5 s68516.csv", "s68516.csv", f27, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K5"}, {"heuristic_variant", "plusplus"}}, "A[False,True,nan] E[] R 2/14/12/4/6"},
      {"h log K3 s68517.csv", "s68517.csv", f28, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K3"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"NaTType\") to str"},
      {"h log K4 s68521.csv", "s68521.csv", f29, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}, {"heuristic_variant", "plusplus"}}, "A[2.0,7.0,nan,nan] E[] R 2/7/6/0/1"},
      {"h log K5 s68521.csv", "s68521.csv", f29, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K5"}, {"heuristic_variant", "plusplus"}}, "A[False,nan] E[] R 2/7/6/0/1"},
      {"h log K3 s68524.csv", "s68524.csv", f30, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K3"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"NaTType\") to str"},
      {"h log K1 s68548.csv", "s68548.csv", f31, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}, {"heuristic_variant", "plusplus"}}, "A[nan] E[] R 1/2/2/0/0"},
      {"h log K6 s68548.csv", "s68548.csv", f31, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K6"}, {"heuristic_variant", "plusplus"}}, "A[nan] E[] R 1/2/2/0/0"},
      {"h log K4 s68560.csv", "s68560.csv", f32, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}, {"heuristic_variant", "plusplus"}}, "A[1.0,2.0] E[] R 1/7/7/3/3"},
      {"h log K6 s68579.csv", "s68579.csv", f33, "log", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K6"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"float\") to str"},
      {"h log K6 h68007.csv", "h68007.csv", f34, "log", "h", {{"case_column", "case"}, {"activity_column", "act"}, {"timestamp_column", "ts"}, {"heuristic_activity_attribute", "K6"}, {"heuristic_variant", "plusplus"}}, "A[1.0,nan] E[] R 3/6/6/0/0"},
      {"h log K6 h68037.csv", "h68037.csv", f35, "log", "h", {{"case_column", "case"}, {"activity_column", "act"}, {"timestamp_column", "ts"}, {"heuristic_activity_attribute", "K6"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"float\") to str"},
      {"h log K6 h68039.csv", "h68039.csv", f36, "log", "h", {{"case_column", "case"}, {"activity_column", "act"}, {"timestamp_column", "ts"}, {"heuristic_activity_attribute", "K6"}, {"heuristic_variant", "plusplus"}}, "error: can only concatenate str (not \"float\") to str"},
  };
  for (const auto& c : cases)
    check(c.label + " (" + c.shape + ")", outcome(client, c.name, c.content, c.shape, c.kind, c.fields), c.expected);

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures) {
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "review HD server checks passed\n";
  return 0;
}
