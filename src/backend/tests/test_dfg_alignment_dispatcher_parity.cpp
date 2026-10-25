// DFG alignment dispatcher against the public reference
// (docs/dfg-alignment-dispatcher-bank.json.gz): whole-log alignment of seeded
// random DFGs and logs with default or random sync/model/log/internal cost
// functions, on an EventLog and on the equivalent DataFrame. Costs, internal
// costs, fitness, best-worst cost and the raised/not-raised outcome are bound
// exactly. Both sides break queue ties by the lexical order of two fresh
// routing UUIDs per search, so the alignment and state counters can vary
// between calls: native is run under every relative order of the routing
// names, and each alignment and counter the reference returned (in sixteen
// processes) must be among those native produces. A DataFrame holds no empty
// cases, so it is checked as the log of its nonempty traces.
#include "minera/dfg_alignment.hpp"
#include "minera/event_log.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using namespace minera;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

enum OutcomeState { kResults, kError, kUnstable };
using Moves = std::vector<std::pair<std::string, std::string>>;

struct Row {
  std::vector<Moves> alignments;
  double cost, internal_cost, fitness, bwc;
  std::vector<std::size_t> visited, closed;
};

struct Outcome {
  OutcomeState state;
  std::string error;
  std::vector<Row> rows;
};

struct CostFunction {
  bool supplied;
  std::map<std::string, double> costs;
};

struct BankCase {
  std::string name;
  std::vector<std::tuple<std::string, std::string, std::uint64_t>> edges;
  std::vector<std::pair<std::string, std::uint64_t>> starts, ends;
  std::vector<std::vector<std::string>> traces;
  std::vector<CostFunction> costs;  // sync, model, log, internal
  Outcome log, frame;
};

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
        if (*at_ == '\\') ++at_;  // the fixture holds no other escapes
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

std::vector<std::size_t> counts(const Json& value) {
  std::vector<std::size_t> out;
  if (const auto* observed = value.get("observed"))
    for (const auto& item : observed->items) out.push_back(static_cast<std::size_t>(item.number));
  else out.push_back(static_cast<std::size_t>(value.number));
  return out;
}

Moves moves(const Json& value) {
  Moves out;
  for (const auto& move : value.items) out.push_back({move.items[0].text, move.items[1].text});
  return out;
}

Outcome outcome(const Json& value) {
  if (value.get("unstable")) return {kUnstable, {}, {}};
  if (const auto* error = value.get("error")) return {kError, error->text, {}};
  Outcome out{kResults, {}, {}};
  for (const auto& item : value.get("results")->items) {
    Row row;
    const auto& alignment = *item.get("alignment");
    if (const auto* observed = alignment.get("observed"))
      for (const auto& choice : observed->items) row.alignments.push_back(moves(choice));
    else row.alignments.push_back(moves(alignment));
    const auto scalar = [&](const char* key) {
      const auto* field = item.get(key);
      return field ? field->number : kNaN;
    };
    row.cost = scalar("cost");
    row.internal_cost = scalar("internal_cost");
    row.fitness = scalar("fitness");
    row.bwc = scalar("bwc");
    row.visited = counts(*item.get("visited_states"));
    row.closed = counts(*item.get("closed"));
    out.rows.push_back(std::move(row));
  }
  return out;
}

const std::vector<BankCase>& bank() {
  static const std::vector<BankCase> cases = [] {
    static const char* const documents[] = {
#include "dfg_alignment_dispatcher_cases.inc"
    };
    std::vector<BankCase> out;
    for (const auto* document : documents) {
      const auto json = JsonReader(document).value();
      BankCase item;
      item.name = json.get("name")->text;
      for (const auto& edge : json.get("edges")->items)
        item.edges.emplace_back(edge.items[0].text, edge.items[1].text,
                                static_cast<std::uint64_t>(edge.items[2].number));
      for (const auto& start : json.get("starts")->items)
        item.starts.emplace_back(start.items[0].text, static_cast<std::uint64_t>(start.items[1].number));
      for (const auto& end : json.get("ends")->items)
        item.ends.emplace_back(end.items[0].text, static_cast<std::uint64_t>(end.items[1].number));
      for (const auto& trace : json.get("traces")->items) {
        item.traces.emplace_back();
        for (const auto& activity : trace.items) item.traces.back().push_back(activity.text);
      }
      const auto& costs = *json.get("costs");
      for (const char* name : {"sync", "model", "log", "internal"}) {
        CostFunction function{false, {}};
        if (const auto* map = costs.get(name)) {
          function.supplied = true;
          for (const auto& [activity, cost] : map->fields) function.costs[activity] = cost.number;
        }
        item.costs.push_back(std::move(function));
      }
      item.log = outcome(*json.get("log"));
      item.frame = outcome(*json.get("frame"));
      out.push_back(std::move(item));
    }
    return out;
  }();
  return cases;
}

bool same(double actual, double expected) {
  if (std::isnan(expected)) return std::isnan(actual);
  return std::abs(actual - expected) <= 1e-9 * std::max(1.0, std::abs(expected));
}

EventLog make_log(const std::vector<std::vector<std::string>>& traces, bool drop_empty) {
  EventLog log;
  std::size_t order = 0;
  for (std::size_t index = 0; index < traces.size(); ++index) {
    if (drop_empty && traces[index].empty()) continue;
    Trace trace;
    trace.case_id = std::to_string(index);
    trace.case_id_state = FirstClassAttributeState::kPresent;
    for (const auto& activity : traces[index]) {
      Event event;
      event.case_id = trace.case_id;
      event.activity = activity;
      event.activity_state = FirstClassAttributeState::kPresent;
      event.input_order = order++;
      trace.events.push_back(std::move(event));
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

// Empty when native agrees, otherwise a description.
std::string difference(const BankCase& item, const Outcome& expected, bool frame) {
  DfgFrequency dfg;
  for (const auto& [from, to, count] : item.edges) dfg[{from, to}] = count;
  const std::map<std::string, std::uint64_t> starts(item.starts.begin(), item.starts.end());
  const std::map<std::string, std::uint64_t> ends(item.ends.begin(), item.ends.end());
  DfgAlignmentOptions options;
  const auto apply = [](const CostFunction& source, std::map<std::string, double>& target, bool& supplied) {
    if (!source.supplied) return;
    target = source.costs;
    supplied = true;
  };
  apply(item.costs[0], options.sync_cost, options.sync_cost_supplied);
  apply(item.costs[1], options.model_move_cost, options.model_move_cost_supplied);
  apply(item.costs[2], options.log_move_cost, options.log_move_cost_supplied);
  apply(item.costs[3], options.internal_log_move_cost, options.internal_log_move_cost_supplied);
  // Enumerate every order of the two routing names relative to the activity
  // names (all single letters a-g here) and to each other: a UUID starts with
  // a digit or one of a-f.
  const std::vector<std::string> classes = {"0", "a", "b", "c", "d", "e", "f"};
  std::vector<std::pair<std::string, std::string>> orders;
  for (const auto& source : classes)
    for (const auto& sink : classes) {
      if (source != sink) orders.push_back({source + "0", sink + "0"});
      else {
        orders.push_back({source + "0", sink + "1"});
        orders.push_back({source + "1", sink + "0"});
      }
    }
  const auto log = make_log(item.traces, frame);
  std::vector<std::vector<Moves>> alignments(expected.rows.size());
  std::vector<std::vector<std::size_t>> visited(expected.rows.size()), closed(expected.rows.size());
  std::string detail;
  for (const auto& order : orders) {
    options.routing_names = order;
    DfgAlignmentLogResult actual;
    try {
      actual = align_log_on_dfg(log, dfg, starts, ends, options);
    } catch (const std::exception& error) {
      if (expected.state == kError) continue;
      return std::string("native threw ") + error.what();
    }
    if (expected.state == kError) return "reference raised " + expected.error;
    if (actual.alignments.size() != expected.rows.size()) return "row count differs";
    for (std::size_t index = 0; index < expected.rows.size(); ++index) {
      const auto& row = expected.rows[index];
      const auto& result = actual.alignments[index];
      if (!result.computed) return "row " + std::to_string(index) + " not computed";
      if (!same(result.cost, row.cost) || !same(result.internal_cost, row.internal_cost) ||
          !same(result.fitness, row.fitness) || !same(result.best_worst_cost, row.bwc))
        return "row " + std::to_string(index) + " scores";
      Moves moves;
      for (const auto& move : result.alignment) moves.push_back({move.log, move.model});
      alignments[index].push_back(std::move(moves));
      visited[index].push_back(result.visited_states);
      closed[index].push_back(result.closed);
    }
  }
  // Every public outcome must be one that some routing-name order produces.
  const auto contains = [](const auto& values, const auto& value) {
    return std::find(values.begin(), values.end(), value) != values.end();
  };
  for (std::size_t index = 0; index < expected.rows.size(); ++index) {
    const auto& row = expected.rows[index];
    for (const auto& alignment : row.alignments)
      if (!contains(alignments[index], alignment)) detail += "row " + std::to_string(index) + " alignment; ";
    for (const auto value : row.visited)
      if (!contains(visited[index], value)) detail += "row " + std::to_string(index) + " visited states; ";
    for (const auto value : row.closed)
      if (!contains(closed[index], value)) detail += "row " + std::to_string(index) + " closed states; ";
  }
  return detail;
}

}  // namespace

int main() {
  int failures = 0;
  std::size_t compared = 0, rows = 0, errors = 0, skipped = 0, tied = 0;
  for (const auto& item : bank()) {
    for (const bool frame : {false, true}) {
      const auto& expected = frame ? item.frame : item.log;
      if (expected.state == kUnstable) { ++skipped; continue; }
      ++compared;
      if (expected.state == kError) ++errors;
      rows += expected.rows.size();
      for (const auto& row : expected.rows)
        if (row.alignments.size() > 1 || row.visited.size() > 1 || row.closed.size() > 1) ++tied;
      const auto detail = difference(item, expected, frame);
      if (!detail.empty()) {
        ++failures;
        std::fprintf(stderr, "%s %s: %s\n", item.name.c_str(), frame ? "frame" : "log", detail.c_str());
      }
    }
  }
  std::printf("%zu calls compared (%zu raising, %zu rows, %zu rows with seed-dependent ties), "
              "%zu skipped as unstable\n", compared, errors, rows, tied, skipped);
  if (compared < 2900 || rows < 6000) {
    ++failures;
    std::fprintf(stderr, "bank coverage below the pinned floor\n");
  }
  return failures == 0 ? 0 : 1;
}
