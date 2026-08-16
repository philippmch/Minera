#include "minera/footprints.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <iterator>
#include <map>
#include <limits>
#include <set>
#include <stdexcept>
#include <unordered_map>

namespace minera {
namespace {

using PairSet = std::set<FootprintPair>;

void fix_relations(Footprint& fp) {
  for (const auto& p : fp.parallel) fp.sequence.erase(p);
  std::vector<FootprintPair> conflict;
  for (const auto& p : fp.sequence) {
    if (fp.sequence.find({p.second, p.first}) != fp.sequence.end()) conflict.push_back(p);
  }
  for (const auto& p : conflict) {
    fp.sequence.erase(p);
    fp.parallel.insert(p);
  }
}

struct ModelFp {
  PairSet sequence, parallel;
  std::set<std::string> starts, ends, activities, aah;
  bool skippable = true;
  std::size_t min_length = 0;
  std::optional<std::size_t> max_length = std::size_t{0};
};

void fix_relations(ModelFp& fp) {
  for (const auto& p : fp.parallel) fp.sequence.erase(p);
  std::vector<FootprintPair> conflict;
  for (const auto& p : fp.sequence)
    if (fp.sequence.find({p.second, p.first}) != fp.sequence.end()) conflict.push_back(p);
  for (const auto& p : conflict) { fp.sequence.erase(p); fp.parallel.insert(p); }
}

Footprint to_public(const ModelFp& m, bool include_max) {
  Footprint fp;
  fp.dfg_present = false;
  fp.sequence = m.sequence;
  fp.parallel = m.parallel;
  fp.start_activities = m.starts;
  fp.end_activities = m.ends;
  fp.activities = m.activities;
  fp.skippable = m.skippable;
  fp.activities_always_happening = m.aah;
  fp.min_trace_length = m.min_length;
  fp.max_trace_length_without_loops = include_max ? m.max_length : std::nullopt;
  fix_relations(fp);
  return fp;
}

ModelFp merge_parallel(const std::vector<ModelFp>& children) {
  ModelFp out;
  out.skippable = true;
  out.max_length = std::size_t{0};
  for (const auto& c : children) {
    out.starts.insert(c.starts.begin(), c.starts.end());
    out.ends.insert(c.ends.begin(), c.ends.end());
    out.activities.insert(c.activities.begin(), c.activities.end());
    out.sequence.insert(c.sequence.begin(), c.sequence.end());
    out.parallel.insert(c.parallel.begin(), c.parallel.end());
    out.skippable = out.skippable && c.skippable;
    out.min_length += c.min_length;
    if (c.max_length) { if (out.max_length) *out.max_length += *c.max_length; }
    else out.max_length.reset();
    if (!c.skippable) out.aah.insert(c.aah.begin(), c.aah.end());
  }
  for (std::size_t i = 0; i < children.size(); ++i)
    for (std::size_t j = i + 1; j < children.size(); ++j)
      for (const auto& a : children[i].activities)
        for (const auto& b : children[j].activities) {
          out.parallel.insert({a, b});
          out.parallel.insert({b, a});
        }
  fix_relations(out);
  return out;
}

ModelFp merge_xor(const std::vector<ModelFp>& children) {
  ModelFp out;
  out.skippable = false;
  bool first = true;
  for (const auto& c : children) {
    out.starts.insert(c.starts.begin(), c.starts.end());
    out.ends.insert(c.ends.begin(), c.ends.end());
    out.activities.insert(c.activities.begin(), c.activities.end());
    out.sequence.insert(c.sequence.begin(), c.sequence.end());
    out.parallel.insert(c.parallel.begin(), c.parallel.end());
    out.skippable = out.skippable || c.skippable;
    if (first) {
      out.min_length = c.min_length;
      out.max_length = c.max_length;
      out.aah = c.aah;
      first = false;
    } else {
      out.min_length = std::min(out.min_length, c.min_length);
      if (out.max_length && c.max_length)
        *out.max_length = std::max(*out.max_length, *c.max_length);
      else
        out.max_length.reset();
      if (!c.skippable) {
        std::set<std::string> intersection;
        std::set_intersection(out.aah.begin(), out.aah.end(), c.aah.begin(),
                              c.aah.end(), std::inserter(intersection, intersection.end()));
        out.aah = std::move(intersection);
      }
    }
    if (c.skippable) out.aah.clear();
  }
  fix_relations(out);
  return out;
}

ModelFp tree_fp(const ProcessTreeNode& n, const std::vector<std::string>& names) {
  if (n.op == ProcessTreeOperator::kTau) return ModelFp{};
  if (n.op == ProcessTreeOperator::kActivity) {
    ModelFp out;
    const std::string label = n.activity < names.size() ? names[n.activity] : std::to_string(n.activity);
    out.starts.insert(label); out.ends.insert(label); out.activities.insert(label);
    out.aah.insert(label); out.skippable = false; out.min_length = 1; out.max_length = 1;
    return out;
  }
  std::vector<ModelFp> cs;
  for (const auto& c : n.children) cs.push_back(tree_fp(c, names));
  if (n.op == ProcessTreeOperator::kExclusive) {
    return merge_xor(cs);
  }
  if (n.op == ProcessTreeOperator::kParallel) return merge_parallel(cs);
  if (n.op == ProcessTreeOperator::kSequence) {
    ModelFp out; out.skippable = true; out.max_length = std::size_t{0};
    for (const auto& c : cs) { out.activities.insert(c.activities.begin(), c.activities.end()); out.sequence.insert(c.sequence.begin(), c.sequence.end()); out.parallel.insert(c.parallel.begin(), c.parallel.end()); out.skippable = out.skippable && c.skippable; out.min_length += c.min_length; if (out.max_length && c.max_length) *out.max_length += *c.max_length; else out.max_length.reset(); if (!c.skippable) out.aah.insert(c.aah.begin(), c.aah.end()); }
    for (std::size_t i = 0; i < cs.size(); ++i) {
      for (std::size_t j = i + 1; j < cs.size(); ++j) { for (const auto& a : cs[i].ends) for (const auto& b : cs[j].starts) out.sequence.insert({a,b}); if (!cs[j].skippable) break; }
    }
    for (const auto& c : cs) { out.starts.insert(c.starts.begin(), c.starts.end()); if (!c.skippable) break; }
    for (std::size_t i = cs.size(); i-- > 0;) { out.ends.insert(cs[i].ends.begin(), cs[i].ends.end()); if (!cs[i].skippable) break; }
    fix_relations(out); return out;
  }
  // Loop: child 0 is do, child 1 (and any remaining children) is redo.
  ModelFp out; if (cs.empty()) return out;
  const ModelFp& do_fp = cs[0]; ModelFp redo;
  if (cs.size() == 2) redo = cs[1]; else { std::vector<ModelFp> rest(cs.begin()+1, cs.end()); redo = merge_xor(rest); }
  out = do_fp; out.activities.insert(redo.activities.begin(), redo.activities.end()); out.sequence.insert(redo.sequence.begin(), redo.sequence.end()); out.parallel.insert(redo.parallel.begin(), redo.parallel.end()); out.skippable = do_fp.skippable; out.min_length = do_fp.min_length; out.max_length = do_fp.max_length; out.aah = do_fp.skippable ? std::set<std::string>{} : do_fp.aah;
  out.starts = do_fp.starts; out.ends = do_fp.ends;
  if (do_fp.skippable) { out.starts.insert(redo.starts.begin(), redo.starts.end()); out.ends.insert(redo.ends.begin(), redo.ends.end()); }
  for (const auto& a : do_fp.ends) for (const auto& b : redo.starts) out.sequence.insert({a,b});
  for (const auto& a : redo.ends) for (const auto& b : do_fp.starts) out.sequence.insert({a,b});
  if (do_fp.skippable) for (const auto& a : redo.ends) for (const auto& b : redo.starts) out.sequence.insert({a,b});
  if (redo.skippable) for (const auto& a : do_fp.ends) for (const auto& b : do_fp.starts) out.sequence.insert({a,b});
  if (out.max_length && redo.max_length) { /* max without loops excludes redo repetitions */ }
  fix_relations(out); return out;
}

// POWL bottom-up implementation mirrors the process-tree algebra while using
// immutable child indices for strict partial orders.
ModelFp powl_fp(const PowlNode& n) {
  if (n.kind() == PowlNodeKind::kSilent) return ModelFp{};
  if (n.kind() == PowlNodeKind::kTransition) {
    ModelFp out; out.skippable = n.skippable(); out.min_length = n.frequency().minimum > 0 ? static_cast<std::size_t>(n.frequency().minimum) : 0; out.max_length = n.frequency().maximum ? std::optional<std::size_t>(static_cast<std::size_t>(*n.frequency().maximum)) : std::nullopt; if (n.frequency().maximum && *n.frequency().maximum > 1) out.sequence.insert({n.label(), n.label()}); out.starts.insert(n.label()); out.ends.insert(n.label()); if (!n.skippable()) out.aah.insert(n.label()); out.activities.insert(n.label()); return out;
  }
  const auto& cs = n.children();
  if (n.kind() == PowlNodeKind::kXor) { ProcessTreeNode fake{ProcessTreeOperator::kExclusive,0,{}}; (void)fake; ModelFp out; out.skippable = false; bool first=true; for (const auto& c : cs) { auto f=powl_fp(c); out.starts.insert(f.starts.begin(),f.starts.end()); out.ends.insert(f.ends.begin(),f.ends.end()); out.activities.insert(f.activities.begin(),f.activities.end()); out.sequence.insert(f.sequence.begin(),f.sequence.end()); out.parallel.insert(f.parallel.begin(),f.parallel.end()); out.skippable = out.skippable || f.skippable; if(first){out.min_length=f.min_length;out.max_length=f.max_length;out.aah=f.aah;first=false;} else {out.min_length=std::min(out.min_length,f.min_length); if(out.max_length&&f.max_length)*out.max_length=std::max(*out.max_length,*f.max_length);else out.max_length.reset(); if(!f.skippable){std::set<std::string> inter;std::set_intersection(out.aah.begin(),out.aah.end(),f.aah.begin(),f.aah.end(),std::inserter(inter,inter.end()));out.aah=std::move(inter);}} if(f.skippable) out.aah.clear(); } fix_relations(out); return out; }
  if (n.kind() == PowlNodeKind::kLoop) { ProcessTreeNode fake{ProcessTreeOperator::kLoop,0,{}}; (void)fake; std::vector<ModelFp> ts; for(const auto& c:cs) ts.push_back(powl_fp(c)); ProcessTreeNode dummy; (void)dummy; ModelFp out; if(!ts.empty()){out=ts[0]; if(ts.size()>1){auto redo=ts[1]; out.activities.insert(redo.activities.begin(),redo.activities.end());out.sequence.insert(redo.sequence.begin(),redo.sequence.end());out.parallel.insert(redo.parallel.begin(),redo.parallel.end());out.skippable=ts[0].skippable;out.aah=ts[0].skippable?std::set<std::string>{}:ts[0].aah;if(ts[0].skippable){out.starts.insert(redo.starts.begin(),redo.starts.end());out.ends.insert(redo.ends.begin(),redo.ends.end());}for(auto&a:ts[0].ends)for(auto&b:redo.starts)out.sequence.insert({a,b});for(auto&a:redo.ends)for(auto&b:ts[0].starts)out.sequence.insert({a,b});if(ts[0].skippable)for(auto&a:redo.ends)for(auto&b:redo.starts)out.sequence.insert({a,b});if(redo.skippable)for(auto&a:ts[0].ends)for(auto&b:ts[0].starts)out.sequence.insert({a,b});}} fix_relations(out); return out; }
  // Strict partial order.
  std::vector<ModelFp> fs; for (const auto& c:cs) fs.push_back(powl_fp(c));
  ModelFp out; out.skippable = true;
  for (const auto& f : fs) { out.activities.insert(f.activities.begin(), f.activities.end()); out.sequence.insert(f.sequence.begin(), f.sequence.end()); out.parallel.insert(f.parallel.begin(), f.parallel.end()); out.skippable = out.skippable && f.skippable; out.min_length += f.min_length; if (f.max_length) { if (!out.max_length) out.max_length = 0; *out.max_length += *f.max_length; } else out.max_length.reset(); if (!f.skippable) out.aah.insert(f.aah.begin(), f.aah.end()); }
  const std::size_t nchild=cs.size(); std::vector<std::set<std::size_t>> closure(nchild);
  for(std::size_t i=0;i<nchild;++i){std::deque<std::size_t> q; for(const auto&e:n.order()) if(e.first==i) q.push_back(e.second); while(!q.empty()){auto x=q.front();q.pop_front(); if(closure[i].insert(x).second) for(const auto&e:n.order())if(e.first==x)q.push_back(e.second);}}
  for(std::size_t i=0;i<nchild;++i){bool st=true,en=true;for(std::size_t j=0;j<nchild;++j)if(i!=j&&!fs[j].skippable&&closure[j].count(i))st=false;for(std::size_t j=0;j<nchild;++j)if(i!=j&&!fs[j].skippable&&closure[i].count(j))en=false;if(st)out.starts.insert(fs[i].starts.begin(),fs[i].starts.end());if(en)out.ends.insert(fs[i].ends.begin(),fs[i].ends.end());}
  for(const auto&e:n.order()){
    bool redundant=false;
    for(std::size_t k=0;k<nchild;++k) if(k!=e.first&&k!=e.second&&closure[e.first].count(k)&&closure[k].count(e.second)) { redundant=true; break; }
    if(!redundant) for(const auto&a:fs[e.first].ends)for(const auto&b:fs[e.second].starts)out.sequence.insert({a,b});
  }
  for(std::size_t i=0;i<nchild;++i)for(std::size_t j=0;j<nchild;++j)if(i!=j&&closure[i].count(j)){bool only_skip=true;for(std::size_t k=0;k<nchild;++k)if(k!=i&&k!=j&&!fs[k].skippable&&closure[i].count(k)&&closure[k].count(j))only_skip=false;if(only_skip)for(const auto&a:fs[i].ends)for(const auto&b:fs[j].starts)out.sequence.insert({a,b});}
  for(std::size_t i=0;i<nchild;++i)for(std::size_t j=i+1;j<nchild;++j)if(!closure[i].count(j)&&!closure[j].count(i))for(const auto&a:fs[i].activities)for(const auto&b:fs[j].activities){out.parallel.insert({a,b});out.parallel.insert({b,a});}
  fix_relations(out); return out;
}

using Marking = std::vector<std::uint64_t>;
struct ReachEdge { std::size_t transition = 0; std::size_t target = 0; };
struct ReachState { Marking marking; std::vector<ReachEdge> edges; };
struct ReachGraph { std::vector<ReachState> states; };

bool dominates(const Marking& a, const Marking& b) {
  bool strictly = false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i] < b[i]) return false;
    strictly = strictly || a[i] > b[i];
  }
  return strictly;
}

ReachGraph build_reachability(const PetriNet& net) {
  const std::size_t places = net.place_count();
  auto enabled = [places](const Marking& marking, const PetriTransition& transition) {
    return places == marking.size() && petri_transition_enabled(transition, marking);
  };
  auto fire = [places](Marking marking, const PetriTransition& transition) {
    for (const auto place : transition.inputs) if (place < places) --marking[place];
    for (const auto place : transition.reset_inputs) if (place < places) marking[place] = 0;
    for (const auto place : transition.outputs) {
      if (place >= places || marking[place] == std::numeric_limits<std::uint64_t>::max())
        throw std::runtime_error("Petri reachability marking overflow");
      ++marking[place];
    }
    return marking;
  };

  Marking initial(places);
  if (net.source() < places) initial[net.source()] = 1;
  ReachGraph graph;
  graph.states.push_back({initial, {}});
  std::map<Marking, std::size_t> ids{{initial, 0}};
  struct Frame {
    std::size_t state = 0;
    std::size_t next_transition = 0;
  };
  std::vector<Frame> stack{{0, 0}};
  std::vector<Marking> ancestors{initial};
  while (!stack.empty()) {
    auto& frame = stack.back();
    if (frame.next_transition == net.transitions().size()) {
      stack.pop_back();
      ancestors.pop_back();
      continue;
    }
    const std::size_t transition_index = frame.next_transition++;
    const auto& transition = net.transitions()[transition_index];
    const Marking current_marking = graph.states[frame.state].marking;
    if (!enabled(current_marking, transition)) continue;
    const Marking next_marking = fire(current_marking, transition);
    for (const auto& ancestor : ancestors) {
      if (dominates(next_marking, ancestor)) {
        throw std::runtime_error(
            "Petri reachability graph is unbounded along an ancestor path");
      }
    }
    auto [it, inserted] = ids.emplace(next_marking, graph.states.size());
    if (inserted) graph.states.push_back({next_marking, {}});
    graph.states[frame.state].edges.push_back({transition_index, it->second});
    if (inserted) {
      ancestors.push_back(next_marking);
      stack.push_back({it->second, 0});
    }
  }
  return graph;
}

} // namespace

Footprint discover_footprints(const ProcessTree& tree) { return to_public(tree_fp(tree.root(), tree.activities()), true); }
Footprint discover_footprints(const PowlNode& model) { return to_public(powl_fp(model), false); }

Footprint discover_footprints(const PetriNet& net) {
  Footprint fp;
  fp.dfg_present = false;
  fp.end_activities_present = false;
  fp.min_trace_length_present = false;
  fp.min_trace_length.reset();
  fp.skippable.reset();
  fp.activities_always_happening.reset();
  const auto graph = build_reachability(net);
  const std::size_t state_count = graph.states.size();
  std::vector<std::set<std::size_t>> incoming(state_count);
  for (std::size_t source = 0; source < state_count; ++source)
    for (const auto& edge : graph.states[source].edges)
      if (net.transitions()[edge.transition].activity) incoming[edge.target].insert(edge.transition);

  auto label = [&net](std::size_t transition) {
    const auto activity = net.transitions()[transition].activity;
    return *activity < net.activities().size() ? net.activities()[*activity]
                                               : std::to_string(*activity);
  };
  auto eventually_enabled = [&net, &graph](std::size_t start) {
    std::set<std::size_t> visible;
    std::set<std::size_t> seen{start};
    std::deque<std::size_t> todo{start};
    while (!todo.empty()) {
      const auto state = todo.front(); todo.pop_front();
      for (const auto& edge : graph.states[state].edges) {
        if (net.transitions()[edge.transition].activity) visible.insert(edge.transition);
        else if (seen.insert(edge.target).second) todo.push_back(edge.target);
      }
    }
    return visible;
  };
  for (std::size_t transition = 0; transition < net.transitions().size(); ++transition)
    if (net.transitions()[transition].activity) fp.activities.insert(label(transition));
  for (const auto transition : eventually_enabled(0)) fp.start_activities.insert(label(transition));

  std::set<std::pair<std::size_t, std::size_t>> output_pairs;
  std::set<std::pair<std::size_t, std::size_t>> predecessor_pairs;
  for (std::size_t state = 0; state < state_count; ++state) {
    std::set<std::size_t> outputs;
    for (const auto& edge : graph.states[state].edges)
      if (net.transitions()[edge.transition].activity) outputs.insert(edge.transition);
    for (auto first = outputs.begin(); first != outputs.end(); ++first)
      for (auto second = std::next(first); second != outputs.end(); ++second) {
        output_pairs.insert({*first, *second}); output_pairs.insert({*second, *first});
      }
    const auto eventually = eventually_enabled(state);
    for (const auto predecessor : incoming[state]) {
      for (const auto successor : eventually) {
        fp.sequence.insert({label(predecessor), label(successor)});
      }
      for (const auto successor : outputs) predecessor_pairs.insert({predecessor, successor});
    }
  }
  for (const auto& pair : predecessor_pairs)
    if (predecessor_pairs.count({pair.second, pair.first}) && output_pairs.count(pair))
      fp.parallel.insert({label(pair.first), label(pair.second)});
  fix_relations(fp);
  return fp;
}

} // namespace minera


