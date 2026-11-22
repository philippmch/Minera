#undef NDEBUG
// Review FZ1: the /api/mine Alpha, heuristics and temporal profile families
// on the frame shapes of an XES upload with gaps follow pm4py's algorithm
// applies on read_xes's frame (`dataframe`) and its Polars copy
// (`lazyframe`); the log skeleton on the Polars copy keeps a missing
// activity as the label None. Alpha and the temporal profile read the
// timestamp first (pandas KeyError naming it, Polars ColumnNotFoundError at
// with_columns), then select the case, activity and timestamp (pandas'
// list-selection KeyError; Polars' resolved plan at Alpha's group_by or the
// temporal profile's sort). A pandas row without an activity leaves Alpha's
// pairs and boundary activities unless its case has no activity at all (the
// label NaN starts and ends it); on Polars it is a boundary activity of its
// own. Heuristics converts the frame to a log: a missing column raises
// KeyError when a case reaches it (the case always, the activity at a pair),
// a missing label is the activity nan/None, its successor raises "not
// subscriptable" and a sort beside a named activity TypeError. Expected
// values are pm4py 2.7.22 black-box on the same uploads (review-harness
// round-fz, famval.py on ct/ and ct2/).
//
// Usage: minera_server_mine_xes_frames_fz_tests <minera_server>

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

std::string event(const std::string& activity, const std::string& minute) {
  std::string out = "<event>";
  if (!activity.empty()) out += "<string key=\"concept:name\" value=\"" + activity + "\"/>";
  if (!minute.empty()) out += "<date key=\"time:timestamp\" value=\"2021-03-01T08:" + minute + ":00.000+00:00\"/>";
  return out + "</event>";
}

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

std::string mine(httplib::Client& client, const std::string& log, const std::string& shape) {
  httplib::MultipartFormDataItems items = {
      {"include_traces", "false", "", ""},
      {"mining_input_shape", shape, "", ""},
      {"log", log, "fz1.xes", "application/xml"},
  };
  const auto result = client.Post("/api/mine", items);
  return result ? result->body : std::string("no response");
}

// The family's error type ("" when it ran or carries none).
std::string error_type(const std::string& entry) {
  const auto at = entry.find("\"error\":{\"type\":\"");
  if (at == std::string::npos) return {};
  const auto from = at + 17;
  return entry.substr(from, entry.find('"', from) - from);
}

// "found" when `entry` holds `needle`, else the start of the entry.
std::string holds(const std::string& entry, const std::string& needle) {
  return entry.find(needle) != std::string::npos ? "found" : entry.substr(0, 600);
}

// Polars' plan for Alpha and the temporal profile: the start column is an
// alias of the timestamp, the selection keeps the read columns in frame order.
std::string polars_plan(const std::string& missing, const std::string& frame, const std::string& selected,
                        const std::string& hint, const std::string& sort_by, const std::string& failed_at,
                        const std::vector<std::string>& stack) {
  std::string message = "unable to find column \"" + missing + "\"; valid columns: [" + selected + "]";
  if (!hint.empty()) message += "\n\nDid you mean \"" + hint + "\"?";
  message += "\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING '" + failed_at + "' <---\n";
  std::string pad;
  if (!sort_by.empty()) {
    message += "SORT BY " + sort_by + "\n";
    pad = "  ";
  }
  std::string cols;
  for (std::size_t at = 0; at < selected.size();) {
    const auto close = selected.find('"', at + 1);
    cols += (cols.empty() ? "col(" : ", col(") + selected.substr(at, close - at + 1) + ")";
    at = selected.find('"', close + 1);
    if (at == std::string::npos) break;
  }
  message += pad + "SELECT [" + cols + "]\n";
  pad += "  ";
  message += pad + " WITH_COLUMNS:\n" + pad + " [col(\"time:timestamp\").alias(\"start_timestamp\")] \n";
  pad += "  ";
  message += pad + "DF [" + frame + "]; PROJECT */" + std::to_string(frame.empty() ? 0 : count_of(frame, ",") + 1) +
             " COLUMNS\n\nThis error occurred with the following context stack:\n";
  for (std::size_t i = 0; i < stack.size(); ++i) message += "\t[" + std::to_string(i + 1) + "] '" + stack[i] + "'\n";
  return message;
}

// Polars' ColumnNotFoundError at the first plan node, over the frame itself.
std::string polars_frame_missing(const std::string& missing, const std::string& frame, const std::string& node) {
  return "unable to find column \"" + missing + "\"; valid columns: [" + frame +
         "]\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING '" + node + "' <---\nDF [" + frame +
         "]; PROJECT */" + std::to_string(frame.empty() ? 0 : count_of(frame, ",") + 1) +
         " COLUMNS\n\nThis error occurred with the following context stack:\n\t[1] '" + node + "'\n";
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
  const std::string no_case = log(trace("", event("a", "01") + event("b", "02")) + trace("", event("b", "03")));
  const std::string no_activity = log(trace("c1", event("", "01") + event("", "02")));
  const std::string no_timestamps =
      log(trace("c1", event("a", "") + event("b", "")) + trace("c2", event("a", "") + event("c", "")));
  const std::string no_events = log(trace("c1", ""));
  const std::string unnamed_trace = log(trace("", event("c", "01") + event("d", "02") + event("e", "03")) +
                                        trace("c2", event("a", "04") + event("b", "05")) +
                                        trace("c3", event("a", "06") + event("c", "07")));
  const std::string silent_case = log(trace("c1", event("", "01") + event("", "02")) +
                                      trace("c2", event("a", "03") + event("b", "04")) +
                                      trace("c3", event("", "05") + event("a", "06")));
  const std::string mid_unnamed =
      log(trace("c1", event("a", "01") + event("", "02") + event("b", "03")) + trace("c2", event("a", "04") + event("b", "05")));
  const std::string split_unnamed =
      log(trace("c1", event("a", "01") + event("", "02")) + trace("c2", event("a", "03") + event("b", "04")));
  const std::string end_unnamed =
      log(trace("c1", event("a", "01") + event("b", "02") + event("", "03")) + trace("c2", event("a", "04") + event("b", "05")));
  // No event names an activity: only a case of two events reaches the key.
  const std::string nameless_single = log(trace("c1", event("", "01")));
  const std::string nameless_pair = log(trace("c1", event("", "01")) + trace("c2", event("", "02") + event("", "03")));
  const std::string nameless_unnamed_pair =
      log(trace("", event("", "01")) + trace("c1", event("", "02")) + trace("", event("", "03")));
  const std::string nameless_unnamed_single = log(trace("", event("", "01")) + trace("c1", event("", "02")));

  const std::vector<std::string> families = {"alpha", "heuristic", "temporal_profile", "log_skeleton"};
  const auto expect = [&](const std::string& label, const std::string& body,
                          const std::vector<std::string>& outcomes) {
    for (std::size_t i = 0; i < families.size(); ++i)
      if (!outcomes[i].empty()) check(label + ", " + families[i], outcome(family(body, families[i])), outcomes[i]);
  };
  const auto heuristic_error = [&](const std::string& label, const std::string& body, const std::string& type) {
    check(label + ", heuristic error type", error_type(family(body, "heuristic")), type);
  };
  const std::string null_transition = "\"activity_name\":null,\"activity_is_null\":true,\"display_name_supplied\":false,"
                                      "\"display_name\":null,";
  const std::string not_subscriptable_tail = " object is not subscriptable";

  {  // pandas: read_xes's frame.
    const std::string at = " (dataframe shape)";
    const std::string case_list = "KeyError: \"['case:concept:name'] not in index\"";
    const std::string activity_list = "KeyError: \"['concept:name'] not in index\"";
    const std::string timestamp_key = "KeyError: 'time:timestamp'";
    const std::string skeleton_column = "KeyError: 'Column not found: concept:name'";
    auto body = mine(client, no_case, "dataframe");
    expect("no trace has a name" + at, body,
           {case_list, "'case:concept:name'", case_list, "KeyError: 'case:concept:name'"});
    heuristic_error("no trace has a name" + at, body, "builtins.KeyError");
    body = mine(client, no_activity, "dataframe");
    expect("no event has an activity" + at, body, {activity_list, "'concept:name'", activity_list, skeleton_column});
    heuristic_error("no event has an activity" + at, body, "builtins.KeyError");
    expect("no events" + at, mine(client, no_events, "dataframe"),
           {timestamp_key, "OK", timestamp_key, "KeyError: 'case:concept:name'"});
    expect("no event has a timestamp" + at, mine(client, no_timestamps, "dataframe"),
           {timestamp_key, "OK", timestamp_key, "OK"});

    body = mine(client, unnamed_trace, "dataframe");
    expect("an unnamed trace" + at, body, {"OK", "OK", "OK", "OK"});
    // The NaN case leaves the groups: c starts only the named cases' nets.
    check("an unnamed trace, alpha" + at,
          holds(family(body, "alpha"), "\"activity_name\":\"c\",\"activity_is_null\":false,\"display_name_supplied\":"
                                       "false,\"display_name\":null,\"inputs\":[2],\"outputs\":[1]"),
          "found");
    check("an unnamed trace, heuristic" + at,
          holds(family(body, "heuristic"), "\"activities\":[\"c\",\"d\",\"e\",\"a\",\"b\"]"), "found");

    body = mine(client, silent_case, "dataframe");
    expect("a case without activities" + at, body,
           {"OK", "'float'" + not_subscriptable_tail, "OK", "KeyError: nan"});
    heuristic_error("a case without activities" + at, body, "builtins.TypeError");
    // Its label NaN starts and ends it.
    check("a case without activities, alpha" + at,
          holds(family(body, "alpha"), null_transition + "\"inputs\":[0],\"outputs\":[1]"), "found");

    body = mine(client, mid_unnamed, "dataframe");
    expect("an unnamed event inside a case" + at, body,
           {"OK", "'float'" + not_subscriptable_tail, "OK", "KeyError: nan"});
    check("an unnamed event inside a case, alpha" + at,
          std::to_string(count_of(family(body, "alpha"), "\"activity_is_null\":true")), "0");
    check("an unnamed event inside a case, temporal profile" + at,
          holds(family(body, "temporal_profile"),
                "\"entries\":[{\"source\":\"a\",\"target\":\"b\",\"count\":2,\"mean_seconds\":90,"),
          "found");

    body = mine(client, split_unnamed, "dataframe");
    expect("an unnamed last event before a named one" + at, body,
           {"OK", "'<' not supported between instances of 'str' and 'float'", "OK", "KeyError: nan"});
    heuristic_error("an unnamed last event before a named one" + at, body, "builtins.TypeError");

    body = mine(client, end_unnamed, "dataframe");
    expect("an unnamed last event" + at, body, {"OK", "OK", "OK", "KeyError: nan"});
    check("an unnamed last event, heuristic" + at,
          holds(family(body, "heuristic"), "\"activities\":[\"a\",\"b\",null]"), "found");

    expect("one nameless event" + at, mine(client, nameless_single, "dataframe"),
           {activity_list, "OK", activity_list, skeleton_column});
    expect("a case of two nameless events" + at, mine(client, nameless_pair, "dataframe"),
           {activity_list, "'concept:name'", activity_list, skeleton_column});
    expect("two unnamed traces of one nameless event" + at, mine(client, nameless_unnamed_pair, "dataframe"),
           {"", "'concept:name'", "", ""});
    expect("one unnamed trace of one nameless event" + at, mine(client, nameless_unnamed_single, "dataframe"),
           {"", "OK", "", ""});
  }

  {  // Polars: the frame's Polars copy.
    const std::string at = " (lazyframe shape)";
    auto body = mine(client, no_case, "lazyframe");
    expect("no trace has a name" + at, body,
           {polars_plan("case:concept:name", "\"concept:name\", \"time:timestamp\"",
                        "\"concept:name\", \"time:timestamp\", \"start_timestamp\"", "concept:name", "", "group_by",
                        {"sort", "with_columns", "filter", "filter", "group_by"}),
            "'case:concept:name'",
            polars_plan("case:concept:name", "\"concept:name\", \"time:timestamp\"",
                        "\"concept:name\", \"time:timestamp\", \"start_timestamp\"", "concept:name", "", "sort",
                        {"sort"}),
            ""});
    heuristic_error("no trace has a name" + at, body, "builtins.KeyError");
    body = mine(client, no_activity, "lazyframe");
    expect("no event has an activity" + at, body,
           {polars_plan("concept:name", "\"time:timestamp\", \"case:concept:name\"",
                        "\"time:timestamp\", \"case:concept:name\", \"start_timestamp\"", "",
                        "[col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]", "group_by",
                        {"with_columns", "filter", "filter", "group_by"}),
            "'concept:name'", "KeyError: \"['concept:name', 'concept:name_2'] not in index\"", ""});
    const auto no_events_missing = polars_frame_missing("time:timestamp", "", "with_columns");
    expect("no events" + at, mine(client, no_events, "lazyframe"),
           {no_events_missing, "OK", no_events_missing, ""});
    const auto no_timestamps_missing =
        polars_frame_missing("time:timestamp", "\"concept:name\", \"case:concept:name\"", "with_columns");
    expect("no event has a timestamp" + at, mine(client, no_timestamps, "lazyframe"),
           {no_timestamps_missing, "OK", no_timestamps_missing, "OK"});

    body = mine(client, unnamed_trace, "lazyframe");
    expect("an unnamed trace" + at, body, {"OK", "OK", "OK", "OK"});
    // The null case's first and last rows are boundary activities; its pairs
    // never count.
    check("an unnamed trace, alpha" + at,
          holds(family(body, "alpha"), "\"activity_name\":\"e\",\"activity_is_null\":false,\"display_name_supplied\":"
                                       "false,\"display_name\":null,\"inputs\":[],\"outputs\":[1]"),
          "found");
    check("an unnamed trace, temporal profile" + at,
          std::to_string(count_of(family(body, "temporal_profile"), "\"source\":")), "2");

    body = mine(client, silent_case, "lazyframe");
    expect("a case without activities" + at, body, {"OK", "'NoneType'" + not_subscriptable_tail, "OK", "OK"});
    heuristic_error("a case without activities" + at, body, "builtins.TypeError");
    check("a case without activities, alpha" + at,
          holds(family(body, "alpha"), null_transition + "\"inputs\":[0],\"outputs\":[1]"), "found");
    check("a case without activities, log skeleton" + at,
          holds(family(body, "log_skeleton"),
                "\"never_together\":[{\"source\":null,\"target\":\"b\"},{\"source\":\"b\",\"target\":null}],"
                "\"directly_follows\":[],\"activ_freq\":{\"a\":[0,1],\"b\":[0,1]},\"null_activ_freq\":[0,1,2]}"),
          "found");

    body = mine(client, mid_unnamed, "lazyframe");
    expect("an unnamed event inside a case" + at, body, {"OK", "'NoneType'" + not_subscriptable_tail, "OK", "OK"});
    check("an unnamed event inside a case, log skeleton" + at,
          holds(family(body, "log_skeleton"), "\"directly_follows\":[{\"source\":null,\"target\":\"b\"}]"), "found");

    body = mine(client, split_unnamed, "lazyframe");
    expect("an unnamed last event before a named one" + at, body,
           {"OK", "'<' not supported between instances of 'str' and 'NoneType'", "OK", "OK"});
    heuristic_error("an unnamed last event before a named one" + at, body, "builtins.TypeError");
    check("an unnamed last event before a named one, alpha" + at,
          holds(family(body, "alpha"), null_transition + "\"inputs\":[],\"outputs\":[1]"), "found");

    body = mine(client, end_unnamed, "lazyframe");
    expect("an unnamed last event" + at, body, {"OK", "OK", "OK", "OK"});
    check("an unnamed last event, alpha" + at,
          holds(family(body, "alpha"), null_transition + "\"inputs\":[],\"outputs\":[1]"), "found");
    check("an unnamed last event, heuristic" + at,
          holds(family(body, "heuristic"), "\"activities\":[\"a\",\"b\",null]"), "found");
    check("an unnamed last event, log skeleton" + at,
          holds(family(body, "log_skeleton"),
                "\"model\":{\"equivalence\":[{\"source\":null,\"target\":\"a\"},{\"source\":null,\"target\":\"b\"},"
                "{\"source\":\"a\",\"target\":\"b\"},{\"source\":\"b\",\"target\":\"a\"}],\"always_after\":[{\"source\":"
                "\"a\",\"target\":\"b\"}],\"always_before\":[{\"source\":null,\"target\":\"a\"},{\"source\":null,"
                "\"target\":\"b\"},{\"source\":\"b\",\"target\":\"a\"}],\"never_together\":[],\"directly_follows\":"
                "[{\"source\":\"a\",\"target\":\"b\"}],\"activ_freq\":{\"a\":[1],\"b\":[1]},\"null_activ_freq\":[0,1]}"),
          "found");

    expect("one nameless event" + at, mine(client, nameless_single, "lazyframe"), {"", "OK", "", ""});
    expect("a case of two nameless events" + at, mine(client, nameless_pair, "lazyframe"),
           {"", "'concept:name'", "", ""});
    expect("two unnamed traces of one nameless event" + at, mine(client, nameless_unnamed_pair, "lazyframe"),
           {"", "'concept:name'", "", ""});
    expect("one unnamed trace of one nameless event" + at, mine(client, nameless_unnamed_single, "lazyframe"),
           {"", "OK", "", ""});
  }

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures != 0) {
    std::cerr << g_failures << " mine XES frame check(s) failed\n";
    return 1;
  }
  std::cout << "review FZ1: Alpha, heuristics and temporal profile on XES upload frames passed\n";
  return 0;
}
