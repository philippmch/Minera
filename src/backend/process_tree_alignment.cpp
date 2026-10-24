#include "minera/process_tree_alignment.hpp"

#include "minera/dfg_variants.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <unordered_map>

namespace minera {
namespace {
using Op = ProcessTreeOperator;
using Clock = std::chrono::steady_clock;
constexpr std::size_t kMaxInclusiveChildren = 12;

void validate_selector(std::string_view selector) {
  if (selector.empty() || std::all_of(selector.begin(), selector.end(),
                                      [](unsigned char c) {
                                        return std::isspace(c) != 0;
                                      })) {
    throw std::invalid_argument(
        "process-tree alignment activity selector must not be blank");
  }
}

std::string selected_activity(const Event& event,
                              std::string_view selector) {
  const auto* value = dfg_event_field_value(event, selector);
  if (value == nullptr)
    throw std::invalid_argument(
        "process-tree alignment: missing or null activity " +
        std::string(selector));
  return *value;
}

struct Expr;
using EP = std::shared_ptr<const Expr>;
struct Expr {
  enum Kind { Epsilon, Activity, Tau, Sequence, Xor, Parallel, Star } kind;
  std::string label, key;
  std::vector<EP> children;
  bool nullable = false;
  std::size_t min_visible = 0;
};

struct ExprFactory {
  // When set, tau leaves compile to explicit silent steps so alignments list
  // them as model moves without a label, as SEARCH_GRAPH_PT does.
  bool explicit_tau = false;
  std::map<std::string, EP> pool;
  EP intern(Expr e) {
    auto it = pool.find(e.key); if (it != pool.end()) return it->second;
    EP p = std::make_shared<Expr>(std::move(e)); pool.emplace(p->key, p); return p;
  }
  EP eps() { Expr e; e.kind=Expr::Epsilon; e.key="e"; e.nullable=true; return intern(std::move(e)); }
  EP tau() { Expr e; e.kind=Expr::Tau; e.key="t"; return intern(std::move(e)); }
  EP act(const std::string& l) { Expr e; e.kind=Expr::Activity; e.label=l; e.key="a"+std::to_string(l.size())+":"+l; e.min_visible=1; return intern(std::move(e)); }
  EP seq(std::vector<EP> xs) {
    std::vector<EP> flat; for (auto& x: xs) { if (x->kind==Expr::Sequence) flat.insert(flat.end(),x->children.begin(),x->children.end()); else if (x->kind!=Expr::Epsilon) flat.push_back(x); }
    if (flat.empty()) return eps(); if (flat.size()==1) return flat[0];
    Expr e; e.kind=Expr::Sequence; e.children=flat; e.key="s("; e.nullable=true; e.min_visible=0;
    for (auto& x: flat) { e.key += x->key+","; e.nullable &= x->nullable; e.min_visible += x->min_visible; } e.key += ")"; return intern(std::move(e));
  }
  EP xorr(std::vector<EP> xs) {
    std::map<std::string,EP> u; for(auto& x:xs) { if(x->kind==Expr::Xor) for(auto& c:x->children) u[c->key]=c; else u[x->key]=x; }
    if(u.empty()) return eps(); if(u.size()==1) return u.begin()->second; Expr e; e.kind=Expr::Xor; for(auto& p:u)e.children.push_back(p.second); e.key="x("; e.nullable=false; e.min_visible=std::numeric_limits<std::size_t>::max(); for(auto& c:e.children){e.key+=c->key+",";e.nullable|=c->nullable;e.min_visible=std::min(e.min_visible,c->min_visible);} e.key+=")"; return intern(std::move(e));
  }
  EP par(std::vector<EP> xs) {
    std::vector<EP> flat; for(auto& x:xs){ if(x->kind==Expr::Parallel) flat.insert(flat.end(),x->children.begin(),x->children.end()); else if(x->kind!=Expr::Epsilon) flat.push_back(x); }
    if(flat.empty()) return eps(); if(flat.size()==1)return flat[0]; std::sort(flat.begin(),flat.end(),[](EP a,EP b){return a->key<b->key;}); Expr e; e.kind=Expr::Parallel;e.children=flat;e.key="p(";e.nullable=true;e.min_visible=0;for(auto& c:flat){e.key+=c->key+",";e.nullable &= c->nullable;e.min_visible+=c->min_visible;}e.key+=")";return intern(std::move(e));
  }
  EP inclusive(std::vector<EP> xs) {
    if (xs.empty()) return eps();
    if (xs.size() == 1) return xs.front();
    if (xs.size() > kMaxInclusiveChildren)
      throw std::invalid_argument(
          "process-tree inclusive OR exceeds the alignment expansion limit of 12 children");
    std::vector<EP> alternatives;
    std::vector<EP> selected;
    const auto visit = [&](const auto& self, std::size_t index) -> void {
      if (index == xs.size()) {
        if (!selected.empty()) alternatives.push_back(par(selected));
        return;
      }
      self(self, index + 1);
      selected.push_back(xs[index]);
      self(self, index + 1);
      selected.pop_back();
    };
    visit(visit, 0);
    return xorr(std::move(alternatives));
  }
  EP star(EP x) { Expr e; e.kind=Expr::Star;e.children={x};e.key="*("+x->key+")";e.nullable=true;return intern(std::move(e)); }
};

void validate(const ProcessTreeNode& n, const ProcessTree& t, ProcessTreeAlignmentVariant v, std::size_t& h) {
  h=std::max<std::size_t>(h,1);
  if(n.op==Op::kActivity){if(!n.children.empty()||n.activity>=t.activities().size()) throw std::invalid_argument("process-tree alignment: invalid activity leaf"); return;}
  if(n.op==Op::kTau){if(!n.children.empty()) throw std::invalid_argument("process-tree alignment: tau cannot have children"); return;}
  if((n.op==Op::kSequence||n.op==Op::kExclusive||n.op==Op::kInclusiveChoice||n.op==Op::kParallel||n.op==Op::kInterleaving) && n.children.empty()) throw std::invalid_argument("process-tree alignment: operator requires children");
  if(n.op==Op::kLoop && n.children.empty()) throw std::invalid_argument("process-tree alignment: loop requires a body");
  if(v==ProcessTreeAlignmentVariant::kMilp && n.op==Op::kLoop && n.children.size()!=2) throw std::invalid_argument("process-tree alignment MILP supports only binary loops");
  for(const auto& c:n.children){std::size_t ch=0;validate(c,t,v,ch);h=std::max(h,ch+1);} }

EP compile(const ProcessTreeNode& n, const ProcessTree& t, ExprFactory& f) {
  switch(n.op){
    case Op::kActivity:return f.act(t.activities()[n.activity]); case Op::kTau:return f.explicit_tau?f.tau():f.eps();
    case Op::kSequence:{std::vector<EP>x;for(auto&c:n.children)x.push_back(compile(c,t,f));return f.seq(x);}
    case Op::kExclusive:{std::vector<EP>x;for(auto&c:n.children)x.push_back(compile(c,t,f));return f.xorr(x);}
    case Op::kInclusiveChoice:{std::vector<EP>x;for(auto&c:n.children)x.push_back(compile(c,t,f));return f.inclusive(x);}
    case Op::kParallel:{std::vector<EP>x;for(auto&c:n.children)x.push_back(compile(c,t,f));return f.par(x);}
    case Op::kInterleaving:{if(n.children.size()>8)throw std::invalid_argument("process-tree alignment: interleaving exceeds the expansion limit of 8 children");std::vector<std::size_t>o(n.children.size());std::iota(o.begin(),o.end(),0);std::vector<EP>a;do{std::vector<EP>x;for(auto i:o)x.push_back(compile(n.children[i],t,f));a.push_back(f.seq(x));}while(std::next_permutation(o.begin(),o.end()));return f.xorr(std::move(a));}
    case Op::kLoop:{EP body=compile(n.children[0],t,f);if(n.children.size()==1)return body;std::vector<EP> rs;for(std::size_t i=1;i<n.children.size();++i)rs.push_back(compile(n.children[i],t,f));return f.seq({body,f.star(f.seq({f.xorr(rs),body}))});}
  } return f.eps();
}

// A derivative step: the label consumed (empty and silent for a tau leaf) and
// the residual expression.
struct Deriv { std::string first; EP second; bool silent = false; };
std::vector<Deriv> derivatives(const EP& e, ExprFactory& f, std::unordered_map<std::string,std::vector<Deriv>>& memo) {
  auto mi=memo.find(e->key); if(mi!=memo.end()) return mi->second; std::vector<Deriv> out;
  switch(e->kind){
    case Expr::Activity: out.push_back({e->label,f.eps()}); break;
    case Expr::Tau: out.push_back({"",f.eps(),true}); break;
    case Expr::Epsilon: break;
    case Expr::Xor: for(auto& c:e->children){auto d=derivatives(c,f,memo);out.insert(out.end(),d.begin(),d.end());} break;
    case Expr::Sequence:{
      for(std::size_t i=0;i<e->children.size();++i){auto d=derivatives(e->children[i],f,memo);EP tail=f.seq(std::vector<EP>(e->children.begin()+i+1,e->children.end()));for(auto& q:d)out.push_back({q.first,f.seq({q.second,tail}),q.silent});if(!e->children[i]->nullable)break;}
      break;}
    case Expr::Parallel:
      for(std::size_t i=0;i<e->children.size();++i){auto d=derivatives(e->children[i],f,memo);for(auto& q:d){auto cs=e->children;cs[i]=q.second;out.push_back({q.first,f.par(cs),q.silent});}} break;
    case Expr::Star:{auto d=derivatives(e->children[0],f,memo);for(auto&q:d)out.push_back({q.first,f.seq({q.second,e}),q.silent});break;}
  }
  std::sort(out.begin(),out.end(),[](const Deriv&a,const Deriv&b){return std::tie(a.silent,a.first,a.second->key)<std::tie(b.silent,b.first,b.second->key);}); out.erase(std::unique(out.begin(),out.end(),[](const Deriv&a,const Deriv&b){return a.silent==b.silent&&a.first==b.first&&a.second->key==b.second->key;}),out.end()); memo[e->key]=out; return out;
}

struct Limits {
  const ProcessTreeAlignmentOptions& o;
  Clock::time_point trace_start, total_start;
  std::size_t expanded = 0;
  bool exhausted = false, timed_out = false, total_timed_out = false;

  bool ok() {
    if (o.max_states && expanded >= o.max_states) {
      exhausted = true;
      return false;
    }
    const auto now = Clock::now();
    double per_trace = 0.0;
    for (const double value : {o.max_time_seconds,
                               o.max_trace_time_seconds,
                               o.max_per_trace_time_seconds}) {
      if (value > 0.0)
        per_trace = per_trace > 0.0 ? std::min(per_trace, value) : value;
    }
    if (per_trace > 0.0 &&
        std::chrono::duration<double>(now - trace_start).count() >= per_trace) {
      timed_out = true;
      return false;
    }
    if (o.max_total_time_seconds > 0.0 &&
        std::chrono::duration<double>(now - total_start).count() >=
            o.max_total_time_seconds) {
      timed_out = total_timed_out = true;
      return false;
    }
    ++expanded;
    return true;
  }
};

struct SearchOut { bool found=false; std::size_t cost=0; std::vector<ProcessTreeAlignmentMove> moves; };
struct QNode { std::size_t f,g,i,id; EP r; bool operator>(const QNode& o)const{return f!=o.f?f>o.f:(g!=o.g?g>o.g:(i!=o.i?i<o.i:id>o.id));} };
SearchOut search(const std::vector<std::string>& tr, EP root, ExprFactory& fac, Limits& lim, bool astar, ProcessTreeAlignmentDiagnostics& diag) {
  struct Rec{std::pair<std::size_t,std::string> from;int move;std::string lab;}; std::priority_queue<QNode,std::vector<QNode>,std::greater<QNode>> pq; std::map<std::pair<std::size_t,std::string>,std::size_t> best; std::map<std::pair<std::size_t,std::string>,Rec> prev; std::unordered_map<std::string,std::vector<Deriv>> dm; std::size_t sid=0; auto heur=[&](std::size_t i,EP r){if(!astar)return std::size_t(0);std::size_t rem=tr.size()-i,m=r->min_visible;return m>rem?m-rem:std::size_t(0);};
  pq.push({heur(0,root),0,0,sid++,root});best[{0,root->key}]=0;diag.queued_states++;
  std::pair<std::size_t,std::string> goal; bool got=false; QNode gn{};
  while(!pq.empty()){auto n=pq.top();pq.pop();auto k=std::make_pair(n.i,n.r->key);if(best[k]!=n.g)continue;if(!lim.ok())break;diag.visited_states++;if(n.i==tr.size()&&n.r->nullable){got=true;goal=k;gn=n;break;}auto ds=derivatives(n.r,fac,dm);
    auto push=[&](std::size_t ni,EP nr,std::size_t ng,int mt,const std::string& lab){auto kk=std::make_pair(ni,nr->key);auto it=best.find(kk);if(it!=best.end()&&it->second<=ng)return;best[kk]=ng;prev[kk]={k,mt,lab};pq.push({ng+heur(ni,nr),ng,ni,sid++,nr});diag.queued_states++;diag.traversed_arcs++;};
    if(n.i<tr.size())push(n.i+1,n.r,n.g+1,1,tr[n.i]);
    for(auto& d:ds){if(d.silent){push(n.i,d.second,n.g,3,d.first);continue;}push(n.i,d.second,n.g+1,2,d.first);if(n.i<tr.size()&&d.first==tr[n.i])push(n.i+1,d.second,n.g,0,d.first);}
  }
  SearchOut out;out.found=got;if(!got)return out;out.cost=gn.g;
  std::vector<std::tuple<int,std::string>> rev; auto cur=goal; while(cur.first!=0||cur.second!=root->key){auto it=prev.find(cur);if(it==prev.end())break;rev.emplace_back(it->second.move,it->second.lab);cur=it->second.from;} std::reverse(rev.begin(),rev.end());
  for(auto& m:rev){int t;std::string l;std::tie(t,l)=m;if(t==0)out.moves.push_back({ProcessTreeAlignmentMoveType::kSynchronous,l,l,l});else if(t==1)out.moves.push_back({ProcessTreeAlignmentMoveType::kLog,l,"",l});else out.moves.push_back({ProcessTreeAlignmentMoveType::kModel,"",l,l});} return out;
}

using Word = std::vector<std::string>;

SearchOut log_only(const Word& trace) {
  SearchOut out;
  out.found = true;
  out.cost = trace.size();
  for (const auto& activity : trace)
    out.moves.push_back({ProcessTreeAlignmentMoveType::kLog, activity, "", activity});
  return out;
}

std::set<std::string> descendant_labels(const ProcessTreeNode& node,
                                        const ProcessTree& tree) {
  std::set<std::string> labels;
  if (node.op == Op::kActivity) {
    labels.insert(tree.activities()[node.activity]);
  } else {
    for (const auto& child : node.children) {
      const auto nested = descendant_labels(child, tree);
      labels.insert(nested.begin(), nested.end());
    }
  }
  return labels;
}

// APPROXIMATED_ORIGINAL fitness floor: the cheapest model run of the tree read
// as nested binary operators (n-ary operators nest to the right), where a
// visible leaf costs 10000, a tau leaf 1 and every binary operator 2. A loop
// runs its body once. The reference divides a trace's cost by this floor
// plus the trace length.
std::uint64_t approximation_floor(const ProcessTreeNode& node) {
  switch (node.op) {
    case Op::kActivity: return 10000;
    case Op::kTau: return 1;
    case Op::kExclusive: {
      const std::size_t n = node.children.size();
      std::uint64_t best = std::numeric_limits<std::uint64_t>::max();
      for (std::size_t i = 0; i < n; ++i)
        best = std::min<std::uint64_t>(
            best, approximation_floor(node.children[i]) + 2 * std::min(i + 1, n - 1));
      return best;
    }
    case Op::kLoop: return approximation_floor(node.children.front()) + 2;
    default: {
      std::uint64_t total = 2 * (node.children.size() - 1);
      for (const auto& child : node.children) total += approximation_floor(child);
      return total;
    }
  }
}

std::size_t node_height(const ProcessTreeNode& node) {
  std::size_t height = 1;
  for (const auto& child : node.children)
    height = std::max(height, node_height(child) + 1);
  return height;
}

struct MemoKey {
  const ProcessTreeNode* node = nullptr;
  Word trace;
  bool approximate = false;
  bool operator<(const MemoKey& other) const {
    if (node != other.node) return std::less<const ProcessTreeNode*>{}(node, other.node);
    if (approximate != other.approximate) return approximate < other.approximate;
    return trace < other.trace;
  }
};

struct RecursiveAligner {
  const ProcessTree& tree;
  const ProcessTreeAlignmentOptions& options;
  ExprFactory& expressions;
  Limits& limits;
  ProcessTreeAlignmentDiagnostics& diagnostics;
  std::map<MemoKey, SearchOut>& memo;
  std::size_t approximation_loop_fallbacks = 0;

  SearchOut exact_subtree(const ProcessTreeNode& node, const Word& trace) {
    const auto expression = compile(node, tree, expressions);
    return search(trace, expression, expressions, limits, false, diagnostics);
  }

  SearchOut leaf(const ProcessTreeNode& node, const Word& trace) {
    if (node.op == Op::kTau) return log_only(trace);
    const auto& label = tree.activities()[node.activity];
    for (std::size_t matched = 0; matched < trace.size(); ++matched) {
      if (trace[matched] != label) continue;
      SearchOut out;
      out.found = true;
      out.cost = trace.size() - 1;
      for (std::size_t i = 0; i < matched; ++i)
        out.moves.push_back({ProcessTreeAlignmentMoveType::kLog, trace[i], "", trace[i]});
      out.moves.push_back({ProcessTreeAlignmentMoveType::kSynchronous, label, label, label});
      for (std::size_t i = matched + 1; i < trace.size(); ++i)
        out.moves.push_back({ProcessTreeAlignmentMoveType::kLog, trace[i], "", trace[i]});
      return out;
    }
    SearchOut out = log_only(trace);
    out.cost += 1;
    out.moves.push_back({ProcessTreeAlignmentMoveType::kModel, "", label, label});
    return out;
  }

  SearchOut choose_xor(const ProcessTreeNode& node, const Word& trace,
                       bool approximate) {
    SearchOut best;
    for (const auto& child : node.children) {
      auto candidate = align(child, trace, approximate);
      if (!candidate.found) return {};
      if (!best.found || candidate.cost < best.cost) best = std::move(candidate);
    }
    return best;
  }

  SearchOut sequence_from(const ProcessTreeNode& node, std::size_t child_index,
                          std::size_t trace_index, const Word& trace,
                          bool approximate,
                          std::map<std::pair<std::size_t, std::size_t>, SearchOut>& cache) {
    if (!limits.ok()) return {};
    const auto key = std::make_pair(child_index, trace_index);
    const auto cached = cache.find(key);
    if (cached != cache.end()) return cached->second;
    if (child_index == node.children.size()) {
      SearchOut out;
      out.found = trace_index == trace.size();
      cache.emplace(key, out);
      return out;
    }
    SearchOut best;
    for (std::size_t end = trace_index; end <= trace.size(); ++end) {
      Word part(trace.begin() + static_cast<std::ptrdiff_t>(trace_index),
                trace.begin() + static_cast<std::ptrdiff_t>(end));
      auto left = align(node.children[child_index], part, approximate);
      if (!left.found) {
        if (limits.exhausted || limits.timed_out) return {};
        continue;
      }
      auto right = sequence_from(node, child_index + 1, end, trace,
                                 approximate, cache);
      if (!right.found) {
        if (limits.exhausted || limits.timed_out) return {};
        continue;
      }
      SearchOut candidate;
      candidate.found = true;
      candidate.cost = left.cost + right.cost;
      candidate.moves = std::move(left.moves);
      candidate.moves.insert(candidate.moves.end(), right.moves.begin(),
                             right.moves.end());
      if (!best.found || candidate.cost < best.cost) best = std::move(candidate);
    }
    cache.emplace(key, best);
    return best;
  }

  static SearchOut joined(std::initializer_list<const SearchOut*> parts) {
    SearchOut out;
    out.found = true;
    for (const auto* part : parts) {
      if (!part->found) return {};
      out.cost += part->cost;
      out.moves.insert(out.moves.end(), part->moves.begin(), part->moves.end());
    }
    return out;
  }

  // DYNAMIC_PROGRAMMING loop, as the reference evaluates it: an empty segment
  // skips the loop, any other segment is an optional leading redo, the body,
  // any number of (redo, body) rounds whose redo consumes at least one event,
  // and an optional extra body at the end.
  SearchOut align_loop(const ProcessTreeNode& node, const Word& trace) {
    SearchOut empty;
    empty.found = true;
    if (trace.empty()) return empty;
    const std::size_t n = trace.size();
    const auto part = [&](std::size_t from, std::size_t to) {
      return Word(trace.begin() + static_cast<std::ptrdiff_t>(from),
                  trace.begin() + static_cast<std::ptrdiff_t>(to));
    };
    const auto body = [&](std::size_t from, std::size_t to) {
      return align(node.children.front(), part(from, to), false);
    };
    const auto redo = [&](std::size_t from, std::size_t to) {
      SearchOut best;
      for (std::size_t r = 1; r < node.children.size(); ++r) {
        auto candidate = align(node.children[r], part(from, to), false);
        if (!candidate.found) return SearchOut{};
        if (!best.found || candidate.cost < best.cost) best = std::move(candidate);
      }
      return best;
    };
    const auto keep = [](SearchOut& best, SearchOut candidate) {
      if (candidate.found && (!best.found || candidate.cost < best.cost))
        best = std::move(candidate);
    };
    // after[s]: the cheapest completion once a body ended at s.
    std::vector<SearchOut> after(n + 1);
    for (std::size_t s = n + 1; s-- > 0;) {
      SearchOut best = s == n ? empty : body(s, n);
      for (std::size_t e = s + 1; e <= n; ++e) {
        const auto r = redo(s, e);
        if (!r.found) continue;
        for (std::size_t e2 = e; e2 <= n; ++e2) {
          const auto b = body(e, e2);
          keep(best, joined({&r, &b, &after[e2]}));
        }
      }
      if (limits.exhausted || limits.timed_out) return {};
      after[s] = std::move(best);
    }
    SearchOut best;
    for (std::size_t lead = 0; lead <= n; ++lead) {
      const auto r = lead == 0 ? empty : redo(0, lead);
      if (!r.found) continue;
      for (std::size_t end = lead; end <= n; ++end) {
        const auto b = body(lead, end);
        keep(best, joined({&r, &b, &after[end]}));
      }
    }
    if (limits.exhausted || limits.timed_out) return {};
    return best;
  }

  SearchOut align_sequence(const ProcessTreeNode& node, const Word& trace,
                           bool approximate) {
    std::map<std::pair<std::size_t, std::size_t>, SearchOut> cache;
    return sequence_from(node, 0, 0, trace, approximate, cache);
  }

  SearchOut align_parallel(const ProcessTreeNode& node, const Word& trace,
                           bool approximate) {
    std::vector<std::set<std::string>> labels;
    labels.reserve(node.children.size());
    for (const auto& child : node.children)
      labels.push_back(descendant_labels(child, tree));

    std::vector<std::vector<std::size_t>> eligible(trace.size());
    for (std::size_t i = 0; i < trace.size(); ++i) {
      for (std::size_t child = 0; child < node.children.size(); ++child) {
        if (labels[child].contains(trace[i])) eligible[i].push_back(child);
      }
    }

    const auto evaluate = [&](const std::vector<Word>& projected,
                              const std::vector<std::vector<std::size_t>>& positions)
        -> SearchOut {
      struct RankedMove {
        std::size_t anchor = 0;
        int phase = 0;
        std::size_t child = 0;
        std::size_t order = 0;
        ProcessTreeAlignmentMove move;
      };
      SearchOut out;
      out.found = true;
      std::vector<RankedMove> ranked;
      std::vector<bool> assigned(trace.size(), false);
      for (const auto& child_positions : positions)
        for (const auto position : child_positions) assigned[position] = true;
      for (std::size_t i = 0; i < trace.size(); ++i) {
        if (!assigned[i]) {
          ++out.cost;
          ranked.push_back({i, 1, node.children.size(), i,
                            {ProcessTreeAlignmentMoveType::kLog, trace[i], "",
                             trace[i]}});
        }
      }
      for (std::size_t child = 0; child < node.children.size(); ++child) {
        auto aligned = align(node.children[child], projected[child], approximate);
        if (!aligned.found) return {};
        out.cost += aligned.cost;
        std::size_t consumed = 0;
        std::size_t local_order = 0;
        for (const auto& move : aligned.moves) {
          if (move.type == ProcessTreeAlignmentMoveType::kModel) {
            const auto anchor = consumed < positions[child].size()
                                    ? positions[child][consumed]
                                    : trace.size();
            ranked.push_back({anchor, 0, child, local_order++, move});
            continue;
          }
          if (consumed >= positions[child].size()) return {};
          ranked.push_back({positions[child][consumed++], 1, child,
                            local_order++, move});
        }
        if (consumed != positions[child].size()) return {};
      }
      std::stable_sort(ranked.begin(), ranked.end(),
                       [](const RankedMove& left, const RankedMove& right) {
        return std::tie(left.anchor, left.phase, left.child, left.order) <
               std::tie(right.anchor, right.phase, right.child, right.order);
      });
      out.moves.reserve(ranked.size());
      for (auto& item : ranked) out.moves.push_back(std::move(item.move));
      return out;
    };

    std::vector<Word> projected(node.children.size());
    std::vector<std::vector<std::size_t>> positions(node.children.size());
    SearchOut best;
    std::function<void(std::size_t)> assign = [&](std::size_t event_index) {
      if (!limits.ok()) return;
      if (event_index == trace.size()) {
        auto candidate = evaluate(projected, positions);
        if (candidate.found && (!best.found || candidate.cost < best.cost))
          best = std::move(candidate);
        return;
      }
      if (eligible[event_index].empty()) {
        assign(event_index + 1);
        return;
      }
      for (const auto child : eligible[event_index]) {
        projected[child].push_back(trace[event_index]);
        positions[child].push_back(event_index);
        assign(event_index + 1);
        positions[child].pop_back();
        projected[child].pop_back();
        if (limits.exhausted || limits.timed_out) return;
      }
    };
    assign(0);
    if (limits.exhausted || limits.timed_out) return {};
    return best;
  }

  SearchOut align(const ProcessTreeNode& node, const Word& trace,
                  bool approximate) {
    const MemoKey key{&node, trace, approximate};
    const auto cached = memo.find(key);
    if (cached != memo.end()) return cached->second;
    if (!limits.ok()) return {};
    ++diagnostics.visited_states;
    ++diagnostics.queued_states;
    ++diagnostics.traversed_arcs;

    if (approximate &&
        ((options.max_trace_length == 0 ||
          trace.size() <= options.max_trace_length) ||
         (options.max_process_tree_height == 0 ||
          node_height(node) <= options.max_process_tree_height))) {
      auto out = exact_subtree(node, trace);
      if (out.found) memo.emplace(key, out);
      return out;
    }

    SearchOut out;
    switch (node.op) {
      case Op::kActivity:
      case Op::kTau:
        out = leaf(node, trace);
        break;
      case Op::kExclusive:
        out = choose_xor(node, trace, approximate);
        break;
      case Op::kSequence:
        out = align_sequence(node, trace, approximate);
        break;
      case Op::kParallel:
        out = align_parallel(node, trace, approximate);
        break;
      case Op::kInclusiveChoice:
      case Op::kInterleaving:
        out = exact_subtree(node, trace);
        break;
      case Op::kLoop:
        if (approximate) {
          ++approximation_loop_fallbacks;
          out = exact_subtree(node, trace);
        } else {
          out = align_loop(node, trace);
        }
        break;
    }
    if (out.found) memo.emplace(key, out);
    return out;
  }
};

} // namespace

ProcessTreeAlignmentResult align_process_tree(
    const EventLog& log, const ProcessTree& tree,
    ProcessTreeAlignmentOptions options) {
  validate_selector(options.activity_attribute);
  switch (options.variant) {
    case ProcessTreeAlignmentVariant::kSearchGraphPt:
    case ProcessTreeAlignmentVariant::kDynamicProgramming:
    case ProcessTreeAlignmentVariant::kMilp:
    case ProcessTreeAlignmentVariant::kApproximatedOriginal:
      break;
    default:
      throw std::invalid_argument("process-tree alignment: unknown variant");
  }
  if (options.batch_size == 0)
    throw std::invalid_argument("process-tree alignment: batch_size must be positive");
  for (const double value : {options.max_time_seconds,
                             options.max_total_time_seconds,
                             options.max_trace_time_seconds,
                             options.max_per_trace_time_seconds}) {
    if (!std::isfinite(value) || value < 0.0)
      throw std::invalid_argument(
          "process-tree alignment: time limits must be finite and non-negative");
  }
  if (options.variant != ProcessTreeAlignmentVariant::kDynamicProgramming &&
      options.variant != ProcessTreeAlignmentVariant::kApproximatedOriginal &&
      (options.use_global_cache || options.batch_size != 1000 ||
       !options.clear_cache)) {
    throw std::invalid_argument(
        "process-tree alignment: cache and batch settings require a recursive variant");
  }
  ProcessTreeAlignmentResult result;
  result.variant = options.variant;
  std::size_t height = 0;
  validate(tree.root(), tree, options.variant, height);
  ExprFactory expressions;
  expressions.explicit_tau =
      options.variant == ProcessTreeAlignmentVariant::kSearchGraphPt;
  const auto root = compile(tree.root(), tree, expressions);
  const auto total_start = Clock::now();
  // A trace's fitness is 1 - cost / (length + floor), where the floor is the
  // variant's own cost for the empty trace (the approximated variant uses its
  // binary-operator floor instead).
  std::uint64_t model_floor = root->min_visible;
  if (options.variant == ProcessTreeAlignmentVariant::kApproximatedOriginal) {
    model_floor = approximation_floor(tree.root());
  } else if (options.variant == ProcessTreeAlignmentVariant::kDynamicProgramming) {
    Limits limits{options, total_start, total_start};
    std::map<MemoKey, SearchOut> memo;
    RecursiveAligner recursive{tree, options, expressions, limits,
                               result.diagnostics, memo};
    const auto empty = recursive.align(tree.root(), {}, false);
    if (empty.found) model_floor = empty.cost;
  }

  std::map<MemoKey, SearchOut> global_memo;
  std::map<Word, ProcessTreeTraceAlignment> variant_cache;
  std::size_t unique_variants = 0;
  std::size_t approximation_loop_fallbacks = 0;
  bool total_deadline_reached = false;

  for (const auto& trace : log.traces) {
    ProcessTreeTraceAlignment row;
    row.trace.reserve(trace.events.size());
    for (const auto& event : trace.events)
      row.trace.push_back(selected_activity(event, options.activity_attribute));

    if (!total_deadline_reached && options.max_total_time_seconds > 0.0 &&
        std::chrono::duration<double>(Clock::now() - total_start).count() >=
            options.max_total_time_seconds) {
      total_deadline_reached = true;
      result.timed_out = true;
      result.computed = result.completed = false;
    }

    if (total_deadline_reached) {
      row.completed = false;
      row.timed_out = true;
      row.fitness = 0.0;
      result.traces.push_back(std::move(row));
      continue;
    }

    const auto repeated = variant_cache.find(row.trace);
    if (repeated != variant_cache.end()) {
      row = repeated->second;
      result.total_cost += row.cost;
      ++result.diagnostics.traces_aligned;
      result.traces.push_back(std::move(row));
      continue;
    }

    Limits limits{options, Clock::now(), total_start};
    std::map<MemoKey, SearchOut> local_memo;
    auto& memo = options.use_global_cache ? global_memo : local_memo;
    SearchOut aligned;
    const auto visited_before = result.diagnostics.visited_states;
    if (options.variant == ProcessTreeAlignmentVariant::kSearchGraphPt) {
      aligned = search(row.trace, root, expressions, limits, false,
                       result.diagnostics);
    } else if (options.variant == ProcessTreeAlignmentVariant::kMilp) {
      aligned = search(row.trace, root, expressions, limits, true,
                       result.diagnostics);
      (void)visited_before;
    } else {
      RecursiveAligner recursive{tree, options, expressions, limits,
                                 result.diagnostics, memo};
      aligned = recursive.align(
          tree.root(), row.trace,
          options.variant == ProcessTreeAlignmentVariant::kApproximatedOriginal);
      approximation_loop_fallbacks += recursive.approximation_loop_fallbacks;
    }

    row.completed = aligned.found;
    row.timed_out = limits.timed_out;
    if (aligned.found) {
      row.cost = aligned.cost;
      row.moves = std::move(aligned.moves);
      // Not clamped: like the reference, a trace whose cost exceeds its
      // length plus the floor gets a negative fitness.
      const std::size_t denominator = row.trace.size() + model_floor;
      row.fitness = denominator == 0
                        ? 0.0
                        : 1.0 - static_cast<double>(row.cost) /
                                    static_cast<double>(denominator);
      result.total_cost += row.cost;
      ++result.diagnostics.traces_aligned;
      variant_cache.emplace(row.trace, row);
    } else {
      row.fitness = 0.0;
      result.computed = result.completed = false;
    }
    result.exhausted = result.exhausted || limits.exhausted;
    result.timed_out = result.timed_out || limits.timed_out;
    total_deadline_reached = limits.total_timed_out;
    result.traces.push_back(std::move(row));

    ++unique_variants;
    if (options.use_global_cache && options.clear_cache &&
        options.batch_size != 0 && unique_variants % options.batch_size == 0)
      global_memo.clear();
  }

  std::size_t denominator = 0;
  for (const auto& trace : log.traces)
    denominator += trace.events.size() + model_floor;
  result.fitness = !result.computed || denominator == 0
                       ? 0.0
                       : std::max(0.0, 1.0 -
                           static_cast<double>(result.total_cost) /
                               static_cast<double>(denominator));

  std::ostringstream message;
  switch (options.variant) {
    case ProcessTreeAlignmentVariant::kSearchGraphPt:
      message << "native residual-expression shortest-path search";
      break;
    case ProcessTreeAlignmentVariant::kDynamicProgramming:
      message << "recursive subtree dynamic programming; loop exact residual fallback";
      break;
    case ProcessTreeAlignmentVariant::kMilp:
      message << "dependency-free residual A* equivalent to the MILP unit-cost objective; no external LP solves";
      break;
    case ProcessTreeAlignmentVariant::kApproximatedOriginal:
      message << "recursive process-tree decomposition";
      if (approximation_loop_fallbacks != 0)
        message << "; " << approximation_loop_fallbacks
                << " loop exact fallback(s)";
      break;
  }
  result.diagnostics.message = message.str();
  return result;
}
}
