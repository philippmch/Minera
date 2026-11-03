#include "minera/dfg_io.hpp"

#include <limits>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/directly_follows_graph.hpp"
#include "pnml_decimal.hpp"

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

// One code point of possibly malformed UTF-8; a malformed byte stands alone
// and matches neither whitespace nor a digit.
struct CodePoint {
  std::uint32_t value;
  std::size_t offset;
  std::size_t size;
};

std::vector<CodePoint> code_points(std::string_view text) {
  std::vector<CodePoint> result;
  for (std::size_t position = 0; position < text.size();) {
    const auto first = static_cast<unsigned char>(text[position]);
    std::size_t count = 0;
    std::uint32_t code = first;
    if (first >= 0xc2 && first <= 0xdf) { code = first & 0x1f; count = 1; }
    else if (first >= 0xe0 && first <= 0xef) { code = first & 0x0f; count = 2; }
    else if (first >= 0xf0 && first <= 0xf4) { code = first & 0x07; count = 3; }
    bool valid = first < 0x80 || count != 0;
    for (std::size_t i = 1; valid && i <= count; ++i) {
      if (position + i >= text.size() ||
          (static_cast<unsigned char>(text[position + i]) & 0xc0) != 0x80) {
        valid = false;
      } else {
        code = (code << 6) | (static_cast<unsigned char>(text[position + i]) & 0x3f);
      }
    }
    if (valid && ((count == 2 && code < 0x800) || (count == 3 && code < 0x10000) ||
                  (code >= 0xd800 && code <= 0xdfff) || code > 0x10ffff)) {
      valid = false;
    }
    if (!valid) { code = 0xffffffffu; count = 0; }
    result.push_back({code, position, count + 1});
    position += count + 1;
  }
  return result;
}

// str.strip(): the str.isspace() set, which adds \x1c-\x1f to the
// whitespace that int() ignores.
bool python_strip_space(std::uint32_t code) {
  return detail::pnml_integer_space(code) || (code >= 0x1c && code <= 0x1f);
}

std::string python_strip(std::string_view input) {
  const auto points = code_points(input);
  std::size_t first = 0, last = points.size();
  while (first < last && python_strip_space(points[first].value)) ++first;
  while (last > first && python_strip_space(points[last - 1].value)) --last;
  if (first == last) return {};
  const auto begin = points[first].offset;
  const auto end = points[last - 1].offset + points[last - 1].size;
  return std::string(input.substr(begin, end - begin));
}

// An integer as int() reads it: surrounding int() whitespace, an optional
// sign, and Unicode decimal digits with single underscores between them.
// "-0" is zero.  A magnitude beyond 64 bits is kept as "overflow".
struct PythonInt {
  bool negative = false;
  std::uint64_t magnitude = 0;
  bool overflow = false;
};

PythonInt parse_python_int(std::string_view raw, std::string_view what) {
  const auto points = code_points(raw);
  std::size_t first = 0, last = points.size();
  while (first < last && detail::pnml_integer_space(points[first].value)) ++first;
  while (last > first && detail::pnml_integer_space(points[last - 1].value)) --last;
  if (first == last) throw std::invalid_argument("DFG: missing " + std::string(what));
  const auto invalid = [&] { return std::invalid_argument("DFG: invalid " + std::string(what)); };
  PythonInt result;
  if (points[first].value == '+' || points[first].value == '-') {
    result.negative = points[first].value == '-';
    ++first;
  }
  bool previous_digit = false, digit_seen = false;
  for (std::size_t i = first; i < last; ++i) {
    if (points[i].value == '_') {
      if (!previous_digit || i + 1 == last ||
          detail::pnml_decimal_digit(points[i + 1].value) < 0) {
        throw invalid();
      }
      previous_digit = false;
      continue;
    }
    const int digit = detail::pnml_decimal_digit(points[i].value);
    if (digit < 0) throw invalid();
    const auto digit_value = static_cast<std::uint64_t>(digit);
    if (result.magnitude > (std::numeric_limits<std::uint64_t>::max() - digit_value) / 10)
      result.overflow = true;
    else
      result.magnitude = result.magnitude * 10 + digit_value;
    previous_digit = digit_seen = true;
  }
  if (!digit_seen) throw invalid();
  if (result.magnitude == 0 && !result.overflow) result.negative = false;
  return result;
}

// A section size: range() of a negative size is empty, so it reads no rows.
std::uint64_t parse_section_size(std::string_view raw, std::string_view what,
                                 std::size_t row_count) {
  const auto value = parse_python_int(raw, what);
  if (value.negative) return 0;
  if (value.overflow || value.magnitude > row_count)
    throw std::invalid_argument("DFG: " + std::string(what) + " is too large");
  return value.magnitude;
}

// Counts are kept unsigned, so a negative count (which the reference keeps)
// and one beyond 64 bits are refused; see the parity notes.
std::uint64_t parse_count(std::string_view raw) {
  const auto value = parse_python_int(raw, "count");
  if (value.negative) throw std::invalid_argument("DFG: negative counts are not supported");
  if (value.overflow) throw std::invalid_argument("DFG: count is too large");
  return value.magnitude;
}

// An activity id indexes the label list as Python does: -1 is the last label.
std::size_t parse_activity_id(std::string_view raw, std::string_view what,
                              std::size_t activity_count, std::string_view range_what) {
  const auto value = parse_python_int(raw, what);
  const auto out_of_range = [&] {
    return std::invalid_argument("DFG: " + std::string(range_what) + " id out of range");
  };
  if (value.overflow) throw out_of_range();
  if (!value.negative) {
    if (value.magnitude >= activity_count) throw out_of_range();
    return static_cast<std::size_t>(value.magnitude);
  }
  if (value.magnitude > activity_count) throw out_of_range();
  return activity_count - static_cast<std::size_t>(value.magnitude);
}

std::pair<std::string_view, std::string_view> split_count(std::string_view value,
                                                          std::string_view what) {
  const auto split = value.find('x');
  if (split == std::string::npos || value.find('x', split + 1) != std::string::npos)
    throw std::invalid_argument("DFG: malformed " + std::string(what) + " row");
  return {value.substr(0, split), value.substr(split + 1)};
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
  // Rows are read as the reference reads them: labels and rows stripped,
  // numbers read like int(), labels by index (a repeated label names the
  // same activity, a negative index counts from the end), zero counts
  // allowed and a repeated start, end or edge row replacing the earlier one.
  const auto activity_count = static_cast<std::size_t>(
      parse_section_size(next("activity count"), "activity count", lines.size()));
  std::vector<std::string> names;
  names.reserve(activity_count);
  for (std::size_t i = 0; i < activity_count; ++i) {
    auto name = python_strip(next("activity label"));
    if (name.find('\0') != std::string::npos)
      throw std::invalid_argument("DFG: activity labels must not contain NUL");
    names.push_back(std::move(name));
  }
  DfgCleanAnalysis result;
  const auto read_activity_rows = [&](std::string_view section, std::string_view row_what,
                                      std::string_view what,
                                      std::map<std::string, std::uint64_t>& target) {
    const auto rows = parse_section_size(next(section), section, lines.size());
    for (std::uint64_t i = 0; i < rows; ++i) {
      const auto row = python_strip(next(row_what));
      const auto [id, count] = split_count(row, what);
      const auto value = parse_count(count);
      target[names[parse_activity_id(id, "activity id", activity_count, what)]] = value;
    }
  };
  read_activity_rows("start activity count", "start activity row", "start activity",
                     result.start_activities);
  read_activity_rows("end activity count", "end activity row", "end activity",
                     result.end_activities);
  while (cursor < lines.size()) {
    const auto row = python_strip(lines[cursor]);
    if (row.empty()) throw std::invalid_argument("DFG: unexpected blank row");
    ++cursor;
    const auto [edge, count] = split_count(row, "edge");
    const auto value = parse_count(count);
    const auto arrow = edge.find('>');
    if (arrow == std::string::npos || edge.find('>', arrow + 1) != std::string::npos)
      throw std::invalid_argument("DFG: malformed edge row");
    const auto source = parse_activity_id(edge.substr(0, arrow), "source activity id",
                                          activity_count, "edge activity");
    const auto target = parse_activity_id(edge.substr(arrow + 1), "target activity id",
                                          activity_count, "edge activity");
    result.graph[std::make_pair(names[source], names[target])] = value;
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
