#pragma once

#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "minera/event_log.hpp"
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

// A heterogeneous graph keeps event, object, and optional object-change
// records as typed nodes, with relation edges retained in input order.
struct OcelGraphNode {
  std::string id;
  std::string type;
  std::string activity;
  std::string timestamp;
  std::string object_type;
  std::string object_id;
  std::string field;
  std::vector<OcelAttribute> attributes;
  std::optional<OcelValue> value;
};

struct OcelGraphEdge {
  std::string source;
  std::string target;
  std::string type;
  std::string qualifier;
  std::string object;
};

struct OcelHeterogeneousGraphOptions {
  bool include_df = true;
  bool include_object_changes = true;
};

struct OcelHeterogeneousGraph {
  bool include_df = true;
  bool include_object_changes = true;
  std::vector<OcelGraphNode> nodes;
  std::vector<OcelGraphEdge> edges;
};

[[nodiscard]] OcelHeterogeneousGraph discover_ocel_heterogeneous_graph(
    const Ocel& log,
    const OcelHeterogeneousGraphOptions& options = {});

[[nodiscard]] std::string serialize_ocel_heterogeneous_graph_json(
    const OcelHeterogeneousGraph& graph);

struct OcelFlattenOptions {
  // Required object type.  An all-whitespace value is invalid; an unknown
  // non-empty type produces an empty EventLog.
  std::string object_type;
  // Optional event attribute selectors.  Empty selectors use the canonical
  // OcelEvent activity/timestamp fields; non-empty selectors name scalar
  // event attributes (or their canonical aliases).
  std::string event_activity_attribute;
  std::string event_timestamp_attribute;
};

// Projects one OCEL object type into a conventional event log.  Referenced
// objects of the selected type become traces, and each event-object relation
// remains one event occurrence, including duplicates.  The source event
// order is the primary output order, with object-table order and relation
// order as stable tie-breakers.  Event and object scalar attributes are
// retained with their native scalar kinds; selected object fields retain
// their original keys in Trace attributes.
[[nodiscard]] EventLog flatten_ocel_to_event_log(
    const Ocel& log, const OcelFlattenOptions& options);

[[nodiscard]] EventLog flatten_ocel_to_event_log(
    const Ocel& log, std::string_view object_type);

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
  std::size_t candidate_count = 0;
  bool truncated = false;
};

[[nodiscard]] OcelSplitResult split_ocel(
    const Ocel& log, const OcelSplitOptions& options = {});

[[nodiscard]] std::string serialize_ocel_split_json(
    const OcelSplitResult& result);

}  // namespace minera
