// Keep assertions live in Release builds.
#undef NDEBUG

#include "minera/trie.hpp"
#include "minera/json_export.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace minera;

Event event(std::string activity) {
  Event result;
  result.activity = std::move(activity);
  return result;
}

Event event_with_attribute(std::string activity, std::string key,
                           std::string value) {
  auto result = event(std::move(activity));
  result.attributes.push_back(
      {std::move(key), std::move(value), AttributeKind::kString});
  return result;
}

Trace trace(std::initializer_list<Event> events) {
  Trace result;
  result.events.assign(events);
  return result;
}

EventLog log(std::initializer_list<Trace> traces) {
  return EventLog{std::vector<Trace>(traces)};
}

void shared_prefixes_and_terminals() {
  const auto model = discover_trie(log({trace({event("a"), event("b")} ),
                                         trace({event("a"), event("c")} ),
                                         trace({event("a")} )}));
  assert(model.root.label.empty());
  assert(model.root.depth == 0);
  assert(!model.root.final);
  assert(model.root.children.size() == 1);
  assert(model.input_traces == 3);
  assert(model.distinct_variants == 3);
  assert(model.node_count == 4);
  assert(!model.truncated);
  const auto& a = model.root.children.front();
  assert(a.label == "a");
  assert(a.depth == 1);
  assert(a.final);  // terminal prefix and extension coexist
  assert(a.children.size() == 2);
  assert(a.children[0].label == "b");
  assert(a.children[1].label == "c");
  assert(a.children[0].final && a.children[1].final);
}

void duplicate_variants_and_ordering() {
  const auto model = discover_trie(log({trace({event("z")} ),
                                         trace({event("a")} ),
                                         trace({event("z")} ),
                                         trace({event("m")} )}));
  assert(model.root.children.size() == 3);
  assert(model.root.children[0].label == "a");
  assert(model.root.children[1].label == "m");
  assert(model.root.children[2].label == "z");
}

void truncation_and_custom_attribute() {
  TrieOptions options;
  options.max_path_length = 2;
  auto model = discover_trie(log({trace({event("a"), event("b"), event("c")})}),
                             options);
  assert(model.root.children[0].children[0].label == "b");
  assert(model.root.children[0].children[0].final);
  assert(model.root.children[0].children[0].children.empty());
  assert(model.truncated);

  options = {};
  options.activity_key = "phase";
  model = discover_trie(log({trace({event_with_attribute("ignored", "phase", "x"),
                                    event_with_attribute("ignored", "phase", "y")})}),
                        options);
  assert(model.root.children[0].label == "x");
  assert(model.root.children[0].children[0].label == "y");

  options.activity_key.clear();
  auto empty_key_event = event("fallback");
  empty_key_event.attributes.push_back(
      {"", "explicit-empty-key", AttributeKind::kString});
  model = discover_trie(log({trace({empty_key_event})}), options);
  assert(model.root.children[0].label == "explicit-empty-key");
}

void empty_and_invalid_options() {
  const auto model = discover_trie(log({trace({}), trace({})}));
  assert(model.root.children.empty());
  assert(!model.root.final);

  TrieOptions options;
  options.max_path_length = 0;
  bool rejected = false;
  try {
    (void)discover_trie(log({trace({event("a")})}), options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void json_shape() {
  const auto model = discover_trie(log({trace({event("A"), event("B")})}));
  const auto json = trie_json(model);
  assert(json.find("\"family\":\"LOG_TO_TRIE\"") != std::string::npos);
  assert(json.find("\"label\":\"A\"") != std::string::npos);
  assert(json.find("\"depth\":2") != std::string::npos);
  assert(json.find("\"input_traces\":1") != std::string::npos);
  assert(json.find("\"distinct_variants\":1") != std::string::npos);
  assert(json.find("\"node_count\":3") != std::string::npos);
  assert(json.find("\"truncated\":false") != std::string::npos);
}

}  // namespace

int main() {
  shared_prefixes_and_terminals();
  duplicate_variants_and_ordering();
  truncation_and_custom_attribute();
  empty_and_invalid_options();
  json_shape();
}
