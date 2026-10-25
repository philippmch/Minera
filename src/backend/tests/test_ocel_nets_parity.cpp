// Object-centric Petri nets and SAW nets against the public reference
// (docs/ocel-nets-bank.json.gz): seeded OCELs through the public OCPN
// dispatcher (defaults, the IMd inductive variant, a lower double-arc
// threshold, token-based-replay diagnostics, custom activity/object-type
// selectors and the WO_ANNOTATION selector), the internal wo_annotation
// module and the public SAW-net dispatcher (defaults and custom
// selectors). The OCDFG annotations and double-arc map compare exactly; each
// per-type Petri net, SAW net and the merged SAW net compares up to
// isomorphism, with replay diagnostics on places and transitions and every
// arc's weight distribution bound. A native result must match one outcome
// the reference showed under four hash seeds; raising calls must raise.
#include "minera/ocel.hpp"
#include "minera/saw_nets.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using namespace minera;

// Minimal JSON reader for the generated fixture.
struct Json {
  enum Kind { kNull, kBool, kNumber, kString, kArray, kObject } kind = kNull;
  double number = 0;
  bool boolean = false;
  std::string text;
  std::vector<Json> items;
  std::map<std::string, Json> fields;
  [[nodiscard]] const Json* get(const std::string& key) const {
    const auto found = fields.find(key);
    return found == fields.end() ? nullptr : &found->second;
  }
};

class JsonReader {
 public:
  explicit JsonReader(const char* text) : at_(text) {}
  Json value() {
    space();
    Json out;
    if (*at_ == '{') {
      out.kind = Json::kObject;
      ++at_;
      for (space(); *at_ != '}'; space()) {
        auto key = value().text;
        space(); ++at_;  // ':'
        out.fields.emplace(std::move(key), value());
        space();
        if (*at_ == ',') ++at_;
      }
      ++at_;
    } else if (*at_ == '[') {
      out.kind = Json::kArray;
      ++at_;
      for (space(); *at_ != ']'; space()) {
        out.items.push_back(value());
        space();
        if (*at_ == ',') ++at_;
      }
      ++at_;
    } else if (*at_ == '"') {
      out.kind = Json::kString;
      for (++at_; *at_ != '"'; ++at_) {
        if (*at_ == '\\') {
          ++at_;
          out.text += *at_ == 'n' ? '\n' : *at_ == 't' ? '\t' : *at_;
          continue;
        }
        out.text += *at_;
      }
      ++at_;
    } else if (*at_ == 't' || *at_ == 'f') {
      out.kind = Json::kBool;
      out.boolean = *at_ == 't';
      at_ += out.boolean ? 4 : 5;
    } else if (*at_ == 'n') {
      at_ += 4;
    } else {
      out.kind = Json::kNumber;
      char* end = nullptr;
      out.number = std::strtod(at_, &end);
      at_ = end;
    }
    return out;
  }

 private:
  void space() { while (*at_ == ' ' || *at_ == '\n') ++at_; }
  const char* at_;
};

// A JSON value rendered canonically (key-sorted objects), so native output
// built in the same shape compares as text.
std::string render(const Json& value) {
  switch (value.kind) {
    case Json::kNull: return "null";
    case Json::kBool: return value.boolean ? "true" : "false";
    case Json::kNumber: {
      char buffer[64];
      std::snprintf(buffer, sizeof buffer, "%.12g", value.number);
      return buffer;
    }
    case Json::kString: return '"' + value.text + '"';
    case Json::kArray: {
      std::string out = "[";
      for (std::size_t i = 0; i < value.items.size(); ++i) out += (i ? "," : "") + render(value.items[i]);
      return out + "]";
    }
    case Json::kObject: {
      std::string out = "{";
      bool first = true;
      for (const auto& [key, field] : value.fields) {
        out += (first ? "\"" : ",\"") + key + "\":" + render(field);
        first = false;
      }
      return out + "}";
    }
  }
  return {};
}

Json text(std::string value) {
  Json out;
  out.kind = Json::kString;
  out.text = std::move(value);
  return out;
}
Json number(double value) {
  Json out;
  out.kind = Json::kNumber;
  out.number = value;
  return out;
}
Json boolean(bool value) {
  Json out;
  out.kind = Json::kBool;
  out.boolean = value;
  return out;
}
Json array(std::vector<Json> items = {}) {
  Json out;
  out.kind = Json::kArray;
  out.items = std::move(items);
  return out;
}
Json strings(const std::vector<std::string>& values) {
  auto out = array();
  for (const auto& value : values) out.items.push_back(text(value));
  return out;
}
// Python's sort of the bank's lists: by rendered form is not Python order, so
// both sides are sorted the same way before comparison instead.
void canonical_sort(Json& list) {
  std::sort(list.items.begin(), list.items.end(),
            [](const Json& a, const Json& b) { return render(a) < render(b); });
}

struct BankCase {
  std::string name, call;
  Json real;
  std::vector<std::string> errors;  // error class names some seed raised
  Json common;                       // fields shared by every value outcome
  std::vector<Json> variants;        // the remaining fields of each outcome
};

const std::vector<BankCase>& bank() {
  static const std::vector<BankCase> cases = [] {
    static const char* const documents[] = {
#include "ocel_nets_cases.inc"
    };
    std::vector<BankCase> out;
    for (const char* document : documents) {
      const auto json = JsonReader(document).value();
      BankCase item;
      item.name = json.get("name")->text;
      item.call = json.get("call")->text;
      item.real = *json.get("real");
      const auto& expected = *json.get("expected");
      for (const auto& error : expected.get("errors")->items) item.errors.push_back(error.text);
      item.common = *expected.get("common");
      item.variants = expected.get("variants")->items;
      out.push_back(std::move(item));
    }
    return out;
  }();
  return cases;
}

std::string upper(std::string value) {
  for (auto& ch : value) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  return value;
}

// Labelled logs add the bank's custom selector attributes: `label` (the
// activity upper-cased, with B merged into A) and `kind` (the type upper-cased).
Ocel ocel(const Json& spec, bool labelled = false) {
  Ocel log;
  std::map<std::string, std::string> types;
  for (const auto& object : spec.get("objects")->items) {
    log.objects.push_back({object.items[0].text, object.items[1].text, {}});
    if (labelled) log.objects.back().attributes.push_back({"kind", upper(object.items[1].text)});
    types[object.items[0].text] = object.items[1].text;
  }
  for (const auto& event : spec.get("events")->items) {
    const int minutes = static_cast<int>(event.items[2].number);
    char stamp[64];
    std::snprintf(stamp, sizeof stamp, "2024-01-01T%02d:%02d:00", minutes / 60, minutes % 60);
    log.events.push_back({event.items[0].text, event.items[1].text, stamp, {}});
    if (labelled) {
      auto label = upper(event.items[1].text);
      log.events.back().attributes.push_back({"label", label == "B" ? std::string("A") : label});
    }
    for (const auto& object : event.items[3].items)
      log.relations.push_back({event.items[0].text, object.text, ""});
  }
  return log;
}

// Tagged containers in the bank's shape.
Json tagged(const char* tag, std::vector<Json> items) {
  Json out;
  out.kind = Json::kObject;
  out.fields[tag] = array(std::move(items));
  return out;
}
Json set(std::vector<Json> items) { return tagged("set", std::move(items)); }
Json dict(std::vector<std::pair<Json, Json>> entries) {
  std::vector<Json> items;
  for (auto& [key, value] : entries) items.push_back(array({std::move(key), std::move(value)}));
  return tagged("dict", std::move(items));
}
Json string_set(const std::vector<std::string>& values) {
  std::vector<Json> items;
  for (const auto& value : values) items.push_back(text(value));
  return set(std::move(items));
}
Json pair_json(const std::pair<std::string, std::string>& value) {
  return array({text(value.first), text(value.second)});
}
Json triple_json(const std::tuple<std::string, std::string, std::string>& value) {
  return array({text(std::get<0>(value)), text(std::get<1>(value)), text(std::get<2>(value))});
}
Json count(double value) { return number(value); }

// Tagged containers compare as sets: their items are put in one order.
void normalize(Json& value) {
  for (auto& item : value.items) normalize(item);
  for (auto& [key, field] : value.fields) {
    normalize(field);
    if (key == "set" || key == "dict") canonical_sort(field);
  }
}

Json activity_metrics(const std::map<std::string, OcdfgActivityMetric>& metrics) {
  std::vector<std::pair<Json, Json>> events, unique, total;
  for (const auto& [activity, metric] : metrics) {
    events.emplace_back(text(activity), string_set(metric.events));
    unique.emplace_back(text(activity), string_set(metric.unique_objects));
    auto pairs = array();
    for (const auto& item : metric.total_objects) pairs.items.push_back(pair_json(item));
    total.emplace_back(text(activity), pairs);
  }
  return array({dict(std::move(events)), dict(std::move(unique)), dict(std::move(total))});
}

// The by-type activity maps: {events|unique_objects|total_objects: {type: {activity: ...}}}.
Json typed_activity_metrics(const std::map<std::string, std::map<std::string, OcdfgActivityMetric>>& metrics) {
  std::vector<std::pair<Json, Json>> events, unique, total;
  for (const auto& [type, by_activity] : metrics) {
    const auto parts = activity_metrics(by_activity);
    events.emplace_back(text(type), parts.items[0]);
    unique.emplace_back(text(type), parts.items[1]);
    total.emplace_back(text(type), parts.items[2]);
  }
  return dict({{text("events"), dict(std::move(events))},
               {text("total_objects"), dict(std::move(total))},
               {text("unique_objects"), dict(std::move(unique))}});
}

Json ocdfg_json_value(const OcdfgGraph& graph) {
  const auto independent = activity_metrics(graph.activities_indep);
  std::vector<std::pair<Json, Json>> couples, unique, total, couple_performance, total_performance;
  for (const auto& [type, by_edge] : graph.edges) {
    std::vector<std::pair<Json, Json>> c, u, t, cp, tp;
    for (const auto& [edge, metric] : by_edge) {
      std::vector<Json> couple_items, triple_items;
      for (const auto& item : metric.event_couples) couple_items.push_back(pair_json(item));
      for (const auto& item : metric.total_objects) triple_items.push_back(triple_json(item));
      c.emplace_back(pair_json(edge), set(std::move(couple_items)));
      u.emplace_back(pair_json(edge), string_set(metric.unique_objects));
      t.emplace_back(pair_json(edge), set(std::move(triple_items)));
      auto samples = [](const std::vector<double>& values) {
        auto out = array();
        for (const auto value : values) out.items.push_back(number(value));
        return out;
      };
      cp.emplace_back(pair_json(edge), samples(metric.performance_event_couples));
      tp.emplace_back(pair_json(edge), samples(metric.performance_total_objects));
    }
    couples.emplace_back(text(type), dict(std::move(c)));
    unique.emplace_back(text(type), dict(std::move(u)));
    total.emplace_back(text(type), dict(std::move(t)));
    couple_performance.emplace_back(text(type), dict(std::move(cp)));
    total_performance.emplace_back(text(type), dict(std::move(tp)));
  }
  std::vector<std::pair<Json, Json>> fields{
      {text("activities"), string_set(graph.activities)},
      {text("activities_indep"), dict({{text("events"), independent.items[0]},
                                       {text("total_objects"), independent.items[2]},
                                       {text("unique_objects"), independent.items[1]}})},
      {text("activities_ot"), typed_activity_metrics(graph.activities_ot)},
      {text("edges"), dict({{text("event_couples"), dict(std::move(couples))},
                            {text("total_objects"), dict(std::move(total))},
                            {text("unique_objects"), dict(std::move(unique))}})},
      {text("end_activities"), typed_activity_metrics(graph.end_activities)},
      {text("object_types"), string_set(graph.object_types)},
      {text("start_activities"), typed_activity_metrics(graph.start_activities)}};
  // Without edge performance the public field holds two empty maps.
  if (!graph.performance_computed) couple_performance.clear(), total_performance.clear();
  fields.emplace_back(text("edges_performance"),
                      dict({{text("event_couples"), dict(std::move(couple_performance))},
                            {text("total_objects"), dict(std::move(total_performance))}}));
  return dict(std::move(fields));
}


// A Petri net as a coloured graph: nodes 0..places-1 are places.
struct Graph {
  std::vector<std::string> colour;  // kind, label, marking, replay values
  std::map<std::pair<std::size_t, std::size_t>, std::string> arcs;  // weights as text
  std::size_t places = 0;
};

// The reference net in the bank's shape; `replay` adds the per-place
// [m, r, c, p] and per-transition firing counts to the node colours.
Graph reference_graph(const Json& value, const Json* replay = nullptr) {
  Graph graph;
  std::map<std::string, std::size_t> place, transition;
  std::map<std::string, std::string> initial, final, place_replay, transition_replay;
  if (const auto* entries = value.get("initial"))
    for (const auto& entry : entries->items) initial[entry.items[0].text] = render(entry.items[1]);
  if (const auto* entries = value.get("final"))
    for (const auto& entry : entries->items) final[entry.items[0].text] = render(entry.items[1]);
  if (replay) {
    for (const auto& entry : replay->get("places")->items) {
      auto values = entry;
      values.items.erase(values.items.begin());
      place_replay[entry.items[0].text] = render(values);
    }
    for (const auto& entry : replay->get("transitions")->items)
      transition_replay[entry.items[0].text] = render(entry.items[1]);
  }
  for (const auto& name : value.get("places")->items) {
    place[name.text] = graph.colour.size();
    graph.colour.push_back("p" + (initial.contains(name.text) ? initial[name.text] : "0") + "/" +
                           (final.contains(name.text) ? final[name.text] : "0") +
                           (replay ? "/" + (place_replay.contains(name.text) ? place_replay[name.text] : "[0,0,0,0]")
                                   : ""));
  }
  graph.places = graph.colour.size();
  for (const auto& entry : value.get("transitions")->items) {
    transition[entry.items[0].text] = graph.colour.size();
    graph.colour.push_back((entry.items[1].kind == Json::kNull ? "t" : "t:" + entry.items[1].text) +
                           (replay ? "#" + (transition_replay.contains(entry.items[0].text)
                                                ? transition_replay[entry.items[0].text]
                                                : "0")
                                   : ""));
  }
  for (const auto& arc : value.get("arcs")->items) {
    const auto& source = arc.items[0].text;
    const auto& target = arc.items[1].text;
    const bool from_place = place.contains(source) && transition.contains(target);
    const auto u = from_place ? place.at(source) : transition.at(source);
    const auto v = from_place ? transition.at(target) : place.at(target);
    graph.arcs[{u, v}] = render(arc.items[2]);
  }
  return graph;
}

Json distribution(const SawWeightDistribution& weights) {
  auto out = array();
  for (const auto& [multiplicity, frequency] : weights)
    out.items.push_back(array({number(static_cast<double>(multiplicity)), number(static_cast<double>(frequency))}));
  return out;
}

// A native net; arc weights come from `weight(transition, input, position)`.
Graph native_graph(const PetriNet& net, bool markings, const OcpnTokenReplayDiagnostics* replay,
                   const std::function<std::string(std::size_t, bool, std::size_t)>& weight) {
  Graph graph;
  for (std::size_t place = 0; place < net.place_count(); ++place) {
    std::string colour = "p";
    if (markings) {
      colour += net.has_source() && net.source() == place ? "1/" : "0/";
      colour += net.has_sink() && net.sink() == place ? "1" : "0";
    } else {
      colour += "0/0";
    }
    if (replay) {
      const auto& values = place < replay->places.size() ? replay->places[place] : OcpnTokenReplayPlaceDiagnostics{};
      colour += "/" + render(array({number(static_cast<double>(values.missing)),
                                    number(static_cast<double>(values.remaining)),
                                    number(static_cast<double>(values.consumed)),
                                    number(static_cast<double>(values.produced))}));
    }
    graph.colour.push_back(colour);
  }
  graph.places = graph.colour.size();
  const auto& transitions = net.transitions();
  for (std::size_t index = 0; index < transitions.size(); ++index) {
    const auto& transition = transitions[index];
    graph.colour.push_back((transition.activity ? "t:" + net.activities()[*transition.activity] : "t") +
                           (replay ? "#" + render(number(static_cast<double>(
                                               index < replay->transition_activations.size()
                                                   ? replay->transition_activations[index]
                                                   : 0)))
                                   : ""));
    const auto node = graph.places + index;
    for (std::size_t k = 0; k < transition.inputs.size(); ++k)
      graph.arcs[{transition.inputs[k], node}] = weight(index, true, k);
    for (std::size_t k = 0; k < transition.outputs.size(); ++k)
      graph.arcs[{node, transition.outputs[k]}] = weight(index, false, k);
  }
  return graph;
}

// Colour refinement followed by a backtracking search for a bijection that
// preserves colours, arcs and weights.
bool isomorphic(const Graph& a, const Graph& b) {
  if (a.colour.size() != b.colour.size() || a.arcs.size() != b.arcs.size() || a.places != b.places) return false;
  const auto refine = [](const Graph& graph, std::size_t rounds) {
    auto colour = graph.colour;
    for (std::size_t round = 0; round < rounds; ++round) {
      std::vector<std::multiset<std::string>> signature(colour.size());
      for (const auto& [arc, weight] : graph.arcs) {
        signature[arc.first].insert(">" + weight + colour[arc.second]);
        signature[arc.second].insert("<" + weight + colour[arc.first]);
      }
      std::vector<std::string> next(colour.size());
      for (std::size_t node = 0; node < colour.size(); ++node) {
        next[node] = colour[node] + "{";
        for (const auto& item : signature[node]) next[node] += item + ";";
        next[node] += "}";
      }
      // Rename colours compactly so the strings stay short.
      std::map<std::string, std::string> names;
      for (const auto& item : next) names.emplace(item, "");
      std::size_t counter = 0;
      for (auto& [_, name] : names) name = "c" + std::to_string(counter++);
      for (std::size_t node = 0; node < colour.size(); ++node) colour[node] = names.at(next[node]);
    }
    return colour;
  };
  // Shared renaming needs both graphs refined together: refine their union.
  Graph joint;
  joint.colour = a.colour;
  joint.colour.insert(joint.colour.end(), b.colour.begin(), b.colour.end());
  joint.arcs = a.arcs;
  const auto offset = a.colour.size();
  for (const auto& [arc, weight] : b.arcs) joint.arcs[{arc.first + offset, arc.second + offset}] = weight;
  const auto colour = refine(joint, std::min<std::size_t>(a.colour.size(), 40));
  std::vector<std::string> ca(colour.begin(), colour.begin() + offset), cb(colour.begin() + offset, colour.end());
  auto sa = ca, sb = cb;
  std::sort(sa.begin(), sa.end());
  std::sort(sb.begin(), sb.end());
  if (sa != sb) return false;
  const auto n = ca.size();
  std::vector<std::size_t> order(n);
  for (std::size_t i = 0; i < n; ++i) order[i] = i;
  std::map<std::string, std::size_t> size;
  for (const auto& item : ca) ++size[item];
  std::stable_sort(order.begin(), order.end(), [&](std::size_t x, std::size_t y) { return size[ca[x]] < size[ca[y]]; });
  std::vector<std::size_t> map(n, n);
  std::vector<bool> used(n, false);
  const auto arc = [](const Graph& graph, std::size_t u, std::size_t v) -> const std::string* {
    const auto found = graph.arcs.find({u, v});
    return found == graph.arcs.end() ? nullptr : &found->second;
  };
  std::function<bool(std::size_t)> search = [&](std::size_t depth) {
    if (depth == n) return true;
    const auto x = order[depth];
    for (std::size_t y = 0; y < n; ++y) {
      if (used[y] || cb[y] != ca[x]) continue;
      bool consistent = true;
      for (std::size_t d = 0; d < depth && consistent; ++d) {
        const auto u = order[d];
        const auto* forward = arc(a, x, u);
        const auto* image = arc(b, y, map[u]);
        const auto* backward = arc(a, u, x);
        const auto* back_image = arc(b, map[u], y);
        consistent = ((forward == nullptr) == (image == nullptr)) && (!forward || *forward == *image) &&
                     ((backward == nullptr) == (back_image == nullptr)) && (!backward || *backward == *back_image);
      }
      if (!consistent) continue;
      map[x] = y;
      used[y] = true;
      if (search(depth + 1)) return true;
      used[y] = false;
    }
    return false;
  };
  return search(0);
}

// The outcome of one native call: plain fields compared as text and named
// graphs compared up to isomorphism.
struct Outcome {
  std::map<std::string, std::string> fields;
  std::map<std::string, Graph> graphs;
};

std::string field_text(Json value) {
  normalize(value);
  return render(value);
}

std::string config_of(const BankCase& item) { return item.call.substr(item.call.find('/') + 1); }

Outcome ocpn_outcome(const OcpnGraph& graph) {
  Outcome out;
  const auto annotations = ocdfg_json_value(graph.annotations);
  for (const auto& entry : annotations.fields.at("dict").items)
    out.fields[entry.items[0].text] = field_text(entry.items[1]);
  std::vector<std::pair<Json, Json>> double_arcs;
  for (const auto& [type, by_activity] : graph.double_arcs_on_activity) {
    std::vector<std::pair<Json, Json>> entries;
    for (const auto& [activity, flag] : by_activity) entries.emplace_back(text(activity), boolean(flag));
    double_arcs.emplace_back(text(type), dict(std::move(entries)));
  }
  out.fields["double_arcs_on_activity"] = field_text(dict(std::move(double_arcs)));
  for (const auto& [type, net] : graph.petri_nets) {
    const auto replay = graph.tbr_results.find(type);
    out.graphs["petri_nets/" + type] =
        native_graph(net, true, replay == graph.tbr_results.end() ? nullptr : &replay->second,
                     [](std::size_t, bool, std::size_t) { return std::string("1"); });
  }
  return out;
}

// The dispatcher, under both selectors (WO_ANNOTATION reaches the classic
// variant as well).
Outcome ocpn_case(const BankCase& item) {
  const auto config = config_of(item);
  OcpnDiscoveryOptions options;
  if (config == "imd") options.variant = OcpnInductiveVariant::kDfg;
  if (config == "double_arc_low") options.double_arc_threshold = 0.5;
  if (config == "tbr") options.diagnostics_with_token_based_replay = true;
  if (config == "selectors") options.activity_attribute = "label", options.object_type_attribute = "kind";
  return ocpn_outcome(discover_ocpn(ocel(item.real, config == "selectors"), options));
}

// The internal wo_annotation module, called directly, returns the classic
// result (annotations included): the default options.
Outcome wo_annotation_case(const BankCase& item) {
  const OcpnDiscoveryOptions defaults;
  return ocpn_outcome(discover_ocpn(ocel(item.real), defaults));
}

Outcome saw_nets_case(const BankCase& item) {
  SawNetDiscoveryOptions options;
  const bool selectors = config_of(item) == "selectors";
  if (selectors) options.activity_attribute = "label", options.object_type_attribute = "kind";
  const auto result = discover_saw_nets(ocel(item.real, selectors), options);
  Outcome out;
  for (const auto& [type, saw] : result.ot_saw_nets) {
    out.graphs["ot_saw_nets/" + type] =
        native_graph(saw.net, false, nullptr, [&](std::size_t transition, bool, std::size_t) {
          const auto found = saw.transition_weights.find(transition);
          return render(distribution(found == saw.transition_weights.end() ? SawWeightDistribution{} : found->second));
        });
  }
  const auto& multi = result.multi_saw_net;
  out.graphs["multi_saw_net"] =
      native_graph(multi.net, false, nullptr, [&](std::size_t transition, bool input, std::size_t position) {
        const auto found = multi.arc_weights.find(std::to_string(transition) + (input ? ":input:" : ":output:") +
                                                  std::to_string(position));
        return render(distribution(found == multi.arc_weights.end() ? SawWeightDistribution{} : found->second));
      });
  return out;
}

// The reference outcome's fields and graphs, from the shared and the
// outcome-specific fields.
Outcome reference_outcome(const BankCase& item, const Json& variant) {
  Json value = item.common;
  for (const auto& [key, field] : variant.fields) value.fields[key] = field;
  Outcome out;
  const Json* replay = value.get("tbr_results");
  for (const auto& [key, field] : value.fields) {
    if (key == "petri_nets") {
      for (const auto& [type, net] : field.fields) {
        const Json* diagnostics = replay ? replay->get(type) : nullptr;
        out.graphs["petri_nets/" + type] = reference_graph(net, diagnostics);
      }
    } else if (key == "ot_saw_nets") {
      for (const auto& [type, net] : field.fields) out.graphs["ot_saw_nets/" + type] = reference_graph(net);
    } else if (key == "multi_saw_net") {
      out.graphs["multi_saw_net"] = reference_graph(field);
    } else if (key != "tbr_results") {
      out.fields[key] = field_text(field);
    }
  }
  return out;
}

std::string difference(const BankCase& item, const std::function<Outcome()>& produce) {
  Outcome actual;
  try {
    actual = produce();
  } catch (const std::exception& error) {
    if (!item.errors.empty()) return {};
    return std::string("native raised: ") + error.what();
  }
  if (item.variants.empty()) return "native returned, reference raised " + item.errors.front();
  // The reported outcome is the one whose graphs matched furthest.
  std::string best;
  std::size_t best_matched = 0;
  for (const auto& variant : item.variants) {
    std::size_t matched = 0;
    const auto expected = reference_outcome(item, variant);
    std::string detail;
    for (const auto& [key, value] : expected.fields) {
      const auto found = actual.fields.find(key);
      if (found == actual.fields.end()) detail = key + " absent";
      else if (found->second != value)
        detail = key + ": native " + found->second.substr(0, 600) + " reference " + value.substr(0, 600);
      if (!detail.empty()) break;
    }
    if (detail.empty()) {
      std::set<std::string> names;
      for (const auto& [key, _] : expected.graphs) names.insert(key);
      for (const auto& [key, _] : actual.graphs) names.insert(key);
      // The merged SAW net is built from the per-type nets: compare it last.
      std::vector<std::string> keys(names.begin(), names.end());
      std::stable_partition(keys.begin(), keys.end(), [](const std::string& key) { return key != "multi_saw_net"; });
      for (const auto& key : keys) {
        const auto a = actual.graphs.find(key);
        const auto e = expected.graphs.find(key);
        if (a == actual.graphs.end() || e == expected.graphs.end()) {
          detail = key + (a == actual.graphs.end() ? " absent natively" : " absent in the reference");
          break;
        }
        if (!isomorphic(a->second, e->second)) {
          if (std::getenv("MINERA_DEBUG_GRAPHS")) {
            for (const Graph* graph : {static_cast<const Graph*>(&a->second), &e->second}) {
              std::fprintf(stderr, "  graph:");
              for (std::size_t node = 0; node < graph->colour.size(); ++node)
                std::fprintf(stderr, " %zu=%s", node, graph->colour[node].c_str());
              for (const auto& [arc, weight] : graph->arcs) std::fprintf(stderr, " %zu->%zu(%s)", arc.first, arc.second, weight.c_str());
              std::fprintf(stderr, "\n");
            }
          }
          detail = key + " not isomorphic (native " + std::to_string(a->second.places) + " places, " +
                   std::to_string(a->second.colour.size() - a->second.places) + " transitions, " +
                   std::to_string(a->second.arcs.size()) + " arcs; reference " +
                   std::to_string(e->second.places) + ", " +
                   std::to_string(e->second.colour.size() - e->second.places) + ", " +
                   std::to_string(e->second.arcs.size()) + ")";
          break;
        }
        ++matched;
      }
    }
    if (detail.empty()) return {};
    if (best.empty() || matched > best_matched) best = detail, best_matched = matched;
  }
  return best;
}

}  // namespace

int main() {
  int failures = 0;
  std::size_t compared = 0, raising = 0;
  for (const auto& item : bank()) {
    ++compared;
    if (item.variants.empty()) ++raising;
    const auto detail = difference(item, [&] {
      if (item.call == "ocpn/module_wo_annotation") return wo_annotation_case(item);
      return item.call.starts_with("ocpn/") ? ocpn_case(item) : saw_nets_case(item);
    });
    if (!detail.empty()) {
      ++failures;
      std::fprintf(stderr, "%s %s: %s\n", item.name.c_str(), item.call.c_str(), detail.c_str());
    }
  }
  std::printf("%zu calls compared (%zu raising)\n", compared, raising);
  if (compared < 1080 || raising < 70) {
    ++failures;
    std::fprintf(stderr, "bank coverage below the pinned floor\n");
  }
  return failures == 0 ? 0 : 1;
}
