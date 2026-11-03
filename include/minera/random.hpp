#pragma once

// The one Mersenne Twister of Minera and the deterministic draws built on it.
//
// Seeded results must not depend on the C++ standard library: the standard
// fixes the engines (std::mt19937, std::mt19937_64) but leaves every
// distribution (uniform_int_distribution, bernoulli_distribution,
// discrete_distribution, normal_distribution, ...) and std::shuffle to the
// implementation. Every seeded draw in Minera therefore goes through this
// header:
//
//  - rng::Mt19937 is the 32-bit MT19937 that CPython's random module and
//    numpy's legacy RandomState share (init_genrand and init_by_array
//    seeding).
//  - rng::PythonRandom reproduces random.Random(seed) for an integer seed:
//    getrandbits, the bit-length rejection behind randrange / randint /
//    choice, random() and shuffle, draw for draw.
//  - rng::NumpyRandomState reproduces the legacy numpy.random.RandomState
//    (numpy.random.seed(s)): random_sample, the masked-rejection integers,
//    permutation, the polar normal with its cached value, the legacy
//    exponential, Laplace, gamma and triangular samplers and choice with p.
//  - The free functions below (below, unit, uniform_int, bernoulli,
//    weighted_index, shuffle, normal, ...) give native-only algorithms the
//    same portability over any standard engine: Minera's own algorithms,
//    fixed here, rather than whatever a standard library ships.
//
// The algorithms are the published ones of the respective libraries; the
// draws were validated against the reference interpreters, not copied.

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace minera::rng {

class Mt19937 {
 public:
  Mt19937() { seed_scalar(5489U); }
  explicit Mt19937(std::uint32_t seed) { seed_scalar(seed); }
  // init_genrand: numpy.random.seed(s) for 0 <= s < 2^32.
  void seed_scalar(std::uint32_t seed);
  // init_by_array: random.seed(s) for an integer s, with the key holding
  // |s| as 32-bit words, least significant first ({0} for zero).
  void seed_key(const std::vector<std::uint32_t>& key);
  [[nodiscard]] std::uint32_t next_u32();

  // A 53-bit double in [0, 1) from two outputs: (a >> 5, b >> 6), the
  // conversion both CPython's random() and numpy's random_sample() use.
  [[nodiscard]] double next_double53();

 private:
  static constexpr std::size_t kSize = 624;
  std::array<std::uint32_t, kSize> state_{};
  std::size_t index_ = kSize;
};

// Python's random.Random seeded with a non-negative integer below 2^64.
class PythonRandom {
 public:
  explicit PythonRandom(std::uint64_t seed);
  [[nodiscard]] std::uint32_t next_u32() { return mt_.next_u32(); }
  // getrandbits(k) for 0 <= k <= 64.
  [[nodiscard]] std::uint64_t getrandbits(unsigned bits);
  // _randbelow(n): a uniform integer in [0, n) by bit-length rejection; n > 0.
  [[nodiscard]] std::uint64_t randbelow(std::uint64_t n);
  // random(): a double in [0, 1).
  [[nodiscard]] double random() { return mt_.next_double53(); }
  // randrange(start, stop) for start < stop; randint(a, b) for a <= b.
  [[nodiscard]] std::int64_t randrange(std::int64_t start, std::int64_t stop);
  [[nodiscard]] std::int64_t randint(std::int64_t a, std::int64_t b) {
    return randrange(a, b + 1);
  }
  // choice(seq): the index seq[randbelow(len(seq))] picks; size > 0.
  [[nodiscard]] std::size_t choice_index(std::size_t size) {
    return static_cast<std::size_t>(randbelow(size));
  }
  // random.shuffle(x), in place.
  template <class T>
  void shuffle(std::vector<T>& values) {
    for (std::size_t i = values.size(); i > 1; --i) {
      const auto j = static_cast<std::size_t>(randbelow(i));
      std::swap(values[i - 1], values[j]);
    }
  }
  // random.sample(range(n), k): the sampled positions in draw order; k <= n.
  [[nodiscard]] std::vector<std::size_t> sample(std::size_t n, std::size_t k);

 private:
  Mt19937 mt_;
};

// numpy's legacy RandomState seeded with numpy.random.seed(seed).
class NumpyRandomState {
 public:
  explicit NumpyRandomState(std::uint32_t seed) : mt_(seed) {}
  [[nodiscard]] std::uint32_t next_u32() { return mt_.next_u32(); }
  // random_sample(): a double in [0, 1).
  [[nodiscard]] double random_sample() { return mt_.next_double53(); }
  // A uniform integer in [0, max] by masked rejection (randint, permutation).
  [[nodiscard]] std::uint64_t interval(std::uint64_t max);
  // permutation(n).
  [[nodiscard]] std::vector<std::size_t> permutation(std::size_t n);
  // standard_normal() by the polar method, caching the second value.
  [[nodiscard]] double gauss();
  [[nodiscard]] double standard_normal() { return gauss(); }
  // standard_exponential() = -log(1 - U); exponential(scale) scales it.
  [[nodiscard]] double standard_exponential() {
    return -std::log(1.0 - random_sample());
  }
  [[nodiscard]] double exponential(double scale) {
    return scale * standard_exponential();
  }
  [[nodiscard]] double laplace(double loc, double scale);
  [[nodiscard]] double standard_gamma(double shape);
  // triangular(left, mode, right): one uniform through the inverse CDF.
  [[nodiscard]] double triangular(double left, double mode, double right);
  // choice(n, p=p): p is normalised by its total, cumulated, the cumulative
  // sums divided by the last one, and the first index whose sum exceeds one
  // uniform is returned. Weights must be finite and non-negative with a
  // positive total.
  [[nodiscard]] std::size_t choice_index(const std::vector<double>& weights);

 private:
  Mt19937 mt_;
  bool has_gauss_ = false;
  double gauss_ = 0.0;
};

// ---------------------------------------------------------------------------
// Portable draws over a standard engine (std::mt19937, std::mt19937_64,
// rng::Mt19937 via its next_u32, ...). Engines whose range is the full 32 or
// 64 bits are supported; a 32-bit engine contributes two outputs per 64-bit
// word, high word first.

template <class Engine>
[[nodiscard]] std::uint64_t next_u64(Engine& engine) {
  using Result = decltype(engine());
  static_assert(std::numeric_limits<Result>::is_integer, "integer engine required");
  if constexpr (Engine::max() == 0xffffffffULL && Engine::min() == 0) {
    const std::uint64_t high = static_cast<std::uint32_t>(engine());
    const std::uint64_t low = static_cast<std::uint32_t>(engine());
    return (high << 32U) | low;
  } else {
    static_assert(Engine::min() == 0 &&
                      Engine::max() == std::numeric_limits<std::uint64_t>::max(),
                  "engine must produce full 32- or 64-bit words");
    return static_cast<std::uint64_t>(engine());
  }
}

// A uniform integer in [0, n), n > 0, by rejection on the smallest
// covering power of two (no modulo bias, no implementation freedom).
template <class Engine>
[[nodiscard]] std::uint64_t below(Engine& engine, std::uint64_t n) {
  if (n == 0) throw std::invalid_argument("rng::below: empty range");
  if (n == 1) return 0;
  std::uint64_t mask = n - 1;
  mask |= mask >> 1U;
  mask |= mask >> 2U;
  mask |= mask >> 4U;
  mask |= mask >> 8U;
  mask |= mask >> 16U;
  mask |= mask >> 32U;
  for (;;) {
    const std::uint64_t value = next_u64(engine) & mask;
    if (value < n) return value;
  }
}

// A uniform integer in [lo, hi], lo <= hi.
template <class Engine, class Int>
[[nodiscard]] Int uniform_int(Engine& engine, Int lo, Int hi) {
  static_assert(std::numeric_limits<Int>::is_integer);
  if (hi < lo) throw std::invalid_argument("rng::uniform_int: empty range");
  const auto span = static_cast<std::uint64_t>(hi) - static_cast<std::uint64_t>(lo);
  if (span == std::numeric_limits<std::uint64_t>::max())
    return static_cast<Int>(next_u64(engine));
  return static_cast<Int>(static_cast<std::uint64_t>(lo) + below(engine, span + 1));
}

// A double in [0, 1) with 53 random bits.
template <class Engine>
[[nodiscard]] double unit(Engine& engine) {
  return static_cast<double>(next_u64(engine) >> 11U) * 0x1.0p-53;
}

// A double in [a, b).
template <class Engine>
[[nodiscard]] double uniform_real(Engine& engine, double a, double b) {
  const double value = a + (b - a) * unit(engine);
  return value < b ? value : a;
}

// true with probability p (clamped to [0, 1]).
template <class Engine>
[[nodiscard]] bool bernoulli(Engine& engine, double p) {
  if (!(p > 0.0)) return false;
  if (p >= 1.0) return true;
  return unit(engine) < p;
}

// An index drawn with probability weight / total; weights must be finite and
// non-negative. A zero total draws uniformly.
template <class Engine>
[[nodiscard]] std::size_t weighted_index(Engine& engine, const std::vector<double>& weights) {
  if (weights.empty()) throw std::invalid_argument("rng::weighted_index: no weights");
  double total = 0.0;
  for (const double w : weights) {
    if (!std::isfinite(w) || w < 0.0)
      throw std::invalid_argument("rng::weighted_index: invalid weight");
    total += w;
  }
  if (!(total > 0.0) || !std::isfinite(total))
    return static_cast<std::size_t>(below(engine, weights.size()));
  const double target = unit(engine) * total;
  double running = 0.0;
  std::size_t last_positive = 0;
  for (std::size_t i = 0; i < weights.size(); ++i) {
    if (weights[i] <= 0.0) continue;
    last_positive = i;
    running += weights[i];
    if (target < running) return i;
  }
  return last_positive;
}

// Fisher-Yates from the back: position i - 1 swaps with below(i).
template <class Iterator, class Engine>
void shuffle(Iterator first, Iterator last, Engine& engine) {
  using std::swap;
  const auto n = static_cast<std::uint64_t>(std::distance(first, last));
  for (std::uint64_t i = n; i > 1; --i) {
    const auto j = below(engine, i);
    swap(*(first + static_cast<std::ptrdiff_t>(i - 1)),
         *(first + static_cast<std::ptrdiff_t>(j)));
  }
}

// Exponential with rate lambda > 0: -log(1 - U) / lambda.
template <class Engine>
[[nodiscard]] double exponential(Engine& engine, double lambda) {
  return -std::log(1.0 - unit(engine)) / lambda;
}

// Normal by the Box-Muller transform (two uniforms per value, no cache, so a
// draw never depends on an earlier call).
template <class Engine>
[[nodiscard]] double normal(Engine& engine, double mean, double stddev) {
  const double u1 = 1.0 - unit(engine);  // (0, 1]
  const double u2 = unit(engine);
  constexpr double kTwoPi = 6.283185307179586476925286766559;
  return mean + stddev * std::sqrt(-2.0 * std::log(u1)) * std::cos(kTwoPi * u2);
}

template <class Engine>
[[nodiscard]] double lognormal(Engine& engine, double m, double s) {
  return std::exp(normal(engine, m, s));
}

// Gamma(shape, scale) by Marsaglia and Tsang (shape < 1 boosted by U^(1/a)).
template <class Engine>
[[nodiscard]] double gamma(Engine& engine, double shape, double scale) {
  if (!(shape > 0.0) || !(scale > 0.0))
    throw std::invalid_argument("rng::gamma: shape and scale must be positive");
  if (shape < 1.0) {
    const double u = 1.0 - unit(engine);
    return gamma(engine, shape + 1.0, scale) * std::pow(u, 1.0 / shape);
  }
  const double d = shape - 1.0 / 3.0;
  const double c = 1.0 / std::sqrt(9.0 * d);
  for (;;) {
    double x, v;
    do {
      x = normal(engine, 0.0, 1.0);
      v = 1.0 + c * x;
    } while (v <= 0.0);
    v = v * v * v;
    const double u = 1.0 - unit(engine);
    if (u < 1.0 - 0.0331 * x * x * x * x) return d * v * scale;
    if (std::log(u) < 0.5 * x * x + d * (1.0 - v + std::log(v))) return d * v * scale;
  }
}

}  // namespace minera::rng
