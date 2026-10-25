// algo.evaluation.precision.utils against the public seeded bank
// (docs/precision-utils-bank.txt.gz, scripts/capture-precision-utils-bank.py):
// log prefixes (EventLog and DataFrame inputs), fake logs and the prefix
// search on synchronous products compare as canonical JSON.
#include "minera/precision_utils.hpp"

#include "../gzip_codec.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace minera;

struct Json {
  enum Kind { kNull, kBool, kNumber, kString, kArray, kObject } kind = kNull;
  double number = 0;
  bool boolean = false;
  std::string text;
  std::vector<Json> items;
  std::map<std::string, Json> fields;
  [[nodiscard]] const Json& at(const std::string& key) const {
    const auto found = fields.find(key);
    if (found == fields.end()) throw std::runtime_error("missing field " + key);
    return found->second;
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

// Canonical text (key-sorted objects) so native output built in the same
// shape compares as text.
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
Json array(std::vector<Json> items = {}) {
  Json out;
  out.kind = Json::kArray;
  out.items = std::move(items);
  return out;
}
Json object(std::map<std::string, Json> fields = {}) {
  Json out;
  out.kind = Json::kObject;
  out.fields = std::move(fields);
  return out;
}

struct BankCase {
  std::string name, kind;
  Json input, output;
};

std::vector<BankCase> read_bank(const char* path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) throw std::runtime_error(std::string("cannot read ") + path);
  const std::string bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
  std::istringstream lines(detail::gzip_decompress(bytes, std::size_t{1} << 31));
  std::vector<BankCase> cases;
  std::string line;
  while (std::getline(lines, line)) {
    if (line.size() < 2) continue;
    const auto rest = line.substr(2);
    if (line[0] == 'C') {
      cases.emplace_back();
      std::istringstream words(rest);
      words >> cases.back().name >> cases.back().kind;
    } else if (line[0] == 'I') {
      cases.back().input = JsonReader(rest.c_str()).value();
    } else if (line[0] == 'O') {
      cases.back().output = JsonReader(rest.c_str()).value();
    }
  }
  return cases;
}

template <typename Call>
std::string compare(const BankCase& item, Call call) {
  std::string actual;
  try {
    actual = render(call());
  } catch (const PrecisionUtilsError& error) {
    actual = render(object({{"error", array({text(error.type), text(error.what())})}}));
  }
  const auto expected = render(item.output);
  return actual == expected ? "" : "\n  expected " + expected + "\n  actual   " + actual;
}

std::size_t counts[3] = {};

Json prefixes_json(const PrecisionLogPrefixes& result) {
  auto prefixes = array(), totals = array();
  for (const auto& [prefix, next] : result.prefixes) {
    auto activities = array();
    for (const auto& activity : next) activities.items.push_back(activity ? text(*activity) : Json{});
    prefixes.items.push_back(array({text(prefix), activities}));
  }
  for (const auto& [prefix, count] : result.counts)
    totals.items.push_back(array({text(prefix), number(static_cast<double>(count))}));
  return object({{"prefixes", prefixes}, {"counts", totals}});
}

// get_log_prefixes on an EventLog or a DataFrame.
std::string check_prefixes(const BankCase& item) {
  ++counts[0];
  const auto& input = item.input;
  const auto key = input.at("key").text;
  return compare(item, [&] {
    if (input.at("kind").text == "log") {
      EventLog log;
      for (const auto& trace : input.at("traces").items) {
        auto& native = log.traces.emplace_back();
        for (const auto& activity : trace.items) {
          Event event;
          if (key == "concept:name") {
            event.activity = activity.text;
            event.activity_state = FirstClassAttributeState::kPresent;
          } else {
            EventAttribute attribute;
            attribute.key = key;
            attribute.value = activity.text;
            event.attributes.push_back(attribute);
          }
          native.events.push_back(std::move(event));
        }
      }
      return prefixes_json(precision_log_prefixes(log, key));
    }
    const bool numeric = input.at("ids").text == "int";
    EventTable table;
    table.columns = {"case:concept:name", key};
    table.dtypes = {numeric ? "int64" : "object", "object"};
    for (const auto& row : input.at("rows").items) {
      EventAttribute case_id, activity;
      case_id.key = "case:concept:name";
      case_id.value = row.items[0].text;
      case_id.kind = numeric ? AttributeKind::kInt : AttributeKind::kString;
      activity.key = key;
      activity.value = row.items[1].text;
      table.rows.push_back({case_id, activity});
    }
    return prefixes_json(precision_log_prefixes(table, key));
  });
}

// form_fake_log on prefix keys.
std::string check_fake(const BankCase& item) {
  ++counts[1];
  const auto key = item.input.at("key").text;
  std::vector<std::string> prefixes;
  for (const auto& prefix : item.input.at("keys").items) prefixes.push_back(prefix.text);
  return compare(item, [&] {
    const auto log = precision_fake_log(prefixes, key);
    auto traces = array(), attributes = array();
    for (const auto& trace : log.traces) {
      auto events = array();
      for (const auto& event : trace.events) {
        const auto& value = key == "concept:name" ? event.activity : event.attributes.at(0).value;
        events.items.push_back(object({{key, text(value)}}));
      }
      traces.items.push_back(events);
      attributes.items.push_back(object());
    }
    return object({{"traces", traces}, {"trace_attributes", attributes}});
  });
}

PrecisionMarking marking_of(const Json& value) {
  PrecisionMarking out;
  for (const auto& pair : value.items)
    out.push_back({static_cast<std::size_t>(pair.items[0].number), static_cast<std::size_t>(pair.items[1].number)});
  return out;
}

// __search on a synchronous product until the trace net's last place.
std::string check_search(const BankCase& item) {
  ++counts[2];
  const auto& net_json = item.input.at("net");
  PrecisionSyncNet net;
  net.places = net_json.at("places").items.size();
  for (const auto& transition : net_json.at("transitions").items) {
    PrecisionSyncTransition native;
    const auto& label = transition.at("label").items;
    if (label[0].kind == Json::kString) native.trace_label = label[0].text;
    if (label[1].kind == Json::kString) native.model_label = label[1].text;
    native.cost = transition.at("cost").number;
    native.in = marking_of(transition.at("in"));
    native.out = marking_of(transition.at("out"));
    net.transitions.push_back(std::move(native));
  }
  return compare(item, [&] {
    const auto found = precision_sync_search(net, marking_of(net_json.at("initial")), marking_of(net_json.at("stop")));
    if (!found) return object({{"markings", Json{}}});
    auto markings = array();
    for (const auto& marking : *found) {
      auto pairs = array();
      for (const auto& [place, tokens] : marking)
        pairs.items.push_back(array({number(static_cast<double>(place)), number(static_cast<double>(tokens))}));
      markings.items.push_back(pairs);
    }
    return object({{"markings", markings}});
  });
}

}  // namespace

int main(int argc, char** argv) try {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " BANK.txt.gz\n";
    return 2;
  }
  std::size_t failures = 0;
  for (const auto& item : read_bank(argv[1])) {
    const auto failure = item.kind == "prefixes" ? check_prefixes(item)
                         : item.kind == "fake"   ? check_fake(item)
                                                 : check_search(item);
    if (failure.empty()) continue;
    if (++failures <= 20) std::cerr << item.name << " (" << item.kind << "): " << failure << '\n';
  }
  std::cout << counts[0] << " prefix calls, " << counts[1] << " fake logs, " << counts[2] << " searches compared, "
            << failures << " failures\n";
  return failures || counts[0] + counts[1] + counts[2] < 3000 ? 1 : 0;
} catch (const std::exception& error) {
  std::cerr << "error: " << error.what() << "\n";
  return 1;
}
