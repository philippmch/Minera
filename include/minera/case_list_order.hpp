#pragma once

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "minera/conformance.hpp"

namespace minera {

// pm4py's frame algorithms list their per-case results in the case-id order
// of the data they ran on (case_id_order). These put a per-case list that a
// native call returned in log order (entry i for trace i of the log it was
// computed from) into that log's order. Callers pass the order of the very
// log the list was computed from, never an order made for another log.

// Reorders `list` by `order` (a permutation of its positions). A list of
// another length is left as it is.
template <class T>
void apply_case_order(std::vector<T>& list, const std::vector<std::size_t>& order) {
  if (order.size() != list.size()) return;
  std::vector<T> ordered;
  ordered.reserve(list.size());
  for (const auto index : order) ordered.push_back(std::move(list[index]));
  list = std::move(ordered);
}

// The token-replay trace diagnostics of `result` in `order`, when they are
// indexed by the log's traces (entry i has trace_index i); every entry keeps
// its own trace_index and case_id. No order (null) leaves them as they are.
inline void order_trace_diagnostics(ConformanceResult& result,
                                    const std::optional<std::vector<std::size_t>>& order) {
  if (!order || !result.trace_diagnostics) return;
  auto& rows = *result.trace_diagnostics;
  if (rows.size() != order->size()) return;
  for (std::size_t i = 0; i < rows.size(); ++i)
    if (rows[i].trace_index != i) return;
  apply_case_order(rows, *order);
}

}  // namespace minera
