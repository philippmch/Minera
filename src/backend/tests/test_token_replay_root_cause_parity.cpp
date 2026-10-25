// Token replay root-cause diagnostics against the public reference
// (docs/token-replay-root-cause-bank.json.gz): the frozen conformance nets
// with seeded logs whose events carry org:resource, amount and cost, replayed
// forward, then diagnosed from the transition fitness and unknown-activity
// maps with random string/numeric selections and ENABLE_MULTIPLIER. Per
// model the feature names, data matrix, target and classes are bound exactly,
// and the fitted tree's root class counts, root Gini gain (equal for every
// optimal root split) and a depth of at most seven; the reference picks among
// equally good features without a seed, so the rest of the tree is not bound.
// A raising call must raise.
#include "minera/conformance.hpp"
#include "minera/pnml.hpp"
#include "minera/root_cause.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
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

struct Model {
  std::string key;
  std::vector<std::string> feature_names, classes;
  std::vector<std::vector<double>> data;
  std::vector<int> target;
  double root_samples = 0, root_gain = 0;
  std::vector<double> root_fractions;
};

struct Outcome {
  bool raised = false;
  std::vector<Model> models;
};

struct BankCase {
  std::string name, pnml;
  Json traces;
  RootCauseOptions options;
  // Every outcome some hash seed produced; native must equal one.
  std::vector<Outcome> transitions, unknown;
};

std::vector<std::string> strings(const Json& value) {
  std::vector<std::string> out;
  for (const auto& item : value.items) out.push_back(item.text);
  return out;
}

Outcome outcome(const Json& value) {
  Outcome out;
  if (value.get("error")) { out.raised = true; return out; }
  for (const auto& item : value.get("models")->items) {
    Model model;
    model.key = item.get("key")->text;
    model.feature_names = strings(*item.get("feature_names"));
    model.classes = strings(*item.get("classes"));
    for (const auto& row : item.get("data")->items) {
      auto& values = model.data.emplace_back();
      for (const auto& cell : row.items) values.push_back(cell.number);
    }
    for (const auto& label : item.get("target")->items) model.target.push_back(static_cast<int>(label.number));
    model.root_samples = item.get("root_samples")->number;
    model.root_gain = item.get("root_gain")->number;
    for (const auto& fraction : item.get("root_fractions")->items) model.root_fractions.push_back(fraction.number);
    out.models.push_back(std::move(model));
  }
  return out;
}

const std::vector<BankCase>& bank() {
  static const std::vector<BankCase> cases = [] {
    static const char* const documents[] = {
#include "token_replay_root_cause_cases.inc"
    };
    std::vector<BankCase> out;
    for (const char* document : documents) {
      const auto json = JsonReader(document).value();
      BankCase item;
      item.name = json.get("name")->text;
      item.pnml = json.get("pnml")->text;
      item.traces = *json.get("traces");
      item.options.string_attributes = strings(*json.get("string_attributes"));
      item.options.numeric_attributes = strings(*json.get("numeric_attributes"));
      item.options.enable_multiplier = json.get("enable_multiplier")->boolean;
      for (const char* form : {"transitions", "unknown"}) {
        const auto& value = *json.get(form);
        auto& outcomes = std::string(form) == "unknown" ? item.unknown : item.transitions;
        if (const auto* observed = value.get("observed"))
          for (const auto& choice : observed->items) outcomes.push_back(outcome(choice));
        else
          outcomes.push_back(outcome(value));
      }
      out.push_back(std::move(item));
    }
    return out;
  }();
  return cases;
}

EventAttribute attribute(const std::string& key, const Json& value) {
  EventAttribute out;
  out.key = key;
  if (value.kind == Json::kString) {
    out.value = value.text;
    return out;
  }
  // The bank's amounts are integers and its costs exact binary fractions.
  char buffer[64];
  if (key == "amount") {
    out.kind = AttributeKind::kInt;
    out.typed_value = static_cast<std::int64_t>(value.number);
    std::snprintf(buffer, sizeof buffer, "%lld", static_cast<long long>(value.number));
  } else {
    out.kind = AttributeKind::kFloat;
    out.typed_value = value.number;
    std::snprintf(buffer, sizeof buffer, "%.17g", value.number);
  }
  out.value = buffer;
  return out;
}

EventLog event_log(const Json& traces) {
  EventLog log;
  std::size_t order = 0;
  for (const auto& spec : traces.items) {
    auto& trace = log.traces.emplace_back();
    trace.case_id = spec.get("key")->text;
    trace.case_id_state = FirstClassAttributeState::kPresent;
    for (const auto& item : spec.get("events")->items) {
      Event event;
      event.case_id = trace.case_id;
      event.input_order = order++;
      for (const auto& [key, value] : item.fields) {
        if (key == "concept:name") {
          event.activity = value.text;
          event.activity_state = FirstClassAttributeState::kPresent;
        } else if (key == "org:resource") {
          event.resource = value.text;
          event.resource_state = FirstClassAttributeState::kPresent;
        } else {
          event.attributes.push_back(attribute(key, value));
        }
      }
      trace.events.push_back(std::move(event));
    }
  }
  return log;
}

double gini(const std::array<std::size_t, 2>& counts) {
  const double total = static_cast<double>(counts[0] + counts[1]);
  if (total == 0) return 0;
  const double a = counts[0] / total, b = counts[1] / total;
  return 1 - a * a - b * b;
}

bool same(double a, double b) { return std::abs(a - b) <= 1e-9; }

std::string compare(const Model& want, const RootCauseModel& have) {
  if (have.feature_names != want.feature_names) return "feature names";
  if (have.classes != want.classes) return "classes";
  if (have.target != want.target) return "target";
  if (have.data.size() != want.data.size()) return "row count";
  for (std::size_t r = 0; r < want.data.size(); ++r) {
    if (have.data[r].size() != want.data[r].size()) return "row width";
    for (std::size_t c = 0; c < want.data[r].size(); ++c)
      if (!same(have.data[r][c], want.data[r][c])) return "data";
  }
  if (have.tree.empty()) return "no tree";
  const auto& root = have.tree.front();
  const double samples = static_cast<double>(root.class_counts[0] + root.class_counts[1]);
  if (!same(samples, want.root_samples)) return "root samples";
  for (std::size_t k = 0; k < 2; ++k)
    if (!same(root.class_counts[k] / samples, want.root_fractions.at(k))) return "root class counts";
  double gain = 0;
  if (root.left >= 0) {
    const auto& left = have.tree.at(static_cast<std::size_t>(root.left));
    const auto& right = have.tree.at(static_cast<std::size_t>(root.right));
    const double l = static_cast<double>(left.class_counts[0] + left.class_counts[1]);
    const double r = static_cast<double>(right.class_counts[0] + right.class_counts[1]);
    gain = gini(root.class_counts) - (l * gini(left.class_counts) + r * gini(right.class_counts)) / samples;
  }
  if (std::abs(gain - want.root_gain) > 1e-6) return "root gain " + std::to_string(gain) + " vs " + std::to_string(want.root_gain);
  // Depth of at most seven.
  std::vector<std::pair<int, int>> stack{{0, 0}};
  while (!stack.empty()) {
    const auto [node, depth] = stack.back();
    stack.pop_back();
    if (depth > 7) return "depth";
    const auto& current = have.tree.at(static_cast<std::size_t>(node));
    if (current.left >= 0) {
      stack.push_back({current.left, depth + 1});
      stack.push_back({current.right, depth + 1});
    }
  }
  return {};
}

std::string side_difference(const Outcome& want, const TokenReplayRootCauseDiagnostics& result,
                            const std::optional<std::string>& raised, bool unknown,
                            const PnmlDocument& document, std::size_t& models) {
  if (raised) return want.raised ? std::string() : "raised " + *raised;
  if (want.raised) return "returned where the reference raised";
  std::vector<std::pair<std::string, const RootCauseModel*>> have;
  if (unknown)
    for (const auto& model : result.unknown_activities) have.emplace_back(model.activity, &model);
  else
    for (const auto& model : result.transitions)
      have.emplace_back(document.transition_ids.at(model.transition_index), &model);
  std::sort(have.begin(), have.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  if (have.size() != want.models.size())
    return "model count " + std::to_string(have.size()) + " vs " + std::to_string(want.models.size());
  std::string detail;
  for (std::size_t m = 0; m < have.size(); ++m) {
    ++models;
    if (have[m].first != want.models[m].key) { detail += "model key " + have[m].first + "; "; continue; }
    const auto why = compare(want.models[m], *have[m].second);
    if (!why.empty()) detail += want.models[m].key + ": " + why + "; ";
  }
  return detail;
}

std::string difference(const BankCase& item, std::size_t& models) {
  const auto document = parse_pnml_document(item.pnml);
  TokenReplayOptions options;
  options.transition_names = document.transition_ids;
  options.place_names = document.place_ids;
  options.exact_initial_marking = document.initial_marking;
  options.exact_final_marking = document.final_marking;
  options.place_outgoing_transition_order = pnml_place_outgoing_transition_order(document);
  options.root_cause = item.options;
  const auto log = event_log(item.traces);
  const auto traces = replay_token_traces(log, document.net, options);
  std::string detail;
  for (const bool unknown : {false, true}) {
    TokenReplayRootCauseDiagnostics result;
    std::optional<std::string> raised;
    try {
      if (unknown)
        result.unknown_activities = compute_token_replay_unknown_root_cause_diagnostics(log, document.net, traces, options);
      else
        result.transitions = compute_token_replay_transition_root_cause_diagnostics(log, document.net, traces, options);
    } catch (const std::exception& error) {
      raised = error.what();
    }
    const auto& wants = unknown ? item.unknown : item.transitions;
    std::string first;
    bool matched = false;
    for (const auto& want : wants) {
      std::size_t counted = 0;
      const auto why = side_difference(want, result, raised, unknown, document, counted);
      if (why.empty()) { matched = true; models += counted; break; }
      if (first.empty()) first = why;
    }
    if (!matched) detail += std::string(unknown ? "unknown: " : "transitions: ") + first + "; ";
  }
  return detail;
}

}  // namespace

int main() {
  int failures = 0;
  std::size_t compared = 0, models = 0;
  for (const auto& item : bank()) {
    ++compared;
    const auto detail = difference(item, models);
    if (!detail.empty()) {
      ++failures;
      std::fprintf(stderr, "%s: %s\n", item.name.c_str(), detail.c_str());
    }
  }
  std::printf("%zu cases compared, %zu models\n", compared, models);
  if (compared < 240 || models < 500) {
    ++failures;
    std::fprintf(stderr, "bank coverage below the pinned floor\n");
  }
  return failures == 0 ? 0 : 1;
}
