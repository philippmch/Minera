// Alignment log enrichment against the public reference
// (docs/alignment-log-enrichment-bank.json.gz): seeded logs enriched with
// synthesized and real alignments in the transition-description move shape,
// under the default and varied target attributes, with and without
// ENABLE_DEEPCOPY. The raised/not-raised outcome, the returned log and the
// input log after the call are bound exactly; IndexError is std::out_of_range
// and TypeError std::invalid_argument.
#include "minera/alignment_log_enrichment.hpp"
#include "minera/event_log.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace minera;

using EventView = std::map<std::string, std::string>;
using LogView = std::vector<std::vector<EventView>>;

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

struct Outcome {
  std::string error;  // empty when the call returned
  std::optional<LogView> returned;
  LogView input_after;
};

struct BankCase {
  std::string name;
  EventLog log;
  std::vector<std::optional<PetriTraceAlignment>> aligned;
  std::vector<std::string> transition_names;
  std::optional<std::string> target;
  Outcome deepcopy, in_place;
};

LogView log_view(const Json& value) {
  LogView out;
  for (const auto& trace : value.items) {
    auto& events = out.emplace_back();
    for (const auto& event : trace.items) {
      auto& view = events.emplace_back();
      for (const auto& [key, field] : event.fields) view.emplace(key, field.text);
    }
  }
  return out;
}

LogView log_view(const EventLog& log) {
  LogView out;
  for (const auto& trace : log.traces) {
    auto& events = out.emplace_back();
    for (const auto& event : trace.events) {
      auto& view = events.emplace_back();
      if (event.activity_state == FirstClassAttributeState::kPresent) view["concept:name"] = event.activity;
      if (event.resource_state == FirstClassAttributeState::kPresent) view["org:resource"] = event.resource;
      if (event.lifecycle_state == FirstClassAttributeState::kPresent)
        view["lifecycle:transition"] = event.lifecycle;
      for (const auto& attribute : event.attributes) view[attribute.key] = attribute.value;
    }
  }
  return out;
}

EventLog event_log(const Json& traces) {
  EventLog log;
  std::size_t order = 0;
  for (std::size_t index = 0; index < traces.items.size(); ++index) {
    auto& trace = log.traces.emplace_back();
    trace.case_id = std::to_string(index);
    trace.case_id_state = FirstClassAttributeState::kPresent;
    for (const auto& item : traces.items[index].items) {
      Event event;
      event.case_id = trace.case_id;
      event.input_order = order++;
      for (const auto& [key, field] : item.fields) {
        if (key == "concept:name") {
          event.activity = field.text;
          event.activity_state = FirstClassAttributeState::kPresent;
        } else if (key == "org:resource") {
          event.resource = field.text;
          event.resource_state = FirstClassAttributeState::kPresent;
        } else if (key == "lifecycle:transition") {
          event.lifecycle = field.text;
          event.lifecycle_state = FirstClassAttributeState::kPresent;
        } else {
          EventAttribute attribute;
          attribute.key = key;
          attribute.value = field.text;
          event.attributes.push_back(std::move(attribute));
        }
      }
      trace.events.push_back(std::move(event));
    }
  }
  return log;
}

Outcome outcome(const Json& value) {
  Outcome out;
  if (value.get("error")->kind == Json::kString) out.error = value.get("error")->text;
  if (value.get("returned")->kind == Json::kArray) out.returned = log_view(*value.get("returned"));
  out.input_after = log_view(*value.get("input_after"));
  return out;
}

// A public move ((log name, model name), (log label, model label)): the model
// name, when not ">>", is the transition; names get native indices in order of
// first appearance.
std::vector<std::optional<PetriTraceAlignment>> alignments(const Json& value,
                                                           std::vector<std::string>& names) {
  std::map<std::string, std::size_t> index;
  std::vector<std::optional<PetriTraceAlignment>> out;
  for (const auto& entry : value.items) {
    if (entry.kind == Json::kNull) { out.emplace_back(); continue; }
    PetriTraceAlignment aligned;
    aligned.computed = true;
    for (const auto& move : entry.get("alignment")->items) {
      PetriTraceAlignmentMove native;
      native.log = move.items[1].items[0].text;
      const auto& model = move.items[1].items[1];
      if (model.kind == Json::kString) native.model = model.text;
      const auto& transition = move.items[0].items[1].text;
      if (transition != ">>") {
        const auto [found, inserted] = index.emplace(transition, names.size());
        if (inserted) names.push_back(transition);
        native.transition = found->second;
      }
      aligned.alignment.push_back(std::move(native));
    }
    out.push_back(std::move(aligned));
  }
  return out;
}

const std::vector<BankCase>& bank() {
  static const std::vector<BankCase> cases = [] {
    static const char* const documents[] = {
#include "alignment_log_enrichment_cases.inc"
    };
    std::vector<BankCase> out;
    for (const char* document : documents) {
      const auto json = JsonReader(document).value();
      BankCase item;
      item.name = json.get("name")->text;
      item.log = event_log(*json.get("traces"));
      item.aligned = alignments(*json.get("aligned"), item.transition_names);
      if (json.get("target")->text != "@@default") item.target = json.get("target")->text;
      item.deepcopy = outcome(*json.get("deepcopy"));
      item.in_place = outcome(*json.get("in_place"));
      out.push_back(std::move(item));
    }
    return out;
  }();
  return cases;
}

std::string error_name(const std::exception& error) {
  if (dynamic_cast<const std::out_of_range*>(&error)) return "IndexError";
  if (dynamic_cast<const std::invalid_argument*>(&error)) return "TypeError";
  return std::string("unexpected: ") + error.what();
}

std::string difference(const BankCase& item, bool deepcopy) {
  const auto& expected = deepcopy ? item.deepcopy : item.in_place;
  AlignmentLogEnrichmentOptions options;
  if (item.target) options.target_attribute = *item.target;
  auto input = item.log;
  std::optional<LogView> returned;
  std::string error;
  try {
    if (deepcopy) {
      returned = log_view(enrich_log_with_alignments(input, item.aligned, item.transition_names, options));
    } else {
      enrich_log_with_alignments_in_place(input, item.aligned, item.transition_names, options);
      returned = log_view(input);
    }
  } catch (const std::exception& exception) {
    error = error_name(exception);
  }
  if (error != expected.error) return "outcome " + (error.empty() ? "returned" : error);
  if (returned != expected.returned) return "returned log";
  if (log_view(input) != expected.input_after) return "input log after the call";
  return {};
}

}  // namespace

int main() {
  int failures = 0;
  std::size_t compared = 0, raising = 0;
  for (const auto& item : bank()) {
    for (const bool deepcopy : {true, false}) {
      ++compared;
      if (!(deepcopy ? item.deepcopy : item.in_place).error.empty()) ++raising;
      const auto detail = difference(item, deepcopy);
      if (!detail.empty()) {
        ++failures;
        std::fprintf(stderr, "%s %s: %s\n", item.name.c_str(), deepcopy ? "deepcopy" : "in place",
                     detail.c_str());
      }
    }
  }
  std::printf("%zu calls compared (%zu raising)\n", compared, raising);
  if (compared < 3000 || raising < 300) {
    ++failures;
    std::fprintf(stderr, "bank coverage below the pinned floor\n");
  }
  return failures == 0 ? 0 : 1;
}
