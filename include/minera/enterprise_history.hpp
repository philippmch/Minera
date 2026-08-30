#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "minera/event_log.hpp"

namespace minera {

enum class EnterpriseHistoryVariant { kOrderToCash, kAccounting };

struct EnterpriseHistoryOptions {
  EnterpriseHistoryVariant variant = EnterpriseHistoryVariant::kOrderToCash;
  std::size_t max_events = 1'000'000;
};

[[nodiscard]] EventLog import_enterprise_history_csv(
    std::string_view csv, const EnterpriseHistoryOptions& options = {});
[[nodiscard]] std::string enterprise_history_json(
    const EventLog& log, EnterpriseHistoryVariant variant);

}  // namespace minera
