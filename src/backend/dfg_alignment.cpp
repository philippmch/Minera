#include "minera/dfg_alignment.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <limits>
#include <optional>
#include <queue>
#include <set>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace minera {
namespace {
constexpr const char* kGap = ">>";
struct Model {
  std::vector<std::string> nodes;
  std::unordered_map<std::string, std::size_t> id;
  std::vector<std::vector<std::size_t>> out;
  std::size_t start = 0, end = 0;
};

void validate_costs(const DfgAlignmentOptions& o) {
  const auto validate_selector = [](std::string_view selector,
                                    std::string_view role) {
    if (selector.empty() || std::all_of(selector.begin(), selector.end(),
                                        [](unsigned char c) {
                                          return std::isspace(c) != 0;
                                        })) {
      throw std::invalid_argument("DFG alignment " + std::string(role) +
                                  " selector must not be blank");
    }
  };
  validate_selector(o.activity_attribute, "activity");
  validate_selector(o.timestamp_attribute, "timestamp");
  auto check = [](const auto& m) {
    for (const auto& kv : m)
      if (!std::isfinite(kv.second) || kv.second < 0.0)
        throw std::invalid_argument("DFG alignment costs must be finite and non-negative");
  };
  check(o.sync_cost); check(o.model_move_cost); check(o.log_move_cost);
  check(o.internal_log_move_cost);
}

const std::string& required_activity(const Event& event,
                                     std::string_view selector) {
  const auto* value = dfg_event_field_value(event, selector);
  if (value == nullptr) {
    throw std::invalid_argument("DFG alignment: missing or null activity " +
                                std::string(selector));
  }
  return *value;
}

const EventAttribute* selected_attribute(const Event& event,
                                         std::string_view selector) {
  const auto matches = [&](std::string_view key) {
    return !key.empty() && key == selector;
  };
  if (event.activity_attribute && !event.activity_attribute->is_null &&
      (matches(event.activity_attribute->key) ||
       matches(event.activity_source_key) ||
       selector == "concept:name" || selector == "activity")) {
    return &*event.activity_attribute;
  }
  if (event.timestamp_attribute && !event.timestamp_attribute->is_null &&
      (matches(event.timestamp_attribute->key) ||
       matches(event.timestamp_source_key) || selector == "time:timestamp" ||
       selector == "timestamp")) {
    return &*event.timestamp_attribute;
  }
  if (event.resource_attribute && !event.resource_attribute->is_null &&
      (matches(event.resource_attribute->key) ||
       matches(event.resource_source_key) || selector == "org:resource" ||
       selector == "resource")) {
    return &*event.resource_attribute;
  }
  if (event.lifecycle_attribute && !event.lifecycle_attribute->is_null &&
      (matches(event.lifecycle_attribute->key) ||
       matches(event.lifecycle_source_key) ||
       selector == "lifecycle:transition" || selector == "lifecycle")) {
    return &*event.lifecycle_attribute;
  }
  for (const auto& attribute : event.attributes) {
    if (attribute.key == selector && !attribute.is_null) return &attribute;
  }
  return nullptr;
}

std::vector<std::string> activity_word(const Trace& trace,
                                       std::string_view selector) {
  std::vector<std::string> result;
  result.reserve(trace.events.size());
  for (const auto& event : trace.events)
    result.push_back(required_activity(event, selector));
  return result;
}

struct SelectedTimestamp {
  std::string value;
  FirstClassAttributeState state = FirstClassAttributeState::kMissing;
  std::string source_key;
  std::optional<EventAttribute> attribute;
};

SelectedTimestamp timestamp_value(const Event& event,
                                  std::string_view selector,
                                  bool allow_conventional_missing = false) {
  const auto* value = dfg_event_field_value(event, selector);
  if (value == nullptr) {
    if (allow_conventional_missing &&
        (selector == "time:timestamp" || selector == "timestamp") &&
        event.timestamp_state == FirstClassAttributeState::kMissing &&
        event.timestamp.empty() && !event.timestamp_attribute) {
      SelectedTimestamp result;
      result.source_key = event.timestamp_source_key;
      result.state = event.timestamp_state;
      return result;
    }
    throw std::invalid_argument("DFG alignment: missing or null timestamp " +
                                std::string(selector));
  }
  SelectedTimestamp result;
  result.value = *value;
  if (const auto* attribute = selected_attribute(event, selector)) {
    result.attribute = *attribute;
    result.source_key = attribute->key;
    result.state = attribute->is_null ? FirstClassAttributeState::kNull
                                      : FirstClassAttributeState::kPresent;
  } else {
    result.source_key = event.timestamp_source_key.empty()
                            ? std::string(selector)
                            : event.timestamp_source_key;
    result.state = event.timestamp_state == FirstClassAttributeState::kMissing
                       ? FirstClassAttributeState::kPresent
                       : event.timestamp_state;
  }
  return result;
}

EventAttribute projected_activity_attribute(const EventAttribute& source,
                                             std::string_view key,
                                             std::string_view value) {
  auto result = source;
  result.key = key.empty() ? source.key : std::string(key);
  result.value = value;
  result.is_null = false;
  result.container = AttributeContainerKind::kScalar;
  result.children.clear();
  if (result.kind != AttributeKind::kString && result.kind != AttributeKind::kId)
    result.kind = AttributeKind::kString;
  result.typed_value = std::monostate{};
  return result;
}

void replace_selected_attribute(std::vector<EventAttribute>& attributes,
                                const EventAttribute& selected) {
  attributes.erase(
      std::remove_if(attributes.begin(), attributes.end(),
                     [&](const EventAttribute& attribute) {
                       return attribute.key == selected.key;
                     }),
      attributes.end());
  attributes.push_back(selected);
}

void set_projected_activity(Event& target, const Event* source,
                            std::string_view selector,
                            std::string_view value) {
  target.activity = value;
  target.activity_state = FirstClassAttributeState::kPresent;
  if (source != nullptr) {
    if (const auto* attribute = selected_attribute(*source, selector)) {
      target.activity_source_key = attribute->key;
      target.activity_attribute = projected_activity_attribute(
          *attribute, attribute->key, value);
      replace_selected_attribute(target.attributes, *target.activity_attribute);
      return;
    }
    if ((selector == "concept:name" || selector == "activity") &&
        !source->activity_source_key.empty()) {
      target.activity_source_key = source->activity_source_key;
      if (source->activity_attribute)
        target.activity_attribute = projected_activity_attribute(
            *source->activity_attribute, target.activity_source_key, value);
      else
        target.activity_attribute.reset();
      if (target.activity_attribute)
        replace_selected_attribute(target.attributes, *target.activity_attribute);
      return;
    }
  }
  target.activity_source_key = std::string(selector);
  target.activity_attribute.reset();
}

// A synchronous projection copies the source event.  Bind the selected
// projection metadata without rebuilding its attribute bag: its original
// kind, typed value, children, and document order are part of the source
// event's observable contract.
void bind_selected_activity(Event& target, const Event& source,
                            std::string_view selector,
                            std::string_view value) {
  target.activity = value;
  target.activity_state = FirstClassAttributeState::kPresent;
  if (const auto* attribute = selected_attribute(source, selector)) {
    target.activity_source_key = attribute->key;
    target.activity_attribute = *attribute;
  } else if ((selector == "concept:name" || selector == "activity" ||
              selector == source.activity_source_key) &&
             !source.activity_source_key.empty()) {
    target.activity_source_key = source.activity_source_key;
    target.activity_attribute = source.activity_attribute;
  } else {
    target.activity_source_key = source.activity_source_key;
    target.activity_attribute = source.activity_attribute;
  }
}

void set_projected_timestamp(Event& target, const SelectedTimestamp& timestamp) {
  target.timestamp = timestamp.value;
  target.timestamp_state = timestamp.state;
  target.timestamp_source_key = timestamp.source_key;
  target.timestamp_attribute = timestamp.attribute;
  if (target.timestamp_attribute)
    replace_selected_attribute(target.attributes, *target.timestamp_attribute);
}

void bind_selected_timestamp(Event& target, const SelectedTimestamp& timestamp) {
  target.timestamp = timestamp.value;
  target.timestamp_state = timestamp.state;
  target.timestamp_source_key = timestamp.source_key;
  target.timestamp_attribute = timestamp.attribute;
}

Model make_model(const DfgFrequency& dfg,
                 const std::map<std::string, std::uint64_t>& sa,
                 const std::map<std::string, std::uint64_t>& ea) {
  std::set<std::string> names;
  for (const auto& e : dfg) { names.insert(e.first.first); names.insert(e.first.second); }
  for (const auto& e : sa) names.insert(e.first);
  for (const auto& e : ea) names.insert(e.first);
  Model m;
  m.nodes.assign(names.begin(), names.end());
  for (std::size_t i=0;i<m.nodes.size();++i) m.id[m.nodes[i]]=i;
  m.start=m.nodes.size(); m.end=m.start+1;
  m.nodes.push_back("\x01START"); m.nodes.push_back("\x01END");
  m.out.resize(m.nodes.size());
  for (const auto& e : dfg) m.out[m.id[e.first.first]].push_back(m.id[e.first.second]);
  // Frequencies are informational; the evaluator treats a supplied key as a valid
  // start/end activity even when its count happens to be zero.
  for (const auto& e : sa) m.out[m.start].push_back(m.id[e.first]);
  for (const auto& e : ea) m.out[m.id[e.first]].push_back(m.end);
  if (names.empty()) m.out[m.start].push_back(m.end);
  for (auto& v : m.out) { std::sort(v.begin(),v.end(),[&](auto a,auto b){return m.nodes[a]<m.nodes[b];}); v.erase(std::unique(v.begin(),v.end()),v.end()); }
  if (!m.nodes.empty() && m.nodes.size()>2) {
    std::vector<char> f(m.nodes.size()), r(m.nodes.size());
    std::queue<std::size_t> q; q.push(m.start); f[m.start]=1;
    while(!q.empty()){auto u=q.front();q.pop();for(auto v:m.out[u])if(!f[v])f[v]=1,q.push(v);}
    std::vector<std::vector<std::size_t>> pred(m.nodes.size());
    for(std::size_t u=0;u<m.out.size();++u)for(auto v:m.out[u])pred[v].push_back(u);
    q.push(m.end); r[m.end]=1;
    while(!q.empty()){auto u=q.front();q.pop();for(auto v:pred[u])if(!r[v])r[v]=1,q.push(v);}
    for(std::size_t i=0;i<m.nodes.size()-2;++i) if(!f[i]||!r[i]) throw std::invalid_argument("DFG must be connected from starts to ends");
    if(!f[m.end]) throw std::invalid_argument("DFG has no path from start to end");
  }
  return m;
}

double get_cost(const std::map<std::string,double>& map, const std::string& a, double def) {
  auto it=map.find(a); return it==map.end()?def:it->second;
}

void set_reserved(std::vector<EventAttribute>& attrs, const std::string& key,
                  const std::string& value, AttributeKind kind) {
  bool set = false;
  attrs.erase(std::remove_if(attrs.begin(), attrs.end(), [&](const EventAttribute& a) {
    if (a.key != key) return false;
    if (!set) { set = true; return false; }
    return true;
  }), attrs.end());
  if (set) {
    for (auto& a : attrs) if (a.key == key) { a.value = value; a.kind = kind; a.is_null = false; break; }
  } else attrs.push_back({key, value, kind});
}

struct Key { std::size_t node, pos; bool last_mm; bool operator==(const Key& o) const {return node==o.node&&pos==o.pos&&last_mm==o.last_mm;} };
struct KeyHash { std::size_t operator()(const Key& k) const { return k.node*1315423911u+k.pos*2654435761u+k.last_mm; } };
struct State {
  double internal=0, reported=0; std::size_t node=0,pos=0,seq=0; bool last_mm=false; std::size_t prev=std::numeric_limits<std::size_t>::max(); DfgAlignmentMove move;
};

DfgAlignmentResult align_impl(const std::vector<std::string>& trace,
                               const Model& m, const DfgAlignmentOptions& o) {
  const auto t0=std::chrono::steady_clock::now();
  const auto no_prev = std::numeric_limits<std::size_t>::max();
  std::vector<State> states; states.push_back({0,0,m.start,0,0,false,no_prev,{}});
  struct Q { double cost; std::size_t seq, idx; bool operator<(const Q& x) const { return cost!=x.cost?cost>x.cost:seq>x.seq; } };
  std::priority_queue<Q> pq; pq.push({0,0,0});
  std::unordered_map<Key,double,KeyHash> best; best[{m.start,0,false}]=0;
  std::size_t visited=0, closed=0; std::size_t goal=std::numeric_limits<std::size_t>::max();
  while(!pq.empty()) {
    auto q=pq.top();pq.pop(); State cur=states[q.idx];
    auto bi=best.find({cur.node,cur.pos,cur.last_mm}); if(bi!=best.end() && cur.internal>bi->second+1e-12) continue;
    ++visited;
    if (o.max_visited_states && visited>o.max_visited_states) throw std::runtime_error("DFG alignment state limit exceeded");
    if (o.max_time_ms && static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-t0).count())>o.max_time_ms) throw std::runtime_error("DFG alignment time limit exceeded");
    if(cur.node==m.end && cur.pos==trace.size()){ goal=q.idx; break; }
    ++closed;
    auto push=[&](std::size_t node,std::size_t pos,bool mm,double in,double rep,const DfgAlignmentMove& mv){
      double ni=cur.internal+in; Key k{node,pos,mm}; auto it=best.find(k); if(it!=best.end()&&ni>=it->second-1e-12)return; best[k]=ni;
      states.push_back({ni,cur.reported+rep,node,pos,states.size(),mm,q.idx,mv}); pq.push({ni,states.back().seq,states.size()-1});
    };
    if(cur.node!=m.end) {
      const bool has_log=cur.pos<trace.size(); const std::string& la=has_log?trace[cur.pos]:m.nodes[m.end];
      bool sync_possible = false;
      if (has_log) for (auto v : m.out[cur.node]) if (v != m.end && m.nodes[v] == la) { sync_possible = true; break; }
      const bool unknown_log = has_log && !m.id.count(la);
      for(auto v:m.out[cur.node]) {
        if (unknown_log) continue;  // The evaluator schedules the LM before routing the model
        if(v==m.end) { push(v,cur.pos,false,0,0,{kGap,kGap}); continue; }
        const auto& a=m.nodes[v];
        if(has_log && la==a) push(v,cur.pos+1,false,get_cost(o.sync_cost,a,0),get_cost(o.sync_cost,a,0),{a,a});
        if(!sync_possible) push(v,cur.pos,true,get_cost(o.model_move_cost,a,10000),get_cost(o.model_move_cost,a,10000),{kGap,a});
      }
    }
    if(cur.pos<trace.size() && !cur.last_mm) {
      const auto& a=trace[cur.pos]; double rep=get_cost(o.log_move_cost,a,10000); double in;
      auto it=o.internal_log_move_cost.find(a); if(it!=o.internal_log_move_cost.end()) in=it->second; else in=(m.id.count(a)?rep:0.0);
      push(cur.node,cur.pos+1,false,in,rep,{a,kGap});
    }
  }
  if(goal==std::numeric_limits<std::size_t>::max()) throw std::runtime_error("DFG alignment search failed");
  DfgAlignmentResult r; r.visited_states=visited; r.closed_states=r.closed=closed;
  for(std::size_t i=goal; states[i].prev != no_prev; i=states[i].prev) r.alignment.push_back(states[i].move);
  std::reverse(r.alignment.begin(),r.alignment.end());
  r.alignment.erase(std::remove_if(r.alignment.begin(),r.alignment.end(),[](const DfgAlignmentMove& x){return x.log==kGap&&x.model==kGap;}),r.alignment.end());
  r.cost=states[goal].reported; r.internal_cost=states[goal].internal;
  return r;
}
}

DfgAlignmentResult align_trace_on_dfg(const std::vector<std::string>& trace,const DfgFrequency& dfg,const std::map<std::string,std::uint64_t>& sa,const std::map<std::string,std::uint64_t>& ea,const DfgAlignmentOptions& o){
  validate_costs(o); auto m=make_model(dfg,sa,ea); auto r=align_impl(trace,m,o);
  auto empty=align_impl({},m,o); double bwc=empty.cost; for(const auto& a:trace) bwc+=get_cost(o.log_move_cost,a,10000); r.best_worst_cost=bwc; r.fitness=bwc>0?1.0-r.cost/bwc:(r.cost==0?1.0:0.0); return r;
}
DfgAlignmentResult align_trace_on_dfg(std::initializer_list<std::string> trace,const DfgFrequency& d,const std::map<std::string,std::uint64_t>& s,const std::map<std::string,std::uint64_t>& e,const DfgAlignmentOptions& o){ return align_trace_on_dfg(std::vector<std::string>(trace),d,s,e,o); }
DfgAlignmentResult align_trace_on_dfg(const Trace& t,const DfgFrequency& d,const std::map<std::string,std::uint64_t>& s,const std::map<std::string,std::uint64_t>& e,const DfgAlignmentOptions& o){
  validate_costs(o);
  return align_trace_on_dfg(activity_word(t, o.activity_attribute), d, s, e, o);
}
DfgAlignmentLogResult align_log_on_dfg(const EventLog& log,const DfgFrequency& d,const std::map<std::string,std::uint64_t>& s,const std::map<std::string,std::uint64_t>& e,const DfgAlignmentOptions& o){
  validate_costs(o);
  std::map<std::vector<std::string>,DfgAlignmentResult> cache;
  DfgAlignmentLogResult out;
  out.alignments.reserve(log.traces.size());
  for(const auto& t:log.traces){
    auto a = activity_word(t, o.activity_attribute);
    auto it=cache.find(a);
    if(it==cache.end())it=cache.emplace(a,align_trace_on_dfg(a,d,s,e,o)).first;
    out.alignments.push_back(it->second);
  }
  out.unique_variants=cache.size();
  return out;
}

EventLog project_log_on_dfg(const EventLog& log,const DfgFrequency& d,const std::map<std::string,std::uint64_t>& s,const std::map<std::string,std::uint64_t>& e,const DfgAlignmentOptions& o){
  validate_costs(o);
  auto ars=align_log_on_dfg(log,d,s,e,o); EventLog out; copy_event_log_context(log, out);
  set_reserved(out.attributes, "@@aligned_dfg", "DFG alignment projection", AttributeKind::kString);
  for(std::size_t i=0;i<log.traces.size();++i){const auto& in=log.traces[i]; Trace t; copy_trace_context(in, t); const Event* activity_template=in.events.empty()?nullptr:&in.events.front(); std::size_t z=0; SelectedTimestamp timestamp; if(!in.events.empty()) timestamp=timestamp_value(in.events.front(), o.timestamp_attribute, true);
    std::string original; for(std::size_t j=0;j<in.events.size();++j){if(j)original+=",";original+=required_activity(in.events[j], o.activity_attribute);}
    set_reserved(t.attributes, "@@original_trace", original, AttributeKind::kString);
    for(const auto& mv:ars.alignments[i].alignment){if(mv.model==kGap){++z;continue;} bool sync=mv.log!=kGap; Event ev;
      if(sync){if(z>=in.events.size()) throw std::invalid_argument("DFG alignment projection consumes too many source events"); const auto& source_event=in.events[z++]; ev=source_event; bind_selected_activity(ev, source_event, o.activity_attribute, mv.model); timestamp=timestamp_value(source_event, o.timestamp_attribute, true); bind_selected_timestamp(ev, timestamp);}
      else {copy_event_case_context(in, ev); set_projected_activity(ev, activity_template, o.activity_attribute, mv.model); set_projected_timestamp(ev, timestamp);}
      ev.input_order=t.events.size();
      set_reserved(ev.attributes, "@@is_conforming", sync?"true":"false", AttributeKind::kBoolean);
      t.events.push_back(std::move(ev)); }
    out.traces.push_back(std::move(t)); }
  return out;
}

DfgAlignmentProjection project_alignments_on_dfg(const EventLog& log,const DfgFrequency& d,const std::map<std::string,std::uint64_t>& s,const std::map<std::string,std::uint64_t>& e,const DfgAlignmentOptions& o){
  auto ars=align_log_on_dfg(log,d,s,e,o); DfgAlignmentProjection p; for(const auto& x:d)p.dfg[x.first]={}; std::set<std::string> acts;for(const auto& x:d){acts.insert(x.first.first);acts.insert(x.first.second);}for(const auto& x:s)acts.insert(x.first);for(const auto& x:e)acts.insert(x.first);for(const auto& a:acts)p.activities[a]={};
  for(const auto& ar:ars.alignments){ std::vector<std::pair<std::string,bool>> model; for(const auto& mv:ar.alignment){if(mv.model==kGap){if(mv.log!=kGap&&p.activities.count(mv.log))p.activities[mv.log].lm++;} else {bool sy=mv.log!=kGap;if(p.activities.count(mv.model)){if(sy)p.activities[mv.model].sync++;else p.activities[mv.model].mm++;} model.push_back({mv.model,sy});}} for(std::size_t i=1;i<model.size();++i){auto ed=DfgActivityPair{model[i-1].first,model[i].first};auto it=p.dfg.find(ed);if(it!=p.dfg.end()){if(model[i-1].second&&model[i].second)it->second.sync++;else it->second.mm++;}} }
  return p;
}
}  // namespace minera
