#include "conformance_tree_visualization.hpp"

#include <map>
#include <stdexcept>
#include <utility>

namespace minera::footprints_conformance {

FootprintProjection FootprintRelationProjection::project(
    const Footprint& footprint) {
  FootprintProjection result;
  result.relations = relation_configurations(footprint);
  result.activities = footprint.activities;
  result.start_activities = footprint.start_activities;
  result.end_activities = footprint.end_activities;
  const auto relation_activities_set = relation_activities(result.relations);
  result.activities.insert(relation_activities_set.begin(),
                           relation_activities_set.end());
  return result;
}

std::set<FootprintPair> FootprintRelationProjection::relation_configurations(
    const Footprint& footprint) {
  std::set<FootprintPair> result = footprint.sequence;
  result.insert(footprint.parallel.begin(), footprint.parallel.end());
  // Atomic interleaving is a native extension of the two relation families
  // represented by the reference footprints object.  It remains visible in
  // the projection so converted interleaving models are not silently treated
  // as unconstrained.
  result.insert(footprint.atomic_interleaving.begin(),
                footprint.atomic_interleaving.end());
  return result;
}

std::set<std::string> FootprintRelationProjection::relation_activities(
    const std::set<FootprintPair>& relations) {
  std::set<std::string> result;
  for (const auto& [source, target] : relations) {
    result.insert(source);
    result.insert(target);
  }
  return result;
}

namespace {

struct TreeNodeInfo {
  std::size_t preorder_index = 0;
  ProcessTreeOperator operation = ProcessTreeOperator::kTau;
  std::string activity;
};

struct TreeIndex {
  std::vector<TreeNodeInfo> nodes;
  std::map<std::string, std::size_t> last_activity_leaf;
};

TreeIndex index_tree(const ProcessTree& tree) {
  TreeIndex index;
  const auto visit = [&](const auto& self,
                         const ProcessTreeNode& node) -> std::size_t {
    const std::size_t current = index.nodes.size();
    TreeNodeInfo info;
    info.preorder_index = current;
    info.operation = node.op;
    if (node.op == ProcessTreeOperator::kActivity &&
        node.activity < tree.activities().size()) {
      info.activity = tree.activities()[node.activity];
      // A label map intentionally resolves duplicate labels to the last
      // deterministic leaf, matching the tree visualizer's label lookup.
      index.last_activity_leaf[info.activity] = current;
    }
    index.nodes.push_back(std::move(info));
    for (const auto& child : node.children) {
      (void)self(self, child);
    }
    return current;
  };

  visit(visit, tree.root());
  return index;
}

void mark_color(std::vector<std::string>& colors, std::size_t index,
                std::string_view color) {
  colors[index] = std::string(color);
}

TreeDecorations decorate_tree(const ProcessTree& tree,
                              const FootprintViolations& relation_violations,
                              const std::set<std::string>& start_violations,
                              const std::set<std::string>& end_violations,
                              const std::set<std::string>& always_violations) {
  (void)relation_violations;
  (void)always_violations;
  const auto index = index_tree(tree);
  std::vector<std::string> colors(index.nodes.size());

  const auto mark_boundary = [&](const std::set<std::string>& violations) {
    for (const auto& activity : violations) {
      const auto it = index.last_activity_leaf.find(activity);
      if (it != index.last_activity_leaf.end()) {
        mark_color(colors, it->second, "blue");
      }
    }
  };
  mark_boundary(start_violations);
  mark_boundary(end_violations);

  TreeDecorations result;
  for (const auto& node : index.nodes) {
    if (!colors[node.preorder_index].empty()) {
      result.push_back({node.preorder_index, node.operation, node.activity,
                        colors[node.preorder_index]});
    }
  }
  return result;
}

}  // namespace

TreeDecorations TreeVisualizationProjection::decorate(
    const ProcessTree& tree, const FootprintViolations& relation_violations,
    const std::set<std::string>& start_violations,
    const std::set<std::string>& end_violations,
    const std::set<std::string>& always_violations) {
  return decorate_tree(tree, relation_violations, start_violations,
                       end_violations, always_violations);
}

TreeDecorations TreeVisualizationProjection::decorate(
    const ProcessTree& tree, const FootprintsExtensiveResult& result) {
  return decorate_tree(tree, result.footprints, result.start_activities,
                       result.end_activities, result.activities_always_happening);
}

TreeDecorations TreeVisualizationProjection::apply(
    const ProcessTree& tree, const FootprintViolations& relation_violations,
    const std::set<std::string>& start_violations,
    const std::set<std::string>& end_violations,
    const std::set<std::string>& always_violations) {
  return decorate(tree, relation_violations, start_violations,
                  end_violations, always_violations);
}

TreeDecorations TreeVisualizationProjection::apply(
    const ProcessTree& tree, const FootprintsExtensiveResult& result) {
  return decorate(tree, result);
}

std::string selected_case_id(const Trace& trace, std::size_t index,
                             std::string_view case_id_key) {
  const bool canonical =
      case_id_key.empty() || case_id_key == "case_id" ||
      case_id_key == "case:id" || case_id_key == "concept:name" ||
      case_id_key == "case:concept:name";
  const bool selected_first_class =
      canonical || (!trace.case_id_source_key.empty() &&
                    trace.case_id_source_key == case_id_key) ||
      (trace.case_id_attribute &&
       trace.case_id_attribute->key == case_id_key);
  if (selected_first_class) {
    if (trace.case_id_state == FirstClassAttributeState::kNull ||
        (trace.case_id_attribute && trace.case_id_attribute->is_null)) {
      throw std::invalid_argument(
          "selected case-id attribute is missing or null");
    }
    if (trace.case_id_state == FirstClassAttributeState::kPresent ||
        !trace.case_id.empty()) {
      return trace.case_id;
    }
    if (trace.case_id_attribute) return trace.case_id_attribute->value;
    if (!canonical) {
      throw std::invalid_argument(
          "selected case-id attribute is missing or null");
    }
  }
  if (!case_id_key.empty()) {
    if (const auto* value = trace.attribute(case_id_key)) return *value;
  }
  if (!canonical) {
    throw std::invalid_argument(
        "selected case-id attribute is missing or null");
  }
  // The native event-log representation permits anonymous traces.  A stable
  // positional fallback keeps diagnostics one-to-one with the input list.
  return "trace-" + std::to_string(index);
}

}  // namespace minera::footprints_conformance
