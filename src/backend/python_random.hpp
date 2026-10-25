#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace minera {

// Python's random module generator: MT19937 seeded by random.seed(int) (the
// reference init_by_array on the 32-bit words of |seed|), getrandbits and the
// rejection-sampled _randbelow behind randrange, choice and sample. These are
// the published CPython algorithms; draws match Python bit for bit.
class PythonRandom {
 public:
  explicit PythonRandom(std::uint64_t seed) {
    std::vector<std::uint32_t> key;
    do {
      key.push_back(static_cast<std::uint32_t>(seed));
      seed >>= 32;
    } while (seed);
    init_genrand(19650218U);
    std::size_t i = 1, j = 0;
    for (std::size_t k = std::max<std::size_t>(kN, key.size()); k; --k) {
      state_[i] = (state_[i] ^ ((state_[i - 1] ^ (state_[i - 1] >> 30)) * 1664525U)) + key[j] + static_cast<std::uint32_t>(j);
      ++i;
      ++j;
      if (i >= kN) {
        state_[0] = state_[kN - 1];
        i = 1;
      }
      if (j >= key.size()) j = 0;
    }
    for (std::size_t k = kN - 1; k; --k) {
      state_[i] = (state_[i] ^ ((state_[i - 1] ^ (state_[i - 1] >> 30)) * 1566083941U)) - static_cast<std::uint32_t>(i);
      ++i;
      if (i >= kN) {
        state_[0] = state_[kN - 1];
        i = 1;
      }
    }
    state_[0] = 0x80000000U;
  }

  std::uint32_t next() {
    if (index_ >= kN) twist();
    std::uint32_t y = state_[index_++];
    y ^= y >> 11;
    y ^= (y << 7) & 0x9d2c5680U;
    y ^= (y << 15) & 0xefc60000U;
    y ^= y >> 18;
    return y;
  }

  // getrandbits(bits) for 1 <= bits <= 32.
  std::uint32_t getrandbits(int bits) { return next() >> (32 - bits); }

  // _randbelow(n) for 1 <= n < 2**32.
  std::uint32_t randbelow(std::uint32_t n) {
    int bits = 0;
    while (bits < 32 && (std::uint64_t{1} << bits) <= n) ++bits;
    std::uint32_t r = getrandbits(bits);
    while (r >= n) r = getrandbits(bits);
    return r;
  }

 private:
  static constexpr std::size_t kN = 624, kM = 397;

  void init_genrand(std::uint32_t seed) {
    state_[0] = seed;
    for (std::size_t i = 1; i < kN; ++i)
      state_[i] = 1812433253U * (state_[i - 1] ^ (state_[i - 1] >> 30)) + static_cast<std::uint32_t>(i);
    index_ = kN;
  }

  void twist() {
    for (std::size_t k = 0; k < kN; ++k) {
      const std::uint32_t y = (state_[k] & 0x80000000U) | (state_[(k + 1) % kN] & 0x7fffffffU);
      state_[k] = state_[(k + kM) % kN] ^ (y >> 1) ^ ((y & 1U) ? 0x9908b0dfU : 0U);
    }
    index_ = 0;
  }

  std::array<std::uint32_t, kN> state_{};
  std::size_t index_ = kN;
};

}  // namespace minera
