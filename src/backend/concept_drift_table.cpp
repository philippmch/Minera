#include "minera/concept_drift.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

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

struct ScalarLess {
  bool operator()(const EventAttribute& left, const EventAttribute& right) const {
    return native_scalar_key_compare(left, right) < 0;
  }
};

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
  std::map<EventAttribute, const EventAttribute*, ScalarLess> first_times;
  for (const auto& row : table.rows) {
    if (missing_table_cell(row[selected_case])) continue;
    const auto& key = ordered_cell(row[selected_case]);
    auto [entry, inserted] = first_times.try_emplace(key, nullptr);
    if (inserted && first_times.size() > trace_limit)
      throw std::invalid_argument("concept drift table: selected case groups exceed safe bound");
    if (!entry->second && !missing_table_cell(row[selected_time])) entry->second = &*row[selected_time];
  }
  std::vector<double> timestamps;
  timestamps.reserve(first_times.size());
  for (const auto& [key, value] : first_times) timestamps.push_back(table_epoch(value));

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

  // Numeric labels retain exact scalar ordering/equality in the feature
  // alphabet. Ordered surrogate strings let the established BOSE core handle
  // their statistics; original event displays remain separate for output.
  std::map<EventAttribute, std::string, ScalarLess> alphabet;
  bool nonstring_activity = false;
  for (const auto& group : projection)
    for (std::size_t i = group.first; i < group.first + group.count; ++i)
      {
        const auto& value = ordered_cell(table.rows[i][activity]);
        nonstring_activity |= native_scalar_order_kind(value) != NativeScalarOrderKind::kString;
        alphabet.try_emplace(value);
      }
  std::size_t label_index = 0;
  for (auto& [key, label] : alphabet) label = std::to_string(1'000'000'000ULL + label_index++);
  std::vector<std::vector<std::string>> encoded, displayed;
  encoded.reserve(projection.size());
  displayed.reserve(projection.size());
  for (const auto& group : projection) {
    encoded.emplace_back(); displayed.emplace_back();
    encoded.back().reserve(group.count); displayed.back().reserve(group.count);
    for (std::size_t i = group.first; i < group.first + group.count; ++i) {
      const auto& value = *table.rows[i][activity];
      encoded.back().push_back(alphabet.at(value));
      displayed.back().push_back(root_cause_attribute_category(value));
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
