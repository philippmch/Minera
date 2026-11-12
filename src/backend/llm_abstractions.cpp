#include "minera/llm_abstractions.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "frame_cells.hpp"
#include "minera/dataframe_columns.hpp"
#include "minera/frame_keys.hpp"
#include "minera/log_conversion.hpp"
#include "minera/log_skeleton.hpp"
#include "minera/log_to_features.hpp"
#include "python_scalar.hpp"
#include "minera/ocel_features.hpp"
#include "minera/timestamp.hpp"
#include "root_cause_unicode.hpp"

namespace minera {
namespace {

// ---------------------------------------------------------------------------
// Python text conventions

std::size_t code_points(std::string_view text) {
  return static_cast<std::size_t>(
      std::count_if(text.begin(), text.end(), [](unsigned char c) { return (c & 0xc0) != 0x80; }));
}

bool decode_code_point(std::string_view value, std::size_t start, char32_t& code, std::size_t& end) {
  const auto first = static_cast<unsigned char>(value[start]);
  unsigned count = 0;
  code = first;
  if (first < 0x80) {
  } else if (first >= 0xc2 && first <= 0xdf) {
    code = first & 0x1f;
    count = 1;
  } else if (first >= 0xe0 && first <= 0xef) {
    code = first & 0x0f;
    count = 2;
  } else if (first >= 0xf0 && first <= 0xf4) {
    code = first & 7;
    count = 3;
  } else {
    return false;
  }
  if (count > value.size() - start - 1) return false;
  for (unsigned i = 1; i <= count; ++i) {
    const auto next = static_cast<unsigned char>(value[start + i]);
    if ((next & 0xc0) != 0x80) return false;
    code = (code << 6) | (next & 63);
  }
  if ((count == 1 && code < 0x80) || (count == 2 && code < 0x800) || (count == 3 && code < 0x10000) ||
      code > 0x10ffff)
    return false;
  end = start + count + 1;
  return true;
}

// repr() of a Python str.
std::string py_repr(std::string_view value) {
  const char delimiter =
      value.find('\'') != std::string_view::npos && value.find('"') == std::string_view::npos ? '"' : '\'';
  std::string out(1, delimiter);
  constexpr char hex[] = "0123456789abcdef";
  for (std::size_t i = 0; i < value.size();) {
    char32_t code = 0;
    std::size_t end = i;
    if (!decode_code_point(value, i, code, end)) {
      out += value[i++];
      continue;
    }
    if (code == '\\' || code == static_cast<unsigned char>(delimiter)) {
      out += '\\';
      out += static_cast<char>(code);
    } else if (code == '\n') {
      out += "\\n";
    } else if (code == '\r') {
      out += "\\r";
    } else if (code == '\t') {
      out += "\\t";
    } else if (!root_cause_unicode_printable(code)) {
      const int digits = code <= 0xff ? 2 : code <= 0xffff ? 4 : 8;
      out += digits == 2 ? "\\x" : digits == 4 ? "\\u" : "\\U";
      for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4) out += hex[(code >> shift) & 15];
    } else {
      out.append(value.substr(i, end - i));
    }
    i = end;
  }
  return out + delimiter;
}

// repr() of a Python float: the shortest round-trip digits, positional for
// decimal exponents in [-4, 16), scientific otherwise.
std::string py_float(double value) {
  if (std::isnan(value)) return "nan";
  if (std::isinf(value)) return value < 0 ? "-inf" : "inf";
  std::array<char, 64> buffer{};
  const auto result =
      std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::scientific);
  std::string text(buffer.data(), result.ptr);
  std::string sign;
  if (text.front() == '-') {
    sign = "-";
    text.erase(0, 1);
  }
  const auto e = text.find('e');
  const int exponent = std::stoi(text.substr(e + 1));
  std::string digits;
  for (const char c : text.substr(0, e))
    if (c != '.') digits += c;
  if (exponent < -4 || exponent >= 16) {
    std::string out = sign + digits.substr(0, 1);
    if (digits.size() > 1) out += "." + digits.substr(1);
    const int magnitude = std::abs(exponent);
    return out + (exponent < 0 ? "e-" : "e+") + (magnitude < 10 ? "0" : "") + std::to_string(magnitude);
  }
  if (exponent < 0) return sign + "0." + std::string(static_cast<std::size_t>(-exponent - 1), '0') + digits;
  const auto whole = static_cast<std::size_t>(exponent) + 1;
  if (digits.size() <= whole) return sign + digits + std::string(whole - digits.size(), '0') + ".0";
  return sign + digits.substr(0, whole) + "." + digits.substr(whole);
}

// Python "%.3f".
std::string fixed3(double value) {
  if (std::isnan(value)) return "nan";
  if (std::isinf(value)) return value < 0 ? "-inf" : "inf";
  std::array<char, 400> buffer{};
  const auto result =
      std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::fixed, 3);
  return {buffer.data(), result.ptr};
}

// ---------------------------------------------------------------------------
// NumPy reductions on float64 arrays

double pairwise_sum(const double* values, std::size_t count) {
  if (count < 8) {
    double total = 0.0;
    for (std::size_t i = 0; i < count; ++i) total += values[i];
    return total;
  }
  if (count <= 128) {
    std::array<double, 8> partial{};
    for (std::size_t j = 0; j < 8; ++j) partial[j] = values[j];
    std::size_t i = 8;
    for (; i < count - count % 8; i += 8)
      for (std::size_t j = 0; j < 8; ++j) partial[j] += values[i + j];
    double total = ((partial[0] + partial[1]) + (partial[2] + partial[3])) +
                   ((partial[4] + partial[5]) + (partial[6] + partial[7]));
    for (; i < count; ++i) total += values[i];
    return total;
  }
  std::size_t half = count / 2;
  half -= half % 8;
  return pairwise_sum(values, half) + pairwise_sum(values + half, count - half);
}

double numpy_mean(const std::vector<double>& values) {
  return pairwise_sum(values.data(), values.size()) / static_cast<double>(values.size());
}

double numpy_std(const std::vector<double>& values, std::size_t ddof = 0) {
  const double mean = numpy_mean(values);
  std::vector<double> squares;
  squares.reserve(values.size());
  for (const double value : values) squares.push_back((value - mean) * (value - mean));
  return std::sqrt(pairwise_sum(squares.data(), squares.size()) / static_cast<double>(values.size() - ddof));
}

// numpy.quantile(method="linear") of an already sorted array.
double numpy_quantile(const std::vector<double>& sorted, double q) {
  const double virtual_index = static_cast<double>(sorted.size() - 1) * q;
  const double previous = std::floor(virtual_index);
  const auto low = static_cast<std::size_t>(previous);
  const auto high = std::min(low + 1, sorted.size() - 1);
  const double gamma = virtual_index - previous;
  const double a = sorted[low], b = sorted[high], difference = b - a;
  return gamma >= 0.5 ? b - difference * (1.0 - gamma) : a + difference * gamma;
}

constexpr std::array<double, 5> kQuantiles{0.0, 0.25, 0.5, 0.75, 1.0};

template <typename Format>
std::string quantile_dict(std::vector<double> values, Format format) {
  std::sort(values.begin(), values.end());
  std::string text = "{";
  for (std::size_t i = 0; i < kQuantiles.size(); ++i) {
    if (i) text += ", ";
    text += py_float(kQuantiles[i]) + ": " + format(numpy_quantile(values, kQuantiles[i]));
  }
  return text + "}";
}

// ---------------------------------------------------------------------------
// Event table view: one row per event, event keys then case: prefixed keys.

enum class CellKind { kMissing, kString, kInt, kFloat, kBool, kDate };

struct Cell {
  CellKind kind = CellKind::kMissing;
  std::string text;      // string payload or integer digits
  double number = 0.0;   // int, float or bool value
  std::int64_t micros = 0;
  bool aware = true;
  std::int32_t offset_minutes = 0;
};

Cell date_cell(std::string_view text) {
  Cell cell;
  const auto parsed = parse_timestamp_exact(text);
  if (!parsed) {
    cell.kind = CellKind::kString;
    cell.text = std::string(text);
    return cell;
  }
  cell.kind = CellKind::kDate;
  cell.micros = parsed->seconds * 1000000 + parsed->nanoseconds / 1000;
  cell.aware = parsed->timezone_aware;
  cell.offset_minutes = parsed->offset_minutes;
  return cell;
}

Cell string_cell(std::string text) {
  Cell cell;
  cell.kind = CellKind::kString;
  cell.text = std::move(text);
  return cell;
}

Cell attribute_cell(const EventAttribute& attribute) {
  if (attribute.is_null) return {};
  Cell cell;
  switch (attribute.kind) {
    case AttributeKind::kInt: {
      cell.kind = CellKind::kInt;
      if (const auto* value = std::get_if<std::int64_t>(&attribute.typed_value)) {
        cell.text = std::to_string(*value);
        cell.number = static_cast<double>(*value);
      } else if (const auto* unsigned_value = std::get_if<std::uint64_t>(&attribute.typed_value)) {
        cell.text = std::to_string(*unsigned_value);
        cell.number = static_cast<double>(*unsigned_value);
      } else {
        const long long value = std::strtoll(attribute.value.c_str(), nullptr, 10);
        cell.text = std::to_string(value);
        cell.number = static_cast<double>(value);
      }
      return cell;
    }
    case AttributeKind::kFloat:
      cell.kind = CellKind::kFloat;
      if (const auto* value = std::get_if<double>(&attribute.typed_value))
        cell.number = *value;
      else
        cell.number = std::strtod(attribute.value.c_str(), nullptr);
      return cell;
    case AttributeKind::kBoolean:
      cell.kind = CellKind::kBool;
      if (const auto* value = std::get_if<bool>(&attribute.typed_value))
        cell.number = *value ? 1.0 : 0.0;
      else
        cell.number = attribute.value == "true" || attribute.value == "True" || attribute.value == "1" ? 1.0 : 0.0;
      return cell;
    case AttributeKind::kDate:
      return date_cell(attribute.value);
    default:
      return string_cell(attribute.value);
  }
}

using Row = std::vector<std::pair<std::string, Cell>>;

Cell first_class_cell(FirstClassAttributeState state, const std::string& text) {
  return state == FirstClassAttributeState::kNull ? Cell{} : string_cell(text);
}

void put(Row& row, std::string key, Cell cell) {
  for (auto& entry : row)
    if (entry.first == key) return;
  row.emplace_back(std::move(key), std::move(cell));
}

Row event_row(const Trace& trace, const Event& event) {
  Row row;
  if (event.has_activity()) put(row, "concept:name", first_class_cell(event.activity_state, event.activity));
  if (event.has_resource()) put(row, "org:resource", first_class_cell(event.resource_state, event.resource));
  if (event.has_timestamp())
    put(row, "time:timestamp",
        event.timestamp_state == FirstClassAttributeState::kNull ? Cell{} : date_cell(event.timestamp));
  if (event.has_lifecycle())
    put(row, "lifecycle:transition", first_class_cell(event.lifecycle_state, event.lifecycle));
  for (const auto& attribute : event.attributes) put(row, attribute.key, attribute_cell(attribute));
  if (trace.has_case_id())
    put(row, "case:concept:name", first_class_cell(trace.case_id_state, trace.case_id));
  for (const auto& attribute : trace.attributes)
    if (attribute.key != "concept:name") put(row, "case:" + attribute.key, attribute_cell(attribute));
  return row;
}

std::int64_t event_micros(const Event& event) {
  if (!event.has_timestamp() || event.timestamp_state == FirstClassAttributeState::kNull)
    throw std::invalid_argument("every event needs a timestamp");
  const auto cell = date_cell(event.timestamp);
  if (cell.kind != CellKind::kDate) throw std::invalid_argument("unparseable event timestamp");
  return cell.micros;
}

// Civil date of a day count since 1970-01-01.
void civil(std::int64_t days, int& year, int& month, int& day) {
  days += 719468;
  const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  const auto doe = static_cast<unsigned>(days - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  year = static_cast<int>(yoe + era * 400 + (month <= 2));
}

struct Fields {
  int year, month, day, hour, minute, second, micro;
};

Fields fields(std::int64_t micros) {
  std::int64_t seconds = micros / 1000000, micro = micros % 1000000;
  if (micro < 0) {
    micro += 1000000;
    --seconds;
  }
  std::int64_t days = seconds / 86400, rest = seconds % 86400;
  if (rest < 0) {
    rest += 86400;
    --days;
  }
  Fields out{};
  civil(days, out.year, out.month, out.day);
  out.hour = static_cast<int>(rest / 3600);
  out.minute = static_cast<int>(rest / 60 % 60);
  out.second = static_cast<int>(rest % 60);
  out.micro = static_cast<int>(micro);
  return out;
}

// repr() of a pandas Timestamp at microsecond resolution.
std::string timestamp_repr(std::int64_t micros, bool aware) {
  const auto f = fields(micros);
  std::array<char, 64> text{};
  std::snprintf(text.data(), text.size(), "%04d-%02d-%02d %02d:%02d:%02d", f.year, f.month, f.day, f.hour, f.minute,
                f.second);
  std::string out = "Timestamp('" + std::string(text.data());
  if (f.micro) {
    std::snprintf(text.data(), text.size(), ".%06d", f.micro);
    out += text.data();
  }
  return out + (aware ? "+0000', tz='UTC')" : "')");
}

std::string timedelta_repr(std::int64_t seconds) {
  std::int64_t days = seconds / 86400, rest = seconds % 86400;
  if (rest < 0) {
    rest += 86400;
    --days;
  }
  std::string out = "datetime.timedelta(";
  if (days) out += "days=" + std::to_string(days);
  if (rest) out += std::string(days ? ", " : "") + "seconds=" + std::to_string(rest);
  return out + ")";
}

// repr() of a Python datetime in its own UTC offset.
std::string datetime_repr(const Cell& cell) {
  const auto f = fields(cell.micros + static_cast<std::int64_t>(cell.offset_minutes) * 60000000);
  std::string out = "datetime.datetime(" + std::to_string(f.year) + ", " + std::to_string(f.month) + ", " +
                    std::to_string(f.day) + ", " + std::to_string(f.hour) + ", " + std::to_string(f.minute);
  if (f.second || f.micro) out += ", " + std::to_string(f.second);
  if (f.micro) out += ", " + std::to_string(f.micro);
  if (cell.aware)
    out += cell.offset_minutes == 0
               ? ", tzinfo=datetime.timezone.utc"
               : ", tzinfo=datetime.timezone(" + timedelta_repr(static_cast<std::int64_t>(cell.offset_minutes) * 60) +
                     ")";
  return out + ")";
}

std::string cell_repr(const Cell& cell) {
  switch (cell.kind) {
    case CellKind::kMissing:
      return "None";
    case CellKind::kString:
      return py_repr(cell.text);
    case CellKind::kInt:
      return cell.text;
    case CellKind::kFloat:
      return py_float(cell.number);
    case CellKind::kBool:
      return cell.number != 0.0 ? "True" : "False";
    case CellKind::kDate:
      return datetime_repr(cell);
  }
  return "None";
}

// str() of a value in an object column.
std::string cell_str(const Cell& cell) {
  switch (cell.kind) {
    case CellKind::kString:
      return cell.text;
    case CellKind::kDate: {
      const auto f = fields(cell.micros + static_cast<std::int64_t>(cell.offset_minutes) * 60000000);
      std::array<char, 64> text{};
      std::snprintf(text.data(), text.size(), "%04d-%02d-%02d %02d:%02d:%02d", f.year, f.month, f.day, f.hour,
                    f.minute, f.second);
      std::string out = text.data();
      if (f.micro) {
        std::snprintf(text.data(), text.size(), ".%06d", f.micro);
        out += text.data();
      }
      if (cell.aware) {
        const int offset = std::abs(cell.offset_minutes);
        std::snprintf(text.data(), text.size(), "%c%02d:%02d", cell.offset_minutes < 0 ? '-' : '+', offset / 60,
                      offset % 60);
        out += text.data();
      }
      return out;
    }
    default:
      return cell_repr(cell);
  }
}

// ---------------------------------------------------------------------------
// Performance aggregations

std::optional<double> aggregate(const std::string& name, std::vector<double> values, bool sample_stdev) {
  if (name == "mean") return numpy_mean(values);
  if (name == "sum") return pairwise_sum(values.data(), values.size());
  if (name == "max") return *std::max_element(values.begin(), values.end());
  if (name == "min") return *std::min_element(values.begin(), values.end());
  if (name == "median") {
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    return values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2.0;
  }
  if (name == "stdev") {
    // DFG arcs use the sample deviation (NaN for one value); variants use the
    // population deviation and have none for a single case.
    if (sample_stdev) return values.size() < 2 ? NAN : numpy_std(values, 1);
    if (values.size() < 2) return std::nullopt;
    return numpy_std(values);
  }
  return std::nullopt;
}

bool known_aggregation(const std::string& name) {
  return name == "mean" || name == "sum" || name == "max" || name == "min" || name == "median" || name == "stdev";
}

// The reference describes the event table, which has no rows for an empty
// trace, so empty traces neither form a variant nor count as cases.
std::size_t nonempty_cases(const EventLog& log) {
  return static_cast<std::size_t>(std::count_if(log.traces.begin(), log.traces.end(),
                                                [](const Trace& trace) { return !trace.events.empty(); }));
}

std::string frequency_text(std::size_t count, std::size_t total, bool relative) {
  if (!relative) return std::to_string(count);
  return std::to_string(static_cast<long long>(100.0 * static_cast<double>(count) / static_cast<double>(total))) +
         "\\%";
}

std::string performance_text(const std::vector<double>& values, const LlmAbstractionOptions& options, bool dfg) {
  const auto primary = aggregate(options.primary_performance_aggregation, values, dfg);
  if (!primary) throw std::invalid_argument("primary performance aggregation has no value");
  std::string text = "  performance = " + fixed3(*primary);
  const auto& secondary = options.secondary_performance_aggregation;
  if (!secondary.empty()) {
    const auto value = aggregate(secondary, values, dfg);
    if (value && !std::isnan(*value)) text += "  " + secondary + " = " + fixed3(*value);
  }
  return text;
}

double seconds_between(std::int64_t from, std::int64_t to) { return static_cast<double>(to - from) / 1e6; }

// Feature columns described by count of positive values and their quantiles,
// by descending count, coefficient of variation and description.
std::string describe_features(const std::vector<std::string>& descriptions,
                              const std::vector<std::vector<double>>& columns, std::string text,
                              std::size_t max_len) {
  struct Entry {
    std::size_t count;
    double variation;
    std::string description, line;
  };
  std::vector<Entry> entries;
  for (std::size_t column = 0; column < columns.size(); ++column) {
    std::vector<double> values;
    for (const double value : columns[column])
      if (value > 0.0) values.push_back(value);
    if (values.empty()) continue;
    const double variation = numpy_std(values) / numpy_mean(values);
    entries.push_back({values.size(), variation, descriptions[column],
                       descriptions[column] + ":    number of non-zero values: " + std::to_string(values.size()) +
                           " ; quantiles of the non-zero: " + quantile_dict(values, py_float) + "\n"});
  }
  std::stable_sort(entries.begin(), entries.end(), [](const Entry& left, const Entry& right) {
    if (left.count != right.count) return left.count > right.count;
    if (left.variation != right.variation) return left.variation > right.variation;
    return left.description > right.description;
  });
  for (const auto& entry : entries) {
    if (code_points(text) > max_len) break;
    text += entry.line;
  }
  return text;
}

std::vector<std::string> split_text(std::string_view text, char separator) {
  std::vector<std::string> parts;
  std::size_t start = 0;
  for (std::size_t at; (at = text.find(separator, start)) != std::string_view::npos; start = at + 1)
    parts.emplace_back(text.substr(start, at - start));
  parts.emplace_back(text.substr(start));
  return parts;
}

// Description of a log-to-features column name.
std::string log_feature_description(const std::string& name) {
  if (name.starts_with("succession:")) {
    const auto parts = split_text(std::string_view(name).substr(11), '@');
    const auto pair = split_text(parts.back(), '#');
    return "Succession '" + pair.front() + "' -> '" + pair.back() + "' for the Values of the Attribute '" +
           parts.front() + "'";
  }
  if (name.starts_with("trace:") || name.starts_with("event:")) {
    const std::string scope = name.starts_with("trace:") ? "Case" : "Event";
    const auto parts = split_text(std::string_view(name).substr(6), '@');
    return parts.size() == 1 ? "Values for " + scope + " Attribute '" + parts.front() + "'"
                             : "Value '" + parts.back() + "' for " + scope + " Attribute '" + parts.front() + "'";
  }
  return "None";
}

// Description of an object feature column; the reference swaps the two
// inheritance-graph texts and names the descendants graph in one of them.
std::string object_feature_description(const std::string& name) {
  static const std::map<std::string, std::string> fixed{
      {"@@object_lifecycle_length", "Number of events in the lifecycle of the object"},
      {"@@object_lifecycle_duration", "Duration of the lifecycle of the object"},
      {"@@object_lifecycle_start_timestamp", "Start timestamp of the lifecycle of the object"},
      {"@@object_lifecycle_end_timestamp", "Completion timestamp of the lifecycle of the object"},
      {"@@object_degree_centrality", "Degree centrality of the object in the object interaction graph"},
      {"@@object_general_interaction_graph", "Number of objects related in the object interaction graph"},
      {"@@object_general_descendants_graph_ascendants",
       "Number of objects which precede the current object in the object descendants graph"},
      {"@@object_general_descendants_graph_descendants",
       "Number of objects which follow the current object in the object descendants graph"},
      {"@@object_general_inheritance_graph_ascendants",
       "Number of objects which follow the current object in the object inheritance graph"},
      {"@@object_general_inheritance_graph_descendants",
       "Number of objects which precede the current object in the object descendants graph"},
      {"@@object_cobirth", "Number of objects starting their lifecycle together with the current object"},
      {"@@object_codeath", "Number of objects ending their lifecycle together with the current object"},
      {"@@object_lifecycle_unq_act", "Number of unique activities in the lifecycle of the object"},
  };
  if (const auto found = fixed.find(name); found != fixed.end()) return found->second;
  constexpr std::string_view activity = "@@ocel_lif_activity_", interaction = "@@object_interaction_graph_";
  if (name.starts_with(activity)) return "Number of occurrences of the activity " + name.substr(activity.size());
  if (name.starts_with(interaction))
    return "Number of object of type " + name.substr(interaction.size()) +
           " related to the current object in the object interaction graph";
  return name;
}

// ---------------------------------------------------------------------------

// One column of the event table to describe: a cell per row and, for a
// DataFrame upload, the class its dtype gives it (kInfer: from the cells, as
// the reference's conversion of an EventLog types them).
enum class ColumnClass { kInfer, kSkip, kNumbers, kDates, kCounts };

struct DescribedColumn {
  std::string key;
  std::vector<Cell> cells;
  ColumnClass kind = ColumnClass::kInfer;
};

bool numeric_cell_kind(CellKind kind) {
  return kind == CellKind::kInt || kind == CellKind::kFloat || kind == CellKind::kBool;
}

// The described columns, in order. With merge_numbers (a DataFrame's object
// and bool columns) value counts key numbers by value, as Python dict keys
// do (True, 1 and 1.0 are one key, named by the first one met).
std::string describe_columns(const std::vector<DescribedColumn>& columns, std::size_t max_len,
                             bool merge_numbers) {
  std::map<std::string, std::string> lines;
  std::size_t measure = 0;
  struct Piece {
    std::size_t count;
    std::string column;
    std::size_t rank;
    std::string text, head;
  };
  std::vector<Piece> pieces;
  for (const auto& described : columns) {
    const auto& key = described.key;
    const auto& column = described.cells;
    std::size_t missing = 0;
    bool all_date = true, all_bool = true, all_int = true, all_number = true, all_string = true;
    std::vector<const Cell*> present;
    for (const auto& cell : column) {
      if (cell.kind == CellKind::kMissing) {
        ++missing;
        continue;
      }
      present.push_back(&cell);
      all_date &= cell.kind == CellKind::kDate;
      all_bool &= cell.kind == CellKind::kBool;
      all_int &= cell.kind == CellKind::kInt;
      all_number &= cell.kind == CellKind::kInt || cell.kind == CellKind::kFloat;
      all_string &= cell.kind == CellKind::kString;
    }
    auto kind = described.kind;
    if (kind == ColumnClass::kInfer)
      kind = all_date                              ? ColumnClass::kDates
             : all_number && !(all_int && missing == 0) ? ColumnClass::kNumbers
             : !all_number && !all_string        ? ColumnClass::kCounts
                                                 : ColumnClass::kSkip;
    const auto empty = std::to_string(missing);
    if (kind == ColumnClass::kDates) {
      bool aware = false;
      std::vector<double> values;
      for (const auto* cell : present) {
        values.push_back(static_cast<double>(cell->micros));
        aware |= cell->aware;
      }
      if (values.empty()) continue;
      lines[key] = key + "  empty: " + empty + "  quantiles: " + quantile_dict(values, [&](double value) {
                     return timestamp_repr(static_cast<std::int64_t>(value), aware);
                   });
      measure += code_points(lines[key]);
    } else if (kind == ColumnClass::kNumbers) {
      std::vector<double> values;
      for (const auto* cell : present) values.push_back(cell->number);
      // A float column of NaN cells only has NaN quantiles.
      lines[key] = key + "  empty: " + empty + "  quantiles: " +
                   (values.empty() ? std::string("{0.0: nan, 0.25: nan, 0.5: nan, 0.75: nan, 1.0: nan}")
                                   : quantile_dict(values, py_float));
      measure += code_points(lines[key]);
    } else if (kind == ColumnClass::kCounts) {
      // Boolean and mixed columns: value counts, by count and then value.
      struct Count {
        const Cell* cell;
        std::size_t count;
      };
      const auto same = [&](const Cell& a, const Cell& b) {
        if (merge_numbers && numeric_cell_kind(a.kind) && numeric_cell_kind(b.kind))
          return a.kind == CellKind::kInt && b.kind == CellKind::kInt ? a.text == b.text : a.number == b.number;
        return a.kind == b.kind && a.text == b.text && a.number == b.number && a.micros == b.micros;
      };
      std::vector<Count> counts;
      for (const auto* cell : present) {
        auto found = std::find_if(counts.begin(), counts.end(), [&](const Count& entry) { return same(*entry.cell, *cell); });
        if (found == counts.end())
          counts.push_back({cell, 1});
        else
          ++found->count;
      }
      if (counts.size() <= 1) continue;
      const auto family = [](const Cell& cell) {
        return cell.kind == CellKind::kString ? 0 : cell.kind == CellKind::kDate ? 2 : 1;
      };
      for (std::size_t i = 0; i < counts.size(); ++i)
        for (std::size_t j = i + 1; j < counts.size(); ++j)
          if (counts[i].count == counts[j].count && family(*counts[i].cell) != family(*counts[j].cell))
            throw std::invalid_argument("column " + key + " ties values that cannot be ordered");
      std::sort(counts.begin(), counts.end(), [&](const Count& left, const Count& right) {
        if (left.count != right.count) return left.count > right.count;
        const auto& a = *left.cell;
        const auto& b = *right.cell;
        if (family(a) == 0) return a.text > b.text;
        if (family(a) == 2) return a.micros > b.micros;
        return a.number > b.number;
      });
      for (std::size_t rank = 0; rank < counts.size(); ++rank)
        pieces.push_back({counts[rank].count, key, rank,
                          " (" + cell_str(*counts[rank].cell) + "; freq. " + std::to_string(counts[rank].count) + ")",
                          key + "  empty: " + empty + " values:"});
    }
  }
  std::stable_sort(pieces.begin(), pieces.end(), [](const Piece& left, const Piece& right) {
    if (left.count != right.count) return left.count > right.count;
    if (left.column != right.column) return left.column > right.column;
    return left.rank < right.rank;
  });
  std::map<std::string, std::string> started;
  for (const auto& piece : pieces) {
    if (measure >= max_len) break;
    auto found = started.find(piece.column);
    if (found == started.end()) {
      found = started.emplace(piece.column, piece.head).first;
      // The reference budget charges eight more characters per started column.
      measure += code_points(piece.head) + 8;
    }
    found->second += piece.text;
    measure += code_points(piece.text);
  }
  lines.insert(started.begin(), started.end());
  std::string text;
  for (const auto& [key, line] : lines) {
    if (!text.empty()) text += '\n';
    text += line;
  }
  return text;
}

// ---------------------------------------------------------------------------
// The DataFrame of a CSV or XES upload (format_dataframe's or read_xes's
// frame), which the reference abstractions read directly there.

enum class FrameDtype { kInt64, kFloat64, kBool, kStr, kStringNA, kObject, kDate };

struct FrameView {
  std::vector<std::string> keys;
  std::vector<FrameDtype> dtypes;
  std::vector<std::vector<Cell>> rows;  // a cell per key
  std::vector<std::int64_t> micros;     // each row's time:timestamp
};

FrameDtype frame_dtype(DataFrameColumnType type) {
  switch (type) {
    case DataFrameColumnType::kInt64: return FrameDtype::kInt64;
    case DataFrameColumnType::kFloat64: return FrameDtype::kFloat64;
    case DataFrameColumnType::kBool: return FrameDtype::kBool;
    case DataFrameColumnType::kString: return FrameDtype::kStr;
    case DataFrameColumnType::kObject: return FrameDtype::kObject;
    case DataFrameColumnType::kDate: return FrameDtype::kDate;
  }
  return FrameDtype::kObject;
}

Cell scalar_cell(const python::Scalar& value) {
  using Kind = python::Scalar::Kind;
  Cell cell;
  switch (value.kind) {
    case Kind::kText:
      return string_cell(value.text);
    case Kind::kBool:
      cell.kind = CellKind::kBool;
      cell.number = value.boolean ? 1.0 : 0.0;
      return cell;
    case Kind::kInt:
      cell.kind = CellKind::kInt;
      cell.text = (value.negative ? "-" : "") + value.digits;
      cell.number = std::strtod(cell.text.c_str(), nullptr);
      return cell;
    case Kind::kFloat:
      if (std::isnan(value.real)) return {};
      cell.kind = CellKind::kFloat;
      cell.number = value.real;
      return cell;
    case Kind::kDate:
      cell.kind = CellKind::kDate;
      cell.micros = value.date.seconds * 1000000 + value.date.nanoseconds / 1000;
      cell.aware = value.date.timezone_aware;
      cell.offset_minutes = value.date.offset_minutes;
      return cell;
    default:
      return {};
  }
}

// An object column's cell as the frame holds the Python object: an XES
// date keeps its own UTC offset (a datetime.datetime, not a Timestamp).
std::optional<python::Scalar> object_scalar(const Trace& trace, const Event& event, const std::string& key) {
  const EventAttribute* found = nullptr;
  for (const auto& attribute : event.attributes)
    if (attribute.key == key) found = &attribute;
  if (!found && key.starts_with("case:"))
    for (const auto& attribute : trace.attributes)
      if (attribute.key == std::string_view(key).substr(5)) found = &attribute;
  if (!found) return std::nullopt;
  const auto value = python::from_attribute(*found);
  if (python::is_missing(value)) return std::nullopt;
  return value;
}

FrameView frame_view(const EventLog& log) {
  const auto& columns = *log.dataframe_columns;
  const bool csv = columns.csv_text;
  FrameView view;
  view.keys = columns.order;
  if (view.keys.empty()) {
    // read_xes's frame lists each key where its rows first have it (each
    // row's event attributes in document order, then the case: attributes),
    // the rows in file order: a case whose <trace> elements repeat its name
    // apart from each other has its rows apart too (round BG, observed
    // black-box).
    const auto stream = event_log_to_event_stream(log);
    std::vector<std::size_t> first_record;
    std::size_t records = 0;
    for (const auto& trace : log.traces) {
      first_record.push_back(records);
      records += trace.events.size();
    }
    std::set<std::string> seen;
    for (const auto& row : frame_rows(log)) {
      const auto trace = static_cast<std::size_t>(&row.trace - log.traces.data());
      const auto event = static_cast<std::size_t>(&row.event - row.trace.events.data());
      for (const auto& field : stream.events[first_record[trace] + event].fields)
        if (seen.insert(field.key).second) view.keys.push_back(field.key);
    }
  }
  std::map<std::string, FrameDtype> dtypes;
  for (const auto& column : columns.columns) dtypes[column.key] = frame_dtype(column.type);
  for (const auto& column : columns.trace_columns) dtypes["case:" + column.key] = frame_dtype(column.type);
  dtypes["time:timestamp"] = FrameDtype::kDate;
  // format_dataframe's case and activity copies are of the string dtype
  // (pd.NA); read_xes's text columns are str.
  dtypes["concept:name"] = csv ? FrameDtype::kStringNA : FrameDtype::kStr;
  dtypes["case:concept:name"] = csv ? FrameDtype::kStringNA : FrameDtype::kStr;
  if (csv) {
    dtypes["@@index"] = dtypes["@@case_index"] = FrameDtype::kInt64;
    for (const auto& trace : log.traces)
      for (const auto& event : trace.events)
        if (!event.activity_source_key.empty() && event.activity_source_key != "concept:name")
          dtypes.emplace(event.activity_source_key, frame_dtype(columns.activity_type));
  }
  const EventLog frame = dataframe_frame_event_log(log);
  const auto keys = frame_keys(frame, FrameKeyScope::kDataFrame);
  std::vector<std::string> kept;
  for (const auto& key : view.keys)
    if (keys.contains(key) && std::find(kept.begin(), kept.end(), key) == kept.end()) kept.push_back(key);
  view.keys = std::move(kept);
  for (const auto& key : view.keys) {
    const auto found = dtypes.find(key);
    view.dtypes.push_back(found == dtypes.end() ? FrameDtype::kObject : found->second);
  }
  // In the frame's row order (an XES upload's file order).
  for (const auto& frame_row : frame_rows(frame)) {
    const auto& trace = frame_row.trace;
    const auto& event = frame_row.event;
    std::vector<Cell> row;
    row.reserve(view.keys.size());
    std::optional<std::int64_t> micros;
    for (std::size_t i = 0; i < view.keys.size(); ++i) {
      const auto& key = view.keys[i];
      const auto value = !csv && view.dtypes[i] == FrameDtype::kObject ? object_scalar(trace, event, key)
                                                                        : frame_row_scalar(frame, trace, event, key);
      row.push_back(value ? scalar_cell(*value) : Cell{});
      if (key == "time:timestamp" && row.back().kind == CellKind::kDate) micros = row.back().micros;
    }
    if (!micros) throw std::invalid_argument("every event needs a timestamp");
    view.rows.push_back(std::move(row));
    view.micros.push_back(*micros);
  }
  return view;
}

std::string frame_columns_description(const EventLog& log, std::size_t max_len) {
  const auto view = frame_view(log);
  std::vector<DescribedColumn> columns;
  for (std::size_t i = 0; i < view.keys.size(); ++i) {
    DescribedColumn column;
    column.key = view.keys[i];
    // The case id column is not described.
    if (column.key == "case:concept:name") continue;
    switch (view.dtypes[i]) {
      case FrameDtype::kFloat64: column.kind = ColumnClass::kNumbers; break;
      case FrameDtype::kDate: column.kind = ColumnClass::kDates; break;
      case FrameDtype::kBool:
      case FrameDtype::kObject:
      case FrameDtype::kStringNA: column.kind = ColumnClass::kCounts; break;
      case FrameDtype::kInt64:
      case FrameDtype::kStr: continue;
    }
    for (const auto& row : view.rows) column.cells.push_back(row[i]);
    columns.push_back(std::move(column));
  }
  return describe_columns(columns, max_len, true);
}

struct StreamItem {
  std::int64_t micros;
  std::string line;
};

// Latest first, ties in log (frame row) order; each line is put in front of
// the others while the text is at most max_len.
std::string stream_text(std::vector<StreamItem> items, const LlmAbstractionOptions& options) {
  std::stable_sort(items.begin(), items.end(),
                   [](const StreamItem& left, const StreamItem& right) { return left.micros > right.micros; });
  const std::string head =
      std::string("\n\n") + (options.response_header ? "If I have the following stream of events:\n\n" : "");
  std::vector<const std::string*> kept;
  std::size_t length = code_points(head);
  for (const auto& item : items) {
    if (length > options.max_len) break;
    kept.push_back(&item.line);
    length += code_points(item.line);
  }
  std::string text = head;
  for (auto line = kept.rbegin(); line != kept.rend(); ++line) text += **line;
  return text + "\n";
}

// The frame's rows as dicts of their present cells in column order (NaN,
// NaT and NA cells left out); a datetime column's cells are Timestamps.
std::string frame_event_stream_description(const EventLog& log, const LlmAbstractionOptions& options) {
  const auto view = frame_view(log);
  std::vector<StreamItem> items;
  items.reserve(view.rows.size());
  for (std::size_t r = 0; r < view.rows.size(); ++r) {
    std::string line = "{";
    for (std::size_t i = 0; i < view.keys.size(); ++i) {
      const auto& cell = view.rows[r][i];
      if (cell.kind == CellKind::kMissing) continue;
      if (line.size() > 1) line += ", ";
      line += py_repr(view.keys[i]) + ": " +
              (view.dtypes[i] == FrameDtype::kDate && cell.kind == CellKind::kDate ? timestamp_repr(cell.micros, true)
                                                                                    : cell_repr(cell));
    }
    items.push_back({view.micros[r], line + "}\n"});
  }
  return stream_text(std::move(items), options);
}

}  // namespace

std::string llm_log_columns_description(const EventLog& log, std::size_t max_len) {
  if (log.dataframe_columns) return frame_columns_description(log, max_len);
  std::vector<DescribedColumn> columns;
  std::map<std::string, std::size_t> index;
  std::size_t rows = 0;
  for (const auto& trace : log.traces)
    for (const auto& event : trace.events) {
      for (auto& [key, cell] : event_row(trace, event)) {
        auto [found, inserted] = index.emplace(key, columns.size());
        if (inserted) columns.push_back({key, {}, ColumnClass::kInfer});
        auto& column = columns[found->second].cells;
        column.resize(rows);
        column.push_back(std::move(cell));
      }
      ++rows;
    }
  for (auto& column : columns) column.cells.resize(rows);
  return describe_columns(columns, max_len, false);
}

std::string llm_log_dfg_description(const EventLog& log, const LlmAbstractionOptions& options) {
  if (!known_aggregation(options.primary_performance_aggregation))
    throw std::invalid_argument("unknown primary performance aggregation");
  std::map<std::pair<std::string, std::string>, std::vector<double>> arcs;
  std::map<std::pair<std::string, std::string>, std::size_t> cases;
  for (const auto& trace : log.traces) {
    std::vector<std::pair<std::int64_t, const Event*>> events;
    for (const auto& event : trace.events) events.emplace_back(event_micros(event), &event);
    std::stable_sort(events.begin(), events.end(),
                     [](const auto& left, const auto& right) { return left.first < right.first; });
    std::map<std::pair<std::string, std::string>, bool> seen;
    for (std::size_t i = 1; i < events.size(); ++i) {
      const std::pair<std::string, std::string> arc{events[i - 1].second->activity, events[i].second->activity};
      arcs[arc].push_back(seconds_between(events[i - 1].first, events[i].first));
      if (!seen[arc]) {
        seen[arc] = true;
        ++cases[arc];
      }
    }
  }
  std::vector<std::pair<std::pair<std::string, std::string>, std::size_t>> order;
  for (const auto& [arc, values] : arcs)
    order.emplace_back(arc, options.relative_frequency ? cases[arc] : values.size());
  std::sort(order.begin(), order.end(), [](const auto& left, const auto& right) {
    return left.second != right.second ? left.second > right.second : left.first > right.first;
  });
  std::string text = options.response_header ? "If I have a process with flow:\n\n" : "\n\n";
  for (const auto& [arc, count] : order) {
    if (code_points(text) > options.max_len) break;
    text += arc.first + " -> " + arc.second + " ( frequency = " +
            frequency_text(count, nonempty_cases(log), options.relative_frequency);
    if (options.include_performance) text += performance_text(arcs[arc], options, true);
    text += " )\n";
  }
  return text + "\n\n";
}

std::string llm_log_variants_description(const EventLog& log, const LlmAbstractionOptions& options) {
  std::map<std::vector<std::string>, std::vector<double>> variants;
  for (const auto& trace : log.traces) {
    if (trace.events.empty()) continue;
    std::vector<std::string> activities;
    std::int64_t first = 0, last = 0;
    for (std::size_t i = 0; i < trace.events.size(); ++i) {
      activities.push_back(trace.events[i].activity);
      const auto micros = event_micros(trace.events[i]);
      first = i ? std::min(first, micros) : micros;
      last = i ? std::max(last, micros) : micros;
    }
    variants[activities].push_back(seconds_between(first, last));
  }
  // A variant's primary aggregation can have no value (the stdev of a single
  // case): the reference then sorts it like an equal value (None == None
  // between variants of one case each) and fails only when it prints that
  // variant's performance, so a line max_len cuts off never fails.
  struct Entry {
    const std::vector<std::string>* activities;
    const std::vector<double>* durations;
    std::optional<double> performance;
  };
  std::vector<Entry> order;
  for (const auto& [activities, durations] : variants)
    order.push_back({&activities, &durations, aggregate(options.primary_performance_aggregation, durations, false)});
  std::sort(order.begin(), order.end(), [](const Entry& left, const Entry& right) {
    if (left.durations->size() != right.durations->size()) return left.durations->size() > right.durations->size();
    if (left.performance.has_value() != right.performance.has_value())
      throw std::invalid_argument("primary performance aggregation has no value");
    if (left.performance && *left.performance != *right.performance) return *left.performance > *right.performance;
    return *left.activities > *right.activities;
  });
  std::string text = options.response_header ? "If I have a process with the following process variants:\n\n" : "\n\n";
  for (const auto& entry : order) {
    if (code_points(text) > options.max_len) break;
    std::string joined;
    for (const auto& activity : *entry.activities) joined += (joined.empty() ? "" : " -> ") + activity;
    text += " " + joined + " ( frequency = " +
            frequency_text(entry.durations->size(), nonempty_cases(log), options.relative_frequency);
    if (options.include_performance) text += performance_text(*entry.durations, options, false);
    text += " )\n";
  }
  return text + "\n\n";
}

std::string llm_log_features_description(const EventLog& log, const LlmAbstractionOptions& options) {
  // A CSV upload reaches the reference as format_dataframe's frame, whose
  // case, activity and timestamp copies carry the standard keys.
  LogToFeaturesOptions feature_options;
  feature_options.standard_keys = log.dataframe_columns && log.dataframe_columns->csv_text;
  const auto features = trace_based_log_features(log, feature_options);
  std::vector<std::string> descriptions;
  std::vector<std::vector<double>> columns(features.feature_names.size());
  for (const auto& name : features.feature_names) descriptions.push_back(log_feature_description(name));
  for (const auto& row : features.trace_rows)
    for (std::size_t column = 0; column < columns.size(); ++column) {
      const auto* value = std::get_if<double>(&row.at(column));
      columns[column].push_back(value ? *value : 0.0);
    }
  return describe_features(descriptions, columns,
                           options.include_header ? "\n Given the following features:\n\n" : "\n", options.max_len);
}

std::string llm_event_stream_description(const EventLog& log, const LlmAbstractionOptions& options) {
  if (log.dataframe_columns) return frame_event_stream_description(log, options);
  std::vector<StreamItem> items;
  for (const auto& trace : log.traces)
    for (const auto& event : trace.events) {
      std::string line = "{";
      for (const auto& [key, cell] : event_row(trace, event)) {
        if (line.size() > 1) line += ", ";
        line += py_repr(key) + ": " + cell_repr(cell);
      }
      items.push_back({event_micros(event), line + "}\n"});
    }
  return stream_text(std::move(items), options);
}

std::string llm_log_skeleton_description(const EventLog& log, const LlmAbstractionOptions& options) {
  const auto model = discover_log_skeleton(log);
  std::string text = options.include_header
                         ? "\n\nThe Log Skeleton process model contains the following declarative constraints:\n- "
                           "Equivalence (if the first activity occurs, then it has the same occurrences as the "
                           "second one)\n- Always Before (if the first activity occur, then the second activity "
                           "should have been executed previously)\n- Always After (if the first activity occur, then "
                           "the second activity is executed in one of the following events)\n- Never Together (the "
                           "two activities cannot co-exist inside the same case)\n- Activity Occurrences (bounds the "
                           "number of occurrences for an activity in a case)\n- Directly-Follows Constraints (if the "
                           "first activity occurs, then the second activity shall occur immediately after)\n    "
                           "\n\nI have a Log Skeleton process model containing the following declarative "
                           "constraints:\n\n"
                         : "\n";
  const auto pairs = [&](const char* label, const std::vector<LogSkeletonPair>& entries) {
    text += std::string(label) + ": ";
    for (const auto& entry : entries) text += " (" + py_repr(entry.source) + ", " + py_repr(entry.target) + ")";
    text += "\n\n";
  };
  pairs("Equivalence", model.equivalence);
  pairs("Always Before", model.always_before);
  pairs("Always After", model.always_after);
  pairs("Never Together", model.never_together);
  std::vector<std::string> activities;
  for (const auto& trace : log.traces)
    for (const auto& event : trace.events)
      if (std::find(activities.begin(), activities.end(), event.activity) == activities.end())
        activities.push_back(event.activity);
  text += "Activity Occurrences: ";
  for (const auto& activity : activities) {
    const auto found = model.activ_freq.find(activity);
    if (found == model.activ_freq.end()) continue;
    text += " " + activity + ":";
    for (std::size_t i = 0; i < found->second.size(); ++i) text += (i ? ", " : " ") + std::to_string(found->second[i]);
    text += ";";
  }
  text += "\n\n";
  pairs("Directly-Follows Constraints", model.directly_follows);
  return text;
}

std::string llm_ocel_ocdfg_description(const Ocel& log, const LlmAbstractionOptions& options) {
  if (!options.include_header) return "\n\n";
  const auto graph = discover_ocdfg_classic(log, options.compute_edges_performance);
  struct Edge {
    std::size_t events, objects;
    double duration;
    const std::string* type;
    const std::pair<std::string, std::string>* arc;
  };
  std::vector<Edge> edges;
  for (const auto& [type, arcs] : graph.edges)
    for (const auto& [arc, metric] : arcs) {
      if (!options.compute_edges_performance)
        throw std::invalid_argument("the OCDFG description needs edge performance");
      const auto& samples = metric.performance_event_couples.empty() ? metric.performance
                                                                     : metric.performance_event_couples;
      edges.push_back({metric.event_couples.size(), metric.unique_objects.size(),
                       samples.empty() ? NAN : numpy_mean(samples), &type, &arc});
    }
  std::sort(edges.begin(), edges.end(), [](const Edge& left, const Edge& right) {
    if (left.events != right.events) return left.events > right.events;
    if (left.duration != right.duration) return left.duration > right.duration;
    if (*left.type != *right.type) return *left.type > *right.type;
    return *left.arc > *right.arc;
  });
  std::vector<std::pair<std::string, std::string>> kept;  // object type, lines
  std::size_t measure = 0;
  for (const auto& edge : edges) {
    if (measure > options.max_len) break;
    std::string line = "  \"" + edge.arc->first + "\" -> \"" + edge.arc->second +
                       "\" (frequency (number of events) = " + std::to_string(edge.events) +
                       ", frequency (number of objects) = " + std::to_string(edge.objects);
    if (options.include_performance) {
      std::array<char, 400> buffer{};
      const auto result =
          std::to_chars(buffer.data(), buffer.data() + buffer.size(), edge.duration, std::chars_format::fixed, 2);
      line += ", duration = " + (std::isnan(edge.duration) ? std::string("nan") : std::string(buffer.data(), result.ptr));
    }
    line += ")\n";
    measure += code_points(line);
    auto group = std::find_if(kept.begin(), kept.end(), [&](const auto& entry) { return entry.first == *edge.type; });
    if (group == kept.end()) {
      kept.emplace_back(*edge.type, "");
      group = std::prev(kept.end());
    }
    group->second += line;
  }
  std::string text =
      "\nIf I have an object-centric event log with the following directly follows graph (split between the "
      "different object types):\n\n";
  for (const auto& [type, lines] : kept) text += "Object type: " + type + "\n" + lines + "\n";
  return text;
}

std::string llm_ocel_features_description(const Ocel& log, const std::string& object_type,
                                          const LlmAbstractionOptions& options) {
  OcelFeatureOptions feature_options;
  feature_options.feature_name_style = OcelFeatureNameStyle::kCompatibility;
  feature_options.filter_per_type = object_type;
  feature_options.include_all_object_features = true;
  // The reference graph families look up every event's objects and raise
  // KeyError for an event without any.
  std::set<std::string> related;
  for (const auto& relation : log.relations) related.insert(relation.event_id);
  for (const auto& event : log.events)
    if (!related.contains(event.id)) throw std::out_of_range("event without objects: " + event.id);
  const auto table = extract_ocel_features(log, OcelFeatureVariant::kObjects, feature_options);
  // The reference default has no object-type indicators or attribute counts.
  std::vector<std::string> descriptions;
  std::vector<std::vector<double>> columns;
  for (std::size_t column = 0; column < table.feature_names.size(); ++column) {
    const auto& name = table.feature_names[column];
    if (name.starts_with("@@object_type_") || name == "@@object_num_attributes") continue;
    descriptions.push_back(object_feature_description(name));
    auto& values = columns.emplace_back();
    for (const auto& row : table.values) values.push_back(row.at(column));
  }
  const std::string header =
      "\n Beforehand, a bit of notions. Given an object-centric event log, the object interaction graph connects "
      "objects that are related in at least an event. The object descendants graph connects objects related in at "
      "least an event, when the lifecycle of the second object starts after the lifecycle of the first. The object "
      "inheritance graph connects objects when there an event that ends the lifecycle of the first object and "
      "starts the lifecycle of the second one. \n\n Given the following features:\n\n";
  return describe_features(descriptions, columns, options.include_header ? header : "\n", options.max_len);
}

}  // namespace minera
