// Petri alignment log dispatcher against the public reference
// (docs/petri-alignment-dispatcher-bank.json.gz): the frozen conformance nets
// with seeded logs, aligned as a whole EventLog and as a DataFrame under the
// four enum variants and the default (state-equation A*), with default costs,
// model/sync cost functions, a nondefault activity key, or the best-worst
// cost disabled. Per trace, None and each (cost, bwc, fitness) are bound to
// the values the reference returned under four hash seeds (the discounted
// search may instead be cheaper than every run, as per trace); raising calls
// must raise. A DataFrame aligns as the log of its cases in ascending case-id
// order. The per-trace alignments themselves are bound by
// minera_conformance_parity_contracts_tests.
#include "minera/petri_trace_alignment.hpp"
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

struct Expected {
  enum Kind { kRows, kError, kSkipped } kind = kSkipped;
  // Per trace: nullopt for None, else the observed rows (cost, bwc, fitness,
  // bwc present, fitness present).
  std::vector<std::optional<std::vector<std::vector<double>>>> rows;
};

struct BankCase {
  std::string name, mode, pnml;
  std::vector<std::vector<std::string>> traces;
  std::vector<std::string> case_ids;
  std::map<std::string, double> model_costs, sync_costs;
  std::map<std::string, Expected> calls;  // "variant/form"
};

Expected expected(const Json& value) {
  Expected out;
  if (value.get("error")) { out.kind = Expected::kError; return out; }
  const auto* rows = value.get("rows");
  if (!rows) return out;
  out.kind = Expected::kRows;
  for (const auto& row : rows->items) {
    if (row.kind == Json::kNull) { out.rows.emplace_back(); continue; }
    std::vector<std::vector<double>> seen;
    for (const auto& tuple : row.items) {
      std::vector<double> values;
      for (const auto& item : tuple.items)
        values.push_back(item.kind == Json::kBool ? (item.boolean ? 1.0 : 0.0)
                         : item.kind == Json::kNull ? std::nan("") : item.number);
      seen.push_back(std::move(values));
    }
    out.rows.emplace_back(std::move(seen));
  }
  return out;
}

const std::vector<BankCase>& bank() {
  static const std::vector<BankCase> cases = [] {
    static const char* const documents[] = {
#include "petri_alignment_dispatcher_cases.inc"
    };
    std::vector<BankCase> out;
    for (const char* document : documents) {
      const auto json = JsonReader(document).value();
      BankCase item;
      item.name = json.get("name")->text;
      item.mode = json.get("mode")->text;
      item.pnml = json.get("pnml")->text;
      for (const auto& trace : json.get("traces")->items) {
        auto& activities = item.traces.emplace_back();
        for (const auto& activity : trace.items) activities.push_back(activity.text);
      }
      for (const auto& id : json.get("case_ids")->items) item.case_ids.push_back(id.text);
      if (const auto* costs = json.get("model_costs"))
        for (const auto& [id, cost] : costs->fields) item.model_costs[id] = cost.number;
      if (const auto* costs = json.get("sync_costs"))
        for (const auto& [id, cost] : costs->fields) item.sync_costs[id] = cost.number;
      for (const auto& [call, value] : json.get("calls")->fields) item.calls[call] = expected(value);
      out.push_back(std::move(item));
    }
    return out;
  }();
  return cases;
}

const std::map<std::string, AlignmentVariant> kVariants = {
    {"default", AlignmentVariant::kStateEquationAStar},
    {"VERSION_STATE_EQUATION_A_STAR", AlignmentVariant::kStateEquationAStar},
    {"VERSION_DIJKSTRA_NO_HEURISTICS", AlignmentVariant::kDijkstraNoHeuristics},
    {"VERSION_DIJKSTRA_LESS_MEMORY", AlignmentVariant::kDijkstraLessMemory},
    {"VERSION_DISCOUNTED_A_STAR", AlignmentVariant::kDiscountedAStar},
};

EventLog event_log(const BankCase& item, bool frame) {
  std::vector<std::size_t> order(item.traces.size());
  for (std::size_t index = 0; index < order.size(); ++index) order[index] = index;
  if (frame) {
    std::stable_sort(order.begin(), order.end(),
                     [&](auto a, auto b) { return item.case_ids[a] < item.case_ids[b]; });
    // A DataFrame holds no empty cases.
    order.erase(std::remove_if(order.begin(), order.end(),
                               [&](auto index) { return item.traces[index].empty(); }),
                order.end());
  }
  const bool keyed = item.mode == "activity_key";
  EventLog log;
  for (const auto index : order) {
    auto& trace = log.traces.emplace_back();
    trace.case_id = item.case_ids[index];
    for (const auto& activity : item.traces[index]) {
      Event event;
      event.case_id = trace.case_id;
      event.activity = keyed ? "noise" : activity;
      event.activity_state = FirstClassAttributeState::kPresent;
      if (keyed) {
        EventAttribute attribute;
        attribute.key = "act";
        attribute.value = activity;
        event.attributes.push_back(std::move(attribute));
      }
      trace.events.push_back(std::move(event));
    }
  }
  return log;
}

bool same(double a, double b) {
  if (std::isnan(a) || std::isnan(b)) return std::isnan(a) && std::isnan(b);
  return std::abs(a - b) <= 1e-9 * std::max(1.0, std::abs(b));
}

std::string difference(const BankCase& item, const std::string& call, const Expected& want) {
  const auto slash = call.find('/');
  const auto variant = kVariants.at(call.substr(0, slash));
  const bool frame = call.substr(slash + 1) == "frame";
  const auto document = parse_pnml_document(item.pnml);
  const auto net = parse_pnml(item.pnml);
  PetriLogAlignmentOptions options;
  options.trace.variant = variant;
  if (item.mode == "activity_key") options.activity_key = "act";
  if (item.mode == "no_bwc") options.enable_best_worst_cost = false;
  if (item.mode == "costed") {
    options.trace.model_costs.assign(document.transition_ids.size(), 0.0);
    options.trace.sync_costs.assign(document.transition_ids.size(), 0.0);
    for (std::size_t index = 0; index < document.transition_ids.size(); ++index) {
      const auto& id = document.transition_ids[index];
      if (!item.model_costs.contains(id)) return "no model cost for transition " + id;
      options.trace.model_costs[index] = item.model_costs.at(id);
      // The frozen sync costs leave out silent transitions, which never move
      // synchronously.
      if (const auto sync = item.sync_costs.find(id); sync != item.sync_costs.end())
        options.trace.sync_costs[index] = sync->second;
    }
  }
  const auto log = event_log(item, frame);
  std::vector<PetriTraceAlignment> actual;
  try {
    actual = align_petri_log(log, net, options);
  } catch (const std::exception& error) {
    return want.kind == Expected::kError ? std::string() : std::string("raised ") + error.what();
  }
  if (want.kind == Expected::kError) return "returned where the reference raised";
  if (actual.size() != want.rows.size()) return "entry count differs";
  std::string detail;
  for (std::size_t index = 0; index < actual.size(); ++index) {
    const auto& row = want.rows[index];
    const auto& result = actual[index];
    if (!row) {
      if (result.computed) detail += "entry " + std::to_string(index) + " not None; ";
      continue;
    }
    if (!result.computed) { detail += "entry " + std::to_string(index) + " None; "; continue; }
    const double bwc = result.bwc ? *result.bwc : std::nan("");
    const double fitness = result.fitness ? *result.fitness : std::nan("");
    const auto matches = [&](const std::vector<double>& tuple, bool at_most) {
      const bool cost = at_most ? result.cost <= tuple[0] + 1e-12 : same(result.cost, tuple[0]);
      return cost && same(bwc, tuple[3] != 0.0 ? tuple[1] : std::nan("")) &&
             same(fitness, tuple[4] != 0.0 ? tuple[2] : std::nan(""));
    };
    bool admissible = std::any_of(row->begin(), row->end(), [&](const auto& t) { return matches(t, false); });
    if (!admissible && variant == AlignmentVariant::kDiscountedAStar)
      admissible = std::all_of(row->begin(), row->end(), [&](const auto& t) { return matches(t, true); });
    if (!admissible)
      detail += "entry " + std::to_string(index) + " cost " + std::to_string(result.cost) + "; ";
  }
  return detail;
}

}  // namespace

int main() {
  int failures = 0;
  std::size_t compared = 0, raising = 0, rows = 0, none = 0, skipped = 0;
  for (const auto& item : bank()) {
    for (const auto& [call, want] : item.calls) {
      if (want.kind == Expected::kSkipped) { ++skipped; continue; }
      ++compared;
      if (want.kind == Expected::kError) ++raising;
      for (const auto& row : want.rows) (row ? rows : none) += 1;
      const auto detail = difference(item, call, want);
      if (!detail.empty()) {
        ++failures;
        std::fprintf(stderr, "%s %s: %s\n", item.name.c_str(), call.c_str(), detail.c_str());
      }
    }
  }
  std::printf("%zu calls compared (%zu raising, %zu rows, %zu None), %zu skipped\n", compared,
              raising, rows, none, skipped);
  if (compared < 1500 || raising < 80 || rows < 7500) {
    ++failures;
    std::fprintf(stderr, "bank coverage below the pinned floor\n");
  }
  return failures == 0 ? 0 : 1;
}
