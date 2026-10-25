#pragma once

// algo.simulation.playout.oc_causal_net (EXTENSIVE) of the PM4Py 2.7.22
// reference on a native object-centric causal net.
//
// The state is a multiset of obligations (from activity, to activity, object
// type, object). Every given object of type t starts with an obligation to
// START_t; a sequence is complete when no obligation is left (so no objects
// give the empty sequence). A binding of START_t consumes the start
// obligations of a non-empty set of its objects; a binding of another
// activity picks one input marker group and, per marker, a set of objects
// with an obligation from the marker's activity, within the marker's
// cardinality. The binding's objects of a type are the union of its markers'
// sets and must not be empty overall. It then picks one output marker group
// (none for an activity without output groups) and, per marker, a subset of
// the binding's objects of the marker's type within its cardinality, so that
// the markers of each type cover exactly the binding's objects of that type;
// the group must have a marker for every type the binding has objects of.
// On both sides, markers of one group with the same key and type take
// disjoint sets (a zero key is a key of its own). Each activity binds at most
// max_bindings_per_activity times per sequence; equal bindings count once.

#include <cstddef>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/oc_causal_net.hpp"

namespace minera {

// A reference failure: the Python exception class and its str(). Unless no
// object is given, an activity other than START_<type> without input marker
// groups raises KeyError (which one, when several, depends on the hash
// seed); an activity other than END_<type> without output marker groups
// raises KeyError once one of its input groups can be bound.
class OcCausalReferenceError : public std::invalid_argument {
 public:
  OcCausalReferenceError(std::string type, const std::string& message)
      : std::invalid_argument(message), type(std::move(type)) {}
  std::string type;
};

struct OcCausalReferenceObligations {
  std::string activity;  // the related activity
  std::string object_type;
  std::vector<std::string> objects;  // sorted, non-empty
};

struct OcCausalReferenceBinding {
  std::string activity;
  // nullopt for START_<type> (consumed) and for activities without output
  // marker groups (produced); entries are sorted.
  std::optional<std::vector<OcCausalReferenceObligations>> consumed, produced;
};

using OcCausalReferenceSequence = std::vector<OcCausalReferenceBinding>;

struct OcCausalReferencePlayoutOptions {
  std::size_t max_bindings_per_activity = 1;
  bool objects_unique_per_sequence = false;
};

// RETURN_SEQUENCES: every complete binding sequence, in search order.
// objects maps each object type to its object ids.
[[nodiscard]] std::vector<OcCausalReferenceSequence> reference_oc_causal_net_sequences(
    const OcCausalNet& net, const std::map<std::string, std::vector<std::string>>& objects,
    const OcCausalReferencePlayoutOptions& options = {});

// The default OCEL result: every binding of an activity other than
// START_<type> and END_<type>, sequence after sequence, as events
// "event_<n>" one second apart from 10,000,001 seconds after the epoch,
// with one relation per consumed obligation (an object consumed from two
// activities is related twice; renamed "<id>_<sequence>" with
// objects_unique_per_sequence).
[[nodiscard]] Ocel reference_oc_causal_net_playout(
    const OcCausalNet& net, const std::map<std::string, std::vector<std::string>>& objects,
    const OcCausalReferencePlayoutOptions& options = {});

}  // namespace minera
