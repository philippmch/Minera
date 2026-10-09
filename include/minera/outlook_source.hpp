#pragma once
#include <cstddef>
#include <string>
#include "minera/communication_history.hpp"
#ifdef _WIN32
struct IDispatch;
#endif

namespace minera {
struct OutlookSourceOptions {
  CommunicationHistoryOptions history;
  std::string email_user;
  unsigned calendar_id = 9;
};
struct OutlookSourceCapture {
  EventLog log;
  std::string connector, email_user;
  unsigned calendar_id = 9;
  std::size_t total_items = 0, examined_items = 0, skipped_items = 0;
  bool limited = false;
  std::string item_table_json, formatted_table_json;
};
// The current Windows account's Outlook COM profile is accessed only on demand.
[[nodiscard]] OutlookSourceCapture capture_outlook_source(const OutlookSourceOptions& options);
#ifdef _WIN32
// Uses the same IDispatch extraction as the live entry point. Does not own or
// initialize the caller's COM apartment; the caller retains the root object.
[[nodiscard]] OutlookSourceCapture capture_outlook_source_with_dispatch(
    const OutlookSourceOptions& options, ::IDispatch* application);
#endif
[[nodiscard]] std::string outlook_source_capture_json(const OutlookSourceCapture& capture);
}  // namespace minera
