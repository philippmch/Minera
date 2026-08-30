#undef NDEBUG
#include <cassert>
#include <stdexcept>
#include "minera/enterprise_history.hpp"
namespace { bool rejects(const auto& fn) { try { fn(); } catch (const std::invalid_argument&) { return true; } return false; } }
int main() {
  const auto order = minera::import_enterprise_history_csv(
      "case_id,activity,timestamp,resource\nO1,Create Sales Document,2024-01-01T09:00:00Z,alice\nO1,Create Delivery,2024-01-02T09:00:00Z,bob\n");
  assert(order.trace_count() == 1 && order.event_count() == 2);
  assert(order.traces[0].events[1].activity == "Create Delivery");
  minera::EnterpriseHistoryOptions options; options.variant = minera::EnterpriseHistoryVariant::kAccounting;
  const auto accounting = minera::import_enterprise_history_csv(
      "case_id,activity,timestamp,resource,document_type\n1000-2024-7,Create Financial Document,2024-01-01T09:00:00Z,alice,SA\n1000-2024-7,Clear Vendor Document,2024-01-02T23:59:59Z,,\n", options);
  assert(*accounting.traces[0].events[0].attribute("document_type") == "SA");
  assert(minera::enterprise_history_json(accounting, options.variant).find("\"variant\":\"accounting\"") != std::string::npos);
  options.max_events = 1;
  assert(rejects([&] { (void)minera::import_enterprise_history_csv("case_id,activity,timestamp,resource\na,A,1,r\na,B,2,r\n", options); }));
}
