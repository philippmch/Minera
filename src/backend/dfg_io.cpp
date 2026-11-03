#include "minera/dfg_io.hpp"

#include <charconv>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/directly_follows_graph.hpp"

namespace minera {
namespace {

// The reference reads every row with str.strip() and int(), so surrounding
// whitespace (ASCII, including the \x1c-\x1f separators) is ignored.
bool row_space(char ch) {
  return ch == ' ' || (ch >= '\t' && ch <= '\r') || (ch >= '\x1c' && ch <= '\x1f');
}

std::string trim(std::string_view input) {
  std::size_t first = 0;
  while (first < input.size() && row_space(input[first])) ++first;
  std::size_t last = input.size();
  while (last > first && row_space(input[last - 1])) --last;
  return std::string(input.substr(first, last - first));
}

// A non-negative decimal as int() reads it: optional whitespace and '+',
// digits with single underscores between them.  Negative values (which the
// reference would accept, and use as Python list indices) are rejected.
std::uint64_t parse_uint(std::string_view raw, std::string_view what) {
  const auto trimmed = trim(raw);
  std::string_view token = trimmed;
  if (token.empty()) throw std::invalid_argument("DFG: missing " + std::string(what));
  if (token.front() == '+') token.remove_prefix(1);
  std::string digits;
  bool valid = !token.empty() && token.front() != '_' && token.back() != '_';
  for (std::size_t i = 0; valid && i < token.size(); ++i) {
    if (token[i] == '_') valid = token[i + 1] != '_';
    else if (token[i] >= '0' && token[i] <= '9') digits.push_back(token[i]);
    else valid = false;
  }
  std::uint64_t value = 0;
  if (valid) {
    const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value, 10);
    valid = result.ec == std::errc() && result.ptr == digits.data() + digits.size();
  }
  if (!valid) throw std::invalid_argument("DFG: invalid " + std::string(what));
  return value;
}

std::pair<std::string_view, std::string_view> split_count(std::string_view value,
                                                          std::string_view what) {
  const auto split = value.find('x');
  if (split == std::string::npos || value.find('x', split + 1) != std::string::npos)
    throw std::invalid_argument("DFG: malformed " + std::string(what) + " row");
  return {value.substr(0, split), value.substr(split + 1)};
}

std::pair<std::uint64_t, std::uint64_t> parse_id_count(std::string_view line,
                                                       std::string_view what) {
  const auto [id, count] = split_count(line, what);
  return {parse_uint(id, "activity id"), parse_uint(count, "count")};
}

std::pair<std::pair<std::uint64_t, std::uint64_t>, std::uint64_t>
parse_edge(std::string_view line) {
  const auto [edge, count] = split_count(line, "edge");
  const auto arrow = edge.find('>');
  if (arrow == std::string::npos || edge.find('>', arrow + 1) != std::string::npos)
    throw std::invalid_argument("DFG: malformed edge row");
  return {{parse_uint(edge.substr(0, arrow), "source activity id"),
           parse_uint(edge.substr(arrow + 1), "target activity id")},
          parse_uint(count, "count")};
}

std::vector<std::string> activity_names(const DfgCleanAnalysis& dfg) {
  std::map<std::string, std::size_t> names;
  for (const auto& [pair, _] : dfg.graph) {
    names.emplace(pair.first, 0);
    names.emplace(pair.second, 0);
  }
  for (const auto& [name, _] : dfg.start_activities) names.emplace(name, 0);
  for (const auto& [name, _] : dfg.end_activities) names.emplace(name, 0);
  std::vector<std::string> result;
  result.reserve(names.size());
  for (const auto& [name, _] : names) {
    if (name.empty() || name != trim(name) || name.find('\n') != std::string::npos ||
        name.find('\r') != std::string::npos || name.find('\0') != std::string::npos) {
      throw std::invalid_argument("DFG: activity labels must be non-empty and single-line");
    }
    result.push_back(name);
  }
  return result;
}

}  // namespace

void write_dfg(std::ostream& out, const DfgCleanAnalysis& dfg) {
  const auto names = activity_names(dfg);
  std::map<std::string, std::uint64_t> starts = dfg.start_activities;
  std::map<std::string, std::uint64_t> ends = dfg.end_activities;
  std::map<std::pair<std::string, std::string>, std::uint64_t> edges = dfg.graph;
  std::map<std::string, std::uint64_t> ids;
  for (std::size_t i = 0; i < names.size(); ++i) ids.emplace(names[i], i);

  for (const auto& [name, count] : starts) {
    if (!ids.contains(name))
      throw std::invalid_argument("DFG: invalid start activity row");
  }
  for (const auto& [name, count] : ends) {
    if (!ids.contains(name))
      throw std::invalid_argument("DFG: invalid end activity row");
  }
  for (const auto& [pair, count] : edges) {
    if (!ids.contains(pair.first) || !ids.contains(pair.second))
      throw std::invalid_argument("DFG: invalid edge row");
  }

  out << names.size() << '\n';
  for (const auto& name : names) out << name << '\n';
  out << starts.size() << '\n';
  for (const auto& [name, count] : starts)
    out << ids.at(name) << 'x' << count << '\n';
  out << ends.size() << '\n';
  for (const auto& [name, count] : ends)
    out << ids.at(name) << 'x' << count << '\n';
  for (const auto& [pair, count] : edges)
    out << ids.at(pair.first) << '>' << ids.at(pair.second) << 'x' << count
        << '\n';
  if (!out) throw std::runtime_error("DFG: failed writing output");
}

std::string to_dfg(const DfgCleanAnalysis& dfg) {
  std::ostringstream out;
  write_dfg(out, dfg);
  return out.str();
}

DfgCleanAnalysis to_dfg_analysis(const DirectlyFollowsGraph& graph) {
  DfgCleanAnalysis result;
  const auto& names = graph.activities();
  for (const auto& edge : graph.edges()) {
    if (edge.source >= names.size() || edge.target >= names.size()) {
      throw std::invalid_argument("DFG: graph edge activity id out of range");
    }
    if (edge.count == 0) continue;
    result.graph[{names[edge.source], names[edge.target]}] = edge.count;
  }
  for (const auto& name : names) {
    const auto start = graph.start_activity_count(name);
    const auto end = graph.end_activity_count(name);
    if (start != 0) result.start_activities[name] = start;
    if (end != 0) result.end_activities[name] = end;
  }
  return result;
}

DfgCleanAnalysis parse_dfg(std::string_view text) {
  std::istringstream input{std::string(text)};
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    lines.push_back(std::move(line));
  }
  std::size_t cursor = 0;
  const auto next = [&](std::string_view what) -> std::string_view {
    if (cursor == lines.size())
      throw std::invalid_argument("DFG: missing " + std::string(what));
    return lines[cursor++];
  };
  // Rows are read as the reference reads them: stripped, labels by index
  // (a repeated label names the same activity), zero counts allowed and a
  // repeated start, end or edge row replacing the earlier one.
  const auto activity_count = parse_uint(next("activity count"), "activity count");
  if (activity_count > lines.size())
    throw std::invalid_argument("DFG: activity count is too large");
  std::vector<std::string> names;
  names.reserve(static_cast<std::size_t>(activity_count));
  for (std::uint64_t i = 0; i < activity_count; ++i) {
    auto name = trim(next("activity label"));
    if (name.find('\0') != std::string::npos)
      throw std::invalid_argument("DFG: activity labels must not contain NUL");
    names.push_back(std::move(name));
  }
  DfgCleanAnalysis result;
  const auto start_count = parse_uint(next("start activity count"), "start activity count");
  if (start_count > lines.size())
    throw std::invalid_argument("DFG: start activity count is too large");
  for (std::uint64_t i = 0; i < start_count; ++i) {
    const auto [id, count] = parse_id_count(next("start activity row"), "start activity");
    if (id >= activity_count)
      throw std::invalid_argument("DFG: start activity id out of range");
    result.start_activities[names[id]] = count;
  }
  const auto end_count = parse_uint(next("end activity count"), "end activity count");
  if (end_count > lines.size())
    throw std::invalid_argument("DFG: end activity count is too large");
  for (std::uint64_t i = 0; i < end_count; ++i) {
    const auto [id, count] = parse_id_count(next("end activity row"), "end activity");
    if (id >= activity_count)
      throw std::invalid_argument("DFG: end activity id out of range");
    result.end_activities[names[id]] = count;
  }
  while (cursor < lines.size()) {
    if (trim(lines[cursor]).empty())
      throw std::invalid_argument("DFG: unexpected blank row");
    const auto [pair, count] = parse_edge(next("edge row"));
    if (pair.first >= activity_count || pair.second >= activity_count)
      throw std::invalid_argument("DFG: edge activity id out of range");
    result.graph[std::make_pair(names[pair.first], names[pair.second])] = count;
  }
  return result;
}

DfgCleanAnalysis import_dfg(std::istream& input) {
  std::ostringstream contents;
  contents << input.rdbuf();
  if (!input.good() && !input.eof())
    throw std::runtime_error("DFG: unable to read input file");
  return parse_dfg(contents.str());
}

DfgCleanAnalysis import_dfg(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("DFG: unable to open input file");
  return import_dfg(input);
}

void export_dfg(const std::filesystem::path& path, const DfgCleanAnalysis& dfg) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) throw std::runtime_error("DFG: unable to open output file");
  write_dfg(output, dfg);
}

}  // namespace minera
