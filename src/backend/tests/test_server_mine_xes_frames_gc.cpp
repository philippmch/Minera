#undef NDEBUG
// Review GC: XES upload frames. The heuristics net's missing label becomes
// Python's "pre_" + NaN (pandas) or None (Polars) in the Petri conversion,
// a TypeError when it has a predecessor. Heuristics++ reads a start column
// as the frame holds it (efg sort, then the performance subtraction, then
// timestamp()) and lowers a lifecycle without one. A start key of numbers,
// booleans or dates beside numbers sorts as pandas' Categorical and Polars
// order it (pandas raising on a date beside a number), and the temporal
// profile compares it with the timestamp (numeric dtypes raise at once, an
// object column value by value, a text column that reads as dates fails at
// the subtraction). Without a start key, Alpha, the temporal profile and its
// conformance read the frame's own start_timestamp column. The token replay
// of a session's model groups the frame's activities by case, a missing
// case column raising. Expected values are pm4py 2.7.22 black-box on the
// same uploads (review-harness round-gc, ct/mk.py).
//
// Usage: minera_server_mine_xes_frames_gc_tests <minera_server>

#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
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

const std::string kHead =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
    "<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n";

std::string event(const std::string& activity, const std::string& minute, const std::string& extra = "") {
  std::string out = "<event>";
  if (!activity.empty()) out += "<string key=\"concept:name\" value=\"" + activity + "\"/>";
  if (!minute.empty()) out += "<date key=\"time:timestamp\" value=\"2021-03-01T08:" + minute + ":00.000+00:00\"/>";
  return out + extra + "</event>";
}

std::string date(const std::string& key, const std::string& minute) {
  return "<date key=\"" + key + "\" value=\"2021-03-01T08:" + minute + ":00.000+00:00\"/>";
}
std::string start(const std::string& minute) { return date("start_timestamp", minute); }
std::string text(const std::string& key, const std::string& value) {
  return "<string key=\"" + key + "\" value=\"" + value + "\"/>";
}
std::string integer(int value) { return "<int key=\"zzz\" value=\"" + std::to_string(value) + "\"/>"; }
std::string boolean(const std::string& value) { return "<boolean key=\"zzz\" value=\"" + value + "\"/>"; }

std::string trace(const std::string& name, const std::string& events) {
  return "<trace>" + (name.empty() ? std::string() : "<string key=\"concept:name\" value=\"" + name + "\"/>") + events +
         "</trace>\n";
}

// The JSON object that starts at `open` (a '{'), strings and escapes skipped.
std::string object_at(const std::string& body, std::size_t open) {
  int depth = 0;
  bool in_string = false;
  for (std::size_t i = open; i < body.size(); ++i) {
    const char c = body[i];
    if (in_string) {
      if (c == '\\') ++i;
      else if (c == '"') in_string = false;
    } else if (c == '"') {
      in_string = true;
    } else if (c == '{') {
      ++depth;
    } else if (c == '}' && --depth == 0) {
      return body.substr(open, i - open + 1);
    }
  }
  return {};
}

// The family's entry in family_results, or "" when the response has none.
std::string family(const std::string& body, const std::string& name) {
  const auto results = body.find("\"family_results\":{");
  if (results == std::string::npos) return {};
  const auto at = body.find("\"" + name + "\":{", results);
  if (at == std::string::npos) return {};
  return object_at(body, body.find('{', at));
}

// The family's error message (JSON escapes decoded), or "OK" when it ran.
std::string outcome(const std::string& entry) {
  if (entry.empty()) return "no family entry";
  if (entry.find("\"executed\":true") != std::string::npos) return "OK";
  const auto message = entry.find("\"message\":\"");
  if (message == std::string::npos) return "no message: " + entry.substr(0, 300);
  std::string out;
  for (std::size_t i = message + 11; i < entry.size() && entry[i] != '"'; ++i) {
    if (entry[i] != '\\') {
      out += entry[i];
      continue;
    }
    const char escaped = entry[++i];
    out += escaped == 'n' ? '\n' : escaped == 't' ? '\t' : escaped;
  }
  return out;
}

std::size_t count_of(const std::string& text, const std::string& needle) {
  std::size_t count = 0;
  for (auto at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1)) ++count;
  return count;
}

// Every value of `"key":` in the entry, comma separated, in order.
std::string values_of(const std::string& entry, const std::string& key) {
  std::string out;
  const std::string needle = "\"" + key + "\":";
  for (auto at = entry.find(needle); at != std::string::npos; at = entry.find(needle, at + 1)) {
    const auto from = at + needle.size();
    const auto to = entry.find_first_of(",}", from);
    out += (out.empty() ? "" : ",") + entry.substr(from, to - from);
  }
  return out;
}

// "found" when `entry` holds `needle`, else the start of the entry.
std::string holds(const std::string& entry, const std::string& needle) {
  return entry.find(needle) != std::string::npos ? "found" : entry.substr(0, 600);
}

std::string mine(httplib::Client& client, const std::string& log, const std::string& shape,
                 const std::vector<std::pair<std::string, std::string>>& fields = {}) {
  httplib::MultipartFormDataItems items = {
      {"include_traces", "false", "", ""},
      {"mining_input_shape", shape, "", ""},
      {"log", log, "gc1.xes", "application/xml"},
  };
  for (const auto& [name, value] : fields) items.push_back({name, value, "", ""});
  const auto result = client.Post("/api/mine", items);
  return result ? result->body : std::string("no response");
}

// The session's source id after a mine.
std::string source_id(httplib::Client& client) {
  const auto result = client.Get("/api/log-query/schema");
  if (!result) return "0";
  const auto at = result->body.find("\"source_id\":\"");
  if (at == std::string::npos) return "0";
  const auto from = at + 13;
  return result->body.substr(from, result->body.find('"', from) - from);
}

// A workbench route on the session `log` mines in `shape`.
std::string route(httplib::Client& client, const std::string& log, const std::string& shape, const std::string& path,
                  httplib::Params params, bool with_source = true) {
  mine(client, log, shape);
  if (with_source) params.emplace("source_id", source_id(client));
  const auto result = client.Post(path, params);
  return result ? result->body : std::string("no response");
}

// The route's error message (JSON escapes decoded), or "OK" when it has none.
std::string route_outcome(const std::string& body) {
  if (body.rfind("{\"error\":", 0) != 0) return "OK";
  auto from = body.find('"', 9);
  return from == std::string::npos ? body : outcome("{\"message\":" + body.substr(from));
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

  const auto log = [](const std::string& traces) { return kHead + traces + "</log>\n"; };
  using Fields = std::vector<std::pair<std::string, std::string>>;
  const Fields plusplus = {{"heuristic_variant", "plusplus"}};
  const std::string conv_pre = log(trace("c1", event("a", "01") + event("b", "02")) + trace("c2", event("f", "30") + event("", "30")));
  const std::string hpp_life = log(trace("c1", event("a", "01", text("lifecycle:transition", "start")) +
                                               event("a", "02", text("lifecycle:transition", "complete")) + event("b", "03")));
  const std::string hpp_start = log(trace("c1", event("a", "01", start("00")) + event("b", "03")) +
                                    trace("c2", event("a", "04", start("02")) + event("b", "06", start("05"))));
  const std::string zzz_int = log(trace("c1", event("a", "01", integer(5)) + event("b", "02", integer(1))) +
                                  trace("c2", event("a", "03", integer(2)) + event("c", "04", integer(3))));
  const std::string zzz_mixed = log(trace("c1", event("a", "01", integer(5)) + event("b", "02", date("zzz", "00"))) +
                                    trace("c2", event("a", "03", integer(2)) + event("c", "04")));
  const std::string zzz_bool = log(trace("c1", event("a", "01", boolean("true")) + event("b", "02", boolean("false"))) +
                                   trace("c2", event("a", "03") + event("c", "04", boolean("true"))));
  // c1 sorts a, b, c by its starts but b, a, c by its timestamps.
  const std::string own_start = log(trace("c1", event("a", "05", start("01")) + event("b", "04", start("02")) + event("c", "09", start("08"))) +
                                    trace("c2", event("a", "10", start("09")) + event("b", "12", start("11")) + event("c", "15", start("14"))));
  const std::string start_parse = log(trace("c1", event("a", "01", text("start_timestamp", "2021-03-01 08:00:00")) +
                                                  event("b", "02", text("start_timestamp", "2021-03-01 08:00:00"))));
  const std::string empty = log("");

  {  // The heuristics net's missing label in the Petri conversion.
    check("conversion of a missing label with a predecessor (dataframe shape)",
          outcome(family(mine(client, conv_pre, "dataframe"), "heuristic_petri_conversion")),
          "can only concatenate str (not \"float\") to str");
    check("conversion of a missing label with a predecessor (lazyframe shape)",
          outcome(family(mine(client, conv_pre, "lazyframe"), "heuristic_petri_conversion")),
          "can only concatenate str (not \"NoneType\") to str");
  }

  {  // Heuristics++ on a lifecycle without a start column, and on a start column.
    check("Heuristics++ with a lifecycle gap (dataframe shape)",
          outcome(family(mine(client, hpp_life, "dataframe", plusplus), "heuristic")), "'float' object has no attribute 'lower'");
    check("Heuristics++ with a lifecycle gap (lazyframe shape)",
          outcome(family(mine(client, hpp_life, "lazyframe", plusplus), "heuristic")), "'NoneType' object has no attribute 'lower'");
    check("Heuristics++ with a start gap (dataframe shape)",
          outcome(family(mine(client, hpp_start, "dataframe", plusplus), "heuristic")), "NaTType does not support timestamp");
    check("Heuristics++ with a start gap (lazyframe shape)",
          outcome(family(mine(client, hpp_start, "lazyframe", plusplus), "heuristic")),
          "'<' not supported between instances of 'NoneType' and 'datetime.datetime'");
  }

  {  // Start keys of other values than dates.
    const Fields alpha_zzz = {{"alpha_start_timestamp_attribute", "zzz"}};
    const Fields tp_zzz = {{"temporal_target_timestamp_attribute", "zzz"}};
    for (const std::string shape : {"dataframe", "lazyframe"}) {
      const std::string at = " (" + shape + " shape)";
      const auto entry = family(mine(client, zzz_int, shape, alpha_zzz), "alpha");
      check("Alpha with an integer start key" + at, outcome(entry), "OK");
      // c1 sorts b (1) before a (5): b leads to a, a to c.
      check("Alpha with an integer start key, places" + at, values_of(entry, "place_count"), "4");
    }
    check("Alpha with a date beside an integer start key (dataframe shape)",
          outcome(family(mine(client, zzz_mixed, "dataframe", alpha_zzz), "alpha")),
          "TypeError: 'values' is not ordered, please explicitly specify the categories order by passing in a categories "
          "argument.");
    const auto polars_mixed = family(mine(client, zzz_mixed, "lazyframe", alpha_zzz), "alpha");
    check("Alpha with a date beside an integer start key (lazyframe shape)", outcome(polars_mixed), "OK");
    check("Alpha with a date beside an integer start key, places (lazyframe shape)", values_of(polars_mixed, "place_count"), "3");
    check("temporal profile with an integer start key (dataframe shape)",
          outcome(family(mine(client, zzz_int, "dataframe", tp_zzz), "temporal_profile")),
          "TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray");
    const auto polars_int = outcome(family(mine(client, zzz_int, "lazyframe", tp_zzz), "temporal_profile"));
    check("temporal profile with an integer start key (lazyframe shape)",
          holds(polars_int, "- not allowed on i64 and datetime[\xce\xbcs, UTC]\n"), "found");
    check("temporal profile with an integer start key, plan (lazyframe shape)",
          holds(polars_int, "FILTER col(\"time:timestamp\").cast(Int64) <= col(\"zzz_2\")\n"), "found");
    check("temporal profile with a boolean start key beside a gap (dataframe shape)",
          outcome(family(mine(client, zzz_bool, "dataframe", tp_zzz), "temporal_profile")),
          "TypeError: '<=' not supported between instances of 'Timestamp' and 'bool'");
    check("temporal profile with text starts that read as dates (dataframe shape)",
          outcome(family(mine(client, start_parse, "dataframe", {{"temporal_target_timestamp_attribute", "start_timestamp"}}),
                         "temporal_profile")),
          "TypeError: cannot subtract DatetimeArray from ndarray[object]");
  }

  {  // The frame's own start column without a start key.
    for (const std::string shape : {"dataframe", "lazyframe"}) {
      const std::string at = " (" + shape + " shape)";
      const auto body = mine(client, own_start, shape);
      const auto alpha = family(body, "alpha");
      check("Alpha on the frame's start column" + at, outcome(alpha), "OK");
      check("Alpha on the frame's start column, places" + at, values_of(alpha, "place_count"), "4");
      const auto profile = family(body, "temporal_profile");
      check("temporal profile on the frame's start column" + at, outcome(profile), "OK");
      check("temporal profile on the frame's start column, means" + at, values_of(profile, "mean_seconds"), "60,210,180");
    }
    const auto conformance = route(client, own_start, "dataframe", "/api/analyze-behavior",
                                   {{"family", "temporal_conformance"}, {"variant", "dataframe"},
                                    {"temporal_profile_json", "[[\"a\",\"b\",240.0,0.0],[\"a\",\"c\",240.0,60.0]]"},
                                    {"temporal_zeta", "1.0"}});
    check("temporal conformance on the frame's start column (dataframe shape)", route_outcome(conformance), "OK");
    // c2's a to b waits 60 s from a's completion to b's start, against a mean of 240 s with no deviation.
    check("temporal conformance on the frame's start column, deviation (dataframe shape)",
          holds(conformance, "[[],[{\"case_id\":\"c2\",\"source\":\"a\",\"target\":\"b\",\"elapsed_seconds\":60,"), "found");
  }

  {  // The token replay of a session's model groups the frame by case.
    check("token replay of an empty frame (dataframe shape)",
          outcome(family(mine(client, empty, "dataframe"), "heuristic_conformance")), "KeyError: 'case:concept:name'");
    const auto polars_replay = outcome(family(mine(client, empty, "lazyframe"), "heuristic_conformance"));
    check("token replay of an empty frame (lazyframe shape)",
          holds(polars_replay, "unable to find column \"case:concept:name\"; valid columns: []\n"), "found");
    check("token replay of an empty frame, stack (lazyframe shape)",
          holds(polars_replay, "\t[1] 'select'\n\t[2] 'group_by'\n"), "found");
  }

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures != 0) {
    std::cerr << g_failures << " XES frame check(s) failed\n";
    return 1;
  }
  std::cout << "review GC: heuristics net conversion, Heuristics++ intervals, start columns and frame replay on XES upload frames passed\n";
  return 0;
}
