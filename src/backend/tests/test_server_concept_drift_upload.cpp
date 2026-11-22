#undef NDEBUG
// Review CV1: /api/concept-drift on an upload follows pm4py's BOSE DataFrame
// adapter on the upload's frame (format_dataframe over read_csv, or
// read_xes): cases sorted by the frame's case:concept:name text (int case ids
// 1..12 sort as "1", "10", "11", "12", "2", ...), change times from the
// selected case groups, activities typed as the frame holds them (an int
// column fails the frames' str.join as numpy.int64; an XES float attribute
// with gaps is a float64 column whose gaps are NaN, failing as
// numpy.float64). Expected values are pm4py 2.7.22 black-box on the same
// uploads (review-harness round-cv, t1/ref.py, OPENBLAS_CORETYPE=Haswell).
//
// Usage: minera_server_concept_drift_upload_tests <minera_server>

#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <initializer_list>
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

const std::string kCsv =
    "case,act,ts,num,flt\n"
    "1,a,2024-01-01T08:00:00Z,10,1.5\n"
    "1,b,2024-01-01T09:00:00Z,11,2.0\n"
    "1,c,2024-01-01T10:00:00Z,12,2.5\n"
    "2,a,2024-01-02T08:00:00Z,20,3.0\n"
    "2,b,2024-01-02T09:00:00Z,21,3.5\n"
    "2,c,2024-01-02T10:00:00Z,22,\n"
    "3,a,2024-01-03T08:00:00Z,30,4.5\n"
    "3,b,2024-01-03T09:00:00Z,31,\n"
    "3,c,2024-01-03T10:00:00Z,32,5.5\n"
    "3,b,2024-01-03T11:00:00Z,33,6.0\n"
    "4,a,2024-01-04T08:00:00Z,40,\n"
    "4,b,2024-01-04T09:00:00Z,41,6.5\n"
    "4,c,2024-01-04T10:00:00Z,42,7.0\n"
    "5,a,2024-01-05T08:00:00Z,50,7.5\n"
    "5,b,2024-01-05T09:00:00Z,51,8.0\n"
    "5,c,2024-01-05T10:00:00Z,52,8.5\n"
    "6,a,2024-01-06T08:00:00Z,60,9.0\n"
    "6,b,2024-01-06T09:00:00Z,61,9.5\n"
    "6,c,2024-01-06T10:00:00Z,62,\n"
    "7,a,2024-01-07T08:00:00Z,70,10.5\n"
    "7,c,2024-01-07T09:00:00Z,71,\n"
    "7,d,2024-01-07T10:00:00Z,72,11.5\n"
    "8,a,2024-01-08T08:00:00Z,80,\n"
    "8,c,2024-01-08T09:00:00Z,81,12.5\n"
    "8,d,2024-01-08T10:00:00Z,82,13.0\n"
    "9,a,2024-01-09T08:00:00Z,90,13.5\n"
    "9,c,2024-01-09T09:00:00Z,91,14.0\n"
    "9,d,2024-01-09T10:00:00Z,92,14.5\n"
    "9,b,2024-01-09T11:00:00Z,93,\n"
    "10,a,2024-01-10T08:00:00Z,100,15.0\n"
    "10,c,2024-01-10T09:00:00Z,101,15.5\n"
    "10,d,2024-01-10T10:00:00Z,102,\n"
    "11,a,2024-01-11T08:00:00Z,110,16.5\n"
    "11,c,2024-01-11T09:00:00Z,111,\n"
    "11,d,2024-01-11T10:00:00Z,112,17.5\n"
    "12,a,2024-01-12T08:00:00Z,120,\n"
    "12,c,2024-01-12T09:00:00Z,121,18.5\n"
    "12,d,2024-01-12T10:00:00Z,122,19.0\n";

const std::string kXes =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<log xes.version=\"1.0\">\n"
    "<trace><string key=\"concept:name\" value=\"c02\"/>\n"
    "<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2024-01-02T08:00:00Z\"/><int key=\"num\" value=\"20\"/><float key=\"flt\" value=\"3.0\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2024-01-02T09:00:00Z\"/><int key=\"num\" value=\"21\"/><float key=\"flt\" value=\"3.5\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2024-01-02T10:00:00Z\"/><int key=\"num\" value=\"22\"/></event>\n"
    "</trace>\n"
    "<trace><string key=\"concept:name\" value=\"c08\"/>\n"
    "<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2024-01-08T08:00:00Z\"/><int key=\"num\" value=\"80\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2024-01-08T09:00:00Z\"/><int key=\"num\" value=\"81\"/><float key=\"flt\" value=\"12.5\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2024-01-08T10:00:00Z\"/><int key=\"num\" value=\"82\"/><float key=\"flt\" value=\"13.0\"/></event>\n"
    "</trace>\n"
    "<trace><string key=\"concept:name\" value=\"c11\"/>\n"
    "<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2024-01-11T08:00:00Z\"/><int key=\"num\" value=\"110\"/><float key=\"flt\" value=\"16.5\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2024-01-11T09:00:00Z\"/><int key=\"num\" value=\"111\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2024-01-11T10:00:00Z\"/><int key=\"num\" value=\"112\"/><float key=\"flt\" value=\"17.5\"/></event>\n"
    "</trace>\n"
    "<trace><string key=\"concept:name\" value=\"c01\"/>\n"
    "<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2024-01-01T08:00:00Z\"/><int key=\"num\" value=\"10\"/><float key=\"flt\" value=\"1.5\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2024-01-01T09:00:00Z\"/><int key=\"num\" value=\"11\"/><float key=\"flt\" value=\"2.0\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2024-01-01T10:00:00Z\"/><int key=\"num\" value=\"12\"/><float key=\"flt\" value=\"2.5\"/></event>\n"
    "</trace>\n"
    "<trace><string key=\"concept:name\" value=\"c07\"/>\n"
    "<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2024-01-07T08:00:00Z\"/><int key=\"num\" value=\"70\"/><float key=\"flt\" value=\"10.5\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2024-01-07T09:00:00Z\"/><int key=\"num\" value=\"71\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2024-01-07T10:00:00Z\"/><int key=\"num\" value=\"72\"/><float key=\"flt\" value=\"11.5\"/></event>\n"
    "</trace>\n"
    "<trace><string key=\"concept:name\" value=\"c12\"/>\n"
    "<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2024-01-12T08:00:00Z\"/><int key=\"num\" value=\"120\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2024-01-12T09:00:00Z\"/><int key=\"num\" value=\"121\"/><float key=\"flt\" value=\"18.5\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2024-01-12T10:00:00Z\"/><int key=\"num\" value=\"122\"/><float key=\"flt\" value=\"19.0\"/></event>\n"
    "</trace>\n"
    "<trace><string key=\"concept:name\" value=\"c05\"/>\n"
    "<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2024-01-05T08:00:00Z\"/><int key=\"num\" value=\"50\"/><float key=\"flt\" value=\"7.5\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2024-01-05T09:00:00Z\"/><int key=\"num\" value=\"51\"/><float key=\"flt\" value=\"8.0\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2024-01-05T10:00:00Z\"/><int key=\"num\" value=\"52\"/><float key=\"flt\" value=\"8.5\"/></event>\n"
    "</trace>\n"
    "<trace><string key=\"concept:name\" value=\"c06\"/>\n"
    "<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2024-01-06T08:00:00Z\"/><int key=\"num\" value=\"60\"/><float key=\"flt\" value=\"9.0\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2024-01-06T09:00:00Z\"/><int key=\"num\" value=\"61\"/><float key=\"flt\" value=\"9.5\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2024-01-06T10:00:00Z\"/><int key=\"num\" value=\"62\"/></event>\n"
    "</trace>\n"
    "<trace><string key=\"concept:name\" value=\"c03\"/>\n"
    "<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2024-01-03T08:00:00Z\"/><int key=\"num\" value=\"30\"/><float key=\"flt\" value=\"4.5\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2024-01-03T09:00:00Z\"/><int key=\"num\" value=\"31\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2024-01-03T10:00:00Z\"/><int key=\"num\" value=\"32\"/><float key=\"flt\" value=\"5.5\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2024-01-03T11:00:00Z\"/><int key=\"num\" value=\"33\"/><float key=\"flt\" value=\"6.0\"/></event>\n"
    "</trace>\n"
    "<trace><string key=\"concept:name\" value=\"c09\"/>\n"
    "<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2024-01-09T08:00:00Z\"/><int key=\"num\" value=\"90\"/><float key=\"flt\" value=\"13.5\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2024-01-09T09:00:00Z\"/><int key=\"num\" value=\"91\"/><float key=\"flt\" value=\"14.0\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2024-01-09T10:00:00Z\"/><int key=\"num\" value=\"92\"/><float key=\"flt\" value=\"14.5\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2024-01-09T11:00:00Z\"/><int key=\"num\" value=\"93\"/></event>\n"
    "</trace>\n"
    "<trace><string key=\"concept:name\" value=\"c10\"/>\n"
    "<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2024-01-10T08:00:00Z\"/><int key=\"num\" value=\"100\"/><float key=\"flt\" value=\"15.0\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2024-01-10T09:00:00Z\"/><int key=\"num\" value=\"101\"/><float key=\"flt\" value=\"15.5\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2024-01-10T10:00:00Z\"/><int key=\"num\" value=\"102\"/></event>\n"
    "</trace>\n"
    "<trace><string key=\"concept:name\" value=\"c04\"/>\n"
    "<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2024-01-04T08:00:00Z\"/><int key=\"num\" value=\"40\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2024-01-04T09:00:00Z\"/><int key=\"num\" value=\"41\"/><float key=\"flt\" value=\"6.5\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2024-01-04T10:00:00Z\"/><int key=\"num\" value=\"42\"/><float key=\"flt\" value=\"7.0\"/></event>\n"
    "</trace>\n"
    "</log>\n";

// The values after each `"key":` in `body` from `from` on, up to `stop`,
// joined by commas (numbers only).
std::string values_after(const std::string& body, const std::string& key, const std::string& from,
                         const std::string& stop) {
  auto begin = body.find(from);
  if (begin == std::string::npos) return "<missing " + from + ">";
  const auto end = stop.empty() ? std::string::npos : body.find(stop, begin + from.size());
  std::string out;
  const std::string needle = "\"" + key + "\":";
  for (auto at = body.find(needle, begin); at != std::string::npos && at < end; at = body.find(needle, at + 1)) {
    auto value = at + needle.size();
    auto last = body.find_first_of(",}]", value);
    if (!out.empty()) out += ',';
    out += body.substr(value, last - value);
  }
  return out;
}

// change timestamps | p-values | returned sublogs' event counts, or the error
// (p-values print with 17 digits: 0.14999999999999999 is pm4py's 0.15).
std::string outcome(const httplib::Result& result) {
  if (!result) return "no response";
  const auto& body = result->body;
  if (result->status != 200) {
    auto at = body.find("\"error\":");
    if (at == std::string::npos) return "ERR " + body;
    at = body.find_first_not_of(' ', at + 8);
    return "ERR " + body.substr(at);
  }
  const auto stamps = body.find("\"change_timestamps\":[");
  std::string cp = stamps == std::string::npos ? "<missing>" : body.substr(stamps + 21, body.find(']', stamps) - stamps - 21);
  return cp + " | " + values_after(body, "p_value", "\"change_points\":", "]") + " | " +
         values_after(body, "event_count", "\"returned_sublogs\":", "]");
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
  const auto drift = [&](const std::string& query) {
    return outcome(client.Post("/api/concept-drift?drift_include_timestamps=true&" + query, "", "text/plain"));
  };
  httplib::MultipartFormDataItems csv = {{"log", kCsv, "cv1.csv", "text/csv"},
                                         {"include_traces", "false", "", ""},
                                         {"case_column", "case", "", ""},
                                         {"activity_column", "act", "", ""},
                                         {"timestamp_column", "ts", "", ""}};
  auto mined = client.Post("/api/mine", csv);
  check("mine the CSV", mined ? std::to_string(mined->status) : "no response", "200");
  check("CSV, frame case order",
        drift("drift_sub_log_size=2&drift_window_size=2&drift_num_permutations=50&drift_threshold=0.5&drift_seed=0"),
        "1704182400 | 0.38 | 12,26");
  check("CSV, one case per sublog",
        drift("drift_sub_log_size=1&drift_window_size=3&drift_num_permutations=20&drift_threshold=1.0&"
              "drift_max_no_change_points=2&drift_seed=7"),
        "1704182400,1704268800 | 0.14999999999999999,0.20000000000000001 | 12,3,23");
  check("CSV, int activity column",
        drift("drift_sub_log_size=2&drift_window_size=1&drift_num_permutations=10&drift_activity_attribute=num&drift_seed=0"),
        "ERR \"TypeError: sequence item 0: expected str instance, numpy.int64 found\"}");
  check("CSV, the CSV's own case column",
        drift("drift_sub_log_size=2&drift_window_size=1&drift_num_permutations=10&drift_case_id_attribute=case&drift_seed=1"),
        " |  | 38");
  httplib::MultipartFormDataItems xes = {{"log", kXes, "cv1.xes", "application/xml"},
                                         {"include_traces", "false", "", ""}};
  mined = client.Post("/api/mine", xes);
  check("mine the XES", mined ? std::to_string(mined->status) : "no response", "200");
  check("XES, file order shuffled",
        drift("drift_sub_log_size=2&drift_window_size=2&drift_num_permutations=50&drift_threshold=0.5&drift_seed=0"),
        " |  | 38");
  check("XES, float attribute with gaps",
        drift("drift_sub_log_size=2&drift_window_size=1&drift_num_permutations=10&drift_activity_attribute=flt&drift_seed=0"),
        "ERR \"TypeError: sequence item 0: expected str instance, numpy.float64 found\"}");
  check("XES, int attribute",
        drift("drift_sub_log_size=2&drift_window_size=1&drift_num_permutations=10&drift_activity_attribute=num&drift_seed=0"),
        "ERR \"TypeError: sequence item 0: expected str instance, numpy.int64 found\"}");
  // Review FX1-2: an unnamed trace is a NaN case id in read_xes's frame, which
  // np.unique's stable object sort compares with a named one (TypeError,
  // operands in that sort's order); with every trace unnamed the frame has
  // no case column (KeyError). pm4py 2.7.22 black-box (review-harness
  // round-fx, dr/).
  const auto trace = [](const char* name, int minute) {
    std::string out = "<trace>";
    if (name) out += std::string("<string key=\"concept:name\" value=\"") + name + "\"/>";
    for (const char* activity : {"a", "b"})
      out += std::string("<event><string key=\"concept:name\" value=\"") + activity +
             "\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:" + (minute < 10 ? "0" : "") +
             std::to_string(minute++) + ":00.000+00:00\"/></event>";
    return out + "</trace>";
  };
  const auto unnamed = [&](std::initializer_list<const char*> names) {
    std::string log = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n";
    int minute = 0;
    for (const auto* name : names) { log += trace(name, minute); minute += 3; }
    log += "\n</log>\n";
    httplib::MultipartFormDataItems items = {{"log", log, "fx2.xes", "application/xml"}, {"include_traces", "false", "", ""}};
    const auto result = client.Post("/api/mine", items);
    if (!result || result->status != 200) return std::string("mine failed");
    return drift("drift_seed=0");
  };
  check("XES, named then unnamed trace", unnamed({"c1", nullptr, "c2"}),
        "ERR \"TypeError: '<' not supported between instances of 'str' and 'float'\"}");
  check("XES, two named then an unnamed trace", unnamed({"c2", "c1", nullptr}),
        "ERR \"TypeError: '<' not supported between instances of 'float' and 'str'\"}");
  check("XES, unnamed then named trace", unnamed({nullptr, "c1"}),
        "ERR \"TypeError: '<' not supported between instances of 'float' and 'str'\"}");
  check("XES, every trace unnamed", unnamed({nullptr, nullptr}), "ERR \"KeyError: 'case:concept:name'\"}");
  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures != 0) {
    std::cerr << g_failures << " concept drift upload check(s) failed\n";
    return 1;
  }
  std::cout << "review CV1: concept drift on upload frames passed\n";
  return 0;
}
