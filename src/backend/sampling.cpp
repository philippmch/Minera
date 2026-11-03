#include "minera/sampling.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

#include "minera/secure_random.hpp"

namespace minera {
namespace sampling {
namespace {

constexpr std::size_t kMtSize = 624;
constexpr std::size_t kMtShift = 397;
constexpr std::uint32_t kMtMatrix = 0x9908b0dfU;
constexpr std::uint32_t kUpperMask = 0x80000000U;
constexpr std::uint32_t kLowerMask = 0x7fffffffU;

unsigned bit_length(std::uint64_t value) {
  unsigned bits = 0;
  while (value != 0) {
    ++bits;
    value >>= 1U;
  }
  return bits;
}

}  // namespace

void Mt19937::seed_scalar(std::uint32_t seed) {
  state_[0] = seed;
  for (std::size_t i = 1; i < kSize; ++i) {
    const std::uint32_t previous = state_[i - 1];
    state_[i] = 1812433253U * (previous ^ (previous >> 30U)) +
                static_cast<std::uint32_t>(i);
  }
  index_ = kSize;
}

void Mt19937::seed_key(const std::vector<std::uint32_t>& key) {
  if (key.empty()) throw std::invalid_argument("MT19937 key must not be empty");
  seed_scalar(19650218U);
  std::size_t i = 1;
  std::size_t j = 0;
  for (std::size_t k = std::max(kSize, key.size()); k > 0; --k) {
    const std::uint32_t previous = state_[i - 1];
    state_[i] = (state_[i] ^ ((previous ^ (previous >> 30U)) * 1664525U)) +
                key[j] + static_cast<std::uint32_t>(j);
    ++i;
    ++j;
    if (i >= kSize) {
      state_[0] = state_[kSize - 1];
      i = 1;
    }
    if (j >= key.size()) j = 0;
  }
  for (std::size_t k = kSize - 1; k > 0; --k) {
    const std::uint32_t previous = state_[i - 1];
    state_[i] = (state_[i] ^ ((previous ^ (previous >> 30U)) * 1566083941U)) -
                static_cast<std::uint32_t>(i);
    ++i;
    if (i >= kSize) {
      state_[0] = state_[kSize - 1];
      i = 1;
    }
  }
  state_[0] = kUpperMask;
  index_ = kSize;
}

std::uint32_t Mt19937::next_u32() {
  if (index_ >= kSize) {
    for (std::size_t k = 0; k < kSize; ++k) {
      const std::uint32_t y =
          (state_[k] & kUpperMask) | (state_[(k + 1) % kSize] & kLowerMask);
      state_[k] = state_[(k + kMtShift) % kSize] ^ (y >> 1U) ^
                  ((y & 1U) != 0 ? kMtMatrix : 0U);
    }
    index_ = 0;
  }
  std::uint32_t y = state_[index_++];
  y ^= y >> 11U;
  y ^= (y << 7U) & 0x9d2c5680U;
  y ^= (y << 15U) & 0xefc60000U;
  y ^= y >> 18U;
  return y;
}

PythonRandom::PythonRandom(std::uint64_t seed) {
  std::vector<std::uint32_t> key;
  for (std::uint64_t rest = seed; rest != 0; rest >>= 32U)
    key.push_back(static_cast<std::uint32_t>(rest & 0xffffffffU));
  if (key.empty()) key.push_back(0);
  mt_.seed_key(key);
}

std::uint64_t PythonRandom::getrandbits(unsigned bits) {
  if (bits > 64) throw std::invalid_argument("getrandbits: at most 64 bits");
  if (bits == 0) return 0;
  if (bits <= 32) return mt_.next_u32() >> (32U - bits);
  // Words are drawn least significant first; the last one keeps its top bits.
  const std::uint64_t low = mt_.next_u32();
  const std::uint64_t high = mt_.next_u32() >> (64U - bits);
  return low | (high << 32U);
}

std::uint64_t PythonRandom::randbelow(std::uint64_t n) {
  if (n == 0) throw std::invalid_argument("randbelow: empty range");
  const unsigned bits = bit_length(n);
  std::uint64_t value = getrandbits(bits);
  while (value >= n) value = getrandbits(bits);
  return value;
}

void PythonRandom::shuffle(std::vector<std::size_t>& values) {
  for (std::size_t i = values.size(); i > 1; --i) {
    const auto j = static_cast<std::size_t>(randbelow(i));
    std::swap(values[i - 1], values[j]);
  }
}

std::vector<std::size_t> PythonRandom::sample(std::size_t n, std::size_t k) {
  if (k > n)
    throw std::invalid_argument("sample larger than population");
  // The pool method runs while the population is no larger than a set the
  // size CPython budgets for the selection-set method: 21 entries, plus
  // 4 ** ceil(log(3k, 4)) once k exceeds 5.
  std::uint64_t setsize = 21;
  if (k > 5) {
    const double exponent = std::ceil(std::log(static_cast<double>(k) * 3.0) /
                                      std::log(4.0));
    if (exponent >= 31.0) {
      setsize = std::numeric_limits<std::uint64_t>::max();
    } else {
      setsize += std::uint64_t{1} << (2U * static_cast<unsigned>(exponent));
    }
  }
  std::vector<std::size_t> result(k);
  if (n <= setsize) {
    std::vector<std::size_t> pool(n);
    for (std::size_t i = 0; i < n; ++i) pool[i] = i;
    for (std::size_t i = 0; i < k; ++i) {
      const auto j = static_cast<std::size_t>(randbelow(n - i));
      result[i] = pool[j];
      pool[j] = pool[n - i - 1];
    }
    return result;
  }
  std::unordered_set<std::size_t> selected;
  for (std::size_t i = 0; i < k; ++i) {
    auto j = static_cast<std::size_t>(randbelow(n));
    while (selected.contains(j)) j = static_cast<std::size_t>(randbelow(n));
    selected.insert(j);
    result[i] = j;
  }
  return result;
}

std::uint64_t NumpyRandomState::interval(std::uint64_t max) {
  if (max == 0) return 0;
  std::uint64_t mask = max;
  for (unsigned shift = 1; shift < 64; shift <<= 1U) mask |= mask >> shift;
  std::uint64_t value = 0;
  if (max <= 0xffffffffU) {
    do {
      value = mt_.next_u32() & mask;
    } while (value > max);
  } else {
    do {
      const std::uint64_t high = mt_.next_u32();
      const std::uint64_t low = mt_.next_u32();
      value = ((high << 32U) | low) & mask;
    } while (value > max);
  }
  return value;
}

std::vector<std::size_t> NumpyRandomState::permutation(std::size_t n) {
  std::vector<std::size_t> values(n);
  for (std::size_t i = 0; i < n; ++i) values[i] = i;
  for (std::size_t i = n; i > 1; --i) {
    const auto j = static_cast<std::size_t>(interval(i - 1));
    std::swap(values[i - 1], values[j]);
  }
  return values;
}

std::vector<std::size_t> pick_points_subset(std::size_t m, std::size_t n) {
  if (n == 0) return {};
  std::vector<std::size_t> points{0, n - 1};
  if (m == 0) {  // no inner points: the two extremes only
    points.erase(std::unique(points.begin(), points.end()), points.end());
    return points;
  }
  if (m >= n) {
    // The inner points step by at most one, so every index is chosen.
    points.resize(n);
    for (std::size_t i = 0; i < n; ++i) points[i] = i;
    return points;
  }
  if (n > 0xffffffffU)
    throw std::invalid_argument("sample_cases: too many cases to sample");
  const std::uint64_t offset = n / (2 * static_cast<std::uint64_t>(m));
  for (std::size_t j = 0; j < m; ++j)
    points.push_back(static_cast<std::size_t>(
        static_cast<std::uint64_t>(j) * n / m + offset));
  std::sort(points.begin(), points.end());
  points.erase(std::unique(points.begin(), points.end()), points.end());
  return points;
}

}  // namespace sampling

namespace {

struct EventRef {
  std::size_t trace = 0;
  std::size_t event = 0;
};

// Rebuilds a log from sampled events: traces in the order their first event
// was sampled, each with its events in original order.
EventLog regroup(const EventLog& log, const std::vector<EventRef>& rows,
                 const std::vector<std::size_t>& order) {
  std::vector<std::vector<std::size_t>> kept(log.traces.size());
  std::vector<std::size_t> trace_order;
  for (const auto index : order) {
    const auto& row = rows[index];
    if (kept[row.trace].empty()) trace_order.push_back(row.trace);
    kept[row.trace].push_back(row.event);
  }
  EventLog result;
  copy_event_log_context(log, result);
  result.traces.reserve(trace_order.size());
  for (const auto trace_index : trace_order) {
    const auto& source = log.traces[trace_index];
    auto& events = kept[trace_index];
    std::sort(events.begin(), events.end());
    Trace trace;
    copy_trace_context(source, trace);
    trace.events.reserve(events.size());
    for (const auto event : events) trace.events.push_back(source.events[event]);
    result.traces.push_back(std::move(trace));
  }
  return result;
}

std::vector<EventRef> event_rows(const EventLog& log) {
  std::vector<EventRef> rows;
  rows.reserve(log.event_count());
  for (std::size_t t = 0; t < log.traces.size(); ++t)
    for (std::size_t e = 0; e < log.traces[t].events.size(); ++e)
      rows.push_back({t, e});
  return rows;
}

EventLog sample_traces(const EventLog& log, std::size_t count,
                       sampling::PythonRandom& random) {
  const auto picked = random.sample(log.traces.size(),
                                    std::min(count, log.traces.size()));
  EventLog result;
  copy_event_log_context(log, result);
  result.traces.reserve(picked.size());
  for (const auto index : picked) result.traces.push_back(log.traces[index]);
  return result;
}

EventLog sample_table_cases(const EventLog& log, std::size_t count,
                            sampling::PythonRandom& random) {
  // Distinct case ids in row order; a trace without events has no rows.
  std::vector<std::string> ids;
  std::unordered_set<std::string> seen;
  for (const auto& trace : log.traces)
    if (!trace.events.empty() && seen.insert(trace.case_id).second)
      ids.push_back(trace.case_id);
  EventLog result;
  copy_event_log_context(log, result);
  if (ids.empty()) return result;
  std::vector<std::size_t> order(ids.size());
  for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
  random.shuffle(order);
  std::unordered_set<std::string> chosen;
  for (const auto point : sampling::pick_points_subset(count, ids.size()))
    chosen.insert(ids[order[point]]);
  for (const auto& trace : log.traces)
    if (!trace.events.empty() && chosen.contains(trace.case_id))
      result.traces.push_back(trace);
  return result;
}

}  // namespace

std::optional<SampleInput> parse_sample_input(std::string_view name) {
  if (name == "dataframe") return SampleInput::kDataFrame;
  if (name == "event_log") return SampleInput::kEventLog;
  if (name == "event_stream") return SampleInput::kEventStream;
  return std::nullopt;
}

std::string_view sample_input_name(SampleInput input) noexcept {
  switch (input) {
    case SampleInput::kDataFrame:
      return "dataframe";
    case SampleInput::kEventLog:
      return "event_log";
    case SampleInput::kEventStream:
      return "event_stream";
  }
  return "dataframe";
}

std::string sampling_options_json(const LogSamplingOptions& options) {
  const auto count = [](const std::optional<std::size_t>& value) {
    return value ? std::to_string(*value) : std::string("null");
  };
  return "{\"cases\":" + count(options.cases) + ",\"events\":" +
         count(options.events) + ",\"seed\":" + std::to_string(options.seed) +
         ",\"input\":\"" + std::string(sample_input_name(options.input)) +
         "\"}";
}

std::uint32_t random_sample_seed() {
  return static_cast<std::uint32_t>(secure_random_u64() & 0xffffffffU);
}

EventLog sample_event_log(const EventLog& log,
                          const LogSamplingOptions& options) {
  // A script seeds both generators up front: random.seed(s) and
  // numpy.random.seed(s); each call then draws from the one it uses.
  sampling::PythonRandom python(options.seed);
  sampling::NumpyRandomState numpy(options.seed);
  EventLog current = log;
  if (options.cases) {
    switch (options.input) {
      case SampleInput::kDataFrame:
        current = sample_table_cases(current, *options.cases, python);
        break;
      case SampleInput::kEventLog:
        current = sample_traces(current, *options.cases, python);
        break;
      case SampleInput::kEventStream:
        throw std::invalid_argument(
            "sample_cases: not defined for an event stream; use dataframe or "
            "event_log");
    }
  }
  if (options.events) {
    const auto count = *options.events;
    switch (options.input) {
      case SampleInput::kDataFrame: {
        const auto rows = event_rows(current);
        if (count > rows.size())
          throw std::invalid_argument(
              "sample_events: cannot take a larger sample (" +
              std::to_string(count) + ") than the " +
              std::to_string(rows.size()) + " events");
        auto order = numpy.permutation(rows.size());
        order.resize(count);
        current = regroup(current, rows, order);
        break;
      }
      case SampleInput::kEventLog:
        current = sample_traces(current, count, python);
        break;
      case SampleInput::kEventStream: {
        const auto rows = event_rows(current);
        const auto order =
            python.sample(rows.size(), std::min(count, rows.size()));
        current = regroup(current, rows, order);
        break;
      }
    }
  }
  return current;
}

}  // namespace minera
