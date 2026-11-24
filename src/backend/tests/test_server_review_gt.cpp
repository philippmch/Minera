#undef NDEBUG
// Review GT: CSV uploads' log shape keyed by other columns and XES frames
// keyed by a column of mixed Python objects.
//
// - The temporal profile and its conformance on a CSV log shape read each
//   pair's source and target cells from convert_to_event_log's events (a
//   start column of ints, text, bools or gaps fails with the first cell's
//   own AttributeError, ValueError or KeyError); a column read_csv keeps as
//   text (ints above int64 beside a negative cell or a gap) is str.
// - Declare keyed by a typed column: int64 second activities shown as
//   floats, a falsy or missing second activity stored under the first alone
//   (the conformance's TypeError); a text column with gaps fails sorting.
// - The DFG's EventLog counters give a float64 column's gaps one node each.
// - The transition system names a node of equal values (0 and False, 1 and
//   True, 1 and 1.0) after the object that created it, and places states in
//   the root set by their own objects' repr.
//
// Expected values are pm4py 2.7.22 black-box on the same inputs
// (review-harness round-gt: tc/mkgt.py, ct/*.py).
//
// Usage: minera_server_review_gt_tests <minera_server>

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

std::string sorted_join(std::vector<std::string> parts, const std::string& by) {
  std::sort(parts.begin(), parts.end());
  std::string out;
  for (std::size_t k = 0; k < parts.size(); ++k) out += (k ? by : "") + parts[k];
  return out;
}

std::string plain_join(const std::vector<std::string>& parts, const std::string& by) {
  std::string out;
  for (std::size_t k = 0; k < parts.size(); ++k) out += (k ? by : "") + parts[k];
  return out;
}

std::string number(const Json& value, const char* format) {
  if (value.kind == Json::Kind::kNull) return "None";
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, format, std::strtod(value.text.c_str(), nullptr));
  return buffer;
}

std::string name_of(const Json& value) { return value.kind == Json::Kind::kNull ? "None" : value.text; }

std::string error_text(const Json& error) {
  if (error.kind == Json::Kind::kObject) {
    const auto& type = error["type"].text;
    return (type.starts_with("builtins.") ? type.substr(9) + ": " : std::string()) + error["message"].text;
  }
  return error.text;
}

using Fields = std::vector<std::pair<std::string, std::string>>;

// mkgt.py's fixed profile for the conformance cases.
const std::string kProfile =
    "[[\"a\", \"b\", 60.0, 10.0], [\"a\", \"c\", 300.0, 0.0], [\"b\", \"c\", 30.0, 5.0], [\"c\", \"d\", 10.0, 100.0], "
    "[\"b\", \"a\", 3600.0, 60.0], [\"a\", \"a\", 0.0, 1.0], [\"d\", \"a\", 120.0, 30.0], [\"b\", \"d\", 600.0, 0.0]]";

const char* const kDeclareTemplates[] = {
    "existence", "exactly_one", "init", "responded_existence", "response", "precedence", "succession",
    "altresponse", "altprecedence", "altsuccession", "chainresponse", "chainprecedence", "chainsuccession",
    "absence", "coexistence", "noncoexistence", "nonsuccession", "nonchainsuccession"};

std::string g6(const Json& value) { return number(value, "%.6g"); }

std::string profile_entry(const std::string& source, const std::string& target, const Json& mean, const Json& stdev) {
  return source + ">" + target + "=" + g6(mean) + "/" + g6(stdev);
}

// One case's outcome in mkgt.py's canonical form. Fields prefixed "a:" go to
// /api/analyze-behavior, the rest to /api/mine (or /api/discover-graph).
std::string outcome(httplib::Client& client, const std::string& name, const std::string& content,
                    const std::string& kind, const Fields& fields) {
  httplib::MultipartFormDataItems items = {{"log", content, name, "application/octet-stream"},
                                           {"include_traces", "false", "", ""}};
  if (kind == "tpd" || kind == "ts") items.push_back({"mining_input_shape", "dataframe", "", ""});
  Fields analyze;
  for (const auto& [key, value] : fields) {
    if (key.starts_with("a:")) analyze.emplace_back(key.substr(2), value);
    else if (kind != "ts") items.push_back({key, value, "", ""});
  }
  const auto response = client.Post("/api/mine", items);
  if (!response) return "no response";
  JsonReader reader{response->body};
  const auto body = reader.value();
  if (kind == "tpd") {
    if (body["error"].kind != Json::Kind::kNull && body["family_results"].kind == Json::Kind::kNull)
      return "mine error: " + error_text(body["error"]);
    const auto& x = body["family_results"]["temporal_profile"];
    if (x["executed"].text != "true") return "error: " + error_text(x["error"]);
    std::vector<std::string> entries;
    for (const auto& e : x["result"]["entries"].items)
      entries.push_back(profile_entry(name_of(e["source"]), name_of(e["target"]), e["mean_seconds"],
                                      e["standard_deviation_seconds"]));
    return "P " + sorted_join(entries, ",");
  }
  if (kind == "tpl") {
    if (body["error"].kind != Json::Kind::kNull) {
      auto text = error_text(body["error"]);
      if (const auto at = text.find("temporal profile: "); at != std::string::npos) text = text.substr(at + 18);
      return "error: " + text;
    }
    std::vector<std::string> names;
    for (const auto& a : body["activities"].items) names.push_back(a["name"].text);
    std::vector<std::string> entries;
    for (const auto& e : body["temporal_profile"].items)
      entries.push_back(profile_entry(names[std::stoul(e["source"].text)], names[std::stoul(e["target"].text)],
                                      e["mean_seconds"], e["standard_deviation_seconds"]));
    return "P " + sorted_join(entries, ",");
  }
  if (body["error"].kind != Json::Kind::kNull) return "error: " + error_text(body["error"]);
  std::vector<std::string> names;
  for (const auto& a : body["activities"].items) names.push_back(a["name"].text);
  const auto named = [&](const Json& id) { return names[std::stoul(id.text)]; };
  if (kind == "dcl") {
    std::vector<std::string> rules;
    for (const auto* family : kDeclareTemplates)
      for (const auto& rule : body["declare"][family].items)
        rules.push_back(std::string(family) + ":" +
                        (rule["activity"].kind != Json::Kind::kNull ? named(rule["activity"])
                                                                    : named(rule["source"]) + ">" + named(rule["target"])));
    std::vector<std::string> fits;
    for (const auto& result : body["declare_conformance"].items) fits.push_back(result["is_fit"].text == "true" ? "T" : "F");
    return "M " + sorted_join(rules, ",") + " | fit " + plain_join(fits, ",");
  }
  if (kind == "dfg") {
    std::vector<std::string> edges;
    for (const auto& e : body["dfg_variants"]["statistical"].items[0]["frequency_dfg"].items)
      edges.push_back(named(e["source"]) + ">" + named(e["target"]) + "=" + e["count"].text);
    return "D " + sorted_join(edges, ",");
  }
  if (kind == "tcl") {
    httplib::Params params = {{"family", "temporal_conformance"},
                              {"source_id", body["source_id"].text},
                              {"variant", "log"},
                              {"temporal_profile_json", kProfile},
                              {"include_traces", "false"}};
    for (const auto& [key, value] : analyze) params.emplace(key, value);
    const auto answer = client.Post("/api/analyze-behavior", params);
    if (!answer) return "no response";
    JsonReader second{answer->body};
    const auto out = second.value();
    if (out["error"].kind != Json::Kind::kNull) return "error: " + error_text(out["error"]);
    std::vector<std::string> traces;
    for (const auto& t : out["conformance"]["traces"].items) {
      std::vector<std::string> deviations;
      for (const auto& d : t.items)
        deviations.push_back(profile_entry(name_of(d["source"]), name_of(d["target"]), d["elapsed_seconds"],
                                           d["standard_deviation_distance"]));
      traces.push_back("[" + plain_join(deviations, ",") + "]");
    }
    return "C " + plain_join(traces, ";");
  }
  // ts: the transition system of the frame session.
  httplib::Params params = {{"family", "transition_system"}, {"source_id", body["source_id"].text}};
  for (const auto& [key, value] : fields) params.emplace(key, value);
  const auto answer = client.Post("/api/discover-graph", params);
  if (!answer) return "no response";
  JsonReader second{answer->body};
  const auto graph = second.value();
  if (graph["error"].kind != Json::Kind::kNull) return "error: " + error_text(graph["error"]);
  const auto& result = graph["native_result"];
  const bool unordered = result["options"]["view"].text == "set" || result["options"]["view"].text == "multiset";
  std::map<std::string, std::string> state_name;
  std::vector<std::string> states;
  for (const auto& s : result["states"].items) {
    std::vector<std::string> payload;
    for (const auto& value : s["payload"].items) payload.push_back(value.text);
    const auto shown = "(" + (unordered ? sorted_join(payload, ",") : plain_join(payload, ",")) + ")";
    state_name[s["id"].text] = shown;
    if (s["in_root"].kind == Json::Kind::kNull || s["in_root"].text == "true") states.push_back(shown);
  }
  std::vector<std::string> transitions;
  for (const auto& t : result["transitions"].items)
    transitions.push_back(state_name[t["source"].text] + "-" + t["label"].text + "->" + state_name[t["target"].text]);
  return "S " + sorted_join(states, ",") + " | " + sorted_join(transitions, ",");
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
    std::string label, name, content, kind;
    Fields fields;
    std::string expected;
  };
  const std::string tpl_f0 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-01 08:08:00,True\nc0,b,2021-03-01 08:09:00,False\nc1,c,2021-03-01 08:10:00,True\nc1,a,,True\nc1,a,2021-03-01 08:17:00,True\nc2,c,2021-03-01 08:18:00,True\nc2,c,2021-03-01 08:21:00,False\nc2,d,2021-03-01 08:23:00,True\n";
  const std::string tpl_f1 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,c,2021-03-11 18:00:00,1\nc0,a,2021-03-12 14:00:00,18446744073709551625\nc0,a,2021-03-13 10:00:00,2\nc0,c,2021-03-15 02:00:00,0\n";
  const std::string tpl_f2 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,c,2021-03-11 08:00:00,0\n";
  const std::string tpl_f3 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,b,2021-03-01 08:05:00,9223372036854775809\nc0,a,2021-03-01 08:09:00,3\nc0,d,2021-03-01 08:11:00,9223372036854775809\n";
  const std::string tcl_f0 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-01 08:08:00,True\nc0,b,2021-03-01 08:09:00,False\nc1,c,2021-03-01 08:10:00,True\nc1,a,,True\nc1,a,2021-03-01 08:17:00,True\nc2,c,2021-03-01 08:18:00,True\nc2,c,2021-03-01 08:21:00,False\nc2,d,2021-03-01 08:23:00,True\n";
  const std::string tcl_f1 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,c,2021-03-11 18:00:00,1\nc0,a,2021-03-12 14:00:00,18446744073709551625\nc0,a,2021-03-13 10:00:00,2\nc0,c,2021-03-15 02:00:00,0\n";
  const std::string tcl_f2 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,c,2021-03-11 08:00:00,0\n";
  const std::string tcl_f3 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,b,2021-03-01 08:05:00,9223372036854775809\nc0,a,2021-03-01 08:09:00,3\nc0,d,2021-03-01 08:11:00,9223372036854775809\n";
  const std::string tpd_f0 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-01 08:08:00,True\nc0,b,2021-03-01 08:09:00,False\nc1,c,2021-03-01 08:10:00,True\nc1,a,,True\nc1,a,2021-03-01 08:17:00,True\nc2,c,2021-03-01 08:18:00,True\nc2,c,2021-03-01 08:21:00,False\nc2,d,2021-03-01 08:23:00,True\n";
  const std::string tpd_f1 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,c,2021-03-11 18:00:00,1\nc0,a,2021-03-12 14:00:00,18446744073709551625\nc0,a,2021-03-13 10:00:00,2\nc0,c,2021-03-15 02:00:00,0\n";
  const std::string tpd_f2 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,b,2021-03-01 08:05:00,9223372036854775809\nc0,a,2021-03-01 08:09:00,3\nc0,d,2021-03-01 08:11:00,9223372036854775809\n";
  const std::string tpd_f3 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,c,2021-03-01 08:32:00,2021-03-01 08:31:00\nc0,c,2021-03-01 08:35:00,2021-03-01 08:34:00\nc0,a,,2021-03-01 08:33:00\nc0,b,,2021-03-01 08:34:00\nc1,a,2021-03-01 08:15:00,2021-03-01 08:14:00\nc2,c,,2021-03-01 08:05:00\nc2,a,2021-03-01 08:07:00,2021-03-01 08:07:00\nc2,c,2021-03-01 08:07:00,2021-03-01 08:04:00\nc2,a,2021-03-01 08:11:00,2021-03-01 08:08:00\nc3,a,,2021-03-01 08:28:00\nc3,c,2021-03-01 08:30:00,2021-03-01 08:29:00\nc3,d,2021-03-01 08:31:00,2021-03-01 08:28:00\nc4,b,,2021-03-01 08:20:00\nc4,a,2021-03-01 08:23:00,2021-03-01 08:22:00\nc4,d,2021-03-01 08:26:00,2021-03-01 08:23:00\nc4,a,2021-03-01 08:30:00,2021-03-01 08:30:00\n";
  const std::string tpd_f4 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,a,2021-03-01 08:33:00,4\nc0,d,2021-03-01 08:34:00,18446744073709551620\nc0,a,2021-03-01 08:36:00,7\nc0,b,,8\nc1,d,2021-03-01 08:00:00,2\nc1,a,2021-03-01 08:03:00,18446744073709551617\nc1,d,2021-03-01 08:03:00,3\nc1,b,2021-03-01 08:06:00,2\n";
  const std::string tpd_f5 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,c,2021-03-02 06:00:00,-1\nc0,a,2021-03-02 07:00:00,-1\nc0,c,2021-03-02 08:00:00,9223372036854775809\nc1,c,2021-03-01 09:00:00,-1\nc2,c,2021-03-01 22:00:00,9223372036854775809\n";
  const std::string dcl_f0 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:10:00,,3.0,2021-03-01 08:00:00,,True,a\nc1,a,2021-03-01 09:41:00,q,,,,True,1\nc1,a,2021-03-01 10:41:00,r,,,2,False,1\nc1,b,2021-03-01 10:46:00,,1.5,,,,a\nc2,b,2021-03-01 08:32:00,p,,2021-03-01 08:00:00,2,,b\nc2,b,2021-03-01 08:33:00,r,3.0,,,,a\nc3,c,2021-03-01 08:06:00,r,2.5,2021-03-02 09:30:00,3,False,1\n";
  const std::string dcl_f1 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:31:00,r,,2021-03-01 08:00:00,7,False,a\nc0,c,2021-03-01 08:36:00,r,,2021-03-02 09:30:00,7,False,\nc1,a,2021-03-01 08:45:00,s,2.5,2021-03-02 09:30:00,3,True,2\nc1,c,2021-03-01 08:50:00,,,2021-03-02 09:30:00,,False,b\nc1,a,2021-03-01 08:55:00,s,3.0,2021-03-03 10:00:00,1,,b\nc2,b,2021-03-01 08:32:00,s,2.5,2021-03-01 08:00:00,,False,a\nc2,b,2021-03-01 08:33:00,r,1.5,2021-03-01 08:00:00,,False,2\n";
  const std::string dcl_f2 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 08:52:00,q,1.5,,,True,a\nc1,c,2021-03-01 09:26:00,p,2.5,2021-03-01 08:00:00,1,True,b\nc1,b,2021-03-01 10:26:00,p,2.5,2021-03-02 09:30:00,1,False,1\nc1,b,2021-03-01 10:31:00,r,1.5,2021-03-02 09:30:00,2,False,1\nc1,c,2021-03-01 10:32:00,,1.5,2021-03-01 08:00:00,,,\nc2,a,2021-03-01 09:16:00,r,1.5,,2,True,1\nc2,a,2021-03-01 10:16:00,p,3.0,2021-03-02 09:30:00,,False,b\nc3,a,2021-03-01 09:43:00,q,2.5,2021-03-01 08:00:00,,True,1\nc4,b,2021-03-01 08:52:00,,,,2,,\nc4,a,2021-03-01 08:53:00,r,1.5,,3,True,1\nc4,c,2021-03-01 08:54:00,,2.5,2021-03-02 09:30:00,3,False,1\nc4,c,2021-03-01 09:54:00,p,2.5,2021-03-01 08:00:00,1,False,\n";
  const std::string dcl_f3 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 08:38:00,p,3.0,2021-03-02 09:30:00,,True,1\nc1,a,2021-03-01 08:15:00,r,1.5,2021-03-01 08:00:00,,False,\nc2,c,2021-03-01 09:28:00,r,2.5,2021-03-01 08:00:00,1,True,\nc2,a,2021-03-01 09:33:00,p,,,,,\nc2,c,2021-03-01 09:38:00,r,,2021-03-02 09:30:00,3,False,a\nc2,a,2021-03-01 09:43:00,r,2.5,,3,False,\nc3,b,2021-03-01 08:52:00,p,3.0,,,False,a\nc4,a,2021-03-01 09:24:00,,2.5,2021-03-01 08:00:00,,True,\nc5,c,2021-03-01 08:16:00,q,,2021-03-02 09:30:00,1,True,1\nc5,c,2021-03-01 09:16:00,q,3.0,2021-03-01 08:00:00,3,True,b\n";
  const std::string dcl_f4 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 08:33:00,p,3.0,2021-03-02 09:30:00,3,False,\nc0,c,2021-03-01 08:34:00,,1.5,2021-03-02 09:30:00,3,False,\nc0,a,2021-03-01 08:39:00,,2.5,2021-03-02 09:30:00,3,True,\nc1,c,2021-03-01 08:44:00,,2.5,2021-03-01 08:00:00,2,,1\nc2,a,2021-03-01 09:25:00,p,1.5,,,,\nc2,c,2021-03-01 09:26:00,,1.5,2021-03-01 08:00:00,3,True,\nc3,c,2021-03-01 09:12:00,,2.5,2021-03-01 08:00:00,2,True,1\nc3,b,2021-03-01 10:12:00,p,,2021-03-02 09:30:00,,False,b\n";
  const std::string dcl_f5 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 08:35:00,q,3.0,2021-03-02 09:30:00,,True,a\nc1,a,2021-03-01 09:43:00,r,3.0,,1,False,1\nc2,a,2021-03-01 08:12:00,q,,2021-03-02 09:30:00,3,False,a\nc2,c,2021-03-01 08:17:00,,3.0,2021-03-02 09:30:00,3,False,a\nc3,a,2021-03-01 09:12:00,r,1.5,2021-03-02 09:30:00,1,False,a\n";
  const std::string dcl_f6 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 08:17:00,r,2.5,2021-03-02 09:30:00,2,True,1\nc1,a,2021-03-01 08:23:00,r,2.5,2021-03-01 08:00:00,2,,\nc1,b,2021-03-01 08:28:00,q,2.5,2021-03-01 08:00:00,1,True,a\nc1,b,2021-03-01 08:29:00,r,,2021-03-02 09:30:00,,True,1\nc1,b,2021-03-01 09:29:00,q,,2021-03-01 08:00:00,2,,1\n";
  const std::string dcl_f7 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:36:00,q,2.5,,1,True,1\nc1,c,2021-03-01 08:50:00,,3.0,2021-03-01 08:00:00,3,False,\nc2,a,2021-03-01 08:48:00,r,2.5,2021-03-02 09:30:00,3,False,1\n";
  const std::string dcl_f8 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:37:00,r,-1.0,,2,False,a\nc0,c,2021-03-01 09:37:00,q,,,,False,b\nc0,b,2021-03-01 09:38:00,s,,2021-03-01 08:00:00,3,False,2\nc0,b,2021-03-01 09:39:00,,,2021-03-03 10:00:00,7,True,b\nc1,b,2021-03-01 08:03:00,s,,2021-03-01 08:00:00,,True,1\nc1,a,2021-03-01 08:04:00,q,3.0,2021-03-03 10:00:00,1,False,2\nc1,a,2021-03-01 09:04:00,q,2.5,,,True,1\nc1,b,2021-03-01 09:05:00,,-1.0,2021-03-01 08:00:00,2,True,a\nc2,c,2021-03-01 08:22:00,s,,2021-03-01 08:00:00,3,True,2\nc2,b,2021-03-01 08:27:00,p,,2021-03-01 08:00:00,,True,2\nc2,b,2021-03-01 08:32:00,r,1.5,2021-03-01 08:00:00,7,True,1\nc2,b,2021-03-01 09:32:00,r,2.5,2021-03-01 08:00:00,7,False,2\nc3,a,2021-03-01 08:26:00,,,2021-03-03 10:00:00,1,True,1\n";
  const std::string dcl_f9 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:24:00,r,1.5,2021-03-01 08:00:00,3,False,1\nc1,c,2021-03-01 08:26:00,r,2.5,2021-03-01 08:00:00,1,False,\nc1,c,2021-03-01 08:31:00,,3.0,,2,,b\nc1,c,2021-03-01 08:36:00,r,2.5,,,,1\nc1,b,2021-03-01 08:41:00,p,1.5,2021-03-02 09:30:00,3,False,\n";
  const std::string dcl_f10 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 09:34:00,,-1.0,2021-03-01 08:00:00,1,False,1\nc0,a,2021-03-01 09:35:00,q,,2021-03-01 08:00:00,2,True,a\nc1,c,2021-03-01 09:48:00,p,-1.0,2021-03-01 08:00:00,2,False,\n";
  const std::string dcl_f11 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 09:07:00,q,1.5,2021-03-01 08:00:00,1,False,b\nc1,b,2021-03-01 08:25:00,q,3.0,2021-03-02 09:30:00,3,,\nc2,b,2021-03-01 08:49:00,q,1.5,2021-03-01 08:00:00,2,False,\nc3,c,2021-03-01 08:22:00,q,3.0,,1,False,a\nc4,a,2021-03-01 08:08:00,q,,2021-03-01 08:00:00,3,False,1\n";
  const std::string dcl_f12 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 09:05:00,s,3.0,,7,True,b\nc1,c,2021-03-01 09:35:00,p,,,3,True,2\nc1,a,2021-03-01 09:40:00,q,,2021-03-03 10:00:00,3,,\nc2,b,2021-03-01 09:12:00,r,1.5,2021-03-01 08:00:00,7,,a\nc3,a,2021-03-01 08:01:00,q,1.5,2021-03-03 10:00:00,2,False,1\nc3,b,2021-03-01 08:06:00,r,,2021-03-03 10:00:00,7,True,1\nc3,a,2021-03-01 09:06:00,q,2.5,2021-03-01 08:00:00,1,False,a\nc3,c,2021-03-01 09:07:00,s,2.5,2021-03-03 10:00:00,7,,\n";
  const std::string dcl_f13 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:12:00,p,2.5,2021-03-01 08:00:00,1,False,\nc1,b,2021-03-01 08:04:00,r,1.5,2021-03-01 08:00:00,1,True,a\nc2,a,2021-03-01 08:20:00,p,1.5,2021-03-02 09:30:00,1,,\nc3,c,2021-03-01 09:11:00,,3.0,2021-03-02 09:30:00,1,True,\nc3,b,2021-03-01 09:16:00,q,3.0,2021-03-01 08:00:00,3,,b\nc4,b,2021-03-01 09:28:00,q,2.5,2021-03-02 09:30:00,2,True,\n";
  const std::string dcl_f14 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:26:00,q,1.5,,3,True,b\nc0,b,2021-03-01 09:26:00,r,1.5,,1,False,2\nc1,b,2021-03-01 08:14:00,r,1.5,,7,False,\nc2,a,2021-03-01 08:18:00,p,3.0,2021-03-01 08:00:00,1,True,b\nc3,c,2021-03-01 08:32:00,r,1.5,,,False,2\nc3,c,2021-03-01 08:33:00,q,2.5,2021-03-03 10:00:00,7,False,a\n";
  const std::string dcl_f15 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 09:34:00,,,2021-03-01 08:00:00,3,False,1\nc1,b,2021-03-01 09:25:00,,1.5,2021-03-01 08:00:00,1,True,\nc1,a,2021-03-01 09:30:00,q,3.0,2021-03-02 09:30:00,3,False,a\nc1,a,2021-03-01 09:31:00,q,3.0,2021-03-02 09:30:00,3,False,a\nc1,a,2021-03-01 10:31:00,,1.5,,3,False,a\nc2,b,2021-03-01 09:30:00,,2.5,2021-03-02 09:30:00,3,False,a\nc2,b,2021-03-01 09:31:00,p,2.5,2021-03-02 09:30:00,,False,1\nc2,b,2021-03-01 10:31:00,r,,2021-03-02 09:30:00,3,True,1\n";
  const std::string dcl_f16 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 09:42:00,s,1.5,2021-03-01 08:00:00,,False,\nc1,c,2021-03-01 09:03:00,p,,,,,\nc2,b,2021-03-01 09:08:00,,1.5,2021-03-03 10:00:00,7,False,2\nc3,a,2021-03-01 08:28:00,s,-1.0,2021-03-02 09:30:00,2,False,\n";
  const std::string dcl_f17 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 09:04:00,q,2.5,,3,True,1\nc0,c,2021-03-01 10:04:00,s,-1.0,,1,,1\nc1,b,2021-03-01 08:37:00,q,-1.0,2021-03-02 09:30:00,3,,a\nc2,b,2021-03-01 08:28:00,,,,3,True,a\nc2,b,2021-03-01 08:33:00,s,-1.0,2021-03-01 08:00:00,7,False,\nc3,a,2021-03-01 08:13:00,p,-1.0,2021-03-02 09:30:00,3,False,a\n";
  const std::string dcl_f18 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 09:06:00,r,2.5,2021-03-01 08:00:00,2,False,b\nc1,a,2021-03-01 08:25:00,r,,2021-03-01 08:00:00,3,,b\nc1,a,2021-03-01 08:26:00,q,2.5,2021-03-01 08:00:00,2,True,a\nc2,c,2021-03-01 08:07:00,q,3.0,,,,b\n";
  const std::string dcl_f19 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 09:26:00,q,-1.0,2021-03-03 10:00:00,3,,1\nc1,c,2021-03-01 09:08:00,p,,2021-03-01 08:00:00,7,,\nc1,c,2021-03-01 10:08:00,r,3.0,2021-03-03 10:00:00,3,True,1\nc1,a,2021-03-01 10:09:00,s,-1.0,,1,False,1\n";
  const std::string dcl_f20 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 09:15:00,q,-1.0,,,False,\nc1,c,2021-03-01 08:44:00,r,3.0,2021-03-03 10:00:00,,False,b\nc1,b,2021-03-01 09:44:00,p,3.0,2021-03-03 10:00:00,1,False,a\nc2,a,2021-03-01 08:26:00,p,-1.0,2021-03-02 09:30:00,7,,a\nc3,b,2021-03-01 08:10:00,q,,2021-03-02 09:30:00,2,True,\nc4,c,2021-03-01 08:05:00,,2.5,2021-03-01 08:00:00,,False,1\nc5,b,2021-03-01 08:02:00,p,2.5,2021-03-02 09:30:00,7,True,a\nc5,c,2021-03-01 09:02:00,r,2.5,,7,,1\n";
  const std::string dcl_f21 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 08:06:00,r,2.5,,3,True,2\nc1,b,2021-03-01 08:25:00,q,3.0,2021-03-02 09:30:00,2,True,b\nc2,c,2021-03-01 08:25:00,q,1.5,,2,True,2\nc3,a,2021-03-01 09:03:00,q,3.0,2021-03-02 09:30:00,7,False,\nc4,a,2021-03-01 09:13:00,r,2.5,2021-03-02 09:30:00,3,True,1\nc4,c,2021-03-01 10:13:00,r,2.5,,,False,\nc5,c,2021-03-01 08:06:00,s,2.5,2021-03-03 10:00:00,7,True,1\n";
  const std::string dfg_f0 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:10:00,,3.0,2021-03-01 08:00:00,,True,a\nc1,a,2021-03-01 09:41:00,q,,,,True,1\nc1,a,2021-03-01 10:41:00,r,,,2,False,1\nc1,b,2021-03-01 10:46:00,,1.5,,,,a\nc2,b,2021-03-01 08:32:00,p,,2021-03-01 08:00:00,2,,b\nc2,b,2021-03-01 08:33:00,r,3.0,,,,a\nc3,c,2021-03-01 08:06:00,r,2.5,2021-03-02 09:30:00,3,False,1\n";
  const std::string dfg_f1 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:31:00,r,,2021-03-01 08:00:00,7,False,a\nc0,c,2021-03-01 08:36:00,r,,2021-03-02 09:30:00,7,False,\nc1,a,2021-03-01 08:45:00,s,2.5,2021-03-02 09:30:00,3,True,2\nc1,c,2021-03-01 08:50:00,,,2021-03-02 09:30:00,,False,b\nc1,a,2021-03-01 08:55:00,s,3.0,2021-03-03 10:00:00,1,,b\nc2,b,2021-03-01 08:32:00,s,2.5,2021-03-01 08:00:00,,False,a\nc2,b,2021-03-01 08:33:00,r,1.5,2021-03-01 08:00:00,,False,2\n";
  const std::string dfg_f2 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 08:52:00,q,1.5,,,True,a\nc1,c,2021-03-01 09:26:00,p,2.5,2021-03-01 08:00:00,1,True,b\nc1,b,2021-03-01 10:26:00,p,2.5,2021-03-02 09:30:00,1,False,1\nc1,b,2021-03-01 10:31:00,r,1.5,2021-03-02 09:30:00,2,False,1\nc1,c,2021-03-01 10:32:00,,1.5,2021-03-01 08:00:00,,,\nc2,a,2021-03-01 09:16:00,r,1.5,,2,True,1\nc2,a,2021-03-01 10:16:00,p,3.0,2021-03-02 09:30:00,,False,b\nc3,a,2021-03-01 09:43:00,q,2.5,2021-03-01 08:00:00,,True,1\nc4,b,2021-03-01 08:52:00,,,,2,,\nc4,a,2021-03-01 08:53:00,r,1.5,,3,True,1\nc4,c,2021-03-01 08:54:00,,2.5,2021-03-02 09:30:00,3,False,1\nc4,c,2021-03-01 09:54:00,p,2.5,2021-03-01 08:00:00,1,False,\n";
  const std::string dfg_f3 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:37:00,r,-1.0,,2,False,a\nc0,c,2021-03-01 09:37:00,q,,,,False,b\nc0,b,2021-03-01 09:38:00,s,,2021-03-01 08:00:00,3,False,2\nc0,b,2021-03-01 09:39:00,,,2021-03-03 10:00:00,7,True,b\nc1,b,2021-03-01 08:03:00,s,,2021-03-01 08:00:00,,True,1\nc1,a,2021-03-01 08:04:00,q,3.0,2021-03-03 10:00:00,1,False,2\nc1,a,2021-03-01 09:04:00,q,2.5,,,True,1\nc1,b,2021-03-01 09:05:00,,-1.0,2021-03-01 08:00:00,2,True,a\nc2,c,2021-03-01 08:22:00,s,,2021-03-01 08:00:00,3,True,2\nc2,b,2021-03-01 08:27:00,p,,2021-03-01 08:00:00,,True,2\nc2,b,2021-03-01 08:32:00,r,1.5,2021-03-01 08:00:00,7,True,1\nc2,b,2021-03-01 09:32:00,r,2.5,2021-03-01 08:00:00,7,False,2\nc3,a,2021-03-01 08:26:00,,,2021-03-03 10:00:00,1,True,1\n";
  const std::string dfg_f4 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 09:38:00,r,3.0,2021-03-01 08:00:00,,True,b\nc0,c,2021-03-01 09:39:00,q,1.5,2021-03-01 08:00:00,2,False,a\nc1,b,2021-03-01 09:37:00,r,2.5,2021-03-01 08:00:00,3,False,\nc1,c,2021-03-01 09:42:00,q,,2021-03-01 08:00:00,2,False,b\nc1,a,2021-03-01 09:43:00,,2.5,2021-03-02 09:30:00,,True,\nc2,c,2021-03-01 08:10:00,r,3.0,2021-03-02 09:30:00,,False,1\nc2,c,2021-03-01 08:11:00,q,,2021-03-02 09:30:00,,,\nc2,c,2021-03-01 08:16:00,,,2021-03-01 08:00:00,2,False,a\nc3,a,2021-03-01 08:11:00,,1.5,,,,a\nc4,c,2021-03-01 08:03:00,r,,,2,True,\n";
  const std::string dfg_f5 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 09:24:00,q,,,,,a\nc0,b,2021-03-01 09:25:00,r,2.5,,1,,a\nc1,a,2021-03-01 08:28:00,r,,2021-03-02 09:30:00,1,,\nc2,c,2021-03-01 08:30:00,,3.0,2021-03-01 08:00:00,3,,1\nc2,a,2021-03-01 09:30:00,,1.5,2021-03-02 09:30:00,,True,b\nc2,a,2021-03-01 10:30:00,r,1.5,2021-03-02 09:30:00,1,False,b\nc2,b,2021-03-01 10:31:00,r,3.0,,3,False,1\n";
  const std::string dfg_f6 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 08:13:00,q,,2021-03-02 09:30:00,3,True,a\nc1,b,2021-03-01 09:50:00,p,2.5,2021-03-01 08:00:00,3,,b\nc1,b,2021-03-01 09:55:00,p,,2021-03-01 08:00:00,,,1\nc2,a,2021-03-01 08:27:00,p,3.0,,3,False,a\nc2,c,2021-03-01 09:27:00,q,,2021-03-01 08:00:00,,,b\nc3,c,2021-03-01 08:41:00,r,2.5,,2,,b\n";
  const std::string dfg_f7 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 08:46:00,q,,,2,True,a\nc1,a,2021-03-01 08:03:00,p,,2021-03-01 08:00:00,2,,a\nc1,c,2021-03-01 09:03:00,,2.5,,2,False,1\nc2,b,2021-03-01 08:24:00,q,,,2,False,1\nc2,a,2021-03-01 08:29:00,q,2.5,2021-03-01 08:00:00,2,True,\nc2,b,2021-03-01 09:29:00,p,2.5,2021-03-01 08:00:00,1,True,1\nc2,b,2021-03-01 10:29:00,,1.5,,2,True,1\nc3,b,2021-03-01 08:16:00,p,2.5,2021-03-01 08:00:00,,,a\nc3,c,2021-03-01 09:16:00,p,1.5,2021-03-01 08:00:00,2,,\nc3,a,2021-03-01 09:17:00,,2.5,2021-03-01 08:00:00,,True,\nc3,c,2021-03-01 09:22:00,q,1.5,2021-03-01 08:00:00,,True,\nc4,c,2021-03-01 08:52:00,q,1.5,2021-03-01 08:00:00,,True,\n";
  const std::string dfg_f8 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 08:41:00,q,2.5,2021-03-01 08:00:00,1,False,a\nc1,b,2021-03-01 08:07:00,q,,2021-03-01 08:00:00,,,\nc1,b,2021-03-01 09:07:00,,1.5,2021-03-01 08:00:00,1,False,\nc1,c,2021-03-01 09:08:00,,,,2,False,1\nc1,c,2021-03-01 09:13:00,p,1.5,,1,True,\nc2,c,2021-03-01 08:04:00,,2.5,2021-03-01 08:00:00,2,True,a\n";
  const std::string dfg_f9 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 08:05:00,q,1.5,2021-03-02 09:30:00,3,True,\nc0,c,2021-03-01 09:05:00,r,2.5,2021-03-02 09:30:00,3,True,1\nc0,c,2021-03-01 09:06:00,p,,2021-03-02 09:30:00,7,False,2\nc0,c,2021-03-01 10:06:00,s,,,7,True,1\nc1,b,2021-03-01 08:36:00,q,2.5,2021-03-02 09:30:00,,True,b\nc2,b,2021-03-01 08:22:00,,-1.0,,1,,2\nc2,c,2021-03-01 08:27:00,s,,2021-03-01 08:00:00,7,,\nc2,c,2021-03-01 09:27:00,,1.5,2021-03-02 09:30:00,2,True,\nc3,a,2021-03-01 08:10:00,s,,2021-03-02 09:30:00,1,True,a\nc4,a,2021-03-01 08:26:00,p,2.5,2021-03-01 08:00:00,2,,1\nc4,b,2021-03-01 08:27:00,q,-1.0,2021-03-01 08:00:00,7,True,a\nc4,a,2021-03-01 08:32:00,s,,,3,False,2\nc4,a,2021-03-01 08:33:00,p,,2021-03-01 08:00:00,,,a\nc5,a,2021-03-01 09:48:00,q,,,1,,\nc5,b,2021-03-01 09:53:00,q,2.5,,2,False,b\n";
  const std::string dfg_f10 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 09:41:00,p,1.5,,2,,b\nc0,b,2021-03-01 10:41:00,s,3.0,2021-03-03 10:00:00,,True,1\nc1,c,2021-03-01 08:47:00,,3.0,2021-03-01 08:00:00,,False,1\nc2,c,2021-03-01 08:30:00,r,2.5,,3,,2\nc3,c,2021-03-01 08:37:00,,-1.0,,2,False,b\nc4,a,2021-03-01 09:20:00,q,,,2,False,1\nc4,c,2021-03-01 09:21:00,s,,2021-03-01 08:00:00,,True,b\nc4,a,2021-03-01 09:26:00,q,1.5,,,True,b\nc4,a,2021-03-01 09:27:00,q,1.5,2021-03-02 09:30:00,1,False,2\n";
  const std::string dfg_f11 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 09:16:00,s,,2021-03-03 10:00:00,7,False,b\nc1,a,2021-03-01 08:44:00,,-1.0,2021-03-01 08:00:00,3,,a\nc1,a,2021-03-01 09:44:00,,,2021-03-02 09:30:00,1,True,\nc1,a,2021-03-01 09:49:00,p,3.0,2021-03-03 10:00:00,1,,1\nc2,a,2021-03-01 09:29:00,p,2.5,,3,True,\nc2,c,2021-03-01 09:34:00,,2.5,,,False,\nc3,b,2021-03-01 08:41:00,,3.0,,,,a\nc3,c,2021-03-01 09:41:00,s,3.0,2021-03-03 10:00:00,1,False,a\nc3,c,2021-03-01 10:41:00,q,,2021-03-02 09:30:00,,False,b\nc3,c,2021-03-01 10:46:00,r,3.0,2021-03-03 10:00:00,3,,\n";
  const std::string dfg_f12 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 08:43:00,q,,,3,True,1\nc1,c,2021-03-01 09:15:00,q,3.0,,3,False,1\nc1,b,2021-03-01 10:15:00,,,2021-03-01 08:00:00,1,True,\nc2,b,2021-03-01 08:32:00,p,3.0,2021-03-01 08:00:00,1,True,1\nc2,b,2021-03-01 09:32:00,,,2021-03-01 08:00:00,,,1\nc3,a,2021-03-01 08:06:00,p,1.5,2021-03-02 09:30:00,,False,1\nc3,b,2021-03-01 09:06:00,,,2021-03-01 08:00:00,,False,a\nc4,b,2021-03-01 09:34:00,q,1.5,2021-03-02 09:30:00,,True,\nc4,c,2021-03-01 09:35:00,p,1.5,,2,False,1\nc4,c,2021-03-01 09:40:00,r,3.0,2021-03-01 08:00:00,1,,b\nc4,b,2021-03-01 10:40:00,q,2.5,2021-03-01 08:00:00,3,True,a\nc5,c,2021-03-01 08:25:00,r,,2021-03-01 08:00:00,1,False,b\nc5,b,2021-03-01 08:26:00,,1.5,2021-03-01 08:00:00,3,True,1\n";
  const std::string dfg_f13 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 08:13:00,,1.5,2021-03-01 08:00:00,,True,a\nc0,c,2021-03-01 08:18:00,p,1.5,2021-03-01 08:00:00,1,False,a\nc1,a,2021-03-01 08:53:00,,2.5,,,,1\nc2,c,2021-03-01 08:31:00,,1.5,2021-03-01 08:00:00,,False,\nc2,c,2021-03-01 08:36:00,,1.5,2021-03-01 08:00:00,1,False,a\nc2,b,2021-03-01 09:36:00,,2.5,2021-03-01 08:00:00,1,True,a\nc2,b,2021-03-01 09:37:00,q,,2021-03-01 08:00:00,,,a\nc3,c,2021-03-01 09:10:00,q,2.5,2021-03-01 08:00:00,1,,1\nc3,c,2021-03-01 09:11:00,q,,,2,True,a\nc4,c,2021-03-01 08:10:00,q,1.5,2021-03-01 08:00:00,1,False,a\nc5,b,2021-03-01 08:36:00,p,,,2,,1\n";
  const std::string ts_f0 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"c\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-01T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><string key=\"k\" value=\"y\"/><date key=\"time:timestamp\" value=\"2021-01-01T01:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"a\"/><int key=\"k\" value=\"0\"/><date key=\"time:timestamp\" value=\"2021-01-02T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><int key=\"k\" value=\"0\"/><date key=\"time:timestamp\" value=\"2021-01-02T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-02T02:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><int key=\"k\" value=\"0\"/><date key=\"time:timestamp\" value=\"2021-01-02T03:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><int key=\"k\" value=\"0\"/><date key=\"time:timestamp\" value=\"2021-01-02T04:00:00+00:00\"/></event>\n</trace>\n</log>\n";
  const std::string ts_f1 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"c\"/><boolean key=\"k\" value=\"false\"/><date key=\"time:timestamp\" value=\"2021-01-01T00:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"c\"/><int key=\"k\" value=\"0\"/><date key=\"time:timestamp\" value=\"2021-01-02T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><int key=\"k\" value=\"0\"/><date key=\"time:timestamp\" value=\"2021-01-02T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><int key=\"k\" value=\"1\"/><date key=\"time:timestamp\" value=\"2021-01-02T02:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><int key=\"k\" value=\"0\"/><date key=\"time:timestamp\" value=\"2021-01-03T00:00:00+00:00\"/></event>\n</trace>\n</log>\n";
  const std::string ts_f2 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"c\"/><float key=\"k\" value=\"1.0\"/><date key=\"time:timestamp\" value=\"2021-01-01T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-01T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-01T02:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><float key=\"k\" value=\"1.0\"/><date key=\"time:timestamp\" value=\"2021-01-01T03:00:00+00:00\"/></event>\n</trace>\n</log>\n";
  const std::string ts_f3 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"c\"/><int key=\"k\" value=\"1\"/><date key=\"time:timestamp\" value=\"2021-01-01T00:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"a\"/><float key=\"k\" value=\"1.5\"/><date key=\"time:timestamp\" value=\"2021-01-02T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><int key=\"k\" value=\"1\"/><date key=\"time:timestamp\" value=\"2021-01-02T01:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><string key=\"k\" value=\"x\"/><date key=\"time:timestamp\" value=\"2021-01-03T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><int key=\"k\" value=\"2\"/><date key=\"time:timestamp\" value=\"2021-01-03T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><int key=\"k\" value=\"1\"/><date key=\"time:timestamp\" value=\"2021-01-03T02:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"c\"/><int key=\"k\" value=\"2\"/><date key=\"time:timestamp\" value=\"2021-01-04T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><int key=\"k\" value=\"2\"/><date key=\"time:timestamp\" value=\"2021-01-04T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><int key=\"k\" value=\"1\"/><date key=\"time:timestamp\" value=\"2021-01-04T02:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><float key=\"k\" value=\"1.5\"/><date key=\"time:timestamp\" value=\"2021-01-04T03:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c4\"/>\n<event><string key=\"concept:name\" value=\"c\"/><int key=\"k\" value=\"1\"/><date key=\"time:timestamp\" value=\"2021-01-05T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><string key=\"k\" value=\"x\"/><date key=\"time:timestamp\" value=\"2021-01-05T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><string key=\"k\" value=\"x\"/><date key=\"time:timestamp\" value=\"2021-01-05T02:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><int key=\"k\" value=\"2\"/><date key=\"time:timestamp\" value=\"2021-01-05T03:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><int key=\"k\" value=\"2\"/><date key=\"time:timestamp\" value=\"2021-01-05T04:00:00+00:00\"/></event>\n</trace>\n</log>\n";
  const std::string ts_f4 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"c\"/><int key=\"k\" value=\"0\"/><date key=\"time:timestamp\" value=\"2021-01-01T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><boolean key=\"k\" value=\"false\"/><date key=\"time:timestamp\" value=\"2021-01-01T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><string key=\"k\" value=\"1\"/><date key=\"time:timestamp\" value=\"2021-01-01T02:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><int key=\"k\" value=\"2\"/><date key=\"time:timestamp\" value=\"2021-01-01T03:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><string key=\"k\" value=\"y\"/><date key=\"time:timestamp\" value=\"2021-01-01T04:00:00+00:00\"/></event>\n</trace>\n</log>\n";
  const std::string ts_f5 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><boolean key=\"k\" value=\"true\"/><date key=\"time:timestamp\" value=\"2021-01-01T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><boolean key=\"k\" value=\"false\"/><date key=\"time:timestamp\" value=\"2021-01-01T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-01T02:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><string key=\"k\" value=\"y\"/><date key=\"time:timestamp\" value=\"2021-01-01T03:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"a\"/><boolean key=\"k\" value=\"true\"/><date key=\"time:timestamp\" value=\"2021-01-02T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><string key=\"k\" value=\"y\"/><date key=\"time:timestamp\" value=\"2021-01-02T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><string key=\"k\" value=\"y\"/><date key=\"time:timestamp\" value=\"2021-01-02T02:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-02T03:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"c\"/><boolean key=\"k\" value=\"false\"/><date key=\"time:timestamp\" value=\"2021-01-03T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><string key=\"k\" value=\"y\"/><date key=\"time:timestamp\" value=\"2021-01-03T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-03T02:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><boolean key=\"k\" value=\"true\"/><date key=\"time:timestamp\" value=\"2021-01-03T03:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><boolean key=\"k\" value=\"true\"/><date key=\"time:timestamp\" value=\"2021-01-03T04:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"b\"/><string key=\"k\" value=\"y\"/><date key=\"time:timestamp\" value=\"2021-01-04T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-04T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-04T02:00:00+00:00\"/></event>\n</trace>\n</log>\n";
  const std::string ts_f6 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"a\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-01T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-01T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-01T02:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-01T03:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><int key=\"k\" value=\"0\"/><date key=\"time:timestamp\" value=\"2021-01-01T04:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"c\"/><int key=\"k\" value=\"0\"/><date key=\"time:timestamp\" value=\"2021-01-02T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><float key=\"k\" value=\"1.5\"/><date key=\"time:timestamp\" value=\"2021-01-02T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><float key=\"k\" value=\"1.5\"/><date key=\"time:timestamp\" value=\"2021-01-02T02:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><float key=\"k\" value=\"1.5\"/><date key=\"time:timestamp\" value=\"2021-01-03T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><boolean key=\"k\" value=\"true\"/><date key=\"time:timestamp\" value=\"2021-01-03T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-03T02:00:00+00:00\"/></event>\n</trace>\n</log>\n";
  const std::string ts_f7 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><boolean key=\"k\" value=\"true\"/><date key=\"time:timestamp\" value=\"2021-01-01T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><boolean key=\"k\" value=\"true\"/><date key=\"time:timestamp\" value=\"2021-01-01T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><float key=\"k\" value=\"1.0\"/><date key=\"time:timestamp\" value=\"2021-01-01T02:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><boolean key=\"k\" value=\"true\"/><date key=\"time:timestamp\" value=\"2021-01-01T03:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"c\"/><int key=\"k\" value=\"0\"/><date key=\"time:timestamp\" value=\"2021-01-02T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><float key=\"k\" value=\"1.0\"/><date key=\"time:timestamp\" value=\"2021-01-02T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-02T02:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><boolean key=\"k\" value=\"false\"/><date key=\"time:timestamp\" value=\"2021-01-02T03:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-02T04:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"c\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-03T00:00:00+00:00\"/></event>\n</trace>\n</log>\n";
  const std::string ts_f8 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"c\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-01T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-01T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><boolean key=\"k\" value=\"false\"/><date key=\"time:timestamp\" value=\"2021-01-01T02:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"c\"/><float key=\"k\" value=\"1.5\"/><date key=\"time:timestamp\" value=\"2021-01-02T00:00:00+00:00\"/></event>\n</trace>\n</log>\n";
  const std::string ts_f9 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"c\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-01T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-01T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-01T02:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><int key=\"k\" value=\"0\"/><date key=\"time:timestamp\" value=\"2021-01-01T03:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><string key=\"k\" value=\"1\"/><date key=\"time:timestamp\" value=\"2021-01-01T04:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"a\"/><int key=\"k\" value=\"0\"/><date key=\"time:timestamp\" value=\"2021-01-02T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-02T01:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"c\"/><int key=\"k\" value=\"0\"/><date key=\"time:timestamp\" value=\"2021-01-03T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><int key=\"k\" value=\"0\"/><date key=\"time:timestamp\" value=\"2021-01-03T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-03T02:00:00+00:00\"/></event>\n</trace>\n</log>\n";
  const std::string ts_f10 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><boolean key=\"k\" value=\"true\"/><date key=\"time:timestamp\" value=\"2021-01-01T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><int key=\"k\" value=\"0\"/><date key=\"time:timestamp\" value=\"2021-01-01T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><boolean key=\"k\" value=\"true\"/><date key=\"time:timestamp\" value=\"2021-01-01T02:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><float key=\"k\" value=\"1.5\"/><date key=\"time:timestamp\" value=\"2021-01-01T03:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"c\"/><boolean key=\"k\" value=\"true\"/><date key=\"time:timestamp\" value=\"2021-01-02T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><boolean key=\"k\" value=\"false\"/><date key=\"time:timestamp\" value=\"2021-01-02T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><int key=\"k\" value=\"0\"/><date key=\"time:timestamp\" value=\"2021-01-02T02:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><boolean key=\"k\" value=\"true\"/><date key=\"time:timestamp\" value=\"2021-01-02T03:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><string key=\"k\" value=\"x\"/><date key=\"time:timestamp\" value=\"2021-01-03T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><int key=\"k\" value=\"0\"/><date key=\"time:timestamp\" value=\"2021-01-03T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><int key=\"k\" value=\"2\"/><date key=\"time:timestamp\" value=\"2021-01-03T02:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><int key=\"k\" value=\"2\"/><date key=\"time:timestamp\" value=\"2021-01-03T03:00:00+00:00\"/></event>\n</trace>\n</log>\n";
  const std::string ts_f11 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-01T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><string key=\"k\" value=\"y\"/><date key=\"time:timestamp\" value=\"2021-01-01T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><string key=\"k\" value=\"y\"/><date key=\"time:timestamp\" value=\"2021-01-01T02:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-01T03:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><boolean key=\"k\" value=\"false\"/><date key=\"time:timestamp\" value=\"2021-01-01T04:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"c\"/><float key=\"k\" value=\"1.0\"/><date key=\"time:timestamp\" value=\"2021-01-02T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-02T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-02T02:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-02T03:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-02T04:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"c\"/><string key=\"k\" value=\"y\"/><date key=\"time:timestamp\" value=\"2021-01-03T00:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"c\"/><string key=\"k\" value=\"y\"/><date key=\"time:timestamp\" value=\"2021-01-04T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><float key=\"k\" value=\"1.0\"/><date key=\"time:timestamp\" value=\"2021-01-04T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><string key=\"k\" value=\"y\"/><date key=\"time:timestamp\" value=\"2021-01-04T02:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c4\"/>\n<event><string key=\"concept:name\" value=\"c\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-05T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><string key=\"k\" value=\"y\"/><date key=\"time:timestamp\" value=\"2021-01-05T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><string key=\"k\" value=\"y\"/><date key=\"time:timestamp\" value=\"2021-01-05T02:00:00+00:00\"/></event>\n</trace>\n</log>\n";
  const std::string ts_f12 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"a\"/><int key=\"k\" value=\"2\"/><date key=\"time:timestamp\" value=\"2021-01-01T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><string key=\"k\" value=\"1\"/><date key=\"time:timestamp\" value=\"2021-01-01T01:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"b\"/><int key=\"k\" value=\"0\"/><date key=\"time:timestamp\" value=\"2021-01-02T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><int key=\"k\" value=\"0\"/><date key=\"time:timestamp\" value=\"2021-01-02T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><int key=\"k\" value=\"1\"/><date key=\"time:timestamp\" value=\"2021-01-02T02:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><boolean key=\"k\" value=\"false\"/><date key=\"time:timestamp\" value=\"2021-01-02T03:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><int key=\"k\" value=\"2\"/><date key=\"time:timestamp\" value=\"2021-01-02T04:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"c\"/><int key=\"k\" value=\"1\"/><date key=\"time:timestamp\" value=\"2021-01-03T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><string key=\"k\" value=\"1\"/><date key=\"time:timestamp\" value=\"2021-01-03T01:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"c\"/><int key=\"k\" value=\"1\"/><date key=\"time:timestamp\" value=\"2021-01-04T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><int key=\"k\" value=\"2\"/><date key=\"time:timestamp\" value=\"2021-01-04T01:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c4\"/>\n<event><string key=\"concept:name\" value=\"a\"/><string key=\"k\" value=\"1\"/><date key=\"time:timestamp\" value=\"2021-01-05T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><boolean key=\"k\" value=\"false\"/><date key=\"time:timestamp\" value=\"2021-01-05T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><string key=\"k\" value=\"1\"/><date key=\"time:timestamp\" value=\"2021-01-05T02:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><int key=\"k\" value=\"2\"/><date key=\"time:timestamp\" value=\"2021-01-05T03:00:00+00:00\"/></event>\n</trace>\n</log>\n";
  const std::string ts_f13 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"c\"/><string key=\"k\" value=\"y\"/><date key=\"time:timestamp\" value=\"2021-01-01T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-01T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-01T02:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"a\"/><float key=\"k\" value=\"1.0\"/><date key=\"time:timestamp\" value=\"2021-01-02T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><float key=\"k\" value=\"1.0\"/><date key=\"time:timestamp\" value=\"2021-01-02T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><int key=\"k\" value=\"1\"/><date key=\"time:timestamp\" value=\"2021-01-02T02:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><int key=\"k\" value=\"1\"/><date key=\"time:timestamp\" value=\"2021-01-02T03:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><int key=\"k\" value=\"1\"/><date key=\"time:timestamp\" value=\"2021-01-02T04:00:00+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"a\"/><float key=\"k\" value=\"1.0\"/><date key=\"time:timestamp\" value=\"2021-01-03T00:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><float key=\"k\" value=\"1.0\"/><date key=\"time:timestamp\" value=\"2021-01-03T01:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-03T02:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><float key=\"k\" value=\"0.0\"/><date key=\"time:timestamp\" value=\"2021-01-03T03:00:00+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><int key=\"k\" value=\"1\"/><date key=\"time:timestamp\" value=\"2021-01-03T04:00:00+00:00\"/></event>\n</trace>\n</log>\n";
  const std::vector<Case> cases = {
      {"tpl def t68500.csv", "t68500.csv", tpl_f0, "tpl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "P c>a=420/0,c>c=180/0,c>d=210/127.279,d>b=60/0"},
      {"tpl sst t68500.csv", "t68500.csv", tpl_f0, "tpl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: 'bool' object has no attribute 'timestamp'"},
      {"tpl t_st t68500.csv", "t68500.csv", tpl_f0, "tpl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: 'bool' object has no attribute 'timestamp'"},
      {"tpl bh_sst t68500.csv", "t68500.csv", tpl_f0, "tpl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: 'bool' object has no attribute 'timestamp'"},
      {"tpl szzz t68500.csv", "t68500.csv", tpl_f0, "tpl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_target_timestamp_attribute", "zzz"}}, "error: KeyError: 'zzz'"},
      {"tpl def t68501.csv", "t68501.csv", tpl_f1, "tpl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "P a>a=72000/0,a>c=180000/50911.7,c>a=108000/50911.7,c>c=288000/0"},
      {"tpl sst t68501.csv", "t68501.csv", tpl_f1, "tpl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: 'int' object has no attribute 'timestamp'"},
      {"tpl t_st t68501.csv", "t68501.csv", tpl_f1, "tpl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: 'int' object has no attribute 'timestamp'"},
      {"tpl bh_sst t68501.csv", "t68501.csv", tpl_f1, "tpl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: 'int' object has no attribute 'timestamp'"},
      {"tpl szzz t68501.csv", "t68501.csv", tpl_f1, "tpl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_target_timestamp_attribute", "zzz"}}, "error: KeyError: 'zzz'"},
      {"tpl def t68502.csv", "t68502.csv", tpl_f2, "tpl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "P "},
      {"tpl sst t68502.csv", "t68502.csv", tpl_f2, "tpl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "P "},
      {"tpl t_st t68502.csv", "t68502.csv", tpl_f2, "tpl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "P "},
      {"tpl bh_sst t68502.csv", "t68502.csv", tpl_f2, "tpl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "P "},
      {"tpl sst t68503.csv", "t68503.csv", tpl_f3, "tpl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: 'int' object has no attribute 'timestamp'"},
      {"tpl t_st t68503.csv", "t68503.csv", tpl_f3, "tpl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: 'int' object has no attribute 'timestamp'"},
      {"tpl bh_sst t68503.csv", "t68503.csv", tpl_f3, "tpl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: 'int' object has no attribute 'timestamp'"},
      {"tpl szzz t68503.csv", "t68503.csv", tpl_f3, "tpl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_target_timestamp_attribute", "zzz"}}, "error: KeyError: 'zzz'"},
      {"tcl def t68500.csv", "t68500.csv", tcl_f0, "tcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "C [];[];[c>d=300/2.9,c>d=120/1.1]"},
      {"tcl sst t68500.csv", "t68500.csv", tcl_f0, "tcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"a:temporal_target_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: 'bool' object has no attribute 'timestamp'"},
      {"tcl t_st t68500.csv", "t68500.csv", tcl_f0, "tcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"a:temporal_source_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: 'bool' object has no attribute 'timestamp'"},
      {"tcl zeta t68500.csv", "t68500.csv", tcl_f0, "tcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"a:temporal_zeta", "0.5"}}, "C [];[];[c>d=300/2.9,c>d=120/1.1]"},
      {"tcl def t68501.csv", "t68501.csv", tcl_f1, "tcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "C [a>a=72000/72000,a>c=216000/9.22337e+18,a>c=144000/9.22337e+18]"},
      {"tcl sst t68501.csv", "t68501.csv", tcl_f1, "tcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"a:temporal_target_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: 'int' object has no attribute 'timestamp'"},
      {"tcl t_st t68501.csv", "t68501.csv", tcl_f1, "tcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"a:temporal_source_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: 'int' object has no attribute 'timestamp'"},
      {"tcl zeta t68501.csv", "t68501.csv", tcl_f1, "tcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"a:temporal_zeta", "0.5"}}, "C [a>a=72000/72000,a>c=216000/9.22337e+18,a>c=144000/9.22337e+18]"},
      {"tcl def t68502.csv", "t68502.csv", tcl_f2, "tcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "C []"},
      {"tcl sst t68502.csv", "t68502.csv", tcl_f2, "tcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"a:temporal_target_timestamp_attribute", "start_timestamp"}}, "C []"},
      {"tcl sst t68503.csv", "t68503.csv", tcl_f3, "tcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"a:temporal_target_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: 'int' object has no attribute 'timestamp'"},
      {"tcl t_st t68503.csv", "t68503.csv", tcl_f3, "tcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"a:temporal_source_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: 'int' object has no attribute 'timestamp'"},
      {"tpd def t68500.csv", "t68500.csv", tpd_f0, "tpd", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"tpd sst t68500.csv", "t68500.csv", tpd_f0, "tpd", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"tpd t_st t68500.csv", "t68500.csv", tpd_f0, "tpd", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: TypeError: numpy boolean subtract, the `-` operator, is not supported, use the bitwise_xor, the `^` operator, or the logical_xor function instead."},
      {"tpd bh_t_st t68500.csv", "t68500.csv", tpd_f0, "tpd", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_business_hours", "true"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: 'bool' object has no attribute 'replace'"},
      {"tpd def t68501.csv", "t68501.csv", tpd_f1, "tpd", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "error: TypeError: '<=' not supported between instances of 'Timestamp' and 'int'"},
      {"tpd sst t68501.csv", "t68501.csv", tpd_f1, "tpd", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: TypeError: '<=' not supported between instances of 'Timestamp' and 'int'"},
      {"tpd t_st t68501.csv", "t68501.csv", tpd_f1, "tpd", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: Can only use .dt accessor with datetimelike values"},
      {"tpd bh_t_st t68501.csv", "t68501.csv", tpd_f1, "tpd", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_business_hours", "true"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: 'int' object has no attribute 'replace'"},
      {"tpd bh_t_st t68503.csv", "t68503.csv", tpd_f2, "tpd", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_business_hours", "true"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: 'int' object has no attribute 'replace'"},
      {"tpd def t68505.csv", "t68505.csv", tpd_f3, "tpd", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "P a>a=240/254.558,a>d=0/0,c>a=60/0,c>c=120/0,d>a=240/0"},
      {"tpd sst t68505.csv", "t68505.csv", tpd_f3, "tpd", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "P a>a=240/254.558,a>d=0/0,c>a=60/0,c>c=120/0,d>a=240/0"},
      {"tpd def t68508.csv", "t68508.csv", tpd_f4, "tpd", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "error: TypeError: '<=' not supported between instances of 'Timestamp' and 'int'"},
      {"tpd sst t68508.csv", "t68508.csv", tpd_f4, "tpd", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: TypeError: '<=' not supported between instances of 'Timestamp' and 'int'"},
      {"tpd t_st t68508.csv", "t68508.csv", tpd_f4, "tpd", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: Can only use .dt accessor with datetimelike values"},
      {"tpd bh_t_st t68508.csv", "t68508.csv", tpd_f4, "tpd", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_business_hours", "true"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: 'int' object has no attribute 'replace'"},
      {"tpd def t68510.csv", "t68510.csv", tpd_f5, "tpd", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and StringArray"},
      {"tpd sst t68510.csv", "t68510.csv", tpd_f5, "tpd", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and StringArray"},
      {"tpd t_st t68510.csv", "t68510.csv", tpd_f5, "tpd", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: TypeError: unsupported operand type(s) for -: 'str' and 'str'"},
      {"dcl K2 k68000.csv", "k68000.csv", dcl_f0, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K2"}}, "error: TypeError: 'float' object is not subscriptable"},
      {"dcl K3 k68000.csv", "k68000.csv", dcl_f0, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K3"}}, "error: TypeError: 'Timestamp' object is not subscriptable"},
      {"dcl K4 k68000.csv", "k68000.csv", dcl_f0, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K4"}}, "error: TypeError: 'float' object is not subscriptable"},
      {"dcl K5 k68000.csv", "k68000.csv", dcl_f0, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K5"}}, "error: TypeError: cannot unpack non-iterable float object"},
      {"dcl K6 k68000.csv", "k68000.csv", dcl_f0, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K6"}}, "M nonchainsuccession:1>a,nonchainsuccession:a>1,nonsuccession:a>1 | fit T,T,T,T"},
      {"dcl K2 k68001.csv", "k68001.csv", dcl_f1, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K2"}}, "error: TypeError: 'float' object is not subscriptable"},
      {"dcl K3 k68001.csv", "k68001.csv", dcl_f1, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K3"}}, "M nonchainsuccession:2021-03-01 08:00:00+00:00>2021-03-03 10:00:00+00:00,nonchainsuccession:2021-03-02 09:30:00+00:00>2021-03-01 08:00:00+00:00,nonchainsuccession:2021-03-03 10:00:00+00:00>2021-03-01 08:00:00+00:00,noncoexistence:2021-03-01 08:00:00+00:00>2021-03-03 10:00:00+00:00,noncoexistence:2021-03-03 10:00:00+00:00>2021-03-01 08:00:00+00:00,nonsuccession:2021-03-01 08:00:00+00:00>2021-03-03 10:00:00+00:00,nonsuccession:2021-03-02 09:30:00+00:00>2021-03-01 08:00:00+00:00,nonsuccession:2021-03-03 10:00:00+00:00>2021-03-01 08:00:00+00:00 | fit T,T,T"},
      {"dcl K4 k68001.csv", "k68001.csv", dcl_f1, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K4"}}, "error: TypeError: 'float' object is not subscriptable"},
      {"dcl K5 k68001.csv", "k68001.csv", dcl_f1, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K5"}}, "error: TypeError: cannot unpack non-iterable float object"},
      {"dcl K6 k68001.csv", "k68001.csv", dcl_f1, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K6"}}, "error-class: TypeError"},
      {"dcl K2 k68002.csv", "k68002.csv", dcl_f2, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K2"}}, "error: TypeError: cannot unpack non-iterable float object"},
      {"dcl K3 k68002.csv", "k68002.csv", dcl_f2, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K3"}}, "error: TypeError: 'Timestamp' object is not subscriptable"},
      {"dcl K4 k68002.csv", "k68002.csv", dcl_f2, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K4"}}, "error: TypeError: 'float' object is not subscriptable"},
      {"dcl K5 k68002.csv", "k68002.csv", dcl_f2, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K5"}}, "error: TypeError: cannot unpack non-iterable bool object"},
      {"dcl K6 k68002.csv", "k68002.csv", dcl_f2, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K6"}}, "error: TypeError: '<' not supported between instances of 'str' and 'float'"},
      {"dcl K1 k68003.csv", "k68003.csv", dcl_f3, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K1"}}, "error: TypeError: '<' not supported between instances of 'str' and 'float'"},
      {"dcl K3 k68003.csv", "k68003.csv", dcl_f3, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K3"}}, "error: TypeError: 'Timestamp' object is not subscriptable"},
      {"dcl K1 k68004.csv", "k68004.csv", dcl_f4, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K1"}}, "error: TypeError: '<' not supported between instances of 'str' and 'float'"},
      {"dcl K2 k68004.csv", "k68004.csv", dcl_f4, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K2"}}, "M nonchainsuccession:2.5>1.5,nonsuccession:2.5>1.5 | fit T,T,T,T"},
      {"dcl K3 k68004.csv", "k68004.csv", dcl_f4, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K3"}}, "M nonchainsuccession:2021-03-02 09:30:00+00:00>2021-03-01 08:00:00+00:00,nonsuccession:2021-03-02 09:30:00+00:00>2021-03-01 08:00:00+00:00 | fit T,T,T,T"},
      {"dcl K2 k68005.csv", "k68005.csv", dcl_f5, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K2"}}, "M nonchainsuccession:1.5>3.0,nonchainsuccession:3.0>1.5,noncoexistence:1.5>3.0,noncoexistence:3.0>1.5,nonsuccession:1.5>3.0,nonsuccession:3.0>1.5 | fit T,T,T,T"},
      {"dcl K1 k68006.csv", "k68006.csv", dcl_f6, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K1"}}, "M existence:r,init:r,nonchainsuccession:q>r,nonsuccession:q>r | fit T,T"},
      {"dcl K3 k68006.csv", "k68006.csv", dcl_f6, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K3"}}, "M exactly_one:2021-03-02 09:30:00+00:00,existence:2021-03-02 09:30:00+00:00,nonchainsuccession:2021-03-01 08:00:00+00:00>2021-03-02 09:30:00+00:00,nonchainsuccession:2021-03-02 09:30:00+00:00>2021-03-01 08:00:00+00:00,nonsuccession:2021-03-01 08:00:00+00:00>2021-03-02 09:30:00+00:00,nonsuccession:2021-03-02 09:30:00+00:00>2021-03-01 08:00:00+00:00 | fit T,T"},
      {"dcl K6 k68006.csv", "k68006.csv", dcl_f6, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K6"}}, "error-class: TypeError"},
      {"dcl K1 k68007.csv", "k68007.csv", dcl_f7, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K1"}}, "error: TypeError: '<' not supported between instances of 'str' and 'float'"},
      {"dcl K2 k68007.csv", "k68007.csv", dcl_f7, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K2"}}, "M nonchainsuccession:2.5>3.0,nonchainsuccession:3.0>2.5,noncoexistence:2.5>3.0,noncoexistence:3.0>2.5,nonsuccession:2.5>3.0,nonsuccession:3.0>2.5 | fit T,T,T"},
      {"dcl K4 k68007.csv", "k68007.csv", dcl_f7, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K4"}}, "M nonchainsuccession:1>3.0,nonchainsuccession:3>1.0,noncoexistence:1>3.0,noncoexistence:3>1.0,nonsuccession:1>3.0,nonsuccession:3>1.0 | fit T,T,T"},
      {"dcl K3 k68008.csv", "k68008.csv", dcl_f8, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K3"}}, "M nonchainsuccession:2021-03-03 10:00:00+00:00>2021-03-01 08:00:00+00:00,nonsuccession:2021-03-03 10:00:00+00:00>2021-03-01 08:00:00+00:00 | fit T,T,T,T"},
      {"dcl K2 k68010.csv", "k68010.csv", dcl_f9, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K2"}}, "M exactly_one:1.5,existence:1.5,nonchainsuccession:1.5>2.5,nonchainsuccession:1.5>3.0,nonchainsuccession:2.5>1.5,nonchainsuccession:3.0>1.5,nonsuccession:1.5>2.5,nonsuccession:1.5>3.0 | fit T,T"},
      {"dcl K4 k68011.csv", "k68011.csv", dcl_f10, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K4"}}, "M exactly_one:2,existence:2,nonchainsuccession:2>1.0,nonsuccession:2>1.0 | fit T,T"},
      {"dcl K4 k68014.csv", "k68014.csv", dcl_f11, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K4"}}, "M absence:2,nonchainsuccession:1>3.0,nonchainsuccession:3>1.0,noncoexistence:1>3.0,noncoexistence:3>1.0,nonsuccession:1>3.0,nonsuccession:3>1.0 | fit T,T,F,T,T"},
      {"dcl K4 k68015.csv", "k68015.csv", dcl_f12, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K4"}}, "M nonchainsuccession:3>7.0,nonchainsuccession:7>3.0,noncoexistence:3>7.0,noncoexistence:7>3.0,nonsuccession:3>7.0,nonsuccession:7>3.0 | fit T,T,T,T"},
      {"dcl K4 k68016.csv", "k68016.csv", dcl_f13, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K4"}}, "M absence:2,absence:3,exactly_one:1,existence:1,init:1,nonchainsuccession:1>2.0,nonchainsuccession:2>1.0,nonchainsuccession:3>1.0,noncoexistence:1>2.0,noncoexistence:2>1.0,nonsuccession:1>2.0,nonsuccession:2>1.0,nonsuccession:3>1.0 | fit T,T,T,F,F"},
      {"dcl K4 k68017.csv", "k68017.csv", dcl_f14, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K4"}}, "M nonchainsuccession:1.0>7.0,nonchainsuccession:7.0>1.0,noncoexistence:1.0>7.0,noncoexistence:7.0>1.0,nonsuccession:1.0>7.0,nonsuccession:7.0>1.0 | fit T,T,T,T"},
      {"dcl K3 k68023.csv", "k68023.csv", dcl_f15, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K3"}}, "M nonchainsuccession:2021-03-01 08:00:00+00:00>2021-03-02 09:30:00+00:00,nonchainsuccession:2021-03-02 09:30:00+00:00>2021-03-01 08:00:00+00:00,nonsuccession:2021-03-02 09:30:00+00:00>2021-03-01 08:00:00+00:00 | fit T,T,T"},
      {"dcl K3 k68026.csv", "k68026.csv", dcl_f16, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K3"}}, "M absence:2021-03-01 08:00:00+00:00,absence:2021-03-02 09:30:00+00:00,absence:2021-03-03 10:00:00+00:00,absence:NaT | fit F,F,F,F"},
      {"dcl K4 k68032.csv", "k68032.csv", dcl_f17, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K4"}}, "M exactly_one:3,existence:3,init:3,nonchainsuccession:1>3.0,nonchainsuccession:7>3.0,nonsuccession:1>3.0,nonsuccession:7>3.0 | fit T,T,T,T"},
      {"dcl K2 k68036.csv", "k68036.csv", dcl_f18, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K2"}}, "M nonchainsuccession:2.5>3.0,nonchainsuccession:3.0>2.5,noncoexistence:2.5>3.0,noncoexistence:3.0>2.5,nonsuccession:2.5>3.0,nonsuccession:3.0>2.5 | fit T,T,T"},
      {"dcl K4 k68039.csv", "k68039.csv", dcl_f19, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K4"}}, "M exactly_one:3,existence:3,nonchainsuccession:1>3.0,nonchainsuccession:3>7.0,nonsuccession:1>3.0,nonsuccession:3>7.0 | fit T,T"},
      {"dcl K3 k68044.csv", "k68044.csv", dcl_f20, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K3"}}, "M absence:2021-03-01 08:00:00+00:00,absence:2021-03-03 10:00:00+00:00,absence:NaT | fit F,F,T,T,F,F"},
      {"dcl K4 k68046.csv", "k68046.csv", dcl_f21, "dcl", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"declare_activity_attribute", "K4"}}, "M absence:2.0,absence:3.0,absence:7.0,absence:nan | fit F,F,F,F,F,F"},
      {"dfg K2 k68000.csv", "k68000.csv", dfg_f0, "dfg", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "native"}, {"dfg_activity_attribute", "K2"}}, "D nan>1.5=1,nan>3.0=1,nan>nan=1"},
      {"dfg K4 k68000.csv", "k68000.csv", dfg_f0, "dfg", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "native"}, {"dfg_activity_attribute", "K4"}}, "D 2.0>nan=1,2.0>nan=1,nan>2.0=1"},
      {"dfg K1 k68000.csv", "k68000.csv", dfg_f0, "dfg", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "native"}, {"dfg_activity_attribute", "K1"}}, "D p>r=1,q>r=1,r>nan=1"},
      {"dfg K2 k68001.csv", "k68001.csv", dfg_f1, "dfg", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "native"}, {"dfg_activity_attribute", "K2"}}, "D 2.5>1.5=1,2.5>nan=1,nan>3.0=1,nan>nan=1"},
      {"dfg K4 k68001.csv", "k68001.csv", dfg_f1, "dfg", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "native"}, {"dfg_activity_attribute", "K4"}}, "D 3.0>nan=1,7.0>7.0=1,nan>1.0=1,nan>nan=1"},
      {"dfg K1 k68001.csv", "k68001.csv", dfg_f1, "dfg", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "native"}, {"dfg_activity_attribute", "K1"}}, "D nan>s=1,r>r=1,s>nan=1,s>r=1"},
      {"dfg K2 k68002.csv", "k68002.csv", dfg_f2, "dfg", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "native"}, {"dfg_activity_attribute", "K2"}}, "D 1.5>1.5=1,1.5>2.5=1,1.5>3.0=1,2.5>1.5=1,2.5>2.5=2,nan>1.5=1"},
      {"dfg K4 k68002.csv", "k68002.csv", dfg_f2, "dfg", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "native"}, {"dfg_activity_attribute", "K4"}}, "D 1.0>1.0=1,1.0>2.0=1,2.0>3.0=1,2.0>nan=1,2.0>nan=1,3.0>1.0=1,3.0>3.0=1"},
      {"dfg K2 k68008.csv", "k68008.csv", dfg_f3, "dfg", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "native"}, {"dfg_activity_attribute", "K2"}}, "D -1.0>nan=1,1.5>2.5=1,2.5>-1.0=1,3.0>2.5=1,nan>1.5=1,nan>3.0=1,nan>nan=1,nan>nan=1,nan>nan=1"},
      {"dfg K4 k68009.csv", "k68009.csv", dfg_f4, "dfg", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "native"}, {"dfg_activity_attribute", "K4"}}, "D 2.0>nan=1,3.0>2.0=1,nan>2.0=1,nan>2.0=1,nan>nan=1"},
      {"dfg K4 k68028.csv", "k68028.csv", dfg_f5, "dfg", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "native"}, {"dfg_activity_attribute", "K4"}}, "D 1.0>3.0=1,3.0>nan=1,nan>1.0=1,nan>1.0=1"},
      {"dfg K4 k68038.csv", "k68038.csv", dfg_f6, "dfg", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "native"}, {"dfg_activity_attribute", "K4"}}, "D 3.0>nan=1,3.0>nan=1"},
      {"dfg K2 k68042.csv", "k68042.csv", dfg_f7, "dfg", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "native"}, {"dfg_activity_attribute", "K2"}}, "D 1.5>2.5=1,2.5>1.5=3,2.5>2.5=1,nan>2.5=1,nan>2.5=1"},
      {"dfg K2 k68048.csv", "k68048.csv", dfg_f8, "dfg", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "native"}, {"dfg_activity_attribute", "K2"}}, "D 1.5>nan=1,nan>1.5=1,nan>1.5=1"},
      {"dfg K2 k68053.csv", "k68053.csv", dfg_f9, "dfg", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "native"}, {"dfg_activity_attribute", "K2"}}, "D -1.0>nan=1,-1.0>nan=1,1.5>2.5=1,2.5>-1.0=1,2.5>nan=1,nan>1.5=1,nan>2.5=1,nan>nan=1,nan>nan=1"},
      {"dfg K4 k68080.csv", "k68080.csv", dfg_f10, "dfg", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "native"}, {"dfg_activity_attribute", "K4"}}, "D 2.0>nan=1,2.0>nan=1,nan>1.0=1,nan>nan=1"},
      {"dfg K2 k68082.csv", "k68082.csv", dfg_f11, "dfg", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "native"}, {"dfg_activity_attribute", "K2"}}, "D -1.0>nan=1,2.5>2.5=1,3.0>3.0=1,3.0>nan=1,nan>3.0=1,nan>3.0=1"},
      {"dfg K2 k68084.csv", "k68084.csv", dfg_f12, "dfg", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "native"}, {"dfg_activity_attribute", "K2"}}, "D 1.5>1.5=1,1.5>3.0=1,1.5>nan=1,3.0>2.5=1,3.0>nan=1,3.0>nan=1,nan>1.5=1"},
      {"dfg K2 k68097.csv", "k68097.csv", dfg_f13, "dfg", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "native"}, {"dfg_activity_attribute", "K2"}}, "D 1.5>1.5=2,1.5>2.5=1,2.5>nan=1,2.5>nan=1"},
      {"dfg K4 k68097.csv", "k68097.csv", dfg_f13, "dfg", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "native"}, {"dfg_activity_attribute", "K4"}}, "D 1.0>1.0=1,1.0>2.0=1,1.0>nan=1,nan>1.0=1,nan>1.0=1"},
      {"ts seq1 s69000.xes", "s69000.xes", ts_f0, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "1"}}, "S (),(0.0),(y) | (0.0)-0->(),(0.0)-0->(0.0),(0.0)-0.0->(y),(y)-y->()"},
      {"ts seq2 s69000.xes", "s69000.xes", ts_f0, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "2"}}, "S (),(0),(0,0),(0,0.0),(0.0,y),(y) | (0)-0->(),(0,0)-0->(0),(0,0)-0->(0,0.0),(0.0,y)-0.0->(y),(y)-y->()"},
      {"ts set2 s69000.xes", "s69000.xes", ts_f0, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "set"}, {"transition_window", "2"}}, "S (),(0),(0.0,y),(y) | (0)-0->(),(0)-0->(0),(0.0,y)-0.0->(y),(y)-y->()"},
      {"ts ms2 s69000.xes", "s69000.xes", ts_f0, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "multiset"}, {"transition_window", "2"}}, "S (),(0),(0,0),(0.0,y),(y) | (0)-0->(),(0,0)-0->(0),(0,0)-0->(0,0),(0.0,y)-0.0->(y),(y)-y->()"},
      {"ts seq2b s69000.xes", "s69000.xes", ts_f0, "ts", {{"transition_activity_attribute", "k"}, {"transition_direction", "backward"}, {"transition_window", "2"}}, "S (),(0,0),(0.0),(0.0,y) | ()-0.0->(0.0),(0,0)-0.0->(0,0),(0.0)-0->(0,0),(0.0)-y->(0.0,y)"},
      {"ts seq1 s69001.xes", "s69001.xes", ts_f1, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "1"}}, "S (),(1),(False) | (1)-1->(),(False)-0->(1),(False)-0->(False),(False)-False->()"},
      {"ts seq2 s69001.xes", "s69001.xes", ts_f1, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "2"}}, "S (),(0,0),(0,1),(1),(False) | (0,0)-0->(0,1),(0,1)-0->(1),(1)-1->(),(False)-False->()"},
      {"ts set2 s69001.xes", "s69001.xes", ts_f1, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "set"}, {"transition_window", "2"}}, "S (),(0,1),(1),(False) | (0,1)-0->(1),(1)-1->(),(False)-0->(0,1),(False)-False->()"},
      {"ts ms2 s69001.xes", "s69001.xes", ts_f1, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "multiset"}, {"transition_window", "2"}}, "S (),(0,0),(0,1),(1),(False) | (0,0)-0->(0,1),(0,1)-0->(1),(1)-1->(),(False)-False->()"},
      {"ts seq2b s69001.xes", "s69001.xes", ts_f1, "ts", {{"transition_activity_attribute", "k"}, {"transition_direction", "backward"}, {"transition_window", "2"}}, "S (),(0,0),(0,1),(False) | ()-False->(False),(0,0)-1->(0,1),(False)-0->(0,0)"},
      {"ts seq1 s69002.xes", "s69002.xes", ts_f2, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "1"}}, "S (),(0.0),(1.0) | (0.0)-0.0->(0.0),(0.0)-0.0->(1.0),(1.0)-1.0->(),(1.0)-1.0->(0.0)"},
      {"ts seq2 s69002.xes", "s69002.xes", ts_f2, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "2"}}, "S (),(0.0,0.0),(0.0,1.0),(1.0),(1.0,0.0) | (0.0,0.0)-0.0->(0.0,1.0),(0.0,1.0)-0.0->(1.0),(1.0)-1.0->(),(1.0,0.0)-1.0->(0.0,0.0)"},
      {"ts set2 s69002.xes", "s69002.xes", ts_f2, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "set"}, {"transition_window", "2"}}, "S (),(0.0),(0.0,1.0),(1.0) | (0.0)-0.0->(0.0,1.0),(0.0,1.0)-0.0->(1.0),(0.0,1.0)-1.0->(0.0),(1.0)-1.0->()"},
      {"ts ms2 s69002.xes", "s69002.xes", ts_f2, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "multiset"}, {"transition_window", "2"}}, "S (),(0.0,0.0),(0.0,1.0),(1.0) | (0.0,0.0)-0.0->(0.0,1.0),(0.0,1.0)-0.0->(1.0),(0.0,1.0)-1.0->(0.0,0.0),(1.0)-1.0->()"},
      {"ts seq2b s69002.xes", "s69002.xes", ts_f2, "ts", {{"transition_activity_attribute", "k"}, {"transition_direction", "backward"}, {"transition_window", "2"}}, "S (),(0.0,0.0),(0.0,1.0),(1.0),(1.0,0.0) | ()-1.0->(1.0),(0.0,0.0)-1.0->(0.0,1.0),(1.0)-0.0->(1.0,0.0),(1.0,0.0)-0.0->(0.0,0.0)"},
      {"ts seq1 s69003.xes", "s69003.xes", ts_f3, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "1"}}, "S (),(1),(1.5),(2),(x) | (1)-1->(),(1)-1->(1.5),(1)-1->(x),(1.5)-1.5->(),(1.5)-1.5->(1),(2)-2->(),(2)-2->(1),(2)-2->(2),(x)-x->(2),(x)-x->(x)"},
      {"ts seq2 s69005.xes", "s69005.xes", ts_f4, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "2"}}, "S (),(0,False),(1,2),(2,y),(False,1),(y) | (0,False)-0->(False,1),(1,2)-1->(2,y),(2,y)-2->(y),(False,1)-False->(1,2),(y)-y->()"},
      {"ts set2 s69005.xes", "s69005.xes", ts_f4, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "set"}, {"transition_window", "2"}}, "S (),(0),(1,2),(1,False),(2,y),(y) | (0)-0->(1,False),(1,2)-1->(2,y),(1,False)-False->(1,2),(2,y)-2->(y),(y)-y->()"},
      {"ts ms2 s69005.xes", "s69005.xes", ts_f4, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "multiset"}, {"transition_window", "2"}}, "S (),(0,0),(1,2),(1,False),(2,y),(y) | (0,0)-0->(1,False),(1,2)-1->(2,y),(1,False)-False->(1,2),(2,y)-2->(y),(y)-y->()"},
      {"ts seq2b s69005.xes", "s69005.xes", ts_f4, "ts", {{"transition_activity_attribute", "k"}, {"transition_direction", "backward"}, {"transition_window", "2"}}, "S (),(0),(0,False),(1,2),(2,y),(False,1) | ()-0->(0),(0)-False->(0,False),(0,False)-1->(False,1),(1,2)-y->(2,y),(False,1)-2->(1,2)"},
      {"ts seq1 s69007.xes", "s69007.xes", ts_f5, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "1"}}, "S (),(False),(True),(y) | (False)-0.0->(),(False)-0.0->(True),(False)-0.0->(y),(False)-False->(False),(True)-True->(),(True)-True->(False),(True)-True->(True),(True)-True->(y),(y)-y->(),(y)-y->(False),(y)-y->(y)"},
      {"ts seq2 s69007.xes", "s69007.xes", ts_f5, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "2"}}, "S (),(0.0),(0.0,True),(0.0,y),(False,0.0),(True),(True,False),(True,True),(True,y),(y),(y,0.0),(y,y) | (0.0)-0.0->(),(0.0,True)-0.0->(True,True),(0.0,y)-0.0->(y),(0.0,y)-False->(y,0.0),(False,0.0)-0.0->(0.0),(False,0.0)-False->(0.0,y),(True)-True->(),(True,False)-True->(False,0.0),(True,True)-True->(True),(True,y)-True->(y,y),(y)-y->(),(y,0.0)-y->(0.0),(y,0.0)-y->(0.0,True),(y,0.0)-y->(False,0.0),(y,y)-y->(y,0.0)"},
      {"ts set2 s69007.xes", "s69007.xes", ts_f5, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "set"}, {"transition_window", "2"}}, "S (),(0.0,y),(False),(False,True),(True),(True,y),(y) | (0.0,y)-0.0->(y),(0.0,y)-False->(0.0,y),(0.0,y)-y->(False),(0.0,y)-y->(False,True),(False)-0.0->(),(False)-0.0->(False),(False)-False->(0.0,y),(False,True)-0.0->(True),(False,True)-True->(False),(True)-True->(),(True)-True->(True),(True,y)-True->(y),(y)-y->(),(y)-y->(0.0,y)"},
      {"ts ms2 s69007.xes", "s69007.xes", ts_f5, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "multiset"}, {"transition_window", "2"}}, "S (),(0.0),(0.0,y),(False,False),(False,True),(True),(True,True),(True,y),(y),(y,y) | (0.0)-0.0->(),(0.0,y)-0.0->(y),(0.0,y)-False->(0.0,y),(0.0,y)-y->(0.0),(0.0,y)-y->(False,False),(0.0,y)-y->(False,True),(False,False)-0.0->(0.0),(False,False)-False->(0.0,y),(False,True)-0.0->(True,True),(False,True)-True->(False,False),(True)-True->(),(True,True)-True->(True),(True,y)-True->(y,y),(y)-y->(),(y,y)-y->(0.0,y)"},
      {"ts seq2b s69007.xes", "s69007.xes", ts_f5, "ts", {{"transition_activity_attribute", "k"}, {"transition_direction", "backward"}, {"transition_window", "2"}}, "S (),(0.0,True),(0.0,y),(False),(False,0.0),(True),(True,False),(True,True),(True,y),(y),(y,0.0),(y,y) | ()-False->(False),()-True->(True),()-y->(y),(0.0,True)-True->(True,True),(0.0,y)-0.0->(y,0.0),(False)-y->(0.0,y),(False,0.0)-y->(0.0,y),(True)-False->(True,False),(True)-y->(True,y),(True,False)-0.0->(False,0.0),(True,y)-y->(y,y),(y)-0.0->(y,0.0),(y,0.0)-0.0->(False,0.0),(y,0.0)-True->(0.0,True),(y,y)-0.0->(y,0.0)"},
      {"ts seq1 s69009.xes", "s69009.xes", ts_f6, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "1"}}, "S (),(0.0),(1.5),(True) | (0.0)-0->(),(0.0)-0->(1.5),(0.0)-0.0->(0.0),(1.5)-1.5->(),(1.5)-1.5->(1.5),(1.5)-1.5->(True),(True)-True->(0.0)"},
      {"ts seq2 s69009.xes", "s69009.xes", ts_f6, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "2"}}, "S (),(0),(0,1.5),(0.0,0.0),(1.5),(1.5,1.5),(1.5,True),(True,0.0) | (0)-0->(),(0,1.5)-0->(1.5,1.5),(0.0,0.0)-0.0->(0),(0.0,0.0)-0.0->(0.0,0.0),(1.5)-1.5->(),(1.5,1.5)-1.5->(1.5),(1.5,True)-1.5->(True,0.0),(True,0.0)-True->(0)"},
      {"ts set2 s69009.xes", "s69009.xes", ts_f6, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "set"}, {"transition_window", "2"}}, "S (),(0,1.5),(0.0),(0.0,True),(1.5),(1.5,True) | (0,1.5)-0->(1.5),(0.0)-0->(),(0.0)-0.0->(0.0),(0.0,True)-True->(0.0),(1.5)-1.5->(),(1.5)-1.5->(1.5),(1.5,True)-1.5->(0.0,True)"},
      {"ts ms2 s69009.xes", "s69009.xes", ts_f6, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "multiset"}, {"transition_window", "2"}}, "S (),(0),(0,1.5),(0.0,0.0),(0.0,True),(1.5),(1.5,1.5),(1.5,True) | (0)-0->(),(0,1.5)-0->(1.5,1.5),(0.0,0.0)-0.0->(0),(0.0,0.0)-0.0->(0.0,0.0),(0.0,True)-True->(0),(1.5)-1.5->(),(1.5,1.5)-1.5->(1.5),(1.5,True)-1.5->(0.0,True)"},
      {"ts seq2b s69009.xes", "s69009.xes", ts_f6, "ts", {{"transition_activity_attribute", "k"}, {"transition_direction", "backward"}, {"transition_window", "2"}}, "S (),(0,1.5),(0.0),(0.0,0.0),(1.5),(1.5,1.5),(1.5,True),(True,0.0) | ()-0.0->(0.0),()-1.5->(1.5),(0,1.5)-1.5->(1.5,1.5),(0.0)-0.0->(0.0,0.0),(0.0)-1.5->(0,1.5),(0.0,0.0)-0.0->(0.0,0.0),(1.5)-True->(1.5,True),(1.5,True)-0.0->(True,0.0)"},
      {"ts seq1 s69018.xes", "s69018.xes", ts_f7, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "1"}}, "S (),(0),(True) | (0)-0->(True),(0)-0.0->(),(0)-0.0->(0),(True)-1.0->(0),(True)-True->(),(True)-True->(True)"},
      {"ts set2 s69018.xes", "s69018.xes", ts_f7, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "set"}, {"transition_window", "2"}}, "S (),(0,1.0),(0.0),(0.0,1.0),(True) | (0,1.0)-0->(0.0,1.0),(0,1.0)-1.0->(0.0),(0.0)-0.0->(),(0.0)-0.0->(0.0),(True)-True->(),(True)-True->(True)"},
      {"ts seq2b s69018.xes", "s69018.xes", ts_f7, "ts", {{"transition_activity_attribute", "k"}, {"transition_direction", "backward"}, {"transition_window", "2"}}, "S (),(0),(0,1.0),(0.0,False),(1.0,0.0),(True),(True,True) | ()-0->(0),()-True->(True),(0)-1.0->(0,1.0),(0,1.0)-0.0->(1.0,0.0),(0.0,False)-0.0->(0.0,False),(1.0,0.0)-False->(0.0,False),(True)-True->(True,True),(True,True)-1.0->(True,True)"},
      {"ts seq1 s69021.xes", "s69021.xes", ts_f8, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "1"}}, "S (),(0.0),(1.5) | (0.0)-0.0->(0.0),(0.0)-False->(),(1.5)-1.5->()"},
      {"ts set2 s69021.xes", "s69021.xes", ts_f8, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "set"}, {"transition_window", "2"}}, "S (),(0.0),(1.5) | (0.0)-0.0->(0.0),(0.0)-False->(),(1.5)-1.5->()"},
      {"ts ms2 s69021.xes", "s69021.xes", ts_f8, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "multiset"}, {"transition_window", "2"}}, "S (),(0.0,0.0),(1.5),(False) | (0.0,0.0)-0.0->(0.0,0.0),(0.0,0.0)-0.0->(False),(1.5)-1.5->(),(False)-False->()"},
      {"ts seq2b s69021.xes", "s69021.xes", ts_f8, "ts", {{"transition_activity_attribute", "k"}, {"transition_direction", "backward"}, {"transition_window", "2"}}, "S (),(0.0),(0.0,0.0),(1.5) | ()-0.0->(0.0),()-1.5->(1.5),(0.0)-0.0->(0.0,0.0),(0.0,0.0)-False->(0.0,0.0)"},
      {"ts seq1 s69023.xes", "s69023.xes", ts_f9, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "1"}}, "S (),(0.0),(1) | (0.0)-0->(1),(0.0)-0.0->(),(0.0)-0.0->(0.0),(1)-1->()"},
      {"ts seq2 s69023.xes", "s69023.xes", ts_f9, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "2"}}, "S (),(0,1),(0.0),(0.0,0.0),(1) | (0,1)-0->(1),(0.0)-0.0->(),(0.0,0.0)-0->(0.0),(0.0,0.0)-0.0->(0,1),(0.0,0.0)-0.0->(0.0,0.0),(1)-1->()"},
      {"ts set2 s69023.xes", "s69023.xes", ts_f9, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "set"}, {"transition_window", "2"}}, "S (),(0,1),(0.0),(1) | (0,1)-0->(1),(0.0)-0.0->(),(0.0)-0.0->(0,1),(0.0)-0.0->(0.0),(1)-1->()"},
      {"ts ms2 s69023.xes", "s69023.xes", ts_f9, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "multiset"}, {"transition_window", "2"}}, "S (),(0,1),(0.0),(0.0,0.0),(1) | (0,1)-0->(1),(0.0)-0.0->(),(0.0,0.0)-0->(0.0),(0.0,0.0)-0.0->(0,1),(0.0,0.0)-0.0->(0.0,0.0),(1)-1->()"},
      {"ts seq2b s69023.xes", "s69023.xes", ts_f9, "ts", {{"transition_activity_attribute", "k"}, {"transition_direction", "backward"}, {"transition_window", "2"}}, "S (),(0,1),(0.0),(0.0,0.0) | ()-0.0->(0.0),(0.0)-0.0->(0.0,0.0),(0.0,0.0)-0.0->(0.0,0.0),(0.0,0.0)-1->(0,1)"},
      {"ts seq1 s69025.xes", "s69025.xes", ts_f10, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "1"}}, "S (),(0),(1.5),(2),(True),(x) | (0)-0->(2),(0)-0->(True),(0)-False->(0),(1.5)-1.5->(),(2)-2->(),(2)-2->(2),(True)-True->(),(True)-True->(0),(True)-True->(1.5),(x)-x->(0)"},
      {"ts seq2 s69025.xes", "s69025.xes", ts_f10, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "2"}}, "S (),(0,2),(0,True),(1.5),(2),(2,2),(False,0),(True),(True,0),(True,1.5),(x,0) | (0,2)-0->(2,2),(0,True)-0->(True),(0,True)-0->(True,1.5),(1.5)-1.5->(),(2)-2->(),(2,2)-2->(2),(False,0)-False->(0,True),(True)-True->(),(True,0)-True->(0,True),(True,0)-True->(False,0),(True,1.5)-True->(1.5),(x,0)-x->(0,2)"},
      {"ts set2 s69025.xes", "s69025.xes", ts_f10, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "set"}, {"transition_window", "2"}}, "S (),(0,2),(0,True),(0,x),(1.5),(1.5,True),(2),(False),(True) | (0,2)-0->(2),(0,True)-0->(1.5,True),(0,True)-0->(True),(0,True)-True->(0,True),(0,True)-True->(False),(0,x)-x->(0,2),(1.5)-1.5->(),(1.5,True)-True->(1.5),(2)-2->(),(2)-2->(2),(False)-False->(0,True),(True)-True->()"},
      {"ts ms2 s69025.xes", "s69025.xes", ts_f10, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "multiset"}, {"transition_window", "2"}}, "S (),(0,2),(0,True),(0,True),(0,x),(1.5),(1.5,True),(2),(2,2),(False,False),(True) | (0,2)-0->(2,2),(0,True)-0->(1.5,True),(0,True)-0->(True),(0,True)-True->(0,True),(0,True)-True->(False,False),(0,x)-x->(0,2),(1.5)-1.5->(),(1.5,True)-True->(1.5),(2)-2->(),(2,2)-2->(2),(False,False)-False->(0,True),(True)-True->()"},
      {"ts seq2b s69025.xes", "s69025.xes", ts_f10, "ts", {{"transition_activity_attribute", "k"}, {"transition_direction", "backward"}, {"transition_window", "2"}}, "S (),(0,2),(0,True),(2,2),(False,0),(True),(True,0),(True,1.5),(x),(x,0) | ()-True->(True),()-x->(x),(0,2)-2->(2,2),(0,True)-1.5->(True,1.5),(False,0)-True->(0,True),(True)-0->(True,0),(True,0)-0->(False,0),(True,0)-True->(0,True),(x)-0->(x,0),(x,0)-2->(0,2)"},
      {"ts seq1 s69026.xes", "s69026.xes", ts_f11, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "1"}}, "S (),(0.0),(1.0),(y) | (0.0)-0.0->(0.0),(0.0)-0.0->(y),(0.0)-False->(),(1.0)-1.0->(0.0),(1.0)-1.0->(y),(y)-y->(),(y)-y->(0.0),(y)-y->(1.0),(y)-y->(y)"},
      {"ts seq2 s69026.xes", "s69026.xes", ts_f11, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "2"}}, "S (),(0.0,False),(0.0,y),(1.0,0.0),(1.0,y),(False),(y),(y,0.0),(y,1.0),(y,y) | (0.0,False)-0.0->(0.0,False),(0.0,False)-0.0->(False),(0.0,y)-0.0->(y,y),(1.0,0.0)-1.0->(0.0,False),(1.0,y)-1.0->(y),(False)-False->(),(y)-y->(),(y,0.0)-y->(0.0,False),(y,1.0)-y->(1.0,y),(y,y)-y->(y),(y,y)-y->(y,0.0)"},
      {"ts set2 s69026.xes", "s69026.xes", ts_f11, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "set"}, {"transition_window", "2"}}, "S (),(0.0),(0.0,1.0),(0.0,y),(1.0,y),(y) | (0.0)-0.0->(0.0),(0.0)-False->(),(0.0,1.0)-1.0->(0.0),(0.0,y)-0.0->(y),(0.0,y)-y->(0.0),(1.0,y)-1.0->(y),(1.0,y)-y->(1.0,y),(y)-y->(),(y)-y->(0.0,y),(y)-y->(y)"},
      {"ts ms2 s69026.xes", "s69026.xes", ts_f11, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "multiset"}, {"transition_window", "2"}}, "S (),(0.0,0.0),(0.0,1.0),(0.0,y),(1.0,y),(1.0,y),(False),(y),(y,y) | (0.0,0.0)-0.0->(0.0,0.0),(0.0,0.0)-0.0->(False),(0.0,1.0)-1.0->(0.0,0.0),(0.0,y)-0.0->(y,y),(0.0,y)-y->(0.0,0.0),(1.0,y)-1.0->(y),(1.0,y)-y->(1.0,y),(False)-False->(),(y)-y->(),(y,y)-y->(0.0,y),(y,y)-y->(y)"},
      {"ts seq2b s69026.xes", "s69026.xes", ts_f11, "ts", {{"transition_activity_attribute", "k"}, {"transition_direction", "backward"}, {"transition_window", "2"}}, "S (),(0.0),(0.0,False),(0.0,y),(1.0),(1.0,0.0),(1.0,y),(y),(y,0.0),(y,1.0),(y,y) | ()-0.0->(0.0),()-1.0->(1.0),()-y->(y),(0.0)-y->(0.0,y),(0.0,False)-0.0->(0.0,False),(0.0,y)-y->(y,y),(1.0)-0.0->(1.0,0.0),(1.0,0.0)-0.0->(0.0,False),(y)-1.0->(y,1.0),(y,0.0)-False->(0.0,False),(y,1.0)-y->(1.0,y),(y,y)-0.0->(y,0.0)"},
      {"ts seq1 s69027.xes", "s69027.xes", ts_f12, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "1"}}, "S (),(0),(1),(1),(2) | (0)-0->(0),(0)-0->(1),(0)-False->(1),(0)-False->(2),(1)-1->(),(1)-1->(0),(1)-1->(0),(1)-1->(1),(1)-1->(2),(1)-1->(2),(2)-2->(),(2)-2->(1)"},
      {"ts seq2 s69027.xes", "s69027.xes", ts_f12, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "2"}}, "S (),(0,0),(0,1),(1),(1,1),(1,2),(1,2),(1,False),(1,False),(2),(2,1),(False,1),(False,2) | (0,0)-0->(0,1),(0,1)-0->(1,False),(1)-1->(),(1,1)-1->(1),(1,2)-1->(2),(1,2)-1->(2),(1,False)-1->(False,1),(1,False)-1->(False,2),(2)-2->(),(2,1)-2->(1),(False,1)-False->(1,2),(False,2)-False->(2)"},
      {"ts set2 s69027.xes", "s69027.xes", ts_f12, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "set"}, {"transition_window", "2"}}, "S (),(0),(0,1),(1),(1,1),(1,2),(1,2),(1,False),(2),(2,False) | (0)-0->(0,1),(0,1)-0->(0,1),(0,1)-1->(2,False),(1)-1->(),(1,1)-1->(1),(1,2)-1->(2),(1,2)-1->(2),(1,2)-2->(1),(1,False)-1->(1,False),(1,False)-False->(1,2),(2)-2->(),(2,False)-False->(2)"},
      {"ts ms2 s69027.xes", "s69027.xes", ts_f12, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "multiset"}, {"transition_window", "2"}}, "S (),(0,0),(0,1),(1),(1,1),(1,2),(1,2),(1,False),(1,False),(2),(2,False) | (0,0)-0->(0,1),(0,1)-0->(0,1),(0,1)-1->(2,False),(1)-1->(),(1,1)-1->(1),(1,2)-1->(2),(1,2)-1->(2),(1,2)-2->(1),(1,False)-1->(1,False),(1,False)-False->(1,2),(2)-2->(),(2,False)-False->(2)"},
      {"ts seq2b s69027.xes", "s69027.xes", ts_f12, "ts", {{"transition_activity_attribute", "k"}, {"transition_direction", "backward"}, {"transition_window", "2"}}, "S (),(0),(0,0),(0,1),(1),(1),(1,1),(1,2),(1,2),(1,False),(1,False),(2),(2,1),(False,1),(False,2) | ()-0->(0),()-1->(1),()-1->(1),()-2->(2),(0)-0->(0,0),(0,0)-1->(0,1),(0,1)-False->(1,False),(1)-1->(1,1),(1)-2->(1,2),(1)-False->(1,False),(1,False)-1->(False,1),(1,False)-2->(False,2),(2)-1->(2,1),(False,1)-2->(1,2)"},
      {"ts seq1 s69031.xes", "s69031.xes", ts_f13, "ts", {{"transition_activity_attribute", "k"}, {"transition_window", "1"}}, "S (),(0.0),(1.0),(y) | (0.0)-0.0->(),(0.0)-0.0->(0.0),(0.0)-0.0->(1.0),(1.0)-1->(),(1.0)-1.0->(0.0),(1.0)-1.0->(1.0),(y)-y->(0.0)"},
      {"ts set2 s69031.xes", "s69031.xes", ts_f13, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "set"}, {"transition_window", "2"}}, "S (),(0.0),(0.0,1.0),(0.0,y),(1.0) | (0.0)-0.0->(),(0.0)-0.0->(0.0),(0.0)-0.0->(0.0,1.0),(0.0,1.0)-0.0->(1.0),(0.0,1.0)-1.0->(0.0),(0.0,y)-y->(0.0),(1.0)-1->(),(1.0)-1.0->(0.0,1.0),(1.0)-1.0->(1.0)"},
      {"ts ms2 s69031.xes", "s69031.xes", ts_f13, "ts", {{"transition_activity_attribute", "k"}, {"transition_view", "multiset"}, {"transition_window", "2"}}, "S (),(0.0),(0.0,0.0),(0.0,1.0),(0.0,y),(1),(1.0,1.0) | (0.0)-0.0->(),(0.0,0.0)-0.0->(0.0),(0.0,0.0)-0.0->(0.0,1.0),(0.0,1.0)-0.0->(1),(0.0,1.0)-1.0->(0.0,0.0),(0.0,y)-y->(0.0,0.0),(1)-1->(),(1.0,1.0)-1->(1),(1.0,1.0)-1.0->(0.0,1.0),(1.0,1.0)-1.0->(1.0,1.0)"},
  };

  // Round HB: a pm4py error whose operands follow the hash seed (the typed
  // rules' sort) compares the error's class.
  for (const auto& c : cases) {
    auto shown = outcome(client, c.name, c.content, c.kind, c.fields);
    if (c.expected.starts_with("error-class: ") && shown.starts_with("error: "))
      shown = "error-class: " + shown.substr(7, shown.find(':', 7) - 7);
    check(c.label, shown, c.expected);
  }

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures) {
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "review GT server checks passed\n";
  return 0;
}
