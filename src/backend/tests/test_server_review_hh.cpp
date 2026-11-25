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

// A small JSON reader: enough for the routes' results.
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

std::string norm(std::string message) {
  for (bool changed = true; changed;) {
    changed = false;
    for (const char* prefix : {"inductive miner: ", "Heuristics Miner: ", "TypeError: ", "KeyError: ", "ValueError: ",
                               "AttributeError: ", "IndexError: ", "ZeroDivisionError: "})
      if (message.starts_with(prefix)) {
        message = message.substr(std::string(prefix).size());
        changed = true;
      }
  }
  return message;
}

std::string f_digits(const Json& value, int digits) {
  if (value.kind != Json::Kind::kNumber || value.text == "NaN") return "nan";
  return fixed(value, digits);
}

std::string lbl(const Json& value) { return plain(value); }

// The session route names a NaN null in its transition system.
std::string ts_label(const Json& value) { return value.kind == Json::Kind::kNull ? std::string("nan") : plain(value); }

std::string totals_of(const Json& replay) {
  std::string totals;
  for (const char* key : {"fitting_traces", "produced", "consumed", "missing", "remaining"})
    totals += (totals.empty() ? "" : "/") + replay[key].text;
  return totals;
}

bool is_csv(const std::string& name) { return name.size() > 4 && name.substr(name.size() - 4) == ".csv"; }

struct Server {
  httplib::Client& client;
  const std::string& name;
  const std::string& content;

  Json parse(const httplib::Result& response) const {
    Json out;
    if (!response) {
      out.kind = Json::Kind::kObject;
      Json error;
      error.kind = Json::Kind::kString;
      error.text = "no response";
      out.members.emplace_back("error", error);
      return out;
    }
    JsonReader reader{response->body};
    return reader.value();
  }

  Json upload(const Fields& fields) const {
    httplib::MultipartFormDataItems items = {{"log", content, name, "application/octet-stream"},
                                             {"include_traces", "false", "", ""}};
    if (is_csv(name)) {
      items.push_back({"case_column", "case:concept:name", "", ""});
      items.push_back({"activity_column", "concept:name", "", ""});
      items.push_back({"timestamp_column", "time:timestamp", "", ""});
    }
    for (const auto& [key, value] : fields) items.push_back({key, value, "", ""});
    return parse(client.Post("/api/mine", items));
  }

  Json post(const std::string& route, const Fields& fields) const {
    httplib::Params params;
    for (const auto& [key, value] : fields) params.emplace(key, value);
    return parse(client.Post(route, params));
  }

  std::string source_id() const {
    const auto body = parse(client.Get("/api/log-query/schema"));
    return body["source_id"].text;
  }
};

std::string fail(const Json& error) { return "error: " + norm(error_text(error)); }

std::string none_or(const Json& value) { return value.kind == Json::Kind::kNull ? std::string("None") : value.text; }

// One footprint in mkhh.py's canonical form (nfp_text).
std::string footprint_text(const Json& f, const std::function<std::string(const Json&)>& name) {
  const auto pairs = [&](const char* key) {
    std::vector<std::string> out;
    for (const auto& e : f[key].items) out.push_back(name(e["source"]) + ">" + name(e["target"]));
    return sorted_join(out, ",");
  };
  const auto names = [&](const char* key) {
    std::vector<std::string> out;
    for (const auto& i : f[key].items) out.push_back(name(i));
    return sorted_join(out, ",");
  };
  std::vector<std::string> dfg;
  for (const auto& e : f["dfg"].items) dfg.push_back(name(e["source"]) + ">" + name(e["target"]) + "=" + e["count"].text);
  return "seq[" + pairs("sequence") + "] par[" + pairs("parallel") + "] st[" + names("start_activities") + "] en[" +
         names("end_activities") + "] act[" + names("activities") + "] dfg[" + sorted_join(dfg, ",") + "] min " +
         none_or(f["min_trace_length"]);
}

// One case's outcome in mkhh.py's canonical form (its srv()).
std::string outcome(const Server& server, const std::string& kind, const std::string& family, const std::string& key,
                    const std::string& extra) {
  if (kind == "eva") {
    const std::string miner = family == "alpha" ? "alpha" : "heuristic";
    Fields fields;
    if (family == "alpha") fields.push_back({"alpha_activity_attribute", key});
    else {
      fields.push_back({"heuristic_activity_attribute", key});
      fields.push_back({"heuristic_variant", "plusplus"});
    }
    const auto mined = server.upload(fields);
    if (mined["error"].kind != Json::Kind::kNull) return "mine " + fail(mined["error"]);
    const std::string family_name = extra == "pr_aaa" ? "precision" : "replay_fitness";
    Fields request = {{"miner", miner}, {"evaluation_family", family_name}};
    if (extra == "pr_aaa") request.push_back({"precision_variant", "automaton_after_align"});
    if (extra == "rf_align") request.push_back({"fitness_variant", "alignment_based"});
    const auto evaluated = server.post("/api/evaluate", request);
    if (evaluated["error"].kind != Json::Kind::kNull) return fail(evaluated["error"]);
    const auto& x = evaluated[family_name];
    if (x["error"].kind != Json::Kind::kNull) return fail(x["error"]);
    if (extra == "pr_aaa") return "P " + f_digits(x["precision"], 6);
    return "F " + f_digits(x["log_fitness"], 6) + "/" + f_digits(x["average_trace_fitness"], 6) + "/" +
           f_digits(x["percentage_of_fitting_traces"], 6);
  }
  if (kind == "aln") {
    const std::string miner = family == "alpha" ? "alpha" : "heuristic";
    Fields fields;
    if (family == "alpha") fields.push_back({"alpha_activity_attribute", key});
    else {
      fields.push_back({"heuristic_activity_attribute", key});
      fields.push_back({"heuristic_variant", "plusplus"});
    }
    const auto mined = server.upload(fields);
    if (mined["error"].kind != Json::Kind::kNull) return "mine " + fail(mined["error"]);
    const auto a = server.post("/api/align", {{"miner", miner}, {"alignment_activity_attribute", key},
                                              {"alignment_variant", extra}, {"cost_profile", "standard"}});
    const bool computed = a["computed"].kind == Json::Kind::kBool && a["computed"].text == "true";
    if (a["error"].kind != Json::Kind::kNull && !computed) return fail(a["error"]);
    return "A " + f_digits(a["fitness"], 6) + " " + none_or(a["total_cost"]);
  }
  if (kind == "fp") {
    const auto mined = server.upload({{"alpha_activity_attribute", key}});
    if (mined["error"].kind != Json::Kind::kNull) return "mine " + fail(mined["error"]);
  } else {
    const auto mined = server.upload({});
    if (mined["error"].kind != Json::Kind::kNull) return "upload " + fail(mined["error"]);
  }
  const auto source = server.source_id();
  if (kind == "msd") {
    const auto d = server.post("/api/analyze-behavior", {{"family", "minimum_self_distance"}, {"source_id", source},
                                                         {"variant", "log"}, {"minimum_self_distance_activity_key", key}});
    if (d["error"].kind != Json::Kind::kNull) return fail(d["error"]);
    std::vector<std::string> entries;
    for (const auto& x : d["native_result"].items) {
      std::vector<std::string> witnesses;
      for (const auto& w : x["witnesses"].items) witnesses.push_back(lbl(w));
      const bool null = x["activity_is_null"].kind == Json::Kind::kBool && x["activity_is_null"].text == "true";
      entries.push_back((null ? std::string("None") : lbl(x["activity"])) + "=" + x["distance"].text + "[" +
                        sorted_join(witnesses, ",") + "]");
    }
    return "MSD " + sorted_join(entries, " ");
  }
  if (kind == "fp") {
    const auto d = server.post("/api/discover-graph",
                               {{"family", "footprints"}, {"source_id", source}, {"footprints_activity_attribute", key},
                                {"footprints_model", "alpha"},
                                {"footprints_discovery_variants", "entire_event_log,trace_by_trace"},
                                {"footprints_conformance_variants", "log_model"}});
    if (d["error"].kind != Json::Kind::kNull) return fail(d["error"]);
    const auto& a = d["result"];
    std::vector<std::string> names;
    for (const auto& x : a["activities"].items) names.push_back(lbl(x["name"]));
    const auto name = [&](const Json& i) {
      const auto index = std::strtoull(i.text.c_str(), nullptr, 10);
      return i.kind == Json::Kind::kNumber && index < names.size() ? names[index] : "#" + i.text;
    };
    const auto& f = a["footprints"];
    const auto& dd = f["discovery"];
    std::string traces;
    bool first = true;
    for (const auto& t : dd["trace_by_trace"].items) {
      traces += (first ? "" : " / ") + footprint_text(t["footprint"], name);
      first = false;
    }
    const auto& model = d["model_footprint"];
    const auto model_pairs = [&](const char* key_name) {
      std::vector<std::string> out;
      for (const auto& e : model[key_name].items) out.push_back(name(e["source"]) + ">" + name(e["target"]));
      return sorted_join(out, ",");
    };
    std::vector<std::string> violations;
    for (const auto& v : f["conformance"]["log_model"]["violations"].items)
      violations.push_back(name(v["source"]) + ">" + name(v["target"]));
    return "FP " + footprint_text(dd["entire_event_log"], name) + " | T " + traces + " | M seq[" + model_pairs("sequence") +
           "] par[" + model_pairs("parallel") + "] | V[" + sorted_join(violations, ",") + "]";
  }
  if (kind == "lsx") {
    const auto d = server.post("/api/discover-constraints", {{"family", "log_skeleton"}, {"source_id", source},
                                                             {"log_skeleton_activity_attribute", key},
                                                             {"conformance_activity_key", extra}});
    if (d["error"].kind != Json::Kind::kNull) return fail(d["error"]);
    std::string confs;
    bool first = true;
    for (const auto& x : d["native_result"]["conformance"].items) {
      std::vector<std::string> devs;
      for (const auto& dv : x["deviations"].items) {
        const auto& constraint = dv["constraint"].text;
        if (constraint == "activ_freq") devs.push_back("activ_freq:" + lbl(dv["activity"]) + "=" + dv["observed_frequency"].text);
        else devs.push_back(constraint + ":" + lbl(dv["relation"]["source"]) + ">" + lbl(dv["relation"]["target"]));
      }
      confs += (first ? "" : " | ") + std::string("[") + sorted_join(devs, ",") + "] " + x["no_dev_total"].text + "/" +
               x["no_constr_total"].text + " " + f_digits(x["dev_fitness"], 6) + " " + x["is_fit"].text;
      first = false;
    }
    return "C " + confs;
  }
  return "unknown kind " + kind;
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
    std::string label, name, content, kind, family, key, extra, expected;
  };
  const std::string f0 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:47.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"0.5\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"-1\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:02.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"9\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:32.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:10.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"9\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:52.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"0.5\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:22.000+00:00\"/><int key=\"K1\" value=\"0\"/><float key=\"K2\" value=\"1.5\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:02.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:10.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"9\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c4\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:48.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"x y\"/></event>\n</trace>\n</log>\n";
  const std::string f1 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:27.000+00:00\"/><int key=\"K1\" value=\"-1\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"x y\"/></event>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:35.000+00:00\"/><int key=\"K1\" value=\"-1\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"x y\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:20.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:51.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:58.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:02.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:24.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:59.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:08.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"1.5\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"-1\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:31.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:40.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:42.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n</log>\n";
  const std::string f2 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:15.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"0.5\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"x y\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:35.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:52.000+00:00\"/><int key=\"K1\" value=\"10\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"f\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:35.000+00:00\"/><int key=\"K1\" value=\"1\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"f\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:02.000+00:00\"/><int key=\"K1\" value=\"1\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"9\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:14.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:58.000+00:00\"/><int key=\"K1\" value=\"10\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:27.000+00:00\"/><int key=\"K1\" value=\"7\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:39.000+00:00\"/><int key=\"K1\" value=\"10\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:52.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:40.000+00:00\"/><int key=\"K1\" value=\"10\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n</trace>\n</log>\n";
  const std::string f3 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:15.000+00:00\"/><int key=\"K1\" value=\"7\"/><float key=\"K2\" value=\"1.5\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:25.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:10.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"inf\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:42.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:18.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:23.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:48.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"B\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:33.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:46.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"inf\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:06.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"inf\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"10\"/></event>\n</trace>\n</log>\n";
  const std::string f4 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:32.000+00:00\"/><int key=\"K1\" value=\"10\"/><float key=\"K2\" value=\"0.5\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:02.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:10.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:29.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:12.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"9\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:51.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"f\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:05.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"z\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:47.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:25.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:30.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:05.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:43.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:06:22.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"1.5\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"f\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:06:30.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"z\"/></event>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:06:54.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:07:37.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:07:51.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:07:54.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:08:22.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:08:53.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"10\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c4\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:09:43.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:10:20.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:10:48.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:11:26.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"1.5\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"-1\"/></event>\n</trace>\n</log>\n";
  const std::string f5 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:39.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"1.5\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:46.000+00:00\"/><int key=\"K1\" value=\"7\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"B\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:36.000+00:00\"/><int key=\"K1\" value=\"10\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"x y\"/></event>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:13.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"f\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:25.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"inf\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:04.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"1.5\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n</log>\n";
  const std::string f6 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:40.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:42.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:09.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:29.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:41.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"x y\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:17.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:02.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:35.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:23.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"x y\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:25.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:59.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:33.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"a\"/></event>\n</trace>\n</log>\n";
  const std::string f7 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:16.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:42.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:52.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:31.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:06.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:52.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:39.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:18.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:29.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:53.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:16.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:43.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:06:09.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"9\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c4\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:06:17.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:06:26.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:07:14.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n</log>\n";
  const std::string f8 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:23.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:03.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"f\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:13.000+00:00\"/><int key=\"K1\" value=\"1\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"x y\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:55.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:40.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:45.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:05.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:18.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:08.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"z\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:40.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"9\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:28.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"z\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c4\"/>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:31.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:06:07.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:06:44.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:06:51.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"9\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c5\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:07:14.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"z\"/></event>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:07:55.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:08:00.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:08:24.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"z\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:08:55.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"9\"/></event>\n</trace>\n</log>\n";
  const std::string f9 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:40.000+00:00\"/><int key=\"K1\" value=\"10\"/><float key=\"K2\" value=\"inf\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:08.000+00:00\"/><int key=\"K1\" value=\"1\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:36.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"1\"/><string key=\"K4\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"B\"/></event>\n</trace>\n</log>\n";
  const std::string f10 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:29.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><string key=\"K2\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"9\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:01.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"f\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:36.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"false\"/><string key=\"K4\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:07.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"a\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:48.000+00:00\"/><string key=\"K1\"/><float key=\"K2\" value=\"1.0\"/><string key=\"K3\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:54.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><string key=\"K5\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:56.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:14.000+00:00\"/><int key=\"K1\" value=\"-1\"/><float key=\"K2\" value=\"1.0\"/><string key=\"K3\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"-1\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:52.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\"/></event>\n<event><string key=\"concept:name\" value=\"f\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:38.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c4\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:08.000+00:00\"/><int key=\"K1\" value=\"-1\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:22.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"1\"/><string key=\"K4\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:48.000+00:00\"/><string key=\"K1\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"f\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:06:25.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:06:34.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"-3.5\"/><string key=\"K3\"/><string key=\"K4\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:06:50.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"0.5\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\"/></event>\n</trace>\n</log>\n";
  const std::string f11 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:24.000+00:00\"/><int key=\"K1\" value=\"2\"/><string key=\"K2\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><string key=\"K5\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:35.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:07.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"7.0\"/><string key=\"K3\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:31.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"1\"/><string key=\"K4\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:01.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><string key=\"K5\"/><string key=\"K6\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:06.000+00:00\"/><int key=\"K1\" value=\"3\"/><string key=\"K2\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><string key=\"K5\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:17.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:51.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:25.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:38.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:06.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:24.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:08.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:35.000+00:00\"/><int key=\"K1\" value=\"2\"/><string key=\"K2\"/><string key=\"K3\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><string key=\"K5\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:40.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"1e3\"/><string key=\"K3\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:06:13.000+00:00\"/><string key=\"K1\"/><float key=\"K2\" value=\"7.0\"/><string key=\"K3\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><string key=\"K5\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n</log>\n";
  const std::string f12 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:31.000+00:00\"/><int key=\"K1\" value=\"0\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"z\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:07.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:52.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"2.25\"/><string key=\"K3\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"10\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:42.000+00:00\"/><int key=\"K1\" value=\"0\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"z\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:24.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><string key=\"K2\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:35.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><string key=\"K5\"/><string key=\"K6\" value=\"10\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:09.000+00:00\"/><int key=\"K1\" value=\"0\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"z\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:14.000+00:00\"/><int key=\"K1\" value=\"7\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:45.000+00:00\"/><int key=\"K1\" value=\"0\"/><string key=\"K2\"/><string key=\"K3\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"z\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:16.000+00:00\"/><int key=\"K1\" value=\"0\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"z\"/></event>\n</trace>\n</log>\n";
  const std::string f13 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:04.000+00:00\"/><int key=\"K1\" value=\"10\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"z\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:38.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:47.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"1.0\"/><string key=\"K3\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"B\"/></event>\n</trace>\n</log>\n";
  const std::string f14 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:07.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:33.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><string key=\"K5\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:15.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:42.000+00:00\"/><int key=\"K1\" value=\"0\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:47.000+00:00\"/><int key=\"K1\" value=\"0\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"FALSE\"/><string key=\"K4\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:18.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"inf\"/><string key=\"K3\"/><string key=\"K4\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:26.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"0.5\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"-1\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:52.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:17.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:54.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"inf\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n</log>\n";
  const std::string f15 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,d,2021-03-01 08:08:00,True,p,1.5,2021-03-01 08:00:00,2,True,b\nc0,b,2021-03-01 08:09:00,False,q,3.0,2021-03-02 09:30:00,,False,1\nc1,c,2021-03-01 08:10:00,True,r,3.0,2021-03-01 08:00:00,3,,a\nc1,a,,True,q,2.5,2021-03-02 09:30:00,1,False,\nc1,a,2021-03-01 08:17:00,True,r,3.0,2021-03-01 08:00:00,3,False,a\nc2,c,2021-03-01 08:18:00,True,r,2.5,,,True,a\nc2,c,2021-03-01 08:21:00,False,,1.5,,1,True,b\nc2,d,2021-03-01 08:23:00,True,p,,,2,True,1\n";
  const std::string f16 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-11 18:00:00,1,p,,2021-03-01 08:00:00,1,True,a\nc0,a,2021-03-12 14:00:00,18446744073709551625,,3.0,2021-03-01 08:00:00,2,True,1\nc0,a,2021-03-13 10:00:00,2,r,2.5,2021-03-01 08:00:00,3,False,a\nc0,c,2021-03-15 02:00:00,0,,1.5,2021-03-02 09:30:00,1,False,1\n";
  const std::string f17 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-11 08:00:00,0,,2.5,,3,True,\n";
  const std::string f18 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 08:05:00,9223372036854775809,p,1.5,2021-03-02 09:30:00,,True,a\nc0,a,2021-03-01 08:09:00,3,,3.0,2021-03-02 09:30:00,1,True,b\nc0,d,2021-03-01 08:11:00,9223372036854775809,p,3.0,,,True,a\n";
  const std::string f19 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,c,,0,r,1.5,2021-03-01 08:00:00,7,False,\nc0,a,2021-03-02 04:00:00,False,s,,2021-03-03 10:00:00,,False,a\nc0,d,2021-03-02 06:00:00,True,,-1.0,2021-03-02 09:30:00,,True,\nc0,d,2021-03-02 07:00:00,False,p,-1.0,2021-03-03 10:00:00,1,False,\nc0,a,2021-03-02 10:00:00,True,s,,2021-03-02 09:30:00,7,False,b\nc1,b,2021-03-02 09:00:00,1,,2.5,2021-03-02 09:30:00,3,True,a\nc1,b,2021-03-02 13:00:00,0,,,,,False,1\nc2,d,2021-03-01 13:00:00,1,,2.5,2021-03-03 10:00:00,7,False,b\nc3,a,2021-03-01 09:00:00,1,,,,,,\nc3,a,2021-03-01 11:00:00,False,q,,,,,\nc3,d,,0,,1.5,2021-03-01 08:00:00,,False,b\nc3,b,2021-03-01 15:00:00,2,q,1.5,2021-03-03 10:00:00,,False,\nc3,a,2021-03-01 19:00:00,True,q,1.5,2021-03-02 09:30:00,7,False,\n";
  const std::string f20 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:32:00,2021-03-01 08:31:00,p,1.5,,,True,b\nc0,c,2021-03-01 08:35:00,2021-03-01 08:34:00,r,1.5,2021-03-01 08:00:00,1,,1\nc0,a,,2021-03-01 08:33:00,q,2.5,,3,False,a\nc0,b,,2021-03-01 08:34:00,p,1.5,2021-03-01 08:00:00,3,False,1\nc1,a,2021-03-01 08:15:00,2021-03-01 08:14:00,p,3.0,2021-03-01 08:00:00,2,False,\nc2,c,,2021-03-01 08:05:00,p,3.0,,3,False,b\nc2,a,2021-03-01 08:07:00,2021-03-01 08:07:00,r,3.0,,2,True,1\nc2,c,2021-03-01 08:07:00,2021-03-01 08:04:00,r,,2021-03-02 09:30:00,,False,1\nc2,a,2021-03-01 08:11:00,2021-03-01 08:08:00,r,1.5,2021-03-02 09:30:00,3,,1\nc3,a,,2021-03-01 08:28:00,q,1.5,,3,,\nc3,c,2021-03-01 08:30:00,2021-03-01 08:29:00,r,1.5,2021-03-01 08:00:00,1,True,b\nc3,d,2021-03-01 08:31:00,2021-03-01 08:28:00,r,,,2,False,b\nc4,b,,2021-03-01 08:20:00,,3.0,,1,False,b\nc4,a,2021-03-01 08:23:00,2021-03-01 08:22:00,q,2.5,,1,False,\nc4,d,2021-03-01 08:26:00,2021-03-01 08:23:00,,1.5,2021-03-01 08:00:00,3,True,1\nc4,a,2021-03-01 08:30:00,2021-03-01 08:30:00,q,,2021-03-01 08:00:00,1,,1\n";
  const std::string f21 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:21:00,late,p,2.5,,1,True,a\nc0,b,2021-03-01 08:23:00,2021-03-01 08:23:00,p,,2021-03-01 08:00:00,1,False,a\nc0,c,,2021-03-01 08:24:00,q,2.5,2021-03-01 08:00:00,2,True,\nc0,c,,NA,p,,2021-03-01 08:00:00,1,True,\nc0,c,2021-03-01 08:27:00,,,2.5,2021-03-01 08:00:00,2,False,1\nc1,d,2021-03-01 08:14:00,NA,,,,2,True,\nc2,b,,2021-03-01 08:07:00,p,,2021-03-01 08:00:00,2,False,1\n";
  const std::string f22 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,d,2021-03-01 08:02:00,2021-03-01 08:00:00,p,2.5,,,,\nc0,d,2021-03-01 08:04:00,2021-03-01 08:04:00,q,1.5,2021-03-01 08:00:00,1,True,a\nc0,b,2021-03-01 08:05:00,2021-03-01 08:05:00,q,2.5,,1,False,a\nc0,a,2021-03-01 08:07:00,2021-03-01 08:07:00,p,,2021-03-01 08:00:00,1,,\nc1,d,2021-03-01 08:30:00,,,2.5,2021-03-01 08:00:00,1,False,a\nc1,d,2021-03-01 08:30:00,,q,,2021-03-01 08:00:00,1,False,a\nc2,b,2021-03-01 08:05:00,2021-03-01 08:05:00,,2.5,2021-03-01 08:00:00,1,False,1\nc2,c,2021-03-01 08:07:00,2021-03-01 08:07:00,,1.5,2021-03-01 08:00:00,2,,1\n";
  const std::string f23 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 08:33:00,4,r,3.0,,3,True,\nc0,d,2021-03-01 08:34:00,18446744073709551620,q,2.5,2021-03-01 08:00:00,3,True,b\nc0,a,2021-03-01 08:36:00,7,r,,,2,False,b\nc0,b,,8,q,,2021-03-01 08:00:00,2,,a\nc1,d,2021-03-01 08:00:00,2,q,2.5,,,True,1\nc1,a,2021-03-01 08:03:00,18446744073709551617,p,1.5,2021-03-01 08:00:00,1,True,\nc1,d,2021-03-01 08:03:00,3,q,2.5,,,False,1\nc1,b,2021-03-01 08:06:00,2,p,1.5,2021-03-02 09:30:00,2,True,a\n";
  const std::string f24 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,d,,True,r,,2021-03-02 09:30:00,3,,1\nc0,a,2021-03-01 08:28:00,True,q,2.5,,2,False,a\nc0,d,2021-03-01 08:31:00,False,q,3.0,2021-03-01 08:00:00,3,True,a\nc1,d,2021-03-01 08:18:00,False,q,,2021-03-01 08:00:00,1,False,\nc2,a,,,q,,2021-03-02 09:30:00,2,,1\n";
  const std::string f25 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 08:24:00,True,p,2.5,2021-03-01 08:00:00,1,True,\nc1,b,2021-03-01 08:03:00,True,r,3.0,,2,False,\nc1,d,2021-03-01 08:06:00,False,q,,,,True,\nc1,c,2021-03-01 08:06:00,True,q,2.5,,,True,\nc1,a,2021-03-01 08:10:00,True,q,1.5,2021-03-01 08:00:00,2,,b\nc1,b,2021-03-01 08:10:00,True,r,,2021-03-02 09:30:00,3,False,1\n";
  const std::string f26 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 08:05:00,late,,3.0,2021-03-01 08:00:00,2,True,1\nc1,a,2021-03-01 08:27:00,late,s,,2021-03-03 10:00:00,2,False,1\n";
  const std::vector<Case> cases = {
      {"f90000.xes eva alpha K1 rf_align", "f90000.xes", f0, "eva", "alpha", "K1", "rf_align", "error: can only concatenate str (not \"int\") to str"},
      {"f90000.xes eva alpha K1 rf_auto", "f90000.xes", f0, "eva", "alpha", "K1", "rf_auto", "error: can only concatenate str (not \"int\") to str"},
      {"f90000.xes eva alpha K1 pr_aaa", "f90000.xes", f0, "eva", "alpha", "K1", "pr_aaa", "error: can only concatenate str (not \"int\") to str"},
      {"f90000.xes eva hpp K1 rf_align", "f90000.xes", f0, "eva", "hpp", "K1", "rf_align", "mine error: can only concatenate str (not \"int\") to str"},
      {"f90000.xes eva hpp K1 rf_auto", "f90000.xes", f0, "eva", "hpp", "K1", "rf_auto", "mine error: can only concatenate str (not \"int\") to str"},
      {"f90000.xes eva hpp K1 pr_aaa", "f90000.xes", f0, "eva", "hpp", "K1", "pr_aaa", "mine error: can only concatenate str (not \"int\") to str"},
      {"f90000.xes msd K1", "f90000.xes", f0, "msd", "", "K1", "", "MSD 42=0[]"},
      {"f90000.xes fp K1", "f90000.xes", f0, "fp", "", "K1", "", "FP seq[0>42,3>0] par[42>42] st[100,3,42] en[100,3,42] act[0,100,3,42] dfg[0>42=1,3>0=1,42>42=2] min 1 | T seq[] par[] st[3] en[3] act[3] dfg[] min 1 / seq[] par[] st[42] en[42] act[42] dfg[] min 1 / seq[] par[42>42] st[42] en[42] act[42] dfg[42>42=1] min 2 / seq[0>42,3>0] par[42>42] st[3] en[42] act[0,3,42] dfg[0>42=1,3>0=1,42>42=1] min 4 / seq[] par[] st[100] en[100] act[100] dfg[] min 1 | M seq[3>0] par[] | V[0>42,42>42]"},
      {"f90000.xes lsx K1 cn", "f90000.xes", f0, "lsx", "", "K1", "concept:name", "C [activ_freq:b=0] 1/1 0.000000 false | [activ_freq:d=0] 1/1 0.000000 false | [activ_freq:d=0] 1/1 0.000000 false | [activ_freq:b=0,activ_freq:c=0,activ_freq:d=0] 3/3 0.000000 false | [activ_freq:a=0] 1/1 0.000000 false"},
      {"f90000.xes lsx K1 next", "f90000.xes", f0, "lsx", "", "K1", "K2", "C [activ_freq:0.5=0] 1/1 0.000000 false | [activ_freq:1000.0=0] 1/1 0.000000 false | [activ_freq:1000.0=0] 1/1 0.000000 false | [activ_freq:0.5=0,activ_freq:1.5=0,activ_freq:1000.0=0] 3/3 0.000000 false | [activ_freq:nan=0] 1/1 0.000000 false"},
      {"f90000.xes lsx K1 same", "f90000.xes", f0, "lsx", "", "K1", "K1", "C [] 0/2 1.000000 true | [] 0/2 1.000000 true | [] 0/2 1.000000 true | [] 0/10 1.000000 true | [] 0/4 1.000000 true"},
      {"f90000.xes eva alpha K2 rf_align", "f90000.xes", f0, "eva", "alpha", "K2", "rf_align", "error: can only concatenate str (not \"float\") to str"},
      {"f90000.xes eva alpha K2 rf_auto", "f90000.xes", f0, "eva", "alpha", "K2", "rf_auto", "error: can only concatenate str (not \"float\") to str"},
      {"f90000.xes eva alpha K2 pr_aaa", "f90000.xes", f0, "eva", "alpha", "K2", "pr_aaa", "error: can only concatenate str (not \"float\") to str"},
      {"f90000.xes eva hpp K2 rf_align", "f90000.xes", f0, "eva", "hpp", "K2", "rf_align", "mine error: can only concatenate str (not \"float\") to str"},
      {"f90000.xes eva hpp K2 rf_auto", "f90000.xes", f0, "eva", "hpp", "K2", "rf_auto", "mine error: can only concatenate str (not \"float\") to str"},
      {"f90000.xes eva hpp K2 pr_aaa", "f90000.xes", f0, "eva", "hpp", "K2", "pr_aaa", "mine error: can only concatenate str (not \"float\") to str"},
      {"f90000.xes msd K2", "f90000.xes", f0, "msd", "", "K2", "", "MSD 1000.0=0[]"},
      {"f90000.xes fp K2", "f90000.xes", f0, "fp", "", "K2", "", "FP seq[0.5>1.5,1.5>1000.0] par[1000.0>1000.0] st[0.5,1000.0,nan] en[0.5,1000.0,nan] act[0.5,1.5,1000.0,nan] dfg[0.5>1.5=1,1.5>1000.0=1,1000.0>1000.0=2] min 1 | T seq[] par[] st[0.5] en[0.5] act[0.5] dfg[] min 1 / seq[] par[] st[1000.0] en[1000.0] act[1000.0] dfg[] min 1 / seq[] par[1000.0>1000.0] st[1000.0] en[1000.0] act[1000.0] dfg[1000.0>1000.0=1] min 2 / seq[0.5>1.5,1.5>1000.0] par[1000.0>1000.0] st[0.5] en[1000.0] act[0.5,1.5,1000.0] dfg[0.5>1.5=1,1.5>1000.0=1,1000.0>1000.0=1] min 4 / seq[] par[] st[nan] en[nan] act[nan] dfg[] min 1 | M seq[0.5>1.5] par[] | V[1.5>1000.0,1000.0>1000.0]"},
      {"f90000.xes lsx K2 cn", "f90000.xes", f0, "lsx", "", "K2", "concept:name", "C [activ_freq:b=0] 1/1 0.000000 false | [activ_freq:d=0] 1/1 0.000000 false | [activ_freq:d=0] 1/1 0.000000 false | [activ_freq:b=0,activ_freq:c=0,activ_freq:d=0] 3/3 0.000000 false | [activ_freq:a=0] 1/1 0.000000 false"},
      {"f90000.xes lsx K2 next", "f90000.xes", f0, "lsx", "", "K2", "K3", "C [activ_freq:True=0] 1/1 0.000000 false | [activ_freq:False=0] 1/1 0.000000 false | [activ_freq:False=0] 1/1 0.000000 false | [activ_freq:False=0,activ_freq:True=0] 2/2 0.000000 false | [activ_freq:False=0] 1/1 0.000000 false"},
      {"f90000.xes lsx K2 same", "f90000.xes", f0, "lsx", "", "K2", "K2", "C [] 0/2 1.000000 true | [] 0/2 1.000000 true | [] 0/2 1.000000 true | [] 0/10 1.000000 true | [] 0/5 1.000000 true"},
      {"f90000.xes eva alpha K3 rf_align", "f90000.xes", f0, "eva", "alpha", "K3", "rf_align", "error: can only concatenate str (not \"bool\") to str"},
      {"f90000.xes eva alpha K3 rf_auto", "f90000.xes", f0, "eva", "alpha", "K3", "rf_auto", "error: can only concatenate str (not \"bool\") to str"},
      {"f90000.xes eva alpha K3 pr_aaa", "f90000.xes", f0, "eva", "alpha", "K3", "pr_aaa", "error: can only concatenate str (not \"bool\") to str"},
      {"f90000.xes eva hpp K3 rf_align", "f90000.xes", f0, "eva", "hpp", "K3", "rf_align", "mine error: can only concatenate str (not \"bool\") to str"},
      {"f90000.xes eva hpp K3 rf_auto", "f90000.xes", f0, "eva", "hpp", "K3", "rf_auto", "mine error: can only concatenate str (not \"bool\") to str"},
      {"f90000.xes eva hpp K3 pr_aaa", "f90000.xes", f0, "eva", "hpp", "K3", "pr_aaa", "mine error: can only concatenate str (not \"bool\") to str"},
      {"f90000.xes msd K3", "f90000.xes", f0, "msd", "", "K3", "", "MSD False=0[]"},
      {"f90000.xes fp K3", "f90000.xes", f0, "fp", "", "K3", "", "FP seq[True>False] par[False>False] st[False,True] en[False,True] act[False,True] dfg[False>False=3,True>False=1] min 1 | T seq[] par[] st[True] en[True] act[True] dfg[] min 1 / seq[] par[] st[False] en[False] act[False] dfg[] min 1 / seq[] par[False>False] st[False] en[False] act[False] dfg[False>False=1] min 2 / seq[True>False] par[False>False] st[True] en[False] act[False,True] dfg[False>False=2,True>False=1] min 4 / seq[] par[] st[False] en[False] act[False] dfg[] min 1 | M seq[] par[] | V[False>False,True>False]"},
      {"f90000.xes lsx K3 cn", "f90000.xes", f0, "lsx", "", "K3", "concept:name", "C [activ_freq:b=0] 1/1 0.000000 false | [activ_freq:d=0] 1/1 0.000000 false | [activ_freq:d=0] 1/1 0.000000 false | [activ_freq:b=0,activ_freq:c=0,activ_freq:d=0] 3/3 0.000000 false | [activ_freq:a=0] 1/1 0.000000 false"},
      {"f90000.xes lsx K3 next", "f90000.xes", f0, "lsx", "", "K3", "K4", "C [activ_freq:2021-03-01 08:00:00+00:00=0] 1/1 0.000000 false | [activ_freq:2020-12-31 23:59:59.999999+00:00=0] 1/1 0.000000 false | [activ_freq:2020-12-31 23:59:59.999999+00:00=0] 1/1 0.000000 false | [activ_freq:2020-12-31 23:59:59.999999+00:00=0,activ_freq:2021-03-01 08:00:00+00:00=0,activ_freq:2021-03-01 09:00:00+00:00=0] 3/3 0.000000 false | [activ_freq:2021-03-03 10:30:00+00:00=0] 1/1 0.000000 false"},
      {"f90000.xes lsx K3 same", "f90000.xes", f0, "lsx", "", "K3", "K3", "C [] 0/1 1.000000 true | [] 0/1 1.000000 true | [] 0/1 1.000000 true | [] 0/2 1.000000 true | [] 0/1 1.000000 true"},
      {"f90000.xes eva alpha K4 rf_align", "f90000.xes", f0, "eva", "alpha", "K4", "rf_align", "error: can only concatenate str (not \"datetime.datetime\") to str"},
      {"f90000.xes eva alpha K4 rf_auto", "f90000.xes", f0, "eva", "alpha", "K4", "rf_auto", "error: can only concatenate str (not \"datetime.datetime\") to str"},
      {"f90000.xes eva alpha K4 pr_aaa", "f90000.xes", f0, "eva", "alpha", "K4", "pr_aaa", "error: can only concatenate str (not \"datetime.datetime\") to str"},
      {"f90000.xes eva hpp K4 rf_align", "f90000.xes", f0, "eva", "hpp", "K4", "rf_align", "mine error: can only concatenate str (not \"datetime.datetime\") to str"},
      {"f90000.xes eva hpp K4 rf_auto", "f90000.xes", f0, "eva", "hpp", "K4", "rf_auto", "mine error: can only concatenate str (not \"datetime.datetime\") to str"},
      {"f90000.xes eva hpp K4 pr_aaa", "f90000.xes", f0, "eva", "hpp", "K4", "pr_aaa", "mine error: can only concatenate str (not \"datetime.datetime\") to str"},
      {"f90000.xes msd K4", "f90000.xes", f0, "msd", "", "K4", "", "MSD 2020-12-31 23:59:59.999999+00:00=0[]"},
      {"f90000.xes fp K4", "f90000.xes", f0, "fp", "", "K4", "", "FP seq[2021-03-01 08:00:00+00:00>2021-03-01 09:00:00+00:00,2021-03-01 09:00:00+00:00>2020-12-31 23:59:59.999999+00:00] par[2020-12-31 23:59:59.999999+00:00>2020-12-31 23:59:59.999999+00:00] st[2020-12-31 23:59:59.999999+00:00,2021-03-01 08:00:00+00:00,2021-03-03 10:30:00+00:00] en[2020-12-31 23:59:59.999999+00:00,2021-03-01 08:00:00+00:00,2021-03-03 10:30:00+00:00] act[2020-12-31 23:59:59.999999+00:00,2021-03-01 08:00:00+00:00,2021-03-01 09:00:00+00:00,2021-03-03 10:30:00+00:00] dfg[2020-12-31 23:59:59.999999+00:00>2020-12-31 23:59:59.999999+00:00=2,2021-03-01 08:00:00+00:00>2021-03-01 09:00:00+00:00=1,2021-03-01 09:00:00+00:00>2020-12-31 23:59:59.999999+00:00=1] min 1 | T seq[] par[] st[2021-03-01 08:00:00+00:00] en[2021-03-01 08:00:00+00:00] act[2021-03-01 08:00:00+00:00] dfg[] min 1 / seq[] par[] st[2020-12-31 23:59:59.999999+00:00] en[2020-12-31 23:59:59.999999+00:00] act[2020-12-31 23:59:59.999999+00:00] dfg[] min 1 / seq[] par[2020-12-31 23:59:59.999999+00:00>2020-12-31 23:59:59.999999+00:00] st[2020-12-31 23:59:59.999999+00:00] en[2020-12-31 23:59:59.999999+00:00] act[2020-12-31 23:59:59.999999+00:00] dfg[2020-12-31 23:59:59.999999+00:00>2020-12-31 23:59:59.999999+00:00=1] min 2 / seq[2021-03-01 08:00:00+00:00>2021-03-01 09:00:00+00:00,2021-03-01 09:00:00+00:00>2020-12-31 23:59:59.999999+00:00] par[2020-12-31 23:59:59.999999+00:00>2020-12-31 23:59:59.999999+00:00] st[2021-03-01 08:00:00+00:00] en[2020-12-31 23:59:59.999999+00:00] act[2020-12-31 23:59:59.999999+00:00,2021-03-01 08:00:00+00:00,2021-03-01 09:00:00+00:00] dfg[2020-12-31 23:59:59.999999+00:00>2020-12-31 23:59:59.999999+00:00=1,2021-03-01 08:00:00+00:00>2021-03-01 09:00:00+00:00=1,2021-03-01 09:00:00+00:00>2020-12-31 23:59:59.999999+00:00=1] min 4 / seq[] par[] st[2021-03-03 10:30:00+00:00] en[2021-03-03 10:30:00+00:00] act[2021-03-03 10:30:00+00:00] dfg[] min 1 | M seq[2021-03-01 08:00:00+00:00>2021-03-01 09:00:00+00:00] par[] | V[2020-12-31 23:59:59.999999+00:00>2020-12-31 23:59:59.999999+00:00,2021-03-01 09:00:00+00:00>2020-12-31 23:59:59.999999+00:00]"},
      {"f90000.xes lsx K4 cn", "f90000.xes", f0, "lsx", "", "K4", "concept:name", "C [activ_freq:b=0] 1/1 0.000000 false | [activ_freq:d=0] 1/1 0.000000 false | [activ_freq:d=0] 1/1 0.000000 false | [activ_freq:b=0,activ_freq:c=0,activ_freq:d=0] 3/3 0.000000 false | [activ_freq:a=0] 1/1 0.000000 false"},
      {"f90000.xes lsx K4 next", "f90000.xes", f0, "lsx", "", "K4", "K5", "C [activ_freq:-1=0] 1/1 0.000000 false | [activ_freq:2.5=0] 1/1 0.000000 false | [activ_freq:2.5=0] 1/1 0.000000 false | [activ_freq:-1=0,activ_freq:0.5=0,activ_freq:2.5=0] 3/3 0.000000 false | [activ_freq:10=0] 1/1 0.000000 false"},
      {"f90000.xes lsx K4 same", "f90000.xes", f0, "lsx", "", "K4", "K4", "C [] 0/2 1.000000 true | [] 0/2 1.000000 true | [] 0/2 1.000000 true | [] 0/10 1.000000 true | [] 0/4 1.000000 true"},
      {"f90000.xes eva alpha K5 rf_align", "f90000.xes", f0, "eva", "alpha", "K5", "rf_align", "error: can only concatenate str (not \"int\") to str"},
      {"f90000.xes eva alpha K5 rf_auto", "f90000.xes", f0, "eva", "alpha", "K5", "rf_auto", "error: can only concatenate str (not \"int\") to str"},
      {"f90000.xes eva alpha K5 pr_aaa", "f90000.xes", f0, "eva", "alpha", "K5", "pr_aaa", "error: can only concatenate str (not \"int\") to str"},
      {"f90000.xes eva hpp K5 rf_align", "f90000.xes", f0, "eva", "hpp", "K5", "rf_align", "mine error: can only concatenate str (not \"float\") to str"},
      {"f90000.xes eva hpp K5 rf_auto", "f90000.xes", f0, "eva", "hpp", "K5", "rf_auto", "mine error: can only concatenate str (not \"float\") to str"},
      {"f90000.xes eva hpp K5 pr_aaa", "f90000.xes", f0, "eva", "hpp", "K5", "pr_aaa", "mine error: can only concatenate str (not \"float\") to str"},
      {"f90000.xes msd K5", "f90000.xes", f0, "msd", "", "K5", "", "MSD 2.5=0[]"},
      {"f90000.xes fp K5", "f90000.xes", f0, "fp", "", "K5", "", "FP seq[-1>0.5,0.5>2.5] par[2.5>2.5] st[-1,10,2.5] en[-1,10,2.5] act[-1,0.5,10,2.5] dfg[-1>0.5=1,0.5>2.5=1,2.5>2.5=2] min 1 | T seq[] par[] st[-1] en[-1] act[-1] dfg[] min 1 / seq[] par[] st[2.5] en[2.5] act[2.5] dfg[] min 1 / seq[] par[2.5>2.5] st[2.5] en[2.5] act[2.5] dfg[2.5>2.5=1] min 2 / seq[-1>0.5,0.5>2.5] par[2.5>2.5] st[-1] en[2.5] act[-1,0.5,2.5] dfg[-1>0.5=1,0.5>2.5=1,2.5>2.5=1] min 4 / seq[] par[] st[10] en[10] act[10] dfg[] min 1 | M seq[-1>0.5] par[] | V[0.5>2.5,2.5>2.5]"},
      {"f90000.xes lsx K5 cn", "f90000.xes", f0, "lsx", "", "K5", "concept:name", "C [activ_freq:b=0] 1/1 0.000000 false | [activ_freq:d=0] 1/1 0.000000 false | [activ_freq:d=0] 1/1 0.000000 false | [activ_freq:b=0,activ_freq:c=0,activ_freq:d=0] 3/3 0.000000 false | [activ_freq:a=0] 1/1 0.000000 false"},
      {"f90000.xes lsx K5 next", "f90000.xes", f0, "lsx", "", "K5", "K6", "C [activ_freq:-1=0] 1/1 0.000000 false | [activ_freq:9=0] 1/1 0.000000 false | [activ_freq:9=0] 1/1 0.000000 false | [activ_freq:-1=0,activ_freq:9=0,activ_freq:B=0] 3/3 0.000000 false | [activ_freq:x y=0] 1/1 0.000000 false"},
      {"f90000.xes lsx K5 same", "f90000.xes", f0, "lsx", "", "K5", "K5", "C [] 0/2 1.000000 true | [] 0/2 1.000000 true | [] 0/2 1.000000 true | [] 0/10 1.000000 true | [] 0/4 1.000000 true"},
      {"f90000.xes eva alpha K6 rf_align", "f90000.xes", f0, "eva", "alpha", "K6", "rf_align", "F 0.714286/0.753333/40.000000"},
      {"f90000.xes eva alpha K6 rf_auto", "f90000.xes", f0, "eva", "alpha", "K6", "rf_auto", "F 0.714286/0.753333/40.000000"},
      {"f90000.xes eva alpha K6 pr_aaa", "f90000.xes", f0, "eva", "alpha", "K6", "pr_aaa", "P 1.000000"},
      {"f90000.xes eva hpp K6 rf_align", "f90000.xes", f0, "eva", "hpp", "K6", "rf_align", "F 0.857121/0.893333/60.000000"},
      {"f90000.xes eva hpp K6 rf_auto", "f90000.xes", f0, "eva", "hpp", "K6", "rf_auto", "F 0.857121/0.893333/60.000000"},
      {"f90000.xes eva hpp K6 pr_aaa", "f90000.xes", f0, "eva", "hpp", "K6", "pr_aaa", "P 1.000000"},
      {"f90000.xes msd K6", "f90000.xes", f0, "msd", "", "K6", "", "MSD 9=0[]"},
      {"f90000.xes fp K6", "f90000.xes", f0, "fp", "", "K6", "", "FP seq[-1>B,B>9] par[9>9] st[-1,9,x y] en[-1,9,x y] act[-1,9,B,x y] dfg[-1>B=1,9>9=2,B>9=1] min 1 | T seq[] par[] st[-1] en[-1] act[-1] dfg[] min 1 / seq[] par[] st[9] en[9] act[9] dfg[] min 1 / seq[] par[9>9] st[9] en[9] act[9] dfg[9>9=1] min 2 / seq[-1>B,B>9] par[9>9] st[-1] en[9] act[-1,9,B] dfg[-1>B=1,9>9=1,B>9=1] min 4 / seq[] par[] st[x y] en[x y] act[x y] dfg[] min 1 | M seq[-1>B] par[] | V[9>9,B>9]"},
      {"f90000.xes lsx K6 cn", "f90000.xes", f0, "lsx", "", "K6", "concept:name", "C [activ_freq:b=0] 1/1 0.000000 false | [activ_freq:d=0] 1/1 0.000000 false | [activ_freq:d=0] 1/1 0.000000 false | [activ_freq:b=0,activ_freq:c=0,activ_freq:d=0] 3/3 0.000000 false | [activ_freq:a=0] 1/1 0.000000 false"},
      {"f90000.xes lsx K6 next", "f90000.xes", f0, "lsx", "", "K6", "K1", "C [activ_freq:3=0] 1/1 0.000000 false | [activ_freq:42=0] 1/1 0.000000 false | [activ_freq:42=0] 1/1 0.000000 false | [activ_freq:0=0,activ_freq:3=0,activ_freq:42=0] 3/3 0.000000 false | [activ_freq:100=0] 1/1 0.000000 false"},
      {"f90000.xes lsx K6 same", "f90000.xes", f0, "lsx", "", "K6", "K6", "C [] 0/2 1.000000 true | [] 0/2 1.000000 true | [] 0/2 1.000000 true | [] 0/10 1.000000 true | [] 0/4 1.000000 true"},
      {"f90001.xes eva alpha K1 rf_align", "f90001.xes", f1, "eva", "alpha", "K1", "rf_align", "error: can only concatenate str (not \"int\") to str"},
      {"f90001.xes eva alpha K1 rf_auto", "f90001.xes", f1, "eva", "alpha", "K1", "rf_auto", "error: can only concatenate str (not \"int\") to str"},
      {"f90001.xes eva alpha K1 pr_aaa", "f90001.xes", f1, "eva", "alpha", "K1", "pr_aaa", "error: can only concatenate str (not \"int\") to str"},
      {"f90001.xes lsx K1 cn", "f90001.xes", f1, "lsx", "", "K1", "concept:name", "error: '<' not supported between instances of 'int' and 'str'"},
      {"f90001.xes eva alpha K2 rf_align", "f90001.xes", f1, "eva", "alpha", "K2", "rf_align", "error: can only concatenate str (not \"float\") to str"},
      {"f90001.xes eva alpha K2 rf_auto", "f90001.xes", f1, "eva", "alpha", "K2", "rf_auto", "error: can only concatenate str (not \"float\") to str"},
      {"f90001.xes eva alpha K2 pr_aaa", "f90001.xes", f1, "eva", "alpha", "K2", "pr_aaa", "error: can only concatenate str (not \"float\") to str"},
      {"f90001.xes msd K2", "f90001.xes", f1, "msd", "", "K2", "", "MSD -0.0=0[] -3.5=0[] 2.25=0[]"},
      {"f90001.xes lsx K2 cn", "f90001.xes", f1, "lsx", "", "K2", "concept:name", "error: '<' not supported between instances of 'float' and 'str'"},
      {"f90001.xes eva alpha K3 rf_align", "f90001.xes", f1, "eva", "alpha", "K3", "rf_align", "error: can only concatenate str (not \"bool\") to str"},
      {"f90001.xes eva alpha K3 rf_auto", "f90001.xes", f1, "eva", "alpha", "K3", "rf_auto", "error: can only concatenate str (not \"bool\") to str"},
      {"f90001.xes eva alpha K3 pr_aaa", "f90001.xes", f1, "eva", "alpha", "K3", "pr_aaa", "error: can only concatenate str (not \"bool\") to str"},
      {"f90001.xes eva hpp K3 rf_align", "f90001.xes", f1, "eva", "hpp", "K3", "rf_align", "error: can only concatenate str (not \"bool\") to str"},
      {"f90001.xes eva hpp K3 rf_auto", "f90001.xes", f1, "eva", "hpp", "K3", "rf_auto", "error: can only concatenate str (not \"bool\") to str"},
      {"f90001.xes eva hpp K3 pr_aaa", "f90001.xes", f1, "eva", "hpp", "K3", "pr_aaa", "error: can only concatenate str (not \"bool\") to str"},
      {"f90001.xes msd K3", "f90001.xes", f1, "msd", "", "K3", "", "MSD False=0[] True=0[]"},
      {"f90001.xes fp K3", "f90001.xes", f1, "fp", "", "K3", "", "FP seq[] par[False>False,False>True,True>False,True>True] st[False,True] en[True] act[False,True] dfg[False>False=3,False>True=2,True>False=1,True>True=2] min 1 | T seq[False>True] par[False>False] st[False] en[True] act[False,True] dfg[False>False=1,False>True=1] min 3 / seq[] par[False>False,False>True,True>False,True>True] st[True] en[True] act[False,True] dfg[False>False=2,False>True=1,True>False=1,True>True=1] min 6 / seq[] par[] st[True] en[True] act[True] dfg[] min 1 / seq[] par[True>True] st[True] en[True] act[True] dfg[True>True=1] min 2 | M seq[] par[] | V[False>False,False>True,True>False,True>True]"},
      {"f90001.xes lsx K3 cn", "f90001.xes", f1, "lsx", "", "K3", "concept:name", "error: '<' not supported between instances of 'bool' and 'str'"},
      {"f90001.xes lsx K3 next", "f90001.xes", f1, "lsx", "", "K3", "K4", "error: '<' not supported between instances of 'bool' and 'datetime.datetime'"},
      {"f90001.xes eva alpha K4 rf_align", "f90001.xes", f1, "eva", "alpha", "K4", "rf_align", "error: can only concatenate str (not \"datetime.datetime\") to str"},
      {"f90001.xes eva alpha K4 rf_auto", "f90001.xes", f1, "eva", "alpha", "K4", "rf_auto", "error: can only concatenate str (not \"datetime.datetime\") to str"},
      {"f90001.xes eva alpha K4 pr_aaa", "f90001.xes", f1, "eva", "alpha", "K4", "pr_aaa", "error: can only concatenate str (not \"datetime.datetime\") to str"},
      {"f90001.xes msd K4", "f90001.xes", f1, "msd", "", "K4", "", "MSD 2021-03-01 08:00:00+00:00=0[] 2021-03-01 08:00:00.123000+00:00=0[] 2021-03-03 10:30:00+00:00=0[]"},
      {"f90001.xes fp K4", "f90001.xes", f1, "fp", "", "K4", "", "FP seq[2021-03-01 08:00:00+00:00>2021-03-03 10:30:00+00:00,2021-03-01 08:00:00.123000+00:00>2021-03-01 08:00:00+00:00,2021-03-03 10:30:00+00:00>2021-03-01 08:00:00.123000+00:00] par[2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00,2021-03-01 08:00:00.123000+00:00>2021-03-01 08:00:00.123000+00:00,2021-03-03 10:30:00+00:00>2021-03-03 10:30:00+00:00] st[2021-03-01 08:00:00+00:00,2021-03-03 10:30:00+00:00] en[2021-03-01 08:00:00+00:00,2021-03-03 10:30:00+00:00] act[2021-03-01 08:00:00+00:00,2021-03-01 08:00:00.123000+00:00,2021-03-03 10:30:00+00:00] dfg[2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00=1,2021-03-01 08:00:00+00:00>2021-03-03 10:30:00+00:00=1,2021-03-01 08:00:00.123000+00:00>2021-03-01 08:00:00+00:00=1,2021-03-01 08:00:00.123000+00:00>2021-03-01 08:00:00.123000+00:00=2,2021-03-03 10:30:00+00:00>2021-03-01 08:00:00.123000+00:00=1,2021-03-03 10:30:00+00:00>2021-03-03 10:30:00+00:00=2] min 1 | T seq[2021-03-01 08:00:00+00:00>2021-03-03 10:30:00+00:00] par[2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00] st[2021-03-01 08:00:00+00:00] en[2021-03-03 10:30:00+00:00] act[2021-03-01 08:00:00+00:00,2021-03-03 10:30:00+00:00] dfg[2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00=1,2021-03-01 08:00:00+00:00>2021-03-03 10:30:00+00:00=1] min 3 / seq[2021-03-01 08:00:00.123000+00:00>2021-03-01 08:00:00+00:00,2021-03-03 10:30:00+00:00>2021-03-01 08:00:00.123000+00:00] par[2021-03-01 08:00:00.123000+00:00>2021-03-01 08:00:00.123000+00:00,2021-03-03 10:30:00+00:00>2021-03-03 10:30:00+00:00] st[2021-03-03 10:30:00+00:00] en[2021-03-01 08:00:00+00:00] act[2021-03-01 08:00:00+00:00,2021-03-01 08:00:00.123000+00:00,2021-03-03 10:30:00+00:00] dfg[2021-03-01 08:00:00.123000+00:00>2021-03-01 08:00:00+00:00=1,2021-03-01 08:00:00.123000+00:00>2021-03-01 08:00:00.123000+00:00=2,2021-03-03 10:30:00+00:00>2021-03-01 08:00:00.123000+00:00=1,2021-03-03 10:30:00+00:00>2021-03-03 10:30:00+00:00=1] min 6 / seq[] par[] st[2021-03-03 10:30:00+00:00] en[2021-03-03 10:30:00+00:00] act[2021-03-03 10:30:00+00:00] dfg[] min 1 / seq[] par[2021-03-03 10:30:00+00:00>2021-03-03 10:30:00+00:00] st[2021-03-03 10:30:00+00:00] en[2021-03-03 10:30:00+00:00] act[2021-03-03 10:30:00+00:00] dfg[2021-03-03 10:30:00+00:00>2021-03-03 10:30:00+00:00=1] min 2 | M seq[] par[2021-03-01 08:00:00+00:00>2021-03-01 08:00:00.123000+00:00,2021-03-01 08:00:00.123000+00:00>2021-03-01 08:00:00+00:00,2021-03-01 08:00:00.123000+00:00>2021-03-01 08:00:00.123000+00:00,2021-03-01 08:00:00.123000+00:00>2021-03-03 10:30:00+00:00,2021-03-03 10:30:00+00:00>2021-03-01 08:00:00.123000+00:00] | V[2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00,2021-03-01 08:00:00+00:00>2021-03-03 10:30:00+00:00,2021-03-03 10:30:00+00:00>2021-03-03 10:30:00+00:00]"},
      {"f90001.xes lsx K4 cn", "f90001.xes", f1, "lsx", "", "K4", "concept:name", "error: '<' not supported between instances of 'datetime.datetime' and 'str'"},
      {"f90001.xes lsx K4 next", "f90001.xes", f1, "lsx", "", "K4", "K5", "error: '<' not supported between instances of 'datetime.datetime' and 'int'"},
      {"f90001.xes eva alpha K5 rf_align", "f90001.xes", f1, "eva", "alpha", "K5", "rf_align", "error: can only concatenate str (not \"float\") to str"},
      {"f90001.xes eva alpha K5 rf_auto", "f90001.xes", f1, "eva", "alpha", "K5", "rf_auto", "error: can only concatenate str (not \"float\") to str"},
      {"f90001.xes eva alpha K5 pr_aaa", "f90001.xes", f1, "eva", "alpha", "K5", "pr_aaa", "error: can only concatenate str (not \"float\") to str"},
      {"f90001.xes lsx K5 cn", "f90001.xes", f1, "lsx", "", "K5", "concept:name", "error: '<' not supported between instances of 'int' and 'str'"},
      {"f90001.xes lsx K5 next", "f90001.xes", f1, "lsx", "", "K5", "K6", "error: '<' not supported between instances of 'int' and 'str'"},
      {"f90001.xes lsx K6 next", "f90001.xes", f1, "lsx", "", "K6", "K1", "error: '<' not supported between instances of 'str' and 'int'"},
      {"f90002.xes eva alpha K1 rf_align", "f90002.xes", f2, "eva", "alpha", "K1", "rf_align", "error: can only concatenate str (not \"int\") to str"},
      {"f90002.xes eva alpha K1 rf_auto", "f90002.xes", f2, "eva", "alpha", "K1", "rf_auto", "error: can only concatenate str (not \"int\") to str"},
      {"f90002.xes eva alpha K1 pr_aaa", "f90002.xes", f2, "eva", "alpha", "K1", "pr_aaa", "error: can only concatenate str (not \"int\") to str"},
      {"f90002.xes eva alpha K2 rf_align", "f90002.xes", f2, "eva", "alpha", "K2", "rf_align", "error: can only concatenate str (not \"float\") to str"},
      {"f90002.xes eva alpha K2 rf_auto", "f90002.xes", f2, "eva", "alpha", "K2", "rf_auto", "error: can only concatenate str (not \"float\") to str"},
      {"f90002.xes eva alpha K2 pr_aaa", "f90002.xes", f2, "eva", "alpha", "K2", "pr_aaa", "error: can only concatenate str (not \"float\") to str"},
      {"f90002.xes msd K2", "f90002.xes", f2, "msd", "", "K2", "", "MSD -0.0=0[] -3.5=1[2.25,nan]"},
      {"f90002.xes eva alpha K3 rf_align", "f90002.xes", f2, "eva", "alpha", "K3", "rf_align", "error: can only concatenate str (not \"bool\") to str"},
      {"f90002.xes eva alpha K3 rf_auto", "f90002.xes", f2, "eva", "alpha", "K3", "rf_auto", "error: can only concatenate str (not \"bool\") to str"},
      {"f90002.xes eva alpha K3 pr_aaa", "f90002.xes", f2, "eva", "alpha", "K3", "pr_aaa", "error: can only concatenate str (not \"bool\") to str"},
      {"f90002.xes eva hpp K3 rf_align", "f90002.xes", f2, "eva", "hpp", "K3", "rf_align", "error: can only concatenate str (not \"bool\") to str"},
      {"f90002.xes eva hpp K3 rf_auto", "f90002.xes", f2, "eva", "hpp", "K3", "rf_auto", "error: can only concatenate str (not \"bool\") to str"},
      {"f90002.xes eva hpp K3 pr_aaa", "f90002.xes", f2, "eva", "hpp", "K3", "pr_aaa", "error: can only concatenate str (not \"bool\") to str"},
      {"f90002.xes msd K3", "f90002.xes", f2, "msd", "", "K3", "", "MSD False=0[]"},
      {"f90002.xes fp K3", "f90002.xes", f2, "fp", "", "K3", "", "FP seq[] par[False>False,False>True,True>False] st[False,True] en[False] act[False,True] dfg[False>False=5,False>True=1,True>False=2] min 1 | T seq[] par[] st[False] en[False] act[False] dfg[] min 1 / seq[True>False] par[False>False] st[True] en[False] act[False,True] dfg[False>False=2,True>False=1] min 4 / seq[] par[False>False,False>True,True>False] st[False] en[False] act[False,True] dfg[False>False=3,False>True=1,True>False=1] min 6 | M seq[] par[] | V[False>False,False>True,True>False]"},
      {"f90002.xes lsx K3 cn", "f90002.xes", f2, "lsx", "", "K3", "concept:name", "error: '<' not supported between instances of 'bool' and 'str'"},
      {"f90002.xes lsx K3 next", "f90002.xes", f2, "lsx", "", "K3", "K4", "error: '<' not supported between instances of 'bool' and 'datetime.datetime'"},
      {"f90002.xes eva alpha K4 rf_align", "f90002.xes", f2, "eva", "alpha", "K4", "rf_align", "error: can only concatenate str (not \"datetime.datetime\") to str"},
      {"f90002.xes eva alpha K4 rf_auto", "f90002.xes", f2, "eva", "alpha", "K4", "rf_auto", "error: can only concatenate str (not \"datetime.datetime\") to str"},
      {"f90002.xes eva alpha K4 pr_aaa", "f90002.xes", f2, "eva", "alpha", "K4", "pr_aaa", "error: can only concatenate str (not \"datetime.datetime\") to str"},
      {"f90002.xes msd K4", "f90002.xes", f2, "msd", "", "K4", "", "MSD 2021-03-01 08:00:00+00:00=0[]"},
      {"f90002.xes fp K4", "f90002.xes", f2, "fp", "", "K4", "", "FP seq[2021-03-03 10:30:00+00:00>2021-03-01 08:00:00+00:00] par[2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00,2021-03-01 08:00:00+00:00>2021-03-01 08:00:00.123000+00:00,2021-03-01 08:00:00+00:00>2021-03-01 09:00:00+00:00,2021-03-01 08:00:00.123000+00:00>2021-03-01 08:00:00+00:00,2021-03-01 09:00:00+00:00>2021-03-01 08:00:00+00:00] st[2021-03-01 08:00:00+00:00,2021-03-01 09:00:00+00:00,2021-03-03 10:30:00+00:00] en[2021-03-01 08:00:00+00:00] act[2021-03-01 08:00:00+00:00,2021-03-01 08:00:00.123000+00:00,2021-03-01 09:00:00+00:00,2021-03-03 10:30:00+00:00] dfg[2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00=2,2021-03-01 08:00:00+00:00>2021-03-01 08:00:00.123000+00:00=1,2021-03-01 08:00:00+00:00>2021-03-01 09:00:00+00:00=1,2021-03-01 08:00:00.123000+00:00>2021-03-01 08:00:00+00:00=1,2021-03-01 09:00:00+00:00>2021-03-01 08:00:00+00:00=2,2021-03-03 10:30:00+00:00>2021-03-01 08:00:00+00:00=1] min 1 | T seq[] par[] st[2021-03-01 08:00:00+00:00] en[2021-03-01 08:00:00+00:00] act[2021-03-01 08:00:00+00:00] dfg[] min 1 / seq[2021-03-01 09:00:00+00:00>2021-03-01 08:00:00+00:00] par[2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00] st[2021-03-01 09:00:00+00:00] en[2021-03-01 08:00:00+00:00] act[2021-03-01 08:00:00+00:00,2021-03-01 09:00:00+00:00] dfg[2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00=2,2021-03-01 09:00:00+00:00>2021-03-01 08:00:00+00:00=1] min 4 / seq[2021-03-03 10:30:00+00:00>2021-03-01 08:00:00+00:00] par[2021-03-01 08:00:00+00:00>2021-03-01 08:00:00.123000+00:00,2021-03-01 08:00:00+00:00>2021-03-01 09:00:00+00:00,2021-03-01 08:00:00.123000+00:00>2021-03-01 08:00:00+00:00,2021-03-01 09:00:00+00:00>2021-03-01 08:00:00+00:00] st[2021-03-03 10:30:00+00:00] en[2021-03-01 08:00:00+00:00] act[2021-03-01 08:00:00+00:00,2021-03-01 08:00:00.123000+00:00,2021-03-01 09:00:00+00:00,2021-03-03 10:30:00+00:00] dfg[2021-03-01 08:00:00+00:00>2021-03-01 08:00:00.123000+00:00=1,2021-03-01 08:00:00+00:00>2021-03-01 09:00:00+00:00=1,2021-03-01 08:00:00.123000+00:00>2021-03-01 08:00:00+00:00=1,2021-03-01 09:00:00+00:00>2021-03-01 08:00:00+00:00=1,2021-03-03 10:30:00+00:00>2021-03-01 08:00:00+00:00=1] min 6 | M seq[] par[2021-03-01 08:00:00+00:00>2021-03-01 08:00:00.123000+00:00,2021-03-01 08:00:00.123000+00:00>2021-03-01 08:00:00+00:00,2021-03-01 08:00:00.123000+00:00>2021-03-01 08:00:00.123000+00:00,2021-03-01 08:00:00.123000+00:00>2021-03-01 09:00:00+00:00,2021-03-01 08:00:00.123000+00:00>2021-03-03 10:30:00+00:00,2021-03-01 09:00:00+00:00>2021-03-01 08:00:00.123000+00:00,2021-03-03 10:30:00+00:00>2021-03-01 08:00:00.123000+00:00] | V[2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00,2021-03-01 08:00:00+00:00>2021-03-01 09:00:00+00:00,2021-03-01 09:00:00+00:00>2021-03-01 08:00:00+00:00,2021-03-03 10:30:00+00:00>2021-03-01 08:00:00+00:00]"},
      {"f90002.xes lsx K4 cn", "f90002.xes", f2, "lsx", "", "K4", "concept:name", "error: '<' not supported between instances of 'datetime.datetime' and 'str'"},
      {"f90002.xes lsx K4 next", "f90002.xes", f2, "lsx", "", "K4", "K5", "error: '<' not supported between instances of 'datetime.datetime' and 'int'"},
      {"f90002.xes eva alpha K5 rf_align", "f90002.xes", f2, "eva", "alpha", "K5", "rf_align", "error: can only concatenate str (not \"int\") to str"},
      {"f90002.xes eva alpha K5 rf_auto", "f90002.xes", f2, "eva", "alpha", "K5", "rf_auto", "error: can only concatenate str (not \"int\") to str"},
      {"f90002.xes eva alpha K5 pr_aaa", "f90002.xes", f2, "eva", "alpha", "K5", "pr_aaa", "error: can only concatenate str (not \"int\") to str"},
      {"f90003.xes msd K2", "f90003.xes", f3, "msd", "", "K2", "", "MSD 1.0=0[] inf=0[]"},
      {"f90003.xes eva hpp K3 rf_align", "f90003.xes", f3, "eva", "hpp", "K3", "rf_align", "error: can only concatenate str (not \"bool\") to str"},
      {"f90003.xes eva hpp K3 rf_auto", "f90003.xes", f3, "eva", "hpp", "K3", "rf_auto", "error: can only concatenate str (not \"bool\") to str"},
      {"f90003.xes eva hpp K3 pr_aaa", "f90003.xes", f3, "eva", "hpp", "K3", "pr_aaa", "error: can only concatenate str (not \"bool\") to str"},
      {"f90003.xes lsx K3 cn", "f90003.xes", f3, "lsx", "", "K3", "concept:name", "error: '<' not supported between instances of 'bool' and 'str'"},
      {"f90003.xes lsx K3 next", "f90003.xes", f3, "lsx", "", "K3", "K4", "error: '<' not supported between instances of 'bool' and 'datetime.datetime'"},
      {"f90004.xes lsx K1 cn", "f90004.xes", f4, "lsx", "", "K1", "concept:name", "error: '<' not supported between instances of 'int' and 'str'"},
      {"f90004.xes lsx K2 cn", "f90004.xes", f4, "lsx", "", "K2", "concept:name", "error: '<' not supported between instances of 'float' and 'str'"},
      {"f90004.xes lsx K4 cn", "f90004.xes", f4, "lsx", "", "K4", "concept:name", "error: '<' not supported between instances of 'datetime.datetime' and 'str'"},
      {"f90004.xes lsx K4 next", "f90004.xes", f4, "lsx", "", "K4", "K5", "error: '<' not supported between instances of 'datetime.datetime' and 'int'"},
      {"f90004.xes msd K5", "f90004.xes", f4, "msd", "", "K5", "", "MSD -1=0[] 0.5=0[] 1=1[0.5]"},
      {"f90004.xes lsx K5 cn", "f90004.xes", f4, "lsx", "", "K5", "concept:name", "error: '<' not supported between instances of 'float' and 'str'"},
      {"f90004.xes lsx K5 next", "f90004.xes", f4, "lsx", "", "K5", "K6", "error: '<' not supported between instances of 'float' and 'str'"},
      {"f90004.xes lsx K6 next", "f90004.xes", f4, "lsx", "", "K6", "K1", "error: '<' not supported between instances of 'str' and 'int'"},
      {"f90005.xes fp K2", "f90005.xes", f5, "fp", "", "K2", "", "FP seq[-0.0>inf,7.0>-0.0,inf>1.5] par[] st[1.0,1.5,7.0] en[1.0,1.5] act[-0.0,1.0,1.5,7.0,inf] dfg[-0.0>inf=1,7.0>-0.0=1,inf>1.5=1] min 1 | T seq[] par[] st[1.5] en[1.5] act[1.5] dfg[] min 1 / seq[] par[] st[1.0] en[1.0] act[1.0] dfg[] min 1 / seq[-0.0>inf,7.0>-0.0,inf>1.5] par[] st[7.0] en[1.5] act[-0.0,1.5,7.0,inf] dfg[-0.0>inf=1,7.0>-0.0=1,inf>1.5=1] min 4 | M seq[-0.0>inf,7.0>-0.0] par[] | V[inf>1.5]"},
      {"f90005.xes eva alpha K4 rf_auto", "f90005.xes", f5, "eva", "alpha", "K4", "rf_auto", "F 0.668182/0.738889/0.000000"},
      {"f90008.xes lsx K1 cn", "f90008.xes", f6, "lsx", "", "K1", "concept:name", "error: '<' not supported between instances of 'int' and 'str'"},
      {"f90008.xes lsx K2 cn", "f90008.xes", f6, "lsx", "", "K2", "concept:name", "error: '<' not supported between instances of 'float' and 'str'"},
      {"f90008.xes lsx K5 cn", "f90008.xes", f6, "lsx", "", "K5", "concept:name", "error: '<' not supported between instances of 'int' and 'str'"},
      {"f90008.xes lsx K5 next", "f90008.xes", f6, "lsx", "", "K5", "K6", "error: '<' not supported between instances of 'int' and 'str'"},
      {"f90008.xes lsx K6 next", "f90008.xes", f6, "lsx", "", "K6", "K1", "error: '<' not supported between instances of 'str' and 'int'"},
      {"f90009.xes eva hpp K1 rf_align", "f90009.xes", f7, "eva", "hpp", "K1", "rf_align", "error: can only concatenate str (not \"int\") to str"},
      {"f90009.xes eva hpp K1 rf_auto", "f90009.xes", f7, "eva", "hpp", "K1", "rf_auto", "error: can only concatenate str (not \"int\") to str"},
      {"f90009.xes eva hpp K1 pr_aaa", "f90009.xes", f7, "eva", "hpp", "K1", "pr_aaa", "error: can only concatenate str (not \"int\") to str"},
      {"f90009.xes eva hpp K2 rf_align", "f90009.xes", f7, "eva", "hpp", "K2", "rf_align", "error: can only concatenate str (not \"float\") to str"},
      {"f90009.xes eva hpp K2 rf_auto", "f90009.xes", f7, "eva", "hpp", "K2", "rf_auto", "error: can only concatenate str (not \"float\") to str"},
      {"f90009.xes eva hpp K2 pr_aaa", "f90009.xes", f7, "eva", "hpp", "K2", "pr_aaa", "error: can only concatenate str (not \"float\") to str"},
      {"f90009.xes fp K2", "f90009.xes", f7, "fp", "", "K2", "", "FP seq[] par[1000.0>1000.0,1000.0>7.0,7.0>1000.0,7.0>7.0] st[1000.0,7.0] en[1000.0,7.0] act[1000.0,7.0] dfg[1000.0>1000.0=2,1000.0>7.0=3,7.0>1000.0=4,7.0>7.0=2] min 1 | T seq[] par[1000.0>7.0,7.0>1000.0] st[1000.0] en[1000.0] act[1000.0,7.0] dfg[1000.0>7.0=1,7.0>1000.0=1] min 3 / seq[] par[] st[1000.0] en[1000.0] act[1000.0] dfg[] min 1 / seq[] par[1000.0>7.0,7.0>1000.0,7.0>7.0] st[7.0] en[1000.0] act[1000.0,7.0] dfg[1000.0>7.0=1,7.0>1000.0=2,7.0>7.0=1] min 5 / seq[] par[1000.0>7.0,7.0>1000.0,7.0>7.0] st[7.0] en[7.0] act[1000.0,7.0] dfg[1000.0>7.0=1,7.0>1000.0=1,7.0>7.0=1] min 4 / seq[] par[1000.0>1000.0] st[1000.0] en[1000.0] act[1000.0] dfg[1000.0>1000.0=2] min 3 | M seq[] par[] | V[1000.0>1000.0,1000.0>7.0,7.0>1000.0,7.0>7.0]"},
      {"f90009.xes eva hpp K4 rf_align", "f90009.xes", f7, "eva", "hpp", "K4", "rf_align", "error: can only concatenate str (not \"datetime.datetime\") to str"},
      {"f90009.xes eva hpp K4 rf_auto", "f90009.xes", f7, "eva", "hpp", "K4", "rf_auto", "error: can only concatenate str (not \"datetime.datetime\") to str"},
      {"f90009.xes eva hpp K4 pr_aaa", "f90009.xes", f7, "eva", "hpp", "K4", "pr_aaa", "error: can only concatenate str (not \"datetime.datetime\") to str"},
      {"f90009.xes eva hpp K5 rf_align", "f90009.xes", f7, "eva", "hpp", "K5", "rf_align", "error: can only concatenate str (not \"float\") to str"},
      {"f90009.xes eva hpp K5 rf_auto", "f90009.xes", f7, "eva", "hpp", "K5", "rf_auto", "error: can only concatenate str (not \"float\") to str"},
      {"f90009.xes eva hpp K5 pr_aaa", "f90009.xes", f7, "eva", "hpp", "K5", "pr_aaa", "error: can only concatenate str (not \"float\") to str"},
      {"f90011.xes eva alpha K1 rf_auto", "f90011.xes", f8, "eva", "alpha", "K1", "rf_auto", "F 0.518750/0.566779/0.000000"},
      {"f90011.xes eva alpha K5 rf_auto", "f90011.xes", f8, "eva", "alpha", "K5", "rf_auto", "F 0.518750/0.566779/0.000000"},
      {"f90200.xes msd K4", "f90200.xes", f9, "msd", "", "K4", "", "MSD "},
      {"f90201.xes eva alpha K1 rf_align", "f90201.xes", f10, "eva", "alpha", "K1", "rf_align", "mine error: '<' not supported between instances of 'NoneType' and 'int'"},
      {"f90201.xes eva alpha K1 rf_auto", "f90201.xes", f10, "eva", "alpha", "K1", "rf_auto", "mine error: '<' not supported between instances of 'NoneType' and 'int'"},
      {"f90201.xes eva alpha K1 pr_aaa", "f90201.xes", f10, "eva", "alpha", "K1", "pr_aaa", "mine error: '<' not supported between instances of 'NoneType' and 'int'"},
      {"f90201.xes fp K1", "f90201.xes", f10, "fp", "", "K1", "", "mine error: '<' not supported between instances of 'NoneType' and 'int'"},
      {"f90201.xes lsx K2 next", "f90201.xes", f10, "lsx", "", "K2", "K3", "error: '<' not supported between instances of 'bool' and 'NoneType'"},
      {"f90201.xes eva alpha K3 rf_align", "f90201.xes", f10, "eva", "alpha", "K3", "rf_align", "mine error: '<' not supported between instances of 'NoneType' and 'bool'"},
      {"f90201.xes eva alpha K3 rf_auto", "f90201.xes", f10, "eva", "alpha", "K3", "rf_auto", "mine error: '<' not supported between instances of 'NoneType' and 'bool'"},
      {"f90201.xes eva alpha K3 pr_aaa", "f90201.xes", f10, "eva", "alpha", "K3", "pr_aaa", "mine error: '<' not supported between instances of 'NoneType' and 'bool'"},
      {"f90201.xes fp K3", "f90201.xes", f10, "fp", "", "K3", "", "mine error: '<' not supported between instances of 'NoneType' and 'bool'"},
      {"f90201.xes eva alpha K4 rf_align", "f90201.xes", f10, "eva", "alpha", "K4", "rf_align", "mine error: '<' not supported between instances of 'NoneType' and 'datetime.datetime'"},
      {"f90201.xes eva alpha K4 rf_auto", "f90201.xes", f10, "eva", "alpha", "K4", "rf_auto", "mine error: '<' not supported between instances of 'NoneType' and 'datetime.datetime'"},
      {"f90201.xes eva alpha K4 pr_aaa", "f90201.xes", f10, "eva", "alpha", "K4", "pr_aaa", "mine error: '<' not supported between instances of 'NoneType' and 'datetime.datetime'"},
      {"f90201.xes fp K4", "f90201.xes", f10, "fp", "", "K4", "", "mine error: '<' not supported between instances of 'NoneType' and 'datetime.datetime'"},
      {"f90201.xes eva alpha K5 rf_align", "f90201.xes", f10, "eva", "alpha", "K5", "rf_align", "mine error: '<' not supported between instances of 'NoneType' and 'int'"},
      {"f90201.xes eva alpha K5 rf_auto", "f90201.xes", f10, "eva", "alpha", "K5", "rf_auto", "mine error: '<' not supported between instances of 'NoneType' and 'int'"},
      {"f90201.xes eva alpha K5 pr_aaa", "f90201.xes", f10, "eva", "alpha", "K5", "pr_aaa", "mine error: '<' not supported between instances of 'NoneType' and 'int'"},
      {"f90201.xes msd K5", "f90201.xes", f10, "msd", "", "K5", "", "MSD 0.5=2[0,None] 0=2[10,2.5]"},
      {"f90201.xes fp K5", "f90201.xes", f10, "fp", "", "K5", "", "mine error: '<' not supported between instances of 'NoneType' and 'int'"},
      {"f90201.xes eva alpha K6 rf_align", "f90201.xes", f10, "eva", "alpha", "K6", "rf_align", "mine error: '<' not supported between instances of 'NoneType' and 'str'"},
      {"f90201.xes eva alpha K6 rf_auto", "f90201.xes", f10, "eva", "alpha", "K6", "rf_auto", "mine error: '<' not supported between instances of 'NoneType' and 'str'"},
      {"f90201.xes eva alpha K6 pr_aaa", "f90201.xes", f10, "eva", "alpha", "K6", "pr_aaa", "mine error: '<' not supported between instances of 'NoneType' and 'str'"},
      {"f90201.xes eva hpp K6 rf_align", "f90201.xes", f10, "eva", "hpp", "K6", "rf_align", "mine error: '<' not supported between instances of 'NoneType' and 'str'"},
      {"f90201.xes eva hpp K6 rf_auto", "f90201.xes", f10, "eva", "hpp", "K6", "rf_auto", "mine error: '<' not supported between instances of 'NoneType' and 'str'"},
      {"f90201.xes eva hpp K6 pr_aaa", "f90201.xes", f10, "eva", "hpp", "K6", "pr_aaa", "mine error: '<' not supported between instances of 'NoneType' and 'str'"},
      {"f90201.xes fp K6", "f90201.xes", f10, "fp", "", "K6", "", "mine error: '<' not supported between instances of 'NoneType' and 'str'"},
      {"f90202.xes eva alpha K2 rf_align", "f90202.xes", f11, "eva", "alpha", "K2", "rf_align", "mine error: '<' not supported between instances of 'float' and 'NoneType'"},
      {"f90202.xes eva alpha K2 rf_auto", "f90202.xes", f11, "eva", "alpha", "K2", "rf_auto", "mine error: '<' not supported between instances of 'float' and 'NoneType'"},
      {"f90202.xes eva alpha K2 pr_aaa", "f90202.xes", f11, "eva", "alpha", "K2", "pr_aaa", "mine error: '<' not supported between instances of 'float' and 'NoneType'"},
      {"f90202.xes fp K2", "f90202.xes", f11, "fp", "", "K2", "", "mine error: '<' not supported between instances of 'float' and 'NoneType'"},
      {"f90202.xes lsx K2 next", "f90202.xes", f11, "lsx", "", "K2", "K3", "error: '<' not supported between instances of 'NoneType' and 'bool'"},
      {"f90203.xes lsx K1 next", "f90203.xes", f12, "lsx", "", "K1", "K2", "error: '<' not supported between instances of 'NoneType' and 'float'"},
      {"f90204.xes fp K2", "f90204.xes", f13, "fp", "", "K2", "", "FP seq[-0.0>1.0,7.0>-0.0] par[] st[7.0] en[1.0] act[-0.0,1.0,7.0] dfg[-0.0>1.0=1,7.0>-0.0=1] min 3 | T seq[-0.0>1.0,7.0>-0.0] par[] st[7.0] en[1.0] act[-0.0,1.0,7.0] dfg[-0.0>1.0=1,7.0>-0.0=1] min 3 | M seq[-0.0>1.0,7.0>-0.0] par[] | V[]"},
      {"f90204.xes lsx K2 next", "f90204.xes", f13, "lsx", "", "K2", "K3", "error: '<' not supported between instances of 'NoneType' and 'bool'"},
      {"f90205.xes eva alpha K2 rf_auto", "f90205.xes", f14, "eva", "alpha", "K2", "rf_auto", "F 0.825000/0.835417/0.000000"},
      {"s68500.csv eva alpha K1 rf_align", "s68500.csv", f15, "eva", "alpha", "K1", "rf_align", "mine error: '<' not supported between instances of 'float' and 'str'"},
      {"s68500.csv eva alpha K1 rf_auto", "s68500.csv", f15, "eva", "alpha", "K1", "rf_auto", "mine error: '<' not supported between instances of 'float' and 'str'"},
      {"s68500.csv eva alpha K1 pr_aaa", "s68500.csv", f15, "eva", "alpha", "K1", "pr_aaa", "mine error: '<' not supported between instances of 'float' and 'str'"},
      {"s68500.csv eva hpp K1 rf_align", "s68500.csv", f15, "eva", "hpp", "K1", "rf_align", "mine error: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"s68500.csv eva hpp K1 rf_auto", "s68500.csv", f15, "eva", "hpp", "K1", "rf_auto", "mine error: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"s68500.csv eva hpp K1 pr_aaa", "s68500.csv", f15, "eva", "hpp", "K1", "pr_aaa", "mine error: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"s68500.csv msd K1", "s68500.csv", f15, "msd", "", "K1", "", "MSD r=0[]"},
      {"s68500.csv fp K1", "s68500.csv", f15, "fp", "", "K1", "", "mine error: '<' not supported between instances of 'float' and 'str'"},
      {"s68500.csv lsx K1 cn", "s68500.csv", f15, "lsx", "", "K1", "concept:name", "C [activ_freq:b=0,activ_freq:d=0] 2/2 0.000000 false | [activ_freq:a=0,activ_freq:c=0] 2/2 0.000000 false | [activ_freq:c=0,activ_freq:d=0] 2/2 0.000000 false"},
      {"s68500.csv lsx K1 next", "s68500.csv", f15, "lsx", "", "K1", "K2", "C [activ_freq:1.5=0,activ_freq:3.0=0] 2/2 0.000000 false | [activ_freq:3.0=0] 1/1 0.000000 false | [activ_freq:1.5=0,activ_freq:2.5=0,activ_freq:nan=0] 3/3 0.000000 false"},
      {"s68500.csv lsx K1 same", "s68500.csv", f15, "lsx", "", "K1", "K1", "C [] 0/6 1.000000 true | [] 0/2 1.000000 true | [] 0/11 1.000000 true"},
      {"s68500.csv eva alpha K2 rf_align", "s68500.csv", f15, "eva", "alpha", "K2", "rf_align", "error: can only concatenate str (not \"float\") to str"},
      {"s68500.csv eva alpha K2 rf_auto", "s68500.csv", f15, "eva", "alpha", "K2", "rf_auto", "error: can only concatenate str (not \"float\") to str"},
      {"s68500.csv eva alpha K2 pr_aaa", "s68500.csv", f15, "eva", "alpha", "K2", "pr_aaa", "error: can only concatenate str (not \"float\") to str"},
      {"s68500.csv eva hpp K2 rf_align", "s68500.csv", f15, "eva", "hpp", "K2", "rf_align", "mine error: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"s68500.csv eva hpp K2 rf_auto", "s68500.csv", f15, "eva", "hpp", "K2", "rf_auto", "mine error: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"s68500.csv eva hpp K2 pr_aaa", "s68500.csv", f15, "eva", "hpp", "K2", "pr_aaa", "mine error: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"s68500.csv msd K2", "s68500.csv", f15, "msd", "", "K2", "", "MSD 3.0=0[]"},
      {"s68500.csv fp K2", "s68500.csv", f15, "fp", "", "K2", "", "FP seq[1.5>3.0,1.5>nan,2.5>1.5] par[3.0>3.0] st[1.5,2.5,3.0] en[3.0,nan] act[1.5,2.5,3.0,nan] dfg[1.5>3.0=1,1.5>nan=1,2.5>1.5=1,3.0>3.0=1] min 2 | T seq[1.5>3.0] par[] st[1.5] en[3.0] act[1.5,3.0] dfg[1.5>3.0=1] min 2 / seq[] par[3.0>3.0] st[3.0] en[3.0] act[3.0] dfg[3.0>3.0=1] min 2 / seq[1.5>nan,2.5>1.5] par[] st[2.5] en[nan] act[1.5,2.5,nan] dfg[1.5>nan=1,2.5>1.5=1] min 3 | M seq[] par[] | V[1.5>3.0,1.5>nan,2.5>1.5,3.0>3.0]"},
      {"s68500.csv lsx K2 cn", "s68500.csv", f15, "lsx", "", "K2", "concept:name", "C [activ_freq:b=0,activ_freq:d=0] 2/2 0.000000 false | [activ_freq:a=0,activ_freq:c=0] 2/2 0.000000 false | [activ_freq:c=0,activ_freq:d=0] 2/2 0.000000 false"},
      {"s68500.csv lsx K2 next", "s68500.csv", f15, "lsx", "", "K2", "K3", "C [activ_freq:2021-03-01 08:00:00+00:00=0,activ_freq:2021-03-02 09:30:00+00:00=0] 2/2 0.000000 false | [activ_freq:2021-03-01 08:00:00+00:00=0] 1/1 0.000000 false | [activ_freq:NaT=0] 1/1 0.000000 false"},
      {"s68500.csv lsx K2 same", "s68500.csv", f15, "lsx", "", "K2", "K2", "C [] 0/4 1.000000 true | [] 0/3 1.000000 true | [] 0/15 1.000000 true"},
      {"s68500.csv eva alpha K3 rf_align", "s68500.csv", f15, "eva", "alpha", "K3", "rf_align", "error: can only concatenate str (not \"Timestamp\") to str"},
      {"s68500.csv eva alpha K3 rf_auto", "s68500.csv", f15, "eva", "alpha", "K3", "rf_auto", "error: can only concatenate str (not \"Timestamp\") to str"},
      {"s68500.csv eva alpha K3 pr_aaa", "s68500.csv", f15, "eva", "alpha", "K3", "pr_aaa", "error: can only concatenate str (not \"Timestamp\") to str"},
      {"s68500.csv eva hpp K3 rf_align", "s68500.csv", f15, "eva", "hpp", "K3", "rf_align", "mine error: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"s68500.csv eva hpp K3 rf_auto", "s68500.csv", f15, "eva", "hpp", "K3", "rf_auto", "mine error: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"s68500.csv eva hpp K3 pr_aaa", "s68500.csv", f15, "eva", "hpp", "K3", "pr_aaa", "mine error: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"s68500.csv msd K3", "s68500.csv", f15, "msd", "", "K3", "", "MSD 2021-03-01 08:00:00+00:00=0[]"},
      {"s68500.csv lsx K3 cn", "s68500.csv", f15, "lsx", "", "K3", "concept:name", "C [activ_freq:b=0,activ_freq:d=0] 2/2 0.000000 false | [activ_freq:a=0,activ_freq:c=0] 2/2 0.000000 false | [activ_freq:c=0,activ_freq:d=0] 2/2 0.000000 false"},
      {"s68500.csv lsx K3 next", "s68500.csv", f15, "lsx", "", "K3", "K4", "C [activ_freq:2.0=0,activ_freq:nan=0] 2/2 0.000000 false | [activ_freq:3.0=0] 1/1 0.000000 false | [activ_freq:1.0=0,activ_freq:2.0=0,activ_freq:nan=0] 3/3 0.000000 false"},
      {"s68500.csv lsx K3 same", "s68500.csv", f15, "lsx", "", "K3", "K3", "C [] 0/6 1.000000 true | [] 0/2 1.000000 true | [] 0/4 1.000000 true"},
      {"s68500.csv eva alpha K4 rf_align", "s68500.csv", f15, "eva", "alpha", "K4", "rf_align", "error: can only concatenate str (not \"float\") to str"},
      {"s68500.csv eva alpha K4 rf_auto", "s68500.csv", f15, "eva", "alpha", "K4", "rf_auto", "error: can only concatenate str (not \"float\") to str"},
      {"s68500.csv eva alpha K4 pr_aaa", "s68500.csv", f15, "eva", "alpha", "K4", "pr_aaa", "error: can only concatenate str (not \"float\") to str"},
      {"s68500.csv eva hpp K4 rf_align", "s68500.csv", f15, "eva", "hpp", "K4", "rf_align", "mine error: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"s68500.csv eva hpp K4 rf_auto", "s68500.csv", f15, "eva", "hpp", "K4", "rf_auto", "mine error: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"s68500.csv eva hpp K4 pr_aaa", "s68500.csv", f15, "eva", "hpp", "K4", "pr_aaa", "mine error: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"s68500.csv msd K4", "s68500.csv", f15, "msd", "", "K4", "", "MSD 3.0=0[]"},
      {"s68500.csv fp K4", "s68500.csv", f15, "fp", "", "K4", "", "FP seq[1.0>2.0,2.0>nan,nan>1.0] par[3.0>3.0] st[2.0,3.0,nan] en[2.0,3.0,nan] act[1.0,2.0,3.0,nan,nan] dfg[1.0>2.0=1,2.0>nan=1,3.0>3.0=1,nan>1.0=1] min 2 | T seq[2.0>nan] par[] st[2.0] en[nan] act[2.0,nan] dfg[2.0>nan=1] min 2 / seq[] par[3.0>3.0] st[3.0] en[3.0] act[3.0] dfg[3.0>3.0=1] min 2 / seq[1.0>2.0,nan>1.0] par[] st[nan] en[2.0] act[1.0,2.0,nan] dfg[1.0>2.0=1,nan>1.0=1] min 3 | M seq[nan>1.0] par[] | V[1.0>2.0,2.0>nan,3.0>3.0]"},
      {"s68500.csv lsx K4 cn", "s68500.csv", f15, "lsx", "", "K4", "concept:name", "C [activ_freq:b=0,activ_freq:d=0] 2/2 0.000000 false | [activ_freq:a=0,activ_freq:c=0] 2/2 0.000000 false | [activ_freq:c=0,activ_freq:d=0] 2/2 0.000000 false"},
      {"s68500.csv lsx K4 same", "s68500.csv", f15, "lsx", "", "K4", "K4", "C [] 0/9 1.000000 true | [] 0/5 1.000000 true | [] 0/19 1.000000 true"},
      {"s68500.csv eva alpha K5 rf_align", "s68500.csv", f15, "eva", "alpha", "K5", "rf_align", "error: can only concatenate str (not \"bool\") to str"},
      {"s68500.csv eva alpha K5 rf_auto", "s68500.csv", f15, "eva", "alpha", "K5", "rf_auto", "error: can only concatenate str (not \"bool\") to str"},
      {"s68500.csv eva alpha K5 pr_aaa", "s68500.csv", f15, "eva", "alpha", "K5", "pr_aaa", "error: can only concatenate str (not \"bool\") to str"},
      {"s68500.csv eva hpp K5 rf_align", "s68500.csv", f15, "eva", "hpp", "K5", "rf_align", "mine error: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"s68500.csv eva hpp K5 rf_auto", "s68500.csv", f15, "eva", "hpp", "K5", "rf_auto", "mine error: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"s68500.csv eva hpp K5 pr_aaa", "s68500.csv", f15, "eva", "hpp", "K5", "pr_aaa", "mine error: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"s68500.csv msd K5", "s68500.csv", f15, "msd", "", "K5", "", "MSD True=0[]"},
      {"s68500.csv fp K5", "s68500.csv", f15, "fp", "", "K5", "", "FP seq[True>False,nan>False] par[True>True] st[True,nan] en[False,True] act[False,True,nan] dfg[True>False=1,True>True=2,nan>False=1] min 2 | T seq[True>False] par[] st[True] en[False] act[False,True] dfg[True>False=1] min 2 / seq[nan>False] par[] st[nan] en[False] act[False,nan] dfg[nan>False=1] min 2 / seq[] par[True>True] st[True] en[True] act[True] dfg[True>True=2] min 3 | M seq[nan>False] par[] | V[True>False,True>True]"},
      {"s68500.csv lsx K5 cn", "s68500.csv", f15, "lsx", "", "K5", "concept:name", "C [activ_freq:b=0,activ_freq:d=0] 2/2 0.000000 false | [activ_freq:a=0,activ_freq:c=0] 2/2 0.000000 false | [activ_freq:c=0,activ_freq:d=0] 2/2 0.000000 false"},
      {"s68500.csv lsx K5 next", "s68500.csv", f15, "lsx", "", "K5", "K6", "C [activ_freq:1=0,activ_freq:b=0] 2/2 0.000000 false | [activ_freq:a=0] 1/1 0.000000 false | [activ_freq:1=0,activ_freq:a=0,activ_freq:b=0] 3/3 0.000000 false"},
      {"s68500.csv lsx K5 same", "s68500.csv", f15, "lsx", "", "K5", "K5", "C [] 0/3 1.000000 true | [] 0/7 1.000000 true | [] 0/2 1.000000 true"},
      {"s68500.csv eva alpha K6 rf_align", "s68500.csv", f15, "eva", "alpha", "K6", "rf_align", "F 0.800000/0.805556/33.333333"},
      {"s68500.csv eva alpha K6 rf_auto", "s68500.csv", f15, "eva", "alpha", "K6", "rf_auto", "F 0.800000/0.805556/33.333333"},
      {"s68500.csv eva alpha K6 pr_aaa", "s68500.csv", f15, "eva", "alpha", "K6", "pr_aaa", "P 1.000000"},
      {"s68500.csv eva hpp K6 rf_align", "s68500.csv", f15, "eva", "hpp", "K6", "rf_align", "mine error: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"s68500.csv eva hpp K6 rf_auto", "s68500.csv", f15, "eva", "hpp", "K6", "rf_auto", "mine error: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"s68500.csv eva hpp K6 pr_aaa", "s68500.csv", f15, "eva", "hpp", "K6", "pr_aaa", "mine error: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"s68500.csv msd K6", "s68500.csv", f15, "msd", "", "K6", "", "MSD a=0[]"},
      {"s68500.csv fp K6", "s68500.csv", f15, "fp", "", "K6", "", "FP seq[a>b,b>1] par[a>a] st[a,b] en[1,a] act[1,a,b] dfg[a>a=1,a>b=1,b>1=2] min 2 | T seq[b>1] par[] st[b] en[1] act[1,b] dfg[b>1=1] min 2 / seq[] par[a>a] st[a] en[a] act[a] dfg[a>a=1] min 2 / seq[a>b,b>1] par[] st[a] en[1] act[1,a,b] dfg[a>b=1,b>1=1] min 3 | M seq[b>1] par[] | V[a>a,a>b]"},
      {"s68500.csv lsx K6 cn", "s68500.csv", f15, "lsx", "", "K6", "concept:name", "C [activ_freq:d=0,always_after:b>1,directly_follows:b>1,equivalence:b>1] 4/5 0.200000 false | [activ_freq:c=0] 1/2 0.500000 false | [activ_freq:c=0,activ_freq:d=0] 2/2 0.000000 false"},
      {"s68500.csv lsx K6 next", "s68500.csv", f15, "lsx", "", "K6", "K1", "error: '<' not supported between instances of 'float' and 'str'"},
      {"s68500.csv lsx K6 same", "s68500.csv", f15, "lsx", "", "K6", "K6", "C [] 0/7 1.000000 true | [] 0/1 1.000000 true | [] 0/8 1.000000 true"},
      {"s68501.csv msd K1", "s68501.csv", f16, "msd", "", "K1", "", "MSD "},
      {"s68501.csv lsx K1 cn", "s68501.csv", f16, "lsx", "", "K1", "concept:name", "error: '<' not supported between instances of 'float' and 'str'"},
      {"s68501.csv lsx K1 next", "s68501.csv", f16, "lsx", "", "K1", "K2", "error: '<' not supported between instances of 'str' and 'float'"},
      {"s68501.csv eva alpha K2 rf_align", "s68501.csv", f16, "eva", "alpha", "K2", "rf_align", "error: can only concatenate str (not \"float\") to str"},
      {"s68501.csv eva alpha K2 rf_auto", "s68501.csv", f16, "eva", "alpha", "K2", "rf_auto", "error: can only concatenate str (not \"float\") to str"},
      {"s68501.csv eva alpha K2 pr_aaa", "s68501.csv", f16, "eva", "alpha", "K2", "pr_aaa", "error: can only concatenate str (not \"float\") to str"},
      {"s68501.csv msd K2", "s68501.csv", f16, "msd", "", "K2", "", "MSD "},
      {"s68501.csv fp K2", "s68501.csv", f16, "fp", "", "K2", "", "FP seq[2.5>1.5,3.0>2.5,nan>3.0] par[] st[nan] en[1.5] act[1.5,2.5,3.0,nan] dfg[2.5>1.5=1,3.0>2.5=1,nan>3.0=1] min 4 | T seq[2.5>1.5,3.0>2.5,nan>3.0] par[] st[nan] en[1.5] act[1.5,2.5,3.0,nan] dfg[2.5>1.5=1,3.0>2.5=1,nan>3.0=1] min 4 | M seq[2.5>1.5,3.0>2.5,nan>3.0] par[] | V[]"},
      {"s68501.csv lsx K2 cn", "s68501.csv", f16, "lsx", "", "K2", "concept:name", "error: '<' not supported between instances of 'float' and 'str'"},
      {"s68501.csv lsx K2 next", "s68501.csv", f16, "lsx", "", "K2", "K3", "error: '<' not supported between instances of 'float' and 'Timestamp'"},
      {"s68501.csv eva alpha K3 rf_align", "s68501.csv", f16, "eva", "alpha", "K3", "rf_align", "error: can only concatenate str (not \"Timestamp\") to str"},
      {"s68501.csv eva alpha K3 rf_auto", "s68501.csv", f16, "eva", "alpha", "K3", "rf_auto", "error: can only concatenate str (not \"Timestamp\") to str"},
      {"s68501.csv eva alpha K3 pr_aaa", "s68501.csv", f16, "eva", "alpha", "K3", "pr_aaa", "error: can only concatenate str (not \"Timestamp\") to str"},
      {"s68501.csv msd K3", "s68501.csv", f16, "msd", "", "K3", "", "MSD 2021-03-01 08:00:00+00:00=0[]"},
      {"s68501.csv lsx K3 cn", "s68501.csv", f16, "lsx", "", "K3", "concept:name", "error: '<' not supported between instances of 'Timestamp' and 'str'"},
      {"s68501.csv lsx K3 next", "s68501.csv", f16, "lsx", "", "K3", "K4", "error: '<' not supported between instances of 'Timestamp' and 'int'"},
      {"s68501.csv eva alpha K4 rf_auto", "s68501.csv", f16, "eva", "alpha", "K4", "rf_auto", "F 0.714286/0.714286/0.000000"},
      {"s68501.csv msd K4", "s68501.csv", f16, "msd", "", "K4", "", "MSD 1=2[2,3]"},
      {"s68501.csv fp K4", "s68501.csv", f16, "fp", "", "K4", "", "FP seq[1>2,2>3,3>1] par[] st[1] en[1] act[1,2,3] dfg[1>2=1,2>3=1,3>1=1] min 4 | T seq[1>2,2>3,3>1] par[] st[1] en[1] act[1,2,3] dfg[1>2=1,2>3=1,3>1=1] min 4 | M seq[] par[] | V[1>2,2>3,3>1]"},
      {"s68501.csv lsx K4 cn", "s68501.csv", f16, "lsx", "", "K4", "concept:name", "error: '<' not supported between instances of 'int' and 'str'"},
      {"s68501.csv lsx K4 next", "s68501.csv", f16, "lsx", "", "K4", "K5", "C [activ_freq:2=0,activ_freq:3=0,activ_freq:False=0] 3/4 0.250000 false"},
      {"s68501.csv eva alpha K5 rf_align", "s68501.csv", f16, "eva", "alpha", "K5", "rf_align", "error: can only concatenate str (not \"bool\") to str"},
      {"s68501.csv eva alpha K5 rf_auto", "s68501.csv", f16, "eva", "alpha", "K5", "rf_auto", "error: can only concatenate str (not \"bool\") to str"},
      {"s68501.csv eva alpha K5 pr_aaa", "s68501.csv", f16, "eva", "alpha", "K5", "pr_aaa", "error: can only concatenate str (not \"bool\") to str"},
      {"s68501.csv msd K5", "s68501.csv", f16, "msd", "", "K5", "", "MSD False=0[] True=0[]"},
      {"s68501.csv lsx K5 cn", "s68501.csv", f16, "lsx", "", "K5", "concept:name", "error: '<' not supported between instances of 'bool' and 'str'"},
      {"s68501.csv lsx K5 next", "s68501.csv", f16, "lsx", "", "K5", "K6", "error: '<' not supported between instances of 'bool' and 'str'"},
      {"s68501.csv lsx K6 next", "s68501.csv", f16, "lsx", "", "K6", "K1", "error: '<' not supported between instances of 'float' and 'str'"},
      {"s68502.csv eva alpha K1 rf_align", "s68502.csv", f17, "eva", "alpha", "K1", "rf_align", "error: can only concatenate str (not \"float\") to str"},
      {"s68502.csv eva alpha K1 rf_auto", "s68502.csv", f17, "eva", "alpha", "K1", "rf_auto", "error: can only concatenate str (not \"float\") to str"},
      {"s68502.csv eva alpha K1 pr_aaa", "s68502.csv", f17, "eva", "alpha", "K1", "pr_aaa", "error: can only concatenate str (not \"float\") to str"},
      {"s68502.csv msd K1", "s68502.csv", f17, "msd", "", "K1", "", "MSD "},
      {"s68502.csv fp K1", "s68502.csv", f17, "fp", "", "K1", "", "FP seq[] par[] st[nan] en[nan] act[nan] dfg[] min 1 | T seq[] par[] st[nan] en[nan] act[nan] dfg[] min 1 | M seq[] par[] | V[]"},
      {"s68502.csv lsx K1 cn", "s68502.csv", f17, "lsx", "", "K1", "concept:name", "error: '<' not supported between instances of 'float' and 'str'"},
      {"s68502.csv lsx K1 next", "s68502.csv", f17, "lsx", "", "K1", "K2", "C [activ_freq:2.5=0,activ_freq:nan=0] 2/2 0.000000 false"},
      {"s68502.csv eva alpha K2 rf_align", "s68502.csv", f17, "eva", "alpha", "K2", "rf_align", "error: can only concatenate str (not \"float\") to str"},
      {"s68502.csv eva alpha K2 rf_auto", "s68502.csv", f17, "eva", "alpha", "K2", "rf_auto", "error: can only concatenate str (not \"float\") to str"},
      {"s68502.csv eva alpha K2 pr_aaa", "s68502.csv", f17, "eva", "alpha", "K2", "pr_aaa", "error: can only concatenate str (not \"float\") to str"},
      {"s68502.csv msd K2", "s68502.csv", f17, "msd", "", "K2", "", "MSD "},
      {"s68502.csv fp K2", "s68502.csv", f17, "fp", "", "K2", "", "FP seq[] par[] st[2.5] en[2.5] act[2.5] dfg[] min 1 | T seq[] par[] st[2.5] en[2.5] act[2.5] dfg[] min 1 | M seq[] par[] | V[]"},
      {"s68502.csv lsx K2 cn", "s68502.csv", f17, "lsx", "", "K2", "concept:name", "error: '<' not supported between instances of 'float' and 'str'"},
      {"s68502.csv lsx K2 next", "s68502.csv", f17, "lsx", "", "K2", "K3", "C [activ_freq:2.5=0,activ_freq:nan=0] 2/2 0.000000 false"},
      {"s68502.csv eva alpha K3 rf_align", "s68502.csv", f17, "eva", "alpha", "K3", "rf_align", "error: can only concatenate str (not \"float\") to str"},
      {"s68502.csv eva alpha K3 rf_auto", "s68502.csv", f17, "eva", "alpha", "K3", "rf_auto", "error: can only concatenate str (not \"float\") to str"},
      {"s68502.csv eva alpha K3 pr_aaa", "s68502.csv", f17, "eva", "alpha", "K3", "pr_aaa", "error: can only concatenate str (not \"float\") to str"},
      {"s68502.csv msd K3", "s68502.csv", f17, "msd", "", "K3", "", "MSD "},
      {"s68502.csv fp K3", "s68502.csv", f17, "fp", "", "K3", "", "FP seq[] par[] st[nan] en[nan] act[nan] dfg[] min 1 | T seq[] par[] st[nan] en[nan] act[nan] dfg[] min 1 | M seq[] par[] | V[]"},
      {"s68502.csv lsx K3 cn", "s68502.csv", f17, "lsx", "", "K3", "concept:name", "error: '<' not supported between instances of 'float' and 'str'"},
      {"s68502.csv lsx K3 next", "s68502.csv", f17, "lsx", "", "K3", "K4", "C [activ_freq:3=0,activ_freq:nan=0] 2/2 0.000000 false"},
      {"s68502.csv eva alpha K4 rf_align", "s68502.csv", f17, "eva", "alpha", "K4", "rf_align", "error: can only concatenate str (not \"int\") to str"},
      {"s68502.csv eva alpha K4 rf_auto", "s68502.csv", f17, "eva", "alpha", "K4", "rf_auto", "error: can only concatenate str (not \"int\") to str"},
      {"s68502.csv eva alpha K4 pr_aaa", "s68502.csv", f17, "eva", "alpha", "K4", "pr_aaa", "error: can only concatenate str (not \"int\") to str"},
      {"s68502.csv lsx K4 cn", "s68502.csv", f17, "lsx", "", "K4", "concept:name", "error: '<' not supported between instances of 'int' and 'str'"},
      {"s68502.csv lsx K4 next", "s68502.csv", f17, "lsx", "", "K4", "K5", "C [activ_freq:3=0,activ_freq:True=0] 2/2 0.000000 false"},
      {"s68502.csv eva alpha K5 rf_align", "s68502.csv", f17, "eva", "alpha", "K5", "rf_align", "error: can only concatenate str (not \"bool\") to str"},
      {"s68502.csv eva alpha K5 rf_auto", "s68502.csv", f17, "eva", "alpha", "K5", "rf_auto", "error: can only concatenate str (not \"bool\") to str"},
      {"s68502.csv eva alpha K5 pr_aaa", "s68502.csv", f17, "eva", "alpha", "K5", "pr_aaa", "error: can only concatenate str (not \"bool\") to str"},
      {"s68502.csv fp K5", "s68502.csv", f17, "fp", "", "K5", "", "FP seq[] par[] st[True] en[True] act[True] dfg[] min 1 | T seq[] par[] st[True] en[True] act[True] dfg[] min 1 | M seq[] par[] | V[]"},
      {"s68502.csv lsx K5 cn", "s68502.csv", f17, "lsx", "", "K5", "concept:name", "error: '<' not supported between instances of 'bool' and 'str'"},
      {"s68502.csv lsx K5 next", "s68502.csv", f17, "lsx", "", "K5", "K6", "C [activ_freq:True=0,activ_freq:nan=0] 2/2 0.000000 false"},
      {"s68502.csv eva alpha K6 rf_align", "s68502.csv", f17, "eva", "alpha", "K6", "rf_align", "error: can only concatenate str (not \"float\") to str"},
      {"s68502.csv eva alpha K6 rf_auto", "s68502.csv", f17, "eva", "alpha", "K6", "rf_auto", "error: can only concatenate str (not \"float\") to str"},
      {"s68502.csv eva alpha K6 pr_aaa", "s68502.csv", f17, "eva", "alpha", "K6", "pr_aaa", "error: can only concatenate str (not \"float\") to str"},
      {"s68502.csv msd K6", "s68502.csv", f17, "msd", "", "K6", "", "MSD "},
      {"s68502.csv fp K6", "s68502.csv", f17, "fp", "", "K6", "", "FP seq[] par[] st[nan] en[nan] act[nan] dfg[] min 1 | T seq[] par[] st[nan] en[nan] act[nan] dfg[] min 1 | M seq[] par[] | V[]"},
      {"s68502.csv lsx K6 cn", "s68502.csv", f17, "lsx", "", "K6", "concept:name", "error: '<' not supported between instances of 'float' and 'str'"},
      {"s68502.csv lsx K6 next", "s68502.csv", f17, "lsx", "", "K6", "K1", "C [activ_freq:nan=0,activ_freq:nan=0] 2/2 0.000000 false"},
      {"s68503.csv lsx K1 cn", "s68503.csv", f18, "lsx", "", "K1", "concept:name", "error: '<' not supported between instances of 'float' and 'str'"},
      {"s68503.csv lsx K1 next", "s68503.csv", f18, "lsx", "", "K1", "K2", "error: '<' not supported between instances of 'str' and 'float'"},
      {"s68503.csv lsx K2 cn", "s68503.csv", f18, "lsx", "", "K2", "concept:name", "error: '<' not supported between instances of 'float' and 'str'"},
      {"s68503.csv lsx K2 next", "s68503.csv", f18, "lsx", "", "K2", "K3", "error: '<' not supported between instances of 'float' and 'NaTType'"},
      {"s68503.csv lsx K3 cn", "s68503.csv", f18, "lsx", "", "K3", "concept:name", "error: '<' not supported between instances of 'Timestamp' and 'str'"},
      {"s68503.csv lsx K3 next", "s68503.csv", f18, "lsx", "", "K3", "K4", "error: '<' not supported between instances of 'Timestamp' and 'float'"},
      {"s68503.csv eva alpha K4 rf_align", "s68503.csv", f18, "eva", "alpha", "K4", "rf_align", "error: can only concatenate str (not \"float\") to str"},
      {"s68503.csv eva alpha K4 rf_auto", "s68503.csv", f18, "eva", "alpha", "K4", "rf_auto", "error: can only concatenate str (not \"float\") to str"},
      {"s68503.csv eva alpha K4 pr_aaa", "s68503.csv", f18, "eva", "alpha", "K4", "pr_aaa", "error: can only concatenate str (not \"float\") to str"},
      {"s68503.csv msd K4", "s68503.csv", f18, "msd", "", "K4", "", "MSD "},
      {"s68503.csv fp K4", "s68503.csv", f18, "fp", "", "K4", "", "FP seq[1.0>nan,nan>1.0] par[] st[nan] en[nan] act[1.0,nan,nan] dfg[1.0>nan=1,nan>1.0=1] min 3 | T seq[1.0>nan,nan>1.0] par[] st[nan] en[nan] act[1.0,nan,nan] dfg[1.0>nan=1,nan>1.0=1] min 3 | M seq[1.0>nan,nan>1.0] par[] | V[]"},
      {"s68503.csv lsx K4 cn", "s68503.csv", f18, "lsx", "", "K4", "concept:name", "error: '<' not supported between instances of 'float' and 'str'"},
      {"s68503.csv lsx K5 cn", "s68503.csv", f18, "lsx", "", "K5", "concept:name", "error: '<' not supported between instances of 'bool' and 'str'"},
      {"s68503.csv lsx K5 next", "s68503.csv", f18, "lsx", "", "K5", "K6", "error: '<' not supported between instances of 'bool' and 'str'"},
      {"s68503.csv lsx K6 next", "s68503.csv", f18, "lsx", "", "K6", "K1", "error: '<' not supported between instances of 'float' and 'str'"},
      {"s68504.csv lsx K1 next", "s68504.csv", f19, "lsx", "", "K1", "K2", "C [activ_freq:-1.0=0,activ_freq:nan=0,activ_freq:nan=0,activ_freq:nan=0] 4/4 0.000000 false | [activ_freq:2.5=0,activ_freq:nan=0,activ_freq:nan=0] 3/3 0.000000 false | [activ_freq:2.5=0,activ_freq:nan=0] 2/2 0.000000 false | [activ_freq:1.5=0,activ_freq:nan=0,activ_freq:nan=0,activ_freq:nan=0] 4/4 0.000000 false"},
      {"s68504.csv msd K2", "s68504.csv", f19, "msd", "", "K2", "", "MSD -1.0=0[] 1.5=0[]"},
      {"s68504.csv lsx K2 next", "s68504.csv", f19, "lsx", "", "K2", "K3", "C [activ_freq:2021-03-02 09:30:00+00:00=0,activ_freq:2021-03-03 10:00:00+00:00=0] 2/2 0.000000 false | [activ_freq:2021-03-02 09:30:00+00:00=0,activ_freq:NaT=0] 2/2 0.000000 false | [activ_freq:2021-03-03 10:00:00+00:00=0] 1/1 0.000000 false | [activ_freq:2021-03-02 09:30:00+00:00=0,activ_freq:2021-03-03 10:00:00+00:00=0,activ_freq:NaT=0] 3/3 0.000000 false"},
      {"s68504.csv fp K3", "s68504.csv", f19, "fp", "", "K3", "", "FP seq[2021-03-02 09:30:00+00:00>NaT,NaT>2021-03-03 10:00:00+00:00] par[2021-03-02 09:30:00+00:00>2021-03-03 10:00:00+00:00,2021-03-03 10:00:00+00:00>2021-03-02 09:30:00+00:00,NaT>NaT] st[2021-03-02 09:30:00+00:00,2021-03-03 10:00:00+00:00,NaT] en[2021-03-02 09:30:00+00:00,2021-03-03 10:00:00+00:00,NaT] act[2021-03-02 09:30:00+00:00,2021-03-03 10:00:00+00:00,NaT] dfg[2021-03-02 09:30:00+00:00>2021-03-03 10:00:00+00:00=1,2021-03-02 09:30:00+00:00>NaT=1,2021-03-03 10:00:00+00:00>2021-03-02 09:30:00+00:00=3,NaT>2021-03-03 10:00:00+00:00=1,NaT>NaT=1] min 1 | T seq[] par[2021-03-02 09:30:00+00:00>2021-03-03 10:00:00+00:00,2021-03-03 10:00:00+00:00>2021-03-02 09:30:00+00:00] st[2021-03-03 10:00:00+00:00] en[2021-03-02 09:30:00+00:00] act[2021-03-02 09:30:00+00:00,2021-03-03 10:00:00+00:00] dfg[2021-03-02 09:30:00+00:00>2021-03-03 10:00:00+00:00=1,2021-03-03 10:00:00+00:00>2021-03-02 09:30:00+00:00=2] min 4 / seq[2021-03-02 09:30:00+00:00>NaT] par[] st[2021-03-02 09:30:00+00:00] en[NaT] act[2021-03-02 09:30:00+00:00,NaT] dfg[2021-03-02 09:30:00+00:00>NaT=1] min 2 / seq[] par[] st[2021-03-03 10:00:00+00:00] en[2021-03-03 10:00:00+00:00] act[2021-03-03 10:00:00+00:00] dfg[] min 1 / seq[2021-03-03 10:00:00+00:00>2021-03-02 09:30:00+00:00,NaT>2021-03-03 10:00:00+00:00] par[NaT>NaT] st[NaT] en[2021-03-02 09:30:00+00:00] act[2021-03-02 09:30:00+00:00,2021-03-03 10:00:00+00:00,NaT] dfg[2021-03-03 10:00:00+00:00>2021-03-02 09:30:00+00:00=1,NaT>2021-03-03 10:00:00+00:00=1,NaT>NaT=1] min 4 | M seq[] par[] | V[2021-03-02 09:30:00+00:00>2021-03-03 10:00:00+00:00,2021-03-02 09:30:00+00:00>NaT,2021-03-03 10:00:00+00:00>2021-03-02 09:30:00+00:00,NaT>2021-03-03 10:00:00+00:00,NaT>NaT]"},
      {"s68504.csv lsx K3 next", "s68504.csv", f19, "lsx", "", "K3", "K4", "C [activ_freq:1.0=0,activ_freq:7.0=0,activ_freq:nan=0,activ_freq:nan=0] 4/4 0.000000 false | [activ_freq:3.0=0,activ_freq:nan=0] 2/2 0.000000 false | [activ_freq:7.0=0] 1/1 0.000000 false | [activ_freq:7.0=0,activ_freq:nan=0,activ_freq:nan=0,activ_freq:nan=0] 4/4 0.000000 false"},
      {"s68504.csv msd K4", "s68504.csv", f19, "msd", "", "K4", "", "MSD "},
      {"s68504.csv fp K4", "s68504.csv", f19, "fp", "", "K4", "", "FP seq[1.0>7.0,3.0>nan,nan>1.0,nan>7.0,nan>nan,nan>nan,nan>nan] par[] st[3.0,7.0,nan,nan] en[7.0,nan] act[1.0,3.0,7.0,nan,nan,nan,nan,nan,nan] dfg[1.0>7.0=1,3.0>nan=1,nan>1.0=1,nan>7.0=1,nan>nan=1,nan>nan=1,nan>nan=1] min 1 | T seq[1.0>7.0,nan>1.0,nan>nan] par[] st[nan] en[7.0] act[1.0,7.0,nan,nan] dfg[1.0>7.0=1,nan>1.0=1,nan>nan=1] min 4 / seq[3.0>nan] par[] st[3.0] en[nan] act[3.0,nan] dfg[3.0>nan=1] min 2 / seq[] par[] st[7.0] en[7.0] act[7.0] dfg[] min 1 / seq[nan>7.0,nan>nan,nan>nan] par[] st[nan] en[7.0] act[7.0,nan,nan,nan] dfg[nan>7.0=1,nan>nan=1,nan>nan=1] min 4 | M seq[3.0>nan,nan>1.0,nan>nan,nan>nan,nan>nan] par[] | V[1.0>7.0,nan>7.0]"},
      {"s68504.csv lsx K4 next", "s68504.csv", f19, "lsx", "", "K4", "K5", "C [activ_freq:False=0,always_after:1.0>7.0,always_before:1.0>nan,always_before:1.0>nan,directly_follows:1.0>7.0,equivalence:1.0>7.0,equivalence:1.0>nan,equivalence:1.0>nan] 8/14 0.428571 false | [activ_freq:False=0,always_after:1.0>7.0,always_before:1.0>nan,always_before:1.0>nan,directly_follows:1.0>7.0,equivalence:1.0>7.0,equivalence:1.0>nan,equivalence:1.0>nan] 8/14 0.428571 false | [activ_freq:False=0] 1/1 0.000000 false | [activ_freq:False=0,activ_freq:nan=0] 2/2 0.000000 false"},
      {"s68504.csv msd K5", "s68504.csv", f19, "msd", "", "K5", "", "MSD False=0[]"},
      {"s68504.csv fp K5", "s68504.csv", f19, "fp", "", "K5", "", "FP seq[nan>False] par[False>False,False>True,True>False,nan>nan] st[False,True,nan] en[False] act[False,True,nan] dfg[False>False=2,False>True=1,True>False=2,nan>False=1,nan>nan=1] min 1 | T seq[] par[False>False,False>True,True>False] st[False] en[False] act[False,True] dfg[False>False=1,False>True=1,True>False=1] min 4 / seq[True>False] par[] st[True] en[False] act[False,True] dfg[True>False=1] min 2 / seq[] par[] st[False] en[False] act[False] dfg[] min 1 / seq[nan>False] par[False>False,nan>nan] st[nan] en[False] act[False,nan] dfg[False>False=1,nan>False=1,nan>nan=1] min 4 | M seq[] par[] | V[False>False,False>True,True>False,nan>False,nan>nan]"},
      {"s68504.csv lsx K5 next", "s68504.csv", f19, "lsx", "", "K5", "K6", "error: '<' not supported between instances of 'bool' and 'str'"},
      {"s68504.csv msd K6", "s68504.csv", f19, "msd", "", "K6", "", "MSD "},
      {"s68505.csv lsx K1 next", "s68505.csv", f20, "lsx", "", "K1", "K2", "C [activ_freq:1.5=0] 1/1 0.000000 false | [activ_freq:3.0=0] 1/1 0.000000 false | [activ_freq:1.5=0,activ_freq:3.0=0,activ_freq:nan=0] 3/3 0.000000 false | [activ_freq:1.5=0,activ_freq:nan=0] 2/2 0.000000 false | [activ_freq:1.5=0,activ_freq:2.5=0,activ_freq:nan=0] 3/3 0.000000 false"},
      {"s68505.csv eva hpp K3 rf_align", "s68505.csv", f20, "eva", "hpp", "K3", "rf_align", "error: can only concatenate str (not \"NaTType\") to str"},
      {"s68505.csv eva hpp K3 rf_auto", "s68505.csv", f20, "eva", "hpp", "K3", "rf_auto", "error: can only concatenate str (not \"NaTType\") to str"},
      {"s68505.csv eva hpp K3 pr_aaa", "s68505.csv", f20, "eva", "hpp", "K3", "pr_aaa", "error: can only concatenate str (not \"NaTType\") to str"},
      {"s68505.csv lsx K3 next", "s68505.csv", f20, "lsx", "", "K3", "K4", "C [activ_freq:1.0=0,activ_freq:nan=0] 2/2 0.000000 false | [activ_freq:2.0=0] 1/1 0.000000 false | [activ_freq:2.0=0,activ_freq:3.0=0,activ_freq:nan=0] 3/3 0.000000 false | [activ_freq:1.0=0,activ_freq:2.0=0] 2/2 0.000000 false | [activ_freq:1.0=0,activ_freq:3.0=0] 2/2 0.000000 false"},
      {"s68505.csv lsx K4 next", "s68505.csv", f20, "lsx", "", "K4", "K5", "C [activ_freq:nan=0] 1/3 0.666667 false | [activ_freq:False=0] 1/1 0.000000 false | [activ_freq:False=0,activ_freq:nan=0] 2/4 0.500000 false | [activ_freq:False=0] 1/3 0.666667 false | [activ_freq:False=0,activ_freq:nan=0] 2/4 0.500000 false"},
      {"s68505.csv eva alpha K5 rf_auto", "s68505.csv", f20, "eva", "alpha", "K5", "rf_auto", "F 0.769737/0.778333/0.000000"},
      {"s68505.csv msd K5", "s68505.csv", f20, "msd", "", "K5", "", "MSD "},
      {"s68505.csv fp K5", "s68505.csv", f20, "fp", "", "K5", "", "FP seq[False>nan,True>nan] par[False>True,True>False] st[False,True] en[False,nan] act[False,True,nan] dfg[False>True=1,False>nan=1,True>False=2,True>nan=2] min 1 | T seq[True>nan] par[] st[True] en[nan] act[True,nan] dfg[True>nan=1] min 2 / seq[] par[] st[False] en[False] act[False] dfg[] min 1 / seq[False>nan,True>False] par[] st[True] en[nan] act[False,True,nan] dfg[False>nan=1,True>False=1] min 3 / seq[True>False] par[] st[True] en[False] act[False,True] dfg[True>False=1] min 2 / seq[False>True,True>nan] par[] st[False] en[nan] act[False,True,nan] dfg[False>True=1,True>nan=1] min 3 | M seq[] par[] | V[False>True,False>nan,True>False,True>nan]"},
      {"s68505.csv lsx K5 next", "s68505.csv", f20, "lsx", "", "K5", "K6", "C [activ_freq:1=0,activ_freq:b=0] 2/2 0.000000 false | [always_before:nan>True,equivalence:nan>True] 2/4 0.500000 false | [activ_freq:1=0] 1/1 0.000000 false | [activ_freq:b=0] 1/1 0.000000 false | [activ_freq:1=0,always_before:nan>True,equivalence:nan>True] 3/5 0.400000 false"},
      {"s68505.csv msd K6", "s68505.csv", f20, "msd", "", "K6", "", "MSD 1=0[] b=0[]"},
      {"s68505.csv lsx K6 next", "s68505.csv", f20, "lsx", "", "K6", "K1", "C [activ_freq:p=0,activ_freq:r=0] 2/2 0.000000 false | [activ_freq:p=0] 1/1 0.000000 false | [activ_freq:r=0] 1/1 0.000000 false | [activ_freq:r=0] 1/1 0.000000 false | [activ_freq:q=0] 1/4 0.750000 false"},
      {"s68506.csv fp K2", "s68506.csv", f21, "fp", "", "K2", "", "FP seq[] par[2.5>nan,nan>2.5] st[2.5,nan] en[2.5,nan] act[2.5,nan,nan] dfg[2.5>nan=1,nan>2.5=1] min 1 | T seq[] par[2.5>nan,nan>2.5] st[2.5] en[2.5] act[2.5,nan] dfg[2.5>nan=1,nan>2.5=1] min 3 / seq[] par[] st[nan] en[nan] act[nan] dfg[] min 1 | M seq[] par[2.5>nan,nan>2.5,nan>nan,nan>nan,nan>nan] | V[]"},
      {"s68506.csv lsx K3 next", "s68506.csv", f21, "lsx", "", "K3", "K4", "error: '<' not supported between instances of 'NaTType' and 'int'"},
      {"s68506.csv lsx K6 next", "s68506.csv", f21, "lsx", "", "K6", "K1", "C [activ_freq:p=0] 1/5 0.800000 false | [] 0/4 1.000000 true"},
      {"s68507.csv lsx K2 next", "s68507.csv", f22, "lsx", "", "K2", "K3", "error: '<' not supported between instances of 'float' and 'Timestamp'"},
      {"s68507.csv fp K3", "s68507.csv", f22, "fp", "", "K3", "", "FP seq[] par[2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00,2021-03-01 08:00:00+00:00>NaT,NaT>2021-03-01 08:00:00+00:00] st[2021-03-01 08:00:00+00:00,NaT] en[2021-03-01 08:00:00+00:00] act[2021-03-01 08:00:00+00:00,NaT] dfg[2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00=2,2021-03-01 08:00:00+00:00>NaT=1,NaT>2021-03-01 08:00:00+00:00=2] min 2 | T seq[] par[2021-03-01 08:00:00+00:00>NaT,NaT>2021-03-01 08:00:00+00:00] st[NaT] en[2021-03-01 08:00:00+00:00] act[2021-03-01 08:00:00+00:00,NaT] dfg[2021-03-01 08:00:00+00:00>NaT=1,NaT>2021-03-01 08:00:00+00:00=2] min 4 / seq[] par[2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00] st[2021-03-01 08:00:00+00:00] en[2021-03-01 08:00:00+00:00] act[2021-03-01 08:00:00+00:00] dfg[2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00=1] min 2 / seq[] par[2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00] st[2021-03-01 08:00:00+00:00] en[2021-03-01 08:00:00+00:00] act[2021-03-01 08:00:00+00:00] dfg[2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00=1] min 2 | M seq[] par[] | V[2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00,2021-03-01 08:00:00+00:00>NaT,NaT>2021-03-01 08:00:00+00:00]"},
      {"s68508.csv eva alpha K1 rf_align", "s68508.csv", f23, "eva", "alpha", "K1", "rf_align", "F 0.555556/0.550000/0.000000"},
      {"s68508.csv eva alpha K1 rf_auto", "s68508.csv", f23, "eva", "alpha", "K1", "rf_auto", "F 0.555556/0.550000/0.000000"},
      {"s68508.csv eva alpha K1 pr_aaa", "s68508.csv", f23, "eva", "alpha", "K1", "pr_aaa", "P 0.714286"},
      {"s68508.csv msd K1", "s68508.csv", f23, "msd", "", "K1", "", "MSD p=1[q] q=1[p] r=1[q]"},
      {"s68508.csv lsx K1 next", "s68508.csv", f23, "lsx", "", "K1", "K2", "error: '<' not supported between instances of 'str' and 'float'"},
      {"s68508.csv eva alpha K2 rf_auto", "s68508.csv", f23, "eva", "alpha", "K2", "rf_auto", "F 0.633333/0.650000/0.000000"},
      {"s68508.csv eva alpha K3 rf_auto", "s68508.csv", f23, "eva", "alpha", "K3", "rf_auto", "F 0.584416/0.579167/0.000000"},
      {"s68508.csv lsx K6 cn", "s68508.csv", f23, "lsx", "", "K6", "concept:name", "error: '<' not supported between instances of 'float' and 'str'"},
      {"s68509.csv fp K1", "s68509.csv", f24, "fp", "", "K1", "", "FP seq[] par[q>q] st[q] en[q] act[q] dfg[q>q=1] min 1 | T seq[] par[q>q] st[q] en[q] act[q] dfg[q>q=1] min 2 / seq[] par[] st[q] en[q] act[q] dfg[] min 1 | M seq[] par[] | V[q>q]"},
      {"s68509.csv lsx K6 next", "s68509.csv", f24, "lsx", "", "K6", "K1", "C [activ_freq:q=0] 1/1 0.000000 false | [activ_freq:q=0] 1/1 0.000000 false"},
      {"s68511.csv lsx K6 cn", "s68511.csv", f25, "lsx", "", "K6", "concept:name", "error: '<' not supported between instances of 'float' and 'str'"},
      {"s68513.csv msd K3", "s68513.csv", f26, "msd", "", "K3", "", "MSD "},
      {"s68513.csv lsx K5 next", "s68513.csv", f26, "lsx", "", "K5", "K6", "C [] 0/2 1.000000 true | [] 0/2 1.000000 true"},
      {"s68513.csv eva alpha K6 rf_align", "s68513.csv", f26, "eva", "alpha", "K6", "rf_align", "error: can only concatenate str (not \"int\") to str"},
      {"s68513.csv eva alpha K6 rf_auto", "s68513.csv", f26, "eva", "alpha", "K6", "rf_auto", "error: can only concatenate str (not \"int\") to str"},
      {"s68513.csv eva alpha K6 pr_aaa", "s68513.csv", f26, "eva", "alpha", "K6", "pr_aaa", "error: can only concatenate str (not \"int\") to str"},
  };
  for (const auto& c : cases) {
    const Server target{client, c.name, c.content};
    check(c.label, outcome(target, c.kind, c.family, c.key, c.extra), c.expected);
  }

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures) {
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "review HH server checks passed\n";
  return 0;
}
