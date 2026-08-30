#undef NDEBUG
#include <cassert>
#include <stdexcept>
#include <vector>
#include "minera/workflow_history.hpp"

namespace { bool rejects(const auto& fn) { try { fn(); } catch (const std::invalid_argument&) { return true; } return false; } }

int main() {
  const std::vector<minera::WorkflowHistoryRow> rows = {
      {"invoice", "c2", "approve", "2024-02-02T10:00:00Z", "bob"},
      {"order", "c1", "ship", "2024-02-01T11:00:00Z", "carol"},
      {"order", "c1", "create", "2024-02-01T09:00:00Z", "alice"}};
  const auto log = minera::import_workflow_history(rows);
  assert(log.trace_count() == 2 && log.event_count() == 3);
  assert(log.traces[0].case_id == "c2" && *log.traces[0].attribute("case:process_id") == "invoice");
  assert(log.traces[1].events[0].activity == "create" && log.traces[1].events[1].activity == "ship");
  assert(minera::workflow_history_json(log).find("\"type\":\"WORKFLOW_HISTORY\"") != std::string::npos);
  const auto csv_log = minera::import_workflow_history_csv(
      "process_id,case_id,activity,timestamp,resource\norder,c1,create,2024-02-01T09:00:00Z,alice\n");
  assert(csv_log.event_count() == 1 && csv_log.traces[0].events[0].resource == "alice");
  minera::WorkflowHistoryOptions options; options.max_events = 2;
  assert(rejects([&] { (void)minera::import_workflow_history(rows, options); }));
  auto invalid = rows; invalid[1].case_id.clear();
  assert(rejects([&] { (void)minera::import_workflow_history(invalid); }));
  invalid = rows; invalid[2].process_id = "other";
  assert(rejects([&] { (void)minera::import_workflow_history(invalid); }));
}
