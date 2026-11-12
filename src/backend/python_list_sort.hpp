#pragma once

// CPython 3.11's list.sort as a sequence of `less` calls: timsort with
// natural runs, binary insertion up to minrun, the powersort merge order,
// and galloping merges with the adaptive min_gallop threshold. For a strict
// weak ordering every stable sort gives the same result; this one also
// asks the same questions in the same order, so a comparison that raises
// (a TypeError between a number and a text) raises for the same pair, and
// an ordering that is not a strict weak one (sets compared by inclusion)
// ends in the same permutation. Written from the algorithm's published
// description (Objects/listsort.txt) and checked black-box against
// CPython 3.11's comparison sequence on random lists.

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

namespace minera::detail {

template <class T, class Less>
class PythonListSort {
 public:
  PythonListSort(std::vector<T>& items, Less& less) : a_(items), less_(less) {}

  void run() {
    const std::size_t n = a_.size();
    if (n < 2) return;
    const std::size_t minrun = compute_minrun(n);
    std::size_t lo = 0;
    std::size_t remaining = n;
    do {
      bool descending = false;
      std::size_t length = count_run(lo, lo + remaining, descending);
      if (descending)
        std::reverse(a_.begin() + static_cast<std::ptrdiff_t>(lo),
                     a_.begin() + static_cast<std::ptrdiff_t>(lo + length));
      if (length < minrun) {
        const std::size_t force = remaining <= minrun ? remaining : minrun;
        binary_insertion(lo, lo + force, lo + length);
        length = force;
      }
      found_new_run(length, n);
      pending_.push_back({lo, length, 0});
      lo += length;
      remaining -= length;
    } while (remaining);
    while (pending_.size() > 1) {
      std::size_t at = pending_.size() - 2;
      if (at > 0 && pending_[at - 1].length < pending_[at + 1].length) --at;
      merge_at(at);
    }
  }

 private:
  static constexpr std::size_t kMinGallop = 7;

  struct Run {
    std::size_t base;
    std::size_t length;
    int power;
  };

  bool lt(const T& x, const T& y) { return less_(x, y); }

  // n itself below 64; otherwise the top six bits of n, plus one when any
  // lower bit is set.
  static std::size_t compute_minrun(std::size_t n) {
    std::size_t carry = 0;
    while (n >= 64) {
      carry |= n & 1;
      n >>= 1;
    }
    return n + carry;
  }

  // Length of the run at lo: strictly descending (reversed by the caller) or
  // non-descending.
  std::size_t count_run(std::size_t lo, std::size_t hi, bool& descending) {
    descending = false;
    if (lo + 1 == hi) return 1;
    std::size_t length = 2;
    std::size_t i = lo + 2;
    if (lt(a_[lo + 1], a_[lo])) {
      descending = true;
      for (; i < hi; ++i, ++length)
        if (!lt(a_[i], a_[i - 1])) break;
    } else {
      for (; i < hi; ++i, ++length)
        if (lt(a_[i], a_[i - 1])) break;
    }
    return length;
  }

  // Sorts [lo, hi) where [lo, start) is already sorted: each later item is
  // placed after its equals by binary search.
  void binary_insertion(std::size_t lo, std::size_t hi, std::size_t start) {
    if (start == lo) ++start;
    for (; start < hi; ++start) {
      std::size_t left = lo, right = start;
      do {
        const std::size_t middle = left + ((right - left) >> 1);
        if (lt(a_[start], a_[middle])) right = middle;
        else left = middle + 1;
      } while (left < right);
      T pivot = std::move(a_[start]);
      for (std::size_t p = start; p > left; --p) a_[p] = std::move(a_[p - 1]);
      a_[left] = std::move(pivot);
    }
  }

  // The powersort node power of the boundary between the run at s1 (n1
  // items) and the next one (n2 items) in a list of n: the first bit where
  // the two runs' midpoints, as fractions of n, differ.
  static int power_of(std::size_t s1, std::size_t n1, std::size_t n2, std::size_t n) {
    int result = 0;
    std::size_t a = 2 * s1 + n1;
    std::size_t b = a + n1 + n2;
    for (;;) {
      ++result;
      if (a >= n) {
        a -= n;
        b -= n;
      } else if (b >= n) {
        break;
      }
      a <<= 1;
      b <<= 1;
    }
    return result;
  }

  void found_new_run(std::size_t n2, std::size_t n) {
    if (pending_.empty()) return;
    const Run& last = pending_.back();
    const int power = power_of(last.base, last.length, n2, n);
    while (pending_.size() > 1 && pending_[pending_.size() - 2].power > power)
      merge_at(pending_.size() - 2);
    pending_.back().power = power;
  }

  // Leftmost position in base[0, n) for key (after every item < key),
  // galloping from hint.
  std::size_t gallop_left(const T& key, const T* base, std::size_t n, std::size_t hint) {
    const T* p = base + hint;
    std::ptrdiff_t last = 0, offset = 1;
    const auto h = static_cast<std::ptrdiff_t>(hint);
    if (lt(*p, key)) {
      const std::ptrdiff_t max_offset = static_cast<std::ptrdiff_t>(n) - h;
      while (offset < max_offset) {
        if (!lt(p[offset], key)) break;
        last = offset;
        offset = (offset << 1) + 1;
      }
      if (offset > max_offset) offset = max_offset;
      last += h;
      offset += h;
    } else {
      const std::ptrdiff_t max_offset = h + 1;
      while (offset < max_offset) {
        if (lt(*(p - offset), key)) break;
        last = offset;
        offset = (offset << 1) + 1;
      }
      if (offset > max_offset) offset = max_offset;
      const std::ptrdiff_t k = last;
      last = h - offset;
      offset = h - k;
    }
    ++last;
    while (last < offset) {
      const std::ptrdiff_t middle = last + ((offset - last) >> 1);
      if (lt(base[middle], key)) last = middle + 1;
      else offset = middle;
    }
    return static_cast<std::size_t>(offset);
  }

  // Rightmost position in base[0, n) for key (after every item <= key),
  // galloping from hint.
  std::size_t gallop_right(const T& key, const T* base, std::size_t n, std::size_t hint) {
    const T* p = base + hint;
    std::ptrdiff_t last = 0, offset = 1;
    const auto h = static_cast<std::ptrdiff_t>(hint);
    if (lt(key, *p)) {
      const std::ptrdiff_t max_offset = h + 1;
      while (offset < max_offset) {
        if (!lt(key, *(p - offset))) break;
        last = offset;
        offset = (offset << 1) + 1;
      }
      if (offset > max_offset) offset = max_offset;
      const std::ptrdiff_t k = last;
      last = h - offset;
      offset = h - k;
    } else {
      const std::ptrdiff_t max_offset = static_cast<std::ptrdiff_t>(n) - h;
      while (offset < max_offset) {
        if (lt(key, p[offset])) break;
        last = offset;
        offset = (offset << 1) + 1;
      }
      if (offset > max_offset) offset = max_offset;
      last += h;
      offset += h;
    }
    ++last;
    while (last < offset) {
      const std::ptrdiff_t middle = last + ((offset - last) >> 1);
      if (lt(key, base[middle])) offset = middle;
      else last = middle + 1;
    }
    return static_cast<std::size_t>(offset);
  }

  void merge_at(std::size_t i) {
    std::size_t sa = pending_[i].base, na = pending_[i].length;
    const std::size_t sb = pending_[i + 1].base;
    std::size_t nb = pending_[i + 1].length;
    pending_[i].length = na + nb;
    if (i + 3 == pending_.size()) pending_[i + 1] = pending_[i + 2];
    pending_.pop_back();
    // Items of a before b's first item, and of b after a's last item, are
    // already in place.
    const std::size_t k = gallop_right(a_[sb], a_.data() + sa, na, 0);
    sa += k;
    na -= k;
    if (na == 0) return;
    nb = gallop_left(a_[sa + na - 1], a_.data() + sb, nb, nb - 1);
    if (nb == 0) return;
    if (na <= nb) merge_low(sa, na, sb, nb);
    else merge_high(sa, na, sb, nb);
  }

  enum class End { kDone, kCopyLast };

  // Merges a = [sa, sa + na) and b = [sb, sb + nb) (adjacent) from the left,
  // with a moved to scratch space.
  void merge_low(std::size_t sa, std::size_t na, std::size_t sb, std::size_t nb) {
    scratch_.clear();
    scratch_.reserve(na);
    for (std::size_t i = 0; i < na; ++i) scratch_.push_back(std::move(a_[sa + i]));
    std::size_t pa = 0, pb = sb, dest = sa;
    End end = End::kDone;
    try {
      end = [&]() -> End {
        a_[dest++] = std::move(a_[pb++]);
        if (--nb == 0) return End::kDone;
        if (na == 1) return End::kCopyLast;
        std::size_t min_gallop = min_gallop_;
        for (;;) {
          std::size_t acount = 0, bcount = 0;
          for (;;) {
            if (lt(a_[pb], scratch_[pa])) {
              a_[dest++] = std::move(a_[pb++]);
              ++bcount;
              acount = 0;
              if (--nb == 0) return End::kDone;
              if (bcount >= min_gallop) break;
            } else {
              a_[dest++] = std::move(scratch_[pa++]);
              ++acount;
              bcount = 0;
              if (--na == 1) return End::kCopyLast;
              if (acount >= min_gallop) break;
            }
          }
          ++min_gallop;
          do {
            min_gallop -= min_gallop > 1;
            min_gallop_ = min_gallop;
            std::size_t k = gallop_right(a_[pb], scratch_.data() + pa, na, 0);
            acount = k;
            if (k) {
              for (std::size_t i = 0; i < k; ++i) a_[dest++] = std::move(scratch_[pa++]);
              na -= k;
              if (na == 1) return End::kCopyLast;
              if (na == 0) return End::kDone;
            }
            a_[dest++] = std::move(a_[pb++]);
            if (--nb == 0) return End::kDone;
            k = gallop_left(scratch_[pa], a_.data() + pb, nb, 0);
            bcount = k;
            if (k) {
              for (std::size_t i = 0; i < k; ++i) a_[dest++] = std::move(a_[pb++]);
              nb -= k;
              if (nb == 0) return End::kDone;
            }
            a_[dest++] = std::move(scratch_[pa++]);
            if (--na == 1) return End::kCopyLast;
          } while (acount >= kMinGallop || bcount >= kMinGallop);
          ++min_gallop;
          min_gallop_ = min_gallop;
        }
      }();
    } catch (...) {
      for (; na; --na) a_[dest++] = std::move(scratch_[pa++]);
      throw;
    }
    if (end == End::kDone) {
      for (; na; --na) a_[dest++] = std::move(scratch_[pa++]);
      return;
    }
    // a's last item goes after what remains of b.
    for (; nb; --nb) a_[dest++] = std::move(a_[pb++]);
    a_[dest] = std::move(scratch_[pa]);
  }

  // Merges a = [sa, sa + na) and b = [sb, sb + nb) (adjacent) from the
  // right, with b moved to scratch space.
  void merge_high(std::size_t sa, std::size_t na, std::size_t sb, std::size_t nb) {
    scratch_.clear();
    scratch_.reserve(nb);
    for (std::size_t i = 0; i < nb; ++i) scratch_.push_back(std::move(a_[sb + i]));
    // a's remaining items are [sa, sa + na), b's are scratch_[0, nb); dest
    // is the highest free slot.
    std::size_t dest = sb + nb - 1;
    const auto last_a = [&] { return sa + na - 1; };
    End end = End::kDone;
    try {
      end = [&]() -> End {
        a_[dest--] = std::move(a_[last_a()]);
        if (--na == 0) return End::kDone;
        if (nb == 1) return End::kCopyLast;
        std::size_t min_gallop = min_gallop_;
        for (;;) {
          std::size_t acount = 0, bcount = 0;
          for (;;) {
            if (lt(scratch_[nb - 1], a_[last_a()])) {
              a_[dest--] = std::move(a_[last_a()]);
              ++acount;
              bcount = 0;
              if (--na == 0) return End::kDone;
              if (acount >= min_gallop) break;
            } else {
              a_[dest--] = std::move(scratch_[nb - 1]);
              ++bcount;
              acount = 0;
              if (--nb == 1) return End::kCopyLast;
              if (bcount >= min_gallop) break;
            }
          }
          ++min_gallop;
          do {
            min_gallop -= min_gallop > 1;
            min_gallop_ = min_gallop;
            std::size_t k = na - gallop_right(scratch_[nb - 1], a_.data() + sa, na, na - 1);
            acount = k;
            if (k) {
              for (std::size_t i = 0; i < k; ++i) a_[dest--] = std::move(a_[last_a() - i]);
              na -= k;
              if (na == 0) return End::kDone;
            }
            a_[dest--] = std::move(scratch_[nb - 1]);
            if (--nb == 1) return End::kCopyLast;
            k = nb - gallop_left(a_[last_a()], scratch_.data(), nb, nb - 1);
            bcount = k;
            if (k) {
              for (std::size_t i = 0; i < k; ++i) a_[dest--] = std::move(scratch_[nb - 1 - i]);
              nb -= k;
              if (nb == 1) return End::kCopyLast;
              if (nb == 0) return End::kDone;
            }
            a_[dest--] = std::move(a_[last_a()]);
            if (--na == 0) return End::kDone;
          } while (acount >= kMinGallop || bcount >= kMinGallop);
          ++min_gallop;
          min_gallop_ = min_gallop;
        }
      }();
    } catch (...) {
      for (; nb; --nb) a_[dest--] = std::move(scratch_[nb - 1]);
      throw;
    }
    if (end == End::kDone) {
      for (; nb; --nb) a_[dest--] = std::move(scratch_[nb - 1]);
      return;
    }
    // b's first item goes before what remains of a.
    for (; na; --na) a_[dest--] = std::move(a_[last_a()]);
    a_[dest] = std::move(scratch_[0]);
  }

  std::vector<T>& a_;
  Less& less_;
  std::size_t min_gallop_ = kMinGallop;
  std::vector<Run> pending_;
  std::vector<T> scratch_;
};

// CPython's list.sort(reverse=...): `less(a, b)` is Python's `a < b` and may
// throw; the first comparison that throws is the reference's. reverse=True
// reverses the list, sorts stably and reverses again, so equal items keep
// their input order.
template <typename T, typename Less>
void python_list_sort(std::vector<T>& items, Less&& less, bool reverse = false) {
  if (reverse) std::reverse(items.begin(), items.end());
  PythonListSort<T, std::remove_reference_t<Less>>(items, less).run();
  if (reverse) std::reverse(items.begin(), items.end());
}

}  // namespace minera::detail
