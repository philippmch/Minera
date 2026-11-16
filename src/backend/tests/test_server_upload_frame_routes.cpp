#undef NDEBUG
// Review CW1: the upload routes rework, boundary activities, cycle time and
// chaotic activities read the upload's frame as pm4py's DataFrame paths do
// (format_dataframe over read_csv, or read_xes): get_rework_cases_per_activity
// checks the case column (missing, then not str, then gaps) before the
// activity column and counts groups of the case column; the rework cases
// module raises KeyError for a missing case or activity column and for any
// case key other than case:concept:name, and skips NaN activity cells; start
// and end activities fail on a column the frame lacks; DataFrame cycle time
// defaults its start key to the timestamp key and selects the set of the two
// columns; chaotic activities read the frame's typed column (KeyError when it
// lacks it, each NaN cell an activity of its own). Every expected value was
// checked against pm4py 2.7.22 black-box on the same uploads (review-harness
// round-cw, ct/ct.py).
//
// Usage: minera_server_upload_frame_routes_tests <minera_server>

#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
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

// The CV1 uploads (test_server_concept_drift_upload.cpp).
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

// The raw JSON value of `"name":` in `body` (up to the comma or bracket that
// closes it), or "<missing name>".
std::string field(const std::string& body, const std::string& name) {
  const auto at = body.find("\"" + name + "\":");
  if (at == std::string::npos) return "<missing " + name + ">";
  auto begin = at + name.size() + 3;
  while (begin < body.size() && body[begin] == ' ') ++begin;
  int depth = 0;
  auto end = begin;
  for (; end < body.size(); ++end) {
    const char c = body[end];
    if (c == '{' || c == '[') ++depth;
    else if (c == '}' || c == ']') {
      if (depth == 0) break;
      --depth;
    } else if (c == ',' && depth == 0) break;
    else if (c == '"')
      for (++end; end < body.size() && body[end] != '"'; end += body[end] == '\\' ? 2 : 1) {
      }
  }
  return body.substr(begin, end - begin);
}

std::string outcome(const std::string& path, const httplib::Result& result) {
  if (!result) return "no response";
  const auto& body = result->body;
  if (result->status != 200) return "ERR " + field(body, "error");
  if (path.starts_with("rework?rework_variant=activities")) return field(body, "activity_cases");
  if (path.starts_with("rework?")) return field(body, "cases");
  if (path.starts_with("boundary-activities?")) return field(body, "starts") + " | " + field(body, "ends");
  if (path.starts_with("chaotic-activities?")) {
    // label:frequency pairs, sorted (the scores' last bits and the order of
    // near ties follow the reference's set order; the sweep checks those).
    std::vector<std::string> pairs;
    for (auto at = body.find("{\"activity\":"); at != std::string::npos; at = body.find("{\"activity\":", at + 1)) {
      const auto row = body.substr(at, body.find('}', at) - at);
      auto label = field(row, "activity");
      if (label.size() >= 2 && label.front() == '"') label = label.substr(1, label.size() - 2);
      pairs.push_back(label + ":" + field(row, "frequency"));
    }
    std::sort(pairs.begin(), pairs.end());
    std::string out;
    for (const auto& pair : pairs) out += (out.empty() ? "" : ",") + pair;
    return out;
  }
  return field(body, "cycle_time");
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
  const auto route = [&](const std::string& path) {
    return outcome(path, client.Post("/api/" + path, "", "text/plain"));
  };
  httplib::MultipartFormDataItems csv = {{"log", kCsv, "cw1.csv", "text/csv"},
                                         {"include_traces", "false", "", ""},
                                         {"case_column", "case", "", ""},
                                         {"activity_column", "act", "", ""},
                                         {"timestamp_column", "ts", "", ""}};
  auto mined = client.Post("/api/mine", csv);
  check("mine the CSV", mined ? std::to_string(mined->status) : "no response", "200");
  check("CSV rework?rework_variant=activities", route("rework?rework_variant=activities"), "{\"b\":1}");
  check("CSV rework?rework_variant=activities&rework_activity_key=num", route("rework?rework_variant=activities&rework_activity_key=num"), "ERR \"the activity column should be of type string.\"");
  check("CSV rework?rework_variant=activities&rework_case_id_key=concept%3Aname&rework_activity_key=case%3Aconcept%3Aname", route("rework?rework_variant=activities&rework_case_id_key=concept%3Aname&rework_activity_key=case%3Aconcept%3Aname"), "{\"3\":1}");
  check("CSV rework?rework_variant=activities&rework_case_id_key=num", route("rework?rework_variant=activities&rework_case_id_key=num"), "ERR \"the case ID column should be of type string.\"");
  check("CSV rework?rework_variant=cases", route("rework?rework_variant=cases"), "{\"1\":{\"number_activities\":3,\"rework\":0},\"10\":{\"number_activities\":3,\"rework\":0},\"11\":{\"number_activities\":3,\"rework\":0},\"12\":{\"number_activities\":3,\"rework\":0},\"2\":{\"number_activities\":3,\"rework\":0},\"3\":{\"number_activities\":4,\"rework\":1},\"4\":{\"number_activities\":3,\"rework\":0},\"5\":{\"number_activities\":3,\"rework\":0},\"6\":{\"number_activities\":3,\"rework\":0},\"7\":{\"number_activities\":3,\"rework\":0},\"8\":{\"number_activities\":3,\"rework\":0},\"9\":{\"number_activities\":4,\"rework\":0}}");
  check("CSV rework?rework_variant=cases&rework_activity_key=flt", route("rework?rework_variant=cases&rework_activity_key=flt"), "{\"1\":{\"number_activities\":3,\"rework\":0},\"10\":{\"number_activities\":2,\"rework\":0},\"11\":{\"number_activities\":2,\"rework\":0},\"12\":{\"number_activities\":2,\"rework\":0},\"2\":{\"number_activities\":2,\"rework\":0},\"3\":{\"number_activities\":3,\"rework\":0},\"4\":{\"number_activities\":2,\"rework\":0},\"5\":{\"number_activities\":3,\"rework\":0},\"6\":{\"number_activities\":2,\"rework\":0},\"7\":{\"number_activities\":2,\"rework\":0},\"8\":{\"number_activities\":2,\"rework\":0},\"9\":{\"number_activities\":3,\"rework\":0}}");
  check("CSV rework?rework_variant=cases&rework_case_id_key=case", route("rework?rework_variant=cases&rework_case_id_key=case"), "ERR \"KeyError: 'case:concept:name'\"");
  check("CSV rework?rework_variant=cases&rework_activity_key=act", route("rework?rework_variant=cases&rework_activity_key=act"), "{\"1\":{\"number_activities\":3,\"rework\":0},\"10\":{\"number_activities\":3,\"rework\":0},\"11\":{\"number_activities\":3,\"rework\":0},\"12\":{\"number_activities\":3,\"rework\":0},\"2\":{\"number_activities\":3,\"rework\":0},\"3\":{\"number_activities\":4,\"rework\":1},\"4\":{\"number_activities\":3,\"rework\":0},\"5\":{\"number_activities\":3,\"rework\":0},\"6\":{\"number_activities\":3,\"rework\":0},\"7\":{\"number_activities\":3,\"rework\":0},\"8\":{\"number_activities\":3,\"rework\":0},\"9\":{\"number_activities\":4,\"rework\":0}}");
  check("CSV boundary-activities?", route("boundary-activities?"), "{\"a\":12} | {\"c\":5,\"d\":5,\"b\":2}");
  check("CSV boundary-activities?boundary_activities_activity_key=num", route("boundary-activities?boundary_activities_activity_key=num"), "ERR \"the activity column should be of type string.\"");
  check("CSV boundary-activities?boundary_activities_activity_key=act", route("boundary-activities?boundary_activities_activity_key=act"), "{\"a\":12} | {\"c\":5,\"d\":5,\"b\":2}");
  check("CSV boundary-activities?boundary_activities_activity_key=case%3Aconcept%3Aname", route("boundary-activities?boundary_activities_activity_key=case%3Aconcept%3Aname"), "{\"1\":1,\"10\":1,\"11\":1,\"12\":1,\"2\":1,\"3\":1,\"4\":1,\"5\":1,\"6\":1,\"7\":1,\"8\":1,\"9\":1} | {\"1\":1,\"10\":1,\"11\":1,\"12\":1,\"2\":1,\"3\":1,\"4\":1,\"5\":1,\"6\":1,\"7\":1,\"8\":1,\"9\":1}");
  check("CSV cycle-time?", route("cycle-time?"), "0");
  check("CSV cycle-time?cycle_time_timestamp_key=ts", route("cycle-time?cycle_time_timestamp_key=ts"), "0");
  check("CSV cycle-time?cycle_time_start_timestamp_key=ts", route("cycle-time?cycle_time_start_timestamp_key=ts"), "0");
  check("CSV cycle-time?cycle_time_timestamp_key=num", route("cycle-time?cycle_time_timestamp_key=num"), "ERR \"AttributeError: 'int' object has no attribute 'timestamp'\"");
  check("CSV chaotic-activities?", route("chaotic-activities?"), "a:12,b:8,c:12,d:6");
  check("CSV chaotic-activities?chaotic_activity_key=num", route("chaotic-activities?chaotic_activity_key=num"), "100:1,101:1,102:1,10:1,110:1,111:1,112:1,11:1,120:1,121:1,122:1,12:1,20:1,21:1,22:1,30:1,31:1,32:1,33:1,40:1,41:1,42:1,50:1,51:1,52:1,60:1,61:1,62:1,70:1,71:1,72:1,80:1,81:1,82:1,90:1,91:1,92:1,93:1");
  check("CSV chaotic-activities?chaotic_activity_key=flt", route("chaotic-activities?chaotic_activity_key=flt"), "1.5:1,10.5:1,11.5:1,12.5:1,13.0:1,13.5:1,14.0:1,14.5:1,15.0:1,15.5:1,16.5:1,17.5:1,18.5:1,19.0:1,2.0:1,2.5:1,3.0:1,3.5:1,4.5:1,5.5:1,6.0:1,6.5:1,7.0:1,7.5:1,8.0:1,8.5:1,9.0:1,9.5:1,nan:1,nan:1,nan:1,nan:1,nan:1,nan:1,nan:1,nan:1,nan:1,nan:1");
  check("CSV chaotic-activities?chaotic_activity_key=act", route("chaotic-activities?chaotic_activity_key=act"), "a:12,b:8,c:12,d:6");
  httplib::MultipartFormDataItems xes = {{"log", kXes, "cw1.xes", "application/xml"},
                                         {"include_traces", "false", "", ""}};
  mined = client.Post("/api/mine", xes);
  check("mine the XES", mined ? std::to_string(mined->status) : "no response", "200");
  check("XES rework?rework_variant=activities", route("rework?rework_variant=activities"), "{\"b\":1}");
  check("XES rework?rework_variant=activities&rework_activity_key=num", route("rework?rework_variant=activities&rework_activity_key=num"), "ERR \"the activity column should be of type string.\"");
  check("XES rework?rework_variant=activities&rework_case_id_key=concept%3Aname&rework_activity_key=case%3Aconcept%3Aname", route("rework?rework_variant=activities&rework_case_id_key=concept%3Aname&rework_activity_key=case%3Aconcept%3Aname"), "{\"c03\":1}");
  check("XES rework?rework_variant=activities&rework_case_id_key=num", route("rework?rework_variant=activities&rework_case_id_key=num"), "ERR \"the case ID column should be of type string.\"");
  check("XES rework?rework_variant=cases", route("rework?rework_variant=cases"), "{\"c01\":{\"number_activities\":3,\"rework\":0},\"c02\":{\"number_activities\":3,\"rework\":0},\"c03\":{\"number_activities\":4,\"rework\":1},\"c04\":{\"number_activities\":3,\"rework\":0},\"c05\":{\"number_activities\":3,\"rework\":0},\"c06\":{\"number_activities\":3,\"rework\":0},\"c07\":{\"number_activities\":3,\"rework\":0},\"c08\":{\"number_activities\":3,\"rework\":0},\"c09\":{\"number_activities\":4,\"rework\":0},\"c10\":{\"number_activities\":3,\"rework\":0},\"c11\":{\"number_activities\":3,\"rework\":0},\"c12\":{\"number_activities\":3,\"rework\":0}}");
  check("XES rework?rework_variant=cases&rework_activity_key=flt", route("rework?rework_variant=cases&rework_activity_key=flt"), "{\"c01\":{\"number_activities\":3,\"rework\":0},\"c02\":{\"number_activities\":2,\"rework\":0},\"c03\":{\"number_activities\":3,\"rework\":0},\"c04\":{\"number_activities\":2,\"rework\":0},\"c05\":{\"number_activities\":3,\"rework\":0},\"c06\":{\"number_activities\":2,\"rework\":0},\"c07\":{\"number_activities\":2,\"rework\":0},\"c08\":{\"number_activities\":2,\"rework\":0},\"c09\":{\"number_activities\":3,\"rework\":0},\"c10\":{\"number_activities\":2,\"rework\":0},\"c11\":{\"number_activities\":2,\"rework\":0},\"c12\":{\"number_activities\":2,\"rework\":0}}");
  check("XES rework?rework_variant=cases&rework_case_id_key=case", route("rework?rework_variant=cases&rework_case_id_key=case"), "ERR \"KeyError: 'case'\"");
  check("XES rework?rework_variant=cases&rework_activity_key=act", route("rework?rework_variant=cases&rework_activity_key=act"), "ERR \"KeyError: 'Column not found: act'\"");
  check("XES boundary-activities?", route("boundary-activities?"), "{\"a\":12} | {\"c\":5,\"d\":5,\"b\":2}");
  check("XES boundary-activities?boundary_activities_activity_key=num", route("boundary-activities?boundary_activities_activity_key=num"), "ERR \"the activity column should be of type string.\"");
  check("XES boundary-activities?boundary_activities_activity_key=act", route("boundary-activities?boundary_activities_activity_key=act"), "ERR \"the specified activity column is not contained in the dataframe. Available columns: ['case:concept:name', 'concept:name', 'flt', 'num', 'time:timestamp']\"");
  check("XES boundary-activities?boundary_activities_activity_key=case%3Aconcept%3Aname", route("boundary-activities?boundary_activities_activity_key=case%3Aconcept%3Aname"), "{\"c02\":1,\"c08\":1,\"c11\":1,\"c01\":1,\"c07\":1,\"c12\":1,\"c05\":1,\"c06\":1,\"c03\":1,\"c09\":1,\"c10\":1,\"c04\":1} | {\"c02\":1,\"c08\":1,\"c11\":1,\"c01\":1,\"c07\":1,\"c12\":1,\"c05\":1,\"c06\":1,\"c03\":1,\"c09\":1,\"c10\":1,\"c04\":1}");
  check("XES cycle-time?", route("cycle-time?"), "0");
  check("XES cycle-time?cycle_time_timestamp_key=ts", route("cycle-time?cycle_time_timestamp_key=ts"), "ERR \"KeyError: \\\"None of [Index(['ts'], dtype='str')] are in the [columns]\\\"\"");
  check("XES cycle-time?cycle_time_start_timestamp_key=ts", route("cycle-time?cycle_time_start_timestamp_key=ts"), "ERR \"KeyError: \\\"['ts'] not in index\\\"\"");
  check("XES cycle-time?cycle_time_timestamp_key=num", route("cycle-time?cycle_time_timestamp_key=num"), "ERR \"AttributeError: 'int' object has no attribute 'timestamp'\"");
  check("XES chaotic-activities?", route("chaotic-activities?"), "a:12,b:8,c:12,d:6");
  check("XES chaotic-activities?chaotic_activity_key=num", route("chaotic-activities?chaotic_activity_key=num"), "100:1,101:1,102:1,10:1,110:1,111:1,112:1,11:1,120:1,121:1,122:1,12:1,20:1,21:1,22:1,30:1,31:1,32:1,33:1,40:1,41:1,42:1,50:1,51:1,52:1,60:1,61:1,62:1,70:1,71:1,72:1,80:1,81:1,82:1,90:1,91:1,92:1,93:1");
  check("XES chaotic-activities?chaotic_activity_key=flt", route("chaotic-activities?chaotic_activity_key=flt"), "1.5:1,10.5:1,11.5:1,12.5:1,13.0:1,13.5:1,14.0:1,14.5:1,15.0:1,15.5:1,16.5:1,17.5:1,18.5:1,19.0:1,2.0:1,2.5:1,3.0:1,3.5:1,4.5:1,5.5:1,6.0:1,6.5:1,7.0:1,7.5:1,8.0:1,8.5:1,9.0:1,9.5:1,nan:1,nan:1,nan:1,nan:1,nan:1,nan:1,nan:1,nan:1,nan:1,nan:1");
  check("XES chaotic-activities?chaotic_activity_key=act", route("chaotic-activities?chaotic_activity_key=act"), "ERR \"KeyError: 'act'\"");
  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures != 0) {
    std::cerr << g_failures << " upload frame route check(s) failed\n";
    return 1;
  }
  std::cout << "review CW1: rework, boundary activities, cycle time and chaotic activities on upload frames passed\n";
  return 0;
}
