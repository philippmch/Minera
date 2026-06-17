#pragma once

#include "minera/event_log.hpp"
#include "minera/process_tree.hpp"

namespace minera {

// Discovers a block-structured process tree from an event log using the
// Inductive Miner. The algorithm recursively detects exclusive-choice,
// sequence, parallel and loop cuts on the directly-follows graph of the
// (sub)log, splits the log accordingly, and recurses; logs without a cut fall
// through to a flower model so the result always fits the log.
[[nodiscard]] ProcessTree discover_process_tree(const EventLog& event_log);

}  // namespace minera
