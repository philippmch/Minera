#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "minera/directly_follows_graph.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/process_tree.hpp"

namespace minera::inductive_detail {

using ActivityId = std::size_t;
using ActivityLabels = std::vector<std::string>;

struct WeightedTrace {
  std::vector<ActivityId> activities;
  std::uint64_t count = 1;

  [[nodiscard]] bool empty() const noexcept;
};

using WeightedSublog = std::vector<WeightedTrace>;

struct InductiveDataStructure {
  WeightedSublog variants;

  [[nodiscard]] bool empty() const noexcept;
};

struct InductiveDfg {
  std::vector<ActivityId> to_global;
  std::unordered_map<ActivityId, std::size_t> to_local;
  std::vector<std::vector<char>> follows;
  std::vector<std::vector<std::size_t>> succ;
  std::vector<char> is_start;
  std::vector<char> is_end;
  std::vector<std::vector<std::uint64_t>> edge_count;
  std::vector<std::uint64_t> start_count;
  std::vector<std::uint64_t> end_count;
  bool skip = false;

  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool has_edge(ActivityId source, ActivityId target) const;
};

struct BaseCaseResult {
  std::uint64_t empty_weight = 0;
  std::uint64_t nonempty_weight = 0;

  [[nodiscard]] bool has_empty() const noexcept;
  [[nodiscard]] bool all_empty() const noexcept;
};

class EmptyLogBaseCase {
 public:
  [[nodiscard]] static bool detects(const WeightedSublog& log);
};

class BaseCaseFactory {
 public:
  [[nodiscard]] static ProcessTreeNode empty();
  [[nodiscard]] static ProcessTreeNode single(ActivityId activity,
                                               bool repeated);
};

class SingleActivityBaseCase {
 public:
  [[nodiscard]] static std::optional<ProcessTreeNode> resolve(
      const WeightedSublog& log, const InductiveDfg& dfg);
};

class BaseCase {
 public:
  [[nodiscard]] static BaseCaseResult inspect(const WeightedSublog& log);
};

enum class CutKind {
  kExclusive,
  kSequence,
  kParallel,
  kLoop,
};

struct Cut {
  CutKind kind = CutKind::kExclusive;
  std::vector<std::vector<ActivityId>> groups;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] ProcessTreeOperator operation() const noexcept;
};

class CutUtils {
 public:
  [[nodiscard]] static std::vector<std::vector<char>> reachability(
      const InductiveDfg& dfg);
};

class ExclusiveChoiceCut {
 public:
  [[nodiscard]] static std::optional<Cut> detect(const InductiveDfg& dfg);
};

class SequenceCut {
 public:
  [[nodiscard]] static std::optional<Cut> detect(const InductiveDfg& dfg);
  [[nodiscard]] static std::optional<Cut> detect_strict(
      const InductiveDfg& dfg);
};

class ConcurrencyCut {
 public:
  [[nodiscard]] static std::optional<Cut> detect(const InductiveDfg& dfg);
};

class LoopCut {
 public:
  [[nodiscard]] static std::optional<Cut> detect(const InductiveDfg& dfg);
};

class CutFactory {
 public:
  [[nodiscard]] static std::optional<Cut> find(
      const InductiveDfg& dfg, const InductiveMinerOptions& options);
};

struct EmptyTraceSplitResult {
  WeightedSublog nonempty;
  std::uint64_t empty_weight = 0;
  std::uint64_t nonempty_weight = 0;

  [[nodiscard]] bool has_empty() const noexcept;
};

struct FallThrough {
  std::optional<ProcessTreeNode> node;

  [[nodiscard]] bool handled() const noexcept;
};

class ActivityConcurrentFallThrough {
 public:
  [[nodiscard]] static std::optional<ProcessTreeNode> apply(
      const WeightedSublog& log, const InductiveDfg& dfg, double threshold,
      const InductiveMinerOptions& options, const ActivityLabels& labels);
};

class ActivityOncePerTraceFallThrough {
 public:
  [[nodiscard]] static std::optional<ProcessTreeNode> apply(
      const WeightedSublog& log, const InductiveDfg& dfg, double threshold,
      const InductiveMinerOptions& options, const ActivityLabels& labels);
};

class EmptyTraceFallThrough {
 public:
  [[nodiscard]] static EmptyTraceSplitResult split(
      const WeightedSublog& log);
};

class FlowerFallThrough {
 public:
  [[nodiscard]] static ProcessTreeNode build(const std::vector<ActivityId>& activities,
                                             const ActivityLabels& labels);
};

class StrictTauLoopFallThrough {
 public:
  [[nodiscard]] static WeightedSublog project(const WeightedSublog& log,
                                              const InductiveDfg& dfg);
};

class TauLoopFallThrough {
 public:
  [[nodiscard]] static WeightedSublog project(const WeightedSublog& log,
                                              const InductiveDfg& dfg);
};

class FallThroughFactory {
 public:
  [[nodiscard]] static FallThrough apply(
      const WeightedSublog& log, const InductiveDfg& dfg, double threshold,
      const InductiveMinerOptions& options, const ActivityLabels& labels);
};

class InductiveMinerLogVariant {
 public:
  [[nodiscard]] static ProcessTree discover(
      const InductiveDataStructure& data, const InductiveMinerOptions& options,
      const ActivityLabels& labels);
};

class InductiveMinerInfrequentVariant {
 public:
  [[nodiscard]] static double threshold(
      const InductiveMinerOptions& options);
  [[nodiscard]] static ProcessTree discover(
      const InductiveDataStructure& data, const InductiveMinerOptions& options,
      const ActivityLabels& labels);
};

class InductiveMinerDfgVariant {
 public:
  [[nodiscard]] static ProcessTree discover(
      const InductiveDataStructure& data, const InductiveMinerOptions& options,
      const ActivityLabels& labels);
  [[nodiscard]] static ProcessTree discover(
      const DirectlyFollowsGraph& graph,
      const InductiveMinerOptions& options);
};

class InductiveMinerInstances {
 public:
  [[nodiscard]] static bool supports(InductiveMinerVariant variant) noexcept;
  [[nodiscard]] static std::vector<InductiveMinerVariant> all();
};

class InductiveMinerFramework {
 public:
  [[nodiscard]] static ProcessTree discover(
      const InductiveDataStructure& data, const InductiveMinerOptions& options,
      const ActivityLabels& labels);
};

}  // namespace minera::inductive_detail
