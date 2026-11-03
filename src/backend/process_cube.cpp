#include "minera/process_cube.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <variant>

namespace minera {
namespace {
struct Axis {
  enum class Mode { kNumeric, kPrefix, kCategorical };
  Mode mode = Mode::kNumeric;
  std::string name;
  std::vector<std::string> columns;
  std::vector<ProcessCubeBin> bins;
  std::map<std::string, std::size_t> labels;
};

std::string choose(const std::string& a, const std::string& b) {
  return a.empty() ? b : a;
}

std::string join_names(const std::vector<std::string>& names) {
  std::ostringstream out;
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (i) out << ',';
    out << names[i];
  }
  return out.str();
}

void merge_numeric_columns(ProcessCubeTable& table) {
  if (table.columns.empty()) {
    table.columns = table.numeric_columns;
    return;
  }
  for (const auto& [name, values] : table.numeric_columns) {
    auto [it, inserted] = table.columns.emplace(name, values);
    if (!inserted && it->second != values)
      throw std::invalid_argument("duplicate process cube numeric column: " + name);
  }
}

void merge_categorical_columns(ProcessCubeTable& table) {
  if (table.categorical_columns.empty()) {
    table.categorical_columns = table.string_columns;
    return;
  }
  for (const auto& [name, values] : table.string_columns) {
    auto [it, inserted] = table.categorical_columns.emplace(name, values);
    if (!inserted && it->second != values)
      throw std::invalid_argument("duplicate process cube categorical column: " + name);
  }
}

std::string scalar_label(const ProcessCubeTable& table, const std::string& name,
                         std::size_t row) {
  if (const auto it = table.categorical_columns.find(name);
      it != table.categorical_columns.end()) {
    return it->second[row];
  }
  const auto it = table.columns.find(name);
  if (it == table.columns.end())
    throw std::invalid_argument("unknown process cube dimension column: " + name);
  std::ostringstream out;
  out.precision(17);
  out << it->second[row];
  return out.str();
}

std::string categorical_key(const std::vector<std::string>& values) {
  // Length-prefixing keeps distinct tuples distinct even when values contain
  // punctuation used by the human-readable bin label.
  std::ostringstream key;
  for (const auto& value : values) key << value.size() << ':' << value;
  return key.str();
}

std::string categorical_axis_key(const std::vector<std::string>& values) {
  return values.size() == 1 ? values.front() : categorical_key(values);
}

std::string categorical_display(const std::vector<std::string>& values) {
  if (values.size() == 1) return values.front();
  std::ostringstream display;
  display << '(';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) display << ',';
    display << values[i];
  }
  display << ')';
  return display.str();
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
  Axis axis; axis.mode = Axis::Mode::kNumeric; axis.name = name;
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
  for (std::size_t i = 0; i + 1 < edges.size(); ++i) {
    std::ostringstream label;
    label.precision(17);
    label << '[' << edges[i] << ',' << edges[i + 1]
          << (i + 2 == edges.size() ? ']' : ')');
    axis.bins.push_back({label.str(), edges[i], edges[i + 1], true});
  }
  return axis;
}

Axis prefix_axis(const std::string& name, const ProcessCubeTable& table) {
  Axis axis; axis.mode = Axis::Mode::kPrefix; axis.name = name;
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

Axis categorical_axis(const std::string& name,
                      const std::vector<std::string>& columns,
                      const ProcessCubeTable& table) {
  Axis axis;
  axis.mode = Axis::Mode::kCategorical;
  axis.name = name;
  axis.columns = columns;
  std::map<std::string, std::vector<std::string>> values;
  for (std::size_t row = 0; row < table.case_ids.size(); ++row) {
    std::vector<std::string> parts;
    parts.reserve(columns.size());
    for (const auto& column : columns) parts.push_back(scalar_label(table, column, row));
    values.emplace(categorical_axis_key(parts), std::move(parts));
  }
  for (auto& [key, parts] : values) {
    const auto index = axis.bins.size();
    axis.labels.emplace(key, index);
    ProcessCubeBin bin;
    bin.label = categorical_display(parts);
    bin.numeric = false;
    axis.bins.push_back(std::move(bin));
  }
  if (axis.bins.empty())
    throw std::invalid_argument("process cube categorical dimension has no values: " + name);
  return axis;
}

std::vector<std::string> dimension_columns(const ProcessCubeOptions& options,
                                           bool x_axis) {
  const auto& explicit_columns = x_axis ? options.x_columns : options.y_columns;
  const auto& short_name = x_axis ? options.x_col : options.y_col;
  const auto& long_name = x_axis ? options.x_dimension : options.y_dimension;
  if (!explicit_columns.empty()) {
    if (!short_name.empty() || !long_name.empty())
      throw std::invalid_argument("process cube composite dimensions cannot be combined with x_col/y_col aliases");
    for (const auto& column : explicit_columns)
      if (column.empty()) throw std::invalid_argument("process cube dimension names must not be empty");
    return explicit_columns;
  }
  const auto name = choose(short_name, long_name);
  if (name.empty()) throw std::invalid_argument("process cube dimensions are required");
  return {name};
}

Axis make_axis(const std::vector<std::string>& columns,
               const ProcessCubeTable& table,
               const std::vector<double>& requested,
               std::size_t max_divisions) {
  if (columns.size() > 1) {
    if (!requested.empty())
      throw std::invalid_argument("process cube composite dimensions do not accept numeric bins");
    return categorical_axis(join_names(columns), columns, table);
  }
  const auto& name = columns.front();
  const auto numeric = table.columns.find(name);
  const auto categorical = table.categorical_columns.find(name);
  if (numeric != table.columns.end() && categorical != table.categorical_columns.end())
    throw std::invalid_argument("process cube dimension has both numeric and categorical columns: " + name);
  if (numeric != table.columns.end()) {
    auto axis = numeric_axis(name, numeric->second, requested, max_divisions);
    axis.columns = {name};
    return axis;
  }
  if (categorical != table.categorical_columns.end()) {
    if (!requested.empty())
      throw std::invalid_argument("process cube categorical dimensions do not accept numeric bins");
    return categorical_axis(name, columns, table);
  }
  return prefix_axis(name, table);
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

int json_hex_digit(char value);
void append_utf8(std::string& output, std::uint32_t codepoint);

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
  // parse_value recurses per nesting level; a deep document would exhaust the
  // request thread's stack. The table shape needs three levels; 64 matches the
  // other request JSON parsers.
  static constexpr std::size_t kMaxDepth = 64;
  std::size_t depth_ = 0;
  struct DepthGuard {
    JsonParser& parser;
    explicit DepthGuard(JsonParser& value) : parser(value) {
      if (++parser.depth_ > kMaxDepth) {
        --parser.depth_;
        parser.fail("nesting exceeds 64 levels");
      }
    }
    ~DepthGuard() { --parser.depth_; }
  };
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
        case 'u': {
          if (pos_ + 4 > input_.size()) fail("short unicode escape");
          std::uint32_t codepoint = 0;
          for (int i = 0; i < 4; ++i) {
            const int digit = json_hex_digit(input_[pos_++]);
            if (digit < 0) fail("invalid unicode escape");
            codepoint = (codepoint << 4) | static_cast<std::uint32_t>(digit);
          }
          if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
            if (pos_ + 6 > input_.size() || input_[pos_] != '\\' ||
                input_[pos_ + 1] != 'u')
              fail("unpaired unicode surrogate");
            pos_ += 2;
            std::uint32_t low = 0;
            for (int i = 0; i < 4; ++i) {
              const int digit = json_hex_digit(input_[pos_++]);
              if (digit < 0) fail("invalid unicode escape");
              low = (low << 4) | static_cast<std::uint32_t>(digit);
            }
            if (low < 0xdc00 || low > 0xdfff)
              fail("invalid unicode surrogate pair");
            codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
          } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
            fail("unpaired unicode surrogate");
          }
          append_utf8(result, codepoint);
          break;
        }
        default: fail("unsupported string escape");
      }
    }
    fail("unterminated string");
  }
  JsonValue parse_value() {
    const DepthGuard depth(*this);
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

int json_hex_digit(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}

void append_utf8(std::string& output, std::uint32_t codepoint) {
  if (codepoint <= 0x7f) {
    output.push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7ff) {
    output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
    output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
  } else if (codepoint <= 0xffff) {
    output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
    output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
    output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
  } else {
    output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
    output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
    output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
    output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
  }
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
  merge_numeric_columns(data);
  merge_categorical_columns(data);
  for (const auto& [name, values] : data.categorical_columns) {
    if (data.columns.contains(name))
      throw std::invalid_argument("process cube column has both numeric and categorical values: " + name);
    (void)values;
  }
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
  for (const auto& [name, values] : data.categorical_columns) {
    if (name.empty()) throw std::invalid_argument("column name must not be empty");
    if (values.size() != table.case_ids.size()) throw std::invalid_argument("process cube row lengths differ");
  }
  const auto xcolumns = dimension_columns(options, true);
  const auto ycolumns = dimension_columns(options, false);
  Axis x = make_axis(xcolumns, data, options.x_bins, options.max_divisions_x);
  Axis y = make_axis(ycolumns, data, options.y_bins, options.max_divisions_y);
  // Safety limit: the grid is materialised cell by cell (value, case-id set
  // and sample list), so a tiny request with large division counts could
  // otherwise allocate gigabytes before anything is serialised.
  constexpr std::size_t kMaxProcessCubeCells = 1000000;
  if (!x.bins.empty() && y.bins.size() > kMaxProcessCubeCells / x.bins.size())
    throw std::invalid_argument(
        "process cube grid exceeds the safety limit of 1000000 cells");
  const std::string xname = x.name;
  const std::string yname = y.name;
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
    if (x.mode == Axis::Mode::kNumeric) {
      const auto it = data.columns.find(x.columns.front());
      auto i = numeric_index(x, it->second[r]);
      if (i < x.bins.size()) xi.push_back(i);
    } else if (x.mode == Axis::Mode::kCategorical) {
      std::vector<std::string> parts;
      for (const auto& column : x.columns) parts.push_back(scalar_label(data, column, r));
      const auto it = x.labels.find(categorical_axis_key(parts));
      if (it != x.labels.end()) xi.push_back(it->second);
    } else {
      for (const auto& [label, i] : x.labels) {
        auto it = data.columns.find(xname + "_" + label);
        if (it != data.columns.end() && it->second[r] >= 1.0) xi.push_back(i);
      }
    }
    if (y.mode == Axis::Mode::kNumeric) {
      const auto it = data.columns.find(y.columns.front());
      auto i = numeric_index(y, it->second[r]);
      if (i < y.bins.size()) yi.push_back(i);
    } else if (y.mode == Axis::Mode::kCategorical) {
      std::vector<std::string> parts;
      for (const auto& column : y.columns) parts.push_back(scalar_label(data, column, r));
      const auto it = y.labels.find(categorical_axis_key(parts));
      if (it != y.labels.end()) yi.push_back(it->second);
    } else {
      for (const auto& [label, i] : y.labels) {
        auto it = data.columns.find(yname + "_" + label);
        if (it != data.columns.end() && it->second[r] >= 1.0) yi.push_back(i);
      }
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
      throw std::invalid_argument("process cube columns must be named scalar arrays");
    if (values.array().empty()) {
      table.columns.emplace(name, std::vector<double>{});
      continue;
    }
    const bool numeric = values.array().front().is_number();
    const bool categorical = values.array().front().is_string();
    if (!numeric && !categorical)
      throw std::invalid_argument("process cube columns must contain numbers or strings");
    if (numeric) {
      auto& output = table.columns[name];
      output.reserve(values.array().size());
      for (const auto& value : values.array()) {
        if (!value.is_number() || !std::isfinite(value.number()))
          throw std::invalid_argument("process cube numeric columns must contain finite numbers");
        output.push_back(value.number());
      }
    } else {
      auto& output = table.categorical_columns[name];
      output.reserve(values.array().size());
      for (const auto& value : values.array()) {
        if (!value.is_string())
          throw std::invalid_argument("process cube categorical columns must contain strings");
        output.push_back(value.string());
      }
    }
  }
  if (const auto categorical_value = root.find("categorical_columns");
      categorical_value != root.end()) {
    if (!categorical_value->second.is_object())
      throw std::invalid_argument("process cube categorical_columns must be an object");
    for (const auto& [name, values] : categorical_value->second.object()) {
      if (name.empty() || !values.is_array())
        throw std::invalid_argument("process cube categorical columns must be named arrays");
      if (table.columns.contains(name) || table.categorical_columns.contains(name))
        throw std::invalid_argument("duplicate process cube categorical column: " + name);
      auto& output = table.categorical_columns[name];
      output.reserve(values.array().size());
      for (const auto& value : values.array()) {
        if (!value.is_string())
          throw std::invalid_argument("process cube categorical columns must contain strings");
        output.push_back(value.string());
      }
    }
  }
  for (const auto& [name, values] : table.columns)
    if (values.size() != table.case_ids.size())
      throw std::invalid_argument("process cube JSON column length differs from case_ids");
  for (const auto& [name, values] : table.categorical_columns)
    if (values.size() != table.case_ids.size())
      throw std::invalid_argument("process cube JSON categorical column length differs from case_ids");
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
