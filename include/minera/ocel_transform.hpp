#pragma once

#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "minera/ocel.hpp"

namespace minera {

enum class OcelSplitVariant {
  kConnectedComponents,
  kAncestorsDescendants,
};

struct OcelSplitOptions {
  OcelSplitVariant variant = OcelSplitVariant::kConnectedComponents;
  // Required for the ancestors/descendants variant. Objects of this type are
  // used as the central objects of the returned projections.
  std::string object_type;
  std::size_t max_objects = std::numeric_limits<std::size_t>::max();
  // The connected-components variant can optionally remove nodes whose
  // undirected degree centrality is above this bound before splitting.
  std::optional<double> max_degree_centrality;
};

struct OcelSplitResult {
  OcelSplitVariant variant = OcelSplitVariant::kConnectedComponents;
  std::vector<Ocel> logs;
  // Empty for connected components; aligned with logs for
  // ancestors/descendants projections.
  std::vector<std::string> central_objects;
};

[[nodiscard]] OcelSplitResult split_ocel(
    const Ocel& log, const OcelSplitOptions& options = {});

[[nodiscard]] std::string serialize_ocel_split_json(
    const OcelSplitResult& result);

}  // namespace minera
