#include "minera/process_cube.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <variant>

namespace minera {
namespace {
struct Axis {
  bool numeric = false;
  std::string name;
  std::vector<ProcessCubeBin> bins;
  std::map<std::string, std::size_t> labels;
};

std::string choose(const std::string& a, const std::string& b) {
  return a.empty() ? b : a;
}

std::vector<double> normalise_edges(std::vector<double> edges) {
  for (double v : edges) if (!std::isfinite(v)) throw std::invalid_argument("process cube bins must be finite");
  std::sort(edges.begin(), edges.end());
  edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
  if (edges.size() < 2) throw std::invalid_argument("process cube requires at least two bin edges");
  for (std::size_t i = 1; i < edges.size(); ++i)
    if (!(edges[i] > edges[i - 1])) throw std::invalid_argument("process cube bins must increase");
  return edges;
}

Axis numeric_axis(const std::string& name, const std::vector<double>& values,
                  const std::vector<double>& requested, std::size_t max_divisions) {
  Axis axis; axis.numeric = true; axis.name = name;
  std::vector<double> edges;
  if (!requested.empty()) edges = normalise_edges(requested);
  else {
    if (max_divisions == 0) throw std::invalid_argument("process cube division limit must be positive");
    if (values.empty()) edges = {0.0, 1.0};
    else {
      auto [lo, hi] = std::minmax_element(values.begin(), values.end());
      if (*lo == *hi) edges = {*lo - 0.5, *hi + 0.5};
      else {
        const std::size_t n = max_divisions;
        const double step = (*hi - *lo) / static_cast<double>(n);
        edges.reserve(n + 1);
        for (std::size_t i = 0; i <= n; ++i) edges.push_back(i == n ? *hi : *lo + step * i);
      }
    }
  }
  for (std::size_t i = 0; i + 1 < edges.size(); ++i)
    axis.bins.push_back({{}, edges[i], edges[i + 1], true});
  return axis;
}

Axis prefix_axis(const std::string& name, const ProcessCubeTable& table) {
  Axis axis; axis.name = name; axis.numeric = false;
  const std::string prefix = name + "_";
  for (const auto& [column, values] : table.columns) {
    if (column.rfind(prefix, 0) != 0 || column.size() == prefix.size()) continue;
    ProcessCubeBin b; b.label = column.substr(prefix.size()); b.numeric = false;
    axis.labels.emplace(b.label, axis.bins.size());
    axis.bins.push_back(std::move(b));
  }
  if (axis.bins.empty()) throw std::invalid_argument("unknown process cube dimension: " + name);
  return axis;
}

std::size_t numeric_index(const Axis& axis, double value) {
  for (std::size_t i = 0; i < axis.bins.size(); ++i) {
    const auto& b = axis.bins[i];
    if (value >= b.lower && (value < b.upper || (i + 1 == axis.bins.size() && value <= b.upper))) return i;
  }
  return axis.bins.size();
}

double aggregate(const std::vector<double>& values, ProcessCubeAggregation fn) {
  if (values.empty()) return 0.0;
  switch (fn) {
    case ProcessCubeAggregation::kCount: return static_cast<double>(values.size());
    case ProcessCubeAggregation::kMean: {
      double s = 0; for (double v : values) s += v; return s / values.size();
    }
    case ProcessCubeAggregation::kSum: { double s = 0; for (double v : values) s += v; return s; }
    case ProcessCubeAggregation::kMin: return *std::min_element(values.begin(), values.end());
    case ProcessCubeAggregation::kMax: return *std::max_element(values.begin(), values.end());
    case ProcessCubeAggregation::kMedian: {
      auto v = values; std::sort(v.begin(), v.end());
      const std::size_t m = v.size() / 2;
      return v.size() % 2 ? v[m] : (v[m - 1] + v[m]) / 2.0;
    }
    case ProcessCubeAggregation::kFirst: return values.front();
    case ProcessCubeAggregation::kLast: return values.back();
    default: throw std::invalid_argument("unknown process cube aggregation");
  }
}

const char* aggregation_name(ProcessCubeAggregation aggregation) {
  switch (aggregation) {
    case ProcessCubeAggregation::kMean: return "mean";
    case ProcessCubeAggregation::kSum: return "sum";
    case ProcessCubeAggregation::kMin: return "min";
    case ProcessCubeAggregation::kMax: return "max";
    case ProcessCubeAggregation::kCount: return "count";
    case ProcessCubeAggregation::kMedian: return "median";
    case ProcessCubeAggregation::kFirst: return "first";
    case ProcessCubeAggregation::kLast: return "last";
  }
  throw std::invalid_argument("unknown process cube aggregation");
}

struct JsonValue {
  using Array = std::vector<JsonValue>;
  using Object = std::map<std::string, JsonValue>;
  std::variant<std::nullptr_t, bool, double, std::string, Array, Object> value;
  bool is_string() const { return std::holds_alternative<std::string>(value); }
  bool is_number() const { return std::holds_alternative<double>(value); }
  bool is_array() const { return std::holds_alternative<Array>(value); }
  bool is_object() const { return std::holds_alternative<Object>(value); }
  const std::string& string() const { return std::get<std::string>(value); }
  const Array& array() const { return std::get<Array>(value); }
  const Object& object() const { return std::get<Object>(value); }
  double number() const { return std::get<double>(value); }
};

class JsonParser {
 public:
  explicit JsonParser(std::string_view input) : input_(input) {
    if (input_.size() > 32u * 1024u * 1024u)
      throw std::invalid_argument("process cube JSON exceeds size limit");
  }
  JsonValue parse() {
    skip();
    auto value = parse_value();
    skip();
    if (pos_ != input_.size()) fail("trailing JSON data");
    return value;
  }

 private:
  [[noreturn]] void fail(const char* message) const {
    throw std::invalid_argument(std::string("invalid process cube JSON: ") + message);
  }
  void skip() {
    while (pos_ < input_.size() &&
           (input_[pos_] == ' ' || input_[pos_] == '\n' ||
            input_[pos_] == '\r' || input_[pos_] == '\t')) ++pos_;
  }
  void expect(char ch) {
    skip();
    if (pos_ >= input_.size() || input_[pos_] != ch) fail("unexpected character");
    ++pos_;
  }
  std::string parse_string() {
    expect('"');
    std::string result;
    while (pos_ < input_.size()) {
      const unsigned char ch = static_cast<unsigned char>(input_[pos_++]);
      if (ch == '"') return result;
      if (ch < 0x20) fail("control character in string");
      if (ch != '\\') { result.push_back(static_cast<char>(ch)); continue; }
      if (pos_ >= input_.size()) fail("unterminated escape");
      const char escaped = input_[pos_++];
      switch (escaped) {
        case '"': result.push_back('"'); break;
        case '\\': result.push_back('\\'); break;
        case '/': result.push_back('/'); break;
        case 'b': result.push_back('\b'); break;
        case 'f': result.push_back('\f'); break;
        case 'n': result.push_back('\n'); break;
        case 'r': result.push_back('\r'); break;
        case 't': result.push_back('\t'); break;
        default: fail("unsupported string escape");
      }
    }
    fail("unterminated string");
  }
  JsonValue parse_value() {
    skip();
    if (pos_ >= input_.size()) fail("unexpected end");
    const char ch = input_[pos_];
    if (ch == '"') return JsonValue{parse_string()};
    if (ch == '{') {
      ++pos_;
      JsonValue::Object object;
      skip();
      if (pos_ < input_.size() && input_[pos_] == '}') { ++pos_; return JsonValue{std::move(object)}; }
      while (true) {
        skip();
        if (pos_ >= input_.size() || input_[pos_] != '"') fail("object key must be string");
        auto key = parse_string();
        expect(':');
        if (!object.emplace(key, parse_value()).second) fail("duplicate object key");
        skip();
        if (pos_ < input_.size() && input_[pos_] == '}') { ++pos_; break; }
        expect(',');
      }
      return JsonValue{std::move(object)};
    }
    if (ch == '[') {
      ++pos_;
      JsonValue::Array array;
      skip();
      if (pos_ < input_.size() && input_[pos_] == ']') { ++pos_; return JsonValue{std::move(array)}; }
      while (true) {
        array.push_back(parse_value());
        skip();
        if (pos_ < input_.size() && input_[pos_] == ']') { ++pos_; break; }
        expect(',');
      }
      return JsonValue{std::move(array)};
    }
    const std::size_t start = pos_;
    if (input_[pos_] == '-') ++pos_;
    while (pos_ < input_.size() &&
           ((input_[pos_] >= '0' && input_[pos_] <= '9') ||
            input_[pos_] == '.' || input_[pos_] == 'e' || input_[pos_] == 'E' ||
            input_[pos_] == '+' || input_[pos_] == '-')) ++pos_;
    if (start == pos_) fail("expected value");
    const std::string token(input_.substr(start, pos_ - start));
    char* end = nullptr;
    const double value = std::strtod(token.c_str(), &end);
    if (end != token.c_str() + token.size() || !std::isfinite(value))
      fail("invalid number");
    return JsonValue{value};
  }
  std::string_view input_;
  std::size_t pos_ = 0;
};

const JsonValue& required_json(const JsonValue::Object& object,
                               const char* key) {
  const auto it = object.find(key);
  if (it == object.end()) throw std::invalid_argument(std::string("process cube JSON is missing ") + key);
  return it->second;
}

void json_string(std::ostringstream& out, std::string_view value) {
  out << '"';
  for (const unsigned char ch : value) {
    switch (ch) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (ch < 0x20) {
          constexpr char digits[] = "0123456789abcdef";
          out << "\\u00" << digits[ch >> 4] << digits[ch & 0xf];
        } else out << static_cast<char>(ch);
    }
  }
  out << '"';
}

void write_bin(std::ostringstream& out, const ProcessCubeBin& bin) {
  if (!std::isfinite(bin.lower) || !std::isfinite(bin.upper))
    throw std::invalid_argument("process cube bin bounds must be finite");
  out << "{\"label\":";
  json_string(out, bin.label);
  out << ",\"lower\":" << bin.lower << ",\"upper\":" << bin.upper
      << ",\"numeric\":" << (bin.numeric ? "true" : "false") << '}';
}
}  // namespace

ProcessCubeResult process_cube(const ProcessCubeTable& table, const ProcessCubeOptions& options) {
  ProcessCubeTable data = table;
  if (data.columns.empty()) data.columns = data.numeric_columns;
  if (options.max_divisions_x == 0 || options.max_divisions_y == 0)
    throw std::invalid_argument("process cube division limits must be positive");
  if (table.case_ids.size() == 0) {
    for (const auto& [name, values] : data.columns)
      if (values.size() != 0) throw std::invalid_argument("process cube row lengths differ");
  }
  for (const auto& id : table.case_ids) if (id.empty()) throw std::invalid_argument("case id must not be empty");
  for (const auto& [name, values] : data.columns) {
    if (name.empty()) throw std::invalid_argument("column name must not be empty");
    if (values.size() != table.case_ids.size()) throw std::invalid_argument("process cube row lengths differ");
    for (double v : values) if (!std::isfinite(v)) throw std::invalid_argument("process cube values must be finite");
  }
  const std::string xname = choose(options.x_col, options.x_dimension);
  const std::string yname = choose(options.y_col, options.y_dimension);
  if (xname.empty() || yname.empty()) throw std::invalid_argument("process cube dimensions are required");
  Axis x, y;
  auto xit = data.columns.find(xname), yit = data.columns.find(yname);
  if (xit != data.columns.end()) x = numeric_axis(xname, xit->second, options.x_bins, options.max_divisions_x);
  else x = prefix_axis(xname, data);
  if (yit != data.columns.end()) y = numeric_axis(yname, yit->second, options.y_bins, options.max_divisions_y);
  else y = prefix_axis(yname, data);
  const std::string value_name = choose(options.aggregation_column, options.value_column);
  if (value_name.empty())
    throw std::invalid_argument("aggregation column is required");
  auto vit = data.columns.find(value_name);
  if (vit == data.columns.end()) throw std::invalid_argument("unknown aggregation column: " + value_name);
  switch (options.aggregation) {
    case ProcessCubeAggregation::kMean:
    case ProcessCubeAggregation::kSum:
    case ProcessCubeAggregation::kMin:
    case ProcessCubeAggregation::kMax:
    case ProcessCubeAggregation::kCount:
    case ProcessCubeAggregation::kMedian:
    case ProcessCubeAggregation::kFirst:
    case ProcessCubeAggregation::kLast:
      break;
    default:
      throw std::invalid_argument("unknown process cube aggregation");
  }

  ProcessCubeResult result;
  result.x_dimension = xname;
  result.y_dimension = yname;
  result.aggregation = aggregation_name(options.aggregation);
  result.x_bins = x.bins;
  result.y_bins = y.bins;
  for (const auto& b : x.bins) if (b.numeric) result.x_edges.push_back(b.lower);
  if (!x.bins.empty() && x.bins.back().numeric) result.x_edges.push_back(x.bins.back().upper);
  for (const auto& b : y.bins) if (b.numeric) result.y_edges.push_back(b.lower);
  if (!y.bins.empty() && y.bins.back().numeric) result.y_edges.push_back(y.bins.back().upper);
  result.cells.assign(x.bins.size(), std::vector<std::optional<double>>(y.bins.size()));
  result.case_ids.assign(x.bins.size(), std::vector<std::set<std::string>>(y.bins.size()));
  std::vector<std::vector<double>> values(x.bins.size() * y.bins.size());
  for (std::size_t r = 0; r < data.case_ids.size(); ++r) {
    std::vector<std::size_t> xi, yi;
    if (x.numeric) { auto i = numeric_index(x, xit->second[r]); if (i < x.bins.size()) xi.push_back(i); }
    else for (const auto& [label, i] : x.labels) {
      auto it = data.columns.find(xname + "_" + label); if (it != data.columns.end() && it->second[r] >= 1.0) xi.push_back(i);
    }
    if (y.numeric) { auto i = numeric_index(y, yit->second[r]); if (i < y.bins.size()) yi.push_back(i); }
    else for (const auto& [label, i] : y.labels) {
      auto it = data.columns.find(yname + "_" + label); if (it != data.columns.end() && it->second[r] >= 1.0) yi.push_back(i);
    }
    for (auto i : xi) for (auto j : yi) {
      const auto k = i * y.bins.size() + j;
      result.case_ids[i][j].insert(data.case_ids[r]);
      if (options.aggregation != ProcessCubeAggregation::kCount) values[k].push_back(vit->second[r]);
      else values[k].push_back(1.0);
    }
  }
  for (std::size_t i = 0; i < x.bins.size(); ++i) for (std::size_t j = 0; j < y.bins.size(); ++j) {
    auto& v = values[i * y.bins.size() + j];
    if (!v.empty()) result.cells[i][j] = aggregate(v, options.aggregation);
  }
  result.pivot = result.cells;
  result.cell_case_ids = result.case_ids;
  return result;
}

ProcessCubeTable parse_process_cube_table_json(std::string_view json) {
  const auto parsed = JsonParser(json).parse();
  if (!parsed.is_object())
    throw std::invalid_argument("process cube table must be a JSON object");
  const auto& root = parsed.object();
  const auto& case_values = required_json(root, "case_ids");
  const auto& columns_value = required_json(root, "columns");
  if (!case_values.is_array() || !columns_value.is_object())
    throw std::invalid_argument("process cube table case_ids/columns have invalid types");

  ProcessCubeTable table;
  table.case_ids.reserve(case_values.array().size());
  for (const auto& value : case_values.array()) {
    if (!value.is_string())
      throw std::invalid_argument("process cube case ids must be strings");
    table.case_ids.push_back(value.string());
  }
  for (const auto& [name, values] : columns_value.object()) {
    if (name.empty() || !values.is_array())
      throw std::invalid_argument("process cube columns must be named numeric arrays");
    auto& output = table.columns[name];
    output.reserve(values.array().size());
    for (const auto& value : values.array()) {
      if (!value.is_number() || !std::isfinite(value.number()))
        throw std::invalid_argument("process cube columns must contain finite numbers");
      output.push_back(value.number());
    }
  }
  for (const auto& [name, values] : table.columns)
    if (values.size() != table.case_ids.size())
      throw std::invalid_argument("process cube JSON column length differs from case_ids");
  return table;
}

std::string serialize_process_cube_json(const ProcessCubeResult& result) {
  if (result.cells.size() != result.x_bins.size() ||
      result.case_ids.size() != result.x_bins.size())
    throw std::invalid_argument("process cube result has inconsistent row dimensions");
  for (std::size_t i = 0; i < result.x_bins.size(); ++i) {
    if (result.cells[i].size() != result.y_bins.size() ||
        result.case_ids[i].size() != result.y_bins.size())
      throw std::invalid_argument("process cube result has inconsistent column dimensions");
    for (const auto& cell : result.cells[i])
      if (cell && !std::isfinite(*cell))
        throw std::invalid_argument("process cube result contains a non-finite cell");
  }
  for (double edge : result.x_edges)
    if (!std::isfinite(edge)) throw std::invalid_argument("process cube x edges must be finite");
  for (double edge : result.y_edges)
    if (!std::isfinite(edge)) throw std::invalid_argument("process cube y edges must be finite");
  std::ostringstream out;
  out.precision(17);
  out << "{\"family\":\"PROCESS_CUBE\",\"x_dimension\":";
  json_string(out, result.x_dimension);
  out << ",\"y_dimension\":";
  json_string(out, result.y_dimension);
  out << ",\"aggregation\":";
  json_string(out, result.aggregation);
  out << ",\"x_bins\":[";
  for (std::size_t i = 0; i < result.x_bins.size(); ++i) {
    if (i) out << ',';
    write_bin(out, result.x_bins[i]);
  }
  out << "],\"y_bins\":[";
  for (std::size_t i = 0; i < result.y_bins.size(); ++i) {
    if (i) out << ',';
    write_bin(out, result.y_bins[i]);
  }
  out << "],\"x_edges\":[";
  for (std::size_t i = 0; i < result.x_edges.size(); ++i) {
    if (i) out << ',';
    out << result.x_edges[i];
  }
  out << "],\"y_edges\":[";
  for (std::size_t i = 0; i < result.y_edges.size(); ++i) {
    if (i) out << ',';
    out << result.y_edges[i];
  }
  out << "],\"cells\":[";
  for (std::size_t i = 0; i < result.cells.size(); ++i) {
    if (i) out << ',';
    out << '[';
    for (std::size_t j = 0; j < result.cells[i].size(); ++j) {
      if (j) out << ',';
      if (result.cells[i][j]) out << *result.cells[i][j];
      else out << "null";
    }
    out << ']';
  }
  out << "],\"case_ids\":[";
  for (std::size_t i = 0; i < result.case_ids.size(); ++i) {
    if (i) out << ',';
    out << '[';
    for (std::size_t j = 0; j < result.case_ids[i].size(); ++j) {
      if (j) out << ',';
      out << '[';
      bool first = true;
      for (const auto& id : result.case_ids[i][j]) {
        if (!first) out << ',';
        first = false;
        json_string(out, id);
      }
      out << ']';
    }
    out << ']';
  }
  out << "]}";
  return out.str();
}
}  // namespace minera
