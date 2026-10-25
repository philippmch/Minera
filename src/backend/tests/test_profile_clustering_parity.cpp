// Profile clustering against the public SKLEARN_PROFILES capture
// (docs/profile-clustering-bank.json.gz, written to
// src/backend/tests/fixtures/profile_clustering_bank.txt). Each seeded log's
// sublogs, as lists of trace positions, or its ValueError must match one of
// the outcomes observed across four hash seeds.
#include "minera/clustering.hpp"
#include "minera/event_log.hpp"
#include "minera/log_to_features.hpp"

#include <cstdio>
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

struct Case {
  std::string name;
  EventLog log;
  std::optional<std::string> error;
  std::vector<std::string> outcomes;
};

void add_trace(EventLog& log, const std::vector<std::string>& tokens) {
  Trace trace;
  trace.case_id = std::to_string(log.traces.size());
  trace.case_id_state = FirstClassAttributeState::kPresent;
  if (tokens.at(1) != "-") trace.attributes.push_back({"region", tokens[1], AttributeKind::kString});
  for (std::size_t index = 2; index < tokens.size(); ++index) {
    const auto fields = split(tokens[index], ';');
    Event event{trace.case_id, fields.at(0), "", "", trace.events.size()};
    event.activity_state = FirstClassAttributeState::kPresent;
    if (fields.at(1) != "-")
      event.attributes.push_back({"amount", fields[1],
                                  fields[1].find('.') == std::string::npos ? AttributeKind::kInt
                                                                           : AttributeKind::kFloat});
    if (fields.at(2) != "-") event.attributes.push_back({"kind", fields[2], AttributeKind::kString});
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
    const auto rest = line.size() > 2 ? line.substr(2) : std::string();
    switch (line[0]) {
      case 'C': cases.push_back({rest, {}, {}, {}}); break;
      case 'T': add_trace(cases.back().log, split(line, ' ')); break;
      case 'X': cases.back().error = rest; break;
      case 'O': cases.back().outcomes.push_back(rest); break;
      default: break;
    }
  }
  return cases;
}

std::size_t tie_dependent = 0;

// A k-means fixpoint up to ties: every trace is nearest to the mean of its own
// cluster, within rounding.
bool fixpoint(const EventLog& log, const std::vector<std::vector<std::size_t>>& clusters) {
  std::vector<std::vector<double>> rows;
  for (const auto& trace : trace_based_log_features(log).trace_rows) {
    std::vector<double> row;
    for (const auto& cell : trace) row.push_back(std::get<double>(cell));
    rows.push_back(std::move(row));
  }
  std::vector<std::vector<double>> means;
  for (const auto& cluster : clusters) {
    std::vector<double> mean(rows.front().size(), 0.0);
    for (const auto member : cluster)
      for (std::size_t j = 0; j < mean.size(); ++j) mean[j] += rows[member][j] / cluster.size();
    means.push_back(std::move(mean));
  }
  const auto distance = [&](std::size_t row, std::size_t cluster) {
    double sum = 0.0;
    for (std::size_t j = 0; j < means[cluster].size(); ++j)
      sum += (rows[row][j] - means[cluster][j]) * (rows[row][j] - means[cluster][j]);
    return sum;
  };
  for (std::size_t c = 0; c < clusters.size(); ++c)
    for (const auto member : clusters[c])
      for (std::size_t other = 0; other < clusters.size(); ++other)
        if (distance(member, other) < distance(member, c) - 1e-9) return false;
  return true;
}

// Empty when the native clusters match an observed outcome. When a native
// decision sat on a tie, the reference decides by BLAS rounding, so the
// native clusters need only form a k-means fixpoint.
std::string profiles_case(const Case& item) {
  std::vector<std::vector<std::size_t>> clusters;
  bool ties = false;
  try {
    const auto result = reference_profile_clustering(item.log);
    clusters = result.clusters;
    ties = result.tie_dependent;
  } catch (const std::invalid_argument& error) {
    return item.error ? "" : std::string("raised ") + error.what();
  }
  if (item.error) return "expected " + *item.error;
  std::string text;
  for (const auto& cluster : clusters) {
    if (!text.empty()) text += "|";
    for (std::size_t i = 0; i < cluster.size(); ++i)
      text += (i ? "," : "") + std::to_string(cluster[i]);
  }
  for (const auto& outcome : item.outcomes)
    if (outcome == text) return "";
  if (ties) {
    ++tie_dependent;
    return fixpoint(item.log, clusters) ? "" : "tie-dependent clusters are not a k-means fixpoint";
  }
  return "native " + text + " | reference " + item.outcomes.front();
}

}  // namespace

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "src/backend/tests/fixtures/profile_clustering_bank.txt";
  int failures = 0;
  std::size_t compared = 0, errors = 0, varying = 0;
  for (const auto& item : load(path)) {
    ++compared;
    if (item.error) ++errors;
    if (item.outcomes.size() > 1) ++varying;
    std::string detail;
    try {
      detail = profiles_case(item);
    } catch (const std::exception& error) {
      detail = std::string("threw ") + error.what();
    }
    if (!detail.empty()) {
      ++failures;
      std::fprintf(stderr, "%s: %s\n", item.name.c_str(), detail.c_str());
    }
  }
  std::printf("%zu logs compared (%zu public errors, %zu with hash-seed variation, "
              "%zu differing at rounding ties)\n",
              compared, errors, varying, tie_dependent);
  if (compared < 300 || errors < 13 || tie_dependent > 15) {
    ++failures;
    std::fprintf(stderr, "bank coverage below the pinned floor\n");
  }
  return failures == 0 ? 0 : 1;
}
