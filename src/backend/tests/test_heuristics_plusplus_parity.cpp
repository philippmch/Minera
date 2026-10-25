// Heuristics Miner++ against the public reference
// (docs/heuristics-plusplus-bank.json.gz): seeded interval logs simulated from
// random process trees, mined by the public plusplus.apply_heu on the
// EventLog and by the public dispatcher apply_heu (PLUSPLUS) on the equivalent
// DataFrame under six parameter sets, and converted to Petri nets by the
// public dispatcher apply on both forms (the DataFrame form takes the pandas
// route). Every public HeuristicsNet field and node connection is bound to
// the reference output; Petri nets are bound up to renaming of places and
// silent transitions, since the reference names them in hash order. Twenty
// DataFrame controls bind the pandas route's pair-free errors and missing
// case id/activity rows through the public plusplus.apply_heu_pandas.
#include "minera/heuristic_miner.hpp"
#include "minera/petri_net.hpp"

#include <algorithm>
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
Json integer(const TokenInteger& value) { return number(std::strtod(value.decimal().c_str(), nullptr)); }
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
  bool with_start = true;
  std::vector<std::vector<std::tuple<std::string, int, int>>> traces;
  std::vector<Json> expected;  // admissible outcomes
  // DataFrame controls: [case id or null, activity or null, start, complete].
  std::optional<Json> frame_rows;
  bool aware = true;
};

const std::vector<BankCase>& bank() {
  static const std::vector<BankCase> cases = [] {
    static const char* const documents[] = {
#include "heuristics_plusplus_cases.inc"
    };
    std::vector<BankCase> out;
    for (const char* document : documents) {
      const auto json = JsonReader(document).value();
      BankCase item;
      item.name = json.get("name")->text;
      item.call = json.get("call")->text;
      item.with_start = json.get("with_start")->boolean;
      for (const auto& trace : json.get("traces")->items) {
        auto& events = item.traces.emplace_back();
        for (const auto& event : trace.items)
          events.emplace_back(event.items[0].text, static_cast<int>(event.items[1].number),
                              static_cast<int>(event.items[2].number));
      }
      if (const auto* rows = json.get("frame_rows")) {
        item.frame_rows = *rows;
        item.aware = json.get("tz")->boolean;
      }
      const auto& value = *json.get("expected");
      if (const auto* observed = value.get("observed")) item.expected = observed->items;
      else item.expected.push_back(value);
      out.push_back(std::move(item));
    }
    return out;
  }();
  return cases;
}

std::string stamp(int seconds) {
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "2024-01-01T%02d:%02d:%02dZ", seconds / 3600, seconds / 60 % 60,
                seconds % 60);
  return buffer;
}

EventAttribute date(const std::string& key, int seconds, bool aware = true) {
  EventAttribute out;
  out.key = key;
  out.value = stamp(seconds);
  if (!aware) out.value.pop_back();
  out.kind = AttributeKind::kDate;
  NativeDate value{out.value};
  if (aware) value.timezone = "UTC";
  out.typed_value = std::move(value);
  return out;
}

EventAttribute string_cell(const std::string& key, const std::string& value) {
  return EventAttribute{key, value, AttributeKind::kString};
}

std::string case_id(std::size_t index) {
  char buffer[16];
  std::snprintf(buffer, sizeof buffer, "c%02zu", index);
  return buffer;
}

EventLog event_log(const BankCase& item) {
  EventLog log;
  for (std::size_t index = 0; index < item.traces.size(); ++index) {
    auto& trace = log.traces.emplace_back();
    trace.case_id = case_id(index);
    trace.case_id_state = FirstClassAttributeState::kPresent;
    trace.case_id_source_key = "case:concept:name";
    for (const auto& [activity, start, end] : item.traces[index]) {
      Event event;
      event.case_id = trace.case_id;
      event.case_id_state = FirstClassAttributeState::kPresent;
      event.case_id_source_key = "case:concept:name";
      event.activity = activity;
      event.activity_state = FirstClassAttributeState::kPresent;
      event.activity_source_key = "concept:name";
      event.timestamp = stamp(end);
      event.timestamp_attribute = date("time:timestamp", end);
      event.timestamp_state = FirstClassAttributeState::kPresent;
      event.timestamp_source_key = "time:timestamp";
      if (item.with_start) event.attributes.push_back(date("start_timestamp", start));
      trace.events.push_back(std::move(event));
    }
  }
  return log;
}

EventTable event_table(const BankCase& item) {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  table.dtypes = {"str", "str", "datetime64[us, UTC]"};
  if (item.with_start) {
    table.columns.push_back("start_timestamp");
    table.dtypes.push_back("datetime64[us, UTC]");
  }
  EventTableIndex index;
  index.kind = "RangeIndex";
  index.dtype = "int64";
  index.names = {std::nullopt};
  std::int64_t label = 0;
  for (std::size_t trace = 0; trace < item.traces.size(); ++trace) {
    for (const auto& [activity, start, end] : item.traces[trace]) {
      EventTable::Row row{string_cell("case:concept:name", case_id(trace)),
                          string_cell("concept:name", activity), date("time:timestamp", end)};
      if (item.with_start) row.push_back(date("start_timestamp", start));
      table.rows.push_back(std::move(row));
      EventAttribute value;
      value.value = std::to_string(label);
      value.kind = AttributeKind::kInt;
      value.typed_value = label++;
      index.labels.push_back(EventTableIndexLabel{std::move(value)});
    }
  }
  table.index = std::move(index);
  table.validate();
  return table;
}

EventTable control_table(const BankCase& item) {
  EventTable table;
  const std::string dtype = item.aware ? "datetime64[us, UTC]" : "datetime64[us]";
  table.columns = {"case:concept:name", "concept:name", "time:timestamp", "start_timestamp"};
  table.dtypes = {"str", "str", dtype, dtype};
  const auto optional_text = [](const std::string& key, const Json& value) {
    if (value.kind == Json::kNull) {
      EventAttribute missing{key, "nan", AttributeKind::kString};
      missing.is_null = true;
      return missing;
    }
    return string_cell(key, value.text);
  };
  for (const auto& row : item.frame_rows->items)
    table.rows.push_back({optional_text("case:concept:name", row.items[0]),
                          optional_text("concept:name", row.items[1]),
                          date("time:timestamp", static_cast<int>(row.items[3].number), item.aware),
                          date("start_timestamp", static_cast<int>(row.items[2].number), item.aware)});
  table.validate();
  return table;
}

HeuristicMinerOptions options_for(const std::string& config) {
  HeuristicMinerOptions options;
  options.variant = HeuristicMinerVariant::kPlusPlus;
  if (config == "dependency_high") options.dependency_threshold = 0.9;
  if (config == "dependency_zero_and_low") {
    options.dependency_threshold = 0.0;
    options.and_threshold = 0.3;
  }
  if (config == "and_high") options.and_threshold = 0.9;
  if (config == "counts") {
    options.min_activity_count = 2;
    options.min_frequency = 2;
  }
  if (config == "performance") options.plusplus_decoration_selector = "performance";
  return options;
}

Json object() {
  Json out;
  out.kind = Json::kObject;
  return out;
}

double scalar(const HeuristicDependency& edge) {
  if (edge.representation_value) {
    if (const auto* value = std::get_if<double>(&*edge.representation_value)) return *value;
    if (const auto* structured = std::get_if<HeuristicStructuredValue>(&*edge.representation_value)) {
      if (const auto* whole = std::get_if<std::uint64_t>(&structured->value)) return static_cast<double>(*whole);
      if (const auto* whole = std::get_if<std::int64_t>(&structured->value)) return static_cast<double>(*whole);
      if (const auto* real = std::get_if<double>(&structured->value)) return *real;
    }
  }
  return edge.representation;
}

Json counts(const std::map<std::string, HeuristicOccurrenceCount>& values) {
  auto out = array();
  for (const auto& [name, count] : values)
    out.items.push_back(array({text(name), number(static_cast<double>(count.to_uint64()))}));
  return out;
}

Json boundary(const std::optional<HeuristicNetMetadata>& metadata, bool start) {
  auto entries = array();
  if (metadata) {
    const auto& value = start ? metadata->start_activities : metadata->end_activities;
    if (const auto* map = std::get_if<std::map<std::string, std::uint64_t>>(&value))
      for (const auto& [name, count] : *map)
        entries.items.push_back(array({text(name), number(static_cast<double>(count))}));
  }
  return array({entries});
}

// The reference's public HeuristicsNet fields, in the bank's shape.
Json heuristics_json(const HeuristicsNet& net) {
  const auto& names = net.activities;
  auto out = object();
  out.fields["activities"] = strings(names);
  out.fields["start_activities"] = boundary(net.metadata, true);
  out.fields["end_activities"] = boundary(net.metadata, false);
  out.fields["activities_occurrences"] = counts(net.metadata->activities_occurrences);
  auto dfg = array(), dependency = array(), performance_matrix = array();
  for (const auto& relation : net.relation_statistics)
    dfg.items.push_back(array({text(names[relation.source]), text(names[relation.target]),
                               number(static_cast<double>(relation.eventually_follows_frequency))}));
  for (const auto& measure : net.dependency_measures)
    dependency.items.push_back(array({text(names[measure.source]), text(names[measure.target]),
                                      number(measure.value)}));
  for (const auto& [pair, value] : net.plusplus_computed_performance_matrix)
    performance_matrix.items.push_back(array({text(pair.first), text(pair.second), number(value)}));
  out.fields["dfg"] = dfg;
  out.fields["dfg_matrix"] = dfg;
  out.fields["dependency_matrix"] = dependency;
  out.fields["performance_matrix"] = performance_matrix;
  if (net.plusplus_computed_performance_dfg) {
    auto performance = array();
    for (const auto& [pair, value] : *net.plusplus_computed_performance_dfg)
      performance.items.push_back(array({text(pair.first), text(pair.second), number(value)}));
    out.fields["performance_dfg"] = performance;
  } else {
    out.fields["performance_dfg"] = Json{};
  }
  auto sojourn = array();
  for (const auto& [name, value] : *net.sojourn_times) sojourn.items.push_back(array({text(name), number(value)}));
  out.fields["sojourn_times"] = sojourn;
  auto concurrent = array();
  for (const auto& pair : net.raw_concurrency_statistics)
    concurrent.items.push_back(array({text(pair.first), text(pair.second),
                                      number(static_cast<double>(pair.frequency))}));
  out.fields["concurrent_activities"] = concurrent;
  out.fields["node_type"] = text(net.decoration == HeuristicNetDecoration::kFrequency ? "frequency" : "performance");
  out.fields["dfg_window_2"] = boolean(net.plusplus_public_dfg_window_2_present);
  out.fields["freq_triples"] = boolean(net.plusplus_public_freq_triples_present);
  auto nodes = array();
  for (std::size_t id = 0; id < names.size(); ++id) {
    auto node = object();
    node.fields["name"] = text(names[id]);
    const auto statistic = std::find_if(net.activity_statistics.begin(), net.activity_statistics.end(),
                                        [&](const auto& item) { return item.activity == id; });
    node.fields["occ"] = number(static_cast<double>(statistic->occurrences.to_uint64()));
    node.fields["is_start"] = boolean(statistic->is_start_activity.value_or(false));
    node.fields["is_end"] = boolean(statistic->is_end_activity.value_or(false));
    auto outputs = array(), inputs = array();
    for (const auto& edge : net.edges) {
      const auto entry = [&](std::size_t other) {
        return array({text(names[other]), number(edge.dependency), number(static_cast<double>(edge.frequency)),
                      number(scalar(edge))});
      };
      if (edge.source == id) outputs.items.push_back(entry(edge.target));
      if (edge.target == id) inputs.items.push_back(entry(edge.source));
    }
    node.fields["outputs"] = outputs;
    node.fields["inputs"] = inputs;
    auto and_in = array(), and_out = array();
    for (const auto& measure : net.and_measures_in)
      if (measure.activity == id)
        and_in.items.push_back(array({text(names[measure.first]), text(names[measure.second]), number(measure.value)}));
    for (const auto& measure : net.and_measures_out)
      if (measure.activity == id)
        and_out.items.push_back(array({text(names[measure.first]), text(names[measure.second]), number(measure.value)}));
    node.fields["and_in"] = and_in;
    node.fields["and_out"] = and_out;
    auto loops = array();
    for (const auto& loop : net.loop_length_two)
      if (loop.source == id)
        loops.items.push_back(array({text(names[loop.target]), number(static_cast<double>(loop.frequency))}));
    node.fields["loop_two"] = loops;
    nodes.items.push_back(std::move(node));
  }
  out.fields["nodes"] = nodes;
  return out;
}

// Lists of the bank are sorted in Python order; both sides are put in one
// canonical order instead.
void normalize_heuristics(Json& value) {
  for (const char* key : {"start_activities", "end_activities"})
    if (value.get(key)) for (auto& item : value.fields[key].items) canonical_sort(item);
  for (const char* key : {"activities_occurrences", "dfg", "dfg_matrix", "dependency_matrix",
                          "performance_matrix", "sojourn_times", "concurrent_activities"})
    if (value.get(key)) canonical_sort(value.fields[key]);
  if (value.get("performance_dfg") && value.fields["performance_dfg"].kind == Json::kArray)
    canonical_sort(value.fields["performance_dfg"]);
  if (!value.get("nodes")) return;
  for (auto& node : value.fields["nodes"].items)
    for (const char* key : {"outputs", "inputs", "and_in", "and_out", "loop_two"}) canonical_sort(node.fields[key]);
  canonical_sort(value.fields["nodes"]);
}

// A Petri net as a labelled graph: nodes 0..places-1 are places.
struct Graph {
  std::vector<std::string> colour;  // initial colour: kind, label, marking
  std::map<std::pair<std::size_t, std::size_t>, std::string> arcs;  // weights as text
  std::size_t places = 0;
};

Graph reference_graph(const Json& value) {
  Graph graph;
  std::map<std::string, std::size_t> place, transition;
  std::map<std::string, std::string> initial, final;
  for (const auto& entry : value.get("initial")->items) initial[entry.items[0].text] = render(entry.items[1]);
  for (const auto& entry : value.get("final")->items) final[entry.items[0].text] = render(entry.items[1]);
  for (const auto& name : value.get("places")->items) {
    place[name.text] = graph.colour.size();
    graph.colour.push_back("p" + (initial.contains(name.text) ? initial[name.text] : "0") + "/" +
                           (final.contains(name.text) ? final[name.text] : "0"));
  }
  graph.places = graph.colour.size();
  for (const auto& entry : value.get("transitions")->items) {
    transition[entry.items[0].text] = graph.colour.size();
    graph.colour.push_back(entry.items[1].kind == Json::kNull ? "t" : "t:" + entry.items[1].text);
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

Graph native_graph(const PetriNetPresentation& net) {
  Graph graph;
  std::map<std::size_t, std::size_t> place, transition;
  std::map<std::size_t, std::int64_t> initial, final;
  for (const auto& entry : net.initial_marking) initial[entry.place_id] += entry.tokens;
  for (const auto& entry : net.final_marking) final[entry.place_id] += entry.tokens;
  for (const auto& item : net.places) {
    place[item.id] = graph.colour.size();
    graph.colour.push_back("p" + render(number(static_cast<double>(initial[item.id]))) + "/" +
                           render(number(static_cast<double>(final[item.id]))));
  }
  graph.places = graph.colour.size();
  for (const auto& item : net.transitions) {
    transition[item.id] = graph.colour.size();
    graph.colour.push_back(item.label ? "t:" + *item.label : "t");
  }
  for (const auto& arc : net.arcs) {
    const auto lookup = [&](const PetriNodeRef& node) {
      return node.kind == PetriNodeKind::kPlace ? place.at(node.id) : transition.at(node.id);
    };
    auto& weight = graph.arcs[{lookup(arc.source), lookup(arc.target)}];
    weight = render(integer(arc.weight));
  }
  return graph;
}

// Colour refinement followed by a backtracking search for a bijection that
// preserves colours, arcs and weights.
bool isomorphic(const Graph& a, const Graph& b) {
  if (a.colour.size() != b.colour.size() || a.arcs.size() != b.arcs.size()) return false;
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
      (void)graph;
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
  const auto colour = refine(joint, a.colour.size());
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

std::string error_name(const std::exception& error) {
  if (const auto* pandas = dynamic_cast<const HeuristicPandasError*>(&error)) {
    const auto& type = pandas->reference_type();
    return type.substr(type.rfind('.') + 1);
  }
  return std::string("native:") + error.what();
}

std::string difference(const BankCase& item) {
  const auto first = item.call.find('/');
  const auto second = item.call.rfind('/');
  const auto kind = item.call.substr(0, first);
  const auto config = item.call.substr(first + 1, second - first - 1);
  const bool frame = item.call.substr(second + 1) == "frame";
  const auto options = options_for(config);
  try {
    if (kind == "heu" || kind == "pandas") {
      auto control = item.frame_rows ? std::optional<EventTable>(control_table(item)) : std::nullopt;
      const auto net = control ? discover_plus_plus_pandas(*control, options)
                       : frame ? discover_heuristics_net(event_table(item), options)
                               : discover_heuristics_net(event_log(item), options);
      auto actual = heuristics_json(net);
      normalize_heuristics(actual);
      const auto rendered = render(actual);
      for (auto expected : item.expected) {
        normalize_heuristics(expected);
        if (render(expected) == rendered) return {};
      }
      auto expected = item.expected.front();
      normalize_heuristics(expected);
      for (const auto& [key, value] : expected.fields)
        if (!actual.get(key) || render(*actual.get(key)) != render(value))
          return key + ": native " + (actual.get(key) ? render(*actual.get(key)).substr(0, 900) : "absent") +
                 " reference " + render(value).substr(0, 900);
      return "field set differs";
    }
    auto table = frame ? std::optional<EventTable>(event_table(item)) : std::nullopt;
    const auto net = frame ? discover_plus_plus_pandas(*table, options)
                           : discover_heuristics_net(event_log(item), options);
    const auto graph = native_graph(to_petri_net_presentation(net));
    for (const auto& expected : item.expected) {
      if (expected.get("error")) continue;
      if (isomorphic(reference_graph(expected), graph)) return {};
    }
    return "Petri net not isomorphic to any reference outcome (" + std::to_string(graph.places) + " places, " +
           std::to_string(graph.colour.size() - graph.places) + " transitions, " +
           std::to_string(graph.arcs.size()) + " arcs; reference " +
           render(item.expected.front()).substr(0, 600) + ")";
  } catch (const std::exception& error) {
    const auto name = error_name(error);
    for (const auto& expected : item.expected) {
      const auto* reference = expected.get("error");
      const auto* message = expected.get("message");
      if (reference && reference->text == name && (!message || message->text == error.what())) return {};
    }
    return "native raised " + name + " (" + error.what() + ")";
  }
}

}  // namespace

int main() {
  int failures = 0;
  std::size_t compared = 0, raising = 0, petri = 0;
  for (const auto& item : bank()) {
    ++compared;
    if (item.expected.front().get("error")) ++raising;
    if (item.call.rfind("petri/", 0) == 0) ++petri;
    const auto detail = difference(item);
    if (!detail.empty()) {
      ++failures;
      std::fprintf(stderr, "%s %s: %s\n", item.name.c_str(), item.call.c_str(), detail.c_str());
    }
  }
  std::printf("%zu calls compared (%zu Petri, %zu raising)\n", compared, petri, raising);
  if (compared < 2580 || petri < 640 || raising < 30) {
    ++failures;
    std::fprintf(stderr, "bank coverage below the pinned floor\n");
  }
  return failures == 0 ? 0 : 1;
}
