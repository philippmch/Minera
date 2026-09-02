#pragma once

#include <cstddef>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "minera/footprints.hpp"

namespace minera::footprints_conformance {

// A footprint projection is the stable, activity-labelled view consumed by
// conformance and by relation-oriented utilities. Keeping this projection in
// one component prevents each selector from making a subtly different choice
// about which relation families are visible.
struct FootprintProjection {
  std::set<FootprintPair> relations;
  std::set<std::string> activities;
  std::set<std::string> start_activities;
  std::set<std::string> end_activities;
};

class FootprintRelationProjection {
 public:
  [[nodiscard]] static FootprintProjection project(
      const Footprint& footprint);

  [[nodiscard]] static std::set<FootprintPair> relation_configurations(
      const Footprint& footprint);

  [[nodiscard]] static std::set<std::string> relation_activities(
      const std::set<FootprintPair>& relations);
};

// Sparse projection of boundary deviations onto process-tree leaves. Colored
// nodes are returned in pre-order and use blue for start/end deviations.
// Relation and always-executed deviations do not produce node colors. When a
// label occurs more than once, the last leaf carrying that label is selected.
struct TreeNodeDecoration {
  std::size_t preorder_index = 0;
  ProcessTreeOperator operation = ProcessTreeOperator::kTau;
  std::string activity;
  std::string color;
};

using TreeDecorations = std::vector<TreeNodeDecoration>;

class TreeVisualizationProjection {
 public:
  [[nodiscard]] static TreeDecorations decorate(
      const ProcessTree& tree, const FootprintViolations& relation_violations,
      const std::set<std::string>& start_violations = {},
      const std::set<std::string>& end_violations = {},
      const std::set<std::string>& always_violations = {});

  [[nodiscard]] static TreeDecorations decorate(
      const ProcessTree& tree, const FootprintsExtensiveResult& result);

  // `apply` mirrors the callable support-module shape while retaining a
  // native, typed sparse result.
  [[nodiscard]] static TreeDecorations apply(
      const ProcessTree& tree, const FootprintViolations& relation_violations,
      const std::set<std::string>& start_violations = {},
      const std::set<std::string>& end_violations = {},
      const std::set<std::string>& always_violations = {});

  [[nodiscard]] static TreeDecorations apply(
      const ProcessTree& tree, const FootprintsExtensiveResult& result);
};

// Resolve the case-id selector used by the diagnostics adapters without
// changing the shape or order of the conformance result itself.
[[nodiscard]] std::string selected_case_id(const Trace& trace,
                                           std::size_t index,
                                           std::string_view case_id_key);

}  // namespace minera::footprints_conformance
