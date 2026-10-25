#pragma once

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace minera::detail {

// CPython's list.sort on fewer than 64 items: one natural run (strictly
// descending runs are reversed), then binary insertion, with reverse=True
// reversing the list before and after a stable sort. `less(a, b)` is Python's
// `a < b` and may throw; the first comparison that throws is the reference's.
// Longer lists keep a stable sort with the same result.
template <typename T, typename Less>
void python_list_sort(std::vector<T>& items, Less&& less, bool reverse = false) {
  if (items.size() >= 64) {
    if (reverse) std::stable_sort(items.begin(), items.end(), [&](const T& a, const T& b) { return less(b, a); });
    else std::stable_sort(items.begin(), items.end(), less);
    return;
  }
  if (reverse) std::reverse(items.begin(), items.end());
  const auto n = items.size();
  if (n >= 2) {
    std::size_t run = 2;
    if (less(items[1], items[0])) {
      while (run < n && less(items[run], items[run - 1])) ++run;
      std::reverse(items.begin(), items.begin() + static_cast<std::ptrdiff_t>(run));
    } else {
      while (run < n && !less(items[run], items[run - 1])) ++run;
    }
    for (std::size_t start = run; start < n; ++start) {
      T pivot = std::move(items[start]);
      std::size_t l = 0, r = start;
      while (l < r) {
        const auto p = l + (r - l) / 2;
        if (less(pivot, items[p])) r = p;
        else l = p + 1;
      }
      for (auto i = start; i > l; --i) items[i] = std::move(items[i - 1]);
      items[l] = std::move(pivot);
    }
  }
  if (reverse) std::reverse(items.begin(), items.end());
}

}  // namespace minera::detail
