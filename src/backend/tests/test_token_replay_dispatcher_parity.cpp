// Token replay dispatcher against the public reference
// (docs/token-replay-dispatcher-bank.json.gz): the frozen conformance nets
// with seeded logs, replayed through the public tokenreplay.algorithm.apply
// under the default, TOKEN_REPLAY and BACKWARDS on the EventLog and on the
// equivalent DataFrame (cases interleaved under unsorted ids), plus the public
// diagnostics dataframe of the DataFrame form. Every row, raising call and
// diagnostics record is bound to an output the reference returned under
// sixteen hash seeds.
#include "minera/conformance.hpp"
#include "minera/pnml.hpp"

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
  std::vector<std::vector<std::string>> traces;
  std::vector<std::string> case_ids;
  std::vector<std::pair<std::string, std::string>> frame_rows;
  std::map<std::string, std::vector<std::string>> expected;  // call -> admissible renders
};

void normalize(Json& value) {
  if (!value.get("rows")) return;
  for (auto& row : value.fields["rows"].items) {
    if (row.get("enabled")) canonical_sort(row.fields["enabled"]);
    if (row.get("reached")) canonical_sort(row.fields["reached"]);
  }
}

const std::vector<BankCase>& bank() {
  static const std::vector<BankCase> cases = [] {
    static const char* const documents[] = {
#include "token_replay_dispatcher_cases.inc"
    };
    std::vector<BankCase> out;
    for (const char* document : documents) {
      const auto json = JsonReader(document).value();
      BankCase item;
      item.name = json.get("name")->text;
      item.pnml = json.get("pnml")->text;
      for (const auto& trace : json.get("traces")->items) {
        auto& activities = item.traces.emplace_back();
        for (const auto& activity : trace.items) activities.push_back(activity.text);
      }
      for (const auto& id : json.get("case_ids")->items) item.case_ids.push_back(id.text);
      for (const auto& row : json.get("frame_rows")->items)
        item.frame_rows.emplace_back(row.items[0].text, row.items[1].text);
      for (const auto& [call, value] : json.get("calls")->fields) {
        std::vector<Json> choices;
        if (const auto* observed = value.get("observed")) choices = observed->items;
        else choices.push_back(value);
        for (auto& choice : choices) {
          normalize(choice);
          item.expected[call].push_back(render(choice));
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
  for (std::size_t index = 0; index < item.traces.size(); ++index) {
    auto& trace = log.traces.emplace_back();
    trace.case_id = item.case_ids[index];
    trace.case_id_state = FirstClassAttributeState::kPresent;
    for (const auto& activity : item.traces[index]) {
      Event event;
      event.case_id = trace.case_id;
      event.activity = activity;
      event.activity_state = FirstClassAttributeState::kPresent;
      event.input_order = order++;
      trace.events.push_back(std::move(event));
    }
  }
  return log;
}

EventAttribute cell(const std::string& key, const std::string& value, AttributeKind kind) {
  EventAttribute out;
  out.key = key;
  out.value = value;
  out.kind = kind;
  if (kind == AttributeKind::kDate) out.typed_value = NativeDate{value};
  return out;
}

EventTable event_table(const BankCase& item) {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  for (std::size_t index = 0; index < item.frame_rows.size(); ++index) {
    const auto& [case_id, activity] = item.frame_rows[index];
    char stamp[64];
    std::snprintf(stamp, sizeof stamp, "2020-01-01T%02zu:%02zu:%02zu", index / 3600, index / 60 % 60, index % 60);
    table.rows.push_back({cell("case:concept:name", case_id, AttributeKind::kString),
                          cell("concept:name", activity, AttributeKind::kString),
                          cell("time:timestamp", stamp, AttributeKind::kDate)});
  }
  return table;
}

// Backwards rows have no reached marking and add replay_interrupted and
// activated_transitions_labels.
Json rows_json(const PnmlDocument& document, const std::vector<TokenReplayTraceDiagnostics>& traces,
               bool backwards) {
  const auto& ids = document.transition_ids;
  auto rows = array();
  for (const auto& row : traces) {
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
    if (!backwards) {
      auto reached = array();
      for (std::size_t p = 0; p < row.reached_marking.size(); ++p) {
        const bool key = row.reached_marking_keys.empty() ? row.reached_marking[p] != TokenInteger(0)
                                                          : row.reached_marking_keys[p];
        if (key) reached.items.push_back(array({text(document.place_ids.at(p)), integer(row.reached_marking[p])}));
      }
      item.fields["reached"] = reached;
    }
    item.fields["counts"] = array({integer(row.missing), integer(row.consumed), integer(row.remaining),
                                   integer(row.produced)});
    item.fields["keys"] = backwards
        ? strings({"activated_transitions", "activated_transitions_labels", "consumed_tokens",
                   "enabled_transitions_in_marking", "missing_tokens", "produced_tokens", "remaining_tokens",
                   "replay_interrupted", "trace_fitness", "trace_is_fit", "transitions_with_problems"})
        : strings({"activated_transitions", "consumed_tokens", "enabled_transitions_in_marking",
                   "missing_tokens", "produced_tokens", "reached_marking", "remaining_tokens",
                   "trace_fitness", "trace_is_fit", "transitions_with_problems"});
    rows.items.push_back(std::move(item));
  }
  return rows;
}

Json cell_json(const std::optional<EventAttribute>& value) {
  if (!value || value->is_null) return Json{};
  if (const auto* flag = std::get_if<bool>(&value->typed_value)) return boolean(*flag);
  if (const auto* whole = std::get_if<std::int64_t>(&value->typed_value)) return number(static_cast<double>(*whole));
  if (const auto* real = std::get_if<double>(&value->typed_value)) return number(*real);
  if (value->kind == AttributeKind::kBoolean) return boolean(value->value == "True" || value->value == "true");
  if (value->kind == AttributeKind::kInt || value->kind == AttributeKind::kFloat)
    return number(std::strtod(value->value.c_str(), nullptr));
  return text(value->value);
}

std::string difference(const BankCase& item, const std::string& call) {
  const auto slash = call.find('/');
  const auto variant = call.substr(0, slash);
  const bool frame = call.substr(slash + 1) == "frame";
  const auto document = parse_pnml_document(item.pnml);
  TokenReplayOptions options;
  options.variant = variant == "BACKWARDS" ? TokenReplayVariant::kBackwards : TokenReplayVariant::kForward;
  options.transition_names = document.transition_ids;
  options.place_names = document.place_ids;
  options.exact_initial_marking = document.initial_marking;
  options.exact_final_marking = document.final_marking;
  options.place_outgoing_transition_order = pnml_place_outgoing_transition_order(document);
  Json actual;
  actual.kind = Json::kObject;
  std::string message;
  try {
    if (frame) {
      const auto table = event_table(item);
      const auto traces = token_replay_dataframe_apply(table, document.net, options);
      actual.fields["rows"] = rows_json(document, traces, variant == "BACKWARDS");
      Json diagnostics;
      diagnostics.kind = Json::kObject;
      try {
        const auto records = token_replay_diagnostics_dataframe(table, traces, options);
        diagnostics.fields["columns"] = strings(records.columns);
        auto rows = array();
        for (const auto& row : records.rows) {
          auto values = array();
          for (const auto& value : row) values.items.push_back(cell_json(value));
          rows.items.push_back(std::move(values));
        }
        diagnostics.fields["rows"] = rows;
      } catch (const std::exception&) {
        diagnostics.fields["error"] = text("raised");
      }
      actual.fields["diagnostics"] = diagnostics;
    } else {
      actual.fields["rows"] = rows_json(document, replay_token_traces(event_log(item), document.net, options),
                                          variant == "BACKWARDS");
    }
  } catch (const std::exception& error) {
    actual = Json{};
    actual.kind = Json::kObject;
    actual.fields["error"] = text("raised");
    message = error.what();
  }
  normalize(actual);
  // The error class name is not bound, only that the call raised.
  std::vector<std::string> admissible;
  for (auto render_text : item.expected.at(call)) {
    auto value = JsonReader(render_text.c_str()).value();
    if (value.get("error")) value.fields["error"] = text("raised");
    if (const auto* diagnostics = value.get("diagnostics"); diagnostics && diagnostics->get("error"))
      value.fields["diagnostics"].fields["error"] = text("raised");
    admissible.push_back(render(value));
  }
  const auto rendered = render(actual);
  if (std::find(admissible.begin(), admissible.end(), rendered) != admissible.end()) return {};
  return "native " + rendered.substr(0, 700) + (message.empty() ? "" : " (" + message + ")") +
         " reference " + admissible.front().substr(0, 700);
}

}  // namespace

int main() {
  int failures = 0;
  std::size_t compared = 0, raising = 0;
  for (const auto& item : bank()) {
    for (const auto& [call, admissible] : item.expected) {
      ++compared;
      if (admissible.front().find("\"error\"") == 1) ++raising;
      const auto detail = difference(item, call);
      if (!detail.empty()) {
        ++failures;
        std::fprintf(stderr, "%s %s: %s\n", item.name.c_str(), call.c_str(), detail.c_str());
      }
    }
  }
  std::printf("%zu calls compared (%zu raising)\n", compared, raising);
  if (compared < 1200 || raising < 300) {
    ++failures;
    std::fprintf(stderr, "bank coverage below the pinned floor\n");
  }
  return failures == 0 ? 0 : 1;
}
