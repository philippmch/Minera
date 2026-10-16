#include "minera/petri_net.hpp"

#include <utility>

namespace minera {
PetriNet PetriNet::from_literal_storage(PetriNetLiteralStorage storage) {
  PetriNet net;
  net.place_count_ = storage.place_count;
  net.source_ = storage.source_storage;
  net.sink_ = storage.sink_storage;
  net.source_set_ = storage.has_source;
  net.sink_set_ = storage.has_sink;
  net.transitions_ = std::move(storage.transitions);
  net.activities_ = std::move(storage.activities);
  net.discovery_markings_ = std::move(storage.discovery_markings);
  net.transition_names_ = std::move(storage.transition_names);
  return net;
}
}  // namespace minera
