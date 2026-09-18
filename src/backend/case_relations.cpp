#include "minera/case_relations.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>

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

struct NumericValue {
  bool integer = false;
  std::int64_t integer_value = 0;
  double floating_value = 0.0;
};

std::optional<NumericValue> numeric_value(const EventAttribute& cell) {
  if (cell.is_null) return std::nullopt;
  if (cell.kind == AttributeKind::kInt) {
    if (const auto* value = std::get_if<std::int64_t>(&cell.typed_value))
      return NumericValue{true, *value, 0.0};
  }
  if (cell.kind == AttributeKind::kFloat) {
    if (const auto* value = std::get_if<double>(&cell.typed_value))
      return NumericValue{false, 0, *value};
  }
  if (cell.kind == AttributeKind::kBoolean) {
    if (const auto* value = std::get_if<bool>(&cell.typed_value))
      return NumericValue{true, *value ? 1 : 0, 0.0};
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
  return !cell || cell->is_null;
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

int compare_numeric(const NumericValue& left, const NumericValue& right) {
  if (left.integer && right.integer) {
    return left.integer_value < right.integer_value
               ? -1
               : left.integer_value > right.integer_value ? 1 : 0;
  }
  if (left.integer && !right.integer)
    return compare_int_double(left.integer_value, right.floating_value);
  if (!left.integer && right.integer)
    return -compare_int_double(right.integer_value, left.floating_value);
  if (std::isnan(left.floating_value) || std::isnan(right.floating_value)) {
    if (std::isnan(left.floating_value) && std::isnan(right.floating_value)) return 0;
    return std::isnan(left.floating_value) ? 1 : -1;
  }
  return left.floating_value < right.floating_value
             ? -1
             : left.floating_value > right.floating_value ? 1 : 0;
}

enum class SortKind { kNumeric, kDate, kString, kMissing };

struct SortValue {
  SortKind kind = SortKind::kMissing;
  std::optional<NumericValue> numeric;
  std::optional<EventTime> date;
  std::string text;
};

SortValue sort_value(const std::optional<EventAttribute>& cell) {
  if (null_cell(cell)) return {};
  if (const auto numeric = numeric_value(*cell))
    return {SortKind::kNumeric, numeric, std::nullopt, {}};
  if (const auto date = native_date_value(*cell))
    return {SortKind::kDate, std::nullopt, date, {}};
  return {SortKind::kString, std::nullopt, std::nullopt, cell->value};
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
  if (left.kind == SortKind::kNumeric)
    return compare_numeric(*left.numeric, *right.numeric);
  if (left.kind == SortKind::kDate)
    return *left.date < *right.date ? -1 : *left.date > *right.date ? 1 : 0;
  return left.text < right.text ? -1 : left.text > right.text ? 1 : 0;
}

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
  return left->kind == right->kind && left->value == right->value;
}

bool has_integer_float_conversion_collision(const EventTable& table,
                                            std::size_t index) {
  std::vector<std::int64_t> integers;
  std::vector<double> floats;
  for (const auto& row : table.rows) {
    if (null_cell(row[index])) continue;
    if (const auto numeric = numeric_value(*row[index])) {
      if (numeric->integer && row[index]->kind == AttributeKind::kInt)
        integers.push_back(numeric->integer_value);
      else if (!numeric->integer && row[index]->kind == AttributeKind::kFloat)
        floats.push_back(numeric->floating_value);
    }
  }
  for (const auto integer : integers) {
    const auto converted = static_cast<double>(integer);
    for (const auto floating : floats) {
      if (std::bit_cast<std::uint64_t>(converted) ==
              std::bit_cast<std::uint64_t>(floating) &&
          compare_int_double(integer, floating) != 0)
        return true;
    }
  }
  return false;
}

bool has_integer_float_conversion_collision(
    const EventTable& first, std::size_t first_index,
    const EventTable& second, std::size_t second_index) {
  std::vector<std::int64_t> integers;
  std::vector<double> floats;
  const auto collect = [&](const EventTable& table, std::size_t index) {
    for (const auto& row : table.rows) {
      if (null_cell(row[index])) continue;
      if (const auto numeric = numeric_value(*row[index])) {
        if (numeric->integer && row[index]->kind == AttributeKind::kInt)
          integers.push_back(numeric->integer_value);
        else if (!numeric->integer && row[index]->kind == AttributeKind::kFloat)
          floats.push_back(numeric->floating_value);
      }
    }
  };
  collect(first, first_index);
  collect(second, second_index);
  for (const auto integer : integers) {
    const auto converted = static_cast<double>(integer);
    for (const auto floating : floats) {
      if (std::bit_cast<std::uint64_t>(converted) ==
              std::bit_cast<std::uint64_t>(floating) &&
          compare_int_double(integer, floating) != 0)
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
        row[output_timestamp_index]->is_null ||
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
    std::size_t sequence = 0;
  };
  std::vector<OutputRow> output_rows;
  output_rows.reserve(base.rows.size());

  const auto write_cell = [&](EventTable::Row& row, const std::string& name,
                              const std::optional<EventAttribute>& cell) {
    row[output_index(name)] = copy_cell(cell, name);
  };
  const auto make_base_row = [&](const EventTable::Row& source,
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
                     sequence};
    output_rows.push_back(std::move(output));
  };
  std::size_t sequence = 0;
  for (const auto& row : base.rows) make_base_row(row, sequence++);

  for (const auto& source : incoming.rows) {
    for (const auto& relation : relations.rows) {
      if (!cell_equal(source[incoming_case_index],
                      relation[relation_right_index]))
        continue;
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
          sequence++};
      output_rows.push_back(std::move(output));
    }
  }

  std::stable_sort(output_rows.begin(), output_rows.end(),
                   [](const OutputRow& left, const OutputRow& right) {
    const auto case_order = compare_sort_values(sort_value(left.case_cell),
                                                sort_value(right.case_cell));
    if (case_order != 0) return case_order < 0;
    const auto time_order = compare_sort_values(sort_value(left.timestamp_cell),
                                                sort_value(right.timestamp_cell));
    if (time_order != 0) return time_order < 0;
    return left.sequence < right.sequence;
  });
  result.rows.reserve(output_rows.size());
  for (auto& output : output_rows) result.rows.push_back(std::move(output.row));
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
