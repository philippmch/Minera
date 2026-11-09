#include "minera/ptml.hpp"
#include "minera/token_integer.hpp"
#include <algorithm>
#include <string_view>

#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "xml_pull_parser.hpp"

namespace minera::inductive_detail {
// RFC 1321 MD5 digest integer of a label (defined with the inductive miner).
TokenInteger activity_order_key(std::string_view label);
}  // namespace minera::inductive_detail

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
      // A reader normalises literal tabs and line breaks in attribute values
      // to spaces; character references keep them.
      case '\t': out << "&#9;"; break;
      case '\n': out << "&#10;"; break;
      case '\r': out << "&#13;"; break;
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
      case ProcessTreeOperator::kParallel:
      case ProcessTreeOperator::kInterleaving: {
        const char* element =
            node.op == ProcessTreeOperator::kSequence   ? "sequence"
            : node.op == ProcessTreeOperator::kExclusive ? "xor"
            : node.op == ProcessTreeOperator::kInclusiveChoice ? "or"
            : node.op == ProcessTreeOperator::kInterleaving ? "interleaving"
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
        // Three or more redo alternatives are written as the reference
        // writes them, one child each: a reader takes four or more children
        // as body and redos (three would read as body, redo and exit).
        if (node.children.size() >= 4) {
          for (const auto& child : node.children) edge(id, emit(child));
          return id;
        }
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
  std::string kind;  // element name: sequence, xor, or, and, interleaving, xorLoop, manualTask, automaticTask
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
         kind == "interleaving" ||
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
    // The reference importer finishes with tree_sort: parallel and choice
    // children are stably ordered by the sum of their leaves' label MD5
    // digest integers (silent leaves contribute zero). Other operators keep
    // their document order.
    std::vector<TokenInteger> keys;
    keys.reserve(activities_.size());
    for (const auto& label : activities_)
      keys.push_back(inductive_detail::activity_order_key(label));
    sort_children(root, keys);
    return ProcessTree(std::move(activities_), std::move(root));
  }

 private:
  ProcessTreeNode build(const std::string& id) {
    // build() recurses once per tree level and visiting_ holds the current
    // path; bound it so a deep model is an error, not a stack overflow.
    constexpr std::size_t kMaxDepth = 1000;
    if (visiting_.size() >= kMaxDepth) {
      throw std::runtime_error("PTML process tree nesting exceeds 1000 levels");
    }
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
      // As in the reference, a manual task is visible even when its name is
      // empty; only an automatic task is silent.
      if (parsed.kind == "manualTask") {
        node = ProcessTreeNode::leaf(activity_id(parsed.name));
      } else {
        node = ProcessTreeNode::tau();
      }
    } else if (parsed.kind == "xorLoop") {
      if (parsed.children.size() < 2) {
        throw std::runtime_error(
            "PTML loop node '" + id +
            "' needs a body and at least one redo child");
      }
      node.op = ProcessTreeOperator::kLoop;
      node.children.push_back(build(parsed.children[0]));
      node.children.push_back(build(parsed.children[1]));
      // Four or more children read as the reference reads them: one body and
      // several redo alternatives.
      for (std::size_t c = 3; parsed.children.size() > 3 && c <= parsed.children.size(); ++c)
        node.children.push_back(build(parsed.children[c - 1]));
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
                : parsed.kind == "interleaving" ? ProcessTreeOperator::kInterleaving
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

  static TokenInteger sort_children(ProcessTreeNode& node,
                                    const std::vector<TokenInteger>& keys) {
    if (node.op == ProcessTreeOperator::kActivity) return keys.at(node.activity);
    TokenInteger sum;
    std::vector<std::pair<TokenInteger, std::size_t>> child_keys;
    child_keys.reserve(node.children.size());
    for (std::size_t index = 0; index < node.children.size(); ++index) {
      auto key = sort_children(node.children[index], keys);
      sum += key;
      child_keys.emplace_back(std::move(key), index);
    }
    if (node.op == ProcessTreeOperator::kParallel ||
        node.op == ProcessTreeOperator::kExclusive) {
      std::stable_sort(child_keys.begin(), child_keys.end(),
                       [](const auto& left, const auto& right) {
                         return left.first < right.first;
                       });
      std::vector<ProcessTreeNode> sorted;
      sorted.reserve(node.children.size());
      for (const auto& entry : child_keys)
        sorted.push_back(std::move(node.children[entry.second]));
      node.children = std::move(sorted);
    }
    return sum;
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

namespace {

struct ParsedPtmlTree {
  std::unordered_map<std::string, ParsedPtmlNode> nodes;
  std::vector<std::string> order;  // node ids in document order
  // Edges in document order — that order is the child order. Resolved after
  // the parse, since a file may legally declare an edge before its child.
  std::vector<std::pair<std::string, std::string>> edges;
  std::optional<std::string> root_id;
};

ProcessTree build_ptml_tree(ParsedPtmlTree& tree) {
  auto& nodes = tree.nodes;
  const auto& order = tree.order;
  if (!tree.root_id)
    throw std::runtime_error("PTML process tree is missing its root attribute");
  const std::string root_id = *tree.root_id;
  // A node listed as the child of two parents is shared, and the same edge
  // twice repeats the child, as in the reference.
  for (const auto& [source, target] : tree.edges) {
    const auto parent = nodes.find(source);
    const auto child = nodes.find(target);
    if (parent == nodes.end() || child == nodes.end()) {
      throw std::runtime_error("PTML edge '" + source + "' -> '" + target +
                               "' references an unknown node");
    }
    child->second.has_parent = true;
    parent->second.children.push_back(target);
  }
  if (nodes.find(root_id) == nodes.end()) {
    throw std::runtime_error("PTML root references unknown node '" + root_id +
                             "'");
  }

  // Observed reference reading of a three-child xorLoop (body, redo, exit),
  // visited in document order: a silent exit is dropped and the loop stays
  // in place. Otherwise the loop keeps its body and redo and is replaced by
  // sequence(loop, exit), appended at the end of its current parent's
  // children; a root loop simply loses its exit.
  std::unordered_map<std::string, std::string> parent_of;
  for (const auto& [id, node] : nodes)
    for (const auto& child : node.children) parent_of[child] = id;
  std::size_t synthetic = 0;
  const auto document_order = order;
  for (const auto& id : document_order) {
    auto& loop = nodes.at(id);
    if (loop.kind != "xorLoop" || loop.children.size() != 3) continue;
    const auto exit = loop.children.back();
    const auto exit_node = nodes.find(exit);
    if (exit_node == nodes.end()) continue;  // reported by the builder
    const bool silent_exit = exit_node->second.children.empty() &&
                             exit_node->second.kind == "automaticTask";
    loop.children.pop_back();
    const auto parent = parent_of.find(id);
    if (silent_exit || parent == parent_of.end()) {
      parent_of.erase(exit);  // a dropped exit is detached from the tree
      continue;
    }
    std::string sequence_id;
    do {
      sequence_id = "\x01loop-exit-sequence-" + std::to_string(synthetic++);
    } while (nodes.contains(sequence_id));
    auto& siblings = nodes.at(parent->second).children;
    const auto position = std::find(siblings.begin(), siblings.end(), id);
    if (position == siblings.end()) continue;  // inside a detached exit
    siblings.erase(position);
    siblings.push_back(sequence_id);
    ParsedPtmlNode sequence;
    sequence.kind = "sequence";
    sequence.children = {id, exit};
    sequence.has_parent = true;
    const auto grandparent = parent->second;
    nodes.emplace(sequence_id, std::move(sequence));
    parent_of[sequence_id] = grandparent;
    parent_of[id] = sequence_id;
    parent_of[exit] = sequence_id;
  }

  return PtmlBuilder(nodes).tree(root_id);
}

}  // namespace

ProcessTree parse_ptml(std::string_view xml) {
  if (xml.substr(0, 3) == "\xEF\xBB\xBF") xml.remove_prefix(3);
  detail::validate_model_xml(xml, "PTML");
  // lxml refuses input that is not well-formed; the strict parser does too.
  detail::XmlPullParser parser(xml, false, false, detail::XmlStrictness::kStrict,
                               detail::model_xml_options("PTML"));

  // The reference reads every child element of the document element as a
  // process tree and keeps the last one; a fault in any tree is an error.
  // Inside a tree only the direct child elements count, and their tag must be
  // a known PTML tag without a namespace.
  std::vector<ParsedPtmlTree> trees;
  std::vector<std::string> default_namespace{""};  // per open element
  while (true) {
    const auto token = parser.next();
    if (token.kind == detail::XmlPullParser::Kind::EndOfInput) break;
    if (token.kind == detail::XmlPullParser::Kind::CharacterData) continue;
    if (token.kind == detail::XmlPullParser::Kind::EndElement) {
      if (default_namespace.size() > 1) default_namespace.pop_back();
      continue;
    }
    if (token.kind != detail::XmlPullParser::Kind::StartElement) continue;
    const std::size_t depth = default_namespace.size() - 1;
    std::string ns = default_namespace.back();
    if (const auto* declared = detail::attribute(token.attributes, "xmlns")) ns = *declared;
    if (!token.self_closing) default_namespace.push_back(ns);

    if (depth == 1) {
      trees.emplace_back();
      if (const auto* root = detail::attribute(token.attributes, "root"))
        trees.back().root_id = *root;
      continue;
    }
    if (depth != 2) continue;
    auto& tree = trees.back();
    const std::string_view name = token.name;
    const bool namespaced = !ns.empty() || name.find(':') != std::string_view::npos;
    if (!namespaced && name == "parentsNode") {
      const auto* source = detail::attribute(token.attributes, "sourceId");
      const auto* target = detail::attribute(token.attributes, "targetId");
      if (!source || !target) {
        throw std::runtime_error(
            "PTML edge is missing its sourceId or targetId");
      }
      tree.edges.emplace_back(*source, *target);
    } else if (!namespaced && is_ptml_node(name)) {
      // A task's name is its label, and the reference refuses a task without
      // one. It refuses an operator without a name too; Minera reads that
      // operator, whose name carries nothing (declared).
      const auto* label = detail::attribute(token.attributes, "name");
      const bool task = name == "manualTask" || name == "automaticTask";
      if (!label && task)
        throw std::runtime_error("PTML " + std::string(name) +
                                 " node is missing its name attribute");
      // A node without an id is skipped; a repeated id keeps the last node.
      const auto* id = detail::attribute(token.attributes, "id");
      if (!id) continue;
      ParsedPtmlNode node;
      node.kind = std::string(name);
      if (label) node.name = *label;
      if (tree.nodes.insert_or_assign(*id, std::move(node)).second)
        tree.order.push_back(*id);
    } else {
      const auto local = name.substr(name.find(':') == std::string_view::npos
                                         ? 0 : name.find(':') + 1);
      throw std::runtime_error(
          "PTML unknown tag: " +
          (ns.empty() ? std::string(name) : "{" + ns + "}" + std::string(local)));
    }
  }

  if (trees.empty()) {
    throw std::runtime_error("PTML input contains no process tree");
  }
  std::optional<ProcessTree> last;
  for (auto& tree : trees) last = build_ptml_tree(tree);
  return std::move(*last);
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
