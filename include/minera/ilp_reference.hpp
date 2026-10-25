#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

// Reference-shaped classic ILP discovery (PM4Py 2.7.22
// `discover_petri_net_ilp` / `algo.discovery.ilp` CLASSIC).
//
// Every trace is wrapped in the artificial start/end activities. Unless a
// causal relation is supplied, the pairs are the one-way directly-follows
// pairs of the wrapped log in first-occurrence order. Each pair gets one LP
// relaxation over binary producer/consumer variables (x, y in [0, 1], the
// pair's producer and consumer fixed to 1): every retained prefix row
// `prefix.x - (prefix + activity).y >= 0`, every retained complete trace
// `trace.x - trace.y == 0`, minimizing the summed marking after each retained
// prefix of the distinct variants. The alpha filter keeps a continuation of a
// prefix (keyed by its occurrence vector) when its trace count is at least
// `(1 - alpha) * max` over the continuations of that prefix; a dropped
// continuation cuts the rest of the trace, including its final row. Arcs are
// the variables above 0.5. Infeasible pairs and repeated places are skipped;
// a new place is named after the place count ("2", "3", ...). Implicit places
// (PM4Py's murata reduction: a place-flow feasibility LP per place, in name
// order, never removing marked places) are removed, then silent transitions
// are merged with their single-entry/single-exit places.
//
// Most pair LPs have tied optima. The reference's choice among them follows
// its LP solver's vertex; this implementation returns some optimal vertex, so
// results agree exactly only when every pair's rounding is determined
// (see `reference_ilp_rounding_is_optimal`).
struct ReferenceIlpOptions {
  double alpha = 1.0;
  std::string artificial_start_activity = "▶";
  std::string artificial_end_activity = "■";
  // Explicit causal pairs in order (the reference's CAUSAL_RELATION keys).
  std::optional<std::vector<std::pair<std::string, std::string>>> causal_relation;
};

struct ReferenceIlpTransition {
  std::string name;
  std::optional<std::string> label;  // empty for the artificial start/end
};

struct ReferenceIlpPlace {
  std::string name;
  std::vector<std::string> inputs;   // names of transitions producing into it
  std::vector<std::string> outputs;  // names of transitions consuming from it
};

// The initial marking is one token in "source", the final marking one token
// in "sink"; every arc has weight 1.
struct ReferenceIlpNet {
  std::vector<ReferenceIlpTransition> transitions;
  std::vector<ReferenceIlpPlace> places;
};

// Thrown for the reference's NetworkXError (artificial start or end missing
// from the causal relation) and ValueError (unknown causal activity).
struct ReferenceIlpError : std::invalid_argument {
  std::string python_type;
  ReferenceIlpError(std::string type, const std::string& message)
      : std::invalid_argument(message), python_type(std::move(type)) {}
};

[[nodiscard]] ReferenceIlpNet reference_ilp_net(
    const std::vector<std::vector<std::string>>& traces,
    const ReferenceIlpOptions& options = {});

// Uses Event::activity, traces in log order.
[[nodiscard]] ReferenceIlpNet reference_ilp_net(
    const EventLog& log, const ReferenceIlpOptions& options = {});

// True when some optimal point of `pair`'s LP rounds (at 0.5, either side of
// an exact 0.5) to the place with the given producer and consumer
// transitions. Used to check reference places against the LP model when the
// optimum is tied.
[[nodiscard]] bool reference_ilp_rounding_is_optimal(
    const std::vector<std::vector<std::string>>& traces,
    const ReferenceIlpOptions& options,
    const std::pair<std::string, std::string>& pair,
    const std::vector<std::string>& inputs,
    const std::vector<std::string>& outputs);

// The pairs the reference solves for, in order.
[[nodiscard]] std::vector<std::pair<std::string, std::string>>
reference_ilp_causal_pairs(const std::vector<std::vector<std::string>>& traces,
                           const ReferenceIlpOptions& options = {});

}  // namespace minera
