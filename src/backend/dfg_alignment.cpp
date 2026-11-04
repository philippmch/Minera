#include "minera/dfg_alignment.hpp"
#include "minera/cancellation.hpp"
#include "root_cause_unicode.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <limits>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <stdexcept>
#include <string_view>
#include <tuple>
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

SelectedTimestamp timestamp_value(const Event& event, std::string_view selector) {
  const auto* value = dfg_event_field_value(event, selector);
  if (value == nullptr)
    throw std::invalid_argument("DFG alignment: missing or null timestamp " +
                                std::string(selector));
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
  result.scalar_wrapper = false;
  result.scalar_children_list = false;
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

// Preserve the selected activity value, including its native type.
void bind_selected_activity(Event& target, const Event& source,
                            std::string_view selector,
                            std::string_view value) {
  target.activity = value;
  target.activity_state = FirstClassAttributeState::kPresent;
  if (const auto* attribute = selected_attribute(source, selector)) {
    target.activity_source_key = attribute->key;
    target.activity_attribute = attribute->value == value ? *attribute :
        projected_activity_attribute(*attribute, attribute->key, value);
    replace_selected_attribute(target.attributes, *target.activity_attribute);
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
  for (auto& v : m.out) { std::sort(v.begin(),v.end(),[&](auto a,auto b){return m.nodes[a]<m.nodes[b];}); v.erase(std::unique(v.begin(),v.end()),v.end()); }
  return m;
}

double get_cost(const std::map<std::string,double>& map, const std::string& a,
                double def, bool supplied, const char* role) {
  const auto it = map.find(a);
  if (it != map.end()) return it->second;
  if (supplied || !map.empty())
    throw std::invalid_argument(std::string("DFG alignment: missing ") + role +
                                " cost for activity " + a);
  return def;
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

struct State {
  double internal=0, reported=0; std::size_t node=0,pos=0,seq=0; bool last_mm=false; std::size_t prev=std::numeric_limits<std::size_t>::max(); DfgAlignmentMove move;
  std::size_t visited_when_queued = 0;
};

std::string routing_uuid() {
  std::random_device entropy;
  std::uniform_int_distribution<std::uint32_t> bits;
  std::array<unsigned char, 16> bytes;
  for (std::size_t i = 0; i < bytes.size(); i += 4) {
    auto word = bits(entropy);
    for (std::size_t j = 0; j < 4; ++j) {
      bytes[i + j] = static_cast<unsigned char>(word & 0xff);
      word >>= 8;
    }
  }
  bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0f) | 0x40);
  bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3f) | 0x80);
  constexpr char hex[] = "0123456789abcdef";
  std::string result;
  result.reserve(36);
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    if (i == 4 || i == 6 || i == 8 || i == 10) result += '-';
    result += hex[bytes[i] >> 4];
    result += hex[bytes[i] & 15];
  }
  return result;
}

DfgAlignmentResult align_impl(const std::vector<std::string>& trace,
                               const Model& m, const DfgAlignmentOptions& o) {
  const auto t0=std::chrono::steady_clock::now();
  // Each raw search receives independent routing identities, including the
  // empty baseline and every uncached variant in a log. Their lexical order
  // is observable when all earlier queue priorities tie.
  const auto fresh_routing_name = [&] {
    std::string name;
    do { name = routing_uuid(); }
    while (m.id.count(name) || std::find(trace.begin(), trace.end(), name) != trace.end());
    return name;
  };
  std::string source_name, sink_name;
  if (o.routing_names) {
    std::tie(source_name, sink_name) = *o.routing_names;
    const auto taken = [&](const std::string& name) {
      return m.id.count(name) || std::find(trace.begin(), trace.end(), name) != trace.end();
    };
    if (source_name == sink_name || taken(source_name) || taken(sink_name))
      throw std::invalid_argument("DFG alignment routing names must be distinct and unused");
  } else {
    source_name = fresh_routing_name();
    do { sink_name = fresh_routing_name(); } while (sink_name == source_name);
  }
  const auto routing_name = [&](std::size_t node) -> std::string_view {
    if (node == m.start) return source_name;
    if (node == m.end) return sink_name;
    return m.nodes[node];
  };
  const auto no_prev = std::numeric_limits<std::size_t>::max();
  std::vector<State> states; states.push_back({0,0,m.start,0,0,false,no_prev,{}});
  struct Q {
    double cost, reported;
    std::size_t pos, queued_at, seq, idx;
    std::string_view node_name;
    bool operator<(const Q& x) const {
      if (cost != x.cost) return cost > x.cost;
      if (pos != x.pos) return pos < x.pos;
      if (queued_at != x.queued_at) return queued_at > x.queued_at;
      if (reported != x.reported) return reported > x.reported;
      if (node_name != x.node_name) return node_name > x.node_name;
      return seq > x.seq;
    }
  };
  std::priority_queue<Q> pq; pq.push({0,0,0,0,0,0,source_name});
  // At a fixed model node, an already expanded state farther through the
  // trace dominates earlier positions. Popped costs are nondecreasing, so
  // the largest closed position is sufficient to represent this frontier.
  std::vector<std::optional<std::size_t>> closed_position(m.nodes.size());
  std::size_t visited=0, closed=0; std::size_t goal=std::numeric_limits<std::size_t>::max();
  CancellationCheck cancelled(o.cancelled, "DFG alignment was cancelled");
  while(!pq.empty()) {
    cancelled();
    auto q=pq.top();pq.pop(); State cur=states[q.idx];
    ++visited;
    if (o.max_visited_states && visited>o.max_visited_states) throw std::runtime_error("DFG alignment state limit exceeded");
    if (o.max_time_ms && static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-t0).count())>o.max_time_ms) throw std::runtime_error("DFG alignment time limit exceeded");
    if(cur.node==m.end && cur.pos==trace.size()){ goal=q.idx; break; }
    if (closed_position[cur.node] && cur.pos <= *closed_position[cur.node]) continue;
    closed_position[cur.node] = cur.pos;
    ++closed;
    auto push=[&](std::size_t node,std::size_t pos,bool mm,double in,double rep,const DfgAlignmentMove& mv){
      if (closed_position[node] && pos <= *closed_position[node]) return;
      const double ni=cur.internal+in;
      // Pending duplicates remain in the queue: CLASSIC counts their pops,
      // even though only the first nondominated occurrence gets expanded.
      states.push_back({ni,cur.reported+rep,node,pos,states.size(),mm,q.idx,mv,visited});
      pq.push({ni,cur.reported+rep,pos,visited,states.back().seq,states.size()-1,routing_name(node)});
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
        if(has_log && la==a) {
          const double cost = get_cost(o.sync_cost,a,0,o.sync_cost_supplied,"synchronous");
          push(v,cur.pos+1,false,cost,cost,{a,a});
        }
        if(!sync_possible) {
          const double cost = get_cost(o.model_move_cost,a,10000,o.model_move_cost_supplied,"model move");
          push(v,cur.pos,true,cost,cost,{kGap,a});
        }
      }
    }
    if(cur.pos<trace.size() && !cur.last_mm) {
      const auto& a=trace[cur.pos];
      const double rep=get_cost(o.log_move_cost,a,10000,o.log_move_cost_supplied,"log move");
      const double in=get_cost(o.internal_log_move_cost,a,m.id.count(a)?rep:0.0,
                               o.internal_log_move_cost_supplied,"internal log move");
      push(cur.node,cur.pos+1,false,in,rep,{a,kGap});
    }
  }
  DfgAlignmentResult r;
  r.fitness = r.best_worst_cost = std::numeric_limits<double>::quiet_NaN();
  r.visited_states=visited;
  r.closed_states=r.closed=closed;
  if(goal==std::numeric_limits<std::size_t>::max()) {
    r.computed = false;
    r.fitness = std::numeric_limits<double>::quiet_NaN();
    return r;
  }
  // The public diagnostic is the counter captured when the winning goal was
  // queued, not the eventual counter after the rest of its cost ties were popped.
  r.visited_states = states[goal].visited_when_queued;
  std::vector<std::size_t> path;
  for (std::size_t i = goal; states[i].prev != no_prev; i = states[i].prev)
    path.push_back(i);
  std::reverse(path.begin(), path.end());
  std::string sink = "@@dfg_sink";
  while (m.id.count(sink) || std::find(trace.begin(), trace.end(), sink) != trace.end())
    sink += "_";
  for (const auto index : path) {
    const auto& state = states[index];
    const auto& parent = states[state.prev];
    // CLASSIC reconstruction excludes leading model routing and the final
    // accepting transition. If routing reached the generated sink early,
    // that transition is exposed with the preceding log label instead.
    if (state.pos == 0 || index == goal) continue;
    if (state.node == m.end && parent.node != m.end) {
      r.alignment.push_back({trace[state.pos - 1], sink});
    } else {
      r.alignment.push_back(state.move);
    }
  }
  r.cost=states[goal].reported; r.internal_cost=states[goal].internal;
  return r;
}
}

DfgAlignmentResult align_trace_on_dfg(const std::vector<std::string>& trace,const DfgFrequency& dfg,const std::map<std::string,std::uint64_t>& sa,const std::map<std::string,std::uint64_t>& ea,const DfgAlignmentOptions& o){
  validate_costs(o);
  return align_impl(trace,make_model(dfg,sa,ea),o);
}
DfgAlignmentResult align_trace_on_dfg(std::initializer_list<std::string> trace,const DfgFrequency& d,const std::map<std::string,std::uint64_t>& s,const std::map<std::string,std::uint64_t>& e,const DfgAlignmentOptions& o){ return align_trace_on_dfg(std::vector<std::string>(trace),d,s,e,o); }
DfgAlignmentResult align_trace_on_dfg(const Trace& t,const DfgFrequency& d,const std::map<std::string,std::uint64_t>& s,const std::map<std::string,std::uint64_t>& e,const DfgAlignmentOptions& o){
  validate_costs(o);
  return align_trace_on_dfg(activity_word(t, o.activity_attribute), d, s, e, o);
}
DfgAlignmentLogResult align_log_on_dfg(const EventLog& log,const DfgFrequency& d,const std::map<std::string,std::uint64_t>& s,const std::map<std::string,std::uint64_t>& e,const DfgAlignmentOptions& o){
  validate_costs(o);
  const auto model = make_model(d,s,e);
  const auto baseline = align_impl({},model,o);
  if (!baseline.computed)
    throw std::invalid_argument("DFG log alignment requires an accepting path for its fitness baseline");
  std::map<std::vector<std::string>,DfgAlignmentResult> cache;
  DfgAlignmentLogResult out;
  out.alignments.reserve(log.traces.size());
  for(const auto& t:log.traces){
    auto a = activity_word(t, o.activity_attribute);
    auto it=cache.find(a);
    if(it==cache.end()) {
      throw_if_cancelled(o.cancelled, "DFG alignment was cancelled");
      auto result = align_impl(a,model,o);
      if (result.computed) {
        double bwc = baseline.cost;
        for (const auto& activity : a)
          bwc += get_cost(o.log_move_cost,activity,10000,o.log_move_cost_supplied,"log move");
        if (bwc == 0)
          throw std::invalid_argument("DFG log alignment fitness denominator is zero");
        result.best_worst_cost = bwc;
        result.fitness = 1.0 - result.cost / bwc;
      }
      it=cache.emplace(a,std::move(result)).first;
    }
    out.alignments.push_back(it->second);
  }
  out.unique_variants=cache.size();
  return out;
}

namespace {
char32_t model_label_codepoint(const std::string& value, std::size_t& position) {
  const auto first = static_cast<unsigned char>(value[position++]);
  char32_t codepoint = first;
  int continuation = 0;
  if (first >= 0xc2 && first <= 0xdf) { codepoint = first & 0x1f; continuation = 1; }
  else if (first >= 0xe0 && first <= 0xef) { codepoint = first & 0x0f; continuation = 2; }
  else if (first >= 0xf0 && first <= 0xf4) { codepoint = first & 0x07; continuation = 3; }
  else if (first >= 0x80) throw std::invalid_argument("DFG projection: invalid UTF-8 model label");
  for (int i = 0; i < continuation; ++i) {
    if (position == value.size())
      throw std::invalid_argument("DFG projection: invalid UTF-8 model label");
    const auto next = static_cast<unsigned char>(value[position++]);
    if ((next & 0xc0) != 0x80)
      throw std::invalid_argument("DFG projection: invalid UTF-8 model label");
    codepoint = (codepoint << 6) | (next & 0x3f);
  }
  if ((continuation == 1 && codepoint < 0x80) ||
      (continuation == 2 && codepoint < 0x800) ||
      (continuation == 3 && codepoint < 0x10000) ||
      codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff))
    throw std::invalid_argument("DFG projection: invalid UTF-8 model label");
  return codepoint;
}

std::string python_string(const std::string& value) {
  const char quote = value.find('\'') != std::string::npos &&
      value.find('"') == std::string::npos ? '"' : '\'';
  std::string result(1, quote);
  constexpr char hex[] = "0123456789abcdef";
  for (std::size_t position = 0; position < value.size();) {
    const auto start = position;
    const auto c = model_label_codepoint(value, position);
    if (c == static_cast<unsigned char>(quote) || c == '\\') {
      result += '\\'; result += static_cast<char>(c);
    }
    else if (c == '\n') result += "\\n";
    else if (c == '\r') result += "\\r";
    else if (c == '\t') result += "\\t";
    else if (!root_cause_unicode_printable(c)) {
      // The shared Unicode 14 table matches the pinned Python 3.11 repr.
      const int digits = c <= 0xff ? 2 : c <= 0xffff ? 4 : 8;
      result += digits == 2 ? "\\x" : digits == 4 ? "\\u" : "\\U";
      for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4)
        result += hex[(c >> shift) & 15];
    } else result.append(value, start, position - start);
  }
  result += quote;
  return result;
}
std::string model_description(const DfgFrequency& d,
    const std::map<std::string, std::uint64_t>& starts,
    const std::map<std::string, std::uint64_t>& ends) {
  std::string result = "[{";
  bool first = true;
  for (const auto& [edge, count] : d) {
    if (!first) result += ", ";
    first = false;
    result += "(" + python_string(edge.first) + ", " + python_string(edge.second) +
              "): " + std::to_string(count);
  }
  result += "}";
  for (const auto* boundary : {&starts, &ends}) {
    result += ", {";
    first = true;
    for (const auto& [name, count] : *boundary) {
      if (!first) result += ", ";
      first = false;
      result += python_string(name) + ": " + std::to_string(count);
    }
    result += "}";
  }
  return result + "]";
}
void require_projection_alignment(const DfgAlignmentResult& result) {
  if (!result.computed)
    throw std::invalid_argument("DFG projection requires an accepting alignment");
}

void validate_projection_results(const EventLog& log,
                                 const DfgAlignmentLogResult& result) {
  if (result.alignments.size() != log.traces.size())
    throw std::invalid_argument(
        "DFG projection requires one alignment per trace");
  for (const auto& alignment : result.alignments)
    require_projection_alignment(alignment);
}
}

EventLog project_log_on_dfg(const EventLog& log, const DfgFrequency& d,
    const std::map<std::string,std::uint64_t>& starts,
    const std::map<std::string,std::uint64_t>& ends, const DfgAlignmentOptions& o) {
  validate_costs(o);
  const auto results = align_log_on_dfg(log, d, starts, ends, o);
  return project_log_on_dfg(log, d, starts, ends, results, o);
}

EventLog project_log_on_dfg(const EventLog& log, const DfgFrequency& d,
    const std::map<std::string,std::uint64_t>& starts,
    const std::map<std::string,std::uint64_t>& ends,
    const DfgAlignmentLogResult& results, const DfgAlignmentOptions& o) {
  validate_costs(o);
  validate_projection_results(log, results);
  EventLog out;
  set_reserved(out.attributes, "@@aligned_dfg", model_description(d, starts, ends),
               AttributeKind::kString);
  for (std::size_t i = 0; i < log.traces.size(); ++i) {
    const auto& source = log.traces[i];
    Trace target;
    std::string original;
    for (std::size_t j = 0; j < source.events.size(); ++j) {
      if (j) original += ",";
      original += required_activity(source.events[j], o.activity_attribute);
    }
    set_reserved(target.attributes, "@@original_trace", original, AttributeKind::kString);
    SelectedTimestamp timestamp;
    if (!source.events.empty())
      timestamp = timestamp_value(source.events.front(), o.timestamp_attribute);
    std::size_t sync_index = 0;
    for (const auto& move : results.alignments[i].alignment) {
      if (move.model == kGap) continue;
      const bool sync = move.log == move.model;
      if (!sync && move.log != kGap) continue;  // Generated sink transitions.
      Event event;
      if (sync) {
        if (sync_index >= source.events.size())
          throw std::invalid_argument("DFG projection consumes too many source events");
        const auto& selected = source.events[sync_index++];
        // CLASSIC advances this index only for synchronous moves, including
        // when a preceding log move was dropped from the projection.
        bind_selected_activity(event, selected, o.activity_attribute, move.model);
        timestamp = timestamp_value(selected, o.timestamp_attribute);
      } else {
        set_projected_activity(event, source.events.empty() ? nullptr : &source.events.front(),
                               o.activity_attribute, move.model);
      }
      set_projected_timestamp(event, timestamp);
      event.input_order = target.events.size();
      set_reserved(event.attributes, "@@is_conforming", sync ? "true" : "false",
                   AttributeKind::kBoolean);
      target.events.push_back(std::move(event));
    }
    out.traces.push_back(std::move(target));
  }
  return out;
}

DfgAlignmentProjection project_alignments_on_dfg(const EventLog& log,
    const DfgFrequency& d, const std::map<std::string,std::uint64_t>& starts,
    const std::map<std::string,std::uint64_t>& ends, const DfgAlignmentOptions& o) {
  const auto results = align_log_on_dfg(log, d, starts, ends, o);
  return project_alignments_on_dfg(log, d, starts, ends, results, o);
}

DfgAlignmentProjection project_alignments_on_dfg(
    const EventLog& log, const DfgFrequency& d,
    const std::map<std::string,std::uint64_t>& starts,
    const std::map<std::string,std::uint64_t>& ends,
    const DfgAlignmentLogResult& results, const DfgAlignmentOptions& o) {
  validate_costs(o);
  validate_projection_results(log, results);
  DfgAlignmentProjection result;
  for (const auto& [edge, count] : d) {
    (void)count;
    result.dfg[edge] = {};
    result.activities[edge.first] = {};
    result.activities[edge.second] = {};
  }
  for (const auto& alignment : results.alignments) {
    // The public projection visits adjacent pairs, so the final move itself
    // never contributes an activity count, and log moves interrupt edges.
    for (std::size_t i = 0; i + 1 < alignment.alignment.size(); ++i) {
      const auto& move = alignment.alignment[i];
      const auto& next = alignment.alignment[i + 1];
      // Activity metrics classify any two nongap labels as synchronous and
      // use its log label, including CLASSIC's exposed generated sink move.
      // Edge lookup below still uses model labels, so a sink adds no DFG edge.
      const bool sync = move.log != kGap && move.model != kGap;
      const auto activity = move.log != kGap ? move.log : move.model;
      if (auto found = result.activities.find(activity); found != result.activities.end()) {
        if (sync) ++found->second.sync;
        else if (move.model == kGap) ++found->second.lm;
        else ++found->second.mm;
      }
      if (move.model != kGap && next.model != kGap) {
        if (auto found = result.dfg.find({move.model, next.model}); found != result.dfg.end()) {
          if (sync && next.log == next.model) ++found->second.sync;
          else ++found->second.mm;
        }
      }
    }
  }
  return result;
}
}  // namespace minera
