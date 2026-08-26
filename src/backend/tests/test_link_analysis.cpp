#include "minera/link_analysis.hpp"

#include <cassert>
#include <stdexcept>

namespace {

minera::Event event(std::string case_id, std::string activity,
                    std::string timestamp, std::string out,
                    std::string in) {
  minera::Event result;
  result.case_id = std::move(case_id);
  result.activity = std::move(activity);
  result.timestamp = std::move(timestamp);
  result.attributes.push_back({"out", std::move(out)});
  result.attributes.push_back({"in", std::move(in)});
  return result;
}

minera::EventLog sample() {
  minera::EventLog log;
  minera::Trace first;
  first.case_id = "c1";
  first.events.push_back(event("c1", "A", "2024-01-01T00:00:00Z", "x", ""));
  first.events.push_back(event("c1", "B", "2024-01-01T00:00:01Z", "y", "x"));
  first.events.push_back(event("c1", "C", "2024-01-01T00:00:02Z", "", "y"));
  log.traces.push_back(std::move(first));
  minera::Trace second;
  second.case_id = "c2";
  second.events.push_back(event("c2", "D", "2023-12-31T23:59:59Z", "x", ""));
  second.events.push_back(event("c2", "E", "2024-01-01T00:00:03Z", "", "x"));
  log.traces.push_back(std::move(second));
  return log;
}

void classic_forward_and_sorting() {
  const auto result = minera::discover_link_analysis(sample());
  assert(result.event_count == 5);
  assert(result.links.size() == 4);
  assert(result.links[0].source.activity == "A" ||
         result.links[0].source.activity == "D");
  assert(result.links[0].target.activity == "B" ||
         result.links[0].target.activity == "E");
  assert(result.links[0].source.index < result.links[0].target.index);
  assert(result.options.index_column == "@@index");
  assert(minera::link_analysis_json(result).find("\"index_column\":\"@@index\"") !=
         std::string::npos);
}

void first_and_propagated_links() {
  auto log = sample();
  minera::LinkAnalysisOptions options;
  options.out_column = "out";
  options.in_column = "in";
  options.keep_first_occurrence = true;
  options.propagate = true;
  const auto result = minera::discover_link_analysis(log, options);
  assert(result.links.size() == 5);
  bool saw_a_c = false;
  for (const auto& link : result.links)
    if (link.source.activity == "A" && link.target.activity == "C") saw_a_c = true;
  assert(saw_a_c);
}

void look_backward_and_custom_trace_attribute() {
  auto log = sample();
  log.traces[0].attributes.push_back({"batch", "left"});
  log.traces[1].attributes.push_back({"batch", "right"});
  minera::LinkAnalysisOptions options;
  options.out_column = "batch";
  options.in_column = "batch";
  options.index_column = "row_id";
  options.look_forward = false;
  options.keep_first_occurrence = true;
  const auto result = minera::discover_link_analysis(log, options);
  assert(result.links.size() == result.event_count);
  assert(result.links.front().source.index == result.links.front().target.index);
  assert(result.options.index_column == "row_id");
  const auto json = minera::link_analysis_json(result);
  assert(json.find(",\"row_id\":") != std::string::npos);
  assert(json.find("\"index\":") != std::string::npos);
}

void event_index_is_trace_local_not_input_order() {
  auto log = sample();
  for (auto& trace : log.traces)
    for (auto& event : trace.events) event.input_order = 99;
  minera::LinkAnalysisOptions options;
  options.out_column = "event_index";
  options.in_column = "event_index";
  options.keep_first_occurrence = true;
  const auto result = minera::discover_link_analysis(log, options);
  assert(result.links.size() == 2);
  assert(result.links[0].source.event_index == 0 &&
         result.links[0].target.event_index == 0);
  assert(result.links[1].source.event_index == 1 &&
         result.links[1].target.event_index == 1);
}

void invalid_columns_and_bound() {
  auto log = sample();
  minera::LinkAnalysisOptions options;
  options.out_column = "missing";
  bool threw = false;
  try { (void)minera::discover_link_analysis(log, options); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  options = {};
  options.index_column.clear();
  threw = false;
  try { (void)minera::discover_link_analysis(log, options); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  options = {};
  options.index_column = "trace_index";
  threw = false;
  try { (void)minera::discover_link_analysis(log, options); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  options = {};
  options.max_links = 1;
  threw = false;
  try { (void)minera::discover_link_analysis(log, options); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  options = {};
  options.max_links = 3;
  threw = false;
  try { (void)minera::discover_link_analysis(log, options); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  assert(minera::link_analysis_json(minera::discover_link_analysis(sample()))
             .find("LINK_ANALYSIS") != std::string::npos);
}

}  // namespace

int main() {
  classic_forward_and_sorting();
  first_and_propagated_links();
  look_backward_and_custom_trace_attribute();
  event_index_is_trace_local_not_input_order();
  invalid_columns_and_bound();
  return 0;
}
