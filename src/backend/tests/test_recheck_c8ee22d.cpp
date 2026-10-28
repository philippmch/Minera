// Replays the reviewer's recheck of c8ee22d (2026-10-28): the saved public
// PM4Py 2.7.22 outputs for the ten fix groups M1-M10 and the smaller output
// items, plus fuzz cases captured from public calls with
// scripts/capture-recheck-c8ee22d-reference.py. Case tables come from
// scripts/generate-recheck-c8ee22d-fixture.py (fixtures/recheck_c8ee22d):
// filter-cases.tsv.gz (M2, M7, M8: pandas sequence and case filters),
// org-cases.tsv.gz (M3-M6, M9: network analysis and batches). Calendar (M1,
// M9, year labels), parser (M9, M10) and Woflan cases use the reviewer's XES,
// CSV and PNML fixtures with the saved expectations written out below.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "minera/attribute_distribution.hpp"
#include "minera/batches.hpp"
#include "minera/event_log.hpp"
#include "minera/filtering_common.hpp"
#include "minera/network_analysis.hpp"
#include "minera/pandas_case_filter.hpp"
#include "minera/pandas_sequence_filters.hpp"
#include "minera/petri_analysis.hpp"
#include "minera/pnml.hpp"
#include "minera/request_params.hpp"
#include "minera/streaming_temporal.hpp"
#include "minera/xes_event_log_importer.hpp"

#include "../full_double.hpp"
#include "../gzip_codec.hpp"
#include "../numpy_object_sort.hpp"

using namespace minera;

namespace {
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

// ------------------------------------------------ filter cases (M2, M7, M8)

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

void filter_cases() {
  std::size_t count = 0;
  for (const auto& line : gz_lines("filter-cases.tsv.gz")) {
    const auto fields = split_tabs(line);
    std::istringstream input(fields.at(0));
    std::string id, op, lo, hi, ck, ak, tk;
    std::size_t rows = 0;
    input >> id >> op >> std::quoted(lo) >> std::quoted(hi) >> std::quoted(ck) >> std::quoted(ak) >> std::quoted(tk) >> rows;
    EventTable table;
    table.columns = {ck, ak, tk, "payload", "row_id"};
    table.dtypes = {"object", "object", op == "performance_us" ? "datetime64[us, UTC]" : "datetime64[ns, UTC]", "object", "int64"};
    table.index = EventTableIndex{"Index", "object", {std::string{"custom_idx"}}, {}};
    for (std::size_t i = 0; i < rows; ++i) {
      std::string cid, act, time, payload, label;
      input >> std::quoted(cid) >> std::quoted(act) >> std::quoted(time) >> std::quoted(payload) >> std::quoted(label);
      table.rows.push_back({value(cid, ck), value(act, ak), value(time, tk), value(payload, "payload"),
                            value("I" + std::to_string(i), "row_id")});
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
        if (hi != "N") p->push_back({"strict", false, filter_value(hi)});
        if (op == "start") result = pandas_start_activities_apply(table, PandasFilterValue::list({filter_value(lo)}), p);
        else if (op == "end") result = pandas_end_activities_apply(table, PandasFilterValue::list({filter_value(lo)}), p);
        else if (op == "rework") result = pandas_rework_filter_apply(table, filter_value(lo), p);
        else if (op == "split") result = pandas_activity_split_apply(table, filter_value(lo), p);
        else if (op == "prefix") result = pandas_prefix_filter_apply(table, filter_value(lo), p);
        else if (op == "suffix") result = pandas_suffix_filter_apply(table, filter_value(lo), p);
        else if (op == "group_act") result = pandas_consecutive_act_case_grouping_apply(table, p);
        else if (op == "group_time") result = pandas_timestamp_case_grouping_apply(table, p);
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
  check(count > 900, "filter case table read " + std::to_string(count));
}

// ------------------------------------------------ organizational cases (M3-M6, M9)

EventAttribute org_cell(const std::string& key, const std::string& token) {
  const char type = token.empty() ? 'S' : token[0];
  const std::string body = token.empty() ? "" : token.substr(1);
  switch (type) {
    case 'X': return EventAttribute{key, "", AttributeKind::kString, true};
    case 'A': return EventAttribute{key, "", AttributeKind::kString, false, NativeNA{}};
    case 'I': return EventAttribute{key, body, AttributeKind::kInt};
    case 'F': return EventAttribute{key, body, AttributeKind::kFloat, false, detail::stod(body)};
    case 'B': return EventAttribute{key, body == "1" ? "true" : "false", AttributeKind::kBoolean, false, body == "1"};
    case 'D': return EventAttribute{key, body, AttributeKind::kDate, false, NativeDate{body}};
    case 'T': return EventAttribute{key, body, AttributeKind::kDate};
    default: return EventAttribute{key, body, AttributeKind::kString};
  }
}
std::string lexical(const std::string& token) {
  if (token.empty() || token[0] == 'X' || token[0] == 'A') return "";
  if (token[0] == 'B') return token.substr(1) == "1" ? "true" : "false";
  return token.substr(1);
}
std::string canonical(const BatchDiscovery& result) {
  std::vector<std::string> groups;
  for (const auto& group : result.groups) {
    std::vector<std::string> batches;
    for (const auto& batch : group.batches) {
      std::vector<std::string> events;
      for (const auto& event : batch.events)
        events.push_back(json_list({json_float(event.start), json_float(event.complete), json_string(event.case_id),
                                    json_string(event.event_id)}));
      batches.push_back(json_list({std::to_string(static_cast<int>(batch.type)), json_float(batch.start),
                                   json_float(batch.complete), sorted_list(events)}));
    }
    groups.push_back(json_list({json_string(group.activity), group.resource_is_null ? "null" : json_string(group.resource),
                                std::to_string(group.batch_count), sorted_list(batches)}));
  }
  return json_list(groups);
}
std::string canonical(const NetworkAnalysisResult& result) {
  std::vector<std::string> edges;
  for (const auto& edge : result.edges) {
    std::vector<std::string> performance;
    for (const auto value : edge.performance) performance.push_back(json_float(value));
    edges.push_back(json_list({json_string(edge.source), json_string(edge.target), json_string(edge.activity),
                               std::to_string(edge.count), json_list(performance)}));
  }
  return sorted_list(edges);
}
// Both raise; the reference names the operands of the first failed '<' in
// an order that can follow Python's hash order, so either order passes.
bool same_error(const std::string& actual, const std::string& expected) {
  if (actual.ends_with(expected)) return true;
  static const std::regex pattern("'<' not supported between instances of '(\\w+)' and '(\\w+)'");
  std::smatch a, b;
  if (!std::regex_search(actual, a, pattern) || !std::regex_search(expected, b, pattern)) return false;
  return std::multiset<std::string>{a[1], a[2]} == std::multiset<std::string>{b[1], b[2]};
}

void org_cases() {
  std::size_t count = 0;
  for (const auto& line : gz_lines("org-cases.tsv.gz")) {
    const auto fields = split_tabs(line);
    std::istringstream in(fields.at(0));
    std::string kind, id;
    in >> kind >> std::quoted(id);
    std::string actual;
    bool ok = true;
    try {
      if (kind == "network") {
        std::string role, key;
        int projection = 0;
        std::size_t n = 0;
        in >> role >> std::quoted(key) >> projection >> n;
        EventLog log;
        Trace trace;
        trace.case_id = "root";
        const char* activities[] = {"Open", "Work", "Close", "Fourth", "Fifth", "Sixth"};
        const char* resources[] = {"p", "q", "r", "s", "t", "u"};
        const int seconds[] = {4, 19, 31, 40, 52, 58};
        for (std::size_t i = 0; i < n; ++i) {
          std::string token;
          in >> std::quoted(token);
          Event e;
          e.case_id = "root";
          e.activity = activities[i];
          e.resource = resources[i];
          char stamp[32];
          std::snprintf(stamp, sizeof stamp, "1970-01-01T00:00:%02dZ", seconds[i]);
          e.timestamp = stamp;
          e.activity_state = e.resource_state = e.timestamp_state = e.case_id_state = FirstClassAttributeState::kPresent;
          e.input_order = i;
          if (projection) {
            if (key == "org:resource") {
              e.resource = lexical(token);
              e.resource_source_key = key;
              e.resource_attribute = org_cell(key, token);
            } else {
              e.activity = lexical(token);
              e.activity_source_key = key;
              e.activity_attribute = org_cell(key, token);
            }
          } else {
            e.attributes.push_back(org_cell(key, token));
          }
          e.attributes.push_back({"target", "q"});
          trace.events.push_back(e);
        }
        log.traces.push_back(trace);
        NetworkAnalysisOptions o;
        o.include_performance = true;
        if (role == "relation") { o.out_relation_attribute = key; o.in_relation_attribute = key; }
        else if (role == "source") { o.source_node_attribute = key; o.target_node_attribute = "target"; o.edge_attribute = "target"; }
        else if (role == "target") { o.source_node_attribute = "target"; o.target_node_attribute = key; o.edge_attribute = "target"; }
        else { o.source_node_attribute = "target"; o.target_node_attribute = "target"; o.edge_attribute = key; }
        actual = canonical(discover_network_analysis(log, o));
      } else {
        std::size_t minimum = 0, n = 0;
        int event_id = 0;
        std::string id_dtype;
        in >> minimum >> event_id >> id_dtype >> n;
        EventTable t;
        t.columns = {"case_key", "phase", "worker", "begun", "finished", "identity"};
        t.dtypes = {"object", "object", "object", "datetime64[ns, UTC]", "datetime64[ns, UTC]", id_dtype};
        for (std::size_t i = 0; i < n; ++i) {
          EventTable::Row row;
          for (std::size_t c = 0; c < 6; ++c) {
            std::string token;
            in >> std::quoted(token);
            row.push_back(org_cell(t.columns[c], token));
          }
          t.rows.push_back(row);
        }
        BatchDiscoveryOptions o;
        o.activity_attribute = "phase"; o.resource_attribute = "worker"; o.case_id_attribute = "case_key";
        o.timestamp_attribute = "finished"; o.start_timestamp_attribute = "begun";
        o.merge_distance_seconds = 0; o.min_batch_size = minimum;
        if (event_id) o.event_id_attribute = "identity";
        actual = canonical(BatchPandasDiscovery::apply(t, o));
      }
    } catch (const std::exception& error) {
      ok = false;
      actual = error.what();
    }
    const bool expected_ok = fields.at(1) == "OK";
    if (ok != expected_ok) check(false, "org " + id + " outcome: " + actual + " | expected " + fields.at(2));
    else if (ok) same(actual, fields.at(2), "org " + id);
    else check(same_error(actual, fields.at(2)), "org " + id + " error: " + actual + " | expected " + fields.at(2));
    ++count;
  }
  check(count > 500, "organizational case table read " + std::to_string(count));
}

// ------------------------------------------------ calendar (M1, M9, year labels)

void calendar_cases() {
  struct Case { const char* fixture; const char* xes_timestamp; std::optional<std::string> key; std::optional<std::string> hour; };
  // selector-neighbors-comparisons.json: the reference hour, or nullopt
  // where the reference raises KeyError.
  const std::vector<Case> cases{
      {"selector-neighbor-canonical.xes", "", std::nullopt, "12"},
      {"selector-neighbor-canonical.xes", "", "time:timestamp", "12"},
      {"selector-neighbor-canonical.xes", "", "timestamp", std::nullopt},
      {"selector-neighbor-canonical.xes", "", "", std::nullopt},
      {"selector-neighbor-literal.xes", "timestamp", std::nullopt, std::nullopt},
      {"selector-neighbor-literal.xes", "timestamp", "time:timestamp", std::nullopt},
      {"selector-neighbor-literal.xes", "timestamp", "timestamp", "17"},
      {"selector-neighbor-literal.xes", "timestamp", "", std::nullopt},
      {"selector-neighbor-empty.xes", "", std::nullopt, "12"},
      {"selector-neighbor-empty.xes", "", "time:timestamp", "12"},
      {"selector-neighbor-empty.xes", "", "timestamp", std::nullopt},
      {"selector-neighbor-empty.xes", "", "", "17"},
  };
  for (const auto& c : cases) {
    XesImportOptions import;
    if (*c.xes_timestamp) import.timestamp_attribute = c.xes_timestamp;
    const auto log = import_xes_event_log(root / c.fixture, import);
    // Through the request parser, as the CLI and HTTP adapters read it: a
    // present empty key is the field named "".
    const auto options = parse_attribute_distribution_params(
        [&](std::string_view name) -> std::string {
          if (name == "attribute_distribution_type") return "hours";
          if (name == "attribute_distribution_timestamp_key" && c.key) return *c.key;
          return "";
        },
        [&](std::string_view name) {
          return name == "attribute_distribution_type" || (name == "attribute_distribution_timestamp_key" && c.key);
        });
    const std::string name = std::string("M1/M9 ") + c.fixture + " key " + (c.key ? "'" + *c.key + "'" : "omitted");
    try {
      const auto result = discover_event_distribution(log, options);
      std::string hit = "none";
      for (std::size_t i = 0; i < result.labels.size(); ++i) if (result.counts[i]) hit = result.labels[i];
      check(c.hour && hit == *c.hour, name + " gave hour " + hit);
    } catch (const std::invalid_argument& error) {
      check(!c.hour, name + " raised " + error.what());
    }
  }
  check(AttributeDistributionOptions{}.timestamp_key == "time:timestamp", "M1 default key is time:timestamp");

  // year-label-comparisons.json: years below 10 have two digits.
  struct Years { std::vector<const char*> timestamps; std::vector<std::string> labels; std::vector<std::uint64_t> counts; };
  const std::vector<Years> years{
      {{"0002-07-02T11:20:00Z"}, {"02"}, {1}},
      {{"0009-07-02T11:20:00Z"}, {"09"}, {1}},
      {{"0010-07-02T11:20:00Z"}, {"10"}, {1}},
      {{"0099-07-02T11:20:00Z"}, {"99"}, {1}},
      {{"0100-07-02T11:20:00Z"}, {"100"}, {1}},
      {{"0008-01-01T00:00:00Z", "0011-01-01T00:00:00Z"}, {"08", "09", "10", "11"}, {1, 0, 0, 1}},
  };
  for (const auto& y : years) {
    EventLog log;
    Trace trace;
    trace.case_id = "case";
    for (const auto* stamp : y.timestamps) {
      Event e;
      e.case_id = "case";
      e.activity = "A";
      e.timestamp = stamp;
      e.activity_state = e.timestamp_state = e.case_id_state = FirstClassAttributeState::kPresent;
      trace.events.push_back(e);
    }
    log.traces.push_back(trace);
    AttributeDistributionOptions o;
    o.distribution = EventDistributionType::kYears;
    const auto result = discover_event_distribution(log, o);
    check(result.labels == y.labels && result.counts == y.counts, std::string("year labels for ") + y.timestamps.front());
  }
}

// ------------------------------------------------ parsers (M9 network selectors, M10 zeta)

void parser_cases() {
  const std::vector<std::string> selectors{
      "network_analysis_sorting_attribute", "network_analysis_timestamp_attribute",
      "network_analysis_source_node_attribute", "network_analysis_target_node_attribute",
      "network_analysis_edge_attribute", "network_analysis_out_relation_attribute",
      "network_analysis_in_relation_attribute"};
  for (const auto& selector : selectors) {
    const auto options = parse_network_analysis_params([](std::string_view) { return std::string{}; },
                                                       [&](std::string_view name) { return name == selector; });
    const std::string values[] = {options.sorting_attribute, options.timestamp_attribute, options.source_node_attribute,
                                  options.target_node_attribute, options.edge_attribute, options.out_relation_attribute,
                                  options.in_relation_attribute};
    const auto at = static_cast<std::size_t>(std::find(selectors.begin(), selectors.end(), selector) - selectors.begin());
    check(values[at].empty(), "M9 present empty " + selector + " is the column named \"\"");
  }

  // adapter-neighbor-comparisons.json: finite subnormal zeta values are
  // numbers; the profile A->B mean 5 s, deviation 1 s, elapsed 3 s gives a
  // deviation at distance 2 for each of them.
  for (const auto* text : {"5e-324", "-5e-324", "1e-308", "-1e-308", "2.2250738585072014e-308"}) {
    const auto zeta = detail::parse_full_double(text);
    check(zeta && std::isfinite(*zeta) && *zeta == std::strtod(text, nullptr), std::string("M10 zeta ") + text + " parses");
    const auto params = parse_mine_params([&](std::string_view name) { return name == "temporal_zeta" ? std::string(text) : std::string{}; },
                                          [](std::string_view name) { return name == "temporal_zeta"; });
    check(params.temporal_profile.zeta == std::strtod(text, nullptr), std::string("M10 temporal_zeta ") + text);
    TemporalProfile profile;
    profile.entries.push_back({"RegisterC8", "CompleteC8", 1, 5.0, 1.0});
    StreamingTemporalOptions o;
    o.zeta = *zeta;
    StreamingTemporalConformance monitor(profile, o);
    for (const auto& [activity, stamp] : {std::pair{"RegisterC8", "2041-08-09T10:00:00Z"}, std::pair{"CompleteC8", "2041-08-09T10:00:03Z"}}) {
      Event e;
      e.case_id = "adapter-c8";
      e.activity = activity;
      e.timestamp = stamp;
      e.case_id_state = e.activity_state = e.timestamp_state = FirstClassAttributeState::kPresent;
      (void)monitor.receive(e, e.case_id);
    }
    const auto snapshot = monitor.snapshot();
    const auto found = snapshot.deviations.find("adapter-c8");
    check(found != snapshot.deviations.end() && found->second.size() == 1 && found->second[0].elapsed_seconds == 3.0 &&
              found->second[0].standard_deviation_distance == 2.0,
          std::string("M10 zeta ") + text + " reports elapsed 3 at distance 2");
  }
  check(!detail::parse_full_double("1e400") && !detail::parse_full_double("5e-324x") && !detail::parse_full_double(""),
        "M10 overflow and partial tokens stay invalid");
  check(detail::parse_full_double("1e-400") == 0.0, "M10 underflow to zero is the number 0");
  check(detail::stod("4.9406564584124654e-324") == std::numeric_limits<double>::denorm_min(), "M10 stod keeps subnormals");
}

// ------------------------------------------------ Woflan dead tasks

void woflan_cases() {
  struct Case { const char* model; bool asap; bool sound; std::optional<std::vector<std::string>> dead; };
  // c8-woflan-reference.jsonl; dead tasks only where the reference lists them.
  const std::vector<Case> cases{
      {"choice", false, true, std::vector<std::string>{}},
      {"choice", true, true, std::vector<std::string>{}},
      {"parallel_join", false, true, std::vector<std::string>{}},
      {"parallel_join", true, true, std::vector<std::string>{}},
      {"unreachable_join", false, false, std::vector<std::string>{"short_circuited_transition", "t2"}},
      {"unreachable_join", true, false, std::nullopt},
      {"silent_dead_task", false, true, std::vector<std::string>{}},
      {"silent_dead_task", true, false, std::nullopt},
      {"visible_dead_task", false, false, std::vector<std::string>{"t2"}},
      {"visible_dead_task", true, false, std::nullopt},
  };
  for (const auto& c : cases) {
    PnmlImportOptions import;
    import.infer_initial_marking = false;
    import.infer_final_marking = false;
    import.require_workflow_boundaries = false;
    const auto document = import_pnml_document(root / (std::string("woflan-") + c.model + ".pnml"), import);
    SoundnessOptions o;
    o.variant = SoundnessVariant::kWoflan;
    o.diagnostics = true;
    o.return_asap_when_unsound = c.asap;
    o.max_states = 20000;
    o.invariant_search_limit = 100000;
    for (const auto x : document.initial_marking) o.initial_marking.push_back(static_cast<std::int64_t>(x));
    for (const auto x : document.final_marking) o.final_marking.push_back(static_cast<std::int64_t>(x));
    const auto result = analyze_soundness(document.net, o);
    const std::string name = std::string("Woflan ") + c.model + (c.asap ? " asap" : "");
    check((result.status == SoundnessStatus::SOUND) == c.sound, name + " soundness");
    if (!c.dead) continue;
    std::vector<std::string> dead;
    for (const auto t : result.dead_tasks)
      dead.push_back(t == document.net.transitions().size() ? "short_circuited_transition" : "t" + std::to_string(t));
    std::sort(dead.begin(), dead.end());
    check(dead == *c.dead, name + " dead tasks " + json_list(dead));
  }
}
}  // namespace

// ------------------------------------------------ numpy object argsorts (M7)

// Group order and the first comparison that raises come from numpy's object
// argsorts; each logged comparison of the saved numpy runs must be made in
// the same order, past the small-array insertion sort.
void sort_log_cases() {
  std::size_t count = 0;
  for (const auto& line : gz_lines("numpy-sort-logs.tsv.gz")) {
    const auto fields = split_tabs(line);
    std::vector<long long> values;
    std::istringstream in(fields.at(1));
    for (long long v; in >> v;) values.push_back(v);
    std::string log;
    const auto cmp = [&](std::size_t a, std::size_t b) {
      const auto pair = std::to_string(a) + "," + std::to_string(b) + " ";
      log += "<" + pair;
      if (values[a] < values[b]) return -1;
      log += ">" + pair;
      return values[a] > values[b] ? 1 : 0;
    };
    const auto order = fields[0] == "quicksort" ? detail::numpy_object_quick_argsort(values.size(), cmp)
                                                : detail::numpy_object_stable_argsort(values.size(), cmp);
    std::string orders;
    for (const auto i : order) orders += (orders.empty() ? "" : " ") + std::to_string(i);
    if (!log.empty()) log.pop_back();
    same(log + "\t" + orders, fields.at(2) + "\t" + fields.at(3),
         "numpy " + fields[0] + " argsort of " + std::to_string(values.size()) + " values");
    ++count;
  }
  check(count == 300, "numpy sort logs read " + std::to_string(count));
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " <fixture-directory>\n";
    return 2;
  }
  root = argv[1];
  filter_cases();
  org_cases();
  calendar_cases();
  parser_cases();
  woflan_cases();
  sort_log_cases();
  if (failures) {
    std::cerr << failures << " recheck c8ee22d cases failed\n";
    return 1;
  }
  std::cout << "recheck c8ee22d cases passed\n";
}
