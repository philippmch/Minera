#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

#include "minera/event_log.hpp"

namespace minera {

enum class BrowserHistoryVariant {
  kChrome,
  kFirefox,
  CHROME_HISTORY = kChrome,
  FIREFOX_HISTORY = kFirefox,
};

struct BrowserHistoryOptions {
  BrowserHistoryVariant variant = BrowserHistoryVariant::kChrome;
  // An explicit database file or profile-root directory is mandatory. Minera
  // never scans a user's browser profile implicitly.
  std::filesystem::path history_path;
  std::size_t max_events = 1'000'000;
};

// Reads browser history databases without launching or controlling a browser.
// Directory input discovers profile databases one level below the supplied
// root; file input uses the deterministic case id "DEFAULT".
[[nodiscard]] EventLog import_browser_history(
    const BrowserHistoryOptions& options);

[[nodiscard]] std::string browser_history_json(
    const EventLog& log, BrowserHistoryVariant variant);

}  // namespace minera
