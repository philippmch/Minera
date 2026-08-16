#include "minera/ocel_csv.hpp"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

template <typename F>
void throws(F&& function) {
  bool rejected = false;
  try {
    function();
  } catch (const std::exception&) {
    rejected = true;
  }
  assert(rejected);
}

}  // namespace

int main() {
  const std::string events =
      "ocel:eid,ocel:activity,ocel:timestamp,ocel:type:item,ocel:type:order,amount,note\n"
      "e2,Ship,2024-01-02T00:00:00Z,\"['i2','i1']\",[o1],2.5,\"quoted, value\"\n"
      "e1,Create,2024-01-01T00:00:00Z,[],[o1],2,\n";
  const std::string objects =
      "ocel:oid,ocel:type,status\n"
      "o1,order,open\n"
      "i1,item,ready\n"
      "i2,item,\"in use\"\n";

  const auto log = minera::parse_ocel_csv(events, objects);
  assert(log.events.size() == 2);
  assert(log.events[0].id == "e1" && log.events[1].id == "e2");
  assert(std::get<std::int64_t>(log.events[0].attributes[0].value) == 2);
  assert(std::get<double>(log.events[1].attributes[0].value) == 2.5);
  assert(log.events[1].attributes[1].key == "note");
  assert(std::get<std::string>(log.events[1].attributes[1].value) ==
         "quoted, value");
  assert(log.objects.size() == 3);
  assert(log.relations.size() == 4);
  assert(log.objects[0].id == "i1" && log.objects[0].type == "item");
  assert(std::get<std::string>(log.objects[0].attributes[0].value) == "ready");

  minera::OcelCsvExportOptions event_only_options;
  event_only_options.allow_lossy = true;
  const auto exported_events = minera::serialize_ocel_csv(log, event_only_options);
  const auto exported_objects = minera::serialize_ocel_objects_csv(log);
  assert(exported_events == minera::serialize_ocel_csv(log, event_only_options));
  const auto round = minera::parse_ocel_csv(exported_events, exported_objects);
  assert(round.events.size() == log.events.size());
  assert(round.objects.size() == log.objects.size());
  assert(round.relations.size() == log.relations.size());
  assert(round.events[1].id == "e2");
  assert(std::get<double>(round.events[1].attributes[0].value) == 2.5);
  assert(std::get<std::string>(round.objects[2].attributes[0].value) ==
         "open");

  const std::string synthesized =
      "ocel:eid,ocel:activity,ocel:timestamp,ocel:type:order\n"
      "e1,A,2024-01-01T00:00:00Z,[o9]\n";
  const auto synthesized_log = minera::parse_ocel_csv(synthesized);
  assert(synthesized_log.objects.size() == 1 &&
         synthesized_log.objects[0].id == "o9" &&
         synthesized_log.objects[0].type == "order");

  throws([&] {
    (void)minera::parse_ocel_csv(
        "ocel:eid,ocel:eid,ocel:activity,ocel:timestamp\n");
  });
  throws([&] {
    (void)minera::parse_ocel_csv(
        "ocel:eid,ocel:activity,ocel:timestamp,ocel:type:order\n"
        "e1,A,2024-01-01T00:00:00Z,[o1\n");
  });
  throws([&] {
    (void)minera::parse_ocel_csv(
        "ocel:eid,ocel:activity,ocel:timestamp,ocel:type:order,ocel:type:item\n"
        "e1,A,2024-01-01T00:00:00Z,[o1],[o1]\n");
  });
  throws([&] {
    minera::Ocel invalid;
    invalid.objects.push_back({"o1", "order", {}});
    invalid.events.push_back({"e1", "A", "2024-01-01T00:00:00Z", {}});
    invalid.relations.push_back({"e1", "o1", "qualified"});
    (void)minera::serialize_ocel_csv(invalid);
  });
  throws([&] {
    minera::Ocel invalid;
    invalid.objects.push_back({"o1", "order", {{"status", std::string("open")}}});
    invalid.events.push_back({"e1", "A", "2024-01-01T00:00:00Z", {}});
    invalid.relations.push_back({"e1", "o1", ""});
    (void)minera::serialize_ocel_csv(invalid);
  });
  throws([&] {
    auto invalid = log;
    invalid.e2e.push_back({"e1", "e2", "precedes"});
    (void)minera::serialize_ocel_csv(invalid);
  });

  return 0;
}
