#include "minera/decomposed_alignment.hpp"

#include "minera/dfg_variants.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <queue>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_map>

namespace minera {
namespace {

constexpr std::size_t npos = static_cast<std::size_t>(-1);

void validate_selector(std::string_view selector) {
  if (selector.empty() || std::all_of(selector.begin(), selector.end(),
                                      [](unsigned char c) {
                                        return std::isspace(c) != 0;
                                      })) {
    throw std::invalid_argument(
        "decomposed alignment activity selector must not be blank");
  }
}

std::string selected_activity(const Event& event,
                              std::string_view selector) {
  const auto* value = dfg_event_field_value(event, selector);
  if (value == nullptr)
    throw std::invalid_argument(
        "decomposed alignment: missing or null activity " +
        std::string(selector));
  return *value;
}

struct SearchNode { std::vector<std::size_t> marking; std::size_t pos = 0; };
struct SearchKey {
  std::vector<std::size_t> marking; std::size_t pos = 0;
  bool operator==(const SearchKey& o) const noexcept { return pos == o.pos && marking == o.marking; }
};
struct SearchHash {
  std::size_t operator()(const SearchKey& k) const noexcept {
    std::size_t h = k.pos + 0x9e3779b9;
    for (auto x : k.marking) h ^= x + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
  }
};
struct SearchResult { bool found=false, timed_out=false, exhausted=false; double cost=0; std::vector<DecomposedAlignmentMove> moves; std::size_t visited=0, queued=0, arcs=0; };
struct QueueItem { double cost; std::size_t node; bool operator>(const QueueItem& o) const noexcept { return cost > o.cost || (cost == o.cost && node > o.node); } };

bool enabled(const PetriTransition& t, const std::vector<std::size_t>& m) {
  std::unordered_map<std::size_t,std::size_t> need;
  for (auto p : t.inputs) ++need[p];
  for (const auto& kv : need) if (kv.first >= m.size() || m[kv.first] < kv.second) return false;
  return true;
}
std::vector<std::size_t> fire(const PetriTransition& t, const std::vector<std::size_t>& m) {
  auto r=m;
  for (auto p:t.inputs) {
    if (p>=r.size() || r[p]==0) return {};
    --r[p];
  }
  for (auto p:t.outputs) {
    if (p>=r.size() || r[p] == std::numeric_limits<std::size_t>::max()) return {};
    ++r[p];
  }
  return r;
}
double cost_of(const std::unordered_map<std::string,double>& tab, const std::string& a, double d) { auto i=tab.find(a); return i==tab.end()?d:i->second; }

struct LocalNet {
  std::vector<std::size_t> places; std::vector<PetriTransition> transitions; std::vector<std::size_t> provenance;
  std::vector<std::string> activities; std::vector<std::size_t> initial, final;
  std::set<std::string> labels;
};

SearchResult search(const LocalNet& net, const std::vector<std::string>& trace,
                    const DecomposedAlignmentOptions& o,
                    std::chrono::steady_clock::time_point deadline,
                    DecomposedAlignmentDiagnostics& d) {
  SearchResult r; std::priority_queue<QueueItem,std::vector<QueueItem>,std::greater<QueueItem>> q;
  struct Node { SearchKey k; double c; std::size_t parent; DecomposedAlignmentMove move; };
  std::vector<Node> nodes; std::unordered_map<SearchKey,double,SearchHash> best;
  nodes.push_back({SearchKey{net.initial,0},0,npos,{}}); q.push({0,0}); best[nodes[0].k]=0;
  while (!q.empty()) {
    if (deadline != std::chrono::steady_clock::time_point::max() && std::chrono::steady_clock::now() >= deadline) { r.timed_out=true; break; }
    auto qi=q.top(); q.pop(); auto& n=nodes[qi.node]; auto bi=best.find(n.k); if (bi==best.end() || bi->second != n.c) continue;
    const SearchKey current_key = n.k; const double current_cost = n.c;
    ++r.visited; if (r.visited > o.max_states) { r.exhausted=true; break; }
    if (n.k.pos==trace.size() && n.k.marking==net.final) { r.found=true; r.cost=n.c; for (auto i=qi.node;i!=0;i=nodes[i].parent) r.moves.push_back(nodes[i].move); std::reverse(r.moves.begin(),r.moves.end()); break; }
    auto relax=[&](SearchKey k,double edge,DecomposedAlignmentMove mv){ ++r.arcs; if (!std::isfinite(edge)||edge<0) return; double nc=current_cost+edge; auto it=best.find(k); if (it!=best.end()&&it->second<=nc) return; std::size_t idx=nodes.size(); nodes.push_back({std::move(k),nc,qi.node,std::move(mv)}); best[nodes.back().k]=nc; q.push({nc,idx}); ++r.queued; };
    if (current_key.pos<trace.size()) relax({current_key.marking,current_key.pos+1},cost_of(o.activity_log_costs,trace[current_key.pos],o.log_move_cost),{trace[current_key.pos],std::string(">>"),npos});
    for (std::size_t i=0;i<net.transitions.size();++i) {
      const auto& t=net.transitions[i]; if (!enabled(t,current_key.marking)) continue; auto nm=fire(t,current_key.marking); if (nm.empty() && (!t.inputs.empty()||!t.outputs.empty())) continue;
      const std::size_t tid=net.provenance[i];
      if (t.silent()) relax({nm,current_key.pos},o.tau_model_cost,{std::string(">>"),std::nullopt,tid});
      else { const auto& a=net.activities[*t.activity]; relax({nm,current_key.pos},cost_of(o.activity_model_costs,a,o.model_move_cost),{std::string(">>"),a,tid}); if (current_key.pos<trace.size()&&trace[current_key.pos]==a) relax({fire(t,current_key.marking),current_key.pos+1},cost_of(o.activity_sync_costs,a,o.sync_cost),{a,a,tid}); }
    }
  }
  d.visited_states += r.visited; d.queued_states = std::max(d.queued_states,r.queued); d.traversed_arcs += r.arcs; return r;
}

bool valid(const DecomposedAlignmentOptions& o) {
  auto good=[](double x){return std::isfinite(x)&&x>=0;};
  if (!good(o.threshold_border_agreement)||!good(o.max_time_trace)||!good(o.max_time_total)||!good(o.log_move_cost)||!good(o.model_move_cost)||!good(o.tau_model_cost)||!good(o.sync_cost)) return false;
  for (const auto& p:o.activity_log_costs) if(!good(p.second)) return false; for(const auto& p:o.activity_model_costs) if(!good(p.second)) return false; for(const auto& p:o.activity_sync_costs) if(!good(p.second)) return false; return true;
}

struct DSU { std::vector<std::size_t> p; explicit DSU(std::size_t n):p(n){std::iota(p.begin(),p.end(),0);} std::size_t find(std::size_t x){return p[x]==x?x:p[x]=find(p[x]);} void unite(std::size_t a,std::size_t b){a=find(a);b=find(b);if(a!=b)p[b]=a;} };

LocalNet make_local(const PetriNet& net, const std::vector<std::size_t>& region_places,
                    const std::vector<std::size_t>& group,
                    const std::vector<std::size_t>& initial, const std::vector<std::size_t>& final) {
  LocalNet l; l.places = region_places; std::unordered_map<std::size_t,std::size_t> map; for(std::size_t i=0;i<l.places.size();++i) map[l.places[i]]=i;
  l.initial.resize(l.places.size()); l.final.resize(l.places.size()); for(std::size_t i=0;i<l.places.size();++i){l.initial[i]=initial[l.places[i]];l.final[i]=final[l.places[i]];}
  l.activities=net.activities();
  for(auto tid:group){ const auto& t=net.transitions()[tid]; PetriTransition x=t; x.inputs.clear();x.outputs.clear(); for(auto p:t.inputs) if(map.count(p)) x.inputs.push_back(map[p]); for(auto p:t.outputs) if(map.count(p)) x.outputs.push_back(map[p]); l.transitions.push_back(std::move(x)); l.provenance.push_back(tid); if(t.activity) l.labels.insert(net.activities()[*t.activity]); }
  return l;
}

struct TaggedMove { DecomposedAlignmentMove move; std::size_t order=0; std::size_t component=0; std::size_t occurrence=0; };

// Recomposition is performed on an event DAG rather than by sorting moves.
// A node is shared only when its key identifies the same logical event in two
// local projections.  Log moves use the original trace position; model moves
// use their label/occurrence and status; tau moves retain transition
// provenance and are intentionally never shared across components.
enum class EventKind { kLog, kTau, kModel, kSync };
struct EventKey {
  EventKind kind = EventKind::kLog;
  std::size_t trace_index = npos;
  std::string label;
  std::size_t ordinal = 0;
  std::size_t transition = npos;
  std::size_t component = 0;
  bool operator==(const EventKey& o) const noexcept {
    return kind == o.kind && trace_index == o.trace_index && label == o.label &&
           ordinal == o.ordinal && transition == o.transition && component == o.component;
  }
};
struct EventKeyHash {
  std::size_t operator()(const EventKey& k) const noexcept {
    std::size_t h = static_cast<std::size_t>(k.kind) + 0x9e3779b9;
    h ^= k.trace_index + (h << 6) + (h >> 2); h ^= k.ordinal + (h << 6) + (h >> 2);
    h ^= k.transition + (h << 6) + (h >> 2); h ^= k.component + (h << 6) + (h >> 2);
    for (unsigned char c : k.label) h = h * 131 + c;
    return h;
  }
};
struct EventNode {
  EventKey key;
  DecomposedAlignmentMove move;
  std::set<std::size_t> components;
  std::set<std::size_t> successors;
};
struct DagResult { bool acyclic = true; std::vector<DecomposedAlignmentMove> moves; double cost = 0.0; std::set<std::size_t> cycle_components; };
double move_cost(const DecomposedAlignmentMove&, const DecomposedAlignmentOptions&);
double trace_worst_cost(double bw_raw, const std::vector<std::string>& names,
                        const DecomposedAlignmentOptions& o) {
  double worst = bw_raw;
  for (const auto& name : names) worst += cost_of(o.activity_log_costs, name, o.log_move_cost);
  return worst;
}

std::string cache_key(const LocalNet& l, const std::vector<std::string>& trace,
                      const DecomposedAlignmentOptions& o) {
  std::string k;
  auto add_num = [&](std::size_t x) { k += std::to_string(x); k.push_back(','); };
  k += "p:"; for (auto x : l.places) add_num(x); k += "t:"; for (auto x : l.provenance) add_num(x);
  k += "m:"; for (auto x : l.initial) add_num(x); k += "f:"; for (auto x : l.final) add_num(x);
  k += "tr:"; for (const auto& a : trace) { k += a; k.push_back('\0'); }
  auto add_map = [&](const auto& m) { std::map<std::string,double> s(m.begin(), m.end()); for (const auto& [a,v] : s) { k += a; k.push_back('='); k += std::to_string(v); k.push_back(';'); } };
  k += "c:" + std::to_string(o.log_move_cost) + ',' + std::to_string(o.model_move_cost) + ',' + std::to_string(o.tau_model_cost) + ',' + std::to_string(o.sync_cost);
  add_map(o.activity_log_costs); add_map(o.activity_model_costs); add_map(o.activity_sync_costs); return k;
}

DagResult compose_dag(const std::vector<SearchResult>& aligns,
                      const std::vector<std::vector<std::size_t>>& projected_indices,
                      const std::vector<std::string>& names,
                      const DecomposedAlignmentOptions& o) {
  DagResult result; std::unordered_map<EventKey,std::size_t,EventKeyHash> ids; std::vector<EventNode> nodes;
  for (std::size_t ci = 0; ci < aligns.size(); ++ci) {
    std::size_t projection_pos = 0; std::unordered_map<std::string,std::size_t> occurrence; std::size_t previous = npos;
    for (const auto& move : aligns[ci].moves) {
      EventKey key; key.component = ci; key.transition = move.transition;
      const bool has_log = move.log && *move.log != ">>";
      const bool has_model = move.model && *move.model != ">>";
      if (has_log) {
        if (projection_pos >= projected_indices[ci].size()) { result.acyclic = false; result.cycle_components.insert(ci); continue; }
        key.trace_index = projected_indices[ci][projection_pos++]; key.label = *move.log;
        key.kind = has_model ? EventKind::kSync : EventKind::kLog;
        if (has_model) key.ordinal = occurrence[*move.model]++;
      } else if (has_model) {
        key.kind = EventKind::kModel; key.label = *move.model; key.ordinal = occurrence[*move.model]++;
      } else {
        key.kind = EventKind::kTau; key.ordinal = occurrence["#tau:" + std::to_string(move.transition)]++;
      }
      // Tau events are always component-local.  Visible model/sync events
      // retain transition provenance: only projections of the same boundary
      // transition may unify.  Pure log moves have no model provenance.
      if (key.kind == EventKind::kLog) {
        key.component = npos;
        key.transition = npos;
      } else if (key.kind == EventKind::kModel || key.kind == EventKind::kSync) {
        key.component = npos;
      }
      auto it = ids.find(key); std::size_t id;
      if (it == ids.end()) { id = nodes.size(); ids.emplace(key,id); nodes.push_back(EventNode{key,move,{ci},{}}); }
      else { id = it->second; nodes[id].components.insert(ci); }
      if (previous != npos && previous != id) nodes[previous].successors.insert(id);
      previous = id;
    }
  }
  // Two different transitions cannot both claim one synchronous trace event.
  // Mark their owner components for a merge so recomposition can be retried.
  std::map<std::pair<std::size_t,std::string>, std::pair<std::size_t,std::set<std::size_t>>> sync_claims;
  for (const auto& node : nodes) if (node.key.kind == EventKind::kSync) {
    auto key = std::make_pair(node.key.trace_index,node.key.label);
    auto [it, inserted] = sync_claims.emplace(key, std::make_pair(node.key.transition, std::set<std::size_t>{}));
    auto& claim = it->second;
    if (inserted) claim.first = node.key.transition;
    else if (claim.first != node.key.transition) {
      result.acyclic = false;
      result.cycle_components.insert(node.components.begin(), node.components.end());
      result.cycle_components.insert(claim.second.begin(), claim.second.end());
    }
    claim.second.insert(node.components.begin(), node.components.end());
  }
  // Events of activities that no component knows are dropped, as the
  // reference does: no projection sees them.
  struct Ready {
    std::size_t id;
    const std::vector<EventNode>* nodes;
    bool operator>(const Ready& o) const {
      const auto& a = (*nodes)[id].key; const auto& b = (*nodes)[o.id].key;
      auto pos = [](const EventKey& k) { return k.trace_index == npos ? std::numeric_limits<std::size_t>::max() : k.trace_index; };
      auto rank = [](EventKind k) { return k == EventKind::kModel || k == EventKind::kTau ? 0 : (k == EventKind::kSync ? 2 : 1); };
      const auto a_component = (*nodes)[id].components.empty()
                                   ? npos
                                   : *(*nodes)[id].components.begin();
      const auto b_component = (*nodes)[o.id].components.empty()
                                   ? npos
                                   : *(*nodes)[o.id].components.begin();
      return std::tuple{pos(a),rank(a.kind),a.label,a.transition,a_component,id} >
             std::tuple{pos(b),rank(b.kind),b.label,b.transition,b_component,o.id};
    }
  };
  std::vector<std::size_t> indegree(nodes.size()); for (const auto& n : nodes) for (auto s : n.successors) ++indegree[s];
  std::priority_queue<Ready,std::vector<Ready>,std::greater<Ready>> q; for (std::size_t i=0;i<nodes.size();++i) if (!indegree[i]) q.push({i,&nodes});
  while (!q.empty()) { auto id=q.top().id; q.pop(); result.moves.push_back(nodes[id].move); result.cost += move_cost(nodes[id].move,o); for(auto s:nodes[id].successors) if(--indegree[s]==0) q.push({s,&nodes}); }
  if (result.moves.size() != nodes.size()) { result.acyclic=false; for(std::size_t i=0;i<nodes.size();++i) if(indegree[i]) result.cycle_components.insert(nodes[i].components.begin(),nodes[i].components.end()); result.moves.clear(); result.cost=0; }
  return result;
}

double move_cost(const DecomposedAlignmentMove& m,const DecomposedAlignmentOptions&o){ if(m.synchronous()) return cost_of(o.activity_sync_costs,*m.log,o.sync_cost); if(m.model && *m.model!=">>") return cost_of(o.activity_model_costs,*m.model,o.model_move_cost); if(m.log && *m.log!=">>") return cost_of(o.activity_log_costs,*m.log,o.log_move_cost); return o.tau_model_cost; }

} // namespace

DecomposedAlignmentResult align_decomposed(const EventLog& log,const PetriNet& net,const DecomposedAlignmentOptions& o){
  validate_selector(o.activity_attribute);
  DecomposedAlignmentResult out; out.trace_count=log.trace_count();
  if (o.clear_cache) out.diagnostics.note = "component and row caches are per-call; clear_cache has no persistent cache to clear";
  if(!valid(o)||o.variant!=DecomposedAlignmentVariant::kRecomposMaximal||o.max_states==0||o.max_total_states==0){out.complete=false;return out;}
  if(net.place_count()==0){out.complete=false;return out;}
  if (net.source() >= net.place_count() || net.sink() >= net.place_count()) { out.complete=false; return out; }
  for (const auto& t : net.transitions()) for (auto p : t.inputs) if (p >= net.place_count()) { out.complete=false; return out; }
  for (const auto& t : net.transitions()) for (auto p : t.outputs) if (p >= net.place_count()) { out.complete=false; return out; }
  for(const auto&t:net.transitions()) if(t.activity && *t.activity>=net.activities().size()){out.complete=false;return out;}
  auto initial=o.initial_marking.empty()?std::vector<std::size_t>(net.place_count(),0):o.initial_marking; auto final=o.final_marking.empty()?std::vector<std::size_t>(net.place_count(),0):o.final_marking;
  if(o.initial_marking.empty()&&net.source()<initial.size()) initial[net.source()]=1; if(o.final_marking.empty()&&net.sink()<final.size()) final[net.sink()]=1; if(initial.size()!=net.place_count()||final.size()!=net.place_count()){out.complete=false;return out;}

  // Initial regions are connected components of places and silent transitions.
  DSU dsu(net.place_count()); for(const auto&t:net.transitions()) if(t.silent()){ std::size_t seed=npos; if(!t.inputs.empty()) seed=t.inputs.front(); else if(!t.outputs.empty()) seed=t.outputs.front(); if(seed!=npos){ for(auto q:t.inputs) dsu.unite(seed,q); for(auto q:t.outputs) dsu.unite(seed,q); } }
  struct Region { std::vector<std::size_t> places, transitions; };
  std::map<std::size_t,Region> by_root; for(std::size_t p=0;p<net.place_count();++p) by_root[dsu.find(p)].places.push_back(p);
  std::vector<Region> comps; for(auto&kv:by_root) comps.push_back(std::move(kv.second));
  for(std::size_t tid=0;tid<net.transitions().size();++tid){const auto&t=net.transitions()[tid]; std::set<std::size_t> roots; for(auto p:t.inputs) roots.insert(dsu.find(p)); for(auto p:t.outputs) roots.insert(dsu.find(p)); if(roots.empty()){Region r;r.transitions.push_back(tid);comps.push_back(std::move(r));} else for(auto root:roots){auto it=std::find_if(comps.begin(),comps.end(),[&](const Region&r){return !r.places.empty()&&dsu.find(r.places.front())==root;}); if(it!=comps.end()) it->transitions.push_back(tid);}}
  if(comps.empty()){out.complete=false;return out;} out.diagnostics.decompositions=1; out.diagnostics.components=comps.size(); out.diagnostics.whole_net_fallback=false;
  const auto started=std::chrono::steady_clock::now(); auto deadline=std::chrono::steady_clock::time_point::max(); if(o.max_time_total>0) deadline=started+std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(o.max_time_total));

  // Empty trace computes the standard best-worst cost using the decomposed model.
  // BWC is a whole-net replay metric; it is not used to generate result moves.
  std::vector<std::size_t> all_places(net.place_count()), all_trans(net.transitions().size()); std::iota(all_places.begin(),all_places.end(),0); std::iota(all_trans.begin(),all_trans.end(),0);
  // The best-worst cost counts the visible model moves of the cheapest run only.
  auto full_local=make_local(net,all_places,all_trans,initial,final); auto bw_search=search(full_local,{},o,deadline,out.diagnostics);
  // A final marking that no run reaches leaves only the log moves in the best-worst cost.
  const bool bw_unreachable=!bw_search.found&&!bw_search.timed_out&&!bw_search.exhausted;
  if(!bw_search.found&&!bw_unreachable){out.complete=false;out.timed_out=bw_search.timed_out;out.exhausted=bw_search.exhausted; for(const auto& src:log.traces){DecomposedAlignmentTrace row; row.complete=false; row.timed_out=bw_search.timed_out; row.exhausted=bw_search.exhausted; for(const auto&e:src.events) row.activities.push_back(selected_activity(e, o.activity_attribute)); out.traces.push_back(std::move(row));} return out;} double bw=0; for(const auto& m:bw_search.moves) if(m.model && *m.model!=">>") bw+=move_cost(m,o); out.best_worst_cost=bw;
  using VecHash=std::function<std::size_t(const std::vector<std::string>&)>; std::unordered_map<std::vector<std::string>,DecomposedAlignmentTrace,VecHash> row_cache(0,[](const auto&v){std::size_t h=0;for(const auto&s:v)for(unsigned char c:s)h=h*131+c;return h;});
  double total_worst=0; std::size_t total_states=bw_search.visited; bool global_state_abort = total_states > o.max_total_states; if (global_state_abort) { out.complete=false; out.exhausted=true; } std::unordered_map<std::string,SearchResult> component_cache;
  for(const auto& src:log.traces){ std::vector<std::string> names; names.reserve(src.events.size()); for(const auto&e:src.events) names.push_back(selected_activity(e, o.activity_attribute));
    const double trace_worst = trace_worst_cost(bw, names, o);
    const auto trace_started = std::chrono::steady_clock::now();
    auto trace_deadline = deadline;
    if (o.max_time_trace > 0) {
      const auto td = trace_started + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(o.max_time_trace));
      if (td < trace_deadline) trace_deadline = td;
    }
    if (global_state_abort) {
      DecomposedAlignmentTrace row; row.activities=names; row.complete=false; row.exhausted=true; row.best_worst_cost=trace_worst;
      total_worst += trace_worst; out.traces.push_back(std::move(row)); continue;
    }
    if(o.use_cache){auto it=row_cache.find(names);if(it!=row_cache.end() && std::chrono::steady_clock::now() < trace_deadline){out.traces.push_back(it->second);out.traces.back().count=1;out.total_cost+=it->second.cost;total_worst+=trace_worst;++out.diagnostics.cache_hits;continue;}}
    if (std::chrono::steady_clock::now() >= trace_deadline) {
      DecomposedAlignmentTrace row; row.activities=names; row.complete=false; row.timed_out=true; row.best_worst_cost=trace_worst; out.timed_out=true; out.complete=false; total_worst+=trace_worst; out.traces.push_back(std::move(row)); continue;
    }
    if(o.use_cache) ++out.diagnostics.cache_misses;
    std::vector<Region> active=comps; std::vector<std::vector<std::string>> projected; std::vector<std::vector<std::size_t>> projected_indices; std::vector<SearchResult> aligns; std::vector<LocalNet> locals; bool aborted=false, forced_timeout=false, forced_exhausted=false; std::size_t trace_disagreements=0;
    for(;;){ projected.clear(); projected_indices.clear(); aligns.clear(); locals.clear(); for(const auto& c:active){auto l=make_local(net,c.places,c.transitions,initial,final); std::vector<std::string> p; std::vector<std::size_t> pi; for(std::size_t i=0;i<names.size();++i) if(l.labels.count(names[i])) { p.push_back(names[i]); pi.push_back(i); } projected.push_back(p); projected_indices.push_back(pi); SearchResult sr; bool executed=false;
      // Like the reference, a component that sees none of the trace's events
      // is not aligned at all and contributes no moves.
      if (p.empty()) { sr.found=true; aligns.push_back(std::move(sr)); locals.push_back(std::move(l)); continue; }
      if (std::chrono::steady_clock::now() >= trace_deadline) { sr.timed_out=true; forced_timeout=true; aborted=true; aligns.push_back(std::move(sr)); locals.push_back(std::move(l)); break; }
      auto key=cache_key(l,p,o); if(o.use_cache){auto hit=component_cache.find(key); if(hit!=component_cache.end()){sr=hit->second; ++out.diagnostics.cache_hits;} else {++out.diagnostics.cache_misses; executed=true; sr=search(l,p,o,trace_deadline,out.diagnostics); if (sr.found && !sr.timed_out && !sr.exhausted) component_cache.emplace(key,sr);}} else {executed=true; sr=search(l,p,o,trace_deadline,out.diagnostics);} aligns.push_back(std::move(sr)); locals.push_back(std::move(l)); if(executed) total_states+=aligns.back().visited; if (aligns.back().timed_out) { forced_timeout=true; aborted=true; break; } if(total_states>o.max_total_states){out.complete=false;out.exhausted=true;forced_exhausted=true;global_state_abort=true;aborted=true;break;}} if(aborted)break;
std::unordered_map<std::string,std::vector<std::pair<std::size_t,std::vector<int>>>> statuses; std::unordered_map<std::string,std::vector<std::size_t>> owners; std::vector<std::unordered_map<std::string,std::vector<int>>> per(active.size()); for(std::size_t ci=0;ci<active.size();++ci){if(projected[ci].empty()) continue; for(const auto&lab:locals[ci].labels) owners[lab].push_back(ci); for(const auto&m:aligns[ci].moves) if(m.model&&*m.model!=">>"){auto&v=per[ci][*m.model];v.push_back(m.synchronous()?0:1);}} for(auto&kv:owners) if(kv.second.size()>1) for(auto ci:kv.second) statuses[kv.first].push_back({ci,per[ci][kv.first]});
      std::set<std::size_t> merge_set; bool disagree=false; for(auto&kv:statuses){if(kv.second.size()<2)continue; const auto&base=kv.second.front().second; bool bad=false; for(std::size_t j=1;j<kv.second.size();++j) if(kv.second[j].second!=base) bad=true; if(bad){disagree=true;++out.diagnostics.border_disagreements; ++trace_disagreements; for(const auto&entry:kv.second) merge_set.insert(entry.first);}}
      auto dag_check = compose_dag(aligns, projected_indices, names, o); if(!dag_check.acyclic){disagree=true;++out.diagnostics.border_disagreements; ++trace_disagreements; merge_set.insert(dag_check.cycle_components.begin(),dag_check.cycle_components.end());}
      if(!disagree) break; ++out.diagnostics.agreement_iterations; if(trace_disagreements>o.threshold_border_agreement){out.complete=false;out.diagnostics.note="border agreement threshold exceeded";aborted=true;break;} if(merge_set.size()<2) break; ++out.diagnostics.merges; Region merged; for(auto i:merge_set){merged.places.insert(merged.places.end(),active[i].places.begin(),active[i].places.end()); merged.transitions.insert(merged.transitions.end(),active[i].transitions.begin(),active[i].transitions.end());} std::sort(merged.places.begin(),merged.places.end()); merged.places.erase(std::unique(merged.places.begin(),merged.places.end()),merged.places.end()); std::sort(merged.transitions.begin(),merged.transitions.end()); merged.transitions.erase(std::unique(merged.transitions.begin(),merged.transitions.end()),merged.transitions.end()); std::vector<Region> next; for(std::size_t i=0;i<active.size();++i) if(!merge_set.count(i)) next.push_back(active[i]); next.push_back(std::move(merged)); active=std::move(next);
    }
    if(aborted) { DecomposedAlignmentTrace row; row.activities=names; row.complete=false; row.exhausted=forced_exhausted; row.timed_out=forced_timeout; for (const auto& sr: aligns) { row.exhausted|=sr.exhausted; row.timed_out|=sr.timed_out; } row.best_worst_cost=trace_worst; double worst=trace_worst; row.fitness=worst<=1e-12?1.0:0.0; total_worst+=worst; out.traces.push_back(std::move(row)); out.complete=false; out.timed_out|=forced_timeout; out.exhausted|=forced_exhausted; continue; } DecomposedAlignmentTrace row; row.activities=names; row.complete=true; for(auto&sr:aligns){row.complete&=sr.found;row.timed_out|=sr.timed_out;row.exhausted|=sr.exhausted;}
    auto dag = compose_dag(aligns, projected_indices, names, o); if(!dag.acyclic){row.complete=false;out.complete=false;}
    // A component without any alignment empties the whole result, as in the reference.
    if(!row.complete && std::any_of(aligns.begin(),aligns.end(),[](const SearchResult& sr){return !sr.found&&!sr.timed_out&&!sr.exhausted;})){dag.moves.clear();dag.cost=0;}
    // Silent moves count in the cost but not against the fitness.
    row.alignment=std::move(dag.moves); row.cost=dag.cost; row.best_worst_cost=trace_worst; double worst=trace_worst; double visible_cost=row.cost; for(const auto& m:row.alignment) if(!(m.log && *m.log!=">>") && !(m.model && *m.model!=">>")) visible_cost-=move_cost(m,o); row.fitness=worst<=1e-12?1.0:1.0-std::max(0.0,visible_cost)/worst; out.total_cost+=row.cost; total_worst+=worst; out.traces.push_back(row); if(o.use_cache && row.complete)row_cache.emplace(names,row); if(!row.complete){out.complete=false;out.timed_out|=row.timed_out;out.exhausted|=row.exhausted;}
  }
  out.fitness=total_worst<=1e-12?1.0:std::clamp(1.0-out.total_cost/total_worst,0.0,1.0); return out;
}

} // namespace minera
