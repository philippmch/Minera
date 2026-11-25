#pragma once

// CPython 3.11's hash of a str, number, None or datetime and the iteration
// order of a set of such objects (round HF, observed black-box under
// PYTHONHASHSEED=0): what sorted(set(...)) of a legacy XES log's activity
// objects compares first.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "python_scalar.hpp"
#include "python_str_set.hpp"

namespace minera::detail {

// A date's hash as pandas' Timestamp (and datetime.datetime) hashes it: an
// instant with nanoseconds as the int of its value, else the tuple hash of
// the timedelta (days, seconds, microseconds) of its UTC instant since
// 0001-01-01.
inline std::uint64_t python_timestamp_hash(const ParsedExactTimestamp& date) {
  const auto int_hash = [](std::int64_t v) -> std::uint64_t {
    constexpr std::uint64_t modulus = (std::uint64_t(1) << 61) - 1;
    const std::uint64_t magnitude = v < 0 ? std::uint64_t(0) - static_cast<std::uint64_t>(v) : static_cast<std::uint64_t>(v);
    std::int64_t h = static_cast<std::int64_t>(magnitude % modulus);
    if (v < 0) h = -h;
    if (h == -1) h = -2;
    return static_cast<std::uint64_t>(h);
  };
  // A Timestamp with nanoseconds has the ns unit, whose values fit int64.
  if (date.nanoseconds % 1000 != 0 && date.seconds > -9223372036 && date.seconds < 9223372036)
    return int_hash(date.seconds * 1000000000 + static_cast<std::int64_t>(date.nanoseconds));
  std::int64_t days = date.seconds / 86400, seconds = date.seconds % 86400;
  if (seconds < 0) {
    seconds += 86400;
    --days;
  }
  const std::int64_t items[3] = {days + 719163, seconds, static_cast<std::int64_t>(date.nanoseconds / 1000)};
  constexpr std::uint64_t p1 = 11400714785074694791ULL, p2 = 14029467366897019727ULL, p5 = 2870177450012600261ULL;
  std::uint64_t acc = p5;
  for (const auto item : items) {
    acc += int_hash(item) * p2;
    acc = (acc << 31) | (acc >> 33);
    acc *= p1;
  }
  acc += 3 ^ (p5 ^ 3527539ULL);
  if (acc == ~std::uint64_t(0)) acc = 1546275796;
  return acc;
}

// hash(None) in the pinned interpreter: None's address, a static object of
// its binary (the same in every process observed).
inline constexpr std::uint64_t kPythonNoneHash = 600792;

// hash() of the object; nullopt for a str that is not valid UTF-8 (or a kind
// outside this model). A NaN hashes by its address in CPython: Minera takes
// numeric_hash's stand-in (declared).
inline std::optional<std::uint64_t> python_object_hash(const python::Scalar& value) {
  using K = python::Scalar::Kind;
  switch (value.kind) {
    case K::kBool:
    case K::kInt:
    case K::kFloat: return python::numeric_hash(value);
    case K::kText: return python_str_hash_seed0(value.text);
    case K::kNone: return kPythonNoneHash;
    case K::kDate: return python_timestamp_hash(value.date);
    default: return std::nullopt;
  }
}

// The indices of `values` in the order iterating a set built by adding them
// in order gives, `identities` telling equal objects (the first kept);
// nullopt when a hash is outside the model.
inline std::vector<std::size_t> python_hash_set_order(const std::vector<std::uint64_t>& hashes,
                                                      const std::vector<std::string>& identities);

inline std::optional<std::vector<std::size_t>> python_object_set_order(const std::vector<python::Scalar>& values,
                                                                       const std::vector<std::string>& identities) {
  std::vector<std::uint64_t> hashes;
  hashes.reserve(values.size());
  for (const auto& value : values) {
    const auto hash = python_object_hash(value);
    if (!hash) return std::nullopt;
    hashes.push_back(*hash);
  }
  return python_hash_set_order(hashes, identities);
}

// The same order from the objects' hashes (round HI: pandas' NaT, whose hash
// is the int64 minimum, beside Timestamps).
inline std::vector<std::size_t> python_hash_set_order(const std::vector<std::uint64_t>& hashes,
                                                      const std::vector<std::string>& identities) {
  struct Slot {
    std::size_t index = 0;
    std::uint64_t hash = 0;
    bool used = false;
  };
  std::vector<Slot> table(8);
  std::size_t fill = 0;
  const auto probe = [](std::vector<Slot>& slots, std::uint64_t hash, const auto& stop) {
    const std::size_t mask = slots.size() - 1;
    std::uint64_t perturb = hash;
    std::size_t i = static_cast<std::size_t>(hash) & mask;
    while (true) {
      std::size_t probes = i + 9 <= mask ? 9 : 0;
      for (std::size_t j = i;; ++j) {
        if (stop(slots[j], j)) return;
        if (probes == 0) break;
        --probes;
      }
      perturb >>= 5;
      i = (i * 5 + 1 + static_cast<std::size_t>(perturb)) & mask;
    }
  };
  for (std::size_t index = 0; index < hashes.size(); ++index) {
    const std::optional<std::uint64_t> hash = hashes[index];
    bool added = false;
    probe(table, *hash, [&](Slot& slot, std::size_t) {
      if (!slot.used) {
        slot = {index, *hash, true};
        added = true;
        return true;
      }
      return slot.hash == *hash && identities[slot.index] == identities[index];
    });
    if (!added || ++fill * 5 < (table.size() - 1) * 3) continue;
    std::size_t size = 8;
    while (size <= fill * 4) size <<= 1;
    std::vector<Slot> grown(size);
    for (const auto& old : table)
      if (old.used)
        probe(grown, old.hash, [&](Slot& slot, std::size_t) {
          if (slot.used) return false;
          slot = old;
          return true;
        });
    table = std::move(grown);
  }
  std::vector<std::size_t> order;
  for (const auto& slot : table)
    if (slot.used) order.push_back(slot.index);
  return order;
}

}  // namespace minera::detail
