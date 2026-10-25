// Object-centric graph discovery and conformance against the public reference
// (docs/ocel-graphs-bank.json.gz): seeded OCELs through the public OTG, ETOT
// and OCDFG discovery dispatchers (OCDFG with and without edge performance
// and with custom activity/object-type selectors)
// and the three graph-comparison conformance dispatchers, with the OCEL and
// with its discovered graph as the real side, under default and custom
// parameters. Every value and raising call is bound to the reference output
// under four hash seeds; sets and dicts compare as sets, lists in order.
#include "minera/ocel.hpp"

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
  Json real, normative;
  std::vector<Json> expected;  // admissible outcomes
};

const std::vector<BankCase>& bank() {
  static const std::vector<BankCase> cases = [] {
    static const char* const documents[] = {
#include "ocel_graphs_cases.inc"
    };
    std::vector<BankCase> out;
    for (const char* document : documents) {
      const auto json = JsonReader(document).value();
      BankCase item;
      item.name = json.get("name")->text;
      item.call = json.get("call")->text;
      item.real = *json.get("real");
      item.normative = *json.get("normative");
      const auto& value = *json.get("expected");
      if (const auto* observed = value.get("observed")) item.expected = observed->items;
      else item.expected.push_back(value);
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

Json otg_json_value(const OtgGraph& graph) {
  std::vector<std::pair<Json, Json>> edges;
  for (const auto& edge : graph.edges)
    edges.emplace_back(array({text(edge.source_type), text(edge.relation), text(edge.target_type)}),
                       count(static_cast<double>(edge.frequency)));
  return array({string_set(graph.object_types), dict(std::move(edges))});
}

Json etot_json_value(const EtotGraph& graph) {
  std::vector<Json> relations;
  std::vector<std::pair<Json, Json>> counts;
  for (const auto& edge : graph.edges) {
    relations.push_back(array({text(edge.activity), text(edge.object_type)}));
    counts.emplace_back(array({text(edge.activity), text(edge.object_type)}),
                        count(static_cast<double>(edge.frequency)));
  }
  return array({string_set(graph.activities), string_set(graph.object_types), set(std::move(relations)),
                dict(std::move(counts))});
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

Json etot_conformance(const EtotConformanceResult& result) {
  std::vector<Json> missing, additional;
  for (const auto& edge : result.missing_edges) missing.push_back(pair_json(edge));
  for (const auto& edge : result.additional_edges) additional.push_back(pair_json(edge));
  std::vector<std::pair<Json, Json>> delta;
  for (const auto& [edge, value] : result.relative_frequency_delta) delta.emplace_back(pair_json(edge), number(value));
  return dict({{text("details"), dict({{text("A_additional"), string_set(result.additional_activities)},
                                        {text("A_missing"), string_set(result.missing_activities)},
                                        {text("OT_additional"), string_set(result.additional_object_types)},
                                        {text("OT_missing"), string_set(result.missing_object_types)},
                                        {text("R_additional"), set(std::move(additional))},
                                        {text("R_missing"), set(std::move(missing))},
                                        {text("delta_rel"), dict(std::move(delta))}})},
               {text("fitness"), number(result.fitness)}});
}

Json otg_conformance(const OtgConformanceResult& result) {
  std::vector<Json> missing, additional;
  for (const auto& edge : result.missing_edges) missing.push_back(triple_json(edge));
  for (const auto& edge : result.additional_edges) additional.push_back(triple_json(edge));
  std::vector<std::pair<Json, Json>> non_conforming;
  for (const auto& [edge, value] : result.non_conforming_edges)
    non_conforming.emplace_back(triple_json(edge), number(value));
  return dict({{text("additional_edges"), set(std::move(additional))},
               {text("additional_object_types"), string_set(result.additional_object_types)},
               {text("fitness"), number(result.fitness)},
               {text("missing_edges"), set(std::move(missing))},
               {text("missing_object_types"), string_set(result.missing_object_types)},
               {text("non_conforming_edges"), dict(std::move(non_conforming))}});
}

Json ocdfg_conformance(const OcdfgConformanceResult& result) {
  const auto flows = [](const std::vector<std::pair<std::string, std::string>>& values) {
    std::vector<Json> items;
    for (const auto& value : values) items.push_back(pair_json(value));
    return set(std::move(items));
  };
  std::vector<std::pair<Json, Json>> activity_differences, flow_differences;
  for (const auto& [activity, value] : result.activity_measure_differences)
    activity_differences.emplace_back(text(activity), number(static_cast<double>(value)));
  for (const auto& [flow, value] : result.flow_measure_differences)
    flow_differences.emplace_back(pair_json(flow), number(static_cast<double>(value)));
  return dict({{text("activity_measure_differences"), dict(std::move(activity_differences))},
               {text("additional_activities"), string_set(result.additional_activities)},
               {text("additional_flows"), flows(result.additional_flows)},
               {text("fitness"), number(result.fitness)},
               {text("flow_measure_differences"), dict(std::move(flow_differences))},
               {text("missing_activities"), string_set(result.missing_activities)},
               {text("missing_flows"), flows(result.missing_flows)},
               {text("non_conforming_activities_in_measure"),
                string_set(result.non_conforming_activities_in_measure)},
               {text("non_conforming_flows_in_measure"), flows(result.non_conforming_flows_in_measure)}});
}

// One case function per public module, each called from main.
Json discover_otg_case(const BankCase& item) { return otg_json_value(discover_otg_classic(ocel(item.real))); }

Json discover_etot_case(const BankCase& item) { return etot_json_value(discover_etot_classic(ocel(item.real))); }

Json discover_ocdfg_case(const BankCase& item) {
  if (item.call == "discover/ocdfg/selectors") {
    OcdfgDiscoveryOptions options;
    options.activity_attribute = "label";
    options.object_type_attribute = "kind";
    return ocdfg_json_value(discover_ocdfg_classic_with_options(ocel(item.real, true), options));
  }
  return ocdfg_json_value(discover_ocdfg_classic(ocel(item.real), item.call == "discover/ocdfg"));
}

bool custom(const BankCase& item) { return item.call.find("/custom/") != std::string::npos; }
bool graph_side(const BankCase& item) { return item.call.ends_with("/graph"); }

Json conform_otg_case(const BankCase& item) {
  OtgConformanceOptions options;
  if (custom(item)) {
    options.theta_by_relation = {{"object_interaction", 0.5}, {"object_cobirth", 0.0}};
    options.alpha = 2.0;
    options.beta = 0.5;
    options.gamma = 1.5;
  }
  const auto real = ocel(item.real);
  const auto model = discover_otg_classic(ocel(item.normative));
  return otg_conformance(graph_side(item) ? compare_otg(discover_otg_classic(real), model, options)
                                          : compare_otg(real, model, options));
}

Json conform_etot_case(const BankCase& item) {
  EtotConformanceOptions options;
  if (custom(item)) {
    options.theta_relative = 0.3;
    options.alpha = 0.5;
    options.beta = 2.0;
    options.gamma = 1.0;
  }
  const auto real = ocel(item.real);
  const auto model = discover_etot_classic(ocel(item.normative));
  return etot_conformance(graph_side(item) ? compare_etot(discover_etot_classic(real), model, options)
                                           : compare_etot(real, model, options));
}

Json conform_ocdfg_case(const BankCase& item) {
  OcdfgConformanceOptions options;
  if (custom(item)) {
    options.theta_activity = 0.2;
    options.theta_flow = 0.25;
    options.alpha = 2.0;
    options.beta = 1.0;
    options.gamma = 0.5;
    options.delta = 1.5;
  }
  const auto real = ocel(item.real);
  const auto model = discover_ocdfg_classic(ocel(item.normative), false);
  return ocdfg_conformance(graph_side(item) ? compare_ocdfg(discover_ocdfg_classic(real, false), model, options)
                                            : compare_ocdfg(real, model, options));
}

std::string difference(const BankCase& item, const std::function<Json()>& produce) {
  Json actual;
  actual.kind = Json::kObject;
  std::string message;
  try {
    actual.fields["value"] = produce();
  } catch (const std::exception& error) {
    actual.fields["error"] = text("raised");
    message = error.what();
  }
  normalize(actual);
  const auto rendered = render(actual);
  std::vector<std::string> admissible;
  for (auto expected : item.expected) {
    // The error class name is not bound, only that the call raised.
    if (expected.get("error")) expected.fields["error"] = text("raised");
    normalize(expected);
    admissible.push_back(render(expected));
  }
  if (std::find(admissible.begin(), admissible.end(), rendered) != admissible.end()) return {};
  // Report the first differing position.
  const auto& reference = admissible.front();
  std::size_t at = 0;
  while (at < rendered.size() && at < reference.size() && rendered[at] == reference[at]) ++at;
  const auto from = at > 200 ? at - 200 : 0;
  return "native ..." + rendered.substr(from, 500) + (message.empty() ? "" : " (" + message + ")") +
         " reference ..." + reference.substr(from, 500);
}

}  // namespace

int main() {
  int failures = 0;
  std::size_t compared = 0, raising = 0;
  for (const auto& item : bank()) {
    ++compared;
    if (item.expected.front().get("error")) ++raising;
    const auto& call = item.call;
    const auto detail = difference(item, [&] {
      if (call.starts_with("discover/otg")) return discover_otg_case(item);
      if (call.starts_with("discover/etot")) return discover_etot_case(item);
      if (call.starts_with("discover/ocdfg")) return discover_ocdfg_case(item);
      if (call.starts_with("conform/otg/")) return conform_otg_case(item);
      if (call.starts_with("conform/etot/")) return conform_etot_case(item);
      return conform_ocdfg_case(item);
    });
    if (!detail.empty()) {
      ++failures;
      std::fprintf(stderr, "%s %s: %s\n", item.name.c_str(), item.call.c_str(), detail.c_str());
    }
  }
  std::printf("%zu calls compared (%zu raising)\n", compared, raising);
  if (compared < 2550 || raising < 200) {
    ++failures;
    std::fprintf(stderr, "bank coverage below the pinned floor\n");
  }
  return failures == 0 ? 0 : 1;
}
