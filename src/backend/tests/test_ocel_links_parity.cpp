// Link analysis and timestamp interleavings against the public reference
// (docs/ocel-links-bank.json.gz). Link analysis: seeded event tables through
// the public link_analysis.algorithm.apply with the default columns and with
// doc->ref links under every LOOK_FORWARD/KEEP_FIRST_OCCURRENCE/PROPAGATE
// combination, bound by the generated index pairs in order. Interleavings:
// seeded related left/right logs through the public interleavings
// algorithm.apply, bound by every row's activities, timestamps, indices,
// direction and elapsed seconds in order; naive timestamps raise.
#include "minera/link_analysis.hpp"
#include "minera/timestamp_interleavings.hpp"

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
  Json data;
  std::vector<Json> expected;  // admissible outcomes
};

const std::vector<BankCase>& bank() {
  static const std::vector<BankCase> cases = [] {
    static const char* const documents[] = {
#include "ocel_links_cases.inc"
    };
    std::vector<BankCase> out;
    for (const char* document : documents) {
      const auto json = JsonReader(document).value();
      BankCase item;
      item.name = json.get("name")->text;
      item.call = json.get("call")->text;
      item.data = *json.get("data");
      const auto& value = *json.get("expected");
      if (const auto* observed = value.get("observed")) item.expected = observed->items;
      else item.expected.push_back(value);
      out.push_back(std::move(item));
    }
    return out;
  }();
  return cases;
}

std::string stamp(int minutes, bool aware) {
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "2024-01-01T%02d:%02d:00%s", minutes / 60, minutes % 60, aware ? "Z" : "");
  return buffer;
}

// Link analysis reads only column values, so each table row becomes its own
// one-event trace and keeps its row position.
EventLog link_log(const Json& rows) {
  EventLog log;
  std::size_t order = 0;
  for (const auto& row : rows.items) {
    auto& trace = log.traces.emplace_back();
    trace.case_id = row.items[0].text;
    Event event;
    event.case_id = trace.case_id;
    event.activity = row.items[1].text;
    event.timestamp = stamp(static_cast<int>(row.items[2].number), false);
    event.input_order = order++;
    event.attributes.push_back({"doc", row.items[3].text});
    event.attributes.push_back({"ref", row.items[4].text});
    trace.events.push_back(std::move(event));
  }
  return log;
}

// Cases in first-appearance order, events in row order, row positions kept.
EventLog interleaving_log(const Json& rows, bool aware) {
  EventLog log;
  std::map<std::string, std::size_t> traces;
  std::size_t order = 0;
  for (const auto& row : rows.items) {
    const auto& id = row.items[0].text;
    if (!traces.contains(id)) {
      traces[id] = log.traces.size();
      log.traces.emplace_back().case_id = id;
    }
    Event event;
    event.case_id = id;
    event.activity = row.items[1].text;
    event.timestamp = stamp(static_cast<int>(row.items[2].number), aware);
    event.input_order = order++;
    log.traces[traces[id]].events.push_back(std::move(event));
  }
  return log;
}

Json link_analysis_case(const BankCase& item) {
  LinkAnalysisOptions options;
  if (item.call != "default") {
    const auto flags = item.call.substr(item.call.size() - 3);
    options.out_column = "doc";
    options.in_column = "ref";
    options.look_forward = flags[0] == '1';
    options.keep_first_occurrence = flags[1] == '1';
    options.propagate = flags[2] == '1';
  }
  const auto result = discover_link_analysis(link_log(*item.data.get("rows")), options);
  auto pairs = array();
  for (const auto& link : result.links)
    pairs.items.push_back(array({number(static_cast<double>(link.source.index)),
                                 number(static_cast<double>(link.target.index))}));
  return pairs;
}

Json interleavings_case(const BankCase& item) {
  const bool aware = item.data.get("aware")->boolean;
  std::vector<CaseRelation> relations;
  for (const auto& relation : item.data.get("relations")->items)
    relations.push_back({relation.items[0].text, relation.items[1].text});
  const auto result = discover_timestamp_interleavings(interleaving_log(*item.data.get("left"), aware),
                                                       interleaving_log(*item.data.get("right"), aware), relations);
  auto rows = array();
  for (const auto& row : result.interleavings)
    rows.items.push_back(array({text(row.source_activity), text(row.target_activity), text(row.source_timestamp),
                                text(row.target_timestamp), number(static_cast<double>(row.left_index)),
                                number(static_cast<double>(row.right_index)),
                                text(row.direction == TimestampInterleavingDirection::kLeftToRight ? "LR" : "RL"),
                                number(row.timestamp_diff_seconds)}));
  return rows;
}

// Link rows come grouped by ascending source index; within a source the
// reference lists targets in its hash-set iteration order, which is not bound:
// targets are compared sorted.
Json sorted_within_sources(const Json& pairs) {
  auto out = pairs;
  std::stable_sort(out.items.begin(), out.items.end(), [](const Json& a, const Json& b) {
    return a.items[0].number < b.items[0].number ||
           (a.items[0].number == b.items[0].number && a.items[1].number < b.items[1].number);
  });
  // Sources must already be ascending in the original order.
  for (std::size_t i = 1; i < pairs.items.size(); ++i)
    if (pairs.items[i].items[0].number < pairs.items[i - 1].items[0].number) return pairs;
  return out;
}

// Reference timestamps print as "YYYY-MM-DD HH:MM:SS[.f]+00:00"; native keeps
// ISO text with a Z designator. Both name the same UTC instant.
Json iso_rows(const Json& rows) {
  auto out = rows;
  for (auto& row : out.items)
    for (const std::size_t column : {2u, 3u}) {
      auto& value = row.items[column].text;
      if (value.size() > 10 && value[10] == ' ') value[10] = 'T';
      if (value.ends_with("+00:00")) value.replace(value.size() - 6, 6, "Z");
    }
  return out;
}

// The bound projection of a reference outcome: link pairs or interleaving rows.
Json projection(const Json& expected) {
  Json out;
  out.kind = Json::kObject;
  if (expected.get("error")) out.fields["error"] = text("raised");
  else if (const auto* pairs = expected.get("pairs")) out.fields["value"] = sorted_within_sources(*pairs);
  else out.fields["value"] = iso_rows(*expected.get("rows"));
  return out;
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
  const auto rendered = render(actual);
  std::vector<std::string> admissible;
  for (const auto& expected : item.expected) admissible.push_back(render(projection(expected)));
  if (std::find(admissible.begin(), admissible.end(), rendered) != admissible.end()) return {};
  return "native " + rendered.substr(0, 900) + (message.empty() ? "" : " (" + message + ")") + " reference " +
         admissible.front().substr(0, 900);
}

}  // namespace

int main() {
  int failures = 0;
  std::size_t compared = 0, raising = 0;
  for (const auto& item : bank()) {
    ++compared;
    if (item.expected.front().get("error")) ++raising;
    const bool links = item.name.starts_with("links-");
    const auto detail = difference(item, [&] {
      return links ? link_analysis_case(item) : interleavings_case(item);
    });
    if (!detail.empty()) {
      ++failures;
      std::fprintf(stderr, "%s %s: %s\n", item.name.c_str(), item.call.c_str(), detail.c_str());
    }
  }
  std::printf("%zu calls compared (%zu raising)\n", compared, raising);
  if (compared < 1000 || raising < 10) {
    ++failures;
    std::fprintf(stderr, "bank coverage below the pinned floor\n");
  }
  return failures == 0 ? 0 : 1;
}
