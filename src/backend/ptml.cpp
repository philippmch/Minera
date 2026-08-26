#include "minera/ptml.hpp"

#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "xml_pull_parser.hpp"

namespace minera {
namespace {

// Escapes the five XML metacharacters so activity names produce well-formed
// PTML in attribute values.
void write_xml_escaped(std::ostream& out, std::string_view value) {
  for (const char c : value) {
    switch (c) {
      case '&': out << "&amp;"; break;
      case '<': out << "&lt;"; break;
      case '>': out << "&gt;"; break;
      case '"': out << "&quot;"; break;
      case '\'': out << "&apos;"; break;
      default: out << c;
    }
  }
}

// Emits the node elements and the parent-child edges of a tree; the edges'
// document order is what a PTML reader uses as child order.
class PtmlWriter {
 public:
  explicit PtmlWriter(const ProcessTree& tree) : tree_(tree) {}

  std::string build() { return emit(tree_.root()); }

  [[nodiscard]] std::string nodes() const { return nodes_.str(); }
  [[nodiscard]] std::string edges() const { return edges_.str(); }

 private:
  std::string fresh() { return "node" + std::to_string(counter_++); }

  void edge(const std::string& parent, const std::string& child) {
    edges_ << "    <parentsNode id=\"edge" << edge_counter_++
           << "\" sourceId=\"" << parent << "\" targetId=\"" << child
           << "\"/>\n";
  }

  std::string leaf(const char* element, std::string_view name) {
    const auto id = fresh();
    nodes_ << "    <" << element << " id=\"" << id << "\" name=\"";
    write_xml_escaped(nodes_, name);
    nodes_ << "\"/>\n";
    return id;
  }

  std::string operator_node(const char* element) {
    const auto id = fresh();
    nodes_ << "    <" << element << " id=\"" << id << "\" name=\"\"/>\n";
    return id;
  }

  std::string emit(const ProcessTreeNode& node) {
    switch (node.op) {
      case ProcessTreeOperator::kActivity:
        return leaf("manualTask", tree_.activities()[node.activity]);
      case ProcessTreeOperator::kTau:
        return leaf("automaticTask", "");
      case ProcessTreeOperator::kSequence:
      case ProcessTreeOperator::kExclusive:
      case ProcessTreeOperator::kInclusiveChoice:
      case ProcessTreeOperator::kParallel: {
        const char* element =
            node.op == ProcessTreeOperator::kSequence   ? "sequence"
            : node.op == ProcessTreeOperator::kExclusive ? "xor"
            : node.op == ProcessTreeOperator::kInclusiveChoice ? "or"
                                                         : "and";
        const auto id = operator_node(element);
        for (const auto& child : node.children) {
          edge(id, emit(child));
        }
        return id;
      }
      case ProcessTreeOperator::kLoop: {
        // The three-child shape readers expect: body, one redo, silent exit.
        // Several redo alternatives become one xor; a redo-less loop (which
        // discovery never produces) degrades to a loop that exits at once.
        const auto id = operator_node("xorLoop");
        edge(id, node.children.empty() ? leaf("automaticTask", "")
                                       : emit(node.children.front()));
        std::string redo;
        if (node.children.size() == 2) {
          redo = emit(node.children[1]);
        } else if (node.children.size() > 2) {
          redo = operator_node("xor");
          for (std::size_t i = 1; i < node.children.size(); ++i) {
            edge(redo, emit(node.children[i]));
          }
        } else {
          redo = leaf("automaticTask", "");
        }
        edge(id, redo);
        edge(id, leaf("automaticTask", ""));
        return id;
      }
    }
    return leaf("automaticTask", "");
  }

  const ProcessTree& tree_;
  std::ostringstream nodes_;
  std::ostringstream edges_;
  int counter_ = 0;
  int edge_counter_ = 0;
};

}  // namespace

void write_ptml(std::ostream& out,
                const ProcessTree& tree,
                std::string_view name) {
  PtmlWriter writer(tree);
  const auto root = writer.build();
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  out << "<ptml>\n";
  out << "  <processTree id=\"tree\" name=\"";
  write_xml_escaped(out, name);
  out << "\" root=\"" << root << "\">\n";
  out << writer.nodes();
  out << writer.edges();
  out << "  </processTree>\n";
  out << "</ptml>\n";
}

std::string to_ptml(const ProcessTree& tree, std::string_view name) {
  std::ostringstream out;
  write_ptml(out, tree, name);
  return out.str();
}

namespace {

// What the parse pass collects per node before the tree is assembled.
struct ParsedPtmlNode {
  std::string kind;  // element name: sequence, xor, or, and, xorLoop, manualTask, automaticTask
  std::string name;
  std::vector<std::string> children;  // child ids, in edge document order
  bool has_parent = false;
};

std::string_view local_name(std::string_view name) {
  const auto colon = name.rfind(':');
  return colon == std::string_view::npos ? name : name.substr(colon + 1);
}

bool is_ptml_node(std::string_view kind) {
  return kind == "sequence" || kind == "xor" || kind == "or" || kind == "and" ||
         kind == "xorLoop" || kind == "manualTask" || kind == "automaticTask";
}

// Assembles ProcessTreeNodes from the parsed elements, interning activity
// labels and guarding against cycles (a malformed file's edges could loop).
class PtmlBuilder {
 public:
  explicit PtmlBuilder(
      const std::unordered_map<std::string, ParsedPtmlNode>& nodes)
      : nodes_(nodes) {}

  ProcessTree tree(const std::string& root_id) {
    auto root = build(root_id);
    return ProcessTree(std::move(activities_), std::move(root));
  }

 private:
  ProcessTreeNode build(const std::string& id) {
    if (!visiting_.insert(id).second) {
      throw std::runtime_error("PTML edges form a cycle at node '" + id + "'");
    }
    const auto it = nodes_.find(id);
    if (it == nodes_.end()) {
      throw std::runtime_error("PTML references unknown node '" + id + "'");
    }
    const auto& parsed = it->second;

    ProcessTreeNode node;
    if (parsed.kind == "manualTask" || parsed.kind == "automaticTask") {
      if (!parsed.children.empty()) {
        throw std::runtime_error("PTML task node '" + id + "' has children");
      }
      // An unnamed manual task carries no observable activity: silent.
      if (parsed.kind == "manualTask" && !parsed.name.empty()) {
        node = ProcessTreeNode::leaf(activity_id(parsed.name));
      } else {
        node = ProcessTreeNode::tau();
      }
    } else if (parsed.kind == "xorLoop") {
      if (parsed.children.size() < 2 || parsed.children.size() > 3) {
        throw std::runtime_error(
            "PTML loop node '" + id +
            "' needs a body and a redo child (plus an optional exit)");
      }
      node.op = ProcessTreeOperator::kLoop;
      node.children.push_back(build(parsed.children[0]));
      node.children.push_back(build(parsed.children[1]));
      if (parsed.children.size() == 3) {
        // The three-child form runs its exit child once when the loop ends:
        // sequence(loop, exit). A silent exit changes nothing and is dropped.
        auto exit = build(parsed.children[2]);
        if (exit.op != ProcessTreeOperator::kTau) {
          ProcessTreeNode sequence;
          sequence.op = ProcessTreeOperator::kSequence;
          sequence.children.push_back(std::move(node));
          sequence.children.push_back(std::move(exit));
          node = std::move(sequence);
        }
      }
    } else {
      node.op = parsed.kind == "sequence" ? ProcessTreeOperator::kSequence
                : parsed.kind == "xor"    ? ProcessTreeOperator::kExclusive
                : parsed.kind == "or"     ? ProcessTreeOperator::kInclusiveChoice
                                          : ProcessTreeOperator::kParallel;
      if (parsed.children.empty()) {
        throw std::runtime_error("PTML operator node '" + id +
                                 "' has no children");
      }
      for (const auto& child : parsed.children) {
        node.children.push_back(build(child));
      }
    }
    visiting_.erase(id);
    return node;
  }

  std::size_t activity_id(const std::string& label) {
    const auto [it, inserted] = activity_ids_.emplace(label, activities_.size());
    if (inserted) activities_.push_back(label);
    return it->second;
  }

  const std::unordered_map<std::string, ParsedPtmlNode>& nodes_;
  std::vector<std::string> activities_;
  std::unordered_map<std::string, std::size_t> activity_ids_;
  std::unordered_set<std::string> visiting_;
};

}  // namespace

ProcessTree parse_ptml(std::string_view xml) {
  detail::XmlPullParser parser(xml);

  std::unordered_map<std::string, ParsedPtmlNode> nodes;
  std::vector<std::string> order;  // node ids in document order
  // Edges in document order — that order is the child order. Resolved after
  // the parse, since a file may legally declare an edge before its child.
  std::vector<std::pair<std::string, std::string>> edges;
  std::string root_id;
  bool in_tree = false;
  bool trees_seen = false;

  while (true) {
    const auto token = parser.next();
    if (token.kind == detail::XmlPullParser::Kind::EndOfInput) break;
    if (token.kind == detail::XmlPullParser::Kind::CharacterData) continue;

    const auto name = local_name(token.name);
    if (token.kind == detail::XmlPullParser::Kind::EndElement) {
      if (name == "processTree") in_tree = false;
      continue;
    }

    if (name == "processTree") {
      if (trees_seen) {
        // Only the first tree is read; skip the rest wholesale.
        if (!token.self_closing) {
          std::size_t depth = 1;
          while (depth > 0) {
            const auto skipped = parser.next();
            if (skipped.kind == detail::XmlPullParser::Kind::EndOfInput) break;
            if (skipped.kind == detail::XmlPullParser::Kind::StartElement &&
                !skipped.self_closing) {
              ++depth;
            } else if (skipped.kind ==
                       detail::XmlPullParser::Kind::EndElement) {
              --depth;
            }
          }
        }
        continue;
      }
      trees_seen = true;
      in_tree = true;
      if (const auto* root = detail::attribute(token.attributes, "root")) {
        root_id = *root;
      }
      continue;
    }
    if (!in_tree) continue;

    if (name == "parentsNode") {
      const auto* source = detail::attribute(token.attributes, "sourceId");
      const auto* target = detail::attribute(token.attributes, "targetId");
      if (!source || !target) {
        throw std::runtime_error(
            "PTML edge is missing its sourceId or targetId");
      }
      edges.emplace_back(*source, *target);
    } else if (is_ptml_node(name)) {
      const auto* id = detail::attribute(token.attributes, "id");
      if (!id) throw std::runtime_error("PTML node is missing its id");
      ParsedPtmlNode node;
      node.kind = std::string(name);
      if (const auto* label = detail::attribute(token.attributes, "name")) {
        node.name = *label;
      }
      if (!nodes.emplace(*id, std::move(node)).second) {
        throw std::runtime_error("PTML has a duplicate node id '" + *id + "'");
      }
      order.push_back(*id);
    } else if (name == "or" || name == "interleaving" || name == "andLoop") {
      throw std::runtime_error(
          "PTML node kind '" + std::string(name) +
          "' has no equivalent here and cannot be checked");
    }
    // Anything else (originalLog, properties, …) is ignored.
  }

  if (nodes.empty()) {
    throw std::runtime_error(
        "PTML input contains no process tree (or an empty one)");
  }

  for (const auto& [source, target] : edges) {
    const auto parent = nodes.find(source);
    const auto child = nodes.find(target);
    if (parent == nodes.end() || child == nodes.end()) {
      throw std::runtime_error("PTML edge '" + source + "' -> '" + target +
                               "' references an unknown node");
    }
    if (child->second.has_parent) {
      throw std::runtime_error("PTML node '" + target +
                               "' has two parents; a process tree is a tree");
    }
    child->second.has_parent = true;
    parent->second.children.push_back(target);
  }

  if (root_id.empty()) {
    // No root attribute: the unique node no edge points at.
    for (const auto& id : order) {
      if (nodes.at(id).has_parent) continue;
      if (!root_id.empty()) {
        throw std::runtime_error(
            "PTML has no root attribute and no unique parentless node");
      }
      root_id = id;
    }
  } else if (nodes.find(root_id) == nodes.end()) {
    throw std::runtime_error("PTML root references unknown node '" + root_id +
                             "'");
  }

  return PtmlBuilder(nodes).tree(root_id);
}

ProcessTree import_ptml(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("Unable to open PTML file: " + path.string());
  }
  std::string xml((std::istreambuf_iterator<char>(input)),
                  std::istreambuf_iterator<char>());
  if (xml.size() >= 3 && static_cast<unsigned char>(xml[0]) == 0xEF &&
      static_cast<unsigned char>(xml[1]) == 0xBB &&
      static_cast<unsigned char>(xml[2]) == 0xBF) {
    xml.erase(0, 3);
  }
  return parse_ptml(xml);
}

bool looks_like_ptml(std::string_view xml) {
  try {
    detail::XmlPullParser parser(xml);
    const auto token = parser.next();
    if (token.kind != detail::XmlPullParser::Kind::StartElement) return false;
    const auto name = local_name(token.name);
    return name == "ptml" || name == "processTree";
  } catch (const std::exception&) {
    return false;
  }
}

}  // namespace minera
