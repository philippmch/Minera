#include "minera/conformance.hpp"

#include <algorithm>
#include <map>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "minera/log_conversion.hpp"
#include "minera/dataframe_validation.hpp"

namespace minera {
namespace {

bool alias(std::string_view key, FirstClassAttributeRole role) {
  if (role == FirstClassAttributeRole::kActivity)
    return key == "concept:name" || key == "activity";
  return key == "case:concept:name" || key == "case:id" ||
         key == "case_id" || key == "case";
}

const EventAttribute& selected_field(const EventRecord& record,
                                     std::string_view selector,
                                     FirstClassAttributeRole role,
                                     bool allow_null = false) {
  const EventAttribute* selected = nullptr;
  int best = 0;
  std::size_t matches = 0;
  for (const auto& field : record.fields) {
    const int priority = field.key == selector ? 3
        : (!field.first_class_source_key.empty() &&
           field.first_class_source_key == selector) ? 2
        : (alias(selector, role) && alias(field.key, role)) ? 1 : 0;
    if (priority > best) {
      best = priority;
      selected = &field;
      matches = 1;
    } else if (priority && priority == best) {
      ++matches;
    }
  }
  if (matches > 1)
    throw std::invalid_argument("token replay: ambiguous selected field '" +
                                std::string(selector) + "'");
  if (!selected || (!allow_null && selected->is_null) ||
      (selected->first_class_role != FirstClassAttributeRole::kNone &&
       selected->first_class_state != FirstClassAttributeState::kPresent &&
       !(allow_null && selected->first_class_state == FirstClassAttributeState::kNull)))
    throw std::invalid_argument("token replay: null or missing selected field '" +
                                std::string(selector) + "'");
  return *selected;
}

void enforce_cap(std::size_t rows, const TokenReplayOptions& options) {
  if (options.max_rows_to_materialize && rows > *options.max_rows_to_materialize)
    throw std::length_error("token replay: row materialization cap exceeded");
}

std::string activity_selector(const TokenReplayOptions& options) {
  return options.activity_attribute.empty() ? "concept:name"
                                             : options.activity_attribute;
}

EventLog project_log(const EventLog& source, const TokenReplayOptions& options) {
  enforce_cap(source.event_count(), options);
  auto log = source;
  const auto selector = activity_selector(options);
  for (auto& trace : log.traces) {
    for (auto& event : trace.events) {
      // Match grouped replay precedence: exact generic fields, transported
      // generic fields, then the first-class projection. Keep trace boundaries.
      const EventAttribute* selected = nullptr;
      int best = 0;
      std::size_t matches = 0;
      for (const auto& field : event.attributes) {
        const int priority = field.key == selector ? 3
            : !field.first_class_source_key.empty() && field.first_class_source_key == selector ? 2 : 0;
        if (priority > best) {
          selected = &field;
          best = priority;
          matches = 1;
        } else if (priority && priority == best) ++matches;
      }
      if (matches > 1) throw std::invalid_argument("token replay: ambiguous selected activity");
      if (!selected) {
        if ((alias(selector, FirstClassAttributeRole::kActivity) ||
             (!event.activity_source_key.empty() && event.activity_source_key == selector)) &&
            (event.has_activity() || event.activity_state == FirstClassAttributeState::kNull))
          continue;
        throw std::invalid_argument("token replay: missing selected activity");
      }
      auto field = *selected;
      if (field.first_class_role != FirstClassAttributeRole::kNone &&
          field.first_class_state == FirstClassAttributeState::kMissing)
        throw std::invalid_argument("token replay: selected activity is missing");
      const bool null = field.is_null || field.first_class_state == FirstClassAttributeState::kNull;
      event.activity = null ? "" : field.value;
      event.activity_state = null ? FirstClassAttributeState::kNull : FirstClassAttributeState::kPresent;
      event.activity_source_key = selector;
      field.key = selector;
      field.first_class_role = FirstClassAttributeRole::kNone;
      field.first_class_state = FirstClassAttributeState::kMissing;
      field.first_class_source_key.clear();
      event.activity_attribute = std::move(field);
    }
  }
  return log;
}

EventLog project_stream(EventStream stream, const TokenReplayOptions& options) {
  const auto activity = activity_selector(options);
  for (auto& record : stream.events) {
    auto selected_case = selected_field(record, options.case_id_attribute,
                                        FirstClassAttributeRole::kCaseId, true);
    auto selected_activity = selected_field(record, activity,
                                            FirstClassAttributeRole::kActivity, true);
    if (selected_case.is_null || selected_case.first_class_state == FirstClassAttributeState::kNull) {
      selected_case.is_null = true;
      selected_case.value.clear();
      selected_case.kind = AttributeKind::kString;
      selected_case.typed_value = std::monostate{};
    }
    for (auto& field : record.fields) {
      if (field.first_class_role != FirstClassAttributeRole::kCaseId &&
          field.first_class_role != FirstClassAttributeRole::kActivity) continue;
      field.first_class_role = FirstClassAttributeRole::kNone;
      field.first_class_state = FirstClassAttributeState::kMissing;
      field.first_class_source_key.clear();
    }
    const auto append = [&](EventAttribute field, FirstClassAttributeRole role,
                            const std::string& selector) {
      const bool null = field.is_null || field.first_class_state == FirstClassAttributeState::kNull;
      field.first_class_role = role;
      field.first_class_state = null ? FirstClassAttributeState::kNull : FirstClassAttributeState::kPresent;
      field.first_class_source_key = selector;
      record.fields.push_back(std::move(field));
    };
    append(std::move(selected_case), FirstClassAttributeRole::kCaseId,
           options.case_id_attribute);
    append(std::move(selected_activity), FirstClassAttributeRole::kActivity, activity);
  }
  LogConversionOptions conversion;
  conversion.case_id_key = options.case_id_attribute;
  conversion.include_case_attributes = false;
  return event_stream_to_event_log(stream, conversion);
}

EventLog project_table(const EventTable& table, const TokenReplayOptions& options) {
  enforce_cap(table.size(), options);
  auto stream = event_table_to_event_stream(table);  // Also validates row shape.
  stream.metadata = table.metadata;
  using Origin = std::pair<FirstClassAttributeRole, std::string>;
  std::vector<std::optional<Origin>> origins(table.columns.size());
  std::vector<bool> ambiguous(table.columns.size(), false);
  for (const auto& row : table.rows) {
    for (std::size_t column = 0; column < table.columns.size(); ++column) {
      if (!row[column]) continue;
      const auto& cell = *row[column];
      if (cell.first_class_source_key.empty() &&
          cell.first_class_role == FirstClassAttributeRole::kNone) continue;
      Origin origin{cell.first_class_role, cell.first_class_source_key};
      if (origins[column] && origins[column] != origin) ambiguous[column] = true;
      else origins[column] = std::move(origin);
    }
  }
  const auto activity = activity_selector(options);
  // Tables require selected columns even when there are no rows. Retained
  // source keys on populated columns can identify transported selectors.
  EventRecord schema;
  for (std::size_t column = 0; column < table.columns.size(); ++column) {
    EventAttribute field{table.columns[column], ""};
    if (origins[column]) field.first_class_source_key = origins[column]->second;
    schema.fields.push_back(std::move(field));
  }
  (void)selected_field(schema, options.case_id_attribute, FirstClassAttributeRole::kCaseId);
  (void)selected_field(schema, activity, FirstClassAttributeRole::kActivity);
  EventStream selected_rows;
  selected_rows.attributes = stream.attributes;
  selected_rows.metadata = stream.metadata;
  for (std::size_t row = 0; row < table.rows.size(); ++row) {
    auto validation = stream.events[row];
    for (std::size_t column = 0; column < table.columns.size(); ++column) {
      if (table.rows[row][column]) continue;
      if (ambiguous[column])
        throw std::invalid_argument("token replay: ambiguous missing column provenance: " +
                                    table.columns[column]);
      EventAttribute missing{table.columns[column], ""};
      missing.is_null = true;
      missing.first_class_state = FirstClassAttributeState::kNull;
      if (origins[column]) {
        missing.first_class_role = origins[column]->first;
        missing.first_class_source_key = origins[column]->second;
      }
      validation.fields.push_back(std::move(missing));
    }
    // Absence in an existing higher-priority column must not fall back to an
    // alias. Only selected absence is materialized; unrelated absence stays absent.
    const auto& selected_case = selected_field(validation, options.case_id_attribute,
                                               FirstClassAttributeRole::kCaseId, true);
    if (selected_case.is_null || selected_case.first_class_state == FirstClassAttributeState::kNull)
      continue;
    const auto& selected_activity = selected_field(
        validation, activity, FirstClassAttributeRole::kActivity, true);
    if (&selected_activity >= validation.fields.data() + stream.events[row].fields.size())
      stream.events[row].fields.push_back(selected_activity);
    selected_rows.events.push_back(std::move(stream.events[row]));
  }
  auto log = project_stream(std::move(selected_rows), options);
  const auto numeric = [](AttributeKind kind) {
    return kind == AttributeKind::kInt || kind == AttributeKind::kFloat ||
           kind == AttributeKind::kBoolean;
  };
  using Number = std::variant<std::int64_t, std::uint64_t, double>;
  const auto number = [](const EventAttribute& field) -> Number {
    if (const auto* value = std::get_if<std::int64_t>(&field.typed_value)) return *value;
    if (const auto* value = std::get_if<std::uint64_t>(&field.typed_value)) return *value;
    if (const auto* value = std::get_if<double>(&field.typed_value)) return *value;
    if (const auto* value = std::get_if<bool>(&field.typed_value)) return std::int64_t{*value ? 1 : 0};
    if (field.kind == AttributeKind::kBoolean)
      return std::int64_t{field.value == "true" || field.value == "1" ? 1 : 0};
    if (field.kind == AttributeKind::kInt) {
      auto text = std::string_view(field.value);
      if (text.starts_with('+')) text.remove_prefix(1);
      std::int64_t signed_value = 0;
      const auto signed_result = std::from_chars(
          text.data(), text.data() + text.size(), signed_value, 10);
      if (signed_result.ec == std::errc{} && signed_result.ptr == text.data() + text.size())
        return signed_value;
      std::uint64_t unsigned_value = 0;
      const auto unsigned_result = std::from_chars(
          text.data(), text.data() + text.size(), unsigned_value, 10);
      if (unsigned_result.ec == std::errc{} && unsigned_result.ptr == text.data() + text.size())
        return unsigned_value;
      throw std::invalid_argument("token replay: invalid integer case-id value");
    }
    return std::stod(field.value);
  };
  const auto compare_integer_real = [](std::int64_t integer, double real) {
    // Do not round integer identifiers through double, including on platforms
    // where long double has no extra precision. Bound the cast before using it.
    if (std::isnan(real) || real >= 0x1p63) return -1;
    if (real < -0x1p63) return 1;
    const auto truncated = static_cast<std::int64_t>(real);
    if (integer < truncated) return -1;
    if (integer > truncated) return 1;
    const auto whole = std::trunc(real);
    return real > whole ? -1 : real < whole ? 1 : 0;
  };
  const auto compare_unsigned_real = [](std::uint64_t integer, double real) {
    // Do not convert a uint64 identifier to double before comparison. A
    // double at or above 2^64 is greater than every uint64 value.
    if (std::isnan(real)) return -1;
    if (real < 0) return 1;
    if (real >= 0x1p64) return -1;
    const auto truncated = static_cast<std::uint64_t>(real);
    if (integer < truncated) return -1;
    if (integer > truncated) return 1;
    const auto whole = std::trunc(real);
    return real > whole ? -1 : real < whole ? 1 : 0;
  };
  const auto number_less = [&](const Number& left, const Number& right) {
    const auto* left_integer = std::get_if<std::int64_t>(&left);
    const auto* left_unsigned = std::get_if<std::uint64_t>(&left);
    const auto* right_integer = std::get_if<std::int64_t>(&right);
    const auto* right_unsigned = std::get_if<std::uint64_t>(&right);
    if (left_integer && right_integer) return *left_integer < *right_integer;
    if (left_unsigned && right_unsigned) return *left_unsigned < *right_unsigned;
    if (left_integer && right_unsigned)
      return *left_integer < 0 || static_cast<std::uint64_t>(*left_integer) < *right_unsigned;
    if (left_unsigned && right_integer)
      return *right_integer >= 0 &&
             *left_unsigned < static_cast<std::uint64_t>(*right_integer);
    if (left_integer)
      return compare_integer_real(*left_integer, std::get<double>(right)) < 0;
    if (left_unsigned)
      return compare_unsigned_real(*left_unsigned, std::get<double>(right)) < 0;
    if (right_integer)
      return compare_integer_real(*right_integer, std::get<double>(left)) > 0;
    if (right_unsigned)
      return compare_unsigned_real(*right_unsigned, std::get<double>(left)) > 0;
    const double a = std::get<double>(left), b = std::get<double>(right);
    if (std::isnan(a)) return false;
    if (std::isnan(b)) return true;
    return a < b;
  };
  std::stable_sort(log.traces.begin(), log.traces.end(), [&](const Trace& a, const Trace& b) {
    const auto& left = *a.case_id_attribute;
    const auto& right = *b.case_id_attribute;
    const bool left_numeric = numeric(left.kind), right_numeric = numeric(right.kind);
    if (left_numeric != right_numeric) return left_numeric;
    if (left_numeric) return number_less(number(left), number(right));
    return left.value < right.value;
  });
  // Public dataframe replay infers the grouped index from the first scalar
  // representative of each exact case. An all-numeric index containing a
  // float (but no bool) promotes its keys to double. Dictionary construction
  // then replaces earlier groups whose inferred keys compare equal.
  bool has_float = false;
  bool floating_index = !log.traces.empty();
  for (const auto& trace : log.traces) {
    const auto& key = *trace.case_id_attribute;
    if (!numeric(key.kind) || key.kind == AttributeKind::kBoolean ||
        std::holds_alternative<bool>(key.typed_value)) {
      floating_index = false;
      break;
    }
    const auto value = number(key);
    if (const auto* real = std::get_if<double>(&value)) {
      has_float = true;
      if (std::isnan(*real)) { floating_index = false; break; }
    }
  }
  if (floating_index && has_float) {
    std::map<double, std::size_t> positions;
    std::vector<Trace> inferred;
    inferred.reserve(log.traces.size());
    for (auto& trace : log.traces) {
      const auto key = std::visit([](auto value) { return static_cast<double>(value); },
                                 number(*trace.case_id_attribute));
      const auto [entry, inserted] = positions.emplace(key, inferred.size());
      if (inserted) inferred.push_back(std::move(trace));
      else inferred[entry->second] = std::move(trace);
    }
    log.traces = std::move(inferred);
  }
  return log;
}

}  // namespace

std::vector<TokenReplayTraceDiagnostics> replay_dataframe_traces(
    const EventTable& table, const PetriNet& net, TokenReplayOptions options) {
  DataFrameColumnOptions columns;
  columns.activity_key = options.activity_attribute;
  columns.case_id_key = options.case_id_attribute;
  columns.timestamp_key = options.timestamp_attribute;
  check_dataframe_columns(table, columns);
  return replay_token_traces(EventDataView(table), net, options);
}

EventLog normalize_token_replay_event_log(const EventDataView& data,
                                          const TokenReplayOptions& options) {
  if (data.kind() == EventDataKind::kEventLog) return project_log(*data.log(), options);
  if (options.case_id_attribute.empty())
    throw std::invalid_argument("token replay: case-id selector must not be empty");
  if (data.kind() == EventDataKind::kEventStream) {
    enforce_cap(data.stream()->size(), options);
    return project_stream(*data.stream(), options);
  }
  if (data.kind() == EventDataKind::kEventTable) return project_table(*data.table(), options);
  return project_table(data.lazy_table()->materialize(options.max_rows_to_materialize), options);
}

ConformanceResult check_conformance(const EventDataView& data, const PetriNet& net,
                                    TokenReplayOptions options) {
  auto log = normalize_token_replay_event_log(data, options);
  return check_conformance(log, net, options);
}

ConformanceResult check_conformance(const EventDataView& data, const ProcessTree& tree,
                                    TokenReplayOptions options) {
  auto log = normalize_token_replay_event_log(data, options);
  return check_conformance(log, tree, options);
}

std::vector<TokenReplayTraceDiagnostics> replay_token_traces(
    const EventDataView& data, const PetriNet& net, TokenReplayOptions options) {
  auto log = normalize_token_replay_event_log(data, options);
  return replay_token_traces(log, net, options);
}

TokenReplayDiagnostics replay_token_diagnostics(
    const EventDataView& data, const PetriNet& net, TokenReplayOptions options) {
  auto log = normalize_token_replay_event_log(data, options);
  return replay_token_diagnostics(log, net, options);
}

}  // namespace minera
