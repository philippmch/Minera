#include "minera/random.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace minera::rng {
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

double Mt19937::next_double53() {
  const auto a = next_u32() >> 5U;
  const auto b = next_u32() >> 6U;
  return (static_cast<double>(a) * 67108864.0 + static_cast<double>(b)) /
         9007199254740992.0;
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

std::int64_t PythonRandom::randrange(std::int64_t start, std::int64_t stop) {
  if (stop <= start) throw std::invalid_argument("randrange: empty range");
  const auto width = static_cast<std::uint64_t>(stop) - static_cast<std::uint64_t>(start);
  return static_cast<std::int64_t>(static_cast<std::uint64_t>(start) + randbelow(width));
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

double NumpyRandomState::gauss() {
  if (has_gauss_) {
    has_gauss_ = false;
    return gauss_;
  }
  double x1, x2, r2;
  do {
    x1 = 2.0 * random_sample() - 1.0;
    x2 = 2.0 * random_sample() - 1.0;
    r2 = x1 * x1 + x2 * x2;
  } while (r2 >= 1.0 || r2 == 0.0);
  const double f = std::sqrt(-2.0 * std::log(r2) / r2);
  gauss_ = f * x1;
  has_gauss_ = true;
  return f * x2;
}

double NumpyRandomState::laplace(double loc, double scale) {
  for (;;) {
    const double u = random_sample();
    if (u >= 0.5) return loc - scale * std::log(2.0 - u - u);
    if (u > 0.0) return loc + scale * std::log(u + u);
  }
}

double NumpyRandomState::standard_gamma(double shape) {
  if (shape == 1.0) return standard_exponential();
  if (shape == 0.0) return 0.0;
  if (shape < 1.0) {
    for (;;) {
      const double u = random_sample();
      const double v = standard_exponential();
      if (u <= 1.0 - shape) {
        const double x = std::pow(u, 1.0 / shape);
        if (x <= v) return x;
      } else {
        const double y = -std::log((1.0 - u) / shape);
        const double x = std::pow(1.0 - shape + shape * y, 1.0 / shape);
        if (x <= v + y) return x;
      }
    }
  }
  const double b = shape - 1.0 / 3.0;
  const double c = 1.0 / std::sqrt(9.0 * b);
  for (;;) {
    double x, v;
    do {
      x = gauss();
      v = 1.0 + c * x;
    } while (v <= 0.0);
    v = v * v * v;
    const double u = random_sample();
    if (u < 1.0 - 0.0331 * (x * x) * (x * x)) return b * v;
    if (std::log(u) < 0.5 * x * x + b * (1.0 - v + std::log(v))) return b * v;
  }
}

double NumpyRandomState::triangular(double left, double mode, double right) {
  const double base = right - left;
  const double left_base = mode - left;
  const double ratio = left_base / base;
  const double left_product = left_base * base;
  const double right_product = (right - mode) * base;
  const double u = random_sample();
  if (u <= ratio) return left + std::sqrt(u * left_product);
  return right - std::sqrt((1.0 - u) * right_product);
}

std::size_t NumpyRandomState::choice_index(const std::vector<double>& weights) {
  if (weights.empty()) throw std::invalid_argument("choice: empty population");
  double total = 0.0;
  for (const double w : weights) {
    if (!std::isfinite(w) || w < 0.0)
      throw std::invalid_argument("choice: probabilities must be finite and non-negative");
    total += w;
  }
  if (!(total > 0.0)) throw std::invalid_argument("choice: probabilities do not sum to a positive value");
  std::vector<double> cdf(weights.size());
  double running = 0.0;
  for (std::size_t i = 0; i < weights.size(); ++i) cdf[i] = running += weights[i] / total;
  for (auto& value : cdf) value /= cdf.back();
  const double uniform = random_sample();
  const auto at = static_cast<std::size_t>(std::upper_bound(cdf.begin(), cdf.end(), uniform) - cdf.begin());
  return std::min(at, weights.size() - 1);
}

}  // namespace minera::rng
