#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "minera/event_log.hpp"

namespace minera {

enum class CommunicationHistoryVariant { kMailbox, kCalendar };
struct CommunicationHistoryOptions {
  CommunicationHistoryVariant variant = CommunicationHistoryVariant::kMailbox;
  bool include_reminders = true;
  std::string current_time;
  std::size_t max_items = 1'000'000;
  std::size_t max_events = 5'000'000;
};
[[nodiscard]] EventLog import_communication_history_csv(
    std::string_view csv, const CommunicationHistoryOptions& options = {});
[[nodiscard]] std::string communication_history_json(
    const EventLog& log, CommunicationHistoryVariant variant);

}  // namespace minera
