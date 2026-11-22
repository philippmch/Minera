#pragma once

// Python's str hash under PYTHONHASHSEED=0 (zero-key SipHash13 over the
// string's narrowest code-point width) and the iteration order of a set of
// strings built by inserting them in a given order (open addressing with
// perturbation, as observed black-box). Shared by the Alpha miner's
// activity sets and the Polars plan texts whose column lists pm4py builds
// from a set (round FX).

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace minera::detail {

// nullopt for text that is not valid UTF-8.
inline std::optional<std::uint64_t> python_str_hash_seed0(std::string_view text) {
  if (text.empty()) return 0;
  std::vector<std::uint32_t> points;
  std::uint32_t largest = 0;
  for (std::size_t i = 0; i < text.size();) {
    const auto lead = static_cast<unsigned char>(text[i++]);
    std::uint32_t point = lead;
    std::size_t extra = 0;
    std::uint32_t minimum = 0;
    if (lead >= 0xc2 && lead <= 0xdf) {
      point = lead & 0x1f; extra = 1; minimum = 0x80;
    } else if (lead >= 0xe0 && lead <= 0xef) {
      point = lead & 0x0f; extra = 2; minimum = 0x800;
    } else if (lead >= 0xf0 && lead <= 0xf4) {
      point = lead & 7; extra = 3; minimum = 0x10000;
    } else if (lead >= 0x80) {
      return std::nullopt;
    }
    if (extra > text.size() - i) return std::nullopt;
    for (std::size_t j = 0; j < extra; ++j) {
      const auto next = static_cast<unsigned char>(text[i++]);
      if ((next & 0xc0) != 0x80) return std::nullopt;
      point = (point << 6) | (next & 0x3f);
    }
    if (point < minimum || point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff)) return std::nullopt;
    points.push_back(point);
    largest = std::max(largest, point);
  }
  const std::size_t width = largest <= 0xff ? 1 : largest <= 0xffff ? 2 : 4;
  std::vector<unsigned char> bytes;
  bytes.reserve(points.size() * width);
  for (const auto point : points)
    for (std::size_t j = 0; j < width; ++j) bytes.push_back(static_cast<unsigned char>(point >> (8 * j)));
  std::uint64_t a = 0x736f6d6570736575ULL, b = 0x646f72616e646f6dULL;
  std::uint64_t c = 0x6c7967656e657261ULL, d = 0x7465646279746573ULL;
  const auto round = [&] {
    a += b; b = std::rotl(b, 13) ^ a; a = std::rotl(a, 32);
    c += d; d = std::rotl(d, 16) ^ c;
    a += d; d = std::rotl(d, 21) ^ a;
    c += b; b = std::rotl(b, 17) ^ c; c = std::rotl(c, 32);
  };
  const auto consume = [&](std::uint64_t block) {
    d ^= block; round(); a ^= block;
  };
  const std::size_t full = bytes.size() / 8 * 8;
  for (std::size_t i = 0; i < full; i += 8) {
    std::uint64_t block = 0;
    for (std::size_t j = 0; j < 8; ++j) block |= static_cast<std::uint64_t>(bytes[i + j]) << (8 * j);
    consume(block);
  }
  std::uint64_t last = static_cast<std::uint64_t>(bytes.size() & 255) << 56;
  for (std::size_t i = full; i < bytes.size(); ++i) last |= static_cast<std::uint64_t>(bytes[i]) << (8 * (i - full));
  consume(last); c ^= 255;
  round(); round(); round();
  const auto value = a ^ b ^ c ^ d;
  return value == UINT64_MAX ? UINT64_MAX - 1 : value;
}

// The distinct `inserted` strings in the order list(set(inserted)) gives
// under PYTHONHASHSEED=0 (the first-seen order when a string is not valid
// UTF-8).
inline std::vector<std::string> python_str_set_order(const std::vector<std::string>& inserted) {
  struct Entry { std::size_t index; std::uint64_t hash; };
  std::vector<Entry> entries;
  for (std::size_t i = 0; i < inserted.size(); ++i) {
    const auto hash = python_str_hash_seed0(inserted[i]);
    if (!hash) {
      std::vector<std::string> distinct;
      for (const auto& value : inserted)
        if (std::find(distinct.begin(), distinct.end(), value) == distinct.end()) distinct.push_back(value);
      return distinct;
    }
    entries.push_back({i, *hash});
  }
  const auto insert = [&](Entry entry, std::vector<std::optional<Entry>>& table) {
    const auto mask = table.size() - 1;
    std::size_t index = static_cast<std::size_t>(entry.hash & mask);
    auto perturb = entry.hash;
    for (;;) {
      const std::size_t probes = index + 9 < table.size() ? 9 : 0;
      for (std::size_t offset = 0; offset <= probes; ++offset) {
        auto& slot = table[index + offset];
        if (!slot) { slot = entry; return true; }
        if (slot->hash == entry.hash && inserted[slot->index] == inserted[entry.index]) return false;
      }
      perturb >>= 5;
      index = (index * 5 + 1 + static_cast<std::size_t>(perturb)) & mask;
    }
  };
  std::vector<std::optional<Entry>> table(8);
  std::size_t used = 0;
  for (const auto entry : entries) {
    if (!insert(entry, table)) continue;
    ++used;
    if (used * 5 >= (table.size() - 1) * 3) {
      std::size_t size = 8;
      while (size <= used * 4) size *= 2;
      auto old = std::move(table);
      table = std::vector<std::optional<Entry>>(size);
      for (const auto& slot : old)
        if (slot) insert(*slot, table);
    }
  }
  std::vector<std::string> ordered;
  ordered.reserve(used);
  for (const auto& slot : table)
    if (slot) ordered.push_back(inserted[slot->index]);
  return ordered;
}

}  // namespace minera::detail
