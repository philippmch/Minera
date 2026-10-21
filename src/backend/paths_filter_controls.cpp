#include "minera/paths_filter_controls.hpp"
#include "minera/literal_controls_json.hpp"
#include <stdexcept>
namespace minera {
EventAttribute parse_paths_filter_pair_json(std::string_view input) {
  const auto values = parse_literal_string_vector_json(input);
  if (values.size() != 2) throw std::invalid_argument("a path must be an array of exactly two strings");
  EventAttribute result; result.container = AttributeContainerKind::kTuple;
  result.children = {{"0", values[0]}, {"1", values[1]}}; return result;
}
EventAttribute parse_paths_filter_pairs_json(std::string_view input) {
  std::size_t at = 0;
  const auto space = [&] {while (at < input.size() && (input[at] == ' ' || input[at] == '\t' || input[at] == '\r' || input[at] == '\n')) ++at;};
  const auto require = [&](char c) {space(); if (at == input.size() || input[at++] != c) throw std::invalid_argument("paths_json must be an array of two-string arrays");};
  EventAttribute result; result.container = AttributeContainerKind::kList;
  require('['); space();
  if (at < input.size() && input[at] == ']') ++at;
  else while (true) {
    space(); const auto begin = at; require('[');
    std::size_t depth = 1; bool quoted = false, escaped = false;
    while (at < input.size() && depth) {
      const char c = input[at++];
      if (quoted) {if (escaped) escaped = false; else if (c == '\\') escaped = true; else if (c == '"') quoted = false;}
      else if (c == '"') quoted = true; else if (c == '[') ++depth; else if (c == ']') --depth;
    }
    if (depth) throw std::invalid_argument("paths_json has an incomplete path pair");
    auto pair = parse_paths_filter_pair_json(input.substr(begin, at - begin)); pair.key = std::to_string(result.children.size()); result.children.push_back(std::move(pair));
    space(); if (at < input.size() && input[at] == ']') {++at; break;} require(',');
  }
  space(); if (at != input.size()) throw std::invalid_argument("paths_json has trailing content");
  return result;
}
}
