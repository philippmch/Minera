#include "minera/bpmn.hpp"
#include "minera/petri_net_reduction.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "xml_pull_parser.hpp"

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

// The BPMN flow graph both writers build before serialization. Nodes keep
// their element kind, optional name and gateway direction; flows may repeat
// between the same pair of nodes.
struct BpmnGraphNode {
  std::string id;
  std::string element;  // startEvent, endEvent, task, *Gateway
  std::optional<std::string> name;
  std::string direction;  // gateways only
  bool alive = true;
};

struct BpmnGraphFlow {
  std::string id;
  std::size_t source = 0;
  std::size_t target = 0;
  bool alive = true;
};

class BpmnGraph {
 public:
  std::size_t add(const char* element, const char* prefix,
                  std::optional<std::string> name = std::nullopt,
                  std::string direction = {}) {
    nodes.push_back({fresh(prefix), element, std::move(name), std::move(direction), true});
    return nodes.size() - 1;
  }
  void flow(std::size_t source, std::size_t target) {
    flows.push_back({fresh("Flow"), source, target, true});
  }

  // Removes every exclusive gateway with exactly one incoming and one
  // outgoing flow (other than a self-loop), joining its neighbours directly.
  void collapse_pass_through_exclusive() {
    for (bool changed = true; changed;) {
      changed = false;
      for (std::size_t n = 0; n < nodes.size(); ++n) {
        if (!nodes[n].alive || nodes[n].element != "exclusiveGateway") continue;
        std::vector<std::size_t> in, out;
        for (std::size_t f = 0; f < flows.size(); ++f) {
          if (!flows[f].alive) continue;
          if (flows[f].target == n) in.push_back(f);
          if (flows[f].source == n) out.push_back(f);
        }
        if (in.size() != 1 || out.size() != 1 || in.front() == out.front()) continue;
        flows[in.front()].target = flows[out.front()].target;
        flows[out.front()].alive = false;
        nodes[n].alive = false;
        changed = true;
      }
    }
  }

  std::vector<BpmnGraphNode> nodes;
  std::vector<BpmnGraphFlow> flows;

 private:
  std::string fresh(const char* prefix) {
    return std::string(prefix) + "_" + std::to_string(counter_++);
  }
  int counter_ = 0;
};

// A fragment of BPMN flow exposing a single entry and single exit node, so the
// surrounding block can wire it up with one sequence flow on each side. A tau
// leaf (or a block whose children are all silent) produces an `empty` fragment,
// which callers route through directly with a sequence flow instead of a node.
struct Fragment {
  bool empty = true;
  std::size_t entry = 0;
  std::size_t exit = 0;
};

// Walks a process tree into a BPMN graph: splits diverge, joins converge.
class BpmnBuilder {
 public:
  explicit BpmnBuilder(const ProcessTree& tree) : tree_(tree) {}

  BpmnGraph build() {
    const auto start = graph_.add("startEvent", "StartEvent", "start");
    const Fragment body = emit(tree_.root());
    const auto end = graph_.add("endEvent", "EndEvent", "end");
    if (body.empty) {
      graph_.flow(start, end);
    } else {
      graph_.flow(start, body.entry);
      graph_.flow(body.exit, end);
    }
    return std::move(graph_);
  }

 private:
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
      case ProcessTreeOperator::kInclusiveChoice:
        return emit_gateway(node, "inclusiveGateway");
      case ProcessTreeOperator::kParallel:
        return emit_gateway(node, "parallelGateway");
      case ProcessTreeOperator::kInterleaving:
        throw std::invalid_argument(
            "direct BPMN tree export does not support interleaving; convert the tree to a Petri net first");
      case ProcessTreeOperator::kLoop:
        return emit_loop(node);
    }
    return Fragment{};
  }

  Fragment emit_activity(const ProcessTreeNode& node) {
    const auto id = graph_.add("task", "Activity", tree_.activities()[node.activity]);
    return Fragment{false, id, id};
  }

  Fragment emit_sequence(const ProcessTreeNode& node) {
    Fragment result;  // stays empty until the first non-silent child appears
    std::size_t last_exit = 0;
    for (const auto& child : node.children) {
      const Fragment frag = emit(child);
      if (frag.empty) continue;  // a tau in a sequence is a no-op
      if (result.empty) {
        result.empty = false;
        result.entry = frag.entry;
      } else {
        graph_.flow(last_exit, frag.entry);
      }
      last_exit = frag.exit;
      result.exit = last_exit;
    }
    return result;
  }

  Fragment emit_gateway(const ProcessTreeNode& node, const char* kind) {
    const auto split = graph_.add(kind, kind, std::nullopt, "Diverging");
    const auto join = graph_.add(kind, kind, std::nullopt, "Converging");
    for (const auto& child : node.children) {
      const Fragment frag = emit(child);
      if (frag.empty) {
        graph_.flow(split, join);  // an empty (skip) branch
      } else {
        graph_.flow(split, frag.entry);
        graph_.flow(frag.exit, join);
      }
    }
    return Fragment{false, split, join};
  }

  Fragment emit_loop(const ProcessTreeNode& node) {
    // child[0] is the do-body (executed at least once); child[1..] are redo
    // paths, each run before the body executes again: a converging gateway at
    // the loop head and a diverging one after the body.
    const auto head = graph_.add("exclusiveGateway", "exclusiveGateway", std::nullopt, "Converging");
    const auto tail = graph_.add("exclusiveGateway", "exclusiveGateway", std::nullopt, "Diverging");
    const Fragment body =
        node.children.empty() ? Fragment{} : emit(node.children.front());
    if (body.empty) {
      graph_.flow(head, tail);
    } else {
      graph_.flow(head, body.entry);
      graph_.flow(body.exit, tail);
    }
    for (std::size_t i = 1; i < node.children.size(); ++i) {
      const Fragment redo = emit(node.children[i]);
      if (redo.empty) {
        graph_.flow(tail, head);
      } else {
        graph_.flow(tail, redo.entry);
        graph_.flow(redo.exit, head);
      }
    }
    return Fragment{false, head, tail};
  }

  const ProcessTree& tree_;
  BpmnGraph graph_;
};

struct ShapeSize {
  int width = 0;
  int height = 0;
};

ShapeSize shape_size(const std::string& element) {
  if (element == "task") return {100, 80};
  if (element == "startEvent" || element == "endEvent") return {36, 36};
  return {50, 50};
}

// A deterministic layered layout: each node's column is its longest-path
// depth from the graph's roots once the back edges of a depth-first search
// are ignored; rows follow node order within a column.
std::vector<std::pair<int, int>> layered_centres(const BpmnGraph& graph) {
  const auto count = graph.nodes.size();
  std::vector<std::vector<std::size_t>> successors(count);
  std::vector<std::size_t> indegree(count, 0);
  for (const auto& flow : graph.flows) {
    if (!flow.alive) continue;
    successors[flow.source].push_back(flow.target);
    ++indegree[flow.target];
  }
  // Iterative DFS colouring: 0 white, 1 on stack, 2 done.
  std::vector<int> colour(count, 0);
  std::vector<std::vector<bool>> forward(count);
  for (std::size_t n = 0; n < count; ++n) forward[n].assign(successors[n].size(), true);
  std::vector<std::size_t> roots;
  for (std::size_t n = 0; n < count; ++n)
    if (graph.nodes[n].alive && graph.nodes[n].element == "startEvent") roots.push_back(n);
  for (std::size_t n = 0; n < count; ++n)
    if (graph.nodes[n].alive && indegree[n] == 0) roots.push_back(n);
  for (std::size_t n = 0; n < count; ++n)
    if (graph.nodes[n].alive) roots.push_back(n);
  for (const auto root : roots) {
    if (colour[root] != 0) continue;
    std::vector<std::pair<std::size_t, std::size_t>> stack{{root, 0}};
    colour[root] = 1;
    while (!stack.empty()) {
      auto& [node, next] = stack.back();
      if (next == successors[node].size()) {
        colour[node] = 2;
        stack.pop_back();
        continue;
      }
      const auto index = next++;
      const auto target = successors[node][index];
      if (colour[target] == 1) {
        forward[node][index] = false;  // back edge
      } else if (colour[target] == 0) {
        colour[target] = 1;
        stack.emplace_back(target, 0);
      }
    }
  }
  std::vector<std::size_t> pending(count, 0);
  for (std::size_t n = 0; n < count; ++n)
    for (std::size_t i = 0; i < successors[n].size(); ++i)
      if (forward[n][i]) ++pending[successors[n][i]];
  std::vector<std::size_t> depth(count, 0);
  std::vector<std::size_t> ready;
  for (std::size_t n = 0; n < count; ++n)
    if (graph.nodes[n].alive && pending[n] == 0) ready.push_back(n);
  for (std::size_t head = 0; head < ready.size(); ++head) {
    const auto node = ready[head];
    for (std::size_t i = 0; i < successors[node].size(); ++i) {
      if (!forward[node][i]) continue;
      const auto target = successors[node][i];
      depth[target] = std::max(depth[target], depth[node] + 1);
      if (--pending[target] == 0) ready.push_back(target);
    }
  }
  std::vector<std::size_t> rows;
  std::vector<std::pair<int, int>> centres(count, {0, 0});
  for (std::size_t n = 0; n < count; ++n) {
    if (!graph.nodes[n].alive) continue;
    if (depth[n] >= rows.size()) rows.resize(depth[n] + 1, 0);
    const auto row = rows[depth[n]]++;
    centres[n] = {100 + static_cast<int>(depth[n]) * 160, 100 + static_cast<int>(row) * 120};
  }
  return centres;
}

// Serializes the graph: flow nodes with their incoming/outgoing flow
// references and gateway directions, the sequence flows, and a diagram
// interchange section with the layered layout so modelling tools can draw it.
void write_bpmn_document(std::ostream& out, std::string_view name,
                         const BpmnGraph& graph) {
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  out << "<definitions "
         "xmlns=\"http://www.omg.org/spec/BPMN/20100524/MODEL\" "
         "xmlns:bpmndi=\"http://www.omg.org/spec/BPMN/20100524/DI\" "
         "xmlns:dc=\"http://www.omg.org/spec/DD/20100524/DC\" "
         "xmlns:di=\"http://www.omg.org/spec/DD/20100524/DI\" "
         "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
         "targetNamespace=\"http://minera.dev/bpmn\" id=\"definitions\">\n";
  out << "  <process id=\"process\" name=\"" << xml_escaped(name)
      << "\" isExecutable=\"false\">\n";
  for (std::size_t n = 0; n < graph.nodes.size(); ++n) {
    const auto& node = graph.nodes[n];
    if (!node.alive) continue;
    out << "    <" << node.element << " id=\"" << node.id << "\"";
    if (node.name) out << " name=\"" << xml_escaped(*node.name) << "\"";
    if (!node.direction.empty())
      out << " gatewayDirection=\"" << node.direction << "\"";
    std::ostringstream children;
    for (const auto& flow : graph.flows)
      if (flow.alive && flow.target == n)
        children << "      <incoming>" << flow.id << "</incoming>\n";
    for (const auto& flow : graph.flows)
      if (flow.alive && flow.source == n)
        children << "      <outgoing>" << flow.id << "</outgoing>\n";
    const auto text = children.str();
    if (text.empty()) {
      out << "/>\n";
    } else {
      out << ">\n" << text << "    </" << node.element << ">\n";
    }
  }
  for (const auto& flow : graph.flows) {
    if (!flow.alive) continue;
    out << "    <sequenceFlow id=\"" << flow.id << "\" sourceRef=\""
        << graph.nodes[flow.source].id << "\" targetRef=\""
        << graph.nodes[flow.target].id << "\"/>\n";
  }
  out << "  </process>\n";
  const auto centres = layered_centres(graph);
  out << "  <bpmndi:BPMNDiagram id=\"BPMNDiagram_process\">\n";
  out << "    <bpmndi:BPMNPlane id=\"BPMNPlane_process\" bpmnElement=\"process\">\n";
  for (std::size_t n = 0; n < graph.nodes.size(); ++n) {
    const auto& node = graph.nodes[n];
    if (!node.alive) continue;
    const auto size = shape_size(node.element);
    out << "      <bpmndi:BPMNShape id=\"" << node.id << "_di\" bpmnElement=\""
        << node.id << "\">\n        <dc:Bounds x=\""
        << centres[n].first - size.width / 2 << "\" y=\""
        << centres[n].second - size.height / 2 << "\" width=\"" << size.width
        << "\" height=\"" << size.height << "\"/>\n      </bpmndi:BPMNShape>\n";
  }
  for (const auto& flow : graph.flows) {
    if (!flow.alive) continue;
    const auto source = shape_size(graph.nodes[flow.source].element);
    const auto target = shape_size(graph.nodes[flow.target].element);
    out << "      <bpmndi:BPMNEdge id=\"" << flow.id << "_di\" bpmnElement=\""
        << flow.id << "\">\n        <di:waypoint x=\""
        << centres[flow.source].first + source.width / 2 << "\" y=\""
        << centres[flow.source].second << "\"/>\n        <di:waypoint x=\""
        << centres[flow.target].first - target.width / 2 << "\" y=\""
        << centres[flow.target].second << "\"/>\n      </bpmndi:BPMNEdge>\n";
  }
  out << "    </bpmndi:BPMNPlane>\n  </bpmndi:BPMNDiagram>\n";
  out << "</definitions>\n";
}

// The reference's workflow-net-to-BPMN mapping: every place becomes an
// exclusive gateway (the source fed by the start event, the sink feeding the
// end event); every visible transition maps to the task of its label (one
// task per label), behind a converging parallel gateway when it has several
// input places and before a diverging one when it has several output places,
// with an exclusive gateway on a side that has no places; a silent transition
// becomes one parallel gateway when it has several input or output places
// (converging when it has several inputs) and an exclusive gateway otherwise.
// Exclusive gateways with exactly one incoming and one outgoing flow are then
// collapsed into direct flows.
BpmnGraph net_bpmn_graph(const PetriNet& net) {
  BpmnGraph graph;
  const auto places = net.place_count();
  std::optional<std::size_t> start;
  std::optional<std::size_t> end;
  if (net.has_source() && net.source() < places)
    start = graph.add("startEvent", "StartEvent", "start");
  if (net.has_sink() && net.sink() < places)
    end = graph.add("endEvent", "EndEvent", "end");
  std::vector<std::size_t> place_node(places);
  for (std::size_t p = 0; p < places; ++p)
    place_node[p] = graph.add("exclusiveGateway", "exclusiveGateway", std::nullopt, "Unspecified");
  if (start) graph.flow(*start, place_node[net.source()]);
  if (end) graph.flow(place_node[net.sink()], *end);
  std::unordered_map<std::string, std::size_t> tasks;
  for (const auto& transition : net.transitions()) {
    const auto& inputs = transition.inputs;
    const auto& outputs = transition.outputs;
    std::size_t entry = 0;
    std::size_t exit = 0;
    if (!transition.silent()) {
      const auto& label = net.activities()[*transition.activity];
      auto found = tasks.find(label);
      if (found == tasks.end())
        found = tasks.emplace(label, graph.add("task", "Activity", label)).first;
      entry = exit = found->second;
      if (inputs.size() > 1) {
        const auto join = graph.add("parallelGateway", "parallelGateway", std::nullopt, "Converging");
        graph.flow(join, entry);
        entry = join;
      }
      if (outputs.size() > 1) {
        const auto split = graph.add("parallelGateway", "parallelGateway", std::nullopt, "Diverging");
        graph.flow(exit, split);
        exit = split;
      }
      if (inputs.empty()) {
        const auto open = graph.add("exclusiveGateway", "exclusiveGateway", std::nullopt, "Unspecified");
        graph.flow(open, entry);
      }
      if (outputs.empty()) {
        const auto open = graph.add("exclusiveGateway", "exclusiveGateway", std::nullopt, "Unspecified");
        graph.flow(exit, open);
      }
    } else if (inputs.size() > 1 || outputs.size() > 1) {
      entry = exit = graph.add("parallelGateway", "parallelGateway", std::nullopt,
                               inputs.size() > 1 ? "Converging" : "Diverging");
    } else {
      entry = exit = graph.add("exclusiveGateway", "exclusiveGateway", std::nullopt, "Unspecified");
    }
    for (const auto place : inputs)
      if (place < places) graph.flow(place_node[place], entry);
    for (const auto place : outputs)
      if (place < places) graph.flow(exit, place_node[place]);
  }
  graph.collapse_pass_through_exclusive();
  return graph;
}

}  // namespace

void write_bpmn(std::ostream& out,
                const ProcessTree& tree,
                std::string_view name) {
  const auto has_interleaving = [](const ProcessTreeNode& root) {
    std::vector<const ProcessTreeNode*> pending{&root};
    while (!pending.empty()) {
      const auto* node = pending.back();
      pending.pop_back();
      if (node->op == ProcessTreeOperator::kInterleaving) return true;
      for (const auto& child : node->children) pending.push_back(&child);
    }
    return false;
  };
  if (has_interleaving(tree.root())) {
    write_bpmn(out, to_petri_net(tree), name);
    return;
  }
  BpmnBuilder builder(tree);
  write_bpmn_document(out, name, builder.build());
}

std::string to_bpmn(const ProcessTree& tree, std::string_view name) {
  std::ostringstream out;
  write_bpmn(out, tree, name);
  return out.str();
}

void write_bpmn(std::ostream& out,
                const PetriNet& net,
                std::string_view name) {
  write_bpmn_document(out, name, net_bpmn_graph(net));
}

std::string to_bpmn(const PetriNet& net, std::string_view name) {
  std::ostringstream out;
  write_bpmn(out, net, name);
  return out.str();
}

namespace {

// What the parse pass collects per flow node before the net is assembled.
enum class BpmnNodeKind {
  task,         // any task kind or call activity; silent when unnamed
  passthrough,  // intermediate events: route the token, consume no event
  start,
  end,
  exclusive,    // exclusive and event-based gateways: one in, one out
  parallel,     // parallel gateways: all in, all out
  inclusive,    // block-structured inclusive split/join pair
  subprocess,   // an embedded scope with its own start and end events
  boundary,     // event attached to an activity
};

constexpr std::size_t kNoIndex = std::numeric_limits<std::size_t>::max();

struct BpmnNode {
  std::string id;
  std::string name;
  std::string element;  // local element name (task, callActivity, ...)
  BpmnNodeKind kind = BpmnNodeKind::task;
  std::size_t child_scope = 0;  // scope index, kind == subprocess only
  std::string attached_to;
  bool interrupting = true;
  std::vector<std::size_t> in_flows;   // indices into the scope's flows
  std::vector<std::size_t> out_flows;
  // Inclusive gateways are compiled as one block: the split owns the
  // selector/control places and records which join input belongs to each
  // branch.  A gateway with one input and one output is a plain pass-through.
  std::size_t inclusive_pair = kNoIndex;
  std::vector<std::size_t> inclusive_join_flows;
};

struct BpmnFlow {
  std::string source;
  std::string target;
  std::string id;
};

// One <process> or expanded <subProcess>: sequence flows only ever connect
// nodes of the same scope.
struct BpmnScope {
  std::vector<BpmnNode> nodes;
  std::vector<BpmnFlow> flows;
  std::unordered_map<std::string, std::size_t> node_index;
};

// The element's name with any namespace prefix removed — BPMN files routinely
// qualify every element (<bpmn:task>, <bpmn2:process>, …).
std::string_view local_name(std::string_view name) {
  const auto colon = name.rfind(':');
  return colon == std::string_view::npos ? name : name.substr(colon + 1);
}

bool is_task_element(std::string_view name) {
  return name == "task" || name == "userTask" || name == "serviceTask" ||
         name == "scriptTask" || name == "manualTask" || name == "sendTask" ||
         name == "receiveTask" || name == "businessRuleTask" ||
         name == "callActivity";
}

// Returns the nodes reachable by following sequence flows from `start`.
// Flow-node reachability is enough to match a structured inclusive split with
// its converging join; sequence-flow places are introduced only afterwards.
std::vector<char> reachable_nodes(const BpmnScope& scope, std::size_t start) {
  std::vector<char> reached(scope.nodes.size(), 0);
  if (start >= scope.nodes.size()) return reached;
  std::vector<std::size_t> pending{start};
  reached[start] = 1;
  while (!pending.empty()) {
    const auto node = pending.back();
    pending.pop_back();
    for (const auto flow : scope.nodes[node].out_flows) {
      const auto target = scope.node_index.find(scope.flows[flow].target);
      if (target == scope.node_index.end()) continue;
      if (reached[target->second]) continue;
      reached[target->second] = 1;
      pending.push_back(target->second);
    }
  }
  return reached;
}

// Inclusive gateways have a faithful workflow-net representation when they
// form a block with one diverging split and one converging join.  Pairing is
// structural rather than id-based: every split branch must reach exactly one
// distinct incoming flow of the same join, and all join inputs must be
// accounted for.  This accepts arbitrary ids and nested blocks while keeping
// ambiguous graphs explicit instead of silently changing their language.
void prepare_inclusive_pairs(BpmnScope& scope) {
  std::vector<std::size_t> splits;
  std::vector<std::size_t> joins;
  for (std::size_t index = 0; index < scope.nodes.size(); ++index) {
    auto& node = scope.nodes[index];
    if (node.kind != BpmnNodeKind::inclusive) continue;
    if (node.in_flows.empty() || node.out_flows.empty()) {
      throw std::runtime_error("BPMN inclusive gateway '" + node.id +
                               "' must have incoming and outgoing flows");
    }
    if (node.in_flows.size() == 1 && node.out_flows.size() > 1) {
      splits.push_back(index);
    } else if (node.in_flows.size() > 1 && node.out_flows.size() == 1) {
      joins.push_back(index);
    } else if (node.in_flows.size() > 1 && node.out_flows.size() > 1) {
      throw std::runtime_error(
          "BPMN inclusive gateway '" + node.id +
          "' mixes split and join semantics; only block-structured pairs are supported");
    }
  }

  std::vector<char> matched_join(scope.nodes.size(), 0);
  for (const auto split_index : splits) {
    auto& split = scope.nodes[split_index];
    std::vector<std::pair<std::size_t, std::vector<std::size_t>>> candidates;
    for (const auto join_index : joins) {
      const auto& join = scope.nodes[join_index];
      std::vector<std::size_t> mapping;
      std::set<std::size_t> used;
      bool valid = true;
      for (const auto out_flow : split.out_flows) {
        const auto target_it = scope.node_index.find(scope.flows[out_flow].target);
        if (target_it == scope.node_index.end()) {
          valid = false;
          break;
        }
        const auto reached = reachable_nodes(scope, target_it->second);
        std::size_t selected = kNoIndex;
        for (const auto join_flow : join.in_flows) {
          if (used.count(join_flow) != 0) continue;
          const auto& incoming = scope.flows[join_flow];
          const auto source_it = scope.node_index.find(incoming.source);
          if (source_it == scope.node_index.end()) continue;
          // An empty branch is represented by a direct split -> join flow;
          // its incoming source is the split itself and is therefore not in
          // the branch's forward reachability set.
          const bool direct_empty = join_flow == out_flow &&
                                    incoming.source == split.id &&
                                    incoming.target == join.id;
          if (!direct_empty && !reached[source_it->second]) continue;
          if (selected != kNoIndex) {
            valid = false;
            break;
          }
          selected = join_flow;
        }
        if (!valid || selected == kNoIndex) {
          valid = false;
          break;
        }
        used.insert(selected);
        mapping.push_back(selected);
      }
      if (valid && used.size() == join.in_flows.size())
        candidates.emplace_back(join_index, std::move(mapping));
    }
    if (candidates.empty()) {
      throw std::runtime_error("BPMN inclusive gateway '" + split.id +
                               "' has no unambiguous matching join");
    }
    if (candidates.size() != 1) {
      throw std::runtime_error("BPMN inclusive gateway '" + split.id +
                               "' has multiple possible matching joins");
    }
    const auto join_index = candidates.front().first;
    if (matched_join[join_index]) {
      throw std::runtime_error("BPMN inclusive join '" +
                               scope.nodes[join_index].id +
                               "' is shared by multiple splits");
    }
    matched_join[join_index] = 1;
    split.inclusive_pair = join_index;
    split.inclusive_join_flows = std::move(candidates.front().second);
    scope.nodes[join_index].inclusive_pair = split_index;
  }

  for (const auto join_index : joins) {
    if (!matched_join[join_index]) {
      throw std::runtime_error("BPMN inclusive join '" +
                               scope.nodes[join_index].id +
                               "' has no matching split");
    }
  }
}

// Wires each scope's flows onto its nodes and assembles the workflow net,
// recursing through subprocess scopes.
class BpmnNetBuilder {
 public:
  explicit BpmnNetBuilder(std::vector<BpmnScope>& scopes) : scopes_(scopes) {}

  PetriNet build(std::size_t root_scope) {
    const auto source = net_.add_place();
    const auto sink = net_.add_place();
    net_.set_source(source);
    net_.set_sink(sink);
    convert(scopes_[root_scope], source, sink);
    net_.set_activities(std::move(activities_));
    return reduce_petri_net(net_).net;
  }

 private:
  void convert(BpmnScope& scope, std::size_t entry, std::size_t exit) {
    resolve_flows(scope);
    prepare_inclusive_pairs(scope);

    bool has_start = false;
    bool has_end = false;
    for (const auto& node : scope.nodes) {
      has_start |= node.kind == BpmnNodeKind::start;
      has_end |= node.kind == BpmnNodeKind::end;
    }
    if (!has_start) {
      throw std::runtime_error("BPMN process has no start event");
    }
    if (!has_end) {
      throw std::runtime_error("BPMN process has no end event");
    }

    // Every sequence flow holds at most one token at a time: a place.
    std::vector<std::size_t> place(scope.flows.size());
    for (auto& p : place) p = net_.add_place();

    std::unordered_map<std::string, std::vector<const BpmnNode*>> boundaries;
    for (const auto& candidate : scope.nodes) {
      if (candidate.kind == BpmnNodeKind::boundary)
        boundaries[candidate.attached_to].push_back(&candidate);
    }
    for (const auto& [attached, _] : boundaries) {
      const auto found = scope.node_index.find(attached);
      if (found == scope.node_index.end())
        throw std::runtime_error("BPMN boundary event references unknown node '" +
                                 attached + "'");
      const auto kind = scope.nodes[found->second].kind;
      if (kind != BpmnNodeKind::task && kind != BpmnNodeKind::subprocess)
        throw std::runtime_error(
            "BPMN boundary event must attach to an activity or subprocess");
    }

    for (const auto& node : scope.nodes) {
      switch (node.kind) {
        case BpmnNodeKind::start:
          // Instantiation: the scope's entry token starts every outgoing
          // flow at once (several starts are alternative instantiations).
          if (!node.in_flows.empty()) {
            throw std::runtime_error("BPMN start event '" + node.id +
                                     "' has an incoming sequence flow");
          }
          silent({entry}, out_places(node, place));
          break;
        case BpmnNodeKind::end:
          // Each token reaching an end event completes independently.
          if (!node.out_flows.empty()) {
            throw std::runtime_error("BPMN end event '" + node.id +
                                     "' has an outgoing sequence flow");
          }
          if (node.in_flows.empty()) {
            throw std::runtime_error("BPMN node '" + node.id +
                                     "' has no incoming sequence flow");
          }
          for (const auto flow : node.in_flows) {
            silent({place[flow]}, {exit});
          }
          break;
        case BpmnNodeKind::task:
        case BpmnNodeKind::passthrough: {
          std::optional<std::size_t> activity;
          if (node.kind == BpmnNodeKind::task && !node.name.empty()) {
            activity = activity_id(node.name);
          }
          const auto attached = boundaries.find(node.id);
          if (node.kind == BpmnNodeKind::task && attached != boundaries.end())
            add_atomic_with_boundaries(node, attached->second, place, activity);
          else
            add_activity_node(node, place, activity);
          break;
        }
        case BpmnNodeKind::exclusive: {
          const auto in = merge_entry(node, place);
          const auto outs = out_places(node, place);
          for (const auto out : outs) {
            silent({in}, {out});
          }
          break;
        }
        case BpmnNodeKind::parallel: {
          if (node.in_flows.empty()) {
            throw std::runtime_error("BPMN node '" + node.id +
                                     "' has no incoming sequence flow");
          }
          std::vector<std::size_t> inputs;
          inputs.reserve(node.in_flows.size());
          for (const auto flow : node.in_flows) inputs.push_back(place[flow]);
          silent(std::move(inputs), out_places(node, place));
          break;
        }
        case BpmnNodeKind::inclusive: {
          // A one-in/one-out inclusive gateway is just a routing point.  A
          // diverging gateway owns the complete selector/control block; its
          // paired converging gateway is handled there and skipped below.
          if (node.inclusive_pair != kNoIndex &&
              node.in_flows.size() == 1 && node.out_flows.size() > 1) {
            add_inclusive_block(node, scope.nodes[node.inclusive_pair], place);
          } else if (node.inclusive_pair != kNoIndex &&
                     node.in_flows.size() > 1 && node.out_flows.size() == 1) {
            // The split created the completion and join transitions already.
            continue;
          } else if (node.in_flows.size() == 1 && node.out_flows.size() == 1) {
            silent({place[node.in_flows.front()]},
                   {place[node.out_flows.front()]});
          } else {
            throw std::runtime_error("BPMN inclusive gateway '" + node.id +
                                     "' has unsupported flow arity");
          }
          break;
        }
        case BpmnNodeKind::subprocess: {
          auto& child = scopes_[node.child_scope];
          if (child.nodes.empty()) {
            // Collapsed subprocess: an atomic step, like a task.
            std::optional<std::size_t> activity;
            if (!node.name.empty()) activity = activity_id(node.name);
            const auto attached = boundaries.find(node.id);
            if (attached != boundaries.end())
              add_atomic_with_boundaries(node, attached->second, place,
                                         activity);
            else
              add_activity_node(node, place, activity);
            break;
          }
          const auto in = merge_entry(node, place);
          const auto outs = out_places(node, place);
          std::size_t local_exit;
          if (outs.size() == 1) {
            local_exit = outs.front();
          } else {
            local_exit = net_.add_place();
            silent({local_exit}, outs);
          }
          const auto attached = boundaries.find(node.id);
          if (attached == boundaries.end()) {
            convert(child, in, local_exit);
            break;
          }

          // Keep a token for the lifetime of this subprocess instance. Normal
          // completion consumes it; an interrupting boundary transition
          // consumes it and resets every place created for the child scope.
          // The child exit is private so a cancelled instance cannot later
          // leak a stale completion token into the parent flow.
          const auto active = net_.add_place();
          const auto child_entry = net_.add_place();
          const auto child_exit = net_.add_place();
          std::vector<std::size_t> armed;
          for (const auto* boundary : attached->second)
            if (!boundary->interrupting) armed.push_back(net_.add_place());
          auto entry_outputs = armed;
          entry_outputs.push_back(active);
          entry_outputs.push_back(child_entry);
          silent({in}, std::move(entry_outputs));
          const auto first_child_place = net_.place_count();
          convert(child, child_entry, child_exit);
          const auto after_child_place = net_.place_count();
          silent({active, child_exit}, {local_exit});
          std::size_t next_armed = 0;
          for (const auto* boundary : attached->second) {
            if (!boundary->in_flows.empty())
              throw std::runtime_error("BPMN boundary event has an incoming flow");
            const auto boundary_activity = boundary->name.empty()
                                               ? std::optional<std::size_t>{}
                                               : std::optional<std::size_t>{
                                                     activity_id(boundary->name)};
            if (boundary->interrupting) {
              const auto interrupt = net_.add_transition(
                  boundary_activity, {active}, out_places(*boundary, place));
              net_.add_reset_arc(interrupt, child_entry);
              net_.add_reset_arc(interrupt, child_exit);
              for (std::size_t child_place = first_child_place;
                   child_place < after_child_place; ++child_place)
                net_.add_reset_arc(interrupt, child_place);
            } else {
              // Attach the one-shot trigger to the subprocess lifetime token
              // without consuming it: both tokens are replaced on firing.
              net_.add_transition(boundary_activity,
                                  {active, armed[next_armed++]},
                                  append(out_places(*boundary, place), active));
            }
          }
          break;
        }
        case BpmnNodeKind::boundary:
          break;
      }
    }
  }

  // Attaches the flows to their endpoint nodes, in document order.
  static void resolve_flows(BpmnScope& scope) {
    for (std::size_t f = 0; f < scope.flows.size(); ++f) {
      const auto& flow = scope.flows[f];
      const auto source = scope.node_index.find(flow.source);
      const auto target = scope.node_index.find(flow.target);
      if (source == scope.node_index.end() ||
          target == scope.node_index.end()) {
        throw std::runtime_error("BPMN sequence flow '" + flow.source +
                                 "' -> '" + flow.target +
                                 "' references an unknown node");
      }
      scope.nodes[source->second].out_flows.push_back(f);
      scope.nodes[target->second].in_flows.push_back(f);
    }
  }

  // A task-like node (task, intermediate event, collapsed subprocess) as one
  // transition. A node some flow reaches consumes its merged entry and starts
  // every outgoing flow; a node with no incoming or outgoing flow — the shape
  // a disconnected activity takes in our own export — gets fresh unconnected
  // places instead, staying a dead transition the way it was a dead
  // transition in the net, so conformance keeps reporting it honestly.
  void add_activity_node(const BpmnNode& node,
                         const std::vector<std::size_t>& place,
                         std::optional<std::size_t> activity) {
    const auto in =
        node.in_flows.empty() ? net_.add_place() : merge_entry(node, place);
    auto outs = node.out_flows.empty()
                    ? std::vector<std::size_t>{net_.add_place()}
                    : out_places(node, place);
    net_.add_transition(activity, {in}, std::move(outs));
  }

  void add_atomic_with_boundaries(
      const BpmnNode& node, const std::vector<const BpmnNode*>& boundaries,
      const std::vector<std::size_t>& place,
      std::optional<std::size_t> activity) {
    if (node.in_flows.empty())
      throw std::runtime_error("BPMN boundary attachment has no incoming flow");
    const auto active = net_.add_place();
    std::vector<std::size_t> armed;
    for (const auto* boundary : boundaries)
      if (!boundary->interrupting) armed.push_back(net_.add_place());
    auto entry_outputs = armed;
    entry_outputs.push_back(active);
    silent({merge_entry(node, place)}, std::move(entry_outputs));
    auto normal_outputs = node.out_flows.empty()
                              ? std::vector<std::size_t>{net_.add_place()}
                              : out_places(node, place);
    net_.add_transition(activity, {active}, std::move(normal_outputs));
    std::size_t next_armed = 0;
    for (const auto* boundary : boundaries) {
      if (!boundary->in_flows.empty())
        throw std::runtime_error("BPMN boundary event has an incoming flow");
      const auto boundary_activity = boundary->name.empty()
                                         ? std::optional<std::size_t>{}
                                         : std::optional<std::size_t>{
                                               activity_id(boundary->name)};
      if (boundary->interrupting) {
        net_.add_transition(boundary_activity, {active},
                            out_places(*boundary, place));
      } else {
        net_.add_transition(boundary_activity, {active, armed[next_armed++]},
                            append(out_places(*boundary, place), active));
      }
    }
  }

  static std::vector<std::size_t> append(std::vector<std::size_t> values,
                                         std::size_t value) {
    values.push_back(value);
    return values;
  }

  // The single place a node consumes from: its one incoming flow, or — since
  // several incoming flows fire a non-gateway node once per token, and an
  // exclusive gateway merges the same way — a hub place fed silently by each.
  std::size_t merge_entry(const BpmnNode& node,
                          const std::vector<std::size_t>& place) {
    if (node.in_flows.empty()) {
      throw std::runtime_error("BPMN node '" + node.id +
                               "' has no incoming sequence flow");
    }
    if (node.in_flows.size() == 1) return place[node.in_flows.front()];
    const auto hub = net_.add_place();
    for (const auto flow : node.in_flows) {
      silent({place[flow]}, {hub});
    }
    return hub;
  }

  std::vector<std::size_t> out_places(const BpmnNode& node,
                                      const std::vector<std::size_t>& place) {
    if (node.out_flows.empty()) {
      throw std::runtime_error("BPMN node '" + node.id +
                               "' has no outgoing sequence flow");
    }
    std::vector<std::size_t> outs;
    outs.reserve(node.out_flows.size());
    for (const auto flow : node.out_flows) outs.push_back(place[flow]);
    return outs;
  }

  // Compiles one block-structured inclusive split/join pair.  The split
  // makes one selector token per branch and an initially unstarted control
  // token.  Each selector may skip its branch or execute it; the first
  // selected branch moves the control token to `started`, while later
  // branches preserve it.  The join waits for every branch completion and
  // the started token, so exactly the non-empty subsets of branches are
  // represented without enumerating a power set.
  void add_inclusive_block(
      const BpmnNode& split, const BpmnNode& join,
      const std::vector<std::size_t>& place) {
    if (split.in_flows.size() != 1 || split.out_flows.size() < 2 ||
        join.in_flows.size() != split.out_flows.size() ||
        join.out_flows.size() != 1 ||
        split.inclusive_join_flows.size() != split.out_flows.size()) {
      throw std::runtime_error(
          "BPMN inclusive split/join pair is not block structured");
    }

    const auto not_started = net_.add_place();
    const auto started = net_.add_place();
    std::vector<std::size_t> selectors;
    std::vector<std::size_t> completions;
    selectors.reserve(split.out_flows.size());
    completions.reserve(split.out_flows.size());
    for (std::size_t i = 0; i < split.out_flows.size(); ++i) {
      selectors.push_back(net_.add_place());
      completions.push_back(net_.add_place());
    }

    std::vector<std::size_t> split_outputs = selectors;
    split_outputs.push_back(not_started);
    silent({place[split.in_flows.front()]}, std::move(split_outputs));

    for (std::size_t i = 0; i < split.out_flows.size(); ++i) {
      // Skip this branch.
      silent({selectors[i]}, {completions[i]});
      // Select the first branch, or a later branch after another one started.
      silent({selectors[i], not_started},
             {place[split.out_flows[i]], started});
      silent({selectors[i], started},
             {place[split.out_flows[i]], started});
    }

    for (std::size_t i = 0; i < split.inclusive_join_flows.size(); ++i) {
      silent({place[split.inclusive_join_flows[i]]}, {completions[i]});
    }
    std::vector<std::size_t> join_inputs = completions;
    join_inputs.push_back(started);
    silent(std::move(join_inputs), {place[join.out_flows.front()]});
  }

  void silent(std::vector<std::size_t> inputs, std::vector<std::size_t> outputs) {
    net_.add_transition(std::nullopt, std::move(inputs), std::move(outputs));
  }

  // Tasks with the same label share one activity, in first-seen order.
  std::size_t activity_id(const std::string& label) {
    const auto [it, inserted] = activity_ids_.emplace(label, activities_.size());
    if (inserted) activities_.push_back(label);
    return it->second;
  }

  std::vector<BpmnScope>& scopes_;
  PetriNet net_;
  std::vector<std::string> activities_;
  std::unordered_map<std::string, std::size_t> activity_ids_;
};


// The reference's BPMN-to-Petri conversion for processes made only of start
// and end events, tasks (any kind but call activities), exclusive gateways and
// parallel gateways (docs/reference-2.7.22-parity.md, "BPMN to Petri net
// conversion"). Every node becomes ent_<id> -> <id> -> exi_<id> (visible under
// a non-empty task name, silent otherwise); every sequence flow becomes a
// place <flow id> fed by sfl_<flow id> from the source's exi place and drained
// by tfl_<flow id> into the target's ent place, except that a parallel gateway
// with several outgoing (incoming) flows forks (joins) them with one silent
// transition. Start events are fed from `source`, end events drain into
// `sink`. The two-pass simple reduction then merges silent transitions into a
// sole producer (first pass) or consumer (second pass), in creation order.
bool reference_convertible(const BpmnScope& scope) {
  for (const auto& node : scope.nodes) {
    switch (node.kind) {
      case BpmnNodeKind::start:
      case BpmnNodeKind::end:
      case BpmnNodeKind::parallel:
        break;
      case BpmnNodeKind::task:
        if (node.element == "callActivity") return false;
        break;
      case BpmnNodeKind::exclusive:
        if (node.element != "exclusiveGateway") return false;
        break;
      default:
        return false;
    }
  }
  return true;
}

class ReferenceBpmnNetBuilder {
 public:
  explicit ReferenceBpmnNetBuilder(const BpmnScope& scope) : scope_(scope) {}

  PetriNet build() {
    const auto source = place("source");
    const auto sink = place("sink");
    std::vector<std::size_t> entry(scope_.nodes.size());
    std::vector<std::size_t> exit(scope_.nodes.size());
    std::vector<std::size_t> out_count(scope_.nodes.size(), 0);
    std::vector<std::size_t> in_count(scope_.nodes.size(), 0);
    std::vector<std::pair<std::size_t, std::size_t>> ends(scope_.flows.size());
    for (std::size_t f = 0; f < scope_.flows.size(); ++f) {
      const auto& flow = scope_.flows[f];
      const auto source_node = scope_.node_index.find(flow.source);
      const auto target_node = scope_.node_index.find(flow.target);
      if (source_node == scope_.node_index.end() ||
          target_node == scope_.node_index.end()) {
        throw std::runtime_error("BPMN sequence flow '" + flow.source +
                                 "' -> '" + flow.target +
                                 "' references an unknown node");
      }
      ends[f] = {source_node->second, target_node->second};
      ++out_count[source_node->second];
      ++in_count[target_node->second];
    }
    for (std::size_t n = 0; n < scope_.nodes.size(); ++n) {
      const auto& node = scope_.nodes[n];
      entry[n] = place("ent_" + node.id);
      exit[n] = place("exi_" + node.id);
      std::optional<std::string> label;
      if (node.kind == BpmnNodeKind::task && !node.name.empty()) label = node.name;
      transition(node.id, std::move(label), {entry[n]}, {exit[n]});
      if (node.kind == BpmnNodeKind::start) connector({source}, {entry[n]});
      if (node.kind == BpmnNodeKind::end) connector({exit[n]}, {sink});
    }
    std::vector<std::size_t> flow_place(scope_.flows.size());
    for (std::size_t f = 0; f < scope_.flows.size(); ++f)
      flow_place[f] = place(scope_.flows[f].id);
    std::vector<bool> forked(scope_.nodes.size(), false);
    std::vector<bool> joined(scope_.nodes.size(), false);
    for (std::size_t f = 0; f < scope_.flows.size(); ++f) {
      const auto [from, to] = ends[f];
      const auto& id = scope_.flows[f].id;
      if (scope_.nodes[from].kind == BpmnNodeKind::parallel && out_count[from] > 1) {
        if (!forked[from]) {
          forked[from] = true;
          std::vector<std::size_t> outputs;
          for (std::size_t g = 0; g < scope_.flows.size(); ++g)
            if (ends[g].first == from) outputs.push_back(flow_place[g]);
          connector({exit[from]}, std::move(outputs));
        }
      } else {
        transition("sfl_" + id, std::nullopt, {exit[from]}, {flow_place[f]});
      }
      if (scope_.nodes[to].kind == BpmnNodeKind::parallel && in_count[to] > 1) {
        if (!joined[to]) {
          joined[to] = true;
          std::vector<std::size_t> inputs;
          for (std::size_t g = 0; g < scope_.flows.size(); ++g)
            if (ends[g].second == to) inputs.push_back(flow_place[g]);
          connector(std::move(inputs), {entry[to]});
        }
      } else {
        transition("tfl_" + id, std::nullopt, {flow_place[f]}, {entry[to]});
      }
    }
    reduce();
    return assemble(source, sink);
  }

 private:
  struct Node {
    std::optional<std::string> label;
    std::vector<std::size_t> inputs;
    std::vector<std::size_t> outputs;
    std::string name;
    bool alive = true;
  };

  std::size_t place(std::string name) {
    place_names_.push_back(std::move(name));
    return place_names_.size() - 1;
  }
  void transition(std::string name, std::optional<std::string> label,
                  std::vector<std::size_t> inputs, std::vector<std::size_t> outputs) {
    nodes_.push_back({std::move(label), std::move(inputs), std::move(outputs),
                      std::move(name), true});
  }
  // The reference names these silent connectors with random UUID4 strings;
  // Minera writes deterministic UUID-shaped names in creation order.
  void connector(std::vector<std::size_t> inputs, std::vector<std::size_t> outputs) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string suffix(12, '0');
    auto value = ++connector_counter_;
    for (std::size_t i = 12; i-- > 0 && value;) {
      suffix[i] = kHex[value % 16];
      value /= 16;
    }
    transition("00000000-0000-4000-8000-" + suffix, std::nullopt,
               std::move(inputs), std::move(outputs));
  }

  static void erase_one(std::vector<std::size_t>& values, std::size_t value) {
    const auto it = std::find(values.begin(), values.end(), value);
    if (it != values.end()) values.erase(it);
  }
  static void replace_all(std::vector<std::size_t>& values, std::size_t from,
                          std::size_t to) {
    for (auto& value : values)
      if (value == from) value = to;
  }

  // Silent transitions in a cycle without a visible transition can meet
  // themselves as sole producer/consumer; the reference then leaves dangling
  // arcs behind, Minera keeps such transitions (declared).
  void reduce() {
    producers_.assign(place_names_.size(), {});
    consumers_.assign(place_names_.size(), {});
    place_alive_.assign(place_names_.size(), true);
    for (std::size_t t = 0; t < nodes_.size(); ++t) {
      for (const auto p : nodes_[t].inputs) consumers_[p].push_back(t);
      for (const auto p : nodes_[t].outputs) producers_[p].push_back(t);
    }
    for (bool changed = true; changed;) {
      changed = false;
      for (std::size_t t = 0; t < nodes_.size(); ++t) {
        auto& node = nodes_[t];
        if (!node.alive || node.label || node.inputs.size() != 1) continue;
        const auto p = node.inputs.front();
        if (producers_[p].size() != 1 || consumers_[p].size() != 1) continue;
        const auto u = producers_[p].front();
        if (u == t) continue;
        erase_one(nodes_[u].outputs, p);
        for (const auto q : node.outputs) {
          nodes_[u].outputs.push_back(q);
          replace_all(producers_[q], t, u);
        }
        producers_[p].clear();
        consumers_[p].clear();
        place_alive_[p] = false;
        node.alive = false;
        changed = true;
      }
    }
    for (bool changed = true; changed;) {
      changed = false;
      for (std::size_t t = 0; t < nodes_.size(); ++t) {
        auto& node = nodes_[t];
        if (!node.alive || node.label || node.outputs.size() != 1) continue;
        const auto q = node.outputs.front();
        if (producers_[q].size() != 1 || consumers_[q].size() != 1) continue;
        const auto v = consumers_[q].front();
        if (v == t) continue;
        erase_one(nodes_[v].inputs, q);
        for (const auto p : node.inputs) {
          nodes_[v].inputs.push_back(p);
          replace_all(consumers_[p], t, v);
        }
        producers_[q].clear();
        consumers_[q].clear();
        place_alive_[q] = false;
        node.alive = false;
        changed = true;
      }
    }
  }

  PetriNet assemble(std::size_t source, std::size_t sink) {
    PetriNet net;
    std::vector<std::size_t> remap(place_names_.size(), 0);
    std::vector<std::string> names;
    for (std::size_t p = 0; p < place_names_.size(); ++p) {
      if (!place_alive_[p]) continue;
      remap[p] = net.add_place();
      names.push_back(place_names_[p]);
    }
    net.set_place_names(std::move(names));
    net.set_source(remap[source]);
    net.set_sink(remap[sink]);
    std::vector<std::string> activities;
    std::unordered_map<std::string, std::size_t> activity_ids;
    PetriTransitionNameList transition_names;
    for (const auto& node : nodes_) {
      if (!node.alive) continue;
      std::optional<std::size_t> activity;
      if (node.label) {
        const auto [it, inserted] = activity_ids.emplace(*node.label, activities.size());
        if (inserted) activities.push_back(*node.label);
        activity = it->second;
      }
      std::vector<std::size_t> inputs;
      std::vector<std::size_t> outputs;
      for (const auto p : node.inputs) inputs.push_back(remap[p]);
      for (const auto p : node.outputs) outputs.push_back(remap[p]);
      net.add_transition(activity, std::move(inputs), std::move(outputs));
      transition_names.emplace_back(node.name);
    }
    net.set_activities(std::move(activities));
    net.set_transition_names(std::move(transition_names));
    return net;
  }

  const BpmnScope& scope_;
  std::vector<std::string> place_names_;
  std::vector<Node> nodes_;
  std::vector<std::vector<std::size_t>> producers_;
  std::vector<std::vector<std::size_t>> consumers_;
  std::vector<bool> place_alive_;
  std::size_t connector_counter_ = 0;
};

}  // namespace

PetriNet parse_bpmn(std::string_view xml) {
  detail::XmlPullParser parser(xml);

  std::vector<BpmnScope> scopes;
  std::vector<std::size_t> processes;    // top-level <process> scopes, in order
  std::vector<std::size_t> scope_stack;  // open <process>/<subProcess> scopes

  // Skips the rest of an element whose subtree must not be interpreted —
  // extension elements carry foreign markup whose local names could collide.
  const auto skip_subtree = [&parser] {
    std::size_t depth = 1;
    while (depth > 0) {
      const auto token = parser.next();
      if (token.kind == detail::XmlPullParser::Kind::EndOfInput) break;
      if (token.kind == detail::XmlPullParser::Kind::StartElement &&
          !token.self_closing) {
        ++depth;
      } else if (token.kind == detail::XmlPullParser::Kind::EndElement) {
        --depth;
      }
    }
  };

  const auto add_node = [&](BpmnScope& scope, BpmnNodeKind kind,
                            const detail::XmlPullParser::Token& token,
                            std::size_t child_scope = 0) {
    BpmnNode node;
    node.kind = kind;
    node.child_scope = child_scope;
    const auto* id = detail::attribute(token.attributes, "id");
    if (!id) {
      throw std::runtime_error("BPMN flow node is missing its id");
    }
    node.id = *id;
    node.element = std::string(local_name(token.name));
    if (const auto* name = detail::attribute(token.attributes, "name")) {
      node.name = *name;
    }
    if (!scope.node_index.emplace(node.id, scope.nodes.size()).second) {
      throw std::runtime_error("BPMN has a duplicate node id '" + node.id +
                               "'");
    }
    scope.nodes.push_back(std::move(node));
  };

  while (true) {
    const auto token = parser.next();
    if (token.kind == detail::XmlPullParser::Kind::EndOfInput) break;
    if (token.kind == detail::XmlPullParser::Kind::CharacterData) continue;

    const auto name = local_name(token.name);
    if (token.kind == detail::XmlPullParser::Kind::EndElement) {
      if ((name == "process" || name == "subProcess" ||
           name == "transaction") &&
          !scope_stack.empty()) {
        scope_stack.pop_back();
      }
      continue;
    }

    if (name == "extensionElements") {
      if (!token.self_closing) skip_subtree();
      continue;
    }
    if (name == "process") {
      scopes.emplace_back();
      processes.push_back(scopes.size() - 1);
      if (!token.self_closing) scope_stack.push_back(scopes.size() - 1);
      continue;
    }
    if (scope_stack.empty()) continue;  // outside any process (diagram, …)
    auto& scope = scopes[scope_stack.back()];

    if (name == "sequenceFlow") {
      const auto* source = detail::attribute(token.attributes, "sourceRef");
      const auto* target = detail::attribute(token.attributes, "targetRef");
      if (!source || !target) {
        throw std::runtime_error(
            "BPMN sequence flow is missing its sourceRef or targetRef");
      }
      const auto* flow_id = detail::attribute(token.attributes, "id");
      scope.flows.push_back({*source, *target, flow_id ? *flow_id : std::string()});
    } else if (is_task_element(name)) {
      add_node(scope, BpmnNodeKind::task, token);
    } else if (name == "startEvent") {
      add_node(scope, BpmnNodeKind::start, token);
    } else if (name == "endEvent") {
      add_node(scope, BpmnNodeKind::end, token);
    } else if (name == "intermediateThrowEvent" ||
               name == "intermediateCatchEvent") {
      add_node(scope, BpmnNodeKind::passthrough, token);
    } else if (name == "exclusiveGateway" || name == "eventBasedGateway") {
      add_node(scope, BpmnNodeKind::exclusive, token);
    } else if (name == "parallelGateway") {
      add_node(scope, BpmnNodeKind::parallel, token);
    } else if (name == "inclusiveGateway") {
      add_node(scope, BpmnNodeKind::inclusive, token);
    } else if (name == "subProcess" || name == "transaction") {
      const auto* triggered =
          detail::attribute(token.attributes, "triggeredByEvent");
      if (triggered && *triggered == "true") {
        throw std::runtime_error(
            "BPMN event subprocesses are not supported; only regular flow "
            "can be checked");
      }
      scopes.emplace_back();
      const auto child = scopes.size() - 1;
      // scopes may have reallocated: re-fetch the enclosing scope.
      add_node(scopes[scope_stack.back()], BpmnNodeKind::subprocess, token,
               child);
      if (!token.self_closing) scope_stack.push_back(child);
    } else if (name == "complexGateway") {
      throw std::runtime_error(
          "BPMN complex gateways are not supported; their token semantics "
          "do not fit a workflow net");
    } else if (name == "boundaryEvent") {
      const auto* attached = detail::attribute(token.attributes, "attachedToRef");
      if (!attached)
        throw std::runtime_error("BPMN boundary event is missing attachedToRef");
      add_node(scope, BpmnNodeKind::boundary, token);
      scope.nodes.back().attached_to = *attached;
      const auto* cancel = detail::attribute(token.attributes, "cancelActivity");
      if (cancel && *cancel != "true" && *cancel != "false")
        throw std::runtime_error(
            "BPMN boundary event cancelActivity must be true or false");
      scope.nodes.back().interrupting = !cancel || *cancel == "true";
    } else if (name == "adHocSubProcess") {
      throw std::runtime_error("BPMN ad-hoc subprocesses are not supported");
    }
    // Anything else (lanes, data objects, annotations, …) is ignored.
  }

  // The first process that actually models flow; pools that only reference
  // another participant stay empty and are skipped.
  for (const auto scope : processes) {
    if (!scopes[scope].nodes.empty()) {
      if (reference_convertible(scopes[scope]))
        return ReferenceBpmnNetBuilder(scopes[scope]).build();
      return BpmnNetBuilder(scopes).build(scope);
    }
  }
  throw std::runtime_error("BPMN input contains no process (or an empty one)");
}

PetriNet import_bpmn(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("Unable to open BPMN file: " + path.string());
  }
  std::string xml((std::istreambuf_iterator<char>(input)),
                  std::istreambuf_iterator<char>());
  if (xml.size() >= 3 && static_cast<unsigned char>(xml[0]) == 0xEF &&
      static_cast<unsigned char>(xml[1]) == 0xBB &&
      static_cast<unsigned char>(xml[2]) == 0xBF) {
    xml.erase(0, 3);
  }
  return parse_bpmn(xml);
}

bool looks_like_bpmn(std::string_view xml) {
  try {
    detail::XmlPullParser parser(xml);
    const auto token = parser.next();
    return token.kind == detail::XmlPullParser::Kind::StartElement &&
           local_name(token.name) == "definitions";
  } catch (const std::exception&) {
    return false;
  }
}

}  // namespace minera
