#pragma once

#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "minera/ocel.hpp"

namespace minera {

enum class OcelSplitVariant {
  kConnectedComponents,
  kAncestorsDescendants,
};

enum class OcelEnrichmentGraph {
  kInteraction,
  kDescendants,
  kInheritance,
  kCobirth,
  kCodeath,
  INTERACTION = kInteraction,
  DESCENDANTS = kDescendants,
  INHERITANCE = kInheritance,
  COBIRTH = kCobirth,
  CODEATH = kCodeath,
};

struct OcelEnrichmentOptions {
  // Each selected graph is materialized as deterministic O2O qualifiers.
  // Defaults enable the complete graph family.
  bool include_interaction = true;
  bool include_descendants = true;
  bool include_inheritance = true;
  bool include_cobirth = true;
  bool include_codeath = true;
  // Qualify every related event with creation, other, or termination while
  // preserving the original qualifier when no lifecycle position exists.
  bool include_lifecycle_qualifiers = true;
};

[[nodiscard]] Ocel enrich_ocel(
    const Ocel& log, const OcelEnrichmentOptions& options = {});

[[nodiscard]] std::string serialize_ocel_enrichment_json(
    const Ocel& log, const OcelEnrichmentOptions& options = {});

// Direct object-graph discovery exposes the five deterministic graph
// relations used by OCEL enrichment without materializing synthetic O2O
// records.  Undirected relations are returned once in lexical pair order;
// directed relations retain their source/target orientation.  object_ids
// includes isolated declared objects so callers can render complete graphs.
struct OcelObjectGraph {
  OcelEnrichmentGraph graph = OcelEnrichmentGraph::kInteraction;
  std::vector<std::string> object_ids;
  std::vector<std::pair<std::string, std::string>> edges;
};

[[nodiscard]] OcelObjectGraph discover_ocel_object_graph(
    const Ocel& log, OcelEnrichmentGraph graph);

[[nodiscard]] std::string serialize_ocel_object_graph_json(
    const OcelObjectGraph& graph);

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
