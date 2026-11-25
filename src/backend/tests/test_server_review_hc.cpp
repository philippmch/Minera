#undef NDEBUG
// Review HC: activity keys over typed or gapped columns of a CSV upload on
// its DataFrame and LazyFrame shapes, and the Declare playout of a typed
// model.
//
// - Alpha, the Inductive miner, the log skeleton, the temporal profile and
//   Heuristics++ keyed by a column read_csv types as numbers, booleans,
//   dates or objects, or with gaps, read format_dataframe's typed cells
//   (the Polars copy: gaps null, an object column str() of each object) as
//   the XES frame paths read theirs: labels are str() of each object, gaps
//   NaN or None, Python-equal objects one activity; a start column of other
//   values than dates, business hours and the frame's copies
//   case:concept:name and time:timestamp under other headers too (Polars'
//   plans list the frame's columns in read_csv's order, then the copies).
// - The classic heuristics miner on a Polars frame keyed by a True/False
//   column with a gap in a row format_dataframe drops (read_csv's object
//   column, a String copy).
// - The Declare playout of a model keyed by a typed column fails as
//   pm4py's playout does on a key that is no tuple or str.
//
// Expected values are pm4py 2.7.22 black-box on the same inputs
// (review-harness round-hc: tc/mkhc.py, ct/csvk.py, ct/csvo.py,
// ct/dplay.py).
//
// Usage: minera_server_review_hc_tests <minera_server>

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

std::string label(const Json& value, const std::string& null_label) {
  return value.kind == Json::Kind::kNull ? null_label : value.text;
}

std::string fixed(const Json& value, int digits) {
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "%.*f", digits, std::strtod(value.text.c_str(), nullptr));
  return buffer;
}

using Fields = std::vector<std::pair<std::string, std::string>>;

std::string mine_error(const Json& family) {
  const auto& type = family["error"]["type"].text;
  return "error: " + (type.starts_with("builtins.") ? type.substr(9) + ": " : std::string()) + family["error"]["message"].text;
}

std::string tree(const Json& node, const Json& names) {
  const auto& op = node["op"].text;
  if (op == "TAU") return "tau";
  if (op == "ACTIVITY") return names.items[std::stoul(node["activity"].text)].text;
  const std::string symbol = op == "SEQUENCE" ? "->" : op == "EXCLUSIVE" ? "X" : op == "PARALLEL" ? "+" : op == "LOOP" ? "*" : "O";
  std::vector<std::string> children;
  for (const auto& child : node["children"].items) children.push_back(tree(child, names));
  if (symbol == "X" || symbol == "+") std::sort(children.begin(), children.end());
  std::string out = symbol + "(";
  for (std::size_t k = 0; k < children.size(); ++k) out += (k ? "," : "") + children[k];
  return out + ")";
}

// One case's outcome in mkhc.py's canonical form.
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
  if (kind == "dp") {
    const auto& x = body["family_results"]["declare"];
    if (x["executed"].text != "true") return "mine: " + mine_error(x).substr(7);
    const auto played = client.Get("/api/playout?miner=declare");
    if (!played) return "no response";
    if (played->body.find('<') == 0 || (played->status == 200 && played->body.find("<?xml") != std::string::npos)) return "ok";
    JsonReader error_reader{played->body};
    return "error: " + error_reader.value()["error"].text;
  }
  const std::string null_label = shape == "dataframe" ? "nan" : "None";
  if (body["error"].kind != Json::Kind::kNull) return "mine error: " + body["error"]["message"].text;
  const std::string family_name = kind == "a" || kind == "a3" ? "alpha" : kind == "i" ? "inductive"
                                  : kind == "ls" ? "log_skeleton" : kind == "tp" || kind == "tpb" ? "temporal_profile"
                                                                                                    : "heuristic";
  const auto& x = body["family_results"][family_name];
  if (x.kind != Json::Kind::kObject) return "no family result";
  if (x["executed"].text != "true") return mine_error(x);
  const auto& result = x["result"];
  if (kind == "ls") {
    const auto& model = result["model"];
    std::string out = "LS";
    for (const char* relation : {"equivalence", "always_after", "always_before", "never_together", "directly_follows"}) {
      std::vector<std::string> pairs;
      for (const auto& p : model[relation].items) pairs.push_back(label(p["source"], null_label) + ">" + label(p["target"], null_label));
      out += std::string(" ") + relation + "[" + sorted_join(pairs, ",") + "]";
    }
    std::vector<std::string> freq;
    for (const auto& [activity, counts] : model["activ_freq"].members) {
      std::vector<std::string> numbers;
      for (const auto& n : counts.items) numbers.push_back(n.text);
      freq.push_back(activity + "=" + sorted_join(numbers, "/"));
    }
    return out + " activ_freq[" + sorted_join(freq, ",") + "]";
  }
  if (kind == "tp" || kind == "tpb") {
    std::vector<std::string> entries;
    for (const auto& e : result["entries"].items)
      entries.push_back(label(e["source"], null_label) + ">" + label(e["target"], null_label) + ":" +
                        fixed(e["mean_seconds"], 4) + "/" + fixed(e["standard_deviation_seconds"], 4));
    return sorted_join(entries, ";");
  }
  if (kind == "a" || kind == "a3") {
    const auto places = std::stoul(result["place_count"].text);
    std::vector<std::vector<std::string>> ins(places), outs(places);
    std::vector<std::string> labels;
    for (const auto& t : result["transitions"].items) {
      const auto name = t["activity_is_null"].text == "true" ? null_label : t["activity_name"].text;
      labels.push_back(name);
      for (const auto& p : t["inputs"].items) outs[std::stoul(p.text)].push_back(name);
      for (const auto& p : t["outputs"].items) ins[std::stoul(p.text)].push_back(name);
    }
    std::vector<std::string> shown;
    for (std::size_t p = 0; p < places; ++p)
      shown.push_back(sorted_join(ins[p], ",") + ">" + sorted_join(outs[p], ",") +
                      (result["source"].text == std::to_string(p) ? "#s" : "") +
                      (result["sink"].text == std::to_string(p) ? "#k" : ""));
    return "T[" + sorted_join(labels, ",") + "] P[" + sorted_join(shown, " ") + "]";
  }
  if (kind == "i") return tree(result["root"], result["activities"]);
  std::vector<std::string> activities, edges;
  for (const auto& a : result["activities"].items) activities.push_back(label(a, null_label));
  for (const auto& e : result["edges"].items)
    edges.push_back(activities[std::stoul(e["source"].text)] + ">" + activities[std::stoul(e["target"].text)] + ":" +
                    fixed(e["dependency"], 6) + "/" + e["frequency"].text);
  return "A[" + sorted_join(activities, ",") + "] E[" + sorted_join(edges, " ") + "]";
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
  const std::string f0 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:10:00,,3.0,2021-03-01 08:00:00,,True,a\nc1,a,2021-03-01 09:41:00,q,,,,True,1\nc1,a,2021-03-01 10:41:00,r,,,2,False,1\nc1,b,2021-03-01 10:46:00,,1.5,,,,a\nc2,b,2021-03-01 08:32:00,p,,2021-03-01 08:00:00,2,,b\nc2,b,2021-03-01 08:33:00,r,3.0,,,,a\nc3,c,2021-03-01 08:06:00,r,2.5,2021-03-02 09:30:00,3,False,1\n";
  const std::string f1 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:31:00,r,,2021-03-01 08:00:00,7,False,a\nc0,c,2021-03-01 08:36:00,r,,2021-03-02 09:30:00,7,False,\nc1,a,2021-03-01 08:45:00,s,2.5,2021-03-02 09:30:00,3,True,2\nc1,c,2021-03-01 08:50:00,,,2021-03-02 09:30:00,,False,b\nc1,a,2021-03-01 08:55:00,s,3.0,2021-03-03 10:00:00,1,,b\nc2,b,2021-03-01 08:32:00,s,2.5,2021-03-01 08:00:00,,False,a\nc2,b,2021-03-01 08:33:00,r,1.5,2021-03-01 08:00:00,,False,2\n";
  const std::string f2 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 08:52:00,q,1.5,,,True,a\nc1,c,2021-03-01 09:26:00,p,2.5,2021-03-01 08:00:00,1,True,b\nc1,b,2021-03-01 10:26:00,p,2.5,2021-03-02 09:30:00,1,False,1\nc1,b,2021-03-01 10:31:00,r,1.5,2021-03-02 09:30:00,2,False,1\nc1,c,2021-03-01 10:32:00,,1.5,2021-03-01 08:00:00,,,\nc2,a,2021-03-01 09:16:00,r,1.5,,2,True,1\nc2,a,2021-03-01 10:16:00,p,3.0,2021-03-02 09:30:00,,False,b\nc3,a,2021-03-01 09:43:00,q,2.5,2021-03-01 08:00:00,,True,1\nc4,b,2021-03-01 08:52:00,,,,2,,\nc4,a,2021-03-01 08:53:00,r,1.5,,3,True,1\nc4,c,2021-03-01 08:54:00,,2.5,2021-03-02 09:30:00,3,False,1\nc4,c,2021-03-01 09:54:00,p,2.5,2021-03-01 08:00:00,1,False,\n";
  const std::string f3 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 08:35:00,q,3.0,2021-03-02 09:30:00,,True,a\nc1,a,2021-03-01 09:43:00,r,3.0,,1,False,1\nc2,a,2021-03-01 08:12:00,q,,2021-03-02 09:30:00,3,False,a\nc2,c,2021-03-01 08:17:00,,3.0,2021-03-02 09:30:00,3,False,a\nc3,a,2021-03-01 09:12:00,r,1.5,2021-03-02 09:30:00,1,False,a\n";
  const std::string f4 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 08:17:00,r,2.5,2021-03-02 09:30:00,2,True,1\nc1,a,2021-03-01 08:23:00,r,2.5,2021-03-01 08:00:00,2,,\nc1,b,2021-03-01 08:28:00,q,2.5,2021-03-01 08:00:00,1,True,a\nc1,b,2021-03-01 08:29:00,r,,2021-03-02 09:30:00,,True,1\nc1,b,2021-03-01 09:29:00,q,,2021-03-01 08:00:00,2,,1\n";
  const std::string f5 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:36:00,q,2.5,,1,True,1\nc1,c,2021-03-01 08:50:00,,3.0,2021-03-01 08:00:00,3,False,\nc2,a,2021-03-01 08:48:00,r,2.5,2021-03-02 09:30:00,3,False,1\n";
  const std::string f6 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 08:38:00,p,3.0,2021-03-02 09:30:00,,True,1\nc1,a,2021-03-01 08:15:00,r,1.5,2021-03-01 08:00:00,,False,\nc2,c,2021-03-01 09:28:00,r,2.5,2021-03-01 08:00:00,1,True,\nc2,a,2021-03-01 09:33:00,p,,,,,\nc2,c,2021-03-01 09:38:00,r,,2021-03-02 09:30:00,3,False,a\nc2,a,2021-03-01 09:43:00,r,2.5,,3,False,\nc3,b,2021-03-01 08:52:00,p,3.0,,,False,a\nc4,a,2021-03-01 09:24:00,,2.5,2021-03-01 08:00:00,,True,\nc5,c,2021-03-01 08:16:00,q,,2021-03-02 09:30:00,1,True,1\nc5,c,2021-03-01 09:16:00,q,3.0,2021-03-01 08:00:00,3,True,b\n";
  const std::string f7 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:37:00,r,-1.0,,2,False,a\nc0,c,2021-03-01 09:37:00,q,,,,False,b\nc0,b,2021-03-01 09:38:00,s,,2021-03-01 08:00:00,3,False,2\nc0,b,2021-03-01 09:39:00,,,2021-03-03 10:00:00,7,True,b\nc1,b,2021-03-01 08:03:00,s,,2021-03-01 08:00:00,,True,1\nc1,a,2021-03-01 08:04:00,q,3.0,2021-03-03 10:00:00,1,False,2\nc1,a,2021-03-01 09:04:00,q,2.5,,,True,1\nc1,b,2021-03-01 09:05:00,,-1.0,2021-03-01 08:00:00,2,True,a\nc2,c,2021-03-01 08:22:00,s,,2021-03-01 08:00:00,3,True,2\nc2,b,2021-03-01 08:27:00,p,,2021-03-01 08:00:00,,True,2\nc2,b,2021-03-01 08:32:00,r,1.5,2021-03-01 08:00:00,7,True,1\nc2,b,2021-03-01 09:32:00,r,2.5,2021-03-01 08:00:00,7,False,2\nc3,a,2021-03-01 08:26:00,,,2021-03-03 10:00:00,1,True,1\n";
  const std::string f8 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:24:00,r,1.5,2021-03-01 08:00:00,3,False,1\nc1,c,2021-03-01 08:26:00,r,2.5,2021-03-01 08:00:00,1,False,\nc1,c,2021-03-01 08:31:00,,3.0,,2,,b\nc1,c,2021-03-01 08:36:00,r,2.5,,,,1\nc1,b,2021-03-01 08:41:00,p,1.5,2021-03-02 09:30:00,3,False,\n";
  const std::string f9 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 09:34:00,,-1.0,2021-03-01 08:00:00,1,False,1\nc0,a,2021-03-01 09:35:00,q,,2021-03-01 08:00:00,2,True,a\nc1,c,2021-03-01 09:48:00,p,-1.0,2021-03-01 08:00:00,2,False,\n";
  const std::string f10 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-11 18:00:00,1,p,,2021-03-01 08:00:00,1,True,a\nc0,a,2021-03-12 14:00:00,18446744073709551625,,3.0,2021-03-01 08:00:00,2,True,1\nc0,a,2021-03-13 10:00:00,2,r,2.5,2021-03-01 08:00:00,3,False,a\nc0,c,2021-03-15 02:00:00,0,,1.5,2021-03-02 09:30:00,1,False,1\n";
  const std::string f11 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-11 08:00:00,0,,2.5,,3,True,\n";
  const std::string f12 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 08:33:00,4,r,3.0,,3,True,\nc0,d,2021-03-01 08:34:00,18446744073709551620,q,2.5,2021-03-01 08:00:00,3,True,b\nc0,a,2021-03-01 08:36:00,7,r,,,2,False,b\nc0,b,,8,q,,2021-03-01 08:00:00,2,,a\nc1,d,2021-03-01 08:00:00,2,q,2.5,,,True,1\nc1,a,2021-03-01 08:03:00,18446744073709551617,p,1.5,2021-03-01 08:00:00,1,True,\nc1,d,2021-03-01 08:03:00,3,q,2.5,,,False,1\nc1,b,2021-03-01 08:06:00,2,p,1.5,2021-03-02 09:30:00,2,True,a\n";
  const std::string f13 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,d,,True,r,,2021-03-02 09:30:00,3,,1\nc0,a,2021-03-01 08:28:00,True,q,2.5,,2,False,a\nc0,d,2021-03-01 08:31:00,False,q,3.0,2021-03-01 08:00:00,3,True,a\nc1,d,2021-03-01 08:18:00,False,q,,2021-03-01 08:00:00,1,False,\nc2,a,,,q,,2021-03-02 09:30:00,2,,1\n";
  const std::string f14 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-02 06:00:00,-1,p,1.5,2021-03-01 08:00:00,3,False,a\nc0,a,2021-03-02 07:00:00,-1,r,3.0,2021-03-02 09:30:00,,,1\nc0,c,2021-03-02 08:00:00,9223372036854775809,r,2.5,2021-03-01 08:00:00,1,False,b\nc1,c,2021-03-01 09:00:00,-1,p,2.5,2021-03-02 09:30:00,1,True,1\nc2,c,2021-03-01 22:00:00,9223372036854775809,q,2.5,2021-03-01 08:00:00,,,1\n";
  const std::string f15 = "cid,act,ts,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-11 08:00:00,0,,2.5,,3,True,\n";
  const std::string f16 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 08:33:00,p,3.0,2021-03-02 09:30:00,3,False,\nc0,c,2021-03-01 08:34:00,,1.5,2021-03-02 09:30:00,3,False,\nc0,a,2021-03-01 08:39:00,,2.5,2021-03-02 09:30:00,3,True,\nc1,c,2021-03-01 08:44:00,,2.5,2021-03-01 08:00:00,2,,1\nc2,a,2021-03-01 09:25:00,p,1.5,,,,\nc2,c,2021-03-01 09:26:00,,1.5,2021-03-01 08:00:00,3,True,\nc3,c,2021-03-01 09:12:00,,2.5,2021-03-01 08:00:00,2,True,1\nc3,b,2021-03-01 10:12:00,p,,2021-03-02 09:30:00,,False,b\n";
  const std::string f17 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 09:38:00,r,3.0,2021-03-01 08:00:00,,True,b\nc0,c,2021-03-01 09:39:00,q,1.5,2021-03-01 08:00:00,2,False,a\nc1,b,2021-03-01 09:37:00,r,2.5,2021-03-01 08:00:00,3,False,\nc1,c,2021-03-01 09:42:00,q,,2021-03-01 08:00:00,2,False,b\nc1,a,2021-03-01 09:43:00,,2.5,2021-03-02 09:30:00,,True,\nc2,c,2021-03-01 08:10:00,r,3.0,2021-03-02 09:30:00,,False,1\nc2,c,2021-03-01 08:11:00,q,,2021-03-02 09:30:00,,,\nc2,c,2021-03-01 08:16:00,,,2021-03-01 08:00:00,2,False,a\nc3,a,2021-03-01 08:11:00,,1.5,,,,a\nc4,c,2021-03-01 08:03:00,r,,,2,True,\n";
  const std::string f18 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,d,2021-03-01 08:08:00,True,p,1.5,2021-03-01 08:00:00,2,True,b\nc0,b,2021-03-01 08:09:00,False,q,3.0,2021-03-02 09:30:00,,False,1\nc1,c,2021-03-01 08:10:00,True,r,3.0,2021-03-01 08:00:00,3,,a\nc1,a,,True,q,2.5,2021-03-02 09:30:00,1,False,\nc1,a,2021-03-01 08:17:00,True,r,3.0,2021-03-01 08:00:00,3,False,a\nc2,c,2021-03-01 08:18:00,True,r,2.5,,,True,a\nc2,c,2021-03-01 08:21:00,False,,1.5,,1,True,b\nc2,d,2021-03-01 08:23:00,True,p,,,2,True,1\n";
  const std::string f19 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:32:00,2021-03-01 08:31:00,p,1.5,,,True,b\nc0,c,2021-03-01 08:35:00,2021-03-01 08:34:00,r,1.5,2021-03-01 08:00:00,1,,1\nc0,a,,2021-03-01 08:33:00,q,2.5,,3,False,a\nc0,b,,2021-03-01 08:34:00,p,1.5,2021-03-01 08:00:00,3,False,1\nc1,a,2021-03-01 08:15:00,2021-03-01 08:14:00,p,3.0,2021-03-01 08:00:00,2,False,\nc2,c,,2021-03-01 08:05:00,p,3.0,,3,False,b\nc2,a,2021-03-01 08:07:00,2021-03-01 08:07:00,r,3.0,,2,True,1\nc2,c,2021-03-01 08:07:00,2021-03-01 08:04:00,r,,2021-03-02 09:30:00,,False,1\nc2,a,2021-03-01 08:11:00,2021-03-01 08:08:00,r,1.5,2021-03-02 09:30:00,3,,1\nc3,a,,2021-03-01 08:28:00,q,1.5,,3,,\nc3,c,2021-03-01 08:30:00,2021-03-01 08:29:00,r,1.5,2021-03-01 08:00:00,1,True,b\nc3,d,2021-03-01 08:31:00,2021-03-01 08:28:00,r,,,2,False,b\nc4,b,,2021-03-01 08:20:00,,3.0,,1,False,b\nc4,a,2021-03-01 08:23:00,2021-03-01 08:22:00,q,2.5,,1,False,\nc4,d,2021-03-01 08:26:00,2021-03-01 08:23:00,,1.5,2021-03-01 08:00:00,3,True,1\nc4,a,2021-03-01 08:30:00,2021-03-01 08:30:00,q,,2021-03-01 08:00:00,1,,1\n";
  const std::string f20 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,d,2021-03-01 08:02:00,2021-03-01 08:00:00,p,2.5,,,,\nc0,d,2021-03-01 08:04:00,2021-03-01 08:04:00,q,1.5,2021-03-01 08:00:00,1,True,a\nc0,b,2021-03-01 08:05:00,2021-03-01 08:05:00,q,2.5,,1,False,a\nc0,a,2021-03-01 08:07:00,2021-03-01 08:07:00,p,,2021-03-01 08:00:00,1,,\nc1,d,2021-03-01 08:30:00,,,2.5,2021-03-01 08:00:00,1,False,a\nc1,d,2021-03-01 08:30:00,,q,,2021-03-01 08:00:00,1,False,a\nc2,b,2021-03-01 08:05:00,2021-03-01 08:05:00,,2.5,2021-03-01 08:00:00,1,False,1\nc2,c,2021-03-01 08:07:00,2021-03-01 08:07:00,,1.5,2021-03-01 08:00:00,2,,1\n";
  const std::string f21 = "cid,act,ts,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:32:00,2021-03-01 08:31:00,p,1.5,,,True,b\nc0,c,2021-03-01 08:35:00,2021-03-01 08:34:00,r,1.5,2021-03-01 08:00:00,1,,1\nc0,a,,2021-03-01 08:33:00,q,2.5,,3,False,a\nc0,b,,2021-03-01 08:34:00,p,1.5,2021-03-01 08:00:00,3,False,1\nc1,a,2021-03-01 08:15:00,2021-03-01 08:14:00,p,3.0,2021-03-01 08:00:00,2,False,\nc2,c,,2021-03-01 08:05:00,p,3.0,,3,False,b\nc2,a,2021-03-01 08:07:00,2021-03-01 08:07:00,r,3.0,,2,True,1\nc2,c,2021-03-01 08:07:00,2021-03-01 08:04:00,r,,2021-03-02 09:30:00,,False,1\nc2,a,2021-03-01 08:11:00,2021-03-01 08:08:00,r,1.5,2021-03-02 09:30:00,3,,1\nc3,a,,2021-03-01 08:28:00,q,1.5,,3,,\nc3,c,2021-03-01 08:30:00,2021-03-01 08:29:00,r,1.5,2021-03-01 08:00:00,1,True,b\nc3,d,2021-03-01 08:31:00,2021-03-01 08:28:00,r,,,2,False,b\nc4,b,,2021-03-01 08:20:00,,3.0,,1,False,b\nc4,a,2021-03-01 08:23:00,2021-03-01 08:22:00,q,2.5,,1,False,\nc4,d,2021-03-01 08:26:00,2021-03-01 08:23:00,,1.5,2021-03-01 08:00:00,3,True,1\nc4,a,2021-03-01 08:30:00,2021-03-01 08:30:00,q,,2021-03-01 08:00:00,1,,1\n";
  const std::string f22 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 08:05:00,9223372036854775809,p,1.5,2021-03-02 09:30:00,,True,a\nc0,a,2021-03-01 08:09:00,3,,3.0,2021-03-02 09:30:00,1,True,b\nc0,d,2021-03-01 08:11:00,9223372036854775809,p,3.0,,,True,a\n";
  const std::string f23 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:21:00,late,p,2.5,,1,True,a\nc0,b,2021-03-01 08:23:00,2021-03-01 08:23:00,p,,2021-03-01 08:00:00,1,False,a\nc0,c,,2021-03-01 08:24:00,q,2.5,2021-03-01 08:00:00,2,True,\nc0,c,,NA,p,,2021-03-01 08:00:00,1,True,\nc0,c,2021-03-01 08:27:00,,,2.5,2021-03-01 08:00:00,2,False,1\nc1,d,2021-03-01 08:14:00,NA,,,,2,True,\nc2,b,,2021-03-01 08:07:00,p,,2021-03-01 08:00:00,2,False,1\n";
  const std::string f24 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:19:00,True,,2.5,,1,,\nc0,c,2021-03-01 08:19:00,True,p,,2021-03-01 08:00:00,1,True,a\nc0,a,2021-03-01 08:21:00,True,p,,,2,,a\nc1,b,2021-03-01 08:23:00,True,p,1.5,2021-03-01 08:00:00,2,False,1\nc1,c,2021-03-01 08:25:00,True,p,,2021-03-01 08:00:00,1,,a\n";
  const std::string f25 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 08:05:00,late,,3.0,2021-03-01 08:00:00,2,True,1\nc1,a,2021-03-01 08:27:00,late,s,,2021-03-03 10:00:00,2,False,1\n";
  const std::string f26 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 08:29:00,3,r,2.5,,1,False,b\nc1,c,2021-03-01 08:33:00,2,,2.5,,3,True,a\n";
  const std::string f27 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-02 07:00:00,True,,2.5,2021-03-01 08:00:00,2,False,a\nc0,c,2021-03-02 07:00:00,,p,,,2,False,a\nc0,d,2021-03-02 07:00:00,False,q,1.5,2021-03-01 08:00:00,2,True,\nc0,b,,False,q,1.5,2021-03-01 08:00:00,,False,\nc0,a,2021-03-02 12:00:00,False,q,2.5,2021-03-01 08:00:00,1,False,1\nc1,d,2021-03-02 10:00:00,False,p,2.5,,1,False,1\nc2,b,2021-03-01 21:00:00,False,p,1.5,,1,,1\nc2,a,,False,,1.5,,1,False,1\nc3,c,2021-03-01 20:00:00,False,p,1.5,2021-03-01 08:00:00,1,,1\nc3,d,2021-03-01 20:00:00,True,p,1.5,,2,True,1\nc3,c,2021-03-01 23:00:00,False,q,2.5,,,,a\nc3,b,2021-03-01 23:00:00,False,,1.5,,2,False,a\n";
  const std::string f28 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-03 10:00:00,,q,2.5,,1,False,b\nc0,a,2021-03-03 10:00:00,True,r,,2021-03-02 09:30:00,,False,1\nc1,b,2021-03-06 18:00:00,False,p,,,,True,a\nc2,d,2021-03-07 04:00:00,True,p,,2021-03-02 09:30:00,3,,b\nc2,c,2021-03-08 10:00:00,False,q,3.0,2021-03-02 09:30:00,2,True,b\nc2,c,,,r,2.5,2021-03-01 08:00:00,,True,1\nc2,b,2021-03-08 20:00:00,,r,3.0,2021-03-02 09:30:00,2,False,b\n";
  const std::string f29 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,b,,7,q,3.0,2021-03-02 09:30:00,2,False,a\nc0,b,,18446744073709551616,r,-1.0,,,True,b\nc1,d,2021-03-01 08:11:00,18446744073709551616,p,3.0,2021-03-02 09:30:00,7,,b\nc2,d,2021-03-01 08:12:00,18446744073709551616,q,3.0,2021-03-01 08:00:00,2,,2\nc2,d,2021-03-01 08:12:00,18446744073709551621,p,2.5,,,False,2\nc3,d,2021-03-01 08:05:00,0,q,-1.0,2021-03-02 09:30:00,,,1\n";
  const std::vector<Case> cases = {
      {"a K1 k68000.csv", "k68000.csv", f0, "dataframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K1"}}, "T[nan,p,q,r] P[>nan,p,q,r#s nan,r>#k p,q>r]"},
      {"a K1 k68000.csv", "k68000.csv", f0, "lazyframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K1"}}, "T[None,p,q,r] P[>None,p,q,r#s None,r>#k p,q>r]"},
      {"a K2 k68000.csv", "k68000.csv", f0, "dataframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K2"}}, "T[1.5,2.5,3.0] P[1.5,2.5,3.0>#k >1.5,2.5,3.0#s]"},
      {"a K2 k68000.csv", "k68000.csv", f0, "lazyframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K2"}}, "T[1.5,2.5,3.0,None] P[1.5,2.5,3.0>#k >2.5,3.0,None#s]"},
      {"a K3 k68000.csv", "k68000.csv", f0, "dataframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K3"}}, "T[2021-03-01 08:00:00+00:00,2021-03-02 09:30:00+00:00,NaT] P[2021-03-01 08:00:00+00:00,2021-03-02 09:30:00+00:00,NaT>#k >2021-03-01 08:00:00+00:00,2021-03-02 09:30:00+00:00,NaT#s]"},
      {"a K3 k68000.csv", "k68000.csv", f0, "lazyframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K3"}}, "T[2021-03-01 08:00:00+00:00,2021-03-02 09:30:00+00:00,None] P[2021-03-01 08:00:00+00:00,2021-03-02 09:30:00+00:00,None>#k >2021-03-01 08:00:00+00:00,2021-03-02 09:30:00+00:00,None#s]"},
      {"a K4 k68000.csv", "k68000.csv", f0, "dataframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K4"}}, "T[2.0,3.0,nan,nan] P[2.0,3.0,nan>#k >2.0,3.0,nan#s]"},
      {"a K4 k68000.csv", "k68000.csv", f0, "lazyframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K4"}}, "T[2.0,3.0,None] P[3.0,None>#k >2.0,3.0,None#s]"},
      {"a K5 k68000.csv", "k68000.csv", f0, "dataframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K5"}}, "T[False,None,True] P[>False,None,True#s False,None,True>#k True>False]"},
      {"a K5 k68000.csv", "k68000.csv", f0, "lazyframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K5"}}, "T[False,None,True] P[>False,None,True#s False,None,True>#k True>False]"},
      {"a K6 k68000.csv", "k68000.csv", f0, "dataframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K6"}}, "T[1,a,b] P[1,a>#k >1,a,b#s b>a]"},
      {"a K6 k68000.csv", "k68000.csv", f0, "lazyframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K6"}}, "T[1,a,b] P[1,a>#k >1,a,b#s b>a]"},
      {"a K1 k68001.csv", "k68001.csv", f1, "dataframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K1"}}, "T[r,s] P[>r,s#s r,s>#k]"},
      {"a K1 k68001.csv", "k68001.csv", f1, "lazyframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K1"}}, "T[r,s] P[>r,s#s r,s>#k]"},
      {"a K2 k68001.csv", "k68001.csv", f1, "dataframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K2"}}, "T[1.5,2.5,3.0,nan,nan] P[1.5,3.0,nan>#k 2.5>1.5 >2.5,nan#s]"},
      {"a K2 k68001.csv", "k68001.csv", f1, "lazyframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K2"}}, "T[1.5,2.5,3.0,None] P[1.5,3.0,None>#k 2.5>1.5 >2.5,None#s]"},
      {"a K3 k68001.csv", "k68001.csv", f1, "dataframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K3"}}, "T[2021-03-01 08:00:00+00:00,2021-03-02 09:30:00+00:00,2021-03-03 10:00:00+00:00] P[2021-03-01 08:00:00+00:00,2021-03-02 09:30:00+00:00,2021-03-03 10:00:00+00:00>#k >2021-03-01 08:00:00+00:00,2021-03-02 09:30:00+00:00#s]"},
      {"a K3 k68001.csv", "k68001.csv", f1, "lazyframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K3"}}, "T[2021-03-01 08:00:00+00:00,2021-03-02 09:30:00+00:00,2021-03-03 10:00:00+00:00] P[2021-03-01 08:00:00+00:00,2021-03-02 09:30:00+00:00,2021-03-03 10:00:00+00:00>#k >2021-03-01 08:00:00+00:00,2021-03-02 09:30:00+00:00#s]"},
      {"a K4 k68001.csv", "k68001.csv", f1, "dataframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K4"}}, "T[1.0,3.0,7.0,nan,nan] P[1.0,7.0,nan>#k >3.0,7.0,nan#s]"},
      {"a K4 k68001.csv", "k68001.csv", f1, "lazyframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K4"}}, "T[1.0,3.0,7.0,None] P[1.0,7.0,None>#k >3.0,7.0,None#s]"},
      {"a K5 k68001.csv", "k68001.csv", f1, "dataframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K5"}}, "T[False,True] P[>False,True#s False>#k]"},
      {"a K5 k68001.csv", "k68001.csv", f1, "lazyframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K5"}}, "T[False,None,True] P[>False,True#s False,None>#k]"},
      {"a K6 k68001.csv", "k68001.csv", f1, "dataframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K6"}}, "T[2,a,b] P[2,a,b>#k >2,a#s a>2]"},
      {"a K6 k68001.csv", "k68001.csv", f1, "lazyframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K6"}}, "T[2,None,a,b] P[2,None,b>#k >2,a#s a>2]"},
      {"a K6 k68002.csv", "k68002.csv", f2, "dataframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K6"}}, "T[1,a,b] P[1,a,b>#k >1,a,b#s]"},
      {"a K6 k68002.csv", "k68002.csv", f2, "lazyframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K6"}}, "T[1,None,a,b] P[1,None,a,b>#k >1,None,a,b#s]"},
      {"a K5 k68005.csv", "k68005.csv", f3, "dataframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K5"}}, "T[False,True] P[>False,True#s False,True>#k]"},
      {"a K5 k68005.csv", "k68005.csv", f3, "lazyframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K5"}}, "T[False,True] P[>False,True#s False,True>#k]"},
      {"a K1 k68006.csv", "k68006.csv", f4, "dataframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K1"}}, "T[q,r] P[>r#s q,r>#k]"},
      {"a K1 k68006.csv", "k68006.csv", f4, "lazyframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K1"}}, "T[q,r] P[>r#s q,r>#k]"},
      {"a K2 k68007.csv", "k68007.csv", f5, "dataframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K2"}}, "T[2.5,3.0] P[2.5,3.0>#k >2.5,3.0#s]"},
      {"a K2 k68007.csv", "k68007.csv", f5, "lazyframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K2"}}, "T[2.5,3.0] P[2.5,3.0>#k >2.5,3.0#s]"},
      {"a K4 k68007.csv", "k68007.csv", f5, "dataframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K4"}}, "T[1,3] P[1,3>#k >1,3#s]"},
      {"a K4 k68007.csv", "k68007.csv", f5, "lazyframe", "a", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K4"}}, "T[1,3] P[1,3>#k >1,3#s]"},
      {"i K1 k68000.csv", "k68000.csv", f0, "dataframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K1"}}, "error: TypeError: '<' not supported between instances of 'float' and 'str'"},
      {"i K2 k68000.csv", "k68000.csv", f0, "dataframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K2"}}, "error: TypeError: can only concatenate str (not \"float\") to str"},
      {"i K3 k68000.csv", "k68000.csv", f0, "dataframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K3"}}, "error: TypeError: can only concatenate str (not \"Timestamp\") to str"},
      {"i K5 k68000.csv", "k68000.csv", f0, "dataframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K5"}}, "error: TypeError: can only concatenate str (not \"bool\") to str"},
      {"i K6 k68000.csv", "k68000.csv", f0, "dataframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K6"}}, "->(X(*(1,tau),b,tau),X(a,tau))"},
      {"i K6 k68000.csv", "k68000.csv", f0, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K6"}}, "->(X(*(1,tau),b,tau),X(a,tau))"},
      {"i K2 k68001.csv", "k68001.csv", f1, "dataframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K2"}}, "error: TypeError: can only concatenate str (not \"float\") to str"},
      {"i K3 k68001.csv", "k68001.csv", f1, "dataframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K3"}}, "error: TypeError: can only concatenate str (not \"Timestamp\") to str"},
      {"i K3 k68001.csv", "k68001.csv", f1, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K3"}}, "error: TypeError: can only concatenate str (not \"datetime.datetime\") to str"},
      {"i K4 k68001.csv", "k68001.csv", f1, "dataframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K4"}}, "error: TypeError: can only concatenate str (not \"float\") to str"},
      {"i K5 k68001.csv", "k68001.csv", f1, "dataframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K5"}}, "error: TypeError: can only concatenate str (not \"bool\") to str"},
      {"i K6 k68001.csv", "k68001.csv", f1, "dataframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K6"}}, "error: TypeError: '<' not supported between instances of 'float' and 'str'"},
      {"i K1 k68002.csv", "k68002.csv", f2, "dataframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K1"}}, "error: TypeError: '<' not supported between instances of 'float' and 'str'"},
      {"i K1 k68002.csv", "k68002.csv", f2, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K1"}}, "error: TypeError: '<' not supported between instances of 'NoneType' and 'str'"},
      {"i K6 k68002.csv", "k68002.csv", f2, "dataframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K6"}}, "error: TypeError: '<' not supported between instances of 'float' and 'str'"},
      {"i K1 k68003.csv", "k68003.csv", f6, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K1"}}, "error: TypeError: '<' not supported between instances of 'NoneType' and 'str'"},
      {"i K4 k68003.csv", "k68003.csv", f6, "dataframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K4"}}, "error: TypeError: can only concatenate str (not \"float\") to str"},
      {"i K5 k68005.csv", "k68005.csv", f3, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K5"}}, "error: TypeError: can only concatenate str (not \"bool\") to str"},
      {"i K6 k68005.csv", "k68005.csv", f3, "dataframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K6"}}, "X(*(a,tau),1)"},
      {"i K6 k68005.csv", "k68005.csv", f3, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K6"}}, "X(*(a,tau),1)"},
      {"i K1 k68006.csv", "k68006.csv", f4, "dataframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K1"}}, "*(->(r,X(q,tau)),tau)"},
      {"i K1 k68006.csv", "k68006.csv", f4, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K1"}}, "*(->(r,X(q,tau)),tau)"},
      {"i K3 k68006.csv", "k68006.csv", f4, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K3"}}, "error: TypeError: can only concatenate str (not \"datetime.datetime\") to str"},
      {"i K2 k68007.csv", "k68007.csv", f5, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K2"}}, "error: TypeError: can only concatenate str (not \"float\") to str"},
      {"i K4 k68007.csv", "k68007.csv", f5, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K4"}}, "error: TypeError: can only concatenate str (not \"int\") to str"},
      {"i K5 k68007.csv", "k68007.csv", f5, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K5"}}, "error: TypeError: can only concatenate str (not \"bool\") to str"},
      {"i K2 k68008.csv", "k68008.csv", f7, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K2"}}, "error: TypeError: '<' not supported between instances of 'NoneType' and 'float'"},
      {"i K4 k68008.csv", "k68008.csv", f7, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K4"}}, "error: TypeError: '<' not supported between instances of 'NoneType' and 'float'"},
      {"i K6 k68008.csv", "k68008.csv", f7, "dataframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K6"}}, "+(->(X(*(1,tau),tau),X(->(a,X(*(b,tau),tau)),tau)),X(*(2,tau),tau))"},
      {"i K6 k68008.csv", "k68008.csv", f7, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K6"}}, "+(->(X(*(1,tau),tau),X(->(a,X(*(b,tau),tau)),tau)),X(*(2,tau),tau))"},
      {"i K2 k68010.csv", "k68010.csv", f8, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K2"}}, "error: TypeError: can only concatenate str (not \"float\") to str"},
      {"i K4 k68011.csv", "k68011.csv", f9, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K4"}}, "error: TypeError: can only concatenate str (not \"int\") to str"},
      {"i K6 s68501.csv", "s68501.csv", f10, "dataframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K6"}}, "*(->(a,1),tau)"},
      {"i K6 s68501.csv", "s68501.csv", f10, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K6"}}, "*(->(a,1),tau)"},
      {"i K2 s68502.csv", "s68502.csv", f11, "dataframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K2"}}, "error: TypeError: __str__ returned non-string (type float)"},
      {"i K2 s68502.csv", "s68502.csv", f11, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K2"}}, "error: TypeError: __str__ returned non-string (type float)"},
      {"i K4 s68502.csv", "s68502.csv", f11, "dataframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K4"}}, "error: TypeError: __str__ returned non-string (type int)"},
      {"i K4 s68502.csv", "s68502.csv", f11, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K4"}}, "error: TypeError: __str__ returned non-string (type int)"},
      {"i K5 s68502.csv", "s68502.csv", f11, "dataframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K5"}}, "error: TypeError: __str__ returned non-string (type bool)"},
      {"i K5 s68502.csv", "s68502.csv", f11, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K5"}}, "error: TypeError: __str__ returned non-string (type bool)"},
      {"i K1 s68508.csv", "s68508.csv", f12, "dataframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K1"}}, "+(*(q,tau),X(*(p,tau),tau),X(*(r,tau),tau))"},
      {"i K1 s68508.csv", "s68508.csv", f12, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K1"}}, "+(*(q,tau),X(*(p,tau),tau),X(*(r,tau),tau))"},
      {"i K5 s68508.csv", "s68508.csv", f12, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K5"}}, "+(*(True,tau),False)"},
      {"i K5 s68509.csv", "s68509.csv", f13, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K5"}}, "->(False,X(True,tau))"},
      {"i K1 s68510.csv", "s68510.csv", f14, "dataframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K1"}}, "X(->(p,X(*(r,tau),tau)),q)"},
      {"i K1 s68510.csv", "s68510.csv", f14, "lazyframe", "i", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"inductive_activity_key", "K1"}}, "X(->(p,X(*(r,tau),tau)),q)"},
      {"i K2 s68502.csv (renamed header)", "s68502.csv", f15, "dataframe", "i", {{"case_column", "cid"}, {"activity_column", "act"}, {"timestamp_column", "ts"}, {"inductive_activity_key", "K2"}}, "error: TypeError: __str__ returned non-string (type float)"},
      {"i K2 s68502.csv (renamed header)", "s68502.csv", f15, "lazyframe", "i", {{"case_column", "cid"}, {"activity_column", "act"}, {"timestamp_column", "ts"}, {"inductive_activity_key", "K2"}}, "error: TypeError: __str__ returned non-string (type float)"},
      {"i K4 s68502.csv (renamed header)", "s68502.csv", f15, "dataframe", "i", {{"case_column", "cid"}, {"activity_column", "act"}, {"timestamp_column", "ts"}, {"inductive_activity_key", "K4"}}, "error: TypeError: __str__ returned non-string (type int)"},
      {"i K4 s68502.csv (renamed header)", "s68502.csv", f15, "lazyframe", "i", {{"case_column", "cid"}, {"activity_column", "act"}, {"timestamp_column", "ts"}, {"inductive_activity_key", "K4"}}, "error: TypeError: __str__ returned non-string (type int)"},
      {"i K5 s68502.csv (renamed header)", "s68502.csv", f15, "dataframe", "i", {{"case_column", "cid"}, {"activity_column", "act"}, {"timestamp_column", "ts"}, {"inductive_activity_key", "K5"}}, "error: TypeError: __str__ returned non-string (type bool)"},
      {"i K5 s68502.csv (renamed header)", "s68502.csv", f15, "lazyframe", "i", {{"case_column", "cid"}, {"activity_column", "act"}, {"timestamp_column", "ts"}, {"inductive_activity_key", "K5"}}, "error: TypeError: __str__ returned non-string (type bool)"},
      {"ls K1 k68000.csv", "k68000.csv", f0, "dataframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K1"}}, "error: KeyError: nan"},
      {"ls K1 k68000.csv", "k68000.csv", f0, "lazyframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K1"}}, "LS equivalence[p>r,q>None,q>r] always_after[p>r,q>None,q>r] always_before[] never_together[None>p,p>None,p>q,q>p] directly_follows[p>r,q>r] activ_freq[p=0/1,q=0/1,r=0/1]"},
      {"ls K2 k68000.csv", "k68000.csv", f0, "dataframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K2"}}, "error: KeyError: nan"},
      {"ls K2 k68000.csv", "k68000.csv", f0, "lazyframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K2"}}, "LS equivalence[] always_after[] always_before[1.5>None] never_together[1.5>2.5,1.5>3.0,2.5>1.5,2.5>3.0,2.5>None,3.0>1.5,3.0>2.5,None>2.5] directly_follows[] activ_freq[1.5=0/1,2.5=0/1,3.0=0/1]"},
      {"ls K3 k68000.csv", "k68000.csv", f0, "dataframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K3"}}, "error: KeyError: NaT"},
      {"ls K3 k68000.csv", "k68000.csv", f0, "lazyframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K3"}}, "LS equivalence[] always_after[] always_before[] never_together[2021-03-01 08:00:00+00:00>2021-03-02 09:30:00+00:00,2021-03-02 09:30:00+00:00>2021-03-01 08:00:00+00:00,2021-03-02 09:30:00+00:00>None,None>2021-03-02 09:30:00+00:00] directly_follows[] activ_freq[2021-03-01 08:00:00+00:00=0/1,2021-03-02 09:30:00+00:00=0/1]"},
      {"ls K4 k68000.csv", "k68000.csv", f0, "dataframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K4"}}, "error: KeyError: nan"},
      {"ls K4 k68000.csv", "k68000.csv", f0, "lazyframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K4"}}, "LS equivalence[] always_after[2.0>None] always_before[] never_together[2.0>3.0,3.0>2.0,3.0>None,None>3.0] directly_follows[2.0>None] activ_freq[2.0=0/1,3.0=0/1]"},
      {"ls K5 k68000.csv", "k68000.csv", f0, "dataframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K5"}}, "error: KeyError: nan"},
      {"ls K5 k68000.csv", "k68000.csv", f0, "lazyframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K5"}}, "LS equivalence[] always_after[] always_before[] never_together[] directly_follows[] activ_freq[False=0/1,True=0/1]"},
      {"ls K6 k68000.csv", "k68000.csv", f0, "dataframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K6"}}, "LS equivalence[b>a] always_after[b>a] always_before[] never_together[1>b,b>1] directly_follows[b>a] activ_freq[1=0/1/2,a=0/1,b=0/1]"},
      {"ls K6 k68000.csv", "k68000.csv", f0, "lazyframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K6"}}, "LS equivalence[b>a] always_after[b>a] always_before[] never_together[1>b,b>1] directly_follows[b>a] activ_freq[1=0/1/2,a=0/1,b=0/1]"},
      {"ls K1 k68001.csv", "k68001.csv", f1, "dataframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K1"}}, "error: KeyError: nan"},
      {"ls K1 k68001.csv", "k68001.csv", f1, "lazyframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K1"}}, "LS equivalence[] always_after[None>s] always_before[None>s] never_together[None>r,r>None] directly_follows[None>s] activ_freq[r=0/1/2,s=0/1/2]"},
      {"ls K2 k68001.csv", "k68001.csv", f1, "dataframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K2"}}, "error: KeyError: nan"},
      {"ls K2 k68001.csv", "k68001.csv", f1, "lazyframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K2"}}, "LS equivalence[1.5>2.5,3.0>2.5,3.0>None] always_after[] always_before[1.5>2.5,3.0>2.5,3.0>None] never_together[1.5>3.0,1.5>None,3.0>1.5,None>1.5] directly_follows[] activ_freq[1.5=0/1,2.5=0/1,3.0=0/1]"},
      {"ls K3 k68001.csv", "k68001.csv", f1, "dataframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K3"}}, "LS equivalence[] always_after[] always_before[2021-03-03 10:00:00+00:00>2021-03-02 09:30:00+00:00] never_together[2021-03-01 08:00:00+00:00>2021-03-03 10:00:00+00:00,2021-03-03 10:00:00+00:00>2021-03-01 08:00:00+00:00] directly_follows[] activ_freq[2021-03-01 08:00:00+00:00=0/1/2,2021-03-02 09:30:00+00:00=0/1/2,2021-03-03 10:00:00+00:00=0/1]"},
      {"ls K3 k68001.csv", "k68001.csv", f1, "lazyframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K3"}}, "LS equivalence[] always_after[] always_before[2021-03-03 10:00:00+00:00>2021-03-02 09:30:00+00:00] never_together[2021-03-01 08:00:00+00:00>2021-03-03 10:00:00+00:00,2021-03-03 10:00:00+00:00>2021-03-01 08:00:00+00:00] directly_follows[] activ_freq[2021-03-01 08:00:00+00:00=0/1/2,2021-03-02 09:30:00+00:00=0/1/2,2021-03-03 10:00:00+00:00=0/1]"},
      {"ls K4 k68001.csv", "k68001.csv", f1, "dataframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K4"}}, "error: KeyError: nan"},
      {"ls K4 k68001.csv", "k68001.csv", f1, "lazyframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K4"}}, "LS equivalence[1.0>3.0,1.0>None,3.0>1.0,3.0>None] always_after[3.0>1.0,3.0>None] always_before[1.0>3.0,1.0>None] never_together[1.0>7.0,3.0>7.0,7.0>1.0,7.0>3.0,7.0>None,None>7.0] directly_follows[3.0>None] activ_freq[1.0=0/1,3.0=0/1,7.0=0/2]"},
      {"ls K5 k68001.csv", "k68001.csv", f1, "dataframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K5"}}, "error: KeyError: nan"},
      {"ls K5 k68001.csv", "k68001.csv", f1, "lazyframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K5"}}, "LS equivalence[None>False,None>True,True>False,True>None] always_after[True>False,True>None] always_before[None>False,None>True] never_together[] directly_follows[True>False] activ_freq[False=1/2,True=0/1]"},
      {"ls K6 k68001.csv", "k68001.csv", f1, "dataframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K6"}}, "error: KeyError: nan"},
      {"ls K6 k68001.csv", "k68001.csv", f1, "lazyframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K6"}}, "LS equivalence[None>a] always_after[] always_before[None>a] never_together[2>None,None>2,None>b,a>b,b>None,b>a] directly_follows[] activ_freq[2=0/1,a=0/1,b=0/2]"},
      {"ls K3 k68002.csv", "k68002.csv", f2, "dataframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K3"}}, "error: KeyError: NaT"},
      {"ls K6 k68002.csv", "k68002.csv", f2, "dataframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K6"}}, "error: KeyError: nan"},
      {"ls K6 k68002.csv", "k68002.csv", f2, "lazyframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K6"}}, "LS equivalence[] always_after[] always_before[] never_together[1>a,None>a,a>1,a>None,a>b,b>a] directly_follows[] activ_freq[1=0/1/2,a=0/1,b=0/1]"},
      {"ls K5 k68005.csv", "k68005.csv", f3, "dataframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K5"}}, "LS equivalence[] always_after[] always_before[] never_together[False>True,True>False] directly_follows[] activ_freq[False=0/1/2,True=0/1]"},
      {"ls K5 k68005.csv", "k68005.csv", f3, "lazyframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K5"}}, "LS equivalence[] always_after[] always_before[] never_together[False>True,True>False] directly_follows[] activ_freq[False=0/1/2,True=0/1]"},
      {"ls K1 k68006.csv", "k68006.csv", f4, "dataframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K1"}}, "LS equivalence[q>r] always_after[] always_before[] never_together[] directly_follows[] activ_freq[q=0/2,r=1/2]"},
      {"ls K1 k68006.csv", "k68006.csv", f4, "lazyframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K1"}}, "LS equivalence[q>r] always_after[] always_before[] never_together[] directly_follows[] activ_freq[q=0/2,r=1/2]"},
      {"ls K3 k68006.csv", "k68006.csv", f4, "dataframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K3"}}, "LS equivalence[] always_after[] always_before[] never_together[] directly_follows[] activ_freq[2021-03-01 08:00:00+00:00=0/3,2021-03-02 09:30:00+00:00=1]"},
      {"ls K2 k68007.csv", "k68007.csv", f5, "dataframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K2"}}, "LS equivalence[] always_after[] always_before[] never_together[2.5>3.0,3.0>2.5] directly_follows[] activ_freq[2.5=0/1,3.0=0/1]"},
      {"ls K2 k68007.csv", "k68007.csv", f5, "lazyframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K2"}}, "LS equivalence[] always_after[] always_before[] never_together[2.5>3.0,3.0>2.5] directly_follows[] activ_freq[2.5=0/1,3.0=0/1]"},
      {"ls K4 k68007.csv", "k68007.csv", f5, "dataframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K4"}}, "LS equivalence[] always_after[] always_before[] never_together[1>3,3>1] directly_follows[] activ_freq[1=0/1,3=0/1]"},
      {"ls K4 k68007.csv", "k68007.csv", f5, "lazyframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K4"}}, "LS equivalence[] always_after[] always_before[] never_together[1>3,3>1] directly_follows[] activ_freq[1=0/1,3=0/1]"},
      {"ls K3 s68502.csv", "s68502.csv", f11, "dataframe", "ls", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K3"}}, "error: KeyError: nan"},
      {"ls K3 s68502.csv (renamed header)", "s68502.csv", f15, "dataframe", "ls", {{"case_column", "cid"}, {"activity_column", "act"}, {"timestamp_column", "ts"}, {"log_skeleton_activity_attribute", "K3"}}, "error: KeyError: nan"},
      {"tp K1 k68000.csv", "k68000.csv", f0, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}}, "p>r:60.0000/0.0000;q>r:3600.0000/0.0000"},
      {"tp K1 k68000.csv", "k68000.csv", f0, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}}, "p>r:60.0000/0.0000;q>r:3600.0000/0.0000"},
      {"tp K2 k68000.csv", "k68000.csv", f0, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K2"}}, ""},
      {"tp K2 k68000.csv", "k68000.csv", f0, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K2"}}, ""},
      {"tp K3 k68000.csv", "k68000.csv", f0, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K3"}}, ""},
      {"tp K3 k68000.csv", "k68000.csv", f0, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K3"}}, ""},
      {"tp K4 k68000.csv", "k68000.csv", f0, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K4"}}, ""},
      {"tp K4 k68000.csv", "k68000.csv", f0, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K4"}}, ""},
      {"tp K5 k68000.csv", "k68000.csv", f0, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K5"}}, "True>False:3600.0000/0.0000"},
      {"tp K5 k68000.csv", "k68000.csv", f0, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K5"}}, "True>False:3600.0000/0.0000"},
      {"tp K6 k68000.csv", "k68000.csv", f0, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K6"}}, "1>1:3600.0000/0.0000;1>a:2100.0000/2545.5844;b>a:60.0000/0.0000"},
      {"tp K6 k68000.csv", "k68000.csv", f0, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K6"}}, "1>1:3600.0000/0.0000;1>a:2100.0000/2545.5844;b>a:60.0000/0.0000"},
      {"tp K1 k68001.csv", "k68001.csv", f1, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}}, "r>r:300.0000/0.0000;s>r:60.0000/0.0000;s>s:600.0000/0.0000"},
      {"tp K1 k68001.csv", "k68001.csv", f1, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}}, "r>r:300.0000/0.0000;s>r:60.0000/0.0000;s>s:600.0000/0.0000"},
      {"tp K2 k68001.csv", "k68001.csv", f1, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K2"}}, "2.5>1.5:60.0000/0.0000;2.5>3.0:600.0000/0.0000"},
      {"tp K2 k68001.csv", "k68001.csv", f1, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K2"}}, "2.5>1.5:60.0000/0.0000;2.5>3.0:600.0000/0.0000"},
      {"tp K3 k68001.csv", "k68001.csv", f1, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K3"}}, "2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00:60.0000/0.0000;2021-03-01 08:00:00+00:00>2021-03-02 09:30:00+00:00:300.0000/0.0000;2021-03-02 09:30:00+00:00>2021-03-02 09:30:00+00:00:300.0000/0.0000;2021-03-02 09:30:00+00:00>2021-03-03 10:00:00+00:00:450.0000/212.1320"},
      {"tp K3 k68001.csv", "k68001.csv", f1, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K3"}}, "2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00:60.0000/0.0000;2021-03-01 08:00:00+00:00>2021-03-02 09:30:00+00:00:300.0000/0.0000;2021-03-02 09:30:00+00:00>2021-03-02 09:30:00+00:00:300.0000/0.0000;2021-03-02 09:30:00+00:00>2021-03-03 10:00:00+00:00:450.0000/212.1320"},
      {"tp K4 k68001.csv", "k68001.csv", f1, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K4"}}, "3.0>1.0:600.0000/0.0000;7.0>7.0:300.0000/0.0000"},
      {"tp K4 k68001.csv", "k68001.csv", f1, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K4"}}, "3.0>1.0:600.0000/0.0000;7.0>7.0:300.0000/0.0000"},
      {"tp K5 k68001.csv", "k68001.csv", f1, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K5"}}, "False>False:180.0000/169.7056;True>False:300.0000/0.0000"},
      {"tp K5 k68001.csv", "k68001.csv", f1, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K5"}}, "False>False:180.0000/169.7056;True>False:300.0000/0.0000"},
      {"tp K6 k68001.csv", "k68001.csv", f1, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K6"}}, "2>b:450.0000/212.1320;a>2:60.0000/0.0000;b>b:300.0000/0.0000"},
      {"tp K6 k68001.csv", "k68001.csv", f1, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K6"}}, "2>b:450.0000/212.1320;a>2:60.0000/0.0000;b>b:300.0000/0.0000"},
      {"tp K1 k68002.csv", "k68002.csv", f2, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}}, "p>p:3600.0000/0.0000;p>r:2100.0000/2545.5844;r>p:3630.0000/42.4264"},
      {"tp K1 k68002.csv", "k68002.csv", f2, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}}, "p>p:3600.0000/0.0000;p>r:2100.0000/2545.5844;r>p:3630.0000/42.4264"},
      {"tp K2 k68002.csv", "k68002.csv", f2, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K2"}}, "1.5>1.5:60.0000/0.0000;1.5>2.5:1860.0000/2545.5844;1.5>3.0:3600.0000/0.0000;2.5>1.5:2130.0000/2078.7496;2.5>2.5:3600.0000/0.0000"},
      {"tp K2 k68002.csv", "k68002.csv", f2, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K2"}}, "1.5>1.5:60.0000/0.0000;1.5>2.5:1860.0000/2545.5844;1.5>3.0:3600.0000/0.0000;2.5>1.5:2130.0000/2078.7496;2.5>2.5:3600.0000/0.0000"},
      {"tp K3 k68002.csv", "k68002.csv", f2, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K3"}}, "2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00:3960.0000/0.0000;2021-03-01 08:00:00+00:00>2021-03-02 09:30:00+00:00:3750.0000/212.1320;2021-03-02 09:30:00+00:00>2021-03-01 08:00:00+00:00:1340.0000/1962.9570;2021-03-02 09:30:00+00:00>2021-03-02 09:30:00+00:00:300.0000/0.0000"},
      {"tp K3 k68002.csv", "k68002.csv", f2, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K3"}}, "2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00:3960.0000/0.0000;2021-03-01 08:00:00+00:00>2021-03-02 09:30:00+00:00:3750.0000/212.1320;2021-03-02 09:30:00+00:00>2021-03-01 08:00:00+00:00:1340.0000/1962.9570;2021-03-02 09:30:00+00:00>2021-03-02 09:30:00+00:00:300.0000/0.0000"},
      {"tp K4 k68002.csv", "k68002.csv", f2, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K4"}}, "1.0>1.0:3600.0000/0.0000;1.0>2.0:2100.0000/2545.5844;2.0>1.0:3720.0000/0.0000;2.0>3.0:90.0000/42.4264;3.0>1.0:3630.0000/42.4264;3.0>3.0:60.0000/0.0000"},
      {"tp K4 k68002.csv", "k68002.csv", f2, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K4"}}, "1.0>1.0:3600.0000/0.0000;1.0>2.0:2100.0000/2545.5844;2.0>1.0:3720.0000/0.0000;2.0>3.0:90.0000/42.4264;3.0>1.0:3630.0000/42.4264;3.0>3.0:60.0000/0.0000"},
      {"tp K5 k68002.csv", "k68002.csv", f2, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K5"}}, "False>False:1950.0000/2333.4524;True>False:2964.0000/1628.0909"},
      {"tp K5 k68002.csv", "k68002.csv", f2, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K5"}}, "False>False:1950.0000/2333.4524;True>False:2964.0000/1628.0909"},
      {"tp K6 k68002.csv", "k68002.csv", f2, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K6"}}, "1>1:180.0000/169.7056;1>b:3600.0000/0.0000;b>1:3750.0000/212.1320"},
      {"tp K6 k68002.csv", "k68002.csv", f2, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K6"}}, "1>1:180.0000/169.7056;1>b:3600.0000/0.0000;b>1:3750.0000/212.1320"},
      {"tp K1 k68003.csv", "k68003.csv", f6, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}}, "p>r:450.0000/212.1320;q>q:3600.0000/0.0000;r>p:300.0000/0.0000;r>r:600.0000/300.0000"},
      {"tp K1 k68003.csv", "k68003.csv", f6, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}}, "p>r:450.0000/212.1320;q>q:3600.0000/0.0000;r>p:300.0000/0.0000;r>r:600.0000/300.0000"},
      {"tp K2 k68003.csv", "k68003.csv", f6, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K2"}}, "2.5>2.5:900.0000/0.0000"},
      {"tp K2 k68003.csv", "k68003.csv", f6, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K2"}}, "2.5>2.5:900.0000/0.0000"},
      {"tp K4 k68003.csv", "k68003.csv", f6, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K4"}}, "1.0>3.0:1700.0000/1652.2712;3.0>3.0:300.0000/0.0000"},
      {"tp K4 k68003.csv", "k68003.csv", f6, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K4"}}, "1.0>3.0:1700.0000/1652.2712;3.0>3.0:300.0000/0.0000"},
      {"tp K6 k68003.csv", "k68003.csv", f6, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K6"}}, "1>b:3600.0000/0.0000"},
      {"tp K6 k68003.csv", "k68003.csv", f6, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K6"}}, "1>b:3600.0000/0.0000"},
      {"tp K1 k68004.csv", "k68004.csv", f16, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}}, ""},
      {"tp K1 k68004.csv", "k68004.csv", f16, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}}, ""},
      {"tp K2 k68004.csv", "k68004.csv", f16, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K2"}}, "1.5>1.5:60.0000/0.0000;1.5>2.5:300.0000/0.0000;3.0>1.5:60.0000/0.0000;3.0>2.5:360.0000/0.0000"},
      {"tp K2 k68004.csv", "k68004.csv", f16, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K2"}}, "1.5>1.5:60.0000/0.0000;1.5>2.5:300.0000/0.0000;3.0>1.5:60.0000/0.0000;3.0>2.5:360.0000/0.0000"},
      {"tp K4 k68004.csv", "k68004.csv", f16, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K4"}}, "3.0>3.0:240.0000/158.7451"},
      {"tp K4 k68004.csv", "k68004.csv", f16, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K4"}}, "3.0>3.0:240.0000/158.7451"},
      {"tp K1 k68005.csv", "k68005.csv", f3, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}}, ""},
      {"tp K1 k68005.csv", "k68005.csv", f3, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}}, ""},
      {"tp K2 k68005.csv", "k68005.csv", f3, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K2"}}, ""},
      {"tp K2 k68005.csv", "k68005.csv", f3, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K2"}}, ""},
      {"tp K5 k68005.csv", "k68005.csv", f3, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K5"}}, "False>False:300.0000/0.0000"},
      {"tp K5 k68005.csv", "k68005.csv", f3, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K5"}}, "False>False:300.0000/0.0000"},
      {"tp K6 k68005.csv", "k68005.csv", f3, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K6"}}, "a>a:300.0000/0.0000"},
      {"tp K6 k68005.csv", "k68005.csv", f3, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K6"}}, "a>a:300.0000/0.0000"},
      {"tp K1 k68006.csv", "k68006.csv", f4, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}}, "q>q:3660.0000/0.0000;q>r:60.0000/0.0000;r>q:2620.0000/2017.2258;r>r:360.0000/0.0000"},
      {"tp K1 k68006.csv", "k68006.csv", f4, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}}, "q>q:3660.0000/0.0000;q>r:60.0000/0.0000;r>q:2620.0000/2017.2258;r>r:360.0000/0.0000"},
      {"tp K5 k68006.csv", "k68006.csv", f4, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K5"}}, "True>True:60.0000/0.0000"},
      {"tp K5 k68006.csv", "k68006.csv", f4, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K5"}}, "True>True:60.0000/0.0000"},
      {"tp K1 k68007.csv", "k68007.csv", f5, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}}, ""},
      {"tp K1 k68007.csv", "k68007.csv", f5, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}}, ""},
      {"tp K2 k68007.csv", "k68007.csv", f5, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K2"}}, ""},
      {"tp K2 k68007.csv", "k68007.csv", f5, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K2"}}, ""},
      {"tp K3 k68007.csv", "k68007.csv", f5, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K3"}}, ""},
      {"tp K3 k68007.csv", "k68007.csv", f5, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K3"}}, ""},
      {"tp K4 k68007.csv", "k68007.csv", f5, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K4"}}, ""},
      {"tp K4 k68007.csv", "k68007.csv", f5, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K4"}}, ""},
      {"tp K5 k68007.csv", "k68007.csv", f5, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K5"}}, ""},
      {"tp K5 k68007.csv", "k68007.csv", f5, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K5"}}, ""},
      {"tp K1 k68009.csv", "k68009.csv", f17, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}}, "r>q:140.0000/138.5641"},
      {"tp K1 k68009.csv", "k68009.csv", f17, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}}, "r>q:140.0000/138.5641"},
      {"tp K6 k68010.csv", "k68010.csv", f8, "dataframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K6"}}, "b>1:300.0000/0.0000"},
      {"tp K6 k68010.csv", "k68010.csv", f8, "lazyframe", "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K6"}}, "b>1:300.0000/0.0000"},
      {"h K1 k68000.csv", "k68000.csv", f0, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<' not supported between instances of 'float' and 'str'"},
      {"h K1 k68000.csv", "k68000.csv", f0, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<' not supported between instances of 'NoneType' and 'str'"},
      {"h K2 k68000.csv", "k68000.csv", f0, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K2"}, {"heuristic_variant", "plusplus"}}, "A[1.5,2.5,3.0,nan,nan,nan] E[nan>1.5:1.000000/1 nan>3.0:1.000000/1 nan>nan:1.000000/1]"},
      {"h K2 k68000.csv", "k68000.csv", f0, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K2"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<' not supported between instances of 'NoneType' and 'NoneType'"},
      {"h K3 k68000.csv", "k68000.csv", f0, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K3"}, {"heuristic_variant", "plusplus"}}, "A[2021-03-01 08:00:00+00:00,2021-03-02 09:30:00+00:00,NaT] E[2021-03-01 08:00:00+00:00>NaT:1.000000/1]"},
      {"h K3 k68000.csv", "k68000.csv", f0, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K3"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<' not supported between instances of 'NoneType' and 'datetime.datetime'"},
      {"h K4 k68000.csv", "k68000.csv", f0, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}, {"heuristic_variant", "plusplus"}}, "A[2.0,3.0,nan,nan,nan,nan] E[2.0>nan:1.000000/1 2.0>nan:1.000000/1 nan>2.0:1.000000/1]"},
      {"h K4 k68000.csv", "k68000.csv", f0, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<' not supported between instances of 'float' and 'NoneType'"},
      {"h K5 k68000.csv", "k68000.csv", f0, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K5"}, {"heuristic_variant", "plusplus"}}, "A[False,True,nan] E[False>nan:1.000000/1 True>False:1.000000/1]"},
      {"h K5 k68000.csv", "k68000.csv", f0, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K5"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<' not supported between instances of 'NoneType' and 'str'"},
      {"h K6 k68000.csv", "k68000.csv", f0, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K6"}, {"heuristic_variant", "plusplus"}}, "A[1,a,b] E[1>a:1.000000/1 b>a:1.000000/1]"},
      {"h K6 k68000.csv", "k68000.csv", f0, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K6"}, {"heuristic_variant", "plusplus"}}, "A[1,a,b] E[1>a:1.000000/1 b>a:1.000000/1]"},
      {"h K1 k68001.csv", "k68001.csv", f1, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<' not supported between instances of 'float' and 'str'"},
      {"h K1 k68001.csv", "k68001.csv", f1, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<' not supported between instances of 'NoneType' and 'str'"},
      {"h K2 k68001.csv", "k68001.csv", f1, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K2"}, {"heuristic_variant", "plusplus"}}, "A[1.5,2.5,3.0,nan,nan,nan] E[2.5>1.5:1.000000/1 2.5>nan:1.000000/1 nan>3.0:1.000000/1 nan>nan:1.000000/1]"},
      {"h K2 k68001.csv", "k68001.csv", f1, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K2"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<' not supported between instances of 'NoneType' and 'NoneType'"},
      {"h K3 k68001.csv", "k68001.csv", f1, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K3"}, {"heuristic_variant", "plusplus"}}, "A[2021-03-01 08:00:00+00:00,2021-03-02 09:30:00+00:00,2021-03-03 10:00:00+00:00] E[2021-03-01 08:00:00+00:00>2021-03-02 09:30:00+00:00:1.000000/1 2021-03-02 09:30:00+00:00>2021-03-03 10:00:00+00:00:1.000000/1]"},
      {"h K3 k68001.csv", "k68001.csv", f1, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K3"}, {"heuristic_variant", "plusplus"}}, "A[2021-03-01 08:00:00+00:00,2021-03-02 09:30:00+00:00,2021-03-03 10:00:00+00:00] E[2021-03-01 08:00:00+00:00>2021-03-02 09:30:00+00:00:1.000000/1 2021-03-02 09:30:00+00:00>2021-03-03 10:00:00+00:00:1.000000/1]"},
      {"h K4 k68001.csv", "k68001.csv", f1, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}, {"heuristic_variant", "plusplus"}}, "A[1.0,3.0,7.0,nan,nan,nan] E[3.0>nan:1.000000/1 nan>1.0:1.000000/1 nan>nan:1.000000/1]"},
      {"h K4 k68001.csv", "k68001.csv", f1, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<' not supported between instances of 'NoneType' and 'float'"},
      {"h K5 k68001.csv", "k68001.csv", f1, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K5"}, {"heuristic_variant", "plusplus"}}, "A[False,True,nan] E[False>nan:1.000000/1 True>False:1.000000/1]"},
      {"h K5 k68001.csv", "k68001.csv", f1, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K5"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<' not supported between instances of 'NoneType' and 'str'"},
      {"h K6 k68001.csv", "k68001.csv", f1, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K6"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<' not supported between instances of 'float' and 'str'"},
      {"h K6 k68001.csv", "k68001.csv", f1, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K6"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<' not supported between instances of 'NoneType' and 'str'"},
      {"h K3 k68002.csv", "k68002.csv", f2, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K3"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<' not supported between instances of 'datetime.datetime' and 'NoneType'"},
      {"h K6 k68002.csv", "k68002.csv", f2, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K6"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<' not supported between instances of 'float' and 'str'"},
      {"h K6 k68002.csv", "k68002.csv", f2, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K6"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<' not supported between instances of 'NoneType' and 'str'"},
      {"h K1 k68003.csv", "k68003.csv", f6, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}, {"heuristic_variant", "plusplus"}}, "A[nan,p,q,r] E[]"},
      {"h K1 k68003.csv", "k68003.csv", f6, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}, {"heuristic_variant", "plusplus"}}, "A[None,p,q,r] E[]"},
      {"h K6 k68004.csv", "k68004.csv", f16, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K6"}, {"heuristic_variant", "plusplus"}}, "A[1,b,nan] E[1>b:1.000000/1]"},
      {"h K3 k68005.csv", "k68005.csv", f3, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K3"}, {"heuristic_variant", "plusplus"}}, "A[2021-03-02 09:30:00+00:00,None] E[]"},
      {"h K4 k68005.csv", "k68005.csv", f3, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}, {"heuristic_variant", "plusplus"}}, "A[1.0,3.0,None] E[]"},
      {"h K5 k68005.csv", "k68005.csv", f3, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K5"}, {"heuristic_variant", "plusplus"}}, "A[False,True] E[]"},
      {"h K5 k68005.csv", "k68005.csv", f3, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K5"}, {"heuristic_variant", "plusplus"}}, "A[False,True] E[]"},
      {"h K1 k68006.csv", "k68006.csv", f4, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}, {"heuristic_variant", "plusplus"}}, "A[q,r] E[]"},
      {"h K1 k68006.csv", "k68006.csv", f4, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}, {"heuristic_variant", "plusplus"}}, "A[q,r] E[]"},
      {"h K1 k68007.csv", "k68007.csv", f5, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}, {"heuristic_variant", "plusplus"}}, "A[nan,q,r] E[]"},
      {"h K1 k68007.csv", "k68007.csv", f5, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}, {"heuristic_variant", "plusplus"}}, "A[None,q,r] E[]"},
      {"h K2 k68007.csv", "k68007.csv", f5, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K2"}, {"heuristic_variant", "plusplus"}}, "A[2.5,3.0] E[]"},
      {"h K2 k68007.csv", "k68007.csv", f5, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K2"}, {"heuristic_variant", "plusplus"}}, "A[2.5,3.0] E[]"},
      {"h K4 k68007.csv", "k68007.csv", f5, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}, {"heuristic_variant", "plusplus"}}, "A[1,3] E[]"},
      {"h K4 k68007.csv", "k68007.csv", f5, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}, {"heuristic_variant", "plusplus"}}, "A[1,3] E[]"},
      {"h K6 k68007.csv", "k68007.csv", f5, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K6"}, {"heuristic_variant", "plusplus"}}, "A[1.0,nan] E[]"},
      {"h K6 k68007.csv", "k68007.csv", f5, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K6"}, {"heuristic_variant", "plusplus"}}, "A[1.0,None] E[]"},
      {"h K6 k68011.csv", "k68011.csv", f9, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K6"}, {"heuristic_variant", "plusplus"}}, "A[1,None,a] E[1>a:1.000000/1]"},
      {"h K1 s68500.csv", "s68500.csv", f18, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"h K1 s68500.csv", "s68500.csv", f18, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'datetime.datetime' and 'bool'"},
      {"h K2 s68500.csv", "s68500.csv", f18, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K2"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"h K2 s68500.csv", "s68500.csv", f18, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K2"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'datetime.datetime' and 'bool'"},
      {"h K3 s68500.csv", "s68500.csv", f18, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K3"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"h K3 s68500.csv", "s68500.csv", f18, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K3"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'datetime.datetime' and 'bool'"},
      {"h K4 s68500.csv", "s68500.csv", f18, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"h K4 s68500.csv", "s68500.csv", f18, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'datetime.datetime' and 'bool'"},
      {"h K5 s68500.csv", "s68500.csv", f18, "dataframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K5"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"h K5 s68500.csv", "s68500.csv", f18, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K5"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'datetime.datetime' and 'bool'"},
      {"h K1 s68501.csv", "s68501.csv", f10, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'datetime.datetime' and 'str'"},
      {"h K2 s68501.csv", "s68501.csv", f10, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K2"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'datetime.datetime' and 'str'"},
      {"h K3 s68501.csv", "s68501.csv", f10, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K3"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'datetime.datetime' and 'str'"},
      {"h K4 s68501.csv", "s68501.csv", f10, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'datetime.datetime' and 'str'"},
      {"h K5 s68501.csv", "s68501.csv", f10, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K5"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'datetime.datetime' and 'str'"},
      {"h K6 s68501.csv", "s68501.csv", f10, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K6"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'datetime.datetime' and 'str'"},
      {"h K1 s68508.csv", "s68508.csv", f12, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'datetime.datetime' and 'str'"},
      {"h K2 s68508.csv", "s68508.csv", f12, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K2"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'datetime.datetime' and 'str'"},
      {"h K3 s68508.csv", "s68508.csv", f12, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K3"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'datetime.datetime' and 'str'"},
      {"h K4 s68508.csv", "s68508.csv", f12, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'datetime.datetime' and 'str'"},
      {"h K5 s68508.csv", "s68508.csv", f12, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K5"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'datetime.datetime' and 'str'"},
      {"h K6 s68508.csv", "s68508.csv", f12, "lazyframe", "h", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K6"}, {"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'datetime.datetime' and 'str'"},
      {"a3 K1 s68500.csv", "s68500.csv", f18, "dataframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K1"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[p,q,r] P[>p,r#s p,q,r>#k p>q]"},
      {"a3 K1 s68500.csv", "s68500.csv", f18, "lazyframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K1"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[p,q,r] P[>p,r#s p,q,r>#k p>q]"},
      {"a3 K2 s68500.csv", "s68500.csv", f18, "dataframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K2"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[1.5,2.5,3.0] P[1.5,3.0>#k 2.5>1.5 >1.5,2.5,3.0#s]"},
      {"a3 K2 s68500.csv", "s68500.csv", f18, "lazyframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K2"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[1.5,2.5,3.0,None] P[2.5>1.5 3.0,None>#k >1.5,2.5,3.0#s]"},
      {"a3 K3 s68500.csv", "s68500.csv", f18, "dataframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K3"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[2021-03-01 08:00:00+00:00,2021-03-02 09:30:00+00:00,NaT] P[2021-03-01 08:00:00+00:00,2021-03-02 09:30:00+00:00,NaT>#k >2021-03-01 08:00:00+00:00,NaT#s]"},
      {"a3 K3 s68500.csv", "s68500.csv", f18, "lazyframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K3"}, {"alpha_start_timestamp_attribute", "K3"}}, "error: the name 'K3_2' passed to `LazyFrame.with_columns` is duplicate\n\nIt's possible that multiple expressions are returning the same default column name. If this is the case, try renaming the columns with `.alias(\"new_name\")` to avoid duplicate column names.\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'group_by' <---\nSORT BY [col(\"case:concept:name\"), col(\"K3\"), col(\"time:timestamp\")]\n  SELECT [col(\"case:concept:name\"), col(\"time:timestamp\"), col(\"K3\")]\n    DF [\"case:concept:name\", \"concept:name\", \"time:timestamp\", \"start_timestamp\", ...]; PROJECT */12 COLUMNS\n\nThis error occurred with the following context stack:\n\t[1] 'with_columns'\n\t[2] 'filter'\n\t[3] 'filter'\n\t[4] 'group_by'\n"},
      {"a3 K4 s68500.csv", "s68500.csv", f18, "dataframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K4"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[1.0,2.0,3.0] P[1.0>2.0 2.0,3.0>#k >1.0,2.0,3.0#s]"},
      {"a3 K4 s68500.csv", "s68500.csv", f18, "lazyframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K4"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[1.0,2.0,3.0,None] P[1.0>2.0 2.0,3.0,None>#k >2.0,3.0,None#s]"},
      {"a3 K5 s68500.csv", "s68500.csv", f18, "dataframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K5"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[False,True] P[>False,True#s False,True>#k]"},
      {"a3 K5 s68500.csv", "s68500.csv", f18, "lazyframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K5"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[False,None,True] P[>None,True#s False,True>#k]"},
      {"a3 K6 s68500.csv", "s68500.csv", f18, "dataframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K6"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[1,a,b] P[1,a>#k >a,b#s b>1]"},
      {"a3 K6 s68500.csv", "s68500.csv", f18, "lazyframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K6"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[1,a,b] P[1,a>#k >a,b#s b>1]"},
      {"a3 K1 s68501.csv", "s68501.csv", f10, "dataframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K1"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[p,r] P[>p#s r>#k]"},
      {"a3 K1 s68501.csv", "s68501.csv", f10, "lazyframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K1"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[None,p] P[>p#s None>#k]"},
      {"a3 K2 s68501.csv", "s68501.csv", f10, "dataframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K2"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[1.5,2.5,3.0] P[1.5>#k 2.5>1.5 3.0>2.5 >3.0#s]"},
      {"a3 K2 s68501.csv", "s68501.csv", f10, "lazyframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K2"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[1.5,2.5,3.0,None] P[1.5>#k 2.5>1.5 3.0>2.5 >None#s]"},
      {"a3 K3 s68501.csv", "s68501.csv", f10, "dataframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K3"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[2021-03-01 08:00:00+00:00,2021-03-02 09:30:00+00:00] P[2021-03-02 09:30:00+00:00>#k >2021-03-01 08:00:00+00:00#s]"},
      {"a3 K3 s68501.csv", "s68501.csv", f10, "lazyframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K3"}, {"alpha_start_timestamp_attribute", "K3"}}, "error: the name 'K3_2' passed to `LazyFrame.with_columns` is duplicate\n\nIt's possible that multiple expressions are returning the same default column name. If this is the case, try renaming the columns with `.alias(\"new_name\")` to avoid duplicate column names.\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'group_by' <---\nSORT BY [col(\"case:concept:name\"), col(\"K3\"), col(\"time:timestamp\")]\n  SELECT [col(\"case:concept:name\"), col(\"time:timestamp\"), col(\"K3\")]\n    DF [\"case:concept:name\", \"concept:name\", \"time:timestamp\", \"start_timestamp\", ...]; PROJECT */12 COLUMNS\n\nThis error occurred with the following context stack:\n\t[1] 'with_columns'\n\t[2] 'filter'\n\t[3] 'filter'\n\t[4] 'group_by'\n"},
      {"a3 K4 s68501.csv", "s68501.csv", f10, "dataframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K4"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[1,2.0,3.0] P[1>#k 1>2.0 2.0>3.0 3.0>1 >1#s]"},
      {"a3 K4 s68501.csv", "s68501.csv", f10, "lazyframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K4"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[1,2,3] P[1>#k 1>2 2>3 3>1 >1#s]"},
      {"a3 K5 s68501.csv", "s68501.csv", f10, "dataframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K5"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[False,True] P[>True#s False>#k]"},
      {"a3 K5 s68501.csv", "s68501.csv", f10, "lazyframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K5"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[False,True] P[>True#s False>#k]"},
      {"a3 K6 s68501.csv", "s68501.csv", f10, "dataframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K6"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[1,a] P[1>#k >a#s]"},
      {"a3 K6 s68501.csv", "s68501.csv", f10, "lazyframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K6"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[1,a] P[1>#k >a#s]"},
      {"a3 K4 s68502.csv", "s68502.csv", f11, "lazyframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K4"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[3] P[3>#k >3#s]"},
      {"a3 K5 s68502.csv", "s68502.csv", f11, "dataframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K5"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[True] P[>True#s True>#k]"},
      {"a3 K5 s68502.csv", "s68502.csv", f11, "lazyframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K5"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[True] P[>True#s True>#k]"},
      {"a3 K6 s68502.csv", "s68502.csv", f11, "dataframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K6"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[nan,nan] P[>nan#s nan>#k]"},
      {"a3 K6 s68502.csv", "s68502.csv", f11, "lazyframe", "a3", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K6"}, {"alpha_start_timestamp_attribute", "K3"}}, "T[None] P[>None#s None>#k]"},
      {"tpb K1 s68500.csv", "s68500.csv", f18, "dataframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}, {"temporal_business_hours", "true"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"tpb K1 s68500.csv", "s68500.csv", f18, "lazyframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}, {"temporal_business_hours", "true"}}, "error: could not evaluate '<=' comparison between series 'time:timestamp' of dtype: Datetime('\u03bcs', 'UTC') and series 'start_timestamp_2' of dtype: Boolean\n\nThis error occurred in the following expression:\n\t[(col(\"time:timestamp\")) <= (col(\"start_timestamp_2\"))]\nwhile evaluating this larger expression:\n\t[([(col(\"@@index\")) < (col(\"@@index_2\"))]) & ([(col(\"time:timestamp\")) <= (col(\"start_timestamp_2\"))])]\n"},
      {"tpb K2 s68500.csv", "s68500.csv", f18, "dataframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K2"}, {"temporal_business_hours", "true"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"tpb K2 s68500.csv", "s68500.csv", f18, "lazyframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K2"}, {"temporal_business_hours", "true"}}, "error: could not evaluate '<=' comparison between series 'time:timestamp' of dtype: Datetime('\u03bcs', 'UTC') and series 'start_timestamp_2' of dtype: Boolean\n\nThis error occurred in the following expression:\n\t[(col(\"time:timestamp\")) <= (col(\"start_timestamp_2\"))]\nwhile evaluating this larger expression:\n\t[([(col(\"@@index\")) < (col(\"@@index_2\"))]) & ([(col(\"time:timestamp\")) <= (col(\"start_timestamp_2\"))])]\n"},
      {"tpb K3 s68500.csv", "s68500.csv", f18, "dataframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K3"}, {"temporal_business_hours", "true"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"tpb K3 s68500.csv", "s68500.csv", f18, "lazyframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K3"}, {"temporal_business_hours", "true"}}, "error: could not evaluate '<=' comparison between series 'time:timestamp' of dtype: Datetime('\u03bcs', 'UTC') and series 'start_timestamp_2' of dtype: Boolean\n\nThis error occurred in the following expression:\n\t[(col(\"time:timestamp\")) <= (col(\"start_timestamp_2\"))]\nwhile evaluating this larger expression:\n\t[([(col(\"@@index\")) < (col(\"@@index_2\"))]) & ([(col(\"time:timestamp\")) <= (col(\"start_timestamp_2\"))])]\n"},
      {"tpb K4 s68500.csv", "s68500.csv", f18, "dataframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K4"}, {"temporal_business_hours", "true"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"tpb K4 s68500.csv", "s68500.csv", f18, "lazyframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K4"}, {"temporal_business_hours", "true"}}, "error: could not evaluate '<=' comparison between series 'time:timestamp' of dtype: Datetime('\u03bcs', 'UTC') and series 'start_timestamp_2' of dtype: Boolean\n\nThis error occurred in the following expression:\n\t[(col(\"time:timestamp\")) <= (col(\"start_timestamp_2\"))]\nwhile evaluating this larger expression:\n\t[([(col(\"@@index\")) < (col(\"@@index_2\"))]) & ([(col(\"time:timestamp\")) <= (col(\"start_timestamp_2\"))])]\n"},
      {"tpb K5 s68500.csv", "s68500.csv", f18, "dataframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K5"}, {"temporal_business_hours", "true"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"tpb K5 s68500.csv", "s68500.csv", f18, "lazyframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K5"}, {"temporal_business_hours", "true"}}, "error: could not evaluate '<=' comparison between series 'time:timestamp' of dtype: Datetime('\u03bcs', 'UTC') and series 'start_timestamp_2' of dtype: Boolean\n\nThis error occurred in the following expression:\n\t[(col(\"time:timestamp\")) <= (col(\"start_timestamp_2\"))]\nwhile evaluating this larger expression:\n\t[([(col(\"@@index\")) < (col(\"@@index_2\"))]) & ([(col(\"time:timestamp\")) <= (col(\"start_timestamp_2\"))])]\n"},
      {"tpb K6 s68500.csv", "s68500.csv", f18, "dataframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K6"}, {"temporal_business_hours", "true"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"tpb K6 s68500.csv", "s68500.csv", f18, "lazyframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K6"}, {"temporal_business_hours", "true"}}, "error: could not evaluate '<=' comparison between series 'time:timestamp' of dtype: Datetime('\u03bcs', 'UTC') and series 'start_timestamp_2' of dtype: Boolean\n\nThis error occurred in the following expression:\n\t[(col(\"time:timestamp\")) <= (col(\"start_timestamp_2\"))]\nwhile evaluating this larger expression:\n\t[([(col(\"@@index\")) < (col(\"@@index_2\"))]) & ([(col(\"time:timestamp\")) <= (col(\"start_timestamp_2\"))])]\n"},
      {"tpb K1 s68501.csv", "s68501.csv", f10, "dataframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}, {"temporal_business_hours", "true"}}, "error: TypeError: '<=' not supported between instances of 'Timestamp' and 'int'"},
      {"tpb K1 s68501.csv", "s68501.csv", f10, "lazyframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}, {"temporal_business_hours", "true"}}, "error: cannot compare 'date/datetime/time' to a string value (create native python { 'date', 'datetime', 'time' } or compare to a temporal column)\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\nFILTER col(\"time:timestamp\") <= col(\"start_timestamp_2\")\nFROM\n  FILTER col(\"@@index\") < col(\"@@index_2\")\n  FROM\n    INNER JOIN:\n    LEFT PLAN ON: [col(\"case:concept:name\")]\n      SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n        SELECT [col(\"case:concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"K1\"), col(\"@@index\")]\n          DF [\"case:concept:name\", \"concept:name\", \"time:timestamp\", \"start_timestamp\", ...]; PROJECT */12 COLUMNS\n    RIGHT PLAN ON: [col(\"case:concept:name\")]\n      SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n        SELECT [col(\"case:concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"K1\"), col(\"@@index\")]\n          DF [\"case:concept:name\", \"concept:name\", \"time:timestamp\", \"start_timestamp\", ...]; PROJECT */12 COLUMNS\n    END INNER JOIN\n\nThis error occurred with the following context stack:\n\t[1] 'filter' failed\n\t[2] 'with_columns'\n"},
      {"tpb K2 s68501.csv", "s68501.csv", f10, "dataframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K2"}, {"temporal_business_hours", "true"}}, "error: TypeError: '<=' not supported between instances of 'Timestamp' and 'int'"},
      {"tpb K1 s68505.csv", "s68505.csv", f19, "dataframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}, {"temporal_business_hours", "true"}}, "p>r:120.0000/0.0000;q>q:420.0000/0.0000;r>r:60.0000/0.0000"},
      {"tpb K1 s68505.csv", "s68505.csv", f19, "lazyframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}, {"temporal_business_hours", "true"}}, "p>r:120.0000/0.0000;q>q:420.0000/0.0000;r>r:60.0000/0.0000"},
      {"tpb K2 s68505.csv", "s68505.csv", f19, "dataframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K2"}, {"temporal_business_hours", "true"}}, "1.5>1.5:120.0000/0.0000;2.5>1.5:0.0000/0.0000;3.0>1.5:60.0000/0.0000"},
      {"tpb K2 s68505.csv", "s68505.csv", f19, "lazyframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K2"}, {"temporal_business_hours", "true"}}, "1.5>1.5:120.0000/0.0000;2.5>1.5:0.0000/0.0000;3.0>1.5:60.0000/0.0000"},
      {"tpb K3 s68505.csv", "s68505.csv", f19, "dataframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K3"}, {"temporal_business_hours", "true"}}, "2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00:240.0000/0.0000;2021-03-02 09:30:00+00:00>2021-03-02 09:30:00+00:00:60.0000/0.0000"},
      {"tpb K3 s68505.csv", "s68505.csv", f19, "lazyframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K3"}, {"temporal_business_hours", "true"}}, "2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00:240.0000/0.0000;2021-03-02 09:30:00+00:00>2021-03-02 09:30:00+00:00:60.0000/0.0000"},
      {"tpb K4 s68505.csv", "s68505.csv", f19, "dataframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K4"}, {"temporal_business_hours", "true"}}, "1.0>1.0:420.0000/0.0000;1.0>3.0:0.0000/0.0000;2.0>3.0:60.0000/0.0000;3.0>1.0:240.0000/0.0000"},
      {"tpb K4 s68505.csv", "s68505.csv", f19, "lazyframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K4"}, {"temporal_business_hours", "true"}}, "1.0>1.0:420.0000/0.0000;1.0>3.0:0.0000/0.0000;2.0>3.0:60.0000/0.0000;3.0>1.0:240.0000/0.0000"},
      {"tpb K5 s68505.csv", "s68505.csv", f19, "dataframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K5"}, {"temporal_business_hours", "true"}}, "False>True:0.0000/0.0000"},
      {"tpb K5 s68505.csv", "s68505.csv", f19, "lazyframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K5"}, {"temporal_business_hours", "true"}}, "False>True:0.0000/0.0000"},
      {"tpb K6 s68505.csv", "s68505.csv", f19, "dataframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K6"}, {"temporal_business_hours", "true"}}, "1>1:120.0000/103.9230;b>1:120.0000/0.0000"},
      {"tpb K6 s68505.csv", "s68505.csv", f19, "lazyframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K6"}, {"temporal_business_hours", "true"}}, "1>1:120.0000/103.9230;b>1:120.0000/0.0000"},
      {"tpb K1 s68507.csv", "s68507.csv", f20, "dataframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}, {"temporal_business_hours", "true"}}, "p>p:300.0000/0.0000;p>q:150.0000/42.4264;q>p:150.0000/42.4264;q>q:60.0000/0.0000"},
      {"tpb K1 s68507.csv", "s68507.csv", f20, "lazyframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}, {"temporal_business_hours", "true"}}, "p>p:300.0000/0.0000;p>q:150.0000/42.4264;q>p:150.0000/42.4264;q>q:60.0000/0.0000"},
      {"tpb K2 s68507.csv", "s68507.csv", f20, "dataframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K2"}, {"temporal_business_hours", "true"}}, "1.5>2.5:60.0000/0.0000;2.5>1.5:120.0000/0.0000;2.5>2.5:180.0000/0.0000"},
      {"tpb K2 s68507.csv", "s68507.csv", f20, "lazyframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K2"}, {"temporal_business_hours", "true"}}, "1.5>2.5:60.0000/0.0000;2.5>1.5:120.0000/0.0000;2.5>2.5:180.0000/0.0000"},
      {"tpb K3 s68507.csv", "s68507.csv", f20, "dataframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K3"}, {"temporal_business_hours", "true"}}, "2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00:150.0000/42.4264"},
      {"tpb K3 s68507.csv", "s68507.csv", f20, "lazyframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K3"}, {"temporal_business_hours", "true"}}, "2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00:150.0000/42.4264"},
      {"tpb K4 s68507.csv", "s68507.csv", f20, "dataframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K4"}, {"temporal_business_hours", "true"}}, "1.0>1.0:120.0000/60.0000;1.0>2.0:120.0000/0.0000"},
      {"tpb K4 s68507.csv", "s68507.csv", f20, "lazyframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K4"}, {"temporal_business_hours", "true"}}, "1.0>1.0:120.0000/60.0000;1.0>2.0:120.0000/0.0000"},
      {"tpb K5 s68507.csv", "s68507.csv", f20, "dataframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K5"}, {"temporal_business_hours", "true"}}, "True>False:60.0000/0.0000"},
      {"tpb K5 s68507.csv", "s68507.csv", f20, "lazyframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K5"}, {"temporal_business_hours", "true"}}, "True>False:60.0000/0.0000"},
      {"tpb K6 s68507.csv", "s68507.csv", f20, "dataframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K6"}, {"temporal_business_hours", "true"}}, "1>1:120.0000/0.0000;a>a:60.0000/0.0000"},
      {"tpb K6 s68507.csv", "s68507.csv", f20, "lazyframe", "tpb", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K6"}, {"temporal_business_hours", "true"}}, "1>1:120.0000/0.0000;a>a:60.0000/0.0000"},
      {"tpb K5 s68505.csv (renamed header)", "s68505.csv", f21, "dataframe", "tpb", {{"case_column", "cid"}, {"activity_column", "act"}, {"timestamp_column", "ts"}, {"temporal_activity_attribute", "K5"}, {"temporal_business_hours", "true"}}, "False>True:0.0000/0.0000"},
      {"tpb K5 s68505.csv (renamed header)", "s68505.csv", f21, "lazyframe", "tpb", {{"case_column", "cid"}, {"activity_column", "act"}, {"timestamp_column", "ts"}, {"temporal_activity_attribute", "K5"}, {"temporal_business_hours", "true"}}, "False>True:0.0000/0.0000"},
      {"hc K5 s68508.csv", "s68508.csv", f12, "lazyframe", "hc", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K5"}}, "A[False,True] E[False>True:0.000000/1 True>False:0.000000/2 True>True:0.666667/2]"},
      {"hc K5 s68509.csv", "s68509.csv", f13, "lazyframe", "hc", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K5"}}, "A[False,True] E[False>True:0.500000/1]"},
      {"dp K2 s68500.csv", "s68500.csv", f18, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K2"}}, "error: TypeError: 'float' object is not subscriptable"},
      {"dp K3 s68500.csv", "s68500.csv", f18, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K3"}}, "error: TypeError: 'Timestamp' object is not subscriptable"},
      {"dp K4 s68500.csv", "s68500.csv", f18, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K4"}}, "error: TypeError: 'float' object is not subscriptable"},
      {"dp K5 s68500.csv", "s68500.csv", f18, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K5"}}, "error: TypeError: 'bool' object is not subscriptable"},
      {"dp K6 s68500.csv", "s68500.csv", f18, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K6"}}, "ok"},
      {"dp K2 s68501.csv", "s68501.csv", f10, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K2"}}, "error: TypeError: 'float' object is not subscriptable"},
      {"dp K3 s68501.csv", "s68501.csv", f10, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K3"}}, "error: TypeError: 'Timestamp' object is not subscriptable"},
      {"dp K4 s68501.csv", "s68501.csv", f10, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K4"}}, "error: TypeError: 'int' object is not subscriptable"},
      {"dp K5 s68501.csv", "s68501.csv", f10, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K5"}}, "error: TypeError: 'bool' object is not subscriptable"},
      {"dp K3 s68502.csv", "s68502.csv", f11, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K3"}}, "error: TypeError: 'float' object is not subscriptable"},
      {"dp K4 s68502.csv", "s68502.csv", f11, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K4"}}, "error: TypeError: 'int' object is not subscriptable"},
      {"dp K6 s68502.csv", "s68502.csv", f11, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K6"}}, "error: TypeError: 'float' object is not subscriptable"},
      {"dp K4 s68503.csv", "s68503.csv", f22, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K4"}}, "error: TypeError: 'float' object is not subscriptable"},
      {"dp K3 s68505.csv", "s68505.csv", f19, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K3"}}, "error: TypeError: 'NaTType' object is not subscriptable"},
      {"dp K6 s68505.csv", "s68505.csv", f19, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K6"}}, "mine: TypeError: '<' not supported between instances of 'str' and 'float'"},
      {"dp K3 s68506.csv", "s68506.csv", f23, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K3"}}, "error: TypeError: 'NaTType' object is not subscriptable"},
      {"dp K4 s68509.csv", "s68509.csv", f13, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K4"}}, "ok"},
      {"dp K3 s68510.csv", "s68510.csv", f14, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K3"}}, "ok"},
      {"dp K5 s68512.csv", "s68512.csv", f24, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K5"}}, "error: TypeError: 'float' object is not subscriptable"},
      {"dp K6 s68513.csv", "s68513.csv", f25, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K6"}}, "error: TypeError: 'int' object is not subscriptable"},
      {"dp K3 s68515.csv", "s68515.csv", f26, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K3"}}, "error: TypeError: 'float' object is not subscriptable"},
      {"dp K2 s68516.csv", "s68516.csv", f27, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K2"}}, "ok"},
      {"dp K5 s68520.csv", "s68520.csv", f28, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K5"}}, "ok"},
      {"dp K5 s68521.csv", "s68521.csv", f29, "dataframe", "dp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K5"}}, "error: TypeError: 'float' object is not subscriptable"},
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
  std::cout << "review HC server checks passed\n";
  return 0;
}
