// Replays the reviewer's recheck of d5b33ef (2026-10-28): the saved public
// PM4Py 2.7.22 outputs for the six targets N1-N6 and their controls, plus fuzz
// cases captured from public calls with
// scripts/capture-recheck-d5b33ef-reference.py. Case tables come from
// scripts/generate-recheck-d5b33ef-fixture.py (fixtures/recheck_d5b33ef):
// filter-cases.tsv.gz (N3: consecutive grouping over wide and typed integer
// keys, and the reviewer's sort-boundary controls), org-cases.tsv.gz (N2:
// network analysis over tied sorting keys; N4: batches with NaN, None and
// equal mixed-type case and event ids), network-cli.tsv (N2 through XES
// import), pnml-numeric.tsv and pnml-properties.tsv.gz (N5: stochastic
// weight, priority and invisible properties). N1 is guarded by
// check_portable_integers.cmake and N6 by the frontend suite.
//
// Organizational cases list every result the reference gave over 40 hash
// seeds: where Python's set order decides which operands a failed '<' names,
// or whether a NaN id meets a string first, any observed result passes.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
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

#include "minera/batches.hpp"
#include "minera/event_log.hpp"
#include "minera/filtering_common.hpp"
#include "minera/network_analysis.hpp"
#include "minera/pandas_case_filter.hpp"
#include "minera/pandas_sequence_filters.hpp"
#include "minera/pnml.hpp"
#include "minera/stochastic_map.hpp"
#include "minera/xes_event_log_importer.hpp"

#include "../full_double.hpp"
#include "../gzip_codec.hpp"

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

// ------------------------------------------------ filter cases (N3 and sort-boundary controls)

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
        if (op == "start_nocc") result = pandas_filter_df_on_start_activities_nocc(table, filter_value(lo), std::nullopt, ck, ak);
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
  check(count >= 782, "filter case table read " + std::to_string(count));
}

// ------------------------------------------------ organizational cases (N2, N4)

// The reviewer's wider probe tokens: S text, I int (lexical), F float, B
// bool, N None, A NA, D timestamp.
EventAttribute org_value(const std::string& token, const std::string& key) {
  EventAttribute a{key, token.substr(1)};
  switch (token.at(0)) {
    case 'N': a.value.clear(); a.is_null = true; break;
    case 'A': a.value.clear(); a.typed_value = NativeNA{}; break;
    case 'I': a.kind = AttributeKind::kInt; break;
    case 'F': a.kind = AttributeKind::kFloat; a.typed_value = std::strtod(a.value.c_str(), nullptr); break;
    case 'B':
      a.kind = AttributeKind::kBoolean;
      a.typed_value = a.value == "1";
      a.value = a.value == "1" ? "true" : "false";
      break;
    case 'D': a.kind = AttributeKind::kDate; a.typed_value = NativeDate{a.value}; break;
    default: break;
  }
  return a;
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
// Both raise; the operands of a failed '<' may be named in either order.
bool same_error(const std::string& actual, const std::string& expected) {
  if (actual.ends_with(expected)) return true;
  static const std::regex pattern("'<' not supported between instances of '(\\w+)' and '(\\w+)'");
  std::smatch a, b;
  if (!std::regex_search(actual, a, pattern) || !std::regex_search(expected, b, pattern)) return false;
  return std::multiset<std::string>{a[1], a[2]} == std::multiset<std::string>{b[1], b[2]};
}
// Passes when the native result is one of the listed reference results. A
// declared set-order case (see the generator) passes with any failed '<'.
void expect_any(bool ok, const std::string& actual, const std::vector<std::string>& alternatives, const std::string& name) {
  for (const auto& alternative : alternatives) {
    if (ok && alternative == "OK " + actual) return;
    if (!ok && alternative.starts_with("ERR ") && same_error(actual, alternative.substr(4))) return;
    if (!ok && alternative == "DECLARED set-layout" &&
        actual.find("'<' not supported between instances of") != std::string::npos)
      return;
  }
  std::cerr << "FAIL " << name << "\n  actual   " << (ok ? "OK " : "ERR ") << actual << "\n";
  for (const auto& alternative : alternatives) std::cerr << "  expected " << alternative << "\n";
  ++failures;
}

void org_cases() {
  std::size_t count = 0, networks = 0;
  for (const auto& line : gz_lines("org-cases.tsv.gz")) {
    const auto fields = split_tabs(line);
    std::istringstream in(fields.at(0));
    std::string kind, id, mode, case_dtype, id_dtype;
    std::size_t minimum = 0, n = 0;
    double distance = 0;
    int event_id = 0;
    in >> kind >> std::quoted(id) >> mode >> minimum >> distance >> event_id >> case_dtype >> id_dtype >> n;
    std::vector<std::vector<std::string>> rows(n, std::vector<std::string>(6));
    for (auto& row : rows)
      for (auto& token : row) in >> std::quoted(token);
    std::string actual;
    bool ok = true;
    try {
      if (kind == "batches") {
        EventTable t;
        t.columns = {"case", "activity", "resource", "start", "end", "identity"};
        t.dtypes = {case_dtype, "object", "object", "datetime64[ns, UTC]", "datetime64[ns, UTC]", id_dtype};
        for (const auto& row : rows) {
          EventTable::Row cells;
          for (std::size_t c = 0; c < row.size(); ++c) cells.push_back(org_value(row[c], t.columns[c]));
          t.rows.push_back(cells);
        }
        BatchDiscoveryOptions o;
        o.case_id_attribute = "case"; o.activity_attribute = "activity"; o.resource_attribute = "resource";
        o.start_timestamp_attribute = "start"; o.timestamp_attribute = "end";
        if (event_id) o.event_id_attribute = "identity";
        o.min_batch_size = minimum;
        o.merge_distance_seconds = distance;
        actual = canonical(BatchPandasDiscovery::apply(t, o));
      } else {
        ++networks;
        EventLog log;
        Trace trace;
        trace.case_id = "container";
        const char* keys[] = {"case:concept:name", "concept:name", "org:resource", "time:timestamp", "rank", "relation"};
        for (std::size_t i = 0; i < rows.size(); ++i) {
          Event e;
          e.input_order = i;
          e.case_id = rows[i][0].substr(1);
          e.activity = e.resource = "placeholder";
          e.timestamp = rows[i][3].substr(1);
          e.case_id_state = e.activity_state = e.resource_state = e.timestamp_state = FirstClassAttributeState::kPresent;
          for (std::size_t c = 0; c < 6; ++c) e.attributes.push_back(org_value(rows[i][c], keys[c]));
          trace.events.push_back(e);
        }
        log.traces.push_back(trace);
        NetworkAnalysisOptions o;
        o.include_performance = true;
        o.sorting_attribute = "rank";
        o.out_relation_attribute = o.in_relation_attribute = "relation";
        if (mode == "group") { o.target_node_attribute = "concept:name"; o.edge_attribute = "relation"; }
        actual = canonical(discover_network_analysis(log, o));
      }
    } catch (const std::exception& error) {
      ok = false;
      actual = error.what();
    }
    expect_any(ok, actual, {fields.begin() + 1, fields.end()}, "org " + id);
    ++count;
  }
  check(count >= 1500 && networks >= 550, "organizational case table read " + std::to_string(count));
}

// N2 through XES import, as the reviewer ran the CLI.
void network_cli_cases() {
  std::ifstream input(root / "network-cli.tsv");
  std::size_t count = 0;
  for (std::string line; std::getline(input, line);) {
    if (line.empty()) continue;
    const auto fields = split_tabs(line);
    NetworkAnalysisOptions o;
    o.include_performance = true;
    o.sorting_attribute = "rank";
    o.out_relation_attribute = o.in_relation_attribute = "relation";
    std::string actual;
    bool ok = true;
    try {
      actual = canonical(discover_network_analysis(import_xes_event_log(root / fields.at(0)), o));
    } catch (const std::exception& error) {
      ok = false;
      actual = error.what();
    }
    expect_any(ok, actual, {fields.at(1)}, "network " + fields.at(0));
    ++count;
  }
  check(count == 6, "network logs read " + std::to_string(count));
}

// ------------------------------------------------ PNML stochastic properties (N5)

std::string unhex(const std::string& text) {
  std::string out;
  for (std::size_t i = 0; i + 1 < text.size(); i += 2) out += static_cast<char>(std::stoi(text.substr(i, 2), nullptr, 16));
  return out;
}
// Native policy where PM4Py keeps any float weight and any int priority.
bool policy_error(const std::string& message) {
  return message.find("must be finite and non-negative") != std::string::npos ||
         message.find("outside the native integer range") != std::string::npos;
}
void expect_pnml(const std::function<PnmlDocument()>& import, const std::string& expected, bool properties,
                 const std::string& name) {
  try {
    const auto document = import();
    check(document.stochastic_map.has_value() && !document.stochastic_map->transitions.empty(), name + " has a stochastic map");
    if (!document.stochastic_map || document.stochastic_map->transitions.empty()) return;
    const auto& record = document.stochastic_map->transitions.front();
    std::string actual = "OK " + filtering_float_repr(record.activation_weight);
    if (properties)
      actual += " " + std::to_string(record.priority) + " " +
                (document.net.transitions().front().activity ? "0" : "1");
    same(actual, expected, name);
  } catch (const std::exception& error) {
    const std::string message = error.what();
    if (expected == "POLICY") check(policy_error(message), name + " policy: " + message);
    else check(expected.starts_with("ERR ") && message.ends_with(expected.substr(4)),
               name + " error: " + message + " | expected " + expected);
  }
}

void pnml_numeric_cases() {
  std::ifstream input(root / "pnml-numeric.tsv");
  std::size_t count = 0;
  for (std::string line; std::getline(input, line);) {
    if (line.empty()) continue;
    const auto fields = split_tabs(line);
    PnmlImportOptions options;
    options.import_stochastic_map = true;
    expect_pnml([&] { return import_pnml_document(root / fields.at(0), options); }, fields.at(1), false,
                "pnml " + fields.at(0));
    ++count;
  }
  check(count == 18, "PNML fixtures read " + std::to_string(count));
}

void pnml_property_cases() {
  std::size_t count = 0;
  for (const auto& line : gz_lines("pnml-properties.tsv.gz")) {
    const auto fields = split_tabs(line);
    const auto xml = unhex(fields.at(2));
    PnmlImportOptions options;
    options.import_stochastic_map = true;
    const auto name = "pnml " + fields.at(0) + " " + (fields.at(1) == "-" ? std::string{"<none>"} : unhex(fields.at(1)));
    expect_pnml([&] { return parse_pnml_document(xml, options); }, fields.at(3), true, name);
    // The property converts while the document is read, so the error is the
    // same without the stochastic map.
    if (fields.at(3).starts_with("ERR ")) {
      try {
        (void)parse_pnml_document(xml);
        check(false, name + " raises without the stochastic map");
      } catch (const std::exception& error) {
        check(std::string(error.what()).ends_with(fields.at(3).substr(4)), name + " error without the map: " + error.what());
      }
    }
    ++count;
  }
  check(count >= 80, "PNML property cases read " + std::to_string(count));
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " <fixture-directory>\n";
    return 2;
  }
  root = argv[1];
  filter_cases();
  org_cases();
  network_cli_cases();
  pnml_numeric_cases();
  pnml_property_cases();
  if (failures) {
    std::cerr << failures << " recheck d5b33ef cases failed\n";
    return 1;
  }
  std::cout << "recheck d5b33ef cases passed\n";
}
