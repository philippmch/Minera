#pragma once

// numpy's pairwise float64 summation of a contiguous array (np.add.reduce,
// and so pandas' Series.sum and Series.mean): fewer than 8 values add in
// order; up to 128 values run 8 accumulators combined as
// ((r0 + r1) + (r2 + r3)) + ((r4 + r5) + (r6 + r7)), the tail added in
// order; above, the array halves at a multiple of 8 (observed black-box on
// numpy 2.4.6, round FP).

#include <cstddef>
#include <vector>

namespace minera::detail {

inline double numpy_pairwise_sum(const double* a, std::size_t n) {
  if (n < 8) {
    double res = 0.0;
    for (std::size_t i = 0; i < n; ++i) res += a[i];
    return res;
  }
  if (n <= 128) {
    double r[8];
    for (int k = 0; k < 8; ++k) r[k] = a[k];
    std::size_t i = 8;
    for (; i < n - n % 8; i += 8)
      for (int k = 0; k < 8; ++k) r[k] += a[i + k];
    double res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
    for (; i < n; ++i) res += a[i];
    return res;
  }
  std::size_t half = n / 2;
  half -= half % 8;
  return numpy_pairwise_sum(a, half) + numpy_pairwise_sum(a + half, n - half);
}

inline double numpy_pairwise_sum(const std::vector<double>& values) {
  return numpy_pairwise_sum(values.data(), values.size());
}

}  // namespace minera::detail
