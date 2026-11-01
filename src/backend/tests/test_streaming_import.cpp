#include "minera/streaming_import.hpp"

#include <atomic>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace {

std::filesystem::path write_file(const std::string& suffix,
                                  const std::string& content) {
  const auto path = std::filesystem::temp_directory_path() /
                    ("minera-streaming-reader-" + suffix);
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  assert(output);
  output << content;
  return path;
}

void csv_events_transform_filter_and_reset() {
  const auto path = write_file(
      "events.csv", "case_id,activity,timestamp\nc1,A,1\nc1,B,2\nc2,C,3\n");
  minera::StreamingEventReaderOptions options;
  options.transform = [](minera::Event event) {
    event.activity += "!";
    return event;
  };
  options.acceptance = [](const minera::Event& event) {
    return event.activity != "B!";
  };
  minera::CsvEventStreamReader reader(path, {}, options);
  auto first = reader.read_event();
  assert(first && first->activity == "A!");
  auto second = reader.read_event();
  assert(second && second->activity == "C!");
  assert(!reader.read_event());
  assert(reader.exhausted());
  reader.reset();
  assert(reader.read_event()->activity == "A!");
  std::filesystem::remove(path);
}

// The reference CSV event stream reader (csv.DictReader) yields every row in
// file order, with empty and pandas-NA-spelled cells as literal text; the
// batch importer drops such rows.
void csv_events_keep_na_spellings_in_file_order() {
  const std::string body =
      "case_id,activity,timestamp\nNA,A,1\nc2,B,2\nNA,NaN,3\n,C,4\nnull,,5\n";
  const auto path = write_file("na.csv", body);
  minera::CsvEventStreamReader reader(path);
  const std::pair<const char*, const char*> expected[] = {
      {"NA", "A"}, {"c2", "B"}, {"NA", "NaN"}, {"", "C"}, {"null", ""}};
  for (const auto& [case_id, activity] : expected) {
    const auto event = reader.read_event();
    assert(event && event->case_id == case_id && event->activity == activity);
    assert(event->has_case_id());
  }
  assert(!reader.read_event());
  std::istringstream batch(body);
  assert(minera::import_csv_event_log(batch).traces.size() == 1);  // only c2
  std::filesystem::remove(path);
}

void xes_event_and_trace_streams_deliver() {
  const auto path = write_file(
      "events.xes",
      "<log><trace><string key=\"concept:name\" value=\"c1\"/>"
      "<event><string key=\"concept:name\" value=\"A\"/></event>"
      "<event><string key=\"concept:name\" value=\"B\"/></event>"
      "</trace><trace><string key=\"concept:name\" value=\"c2\"/>"
      "<event><string key=\"concept:name\" value=\"C\"/></event>"
      "</trace></log>");

  minera::XesEventStreamReader events(path);
  minera::LiveEventStream event_stream(2);
  std::atomic<std::size_t> event_count = 0;
  const auto event_observer = event_stream.register_observer(
      [&event_count](const minera::Event&) { ++event_count; });
  event_stream.start();
  assert(events.to_event_stream(event_stream) == 3);
  event_stream.stop();
  assert(event_count == 3);
  assert(event_stream.deregister_observer(event_observer));

  minera::StreamingTraceReaderOptions options;
  options.acceptance = [](const minera::Trace& trace) {
    return trace.case_id == "c2";
  };
  minera::XesTraceStreamReader traces(path, {}, options);
  minera::LiveTraceStream trace_stream(1);
  std::atomic<std::size_t> trace_count = 0;
  const auto trace_observer = trace_stream.register_observer(
      [&trace_count](const minera::Trace& trace) {
        trace_count += trace.events.size();
      });
  trace_stream.start();
  assert(traces.to_trace_stream(trace_stream) == 1);
  trace_stream.stop();
  assert(trace_count == 1);
  assert(trace_stream.deregister_observer(trace_observer));
  traces.reset();
  assert(traces.read_trace()->case_id == "c2");
  std::filesystem::remove(path);
}

// The reference XES event stream gives each event its trace's attributes
// under "case:" keys; a trace value replaces an event's own "case:" key.
void xes_events_carry_trace_attributes() {
  const auto path = write_file(
      "case-attributes.xes",
      "<log><trace><string key=\"concept:name\" value=\"c1\"/>"
      "<string key=\"region\" value=\"north\"/>"
      "<event><string key=\"concept:name\" value=\"A\"/>"
      "<string key=\"case:region\" value=\"own\"/></event>"
      "</trace></log>");
  minera::XesEventStreamReader events(path);
  const auto event = events.read_event();
  assert(event);
  std::size_t regions = 0;
  for (const auto& attribute : event->attributes) {
    assert(attribute.key != "case:concept:name");
    if (attribute.key == "case:region") {
      ++regions;
      assert(attribute.value == "north");
    }
  }
  assert(regions == 1);
  std::filesystem::remove(path);
}

}  // namespace

int main() {
  xes_events_carry_trace_attributes();
  csv_events_transform_filter_and_reset();
  csv_events_keep_na_spellings_in_file_order();
  xes_event_and_trace_streams_deliver();
  return 0;
}
