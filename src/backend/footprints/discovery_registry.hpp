#pragma once

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "minera/dfg_dataframe.hpp"
#include "minera/footprints.hpp"
#include "minera/log_conversion.hpp"

namespace minera::footprints_detail {

// Parameters shared by the log/table adapters.  EventLog callers normally
// need only activity_key; table callers may select the case, timestamps, and
// stable source index explicitly.
struct DiscoveryParameters {
  std::string activity_key = "concept:name";
  std::string case_id_key = "case:concept:name";
  std::string timestamp_key = "time:timestamp";
  std::optional<std::string> start_timestamp_key;
  std::string index_key = "@@index";
  bool sort_required = true;
  // An omitted marking means one token at the net source.  When supplied, its
  // length must equal the number of places in the net.
  std::optional<std::vector<std::uint64_t>> initial_marking;
  // Petri reachability: names in index order for the eventually-enabled scan.
  std::vector<std::string> transition_names;
};

class DfgFootprintsDiscovery {
 public:
  [[nodiscard]] static Footprint apply(const DirectlyFollowsGraph& graph);
  [[nodiscard]] static Footprint apply(
      const std::map<FootprintPair, std::uint64_t>& dfg);
};

class EntireEventLogFootprintsDiscovery {
 public:
  [[nodiscard]] static Footprint apply(
      const EventLog& log, const DiscoveryParameters& parameters = {});
};

class EntireDataframeFootprintsDiscovery {
 public:
  [[nodiscard]] static Footprint apply(
      const EventTable& table, const DiscoveryParameters& parameters = {});
};

class PolarsLazyframesFootprintsDiscovery {
 public:
  [[nodiscard]] static Footprint apply(
      const LazyEventTable& table, const DiscoveryParameters& parameters = {});
};

class TraceByTraceFootprintsDiscovery {
 public:
  [[nodiscard]] static std::vector<Footprint> apply(
      const EventLog& log, const DiscoveryParameters& parameters = {});
};

class PetriReachabilityFootprintsDiscovery {
 public:
  [[nodiscard]] static Footprint apply(
      const PetriNet& net, const DiscoveryParameters& parameters = {});
};

class ProcessTreeBottomUpFootprintsDiscovery {
 public:
  [[nodiscard]] static Footprint apply(const ProcessTree& tree);
};

class PowlBottomUpFootprintsDiscovery {
 public:
  [[nodiscard]] static Footprint apply(const PowlNode& model);
};

enum class FootprintsDiscoverySelector {
  kEntireEventLog,
  kEntireDataframe,
  kTraceByTrace,
  kPetriReachGraph,
  kProcessTree,
  kPowl,
  kDfg,
  kPolarsLazyframes,

  ENTIRE_EVENT_LOG = kEntireEventLog,
  ENTIRE_DATAFRAME = kEntireDataframe,
  TRACE_BY_TRACE = kTraceByTrace,
  PETRI_REACH_GRAPH = kPetriReachGraph,
  PROCESS_TREE = kProcessTree,
  POWL = kPowl,
  DFG = kDfg,
  POLARS_LAZYFRAMES = kPolarsLazyframes,
};

struct FootprintsDiscoverySelectorDescriptor {
  FootprintsDiscoverySelector selector;
  std::string name;
};

using FootprintsDiscoveryResult =
    std::variant<Footprint, std::vector<Footprint>>;

class FootprintsDiscoverySelectorRegistry {
 public:
  [[nodiscard]] static const std::vector<
      FootprintsDiscoverySelectorDescriptor>& all();
  [[nodiscard]] static bool supports(
      FootprintsDiscoverySelector selector) noexcept;
  [[nodiscard]] static std::optional<FootprintsDiscoverySelector> parse(
      std::string_view name);

  [[nodiscard]] static FootprintsDiscoveryResult dispatch(
      const EventLog& log, FootprintsDiscoverySelector selector,
      const DiscoveryParameters& parameters = {});
  [[nodiscard]] static FootprintsDiscoveryResult dispatch(
      const EventTable& table, FootprintsDiscoverySelector selector,
      const DiscoveryParameters& parameters = {});
  [[nodiscard]] static FootprintsDiscoveryResult dispatch(
      const LazyEventTable& table, FootprintsDiscoverySelector selector,
      const DiscoveryParameters& parameters = {});
  [[nodiscard]] static FootprintsDiscoveryResult dispatch(
      const DirectlyFollowsGraph& graph,
      FootprintsDiscoverySelector selector);
  [[nodiscard]] static FootprintsDiscoveryResult dispatch(
      const std::map<FootprintPair, std::uint64_t>& dfg,
      FootprintsDiscoverySelector selector);
  [[nodiscard]] static FootprintsDiscoveryResult dispatch(
      const ProcessTree& tree, FootprintsDiscoverySelector selector);
  [[nodiscard]] static FootprintsDiscoveryResult dispatch(
      const PowlNode& model, FootprintsDiscoverySelector selector);
  [[nodiscard]] static FootprintsDiscoveryResult dispatch(
      const PetriNet& net, FootprintsDiscoverySelector selector,
      const DiscoveryParameters& parameters = {});
};

}  // namespace minera::footprints_detail
