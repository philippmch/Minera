#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

struct WorkflowHistoryRow {
  std::string process_id;
  std::string case_id;
  std::string activity;
  std::string timestamp;
  std::string resource;
};

struct WorkflowHistoryOptions {
  std::size_t max_events = 1'000'000;
};

[[nodiscard]] EventLog import_workflow_history(
    const std::vector<WorkflowHistoryRow>& rows,
    const WorkflowHistoryOptions& options = {});
[[nodiscard]] std::string workflow_history_json(const EventLog& log);
[[nodiscard]] EventLog import_workflow_history_csv(
    std::string_view csv, const WorkflowHistoryOptions& options = {});

}  // namespace minera
