#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "minera/dfg_alignment.hpp"

namespace {
using namespace minera;

std::string quote(const std::string& value) {
  constexpr char hex[] = "0123456789abcdef";
  std::string result = "\"";
  for (const unsigned char c : value) {
    if (c == '"' || c == '\\') { result += '\\'; result += static_cast<char>(c); }
    else if (c < 0x20) {
      result += "\\u00";
      result += hex[c >> 4]; result += hex[c & 15];
    } else result += static_cast<char>(c);
  }
  return result + '"';
}

void attributes(std::ostream& out, const std::vector<EventAttribute>& values) {
  out << '{';
  bool first = true;
  for (const auto& value : values) {
    if (!first) out << ',';
    first = false;
    out << quote(value.key) << ':' << quote(value.value);
  }
  out << '}';
}

void alignment(std::ostream& out, const DfgAlignmentResult& result, bool enriched) {
  if (!result.computed) { out << "null"; return; }
  out << "{\"alignment\":[";
  bool first = true;
  for (const auto& move : result.alignment) {
    if (!first) out << ',';
    first = false;
    out << '[' << quote(move.log) << ',' << quote(move.model) << ']';
  }
  out << "],\"cost\":" << result.cost << ",\"internal_cost\":" << result.internal_cost
      << ",\"visited_states\":" << result.visited_states << ",\"closed\":" << result.closed;
  if (enriched) out << ",\"fitness\":" << result.fitness << ",\"bwc\":" << result.best_worst_cost;
  out << '}';
}

void projection(std::ostream& out, const EventLog& log) {
  out << "{\"attributes\":";
  attributes(out, log.attributes);
  out << ",\"traces\":[";
  bool first_trace = true;
  for (const auto& trace : log.traces) {
    if (!first_trace) out << ',';
    first_trace = false;
    out << "{\"attributes\":";
    attributes(out, trace.attributes);
    if (!trace.case_id.empty()) out << ",\"case_id\":" << quote(trace.case_id);
    out << ",\"events\":[";
    bool first_event = true;
    for (const auto& event : trace.events) {
      if (!first_event) out << ',';
      first_event = false;
      const auto* conforming = event.attribute("@@is_conforming");
      out << "{\"activity\":" << quote(event.activity)
          << ",\"timestamp\":" << quote(event.timestamp) << ",\"conforming\":";
      if (conforming) out << (*conforming == "true" ? "true" : "false");
      else out << "null";
      // Surface unexpected retained context so normalization cannot hide a leak.
      if (!event.case_id.empty()) out << ",\"case_id\":" << quote(event.case_id);
      if (!event.resource.empty()) out << ",\"resource\":" << quote(event.resource);
      if (!event.lifecycle.empty()) out << ",\"lifecycle\":" << quote(event.lifecycle);
      out << ",\"extra_attributes\":{";
      bool first_extra = true;
      for (const auto& value : event.attributes) {
        if (value.key != "@@is_conforming" && value.key != "concept:name" && value.key != "time:timestamp") {
          if (!first_extra) out << ',';
          first_extra = false;
          out << quote(value.key) << ':' << quote(value.value);
        }
      }
      out << "}}";
    }
    out << "]}";
  }
  out << "]}";
}

void metrics(std::ostream& out, const DfgAlignmentProjection& result) {
  out << "{\"activities\":{";
  bool first = true;
  for (const auto& [name, counts] : result.activities) {
    if (!first) out << ',';
    first = false;
    out << quote(name) << ":{\"sync\":" << counts.sync << ",\"mm\":" << counts.mm
        << ",\"lm\":" << counts.lm << '}';
  }
  out << "},\"edges\":[";
  first = true;
  for (const auto& [edge, counts] : result.dfg) {
    if (!first) out << ',';
    first = false;
    out << "{\"source\":" << quote(edge.first) << ",\"target\":" << quote(edge.second)
        << ",\"sync\":" << counts.sync << ",\"mm\":" << counts.mm << '}';
  }
  out << "]}";
}

template <typename Operation>
std::string capture(Operation operation) {
  std::ostringstream out;
  out << std::setprecision(17);
  try { operation(out); }
  catch (const std::exception& error) {
    return "{\"error\":true,\"message\":" + quote(error.what()) + '}';
  }
  return out.str();
}

void run(std::ostream& out, const std::string& model, const std::string& scenario,
         const std::string& input_word) {
  DfgFrequency dfg;
  std::map<std::string, std::uint64_t> starts{{"a",1}}, ends;
  if (model == "path") { dfg = {{{"a","b"},1}}; ends = {{"b",1}}; }
  else if (model == "singleton") ends = {{"a",1}};
  else if (model == "cycle") { dfg = {{{"a","b"},1},{{"b","a"},1}}; ends = {{"a",1}}; }
  else if (model == "fork") { dfg = {{{"a","b"},1},{{"a","c"},1}}; ends = {{"b",1},{"c",1}}; }
  else if (model == "chain3") { dfg = {{{"a","b"},1},{{"b","c"},1}}; ends = {{"c",1}}; }
  else if (model == "selfloop") { dfg = {{{"a","a"},1}}; ends = {{"a",1}}; }
  else if (model == "multistart") { dfg = {{{"a","c"},1},{{"b","c"},1}}; starts = {{"a",1},{"b",1}}; ends = {{"c",1}}; }
  else if (model == "disconnected") { dfg = {{{"a","b"},1},{{"c","c"},1}}; ends = {{"b",1}}; }
  else if (model == "zero_frequency") { dfg = {{{"a","b"},0}}; starts = {{"a",0}}; ends = {{"b",0}}; }
  else if (model == "unreachable") { dfg = {{{"a","b"},1}}; ends = {{"c",1}}; }
  else if (model == "empty") starts.clear();
  else throw std::invalid_argument("unknown model: " + model);
  DfgAlignmentOptions options;
  const std::map<std::string,double> ordinary{{"a",2},{"b",3},{"c",4},{"x",5}};
  if (scenario == "sync") options.sync_cost = ordinary;
  else if (scenario == "model") options.model_move_cost = ordinary;
  else if (scenario == "log") options.log_move_cost = ordinary;
  else if (scenario == "internal") options.internal_log_move_cost = ordinary;
  else if (scenario == "unequal") {
    options.sync_cost = {{"a",2},{"b",3},{"c",7},{"x",11}};
    options.log_move_cost = {{"a",123},{"b",456},{"c",147},{"x",59}};
    options.model_move_cost = {{"a",789},{"b",135},{"c",249},{"x",41}};
  } else if (scenario == "zero") {
    const std::map<std::string,double> zero{{"a",0},{"b",0},{"c",0},{"x",0}};
    options.sync_cost = options.model_move_cost = options.log_move_cost = options.internal_log_move_cost = zero;
  } else if (scenario == "equal") {
    const std::map<std::string,double> equal{{"a",1},{"b",1},{"c",1},{"x",1}};
    options.sync_cost = options.model_move_cost = options.log_move_cost = options.internal_log_move_cost = equal;
  } else if (scenario == "zero_internal") {
    const std::map<std::string,double> zero{{"a",0},{"b",0},{"c",0},{"x",0}};
    options.sync_cost = options.model_move_cost = options.internal_log_move_cost = zero;
    options.log_move_cost = ordinary;
  } else if (scenario == "empty_sync") options.sync_cost_supplied = true;
  else if (scenario == "empty_model") options.model_move_cost_supplied = true;
  else if (scenario == "empty_log") options.log_move_cost_supplied = true;
  else if (scenario == "empty_internal") options.internal_log_move_cost_supplied = true;
  else if (scenario == "partial_sync") options.sync_cost = {{"a",2}};
  else if (scenario == "partial_model") options.model_move_cost = {{"a",2}};
  else if (scenario == "partial_log") options.log_move_cost = {{"a",2}};
  else if (scenario == "partial_internal") options.internal_log_move_cost = {{"a",2}};
  else if (scenario != "default") throw std::invalid_argument("unknown scenario: " + scenario);
  const auto word = input_word == "-" ? std::string{} : input_word;
  if (word.size() > 6 || word.find_first_not_of("abcx") != std::string::npos)
    throw std::invalid_argument("word must contain at most six letters from abcx, or '-' for empty");
  std::vector<std::string> labels;
  Trace trace;
  trace.case_id = "source-case";
  trace.attributes.push_back({"extra_trace","discard",AttributeKind::kString});
  for (std::size_t i = 0; i < word.size(); ++i) {
    labels.emplace_back(1, word[i]);
    Event event{"source-case",labels.back(),"2026-01-01T00:00:0" + std::to_string(i) + "Z"};
    event.resource = "source-resource";
    event.lifecycle = "complete";
    event.attributes.push_back({"extra_event","discard",AttributeKind::kString});
    trace.events.push_back(std::move(event));
  }
  EventLog log;
  log.attributes.push_back({"extra_log","discard",AttributeKind::kString});
  log.traces.push_back(std::move(trace));
  out << "{\"trace\":" << capture([&](auto& json) {
    alignment(json,align_trace_on_dfg(labels,dfg,starts,ends,options),false);
  }) << ",\"log\":" << capture([&](auto& json) {
    alignment(json,align_log_on_dfg(log,dfg,starts,ends,options).alignments.at(0),true);
  }) << ",\"projection\":" << capture([&](auto& json) {
    projection(json,project_log_on_dfg(log,dfg,starts,ends,options));
  }) << ",\"metrics\":" << capture([&](auto& json) {
    metrics(json,project_alignments_on_dfg(log,dfg,starts,ends,options));
  }) << '}';
}
}  // namespace

int main() {
  std::string line;
  while (std::getline(std::cin,line)) {
    std::cout << capture([&](auto& out) {
      std::istringstream input(line);
      std::string model,scenario,word,extra;
      if (!(input >> model >> scenario >> word) || (input >> extra))
        throw std::invalid_argument("expected: model scenario word");
      run(out,model,scenario,word);
    }) << '\n';
  }
}
