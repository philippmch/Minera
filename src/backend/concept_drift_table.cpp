#include "minera/concept_drift.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "minera/dataframe_validation.hpp"
#include "minera/timestamp.hpp"
#include "concept_drift_table_output.hpp"
#include "concept_drift_table_limits.hpp"
#include "exact_count_ratio.hpp"
#include "root_cause_trace_key.hpp"

namespace minera {
namespace {
constexpr std::size_t kTableEventLimit = 20'000'000;
constexpr std::size_t kTableTraceLimit = 2'000'000;

bool missing_table_cell(const std::optional<EventAttribute>& cell) {
  if (!cell || cell->is_null || cell->first_class_state == FirstClassAttributeState::kNull)
    return true;
  const auto* real = std::get_if<double>(&cell->typed_value);
  return real && std::isnan(*real);
}

struct ExactDateKey {
  std::int64_t seconds = 0;
  std::uint32_t nanoseconds = 0;
  bool timezone_aware = false;
};

ExactDateKey exact_date_key(const EventAttribute& value) {
  if (value.kind != AttributeKind::kDate &&
      !std::holds_alternative<NativeDate>(value.typed_value))
    throw std::invalid_argument("concept drift table: value is not a date");
  const auto* native = std::get_if<NativeDate>(&value.typed_value);
  const auto parsed = parse_timestamp_exact(native ? native->value : value.value);
  if (!parsed)
    throw std::invalid_argument("concept drift table: invalid date key");
  return {parsed->seconds, parsed->nanoseconds, parsed->timezone_aware};
}

int compare_exact_dates(const EventAttribute& left,
                        const EventAttribute& right) {
  const auto a = exact_date_key(left);
  const auto b = exact_date_key(right);
  if (a.timezone_aware != b.timezone_aware)
    throw std::invalid_argument(
        "concept drift table: date keys have incompatible time zones");
  if (a.seconds != b.seconds) return a.seconds < b.seconds ? -1 : 1;
  if (a.nanoseconds != b.nanoseconds)
    return a.nanoseconds < b.nanoseconds ? -1 : 1;
  return 0;
}

int compare_same_kind(const EventAttribute& left,
                      const EventAttribute& right) {
  const auto kind = native_scalar_order_kind(left);
  if (kind == NativeScalarOrderKind::kUnsupported ||
      kind != native_scalar_order_kind(right))
    throw std::invalid_argument(
        "concept drift table: incompatible scalar key types");
  if (kind == NativeScalarOrderKind::kDate) {
    // The shared comparator performs the existing date-kind and aware/naive
    // validation. Refine its microsecond comparison with the nanoseconds that
    // pandas datetime64[ns] keys retain.
    (void)native_scalar_key_compare(left, right);
    return compare_exact_dates(left, right);
  }
  return native_scalar_key_compare(left, right);
}

struct ScalarLess {
  bool operator()(const EventAttribute& left, const EventAttribute& right) const {
    return compare_same_kind(left, right) < 0;
  }
};

enum class ActivityFamily { kString, kNumeric, kBytes, kDate, kNull, kTuple };

constexpr std::size_t kMaxActivityTupleDepth = 128;
constexpr std::size_t kMaxActivityTupleNodes = 1'000'000;

struct ActivityTraversal {
  std::size_t nodes = 0;
};

bool activity_nan(const EventAttribute& value) {
  if (const auto* number = std::get_if<double>(&value.typed_value))
    return std::isnan(*number);
  return value.kind == AttributeKind::kFloat &&
         (value.value == "nan" || value.value == "NaN" ||
          value.value == "NAN");
}

ActivityFamily activity_family_shallow(const EventAttribute& value) {
  if (value.is_null || value.first_class_state == FirstClassAttributeState::kNull)
    return ActivityFamily::kNull;
  if (value.container == AttributeContainerKind::kTuple)
    return ActivityFamily::kTuple;
  if (value.container != AttributeContainerKind::kScalar ||
      value.scalar_wrapper || value.scalar_children_list ||
      !value.children.empty())
    throw std::invalid_argument(
        "concept drift table: mutable activity containers are unsupported");
  if (value.kind == AttributeKind::kBytes)
    return ActivityFamily::kBytes;
  if (value.kind == AttributeKind::kDate ||
      std::holds_alternative<NativeDate>(value.typed_value))
    return ActivityFamily::kDate;
  if (value.kind == AttributeKind::kInt || value.kind == AttributeKind::kFloat ||
      value.kind == AttributeKind::kBoolean ||
      std::holds_alternative<std::int64_t>(value.typed_value) ||
      std::holds_alternative<std::uint64_t>(value.typed_value) ||
      std::holds_alternative<double>(value.typed_value) ||
      std::holds_alternative<bool>(value.typed_value))
    return ActivityFamily::kNumeric;
  return ActivityFamily::kString;
}

ActivityFamily validate_activity_tree(const EventAttribute& value,
                                      std::size_t depth,
                                      ActivityTraversal& traversal) {
  if (depth > kMaxActivityTupleDepth ||
      traversal.nodes++ >= kMaxActivityTupleNodes)
    throw std::invalid_argument(
        "concept drift table: activity tuple nesting exceeds safe bound");
  const auto family = activity_family_shallow(value);
  if (family == ActivityFamily::kTuple)
    for (const auto& child : value.children)
      (void)validate_activity_tree(child, depth + 1, traversal);
  return family;
}

ActivityFamily activity_family(const EventAttribute& value) {
  ActivityTraversal traversal;
  return validate_activity_tree(value, 0, traversal);
}

std::string activity_identity_impl(const EventAttribute& value,
                                   std::size_t depth) {
  if (depth > kMaxActivityTupleDepth)
    throw std::invalid_argument(
        "concept drift table: activity tuple nesting exceeds safe bound");
  switch (const auto family = activity_family_shallow(value)) {
    case ActivityFamily::kNull:
      return "null";
    case ActivityFamily::kNumeric:
      return activity_nan(value)
                 ? "numeric:nan:" +
                       std::to_string(value.typed_value.scalar_identity())
                 : "numeric:" + root_cause_scalar_key_fingerprint(value);
    case ActivityFamily::kString:
      return "string:" + root_cause_scalar_key_fingerprint(value);
    case ActivityFamily::kBytes:
      return "bytes:" + root_cause_scalar_key_fingerprint(value);
    case ActivityFamily::kDate: {
      const auto date = exact_date_key(value);
      return "date:" + std::to_string(date.timezone_aware) + ":" +
             std::to_string(date.seconds) + ":" +
             std::to_string(date.nanoseconds);
    }
    case ActivityFamily::kTuple: {
      std::string result = "tuple:" + std::to_string(value.children.size());
      for (const auto& child : value.children) {
        const auto child_identity = activity_identity_impl(child, depth + 1);
        result += ":" + std::to_string(child_identity.size()) + ":" +
                  child_identity;
      }
      return result;
    }
  }
  throw std::invalid_argument("concept drift table: invalid activity key");
}

std::string activity_identity(const EventAttribute& value) {
  ActivityTraversal traversal;
  (void)validate_activity_tree(value, 0, traversal);
  return activity_identity_impl(value, 0);
}

int compare_activity_values_impl(const EventAttribute& left,
                                 const EventAttribute& right,
                                 std::size_t depth) {
  if (depth > kMaxActivityTupleDepth)
    throw std::invalid_argument(
        "concept drift table: activity tuple nesting exceeds safe bound");
  const auto left_family = activity_family_shallow(left);
  const auto right_family = activity_family_shallow(right);
  if (left_family != right_family)
    throw std::invalid_argument(
        "concept drift table: incompatible activity key types");
  switch (left_family) {
    case ActivityFamily::kNull:
      return 0;
    case ActivityFamily::kNumeric: {
      const bool left_nan = activity_nan(left), right_nan = activity_nan(right);
      if (left_nan || right_nan) {
        if (left_nan && right_nan) {
          const auto left_identity = left.typed_value.scalar_identity();
          const auto right_identity = right.typed_value.scalar_identity();
          if (left_identity == right_identity) return 0;
          return left_identity < right_identity ? -1 : 1;
        }
        return left_nan ? -1 : 1;
      }
      return compare_same_kind(left, right);
    }
    case ActivityFamily::kString:
    case ActivityFamily::kBytes:
    case ActivityFamily::kDate:
      return compare_same_kind(left, right);
    case ActivityFamily::kTuple: {
      const auto common = std::min(left.children.size(), right.children.size());
      for (std::size_t index = 0; index < common; ++index) {
        const auto order =
            compare_activity_values_impl(left.children[index],
                                         right.children[index], depth + 1);
        if (order != 0) return order;
      }
      if (left.children.size() == right.children.size()) return 0;
      return left.children.size() < right.children.size() ? -1 : 1;
    }
  }
  throw std::invalid_argument("concept drift table: invalid activity key");
}

int compare_activity_values(const EventAttribute& left,
                            const EventAttribute& right) {
  ActivityTraversal traversal;
  (void)validate_activity_tree(left, 0, traversal);
  (void)validate_activity_tree(right, 0, traversal);
  return compare_activity_values_impl(left, right, 0);
}

std::string table_key_identity(const EventAttribute& value) {
  const auto kind = native_scalar_order_kind(value);
  if (kind == NativeScalarOrderKind::kDate) {
    const auto date = exact_date_key(value);
    return "date:" + std::to_string(date.timezone_aware) + ":" +
           std::to_string(date.seconds) + ":" +
           std::to_string(date.nanoseconds);
  }
  if (kind == NativeScalarOrderKind::kUnsupported)
    throw std::invalid_argument("concept drift table: unordered key");
  return std::to_string(static_cast<int>(kind)) + ":" +
         root_cause_scalar_key_fingerprint(value);
}

int compare_selected_keys(const EventAttribute& left,
                          const EventAttribute& right) {
  const auto left_kind = native_scalar_order_kind(left);
  const auto right_kind = native_scalar_order_kind(right);
  if (left_kind != right_kind) {
    const bool numeric_string =
        (left_kind == NativeScalarOrderKind::kNumeric &&
         right_kind == NativeScalarOrderKind::kString) ||
        (left_kind == NativeScalarOrderKind::kString &&
         right_kind == NativeScalarOrderKind::kNumeric);
    if (!numeric_string)
      throw std::invalid_argument(
          "concept drift table: incompatible selected case key types");
    return left_kind == NativeScalarOrderKind::kNumeric ? -1 : 1;
  }
  return compare_same_kind(left, right);
}

const EventTableCategoricalDtype* category_descriptor(
    const EventTable& table, std::size_t column) {
  if (table.categorical_dtypes.empty() ||
      column >= table.categorical_dtypes.size() ||
      !table.categorical_dtypes[column])
    return nullptr;
  return &*table.categorical_dtypes[column];
}

using CategoryRanks = std::unordered_map<std::string, std::size_t>;

CategoryRanks category_ranks(const EventTableCategoricalDtype* descriptor) {
  CategoryRanks result;
  if (!descriptor) return result;
  result.reserve(descriptor->categories.size());
  for (std::size_t index = 0; index < descriptor->categories.size(); ++index)
    result.emplace(table_key_identity(descriptor->categories[index]), index);
  return result;
}

std::optional<std::size_t> category_rank(const EventAttribute& value,
                                         const CategoryRanks& ranks) {
  if (ranks.empty()) return std::nullopt;
  const auto identity = table_key_identity(value);
  const auto found = ranks.find(identity);
  return found == ranks.end() ? std::nullopt
                              : std::optional<std::size_t>(found->second);
}

int compare_selected_keys(const EventAttribute& left,
                          const EventAttribute& right,
                          const CategoryRanks& ranks) {
  if (!ranks.empty()) {
    const auto left_category = category_rank(left, ranks);
    const auto right_category = category_rank(right, ranks);
    if (left_category && right_category && *left_category != *right_category)
      return *left_category < *right_category ? -1 : 1;
    if (left_category != right_category)
      return left_category ? -1 : 1;
  }
  return compare_selected_keys(left, right);
}

const EventAttribute& ordered_cell(const std::optional<EventAttribute>& cell) {
  if (missing_table_cell(cell) || native_scalar_order_kind(*cell) == NativeScalarOrderKind::kUnsupported)
    throw std::invalid_argument("concept drift table: missing or unordered projection value");
  return *cell;
}

double table_epoch(const EventAttribute* cell) {
  if (!cell || (cell->kind != AttributeKind::kDate &&
                !std::holds_alternative<NativeDate>(cell->typed_value)))
    throw std::invalid_argument("concept drift table: case-start value must be a date");
  const auto* native = std::get_if<NativeDate>(&cell->typed_value);
  const auto parsed = parse_timestamp_exact(native ? native->value : cell->value);
  if (!parsed) throw std::invalid_argument("concept drift table: invalid case-start date");
  const bool negative = parsed->seconds < 0;
  const auto seconds = negative ? std::uint64_t{0} - static_cast<std::uint64_t>(parsed->seconds)
                                : static_cast<std::uint64_t>(parsed->seconds);
  constexpr std::uint64_t scale = 1000000000;
  const auto low = (seconds & 0xffffffffULL) * scale;
  const auto high = (seconds >> 32) * scale;
  auto numerator = detail::add(detail::CountUint128{0, low},
                               detail::CountUint128{high >> 32, high << 32});
  const auto fraction = detail::CountUint128::from_uint64(parsed->nanoseconds);
  numerator = negative ? detail::subtract(numerator, fraction) : detail::add(numerator, fraction);
  return detail::exact_ratio(numerator, detail::CountUint128::from_uint64(scale), negative);
}

struct SelectedGroup {
  EventAttribute key;
  const EventAttribute* first_time = nullptr;
};

std::vector<SelectedGroup> collect_selected_groups(
    const EventTable& table, std::size_t case_column,
    std::size_t timestamp_column, std::size_t trace_limit) {
  std::vector<SelectedGroup> groups;
  std::unordered_map<std::string, std::size_t> positions;
  positions.reserve(std::min(table.rows.size(), trace_limit));
  for (const auto& row : table.rows) {
    if (missing_table_cell(row[case_column])) continue;
    const auto& key = ordered_cell(row[case_column]);
    const auto identity = table_key_identity(key);
    const auto [entry, inserted] = positions.emplace(identity, groups.size());
    if (inserted) {
      if (groups.size() >= trace_limit)
        throw std::invalid_argument(
            "concept drift table: selected case groups exceed safe bound");
      groups.push_back(SelectedGroup{key, nullptr});
    }
    auto& group = groups[entry->second];
    if (!group.first_time && !missing_table_cell(row[timestamp_column]))
      group.first_time = &*row[timestamp_column];
  }
  const auto ranks = category_ranks(category_descriptor(table, case_column));
  std::sort(groups.begin(), groups.end(),
            [&](const SelectedGroup& left, const SelectedGroup& right) {
              return compare_selected_keys(left.key, right.key, ranks) < 0;
            });
  return groups;
}
}  // namespace

ConceptDriftTableResult detail::apply_concept_drift_table_bounded(
    const EventTable& table, ConceptDriftTableOptions options, std::mt19937& rng,
    std::size_t trace_limit) {
  table.validate();
  if (table.rows.size() > kTableEventLimit)
    throw std::invalid_argument("concept drift table: event materialization exceeds safe bound");
  const auto selected_case = table.column_index(options.case_id_attribute);
  const auto selected_time = table.column_index(options.timestamp_attribute);
  // Timestamp lookup and activity projection use distinct grouping paths in
  // the public adapter. Group-first timestamps skip null cells.
  const auto selected_groups = collect_selected_groups(
      table, selected_case, selected_time, trace_limit);
  std::vector<double> timestamps;
  timestamps.reserve(selected_groups.size());
  for (const auto& group : selected_groups)
    timestamps.push_back(table_epoch(group.first_time));

  check_dataframe_columns(table);
  const auto standard_case = table.column_index("case:concept:name");
  (void)table.column_index("time:timestamp");
  const auto activity = table.column_index(options.activity_attribute);
  struct Group { std::size_t first; std::size_t count; };
  std::map<EventAttribute, Group, ScalarLess> groups;
  std::optional<Group> null_group;
  for (std::size_t i = 0; i < table.rows.size(); ++i) {
    if (missing_table_cell(table.rows[i][standard_case])) {
      // NumPy numeric unique groups NaNs last. Object/extension nulls cannot
      // generally be sorted, but a singleton requires no comparisons.
      const bool numeric_nan = !table.dtypes.empty() &&
          table.dtypes[standard_case].starts_with("float");
      if (table.rows.size() != 1 && !numeric_nan)
        throw std::invalid_argument("concept drift table: unordered null case values");
      if (!null_group) null_group = Group{i, 0};
      if (groups.size() >= trace_limit)
        throw std::invalid_argument("concept drift table: projected case groups exceed safe bound");
      ++null_group->count;
      continue;
    }
    const auto& key = ordered_cell(table.rows[i][standard_case]);
    auto [entry, inserted] = groups.try_emplace(key, Group{i, 0});
    if (inserted && groups.size() + static_cast<std::size_t>(null_group.has_value()) > trace_limit)
      throw std::invalid_argument("concept drift table: projected case groups exceed safe bound");
    ++entry->second.count;
  }
  std::vector<Group> projection;
  projection.reserve(groups.size() + static_cast<std::size_t>(null_group.has_value()));
  for (const auto& [key, group] : groups) projection.push_back(group);
  if (null_group) projection.push_back(*null_group);

  // Activity labels have a separate key path from case grouping. The detector
  // still receives strings, but scalar aliases, nulls, NaNs, and immutable
  // tuples are compared before assigning ordered surrogate labels.
  EventAttribute null_activity{table.columns[activity], "",
                               AttributeKind::kString, true};
  null_activity.first_class_state = FirstClassAttributeState::kNull;
  std::vector<std::vector<const EventAttribute*>> activity_cells;
  struct ActivityEntry {
    const EventAttribute* value = nullptr;
    std::string identity;
  };
  std::vector<ActivityEntry> activity_values;
  std::unordered_map<std::string, std::size_t> activity_positions;
  activity_cells.reserve(projection.size());
  for (const auto& group : projection) {
    auto& cells = activity_cells.emplace_back();
    cells.reserve(group.count);
    for (std::size_t i = group.first; i < group.first + group.count; ++i) {
      const auto* value = table.rows[i][activity]
                              ? &*table.rows[i][activity]
                              : &null_activity;
      cells.push_back(value);
      const auto identity = activity_identity(*value);
      if (activity_positions.emplace(identity, activity_values.size()).second)
        activity_values.push_back(ActivityEntry{value, identity});
    }
  }
  std::sort(activity_values.begin(), activity_values.end(),
            [](const ActivityEntry& left, const ActivityEntry& right) {
              return compare_activity_values(*left.value, *right.value) < 0;
            });
  const bool nonstring_activity =
      !activity_values.empty() &&
      activity_family(*activity_values.front().value) != ActivityFamily::kString;
  std::unordered_map<std::string, std::string> alphabet;
  alphabet.reserve(activity_values.size());
  std::size_t label_index = 0;
  for (const auto& entry : activity_values)
    alphabet.emplace(entry.identity,
                     std::to_string(1'000'000'000ULL + label_index++));
  std::vector<std::vector<std::string>> encoded, displayed;
  encoded.reserve(projection.size());
  displayed.reserve(projection.size());
  for (const auto& cells : activity_cells) {
    encoded.emplace_back(); displayed.emplace_back();
    encoded.back().reserve(cells.size()); displayed.back().reserve(cells.size());
    for (const auto* value : cells) {
      const auto found = alphabet.find(activity_identity(*value));
      if (found == alphabet.end())
        throw std::invalid_argument(
            "concept drift table: activity key encoding failed");
      encoded.back().push_back(found->second);
      displayed.back().push_back(
          nonstring_activity ? std::string{}
                             : root_cause_attribute_category(*value));
    }
  }
  const auto detected = detect_concept_drift_traces(encoded, options, rng);
  ConceptDriftTableResult result;
  for (const auto& point : detected.change_points) {
    // The product can exceed int64 even when either option alone fits. Only
    // products within the bounded timestamp vector can be valid indices;
    // those small integer products are exact even on double-width long double.
    long double index = static_cast<long double>(point.sublog_index) * options.sub_log_size;
    const auto count = static_cast<long double>(timestamps.size());
    if (index < 0) index += count;
    if (index < 0 || index >= count)
      throw std::invalid_argument("concept drift table: change timestamp index is out of range");
    result.change_timestamps.push_back(timestamps[static_cast<std::size_t>(index)]);
    result.p_values.push_back(point.p_value);
  }
  // The public adapter joins activity strings only when emitting frames;
  // numeric labels therefore reject after the detector has advanced RNG.
  if (nonstring_activity && !detected.sublogs.empty())
    throw std::invalid_argument("concept drift table: output activities must be strings");
  std::set<std::int64_t> boundaries;
  for (const auto& point : detected.change_points) boundaries.insert(point.sublog_index);
  std::vector<std::vector<std::vector<std::string>>> partitions;
  std::vector<std::vector<std::string>> current;
  std::size_t cursor = 0;
  for (std::size_t i = 0; i < detected.sublogs.size(); ++i) {
    for (std::size_t j = 0; j < detected.sublogs[i].size(); ++j)
      current.push_back(std::move(displayed.at(cursor++)));
    if (boundaries.contains(static_cast<std::int64_t>(i + 1)) || i + 1 == detected.sublogs.size()) {
      partitions.push_back(std::move(current)); current.clear();
    }
  }
  result.returned_tables = detail::make_bose_output_tables(partitions);
  return result;
}

ConceptDriftTableResult apply_concept_drift_table(
    const EventTable& table, ConceptDriftTableOptions options, std::mt19937& rng) {
  return detail::apply_concept_drift_table_bounded(table, options, rng, kTableTraceLimit);
}

ConceptDriftTableResult apply_concept_drift_table(
    const EventTable& table, ConceptDriftTableOptions options) {
  std::mt19937 rng;
  if (options.seed <= std::numeric_limits<std::uint32_t>::max()) {
    rng.seed(static_cast<std::uint32_t>(options.seed));
  } else {
    std::seed_seq sequence{static_cast<std::uint32_t>(options.seed),
                           static_cast<std::uint32_t>(options.seed >> 32)};
    rng.seed(sequence);
  }
  return apply_concept_drift_table(table, options, rng);
}
}  // namespace minera
