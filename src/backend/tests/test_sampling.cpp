#include "minera/log_filter.hpp"
#include "minera/sampling.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace minera;
using sampling::Mt19937;
using sampling::NumpyRandomState;
using sampling::PythonRandom;

struct CaseCapture {
  SampleInput input;
  bool cases;  // sample_cases, otherwise sample_events
  std::uint32_t seed;
  std::size_t n;
  std::vector<std::string> case_ids;  // output trace order
};

struct RowCapture {
  SampleInput input;
  std::uint32_t seed;
  std::size_t n;
  std::vector<std::size_t> rows;  // sampled rows in sample order
};

struct PickerCapture {
  std::size_t m;
  std::size_t n;
  std::vector<std::size_t> points;
};

#include "sampling_reference_cases.inc"

std::string two_digits(std::size_t value) {
  return (value < 10 ? "0" : "") + std::to_string(value);
}

// 12 cases k00..k11; case i holds (i % 3) + 1 events a, b, c, each tagged
// with its row number in the flat table (rows follow trace order).
EventLog sampling_fixture() {
  EventLog log;
  std::size_t row = 0;
  for (std::size_t i = 0; i < 12; ++i) {
    Trace trace;
    trace.case_id = "k" + two_digits(i);
    for (std::size_t j = 0; j < i % 3 + 1; ++j) {
      Event event;
      event.case_id = trace.case_id;
      event.activity = std::string(1, "abc"[j]);
      const auto minute = 10 * i + j;
      event.timestamp = "2024-01-01T" + two_digits(minute / 60) + ":" +
                        two_digits(minute % 60) + ":00Z";
      event.input_order = row;
      event.attributes.push_back(EventAttribute{"row", std::to_string(row)});
      trace.events.push_back(std::move(event));
      ++row;
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

std::vector<std::string> case_ids(const EventLog& log) {
  std::vector<std::string> ids;
  for (const auto& trace : log.traces) ids.push_back(trace.case_id);
  return ids;
}

std::size_t row_of(const Event& event) {
  return static_cast<std::size_t>(std::stoul(*event.attribute("row")));
}

// The documented regrouping of sampled rows: traces in first-sampled order,
// events in original order.
std::vector<std::vector<std::size_t>> regrouped(const EventLog& fixture,
                                                const std::vector<std::size_t>& rows) {
  std::vector<std::size_t> trace_of;
  for (std::size_t t = 0; t < fixture.traces.size(); ++t)
    for (std::size_t e = 0; e < fixture.traces[t].events.size(); ++e)
      trace_of.push_back(t);
  std::vector<std::size_t> order;
  std::vector<std::vector<std::size_t>> groups(fixture.traces.size());
  for (const auto row : rows) {
    if (groups[trace_of[row]].empty()) order.push_back(trace_of[row]);
    groups[trace_of[row]].push_back(row);
  }
  std::vector<std::vector<std::size_t>> result;
  for (const auto t : order) {
    auto group = groups[t];
    std::sort(group.begin(), group.end());
    result.push_back(group);
  }
  return result;
}

std::vector<std::vector<std::size_t>> rows_by_trace(const EventLog& log) {
  std::vector<std::vector<std::size_t>> result;
  for (const auto& trace : log.traces) {
    result.emplace_back();
    for (const auto& event : trace.events) result.back().push_back(row_of(event));
  }
  return result;
}

void python_random_matches_cpython() {
  PythonRandom zero(0);
  assert(zero.sample(10, 3) == k_py_sample_10_3);
  PythonRandom one(1);
  assert(one.sample(1000, 5) == k_py_sample_1000_5);  // selection-set method
  PythonRandom pool(1);
  assert(pool.sample(100, 30) == k_py_sample_100_30);  // pool method
  PythonRandom large(3);
  assert(large.sample(5000, 40) == k_py_sample_5000_40);
  PythonRandom shuffled(2);
  std::vector<std::size_t> values(15);
  for (std::size_t i = 0; i < values.size(); ++i) values[i] = i;
  shuffled.shuffle(values);
  assert(values == k_py_shuffle_15);
  PythonRandom big_seed(12345678901ULL);  // a two-word init_by_array key
  for (const auto expected : k_py_bigseed_bits) assert(big_seed.getrandbits(32) == expected);
  PythonRandom bits(9);
  for (const auto expected : k_py_getrandbits_40) assert(bits.getrandbits(40) == expected);
  PythonRandom below(9);
  for (const auto expected : k_py_randbelow_big)
    assert(below.randbelow((std::uint64_t{1} << 40) + 3) == expected);
}

void numpy_random_state_matches_legacy_numpy() {
  NumpyRandomState zero(0);
  assert(zero.permutation(10) == k_np_perm_10);
  NumpyRandomState top(4294967295U);
  assert(top.permutation(7) == k_np_perm_7);
  NumpyRandomState wide(5);  // 64-bit masked draws
  for (const auto expected : k_np_interval_big)
    assert(wide.interval((std::uint64_t{1} << 40) + 4) == expected);
  Mt19937 reference;  // the MT19937 default seed 5489 yields 3499211612 first
  assert(reference.next_u32() == 3499211612U);
}

void points_subset_matches_reference_picker() {
  for (const auto& capture : kPickerCaptures)
    assert(sampling::pick_points_subset(capture.m, capture.n) == capture.points);
}

void sample_cases_and_traces_match_reference() {
  const auto fixture = sampling_fixture();
  for (const auto& capture : kCaseCaptures) {
    LogSamplingOptions options;
    options.input = capture.input;
    options.seed = capture.seed;
    (capture.cases ? options.cases : options.events) = capture.n;
    assert(case_ids(sample_event_log(fixture, options)) == capture.case_ids);
  }
}

void sample_events_match_reference_rows() {
  const auto fixture = sampling_fixture();
  for (const auto& capture : kRowCaptures) {
    LogSamplingOptions options;
    options.input = capture.input;
    options.seed = capture.seed;
    options.events = capture.n;
    const auto sampled = sample_event_log(fixture, options);
    assert(rows_by_trace(sampled) == regrouped(fixture, capture.rows));
  }
  for (const auto& [seed, rows] : kBothCaptures) {
    LogSamplingOptions options;
    options.seed = seed;
    options.cases = 3;
    options.events = 4;
    assert(rows_by_trace(sample_event_log(fixture, options)) == regrouped(fixture, rows));
  }
}

void sampling_edge_contracts() {
  auto fixture = sampling_fixture();
  LogSamplingOptions options;
  options.events = fixture.event_count() + 1;  // df.sample rejects n > rows
  bool rejected = false;
  try { (void)sample_event_log(fixture, options); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
  options.events.reset();
  options.cases = 3;
  options.input = SampleInput::kEventStream;  // sample_cases needs a log or table
  rejected = false;
  try { (void)sample_event_log(fixture, options); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
  // An empty trace has no table rows, so the DataFrame path never keeps it;
  // the EventLog path samples it like any trace.
  Trace empty;
  empty.case_id = "empty";
  fixture.traces.insert(fixture.traces.begin(), empty);
  options.input = SampleInput::kDataFrame;
  options.cases = 100;
  assert(sample_event_log(fixture, options).traces.size() == 12);
  options.input = SampleInput::kEventLog;
  assert(sample_event_log(fixture, options).traces.size() == 13);
  // An empty table samples to an empty log (the reference raises IndexError).
  options.input = SampleInput::kDataFrame;
  assert(sample_event_log(EventLog{}, options).traces.empty());
  // Log metadata is kept.
  fixture.attributes.push_back(EventAttribute{"source", "fixture"});
  const auto sampled = sample_event_log(fixture, options);
  assert(sampled.attribute("source") && *sampled.attribute("source") == "fixture");
}

void sampling_runs_first_in_filter_event_log() {
  const auto fixture = sampling_fixture();
  LogFilterOptions filter;
  LogSamplingOptions sampling;
  sampling.seed = 0;
  sampling.cases = 3;
  filter.sampling = sampling;
  assert(filter.any());
  // Sampling only: the sampled log passes the remaining (inactive) filters.
  assert(case_ids(filter_event_log(fixture, filter)) ==
         std::vector<std::string>({"k01", "k03", "k06", "k08", "k11"}));
  // max_cases applies to the sampled log, not before it.
  filter.max_cases = 2;
  assert(case_ids(filter_event_log(fixture, filter)) ==
         std::vector<std::string>({"k01", "k03"}));
  assert(parse_sample_input("dataframe") == SampleInput::kDataFrame);
  assert(parse_sample_input("event_log") == SampleInput::kEventLog);
  assert(parse_sample_input("event_stream") == SampleInput::kEventStream);
  assert(!parse_sample_input("table"));
  assert(sampling_options_json(sampling) ==
         R"({"cases":3,"events":null,"seed":0,"input":"dataframe"})");
}

}  // namespace

int main() {
  python_random_matches_cpython();
  numpy_random_state_matches_legacy_numpy();
  points_subset_matches_reference_picker();
  sample_cases_and_traces_match_reference();
  sample_events_match_reference_rows();
  sampling_edge_contracts();
  sampling_runs_first_in_filter_event_log();
  return 0;
}
