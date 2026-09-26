#include "minera/concept_drift.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
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
constexpr std::size_t kTypedFeatureCellLimit = 20'000'000;
constexpr std::size_t kTypedRelationPairLimit = 2'000'000;
constexpr std::size_t kTypedRelationWorkLimit = 200'000'000;

void validate_typed_trace_materialization(
    const std::vector<std::vector<EventAttribute>>& traces) {
  if (traces.size() > kTableTraceLimit)
    throw std::invalid_argument(
        "concept drift: trace materialization exceeds safe bound");
  std::size_t event_count = 0;
  for (const auto& trace : traces) {
    if (trace.size() > kTableEventLimit - event_count)
      throw std::invalid_argument(
          "concept drift: event materialization exceeds safe bound");
    event_count += trace.size();
  }
}

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

enum class ActivityFamily {
  kString,
  kNumeric,
  kBytes,
  kDate,
  kNull,
  kTuple,
  kFrozenSet,
  kNativeNA
};

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

bool activity_calendar_date(const EventAttribute& value) {
  const auto* native = std::get_if<NativeDate>(&value.typed_value);
  const std::string_view text = native ? native->value : value.value;
  // The pinned Python behavior keeps date(YYYY-MM-DD) distinct from a
  // datetime at midnight. NativeDate retains the canonical ISO spelling.
  return text.size() == 10 && text[4] == '-' && text[7] == '-';
}

ActivityFamily activity_family_shallow(const EventAttribute& value) {
  if (is_native_na_attribute(value)) {
    validate_native_na_attribute(value, "concept drift activity");
    return ActivityFamily::kNativeNA;
  }
  if (value.is_null || value.first_class_state == FirstClassAttributeState::kNull)
    return ActivityFamily::kNull;
  if (value.container == AttributeContainerKind::kTuple)
    return ActivityFamily::kTuple;
  if (value.container == AttributeContainerKind::kFrozenSet)
    return ActivityFamily::kFrozenSet;
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
  if (family == ActivityFamily::kTuple ||
      family == ActivityFamily::kFrozenSet)
    for (const auto& child : value.children)
      (void)validate_activity_tree(child, depth + 1, traversal);
  return family;
}

ActivityFamily activity_family(const EventAttribute& value) {
  ActivityTraversal traversal;
  return validate_activity_tree(value, 0, traversal);
}

bool activity_contains_native_na_impl(const EventAttribute& value,
                                      std::size_t depth) {
  if (depth > kMaxActivityTupleDepth)
    throw std::invalid_argument(
        "concept drift table: activity tuple nesting exceeds safe bound");
  if (is_native_na_attribute(value)) return true;
  if (value.container != AttributeContainerKind::kTuple &&
      value.container != AttributeContainerKind::kFrozenSet)
    return false;
  for (const auto& child : value.children)
    if (activity_contains_native_na_impl(child, depth + 1)) return true;
  return false;
}

bool activity_contains_native_na(const EventAttribute& value) {
  // Callers have already obtained an activity identity for this value, which
  // validates the complete immutable tuple tree and its materialization bound.
  return activity_contains_native_na_impl(value, 0);
}

void consume_activity_membership_work(std::size_t& work) {
  if (work >= kTypedRelationWorkLimit)
    throw std::invalid_argument(
        "concept drift: activity membership work exceeds safe bound");
  ++work;
}

std::string activity_identity_impl(const EventAttribute& value,
                                   std::size_t depth,
                                   std::size_t* membership_work,
                                   std::unordered_map<
                                       const EventAttribute*,
                                       std::vector<std::string>>* order_cache);

std::vector<std::string> activity_frozen_member_identities(
    const EventAttribute& value, std::size_t depth,
    std::size_t* membership_work) {
  std::vector<std::string> members;
  members.reserve(value.children.size());
  for (const auto& child : value.children)
    members.push_back(activity_identity_impl(child, depth + 1,
                                             membership_work, nullptr));
  std::sort(members.begin(), members.end());
  members.erase(std::unique(members.begin(), members.end()), members.end());
  return members;
}

std::string activity_identity_impl(const EventAttribute& value,
                                   std::size_t depth,
                                   std::size_t* membership_work,
                                   std::unordered_map<
                                       const EventAttribute*,
                                       std::vector<std::string>>* order_cache) {
  if (depth > kMaxActivityTupleDepth)
    throw std::invalid_argument(
        "concept drift table: activity tuple nesting exceeds safe bound");
  if (membership_work) consume_activity_membership_work(*membership_work);
  switch (const auto family = activity_family_shallow(value)) {
    case ActivityFamily::kNull:
      return "null";
    case ActivityFamily::kNativeNA:
      return "native-na";
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
      return std::string(activity_calendar_date(value) ? "date:calendar:" :
                                                          "date:datetime:") +
             std::to_string(date.timezone_aware) + ":" +
             std::to_string(date.seconds) + ":" +
             std::to_string(date.nanoseconds);
    }
    case ActivityFamily::kTuple: {
      std::string result = "tuple:" + std::to_string(value.children.size());
      for (const auto& child : value.children) {
        const auto child_identity =
            activity_identity_impl(child, depth + 1, membership_work,
                                   order_cache);
        result += ":" + std::to_string(child_identity.size()) + ":" +
                  child_identity;
      }
      return result;
    }
    case ActivityFamily::kFrozenSet: {
      auto members = activity_frozen_member_identities(
          value, depth, membership_work);
      const std::vector<std::string>* canonical_members = &members;
      if (order_cache) {
        const auto [it, inserted] =
            order_cache->emplace(&value, std::move(members));
        (void)inserted;
        canonical_members = &it->second;
      }
      std::string result =
          "frozenset:" + std::to_string(canonical_members->size());
      for (const auto& member : *canonical_members)
        result += ":" + std::to_string(member.size()) + ":" + member;
      return result;
    }
  }
  throw std::invalid_argument("concept drift table: invalid activity key");
}

std::string activity_identity(const EventAttribute& value) {
  ActivityTraversal traversal;
  (void)validate_activity_tree(value, 0, traversal);
  return activity_identity_impl(value, 0, nullptr, nullptr);
}

std::string activity_identity(const EventAttribute& value,
                              std::size_t& membership_work) {
  ActivityTraversal traversal;
  (void)validate_activity_tree(value, 0, traversal);
  return activity_identity_impl(value, 0, &membership_work, nullptr);
}

std::string activity_identity_for_order(
    const EventAttribute& value,
    std::unordered_map<const EventAttribute*, std::vector<std::string>>& cache) {
  ActivityTraversal traversal;
  (void)validate_activity_tree(value, 0, traversal);
  return activity_identity_impl(value, 0, nullptr, &cache);
}

int compare_activity_values_impl(const EventAttribute& left,
                                 const EventAttribute& right,
                                 std::size_t depth,
                                 const std::unordered_map<
                                     const EventAttribute*,
                                     std::vector<std::string>>* order_cache) {
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
    case ActivityFamily::kNativeNA:
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
      return compare_same_kind(left, right);
    case ActivityFamily::kDate:
      if (activity_calendar_date(left) != activity_calendar_date(right))
        throw std::invalid_argument(
            "concept drift table: incompatible calendar date and datetime activities");
      return activity_calendar_date(left)
                 ? compare_exact_dates(left, right)
                 : compare_same_kind(left, right);
    case ActivityFamily::kTuple: {
      const auto common = std::min(left.children.size(), right.children.size());
      for (std::size_t index = 0; index < common; ++index) {
        const auto order =
            compare_activity_values_impl(left.children[index],
                                         right.children[index], depth + 1,
                                         order_cache);
        if (order != 0) return order;
      }
      if (left.children.size() == right.children.size()) return 0;
      return left.children.size() < right.children.size() ? -1 : 1;
    }
    case ActivityFamily::kFrozenSet: {
      // Native ordering extends the set subset order with a safe total order:
      // fewer unique members first, then lexicographic canonical BOSE member
      // identities. Incomparable sets are ordered by this extension; they are
      // never treated as equal for tuple-prefix comparison.
      std::vector<std::string> left_owned_members, right_owned_members;
      const auto members_for = [&](const EventAttribute& value,
                                   std::vector<std::string>& owned_members)
          -> const std::vector<std::string>& {
        if (order_cache) {
          const auto found = order_cache->find(&value);
          if (found == order_cache->end())
            throw std::invalid_argument(
                "concept drift table: missing cached frozenset ordering key");
          return found->second;
        }
        owned_members = activity_frozen_member_identities(
            value, depth, nullptr);
        return owned_members;
      };
      const auto& left_members = members_for(left, left_owned_members);
      const auto& right_members = members_for(right, right_owned_members);
      if (left_members.size() != right_members.size())
        return left_members.size() < right_members.size() ? -1 : 1;
      if (std::lexicographical_compare(left_members.begin(), left_members.end(),
                                       right_members.begin(), right_members.end()))
        return -1;
      if (std::lexicographical_compare(right_members.begin(), right_members.end(),
                                       left_members.begin(), left_members.end()))
        return 1;
      return 0;
    }
  }
  throw std::invalid_argument("concept drift table: invalid activity key");
}

int compare_activity_values(const EventAttribute& left,
                            const EventAttribute& right) {
  ActivityTraversal traversal;
  (void)validate_activity_tree(left, 0, traversal);
  (void)validate_activity_tree(right, 0, traversal);
  return compare_activity_values_impl(left, right, 0, nullptr);
}

enum class ActivityEquality { kEqual, kNotEqual, kAmbiguous };

ActivityEquality activity_equality_for_membership_impl(
    const EventAttribute& candidate, const EventAttribute& target,
    std::size_t depth, std::size_t& work) {
  consume_activity_membership_work(work);
  if (depth > kMaxActivityTupleDepth)
    throw std::invalid_argument(
        "concept drift table: activity tuple nesting exceeds safe bound");

  const auto candidate_family = activity_family_shallow(candidate);
  const auto target_family = activity_family_shallow(target);
  if (candidate_family == ActivityFamily::kTuple &&
      target_family == ActivityFamily::kTuple) {
    const auto common = std::min(candidate.children.size(), target.children.size());
    for (std::size_t index = 0; index < common; ++index) {
      const auto child_equality = activity_equality_for_membership_impl(
          candidate.children[index], target.children[index], depth + 1, work);
      if (child_equality != ActivityEquality::kEqual) return child_equality;
    }
    return candidate.children.size() == target.children.size()
               ? ActivityEquality::kEqual
               : ActivityEquality::kNotEqual;
  }
  if ((candidate_family == ActivityFamily::kTuple) !=
      (target_family == ActivityFamily::kTuple))
    return ActivityEquality::kNotEqual;

  // Python tuple equality treats the same pandas.NA singleton as equal while
  // advancing to the next tuple member. Direct pandas.NA comparisons with
  // ordinary non-null scalars yield an NA boolean whose truth value is
  // ambiguous; None is a deterministic non-match, and a tuple is a different
  // equality type.
  if (candidate_family == ActivityFamily::kNativeNA ||
      target_family == ActivityFamily::kNativeNA) {
    if (candidate_family == target_family) return ActivityEquality::kEqual;
    if (candidate_family == ActivityFamily::kNull ||
        target_family == ActivityFamily::kNull ||
        candidate_family == ActivityFamily::kTuple ||
        target_family == ActivityFamily::kTuple ||
        candidate_family == ActivityFamily::kFrozenSet ||
        target_family == ActivityFamily::kFrozenSet)
      return ActivityEquality::kNotEqual;
    return ActivityEquality::kAmbiguous;
  }

  return activity_identity(candidate, work) == activity_identity(target, work)
             ? ActivityEquality::kEqual
             : ActivityEquality::kNotEqual;
}

ActivityEquality activity_equality_for_membership(
    const EventAttribute& candidate, const EventAttribute& target,
    std::size_t& work) {
  return activity_equality_for_membership_impl(candidate, target, 0, work);
}

bool activity_sequence_contains(
    const EventAttribute& target,
    const std::vector<EventAttribute>& sequence,
    std::size_t& work) {
  for (const auto& candidate : sequence) {
    const auto equality =
        activity_equality_for_membership(candidate, target, work);
    if (equality == ActivityEquality::kAmbiguous)
      throw std::invalid_argument(
          "concept drift: boolean value of NA is ambiguous");
    if (equality == ActivityEquality::kEqual) return true;
  }
  return false;
}

struct OrderedActivityEncoding {
  std::unordered_map<std::string, std::string> labels;
  bool nonstring = false;
};

struct OrderedActivityValue {
  const EventAttribute* value = nullptr;
  std::string identity;
};

template <class VisitValues>
std::vector<OrderedActivityValue> ordered_unique_activity_values(
    VisitValues&& visit_values) {
  std::vector<OrderedActivityValue> activity_values;
  std::unordered_map<const EventAttribute*, std::vector<std::string>>
      frozen_set_order_cache;
  std::unordered_map<std::string, std::size_t> activity_positions;
  const auto collect = [&](const EventAttribute& value) {
    std::unordered_map<const EventAttribute*, std::vector<std::string>>
        value_order_cache;
    const auto identity =
        activity_identity_for_order(value, value_order_cache);
    if (activity_positions.emplace(identity, activity_values.size()).second) {
      // Keep order keys only for representatives retained by the sort.
      frozen_set_order_cache.merge(value_order_cache);
      activity_values.push_back(OrderedActivityValue{&value, identity});
    }
  };
  visit_values(collect);
  std::sort(activity_values.begin(), activity_values.end(),
            [&frozen_set_order_cache](const OrderedActivityValue& left,
                                      const OrderedActivityValue& right) {
              if (!frozen_set_order_cache.empty())
                return compare_activity_values_impl(
                           *left.value, *right.value, 0,
                           &frozen_set_order_cache) < 0;
              return compare_activity_values(*left.value, *right.value) < 0;
            });
  return activity_values;
}

// Assign surrogate string labels in the shared typed activity order. Both the
// table adapter and the typed direct detector use this path so aliases,
// tuples, nulls, and NaN identity retain one consistent ordering contract.
template <class VisitValues>
OrderedActivityEncoding ordered_activity_encoding(VisitValues&& visit_values) {
  const auto activity_values =
      ordered_unique_activity_values(std::forward<VisitValues>(visit_values));

  OrderedActivityEncoding result;
  result.nonstring =
      !activity_values.empty() &&
      activity_family(*activity_values.front().value) != ActivityFamily::kString;
  result.labels.reserve(activity_values.size());
  std::size_t label_index = 0;
  for (const auto& entry : activity_values)
    result.labels.emplace(entry.identity,
                          std::to_string(1'000'000'000ULL + label_index++));
  return result;
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
  const double epoch = detail::exact_ratio(
      numerator, detail::CountUint128::from_uint64(scale), negative);

  // The public BOSE adapter exposes timestamp values rounded to six decimal
  // places. Round the binary64 epoch value through decimal formatting so
  // halfway cases follow the same behavior as Python's rounded float path.
  char rounded[64];
  const auto formatted = std::to_chars(
      rounded, rounded + sizeof(rounded), epoch, std::chars_format::fixed, 6);
  if (formatted.ec != std::errc{})
    throw std::invalid_argument(
        "concept drift table: case-start date is out of range");
  double result = 0.0;
  const auto parsed_rounded =
      std::from_chars(rounded, formatted.ptr, result);
  if (parsed_rounded.ec != std::errc{})
    throw std::invalid_argument(
        "concept drift table: case-start date is out of range");
  return result;
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
    // pandas groupby drops its nullable-missing case key even though pandas.NA
    // remains a present activity scalar everywhere else in the adapter.
    if (missing_table_cell(row[case_column]) ||
        (row[case_column] && is_native_na_attribute(*row[case_column])))
      continue;
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
    if (!group.first_time && !missing_table_cell(row[timestamp_column]) &&
        !(row[timestamp_column] &&
          is_native_na_attribute(*row[timestamp_column])))
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

ConceptDriftTypedTraceResult detect_concept_drift_typed_traces(
    const std::vector<std::vector<EventAttribute>>& traces,
    ConceptDriftTraceOptions options, std::mt19937& rng) {
  if (options.sub_log_size == 0)
    throw std::invalid_argument("concept drift: sub_log_size must not be zero");
  validate_typed_trace_materialization(traces);

  const auto encoding = ordered_activity_encoding([&](const auto& collect) {
    for (const auto& trace : traces)
      for (const auto& activity : trace) collect(activity);
  });
  std::vector<std::vector<std::string>> encoded;
  encoded.reserve(traces.size());
  for (const auto& trace : traces) {
    auto& labels = encoded.emplace_back();
    labels.reserve(trace.size());
    for (const auto& activity : trace) {
      const auto found = encoding.labels.find(activity_identity(activity));
      if (found == encoding.labels.end())
        throw std::invalid_argument(
            "concept drift: typed activity key encoding failed");
      labels.push_back(found->second);
    }
  }

  const auto detected = detect_concept_drift_traces(encoded, options, rng);
  ConceptDriftTypedTraceResult result;
  result.change_points = detected.change_points;
  result.sublogs.reserve(detected.sublogs.size());
  std::size_t trace_index = 0;
  for (const auto& detected_group : detected.sublogs) {
    auto& group = result.sublogs.emplace_back();
    group.reserve(detected_group.size());
    for (const auto& ignored_trace : detected_group) {
      (void)ignored_trace;
      if (trace_index >= traces.size())
        throw std::logic_error(
            "concept drift: typed sublog reconstruction exceeded input");
      group.push_back(traces[trace_index++]);
    }
  }
  return result;
}

std::vector<EventAttribute> extract_concept_drift_typed_activities(
    const std::vector<std::vector<EventAttribute>>& traces) {
  validate_typed_trace_materialization(traces);
  const auto values = ordered_unique_activity_values([&](const auto& collect) {
    for (const auto& trace : traces)
      for (const auto& activity : trace) collect(activity);
  });
  std::vector<EventAttribute> result;
  result.reserve(values.size());
  for (const auto& value : values) result.push_back(*value.value);
  return result;
}

std::vector<double> extract_concept_drift_typed_features(
    const std::vector<std::vector<EventAttribute>>& traces,
    const std::vector<EventAttribute>& alphabet) {
  validate_typed_trace_materialization(traces);
  if (alphabet.size() > kTypedFeatureCellLimit / 3)
    throw std::invalid_argument(
        "concept drift: activity feature vector is too large");

  std::unordered_map<std::string, std::size_t> ids;
  ids.reserve(alphabet.size());
  std::vector<std::size_t> ordered_ids;
  ordered_ids.reserve(alphabet.size());
  for (const auto& activity : alphabet) {
    const auto identity = activity_identity(activity);
    auto [entry, inserted] = ids.emplace(identity, ids.size());
    (void)inserted;
    ordered_ids.push_back(entry->second);
  }
  bool alphabet_contains_native_na = false;
  for (const auto& activity : alphabet)
    alphabet_contains_native_na =
        activity_contains_native_na(activity) || alphabet_contains_native_na;
  const std::size_t unique_size = ids.size();
  if (unique_size != 0 &&
      unique_size > kTypedFeatureCellLimit / unique_size)
    throw std::invalid_argument(
        "concept drift: activity relation matrix is too large");

  // Validate every trace label before allocating the relation matrix. Explicit
  // alphabets preserve mixed types and order, so this path hashes identity but
  // never invokes the sorting comparator.
  for (const auto& trace : traces)
    for (const auto& activity : trace)
      if (!ids.contains(activity_identity(activity)))
        throw std::invalid_argument(
            "concept drift: event activity is absent from the supplied alphabet");

  std::vector<std::size_t> trace_counts(unique_size, 0);
  std::vector<std::size_t> follows(unique_size * unique_size, 0);
  std::size_t relation_work = 0;
  std::size_t membership_work = 0;
  for (const auto& trace : traces) {
    std::unordered_set<std::size_t> present;
    present.reserve(std::min(trace.size(), unique_size));
    std::unordered_set<std::size_t> relation;
    relation.reserve(std::min(trace.size() * 2, unique_size * unique_size));
    std::vector<std::size_t> seen;
    seen.reserve(std::min(trace.size(), unique_size));
    for (const auto& activity : trace) {
      const auto target = ids.at(activity_identity(activity));
      if (!seen.empty() &&
          (alphabet_contains_native_na ||
           activity_contains_native_na(activity))) {
        // Compute the feature relation with the same ordered sequence
        // membership semantics as the standalone follows helper. A pandas.NA
        // comparison may be ambiguous before a later equal alphabet entry;
        // duplicate and reordered alphabet members therefore remain
        // observable here.
        if (!activity_sequence_contains(activity, alphabet,
                                        membership_work))
          throw std::invalid_argument(
              "concept drift: event activity is absent from the supplied alphabet");
      }
      const bool first_occurrence = present.insert(target).second;
      for (const auto source : seen) {
        if (relation_work >= kTypedRelationWorkLimit)
          throw std::invalid_argument(
              "concept drift: activity relation work exceeds safe bound");
        ++relation_work;
        relation.insert(source * unique_size + target);
      }
      if (first_occurrence) seen.push_back(target);
    }
    for (const auto source : present) ++trace_counts[source];
    for (const auto key : relation) ++follows[key];
  }

  std::vector<std::size_t> multiplicities(unique_size, 0);
  for (const auto id : ordered_ids) ++multiplicities[id];
  std::vector<std::size_t> always_by_target(unique_size, 0);
  std::vector<std::size_t> sometimes_by_target(unique_size, 0);
  std::vector<std::size_t> never_by_target(unique_size, 0);
  for (std::size_t target = 0; target < unique_size; ++target) {
    std::size_t always = 0, sometimes = 0, never = 0;
    for (std::size_t source = 0; source < unique_size; ++source) {
      if (trace_counts[source] == 0 ||
          follows[source * unique_size + target] == 0) {
        never += multiplicities[source];
      } else if (follows[source * unique_size + target] ==
                 trace_counts[source]) {
        always += multiplicities[source];
      } else {
        sometimes += multiplicities[source];
      }
    }
    always_by_target[target] = always;
    sometimes_by_target[target] = sometimes;
    never_by_target[target] = never;
  }

  std::vector<double> features;
  features.reserve(alphabet.size() * 3);
  for (const auto target : ordered_ids) {
    features.push_back(static_cast<double>(always_by_target[target]));
    features.push_back(static_cast<double>(sometimes_by_target[target]));
    features.push_back(static_cast<double>(never_by_target[target]));
  }
  return features;
}

std::vector<std::pair<EventAttribute, EventAttribute>>
compute_concept_drift_typed_follows_relation(
    const std::vector<EventAttribute>& trace,
    const std::vector<EventAttribute>& alphabet) {
  if (trace.size() > kTableEventLimit || alphabet.size() > kTableEventLimit)
    throw std::invalid_argument(
        "concept drift: activity relation materialization exceeds safe bound");

  std::unordered_set<std::string> allowed_targets;
  allowed_targets.reserve(alphabet.size());
  bool alphabet_contains_native_na = false;
  for (const auto& activity : alphabet) {
    allowed_targets.insert(activity_identity(activity));
    alphabet_contains_native_na =
        activity_contains_native_na(activity) || alphabet_contains_native_na;
  }

  bool trace_contains_native_na = false;
  for (const auto& activity : trace) {
    (void)activity_identity(activity);
    trace_contains_native_na =
        activity_contains_native_na(activity) || trace_contains_native_na;
  }

  // Preserve the existing set-membership fast path and its early return for
  // inputs without pandas.NA.
  bool has_allowed_target = false;
  if (!alphabet_contains_native_na && !trace_contains_native_na) {
    for (const auto& activity : trace)
      if (allowed_targets.contains(activity_identity(activity)))
        has_allowed_target = true;
    if (!has_allowed_target) return {};
  }

  struct SeenValue {
    const EventAttribute* value = nullptr;
    std::string identity;
  };
  std::vector<SeenValue> seen_values;
  std::unordered_set<std::string> seen_identities;
  const auto initial_capacity =
      std::min(trace.size(), kTypedRelationPairLimit);
  seen_values.reserve(initial_capacity);
  seen_identities.reserve(initial_capacity);
  std::vector<std::pair<EventAttribute, EventAttribute>> relations;
  std::unordered_set<std::string> relation_identities;
  std::size_t relation_work = 0;
  std::size_t membership_work = 0;
  for (const auto& activity : trace) {
    const auto target_identity = activity_identity(activity);
    bool target_is_allowed = false;
    if (!seen_values.empty()) {
      if (alphabet_contains_native_na ||
          activity_contains_native_na(activity)) {
        target_is_allowed = activity_sequence_contains(
            activity, alphabet, membership_work);
      } else {
        target_is_allowed = allowed_targets.contains(target_identity);
      }
    }
    if (target_is_allowed) {
      for (const auto& source : seen_values) {
        if (relation_work >= kTypedRelationWorkLimit)
          throw std::invalid_argument(
              "concept drift: activity relation work exceeds safe bound");
        ++relation_work;
        std::string identity = std::to_string(source.identity.size()) + ":" +
                               source.identity +
                               std::to_string(target_identity.size()) + ":" +
                               target_identity;
        if (relation_identities.contains(identity)) continue;
        if (relations.size() >= kTypedRelationPairLimit)
          throw std::invalid_argument(
              "concept drift: activity relation result exceeds safe bound");
        relation_identities.insert(std::move(identity));
        relations.emplace_back(*source.value, activity);
      }
    }
    if (seen_identities.insert(target_identity).second)
      seen_values.push_back(SeenValue{&activity, target_identity});
  }
  return relations;
}

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
  activity_cells.reserve(projection.size());
  for (const auto& group : projection) {
    auto& cells = activity_cells.emplace_back();
    cells.reserve(group.count);
    for (std::size_t i = group.first; i < group.first + group.count; ++i) {
      const auto* value = table.rows[i][activity]
                              ? &*table.rows[i][activity]
                              : &null_activity;
      cells.push_back(value);
    }
  }
  const auto encoding = ordered_activity_encoding([&](const auto& collect) {
    for (const auto& cells : activity_cells)
      for (const auto* value : cells) collect(*value);
  });
  std::vector<std::vector<std::string>> encoded, displayed;
  encoded.reserve(projection.size());
  displayed.reserve(projection.size());
  for (const auto& cells : activity_cells) {
    encoded.emplace_back(); displayed.emplace_back();
    encoded.back().reserve(cells.size()); displayed.back().reserve(cells.size());
    for (const auto* value : cells) {
      const auto found = encoding.labels.find(activity_identity(*value));
      if (found == encoding.labels.end())
        throw std::invalid_argument(
            "concept drift table: activity key encoding failed");
      encoded.back().push_back(found->second);
      displayed.back().push_back(
          encoding.nonstring ? std::string{}
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
  if (encoding.nonstring && !detected.sublogs.empty())
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
