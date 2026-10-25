// Forward token replay PLTR and RETURN_NAMES outputs against the public
// reference (docs/token-replay-pltr-bank.json.gz): the frozen conformance
// nets with seeded logs (repeated variants, unknown labels, empty traces and
// value-equal trace copies), replayed with ENABLE_PLTR_FITNESS on source
// traces and on grouped variants, with RETURN_NAMES, and with both. Every
// row (fitness, counters, activated/problem/enabled transitions, reached
// marking, labels), every place's trace sets and m/r/c/p counters, every
// transition's underfed/fit event contexts and every unknown activity's
// contexts are bound to an output the reference returned under sixteen hash
// seeds. Trace keys are the source case ids, or "variant:" and the activities
// for a grouped variant; events are named by their eid attribute.
#include "minera/conformance.hpp"
#include "minera/pnml.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <map>
#include <optional>
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
  std::string name, pnml;
  struct TraceSpec { std::string key; std::vector<std::string> events, eids; };
  std::vector<TraceSpec> traces;
  std::map<std::string, std::vector<std::string>> expected;  // config -> admissible renders
  std::map<std::string, bool> raised;
};

// Reorders every list the reference sorted, so both sides use one order.
void normalize(Json& value, const std::string& config) {
  if (value.get("rows")) {
    for (auto& row : value.fields["rows"].items) {
      canonical_sort(row.fields["enabled"]);
      canonical_sort(row.fields["reached"]);
      if (row.get("enabled_pairs")) canonical_sort(row.fields["enabled_pairs"]);
    }
  }
  for (const char* key : {"places", "transitions", "unknown"}) {
    if (!value.get(key)) continue;
    auto& list = value.fields[key];
    for (auto& entry : list.items) {
      if (std::string(key) == "places") {
        canonical_sort(entry.items[1]);
        canonical_sort(entry.items[2]);
      } else {
        for (std::size_t i = 1; i < entry.items.size(); ++i) canonical_sort(entry.items[i]);
      }
    }
    canonical_sort(list);
  }
  (void)config;
}

const std::vector<BankCase>& bank() {
  static const std::vector<BankCase> cases = [] {
    static const char* const documents[] = {
#include "token_replay_pltr_cases.inc"
    };
    std::vector<BankCase> out;
    for (const char* document : documents) {
      const auto json = JsonReader(document).value();
      BankCase item;
      item.name = json.get("name")->text;
      item.pnml = json.get("pnml")->text;
      for (const auto& trace : json.get("traces")->items) {
        BankCase::TraceSpec spec;
        spec.key = trace.get("key")->text;
        for (const auto& event : trace.get("events")->items) spec.events.push_back(event.text);
        for (const auto& eid : trace.get("eids")->items) spec.eids.push_back(eid.text);
        item.traces.push_back(std::move(spec));
      }
      for (const auto& [config, call] : json.get("calls")->fields) {
        if (call.get("timeout")) continue;
        if (call.get("error")) { item.raised[config] = true; continue; }
        std::vector<Json> choices;
        if (const auto* observed = call.get("observed")) choices = observed->items;
        else choices.push_back(call);
        auto& renders = item.expected[config];
        for (auto& choice : choices) {
          normalize(choice, config);
          renders.push_back(render(choice));
        }
      }
      out.push_back(std::move(item));
    }
    return out;
  }();
  return cases;
}

EventLog event_log(const BankCase& item) {
  EventLog log;
  std::size_t order = 0;
  for (const auto& spec : item.traces) {
    auto& trace = log.traces.emplace_back();
    trace.case_id = spec.key;
    trace.case_id_state = FirstClassAttributeState::kPresent;
    for (std::size_t i = 0; i < spec.events.size(); ++i) {
      Event event;
      event.case_id = spec.key;
      event.activity = spec.events[i];
      event.activity_state = FirstClassAttributeState::kPresent;
      event.input_order = order++;
      EventAttribute eid;
      eid.key = "eid";
      eid.value = spec.eids[i];
      event.attributes.push_back(std::move(eid));
      trace.events.push_back(std::move(event));
    }
  }
  return log;
}

// Variant traces may hold the activity as a plain attribute.
std::optional<std::string> activity(const Event& event) {
  if (event.has_activity() && !event.activity.empty()) return event.activity;
  if (const auto* value = event.attribute("concept:name")) return *value;
  if (event.has_activity()) return event.activity;
  return std::nullopt;
}

std::string trace_key(const TokenReplayTraceReference& reference) {
  if (reference.source_trace_index) return reference.value.case_id;
  std::string out = "variant:";
  for (std::size_t i = 0; i < reference.value.events.size(); ++i)
    out += (i ? "," : "") + activity(reference.value.events[i]).value_or("");
  return out;
}

std::string event_name(const Event& event) {
  if (const auto* eid = event.attribute("eid")) return *eid;
  return activity(event).value_or("@@empty");
}

Json contexts(const TokenReplayPltrFitness& fitness,
              const std::vector<TokenReplayTraceContexts>& entries) {
  auto out = array();
  for (const auto& entry : entries) {
    auto names = array();
    for (const auto& event : entry.contexts) names.items.push_back(text(event_name(event)));
    out.items.push_back(array({text(trace_key(fitness.trace_references.at(entry.trace_reference))), names}));
  }
  return out;
}

Json native_output(const PnmlDocument& document, const TokenReplayResult& result, bool pltr,
                   bool names) {
  const auto& ids = document.transition_ids;
  const auto label = [&](std::size_t t) {
    const auto& activity = document.net.transitions().at(t).activity;
    Json out;
    if (activity) out = text(document.net.activities().at(*activity));
    return out;
  };
  Json out;
  out.kind = Json::kObject;
  auto rows = array();
  for (const auto& row : result.traces) {
    Json item;
    item.kind = Json::kObject;
    const auto transitions = [&](const std::vector<std::size_t>& list) {
      auto values = array();
      for (const auto t : list) values.items.push_back(text(ids.at(t)));
      return values;
    };
    item.fields["fit"] = boolean(row.trace_is_fit);
    item.fields["fitness"] = number(row.trace_fitness);
    item.fields["activated"] = transitions(row.activated_transitions);
    item.fields["problems"] = transitions(row.transitions_with_problems);
    item.fields["enabled"] = transitions(row.enabled_transitions);
    auto reached = array();
    for (std::size_t p = 0; p < row.reached_marking.size(); ++p) {
      const bool key = row.reached_marking_keys.empty() ? row.reached_marking[p] != TokenInteger(0)
                                                        : row.reached_marking_keys[p];
      if (key) reached.items.push_back(array({text(document.place_ids.at(p)), integer(row.reached_marking[p])}));
    }
    item.fields["reached"] = reached;
    item.fields["counts"] = array({integer(row.missing), integer(row.consumed), integer(row.remaining),
                                   integer(row.produced)});
    std::vector<std::string> keys = {"activated_transitions", "consumed_tokens",
                                     "enabled_transitions_in_marking", "missing_tokens",
                                     "produced_tokens", "reached_marking", "remaining_tokens",
                                     "trace_fitness", "trace_is_fit", "transitions_with_problems"};
    if (names) {
      auto labels = array();
      for (const auto t : row.activated_transitions) labels.items.push_back(label(t));
      item.fields["activated_labels"] = labels;
      auto pairs = array();
      for (const auto t : row.enabled_transitions) pairs.items.push_back(array({text(ids.at(t)), label(t)}));
      item.fields["enabled_pairs"] = pairs;
      keys.push_back("activated_transitions_labels");
      keys.push_back("enabled_transitions_in_marking_labels");
      std::sort(keys.begin(), keys.end());
    }
    item.fields["keys"] = strings(keys);
    rows.items.push_back(std::move(item));
  }
  out.fields["rows"] = rows;
  if (!pltr) return out;
  const auto& fitness = *result.pltr_fitness;
  auto places = array();
  for (std::size_t p = 0; p < fitness.places.size(); ++p) {
    const auto& place = fitness.places[p];
    auto under = array(), over = array();
    for (const auto t : place.underfed_traces) under.items.push_back(text(trace_key(fitness.trace_references.at(t))));
    for (const auto t : place.overfed_traces) over.items.push_back(text(trace_key(fitness.trace_references.at(t))));
    places.items.push_back(array({text(document.place_ids.at(p)), under, over,
                                  array({integer(place.missing), integer(place.remaining),
                                         integer(place.consumed), integer(place.produced)})}));
  }
  out.fields["places"] = places;
  auto transitions = array();
  for (const auto& transition : fitness.transitions)
    transitions.items.push_back(array({text(ids.at(transition.transition_index)),
                                       contexts(fitness, transition.underfed_traces),
                                       contexts(fitness, transition.fit_traces)}));
  out.fields["transitions"] = transitions;
  auto unknown = array();
  for (const auto& activity : fitness.unknown_activities)
    unknown.items.push_back(array({text(activity.activity.value), contexts(fitness, activity.traces)}));
  out.fields["unknown"] = unknown;
  return out;
}

std::string difference(const BankCase& item, const std::string& config) {
  const auto document = parse_pnml_document(item.pnml);
  TokenReplayOptions options;
  options.transition_names = document.transition_ids;
  options.place_names = document.place_ids;
  options.exact_initial_marking = document.initial_marking;
  options.exact_final_marking = document.final_marking;
  options.place_outgoing_transition_order = pnml_place_outgoing_transition_order(document);
  const bool pltr = config != "names";
  const bool names = config.rfind("names", 0) == 0;
  options.enable_pltr_fitness = pltr;
  options.return_names = names;
  if (config == "pltr_grouped") options.disable_variants = false;
  TokenReplayResult result;
  try {
    result = replay_token_result(event_log(item), document.net, options);
  } catch (const std::exception& error) {
    return item.raised.contains(config) ? std::string() : std::string("raised ") + error.what();
  }
  if (item.raised.contains(config)) return "returned where the reference raised";
  auto actual = native_output(document, result, pltr, names);
  normalize(actual, config);
  const auto rendered = render(actual);
  const auto& admissible = item.expected.at(config);
  if (std::find(admissible.begin(), admissible.end(), rendered) != admissible.end()) return {};
  // Name the first differing section for the log.
  const auto reference = JsonReader(admissible.front().c_str()).value();
  for (const auto& [key, field] : actual.fields) {
    const auto* other = reference.get(key);
    if (!other || render(*other) != render(field)) {
      if (key == "rows" && other)
        for (std::size_t i = 0; i < std::min(field.items.size(), other->items.size()); ++i)
          if (render(field.items[i]) != render(other->items[i]))
            return "row " + std::to_string(i) + ": native " + render(field.items[i]) + " reference " +
                   render(other->items[i]);
      return key + ": native " + render(field).substr(0, 600) + " reference " +
             (other ? render(*other).substr(0, 600) : std::string("absent"));
    }
  }
  return "differs";
}

}  // namespace

int main() {
  int failures = 0;
  std::size_t compared = 0, varying = 0;
  for (const auto& item : bank()) {
    for (const auto& [config, admissible] : item.expected) {
      ++compared;
      if (admissible.size() > 1) ++varying;
      const auto detail = difference(item, config);
      if (!detail.empty()) {
        ++failures;
        std::fprintf(stderr, "%s %s: %s\n", item.name.c_str(), config.c_str(), detail.c_str());
      }
    }
  }
  std::printf("%zu calls compared (%zu with seed-dependent output)\n", compared, varying);
  if (compared < 1600) {
    ++failures;
    std::fprintf(stderr, "bank coverage below the pinned floor\n");
  }
  return failures == 0 ? 0 : 1;
}
