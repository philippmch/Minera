#pragma once

#include <string>

#include "minera/dfg_dataframe.hpp"

namespace minera {

// Deterministic transport serializers for the three table-backed DFG
// operations. Map-backed results are emitted in their native lexical order;
// pair rows retain discovery order. Event attributes are copied recursively,
// including explicit null markers and typed/container metadata.
[[nodiscard]] std::string serialize_dfg_dataframe_json(
    const DfgTableResult& result);
[[nodiscard]] std::string serialize_dfg_partial_order_json(
    const DfgPartialOrderResult& result);
[[nodiscard]] std::string serialize_dfg_concurrent_events_json(
    const DfgConcurrentResult& result);

// Overloads keep the route-oriented name convenient for callers that dispatch
// after selecting an operation, while the explicit names above remain useful
// when the result type is part of a public API.
[[nodiscard]] inline std::string serialize_dfg_dataframe_json(
    const DfgPartialOrderResult& result) {
  return serialize_dfg_partial_order_json(result);
}
[[nodiscard]] inline std::string serialize_dfg_dataframe_json(
    const DfgConcurrentResult& result) {
  return serialize_dfg_concurrent_events_json(result);
}

// Short aliases used by transport callers that already name their route by
// operation.
[[nodiscard]] inline std::string dfg_dataframe_json(
    const DfgTableResult& result) {
  return serialize_dfg_dataframe_json(result);
}
[[nodiscard]] inline std::string dfg_partial_order_json(
    const DfgPartialOrderResult& result) {
  return serialize_dfg_partial_order_json(result);
}
[[nodiscard]] inline std::string dfg_concurrent_events_json(
    const DfgConcurrentResult& result) {
  return serialize_dfg_concurrent_events_json(result);
}
[[nodiscard]] inline std::string dfg_concurrent_json(
    const DfgConcurrentResult& result) {
  return serialize_dfg_concurrent_events_json(result);
}

}  // namespace minera
