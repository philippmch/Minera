#include "minera/alpha_result_json.hpp"

#include <sstream>
#include <stdexcept>
#include <string_view>

namespace minera {
namespace {
void text(std::ostream& out, std::string_view value) {
  constexpr char hex[] = "0123456789abcdef";
  out << '"';
  for (const unsigned char c : value) {
    if (c == '"' || c == '\\') out << '\\' << static_cast<char>(c);
    else if (c < 0x20) out << "\\u00" << hex[c >> 4] << hex[c & 15];
    else out << static_cast<char>(c);
  }
  out << '"';
}
void names(std::ostream& out, const std::vector<std::string>& values) {
  out << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) out << ',';
    text(out, values[i]);
  }
  out << ']';
}
void place_name(std::ostream& out, const AlphaPlaceName& name) {
  if (!name.boundary.empty()) {
    if (!name.before.empty() || !name.after.empty())
      throw std::invalid_argument("Alpha+ result: boundary name also has pair members");
    out << "{\"boundary\":";
    text(out, name.boundary);
  } else {
    out << "{\"before\":";
    names(out, name.before);
    out << ",\"after\":";
    names(out, name.after);
  }
  out << '}';
}
void detached(std::ostream& out, const std::vector<AlphaDetachedMarking>& markings) {
  out << '[';
  for (std::size_t i = 0; i < markings.size(); ++i) {
    if (!markings[i].tokens)
      throw std::invalid_argument("Alpha+ result: detached marking must contain tokens");
    if (i) out << ',';
    out << "{\"name\":";
    place_name(out, markings[i].name);
    out << ",\"tokens\":" << markings[i].tokens << '}';
  }
  out << ']';
}
void arcs(std::ostream& out, const std::vector<std::size_t>& places, std::size_t count) {
  out << '[';
  for (std::size_t i = 0; i < places.size(); ++i) {
    if (places[i] >= count) throw std::invalid_argument("Alpha+ result: invalid arc place");
    if (i) out << ',';
    out << places[i];
  }
  out << ']';
}
}

std::string alpha_plus_result_json(const AlphaPlusDiscoveryResult& result) {
  const auto count = result.net.place_count();
  if (result.place_names.size() != count || result.initial_marking.size() != count ||
      result.final_marking.size() != count)
    throw std::invalid_argument("Alpha+ result: place and marking dimensions differ");
  std::ostringstream out;
  out << "{\"family\":\"ALPHA_PLUS\",\"places\":[";
  for (std::size_t i = 0; i < count; ++i) {
    if (i) out << ',';
    out << "{\"id\":" << i << ",\"name\":";
    place_name(out, result.place_names[i]);
    out << ",\"initial\":" << result.initial_marking[i]
        << ",\"final\":" << result.final_marking[i] << '}';
  }
  out << "],\"transitions\":[";
  const auto& transitions = result.net.transitions();
  for (std::size_t i = 0; i < transitions.size(); ++i) {
    const auto& transition = transitions[i];
    if (transition.has_extended_arcs())
      throw std::invalid_argument("Alpha+ result: extended arcs are not supported");
    if (i) out << ',';
    out << "{\"id\":" << i << ",\"label\":";
    if (transition.activity) {
      if (*transition.activity >= result.net.activities().size())
        throw std::invalid_argument("Alpha+ result: invalid transition activity");
      text(out, result.net.activities()[*transition.activity]);
    } else out << "null";
    out << ",\"inputs\":";
    arcs(out, transition.inputs, count);
    out << ",\"outputs\":";
    arcs(out, transition.outputs, count);
    out << '}';
  }
  out << "],\"detached_initial_marking\":";
  detached(out, result.detached_initial_marking);
  out << ",\"detached_final_marking\":";
  detached(out, result.detached_final_marking);
  out << '}';
  return out.str();
}
}
