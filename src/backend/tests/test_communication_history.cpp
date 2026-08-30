#undef NDEBUG
#include <cassert>
#include <stdexcept>
#include "minera/communication_history.hpp"
namespace { bool rejects(const auto& fn) { try { fn(); } catch (const std::invalid_argument&) { return true; } return false; } }
int main() {
  const auto mail = minera::import_communication_history_csv(
      "conversation_id,direction,item_class,created_at,sender,recipients,topic,subject\nc1,Sent,43,2024-01-01T09:00:00Z,Alice,Bob,Order,Hello\nc1,Received,56,2024-01-02T09:00:00Z,Bob,Alice,Order,Accepted\nc2,Sent,999,2024-01-03T09:00:00Z,X,Y,Z,Ignored\n");
  assert(mail.trace_count() == 1 && mail.event_count() == 2);
  assert(mail.traces[0].events[0].activity == "Sent Mail");
  assert(*mail.traces[0].events[1].attribute("subject") == "Accepted");
  minera::CommunicationHistoryOptions options; options.variant = minera::CommunicationHistoryVariant::kCalendar; options.current_time = "2024-02-01T00:00:00Z";
  const auto calendar = minera::import_communication_history_csv(
      "conversation_id,subject,created_at,last_changed_at,start_at,end_at,cancelled,reminder_at\nm1,Review,2024-01-01T08:00:00Z,2024-01-02T08:00:00Z,2024-01-03T09:00:00Z,2024-01-03T10:00:00Z,true,2024-01-03T08:45:00Z\n", options);
  assert(calendar.event_count() == 6);
  assert(calendar.traces[0].events[0].activity == "Meeting Created");
  assert(calendar.traces[0].events.back().activity == "Meeting Completed");
  assert(minera::communication_history_json(calendar, options.variant).find("\"variant\":\"calendar\"") != std::string::npos);
  options.max_events = 5;
  assert(rejects([&] { (void)minera::import_communication_history_csv("conversation_id,subject,created_at,last_changed_at,start_at,end_at,cancelled,reminder_at\nm1,x,1,2,3,4,true,5\n", options); }));
}
