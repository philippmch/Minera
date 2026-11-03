#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/random.hpp"

namespace minera {

// How the sampled log is seen, matching the object pm4py 2.7.22's
// sample_cases / sample_events receive:
//  - kDataFrame (default): a flat event table, which is what pm4py.read_xes
//    and a pandas CSV read return. One row per event in trace order; traces
//    without events have no rows and so never survive sampling.
//      sample_cases(df, n): the distinct case ids in row order are shuffled
//        with Python's random.shuffle, the points-subset picker selects
//        indices {0, len-1} plus n evenly spread inner points, and the rows of
//        the selected cases are kept in their original order (so n = 0 keeps
//        two cases and n >= 1 keeps up to n + 2).
//      sample_events(df, n): numpy's df.sample(n), the first n entries of a
//        legacy-RandomState permutation of the rows; n above the row count is
//        rejected as the reference rejects it.
//  - kEventLog: a legacy pm4py EventLog object. Both operations call Python's
//    random.sample over the traces with min(n, trace count) draws and keep the
//    sampled order (sample_events on an EventLog samples traces too, because
//    an EventLog iterates its traces).
//  - kEventStream: an EventStream (the log flattened in trace order);
//    sample_events is random.sample over the events with min(n, event count)
//    draws. sample_cases is not defined for streams and is rejected.
//
// Sampled events are regrouped into traces as the log representation needs:
// traces appear in the order their first event was sampled and every trace
// keeps its events in their original relative order (the order pm4py's
// table-based algorithms restore by sorting on case and timestamp).
enum class SampleInput { kDataFrame, kEventLog, kEventStream };

struct LogSamplingOptions {
  std::optional<std::size_t> cases;   // sample_cases(n), applied first
  std::optional<std::size_t> events;  // sample_events(n), applied second
  // The value both random.seed(seed) and numpy.random.seed(seed) receive
  // before the first call. numpy's legacy seeding accepts 0..2^32-1, so the
  // seed is a 32-bit value. Callers without a seed draw one with
  // random_sample_seed() and report it.
  std::uint32_t seed = 0;
  SampleInput input = SampleInput::kDataFrame;
};

// Maps "dataframe", "event_log" or "event_stream" to the input kind.
[[nodiscard]] std::optional<SampleInput> parse_sample_input(
    std::string_view name);
[[nodiscard]] std::string_view sample_input_name(SampleInput input) noexcept;

// A fresh seed from the operating system's cryptographic source, used when a
// request gives none (the reference's unseeded default draws from OS entropy
// as well, so unseeded runs are not reproducible on either side).
[[nodiscard]] std::uint32_t random_sample_seed();

// Applies options.cases and then options.events. Throws
// std::invalid_argument when an operation is not defined for the input kind
// or when a DataFrame sample_events asks for more rows than exist.
[[nodiscard]] EventLog sample_event_log(const EventLog& log,
                                        const LogSamplingOptions& options);

// {"cases":n|null,"events":n|null,"seed":s,"input":"dataframe"|...}, the
// record mining responses carry so an unseeded request can be replayed.
[[nodiscard]] std::string sampling_options_json(
    const LogSamplingOptions& options);

namespace sampling {

// The shared generators (minera/random.hpp), under their historical names.
using Mt19937 = rng::Mt19937;
using PythonRandom = rng::PythonRandom;
using NumpyRandomState = rng::NumpyRandomState;

// The sorted distinct indices the reference's points-subset picker returns
// for m requested points out of n >= 1 items, extremes included: 0, n - 1
// and (j * n) / m + n / (2 m) for 0 <= j < m.
[[nodiscard]] std::vector<std::size_t> pick_points_subset(std::size_t m,
                                                          std::size_t n);

}  // namespace sampling
}  // namespace minera
