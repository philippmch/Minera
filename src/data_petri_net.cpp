#include "minera/data_petri_net.hpp"

#include <algorithm>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "minera/json_export.hpp"

namespace minera {
namespace {

struct GuardPath {
  std::string expression;
  std::set<std::string> variables;
};

struct GuardGroup {
  std::vector<std::string> clauses;
  std::set<std::string> variables;
};

void write_json_string(std::ostream& out, std::string_view value) {
  out << '"';
  for (const unsigned char c : value) {
    switch (c) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (c < 0x20) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
              << static_cast<unsigned int>(c) << std::dec << std::setfill(' ');
        } else {
          out << static_cast<char>(c);
        }
    }
  }
  out << '"';
}

std::string quote_guard_value(std::string_view value) {
  std::ostringstream out;
  out << '"';
  for (const char c : value) {
    if (c == '\\' || c == '"') out << '\\';
    out << c;
  }
  out << '"';
  return out.str();
}

std::string split_expression(const DecisionSplit& split, bool then_side) {
  std::ostringstream out;
  if (split.numeric) {
    out << split.key << " <= " << std::setprecision(17) << split.threshold;
  } else {
    out << split.key << " == " << quote_guard_value(split.value);
  }
  const auto condition = out.str();
  return then_side ? condition : "!(" + condition + ")";
}

std::string join_conditions(const std::vector<std::string>& conditions) {
  if (conditions.empty()) return "true";
  std::string result;
  for (std::size_t i = 0; i < conditions.size(); ++i) {
    if (i != 0) result += " && ";
    result += '(' + conditions[i] + ')';
  }
  return result;
}

void collect_paths(const std::vector<DecisionNode>& nodes, std::size_t index,
                   std::vector<std::string> conditions,
                   std::set<std::string> variables,
                   std::vector<std::vector<GuardPath>>& by_branch) {
  if (index >= nodes.size())
    throw std::invalid_argument("data Petri net decision tree index is invalid");
  const auto& node = nodes[index];
  if (node.then_child < 0 && node.else_child < 0) {
    if (node.branch_counts.empty()) return;
    const auto branch = static_cast<std::size_t>(
        std::max_element(node.branch_counts.begin(), node.branch_counts.end()) -
        node.branch_counts.begin());
    if (branch >= by_branch.size())
      throw std::invalid_argument("data Petri net decision branch is invalid");
    by_branch[branch].push_back(
        GuardPath{join_conditions(conditions), std::move(variables)});
    return;
  }
  if (node.then_child < 0 || node.else_child < 0 || node.split.key.empty()) {
    throw std::invalid_argument("data Petri net decision split is malformed");
  }
  variables.insert(node.split.key);
  auto then_conditions = conditions;
  then_conditions.push_back(split_expression(node.split, true));
  collect_paths(nodes, static_cast<std::size_t>(node.then_child),
                std::move(then_conditions), variables, by_branch);
  auto else_conditions = std::move(conditions);
  else_conditions.push_back(split_expression(node.split, false));
  collect_paths(nodes, static_cast<std::size_t>(node.else_child),
                std::move(else_conditions), std::move(variables), by_branch);
}

void write_size_list(std::ostream& out, const std::vector<std::size_t>& values) {
  out << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i != 0) out << ',';
    out << values[i];
  }
  out << ']';
}

void write_string_list(std::ostream& out,
                       const std::vector<std::string>& values) {
  out << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i != 0) out << ',';
    write_json_string(out, values[i]);
  }
  out << ']';
}

}  // namespace

DataPetriNet mine_data_petri_net(const EventLog& log, const PetriNet& net,
                                 const DataPetriNetOptions& options) {
  DataPetriNet result;
  result.net = net;
  const auto learned_decisions = mine_decisions(log, net);
  result.decisions = learned_decisions;

  std::vector<std::vector<std::size_t>> outgoing(net.place_count());
  for (std::size_t transition = 0; transition < net.transitions().size();
       ++transition) {
    for (const auto place : net.transitions()[transition].inputs) {
      if (place >= outgoing.size())
        throw std::invalid_argument("data Petri net transition place is invalid");
      outgoing[place].push_back(transition);
    }
  }

  std::map<std::size_t, std::vector<GuardGroup>> groups;
  for (const auto& point : learned_decisions.decision_points) {
    if (point.place >= outgoing.size())
      throw std::invalid_argument("data Petri net decision place is invalid");
    const auto& choices = outgoing[point.place];
    if (choices.size() != point.branches.size())
      throw std::invalid_argument("data Petri net decision branch count differs");
    if (point.nodes.empty()) continue;

    std::vector<std::vector<GuardPath>> paths(point.branches.size());
    collect_paths(point.nodes, 0, {}, {}, paths);
    for (std::size_t branch = 0; branch < paths.size(); ++branch) {
      if (paths[branch].empty()) continue;
      GuardGroup group;
      for (const auto& path : paths[branch]) {
        group.clauses.push_back(path.expression);
        group.variables.insert(path.variables.begin(), path.variables.end());
      }
      groups[choices[branch]].push_back(std::move(group));
    }
  }

  for (auto& [transition, transition_groups] : groups) {
    DataPetriNetTransition guarded;
    guarded.transition_index = transition;
    for (std::size_t i = 0; i < transition_groups.size(); ++i) {
      const auto& group = transition_groups[i];
      if (i != 0) guarded.guard += " && ";
      guarded.guard += '(';
      for (std::size_t j = 0; j < group.clauses.size(); ++j) {
        if (j != 0) guarded.guard += " || ";
        guarded.guard += '(' + group.clauses[j] + ')';
      }
      guarded.guard += ')';
      guarded.read_variables.insert(guarded.read_variables.end(),
                                    group.variables.begin(),
                                    group.variables.end());
    }
    std::sort(guarded.read_variables.begin(), guarded.read_variables.end());
    guarded.read_variables.erase(
        std::unique(guarded.read_variables.begin(), guarded.read_variables.end()),
        guarded.read_variables.end());
    result.guarded_transitions.push_back(std::move(guarded));
  }
  if (!options.include_decision_diagnostics) {
    result.decisions.decision_points.clear();
  }
  return result;
}

std::string serialize_data_petri_net_json(const DataPetriNet& result,
                                          std::string_view miner) {
  const auto& net = result.net;
  std::map<std::size_t, const DataPetriNetTransition*> guarded;
  for (const auto& transition : result.guarded_transitions) {
    if (transition.transition_index >= net.transitions().size())
      throw std::invalid_argument("data Petri net guard transition is invalid");
    if (!guarded.emplace(transition.transition_index, &transition).second)
      throw std::invalid_argument("data Petri net has duplicate guard transition");
  }

  std::ostringstream out;
  out << "{\"family\":\"DATA_PETRI_NET\",\"variant\":\""
      << result.variant << "\"";
  if (!miner.empty()) {
    out << ",\"miner\":";
    write_json_string(out, miner);
  }
  out << ",\"place_count\":" << net.place_count()
      << ",\"source\":" << net.source() << ",\"sink\":" << net.sink()
      << ",\"activities\":[";
  for (std::size_t i = 0; i < net.activities().size(); ++i) {
    if (i != 0) out << ',';
    write_json_string(out, net.activities()[i]);
  }
  out << "],\"transitions\":[";
  for (std::size_t i = 0; i < net.transitions().size(); ++i) {
    if (i != 0) out << ',';
    const auto& transition = net.transitions()[i];
    out << "{\"index\":" << i << ",\"activity\":";
    if (transition.silent())
      out << "null";
    else
      write_json_string(out, net.activities()[*transition.activity]);
    out << ",\"silent\":" << (transition.silent() ? "true" : "false")
        << ",\"inputs\":";
    write_size_list(out, transition.inputs);
    out << ",\"outputs\":";
    write_size_list(out, transition.outputs);
    out << ",\"guard\":";
    const auto it = guarded.find(i);
    if (it == guarded.end()) {
      out << "null,\"read_variables\":[],\"write_variables\":[]";
    } else {
      write_json_string(out, it->second->guard);
      out << ",\"read_variables\":";
      write_string_list(out, it->second->read_variables);
      out << ",\"write_variables\":";
      write_string_list(out, it->second->write_variables);
    }
    out << '}';
  }
  out << "],\"decisions\":" << decisions_json(result.decisions) << '}';
  return out.str();
}

}  // namespace minera
