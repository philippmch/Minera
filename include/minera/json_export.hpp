#pragma once

#include <ostream>
#include <string>
#include <string_view>

#include "minera/directly_follows_graph.hpp"
#include "minera/event_log.hpp"

namespace minera {

// Serializes an analysis (the imported event log plus its discovered
// directly-follows graph) into the JSON document consumed by the frontend.
void write_analysis_json(std::ostream& out,
                         std::string_view dataset,
                         const EventLog& event_log,
                         const DirectlyFollowsGraph& graph);

[[nodiscard]] std::string analysis_json(std::string_view dataset,
                                        const EventLog& event_log,
                                        const DirectlyFollowsGraph& graph);

}  // namespace minera
