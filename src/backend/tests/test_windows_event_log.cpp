#undef NDEBUG

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "minera/windows_event_log.hpp"

namespace {
bool rejects(const auto& fn) {
  try { fn(); } catch (const std::invalid_argument&) { return true; }
  return false;
}
}  // namespace

int main() {
  const auto path = std::filesystem::temp_directory_path() /
                    "minera_windows_events.xml";
  {
    std::ofstream out(path, std::ios::binary);
    out << R"xml(<?xml version="1.0"?>
<Events xmlns="http://schemas.microsoft.com/win/2004/08/events/event">
 <Event><System><Provider Name="Service Control Manager"/><EventID>7045</EventID>
  <Level>4</Level><Task>0</Task><Opcode>0</Opcode><Keywords>0x808</Keywords>
  <TimeCreated SystemTime="2024-03-01T10:00:01.1234567Z"/>
  <EventRecordID>12</EventRecordID><Channel>System</Channel><Computer>HOST-B</Computer>
  <Security UserID="S-1-5-18"/></System>
  <EventData><Data Name="ServiceName">Demo &amp; Worker</Data><Data>automatic</Data></EventData>
  <RenderingInfo><Message>Installed &lt;service&gt;</Message></RenderingInfo></Event>
 <Event><System><Provider Name="Kernel"/><EventID Qualifiers="0">1</EventID>
  <TimeCreated SystemTime="2024-03-01T09:00:00Z"/><Computer>HOST-A</Computer></System></Event>
 <Event><System><Provider Name="Kernel"/><EventID>2</EventID>
  <TimeCreated SystemTime="2024-03-01T08:00:00Z"/><Computer>HOST-B</Computer></System></Event>
</Events>)xml";
  }
  minera::WindowsEventLogOptions options;
  options.path = path;
  const auto log = minera::import_windows_event_log(options);
  assert(log.trace_count() == 2 && log.event_count() == 3);
  assert(log.traces[0].case_id == "HOST-B");
  assert(log.traces[0].events[0].activity == "Kernel 2");
  assert(log.traces[0].events[1].activity == "Service Control Manager 7045");
  assert(log.traces[0].events[1].resource == "S-1-5-18");
  assert(*log.traces[0].events[1].attribute("data:ServiceName") ==
         "Demo & Worker");
  assert(*log.traces[0].events[1].attribute("data_1") == "automatic");
  assert(*log.traces[0].events[1].attribute("message") ==
         "Installed <service>");
  const auto json = minera::windows_event_log_json(log);
  assert(json.find("\"type\":\"WINDOWS_EVENTS\"") != std::string::npos);
  assert(json.find("Demo & Worker") != std::string::npos);

  options.max_events = 2;
  assert(rejects([&] { (void)minera::import_windows_event_log(options); }));
  options.max_events = 1;
  options.path = path.string() + ".missing";
  assert(rejects([&] { (void)minera::import_windows_event_log(options); }));
  std::error_code error;
  std::filesystem::remove(path, error);
}
