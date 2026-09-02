#include "minera/footprints.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include "footprints/discovery_registry.hpp"

namespace minera {

namespace {

void validate_selector(std::string_view value, std::string_view name) {
  if (value.empty() || std::all_of(value.begin(), value.end(), [](char c) {
        return std::isspace(static_cast<unsigned char>(c)) != 0;
      })) {
    throw std::invalid_argument(std::string(name) +
                                " selector must not be empty");
  }
}

footprints_detail::DiscoveryParameters discovery_parameters(
    const FootprintsDiscoveryOptions& options) {
  validate_selector(options.activity_attribute, "activity");
  validate_selector(options.case_id_attribute, "case");
  validate_selector(options.timestamp_attribute, "timestamp");
  if (options.start_timestamp_attribute)
    validate_selector(*options.start_timestamp_attribute, "start timestamp");
  footprints_detail::DiscoveryParameters parameters;
  parameters.activity_key = options.activity_attribute;
  parameters.case_id_key = options.case_id_attribute;
  parameters.timestamp_key = options.timestamp_attribute;
  parameters.start_timestamp_key = options.start_timestamp_attribute;
  parameters.index_key = options.index_attribute;
  parameters.sort_required = options.sort_required;
  parameters.initial_marking = options.initial_marking;
  return parameters;
}

}  // namespace

Footprint discover_footprints(const ProcessTree& tree) {
  return footprints_detail::ProcessTreeBottomUpFootprintsDiscovery::apply(tree);
}

Footprint discover_footprints(const PowlNode& model) {
  return footprints_detail::PowlBottomUpFootprintsDiscovery::apply(model);
}

Footprint discover_footprints(const PetriNet& net) {
  return discover_footprints(net, FootprintsDiscoveryOptions{});
}

Footprint discover_footprints(const PetriNet& net,
                              const FootprintsDiscoveryOptions& options) {
  return footprints_detail::PetriReachabilityFootprintsDiscovery::apply(
      net, discovery_parameters(options));
}

}  // namespace minera
