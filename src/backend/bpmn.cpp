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
      case ProcessTreeOperator::kInclusiveChoice:
        return emit_gateway(node, "inclusiveGateway");
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

// Emits the BPMN document wrapper shared by the tree- and net-based builders.
void write_bpmn_document(std::ostream& out, std::string_view name,
                         const std::string& nodes, const std::string& flows) {
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  out << "<definitions "
         "xmlns=\"http://www.omg.org/spec/BPMN/20100524/MODEL\" "
         "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
         "targetNamespace=\"http://minera.dev/bpmn\" id=\"definitions\">\n";
  out << "  <process id=\"process\" name=\"" << xml_escaped(name)
      << "\" isExecutable=\"false\">\n";
  out << nodes;
  out << flows;
  out << "  </process>\n";
  out << "</definitions>\n";
}

// Converts a workflow net to BPMN by its token semantics: a transition joins
// (forks) its places in parallel, a place hands its token to exactly one
// consumer. Each net element maps to a chain of zero or more BPMN nodes; an
// element that needs none (a trivial place, a silent one-in-one-out
// transition) stays a pass-through that later collapses into direct flows.
class NetBpmnBuilder {
 public:
  explicit NetBpmnBuilder(const PetriNet& net) : net_(net) {}

  void build() {
    const std::size_t places = net_.place_count();
    const auto& transitions = net_.transitions();
    const std::size_t elements = places + transitions.size();

    // Element graph: places are 0..places-1, transition t is places + t.
    successors_.assign(elements, {});
    std::vector<std::size_t> place_in(places, 0);
    std::vector<std::size_t> place_out(places, 0);
    for (std::size_t t = 0; t < transitions.size(); ++t) {
      for (const auto place : transitions[t].inputs) {
        successors_[place].push_back(places + t);
        ++place_out[place];
      }
      for (const auto place : transitions[t].outputs) {
        successors_[places + t].push_back(place);
        ++place_in[place];
      }
    }

    entry_.assign(elements, {});
    exit_.assign(elements, {});

    for (std::size_t p = 0; p < places; ++p) {
      if (p == net_.source()) {
        const auto start = fresh("StartEvent");
        nodes_ << "    <startEvent id=\"" << start << "\"/>\n";
        entry_[p] = exit_[p] = start;
        if (place_out[p] > 1) {
          // A start event should emit one token; the place's choice between
          // several consumers becomes an explicit exclusive split.
          const auto split = fresh("exclusiveGateway");
          nodes_ << "    <exclusiveGateway id=\"" << split << "\"/>\n";
          flow(start, split);
          exit_[p] = split;
        }
      } else if (p == net_.sink()) {
        const auto end = fresh("EndEvent");
        nodes_ << "    <endEvent id=\"" << end << "\"/>\n";
        entry_[p] = exit_[p] = end;
        if (place_in[p] > 1) {
          const auto join = fresh("exclusiveGateway");
          nodes_ << "    <exclusiveGateway id=\"" << join << "\"/>\n";
          flow(join, end);
          entry_[p] = join;
        }
      } else if (place_in[p] > 1 || place_out[p] > 1) {
        // One gateway serves as XOR join and split at once.
        const auto gateway = fresh("exclusiveGateway");
        nodes_ << "    <exclusiveGateway id=\"" << gateway << "\"/>\n";
        entry_[p] = exit_[p] = gateway;
      }
      // else: a trivial connector place — pass-through.
    }

    for (std::size_t t = 0; t < transitions.size(); ++t) {
      const auto& transition = transitions[t];
      std::vector<std::string> chain;
      if (transition.inputs.size() > 1) {
        const auto join = fresh("parallelGateway");
        nodes_ << "    <parallelGateway id=\"" << join << "\"/>\n";
        chain.push_back(join);
      }
      if (!transition.silent()) {
        const auto task = fresh("Activity");
        nodes_ << "    <task id=\"" << task << "\" name=\""
               << xml_escaped(net_.activities()[*transition.activity])
               << "\"/>\n";
        chain.push_back(task);
      }
      if (transition.outputs.size() > 1) {
        const auto split = fresh("parallelGateway");
        nodes_ << "    <parallelGateway id=\"" << split << "\"/>\n";
        chain.push_back(split);
      }
      for (std::size_t i = 1; i < chain.size(); ++i) {
        flow(chain[i - 1], chain[i]);
      }
      if (!chain.empty()) {
        entry_[places + t] = chain.front();
        exit_[places + t] = chain.back();
      }
      // else: a silent one-in-one-out router — pass-through.
    }

    // Connect the real nodes, walking through pass-through elements. The
    // visited set guards against cycles made purely of pass-throughs (a
    // behaviorless silent loop in a degenerate net).
    std::vector<char> visited(elements, 0);
    for (std::size_t element = 0; element < elements; ++element) {
      if (exit_[element].empty()) continue;
      std::fill(visited.begin(), visited.end(), 0);
      for (const auto successor : successors_[element]) {
        connect(exit_[element], successor, visited);
      }
    }
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

  void connect(const std::string& from, std::size_t element,
               std::vector<char>& visited) {
    if (!entry_[element].empty()) {
      flow(from, entry_[element]);
      return;
    }
    if (visited[element]) return;
    visited[element] = 1;
    for (const auto successor : successors_[element]) {
      connect(from, successor, visited);
    }
  }

  const PetriNet& net_;
  std::ostringstream nodes_;
  std::ostringstream flows_;
  int counter_ = 0;
  std::vector<std::vector<std::size_t>> successors_;
  std::vector<std::string> entry_;  // empty string = pass-through element
  std::vector<std::string> exit_;
};

}  // namespace

void write_bpmn(std::ostream& out,
                const ProcessTree& tree,
                std::string_view name) {
  BpmnBuilder builder(tree);
  builder.build();
  write_bpmn_document(out, name, builder.nodes(), builder.flows());
}

std::string to_bpmn(const ProcessTree& tree, std::string_view name) {
  std::ostringstream out;
  write_bpmn(out, tree, name);
  return out.str();
}

void write_bpmn(std::ostream& out,
                const PetriNet& net,
                std::string_view name) {
  NetBpmnBuilder builder(net);
  builder.build();
  write_bpmn_document(out, name, builder.nodes(), builder.flows());
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
  boundary,     // interrupting event attached to an atomic activity
};

constexpr std::size_t kNoIndex = std::numeric_limits<std::size_t>::max();

struct BpmnNode {
  std::string id;
  std::string name;
  BpmnNodeKind kind = BpmnNodeKind::task;
  std::size_t child_scope = 0;  // scope index, kind == subprocess only
  std::string attached_to;
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
          if (boundaries.contains(node.id))
            throw std::runtime_error(
                "BPMN boundary events on expanded subprocesses are not supported");
          const auto in = merge_entry(node, place);
          const auto outs = out_places(node, place);
          std::size_t local_exit;
          if (outs.size() == 1) {
            local_exit = outs.front();
          } else {
            local_exit = net_.add_place();
            silent({local_exit}, outs);
          }
          convert(child, in, local_exit);
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
    silent({merge_entry(node, place)}, {active});
    auto normal_outputs = node.out_flows.empty()
                              ? std::vector<std::size_t>{net_.add_place()}
                              : out_places(node, place);
    net_.add_transition(activity, {active}, std::move(normal_outputs));
    for (const auto* boundary : boundaries) {
      if (!boundary->in_flows.empty())
        throw std::runtime_error("BPMN boundary event has an incoming flow");
      silent({active}, out_places(*boundary, place));
    }
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
      scope.flows.push_back({*source, *target});
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
      const auto* cancel = detail::attribute(token.attributes, "cancelActivity");
      if (cancel && *cancel == "false")
        throw std::runtime_error(
            "BPMN non-interrupting boundary events are not supported");
      add_node(scope, BpmnNodeKind::boundary, token);
      scope.nodes.back().attached_to = *attached;
    } else if (name == "adHocSubProcess") {
      throw std::runtime_error("BPMN ad-hoc subprocesses are not supported");
    }
    // Anything else (lanes, data objects, annotations, …) is ignored.
  }

  // The first process that actually models flow; pools that only reference
  // another participant stay empty and are skipped.
  for (const auto scope : processes) {
    if (!scopes[scope].nodes.empty()) {
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
