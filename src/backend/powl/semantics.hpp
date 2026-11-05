#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "minera/powl.hpp"
#include "minera/powl_discovery.hpp"

namespace minera::powl_detail {

using Sequence = std::vector<std::string>;

struct SequenceHash {
  std::size_t operator()(const Sequence& sequence) const noexcept {
    std::size_t hash = 1469598103934665603ull;
    for (const auto& activity : sequence) {
      for (const unsigned char byte : activity) {
        hash ^= static_cast<std::size_t>(byte);
        hash *= 1099511628211ull;
      }
      hash ^= 0xffu;
      hash *= 1099511628211ull;
    }
    return hash;
  }
};

// The discovery implementation works on a compressed variant log.  This is
// intentionally an internal type: public callers continue to use EventLog.
struct VariantLog {
  using Entry = std::pair<Sequence, std::uint64_t>;

  std::vector<Entry> entries;
  std::unordered_map<Sequence, std::size_t, SequenceHash> index;

  [[nodiscard]] bool empty() const noexcept { return entries.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return entries.size(); }
  [[nodiscard]] auto begin() const noexcept { return entries.begin(); }
  [[nodiscard]] auto end() const noexcept { return entries.end(); }

  friend bool operator==(const VariantLog& left,
                         const VariantLog& right) noexcept {
    if (left.entries.size() != right.entries.size()) return false;
    for (const auto& [sequence, count] : left.entries) {
      const auto found = right.index.find(sequence);
      if (found == right.index.end() ||
          right.entries[found->second].second != count)
        return false;
    }
    return true;
  }
};

using UVCL = VariantLog;

struct State {
  PowlDiscoveryOptions opts;
  std::size_t depth = 0;
  bool probe_only = false;
  // Polled at each recursion and fall-through candidate when set.
  const std::function<bool()>* cancelled = nullptr;
};

struct BaseCaseResult {
  std::uint64_t empty_weight = 0;
  std::uint64_t nonempty_weight = 0;

  [[nodiscard]] bool has_empty() const noexcept {
    return empty_weight != 0;
  }
  [[nodiscard]] bool all_empty() const noexcept {
    return nonempty_weight == 0;
  }
};

class EmptyLogBaseCase {
 public:
  [[nodiscard]] static bool detects(const VariantLog& log);
};

class BaseCaseFactory {
 public:
  [[nodiscard]] static PowlNode empty();
  [[nodiscard]] static PowlNode single(const std::string& activity,
                                       bool repeated);
};

class SingleActivityBaseCase {
 public:
  [[nodiscard]] static std::optional<PowlNode> resolve(
      const VariantLog& log);
};

class BaseCase {
 public:
  [[nodiscard]] static BaseCaseResult inspect(const VariantLog& log);
};

class ExclusiveChoiceCut {
 public:
  [[nodiscard]] static std::optional<PowlNode> detect(const VariantLog& log,
                                                       State& state);
};

class SequenceCut {
 public:
  [[nodiscard]] static std::optional<PowlNode> detect(const VariantLog& log,
                                                       State& state);
};

class ConcurrencyCut {
 public:
  [[nodiscard]] static std::optional<PowlNode> detect(const VariantLog& log,
                                                       State& state);
};

class LoopCut {
 public:
  [[nodiscard]] static std::optional<PowlNode> detect(const VariantLog& log,
                                                       State& state);
};

class CutFactory {
 public:
  [[nodiscard]] static std::optional<PowlNode> find(const VariantLog& log,
                                                     State& state);
};

struct EmptyTraceSplitResult {
  VariantLog nonempty;
  std::uint64_t empty_weight = 0;
  std::uint64_t nonempty_weight = 0;

  [[nodiscard]] bool has_empty() const noexcept {
    return empty_weight != 0;
  }
};

struct FallThrough {
  std::optional<PowlNode> node;

  [[nodiscard]] bool handled() const noexcept { return node.has_value(); }
};

class ActivityConcurrentFallThrough {
 public:
  [[nodiscard]] static std::optional<PowlNode> apply(const VariantLog& log,
                                                      State& state);
};

class ActivityOncePerTraceFallThrough {
 public:
  [[nodiscard]] static std::optional<PowlNode> apply(const VariantLog& log,
                                                      State& state);
};

class EmptyTraceFallThrough {
 public:
  [[nodiscard]] static EmptyTraceSplitResult split(const VariantLog& log);
};

class FlowerFallThrough {
 public:
  [[nodiscard]] static PowlNode build(
      const std::vector<std::string>& activities);
};

class StrictTauLoopFallThrough {
 public:
  [[nodiscard]] static VariantLog project(const VariantLog& log);
};

class TauLoopFallThrough {
 public:
  [[nodiscard]] static VariantLog project(const VariantLog& log);
};

class FallThroughFactory {
 public:
  [[nodiscard]] static FallThrough apply(const VariantLog& log,
                                          State& state);
};

class PowlFiltering {
 public:
  [[nodiscard]] static std::optional<VariantLog> apply(
      const VariantLog& log, const PowlDiscoveryOptions& options);
};

class BruteForcePartialOrderCut {
 public:
  [[nodiscard]] static std::optional<PowlNode> detect(const VariantLog& log,
                                                       State& state);
};

class BruteForceCutFactory {
 public:
  [[nodiscard]] static std::optional<PowlNode> find(const VariantLog& log,
                                                     State& state);
};

class DynamicClusteringFrequencyPartialOrderCut {
 public:
  [[nodiscard]] static std::optional<PowlNode> detect(
      const VariantLog& log, State& state);
};

class DynamicClusteringFrequencyCutFactory {
 public:
  [[nodiscard]] static std::optional<PowlNode> find(const VariantLog& log,
                                                     State& state);
};

class DynamicClusteringPartialOrderCut {
 public:
  [[nodiscard]] static std::optional<PowlNode> detect(const VariantLog& log,
                                                       State& state);
};

class DynamicClusteringCutFactory {
 public:
  [[nodiscard]] static std::optional<PowlNode> find(const VariantLog& log,
                                                     State& state);
};

class MaximalPartialOrderCut {
 public:
  [[nodiscard]] static std::optional<PowlNode> detect(const VariantLog& log,
                                                       State& state);
};

class MaximalCutFactory {
 public:
  [[nodiscard]] static std::optional<PowlNode> find(const VariantLog& log,
                                                     State& state);
};

class PowlTreeVariant {
 public:
  [[nodiscard]] static PowlNode discover(const VariantLog& log, State& state);
};

class PowlBruteForceVariant {
 public:
  [[nodiscard]] static PowlNode discover(const VariantLog& log, State& state);
};

class PowlMaximalVariant {
 public:
  [[nodiscard]] static PowlNode discover(const VariantLog& log, State& state);
};

class PowlDynamicClusteringVariant {
 public:
  [[nodiscard]] static PowlNode discover(const VariantLog& log, State& state);
};

class PowlDynamicClusteringFrequencyVariant {
 public:
  [[nodiscard]] static PowlNode discover(const VariantLog& log, State& state);
};

struct PowlVariantDescriptor {
  PowlDiscoveryVariant variant;
  std::string name;
};

class PowlSelectorRegistry {
 public:
  [[nodiscard]] static const std::vector<PowlVariantDescriptor>& all();
  [[nodiscard]] static bool supports(PowlDiscoveryVariant variant) noexcept;
  [[nodiscard]] static PowlNode discover(const VariantLog& log, State& state);
};

}  // namespace minera::powl_detail
