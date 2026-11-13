#pragma once

// The row order of a Polars 1.44.1 sort by several columns
// (`LazyFrame.sort([case, timestamp])`, maintain_order=False,
// multithreaded): Polars arg-sorts (row, first key) pairs with rayon's
// par_sort_unstable_by, a parallel port of the Rust pdqsort. Rows with equal
// keys leave their input order in a fixed way that does not depend on the
// thread count: slices of up to 20 rows are insertion-sorted (stable), longer
// ones are partitioned around median-of-three (ninther from 50 rows) pivots
// with BlockQuicksort (128-row blocks), a pivot equal to the slice's
// predecessor splits off its equal run, an imbalanced split shuffles three
// elements with a xorshift generator, and a slice whose larger side exceeds
// 2,000 rows recurses into both sides afresh (rayon join). Round BX: this
// model reproduced Polars' order on 2,000 random frames of 21 to 4,100 rows
// (ties, nulls, string and integer first keys), observed black-box. A
// Null-dtype key drops out of the sort, and the single-column sort that
// remains keeps tied rows in their input order (700 random frames).

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace minera::polars_sort {
namespace detail {

template <class T, class Less>
void shift_tail(T* v, std::size_t n, Less& less) {
  if (n < 2 || !less(v[n - 1], v[n - 2])) return;
  T x = v[n - 1];
  v[n - 1] = v[n - 2];
  std::size_t j = n - 2;
  while (j > 0 && less(x, v[j - 1])) {
    v[j] = v[j - 1];
    --j;
  }
  v[j] = x;
}

template <class T, class Less>
void shift_head(T* v, std::size_t n, Less& less) {
  if (n < 2 || !less(v[1], v[0])) return;
  T x = v[0];
  v[0] = v[1];
  std::size_t j = 1;
  while (j + 1 < n && less(v[j + 1], x)) {
    v[j] = v[j + 1];
    ++j;
  }
  v[j] = x;
}

template <class T, class Less>
void insertion_sort(T* v, std::size_t n, Less& less) {
  for (std::size_t i = 2; i <= n; ++i) shift_tail(v, i, less);
}

// Up to five adjacent out-of-order pairs are fixed on slices of 50 or more;
// both shifts run on the prefix before the pair's second element.
template <class T, class Less>
bool partial_insertion_sort(T* v, std::size_t n, Less& less) {
  std::size_t i = 1;
  for (int step = 0; step < 5; ++step) {
    while (i < n && !less(v[i], v[i - 1])) ++i;
    if (i == n) return true;
    if (n < 50) return false;
    std::swap(v[i - 1], v[i]);
    if (i >= 2) {
      shift_tail(v, i, less);
      shift_head(v, i, less);
    }
  }
  return false;
}

template <class T, class Less>
void heapsort(T* v, std::size_t n, Less& less) {
  const auto sift_down = [&](std::size_t end, std::size_t node) {
    while (true) {
      auto child = 2 * node + 1;
      if (child >= end) break;
      if (child + 1 < end && less(v[child], v[child + 1])) ++child;
      if (!less(v[node], v[child])) break;
      std::swap(v[node], v[child]);
      node = child;
    }
  };
  for (std::size_t i = n / 2; i-- > 0;) sift_down(n, i);
  for (std::size_t i = n; i-- > 1;) {
    std::swap(v[0], v[i]);
    sift_down(i, 0);
  }
}

template <class T>
void break_patterns(T* v, std::size_t n) {
  if (n < 8) return;
  std::uint64_t seed = n;
  std::size_t modulus = 1;
  while (modulus < n) modulus <<= 1;
  const std::size_t pos = n / 4 * 2;
  for (std::size_t i = 0; i < 3; ++i) {
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    auto other = static_cast<std::size_t>(seed) & (modulus - 1);
    if (other >= n) other -= n;
    std::swap(v[pos - 1 + i], v[other]);
  }
}

// The pivot index and whether the slice looks sorted (no swaps).
template <class T, class Less>
std::pair<std::size_t, bool> choose_pivot(T* v, std::size_t n, Less& less) {
  std::size_t a = n / 4, b = n / 4 * 2, c = n / 4 * 3, swaps = 0;
  if (n >= 8) {
    const auto sort2 = [&](std::size_t& x, std::size_t& y) {
      if (less(v[y], v[x])) {
        std::swap(x, y);
        ++swaps;
      }
    };
    const auto sort3 = [&](std::size_t& x, std::size_t& y, std::size_t& z) {
      sort2(x, y);
      sort2(y, z);
      sort2(x, y);
    };
    if (n >= 50) {
      const auto adjacent = [&](std::size_t& m) {
        std::size_t lo = m - 1, hi = m + 1;
        sort3(lo, m, hi);
      };
      adjacent(a);
      adjacent(b);
      adjacent(c);
    }
    sort3(a, b, c);
  }
  if (swaps < 12) return {b, swaps == 0};
  std::reverse(v, v + n);
  return {n - 1 - b, true};
}

template <class T, class Less>
std::size_t partition_in_blocks(T* v, std::size_t n, const T& pivot, Less& less) {
  constexpr std::size_t kBlock = 128;
  std::size_t l = 0, r = n, block_l = kBlock, block_r = kBlock;
  std::uint8_t offsets_l[kBlock], offsets_r[kBlock];
  std::size_t start_l = 0, end_l = 0, start_r = 0, end_r = 0;
  while (true) {
    const bool done = r - l <= 2 * kBlock;
    if (done) {
      auto rem = r - l;
      if (start_l < end_l || start_r < end_r) rem -= kBlock;
      if (start_l < end_l) block_r = rem;
      else if (start_r < end_r) block_l = rem;
      else {
        block_l = rem / 2;
        block_r = rem - block_l;
      }
    }
    if (start_l == end_l) {
      start_l = end_l = 0;
      for (std::size_t i = 0; i < block_l; ++i)
        if (!less(v[l + i], pivot)) offsets_l[end_l++] = static_cast<std::uint8_t>(i);
    }
    if (start_r == end_r) {
      start_r = end_r = 0;
      for (std::size_t i = 0; i < block_r; ++i)
        if (less(v[r - 1 - i], pivot)) offsets_r[end_r++] = static_cast<std::uint8_t>(i);
    }
    const auto count = std::min(end_l - start_l, end_r - start_r);
    if (count > 0) {
      const auto left = [&] { return l + offsets_l[start_l]; };
      const auto right = [&] { return r - offsets_r[start_r] - 1; };
      // A cyclic permutation, as the reference sort moves the elements.
      T tmp = v[left()];
      v[left()] = v[right()];
      for (std::size_t k = 1; k < count; ++k) {
        ++start_l;
        v[right()] = v[left()];
        ++start_r;
        v[left()] = v[right()];
      }
      v[right()] = tmp;
      ++start_l;
      ++start_r;
    }
    if (start_l == end_l) l += block_l;
    if (start_r == end_r) r -= block_r;
    if (done) break;
  }
  if (start_l < end_l) {
    while (start_l < end_l) {
      --end_l;
      std::swap(v[l + offsets_l[end_l]], v[r - 1]);
      --r;
    }
    return r;
  }
  if (start_r < end_r) {
    while (start_r < end_r) {
      --end_r;
      std::swap(v[l], v[r - offsets_r[end_r] - 1]);
      ++l;
    }
    return l;
  }
  return l;
}

// The pivot's final index and whether the slice was already partitioned.
template <class T, class Less>
std::pair<std::size_t, bool> partition(T* v, std::size_t n, std::size_t pivot_at, Less& less) {
  std::swap(v[0], v[pivot_at]);
  const T pivot = v[0];
  T* rest = v + 1;
  std::size_t l = 0, r = n - 1;
  while (l < r && less(rest[l], pivot)) ++l;
  while (l < r && !less(rest[r - 1], pivot)) --r;
  const auto mid = l + partition_in_blocks(rest + l, r - l, pivot, less);
  std::swap(v[0], v[mid]);
  return {mid, l >= r};
}

// Splits off the elements equal to a pivot that equals the predecessor; the
// pivot stays first. Returns the length of the equal run.
template <class T, class Less>
std::size_t partition_equal(T* v, std::size_t n, std::size_t pivot_at, Less& less) {
  std::swap(v[0], v[pivot_at]);
  const T pivot = v[0];
  T* rest = v + 1;
  std::size_t l = 0, r = n - 1;
  while (true) {
    while (l < r && !less(pivot, rest[l])) ++l;
    while (l < r && less(pivot, rest[r - 1])) --r;
    if (l >= r) break;
    --r;
    std::swap(rest[l], rest[r]);
    ++l;
  }
  return l + 1;
}

template <class T, class Less>
void recurse(T* v, std::size_t n, Less& less, const T* pred, unsigned limit) {
  bool was_balanced = true, was_partitioned = true;
  T pred_value{};
  if (pred) pred_value = *pred;
  bool has_pred = pred != nullptr;
  while (true) {
    if (n <= 20) {
      insertion_sort(v, n, less);
      return;
    }
    if (limit == 0) {
      heapsort(v, n, less);
      return;
    }
    if (!was_balanced) {
      break_patterns(v, n);
      --limit;
    }
    const auto [pivot, likely_sorted] = choose_pivot(v, n, less);
    if (was_balanced && was_partitioned && likely_sorted && partial_insertion_sort(v, n, less)) return;
    if (has_pred && !less(pred_value, v[pivot])) {
      const auto mid = partition_equal(v, n, pivot, less);
      v += mid;
      n -= mid;
      continue;
    }
    const auto [mid, partitioned] = partition(v, n, pivot, less);
    was_balanced = std::min(mid, n - mid) >= n / 8;
    was_partitioned = partitioned;
    T* left = v;
    const auto left_n = mid;
    const T pivot_value = v[mid];
    T* right = v + mid + 1;
    const auto right_n = n - mid - 1;
    if (std::max(left_n, right_n) > 2000) {
      recurse(left, left_n, less, has_pred ? &pred_value : nullptr, limit);
      recurse(right, right_n, less, &pivot_value, limit);
      return;
    }
    if (left_n < right_n) {
      recurse(left, left_n, less, has_pred ? &pred_value : nullptr, limit);
      v = right;
      n = right_n;
      pred_value = pivot_value;
      has_pred = true;
    } else {
      recurse(right, right_n, less, &pivot_value, limit);
      n = left_n;
    }
  }
}

}  // namespace detail

// Sorts `values` (in their input order, e.g. row indices 0..n-1) as Polars'
// multi-column sort orders them; `less` compares the full sort key.
template <class T, class Less>
void sort_unstable(std::vector<T>& values, Less less) {
  const auto n = values.size();
  if (n < 2) return;
  unsigned limit = 0;
  for (auto m = n; m; m >>= 1) ++limit;
  detail::recurse(values.data(), n, less, static_cast<const T*>(nullptr), limit);
}

// Sorts by a case and a timestamp key of the given dtypes as Polars does.
template <class T, class Less>
void sort_by_keys(std::vector<T>& values, Less less, std::string_view first_dtype, std::string_view second_dtype) {
  if (first_dtype == "Null" || second_dtype == "Null") std::stable_sort(values.begin(), values.end(), less);
  else sort_unstable(values, less);
}

}  // namespace minera::polars_sort
