#undef NDEBUG
// Review FY1: the /api/mine transition system, Inductive, POWL and log
// skeleton families on the frame shapes of an XES upload follow pm4py's
// algorithm applies on read_xes's frame (`dataframe`) and its Polars copy
// (`lazyframe`). read_xes's frame has no column for an attribute no event
// carries: the pandas paths raise KeyError (a list selection names the
// missing labels), the Polars plans ColumnNotFoundError with the resolved
// plan. An unnamed trace is a NaN case (pandas groups leave it out, its
// activities still count where pm4py counts every row; Polars keeps one null
// group), an unnamed event a NaN activity (pandas: one transition per step,
// np.unique's TypeError, KeyError: nan). Expected values are pm4py 2.7.22
// black-box on the same uploads (review-harness round-fy, famval.py on ct/).
//
// Usage: minera_server_mine_xes_frames_tests <minera_server>

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
      {"log", log, "fy1.xes", "application/xml"},
  };
  const auto result = client.Post("/api/mine", items);
  return result ? result->body : std::string("no response");
}

// pm4py's Polars plans on the frame: ColumnNotFoundError at the group_by, with
// one select (the transition system's and the log skeleton's projection) or
// two (Inductive's and POWL's).
std::string polars_missing(const std::string& missing, const std::string& columns, const std::string& hint,
                           int selects) {
  std::string message = "unable to find column \"" + missing + "\"; valid columns: [" + columns + "]";
  if (!hint.empty()) message += "\n\nDid you mean \"" + hint + "\"?";
  message += "\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'group_by' <---\nDF [" + columns +
             "]; PROJECT */" + std::to_string(columns.empty() ? 0 : count_of(columns, ",") + 1) +
             " COLUMNS\n\nThis error occurred with the following context stack:\n";
  int frame = 0;
  for (int i = 0; i < selects; ++i) message += "\t[" + std::to_string(++frame) + "] 'select'\n";
  message += "\t[" + std::to_string(++frame) + "] 'group_by'\n";
  return message;
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

  const std::string no_case =
      kHead + trace("", event("a", "01") + event("b", "02")) + trace("", event("b", "03")) + "</log>\n";
  const std::string no_activity = kHead + trace("c1", event("", "01") + event("", "02")) + "</log>\n";
  const std::string no_events = kHead + trace("c1", "") + "</log>\n";
  const std::string no_timestamps =
      kHead + trace("c1", event("a", "") + event("b", "")) + trace("c2", event("a", "") + event("c", "")) + "</log>\n";
  const std::string unnamed_trace = kHead + trace("", event("a", "01") + event("b", "02")) +
                                    trace("c2", event("a", "03") + event("c", "04")) +
                                    trace("c3", event("a", "05") + event("c", "06") + event("b", "07")) + "</log>\n";
  const std::string unnamed_event = kHead + trace("c1", event("a", "01") + event("", "02") + event("b", "03")) +
                                    trace("c2", event("a", "04") + event("b", "05")) + "</log>\n";
  const std::string unnamed_twice = kHead + trace("c1", event("a", "01") + event("", "02") + event("b", "03")) +
                                    trace("c2", event("a", "04") + event("", "05") + event("b", "06")) + "</log>\n";
  const std::vector<std::string> families = {"transition_system", "inductive", "powl", "log_skeleton"};
  const auto expect = [&](const std::string& label, const std::string& body,
                          const std::vector<std::string>& outcomes) {
    for (std::size_t i = 0; i < families.size(); ++i)
      check(label + ", " + families[i], outcome(family(body, families[i])), outcomes[i]);
  };

  {  // pandas: read_xes's frame.
    const std::string at = " (dataframe shape)";
    const std::string case_key = "KeyError: 'case:concept:name'";
    expect("no trace has a name" + at, mine(client, no_case, "dataframe"),
           {case_key, "KeyError: \"['case:concept:name'] not in index\"",
            "KeyError: \"['case:concept:name'] not in index\"", case_key});
    expect("no event has an activity" + at, mine(client, no_activity, "dataframe"),
           {"KeyError: 'Column not found: concept:name'", "KeyError: \"['concept:name'] not in index\"",
            "KeyError: \"['concept:name'] not in index\"", "KeyError: 'Column not found: concept:name'"});
    const std::string none_of =
        "KeyError: \"None of [Index(['concept:name', 'case:concept:name', 'time:timestamp'], dtype='str')] are in "
        "the [columns]\"";
    expect("no events" + at, mine(client, no_events, "dataframe"), {case_key, none_of, none_of, case_key});
    expect("no event has a timestamp" + at, mine(client, no_timestamps, "dataframe"),
           {"OK", "KeyError: \"['time:timestamp'] not in index\"", "KeyError: \"['time:timestamp'] not in index\"",
            "OK"});
    const std::string str_float = "TypeError: '<' not supported between instances of 'str' and 'float'";
    const std::string float_str = "TypeError: '<' not supported between instances of 'float' and 'str'";
    const auto trace_body = mine(client, unnamed_trace, "dataframe");
    expect("an unnamed trace" + at, trace_body, {"OK", str_float, str_float, "OK"});
    // Counts over every row (the unnamed trace's a and b too), except
    // always_after and always_before, which count the traces only.
    check("an unnamed trace, log skeleton" + at,
          count_of(family(trace_body, "log_skeleton"),
                   "\"model\":{\"equivalence\":[{\"source\":\"c\",\"target\":\"a\"}],\"always_after\":[{\"source\":"
                   "\"a\",\"target\":\"c\"}],\"always_before\":[{\"source\":\"b\",\"target\":\"a\"},{\"source\":\"b\","
                   "\"target\":\"c\"},{\"source\":\"c\",\"target\":\"a\"}],\"never_together\":[],\"directly_follows\":"
                   "[],\"activ_freq\":{\"a\":[1],\"b\":[0,1],\"c\":[1]}}") == 1
              ? "model"
              : family(trace_body, "log_skeleton").substr(0, 600),
          "model");
    expect("an unnamed event" + at, mine(client, unnamed_event, "dataframe"),
           {"OK", float_str, float_str, "KeyError: nan"});
    // NaN != NaN: one transition per unnamed step.
    const auto twice = family(mine(client, unnamed_twice, "dataframe"), "transition_system");
    check("two unnamed events, transition system" + at,
          std::to_string(count_of(twice, "\"label\":null,\"count\":1,")), "2");
  }

  {  // Polars: the frame's Polars copy.
    const std::string at = " (lazyframe shape)";
    const std::string no_case_columns = "\"concept:name\", \"time:timestamp\"";
    expect("no trace has a name" + at, mine(client, no_case, "lazyframe"),
           {polars_missing("case:concept:name", no_case_columns, "concept:name", 1),
            polars_missing("case:concept:name", no_case_columns, "concept:name", 2),
            polars_missing("case:concept:name", no_case_columns, "concept:name", 2),
            polars_missing("case:concept:name", no_case_columns, "concept:name", 1)});
    const std::string no_activity_columns = "\"time:timestamp\", \"case:concept:name\"";
    expect("no event has an activity" + at, mine(client, no_activity, "lazyframe"),
           {polars_missing("concept:name", no_activity_columns, "", 1),
            polars_missing("concept:name", no_activity_columns, "", 2),
            polars_missing("concept:name", no_activity_columns, "", 2),
            polars_missing("concept:name", no_activity_columns, "", 1)});
    expect("no events" + at, mine(client, no_events, "lazyframe"),
           {polars_missing("case:concept:name", "", "", 1), polars_missing("case:concept:name", "", "", 2),
            polars_missing("case:concept:name", "", "", 2), polars_missing("case:concept:name", "", "", 1)});
    const std::string no_time_columns = "\"concept:name\", \"case:concept:name\"";
    expect("no event has a timestamp" + at, mine(client, no_timestamps, "lazyframe"),
           {"OK", polars_missing("time:timestamp", no_time_columns, "", 2),
            polars_missing("time:timestamp", no_time_columns, "", 2), "OK"});
    // The unnamed trace is one more (null) case.
    const auto trace_body = mine(client, unnamed_trace, "lazyframe");
    expect("an unnamed trace" + at, trace_body, {"OK", "OK", "OK", "OK"});
    check("an unnamed trace, log skeleton" + at,
          count_of(family(trace_body, "log_skeleton"),
                   "\"model\":{\"equivalence\":[{\"source\":\"b\",\"target\":\"a\"},{\"source\":\"c\",\"target\":"
                   "\"a\"}],\"always_after\":[],\"always_before\":[{\"source\":\"b\",\"target\":\"a\"},{\"source\":"
                   "\"c\",\"target\":\"a\"}],\"never_together\":[],\"directly_follows\":[],\"activ_freq\":{\"a\":[1],"
                   "\"b\":[0,1],\"c\":[0,1]}}") == 1
              ? "model"
              : family(trace_body, "log_skeleton").substr(0, 600),
          "model");
    check("an unnamed trace, Inductive" + at,
          count_of(family(trace_body, "inductive"), "\"result\":{\"activities\":[\"a\",\"b\",\"c\"]") == 1 ? "a, b, c"
                                                                                                         : "other",
          "a, b, c");
    // A null activity is one transition label.
    const auto twice = family(mine(client, unnamed_twice, "lazyframe"), "transition_system");
    check("two unnamed events, transition system" + at,
          std::to_string(count_of(twice, "\"label\":null,\"count\":2,")) + "/" +
              std::to_string(count_of(twice, "\"label\":null")),
          "1/1");
  }

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures != 0) {
    std::cerr << g_failures << " mine XES frame check(s) failed\n";
    return 1;
  }
  std::cout << "review FY1: mine families on XES upload frames passed\n";
  return 0;
}
