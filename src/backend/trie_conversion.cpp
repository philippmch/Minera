#include "minera/trie_conversion.hpp"

#include <algorithm>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace minera {
namespace {

void collect_labels(const TrieNode& node, std::vector<std::string>& labels,
                    bool include_self) {
  if (include_self) labels.push_back(node.label);
  for (const auto& child : node.children)
    collect_labels(child, labels, true);
}

std::string json_escape(std::string_view value) {
  std::string out;
  for (const unsigned char c : value) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out.push_back(static_cast<char>(c)); break;
    }
  }
  return out;
}

void add_node(const TrieNode& node, PetriNet& net, std::size_t entry,
              std::size_t sink, const std::map<std::string, std::size_t>& ids,
              std::size_t depth) {
  if (depth > 4096) {
    throw std::invalid_argument("trie conversion: maximum depth exceeded");
  }
  std::vector<const TrieNode*> children;
  children.reserve(node.children.size());
  for (const auto& child : node.children) children.push_back(&child);
  std::stable_sort(children.begin(), children.end(),
                   [](const TrieNode* left, const TrieNode* right) {
                     return left->label < right->label;
                   });
  for (std::size_t i = 1; i < children.size(); ++i) {
    if (children[i - 1]->label == children[i]->label) {
      throw std::invalid_argument("trie conversion: duplicate sibling label");
    }
  }

  std::size_t continuation = sink;
  if (!children.empty()) continuation = net.add_place();
  const auto transition = net.add_transition(ids.at(node.label), {entry},
                                              {continuation});
  (void)transition;
  if (children.empty()) return;

  for (const auto* child : children) {
    add_node(*child, net, continuation, sink, ids, depth + 1);
  }
  if (node.final) net.add_transition(std::nullopt, {continuation}, {sink});
}

}  // namespace

PetriNet convert_trie_to_petri_net(const TrieModel& trie) {
  std::vector<std::string> labels;
  collect_labels(trie.root, labels, false);
  std::sort(labels.begin(), labels.end());
  labels.erase(std::unique(labels.begin(), labels.end()), labels.end());

  PetriNet net;
  net.set_activities(labels);
  const auto source = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  std::map<std::string, std::size_t> ids;
  for (std::size_t i = 0; i < labels.size(); ++i) ids.emplace(labels[i], i);

  std::vector<const TrieNode*> roots;
  roots.reserve(trie.root.children.size());
  for (const auto& child : trie.root.children) roots.push_back(&child);
  std::stable_sort(roots.begin(), roots.end(),
                   [](const TrieNode* left, const TrieNode* right) {
                     return left->label < right->label;
                   });
  for (std::size_t i = 1; i < roots.size(); ++i) {
    if (roots[i - 1]->label == roots[i]->label) {
      throw std::invalid_argument("trie conversion: duplicate root label");
    }
  }
  if (trie.root.final) net.add_transition(std::nullopt, {source}, {sink});
  for (const auto* child : roots) add_node(*child, net, source, sink, ids, 1);
  return net;
}

std::string serialize_trie_petri_net_json(const PetriNet& net) {
  if (net.source() >= net.place_count() || net.sink() >= net.place_count()) {
    throw std::invalid_argument("trie conversion: net source or sink is invalid");
  }
  for (const auto& transition : net.transitions()) {
    if (transition.activity && *transition.activity >= net.activities().size())
      throw std::invalid_argument("trie conversion: invalid activity reference");
  }
  std::ostringstream out;
  out << "{\"family\":\"TRIE_CONVERSION\",\"variant\":\"TO_PETRI_NET\",\"source\":"
      << net.source() << ",\"sink\":" << net.sink() << ",\"activities\":[";
  for (std::size_t i = 0; i < net.activities().size(); ++i) {
    if (i != 0) out << ',';
    out << '\"' << json_escape(net.activities()[i]) << '\"';
  }
  out << "],\"place_count\":" << net.place_count() << ",\"transitions\":[";
  for (std::size_t i = 0; i < net.transitions().size(); ++i) {
    if (i != 0) out << ',';
    const auto& transition = net.transitions()[i];
    out << "{\"activity\":";
    if (transition.activity) out << *transition.activity;
    else out << "null";
    out << ",\"inputs\":[";
    for (std::size_t j = 0; j < transition.inputs.size(); ++j) {
      if (j != 0) out << ',';
      out << transition.inputs[j];
    }
    out << "],\"outputs\":[";
    for (std::size_t j = 0; j < transition.outputs.size(); ++j) {
      if (j != 0) out << ',';
      out << transition.outputs[j];
    }
    out << "]}";
  }
  out << "]}";
  return out.str();
}

}  // namespace minera
