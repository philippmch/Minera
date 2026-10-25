#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace minera::detail {

// The comparisons numpy's stable argsort (np.unique with return_index) makes
// on an object array, for arrays of up to 16 values: one timsort run, found
// ascending or strictly descending and then extended by insertion. `cmp(a,
// b)` compares the values at positions a and b like Python (negative, zero,
// positive) and may throw; the first comparison that throws is the one the
// reference raises.
template <typename Compare>
void numpy_object_stable_argsort(std::size_t size, Compare&& cmp) {
  if (size < 2) return;
  const auto n = static_cast<std::ptrdiff_t>(size);
  std::vector<std::size_t> order(size);
  for (std::size_t i = 0; i < size; ++i) order[i] = i;
  std::ptrdiff_t pi = 1;
  if (cmp(order[0], order[1]) <= 0) {
    while (pi < n - 1 && cmp(order[pi], order[pi + 1]) <= 0) ++pi;
  } else {
    while (pi < n - 1 && cmp(order[pi + 1], order[pi]) < 0) ++pi;
    std::reverse(order.begin(), order.begin() + pi + 1);
  }
  for (++pi; pi < n; ++pi) {
    const auto vi = order[pi];
    auto pj = pi;
    while (pj > 0 && cmp(vi, order[pj - 1]) < 0) {
      order[pj] = order[pj - 1];
      --pj;
    }
    order[pj] = vi;
  }
}

}  // namespace minera::detail
