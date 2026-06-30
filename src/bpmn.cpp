#include "minera/bpmn.hpp"

#include <cstddef>
#include <sstream>
#include <string>

namespace minera {
namespace {

// Escapes the five XML metacharacters so activity names and the process name
// produce well-formed BPMN whether used as element text or attribute values.
std::string xml_escaped(std::string_view value) {
  std::string out;
  for (const char c : value) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&apos;"; break;
      default: out += c;
    }
  }
  return out;
}

// A fragment of BPMN flow exposing a single entry and single exit node, so the
// surrounding block can wire it up with one sequence flow on each side. A tau
// leaf (or a block whose children are all silent) produces an `empty` fragment,
// which callers route through directly with a sequence flow instead of a node.
struct Fragment {
  bool empty = true;
  std::string entry;
  std::string exit;
};

// Walks a process tree, emitting BPMN flow nodes and the sequence flows that
// connect them into the two string buffers exposed after build().
class BpmnBuilder {
 public:
  explicit BpmnBuilder(const ProcessTree& tree) : tree_(tree) {}

  void build() {
    const std::string start = fresh("StartEvent");
    nodes_ << "    <startEvent id=\"" << start << "\"/>\n";

    const Fragment body = emit(tree_.root());
    const std::string end = fresh("EndEvent");

    if (body.empty) {
      flow(start, end);
    } else {
      flow(start, body.entry);
      flow(body.exit, end);
    }
    nodes_ << "    <endEvent id=\"" << end << "\"/>\n";
  }

  [[nodiscard]] std::string nodes() const { return nodes_.str(); }
  [[nodiscard]] std::string flows() const { return flows_.str(); }

 private:
  std::string fresh(const char* prefix) {
    return std::string(prefix) + "_" + std::to_string(counter_++);
  }

  void flow(const std::string& source, const std::string& target) {
    flows_ << "    <sequenceFlow id=\"" << fresh("Flow") << "\" sourceRef=\""
           << source << "\" targetRef=\"" << target << "\"/>\n";
  }

  Fragment emit(const ProcessTreeNode& node) {
    switch (node.op) {
      case ProcessTreeOperator::kActivity:
        return emit_activity(node);
      case ProcessTreeOperator::kTau:
        return Fragment{};  // empty: routed through directly
      case ProcessTreeOperator::kSequence:
        return emit_sequence(node);
      case ProcessTreeOperator::kExclusive:
        return emit_gateway(node, "exclusiveGateway");
      case ProcessTreeOperator::kParallel:
        return emit_gateway(node, "parallelGateway");
      case ProcessTreeOperator::kLoop:
        return emit_loop(node);
    }
    return Fragment{};
  }

  Fragment emit_activity(const ProcessTreeNode& node) {
    const std::string id = fresh("Activity");
    nodes_ << "    <task id=\"" << id << "\" name=\""
           << xml_escaped(tree_.activities()[node.activity]) << "\"/>\n";
    return Fragment{false, id, id};
  }

  Fragment emit_sequence(const ProcessTreeNode& node) {
    Fragment result;  // stays empty until the first non-silent child appears
    std::string last_exit;
    for (const auto& child : node.children) {
      const Fragment frag = emit(child);
      if (frag.empty) {
        continue;  // a tau in a sequence is a no-op
      }
      if (result.empty) {
        result.empty = false;
        result.entry = frag.entry;
      } else {
        flow(last_exit, frag.entry);
      }
      last_exit = frag.exit;
      result.exit = last_exit;
    }
    return result;
  }

  Fragment emit_gateway(const ProcessTreeNode& node, const char* kind) {
    const std::string split = fresh(kind);
    const std::string join = fresh(kind);
    nodes_ << "    <" << kind << " id=\"" << split << "\"/>\n";
    nodes_ << "    <" << kind << " id=\"" << join << "\"/>\n";
    for (const auto& child : node.children) {
      const Fragment frag = emit(child);
      if (frag.empty) {
        flow(split, join);  // an empty (skip) branch
      } else {
        flow(split, frag.entry);
        flow(frag.exit, join);
      }
    }
    return Fragment{false, split, join};
  }

  Fragment emit_loop(const ProcessTreeNode& node) {
    // child[0] is the do-body (executed at least once); child[1..] are redo
    // paths, each run before the body executes again. Model the cycle with a
    // join gateway at the loop head and a split gateway after the body.
    const std::string head = fresh("exclusiveGateway");
    const std::string tail = fresh("exclusiveGateway");
    nodes_ << "    <exclusiveGateway id=\"" << head << "\"/>\n";
    nodes_ << "    <exclusiveGateway id=\"" << tail << "\"/>\n";

    const Fragment body =
        node.children.empty() ? Fragment{} : emit(node.children.front());
    if (body.empty) {
      flow(head, tail);
    } else {
      flow(head, body.entry);
      flow(body.exit, tail);
    }

    for (std::size_t i = 1; i < node.children.size(); ++i) {
      const Fragment redo = emit(node.children[i]);
      if (redo.empty) {
        flow(tail, head);
      } else {
        flow(tail, redo.entry);
        flow(redo.exit, head);
      }
    }
    return Fragment{false, head, tail};
  }

  const ProcessTree& tree_;
  std::ostringstream nodes_;
  std::ostringstream flows_;
  int counter_ = 0;
};

}  // namespace

void write_bpmn(std::ostream& out,
                const ProcessTree& tree,
                std::string_view name) {
  BpmnBuilder builder(tree);
  builder.build();

  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  out << "<definitions "
         "xmlns=\"http://www.omg.org/spec/BPMN/20100524/MODEL\" "
         "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
         "targetNamespace=\"http://minera.dev/bpmn\" id=\"definitions\">\n";
  out << "  <process id=\"process\" name=\"" << xml_escaped(name)
      << "\" isExecutable=\"false\">\n";
  out << builder.nodes();
  out << builder.flows();
  out << "  </process>\n";
  out << "</definitions>\n";
}

std::string to_bpmn(const ProcessTree& tree, std::string_view name) {
  std::ostringstream out;
  write_bpmn(out, tree, name);
  return out.str();
}

}  // namespace minera
