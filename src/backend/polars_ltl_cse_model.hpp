// Round CW: the order in which the reference's eventually_follows plan runs
// its activity filters and sorts (Polars 1.44.1, lazy collect).
//
// Observed (probes cw/w1, t3/ef2, t3/ef3 and the optimized plans printed by
// explain() for every pattern of repeated activities up to six): the plan
// sorts the frame by case and timestamp, numbers the rows per case
// (int_range over the case) and, for each activity i, filters it
// (col(attribute) == a_i). The cases holding every activity are the inner
// join of the frame's case column with unique(case) of each activity's
// selection; each consecutive pair (i, i+1) joins the two selections on the
// case and keeps _index < _index_next; the result is the frame semi (anti)
// joined with all of these.
//
// Polars then
// - pushes each activity filter below the sort and the row numbering when
//   the attribute column is the case column (the window's partition key):
//   every selection sorts only its own rows; otherwise the filter stays above
//   the shared sort;
// - drops the sort and numbering wherever nothing reads _index (the case
//   joins and the semi/anti join's left side);
// - eliminates common subplans top-down: the first occurrence of a subplan
//   that occurs more than once becomes a cache (its inner plan is left as
//   it is), and a cache with a single consumer is inlined again;
// - fills every cache before the rest of the plan runs, in the order the
//   caches first appear (depth first, left input first), then runs the rest
//   in the same order.
//
// The first error of the collect is the first failing step in that order:
// an activity filter's compute error, or the literal sort key failing a sort
// whose frame does not have exactly one row. eventually_unit_order returns
// that order for the activities' identity classes (equal literals share a
// class); a kSort step carries the class whose filter sits below it (the
// pushed-down case) or -1 (the sort of the whole frame).
//
// A None activity filters on null.repeat(len()): Polars neither pushes it
// below the sort nor drops the sort beneath it (probe cw/w3), so its
// selections always sort the whole frame first (`bound` classes).
#pragma once

#include <cstddef>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

namespace minera::detail::polars_ltl_cse {

enum class StepKind { kFilter, kSort };
struct Step {
  StepKind kind;
  int group;  // activity class; -1 for the whole frame's sort
};

class PlanModel {
 public:
  PlanModel(const std::vector<int>& groups, bool pushdown, std::vector<bool> bound = {})
      : pushdown_(pushdown), bound_(std::move(bound)) {
    df_ = make(kDf);
    base_ = make(kWithColumns, -1, {make(kSort, -1, {df_})});
    int all = base_;
    for (const int g : groups) all = make(kJoin, -1, {all, unique(filter(g))});
    int cases = unique(all);
    for (std::size_t i = 0; i + 1 < groups.size(); ++i) {
      const int pair = make(kJoin, -1, {make(kSelect3, -1, {filter(groups[i])}), make(kSelect3, -1, {filter(groups[i + 1])})});
      cases = make(kJoin, -1, {cases, unique(make(kFilterIndex, -1, {pair}))});
    }
    root_ = make(kSelectAll, -1, {make(kAnti, -1, {base_, cases})});
  }

  std::vector<Step> order() {
    counts_.assign(nodes_.size(), 0);
    count(root_);
    consumers_.assign(nodes_.size(), 0);
    needs_order_.assign(nodes_.size(), false);
    mark(root_, false);
    std::vector<std::vector<Step>> caches;
    std::vector<int> cache_of(nodes_.size(), -1);
    std::vector<Step> main;
    emit(root_, main, false, false, caches, cache_of);
    std::vector<Step> out;
    for (const auto& cache : caches) out.insert(out.end(), cache.begin(), cache.end());
    out.insert(out.end(), main.begin(), main.end());
    return out;
  }

 private:
  enum Op { kDf, kWithColumns, kSort, kFilter, kUnique, kSelectCase, kSelect3, kJoin, kFilterIndex, kAnti, kSelectAll };
  struct Node {
    Op op;
    int payload;
    std::vector<int> kids;
  };

  int make(Op op, int payload = -1, std::vector<int> kids = {}) {
    const auto key = std::make_tuple(static_cast<int>(op), payload, kids);
    const auto found = ids_.find(key);
    if (found != ids_.end()) return found->second;
    nodes_.push_back({op, payload, std::move(kids)});
    const int id = static_cast<int>(nodes_.size()) - 1;
    ids_.emplace(key, id);
    return id;
  }
  bool bound(int group) const { return group >= 0 && static_cast<std::size_t>(group) < bound_.size() && bound_[static_cast<std::size_t>(group)]; }
  int filter(int group) {
    if (pushdown_ && !bound(group)) return make(kWithColumns, -1, {make(kSort, -1, {make(kFilter, group, {df_})})});
    return make(kFilter, group, {base_});
  }
  int unique(int x) { return make(kUnique, -1, {make(kSelectCase, -1, {x})}); }
  bool cached(int id) const { return nodes_[static_cast<std::size_t>(id)].op != kDf && counts_[static_cast<std::size_t>(id)] > 1; }

  void count(int id) {
    ++counts_[static_cast<std::size_t>(id)];
    for (const int kid : nodes_[static_cast<std::size_t>(id)].kids) count(kid);
  }
  bool ordered_after(const Node& node, bool ordered) const {
    if (node.op == kFilter && bound(node.payload)) return true;
    return node.op == kUnique ? false : node.op == kSelect3 ? true : ordered;
  }
  // Top-down: the first occurrence with more than one count becomes a cache
  // reference and its inner plan is not visited.
  void mark(int id, bool ordered) {
    const auto& node = nodes_[static_cast<std::size_t>(id)];
    if (cached(id)) {
      ++consumers_[static_cast<std::size_t>(id)];
      needs_order_[static_cast<std::size_t>(id)] = needs_order_[static_cast<std::size_t>(id)] || ordered;
      return;
    }
    const bool inner = ordered_after(node, ordered);
    for (const int kid : node.kids) mark(kid, inner);
  }
  // Post-order steps: inputs first, then a filter's compare or a sort that
  // something above still reads.
  void emit(int id, std::vector<Step>& out, bool ordered, bool inline_plan, std::vector<std::vector<Step>>& caches,
            std::vector<int>& cache_of) {
    const auto& node = nodes_[static_cast<std::size_t>(id)];
    if (!inline_plan && cached(id)) {
      if (consumers_[static_cast<std::size_t>(id)] > 1) {
        if (cache_of[static_cast<std::size_t>(id)] < 0) {
          cache_of[static_cast<std::size_t>(id)] = static_cast<int>(caches.size());
          caches.emplace_back();
          std::vector<Step> content;
          emit_inline(id, content, needs_order_[static_cast<std::size_t>(id)]);
          caches[static_cast<std::size_t>(cache_of[static_cast<std::size_t>(id)])] = std::move(content);
        }
        return;
      }
      emit_inline(id, out, ordered);
      return;
    }
    const bool inner = ordered_after(node, ordered);
    for (const int kid : node.kids) emit(kid, out, inner, inline_plan, caches, cache_of);
    own_step(node, out, inner);
  }
  void emit_inline(int id, std::vector<Step>& out, bool ordered) {
    const auto& node = nodes_[static_cast<std::size_t>(id)];
    const bool inner = ordered_after(node, ordered);
    for (const int kid : node.kids) emit_inline(kid, out, inner);
    own_step(node, out, inner);
  }
  void own_step(const Node& node, std::vector<Step>& out, bool ordered) const {
    if (node.op == kFilter) out.push_back({StepKind::kFilter, node.payload});
    if (node.op == kSort && ordered) {
      int group = -1;
      const auto& below = nodes_[static_cast<std::size_t>(node.kids.front())];
      if (below.op == kFilter) group = below.payload;
      out.push_back({StepKind::kSort, group});
    }
  }

  bool pushdown_;
  std::vector<bool> bound_;
  int df_ = -1, base_ = -1, root_ = -1;
  std::vector<Node> nodes_;
  std::map<std::tuple<int, int, std::vector<int>>, int> ids_;
  std::vector<int> counts_, consumers_;
  std::vector<bool> needs_order_;
};

inline std::vector<Step> eventually_unit_order(const std::vector<int>& groups, bool pushdown, std::vector<bool> bound = {}) {
  return PlanModel(groups, pushdown, std::move(bound)).order();
}

}  // namespace minera::detail::polars_ltl_cse
