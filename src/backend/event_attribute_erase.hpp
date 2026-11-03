#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "minera/event_log.hpp"

namespace minera::detail {

// Removes the generic attributes that `drop` selects and keeps the selected
// cells' source positions (Event::first_class_source_positions) pointing at
// the same neighbours, so flat adapters and the XES writer still place them
// where a dict that lost those keys would list them.
template <class Drop>
void erase_event_attributes(Event& event, Drop&& drop) {
  auto& attributes = event.attributes;
  std::vector<std::size_t> kept_before(attributes.size() + 1, 0);
  std::vector<bool> dropped(attributes.size(), false);
  for (std::size_t i = 0; i < attributes.size(); ++i) {
    dropped[i] = static_cast<bool>(drop(attributes[i]));
    kept_before[i + 1] = kept_before[i] + (dropped[i] ? 0 : 1);
  }
  if (kept_before.back() == attributes.size()) return;
  std::vector<std::pair<std::uint32_t, std::size_t>> known;
  for (std::size_t role = 0; role < event.first_class_source_positions.size(); ++role)
    if (event.first_class_source_positions[role] != Event::kUnknownSourcePosition)
      known.emplace_back(event.first_class_source_positions[role], role);
  std::sort(known.begin(), known.end());
  // The k-th known cell had (position - k) generic attributes before it.
  for (std::size_t k = 0; k < known.size(); ++k) {
    const auto before = std::min<std::size_t>(
        known[k].first >= k ? known[k].first - k : 0, attributes.size());
    event.first_class_source_positions[known[k].second] =
        static_cast<std::uint32_t>(kept_before[before] + k);
  }
  std::size_t write = 0;
  for (std::size_t read = 0; read < attributes.size(); ++read)
    if (!dropped[read]) {
      if (write != read) attributes[write] = std::move(attributes[read]);
      ++write;
    }
  attributes.resize(write);
}

}  // namespace minera::detail
