#pragma once
// The row order of pandas' (3.0) inner merge on one key column, observed
// black-box and fuzzed against DataFrame.merge over every dtype pair the
// filters and link analysis meet (rounds DQ and DR, harness dq/mo and dr/la).
//
// A key is its label text (equal for values Python finds equal) or nullopt
// for a missing key, which matches a missing key and is labelled last.
//
// - When both key arrays are non-decreasing (no missing key, every pair of
//   neighbours ordered) and one of them holds no key twice, the merge keeps
//   plain order: by left row, then right row (round DR).
// - Otherwise a numeric key (numbers, bools, datetimes, masked numbers)
//   labels the right side first; a right side of distinct present keys
//   merges in plain order. A str or object key labels the left side first.
// - The rows are grouped by key label (a missing key last), left rows in
//   label order and each one's right rows in row order; when that gives
//   exactly as many rows as the left side has, they are put back by the
//   left rows' label order (rows of keys without a match shift the rest),
//   otherwise they are ordered by left row.
#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace minera::detail {

using PandasMergeKey = std::optional<std::string>;

inline bool pandas_merge_keys_unique(const std::vector<PandasMergeKey>& keys) {
  std::unordered_set<std::string> seen;
  for (const auto& key : keys)
    if (!key || !seen.insert(*key).second) return false;
  return true;
}

// The (left row, right row) pairs of the merge in pandas' order. The caller
// says whether each side's keys are non-decreasing in its dtype's order
// (false whenever a key is missing or a pair of keys does not compare).
inline std::vector<std::pair<std::size_t, std::size_t>> pandas_inner_merge_order(
    const std::vector<PandasMergeKey>& lk, const std::vector<PandasMergeKey>& rk, bool numeric,
    bool left_monotonic = false, bool right_monotonic = false) {
  std::vector<std::pair<std::size_t, std::size_t>> result;
  if (left_monotonic && right_monotonic && (pandas_merge_keys_unique(lk) || pandas_merge_keys_unique(rk))) {
    std::unordered_map<std::string, std::vector<std::size_t>> rows;
    for (std::size_t j = 0; j < rk.size(); ++j)
      if (rk[j]) rows[*rk[j]].push_back(j);
    for (std::size_t i = 0; i < lk.size(); ++i) {
      if (!lk[i]) continue;
      const auto it = rows.find(*lk[i]);
      if (it == rows.end()) continue;
      for (const auto j : it->second) result.emplace_back(i, j);
    }
    return result;
  }
  std::unordered_map<std::string, std::size_t> labels;
  const auto label = [&](const PandasMergeKey& k) -> std::optional<std::size_t> {
    if (!k) return std::nullopt;
    const auto it = labels.find(*k);
    if (it != labels.end()) return it->second;
    const auto n = labels.size();
    labels.emplace(*k, n);
    return n;
  };
  if (numeric) {
    for (const auto& k : rk) label(k);
    if (labels.size() == rk.size()) {
      for (std::size_t i = 0; i < lk.size(); ++i)
        if (lk[i]) {
          const auto it = labels.find(*lk[i]);
          if (it != labels.end()) result.emplace_back(i, it->second);
        }
      return result;
    }
  }
  std::vector<std::optional<std::size_t>> left;
  for (const auto& k : lk) left.push_back(label(k));
  std::vector<std::optional<std::size_t>> right;
  for (const auto& k : rk) right.push_back(label(k));
  const auto na = labels.size();
  std::vector<std::size_t> llab;
  for (const auto& l : left) llab.push_back(l.value_or(na));
  std::vector<std::vector<std::size_t>> rows(na + 1);
  for (std::size_t j = 0; j < rk.size(); ++j) rows[right[j].value_or(na)].push_back(j);
  std::vector<std::size_t> sorter(lk.size());
  for (std::size_t i = 0; i < sorter.size(); ++i) sorter[i] = i;
  std::stable_sort(sorter.begin(), sorter.end(), [&](std::size_t a, std::size_t b) { return llab[a] < llab[b]; });
  for (const auto i : sorter)
    for (const auto j : rows[llab[i]]) result.emplace_back(i, j);
  if (result.size() == lk.size()) {
    std::vector<std::pair<std::size_t, std::size_t>> placed(result.size());
    for (std::size_t q = 0; q < result.size(); ++q) placed[sorter[q]] = result[q];
    return placed;
  }
  std::stable_sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  return result;
}

// Whether `values` never decrease under `compare` (a three-way comparison
// that throws when two values do not order); false if any is missing.
template <typename T, typename Missing, typename Compare>
bool pandas_keys_monotonic(const std::vector<T>& values, Missing missing, Compare compare) {
  for (const auto& value : values)
    if (missing(value)) return false;
  try {
    for (std::size_t i = 1; i < values.size(); ++i)
      if (compare(values[i], values[i - 1]) < 0) return false;
  } catch (...) {
    return false;
  }
  return true;
}

}  // namespace minera::detail
