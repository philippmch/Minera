#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace minera::detail {

// numpy sorts an object array with its generic sorts, which call the item
// comparison (Python's a < b, then a > b) and carry on after an error with
// every later comparison reported equal; the first comparison that raised is
// the error the reference reports. These emulate numpy's generic argsorts
// comparison for comparison, so `cmp(a, b)` (negative, zero, positive for
// the values at positions a and b, like Python) is called on the same pairs
// in the same order and may throw: the first throw is the reference's error.
// Without a throw they return the argsort numpy returns, including its order
// for values that compare equal both ways (NaN).

namespace numpy_sort {

using Index = std::ptrdiff_t;

// npy_atimsort's minimum run for generic (non-numeric) items.
inline Index min_run(Index num) {
  Index r = 0;
  while (16 < num) {
    r |= num & 1;
    num >>= 1;
  }
  return num + r;
}

template <typename Compare>
Index count_run(std::vector<std::size_t>& order, Index l, Index num, Index minrun, Compare& cmp) {
  if (num - l == 1) return 1;
  const Index pl = l;
  Index pi = pl + 1;
  if (cmp(order[pl], order[pl + 1]) <= 0) {
    while (pi < num - 1 && cmp(order[pi], order[pi + 1]) <= 0) ++pi;
  } else {
    while (pi < num - 1 && cmp(order[pi + 1], order[pi]) < 0) ++pi;
    std::reverse(order.begin() + pl, order.begin() + pi + 1);
  }
  ++pi;
  Index size = pi - pl;
  if (size < minrun) {
    size = l + minrun < num ? minrun : num - l;
    const Index pr = pl + size;
    for (; pi < pr; ++pi) {
      const auto vi = order[pi];
      Index pj = pi;
      while (pl < pj && cmp(vi, order[pj - 1]) < 0) {
        order[pj] = order[pj - 1];
        --pj;
      }
      order[pj] = vi;
    }
  }
  return size;
}

// Where `key` belongs after its equals in order[base, base + size).
template <typename Compare>
Index gallop_right(const std::vector<std::size_t>& order, Index base, Index size, std::size_t key, Compare& cmp) {
  if (cmp(key, order[base]) < 0) return 0;
  Index last = 0, ofs = 1;
  for (;;) {
    if (size <= ofs || ofs < 0) {
      ofs = size;
      break;
    }
    if (cmp(key, order[base + ofs]) < 0) break;
    last = ofs;
    ofs = (ofs << 1) + 1;
  }
  while (last + 1 < ofs) {
    const Index m = last + ((ofs - last) >> 1);
    if (cmp(key, order[base + m]) < 0) ofs = m;
    else last = m;
  }
  return ofs;
}

// Where `key` belongs before its equals in order[base, base + size).
template <typename Compare>
Index gallop_left(const std::vector<std::size_t>& order, Index base, Index size, std::size_t key, Compare& cmp) {
  if (cmp(order[base + size - 1], key) < 0) return size;
  Index last = 0, ofs = 1;
  for (;;) {
    if (size <= ofs || ofs < 0) {
      ofs = size;
      break;
    }
    if (cmp(order[base + size - ofs - 1], key) < 0) break;
    last = ofs;
    ofs = (ofs << 1) + 1;
  }
  Index l = size - ofs - 1, r = size - last - 1;
  while (l + 1 < r) {
    const Index m = l + ((r - l) >> 1);
    if (cmp(order[base + m], key) < 0) l = m;
    else r = m;
  }
  return r;
}

template <typename Compare>
void merge_at(std::vector<std::size_t>& order, Index s1, Index l1, Index s2, Index l2, Compare& cmp) {
  const Index k = gallop_right(order, s1, l1, order[s2], cmp);
  if (l1 == k) return;
  const Index p1 = s1 + k;
  l1 -= k;
  l2 = gallop_left(order, s2, l2, order[s2 - 1], cmp);
  if (l2 < l1) {
    // merge_right: from the back, the second run copied aside.
    std::vector<std::size_t> buffer(order.begin() + s2, order.begin() + s2 + l2);
    const Index start = p1 - 1;
    Index a = p1 + l1 - 1, out = s2 + l2 - 1, b = l2 - 1;
    order[out--] = order[a--];
    while (a < out && start < a) {
      if (cmp(buffer[b], order[a]) < 0) order[out--] = order[a--];
      else order[out--] = buffer[b--];
    }
    if (a != out) {
      const Index ofs = out - start;
      std::copy(buffer.begin() + (b - ofs + 1), buffer.begin() + b + 1, order.begin() + start + 1);
    }
  } else {
    // merge_left: from the front, the first run copied aside.
    std::vector<std::size_t> buffer(order.begin() + p1, order.begin() + p1 + l1);
    const Index end = s2 + l2;
    Index out = p1, b = s2, a = 0;
    order[out++] = order[b++];
    while (out < b && b < end) {
      if (cmp(order[b], buffer[a]) < 0) order[out++] = order[b++];
      else order[out++] = buffer[a++];
    }
    if (out != b) std::copy(buffer.begin() + a, buffer.begin() + a + (b - out), order.begin() + out);
  }
}

struct Run {
  Index start;
  Index length;
};

template <typename Compare>
void try_collapse(std::vector<std::size_t>& order, std::vector<Run>& stack, Compare& cmp) {
  while (1 < stack.size()) {
    const auto top = static_cast<Index>(stack.size());
    const Index B = stack[top - 2].length;
    const Index C = stack[top - 1].length;
    if ((2 < top && stack[top - 3].length <= B + C) || (3 < top && stack[top - 4].length <= stack[top - 3].length + B)) {
      const Index A = stack[top - 3].length;
      if (A <= C) {
        merge_at(order, stack[top - 3].start, A, stack[top - 2].start, B, cmp);
        stack[top - 3].length += B;
        stack[top - 2] = stack[top - 1];
      } else {
        merge_at(order, stack[top - 2].start, B, stack[top - 1].start, C, cmp);
        stack[top - 2].length += C;
      }
      stack.pop_back();
    } else if (B <= C) {
      merge_at(order, stack[top - 2].start, B, stack[top - 1].start, C, cmp);
      stack[top - 2].length += C;
      stack.pop_back();
    } else {
      break;
    }
  }
}

template <typename Compare>
void force_collapse(std::vector<std::size_t>& order, std::vector<Run>& stack, Compare& cmp) {
  while (2 < stack.size()) {
    const auto top = static_cast<Index>(stack.size());
    if (stack[top - 3].length <= stack[top - 1].length) {
      merge_at(order, stack[top - 3].start, stack[top - 3].length, stack[top - 2].start, stack[top - 2].length, cmp);
      stack[top - 3].length += stack[top - 2].length;
      stack[top - 2] = stack[top - 1];
    } else {
      merge_at(order, stack[top - 2].start, stack[top - 2].length, stack[top - 1].start, stack[top - 1].length, cmp);
      stack[top - 2].length += stack[top - 1].length;
    }
    stack.pop_back();
  }
  if (1 < stack.size()) merge_at(order, stack[0].start, stack[0].length, stack[1].start, stack[1].length, cmp);
}

template <typename Compare>
void heapsort(std::vector<std::size_t>& order, Index base, Index n, Compare& cmp) {
  // npy_aheapsort, 1-based over order[base, base + n).
  const auto at = [&](Index i) -> std::size_t& { return order[base + i - 1]; };
  Index i, j, l;
  std::size_t tmp;
  for (l = n >> 1; l > 0; --l) {
    tmp = at(l);
    for (i = l, j = l << 1; j <= n;) {
      if (j < n && cmp(at(j), at(j + 1)) < 0) j += 1;
      if (cmp(tmp, at(j)) < 0) {
        at(i) = at(j);
        i = j;
        j += j;
      } else {
        break;
      }
    }
    at(i) = tmp;
  }
  for (; n > 1;) {
    tmp = at(n);
    at(n) = at(1);
    n -= 1;
    for (i = 1, j = 2; j <= n;) {
      if (j < n && cmp(at(j), at(j + 1)) < 0) j++;
      if (cmp(tmp, at(j)) < 0) {
        at(i) = at(j);
        i = j;
        j += j;
      } else {
        break;
      }
    }
    at(i) = tmp;
  }
}

}  // namespace numpy_sort

// numpy's stable argsort (kind="stable", np.unique's return_index) of an
// object array: npy_atimsort with its generic minimum run.
template <typename Compare>
std::vector<std::size_t> numpy_object_stable_argsort(std::size_t size, Compare&& cmp) {
  using namespace numpy_sort;
  std::vector<std::size_t> order(size);
  for (std::size_t i = 0; i < size; ++i) order[i] = i;
  const auto num = static_cast<Index>(size);
  const Index minrun = min_run(num);
  std::vector<Run> stack;
  for (Index l = 0; l < num;) {
    const Index n = count_run(order, l, num, minrun, cmp);
    stack.push_back({l, n});
    try_collapse(order, stack, cmp);
    l += n;
  }
  force_collapse(order, stack, cmp);
  return order;
}

// numpy's default argsort (kind="quicksort") of an object array:
// npy_aquicksort, an introsort with median-of-three partitions and
// insertion sort for 16 items or fewer.
template <typename Compare>
std::vector<std::size_t> numpy_object_quick_argsort(std::size_t size, Compare&& cmp) {
  using namespace numpy_sort;
  std::vector<std::size_t> order(size);
  for (std::size_t i = 0; i < size; ++i) order[i] = i;
  if (size < 2) return order;
  constexpr Index kSmall = 15;
  int msb = 0;
  for (auto n = size; n > 1; n >>= 1) ++msb;
  int cdepth = msb * 2;
  Index pl = 0, pr = static_cast<Index>(size) - 1;
  std::vector<Index> stack;
  std::vector<int> depths;
  for (;;) {
    bool heap = false;
    if (cdepth < 0) {
      heapsort(order, pl, pr - pl + 1, cmp);
      heap = true;
    }
    if (!heap) {
      while ((pr - pl) > kSmall) {
        Index pm = pl + ((pr - pl) >> 1);
        if (cmp(order[pm], order[pl]) < 0) std::swap(order[pm], order[pl]);
        if (cmp(order[pr], order[pm]) < 0) std::swap(order[pr], order[pm]);
        if (cmp(order[pm], order[pl]) < 0) std::swap(order[pm], order[pl]);
        const auto pivot = order[pm];
        Index pi = pl, pj = pr - 1;
        std::swap(order[pm], order[pj]);
        for (;;) {
          do ++pi; while (cmp(order[pi], pivot) < 0 && pi < pj);
          do --pj; while (cmp(pivot, order[pj]) < 0 && pi < pj);
          if (pi >= pj) break;
          std::swap(order[pi], order[pj]);
        }
        std::swap(order[pi], order[pr - 1]);
        if (pi - pl < pr - pi) {
          stack.push_back(pi + 1);
          stack.push_back(pr);
          pr = pi - 1;
        } else {
          stack.push_back(pl);
          stack.push_back(pi - 1);
          pl = pi + 1;
        }
        depths.push_back(--cdepth);
      }
      for (Index pi = pl + 1; pi <= pr; ++pi) {
        const auto vi = order[pi];
        Index pj = pi;
        while (pj > pl && cmp(vi, order[pj - 1]) < 0) {
          order[pj] = order[pj - 1];
          --pj;
        }
        order[pj] = vi;
      }
    }
    if (stack.empty()) break;
    pr = stack.back();
    stack.pop_back();
    pl = stack.back();
    stack.pop_back();
    cdepth = depths.back();
    depths.pop_back();
  }
  return order;
}

}  // namespace minera::detail
