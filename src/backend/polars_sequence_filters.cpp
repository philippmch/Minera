#include "minera/polars_sequence_filters.hpp"
#include "polars_column_hint.hpp"
#include "polars_sort.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <variant>

namespace minera {

namespace {

using Value = PolarsFilterValue;
using Kind = PolarsFilterValue::Kind;
using Cell = std::optional<EventAttribute>;

[[noreturn]] void fail(const std::string& type, const std::string& message) {
  throw PolarsSequenceFilterError(type, message);
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
bool at_least(const Number& a, const Number& b) {
  if (a.integral && b.integral) return a.integer >= b.integer;
  const double x = a.integral ? static_cast<double>(a.integer) : a.real;
  const double y = b.integral ? static_cast<double>(b.integer) : b.real;
  return x >= y;
}

const Value* parameter(const PolarsFilterParameters& parameters, std::string_view member, std::string_view literal) {
  if (!parameters) return nullptr;
  for (const auto& p : *parameters)
    if (p.enum_member && p.key == member) return &p.value;
  for (const auto& p : *parameters)
    if (!p.enum_member && p.key == literal) return &p.value;
  return nullptr;
}
Value parameter_or(const PolarsFilterParameters& parameters, std::string_view member, std::string_view literal,
                   Value fallback) {
  const auto* found = parameter(parameters, member, literal);
  return found ? *found : fallback;
}
std::string text_parameter(const PolarsFilterParameters& parameters, std::string_view member,
                           std::string_view literal, std::string_view fallback) {
  const auto* found = parameter(parameters, member, literal);
  return found ? found->text : std::string(fallback);
}
std::string case_key(const PolarsFilterParameters& p) {
  return text_parameter(p, "CASE_ID_KEY", kPolarsFilterCaseIdKeyParameter, kPolarsFilterCaseConceptName);
}
std::string activity_key(const PolarsFilterParameters& p) {
  return text_parameter(p, "ACTIVITY_KEY", kPolarsFilterActivityKeyParameter, kPolarsFilterDefaultNameKey);
}
bool positive_of(const PolarsFilterParameters& p) {
  return truthy(parameter_or(p, "POSITIVE", "positive", Value::of(true)));
}

// ------------------------------------------------------------------ plan errors

std::string quote(const std::string& v) { return '"' + v + '"'; }
std::string debug_quote(const std::string& v) {
  std::string result = "\"";
  for (const auto c : v) {
    switch (c) {
      case '\\': result += "\\\\"; break;
      case '"': result += "\\\""; break;
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      case '\0': result += "\\0"; break;
      default: result += c;
    }
  }
  return result + '"';
}
std::string frame_plan(const std::vector<std::string>& columns) {
  std::string out = "DF [";
  for (std::size_t i = 0; i < std::min<std::size_t>(4, columns.size()); ++i) {
    if (i) out += ", ";
    out += quote(columns[i]);
  }
  if (columns.size() > 4) out += ", ...";
  return out + "]; PROJECT */" + std::to_string(columns.size()) + " COLUMNS";
}
std::string contexts(const std::vector<std::string>& nodes) {
  std::string out = "\n\nThis error occurred with the following context stack:\n";
  for (std::size_t i = 0; i < nodes.size(); ++i) out += "\t[" + std::to_string(i + 1) + "] '" + nodes[i] + "'\n";
  return out;
}
std::string resolved(const std::string& node, const std::string& plan, const std::vector<std::string>& nodes) {
  return "\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING '" + node + "' <---\n" + plan + contexts(nodes);
}

// A plan node over the source frame: the failing node, the text above the
// source DF line, and the context stack.
struct Plan {
  std::string node;
  std::vector<std::string> stack;
  std::string above;  // printed before the indented source DF line
  std::vector<std::string> extra_columns;
  std::string source;  // the DF line when it is not the source frame
};

[[noreturn]] void column_not_found(const EventTable& df, const std::string& key, const Plan& plan) {
  auto valid = df.columns;
  valid.insert(valid.end(), plan.extra_columns.begin(), plan.extra_columns.end());
  std::string message = "unable to find column " + debug_quote(key) + "; valid columns: [";
  for (std::size_t i = 0; i < valid.size(); ++i) {
    if (i) message += ", ";
    message += debug_quote(valid[i]);
  }
  message += ']';
  if (const auto hint = detail::polars_column_hint(key, valid)) message += "\n\nDid you mean " + debug_quote(*hint) + '?';
  const auto source = plan.source.empty() ? frame_plan(df.columns) : plan.source;
  fail("ColumnNotFoundError", message + resolved(plan.node, plan.above.empty() ? source : plan.above + "\n  " + source,
                                                 plan.stack));
}

std::optional<std::size_t> find_column(const EventTable& df, const std::string& name) {
  for (std::size_t i = 0; i < df.columns.size(); ++i)
    if (df.columns[i] == name) return i;
  return std::nullopt;
}
std::size_t require(const EventTable& df, const std::string& name, const Plan& plan) {
  const auto found = find_column(df, name);
  if (!found) column_not_found(df, name, plan);
  return *found;
}

std::string dtype_of(const EventTable& df, std::size_t column) {
  return column < df.dtypes.size() ? df.dtypes[column] : std::string("String");
}
// The short dtype names of the join SchemaError.
std::string short_dtype(const std::string& dtype) {
  if (dtype == "String") return "str";
  if (dtype == "Int64") return "i64";
  if (dtype.rfind("Categorical", 0) == 0) return "cat";
  if (dtype == "Null") return "null";
  if (dtype == "Float64") return "f64";
  return dtype;
}
[[noreturn]] void join_dtype_error(const std::string& key, const std::string& left, const std::string& right) {
  fail("SchemaError", "datatypes of join keys don't match - `" + key + "`: " + left + " on left does not match `" + key +
                          "`: " + right + " on right (and no other type was available to cast to)" +
                          resolved("join", frame_plan({key}), {"join"}));
}

// ------------------------------------------------------------------ cells

bool null_cell(const Cell& cell) { return !cell || cell->is_null || is_native_na_attribute(*cell); }

// A case id (or other grouping value): null, or its value text.
struct Key {
  bool null = true;
  std::string text;
  bool operator==(const Key&) const = default;
  bool operator<(const Key& o) const { return null != o.null ? null : text < o.text; }
};
Key key_of(const Cell& cell) { return null_cell(cell) ? Key{} : Key{false, cell->value}; }

std::int64_t integer_of(const EventAttribute& cell) {
  if (const auto* i = std::get_if<std::int64_t>(&cell.typed_value)) return *i;
  if (const auto* u = std::get_if<std::uint64_t>(&cell.typed_value)) return static_cast<std::int64_t>(*u);
  return std::stoll(cell.value);
}

// Polars sort order of one column's cells: nulls first, integers by value,
// everything else by its UTF-8 text.
bool cell_less(const Cell& a, const Cell& b, bool integral) {
  const bool an = null_cell(a), bn = null_cell(b);
  if (an || bn) return an && !bn;
  if (integral) return integer_of(*a) < integer_of(*b);
  return a->value < b->value;
}

EventAttribute string_cell(const std::string& column, std::string value) {
  EventAttribute out;
  out.key = column;
  out.value = std::move(value);
  return out;
}
EventAttribute null_cell_of(const std::string& column) {
  EventAttribute out;
  out.key = column;
  out.is_null = true;
  return out;
}
EventAttribute int_cell(const std::string& column, std::int64_t value) {
  EventAttribute out;
  out.key = column;
  out.value = std::to_string(value);
  out.kind = AttributeKind::kInt;
  out.typed_value = value;
  return out;
}

EventTable subset(const EventTable& df, const std::vector<bool>& keep) {
  EventTable out = df;
  out.rows.clear();
  for (std::size_t i = 0; i < df.size(); ++i)
    if (keep[i]) out.rows.push_back(df.rows[i]);
  return out;
}

// df.join(right, how="inner") (positive) or how="anti" with a frame of case
// ids (`right`, in its row order; a null case id never matches). An inner
// join no taller than its right side comes in the right side's order, each
// right row followed by its matching left rows.
EventTable join_cases(const EventTable& df, std::size_t case_column, const std::vector<Key>& right, bool positive) {
  const std::set<Key> cases(right.begin(), right.end());
  if (positive && df.size() <= right.size()) {
    EventTable out = df;
    out.rows.clear();
    for (const auto& key : right) {
      if (key.null) continue;
      for (const auto& row : df.rows)
        if (key_of(row[case_column]) == key) out.rows.push_back(row);
    }
    return out;
  }
  std::vector<bool> keep(df.size());
  for (std::size_t i = 0; i < df.size(); ++i) {
    const auto key = key_of(df.rows[i][case_column]);
    const bool in = !key.null && cases.count(key);
    keep[i] = positive ? in : !in;
  }
  return subset(df, keep);
}

// Case groups (the null case id included) in first-appearance order.
struct Groups {
  std::vector<Key> order;
  std::map<Key, std::vector<std::size_t>> rows;
};
Groups groups_of(const EventTable& df, std::size_t case_column, const std::vector<std::size_t>& rows) {
  Groups out;
  for (const auto i : rows) {
    const auto key = key_of(df.rows[i][case_column]);
    auto [it, added] = out.rows.emplace(key, std::vector<std::size_t>{});
    if (added) out.order.push_back(key);
    it->second.push_back(i);
  }
  return out;
}
std::vector<std::size_t> all_rows(const EventTable& df) {
  std::vector<std::size_t> out(df.size());
  for (std::size_t i = 0; i < out.size(); ++i) out[i] = i;
  return out;
}

// is_in over a list of Python values: a null activity never matches.
bool member(const Cell& cell, const std::vector<Value>& values) {
  if (null_cell(cell)) return false;
  for (const auto& v : values)
    if (v.kind == Kind::kString && v.text == cell->value) return true;
  return false;
}

// ------------------------------------------------------------------ boundaries

const Plan kBoundaryPlan{"join", {"group_by", "filter", "select", "join right", "join"}, {}, {}};

EventTable boundary_filter(const EventTable& df, const Value& values, const std::string& case_glue,
                           const std::string& act_key, bool positive, bool end) {
  const auto case_column = require(df, case_glue, kBoundaryPlan);
  const auto act = require(df, act_key, kBoundaryPlan);
  const auto groups = groups_of(df, case_column, all_rows(df));
  std::vector<Key> kept;
  for (const auto& key : groups.order) {
    const auto& rows = groups.rows.at(key);
    if (member(df.rows[end ? rows.back() : rows.front()][act], values.items)) kept.push_back(key);
  }
  return join_cases(df, case_column, kept, positive);
}

// End activity counts: (activity or null, count) by first appearance.
using Counts = std::vector<std::pair<Value, Value>>;
const Plan kEndCountPlan{"group_by", {"group_by", "group_by"}, {}, {}};

Counts end_counts(const EventTable& df, const std::string& case_glue, const std::string& act_key) {
  const auto case_column = require(df, case_glue, kEndCountPlan);
  const auto act = require(df, act_key, kEndCountPlan);
  const auto groups = groups_of(df, case_column, all_rows(df));
  Counts out;
  for (const auto& key : groups.order) {
    const auto& cell = df.rows[groups.rows.at(key).back()][act];
    const auto activity = null_cell(cell) ? Value::none() : Value::of(cell->value);
    bool found = false;
    for (auto& [k, v] : out)
      if (k.kind == activity.kind && k.text == activity.text) {
        ++v.integer;
        found = true;
        break;
      }
    if (!found) out.emplace_back(activity, Value::of(std::int64_t{1}));
  }
  return out;
}

// ------------------------------------------------------------------ variants

const std::string kTimestamp(kPolarsFilterDefaultTimestampKey);

// Rows sorted by case and timestamp (Polars' unstable sort, nulls first),
// grouped by case.
struct Variants {
  std::size_t case_column = 0, act = 0;
  std::vector<Key> order;                              // cases in sorted order
  std::vector<Cell> case_cells;                        // one per case
  std::vector<std::vector<Cell>> activities;           // per case, sorted
};
Variants variants_of(const EventTable& df, const std::string& case_glue, const std::string& act_key,
                     const Plan& sort_plan, const Plan& agg_plan) {
  Variants out;
  out.case_column = require(df, case_glue, sort_plan);
  const auto ts = require(df, kTimestamp, sort_plan);
  out.act = find_column(df, act_key).value_or(df.columns.size());
  if (out.act == df.columns.size()) {
    auto plan = agg_plan;
    plan.above = "SORT BY [col(" + quote(case_glue) + "), col(" + quote(kTimestamp) + ")]";
    column_not_found(df, act_key, plan);
  }
  const bool integral = dtype_of(df, out.case_column) == "Int64";
  auto rows = all_rows(df);
  // Polars' unstable multi-column sort leaves equal rows in its own order (round BX).
  polars_sort::sort_by_keys(rows, [&](std::size_t a, std::size_t b) {
    const auto& ca = df.rows[a][out.case_column];
    const auto& cb = df.rows[b][out.case_column];
    if (cell_less(ca, cb, integral)) return true;
    if (cell_less(cb, ca, integral)) return false;
    return cell_less(df.rows[a][ts], df.rows[b][ts], false);
  }, dtype_of(df, out.case_column), dtype_of(df, ts));
  for (const auto i : rows) {
    const auto key = key_of(df.rows[i][out.case_column]);
    if (out.order.empty() || !(out.order.back() == key)) {
      out.order.push_back(key);
      out.case_cells.push_back(df.rows[i][out.case_column]);
      out.activities.emplace_back();
    }
    out.activities.back().push_back(df.rows[i][out.act]);
  }
  return out;
}

std::string variant_string(const std::vector<Cell>& activities) {
  std::string out;
  bool first = true;
  for (const auto& cell : activities) {
    if (null_cell(cell)) continue;
    if (!first) out += ",";
    out += cell->value;
    first = false;
  }
  return out;
}

// The characters of a Python str (UTF-8 sequences).
std::vector<std::string> characters(const std::string& text) {
  std::vector<std::string> out;
  for (std::size_t i = 0; i < text.size();) {
    std::size_t len = 1;
    const auto c = static_cast<unsigned char>(text[i]);
    if (c >= 0xF0) len = 4;
    else if (c >= 0xE0) len = 3;
    else if (c >= 0xC0) len = 2;
    out.push_back(text.substr(i, len));
    i += len;
  }
  return out;
}
// The activities of an admitted item: a str iterates over its characters.
std::vector<std::string> pattern_items(const Value& item) {
  if (item.kind == Kind::kString) return characters(item.text);
  std::vector<std::string> out;
  for (const auto& v : item.items) out.push_back(v.text);
  return out;
}
std::string join_pattern(const Value& item) {
  std::string out;
  const auto items = pattern_items(item);
  for (std::size_t i = 0; i < items.size(); ++i) out += (i ? "," : "") + items[i];
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

const Plan kTraceSortPlan{"with_columns", {"sort", "group_by", "with_columns"}, {}, {}};
const Plan kTraceAggPlan{"with_columns", {"group_by", "with_columns"}, {}, {}};
const Plan kSuffixSortPlan{"group_by", {"sort", "group_by"}, {}, {}};
const Plan kSuffixAggPlan{"group_by", {"group_by"}, {}, {}};
const Plan kLazySortPlan{"join", {"sort", "group_by", "filter", "select", "join right", "join"}, {}, {}};
const Plan kLazyAggPlan{"join", {"group_by", "filter", "select", "join right", "join"}, {}, {}};

EventTable trace_variants_frame(const EventTable& df, const Variants& v) {
  EventTable out;
  const auto& case_name = df.columns[v.case_column];
  out.columns = {case_name, "variant_list", "count", "variant"};
  out.dtypes = {dtype_of(df, v.case_column), "List(String)", "UInt32", "String"};
  for (std::size_t c = 0; c < v.order.size(); ++c) {
    EventAttribute list;
    list.key = "variant_list";
    list.container = AttributeContainerKind::kList;
    for (const auto& cell : v.activities[c]) list.children.push_back(null_cell(cell) ? null_cell_of("") : string_cell("", cell->value));
    out.rows.push_back({v.case_cells[c], list, int_cell("count", static_cast<std::int64_t>(v.activities[c].size())),
                        string_cell("variant", variant_string(v.activities[c]))});
  }
  return out;
}

// Matching case ids joined back as a Python list: its dtype comes from the
// list's values, so the join needs the source dtype to agree.
EventTable join_case_list(const EventTable& df, std::size_t case_column, const std::vector<Key>& matched,
                          bool positive) {
  if (matched.empty()) return positive ? subset(df, std::vector<bool>(df.size(), false)) : df;
  const auto left = short_dtype(dtype_of(df, case_column));
  bool any = false;
  for (const auto& k : matched) any = any || !k.null;
  const std::string right = !any ? "null" : dtype_of(df, case_column) == "Int64" ? "i64" : "str";
  if (left != right) join_dtype_error(df.columns[case_column], left, right);
  return join_cases(df, case_column, matched, positive);
}

// A lazy variants filter over the sorted, grouped rows.
template <typename Predicate>
EventTable lazy_variant_filter(const EventTable& df, const PolarsFilterParameters& parameters, Predicate&& matches) {
  const auto v = variants_of(df, case_key(parameters), activity_key(parameters), kLazySortPlan, kLazyAggPlan);
  std::vector<Key> kept;
  for (std::size_t c = 0; c < v.order.size(); ++c)
    if (matches(v.activities[c])) kept.push_back(v.order[c]);
  return join_cases(df, v.case_column, kept, positive_of(parameters));
}

// ------------------------------------------------------------------ rework

const Plan kReworkPlan{"join", {"group_by", "filter", "select", "unique", "join right", "join"}, {}, {}};

std::string is_in_literal(const Value& activities) {
  std::string out = "[[";
  for (std::size_t i = 0; i < activities.items.size(); ++i) {
    if (i) out += ", ";
    out += quote(activities.items[i].text);
  }
  return out + "]]";
}

EventTable rework(const EventTable& df, const PolarsFilterParameters& parameters, const Value* activities) {
  const auto case_glue = case_key(parameters);
  const auto act_key = activity_key(parameters);
  auto plan = kReworkPlan;
  std::size_t act = 0;
  if (activities) {
    act = require(df, act_key, kReworkPlan);
    plan.above = (activities->items.empty() ? "FILTER false.alias(" + quote(act_key) + ")"
                                            : "FILTER col(" + quote(act_key) + ").is_in(" + is_in_literal(*activities) + ")") +
                 "\nFROM";
  }
  const auto case_column = require(df, case_glue, plan);
  if (!activities) act = require(df, act_key, plan);
  const auto minimum = parameter_or(parameters, "MIN_OCCURRENCES", "min_occurrences", Value::of(std::int64_t{2}));
  const auto threshold = number_of(minimum);
  std::vector<std::size_t> rows;
  for (std::size_t i = 0; i < df.size(); ++i)
    if (!activities || member(df.rows[i][act], activities->items)) rows.push_back(i);
  std::map<std::pair<Key, Key>, std::int64_t> counts;
  for (const auto i : rows) ++counts[{key_of(df.rows[i][case_column]), key_of(df.rows[i][act])}];
  std::vector<Key> kept;
  if (threshold)
    for (const auto& [key, count] : counts)
      if (at_least(Number{true, count, 0.0}, *threshold) &&
          std::find(kept.begin(), kept.end(), key.first) == kept.end())
        kept.push_back(key.first);
  return join_cases(df, case_column, kept, true);
}

// ------------------------------------------------------------------ grouping

std::string filter_type(const PolarsFilterParameters& parameters) {
  const auto value = parameter_or(parameters, "FILTER_TYPE", "filter_type", Value::of("first"));
  return value.kind == Kind::kString ? value.text : std::string();
}

}  // namespace

// ------------------------------------------------------------------ public

EventTable polars_start_activities_apply(const EventTable& df, const PolarsFilterValue& values,
                                         const PolarsFilterParameters& parameters) {
  return boundary_filter(df, values, case_key(parameters), activity_key(parameters), positive_of(parameters), false);
}
EventTable polars_end_activities_apply(const EventTable& df, const PolarsFilterValue& values,
                                       const PolarsFilterParameters& parameters) {
  return boundary_filter(df, values, case_key(parameters), activity_key(parameters), positive_of(parameters), true);
}
EventTable polars_filter_df_on_end_activities(const EventTable& df, const PolarsFilterValue& values,
                                              const std::string& case_id_glue, const std::string& activity_key,
                                              const PolarsFilterValue& positive) {
  return boundary_filter(df, values, case_id_glue, activity_key, truthy(positive), true);
}

PolarsFilterValue polars_get_end_activities(const EventTable& df, const std::string& case_id_key,
                                            const std::string& activity_key) {
  return Value::dict(end_counts(df, case_id_key, activity_key));
}

PolarsEndActivitiesNocc polars_filter_df_on_end_activities_nocc(
    const EventTable& df, const PolarsFilterValue& nocc, const std::optional<PolarsFilterValue>& ea_count0,
    const std::string& case_id_glue, const std::string& activity_key, bool return_dict,
    const std::optional<PolarsFilterValue>& most_common_variant) {
  const Counts counts = ea_count0 ? ea_count0->entries : end_counts(df, case_id_glue, activity_key);
  const auto threshold = number_of(nocc);
  if (!threshold) fail("TypeError", "'>=' not supported between instances of 'int' and 'NoneType'");
  const Value* protected_end = most_common_variant && truthy(*most_common_variant) ? &most_common_variant->items.back()
                                                                                    : nullptr;
  Counts kept;
  for (const auto& entry : counts) {
    const auto count = number_of(entry.second);
    const bool ok = count && at_least(*count, *threshold);
    const bool keep_end = protected_end && entry.first.kind == protected_end->kind &&
                          entry.first.text == protected_end->text;
    if (ok || keep_end) kept.push_back(entry);
  }
  PolarsEndActivitiesNocc out;
  if (kept.size() == counts.size()) {
    out.table = df;
  } else {
    std::vector<Value> values;
    for (const auto& entry : kept) values.push_back(entry.first);
    out.table = boundary_filter(df, Value::list(values), case_id_glue, activity_key, true, true);
  }
  if (return_dict) out.counts = Value::dict(kept);
  return out;
}

EventTable polars_trace_filter_get_variants_df(const EventTable& df, const std::string& case_id_key,
                                               const std::string& activity_key) {
  return trace_variants_frame(df, variants_of(df, case_id_key, activity_key, kTraceSortPlan, kTraceAggPlan));
}

EventTable polars_ends_with_filter_get_variants_df(const EventTable& df, const std::string& case_id_key,
                                                   const std::string& activity_key) {
  const auto v = variants_of(df, case_id_key, activity_key, kSuffixSortPlan, kSuffixAggPlan);
  EventTable out;
  out.columns = {df.columns[v.case_column], "variant"};
  out.dtypes = {dtype_of(df, v.case_column), "String"};
  for (std::size_t c = 0; c < v.order.size(); ++c)
    out.rows.push_back({v.case_cells[c], string_cell("variant", variant_string(v.activities[c]))});
  return out;
}

EventTable polars_trace_filter_apply(const EventTable& df, const PolarsFilterValue& admitted_traces,
                                     const PolarsFilterParameters& parameters) {
  const auto v = variants_of(df, case_key(parameters), activity_key(parameters), kTraceSortPlan, kTraceAggPlan);
  std::vector<std::string> patterns;
  for (const auto& item : admitted_traces.items) patterns.push_back(join_pattern(item));
  std::vector<Key> matched;
  for (std::size_t c = 0; c < v.order.size(); ++c) {
    const auto variant = variant_string(v.activities[c]);
    bool ok = patterns.empty();
    for (const auto& p : patterns) ok = ok || trace_match(variant, p);
    if (ok) matched.push_back(v.order[c]);
  }
  return join_case_list(df, v.case_column, matched, positive_of(parameters));
}

EventTable polars_starts_with_filter_apply(const EventTable& df, const PolarsFilterValue& prefixes,
                                           const PolarsFilterParameters& parameters) {
  if (prefixes.items.empty()) fail("TypeError", "invalid predicate for `filter`: None");
  std::vector<std::string> patterns;
  for (const auto& item : prefixes.items) patterns.push_back(join_pattern(item));
  return lazy_variant_filter(df, parameters, [&](const std::vector<Cell>& activities) {
    const auto variant = variant_string(activities);
    for (const auto& p : patterns)
      if (variant.starts_with(p)) return true;
    return false;
  });
}

EventTable polars_ends_with_filter_apply(const EventTable& df, const PolarsFilterValue& admitted_suffixes,
                                         const PolarsFilterParameters& parameters) {
  const auto v = variants_of(df, case_key(parameters), activity_key(parameters), kSuffixSortPlan, kSuffixAggPlan);
  if (admitted_suffixes.items.empty()) fail("IndexError", "list index out of range");
  const bool joined = admitted_suffixes.items.front().kind != Kind::kString;
  std::vector<std::string> suffixes;
  for (const auto& item : admitted_suffixes.items) {
    // A str first item keeps the items as they are: the first list among
    // them (its inner dtype Null when empty) cannot become a str literal.
    if (!joined && item.kind != Kind::kString)
      fail("InvalidOperationError", std::string("cannot cast List type (inner: '") +
                                        (item.items.empty() ? "Null" : "String") + "', to: 'String')");
    suffixes.push_back(joined ? join_pattern(item) : item.text);
  }
  std::vector<Key> kept;
  for (std::size_t c = 0; c < v.order.size(); ++c) {
    const auto variant = variant_string(v.activities[c]);
    for (const auto& s : suffixes)
      if (variant.ends_with(s)) {
        kept.push_back(v.order[c]);
        break;
      }
  }
  return join_cases(df, v.case_column, kept, positive_of(parameters));
}

namespace {
EventTable list_filter(const EventTable& df, const PolarsFilterValue& patterns,
                       const PolarsFilterParameters& parameters, bool tail) {
  if (patterns.items.empty()) {
    const auto case_glue = case_key(parameters);
    const auto column = require(df, case_glue, Plan{"join", {"join"}, {}, {}, frame_plan({case_glue})});
    join_dtype_error(case_glue, short_dtype(dtype_of(df, column)), "null");
  }
  std::vector<std::vector<std::string>> lists;
  for (const auto& item : patterns.items) lists.push_back(pattern_items(item));
  return lazy_variant_filter(df, parameters, [&](const std::vector<Cell>& activities) {
    for (const auto& p : lists) {
      if (p.size() > activities.size()) continue;
      const std::size_t offset = tail ? activities.size() - p.size() : 0;
      bool same = true;
      for (std::size_t i = 0; i < p.size() && same; ++i) {
        const auto& cell = activities[offset + i];
        same = !null_cell(cell) && cell->value == p[i];
      }
      if (same) return true;
    }
    return false;
  });
}
}  // namespace

EventTable polars_prefix_filter_apply(const EventTable& df, const PolarsFilterValue& prefixes,
                                      const PolarsFilterParameters& parameters) {
  return list_filter(df, prefixes, parameters, false);
}
EventTable polars_suffix_filter_apply(const EventTable& df, const PolarsFilterValue& suffixes,
                                      const PolarsFilterParameters& parameters) {
  return list_filter(df, suffixes, parameters, true);
}

EventTable polars_rework_filter_apply(const EventTable& df, const PolarsFilterParameters& parameters) {
  return rework(df, parameters, nullptr);
}
EventTable polars_rework_filter_apply_activity_set(const EventTable& df, const PolarsFilterValue& activities,
                                                   const PolarsFilterParameters& parameters) {
  return rework(df, parameters, &activities);
}

EventTable polars_consecutive_act_case_grouping_apply(const EventTable& df, const PolarsFilterParameters& parameters) {
  const auto type = filter_type(parameters);
  const bool grouped = type == "first" || type == "last";
  const Plan plan{"select",
                  grouped ? std::vector<std::string>{"with_columns", "group_by", "select", "select"}
                          : std::vector<std::string>{"with_columns", "select"},
                  " WITH_COLUMNS:\n [dyn int: 1.cum_sum().alias(\"_row_num\")] ",
                  {"_row_num"}};
  const auto case_column = require(df, case_key(parameters), plan);
  const auto act = require(df, activity_key(parameters), plan);
  if (!grouped) return df;
  EventTable out = df;
  out.rows.clear();
  std::size_t start = 0;
  for (std::size_t i = 1; i <= df.size(); ++i) {
    bool same = false;
    if (i < df.size()) {
      const auto &c0 = df.rows[i - 1][case_column], &c1 = df.rows[i][case_column];
      const auto &a0 = df.rows[i - 1][act], &a1 = df.rows[i][act];
      same = !null_cell(c0) && !null_cell(c1) && c0->value == c1->value && !null_cell(a0) && !null_cell(a1) &&
             a0->value == a1->value;
    }
    if (same) continue;
    out.rows.push_back(df.rows[type == "first" ? start : i - 1]);
    start = i;
  }
  return out;
}

EventTable polars_timestamp_case_grouping_apply(const EventTable& df, const PolarsFilterParameters& parameters) {
  const auto type = filter_type(parameters);
  if (type != "first" && type != "last") return df;
  const Plan plan{"group_by", {"group_by"}, {}, {}};
  const auto case_column = require(df, case_key(parameters), plan);
  const auto ts = require(df, text_parameter(parameters, "TIMESTAMP_KEY", kPolarsFilterTimestampKeyParameter,
                                             kPolarsFilterDefaultTimestampKey),
                          plan);
  if (type != "first" && type != "last") return df;
  std::vector<std::size_t> order{case_column, ts};
  for (std::size_t c = 0; c < df.columns.size(); ++c)
    if (c != case_column && c != ts) order.push_back(c);
  std::vector<std::pair<Key, Key>> groups;
  std::map<std::pair<Key, Key>, std::vector<std::size_t>> rows;
  for (std::size_t i = 0; i < df.size(); ++i) {
    const std::pair<Key, Key> key{key_of(df.rows[i][case_column]), key_of(df.rows[i][ts])};
    auto [it, added] = rows.emplace(key, std::vector<std::size_t>{});
    if (added) groups.push_back(key);
    it->second.push_back(i);
  }
  EventTable out = df;
  out.columns.clear();
  out.dtypes.clear();
  out.categorical_dtypes.clear();
  out.rows.clear();
  for (const auto c : order) {
    out.columns.push_back(df.columns[c]);
    if (!df.dtypes.empty()) out.dtypes.push_back(df.dtypes[c]);
    if (!df.categorical_dtypes.empty()) out.categorical_dtypes.push_back(df.categorical_dtypes[c]);
  }
  for (const auto& key : groups) {
    const auto& members = rows.at(key);
    const auto& source = df.rows[type == "first" ? members.front() : members.back()];
    EventTable::Row row;
    for (const auto c : order) row.push_back(source[c]);
    out.rows.push_back(std::move(row));
  }
  return out;
}

EventTable polars_activity_split_apply(const EventTable& df, const PolarsFilterValue& activity,
                                       const PolarsFilterParameters& parameters) {
  const auto case_glue = case_key(parameters);
  const auto act_key = activity_key(parameters);
  const auto mode = parameter_or(parameters, "CUT_MODE", "cut_mode", Value::of("this"));
  const bool this_mode = mode.kind == Kind::kString && mode.text == "this";
  const auto concat = parameter_or(parameters, "SUBCASE_CONCAT_STR", "subcase_concat_str", Value::of("##@@"));
  const auto case_column = require(
      df, case_glue,
      Plan{"select",
           this_mode ? std::vector<std::string>{"with_columns", "with_columns", "with_columns", "with_columns", "select"}
                     : std::vector<std::string>{"with_columns", "with_columns", "with_columns", "with_columns",
                                                "select", "with_columns", "select"},
           {},
           {}});
  const auto act = require(
      df, act_key,
      Plan{"select",
           this_mode ? std::vector<std::string>{"with_columns", "with_columns", "with_columns", "select"}
                     : std::vector<std::string>{"with_columns", "with_columns", "with_columns", "select", "with_columns",
                                                "select"},
           " WITH_COLUMNS:\n [col(" + quote(case_glue) + ").cum_count().over([col(" + quote(case_glue) +
               ")]).alias(\"_row_within_case\")] ",
           {"_row_within_case"}});
  const std::vector<Value> activities = activity.kind == Kind::kString ? std::vector<Value>{activity} : activity.items;
  // Per case: the running count ("this"), null on a null activity.
  std::map<Key, std::int64_t> running;
  std::map<Key, std::optional<std::int64_t>> previous;  // the previous row's running count
  std::map<Key, bool> seen;
  EventTable out = df;
  if (!out.dtypes.empty()) out.dtypes[case_column] = "String";
  if (!out.categorical_dtypes.empty()) out.categorical_dtypes[case_column] = std::nullopt;
  for (std::size_t i = 0; i < df.size(); ++i) {
    const auto& case_cell = df.rows[i][case_column];
    const auto key = key_of(case_cell);
    const auto& a = df.rows[i][act];
    std::optional<std::int64_t> current;
    if (!null_cell(a)) {
      running[key] += member(a, activities) ? 1 : 0;
      current = running[key];
    }
    std::optional<std::int64_t> counter;
    if (this_mode) counter = current;
    else counter = seen[key] ? previous[key] : std::optional<std::int64_t>(0);
    seen[key] = true;
    previous[key] = current;
    auto& cell = out.rows[i][case_column];
    if (key.null || !counter || concat.kind != Kind::kString) cell = null_cell_of(case_glue);
    else cell = string_cell(case_glue, case_cell->value + concat.text + std::to_string(*counter));
  }
  return out;
}

// ------------------------------------------------------------------ lazy

namespace {
template <typename Function>
LazyEventTable lazily(const LazyEventTable& df, Function&& function) {
  return LazyEventTable::from_table(function(df.materialize()));
}
}  // namespace

LazyEventTable polars_start_activities_apply(const LazyEventTable& df, const PolarsFilterValue& values,
                                             const PolarsFilterParameters& parameters) {
  return lazily(df, [&](const EventTable& t) { return polars_start_activities_apply(t, values, parameters); });
}
LazyEventTable polars_end_activities_apply(const LazyEventTable& df, const PolarsFilterValue& values,
                                           const PolarsFilterParameters& parameters) {
  return lazily(df, [&](const EventTable& t) { return polars_end_activities_apply(t, values, parameters); });
}
LazyEventTable polars_trace_filter_apply(const LazyEventTable& df, const PolarsFilterValue& admitted_traces,
                                         const PolarsFilterParameters& parameters) {
  return lazily(df, [&](const EventTable& t) { return polars_trace_filter_apply(t, admitted_traces, parameters); });
}
LazyEventTable polars_starts_with_filter_apply(const LazyEventTable& df, const PolarsFilterValue& prefixes,
                                               const PolarsFilterParameters& parameters) {
  return lazily(df, [&](const EventTable& t) { return polars_starts_with_filter_apply(t, prefixes, parameters); });
}
LazyEventTable polars_ends_with_filter_apply(const LazyEventTable& df, const PolarsFilterValue& admitted_suffixes,
                                             const PolarsFilterParameters& parameters) {
  return lazily(df, [&](const EventTable& t) { return polars_ends_with_filter_apply(t, admitted_suffixes, parameters); });
}
LazyEventTable polars_prefix_filter_apply(const LazyEventTable& df, const PolarsFilterValue& prefixes,
                                          const PolarsFilterParameters& parameters) {
  return lazily(df, [&](const EventTable& t) { return polars_prefix_filter_apply(t, prefixes, parameters); });
}
LazyEventTable polars_suffix_filter_apply(const LazyEventTable& df, const PolarsFilterValue& suffixes,
                                          const PolarsFilterParameters& parameters) {
  return lazily(df, [&](const EventTable& t) { return polars_suffix_filter_apply(t, suffixes, parameters); });
}
LazyEventTable polars_rework_filter_apply(const LazyEventTable& df, const PolarsFilterParameters& parameters) {
  return lazily(df, [&](const EventTable& t) { return polars_rework_filter_apply(t, parameters); });
}
LazyEventTable polars_consecutive_act_case_grouping_apply(const LazyEventTable& df,
                                                          const PolarsFilterParameters& parameters) {
  return lazily(df, [&](const EventTable& t) { return polars_consecutive_act_case_grouping_apply(t, parameters); });
}
LazyEventTable polars_timestamp_case_grouping_apply(const LazyEventTable& df, const PolarsFilterParameters& parameters) {
  return lazily(df, [&](const EventTable& t) { return polars_timestamp_case_grouping_apply(t, parameters); });
}
LazyEventTable polars_activity_split_apply(const LazyEventTable& df, const PolarsFilterValue& activity,
                                           const PolarsFilterParameters& parameters) {
  return lazily(df, [&](const EventTable& t) { return polars_activity_split_apply(t, activity, parameters); });
}

}  // namespace minera
