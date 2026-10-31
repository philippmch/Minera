// Replays the recheck of 7270a1f (2026-10-28): fuzz cases captured from
// public PM4Py 2.7.22 calls with scripts/capture-recheck-7270a1f-reference.py,
// in tables built by scripts/generate-recheck-7270a1f-fixture.py
// (fixtures/recheck_7270a1f):
//
//   filter-cases.tsv.gz  consecutive activity/case grouping whose key columns
//                        hold NaN, None, pd.NA, NaT, infinities and integers
//                        too large for a float, in object and float64 columns
//                        and next to typed int64/uint64/float64/bool columns
//                        (O1, O2); then the pandas sequence and case filters
//                        over case and activity columns of numpy and masked
//                        dtypes and mixed object activities, with fixed cases
//                        for float16 grouping, int64 ids that wrap as a range
//                        and masked ids one double holds (the same rules
//                        beyond the grouping filter)
//   pnml-cases.tsv.gz    PNML transitions with toolspecific elements of many
//                        tools, tags and namespaces and stochastic properties
//                        of every key, distribution type and parameter text,
//                        imported with and without the stochastic map (O3, O4),
//                        and the reviewer's models whose saved captures their
//                        harness made ambiguous, captured again
//   reviewer-pnml.tsv.gz the reviewer's PNML captures with their saved public
//                        results, read as their CLI comparison reads an
//                        import: transition labels in order and [weight,
//                        priority] per map record (O3, O4); the reviewer's
//                        100 grouping tables are in filter-cases.tsv.gz
//
// The harness for filter cases is the one of test_recheck_d5b33ef.cpp.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/filtering_common.hpp"
#include "minera/pandas_case_filter.hpp"
#include "minera/pandas_sequence_filters.hpp"
#include "minera/pnml.hpp"
#include "minera/stochastic_map.hpp"

#include "../full_double.hpp"
#include "../gzip_codec.hpp"

using namespace minera;

namespace {
constexpr std::size_t FILTER_CASES = 13000;
constexpr std::size_t PNML_CASES = 3000;
constexpr std::size_t REVIEWER_PNML_CASES = 75;
int failures = 0;
std::filesystem::path root;
void check(bool ok, const std::string& name) {
  if (!ok) { std::cerr << "FAIL " << name << "\n"; ++failures; }
}
void same(const std::string& actual, const std::string& expected, const std::string& name) {
  if (actual != expected) {
    std::cerr << "FAIL " << name << "\n  expected " << expected << "\n  actual   " << actual << "\n";
    ++failures;
  }
}

std::vector<std::string> gz_lines(const std::string& name) {
  std::ifstream input(root / name, std::ios::binary);
  if (!input) throw std::runtime_error("cannot read " + name);
  const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  std::istringstream text(detail::gzip_decompress(bytes, std::size_t{1} << 28));
  std::vector<std::string> lines;
  for (std::string line; std::getline(text, line);)
    if (!line.empty()) lines.push_back(line);
  return lines;
}
std::vector<std::string> split_tabs(const std::string& line) {
  std::vector<std::string> out;
  std::size_t begin = 0;
  for (std::size_t tab; (tab = line.find('\t', begin)) != std::string::npos; begin = tab + 1)
    out.push_back(line.substr(begin, tab - begin));
  out.push_back(line.substr(begin));
  return out;
}

// ------------------------------------------------ Python's json.dumps text

std::string json_string(std::string_view text) {
  std::string out = "\"";
  for (const unsigned char c : text) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      default:
        if (c < 0x20) {
          char buffer[8];
          std::snprintf(buffer, sizeof buffer, "\\u%04x", c);
          out += buffer;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  return out + "\"";
}
std::string json_float(double value) {
  if (std::isinf(value)) return value > 0 ? "Infinity" : "-Infinity";
  return filtering_float_repr(value);
}
std::string json_list(const std::vector<std::string>& items) {
  std::string out = "[";
  for (std::size_t i = 0; i < items.size(); ++i) out += (i ? "," : "") + items[i];
  return out + "]";
}
std::string sorted_list(std::vector<std::string> items) {
  std::sort(items.begin(), items.end());
  return json_list(items);
}

// ------------------------------------------------ filter cases (O1, O2)

// The capture script's tokens: S text, I int, U unsigned int, F float, B bool,
// N None, A NA, D timestamp, J/G an int/float kept only as its text (the
// lexical value a log reader keeps), L/T list/tuple.
EventAttribute value(const std::string& token, const std::string& key = "") {
  EventAttribute result{key, ""};
  if (token == "N") { result.is_null = true; return result; }
  if (token == "A") return make_native_na_attribute(key);
  const auto type = token.empty() ? 'S' : token[0];
  const auto body = token.empty() ? std::string{} : token.substr(1);
  result.value = body;
  if (type == 'J') {
    result.kind = AttributeKind::kInt;
  } else if (type == 'G') {
    result.kind = AttributeKind::kFloat;
  } else if (type == 'U') {
    result.kind = AttributeKind::kInt;
    result.typed_value = static_cast<std::uint64_t>(std::stoull(body));
  } else if (type == 'I') {
    result.kind = AttributeKind::kInt;
    result.typed_value = static_cast<std::int64_t>(std::stoll(body));
  } else if (type == 'F') {
    result.kind = AttributeKind::kFloat;
    result.typed_value = detail::stod(body);
  } else if (type == 'B') {
    result.kind = AttributeKind::kBoolean;
    result.typed_value = body == "1";
    result.value = body == "1" ? "true" : "false";
  } else if (type == 'D') {
    result.kind = AttributeKind::kDate;
    std::optional<std::string> zone;
    if (body.ends_with("Z")) zone = "UTC";
    else if (body.size() >= 6 && (body[body.size() - 6] == '+' || body[body.size() - 6] == '-')) zone = "UTC" + body.substr(body.size() - 6);
    const auto dot = body.find('.');
    std::size_t digits = 0;
    if (dot != std::string::npos)
      for (std::size_t j = dot + 1; j < body.size() && std::isdigit(static_cast<unsigned char>(body[j])); ++j) ++digits;
    result.typed_value = NativeDate{
        body, digits > 6 ? NativeDateResolution::kNanosecond : NativeDateResolution::kMicrosecond, zone};
    if (body == "NaT") result.is_null = true;
  } else if (type == 'L' || type == 'T') {
    result.value.clear();
    result.container = type == 'L' ? AttributeContainerKind::kList : AttributeContainerKind::kTuple;
    std::istringstream items(body);
    for (std::string item; std::getline(items, item, ',');) result.children.push_back(value(item));
  }
  return result;
}
PandasFilterValue filter_value(const std::string& token) {
  if (token == "N") return PandasFilterValue::none();
  if (token.starts_with('L') || token.starts_with('T')) {
    std::vector<PandasFilterValue> items;
    std::istringstream in(token.substr(1));
    for (std::string item; std::getline(in, item, ',');) items.push_back(filter_value(item));
    return token[0] == 'L' ? PandasFilterValue::list(items) : PandasFilterValue::tuple(items);
  }
  if (token.starts_with('I')) return PandasFilterValue::of(static_cast<std::int64_t>(std::stoll(token.substr(1))));
  if (token.starts_with('F')) return PandasFilterValue::of(detail::stod(token.substr(1)));
  if (token.starts_with('B')) return PandasFilterValue::of(token.substr(1) == "1");
  return PandasFilterValue::of(token.substr(1));
}

// pandas Timestamp.isoformat() of a native datetime text.
std::string isoformat(std::string text) {
  if (text.size() > 10 && text[10] == ' ') text[10] = 'T';
  std::string zone;
  if (text.ends_with("Z")) {
    text.pop_back();
    zone = "+00:00";
  } else if (text.size() > 19 && (text[text.size() - 6] == '+' || text[text.size() - 6] == '-')) {
    zone = text.substr(text.size() - 6);
    text.resize(text.size() - 6);
  }
  if (const auto dot = text.find('.'); dot != std::string::npos) {
    auto digits = text.substr(dot + 1);
    text.resize(dot);
    digits.resize(9, '0');
    if (digits.substr(6) != "000") text += "." + digits;
    else if (digits != "000000000") text += "." + digits.substr(0, 6);
  }
  return text + zone;
}
std::string integer_text(std::string text) {
  bool negative = false;
  if (!text.empty() && (text[0] == '-' || text[0] == '+')) {
    negative = text[0] == '-';
    text.erase(0, 1);
  }
  text.erase(0, std::min(text.find_first_not_of('0'), text.size() - 1));
  return (negative && text != "0" ? "-" : "") + text;
}
// The reviewer's snapshot of one cell (compare.py's cell()).
std::string cell_json(const std::optional<EventAttribute>& cell) {
  if (!cell) return R"({"null":"None"})";
  if (cell->is_null) return cell->kind == AttributeKind::kDate ? R"({"null":"NaT"})" : R"({"null":"None"})";
  if (is_native_na_attribute(*cell)) return R"({"null":"NA"})";
  const auto& typed = cell->typed_value;
  switch (cell->kind) {
    case AttributeKind::kDate: {
      const auto* date = std::get_if<NativeDate>(&typed);
      return "{\"timestamp\":" + json_string(isoformat(date ? date->value : cell->value)) + "}";
    }
    case AttributeKind::kInt:
      if (const auto* v = std::get_if<std::int64_t>(&typed)) return std::to_string(*v);
      if (const auto* v = std::get_if<std::uint64_t>(&typed)) return std::to_string(*v);
      return integer_text(cell->value);
    case AttributeKind::kFloat: {
      const auto* v = std::get_if<double>(&typed);
      const double number = v ? *v : detail::stod(cell->value);
      return std::isnan(number) ? R"({"null":"NaN"})" : json_float(number);
    }
    case AttributeKind::kBoolean: {
      const auto* v = std::get_if<bool>(&typed);
      return (v ? *v : cell->value == "true") ? "true" : "false";
    }
    default:
      if (const auto* v = std::get_if<std::int64_t>(&typed)) return std::to_string(*v);
      if (const auto* v = std::get_if<double>(&typed)) return json_float(*v);
      if (const auto* v = std::get_if<bool>(&typed)) return *v ? "true" : "false";
      return json_string(cell->value);
  }
}
std::string snapshot(const EventTable& table) {
  std::vector<std::string> columns, row_ids, index, rows, dtypes;
  std::optional<std::size_t> row_id;
  for (std::size_t c = 0; c < table.columns.size(); ++c) {
    columns.push_back(json_string(table.columns[c]));
    if (table.columns[c] == "row_id") row_id = c;
  }
  for (const auto& row : table.rows) {
    std::vector<std::string> cells;
    for (const auto& cell : row) cells.push_back(cell_json(cell));
    rows.push_back(json_list(cells));
    if (row_id) row_ids.push_back(cell_json(row[*row_id]));
  }
  std::string name = "null";
  if (table.index) {
    for (const auto& label : table.index->labels) {
      const auto* attribute = std::get_if<EventAttribute>(&label.value);
      index.push_back(attribute ? cell_json(*attribute) : "\"<tuple>\"");
    }
    if (!table.index->names.empty() && table.index->names[0]) name = json_string(*table.index->names[0]);
  } else {
    for (std::size_t i = 0; i < table.rows.size(); ++i) index.push_back(std::to_string(i));
  }
  for (const auto& dtype : table.dtypes) dtypes.push_back(json_string(dtype));
  return "{\"columns\":" + json_list(columns) + ",\"row_ids\":" + json_list(row_ids) + ",\"index\":" + json_list(index) +
         ",\"index_name\":" + name + ",\"rows\":" + json_list(rows) + ",\"dtypes\":" + json_list(dtypes) + "}";
}

// A cell as a column of `dtype` holds the token's value: float32 and float16
// columns round it, and a masked (nullable) column reads NaN as pd.NA.
std::uint16_t float_to_half(float value) {
  std::uint32_t f;
  std::memcpy(&f, &value, sizeof f);
  const std::uint16_t sign = static_cast<std::uint16_t>((f >> 16) & 0x8000u);
  const std::uint32_t magnitude = f & 0x7fffffffu;
  if (magnitude >= 0x7f800000u)
    return sign | (magnitude > 0x7f800000u ? 0x7e00u : 0x7c00u);
  if (magnitude >= 0x477ff000u) return sign | 0x7c00u;  // rounds past 65504
  if (magnitude < 0x38800000u) {  // a subnormal half or zero
    if (magnitude < 0x33000000u) return sign;
    const std::uint32_t exponent = magnitude >> 23;
    const std::uint32_t mantissa = (magnitude & 0x7fffffu) | 0x800000u;
    const std::uint32_t shift = 126 - exponent;  // 14 + 23 - 10 - (exponent - 127) + 1
    std::uint32_t half = mantissa >> shift;
    const std::uint32_t rest = mantissa & ((1u << shift) - 1);
    const std::uint32_t midpoint = 1u << (shift - 1);
    if (rest > midpoint || (rest == midpoint && (half & 1u))) ++half;
    return sign | static_cast<std::uint16_t>(half);
  }
  std::uint32_t half = ((magnitude >> 13) - (112u << 10));
  const std::uint32_t rest = magnitude & 0x1fffu;
  if (rest > 0x1000u || (rest == 0x1000u && (half & 1u))) ++half;
  return sign | static_cast<std::uint16_t>(half);
}
float half_to_float(std::uint16_t half) {
  const std::uint32_t sign = static_cast<std::uint32_t>(half & 0x8000u) << 16;
  const std::uint32_t exponent = (half >> 10) & 0x1fu;
  std::uint32_t mantissa = half & 0x3ffu;
  std::uint32_t f;
  if (exponent == 0x1fu) f = sign | 0x7f800000u | (mantissa << 13);
  else if (exponent) f = sign | ((exponent + 112u) << 23) | (mantissa << 13);
  else if (!mantissa) f = sign;
  else {
    int shift = 0;
    while (!(mantissa & 0x400u)) { mantissa <<= 1; ++shift; }
    f = sign | ((113u - shift) << 23) | ((mantissa & 0x3ffu) << 13);
  }
  float value;
  std::memcpy(&value, &f, sizeof value);
  return value;
}
void as_dtype(EventAttribute& cell, const std::string& dtype) {
  auto* real = std::get_if<double>(&cell.typed_value);
  if (!real) return;
  const bool masked = !dtype.empty() && std::isupper(static_cast<unsigned char>(dtype[0]));
  if (masked && std::isnan(*real)) {
    cell = make_native_na_attribute(cell.key);
    return;
  }
  if (dtype == "float32" || dtype == "Float32") *real = static_cast<float>(*real);
  else if (dtype == "float16") *real = half_to_float(float_to_half(static_cast<float>(*real)));
  else return;
  cell.value = filtering_float_repr(*real);
}

void filter_cases() {
  std::size_t count = 0;
  for (const auto& line : gz_lines("filter-cases.tsv.gz")) {
    const auto fields = split_tabs(line);
    std::istringstream input(fields.at(0));
    std::string id, op, lo, hi, ck, ak, tk, case_dtype, activity_dtype;
    std::size_t rows = 0;
    input >> id >> op >> std::quoted(lo) >> std::quoted(hi) >> std::quoted(ck) >> std::quoted(ak) >> std::quoted(tk) >>
        case_dtype >> activity_dtype >> rows;
    EventTable table;
    table.columns = {ck, ak, tk, "payload", "row_id"};
    table.dtypes = {case_dtype, activity_dtype, op == "performance_us" ? "datetime64[us, UTC]" : "datetime64[ns, UTC]",
                    "object", "int64"};
    table.index = EventTableIndex{"Index", "object", {std::string{"custom_idx"}}, {}};
    for (std::size_t i = 0; i < rows; ++i) {
      std::string cid, act, time, payload, label;
      input >> std::quoted(cid) >> std::quoted(act) >> std::quoted(time) >> std::quoted(payload) >> std::quoted(label);
      table.rows.push_back({value(cid, ck), value(act, ak), value(time, tk), value(payload, "payload"),
                            value("I" + std::to_string(i), "row_id")});
      as_dtype(*table.rows.back()[0], case_dtype);
      as_dtype(*table.rows.back()[1], activity_dtype);
      if (auto* date = std::get_if<NativeDate>(&table.rows.back()[2]->typed_value); date && op == "performance_us")
        date->resolution = NativeDateResolution::kMicrosecond;
      table.index->labels.push_back({value(label)});
    }
    std::string status = "OK", actual;
    try {
      EventTable result;
      PandasCaseFilterOptions o;
      o.case_id_key = value("S" + ck);
      o.timestamp_key = value("S" + tk);
      if (op == "size") result = filter_pandas_case_size(table, value(lo), value(hi), o);
      else if (op == "ncases") result = filter_pandas_ncases(table, value(hi), o);
      else if (op.starts_with("performance")) result = filter_pandas_case_performance(table, value(lo), value(hi), o);
      else {
        PandasFilterParameters p = std::vector<PandasFilterParameter>{
            {"pm4py:param:case_id_key", false, PandasFilterValue::of(ck)},
            {"pm4py:param:activity_key", false, PandasFilterValue::of(ak)},
            {"pm4py:param:timestamp_key", false, PandasFilterValue::of(tk)}};
        if (op.ends_with("_last")) p->push_back({"first_or_last", false, PandasFilterValue::of("last")});
        if (op == "group_act_last" || op == "group_time_last")
          p->push_back({"filter_type", false, PandasFilterValue::of("last")});
        if (hi != "N") p->push_back({"strict", false, filter_value(hi)});
        if (op == "start") result = pandas_start_activities_apply(table, PandasFilterValue::list({filter_value(lo)}), p);
        else if (op == "end") result = pandas_end_activities_apply(table, PandasFilterValue::list({filter_value(lo)}), p);
        else if (op == "trace") result = pandas_trace_filter_apply(table, PandasFilterValue::list({filter_value(lo)}), p);
        else if (op == "starts") result = pandas_starts_with_filter_apply(table, PandasFilterValue::list({filter_value(lo)}), p);
        else if (op == "ends") result = pandas_ends_with_filter_apply(table, PandasFilterValue::list({filter_value(lo)}), p);
        else if (op == "start_nocc") result = pandas_filter_df_on_start_activities_nocc(table, filter_value(lo), std::nullopt, ck, ak);
        else if (op == "end_nocc") result = pandas_filter_df_on_end_activities_nocc(table, filter_value(lo), std::nullopt, ck, ak).table;
        else if (op == "rework") result = pandas_rework_filter_apply(table, filter_value(lo), p);
        else if (op == "split") result = pandas_activity_split_apply(table, filter_value(lo), p);
        else if (op.starts_with("prefix")) result = pandas_prefix_filter_apply(table, filter_value(lo), p);
        else if (op.starts_with("suffix")) result = pandas_suffix_filter_apply(table, filter_value(lo), p);
        else if (op.starts_with("group_act")) result = pandas_consecutive_act_case_grouping_apply(table, p);
        else if (op.starts_with("group_time")) result = pandas_timestamp_case_grouping_apply(table, p);
        else throw std::invalid_argument("unknown op " + op);
      }
      actual = snapshot(result);
    } catch (const PandasCaseFilterError& e) {
      status = e.type;
      actual = e.message;
    } catch (const PandasSequenceFilterError& e) {
      status = e.type;
      actual = e.message;
    } catch (const std::exception& e) {
      status = "ERROR";
      actual = e.what();
    }
    same(status + " " + actual, fields.at(1) + " " + fields.at(2), "filter " + id);
    ++count;
  }
  check(count >= FILTER_CASES, "filter case table read " + std::to_string(count));
}

// ------------------------------------------------ PNML toolspecific metadata (O3, O4)

std::string unhex(const std::string& text) {
  std::string out;
  for (std::size_t i = 0; i + 1 < text.size(); i += 2) out += static_cast<char>(std::stoi(text.substr(i, 2), nullptr, 16));
  return out;
}
// Native policy where PM4Py keeps text, non-finite values, an undefined
// exponential rate, any float weight or any int priority.
bool policy_error(const std::string& message) {
  return message.starts_with("PNML stochastic parameters must be finite numbers") ||
         message.find("must be finite and non-negative") != std::string::npos ||
         message.find("outside the native integer range") != std::string::npos;
}
std::string pnml_import(const PnmlDocument& document, bool stochastic) {
  std::vector<std::pair<std::string, std::size_t>> order;
  for (std::size_t t = 0; t < document.transition_ids.size(); ++t) order.emplace_back(document.transition_ids[t], t);
  std::sort(order.begin(), order.end());
  std::vector<std::string> labels;
  for (const auto& [id, t] : order) {
    const auto& activity = document.net.transitions()[t].activity;
    labels.push_back("[" + json_string(id) + "," + (activity ? json_string(document.net.activities()[*activity]) : "null") + "]");
  }
  std::string text = "OK " + json_list(labels);
  if (!stochastic) return text;
  if (!document.stochastic_map) return text + " null";
  std::vector<std::string> records;
  for (const auto& [id, t] : order) {
    const auto& record = document.stochastic_map->transitions.at(t);
    check(record.transition == t, "PNML record index " + id);
    records.push_back("[" + json_string(id) + "," + json_string(stochastic_map_distribution_name(record.distribution)) +
                      "," + json_string(stochastic_map_timing_name(record.timing)) + "," +
                      filtering_float_repr(record.parameter) + "," + filtering_float_repr(record.secondary_parameter) +
                      "," + filtering_float_repr(record.tertiary_parameter) + "," +
                      filtering_float_repr(record.activation_weight) + "," + std::to_string(record.priority) + "]");
  }
  return text + " " + json_list(records);
}

void pnml_cases() {
  std::size_t count = 0;
  for (const auto& line : gz_lines("pnml-cases.tsv.gz")) {
    const auto fields = split_tabs(line);
    const auto xml = unhex(fields.at(1));
    for (const bool stochastic : {false, true}) {
      const auto& expected = fields.at(stochastic ? 3 : 2);
      const auto name = "pnml " + fields.at(0) + (stochastic ? " with the map" : "");
      PnmlImportOptions options;
      options.import_stochastic_map = stochastic;
      try {
        same(pnml_import(parse_pnml_document(xml, options), stochastic), expected, name);
      } catch (const std::exception& error) {
        const std::string message = error.what();
        if (expected == "POLICY") check(policy_error(message), name + " policy: " + message);
        else same("ERR " + message, expected, name);
      }
    }
    ++count;
  }
  check(count >= PNML_CASES, "PNML case table read " + std::to_string(count));
}

// An import as the reviewer's comparison reads it: the labels in transition
// order and [weight, priority] per map record, [] when a requested map was
// not built and null without the map.
std::string reviewer_import(const PnmlDocument& document, bool stochastic) {
  std::vector<std::string> labels;
  for (const auto& transition : document.net.transitions())
    labels.push_back(transition.activity ? json_string(document.net.activities()[*transition.activity]) : "null");
  std::string properties = "null";
  if (stochastic) {
    std::vector<std::string> records;
    if (document.stochastic_map)
      for (const auto& record : document.stochastic_map->transitions)
        records.push_back("[" + filtering_float_repr(record.activation_weight) + "," + std::to_string(record.priority) + "]");
    properties = json_list(records);
  }
  return "OK " + json_list(labels) + " " + properties;
}

// Declared: priorities outside the native int range are a native policy
// error, and digits Unicode added after version 14 (the native tables) do
// not convert natively.
bool declared(const std::string& qualification, const std::string& message) {
  if (qualification == "priority-range") return policy_error(message);
  if (qualification == "unicode14")
    return message.starts_with("could not convert string to float: ") ||
           message.starts_with("invalid literal for int() with base 10: ");
  return false;
}

void reviewer_pnml_cases() {
  std::size_t count = 0;
  for (const auto& line : gz_lines("reviewer-pnml.tsv.gz")) {
    const auto fields = split_tabs(line);
    const auto name = "reviewer pnml " + fields.at(0);
    const bool stochastic = fields.at(2) == "1";
    const auto& expected = fields.at(3);
    PnmlImportOptions options;
    options.import_stochastic_map = stochastic;
    try {
      const auto actual = reviewer_import(parse_pnml_document(unhex(fields.at(1)), options), stochastic);
      same(actual, expected, name);
    } catch (const std::exception& error) {
      const std::string message = error.what();
      if (expected.starts_with("DECLARED ")) check(declared(expected.substr(9), message), name + ": " + message);
      else same("ERR " + message, expected, name);
    }
    ++count;
  }
  check(count >= REVIEWER_PNML_CASES, "reviewer PNML case table read " + std::to_string(count));
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " <fixture-directory>\n";
    return 2;
  }
  root = argv[1];
  filter_cases();
  pnml_cases();
  reviewer_pnml_cases();
  if (failures) {
    std::cerr << failures << " recheck 7270a1f cases failed\n";
    return 1;
  }
  std::cout << "recheck 7270a1f cases passed\n";
}
