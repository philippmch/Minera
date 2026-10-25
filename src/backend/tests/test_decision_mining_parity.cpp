// Reference decision tables against the public decision-mining capture
// (docs/decision-mining-bank.json.gz, written to
// src/backend/tests/fixtures/decision_mining_bank.txt). Each seeded log was
// mined with the public inductive miner; every decision point's encoded table
// (columns, dtypes, cells, targets, classes) or its ValueError is replayed here
// through reference_decision_table over the same PNML document. The public
// decision tree raises for every point in the pinned environment, which the
// fixture also pins.
#include "minera/decision_table.hpp"
#include "minera/event_log.hpp"
#include "minera/pnml.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace minera;

std::vector<std::string> split(const std::string& text, char separator) {
  std::vector<std::string> parts;
  std::string part;
  std::istringstream stream(text);
  while (std::getline(stream, part, separator)) parts.push_back(part);
  if (!text.empty() && text.back() == separator) parts.emplace_back();
  return parts;
}

struct Point {
  std::string place;
  std::string labels;
  std::optional<std::string> error;
  std::vector<std::string> header;  // column|dtype
  std::vector<std::pair<std::string, std::vector<std::string>>> rows;
  std::vector<std::string> classes;
  std::string tree;
};

struct Case {
  std::string name;
  std::string pnml;
  EventLog log;
  std::vector<Point> points;
};

EventAttribute attribute(const std::string& key, const std::string& value, AttributeKind kind) {
  EventAttribute out;
  out.key = key;
  out.value = value;
  out.kind = kind;
  return out;
}

void add_trace(EventLog& log, const std::vector<std::string>& tokens) {
  Trace trace;
  trace.case_id = "c" + std::to_string(log.traces.size());
  trace.case_id_state = FirstClassAttributeState::kPresent;
  for (std::size_t index = 3; index < tokens.size(); ++index) {
    const auto fields = split(tokens[index], ';');
    Event event;
    event.case_id = trace.case_id;
    event.activity = fields.at(0);
    event.activity_state = FirstClassAttributeState::kPresent;
    event.input_order = trace.events.size();
    if (fields.at(1) != "-")
      event.attributes.push_back(attribute(
          "amount", fields[1],
          fields[1].find('.') == std::string::npos ? AttributeKind::kInt : AttributeKind::kFloat));
    if (fields.at(2) != "-") event.attributes.push_back(attribute("kind", fields[2], AttributeKind::kString));
    if (fields.at(3) != "-")
      event.attributes.push_back(attribute("flag", fields[3] == "t" ? "true" : "false",
                                           AttributeKind::kBoolean));
    trace.events.push_back(std::move(event));
  }
  log.traces.push_back(std::move(trace));
}

std::vector<Case> load(const char* path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error(std::string("cannot read ") + path);
  std::vector<Case> cases;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#') continue;
    const auto tag = line.substr(0, 1);
    const auto rest = line.size() > 2 ? line.substr(2) : std::string();
    if (tag == "C") {
      cases.push_back({rest, {}, {}, {}});
    } else if (tag == "N") {
      cases.back().pnml = rest;
    } else if (tag == "T") {
      add_trace(cases.back().log, split(line, ' '));
    } else if (tag == "P") {
      Point point;
      const auto fields = split(rest, ' ');
      point.place = fields.at(0);
      point.labels = fields.at(1);
      cases.back().points.push_back(std::move(point));
    } else if (tag == "X") {
      cases.back().points.back().error = rest;
    } else if (tag == "H") {
      cases.back().points.back().header = split(rest, ' ');
    } else if (tag == "R") {
      auto tokens = split(rest, ' ');
      const auto target = tokens.front();
      tokens.erase(tokens.begin());
      cases.back().points.back().rows.emplace_back(target, std::move(tokens));
    } else if (tag == "K") {
      cases.back().points.back().classes = split(rest, ' ');
    } else if (tag == "D") {
      cases.back().points.back().tree = rest;
    }
  }
  return cases;
}

const char* dtype_name(ReferenceDecisionColumnType type) {
  switch (type) {
    case ReferenceDecisionColumnType::kBool: return "bool";
    case ReferenceDecisionColumnType::kInt64: return "int64";
    case ReferenceDecisionColumnType::kFloat64: return "float64";
  }
  return "?";
}

bool cell_matches(const std::optional<double>& actual, const std::string& expected,
                  ReferenceDecisionColumnType type) {
  if (expected == "-") return !actual;
  if (!actual) return false;
  if (type == ReferenceDecisionColumnType::kBool) return *actual == (expected == "t" ? 1.0 : 0.0);
  return *actual == std::strtod(expected.c_str(), nullptr);
}

// The public decision points: places with several outgoing transitions,
// listed with the sorted labels of those transitions (None when silent).
std::string points_case(const PnmlDocument& document, const std::vector<Point>& points) {
  std::vector<std::string> expected, actual;
  for (const auto& point : points) expected.push_back(point.place + " " + point.labels);
  for (std::size_t place = 0; place < document.place_ids.size(); ++place) {
    std::vector<std::string> labels;
    for (const auto& transition : document.net.transitions())
      for (const auto input : transition.inputs)
        if (input == place)
          labels.push_back(transition.activity ? document.net.activities().at(*transition.activity) : "None");
    if (labels.size() < 2) continue;
    std::sort(labels.begin(), labels.end());
    std::string joined;
    for (const auto& label : labels) joined += (joined.empty() ? "" : ",") + label;
    actual.push_back(document.place_ids[place] + " " + joined);
  }
  std::sort(expected.begin(), expected.end());
  std::sort(actual.begin(), actual.end());
  return expected == actual ? "" : "decision points differ";
}

// Empty when the native table matches the point, otherwise a description.
std::string table_case(const PnmlDocument& document, const EventLog& log, const Point& point) {
  std::size_t place = document.place_ids.size();
  for (std::size_t index = 0; index < document.place_ids.size(); ++index)
    if (document.place_ids[index] == point.place) place = index;
  if (place == document.place_ids.size()) return "unknown place";
  TokenReplayOptions replay;
  replay.exact_initial_marking = document.initial_marking;
  replay.exact_final_marking = document.final_marking;
  replay.initial_marking_keys = document.initial_marking_keys;
  replay.final_marking_keys = document.final_marking_keys;
  replay.place_names = document.place_ids;
  replay.transition_names = document.transition_ids;
  replay.place_outgoing_transition_order = pnml_place_outgoing_transition_order(document);
  ReferenceDecisionTable table;
  try {
    table = reference_decision_table(log, document.net, place, replay);
  } catch (const std::invalid_argument& error) {
    return point.error ? "" : std::string("raised ") + error.what();
  }
  if (point.error) return "expected " + *point.error;
  std::vector<std::string> header;
  for (std::size_t column = 0; column < table.columns.size(); ++column)
    header.push_back(table.columns[column] + "|" + dtype_name(table.column_types[column]));
  if (header != point.header) return "columns differ";
  if (table.rows.size() != point.rows.size()) return "row count differs";
  for (std::size_t row = 0; row < table.rows.size(); ++row) {
    if (std::to_string(table.targets[row]) != point.rows[row].first)
      return "target differs at row " + std::to_string(row);
    for (std::size_t column = 0; column < table.columns.size(); ++column)
      if (!cell_matches(table.rows[row][column], point.rows[row].second.at(column),
                        table.column_types[column]))
        return "cell differs at row " + std::to_string(row);
  }
  // The public target labels pass through a string column: silent labels
  // read None when every label is silent and NaN beside visible ones.
  bool all_silent = true;
  for (const auto& label : table.classes) all_silent = all_silent && !label;
  std::vector<std::string> classes;
  for (const auto& label : table.classes) classes.push_back(label ? *label : all_silent ? "None" : "nan");
  if (classes != point.classes) return "classes differ";
  return "";
}

}  // namespace

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "src/backend/tests/fixtures/decision_mining_bank.txt";
  int failures = 0;
  std::size_t compared = 0, tables = 0, errors = 0, tree_errors = 0;
  for (const auto& item : load(path)) {
    PnmlImportOptions import;
    import.public_marking_semantics = true;
    const auto document = parse_pnml_document(item.pnml, import);
    if (const auto detail = points_case(document, item.points); !detail.empty()) {
      ++failures;
      std::fprintf(stderr, "%s: %s\n", item.name.c_str(), detail.c_str());
    }
    for (const auto& point : item.points) {
      ++compared;
      ++(point.error ? errors : tables);
      if (point.tree == "!ValueError") ++tree_errors;
      std::string detail;
      try {
        detail = table_case(document, item.log, point);
      } catch (const std::exception& error) {
        detail = std::string("threw ") + error.what();
      }
      if (!detail.empty()) {
        ++failures;
        std::fprintf(stderr, "%s %s: %s\n", item.name.c_str(), point.place.c_str(), detail.c_str());
      }
    }
  }
  std::printf("%zu decision points compared (%zu tables, %zu public errors, %zu tree errors)\n",
              compared, tables, errors, tree_errors);
  if (compared < 632 || tables < 134 || errors < 498 || tree_errors != compared) {
    ++failures;
    std::fprintf(stderr, "bank coverage below the pinned floor\n");
  }
  return failures == 0 ? 0 : 1;
}
