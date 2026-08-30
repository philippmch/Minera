#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

#include "minera/event_log.hpp"

namespace minera {

struct WindowsEventLogOptions {
  // Explicitly exported XML only. The connector never queries the live event
  // service, so importing cannot request elevation, open management consoles,
  // or subscribe to machine-wide events.
  std::filesystem::path path;
  std::size_t max_events = 1'000'000;
};

[[nodiscard]] EventLog import_windows_event_log(
    const WindowsEventLogOptions& options);
[[nodiscard]] std::string windows_event_log_json(const EventLog& log);

}  // namespace minera
