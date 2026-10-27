#include "minera/pandas_sequence_filters.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <variant>

#include "minera/filtering_common.hpp"
#include "numpy_object_sort.hpp"

namespace minera {

PandasFilterValue PandasFilterValue::of(bool value) {
  PandasFilterValue out;
  out.kind = Kind::kBool;
  out.boolean = value;
  return out;
}
PandasFilterValue PandasFilterValue::of(std::int64_t value) {
  PandasFilterValue out;
  out.kind = Kind::kInt;
  out.integer = value;
  return out;
}
PandasFilterValue PandasFilterValue::of(double value) {
  PandasFilterValue out;
  out.kind = Kind::kFloat;
  out.real = value;
  return out;
}
PandasFilterValue PandasFilterValue::of(std::string value) {
  PandasFilterValue out;
  out.kind = Kind::kString;
  out.text = std::move(value);
  return out;
}
PandasFilterValue PandasFilterValue::list(std::vector<PandasFilterValue> values) {
  PandasFilterValue out;
  out.kind = Kind::kList;
  out.items = std::move(values);
  return out;
}
PandasFilterValue PandasFilterValue::tuple(std::vector<PandasFilterValue> values) {
  PandasFilterValue out;
  out.kind = Kind::kTuple;
  out.items = std::move(values);
  return out;
}
PandasFilterValue PandasFilterValue::dict(std::vector<std::pair<PandasFilterValue, PandasFilterValue>> values) {
  PandasFilterValue out;
  out.kind = Kind::kDict;
  out.entries = std::move(values);
  return out;
}

namespace {

using Value = PandasFilterValue;
using Kind = PandasFilterValue::Kind;
using Cell = std::optional<EventAttribute>;

[[noreturn]] void fail(const std::string& type, const std::string& message) {
  throw PandasSequenceFilterError(type, message);
}

// Python repr of a str.
std::string py_repr(const std::string& text) {
  const bool double_quotes = text.find('\'') != std::string::npos && text.find('"') == std::string::npos;
  const char quote = double_quotes ? '"' : '\'';
  std::string out(1, quote);
  for (const char c : text) {
    if (c == '\\') out += "\\\\";
    else if (c == quote) out += std::string("\\") + quote;
    else if (c == '\n') out += "\\n";
    else if (c == '\t') out += "\\t";
    else out += c;
  }
  return out + quote;
}

[[noreturn]] void key_error(const std::string& message) { fail("KeyError", py_repr(message)); }

std::string type_name(const Value& v) {
  switch (v.kind) {
    case Kind::kNone: return "NoneType";
    case Kind::kBool: return "bool";
    case Kind::kInt: return "int";
    case Kind::kFloat: return "float";
    case Kind::kString: return "str";
    case Kind::kList: return "list";
    case Kind::kTuple: return "tuple";
    case Kind::kDict: return "dict";
  }
  return "object";
}

bool truthy(const Value& v) {
  switch (v.kind) {
    case Kind::kNone: return false;
    case Kind::kBool: return v.boolean;
    case Kind::kInt: return v.integer != 0;
    case Kind::kFloat: return v.real != 0.0;
    case Kind::kString: return !v.text.empty();
    case Kind::kList:
    case Kind::kTuple: return !v.items.empty();
    case Kind::kDict: return !v.entries.empty();
  }
  return false;
}

// A Python number (bool and int as integers).
struct Number {
  bool integral = true;
  std::int64_t integer = 0;
  double real = 0.0;
};
std::optional<Number> number_of(const Value& v) {
  if (v.kind == Kind::kBool) return Number{true, v.boolean ? 1 : 0, 0.0};
  if (v.kind == Kind::kInt) return Number{true, v.integer, 0.0};
  if (v.kind == Kind::kFloat) return Number{false, 0, v.real};
  return std::nullopt;
}
// a >= b for an int a and a Python number b.
bool int_at_least(std::int64_t a, const Number& b) {
  if (b.integral) return a >= b.integer;
  return static_cast<double>(a) >= b.real;  // the small counts here convert exactly
}

const Value* parameter(const PandasFilterParameters& parameters, std::string_view member, std::string_view literal) {
  if (!parameters) return nullptr;
  for (const auto& p : *parameters)
    if (p.enum_member && p.key == member) return &p.value;
  for (const auto& p : *parameters)
    if (!p.enum_member && p.key == literal) return &p.value;
  return nullptr;
}
Value parameter_or(const PandasFilterParameters& parameters, std::string_view member, std::string_view literal,
                   Value fallback) {
  const auto* found = parameter(parameters, member, literal);
  return found ? *found : fallback;
}
std::string text_parameter(const PandasFilterParameters& parameters, std::string_view member,
                           std::string_view literal, const std::string& fallback) {
  const auto* found = parameter(parameters, member, literal);
  return found ? found->text : fallback;
}
std::string case_key(const PandasFilterParameters& p) {
  return text_parameter(p, "CASE_ID_KEY", kPandasFilterCaseIdKeyParameter, std::string(kPandasFilterCaseConceptName));
}
std::string activity_key(const PandasFilterParameters& p) {
  return text_parameter(p, "ACTIVITY_KEY", kPandasFilterActivityKeyParameter, std::string(kPandasFilterDefaultNameKey));
}

// ------------------------------------------------------------------ cells

bool null_cell(const Cell& cell) {
  if (!cell || cell->is_null || is_native_na_attribute(*cell)) return true;
  if (const auto* d = std::get_if<double>(&cell->typed_value)) return std::isnan(*d);
  if (cell->kind == AttributeKind::kFloat) return cell->value == "NaN" || cell->value == "nan";
  if (cell->kind == AttributeKind::kDate) return cell->value == "NaT";
  return false;
}

// A hashable, orderable identity of a non-null cell: ints by value, every
// other scalar by its kind and text.
struct Key {
  int tag = 0;  // 0 int, 1 string, 2 other
  std::int64_t integer = 0;
  std::string text;
  bool operator==(const Key&) const = default;
  bool operator<(const Key& o) const {
    if (tag != o.tag) return tag < o.tag;
    if (tag == 0) return integer < o.integer;
    return text < o.text;
  }
};
// Python dict/groupby keys: 1, True and 1.0 are one key (equal values hash
// equally); the string '1' stays distinct.
Key key_of(const EventAttribute& cell) {
  if (const auto* i = std::get_if<std::int64_t>(&cell.typed_value)) return {0, *i, {}};
  if (const auto* u = std::get_if<std::uint64_t>(&cell.typed_value)) return {0, static_cast<std::int64_t>(*u), {}};
  if (const auto* b = std::get_if<bool>(&cell.typed_value)) return {0, *b ? 1 : 0, {}};
  if (cell.kind == AttributeKind::kBoolean && !cell.typed_value.index())
    if (cell.value == "True" || cell.value == "true" || cell.value == "False" || cell.value == "false")
      return {0, cell.value == "True" || cell.value == "true" ? 1 : 0, {}};
  if (const auto* d = std::get_if<double>(&cell.typed_value);
      d && std::isfinite(*d) && std::floor(*d) == *d && std::fabs(*d) < 9.2e18)
    return {0, static_cast<std::int64_t>(*d), {}};
  if ((cell.kind == AttributeKind::kFloat || cell.kind == AttributeKind::kInt) && !cell.typed_value.index()) {
    char* end = nullptr;
    const double d = std::strtod(cell.value.c_str(), &end);
    if (end && *end == '\0' && !cell.value.empty() && std::isfinite(d) && std::floor(d) == d && std::fabs(d) < 9.2e18)
      return {0, static_cast<std::int64_t>(d), {}};
  }
  if (cell.kind == AttributeKind::kString) return {1, 0, cell.value};
  return {2, 0, std::to_string(static_cast<int>(cell.kind)) + ":" + cell.value};
}
bool is_text(const Cell& cell) { return cell && !null_cell(cell) && cell->kind == AttributeKind::kString; }

// Python str() of a non-null case id.
std::string py_str(const EventAttribute& cell) { return cell.value; }

std::optional<std::size_t> find_column(const EventTable& df, const std::string& name) {
  for (std::size_t i = 0; i < df.columns.size(); ++i)
    if (df.columns[i] == name) return i;
  return std::nullopt;
}
std::size_t require_column(const EventTable& df, const std::string& name, bool groupby_selection = false) {
  const auto found = find_column(df, name);
  if (!found) key_error(groupby_selection ? "Column not found: " + name : name);
  return *found;
}

EventAttribute make_cell(const std::string& column, std::string text, AttributeKind kind) {
  EventAttribute out;
  out.key = column;
  out.value = std::move(text);
  out.kind = kind;
  return out;
}
EventAttribute int_cell(const std::string& column, std::int64_t value) {
  auto out = make_cell(column, std::to_string(value), AttributeKind::kInt);
  out.typed_value = value;
  return out;
}
EventAttribute float_cell(const std::string& column, double value) {
  auto out = make_cell(column, std::isnan(value) ? "NaN" : filtering_float_repr(value), AttributeKind::kFloat);
  out.typed_value = value;
  return out;
}
EventAttribute string_cell(const std::string& column, std::string value) {
  return make_cell(column, std::move(value), AttributeKind::kString);
}

EventTableIndexLabel range_label(std::size_t i) { return {int_cell("", static_cast<std::int64_t>(i))}; }

std::vector<EventTableIndexLabel> labels_of(const EventTable& df) {
  if (df.index) return df.index->labels;
  std::vector<EventTableIndexLabel> out;
  for (std::size_t i = 0; i < df.size(); ++i) out.push_back(range_label(i));
  return out;
}

bool arithmetic(const std::vector<EventTableIndexLabel>& labels) {
  std::vector<std::int64_t> values;
  for (const auto& label : labels) {
    const auto* v = std::get_if<EventAttribute>(&label.value);
    if (!v) return false;
    if (const auto* i = std::get_if<std::int64_t>(&v->typed_value)) values.push_back(*i);
    else if (const auto* u = std::get_if<std::uint64_t>(&v->typed_value)) values.push_back(static_cast<std::int64_t>(*u));
    else return false;
  }
  if (values.size() < 2) return true;
  const auto step = values[1] - values[0];
  if (step == 0) return false;
  for (std::size_t i = 2; i < values.size(); ++i)
    if (values[i] - values[i - 1] != step) return false;
  return true;
}

EventTable subset(const EventTable& df, const std::vector<bool>& keep) {
  EventTable out = df;
  out.rows.clear();
  const auto labels = labels_of(df);
  if (!out.index) out.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  out.index->labels.clear();
  for (std::size_t i = 0; i < df.size(); ++i) {
    if (!keep[i]) continue;
    out.rows.push_back(df.rows[i]);
    out.index->labels.push_back(labels[i]);
  }
  if (out.index->kind == "RangeIndex" && !arithmetic(out.index->labels)) out.index->kind = "Index";
  return out;
}

void reset_index(EventTable& df) {
  df.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  for (std::size_t i = 0; i < df.size(); ++i) df.index->labels.push_back(range_label(i));
}

// Rows whose case id is in `cases` (positive) or not (negative; null case
// ids are never in the set).
EventTable keep_cases(const EventTable& df, std::size_t case_column, const std::set<Key>& cases, bool positive) {
  std::vector<bool> keep(df.size());
  for (std::size_t i = 0; i < df.size(); ++i) {
    const auto& c = df.rows[i][case_column];
    const bool in = !null_cell(c) && cases.count(key_of(*c));
    keep[i] = positive ? in : !in;
  }
  return subset(df, keep);
}

// Case ids in first-appearance order with their row positions.
struct Cases {
  std::vector<Key> order;
  std::map<Key, std::vector<std::size_t>> rows;
};
Cases cases_of(const EventTable& df, std::size_t case_column) {
  Cases out;
  for (std::size_t i = 0; i < df.size(); ++i) {
    const auto& c = df.rows[i][case_column];
    if (null_cell(c)) continue;
    const auto key = key_of(*c);
    auto [it, added] = out.rows.emplace(key, std::vector<std::size_t>{});
    if (added) out.order.push_back(key);
    it->second.push_back(i);
  }
  return out;
}

// An activity value as the boundary filters see it: absent for null.
using Activity = std::optional<std::string>;

// The first (or last) non-null activity of each case.
std::map<Key, Activity> boundary_activities(const EventTable& df, const Cases& cases, std::size_t act, bool end) {
  std::map<Key, Activity> out;
  for (const auto& key : cases.order) {
    Activity found;
    const auto& rows = cases.rows.at(key);
    for (std::size_t j = 0; j < rows.size(); ++j) {
      const auto& cell = df.rows[end ? rows[rows.size() - 1 - j] : rows[j]][act];
      if (!null_cell(cell)) {
        found = cell->value;
        break;
      }
    }
    out[key] = found;
  }
  return out;
}

bool value_is_null(const Value& v) { return v.kind == Kind::kNone || (v.kind == Kind::kFloat && std::isnan(v.real)); }

bool matches_value(const Activity& a, const Value& v) {
  if (!a) return value_is_null(v);
  return v.kind == Kind::kString && v.text == *a;
}

EventTable boundary_filter(const EventTable& df, const Value& values, const std::string& case_glue,
                           const std::string& act_key, const Value& positive, bool end) {
  const auto case_column = require_column(df, case_glue);
  const auto act = require_column(df, act_key, true);
  const auto cases = cases_of(df, case_column);
  const auto boundary = boundary_activities(df, cases, act, end);
  std::set<Key> kept;
  for (const auto& [key, a] : boundary)
    for (const auto& v : values.items)
      if (matches_value(a, v)) {
        kept.insert(key);
        break;
      }
  return keep_cases(df, case_column, kept, truthy(positive));
}

// Counted boundary activities: (activity or null, count) in insertion order.
using Counts = std::vector<std::pair<Activity, Value>>;
Counts count_boundaries(const std::vector<Key>& order, const std::map<Key, Activity>& boundary) {
  Counts out;
  for (const auto& key : order) {
    const auto& a = boundary.at(key);
    bool found = false;
    for (auto& [k, v] : out)
      if (k == a) {
        ++v.integer;
        found = true;
        break;
      }
    if (!found) out.emplace_back(a, Value::of(std::int64_t{1}));
  }
  return out;
}
Counts counts_argument(const Value& dict) {
  Counts out;
  for (const auto& [k, v] : dict.entries)
    out.emplace_back(value_is_null(k) ? Activity{} : Activity{k.text}, v);
  return out;
}

struct NoccOutcome {
  EventTable table;
  Counts kept;
};
NoccOutcome nocc_filter(const EventTable& df, const Value& nocc, const std::optional<Value>& count0,
                        const std::string& case_glue, const std::string& act_key, bool end,
                        const std::optional<Value>& most_common_variant = std::nullopt) {
  const auto case_column = require_column(df, case_glue);
  const auto act = require_column(df, act_key, true);
  const auto cases = cases_of(df, case_column);
  const auto boundary = boundary_activities(df, cases, act, end);
  const Counts counts = count0 ? counts_argument(*count0) : count_boundaries(cases.order, boundary);
  const auto threshold = number_of(nocc);
  if (!threshold) fail("TypeError", "'>=' not supported between instances of 'int' and '" + type_name(nocc) + "'");
  Counts kept;
  for (const auto& entry : counts) {
    const auto count = number_of(entry.second);
    const bool ok = count && (count->integral ? int_at_least(count->integer, *threshold)
                                              : (threshold->integral ? count->real >= static_cast<double>(threshold->integer)
                                                                     : count->real >= threshold->real));
    // The last activity of the most common variant is always kept.
    const bool protected_end = most_common_variant && !most_common_variant->items.empty() && entry.first &&
                               most_common_variant->items.back().kind == Kind::kString &&
                               *entry.first == most_common_variant->items.back().text;
    if (ok || protected_end) kept.push_back(entry);
  }
  if (kept.size() == counts.size()) return {df, kept};
  std::set<Key> keep;
  for (const auto& [key, a] : boundary)
    for (const auto& entry : kept)
      if (entry.first == a) {
        keep.insert(key);
        break;
      }
  return {keep_cases(df, case_column, keep, true), kept};
}

Value counts_value(const Counts& counts) {
  std::vector<std::pair<Value, Value>> entries;
  for (const auto& [k, v] : counts) entries.emplace_back(k ? Value::of(*k) : Value::of(std::nan("")), v);
  return Value::dict(std::move(entries));
}

// ------------------------------------------------------------------ variants

// ",".join(item) of an admitted trace, prefix or suffix.
std::string join_pattern(const Value& item) {
  std::string out;
  if (item.kind == Kind::kString) {
    // A str iterates over its characters (UTF-8 sequences).
    bool first = true;
    for (std::size_t i = 0; i < item.text.size();) {
      std::size_t len = 1;
      const auto c = static_cast<unsigned char>(item.text[i]);
      if (c >= 0xF0) len = 4;
      else if (c >= 0xE0) len = 3;
      else if (c >= 0xC0) len = 2;
      if (!first) out += ",";
      out += item.text.substr(i, len);
      first = false;
      i += len;
    }
    return out;
  }
  for (std::size_t i = 0; i < item.items.size(); ++i) {
    if (i) out += ",";
    out += item.items[i].text;
  }
  return out;
}

// Each case's variant string, by first appearance of the case.
std::vector<std::pair<Key, std::string>> variants(const EventTable& df, std::size_t case_column, std::size_t act) {
  const auto cases = cases_of(df, case_column);
  std::vector<std::pair<Key, std::string>> out;
  for (const auto& key : cases.order) {
    const auto& rows = cases.rows.at(key);
    std::string joined;
    for (std::size_t j = 0; j < rows.size(); ++j) {
      const auto& cell = df.rows[rows[j]][act];
      if (null_cell(cell))
        fail("TypeError", "sequence item " + std::to_string(j) + ": expected str instance, float found");
      if (j) joined += ",";
      joined += cell->value;
    }
    out.emplace_back(key, std::move(joined));
  }
  return out;
}

std::vector<std::string> split_commas(const std::string& text) {
  std::vector<std::string> out(1);
  for (const char c : text) {
    if (c == ',') out.emplace_back();
    else out.back() += c;
  }
  return out;
}

// The trace filter's match: tokens of the joined pattern, where "..." spans
// any run of variant tokens (a run between two wildcards must be nonempty).
bool trace_match(const std::string& variant, const std::string& pattern) {
  const auto tokens = split_commas(pattern);
  bool wildcard = false;
  for (const auto& t : tokens) wildcard = wildcard || t == "...";
  if (!wildcard) return variant == pattern;
  const auto words = split_commas(variant);
  std::vector<std::vector<std::string>> segments(1);
  for (const auto& t : tokens) {
    if (t == "...") segments.emplace_back();
    else segments.back().push_back(t);
  }
  const auto& prefix = segments.front();
  const auto& suffix = segments.back();
  if (words.size() < prefix.size()) return false;
  for (std::size_t i = 0; i < prefix.size(); ++i)
    if (words[i] != prefix[i]) return false;
  std::size_t pos = prefix.size();
  for (std::size_t s = 1; s + 1 < segments.size(); ++s) {
    const auto& middle = segments[s];
    if (middle.empty()) return false;
    bool found = false;
    for (std::size_t start = pos; start + middle.size() <= words.size(); ++start) {
      bool same = true;
      for (std::size_t k = 0; k < middle.size() && same; ++k) same = words[start + k] == middle[k];
      if (same) {
        pos = start + middle.size();
        found = true;
        break;
      }
    }
    if (!found) return false;
  }
  if (words.size() < pos + suffix.size()) return false;
  for (std::size_t i = 0; i < suffix.size(); ++i)
    if (words[words.size() - suffix.size() + i] != suffix[i]) return false;
  return true;
}

enum class VariantMode { kTrace, kStartsWith, kEndsWith };

EventTable variant_filter(const EventTable& df, const Value& admitted, const PandasFilterParameters& parameters,
                          VariantMode mode) {
  const auto case_column = require_column(df, case_key(parameters));
  const auto act = require_column(df, activity_key(parameters), true);
  const bool positive = truthy(parameter_or(parameters, "POSITIVE", "positive", Value::of(true)));
  std::vector<std::string> patterns;
  for (const auto& item : admitted.items) patterns.push_back(join_pattern(item));
  std::set<Key> kept;
  for (const auto& [key, variant] : variants(df, case_column, act)) {
    // An empty list of admitted traces admits every case.
    bool ok = mode == VariantMode::kTrace && patterns.empty();
    for (const auto& p : patterns) {
      if (ok) break;
      if (mode == VariantMode::kTrace) ok = trace_match(variant, p);
      else if (mode == VariantMode::kStartsWith) ok = variant.compare(0, p.size(), p) == 0 && variant.size() >= p.size();
      else ok = variant.size() >= p.size() && variant.compare(variant.size() - p.size(), p.size(), p) == 0;
      if (ok) break;
    }
    if (ok) kept.insert(key);
  }
  return keep_cases(df, case_column, kept, positive);
}

// ------------------------------------------------------------------ positions

std::string index_repr(const std::vector<std::string>& names) {
  std::string out = "Index([";
  for (std::size_t i = 0; i < names.size(); ++i) out += (i ? ", " : "") + py_repr(names[i]);
  return out + "], dtype='str')";
}

// df[[a, b]] column selection failures.
void require_columns(const EventTable& df, std::vector<std::string> names) {
  std::vector<std::string> unique;
  for (const auto& n : names)
    if (std::find(unique.begin(), unique.end(), n) == unique.end()) unique.push_back(n);
  names = unique;
  std::vector<std::string> missing;
  for (const auto& n : names)
    if (!find_column(df, n)) missing.push_back(n);
  if (missing.empty()) return;
  if (missing.size() == names.size()) key_error("None of [" + index_repr(names) + "] are in the [columns]");
  std::string list = "[";
  for (std::size_t i = 0; i < missing.size(); ++i) list += (i ? ", " : "") + py_repr(missing[i]);
  key_error(list + "] not in index");
}

// Position of each row in its case (absent for null case ids).
std::vector<std::optional<std::int64_t>> positions(const EventTable& df, std::size_t case_column) {
  std::vector<std::optional<std::int64_t>> out(df.size());
  std::map<Key, std::int64_t> next;
  for (std::size_t i = 0; i < df.size(); ++i) {
    const auto& c = df.rows[i][case_column];
    if (null_cell(c)) continue;
    out[i] = next[key_of(*c)]++;
  }
  return out;
}

EventTable position_filter(const EventTable& df, const Value& activity, const PandasFilterParameters& parameters,
                           bool prefix) {
  const auto case_name = case_key(parameters);
  const auto act_name = activity_key(parameters);
  const auto first_or_last = parameter_or(parameters, "FIRST_OR_LAST", "first_or_last", Value::of("first"));
  const bool strict = truthy(parameter_or(parameters, "STRICT", "strict", Value::of(true)));
  const auto case_column = require_column(df, case_name);
  const auto act = require_column(df, act_name);
  const auto index = positions(df, case_column);
  const bool first = first_or_last.kind == Kind::kString && first_or_last.text == "first";
  const bool last = first_or_last.kind == Kind::kString && first_or_last.text == "last";
  if (!first && !last) fail("AttributeError", "'DataFrameGroupBy' object has no attribute 'reset_index'");
  std::map<Key, std::int64_t> occurrence;
  for (std::size_t i = 0; i < df.size(); ++i) {
    const auto& c = df.rows[i][case_column];
    const auto& a = df.rows[i][act];
    if (!index[i] || null_cell(a) || activity.kind != Kind::kString || a->value != activity.text) continue;
    const auto key = key_of(*c);
    if (first) occurrence.emplace(key, *index[i]);
    else occurrence[key] = *index[i];
  }
  std::vector<std::optional<std::int64_t>> temp(df.size());
  bool index_null = false, temp_null = df.empty();
  for (std::size_t i = 0; i < df.size(); ++i) {
    if (!index[i]) {
      index_null = temp_null = true;
      continue;
    }
    const auto found = occurrence.find(key_of(*df.rows[i][case_column]));
    if (found == occurrence.end()) temp_null = true;
    else temp[i] = found->second;
  }
  EventTable out = df;
  const std::string index_column = "@@index_in_trace", temp_column = "@@temp_column";
  out.columns.push_back(index_column);
  out.columns.push_back(temp_column);
  if (!out.dtypes.empty()) {
    out.dtypes.push_back(index_null ? "float64" : "int64");
    out.dtypes.push_back(temp_null ? "float64" : "int64");
  }
  if (!out.categorical_dtypes.empty()) {
    out.categorical_dtypes.emplace_back();
    out.categorical_dtypes.emplace_back();
  }
  std::vector<bool> keep(df.size());
  for (std::size_t i = 0; i < df.size(); ++i) {
    auto& row = out.rows[i];
    if (index_null) row.push_back(float_cell(index_column, index[i] ? static_cast<double>(*index[i]) : std::nan("")));
    else row.push_back(int_cell(index_column, *index[i]));
    if (temp_null) row.push_back(float_cell(temp_column, temp[i] ? static_cast<double>(*temp[i]) : std::nan("")));
    else row.push_back(int_cell(temp_column, *temp[i]));
    if (index[i] && temp[i]) {
      const auto x = *index[i], t = *temp[i];
      keep[i] = prefix ? (strict ? x < t : x <= t) : (strict ? x > t : x >= t);
    }
  }
  return subset(out, keep);
}

// ------------------------------------------------------------------ grouping

EventTable merge_groups(const EventTable& df, const std::vector<std::vector<std::size_t>>& groups, bool first,
                        const std::vector<std::size_t>& column_order) {
  EventTable out = df;
  out.rows.clear();
  out.columns.clear();
  out.dtypes.clear();
  out.categorical_dtypes.clear();
  for (const auto c : column_order) {
    out.columns.push_back(df.columns[c]);
    if (!df.dtypes.empty()) out.dtypes.push_back(df.dtypes[c]);
    if (!df.categorical_dtypes.empty()) out.categorical_dtypes.push_back(df.categorical_dtypes[c]);
  }
  for (const auto& members : groups) {
    EventTable::Row row;
    for (const auto c : column_order) {
      Cell chosen = df.rows[first ? members.front() : members.back()][c];
      for (std::size_t j = 0; j < members.size(); ++j) {
        const auto& cell = df.rows[first ? members[j] : members[members.size() - 1 - j]][c];
        if (!null_cell(cell)) {
          chosen = cell;
          break;
        }
      }
      row.push_back(chosen);
    }
    out.rows.push_back(std::move(row));
  }
  reset_index(out);
  return out;
}

std::vector<std::size_t> identity_order(std::size_t n) {
  std::vector<std::size_t> out(n);
  for (std::size_t i = 0; i < n; ++i) out[i] = i;
  return out;
}

}  // namespace

// ------------------------------------------------------------------ public API

EventTable pandas_filter_df_on_start_activities(const EventTable& df, const PandasFilterValue& values,
                                                const std::string& case_id_glue, const std::string& activity_key,
                                                const PandasFilterValue& positive) {
  return boundary_filter(df, values, case_id_glue, activity_key, positive, false);
}

EventTable pandas_filter_df_on_end_activities(const EventTable& df, const PandasFilterValue& values,
                                              const std::string& case_id_glue, const std::string& activity_key,
                                              const PandasFilterValue& positive) {
  return boundary_filter(df, values, case_id_glue, activity_key, positive, true);
}

EventTable pandas_start_activities_apply(const EventTable& df, const PandasFilterValue& values,
                                         const PandasFilterParameters& parameters) {
  return pandas_filter_df_on_start_activities(df, values, case_key(parameters), activity_key(parameters),
                                              parameter_or(parameters, "POSITIVE", "positive", Value::of(true)));
}

EventTable pandas_end_activities_apply(const EventTable& df, const PandasFilterValue& values,
                                       const PandasFilterParameters& parameters) {
  return pandas_filter_df_on_end_activities(df, values, case_key(parameters), activity_key(parameters),
                                            parameter_or(parameters, "POSITIVE", "positive", Value::of(true)));
}

EventTable pandas_filter_df_on_start_activities_nocc(const EventTable& df, const PandasFilterValue& nocc,
                                                     const std::optional<PandasFilterValue>& sa_count0,
                                                     const std::string& case_id_glue,
                                                     const std::string& activity_key) {
  return nocc_filter(df, nocc, sa_count0, case_id_glue, activity_key, false).table;
}

PandasEndActivitiesNocc pandas_filter_df_on_end_activities_nocc(const EventTable& df, const PandasFilterValue& nocc,
                                                                const std::optional<PandasFilterValue>& ea_count0,
                                                                const std::string& case_id_glue,
                                                                const std::string& activity_key, bool return_dict,
                                                                const std::optional<PandasFilterValue>& most_common_variant) {
  auto outcome = nocc_filter(df, nocc, ea_count0, case_id_glue, activity_key, true, most_common_variant);
  PandasEndActivitiesNocc out{std::move(outcome.table), std::nullopt};
  if (return_dict && !df.empty()) out.counts = counts_value(outcome.kept);
  return out;
}

EventTable pandas_trace_filter_apply(const EventTable& df, const PandasFilterValue& admitted_traces,
                                     const PandasFilterParameters& parameters) {
  return variant_filter(df, admitted_traces, parameters, VariantMode::kTrace);
}
EventTable pandas_starts_with_filter_apply(const EventTable& df, const PandasFilterValue& admitted_prefixes,
                                           const PandasFilterParameters& parameters) {
  return variant_filter(df, admitted_prefixes, parameters, VariantMode::kStartsWith);
}
EventTable pandas_ends_with_filter_apply(const EventTable& df, const PandasFilterValue& admitted_suffixes,
                                         const PandasFilterParameters& parameters) {
  return variant_filter(df, admitted_suffixes, parameters, VariantMode::kEndsWith);
}

EventTable pandas_rework_filter_apply(const EventTable& df, const PandasFilterValue& activity,
                                      const PandasFilterParameters& parameters) {
  const auto case_name = case_key(parameters);
  const auto act_name = activity_key(parameters);
  const auto minimum = parameter_or(parameters, "MIN_OCCURRENCES", "min_occurrences", Value::of(std::int64_t{2}));
  const bool positive = truthy(parameter_or(parameters, "POSITIVE", "positive", Value::of(true)));
  require_columns(df, {act_name, case_name});
  const auto bound = number_of(minimum);
  if (!bound) fail("TypeError", "unsupported operand type(s) for -: '" + type_name(minimum) + "' and 'int'");
  const Number threshold = bound->integral ? Number{true, bound->integer - 1, 0.0} : Number{false, 0, bound->real - 1};
  const auto case_column = *find_column(df, case_name);
  const auto act = *find_column(df, act_name);
  std::map<Key, std::int64_t> seen;
  std::set<Key> kept;
  for (std::size_t i = 0; i < df.size(); ++i) {
    const auto& c = df.rows[i][case_column];
    const auto& a = df.rows[i][act];
    if (null_cell(c) || null_cell(a) || activity.kind != Kind::kString || a->value != activity.text) continue;
    const auto key = key_of(*c);
    const auto occurrence = seen[key]++;
    if (int_at_least(occurrence, threshold)) kept.insert(key);
  }
  return keep_cases(df, case_column, kept, positive);
}

EventTable pandas_prefix_filter_apply(const EventTable& df, const PandasFilterValue& activity,
                                      const PandasFilterParameters& parameters) {
  return position_filter(df, activity, parameters, true);
}
EventTable pandas_suffix_filter_apply(const EventTable& df, const PandasFilterValue& activity,
                                      const PandasFilterParameters& parameters) {
  return position_filter(df, activity, parameters, false);
}

EventTable pandas_consecutive_act_case_grouping_apply(const EventTable& df, const PandasFilterParameters& parameters) {
  const auto case_column = require_column(df, case_key(parameters));
  const auto act = require_column(df, activity_key(parameters));
  const auto filter_type = parameter_or(parameters, "FILTER_TYPE", "filter_type", Value::of("first"));
  // The run key concatenates case id and activity; an int case id and a str
  // activity do not add.
  bool text_activity = false;
  for (const auto& row : df.rows) text_activity = text_activity || is_text(row[act]);
  for (const auto& row : df.rows)
    if (!null_cell(row[case_column]) && !is_text(row[case_column]) && text_activity)
      fail("TypeError", "unsupported operand type(s) for +: 'int' and 'str'");
  const bool first = filter_type.kind == Kind::kString && filter_type.text == "first";
  const bool last = filter_type.kind == Kind::kString && filter_type.text == "last";
  if (!first && !last) {
    EventTable out = df;
    reset_index(out);
    return out;
  }
  std::vector<std::vector<std::size_t>> runs;
  std::optional<std::string> previous;
  for (std::size_t i = 0; i < df.size(); ++i) {
    const auto& c = df.rows[i][case_column];
    const auto& a = df.rows[i][act];
    std::optional<std::string> key;
    if (!null_cell(c) && !null_cell(a)) key = py_str(*c) + a->value;
    if (key && previous && *key == *previous) runs.back().push_back(i);
    else runs.push_back({i});
    previous = key;
  }
  auto out = merge_groups(df, runs, first, identity_order(df.columns.size()));
  // In an object column a missing case id (None, NaN or pd.NA) comes back as
  // None, as the reference groups it; typed columns keep their own missing.
  bool missing_case = false;
  const bool object_case = case_column < df.dtypes.size() && df.dtypes[case_column] == "object";
  if (object_case) for (auto& row : out.rows)
    if (null_cell(row[case_column])) {
      EventAttribute none{df.columns[case_column], "", AttributeKind::kString};
      none.is_null = true;
      row[case_column] = std::move(none);
      missing_case = true;
    }
  if (missing_case && !out.dtypes.empty()) out.dtypes[case_column] = "object";
  return out;
}

EventTable pandas_timestamp_case_grouping_apply(const EventTable& df, const PandasFilterParameters& parameters) {
  const auto case_name = case_key(parameters);
  const auto timestamp_name =
      text_parameter(parameters, "TIMESTAMP_KEY", "pm4py:param:timestamp_key", "time:timestamp");
  const auto filter_type = parameter_or(parameters, "FILTER_TYPE", "filter_type", Value::of("first"));
  const bool first = filter_type.kind == Kind::kString && filter_type.text == "first";
  const bool last = filter_type.kind == Kind::kString && filter_type.text == "last";
  // pandas groups a two-row table by the key list itself when a key is not a
  // column: row i joins the group named by key i, and the group labels come
  // back as an "index" column.
  if (df.size() == 2 && (!find_column(df, case_name) || !find_column(df, timestamp_name))) {
    if (!first && !last) fail("AttributeError", "'NoneType' object has no attribute 'reset_index'");
    std::vector<std::vector<std::size_t>> groups{{0}};
    std::vector<std::string> labels{case_name};
    if (timestamp_name == case_name) {
      groups[0].push_back(1);
    } else {
      groups.push_back({1});
      labels.push_back(timestamp_name);
    }
    auto out = merge_groups(df, groups, first, identity_order(df.columns.size()));
    out.columns.insert(out.columns.begin(), "index");
    if (!out.dtypes.empty()) out.dtypes.insert(out.dtypes.begin(), "str");
    if (!out.categorical_dtypes.empty()) out.categorical_dtypes.insert(out.categorical_dtypes.begin(), {});
    for (std::size_t g = 0; g < out.rows.size(); ++g)
      out.rows[g].insert(out.rows[g].begin(), string_cell("index", labels[g]));
    return out;
  }
  const auto case_column = require_column(df, case_name);
  const auto timestamp = require_column(df, timestamp_name);
  if (!first && !last) fail("AttributeError", "'NoneType' object has no attribute 'reset_index'");
  std::vector<std::vector<std::size_t>> groups;
  std::map<std::pair<Key, std::string>, std::size_t> group_of;
  for (std::size_t i = 0; i < df.size(); ++i) {
    const auto& c = df.rows[i][case_column];
    const auto& t = df.rows[i][timestamp];
    if (null_cell(c) || null_cell(t)) continue;
    const auto [it, added] = group_of.emplace(std::make_pair(key_of(*c), t->value), groups.size());
    if (added) groups.emplace_back();
    groups[it->second].push_back(i);
  }
  std::vector<std::size_t> order{case_column, timestamp};
  for (std::size_t c = 0; c < df.columns.size(); ++c)
    if (c != case_column && c != timestamp) order.push_back(c);
  auto out = merge_groups(df, groups, first, order);
  // The group keys form a new column whose dtype pandas infers from the keys:
  // object ids 1 and True give int64, strings give str, a mix stays object.
  if (!out.dtypes.empty() && out.dtypes[0] == "object" && !out.rows.empty()) {
    bool integers = true, strings = true;
    for (const auto& row : out.rows) {
      const auto& cell = row[0];
      const bool integer = cell && !null_cell(cell) &&
          (std::holds_alternative<std::int64_t>(cell->typed_value) ||
           (cell->kind == AttributeKind::kInt && !cell->typed_value.index()));
      integers = integers && integer;
      strings = strings && is_text(cell);
    }
    if (integers) out.dtypes[0] = "int64";
    else if (strings) out.dtypes[0] = "str";
  }
  return out;
}

namespace {

// The reference sorts the case column as an object array (numpy's stable
// argsort, see numpy_object_sort.hpp). A str id
// compared with a null (NaN) one raises the TypeError of Python's "<", named
// by the operand order of the first such comparison.
void require_sortable_case_ids(const EventTable& df, std::size_t column) {
  bool any_null = false;
  bool any_text = false;
  for (const auto& row : df.rows) (null_cell(row[column]) ? any_null : any_text) = true;
  if (!any_null || !any_text) return;
  auto cmp = [&](std::size_t a, std::size_t b) {
    const auto& x = df.rows[a][column];
    const auto& y = df.rows[b][column];
    if (null_cell(x) != null_cell(y)) {
      throw PandasSequenceFilterError(
          "TypeError", std::string("'<' not supported between instances of '") +
                           (null_cell(x) ? "float" : "str") + "' and '" + (null_cell(x) ? "str" : "float") + "'");
    }
    if (null_cell(x)) return 0;
    return x->value < y->value ? -1 : y->value < x->value ? 1 : 0;
  };
  detail::numpy_object_stable_argsort(df.size(), cmp);
}

}  // namespace

EventTable pandas_activity_split_apply(const EventTable& df, const PandasFilterValue& activity,
                                       const PandasFilterParameters& parameters) {
  const auto case_column = require_column(df, case_key(parameters));
  const auto act = require_column(df, activity_key(parameters));
  const auto mode = parameter_or(parameters, "CUT_MODE", "cut_mode", Value::of("this"));
  const auto separator = text_parameter(parameters, "SUBCASE_CONCAT_STR", "subcase_concat_str", "##@@");
  const bool this_row = mode.kind == Kind::kString && mode.text == "this";
  require_sortable_case_ids(df, case_column);
  std::set<std::string> cuts;
  if (activity.kind == Kind::kString) cuts.insert(activity.text);
  for (const auto& item : activity.items) cuts.insert(item.text);
  EventTable out = df;
  if (!out.dtypes.empty()) out.dtypes[case_column] = df.empty() ? "float64" : "str";
  std::map<Key, std::int64_t> count;
  const auto& name = df.columns[case_column];
  for (std::size_t i = 0; i < df.size(); ++i) {
    const auto& c = df.rows[i][case_column];
    const auto& a = df.rows[i][act];
    const int hit = !null_cell(a) && cuts.count(a->value) ? 1 : 0;
    // A null case id (str "nan") is a group of its own row.
    std::int64_t single = 0;
    auto& n = null_cell(c) ? single : count[key_of(*c)];
    const auto before = n;
    n += hit;
    const auto id = null_cell(c) ? std::string("nan") : py_str(*c);
    out.rows[i][case_column] = string_cell(name, id + separator + std::to_string(this_row ? n : before));
  }
  return out;
}

}  // namespace minera
