#include "minera/case_relations.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>

#include "minera/csv_event_log_importer.hpp"
#include "minera/timestamp.hpp"

namespace minera {
namespace {

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::vector<std::string> parse_csv_row(std::string_view line) {
  // getline removes LF, while multipart uploads retain the preceding CR.
  if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
  std::vector<std::string> fields;
  std::string field;
  bool in_quotes = false;
  bool closed_quote = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (in_quotes) {
      if (c == '"') {
        if (i + 1 < line.size() && line[i + 1] == '"') {
          field.push_back('"');
          ++i;
        } else {
          in_quotes = false;
          closed_quote = true;
        }
      } else {
        field.push_back(c);
      }
    } else if (closed_quote) {
      if (c != ',')
        throw std::invalid_argument(
            "case relations: characters after a quoted CSV field");
      fields.push_back(trim(std::move(field)));
      field.clear();
      closed_quote = false;
    } else if (c == ',') {
      fields.push_back(trim(std::move(field)));
      field.clear();
    } else if (c == '"') {
      if (!field.empty())
        throw std::invalid_argument(
            "case relations: quote inside an unquoted CSV field");
      in_quotes = true;
    } else {
      field.push_back(c);
    }
  }
  if (in_quotes)
    throw std::invalid_argument("case relations: unterminated CSV quote");
  fields.push_back(trim(std::move(field)));
  return fields;
}

const std::string* timestamp_text(const Event& event,
                                   const std::string& key) {
  const EventAttribute* attribute = nullptr;
  if (key == "time:timestamp" || key == "timestamp" ||
      (!event.timestamp_source_key.empty() && key == event.timestamp_source_key)) {
    if (event.timestamp_state == FirstClassAttributeState::kNull) return nullptr;
    attribute = event.timestamp_attribute ? &*event.timestamp_attribute : nullptr;
    if (!attribute) return event.has_timestamp() ? &event.timestamp : nullptr;
  } else {
    for (const auto& value : event.attributes)
      if (value.key == key) { attribute = &value; break; }
  }
  if (!attribute || attribute->is_null) return nullptr;
  if (const auto* native = std::get_if<NativeDate>(&attribute->typed_value))
    return &native->value;
  return &attribute->value;
}

using EventTime = std::pair<std::int64_t, std::int64_t>;

std::optional<EventTime> exact_timestamp(std::string text) {
  if (!parse_timestamp(text)) return std::nullopt;
  // Whole seconds remain exact over the supported calendar range. Parse the
  // fraction separately so adjacent nanoseconds do not collapse in a double.
  std::int64_t nanos = 0;
  if (const auto dot = text.find('.'); dot != std::string::npos) {
    auto end = dot + 1;
    std::size_t digits = 0;
    while (end < text.size() && text[end] >= '0' && text[end] <= '9') {
      if (digits++ < 9) nanos = nanos * 10 + text[end] - '0';
      ++end;
    }
    while (digits++ < 9) nanos *= 10;
    text.erase(dot, end - dot);
  }
  const auto seconds = parse_timestamp(text);
  if (!seconds) return std::nullopt;
  return EventTime{static_cast<std::int64_t>(*seconds), nanos};
}

std::optional<EventTime> event_time(const Event& event, const std::string& key) {
  const auto* text = timestamp_text(event, key);
  if (!text) return std::nullopt;
  const auto parsed = exact_timestamp(*text);
  if (!parsed) throw std::invalid_argument("case relations: invalid timestamp");
  return parsed;
}

struct TaggedEvent {
  Event event;
  std::size_t sequence = 0;
  std::optional<EventTime> time;
};

const std::string* trace_case_value(const Trace& trace,
                                    const std::string& key) {
  if (key == "case_id" || key == "case:concept:name" ||
      key == "concept:name") {
    return &trace.case_id;
  }
  if (const auto* value = trace.attribute(key)) return value;
  // CSV case columns are stored without their `case:` prefix, while XES
  // trace attributes retain their original key.  Accept both spellings.
  if (key.rfind("case:", 0) == 0)
    return trace.attribute(std::string_view(key).substr(5));
  return nullptr;
}

std::optional<std::size_t> find_column(const EventTable& table,
                                       std::string_view name) {
  const auto found = std::find(table.columns.begin(), table.columns.end(), name);
  if (found == table.columns.end()) return std::nullopt;
  return static_cast<std::size_t>(found - table.columns.begin());
}

std::size_t required_column(const EventTable& table,
                            const std::vector<std::string>& candidates,
                            std::string_view description) {
  for (const auto& candidate : candidates) {
    if (const auto index = find_column(table, candidate)) return *index;
  }
  throw std::invalid_argument("case relations: missing " +
                              std::string(description));
}

std::size_t resolve_case_column(const EventTable& table,
                                const std::string& key) {
  return required_column(table, {key}, "case id column " + key);
}

std::optional<std::size_t> resolve_timestamp_column(
    const EventTable& table, const std::string& key) {
  return find_column(table, key);
}

std::size_t resolve_relation_column(const EventTable& table,
                                    const std::string& configured_column,
                                    std::string_view description) {
  return required_column(table, {configured_column}, description);
}

std::optional<EventAttribute> copy_cell(const std::optional<EventAttribute>& cell,
                                        const std::string& column) {
  if (!cell) return std::nullopt;
  auto result = *cell;
  result.key = column;
  return result;
}

EventTableIndexLabel implicit_index_label(std::size_t position) {
  EventAttribute value;
  value.kind = AttributeKind::kInt;
  value.value = std::to_string(position);
  value.typed_value = static_cast<std::int64_t>(position);
  return EventTableIndexLabel{std::move(value)};
}

EventTableIndexLabel floating_index_label(std::size_t position) {
  EventAttribute value;
  value.kind = AttributeKind::kFloat;
  value.value = std::to_string(static_cast<double>(position));
  value.typed_value = static_cast<double>(position);
  return EventTableIndexLabel{std::move(value)};
}

std::optional<std::int64_t> integer_index_label(
    const EventTableIndexLabel& label) {
  const auto* scalar = std::get_if<EventAttribute>(&label.value);
  if (!scalar || scalar->is_null || scalar->kind != AttributeKind::kInt)
    return std::nullopt;
  if (const auto* value = std::get_if<std::int64_t>(&scalar->typed_value))
    return *value;
  return std::nullopt;
}

std::uint64_t index_magnitude(std::int64_t value) {
  if (value >= 0) return static_cast<std::uint64_t>(value);
  return static_cast<std::uint64_t>(-(value + 1)) + 1;
}

struct IndexStep {
  bool negative = false;
  std::uint64_t magnitude = 0;

  bool operator==(const IndexStep&) const = default;
};

IndexStep index_difference(std::int64_t right, std::int64_t left) {
  if ((right < 0) == (left < 0)) {
    if (right >= left)
      return {false, static_cast<std::uint64_t>(right - left)};
    return {true, static_cast<std::uint64_t>(left - right)};
  }
  const auto gap = index_magnitude(right) + index_magnitude(left);
  return {right < left, gap};
}

bool arithmetic_index_labels(const std::vector<EventTableIndexLabel>& labels) {
  if (labels.size() < 2) return true;
  const auto first = integer_index_label(labels[0]);
  const auto second = integer_index_label(labels[1]);
  if (!first || !second) return false;
  const auto step = index_difference(*second, *first);
  if (step.magnitude == 0) return false;
  for (std::size_t position = 2; position < labels.size(); ++position) {
    const auto previous = integer_index_label(labels[position - 1]);
    const auto current = integer_index_label(labels[position]);
    if (!previous || !current ||
        index_difference(*current, *previous) != step)
      return false;
  }
  return true;
}

bool valid_range_index(const EventTable& table) {
  if (!table.index) return true;
  return table.index->kind == "RangeIndex" &&
         table.index->dtype == "int64" &&
         arithmetic_index_labels(table.index->labels);
}

EventTableIndexLabel source_index_label(const EventTable& table,
                                        std::size_t position) {
  if (table.index) return table.index->labels[position];
  return implicit_index_label(position);
}

struct NumericValue {
  enum class Kind { kSigned, kUnsigned, kFloat } kind = Kind::kSigned;
  std::int64_t signed_value = 0;
  std::uint64_t unsigned_value = 0;
  double floating_value = 0.0;

  [[nodiscard]] bool integer() const noexcept {
    return kind != Kind::kFloat;
  }
};

std::optional<NumericValue> numeric_value(const EventAttribute& cell) {
  if (cell.is_null) return std::nullopt;
  if (cell.kind == AttributeKind::kInt) {
    if (const auto* value = std::get_if<std::int64_t>(&cell.typed_value))
      return NumericValue{NumericValue::Kind::kSigned, *value, 0, 0.0};
    if (const auto* value = std::get_if<std::uint64_t>(&cell.typed_value))
      return NumericValue{NumericValue::Kind::kUnsigned, 0, *value, 0.0};
  }
  if (cell.kind == AttributeKind::kFloat) {
    if (const auto* value = std::get_if<double>(&cell.typed_value))
      return NumericValue{NumericValue::Kind::kFloat, 0, 0, *value};
  }
  if (cell.kind == AttributeKind::kBoolean) {
    if (const auto* value = std::get_if<bool>(&cell.typed_value))
      return NumericValue{NumericValue::Kind::kSigned, *value ? 1 : 0, 0, 0.0};
  }
  return std::nullopt;
}

std::optional<EventTime> native_date_value(const EventAttribute& cell) {
  if (cell.is_null || cell.kind != AttributeKind::kDate) return std::nullopt;
  const auto* native = std::get_if<NativeDate>(&cell.typed_value);
  if (!native) return std::nullopt;
  const auto parsed = exact_timestamp(native->value);
  if (!parsed)
    throw std::invalid_argument("case relations: invalid timestamp");
  return parsed;
}

bool null_cell(const std::optional<EventAttribute>& cell) {
  return !cell || cell->is_null ||
         cell->first_class_state == FirstClassAttributeState::kNull;
}

// Compare an exact signed integer against a finite IEEE-754 double without
// converting the integer to double (which would collapse values above 2^53).
int compare_int_double(std::int64_t integer, double floating) {
  if (std::isnan(floating)) return -1;
  if (std::isinf(floating)) return floating > 0 ? -1 : 1;
  const bool integer_negative = integer < 0;
  const std::uint64_t integer_magnitude =
      integer_negative
          ? static_cast<std::uint64_t>(-(integer + 1)) + 1
          : static_cast<std::uint64_t>(integer);
  const std::uint64_t bits = std::bit_cast<std::uint64_t>(floating);
  const bool floating_negative = (bits >> 63) != 0;
  const std::uint64_t magnitude_bits = bits & ((std::uint64_t{1} << 63) - 1);
  const std::uint64_t exponent_bits = (magnitude_bits >> 52) & 0x7ff;
  const std::uint64_t fraction = magnitude_bits & ((std::uint64_t{1} << 52) - 1);
  if (exponent_bits == 0 && fraction == 0) {
    if (integer_magnitude == 0) return 0;
    return integer_negative ? -1 : 1;
  }
  if (integer_negative != floating_negative)
    return integer_negative ? -1 : 1;

  const std::uint64_t significand =
      exponent_bits == 0 ? fraction : (std::uint64_t{1} << 52) | fraction;
  const int exponent = exponent_bits == 0 ? -1074
                                          : static_cast<int>(exponent_bits) - 1023 - 52;
  int magnitude_comparison = 0;
  if (exponent >= 0) {
    if (exponent >= 64 || significand > (std::numeric_limits<std::uint64_t>::max() >> exponent)) {
      magnitude_comparison = -1;
    } else {
      const auto floating_integer = significand << exponent;
      magnitude_comparison = integer_magnitude < floating_integer
                                  ? -1
                                  : integer_magnitude > floating_integer ? 1 : 0;
    }
  } else {
    const int shift = -exponent;
    if (shift >= 64) {
      magnitude_comparison = integer_magnitude == 0 ? -1 : 1;
    } else {
      const std::uint64_t whole = significand >> shift;
      const std::uint64_t remainder_mask = (std::uint64_t{1} << shift) - 1;
      const std::uint64_t remainder = significand & remainder_mask;
      magnitude_comparison = integer_magnitude < whole
                                  ? -1
                                  : integer_magnitude > whole
                                        ? 1
                                        : remainder == 0 ? 0 : -1;
    }
  }
  return integer_negative ? -magnitude_comparison : magnitude_comparison;
}

// Compare an exact non-negative integer against a finite IEEE-754 double
// without converting the integer to double.  This is the unsigned companion
// to compare_int_double and keeps uint64 case ids/keys exact above 2^53.
int compare_uint_double(std::uint64_t integer, double floating) {
  if (std::isnan(floating)) return -1;
  if (std::isinf(floating)) return floating > 0 ? -1 : 1;
  if (floating < 0) return 1;
  const std::uint64_t bits = std::bit_cast<std::uint64_t>(floating);
  const std::uint64_t magnitude_bits = bits & ((std::uint64_t{1} << 63) - 1);
  const std::uint64_t exponent_bits = (magnitude_bits >> 52) & 0x7ff;
  const std::uint64_t fraction = magnitude_bits & ((std::uint64_t{1} << 52) - 1);
  if (exponent_bits == 0 && fraction == 0)
    return integer == 0 ? 0 : 1;

  const std::uint64_t significand =
      exponent_bits == 0 ? fraction : (std::uint64_t{1} << 52) | fraction;
  const int exponent = exponent_bits == 0 ? -1074
                                          : static_cast<int>(exponent_bits) - 1023 - 52;
  if (exponent >= 0) {
    if (exponent >= 64 ||
        significand > (std::numeric_limits<std::uint64_t>::max() >> exponent))
      return -1;
    const auto floating_integer = significand << exponent;
    return integer < floating_integer ? -1 : integer > floating_integer ? 1 : 0;
  }
  const int shift = -exponent;
  if (shift >= 64) return integer == 0 ? -1 : 1;
  const std::uint64_t whole = significand >> shift;
  const std::uint64_t remainder_mask = (std::uint64_t{1} << shift) - 1;
  const std::uint64_t remainder = significand & remainder_mask;
  if (integer < whole) return -1;
  if (integer > whole) return 1;
  return remainder == 0 ? 0 : -1;
}

int compare_integer(const NumericValue& left, const NumericValue& right) {
  if (left.kind == NumericValue::Kind::kSigned &&
      right.kind == NumericValue::Kind::kSigned)
    return left.signed_value < right.signed_value
               ? -1
               : left.signed_value > right.signed_value ? 1 : 0;
  if (left.kind == NumericValue::Kind::kUnsigned &&
      right.kind == NumericValue::Kind::kUnsigned)
    return left.unsigned_value < right.unsigned_value
               ? -1
               : left.unsigned_value > right.unsigned_value ? 1 : 0;
  if (left.kind == NumericValue::Kind::kSigned) {
    if (left.signed_value < 0) return -1;
    const auto right_value = right.unsigned_value;
    const auto left_value = static_cast<std::uint64_t>(left.signed_value);
    return left_value < right_value ? -1 : left_value > right_value ? 1 : 0;
  }
  if (right.signed_value < 0) return 1;
  const auto left_value = left.unsigned_value;
  const auto right_value = static_cast<std::uint64_t>(right.signed_value);
  return left_value < right_value ? -1 : left_value > right_value ? 1 : 0;
}

int compare_numeric(const NumericValue& left, const NumericValue& right) {
  if (left.integer() && right.integer()) return compare_integer(left, right);
  if (left.integer()) {
    if (left.kind == NumericValue::Kind::kSigned)
      return compare_int_double(left.signed_value, right.floating_value);
    return compare_uint_double(left.unsigned_value, right.floating_value);
  }
  if (right.integer()) {
    if (right.kind == NumericValue::Kind::kSigned)
      return -compare_int_double(right.signed_value, left.floating_value);
    return -compare_uint_double(right.unsigned_value, left.floating_value);
  }
  if (std::isnan(left.floating_value) || std::isnan(right.floating_value)) {
    if (std::isnan(left.floating_value) && std::isnan(right.floating_value)) return 0;
    return std::isnan(left.floating_value) ? 1 : -1;
  }
  return left.floating_value < right.floating_value
             ? -1
             : left.floating_value > right.floating_value ? 1 : 0;
}

enum class SortKind { kNumeric, kDate, kString, kMissing };

class CategoryLookup {
 public:
  explicit CategoryLookup(const EventTableCategoricalDtype* descriptor);

  [[nodiscard]] std::optional<std::size_t> code(
      const std::optional<EventAttribute>& cell) const;

 private:
  const EventTableCategoricalDtype* descriptor_ = nullptr;
  std::unordered_map<std::string, std::vector<std::size_t>> buckets_;
};

std::optional<std::size_t> category_code(
    const std::optional<EventAttribute>& cell,
    const CategoryLookup* lookup);

struct SortValue {
  SortKind kind = SortKind::kMissing;
  std::optional<NumericValue> numeric;
  std::optional<EventTime> date;
  std::string text;
  std::optional<std::size_t> category;
};

SortValue sort_value(const std::optional<EventAttribute>& cell,
                     const CategoryLookup* lookup = nullptr) {
  if (null_cell(cell)) return {};
  if (const auto code = category_code(cell, lookup))
    return {SortKind::kNumeric, std::nullopt, std::nullopt, {}, code};
  if (const auto numeric = numeric_value(*cell))
    return {SortKind::kNumeric, numeric, std::nullopt, {}, std::nullopt};
  if (const auto date = native_date_value(*cell))
    return {SortKind::kDate, std::nullopt, date, {}, std::nullopt};
  return {SortKind::kString, std::nullopt, std::nullopt, cell->value,
          std::nullopt};
}

int compare_sort_values(const SortValue& left, const SortValue& right) {
  if (left.kind == SortKind::kMissing || right.kind == SortKind::kMissing) {
    if (left.kind == right.kind) return 0;
    return left.kind == SortKind::kMissing ? 1 : -1;
  }
  if (left.kind != right.kind) {
    // Public pandas ordering places numeric and datetime values before object
    // strings.  Numeric/datetime mixtures are rejected by the caller.
    return static_cast<int>(left.kind) < static_cast<int>(right.kind) ? -1 : 1;
  }
  if (left.kind == SortKind::kNumeric) {
    if (left.category || right.category) {
      if (left.category && right.category)
        return *left.category < *right.category
                   ? -1
                   : *left.category > *right.category ? 1 : 0;
      return left.category ? 1 : -1;
    }
    return compare_numeric(*left.numeric, *right.numeric);
  }
  if (left.kind == SortKind::kDate)
    return *left.date < *right.date ? -1 : *left.date > *right.date ? 1 : 0;
  return left.text < right.text ? -1 : left.text > right.text ? 1 : 0;
}

bool category_value_equal(const EventAttribute& left,
                          const EventAttribute& right);

bool cell_equal(const std::optional<EventAttribute>& left,
                const std::optional<EventAttribute>& right) {
  if (null_cell(left) || null_cell(right)) return null_cell(left) && null_cell(right);
  const auto left_numeric = numeric_value(*left);
  const auto right_numeric = numeric_value(*right);
  if (left_numeric || right_numeric)
    return left_numeric && right_numeric &&
           compare_numeric(*left_numeric, *right_numeric) == 0;
  const auto left_date = native_date_value(*left);
  const auto right_date = native_date_value(*right);
  if (left_date || right_date)
    return left_date && right_date && *left_date == *right_date;
  return category_value_equal(*left, *right);
}

bool has_integer_float_conversion_collision(const EventTable& table,
                                            std::size_t index) {
  std::vector<NumericValue> integers;
  std::vector<double> floats;
  for (const auto& row : table.rows) {
    if (null_cell(row[index])) continue;
    if (const auto numeric = numeric_value(*row[index])) {
      if (numeric->integer() && row[index]->kind == AttributeKind::kInt)
        integers.push_back(*numeric);
      else if (!numeric->integer() && row[index]->kind == AttributeKind::kFloat)
        floats.push_back(numeric->floating_value);
    }
  }
  for (const auto integer : integers) {
    const auto converted = integer.kind == NumericValue::Kind::kSigned
                               ? static_cast<double>(integer.signed_value)
                               : static_cast<double>(integer.unsigned_value);
    for (const auto floating : floats) {
      if (std::bit_cast<std::uint64_t>(converted) ==
              std::bit_cast<std::uint64_t>(floating) &&
          compare_numeric(integer, NumericValue{NumericValue::Kind::kFloat, 0,
                                               0, floating}) != 0)
        return true;
    }
  }
  return false;
}

bool has_integer_float_conversion_collision(
    const EventTable& first, std::size_t first_index,
    const EventTable& second, std::size_t second_index) {
  std::vector<NumericValue> integers;
  std::vector<double> floats;
  const auto collect = [&](const EventTable& table, std::size_t index) {
    for (const auto& row : table.rows) {
      if (null_cell(row[index])) continue;
      if (const auto numeric = numeric_value(*row[index])) {
        if (numeric->integer() && row[index]->kind == AttributeKind::kInt)
          integers.push_back(*numeric);
        else if (!numeric->integer() && row[index]->kind == AttributeKind::kFloat)
          floats.push_back(numeric->floating_value);
      }
    }
  };
  collect(first, first_index);
  collect(second, second_index);
  for (const auto integer : integers) {
    const auto converted = integer.kind == NumericValue::Kind::kSigned
                               ? static_cast<double>(integer.signed_value)
                               : static_cast<double>(integer.unsigned_value);
    for (const auto floating : floats) {
      if (std::bit_cast<std::uint64_t>(converted) ==
              std::bit_cast<std::uint64_t>(floating) &&
          compare_numeric(integer, NumericValue{NumericValue::Kind::kFloat, 0,
                                               0, floating}) != 0)
        return true;
    }
  }
  return false;
}

enum class CellDomain { kMissingOnly, kNumeric, kDate, kString, kOther, kMixed };

struct DomainPresence {
  bool numeric = false;
  bool date = false;
};

CellDomain cell_domain(const EventAttribute& cell) {
  if (cell.is_null) return CellDomain::kMissingOnly;
  if (numeric_value(cell)) return CellDomain::kNumeric;
  if (native_date_value(cell)) return CellDomain::kDate;
  if (cell.kind == AttributeKind::kString || cell.kind == AttributeKind::kId ||
      cell.kind == AttributeKind::kBytes || cell.kind == AttributeKind::kDate)
    return CellDomain::kString;
  return CellDomain::kOther;
}

CellDomain column_domain(const EventTable& table, std::size_t index) {
  CellDomain domain = CellDomain::kMissingOnly;
  for (const auto& row : table.rows) {
    if (null_cell(row[index])) continue;
    const auto current = cell_domain(*row[index]);
    if (domain == CellDomain::kMissingOnly) {
      domain = current;
    } else if (domain != current) {
      domain = CellDomain::kMixed;
    }
  }
  return domain;
}

DomainPresence column_domain_presence(const EventTable& table,
                                      std::size_t index) {
  DomainPresence presence;
  for (const auto& row : table.rows) {
    if (null_cell(row[index])) continue;
    if (numeric_value(*row[index]))
      presence.numeric = true;
    else if (native_date_value(*row[index]))
      presence.date = true;
  }
  return presence;
}

bool column_has_kind(const EventTable& table, std::size_t index,
                     AttributeKind kind) {
  return std::any_of(table.rows.begin(), table.rows.end(), [&](const auto& row) {
    return !null_cell(row[index]) && row[index]->kind == kind;
  });
}

void reject_incompatible_join_domains(const EventTable& incoming,
                                      std::size_t incoming_case,
                                      const EventTable& relations,
                                      std::size_t relation_right) {
  const auto incoming_domain = column_domain(incoming, incoming_case);
  const auto relation_domain = column_domain(relations, relation_right);
  if (incoming_domain == CellDomain::kMissingOnly ||
      relation_domain == CellDomain::kMissingOnly ||
      incoming_domain == CellDomain::kMixed ||
      relation_domain == CellDomain::kMixed || incoming_domain == relation_domain)
    return;
  throw std::invalid_argument(
      "case relations: incoming and relation case id columns have incompatible types");
}

void reject_incompatible_timestamp_domains(const EventTable& base,
                                           const std::optional<std::size_t>& base_time,
                                           const EventTable& incoming,
                                           const std::optional<std::size_t>& incoming_time) {
  if ((base_time && has_integer_float_conversion_collision(base, *base_time)) ||
      (incoming_time &&
       has_integer_float_conversion_collision(incoming, *incoming_time)) ||
      (base_time && incoming_time &&
       has_integer_float_conversion_collision(base, *base_time, incoming,
                                              *incoming_time)))
    throw std::invalid_argument(
        "case relations: numeric timestamp categories are not unique");
  DomainPresence presence;
  if (base_time) {
    const auto current = column_domain_presence(base, *base_time);
    presence.numeric = presence.numeric || current.numeric;
    presence.date = presence.date || current.date;
  }
  if (incoming_time) {
    const auto current = column_domain_presence(incoming, *incoming_time);
    presence.numeric = presence.numeric || current.numeric;
    presence.date = presence.date || current.date;
  }
  if (presence.numeric && presence.date)
    throw std::invalid_argument(
        "case relations: numeric and datetime timestamp columns are not orderable");
}

struct TableColumnPlan {
  std::size_t source_index = 0;
  std::string output_name;
};

void add_output_column(std::vector<std::string>& columns,
                       const std::string& name) {
  if (std::find(columns.begin(), columns.end(), name) == columns.end())
    columns.push_back(name);
}

std::vector<TableColumnPlan> plan_columns(
    const EventTable& source, const EventTable& other,
    const std::string& suffix) {
  std::vector<TableColumnPlan> plans;
  plans.reserve(source.columns.size());
  for (std::size_t index = 0; index < source.columns.size(); ++index) {
    const auto& column = source.columns[index];
    const auto output_name = find_column(other, column) ? column + suffix : column;
    plans.push_back({index, output_name});
  }
  return plans;
}

// The table adapter carries pandas' dtype spelling as metadata.  Merge
// coercion uses the spelling for empty columns as well as populated cells;
// values are still checked below before a narrowing conversion is made.
enum class DtypeFamily {
  kUnknown,
  kObject,
  kString,
  kSigned,
  kUnsigned,
  kFloat,
  kBoolean,
  kCategory,
  kDatetime,
};

struct DtypeInfo {
  std::string name;
  DtypeFamily family = DtypeFamily::kUnknown;
  int width = 0;
  bool extension = false;
  std::string datetime_timezone;
};

DtypeInfo parse_dtype(const std::string& name) {
  DtypeInfo result;
  result.name = name;
  if (name == "object") result.family = DtypeFamily::kObject;
  else if (name == "str") result.family = DtypeFamily::kString;
  else if (name == "string") {
    result.family = DtypeFamily::kString;
    result.extension = true;
  } else if (name == "bool") result.family = DtypeFamily::kBoolean;
  else if (name == "boolean") {
    result.family = DtypeFamily::kBoolean;
    result.extension = true;
  } else if (name == "Int64") {
    result.family = DtypeFamily::kSigned;
    result.width = 64;
    result.extension = true;
  } else if (name == "UInt64") {
    result.family = DtypeFamily::kUnsigned;
    result.width = 64;
    result.extension = true;
  } else if (name == "Float64") {
    result.family = DtypeFamily::kFloat;
    result.width = 64;
    result.extension = true;
  } else if (name == "category") result.family = DtypeFamily::kCategory;
  else if (name.rfind("datetime64[", 0) == 0) {
    const auto open = name.find('[');
    const auto close = name.rfind(']');
    if (open != std::string::npos && close == name.size() - 1 && close > open) {
      const auto specification = trim(name.substr(open + 1, close - open - 1));
      const auto comma = specification.find(',');
      const auto unit = trim(comma == std::string::npos
                                 ? specification
                                 : specification.substr(0, comma));
      const auto timezone =
          comma == std::string::npos
              ? std::string{}
              : trim(specification.substr(comma + 1));
      const bool valid_unit =
          unit == "s" || unit == "ms" || unit == "us" || unit == "ns";
      const bool valid_timezone =
          comma == std::string::npos ||
          (!timezone.empty() && specification.find(',', comma + 1) ==
                                     std::string::npos);
      if (valid_unit && valid_timezone) {
        result.family = DtypeFamily::kDatetime;
        // Width ranks the resolution so promotion keeps the finer unit.
        result.width = unit == "ns" ? 64 : unit == "us" ? 32 : unit == "ms" ? 16 : 8;
        result.datetime_timezone = timezone;
      }
    }
  } else if (name == "int8") {
    result.family = DtypeFamily::kSigned;
    result.width = 8;
  } else if (name == "int16") {
    result.family = DtypeFamily::kSigned;
    result.width = 16;
  } else if (name == "int32") {
    result.family = DtypeFamily::kSigned;
    result.width = 32;
  } else if (name == "int64") {
    result.family = DtypeFamily::kSigned;
    result.width = 64;
  } else if (name == "uint8") {
    result.family = DtypeFamily::kUnsigned;
    result.width = 8;
  } else if (name == "uint16") {
    result.family = DtypeFamily::kUnsigned;
    result.width = 16;
  } else if (name == "uint32") {
    result.family = DtypeFamily::kUnsigned;
    result.width = 32;
  } else if (name == "uint64") {
    result.family = DtypeFamily::kUnsigned;
    result.width = 64;
  } else if (name == "float32") {
    result.family = DtypeFamily::kFloat;
    result.width = 32;
  } else if (name == "float64") {
    result.family = DtypeFamily::kFloat;
    result.width = 64;
  }
  return result;
}

std::string signed_dtype(int width) {
  if (width <= 8) return "int8";
  if (width <= 16) return "int16";
  if (width <= 32) return "int32";
  return "int64";
}

std::string unsigned_dtype(int width) {
  if (width <= 8) return "uint8";
  if (width <= 16) return "uint16";
  if (width <= 32) return "uint32";
  return "uint64";
}

bool category_value_equal(const EventAttribute& left,
                          const EventAttribute& right) {
  const bool left_null =
      left.is_null || left.first_class_state == FirstClassAttributeState::kNull;
  const bool right_null =
      right.is_null || right.first_class_state == FirstClassAttributeState::kNull;
  if (left_null != right_null || left.container != right.container ||
      left.children.size() != right.children.size())
    return false;
  if (left_null) return true;
  if (const auto left_numeric = numeric_value(left)) {
    const auto right_numeric = numeric_value(right);
    if (!right_numeric || compare_numeric(*left_numeric, *right_numeric) != 0)
      return false;
  } else if (const auto left_date = native_date_value(left)) {
    const auto right_date = native_date_value(right);
    if (!right_date || *left_date != *right_date) return false;
  } else if (left.container == AttributeContainerKind::kScalar &&
             (left.kind != right.kind || left.value != right.value)) {
    return false;
  } else if (left.container == AttributeContainerKind::kTuple &&
             left.kind != right.kind) {
    return false;
  }
  for (std::size_t index = 0; index < left.children.size(); ++index)
    if (!category_value_equal(left.children[index], right.children[index]))
      return false;
  return true;
}

void append_category_key_part(std::string& key, std::string_view value) {
  key += std::to_string(value.size());
  key.push_back(':');
  key.append(value);
  key.push_back(';');
}

std::string numeric_category_key(const NumericValue& value) {
  if (value.kind == NumericValue::Kind::kSigned)
    return "integer:" + std::to_string(value.signed_value);
  if (value.kind == NumericValue::Kind::kUnsigned)
    return "integer:" + std::to_string(value.unsigned_value);
  const double real = value.floating_value;
  if (std::isnan(real)) return "real:nan";
  if (std::isinf(real)) return real < 0 ? "real:-inf" : "real:inf";
  if (std::trunc(real) == real) {
    if (real >= -0x1p63 && real < 0x1p63)
      return "integer:" + std::to_string(static_cast<std::int64_t>(real));
    if (real >= 0 && real < 0x1p64)
      return "integer:" + std::to_string(static_cast<std::uint64_t>(real));
  }
  std::ostringstream text;
  text << std::setprecision(std::numeric_limits<double>::max_digits10) << real;
  return "real:" + text.str();
}

std::string category_semantic_key(const EventAttribute& attribute) {
  std::string key = "container:" +
                    std::to_string(static_cast<int>(attribute.container)) +
                    ";";
  if (attribute.is_null ||
      attribute.first_class_state == FirstClassAttributeState::kNull) {
    key += "null:" + std::to_string(attribute.children.size()) + ";";
    return key;
  }
  if (const auto numeric = numeric_value(attribute)) {
    key += "numeric:" + numeric_category_key(*numeric) + ";";
  } else if (const auto date = native_date_value(attribute)) {
    key += "date:" + std::to_string(date->first) + ":" +
           std::to_string(date->second) + ";";
  } else if (attribute.container == AttributeContainerKind::kScalar) {
    key += "scalar:" + std::to_string(static_cast<int>(attribute.kind)) + ":";
    append_category_key_part(key, attribute.value);
  } else if (attribute.container == AttributeContainerKind::kTuple) {
    key += "tuple:" + std::to_string(static_cast<int>(attribute.kind)) + ";";
  }
  for (const auto& child : attribute.children)
    append_category_key_part(key, category_semantic_key(child));
  return key;
}

CategoryLookup::CategoryLookup(
    const EventTableCategoricalDtype* descriptor)
    : descriptor_(descriptor) {
  if (!descriptor_) return;
  buckets_.reserve(descriptor_->categories.size());
  for (std::size_t index = 0; index < descriptor_->categories.size(); ++index)
    buckets_[category_semantic_key(descriptor_->categories[index])].push_back(index);
}

std::optional<std::size_t> CategoryLookup::code(
    const std::optional<EventAttribute>& cell) const {
  if (null_cell(cell) ||
      cell->first_class_state == FirstClassAttributeState::kNull ||
      !descriptor_)
    return std::nullopt;
  const auto found = buckets_.find(category_semantic_key(*cell));
  if (found == buckets_.end()) return std::nullopt;
  for (const auto index : found->second)
    if (category_value_equal(*cell, descriptor_->categories[index])) return index;
  return std::nullopt;
}

std::optional<std::size_t> category_code(
    const std::optional<EventAttribute>& cell,
    const CategoryLookup* lookup) {
  return lookup ? lookup->code(cell) : std::nullopt;
}

bool category_descriptor_compatible(
    const EventTableCategoricalDtype& left,
    const EventTableCategoricalDtype& right) {
  if (left.categories_dtype != right.categories_dtype ||
      left.ordered != right.ordered ||
      left.categories.size() != right.categories.size())
    return false;
  if (left.ordered) {
    for (std::size_t index = 0; index < left.categories.size(); ++index)
      if (!category_value_equal(left.categories[index], right.categories[index]))
        return false;
    return true;
  }
  const CategoryLookup right_lookup(&right);
  for (const auto& category : left.categories) {
    if (!right_lookup.code(category)) return false;
  }
  return true;
}

enum class CategoryValueFamily { kUnknown, kString, kNumeric, kOther };

CategoryValueFamily category_family(
    const std::optional<EventTableCategoricalDtype>& descriptor) {
  if (!descriptor) return CategoryValueFamily::kUnknown;
  if (descriptor->categories.empty()) return CategoryValueFamily::kUnknown;
  CategoryValueFamily family = CategoryValueFamily::kUnknown;
  for (const auto& category : descriptor->categories) {
    const auto current = numeric_value(category)
                             ? CategoryValueFamily::kNumeric
                             : (category.container == AttributeContainerKind::kScalar &&
                                (category.kind == AttributeKind::kString ||
                                 category.kind == AttributeKind::kId))
                                   ? CategoryValueFamily::kString
                                   : CategoryValueFamily::kOther;
    if (family == CategoryValueFamily::kUnknown) family = current;
    else if (family != current) return CategoryValueFamily::kOther;
  }
  return family;
}

std::string category_underlying_dtype(
    const std::optional<EventTableCategoricalDtype>& descriptor) {
  if (!descriptor || descriptor->categories_dtype.empty()) return {};
  if (parse_dtype(descriptor->categories_dtype).family == DtypeFamily::kUnknown)
    return {};
  return descriptor->categories_dtype;
}

std::string promote_dtype_pair(
    const std::string& left_name, const std::string& right_name,
    const std::optional<EventTableCategoricalDtype>& left_category,
    const std::optional<EventTableCategoricalDtype>& right_category);

std::string category_fallback_dtype(
    const std::optional<EventTableCategoricalDtype>& left,
    const std::optional<EventTableCategoricalDtype>& right) {
  const auto left_underlying = category_underlying_dtype(left);
  const auto right_underlying = category_underlying_dtype(right);
  if (left_underlying.empty() || right_underlying.empty()) return "object";
  return promote_dtype_pair(left_underlying, right_underlying, {}, {});
}

std::string promote_dtype_pair(
    const std::string& left_name, const std::string& right_name,
    const std::optional<EventTableCategoricalDtype>& left_category = {},
    const std::optional<EventTableCategoricalDtype>& right_category = {}) {
  const auto left = parse_dtype(left_name);
  const auto right = parse_dtype(right_name);
  if (left.family == DtypeFamily::kUnknown ||
      right.family == DtypeFamily::kUnknown)
    return "object";
  if (left_name == right_name &&
      !(left.family == DtypeFamily::kCategory &&
        right.family == DtypeFamily::kCategory))
    return left_name;

  if (left.family == DtypeFamily::kCategory &&
      right.family == DtypeFamily::kCategory) {
    if (left_category && right_category &&
        category_descriptor_compatible(*left_category, *right_category))
      return "category";
    if (!left_category || !right_category) return "category";
    return category_fallback_dtype(left_category, right_category);
  }
  if (left.family == DtypeFamily::kCategory ||
      right.family == DtypeFamily::kCategory) {
    const bool category_is_left = left.family == DtypeFamily::kCategory;
    const auto& category = category_is_left ? left_category : right_category;
    const auto& other_name = category_is_left ? right_name : left_name;
    const auto other = category_is_left ? right : left;
    const auto family = category_family(category);
    if (other.family == DtypeFamily::kObject) return "object";
    if (other.family == DtypeFamily::kString) {
      return family == CategoryValueFamily::kString
                 ? (other.extension ? "string" : "str")
                 : "object";
    }
    if (other.family == DtypeFamily::kCategory) return "category";
    if (family != CategoryValueFamily::kNumeric) return "object";
    const auto underlying = category_underlying_dtype(category);
    if (underlying.empty()) return "object";
    if (category_is_left)
      return promote_dtype_pair(underlying, other_name);
    return promote_dtype_pair(other_name, underlying);
  }

  if (left.family == DtypeFamily::kObject ||
      right.family == DtypeFamily::kObject)
    return "object";
  if (left.family == DtypeFamily::kString ||
      right.family == DtypeFamily::kString) {
    if (left.family == DtypeFamily::kString &&
        right.family == DtypeFamily::kString)
      return left.extension || right.extension ? "string" : "str";
    return "object";
  }
  if (left.family == DtypeFamily::kDatetime ||
      right.family == DtypeFamily::kDatetime) {
    if (left.family == DtypeFamily::kDatetime &&
        right.family == DtypeFamily::kDatetime) {
      if (left.datetime_timezone != right.datetime_timezone) return "object";
      const auto width = std::max(left.width, right.width);
      const auto resolution =
          width == 64 ? "ns" : width == 32 ? "us" : width == 16 ? "ms" : "s";
      if (left.datetime_timezone.empty())
        return std::string("datetime64[") + resolution + "]";
      return std::string("datetime64[") + resolution + ", " +
             left.datetime_timezone + "]";
    }
    return "object";
  }

  // Nullable boolean only combines with its plain counterpart.  A nullable
  // numeric and boolean pair is object in pandas, while plain bool is an
  // integer-like dtype for integer promotion.
  if (left.name == "boolean" || right.name == "boolean") {
    if (left.name == "boolean" && right.name == "bool") return "boolean";
    if (right.name == "boolean" && left.name == "bool") return "boolean";
    return "object";
  }
  if (left.name == "Int64" || right.name == "Int64" ||
      left.name == "UInt64" || right.name == "UInt64" ||
      left.name == "Float64" || right.name == "Float64") {
    if (left.name == "Int64" && right.name == "Int64") return "Int64";
    if (left.name == "UInt64" && right.name == "UInt64") return "UInt64";
    if (left.name == "Float64" && right.name == "Float64") return "Float64";
    if (left.family == DtypeFamily::kBoolean ||
        right.family == DtypeFamily::kBoolean)
      return "object";
    if (left.name == "UInt64" || right.name == "UInt64") {
      if (left.name == "UInt64" && right.name == "UInt64") return "UInt64";
      if (left.family == DtypeFamily::kUnsigned &&
          right.family == DtypeFamily::kUnsigned)
        return "UInt64";
      return "Float64";
    }
    if (left.name == "Float64" || right.name == "Float64") return "Float64";
    if (left.name == "Int64" || right.name == "Int64") {
      if (left.family == DtypeFamily::kFloat ||
          right.family == DtypeFamily::kFloat)
        return "Float64";
      if ((left.family == DtypeFamily::kUnsigned && left.width == 64) ||
          (right.family == DtypeFamily::kUnsigned && right.width == 64))
        return "Float64";
      return "Int64";
    }
  }

  if (left.family == DtypeFamily::kBoolean &&
      right.family == DtypeFamily::kBoolean)
    return "bool";
  if (left.family == DtypeFamily::kBoolean) {
    if (right.family == DtypeFamily::kFloat)
      return "object";  // bool base + float incoming retains object kinds.
    if (right.family == DtypeFamily::kSigned ||
        right.family == DtypeFamily::kUnsigned)
      return right.name;
  }
  if (right.family == DtypeFamily::kBoolean) {
    if (left.family == DtypeFamily::kFloat)
      return left.name;  // float base + bool incoming recasts to float.
    if (left.family == DtypeFamily::kSigned ||
        left.family == DtypeFamily::kUnsigned)
      return left.name;
  }
  if (left.family == DtypeFamily::kFloat ||
      right.family == DtypeFamily::kFloat) {
    const auto width = std::max(left.family == DtypeFamily::kFloat ? left.width : 0,
                                right.family == DtypeFamily::kFloat ? right.width : 0);
    if (width >= 64) return "float64";
    const auto integer_width = std::max(
        left.family == DtypeFamily::kSigned || left.family == DtypeFamily::kUnsigned
            ? left.width
            : 1,
        right.family == DtypeFamily::kSigned || right.family == DtypeFamily::kUnsigned
            ? right.width
            : 1);
    return integer_width <= 16 ? "float32" : "float64";
  }

  if ((left.family == DtypeFamily::kSigned ||
       left.family == DtypeFamily::kUnsigned) &&
      (right.family == DtypeFamily::kSigned ||
       right.family == DtypeFamily::kUnsigned)) {
    if (left.family == right.family)
      return left.family == DtypeFamily::kSigned
                 ? signed_dtype(std::max(left.width, right.width))
                 : unsigned_dtype(std::max(left.width, right.width));
    const auto signed_width = left.family == DtypeFamily::kSigned ? left.width : right.width;
    const auto unsigned_width = left.family == DtypeFamily::kUnsigned ? left.width : right.width;
    if (signed_width > unsigned_width) return signed_dtype(signed_width);
    if (signed_width < 64 && signed_width + 1 > unsigned_width)
      return signed_dtype(std::min(64, signed_width * 2));
    if (unsigned_width < 64) return signed_dtype(std::min(64, unsigned_width * 2));
    return "float64";
  }
  return "object";
}

std::string promote_dtype_sources(
    const std::vector<std::pair<std::string,
                                std::optional<EventTableCategoricalDtype>>>& sources,
    bool missing) {
  if (sources.empty()) return "object";
  std::string result = sources.front().first;
  std::optional<EventTableCategoricalDtype> descriptor = sources.front().second;
  for (std::size_t index = 1; index < sources.size(); ++index) {
    result = promote_dtype_pair(result, sources[index].first, descriptor,
                                sources[index].second);
    if (result != "category") descriptor.reset();
    else if (!descriptor || !sources[index].second)
      descriptor.reset();
  }
  const auto parsed = parse_dtype(result);
  if (missing && !parsed.extension) {
    if (parsed.family == DtypeFamily::kSigned ||
        parsed.family == DtypeFamily::kUnsigned)
      return "float64";
    // The default "str" dtype is NaN-capable, so only bool falls back.
    if (parsed.family == DtypeFamily::kBoolean) return "object";
  }
  return result;
}

const EventTableCategoricalDtype* category_descriptor_at(
    const EventTable& table, std::size_t index) {
  if (table.categorical_dtypes.empty() || index >= table.categorical_dtypes.size() ||
      !table.categorical_dtypes[index])
    return nullptr;
  return &*table.categorical_dtypes[index];
}

std::optional<EventAttribute> coerce_cell_to_dtype(
    const std::optional<EventAttribute>& source, const std::string& column,
    const std::string& dtype) {
  if (!source) return std::nullopt;
  EventAttribute result = *source;
  result.key = column;
  if (result.is_null ||
      result.first_class_state == FirstClassAttributeState::kNull)
    return result;
  const auto parsed = parse_dtype(dtype);
  if (parsed.family == DtypeFamily::kObject ||
      parsed.family == DtypeFamily::kCategory ||
      parsed.family == DtypeFamily::kUnknown)
    return result;
  const auto numeric = numeric_value(result);
  if (parsed.family == DtypeFamily::kFloat) {
    if (!numeric) return result;
    double value = 0.0;
    if (numeric->kind == NumericValue::Kind::kFloat) value = numeric->floating_value;
    else if (numeric->kind == NumericValue::Kind::kSigned)
      value = static_cast<double>(numeric->signed_value);
    else value = static_cast<double>(numeric->unsigned_value);
    if (parsed.width == 32)
      value = static_cast<double>(static_cast<float>(value));
    result.kind = AttributeKind::kFloat;
    std::ostringstream text;
    text << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
    result.value = text.str();
    result.typed_value = value;
    return result;
  }
  if (parsed.family == DtypeFamily::kSigned ||
      parsed.family == DtypeFamily::kUnsigned) {
    if (!numeric) return result;
    std::int64_t signed_value = 0;
    std::uint64_t unsigned_value = 0;
    bool exact = true;
    if (numeric->kind == NumericValue::Kind::kSigned) {
      signed_value = numeric->signed_value;
      if (parsed.family == DtypeFamily::kUnsigned && signed_value < 0)
        exact = false;
      unsigned_value = static_cast<std::uint64_t>(signed_value);
    } else if (numeric->kind == NumericValue::Kind::kUnsigned) {
      unsigned_value = numeric->unsigned_value;
      if (parsed.family == DtypeFamily::kSigned &&
          unsigned_value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        exact = false;
      if (exact) signed_value = static_cast<std::int64_t>(unsigned_value);
    } else {
      const auto floating = numeric->floating_value;
      if (!std::isfinite(floating) || std::trunc(floating) != floating)
        exact = false;
      if (parsed.family == DtypeFamily::kSigned) {
        // 2^63 is exactly representable but is outside int64's half-open
        // range; using the half-open bound avoids an out-of-range cast.
        if (floating < -9223372036854775808.0 ||
            floating >= 9223372036854775808.0)
          exact = false;
        if (exact) signed_value = static_cast<std::int64_t>(floating);
      } else {
        // 2^64 is exactly representable as a double and is the exclusive
        // upper bound for uint64 conversion.
        if (floating < 0.0 || floating >= 18446744073709551616.0)
          exact = false;
        if (exact) unsigned_value = static_cast<std::uint64_t>(floating);
      }
    }
    if (!exact) return result;
    result.kind = AttributeKind::kInt;
    if (parsed.family == DtypeFamily::kSigned) {
      result.value = std::to_string(signed_value);
      result.typed_value = signed_value;
    } else {
      result.value = std::to_string(unsigned_value);
      result.typed_value = unsigned_value;
    }
    return result;
  }
  if (parsed.family == DtypeFamily::kBoolean) {
    if (const auto* value = std::get_if<bool>(&result.typed_value)) {
      result.kind = AttributeKind::kBoolean;
      result.value = *value ? "True" : "False";
    }
    return result;
  }
  if (parsed.family == DtypeFamily::kString) {
    if (result.kind == AttributeKind::kString || result.kind == AttributeKind::kId)
      result.kind = AttributeKind::kString;
    return result;
  }
  return result;
}

// Pairs (incoming row, relation row) in the order pandas' inner merge emits
// them. Rows normally follow the incoming table with relation rows ascending.
// When the pair count equals the incoming row count, pandas instead takes
// the key-grouped pairs at each incoming row's position in a stable sort by
// key code. Codes follow first appearance in the incoming keys, or, for
// numeric keys, in the relation keys and then the incoming keys; null keys
// form the last group. Numeric keys skip the regrouping when the incoming
// keys are unique and both key columns are monotonic increasing.
std::vector<std::pair<std::size_t, std::size_t>> inner_merge_pairs(
    const EventTable& incoming, std::size_t incoming_key,
    const EventTable& relations, std::size_t relation_key) {
  std::vector<std::pair<std::size_t, std::size_t>> pairs;
  for (std::size_t row = 0; row < incoming.rows.size(); ++row)
    for (std::size_t relation = 0; relation < relations.rows.size(); ++relation)
      if (cell_equal(incoming.rows[row][incoming_key],
                     relations.rows[relation][relation_key]))
        pairs.push_back({row, relation});
  if (pairs.size() != incoming.rows.size() || pairs.empty()) return pairs;

  const auto numeric_family = [](const EventTable& table, std::size_t key) {
    if (table.dtypes.size() == table.columns.size()) {
      const auto family = parse_dtype(table.dtypes[key]).family;
      return family == DtypeFamily::kSigned || family == DtypeFamily::kUnsigned ||
             family == DtypeFamily::kFloat || family == DtypeFamily::kBoolean;
    }
    for (const auto& row : table.rows)
      if (!null_cell(row[key]) && !numeric_value(*row[key])) return false;
    return true;
  };
  const bool numeric = numeric_family(incoming, incoming_key) &&
                       numeric_family(relations, relation_key);
  const auto monotonic = [](const EventTable& table, std::size_t key) {
    std::optional<NumericValue> previous;
    for (const auto& row : table.rows) {
      if (null_cell(row[key])) return false;
      const auto current = numeric_value(*row[key]);
      if (!current) return false;
      if (previous && compare_numeric(*previous, *current) > 0) return false;
      previous = current;
    }
    return true;
  };

  std::vector<const std::optional<EventAttribute>*> codes;
  const auto code_of = [&](const std::optional<EventAttribute>& cell) {
    if (null_cell(cell)) return codes.size();
    for (std::size_t code = 0; code < codes.size(); ++code)
      if (cell_equal(*codes[code], cell)) return code;
    return codes.size();
  };
  const auto register_keys = [&](const EventTable& table, std::size_t key) {
    for (const auto& row : table.rows)
      if (!null_cell(row[key]) && code_of(row[key]) == codes.size())
        codes.push_back(&row[key]);
  };
  if (numeric) register_keys(relations, relation_key);
  register_keys(incoming, incoming_key);
  std::vector<std::size_t> incoming_codes;
  bool unique = true;
  for (const auto& row : incoming.rows) {
    const auto code = code_of(row[incoming_key]);
    unique = unique && std::find(incoming_codes.begin(), incoming_codes.end(),
                                 code) == incoming_codes.end();
    incoming_codes.push_back(code);
  }
  if (numeric && unique && monotonic(incoming, incoming_key) &&
      monotonic(relations, relation_key))
    return pairs;

  std::vector<std::size_t> sorter(incoming.rows.size());
  for (std::size_t row = 0; row < sorter.size(); ++row) sorter[row] = row;
  std::stable_sort(sorter.begin(), sorter.end(),
                   [&](std::size_t left, std::size_t right) {
                     return incoming_codes[left] < incoming_codes[right];
                   });
  std::vector<std::vector<std::pair<std::size_t, std::size_t>>> by_row(
      incoming.rows.size());
  for (const auto& pair : pairs) by_row[pair.first].push_back(pair);
  std::vector<std::pair<std::size_t, std::size_t>> grouped;
  grouped.reserve(pairs.size());
  for (const auto row : sorter)
    grouped.insert(grouped.end(), by_row[row].begin(), by_row[row].end());
  std::vector<std::pair<std::size_t, std::size_t>> ordered(pairs.size());
  for (std::size_t position = 0; position < sorter.size(); ++position)
    ordered[sorter[position]] = grouped[position];
  return ordered;
}

}  // namespace

std::vector<CaseRelation> parse_case_relations_csv(std::istream& input,
                                                    std::string left_column,
                                                    std::string right_column) {
  if (left_column.empty() || right_column.empty() ||
      left_column == right_column)
    throw std::invalid_argument("case relations: relation columns must differ");

  std::string line;
  std::vector<std::string> header;
  while (std::getline(input, line)) {
    if (!trim(line).empty()) {
      header = parse_csv_row(line);
      break;
    }
  }
  if (header.empty()) throw std::invalid_argument("case relations: missing header");
  std::set<std::string> header_names;
  for (const auto& name : header) {
    if (!header_names.insert(name).second)
      throw std::invalid_argument("case relations: duplicate header " + name);
  }
  const auto find_column = [&](const std::string& name) {
    const auto it = std::find(header.begin(), header.end(), name);
    if (it == header.end())
      throw std::invalid_argument("case relations: missing column " + name);
    return static_cast<std::size_t>(it - header.begin());
  };
  const std::size_t left_index = find_column(left_column);
  const std::size_t right_index = find_column(right_column);

  std::vector<CaseRelation> relations;
  while (std::getline(input, line)) {
    if (trim(line).empty()) continue;
    const auto fields = parse_csv_row(line);
    if (fields.size() != header.size())
      throw std::invalid_argument("case relations: row has wrong column count");
    if (fields[left_index].empty() || fields[right_index].empty())
      throw std::invalid_argument("case relations: case ids must not be empty");
    relations.push_back({fields[left_index], fields[right_index]});
  }
  return relations;
}

EventTable parse_case_relations_table_csv(std::istream& input) {
  std::string line;
  std::vector<std::string> header;
  while (std::getline(input, line)) {
    if (!trim(line).empty()) {
      header = parse_csv_row(line);
      break;
    }
  }
  if (header.empty())
    throw std::invalid_argument("case relations: missing header");

  std::set<std::string> header_names;
  for (const auto& name : header) {
    if (name.empty())
      throw std::invalid_argument(
          "case relations: relation header names must not be empty");
    if (!header_names.insert(name).second)
      throw std::invalid_argument("case relations: duplicate header " + name);
  }

  EventTable table;
  table.columns = header;
  // The table adapter parses every field as text, so preserve that declared
  // dtype even when the relation file has no data rows.
  table.dtypes.assign(header.size(), "str");
  while (std::getline(input, line)) {
    if (trim(line).empty()) continue;
    const auto fields = parse_csv_row(line);
    if (fields.size() != header.size())
      throw std::invalid_argument("case relations: row has wrong column count");
    EventTable::Row row;
    row.reserve(fields.size());
    for (std::size_t index = 0; index < fields.size(); ++index)
      row.emplace_back(EventAttribute{header[index], fields[index],
                                      AttributeKind::kString});
    table.rows.push_back(std::move(row));
  }
  table.validate();
  return table;
}

namespace {

// One key column as read_csv types it (reference_csv_column), the way the
// reference's merge compares and concatenates it.
using KeyKind = ReferenceCsvColumnKind;

struct KeyColumn {
  KeyKind kind = KeyKind::kObject;
  std::vector<std::optional<std::string>> text;  // nullopt: NA
  bool has_na = false;
};

KeyColumn key_column(const std::vector<std::string>& cells) {
  auto typed = reference_csv_column(cells);
  KeyColumn column{typed.kind, std::move(typed.text), false};
  for (const auto& cell : column.text)
    if (!cell) column.has_na = true;
  // A bool column with an NA cell is an object column of bools; a column
  // with no cells at all is an empty object column.
  if (column.kind == KeyKind::kBoolean && column.has_na) column.kind = KeyKind::kObject;
  if (cells.empty()) column.kind = KeyKind::kObject;
  return column;
}

// The kind of a log's case-id column, re-read from its typed case-id text
// (an int64 column with NA was already rendered as floats there).
KeyKind log_key_kind(const std::vector<std::string>& case_ids) {
  return key_column(case_ids).kind;
}

bool numeric_kind(KeyKind kind) {
  return kind == KeyKind::kInteger || kind == KeyKind::kFloat;
}

// The value of a typed int64/float64/bool key text.
struct KeyNumber {
  bool exact = false;  // an int64 value
  std::int64_t integer = 0;
  double number = 0.0;
};

std::optional<KeyNumber> key_number(KeyKind kind, const std::string& text) {
  if (kind == KeyKind::kBoolean) {
    const bool value = text == "True";
    return KeyNumber{true, value ? 1 : 0, value ? 1.0 : 0.0};
  }
  if (kind == KeyKind::kInteger) {
    std::int64_t integer = 0;
    const auto* end = text.data() + text.size();
    const auto [ptr, error] = std::from_chars(text.data(), end, integer);
    if (error == std::errc{} && ptr == end)
      return KeyNumber{true, integer, static_cast<double>(integer)};
  }
  if (kind == KeyKind::kInteger || kind == KeyKind::kFloat) {
    char* end = nullptr;
    const double number = std::strtod(text.c_str(), &end);
    if (end != text.c_str()) return KeyNumber{false, 0, number};
  }
  return std::nullopt;
}

bool numbers_equal(const KeyNumber& left, const KeyNumber& right) {
  if (left.exact && right.exact) return left.integer == right.integer;
  return left.number == right.number;
}

std::string integer_text(const KeyNumber& number) {
  return std::to_string(number.integer);
}

std::string float_text(const KeyNumber& number) {
  return reference_csv_float_text(number.number);
}

// The text a joined key cell must have to meet the log's case id, as pandas'
// typed merge compares keys: numbers and bools meet by value (True == 1 ==
// 1.0), a str never meets a bool, and a str column cannot be merged with a
// numeric one (pandas raises). An NA key meets nothing. nullopt: the row
// meets no case. Object columns keep their typed text.
std::vector<std::optional<std::string>> joined_key_text(
    KeyKind log_kind, const KeyColumn& relation, const char* side) {
  const auto count = relation.text.size();
  if (log_kind == KeyKind::kObject || relation.kind == KeyKind::kObject ||
      (log_kind == KeyKind::kText && relation.kind == KeyKind::kText))
    return relation.text;
  std::vector<std::optional<std::string>> result(count);
  const bool log_bool = log_kind == KeyKind::kBoolean;
  if (log_kind == KeyKind::kText || relation.kind == KeyKind::kText) {
    if (log_bool || relation.kind == KeyKind::kBoolean) return result;
    throw std::invalid_argument(
        std::string("case relations: the ") + side +
        " case id column and its relation column cannot be joined (str and number)");
  }
  for (std::size_t row = 0; row < count; ++row) {
    if (!relation.text[row]) continue;
    const auto number = key_number(relation.kind, *relation.text[row]);
    if (!number) continue;
    if (log_bool) {
      if (number->number == 1.0) result[row] = "True";
      else if (number->number == 0.0) result[row] = "False";
    } else if (log_kind == KeyKind::kFloat) {
      result[row] = float_text(*number);
    } else if (number->exact) {
      result[row] = integer_text(*number);
    } else if (std::isfinite(number->number) &&
               std::trunc(number->number) == number->number &&
               number->number >= -9223372036854775808.0 &&
               number->number < 9223372036854775808.0) {
      result[row] = std::to_string(static_cast<std::int64_t>(number->number));
    }
  }
  return result;
}

enum class ConcatKind { kInteger, kFloat, kBoolean, kObject };

// The dtype of the case column once the base rows (`log`) and the relation
// rows (`relation`, its LEFT column) are concatenated.
ConcatKind concat_kind(KeyKind log, KeyKind relation) {
  if (log == KeyKind::kText || log == KeyKind::kObject ||
      relation == KeyKind::kText || relation == KeyKind::kObject)
    return ConcatKind::kObject;
  switch (log) {
    case KeyKind::kInteger:
      return relation == KeyKind::kFloat ? ConcatKind::kFloat : ConcatKind::kInteger;
    case KeyKind::kFloat:
      return ConcatKind::kFloat;
    case KeyKind::kBoolean:
      return relation == KeyKind::kInteger  ? ConcatKind::kInteger
             : relation == KeyKind::kFloat ? ConcatKind::kObject
                                           : ConcatKind::kBoolean;
    default:
      return ConcatKind::kObject;
  }
}

// A key text rendered in the concatenated column. In an object column every
// cell keeps its own rendering, but numbers (and bools) that are equal group
// as one case, under the spelling seen first (base rows, then relations).
struct ConcatRenderer {
  ConcatKind kind;
  std::vector<std::pair<KeyNumber, std::string>> seen;

  std::string render(KeyKind source, const std::string& text) {
    if (source == KeyKind::kText || source == KeyKind::kObject) return text;
    const auto number = key_number(source, text);
    if (!number) return text;
    switch (kind) {
      case ConcatKind::kInteger:
        return source == KeyKind::kBoolean ? integer_text(*number) : text;
      case ConcatKind::kFloat:
        return source == KeyKind::kFloat ? text : float_text(*number);
      case ConcatKind::kBoolean:
        return text;
      case ConcatKind::kObject:
        for (const auto& [value, spelling] : seen)
          if (numbers_equal(value, *number)) return spelling;
        seen.emplace_back(*number, text);
        return text;
    }
    return text;
  }
};

std::optional<std::size_t> table_column(const EventTable& table,
                                        const std::string& name) {
  const auto found = std::find(table.columns.begin(), table.columns.end(), name);
  if (found == table.columns.end()) return std::nullopt;
  return static_cast<std::size_t>(found - table.columns.begin());
}

std::vector<std::string> column_texts(const EventTable& table, std::size_t index) {
  std::vector<std::string> cells;
  cells.reserve(table.rows.size());
  for (const auto& row : table.rows)
    cells.push_back(index < row.size() && row[index] ? row[index]->value
                                                     : std::string{});
  return cells;
}

std::vector<std::string> present_texts(const EventTable& table, std::size_t index) {
  std::vector<std::string> cells;
  cells.reserve(table.rows.size());
  for (const auto& row : table.rows)
    if (index < row.size() && row[index] && !row[index]->is_null)
      cells.push_back(row[index]->value);
  return cells;
}

}  // namespace

void type_csv_case_relation_keys(EventTable& relations,
                                 const std::string& left_column,
                                 const std::string& right_column,
                                 EventTable* base_csv, EventTable* incoming_csv,
                                 const std::string& case_key) {
  const auto left_index = table_column(relations, left_column);
  const auto right_index = table_column(relations, right_column);
  if (incoming_csv && right_index) {
    if (const auto case_index = table_column(*incoming_csv, case_key)) {
      const auto relation = key_column(column_texts(relations, *right_index));
      const auto joined = joined_key_text(
          log_key_kind(present_texts(*incoming_csv, *case_index)), relation,
          "incoming");
      for (std::size_t row = 0; row < relations.rows.size(); ++row) {
        auto& cell = relations.rows[row][*right_index];
        if (!cell) continue;
        if (joined[row]) {
          cell->value = *joined[row];
        } else {
          cell->value.clear();
          cell->is_null = true;
        }
      }
    }
  }
  if (base_csv && left_index) {
    if (const auto case_index = table_column(*base_csv, case_key)) {
      const auto relation = key_column(column_texts(relations, *left_index));
      const auto base_kind = log_key_kind(present_texts(*base_csv, *case_index));
      ConcatRenderer renderer{concat_kind(base_kind, relation.kind), {}};
      for (auto& row : base_csv->rows) {
        auto& cell = row[*case_index];
        if (cell && !cell->is_null) cell->value = renderer.render(base_kind, cell->value);
      }
      // An NA LEFT cell keeps its text, as before.
      for (std::size_t row = 0; row < relations.rows.size(); ++row) {
        auto& cell = relations.rows[row][*left_index];
        if (cell && relation.text[row])
          cell->value = renderer.render(relation.kind, *relation.text[row]);
      }
    }
  }
}

void type_case_relations_like_csv_logs(std::vector<CaseRelation>& relations,
                                       const EventLog* left_csv,
                                       const EventLog* right_csv) {
  std::vector<bool> keep(relations.size(), true);
  const auto apply = [&](const EventLog& log, std::string CaseRelation::*member,
                         const char* side) {
    std::vector<std::string> cells;
    cells.reserve(relations.size());
    for (const auto& relation : relations) cells.push_back(relation.*member);
    std::vector<std::string> case_ids;
    case_ids.reserve(log.traces.size());
    for (const auto& trace : log.traces) case_ids.push_back(trace.case_id);
    const auto joined = joined_key_text(log_key_kind(case_ids), key_column(cells), side);
    for (std::size_t row = 0; row < relations.size(); ++row) {
      if (joined[row]) relations[row].*member = *joined[row];
      else keep[row] = false;
    }
  };
  if (left_csv) {
    apply(*left_csv, &CaseRelation::left_case_id, "left");
    // The reference joins the left log first and stops when no relation met
    // one of its cases, so the right join is not attempted (and cannot fail).
    std::unordered_set<std::string> cases;
    for (const auto& trace : left_csv->traces) cases.insert(trace.case_id);
    bool met = false;
    for (std::size_t row = 0; row < relations.size() && !met; ++row)
      met = keep[row] && cases.contains(relations[row].left_case_id);
    if (!met) {
      relations.clear();
      return;
    }
  }
  if (right_csv) apply(*right_csv, &CaseRelation::right_case_id, "right");
  std::size_t kept = 0;
  for (std::size_t row = 0; row < relations.size(); ++row)
    if (keep[row]) {
      if (kept != row) relations[kept] = std::move(relations[row]);
      ++kept;
    }
  relations.resize(kept);
}

EventLog merge_case_relations(const EventLog& base, const EventLog& incoming,
                              const std::vector<CaseRelation>& relations,
                              const CaseRelationsOptions& options) {
  if (options.variant != CaseRelationsVariant::kPandas)
    throw std::invalid_argument("case relations variant must be pandas");
  if (options.timestamp_key.empty())
    throw std::invalid_argument("case relations: timestamp key must not be empty");
  if (options.case_id_key.empty())
    throw std::invalid_argument("case relations: case id key must not be empty");
  if ((options.relation_left_column.empty() && options.left_suffix.empty()) ||
      (options.relation_right_column.empty() && options.right_suffix.empty()))
    throw std::invalid_argument(
        "case relations: unspecified relation columns require non-empty suffixes");
  const auto relation_left_name =
      options.relation_left_column.empty()
          ? options.case_id_key + options.left_suffix
          : options.relation_left_column;
  const auto relation_right_name =
      options.relation_right_column.empty()
          ? options.case_id_key + options.right_suffix
          : options.relation_right_column;
  if (relation_left_name == relation_right_name)
    throw std::invalid_argument("case relations: relation columns must differ");

  std::unordered_map<std::string, std::vector<std::string>> targets;
  for (const auto& relation : relations) {
    if (relation.left_case_id.empty() || relation.right_case_id.empty())
      throw std::invalid_argument("case relations: case ids must not be empty");
    targets[relation.right_case_id].push_back(relation.left_case_id);
  }

  struct OutputTrace {
    Trace trace;
    std::vector<TaggedEvent> events;
  };
  std::map<std::string, OutputTrace> grouped;
  std::size_t sequence = 0;
  const auto case_value = [&](const Trace& source) -> std::string {
    const auto* value = trace_case_value(source, options.case_id_key);
    if (!value || value->empty())
      throw std::invalid_argument("case relations: trace is missing case id key " +
                                  options.case_id_key);
    return *value;
  };
  const auto add_base = [&](const Trace& source) {
    const auto selected_case_id = case_value(source);
    auto& out = grouped[selected_case_id];
    if (out.trace.case_id.empty()) {
      out.trace.case_id = selected_case_id;
      out.trace.attributes = source.attributes;
      out.trace.properties = source.properties;
    }
    for (const auto& source_event : source.events) {
      Event event = source_event;
      event.case_id = selected_case_id;
      out.events.push_back({std::move(event), sequence++});
    }
  };
  for (const auto& trace : base.traces) add_base(trace);

  for (const auto& source : incoming.traces) {
    const auto target_it = targets.find(case_value(source));
    if (target_it == targets.end()) continue;
    for (const auto& left_case_id : target_it->second) {
      auto& out = grouped[left_case_id];
      if (out.trace.case_id.empty()) out.trace.case_id = left_case_id;
    }
    for (const auto& source_event : source.events) {
      for (const auto& left_case_id : target_it->second) {
        auto& out = grouped[left_case_id];
        Event event = source_event;
        event.case_id = left_case_id;
        out.events.push_back({std::move(event), sequence++});
      }
    }
  }

  EventLog result;
  copy_event_log_context(base, result);
  result.traces.reserve(grouped.size());
  for (auto& [case_id, output] : grouped) {
    for (auto& tagged : output.events)
      tagged.time = event_time(tagged.event, options.timestamp_key);
    std::stable_sort(output.events.begin(), output.events.end(),
                     [&](const TaggedEvent& lhs, const TaggedEvent& rhs) {
      if (lhs.time.has_value() != rhs.time.has_value()) return lhs.time.has_value();
      if (lhs.time != rhs.time) return lhs.time < rhs.time;
      return lhs.sequence < rhs.sequence;
    });
    output.trace.events.reserve(output.events.size());
    for (auto& tagged : output.events)
      output.trace.events.push_back(std::move(tagged.event));
    result.traces.push_back(std::move(output.trace));
  }
  return result;
}

EventTable merge_case_relations(const EventTable& base,
                                const EventTable& incoming,
                                const EventTable& relations,
                                const CaseRelationsOptions& options) {
  if (options.variant != CaseRelationsVariant::kPandas)
    throw std::invalid_argument("case relations variant must be pandas");
  if (options.timestamp_key.empty())
    throw std::invalid_argument("case relations: timestamp key must not be empty");
  if (options.case_id_key.empty())
    throw std::invalid_argument("case relations: case id key must not be empty");
  if ((options.relation_left_column.empty() && options.left_suffix.empty()) ||
      (options.relation_right_column.empty() && options.right_suffix.empty()))
    throw std::invalid_argument(
        "case relations: unspecified relation columns require non-empty suffixes");

  base.validate();
  incoming.validate();
  relations.validate();

  const auto base_case_index = resolve_case_column(base, options.case_id_key);
  const auto incoming_case_index =
      resolve_case_column(incoming, options.case_id_key);
  const auto base_case_name = base.columns[base_case_index];
  const auto relation_left_name =
      options.relation_left_column.empty()
          ? options.case_id_key + options.left_suffix
          : options.relation_left_column;
  const auto relation_right_name =
      options.relation_right_column.empty()
          ? options.case_id_key + options.right_suffix
          : options.relation_right_column;
  if (relation_left_name == relation_right_name)
    throw std::invalid_argument("case relations: relation columns must differ");
  const auto relation_left_index = resolve_relation_column(
      relations, relation_left_name,
      "left relation case id column");
  const auto relation_right_index = resolve_relation_column(
      relations, relation_right_name,
      "right relation case id column");
  const auto base_time_index =
      resolve_timestamp_column(base, options.timestamp_key);
  const auto incoming_time_index =
      resolve_timestamp_column(incoming, options.timestamp_key);
  if (!base_time_index && !incoming_time_index)
    throw std::invalid_argument("case relations: missing timestamp column " +
                                options.timestamp_key);

  reject_incompatible_join_domains(incoming, incoming_case_index, relations,
                                   relation_right_index);
  reject_incompatible_timestamp_domains(base, base_time_index, incoming,
                                        incoming_time_index);

  // The public merge first combines incoming rows with relation rows.  A
  // relation field colliding with an incoming field receives pandas' _x/_y
  // suffixes, while ordinary base/incoming fields remain one aligned column.
  const auto incoming_plans =
      plan_columns(incoming, relations, "_x");
  const auto relation_plans =
      plan_columns(relations, incoming, "_y");
  // Suffixes must not silently fold distinct incoming/relation fields into
  // one table cell. Base columns may still align with the merged columns.
  std::set<std::string> joined_names;
  for (const auto* plans : {&incoming_plans, &relation_plans})
    for (const auto& plan : *plans)
      if (!joined_names.insert(plan.output_name).second)
        throw std::invalid_argument(
            "case relations: relation suffixes create duplicate column " +
            plan.output_name);
  EventTable result;
  result.attributes = base.attributes;
  result.metadata = base.metadata;
  result.columns = base.columns;
  for (const auto& plan : incoming_plans)
    add_output_column(result.columns, plan.output_name);
  for (const auto& plan : relation_plans)
    add_output_column(result.columns, plan.output_name);

  const auto output_index = [&](const std::string& name) {
    const auto found = std::find(result.columns.begin(), result.columns.end(), name);
    if (found == result.columns.end())
      throw std::logic_error("case relations: output column plan is missing");
    return static_cast<std::size_t>(found - result.columns.begin());
  };
  const std::string output_case_name = base_case_name;
  const auto output_case_index = output_index(output_case_name);
  const auto output_timestamp_name =
      base_time_index ? base.columns[*base_time_index]
                      : incoming.columns[*incoming_time_index];
  const auto output_timestamp_index = output_index(output_timestamp_name);
  std::vector<std::optional<std::size_t>> incoming_sources(result.columns.size());
  std::vector<std::optional<std::size_t>> relation_sources(result.columns.size());
  for (const auto& plan : incoming_plans)
    incoming_sources[output_index(plan.output_name)] = plan.source_index;
  for (const auto& plan : relation_plans)
    relation_sources[output_index(plan.output_name)] = plan.source_index;
  const bool coerce_boolean_timestamp =
      (base_time_index && column_has_kind(base, *base_time_index,
                                          AttributeKind::kBoolean) &&
       ((base_time_index && column_has_kind(base, *base_time_index,
                                            AttributeKind::kInt)) ||
        (incoming_time_index && column_has_kind(incoming, *incoming_time_index,
                                                AttributeKind::kInt)))) ||
      (incoming_time_index && column_has_kind(incoming, *incoming_time_index,
                                              AttributeKind::kBoolean) &&
       ((base_time_index && column_has_kind(base, *base_time_index,
                                            AttributeKind::kInt)) ||
        (incoming_time_index && column_has_kind(incoming, *incoming_time_index,
                                                AttributeKind::kInt))));

  const auto coerce_timestamp_cell = [&](EventTable::Row& row) {
    if (!coerce_boolean_timestamp || !row[output_timestamp_index] ||
        null_cell(row[output_timestamp_index]) ||
        row[output_timestamp_index]->kind != AttributeKind::kBoolean)
      return;
    const auto* value = std::get_if<bool>(&row[output_timestamp_index]->typed_value);
    if (!value) return;
    auto& cell = *row[output_timestamp_index];
    cell.kind = AttributeKind::kInt;
    cell.value = *value ? "1" : "0";
    cell.typed_value = static_cast<std::int64_t>(*value ? 1 : 0);
  };

  struct OutputRow {
    EventTable::Row row;
    std::optional<EventAttribute> case_cell;
    std::optional<EventAttribute> timestamp_cell;
    EventTableIndexLabel index_label;
    bool index_label_from_expansion = false;
    std::size_t sequence = 0;
    std::optional<SortValue> case_sort;
    std::optional<SortValue> timestamp_sort;
  };
  std::vector<OutputRow> output_rows;
  output_rows.reserve(base.rows.size());
  std::size_t joined_row_count = 0;

  const auto write_cell = [&](EventTable::Row& row, const std::string& name,
                              const std::optional<EventAttribute>& cell) {
    row[output_index(name)] = copy_cell(cell, name);
  };
  const auto make_base_row = [&](const EventTable::Row& source,
                                 std::size_t source_position,
                                 std::size_t sequence) {
    EventTable::Row row(result.columns.size());
    for (std::size_t index = 0; index < base.columns.size(); ++index)
      write_cell(row, base.columns[index], source[index]);
    coerce_timestamp_cell(row);
    OutputRow output{std::move(row), copy_cell(source[base_case_index],
                                               output_case_name),
                     base_time_index ? copy_cell(source[*base_time_index],
                                                 options.timestamp_key)
                                     : std::nullopt,
                     source_index_label(base, source_position),
                     false,
                     sequence};
    output_rows.push_back(std::move(output));
  };
  std::size_t sequence = 0;
  for (std::size_t position = 0; position < base.rows.size(); ++position)
    make_base_row(base.rows[position], position, sequence++);

  std::size_t incoming_index_sequence = 0;
  for (const auto& [source_position, relation_position] : inner_merge_pairs(
           incoming, incoming_case_index, relations, relation_right_index)) {
    {
      const auto& source = incoming.rows[source_position];
      const auto& relation = relations.rows[relation_position];
      EventTable::Row row(result.columns.size());
      for (const auto& plan : incoming_plans)
        write_cell(row, plan.output_name, source[plan.source_index]);
      for (const auto& plan : relation_plans)
        write_cell(row, plan.output_name, relation[plan.source_index]);
      const auto& left_cell = relation[relation_left_index];
      row[output_case_index] = copy_cell(left_cell, output_case_name);
      coerce_timestamp_cell(row);
      OutputRow output{
          std::move(row), copy_cell(left_cell, output_case_name),
          incoming_time_index
              ? copy_cell(source[*incoming_time_index], options.timestamp_key)
              : std::nullopt,
          implicit_index_label(incoming_index_sequence++),
          true,
          sequence++};
      output_rows.push_back(std::move(output));
      ++joined_row_count;
    }
  }

  // Dtype metadata is complete only when every source declares one dtype per
  // column.  In that case derive each output column from the branch that
  // actually supplies its cells: base rows use base columns, joined rows use
  // relation columns when their planned name wins and incoming columns
  // otherwise.  The output case id is explicitly overwritten by relation
  // LEFT, so the incoming join key does not participate in its dtype.
  const bool complete_dtypes =
      base.dtypes.size() == base.columns.size() &&
      incoming.dtypes.size() == incoming.columns.size() &&
      relations.dtypes.size() == relations.columns.size();
  if (complete_dtypes) {
    result.dtypes.reserve(result.columns.size());
    result.categorical_dtypes.assign(result.columns.size(), std::nullopt);
    const auto source_dtype = [](const EventTable& table,
                                 std::optional<std::size_t> index) {
      return index ? table.dtypes[*index] : std::string{};
    };
    const auto source_category = [](const EventTable& table,
                                    std::optional<std::size_t> index)
        -> std::optional<EventTableCategoricalDtype> {
      if (!index || table.categorical_dtypes.empty() ||
          !table.categorical_dtypes[*index])
        return std::nullopt;
      return table.categorical_dtypes[*index];
    };
    for (std::size_t column = 0; column < result.columns.size(); ++column) {
      const auto base_source = find_column(base, result.columns[column]);
      std::optional<std::size_t> joined_source;
      const EventTable* joined_table = nullptr;
      if (column == output_case_index) {
        joined_source = relation_left_index;
        joined_table = &relations;
      } else if (relation_sources[column]) {
        joined_source = relation_sources[column];
        joined_table = &relations;
      } else if (incoming_sources[column]) {
        joined_source = incoming_sources[column];
        joined_table = &incoming;
      }

      std::vector<std::pair<std::string,
                            std::optional<EventTableCategoricalDtype>>> sources;
      if (base_source)
        sources.push_back({source_dtype(base, base_source),
                           source_category(base, base_source)});
      if (joined_source)
        sources.push_back({source_dtype(*joined_table, joined_source),
                           source_category(*joined_table, joined_source)});

      bool missing = false;
      // Pandas concatenation pads an absent column with nulls even when the
      // frame lacking it is empty.
      if (!base_source) {
        missing = true;
      } else if (!base.rows.empty()) {
        {
          for (const auto& row : base.rows)
            if (null_cell(row[*base_source])) {
              missing = true;
              break;
            }
        }
      }
      if (!joined_source) {
        missing = true;
      } else if (joined_row_count != 0) {
        {
          for (const auto& output : output_rows) {
            if (!output.index_label_from_expansion) continue;
            if (null_cell(output.row[column])) {
              missing = true;
              break;
            }
          }
        }
      }
      result.dtypes.push_back(promote_dtype_sources(sources, missing));
      if (result.dtypes.back() == "category") {
        bool complete_categories = true;
        std::optional<EventTableCategoricalDtype> selected;
        for (const auto& source : sources) {
          if (source.first != "category") continue;
          if (!source.second) {
            complete_categories = false;
            continue;
          }
          if (!selected) selected = source.second;
        }
        if (complete_categories && selected)
          result.categorical_dtypes[column] = std::move(selected);
      }
    }
  }

  // Coerce cells before sorting.  Pandas computes case/timestamp ordering on
  // the merged column dtype, including categorical code order and exact
  // integer comparisons, rather than on the source cell representation.
  if (!result.dtypes.empty()) {
    for (auto& output : output_rows)
      for (std::size_t column = 0; column < result.columns.size(); ++column)
        output.row[column] = coerce_cell_to_dtype(
            output.row[column], result.columns[column], result.dtypes[column]);
  }

  // Build categorical lookups once for the two sort columns and cache the
  // resulting sort values on each row.  Sorting repeatedly scanning a
  // descriptor's categories makes a large merge needlessly quadratic in the
  // category count.
  const auto* case_descriptor =
      category_descriptor_at(result, output_case_index);
  const auto* timestamp_descriptor =
      category_descriptor_at(result, output_timestamp_index);
  std::optional<CategoryLookup> case_lookup;
  std::optional<CategoryLookup> timestamp_lookup;
  if (case_descriptor) case_lookup.emplace(case_descriptor);
  if (timestamp_descriptor) timestamp_lookup.emplace(timestamp_descriptor);
  for (auto& output : output_rows) {
    output.case_sort = sort_value(
        output.row[output_case_index],
        case_lookup ? &*case_lookup : nullptr);
    output.timestamp_sort = sort_value(
        output.row[output_timestamp_index],
        timestamp_lookup ? &*timestamp_lookup : nullptr);
  }

  std::vector<EventTableIndexLabel> preconcat_index_labels;
  preconcat_index_labels.reserve(output_rows.size());
  for (const auto& output : output_rows)
    preconcat_index_labels.push_back(output.index_label);
  std::stable_sort(output_rows.begin(), output_rows.end(),
                   [&](const OutputRow& left, const OutputRow& right) {
    const auto case_order = compare_sort_values(
        *left.case_sort, *right.case_sort);
    if (case_order != 0) return case_order < 0;
    const auto time_order = compare_sort_values(
        *left.timestamp_sort, *right.timestamp_sort);
    if (time_order != 0) return time_order < 0;
    return left.sequence < right.sequence;
  });
  result.rows.reserve(output_rows.size());
  for (auto& output : output_rows) result.rows.push_back(std::move(output.row));

  // Pandas carries the base table index through the stable sort.  Rows
  // introduced by the relation expansion receive a fresh integer label in
  // expansion order; incoming and relation source indexes do not participate
  // in that numbering.  A plain implicit RangeIndex remains implicit only
  // when sorting leaves its labels in positional order.  Combining expanded
  // rows with a non-integer base index promotes an ordinary result index to
  // object, while nullable integer and floating indexes retain their
  // extension/numeric dtype.
  std::vector<EventTableIndexLabel> sorted_index_labels;
  sorted_index_labels.reserve(output_rows.size());
  for (const auto& output : output_rows)
    sorted_index_labels.push_back(output.index_label);
  const bool has_incoming_rows = incoming_index_sequence != 0;
  bool implicit_index_is_identity = output_rows.size() == base.rows.size();
  for (std::size_t position = 0;
       implicit_index_is_identity && position < output_rows.size(); ++position) {
    const auto* label = std::get_if<EventAttribute>(&output_rows[position].index_label.value);
    const auto* value = label ? std::get_if<std::int64_t>(&label->typed_value) : nullptr;
    if (!value || *value != static_cast<std::int64_t>(position))
      implicit_index_is_identity = false;
  }
  const bool need_explicit_implicit_index =
      !base.index && !has_incoming_rows && !implicit_index_is_identity &&
      !output_rows.empty();
  // The relation merge creates fresh positional labels for incoming rows;
  // their source index does not participate in the concat index class.  The
  // base side still determines whether the concat starts from a RangeIndex.
  const bool range_inputs_valid = valid_range_index(base);
  const bool output_range_index =
      (!base.index || base.index->kind == "RangeIndex") &&
      range_inputs_valid &&
      arithmetic_index_labels(preconcat_index_labels) &&
      arithmetic_index_labels(sorted_index_labels);
  if (base.index || has_incoming_rows || need_explicit_implicit_index) {
    EventTableIndex output_index;
    if (output_range_index) {
      output_index.kind = "RangeIndex";
      output_index.dtype = "int64";
    } else if (has_incoming_rows) {
      output_index.kind = "Index";
      if (!base.index || base.index->kind == "RangeIndex" ||
          base.index->dtype == "int64" || base.index->dtype == "Int64" ||
          base.index->dtype == "float64") {
        output_index.dtype = base.index ? base.index->dtype : "int64";
      } else {
        output_index.dtype = "object";
      }
    } else if (base.index) {
      output_index.kind = base.index->kind == "RangeIndex"
                              ? "Index"
                              : base.index->kind;
      output_index.dtype = base.index->dtype;
    } else {
      output_index.kind = "Index";
      output_index.dtype = "int64";
    }
    const auto level_count = output_index.kind == "MultiIndex"
                                 ? base.index->names.size()
                                 : std::size_t{1};
    output_index.names.assign(level_count, std::nullopt);
    output_index.labels.reserve(output_rows.size());
    for (const auto& output : output_rows) {
      if (output_range_index) {
        output_index.labels.push_back(output.index_label);
      } else if (output.index_label_from_expansion &&
          output_index.dtype == "float64") {
        const auto& label = std::get<EventAttribute>(output.index_label.value);
        output_index.labels.push_back(floating_index_label(static_cast<std::size_t>(
            std::get<std::int64_t>(label.typed_value))));
      } else {
        output_index.labels.push_back(output.index_label);
      }
    }
    result.index = std::move(output_index);
  }
  result.validate();
  return result;
}

EventTable merge_case_relations(const EventTable& base,
                                const EventTable& incoming,
                                const std::vector<CaseRelation>& relations,
                                const CaseRelationsOptions& options) {
  if (options.case_id_key.empty())
    throw std::invalid_argument("case relations: case id key must not be empty");
  if ((options.relation_left_column.empty() && options.left_suffix.empty()) ||
      (options.relation_right_column.empty() && options.right_suffix.empty()))
    throw std::invalid_argument(
        "case relations: unspecified relation columns require non-empty suffixes");

  EventTable relation_table;
  const std::string left_column =
      options.relation_left_column.empty()
          ? options.case_id_key + options.left_suffix
          : options.relation_left_column;
  const std::string right_column =
      options.relation_right_column.empty()
          ? options.case_id_key + options.right_suffix
          : options.relation_right_column;
  if (left_column == right_column)
    throw std::invalid_argument("case relations: relation columns must differ");
  relation_table.columns = {left_column, right_column};
  relation_table.dtypes = {"str", "str"};
  relation_table.rows.reserve(relations.size());
  for (const auto& relation : relations) {
    if (relation.left_case_id.empty() || relation.right_case_id.empty())
      throw std::invalid_argument("case relations: case ids must not be empty");
    relation_table.rows.push_back({
        EventAttribute{left_column, relation.left_case_id, AttributeKind::kString},
        EventAttribute{right_column, relation.right_case_id, AttributeKind::kString}});
  }
  return merge_case_relations(base, incoming, relation_table, options);
}

}  // namespace minera
