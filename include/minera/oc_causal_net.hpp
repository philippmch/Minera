#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "minera/ocel.hpp"
#include "minera/ocel_playout.hpp"

namespace minera {

// The maximum value denotes an unbounded marker cardinality in the native
// representation. JSON serialization keeps that value numeric so callers
// can round-trip the model without a special floating-point value.
inline constexpr std::size_t kOcCausalInfiniteCardinality =
    std::numeric_limits<std::size_t>::max();

struct OcCausalMarker {
  std::string related_activity;
  std::string object_type;
  std::size_t min_cardinality = 1;
  std::size_t max_cardinality = 1;
  // Markers with the same nonzero key and object type in one group cannot
  // share an object when they refer to different related activities. A zero
  // key is normalized as a unique marker key.
  std::int64_t marker_key = 0;
};

// A marker group is conjunctive: every marker in the group must be assigned
// when the group is selected. A model may expose several input or output
// groups; those groups are alternatives.
struct OcCausalMarkerGroup {
  std::vector<OcCausalMarker> markers;
  std::size_t support_count = kOcCausalInfiniteCardinality;
};

struct OcCausalActivity {
  std::string activity;
  std::vector<OcCausalMarkerGroup> input_marker_groups;
  std::vector<OcCausalMarkerGroup> output_marker_groups;
};

// The workflow projection remains available for clients that only need a
// per-object-type Petri representation. Native marker groups drive causal
// extensive playout whenever they are present.
struct OcCausalNet {
  OcpnGraph workflow_projection;
  std::vector<std::string> object_types;
  std::vector<OcCausalActivity> activities;
  // Internal activity name -> event label, for activities whose label could
  // not be used as their name (see oc_causal_net_from_ocpn). The reference
  // binding playout reports these activities by label.
  std::map<std::string, std::string> activity_labels;
};

[[nodiscard]] OcCausalNet discover_oc_causal_net(
    const Ocel& log, const OcpnDiscoveryOptions& options = {});

// Converts an object-centric Petri net into the causal net the reference
// builds with pm4py.objects.ocpn.factory.create followed by the
// ocpn converter (TO_OC_CAUSAL_NET), the net its causal-net playout runs on:
// every place becomes an activity ("<type>_<place>"), every transition an
// activity (labelled ones by label where the reference uses a random uuid,
// silent ones "<type>_<name>"), a transition with several places of one type
// on one side goes through "_silent_aux_in_/out_<transition>_<type>", and
// START_<type>/END_<type> feed the source and drain the sink places.
// Variable (double) arcs carry cardinality 0..inf, other transition arcs
// 1..1 and place arcs 1..inf. A label that collides with a generated name or
// starts with START_/END_ gets an internal "_label_<n>_<label>" name, listed in
// activity_labels.
[[nodiscard]] OcCausalNet oc_causal_net_from_ocpn(const OcpnGraph& graph);

[[nodiscard]] std::string serialize_oc_causal_net_json(
    const OcCausalNet& net);

[[nodiscard]] Ocel oc_causal_net_extensive_playout(
    const OcCausalNet& net,
    const OcpnPlayoutOptions& options = {});

[[nodiscard]] OcpnPlayoutResult oc_causal_net_extensive_playout_result(
    const OcCausalNet& net,
    const OcpnPlayoutOptions& options = {});

[[nodiscard]] std::string serialize_oc_causal_net_playout_json(
    const Ocel& log);
[[nodiscard]] std::string serialize_oc_causal_net_playout_json(
    const Ocel& log, OcpnEventBinding binding);
[[nodiscard]] std::string serialize_oc_causal_net_playout_json(
    const OcpnPlayoutResult& result, OcpnEventBinding binding);

}  // namespace minera
