#include "minera/ocel_csv.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
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

  minera::OcelCsvImportOptions custom_import;
  custom_import.event_id_column = "event_id";
  custom_import.activity_column = "activity_name";
  custom_import.timestamp_column = "occurred_at";
  custom_import.object_type_prefix = "ref.";
  custom_import.object_id_column = "object_id";
  custom_import.object_type_column = "object_kind";
  custom_import.delimiter = ';';
  const auto custom = minera::parse_ocel_csv(
      "event_id;activity_name;occurred_at;ref.item;score\n"
      " e1 ; Pick ; 2024-01-01T00:00:00Z ; [i1] ; 4 \n",
      "object_id;object_kind;state\n"
      "i1;item;ready\n",
      custom_import);
  assert(custom.events.size() == 1 && custom.events[0].id == "e1");
  assert(custom.events[0].activity == "Pick");
  assert(custom.objects.size() == 1 && custom.objects[0].id == "i1");
  assert(custom.relations.size() == 1);

  minera::OcelCsvExportOptions custom_export;
  custom_export.event_id_column = custom_import.event_id_column;
  custom_export.activity_column = custom_import.activity_column;
  custom_export.timestamp_column = custom_import.timestamp_column;
  custom_export.object_type_prefix = custom_import.object_type_prefix;
  custom_export.object_id_column = custom_import.object_id_column;
  custom_export.object_type_column = custom_import.object_type_column;
  custom_export.delimiter = custom_import.delimiter;
  custom_export.allow_lossy = true;
  const auto custom_events = minera::serialize_ocel_csv(custom, custom_export);
  const auto custom_objects = minera::serialize_ocel_objects_csv(custom, custom_export);
  assert(custom_events.rfind("event_id;activity_name;occurred_at;ref.item", 0) == 0);
  assert(custom_objects.rfind("object_id;object_kind;state", 0) == 0);
  const auto custom_roundtrip =
      minera::parse_ocel_csv(custom_events, custom_objects, custom_import);
  assert(custom_roundtrip.events[0].id == "e1");
  assert(custom_roundtrip.objects[0].type == "item");

  throws([&] {
    auto invalid = custom_import;
    invalid.delimiter = '"';
    (void)minera::parse_ocel_csv("event_id;activity_name;occurred_at\n", invalid);
  });
  throws([&] {
    auto invalid = custom_import;
    invalid.delimiter = '\n';
    (void)minera::parse_ocel_csv("event_id;activity_name;occurred_at\n", invalid);
  });

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

  // A two-file export must not accept suppressed object output when object
  // attributes would otherwise disappear without an error.
  const auto event_path = std::filesystem::temp_directory_path() /
                          "minera_ocel_csv_emit_objects_false_events.csv";
  const auto object_path = std::filesystem::temp_directory_path() /
                           "minera_ocel_csv_emit_objects_false_objects.csv";
  std::filesystem::remove(event_path);
  std::filesystem::remove(object_path);
  auto suppressed_objects = minera::OcelCsvExportOptions{};
  suppressed_objects.emit_objects = false;
  throws([&] {
    minera::export_ocel_csv(log, event_path, object_path, suppressed_objects);
  });
  std::filesystem::remove(event_path);
  std::filesystem::remove(object_path);

  return 0;
}
