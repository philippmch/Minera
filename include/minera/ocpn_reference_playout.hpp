#pragma once

// algo.simulation.playout.ocpn (EXTENSIVE) of the PM4Py 2.7.22 reference on
// a mined object-centric Petri net (one workflow net per object type, as
// produced by discover_ocpn and converted by the reference factory).
//
// The reference plays out with one object per type, named "<type>_0", with a
// token on its type's source place; a trace is complete when every object
// sits on its type's sink place and nowhere else. A visible activity is one
// transition joining every type net that has it. A binding fires it in a
// set of those nets at once: every type whose arcs on the activity are not
// double (variable) must take part, a type with double arcs may, at least
// one type takes part, and each taking part enables it for its object. A
// silent transition fires in its own type net. Every transition (silent ones
// included) fires at most `max_bindings_per_activity` times per trace. The
// search enumerates every complete firing sequence.

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "minera/ocel.hpp"

namespace minera {

struct OcpnReferenceStep {
  std::optional<std::string> activity;  // nullopt for a silent transition
  std::string object_type;              // the silent transition's type
  std::size_t transition_index = 0;     // in that type net (silent steps)
  std::vector<std::string> objects;     // sorted bound object ids
};

using OcpnReferenceTrace = std::vector<OcpnReferenceStep>;

struct OcpnReferencePlayoutOptions {
  std::size_t max_bindings_per_activity = 1;
  bool objects_unique_per_trace = false;
};

// RETURN_TRACES: every complete firing sequence, in search order.
[[nodiscard]] std::vector<OcpnReferenceTrace> reference_ocpn_extensive_traces(
    const OcpnGraph& graph, const OcpnReferencePlayoutOptions& options = {});

// Calls `visit` with every complete firing sequence, in search order, without
// keeping them (the trace is valid during the call only). The search stops
// when `visit` returns false. Returns the number of sequences visited.
std::size_t reference_ocpn_extensive_visit(
    const OcpnGraph& graph, const OcpnReferencePlayoutOptions& options,
    const std::function<bool(const OcpnReferenceTrace&)>& visit);

// EXISTS_TRACE: whether a complete firing sequence exists.
[[nodiscard]] bool reference_ocpn_trace_exists(const OcpnGraph& graph,
                                               const OcpnReferencePlayoutOptions& options = {});

// variants.utils.feasible_traces_to_ocel: the visible steps of every trace
// in order, as events "event_<n>" one second apart from 10,000,001 seconds
// after the epoch, related to their bound objects (renamed "<id>_<trace>"
// with objects_unique_per_trace), and the objects that appear. object_types
// maps every bound object id to its type.
[[nodiscard]] Ocel reference_ocpn_traces_to_ocel(const std::vector<OcpnReferenceTrace>& traces,
                                                 const std::map<std::string, std::string>& object_types,
                                                 bool objects_unique_per_trace = false);

// The same conversion one trace at a time (trace numbers count from 0 in the
// order the traces are added).
class OcpnReferenceOcelWriter {
 public:
  OcpnReferenceOcelWriter(const std::map<std::string, std::string>& object_types,
                          bool objects_unique_per_trace);
  void add(const OcpnReferenceTrace& trace);
  [[nodiscard]] Ocel take() { return std::move(log_); }

 private:
  const std::map<std::string, std::string>& object_types_;
  bool objects_unique_per_trace_;
  Ocel log_;
  std::set<std::string> seen_;
  long long clock_ = 10000001;
  std::size_t trace_ = 0;
};

// The default OCEL result: the traces converted by the function above.
[[nodiscard]] Ocel reference_ocpn_extensive_playout(const OcpnGraph& graph,
                                                    const OcpnReferencePlayoutOptions& options = {});

}  // namespace minera
