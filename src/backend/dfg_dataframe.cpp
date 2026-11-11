#include "minera/dfg_dataframe.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "minera/timestamp.hpp"
#include "business_hours.hpp"
#include "full_double.hpp"
#include "performance_arithmetic.hpp"

namespace minera {
namespace {

constexpr double kDaySeconds = 86400.0;
constexpr double kWeekSeconds = 7.0 * kDaySeconds;

enum class SelectorRole {
  kCase,
  kActivity,
  kTimestamp,
  kResource,
  kLifecycle,
  kGeneric,
};

struct Selector {
  std::string name;
  std::optional<std::size_t> column;
  SelectorRole role = SelectorRole::kGeneric;
};

struct TableRow {
  EventTable::Row cells;
  std::size_t original_position = 0;
  Event event;
  std::optional<std::string> case_value;
  std::optional<std::string> activity_value;
  std::optional<std::string> target_activity_value;
  std::optional<std::string> completion_text;
  std::optional<std::string> start_text;
  std::optional<std::string> cost_text;
  std::optional<std::string> index_text;
  std::optional<EventAttribute> index_attribute;
  bool index_is_generated = true;
  std::optional<double> completion;
  std::optional<double> start;
  // Exact instants: ordering and Polars durations must not collapse
  // neighbouring nanoseconds through a double.
  std::optional<ParsedExactTimestamp> completion_exact;
  std::optional<ParsedExactTimestamp> start_exact;
  std::optional<double> cost;
  // This is assigned after adapter sorting. It is the fallback index used by
  // both pair-producing adapters when no source index column is available.
  std::size_t generated_index = 0;
};

std::string selector_error(std::string_view what, std::string_view selector) {
  return "DFG table discovery: missing or invalid " + std::string(what) +
         " selector '" + std::string(selector) + "'";
}

bool is_alias(std::string_view value,
              std::initializer_list<std::string_view> aliases) {
  return std::find(aliases.begin(), aliases.end(), value) != aliases.end();
}

std::optional<SelectorRole> role_for_selector(std::string_view selector) {
  if (is_alias(selector, {"case:concept:name", "case_id", "case"}))
    return SelectorRole::kCase;
  if (is_alias(selector, {"concept:name", "activity"}))
    return SelectorRole::kActivity;
  if (is_alias(selector, {"time:timestamp", "timestamp"}))
    return SelectorRole::kTimestamp;
  if (is_alias(selector, {"org:resource", "resource"}))
    return SelectorRole::kResource;
  if (is_alias(selector, {"lifecycle:transition", "lifecycle"}))
    return SelectorRole::kLifecycle;
  return std::nullopt;
}

FirstClassAttributeRole first_class_role_for_column(std::string_view column) {
  if (is_alias(column, {"case:concept:name", "case_id", "case"}))
    return FirstClassAttributeRole::kCaseId;
  if (is_alias(column, {"concept:name", "activity"}))
    return FirstClassAttributeRole::kActivity;
  if (is_alias(column, {"time:timestamp", "timestamp"}))
    return FirstClassAttributeRole::kTimestamp;
  if (is_alias(column, {"org:resource", "resource"}))
    return FirstClassAttributeRole::kResource;
  if (is_alias(column, {"lifecycle:transition", "lifecycle"}))
    return FirstClassAttributeRole::kLifecycle;
  return FirstClassAttributeRole::kNone;
}

std::optional<std::size_t> exact_column(const EventTable& table,
                                        std::string_view name) {
  const auto found = std::find(table.columns.begin(), table.columns.end(), name);
  if (found == table.columns.end()) return std::nullopt;
  return static_cast<std::size_t>(found - table.columns.begin());
}

std::optional<std::size_t> alias_column(const EventTable& table,
                                        std::string_view selector,
                                        SelectorRole role) {
  if (const auto exact = exact_column(table, selector)) return exact;
  // A caller-provided custom name is not interchangeable with a standard
  // alias.  Only empty selectors and recognized first-class spellings may
  // fall through to the alias set; this keeps invalid selectors observable.
  if (!selector.empty() && !role_for_selector(selector)) return std::nullopt;
  std::vector<std::string_view> aliases;
  switch (role) {
    case SelectorRole::kCase:
      aliases = {"case:concept:name", "case_id", "case"};
      break;
    case SelectorRole::kActivity:
      aliases = {"concept:name", "activity"};
      break;
    case SelectorRole::kTimestamp:
      aliases = {"time:timestamp", "timestamp"};
      break;
    case SelectorRole::kResource:
      aliases = {"org:resource", "resource"};
      break;
    case SelectorRole::kLifecycle:
      aliases = {"lifecycle:transition", "lifecycle"};
      break;
    case SelectorRole::kGeneric:
      break;
  }
  for (const auto alias : aliases) {
    if (const auto found = exact_column(table, alias)) return found;
  }
  return std::nullopt;
}

const EventAttribute* row_cell(const EventTable::Row& row,
                               const EventTable& table,
                               const Selector& selector,
                               FirstClassAttributeRole first_class_role) {
  if (selector.column) {
    if (*selector.column >= row.size() || !row[*selector.column]) return nullptr;
    const auto& value = *row[*selector.column];
    return value.is_null ? nullptr : &value;
  }
  for (std::size_t i = 0; i < row.size(); ++i) {
    if (!row[i]) continue;
    const auto& value = *row[i];
    if (value.is_null) continue;
    // A standard selector (or an omitted natural selector) may resolve by
    // first-class role.  A caller-provided custom name must match its source
    // key/column exactly; otherwise a missing selector would silently pick a
    // different timestamp, activity, or other role.
    const auto selected_role = role_for_selector(selector.name);
    const bool role_alias = selector.name.empty() ||
                            (selected_role && *selected_role ==
                                                    [&] {
                                                      switch (first_class_role) {
                                                        case FirstClassAttributeRole::kCaseId:
                                                          return SelectorRole::kCase;
                                                        case FirstClassAttributeRole::kActivity:
                                                          return SelectorRole::kActivity;
                                                        case FirstClassAttributeRole::kTimestamp:
                                                          return SelectorRole::kTimestamp;
                                                        case FirstClassAttributeRole::kResource:
                                                          return SelectorRole::kResource;
                                                        case FirstClassAttributeRole::kLifecycle:
                                                          return SelectorRole::kLifecycle;
                                                        case FirstClassAttributeRole::kNone:
                                                          return SelectorRole::kGeneric;
                                                      }
                                                      return SelectorRole::kGeneric;
                                                    }());
    if (role_alias && value.first_class_role == first_class_role &&
        first_class_role != FirstClassAttributeRole::kNone)
      return &value;
    if (!selector.name.empty() &&
        (value.first_class_source_key == selector.name ||
         value.key == selector.name))
      return &value;
    if (i < table.columns.size() && table.columns[i] == selector.name)
      return &value;
  }
  return nullptr;
}

bool selector_present_in_rows(const EventTable& table, const Selector& selector,
                              FirstClassAttributeRole first_class_role) {
  for (const auto& row : table.rows) {
    if (row_cell(row, table, selector, first_class_role) != nullptr) return true;
  }
  return false;
}

Selector required_selector(const EventTable& table, std::string name,
                           SelectorRole role, std::string_view description) {
  const auto semantic_role = role_for_selector(name);
  Selector result{std::move(name), std::nullopt,
                  semantic_role.value_or(role)};
  if (const auto found = alias_column(table, result.name, result.role)) {
    result.column = found;
    return result;
  }
  const auto first_class = [&] {
    switch (result.role) {
      case SelectorRole::kCase: return FirstClassAttributeRole::kCaseId;
      case SelectorRole::kActivity: return FirstClassAttributeRole::kActivity;
      case SelectorRole::kTimestamp: return FirstClassAttributeRole::kTimestamp;
      case SelectorRole::kResource: return FirstClassAttributeRole::kResource;
      case SelectorRole::kLifecycle:
        return FirstClassAttributeRole::kLifecycle;
      case SelectorRole::kGeneric: return FirstClassAttributeRole::kNone;
    }
    return FirstClassAttributeRole::kNone;
  }();
  if (first_class != FirstClassAttributeRole::kNone &&
      selector_present_in_rows(table, result, first_class)) {
    return result;
  }
  throw std::invalid_argument(selector_error(description, result.name));
}

Selector optional_selector(const EventTable& table, std::string name,
                           SelectorRole role) {
  const auto semantic_role = role_for_selector(name);
  Selector result{std::move(name), std::nullopt,
                  semantic_role.value_or(role)};
  if (const auto found = alias_column(table, result.name, result.role))
    result.column = found;
  return result;
}

const EventAttribute* selected_cell(const TableRow& row, const EventTable& table,
                                    const Selector& selector,
                                    FirstClassAttributeRole role) {
  const auto selected_role = [&] {
    switch (selector.role) {
      case SelectorRole::kCase: return FirstClassAttributeRole::kCaseId;
      case SelectorRole::kActivity: return FirstClassAttributeRole::kActivity;
      case SelectorRole::kTimestamp: return FirstClassAttributeRole::kTimestamp;
      case SelectorRole::kResource: return FirstClassAttributeRole::kResource;
      case SelectorRole::kLifecycle:
        return FirstClassAttributeRole::kLifecycle;
      case SelectorRole::kGeneric: return role;
    }
    return role;
  }();
  return row_cell(row.cells, table, selector, selected_role);
}

std::optional<std::string> cell_text(const TableRow& row,
                                     const EventTable& table,
                                     const Selector& selector,
                                     FirstClassAttributeRole role) {
  const auto* cell = selected_cell(row, table, selector, role);
  if (!cell) return std::nullopt;
  if (!cell->value.empty()) return cell->value;
  if (const auto* date = std::get_if<NativeDate>(&cell->typed_value))
    return date->value;
  if (const auto* unsigned_integer =
          std::get_if<std::uint64_t>(&cell->typed_value))
    return std::to_string(*unsigned_integer);
  return cell->value;
}

std::optional<double> parse_time(const std::optional<std::string>& value,
                                 std::string_view description) {
  if (!value) return std::nullopt;
  const auto parsed = parse_timestamp(*value);
  if (!parsed)
    throw std::invalid_argument("DFG table discovery: invalid " +
                                std::string(description) + " timestamp");
  return parsed;
}

std::optional<double> parse_number(const std::optional<std::string>& value) {
  if (!value || value->empty()) return std::nullopt;
  try {
    std::size_t consumed = 0;
    const double result = ::minera::detail::stod(*value, &consumed);
    if (consumed != value->size() || !std::isfinite(result)) {
      throw std::invalid_argument(
          "DFG table discovery: cost values must be finite numbers");
    }
    return result;
  } catch (const std::exception&) {
    throw std::invalid_argument(
        "DFG table discovery: cost values must be finite numbers");
  }
}

void set_event_projection(Event& event, const EventAttribute& source,
                          std::string_view column,
                          FirstClassAttributeRole role) {
  auto state = source.is_null ? FirstClassAttributeState::kNull
                              : FirstClassAttributeState::kPresent;
  switch (role) {
    case FirstClassAttributeRole::kCaseId:
      event.case_id = source.value;
      event.case_id_state = state;
      event.case_id_source_key = source.key.empty() ? std::string(column)
                                                     : source.key;
      break;
    case FirstClassAttributeRole::kActivity:
      event.activity = source.value;
      event.activity_state = state;
      event.activity_source_key = source.key.empty() ? std::string(column)
                                                      : source.key;
      event.activity_attribute = source;
      break;
    case FirstClassAttributeRole::kTimestamp:
      event.timestamp = source.value;
      event.timestamp_state = state;
      event.timestamp_source_key = source.key.empty() ? std::string(column)
                                                       : source.key;
      event.timestamp_attribute = source;
      break;
    case FirstClassAttributeRole::kResource:
      event.resource = source.value;
      event.resource_state = state;
      event.resource_source_key = source.key.empty() ? std::string(column)
                                                      : source.key;
      event.resource_attribute = source;
      break;
    case FirstClassAttributeRole::kLifecycle:
      event.lifecycle = source.value;
      event.lifecycle_state = state;
      event.lifecycle_source_key = source.key.empty() ? std::string(column)
                                                       : source.key;
      event.lifecycle_attribute = source;
      break;
    case FirstClassAttributeRole::kNone:
      break;
  }
}

Event event_from_row(const EventTable::Row& row, const EventTable& table,
                     std::size_t original_position,
                     const Selector& case_selector,
                     const Selector& activity_selector,
                     const Selector& timestamp_selector) {
  Event result;
  result.input_order = original_position;
  for (std::size_t i = 0; i < row.size(); ++i) {
    if (!row[i]) continue;
    auto copy = *row[i];
    if (copy.key.empty()) copy.key = table.columns[i];
    result.attributes.push_back(copy);
    const auto role = copy.first_class_role != FirstClassAttributeRole::kNone
                          ? copy.first_class_role
                          : first_class_role_for_column(table.columns[i]);
    set_event_projection(result, copy, table.columns[i], role);
  }
  // One named row for every selector lookup: selected_cell returns a pointer
  // into the row, so it must outlive each dereference below.
  const TableRow wrapped{row};
  const auto case_value = cell_text(
      wrapped, table, case_selector, FirstClassAttributeRole::kCaseId);
  if (case_value) {
    result.case_id = *case_value;
    result.case_id_state = FirstClassAttributeState::kPresent;
    result.case_id_source_key = case_selector.name;
  }
  const auto activity_value = cell_text(
      wrapped, table, activity_selector,
      FirstClassAttributeRole::kActivity);
  if (activity_value) {
    result.activity = *activity_value;
    result.activity_state = FirstClassAttributeState::kPresent;
    result.activity_source_key = activity_selector.name;
    if (const auto* selected = selected_cell(
            wrapped, table, activity_selector,
            FirstClassAttributeRole::kActivity))
      result.activity_attribute = *selected;
  }
  const auto timestamp_value = cell_text(
      wrapped, table, timestamp_selector,
      FirstClassAttributeRole::kTimestamp);
  if (timestamp_value) {
    result.timestamp = *timestamp_value;
    result.timestamp_state = FirstClassAttributeState::kPresent;
    result.timestamp_source_key = timestamp_selector.name;
    if (const auto* selected = selected_cell(
            wrapped, table, timestamp_selector,
            FirstClassAttributeRole::kTimestamp))
      result.timestamp_attribute = *selected;
  }
  return result;
}

std::vector<TableRow> read_rows(const EventTable& table,
                                const DfgTableOptions& options,
                                DfgTableBackend backend,
                                bool need_completion, bool need_start,
                                bool need_index, bool need_cost,
                                bool overwrite_omitted_start = false,
                                bool require_explicit_start = false,
                                bool require_completion = true) {
  table.validate();
  const auto case_selector = required_selector(
      table, options.case_id_attribute, SelectorRole::kCase, "case");
  const auto activity_selector = required_selector(
      table, options.activity_attribute, SelectorRole::kActivity, "activity");
  const auto target_selector = options.target_activity_attribute.empty()
                                   ? activity_selector
                                   : required_selector(
                                         table, options.target_activity_attribute,
                                         SelectorRole::kActivity, "target activity");
  const auto timestamp_selector = need_completion && require_completion
                                      ? required_selector(
                                            table, options.timestamp_attribute,
                                            SelectorRole::kTimestamp,
                                            "completion")
                                      : optional_selector(
                                            table, options.timestamp_attribute,
                                            SelectorRole::kTimestamp);

  Selector start_selector;
  bool start_uses_completion = true;
  if (need_start) {
    if (options.start_timestamp_attribute &&
        !options.start_timestamp_attribute->empty()) {
      start_selector = optional_selector(table,
                                         *options.start_timestamp_attribute,
                                         SelectorRole::kTimestamp);
      const bool start_available =
          start_selector.column.has_value() ||
          selector_present_in_rows(table, start_selector,
                                   FirstClassAttributeRole::kTimestamp);
      if (!start_available && require_explicit_start &&
          backend == DfgTableBackend::kPandas) {
        throw std::invalid_argument(
            selector_error("start timestamp",
                           *options.start_timestamp_attribute));
      }
      start_uses_completion = !start_available;
      if (start_uses_completion) start_selector = timestamp_selector;
    } else {
      const auto conventional = exact_column(table, "start_timestamp");
      if (conventional && !overwrite_omitted_start) {
        start_selector = Selector{"start_timestamp", conventional,
                                  SelectorRole::kTimestamp};
        start_uses_completion = false;
      } else {
        start_selector = timestamp_selector;
        start_uses_completion = true;
      }
    }
  }

  Selector cost_selector;
  if (need_cost && options.cost_attribute && !options.cost_attribute->empty()) {
    cost_selector = required_selector(table, *options.cost_attribute,
                                      SelectorRole::kGeneric, "cost");
  }
  Selector index_selector;
  bool index_selector_available = false;
  if (need_index) {
    index_selector = optional_selector(table, options.event_index_attribute,
                                      SelectorRole::kGeneric);
    index_selector_available =
        index_selector.column.has_value() ||
        selector_present_in_rows(table, index_selector,
                                 FirstClassAttributeRole::kNone);
  }

  std::vector<TableRow> rows;
  rows.reserve(table.rows.size());
  for (std::size_t i = 0; i < table.rows.size(); ++i) {
    TableRow result;
    result.cells = table.rows[i];
    result.original_position = i;
    result.case_value = cell_text(result, table, case_selector,
                                  FirstClassAttributeRole::kCaseId);
    result.activity_value = cell_text(result, table, activity_selector,
                                      FirstClassAttributeRole::kActivity);
    result.target_activity_value = cell_text(
        result, table, target_selector, FirstClassAttributeRole::kActivity);
    if (need_completion) {
      result.completion_text = cell_text(
          result, table, timestamp_selector,
          FirstClassAttributeRole::kTimestamp);
    }
    if (need_start) {
      result.start_text = start_uses_completion
                              ? result.completion_text
                              : cell_text(result, table, start_selector,
                                          FirstClassAttributeRole::kTimestamp);
    }
    if (need_cost && options.cost_attribute && !options.cost_attribute->empty())
      result.cost_text = cell_text(result, table, cost_selector,
                                   FirstClassAttributeRole::kNone);
    if (need_index) {
      result.index_is_generated = !index_selector_available;
      if (index_selector_available) {
        if (const auto* selected = selected_cell(
                result, table, index_selector, FirstClassAttributeRole::kNone)) {
          result.index_attribute = *selected;
          result.index_text = cell_text(result, table, index_selector,
                                        FirstClassAttributeRole::kNone);
        }
      }
    }
    result.completion = need_completion
                            ? parse_time(result.completion_text, "completion")
                            : std::nullopt;
    result.start = need_start ? parse_time(result.start_text, "start")
                              : std::nullopt;
    if (result.completion) result.completion_exact = parse_timestamp_exact(*result.completion_text);
    if (result.start) result.start_exact = parse_timestamp_exact(*result.start_text);
    result.cost = parse_number(result.cost_text);
    result.event = event_from_row(result.cells, table, i, case_selector,
                                  activity_selector, timestamp_selector);
    rows.push_back(std::move(result));
  }
  return rows;
}

// Rows sort by start, then completion, comparing exact instants. A null cell
// in an explicit start column is a missing start (it does not borrow the
// completion time); missing values sort last, except that Polars sorts them
// first.
void sort_rows(std::vector<TableRow>& rows, const DfgTableOptions& options,
               bool nulls_first = false) {
  if (!options.sort_caseid_required) return;
  // -1: lhs first, 1: rhs first, 0: tie.
  const auto compare = [&](const std::optional<ParsedExactTimestamp>& lhs,
                           const std::optional<ParsedExactTimestamp>& rhs) {
    if (!lhs || !rhs) {
      if (!lhs && !rhs) return 0;
      return (!lhs) == nulls_first ? -1 : 1;
    }
    if (lhs->seconds != rhs->seconds) return lhs->seconds < rhs->seconds ? -1 : 1;
    if (lhs->nanoseconds != rhs->nanoseconds) return lhs->nanoseconds < rhs->nanoseconds ? -1 : 1;
    return 0;
  };
  std::stable_sort(rows.begin(), rows.end(), [&](const TableRow& lhs,
                                                 const TableRow& rhs) {
    if (lhs.case_value != rhs.case_value) return lhs.case_value < rhs.case_value;
    if (options.sort_timestamp_along_case_id) {
      if (const int order = compare(lhs.start_exact, rhs.start_exact)) return order < 0;
      if (const int order = compare(lhs.completion_exact, rhs.completion_exact)) return order < 0;
    }
    return lhs.original_position < rhs.original_position;
  });
  for (std::size_t i = 0; i < rows.size(); ++i) rows[i].generated_index = i;
}

void assign_generated_indices(std::vector<TableRow>& rows) {
  for (std::size_t i = 0; i < rows.size(); ++i) rows[i].generated_index = i;
}

std::optional<double> numeric_index(const TableRow& row) {
  if (!row.index_text || row.index_text->empty()) return std::nullopt;
  try {
    std::size_t used = 0;
    const auto result = ::minera::detail::stod(*row.index_text, &used);
    if (used != row.index_text->size()) return std::nullopt;
    if (!std::isfinite(result)) return std::nullopt;
    return result;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

bool numeric_index_kind(const EventAttribute& attribute) {
  return attribute.kind == AttributeKind::kInt ||
         attribute.kind == AttributeKind::kFloat ||
         std::holds_alternative<std::int64_t>(attribute.typed_value) ||
         std::holds_alternative<std::uint64_t>(attribute.typed_value) ||
         std::holds_alternative<double>(attribute.typed_value);
}

using ComparableNumber = std::variant<std::int64_t, std::uint64_t, double>;

std::optional<ComparableNumber> comparable_number(
    const EventAttribute& attribute) {
  if (const auto* value = std::get_if<std::int64_t>(&attribute.typed_value))
    return *value;
  if (const auto* value = std::get_if<std::uint64_t>(&attribute.typed_value))
    return *value;
  if (const auto* value = std::get_if<double>(&attribute.typed_value)) {
    if (std::isfinite(*value)) return *value;
    return std::nullopt;
  }
  if (attribute.kind == AttributeKind::kInt) {
    std::string_view text = attribute.value;
    if (text.starts_with('+')) text.remove_prefix(1);
    std::int64_t signed_value = 0;
    const auto signed_result = std::from_chars(
        text.data(), text.data() + text.size(), signed_value, 10);
    if (signed_result.ec == std::errc{} &&
        signed_result.ptr == text.data() + text.size())
      return signed_value;
    std::uint64_t unsigned_value = 0;
    const auto unsigned_result = std::from_chars(
        text.data(), text.data() + text.size(), unsigned_value, 10);
    if (unsigned_result.ec == std::errc{} &&
        unsigned_result.ptr == text.data() + text.size())
      return unsigned_value;
    return std::nullopt;
  }
  try {
    std::size_t consumed = 0;
    const auto value = ::minera::detail::stod(attribute.value, &consumed);
    if (consumed == attribute.value.size() && std::isfinite(value)) return value;
  } catch (const std::exception&) {
  }
  return std::nullopt;
}

int compare_signed_real(std::int64_t integer, double real) {
  if (std::isnan(real)) return -1;
  if (real >= 0x1p63) return -1;
  if (real < -0x1p63) return 1;
  const auto truncated = static_cast<std::int64_t>(real);
  if (integer < truncated) return -1;
  if (integer > truncated) return 1;
  const auto whole = std::trunc(real);
  return real > whole ? -1 : real < whole ? 1 : 0;
}

int compare_unsigned_real(std::uint64_t integer, double real) {
  if (std::isnan(real)) return -1;
  if (real < 0) return 1;
  if (real >= 0x1p64) return -1;
  const auto truncated = static_cast<std::uint64_t>(real);
  if (integer < truncated) return -1;
  if (integer > truncated) return 1;
  const auto whole = std::trunc(real);
  return real > whole ? -1 : real < whole ? 1 : 0;
}

int compare_numbers(const ComparableNumber& left,
                     const ComparableNumber& right) {
  if (const auto* left_signed = std::get_if<std::int64_t>(&left)) {
    if (const auto* right_signed = std::get_if<std::int64_t>(&right))
      return *left_signed < *right_signed ? -1 : *left_signed > *right_signed;
    if (const auto* right_unsigned = std::get_if<std::uint64_t>(&right)) {
      if (*left_signed < 0) return -1;
      const auto converted = static_cast<std::uint64_t>(*left_signed);
      return converted < *right_unsigned ? -1 : converted > *right_unsigned;
    }
    return compare_signed_real(*left_signed, std::get<double>(right));
  }
  if (const auto* left_unsigned = std::get_if<std::uint64_t>(&left)) {
    if (const auto* right_unsigned = std::get_if<std::uint64_t>(&right))
      return *left_unsigned < *right_unsigned ? -1 : *left_unsigned > *right_unsigned;
    if (const auto* right_signed = std::get_if<std::int64_t>(&right)) {
      if (*right_signed < 0) return 1;
      const auto converted = static_cast<std::uint64_t>(*right_signed);
      return *left_unsigned < converted ? -1 : *left_unsigned > converted;
    }
    return compare_unsigned_real(*left_unsigned, std::get<double>(right));
  }
  const double left_real = std::get<double>(left);
  if (const auto* right_real = std::get_if<double>(&right))
    return left_real < *right_real ? -1 : left_real > *right_real;
  if (const auto* right_signed = std::get_if<std::int64_t>(&right))
    return -compare_signed_real(*right_signed, left_real);
  return -compare_unsigned_real(std::get<std::uint64_t>(right), left_real);
}

bool index_less(const TableRow& lhs, const TableRow& rhs) {
  if (lhs.index_is_generated || rhs.index_is_generated) {
    return lhs.index_is_generated && rhs.index_is_generated &&
           lhs.generated_index < rhs.generated_index;
  }
  if (!lhs.index_attribute || !rhs.index_attribute ||
      lhs.index_attribute->is_null || rhs.index_attribute->is_null ||
      !lhs.index_text || !rhs.index_text)
    return false;
  const bool lhs_numeric = numeric_index_kind(*lhs.index_attribute);
  const bool rhs_numeric = numeric_index_kind(*rhs.index_attribute);
  if (lhs_numeric != rhs_numeric) return false;
  if (lhs_numeric) {
    const auto left = comparable_number(*lhs.index_attribute);
    const auto right = comparable_number(*rhs.index_attribute);
    return left && right && compare_numbers(*left, *right) < 0;
  }
  return *lhs.index_text < *rhs.index_text;
}

double output_index(const TableRow& row) {
  // DFG pair results expose both this legacy double projection and the
  // source index text. Keep the latter exact for uint64 identifiers; this
  // floating projection is only the established numeric API.
  return numeric_index(row).value_or(static_cast<double>(row.generated_index));
}

std::string output_index_text(const TableRow& row) {
  return row.index_is_generated
             ? std::to_string(row.generated_index)
             : row.index_text.value_or("");
}

bool row_has_no_nulls(const TableRow& row) {
  return std::all_of(row.cells.begin(), row.cells.end(),
                     [](const auto& cell) { return cell && !cell->is_null; });
}

std::vector<BusinessHourSlot> normalized_slots(const DfgTableOptions& options) {
  std::vector<BusinessHourSlot> slots;
  if (!options.business_hour_slots) {
    slots.reserve(5);
    for (int day = 0; day < 5; ++day) {
      const double begin = day * kDaySeconds + 7.0 * 3600.0;
      slots.push_back({begin, begin + 10.0 * 3600.0});
    }
  } else {
    slots = *options.business_hour_slots;
  }
  for (const auto& slot : slots) {
    if (!std::isfinite(slot.start_seconds) ||
        !std::isfinite(slot.end_seconds) || slot.start_seconds < 0.0 ||
        slot.start_seconds >= slot.end_seconds || slot.end_seconds > kWeekSeconds) {
      throw std::invalid_argument(
          "DFG table discovery: business-hour slots must satisfy 0 <= start < end <= 604800");
    }
  }
  std::sort(slots.begin(), slots.end(), [](const auto& lhs, const auto& rhs) {
    return std::tie(lhs.start_seconds, lhs.end_seconds) <
           std::tie(rhs.start_seconds, rhs.end_seconds);
  });
  std::vector<BusinessHourSlot> merged;
  for (const auto& slot : slots) {
    if (!merged.empty() && slot.start_seconds <= merged.back().end_seconds + 1.0) {
      merged.back().end_seconds =
          std::max(merged.back().end_seconds, slot.end_seconds);
    } else {
      merged.push_back(slot);
    }
  }
  return merged;
}

std::optional<double> local_wall_timestamp(const std::string& text) {
  if (!parse_timestamp(text)) return std::nullopt;
  std::size_t end = text.size();
  while (end > 0 && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
    --end;
  if (const auto timezone = text.substr(0, end).find_first_of("Zz+-", 10);
      timezone != std::string::npos)
    end = timezone;
  return parse_timestamp(std::string_view(text).substr(0, end));
}

// pm4py's BusinessHours on microsecond wall clocks
// (detail::business_elapsed_seconds_microseconds); an epoch double holds its
// microsecond exactly enough for rounding to recover it (review AY1).
double business_elapsed(double begin, double end,
                        const std::vector<BusinessHourSlot>& slots) {
  return detail::business_elapsed_seconds_microseconds(std::llround(begin * 1e6),
                                                       std::llround(end * 1e6), slots);
}

// Polars measures a flow in whole seconds: Duration.total_seconds() truncates
// the exact nanosecond difference toward zero.
double polars_whole_seconds(const ParsedExactTimestamp& from, const ParsedExactTimestamp& to) {
  auto seconds = to.seconds - from.seconds;
  const auto nanoseconds =
      static_cast<std::int64_t>(to.nanoseconds) - static_cast<std::int64_t>(from.nanoseconds);
  if (seconds > 0 && nanoseconds < 0) --seconds;
  else if (seconds < 0 && nanoseconds > 0) ++seconds;
  return static_cast<double>(seconds);
}

double flow_time(const TableRow& source, const TableRow& target,
                 const DfgTableOptions& options, bool clamp_start, bool polars = false) {
  if (!source.completion || !target.start) return 0.0;
  double target_start = *target.start;
  std::optional<std::string> target_start_text = target.start_text;
  auto target_start_exact = target.start_exact;
  if (clamp_start && target_start < *source.completion) {
    target_start = *source.completion;
    target_start_text = source.completion_text;
    target_start_exact = source.completion_exact;
  }
  if (!options.business_hours) {
    if (polars && source.completion_exact && target_start_exact)
      return polars_whole_seconds(*source.completion_exact, *target_start_exact);
    // pandas: Timedelta.total_seconds of the exact gap; epoch doubles would
    // lose the sub-second part.
    if (source.completion_exact && target_start_exact)
      if (const auto gap = detail::exact_gap(*source.completion_exact, *target_start_exact))
        return detail::dataframe_gap_seconds(*gap);
    return target_start - *source.completion;
  }
  if (!polars) {
    // pandas holds UTC timestamps (a CSV offset is converted), so business
    // time runs on the UTC clock, from exact microseconds.
    const auto source_utc = source.completion_text
                                ? parse_timestamp_microseconds(*source.completion_text)
                                : std::nullopt;
    const auto target_utc = target_start_text
                                ? parse_timestamp_microseconds(*target_start_text)
                                : std::nullopt;
    if (!source_utc || !target_utc)
      throw std::invalid_argument(
          "DFG table discovery: invalid business-hours timestamp");
    return detail::business_elapsed_microseconds(*source_utc, *target_utc,
                                                 normalized_slots(options));
  }
  const auto source_local = source.completion_text
                                ? local_wall_timestamp(*source.completion_text)
                                : std::nullopt;
  const auto target_local = target_start_text
                                ? local_wall_timestamp(*target_start_text)
                                : std::nullopt;
  if (!source_local || !target_local)
    throw std::invalid_argument(
        "DFG table discovery: invalid business-hours timestamp");
  return business_elapsed(*source_local, *target_local,
                          normalized_slots(options));
}

DfgPerformanceSummary summarize_values(const std::vector<double>& values,
                                       bool polars) {
  DfgPerformanceSummary result;
  result.count = static_cast<std::uint64_t>(values.size());
  if (values.empty()) return result;
  if (!polars) {
    // pandas groupby: Kahan sum, sum / count mean, Welford deviation (NaN
    // for one observation).
    const auto aggregates = detail::aggregate_performance(values, true);
    result.sum = aggregates.sum;
    result.mean = aggregates.mean;
    result.median = aggregates.median;
    result.minimum = aggregates.minimum;
    result.maximum = aggregates.maximum;
    result.standard_deviation = values.size() > 1
        ? aggregates.stdev : std::numeric_limits<double>::quiet_NaN();
    return result;
  }
  result.sum = std::accumulate(values.begin(), values.end(), 0.0);
  result.mean = result.sum / static_cast<double>(values.size());
  const auto [minimum, maximum] =
      std::minmax_element(values.begin(), values.end());
  result.minimum = *minimum;
  result.maximum = *maximum;
  auto ordered = values;
  std::sort(ordered.begin(), ordered.end());
  const auto middle = ordered.size() / 2;
  result.median = ordered.size() % 2 == 0
                      ? (ordered[middle - 1] + ordered[middle]) / 2.0
                      : ordered[middle];
  if (values.size() > 1) {
    double square_sum = 0.0;
    for (const auto value : values) {
      const double delta = value - result.mean;
      square_sum += delta * delta;
    }
    result.standard_deviation =
        std::sqrt(square_sum / static_cast<double>(values.size() - 1));
  }
  return result;
}

DfgPerformanceValue select_value(const DfgPerformanceSummary& summary,
                                 const std::vector<double>& values,
                                 const DfgTableOptions& options) {
  if (options.custom_aggregation) return options.custom_aggregation(values);
  switch (options.performance_aggregation) {
    case DfgPerformanceAggregation::kMedian: return summary.median;
    case DfgPerformanceAggregation::kMinimum: return summary.minimum;
    case DfgPerformanceAggregation::kMaximum: return summary.maximum;
    case DfgPerformanceAggregation::kStandardDeviation:
      return summary.standard_deviation;
    case DfgPerformanceAggregation::kSum: return summary.sum;
    case DfgPerformanceAggregation::kRawValues: return values;
    case DfgPerformanceAggregation::kAll: return summary;
    case DfgPerformanceAggregation::kMean: return summary.mean;
  }
  throw std::invalid_argument("DFG table discovery: unsupported aggregation");
}

DfgPerformanceStatistics performance_statistics(
    const std::vector<double>& observations, const DfgTableOptions& options,
    bool polars) {
  auto summary = summarize_values(observations, polars);
  // Polars aggregates skip null durations and give null (NaN here) for an
  // edge with none left (except sum, which is 0), and a null sample
  // deviation for one observation.
  const double nan = std::numeric_limits<double>::quiet_NaN();
  if (polars && observations.empty())
    summary = {0, nan, nan, nan, nan, nan, 0.0};
  if (polars && observations.size() == 1) summary.standard_deviation = nan;
  return {summary, observations, select_value(summary, observations, options)};
}

template <typename Key>
void finish_performance(const std::map<Key, std::vector<double>>& values,
                        const DfgTableOptions& options, bool polars,
                        std::map<Key, DfgPerformanceStatistics>& output) {
  for (const auto& [key, observations] : values)
    output.emplace(key, performance_statistics(observations, options, polars));
}

using PairPositions = std::vector<std::pair<std::size_t, std::size_t>>;

PairPositions graph_pairs(const std::vector<TableRow>& rows,
                          const DfgTableOptions& options,
                          DfgTableBackend backend) {
  PairPositions pairs;
  if (backend == DfgTableBackend::kPandas) {
    if (options.window > rows.size()) return pairs;
    for (std::size_t i = 0; i + options.window < rows.size() + 1; ++i) {
      const auto target = i + options.window;
      if (target >= rows.size()) break;
      if (rows[i].case_value && rows[target].case_value &&
          *rows[i].case_value == *rows[target].case_value)
        pairs.emplace_back(i, target);
    }
    return pairs;
  }
  std::map<std::string, std::vector<std::size_t>> by_case;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    if (rows[i].case_value) by_case[*rows[i].case_value].push_back(i);
  }
  for (const auto& [case_id, positions] : by_case) {
    (void)case_id;
    if (options.window > positions.size()) continue;
    for (std::size_t i = 0; i + options.window < positions.size() + 1; ++i) {
      const auto target = i + options.window;
      if (target >= positions.size()) break;
      pairs.emplace_back(positions[i], positions[target]);
    }
  }
  std::sort(pairs.begin(), pairs.end());
  return pairs;
}

DfgTableResult discover_table_dfg(const EventTable& table,
                                  const DfgTableOptions& options,
                                  DfgTableBackend backend) {
  if (options.measure != DfgTableMeasure::kFrequency &&
      options.measure != DfgTableMeasure::kPerformance &&
      options.measure != DfgTableMeasure::kBoth &&
      options.measure != DfgTableMeasure::kCost)
    throw std::invalid_argument("DFG table discovery: unsupported measure");
  if (options.measure == DfgTableMeasure::kCost &&
      (!options.cost_attribute || options.cost_attribute->empty())) {
    throw std::invalid_argument(
        "DFG table discovery: cost measure requires a cost selector");
  }
  const bool timestamp_sort_requested = options.sort_timestamp_along_case_id;
  const bool computes_duration =
      options.measure == DfgTableMeasure::kPerformance ||
      options.measure == DfgTableMeasure::kBoth;
  const bool reads_timestamps = computes_duration || timestamp_sort_requested;
  const bool requires_completion =
      computes_duration ||
      (backend == DfgTableBackend::kPandas && timestamp_sort_requested);
  const bool polars = backend == DfgTableBackend::kPolars;
  if (!polars && (computes_duration || options.measure == DfgTableMeasure::kCost) &&
      !options.custom_aggregation &&
      options.performance_aggregation == DfgPerformanceAggregation::kStandardDeviation)
    // The eager adapter calls a groupby method named after the key, and
    // pandas has no SeriesGroupBy.stdev.
    throw std::invalid_argument("'SeriesGroupBy' object has no attribute 'stdev'");
  if (polars && (computes_duration || options.measure == DfgTableMeasure::kCost) &&
      !options.custom_aggregation) {
    // The lazy adapter names its aggregations differently from the eager one.
    if (options.performance_aggregation == DfgPerformanceAggregation::kStandardDeviation)
      throw std::invalid_argument("Unsupported aggregation key: stdev");
    if (options.performance_aggregation == DfgPerformanceAggregation::kRawValues)
      throw std::invalid_argument("'ExprListNameSpace' object is not callable");
  }
  auto rows = read_rows(table, options, backend, reads_timestamps,
                        reads_timestamps,
                        false, options.measure == DfgTableMeasure::kCost, false,
                        backend == DfgTableBackend::kPandas,
                        requires_completion);
  sort_rows(rows, options, polars);
  assign_generated_indices(rows);
  const auto pairs = graph_pairs(rows, options, backend);

  DfgTableResult result;
  result.backend = backend;
  result.measure = options.measure;
  std::map<DfgActivityPair, std::vector<double>> performance_values;
  std::set<std::tuple<std::string, std::string, std::string>> kept;
  const bool calculate_performance =
      options.measure == DfgTableMeasure::kPerformance ||
      options.measure == DfgTableMeasure::kBoth ||
      options.measure == DfgTableMeasure::kCost;
  const auto timestamp_column =
      alias_column(table, options.timestamp_attribute, SelectorRole::kTimestamp);
  const auto conventional_start_column = exact_column(table, "start_timestamp");
  bool explicit_start_available = false;
  if (options.start_timestamp_attribute &&
      !options.start_timestamp_attribute->empty()) {
    const auto selector = optional_selector(
        table, *options.start_timestamp_attribute, SelectorRole::kTimestamp);
    explicit_start_available =
        selector.column.has_value() ||
        selector_present_in_rows(table, selector,
                                 FirstClassAttributeRole::kTimestamp);
  }
  const bool clamp_start =
      (options.start_timestamp_attribute &&
       !options.start_timestamp_attribute->empty() &&
       explicit_start_available &&
       options.start_timestamp_attribute.value() != options.timestamp_attribute) ||
      (backend == DfgTableBackend::kPolars &&
       !options.start_timestamp_attribute && conventional_start_column &&
       timestamp_column && *conventional_start_column != *timestamp_column);
  // Polars keeps an edge whose duration is null and one whose source activity
  // is null in the performance result, but neither counts in frequency.
  std::map<std::string, std::vector<double>> null_source_values;
  std::set<std::pair<std::string, std::string>> kept_null_source;
  for (const auto [source_position, target_position] : pairs) {
    const auto& source = rows[source_position];
    const auto& target = rows[target_position];
    if (!target.target_activity_value || !source.case_value) continue;
    if (!source.activity_value) {
      if (!polars || !calculate_performance) continue;
      if (options.keep_once_per_case &&
          !kept_null_source.emplace(*source.case_value, *target.target_activity_value).second)
        continue;
      auto& values = null_source_values[*target.target_activity_value];
      if (options.measure == DfgTableMeasure::kCost)
        values.push_back(target.cost.value_or(0.0));
      else if (source.completion && target.start)
        values.push_back(flow_time(source, target, options, clamp_start, polars));
      else if (source.completion && clamp_start)
        values.push_back(0.0);
      continue;
    }
    const DfgActivityPair pair{*source.activity_value,
                               *target.target_activity_value};
    if (options.keep_once_per_case) {
      const auto key = std::make_tuple(*source.case_value, pair.first, pair.second);
      if (!kept.insert(key).second) continue;
    }
    if (options.measure == DfgTableMeasure::kFrequency ||
        options.measure == DfgTableMeasure::kBoth)
      ++result.frequency[pair];
    if (!calculate_performance) continue;
    if (options.measure == DfgTableMeasure::kCost) {
      performance_values[pair].push_back(target.cost.value_or(0.0));
    } else {
      if (!source.completion || !target.start) {
        // Polars clamps the start with max_horizontal(start, completion),
        // which ignores a null start: the flow is then zero seconds.
        if (polars && clamp_start && source.completion)
          performance_values[pair].push_back(0.0);
        else if (polars)
          (void)performance_values[pair];
        continue;
      }
      performance_values[pair].push_back(
          flow_time(source, target, options, clamp_start, polars));
    }
  }
  finish_performance(performance_values, options, polars, result.performance);
  finish_performance(null_source_values, options, polars,
                     result.null_source_performance);
  return result;
}

}  // namespace

DfgTableResult discover_pandas_dfg(const EventTable& table,
                                   const DfgTableOptions& options) {
  return discover_table_dfg(table, options, DfgTableBackend::kPandas);
}

DfgTableResult discover_polars_dfg(const LazyEventTable& table,
                                   const DfgTableOptions& options) {
  return discover_table_dfg(table.materialize(), options,
                            DfgTableBackend::kPolars);
}

namespace {

DfgTripleTable discover_triples_table(const EventTable& table,
                                      const DfgTableOptions& options,
                                      DfgTableBackend backend) {
  const bool needs_completion = options.sort_caseid_required &&
                                options.sort_timestamp_along_case_id;
  auto rows = read_rows(table, options, backend, needs_completion, false,
                        false, false);
  sort_rows(rows, options, backend == DfgTableBackend::kPolars);
  assign_generated_indices(rows);
  DfgTripleTable table_result;
  auto& result = table_result.triples;
  if (backend == DfgTableBackend::kPandas) {
    for (std::size_t i = 0; i + 2 < rows.size(); ++i) {
      const auto& first = rows[i];
      const auto& second = rows[i + 1];
      const auto& third = rows[i + 2];
      if (!first.case_value || !second.case_value || !third.case_value ||
          *first.case_value != *second.case_value ||
          *first.case_value != *third.case_value || !first.activity_value ||
          !second.activity_value || !third.activity_value)
        continue;
      ++result[{*first.activity_value, *second.activity_value,
                *third.activity_value}];
    }
    return table_result;
  }
  // Polars groups the null case id as one more case.
  std::map<std::optional<std::string>, std::vector<const TableRow*>> by_case;
  for (const auto& row : rows) by_case[row.case_value].push_back(&row);
  for (const auto& [case_id, events] : by_case) {
    (void)case_id;
    for (std::size_t i = 0; i + 2 < events.size(); ++i) {
      if (!events[i + 1]->activity_value || !events[i + 2]->activity_value)
        continue;
      if (!events[i]->activity_value) {
        ++table_result.null_first[{*events[i + 1]->activity_value,
                                   *events[i + 2]->activity_value}];
        continue;
      }
      ++result[{*events[i]->activity_value, *events[i + 1]->activity_value,
                *events[i + 2]->activity_value}];
    }
  }
  return table_result;
}

DfgPartialOrderResult discover_partial_table(const EventTable& table,
                                             const DfgTableOptions& options,
                                             DfgTableBackend backend) {
  // The eager adapter's final reduction is hard-coded to the conventional
  // index column instead of the caller's event-index selector. Preserve that
  // observable failure for custom indices so callers can detect it rather
  // than receiving a silently different reduction.
  if (backend == DfgTableBackend::kPandas && options.keep_first_following &&
      options.event_index_attribute != "@@index") {
    throw std::out_of_range(
        "DFG table discovery: eager first-following reduction requires @@index");
  }
  auto rows = read_rows(table, options, backend, true, true, true, false);
  sort_rows(rows, options);
  assign_generated_indices(rows);
  if (options.business_hours) (void)normalized_slots(options);

  DfgPartialOrderResult result;
  result.backend = backend;
  std::vector<DfgPartialOrderPair> pairs;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    for (std::size_t j = 0; j < rows.size(); ++j) {
      const auto& source = rows[i];
      const auto& target = rows[j];
      if (!source.case_value || !target.case_value ||
          *source.case_value != *target.case_value || !index_less(source, target))
        continue;
      if (!source.completion || !target.start ||
          *source.completion > *target.start)
        continue;
      DfgPartialOrderPair pair;
      pair.source = source.event;
      pair.target = target.event;
      pair.source_index = output_index(source);
      pair.target_index = output_index(target);
      pair.source_index_value = output_index_text(source);
      pair.target_index_value = output_index_text(target);
      pair.flow_time_seconds = flow_time(source, target, options, false);
      pairs.push_back(std::move(pair));
    }
  }
  if (options.keep_first_following) {
    if (backend == DfgTableBackend::kPolars) {
      std::stable_sort(pairs.begin(), pairs.end(), [](const auto& lhs,
                                                      const auto& rhs) {
        return std::tie(lhs.source.case_id, lhs.source_index,
                        lhs.source_index_value, lhs.target_index,
                        lhs.target_index_value) <
               std::tie(rhs.source.case_id, rhs.source_index,
                        rhs.source_index_value, rhs.target_index,
                        rhs.target_index_value);
      });
    }
    // Both adapters reduce on the source index alone. Existing duplicate
    // indices across cases therefore share one retained pair.
    std::set<std::string> seen;
    std::vector<DfgPartialOrderPair> first;
    for (auto& pair : pairs) {
      if (seen.insert(pair.source_index_value).second)
        first.push_back(std::move(pair));
    }
    pairs = std::move(first);
  }
  result.pairs = std::move(pairs);
  return result;
}

DfgConcurrentResult discover_concurrent_table(const EventTable& table,
                                               const DfgTableOptions& options,
                                               DfgTableBackend backend) {
  const bool pandas = backend == DfgTableBackend::kPandas;
  auto effective_options = options;
  // The concurrency adapters use their conventional generated index; unlike
  // partial-order discovery this operation has no custom index parameter.
  effective_options.event_index_attribute = "@@index";
  auto rows = read_rows(table, effective_options, backend, true, true, true,
                        false, pandas, pandas);
  if (pandas) {
    // The eager adapter always regenerates its conventional index before the
    // self-join, even when an input column with that name already exists.
    for (auto& row : rows) {
      row.index_text.reset();
      row.index_attribute.reset();
      row.index_is_generated = true;
    }
  }
  sort_rows(rows, options);
  assign_generated_indices(rows);
  if (options.business_hours) (void)normalized_slots(options);

  DfgConcurrentResult result;
  result.backend = backend;
  result.max_start_column = options.max_start_column;
  result.min_complete_column = options.min_complete_column;
  result.overlap_column = options.overlap_column;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    for (std::size_t j = 0; j < rows.size(); ++j) {
      if (i == j || (pandas && j <= i)) continue;
      const auto& left = rows[i];
      const auto& right = rows[j];
      if (backend == DfgTableBackend::kPandas && !options.reduce_columns &&
          (!row_has_no_nulls(left) || !row_has_no_nulls(right)))
        continue;
      if (pandas && options.reduce_columns &&
          (!left.activity_value || !right.activity_value))
        continue;
      if (backend == DfgTableBackend::kPolars && !index_less(left, right))
        continue;
      if (!left.case_value || !right.case_value ||
          *left.case_value != *right.case_value || !left.completion ||
          !right.start || !left.start || !right.completion)
        continue;
      // Only the lazy adapter applies this one-sided prefilter before it
      // calculates interval overlap.
      if (!pandas && *left.completion > *right.start) continue;
      const double maximum_start = std::max(*left.start, *right.start);
      const double minimum_completion =
          std::min(*left.completion, *right.completion);
      const double overlap = minimum_completion - maximum_start;
      if (options.strict ? overlap <= 0.0 : overlap < 0.0) continue;
      DfgConcurrentPair pair;
      pair.left = left.event;
      pair.right = right.event;
      pair.left_index = output_index(left);
      pair.right_index = output_index(right);
      pair.left_index_value = output_index_text(left);
      pair.right_index_value = output_index_text(right);
      pair.max_start_seconds = maximum_start;
      pair.min_complete_seconds = minimum_completion;
      pair.overlap_seconds = overlap;
      result.pairs.push_back(std::move(pair));
    }
  }
  return result;
}

}  // namespace

DfgPartialOrderResult discover_pandas_partial_order(
    const EventTable& table, const DfgTableOptions& options) {
  return discover_partial_table(table, options, DfgTableBackend::kPandas);
}

DfgPartialOrderResult discover_polars_partial_order(
    const LazyEventTable& table, const DfgTableOptions& options) {
  return discover_partial_table(table.materialize(), options,
                                DfgTableBackend::kPolars);
}

DfgConcurrentResult discover_pandas_concurrent_events(
    const EventTable& table, const DfgTableOptions& options) {
  return discover_concurrent_table(table, options, DfgTableBackend::kPandas);
}

DfgConcurrentResult discover_polars_concurrent_events(
    const LazyEventTable& table, const DfgTableOptions& options) {
  return discover_concurrent_table(table.materialize(), options,
                                   DfgTableBackend::kPolars);
}

DfgTripleFrequency discover_pandas_frequency_triples(
    const EventTable& table, const DfgTableOptions& options) {
  return discover_triples_table(table, options, DfgTableBackend::kPandas).triples;
}

DfgTripleFrequency discover_polars_frequency_triples(
    const LazyEventTable& table, const DfgTableOptions& options) {
  return discover_polars_frequency_triple_table(table, options).triples;
}

DfgTripleTable discover_polars_frequency_triple_table(
    const LazyEventTable& table, const DfgTableOptions& options) {
  return discover_triples_table(table.materialize(), options,
                                DfgTableBackend::kPolars);
}

}  // namespace minera
