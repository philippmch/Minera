#include "minera/declare.hpp"

#include "minera/cancellation.hpp"
#include "minera/log_conversion.hpp"
#include "frame_cells.hpp"
#include "minera/dataframe_validation.hpp"
#include "numpy_object_sort.hpp"
#include "polars_sort.hpp"
#include "python_scalar.hpp"
#include "python_str_set.hpp"
#include "polars_missing_column.hpp"
#include "xes_frame_columns.hpp"
#include "xes_frame_typed.hpp"
#include "xes_frame_activity.hpp"
#include "minera/timestamp.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace minera {
namespace {

void validate_activity_key(std::string_view activity_key) {
  if (activity_key.empty() ||
      std::all_of(activity_key.begin(), activity_key.end(), [](char value) {
        return std::isspace(static_cast<unsigned char>(value)) != 0;
      })) {
    throw std::invalid_argument("declare: activity selector must not be blank");
  }
}

void validate_case_selector_key(std::string_view case_id_key) {
  if (case_id_key.empty() ||
      std::all_of(case_id_key.begin(), case_id_key.end(), [](char value) {
        return std::isspace(static_cast<unsigned char>(value)) != 0;
      })) {
    throw std::invalid_argument("declare: case-id selector must not be blank");
  }
}

const std::vector<std::string>& families() {
  static const std::vector<std::string> value = {
      kDeclareExistence, kDeclareExactlyOne, kDeclareInit,
      kDeclareRespondedExistence, kDeclareResponse, kDeclarePrecedence,
      kDeclareSuccession, kDeclareAltResponse, kDeclareAltPrecedence,
      kDeclareAltSuccession, kDeclareChainResponse, kDeclareChainPrecedence,
      kDeclareChainSuccession, kDeclareAbsence, kDeclareCoexistence,
      kDeclareNonCoexistence, kDeclareNonSuccession,
      kDeclareNonChainSuccession};
  return value;
}

bool has(const std::set<std::string>& allowed, const char* family) {
  return allowed.find(family) != allowed.end();
}

// Dense Declare candidate evaluation over interned activities.
//
// Every candidate (family, a[, b]) of a computed family has a status per
// projected trace: 1 satisfied, -1 violated, 0 not activated. Support counts
// the weighted traces with status != 0 and confidence those with status 1.
// A binary candidate whose activities are both absent from a trace has status
// 0 in every family (each base template is activated only by an occurrence,
// and the compound templates are minima/negations of statuses that are then
// all 0), so only the pairs touching a present activity are evaluated: the
// cost per variant is O(k * A) for k present activities instead of a
// string-keyed map of every candidate.
enum BinaryFamily : std::size_t {
  kRe, kResp, kPrec, kAltResp, kChainResp, kAltPrec, kChainPrec, kSucc,
  kAltSucc, kChainSucc, kCoex, kNonCoex, kNonSucc, kNonChainSucc, kBinaryFamilies
};
enum UnaryFamily : std::size_t { kExists, kExactlyOne, kInit, kAbsent, kUnaryFamilies };

const char* const kBinaryFamilyNames[kBinaryFamilies] = {
    kDeclareRespondedExistence, kDeclareResponse, kDeclarePrecedence,
    kDeclareAltResponse, kDeclareChainResponse, kDeclareAltPrecedence,
    kDeclareChainPrecedence, kDeclareSuccession, kDeclareAltSuccession,
    kDeclareChainSuccession, kDeclareCoexistence, kDeclareNonCoexistence,
    kDeclareNonSuccession, kDeclareNonChainSuccession};
const char* const kUnaryFamilyNames[kUnaryFamilies] = {
    kDeclareExistence, kDeclareExactlyOne, kDeclareInit, kDeclareAbsence};

bool alt_response_positions(const std::vector<int>& ia, const std::vector<int>& ib,
                            int length) {
  if (ia.empty()) return true;
  if (ib.empty()) return false;
  std::size_t p = 0;
  for (std::size_t i = 0; i < ia.size(); ++i) {
    const int next = i + 1 < ia.size() ? ia[i + 1] : length;
    while (p < ib.size() && ib[p] <= ia[i]) ++p;
    if (p == ib.size() || ib[p] >= next) return false;
  }
  return true;
}

bool alt_precedence_positions(const std::vector<int>& ia, const std::vector<int>& ib) {
  if (ib.empty()) return true;
  if (ia.empty()) return false;
  std::size_t p = 0;
  int previous = -1;
  for (const int bi : ib) {
    while (p < ia.size() && ia[p] <= previous) ++p;
    if (p == ia.size() || ia[p] >= bi) return false;
    previous = bi;
  }
  return true;
}

struct DenseDeclareStats {
  std::size_t activity_count = 0;
  // Which families the reference evaluation computes for these templates.
  std::array<bool, kBinaryFamilies> binary{};
  std::array<bool, kUnaryFamilies> unary{};
  // [family][a][b] and [family][a].
  std::vector<int> binary_support, binary_confidence;
  std::vector<int> unary_support, unary_confidence;

  std::size_t at(std::size_t family, std::size_t a, std::size_t b) const {
    return (family * activity_count + a) * activity_count + b;
  }
};

DenseDeclareStats evaluate_dense(
    const std::map<std::vector<std::size_t>, int>& variants,
    std::size_t activity_count, const std::set<std::string>& allowed) {
  DenseDeclareStats stats;
  stats.activity_count = activity_count;
  const auto allows = [&](std::initializer_list<const char*> names) {
    for (const auto* name : names)
      if (!has(allowed, name)) return false;
    return true;
  };
  stats.unary[kExists] = allows({kDeclareExistence});
  stats.unary[kExactlyOne] = allows({kDeclareExactlyOne});
  stats.unary[kInit] = allows({kDeclareInit});
  stats.unary[kAbsent] = allows({kDeclareAbsence, kDeclareExistence});
  for (const auto f : {kRe, kResp, kPrec, kAltResp, kChainResp, kAltPrec, kChainPrec})
    stats.binary[f] = allows({kBinaryFamilyNames[f]});
  stats.binary[kSucc] = allows({kDeclareSuccession, kDeclareResponse, kDeclarePrecedence});
  stats.binary[kAltSucc] =
      allows({kDeclareAltSuccession, kDeclareAltResponse, kDeclareAltPrecedence});
  stats.binary[kChainSucc] =
      allows({kDeclareChainSuccession, kDeclareChainResponse, kDeclareChainPrecedence});
  stats.binary[kCoex] = allows({kDeclareCoexistence, kDeclareRespondedExistence});
  stats.binary[kNonCoex] = allows(
      {kDeclareNonCoexistence, kDeclareCoexistence, kDeclareRespondedExistence});
  stats.binary[kNonSucc] = allows({kDeclareNonSuccession, kDeclareSuccession,
                                   kDeclareResponse, kDeclarePrecedence});
  stats.binary[kNonChainSucc] =
      allows({kDeclareNonChainSuccession, kDeclareChainSuccession,
              kDeclareChainResponse, kDeclareChainPrecedence});

  const std::size_t n = activity_count;
  stats.binary_support.assign(kBinaryFamilies * n * n, 0);
  stats.binary_confidence.assign(stats.binary_support.size(), 0);
  stats.unary_support.assign(kUnaryFamilies * n, 0);
  stats.unary_confidence.assign(stats.unary_support.size(), 0);

  std::vector<std::vector<int>> positions(n);
  std::vector<char> in(n, 0);
  std::vector<char> visited(n * n, 0);
  std::vector<std::size_t> touched;
  std::vector<std::size_t> present;
  for (const auto& [trace, count] : variants) {
    present.clear();
    for (std::size_t i = 0; i < trace.size(); ++i) {
      auto& list = positions[trace[i]];
      if (list.empty()) present.push_back(trace[i]);
      list.push_back(static_cast<int>(i));
    }
    for (const auto a : present) in[a] = 1;
    const int length = static_cast<int>(trace.size());
    const auto add_unary = [&](std::size_t family, std::size_t a, int status) {
      if (!stats.unary[family]) return;
      if (status != 0) stats.unary_support[family * n + a] += count;
      if (status == 1) stats.unary_confidence[family * n + a] += count;
    };
    for (std::size_t a = 0; a < n; ++a) {
      const int exists = in[a] ? 1 : -1;
      add_unary(kExists, a, exists);
      add_unary(kExactlyOne, a, in[a] && positions[a].size() == 1 ? 1 : -1);
      add_unary(kInit, a, !trace.empty() && trace.front() == a ? 1 : -1);
      add_unary(kAbsent, a, -exists);
    }
    const auto evaluate_pair = [&](std::size_t a, std::size_t b) {
      if (a == b || visited[a * n + b]) return;
      visited[a * n + b] = 1;
      touched.push_back(a * n + b);
      const bool ia = in[a] != 0, ib = in[b] != 0;
      const auto& pa = positions[a];
      const auto& pb = positions[b];
      std::array<int, kBinaryFamilies> s{};
      s[kRe] = ia ? (ib ? 1 : -1) : 0;
      s[kResp] = ia ? ((!ib || pa.back() >= pb.back()) ? -1 : 1) : 0;
      s[kPrec] = ib ? ((!ia || pa.front() >= pb.front()) ? -1 : 1) : 0;
      s[kAltResp] = ia ? (alt_response_positions(pa, pb, length) ? 1 : -1) : 0;
      if (ia) {
        bool ok = true;
        for (const int i : pa)
          if (i + 1 >= length || trace[static_cast<std::size_t>(i) + 1] != b) { ok = false; break; }
        s[kChainResp] = ok ? 1 : -1;
      }
      s[kAltPrec] = ib ? (alt_precedence_positions(pa, pb) ? 1 : -1) : 0;
      if (ib) {
        bool ok = true;
        for (const int i : pb)
          if (i == 0 || trace[static_cast<std::size_t>(i) - 1] != a) { ok = false; break; }
        s[kChainPrec] = ok ? 1 : -1;
      }
      s[kSucc] = std::min(s[kResp], s[kPrec]);
      s[kAltSucc] = std::min(s[kAltResp], s[kAltPrec]);
      s[kChainSucc] = std::min(s[kChainResp], s[kChainPrec]);
      s[kCoex] = std::min(s[kRe], ib ? (ia ? 1 : -1) : 0);
      s[kNonCoex] = -s[kCoex];
      s[kNonSucc] = -s[kSucc];
      s[kNonChainSucc] = -s[kChainSucc];
      for (std::size_t f = 0; f < kBinaryFamilies; ++f) {
        if (!stats.binary[f] || s[f] == 0) continue;
        const auto index = stats.at(f, a, b);
        stats.binary_support[index] += count;
        if (s[f] == 1) stats.binary_confidence[index] += count;
      }
    };
    for (const auto a : present)
      for (std::size_t b = 0; b < n; ++b) {
        evaluate_pair(a, b);
        evaluate_pair(b, a);
      }
    for (const auto index : touched) visited[index] = 0;
    touched.clear();
    for (const auto a : present) {
      in[a] = 0;
      positions[a].clear();
    }
  }
  return stats;
}

const std::string* first_class_value(
    const std::string& value, FirstClassAttributeState state) {
  if (state == FirstClassAttributeState::kNull) return nullptr;
  if (state != FirstClassAttributeState::kMissing || !value.empty())
    return &value;
  return nullptr;
}

const std::string* resolve_event_field(const Event& event,
                                       std::string_view selector) {
  const auto resolve = [](const std::string& value,
                          FirstClassAttributeState state,
                          const std::optional<EventAttribute>& attribute)
      -> const std::string* {
    if (state == FirstClassAttributeState::kNull) return nullptr;
    if (const auto* selected = first_class_value(value, state))
      return selected;
    if (attribute && !attribute->is_null) return &attribute->value;
    return nullptr;
  };

  std::vector<const std::string*> candidates;
  int best_priority = 0;
  const auto consider = [&](int priority, const std::string* value) {
    if (priority < best_priority) return;
    if (priority > best_priority) {
      best_priority = priority;
      candidates.clear();
    }
    candidates.push_back(value);
  };
  const auto consider_first_class =
      [&](std::string_view source_key, const std::string& value,
          FirstClassAttributeState state,
          const std::optional<EventAttribute>& attribute, bool alias) {
        const bool source_match =
            (!source_key.empty() && source_key == selector) ||
            (attribute && attribute->key == selector);
        if (!source_match && !alias) return;
        consider(source_match ? 2 : 1, resolve(value, state, attribute));
      };

  for (const auto& attribute : event.attributes)
    if (attribute.key == selector)
      consider(3, attribute.is_null ? nullptr : &attribute.value);
  consider_first_class(
      event.activity_source_key, event.activity, event.activity_state,
      event.activity_attribute,
      selector == "concept:name" || selector == "activity");
  consider_first_class(
      event.timestamp_source_key, event.timestamp, event.timestamp_state,
      event.timestamp_attribute,
      selector == "time:timestamp" || selector == "timestamp");
  consider_first_class(
      event.resource_source_key, event.resource, event.resource_state,
      event.resource_attribute,
      selector == "org:resource" || selector == "resource");
  consider_first_class(
      event.lifecycle_source_key, event.lifecycle, event.lifecycle_state,
      event.lifecycle_attribute,
      selector == "lifecycle:transition" || selector == "lifecycle");
  const bool case_source = !event.case_id_source_key.empty() &&
                           event.case_id_source_key == selector;
  const bool case_alias = selector == "case:concept:name" ||
                          selector == "case:id" || selector == "case_id" ||
                          selector == "case";
  // A trace id can share its source spelling with an event activity in XES.
  // Preserve case selectors as a fallback without mixing trace provenance
  // into competing event-field candidates, including null event values.
  if ((case_source || case_alias) && candidates.empty())
    consider(0,
             first_class_value(event.case_id, event.case_id_state));

  if (candidates.size() > 1)
    throw std::invalid_argument("declare: event selector is ambiguous: '" +
                                std::string(selector) + "'");
  return candidates.empty() ? nullptr : candidates.front();
}

std::string selected_activity(const Event& event, std::string_view activity_key) {
  validate_activity_key(activity_key);
  if (const auto* value = resolve_event_field(event, activity_key)) return *value;
  throw std::invalid_argument(
      "declare: event activity selector is missing or null: '" +
      std::string(activity_key) + "'");
}

std::vector<std::vector<std::string>> selected_traces(const EventLog& log,
                                                       std::string_view activity_key) {
  validate_activity_key(activity_key);
  std::vector<std::vector<std::string>> result;
  result.reserve(log.traces.size());
  for (const auto& trace : log.traces) {
    auto& selected = result.emplace_back();
    selected.reserve(trace.events.size());
    for (const auto& event : trace.events)
      selected.push_back(selected_activity(event, activity_key));
  }
  return result;
}

std::set<std::string> default_activities(
    const std::vector<std::vector<std::string>>& traces) {
  std::set<std::string> result;
  for (const auto& trace : traces)
    result.insert(trace.begin(), trace.end());
  return result;
}

std::set<std::string> to_set(const std::vector<std::string>& values) {
  return std::set<std::string>(values.begin(), values.end());
}

int activity_selector_priority(const EventAttribute& field,
                               std::string_view selector) {
  if (field.key == selector) return 3;
  if (!field.first_class_source_key.empty() &&
      field.first_class_source_key == selector)
    return 2;
  const bool activity_alias = selector == "concept:name" || selector == "activity";
  return activity_alias &&
                 (field.key == "concept:name" || field.key == "activity")
             ? 1
             : 0;
}

int case_selector_priority(const EventAttribute& field,
                           std::string_view selector) {
  if (field.key == selector) return 3;
  if (!field.first_class_source_key.empty() &&
      field.first_class_source_key == selector)
    return 2;
  const bool case_alias = selector == "case:concept:name" ||
                          selector == "case:id" || selector == "case_id" ||
                          selector == "case";
  return case_alias &&
                 (field.key == "case:concept:name" || field.key == "case:id" ||
                  field.key == "case_id" || field.key == "case")
             ? 1
             : 0;
}

template <typename Matcher>
void promote_selected_role(std::vector<EventAttribute>& fields,
                           std::string_view selector,
                           FirstClassAttributeRole role,
                           std::string_view field_name, Matcher&& matcher) {
  struct Candidate {
    std::size_t index = 0;
    FirstClassAttributeState state = FirstClassAttributeState::kMissing;
    std::string source_key;
    bool already_projected = false;
  };
  std::vector<Candidate> candidates;
  int best_priority = 0;
  for (std::size_t index = 0; index < fields.size(); ++index) {
    const auto& field = fields[index];
    const int priority = matcher(field, selector);
    if (priority == 0) continue;
    if (priority > best_priority) {
      best_priority = priority;
      candidates.clear();
    }
    if (priority == best_priority)
      candidates.push_back({index, field.first_class_state,
                            field.first_class_source_key,
                            field.first_class_role !=
                                FirstClassAttributeRole::kNone});
  }
  if (candidates.size() > 1)
    throw std::invalid_argument("declare: " + std::string(field_name) +
                                " selector is ambiguous: " +
                                std::string(selector));
  for (auto& field : fields) {
    if (field.first_class_role != role) continue;
    field.first_class_role = FirstClassAttributeRole::kNone;
    field.first_class_state = FirstClassAttributeState::kMissing;
    field.first_class_source_key.clear();
  }
  if (candidates.empty()) return;
  const auto candidate = candidates.front();
  auto& field = fields[candidate.index];
  field.first_class_role = role;
  field.first_class_state =
      candidate.already_projected
          ? candidate.state
          : (field.is_null ? FirstClassAttributeState::kNull
                           : FirstClassAttributeState::kPresent);
  field.first_class_source_key =
      candidate.source_key == selector ? candidate.source_key : field.key;
}

void promote_selected(std::vector<EventAttribute>& fields,
                      std::string_view activity_key,
                      std::string_view case_id_key) {
  promote_selected_role(fields, activity_key,
                        FirstClassAttributeRole::kActivity, "activity",
                        activity_selector_priority);
  promote_selected_role(fields, case_id_key,
                        FirstClassAttributeRole::kCaseId, "case-id",
                        case_selector_priority);
}

void normalize_row_keys(EventTable::Row& row,
                        const std::vector<std::string>& columns) {
  for (std::size_t index = 0; index < row.size() && index < columns.size();
       ++index)
    if (row[index]) row[index]->key = columns[index];
}

void promote_selected(EventRecord& record, std::string_view activity_key,
                      std::string_view case_id_key) {
  promote_selected(record.fields, activity_key, case_id_key);
}

void promote_selected(EventStream& stream, std::string_view activity_key,
                      std::string_view case_id_key) {
  for (auto& record : stream.events)
    promote_selected(record, activity_key, case_id_key);
}

void promote_selected(EventTable& table, std::string_view activity_key,
                      std::string_view case_id_key) {
  table.validate();
  for (auto& row : table.rows) {
    normalize_row_keys(row, table.columns);
    std::vector<EventAttribute> fields;
    std::vector<std::size_t> indexes;
    fields.reserve(row.size());
    indexes.reserve(row.size());
    for (std::size_t index = 0; index < row.size(); ++index) {
      if (!row[index]) continue;
      indexes.push_back(index);
      fields.push_back(*row[index]);
    }
    promote_selected(fields, activity_key, case_id_key);
    for (std::size_t index = 0; index < fields.size(); ++index)
      *row[indexes[index]] = std::move(fields[index]);
  }
}

LazyEventTable promoted_selector_table(const LazyEventTable& source,
                                       std::string activity_key,
                                       std::string case_id_key) {
  return LazyEventTable(
      source.columns(),
      [source, activity_key = std::move(activity_key),
       case_id_key = std::move(case_id_key)](
          const LazyEventTable::RowConsumer& consumer) {
        source.scan([&](const LazyEventTable::Row& input) {
          auto row = input;
          normalize_row_keys(row, source.columns());
          std::vector<EventAttribute> fields;
          std::vector<std::size_t> indexes;
          fields.reserve(row.size());
          indexes.reserve(row.size());
          for (std::size_t index = 0; index < row.size(); ++index) {
            if (!row[index]) continue;
            indexes.push_back(index);
            fields.push_back(*row[index]);
          }
          promote_selected(fields, activity_key, case_id_key);
          for (std::size_t index = 0; index < fields.size(); ++index)
            *row[indexes[index]] = std::move(fields[index]);
          if (consumer) consumer(row);
        });
      },
      source.attributes(), source.metadata(), source.row_count(),
      source.dtypes(), source.index(), source.categorical_dtypes());
}

const EventTable& present_columns(const EventTable& table, std::optional<EventTable>& storage) {
  return detail::xes_frame_present_columns(table, storage);
}

// Round GO (observed black-box): pm4py's frame check reads each column's
// dtype as read_xes types it (xes_frame_typed.hpp): a dates column with
// gaps, NaN floats or valueless cells is still a date column, ints beside a
// gap float64, a column of NaN alone float64, mixes object; Polars' copy
// holds an object column (and one of None alone) as String. The table's own
// inference stays where a column holds kinds outside that model. Round HA:
// ints past int64 are typed too (an object column of them beside negative
// ints is String in Polars' copy, which pm4py's Polars check accepts).
EventTable xes_frame_typed_dtypes(const EventTable& table, bool polars) {
  EventTable typed = table;
  std::vector<std::string> dtypes;
  for (std::size_t column = 0; column < table.columns.size(); ++column) {
    detail::XesFrameDtype dtype{};
    if (!detail::xes_frame_cells(table, column, polars, &dtype, true)) return typed;
    using D = detail::XesFrameDtype;
    switch (dtype) {
      case D::kDatetime: dtypes.emplace_back(polars ? "Datetime(time_unit='us', time_zone='UTC')" : "datetime64[us, UTC]"); break;
      case D::kString: dtypes.emplace_back(polars ? "String" : "str"); break;
      case D::kInt: dtypes.emplace_back(polars ? "Int64" : "int64"); break;
      case D::kFloat:
      case D::kNaNFloat: dtypes.emplace_back(polars ? "Float64" : "float64"); break;
      case D::kBool: dtypes.emplace_back(polars ? "Boolean" : "bool"); break;
      case D::kObject:
      case D::kNoneObject: dtypes.emplace_back(polars ? "String" : "object"); break;
    }
  }
  typed.dtypes = std::move(dtypes);
  return typed;
}

// pm4py's Declare on a Polars frame reads each case after
// LazyFrame.sort([case, "time:timestamp", row index]): the timestamp column
// is always "time:timestamp" (left out when the frame has none), a missing
// value sorts first and the row index settles every tie, so the order is
// fixed. Cases follow in sorted order (group_by maintain_order). Round FU,
// XES uploads with tied timestamps, observed black-box. A frame with its own
// "@@index" column (an XES event attribute of that name) sorts by that
// column instead of a row count: its values can tie, and Polars' unstable
// pdqsort then decides the order (round FV, observed black-box).
EventTable polars_projection_table(EventTable table, std::string_view case_id_key,
                                   std::string_view activity_key, bool xes_columns = false) {
  table.validate();
  const auto column_of = [&](std::string_view name) -> std::optional<std::size_t> {
    const auto found = std::find(table.columns.begin(), table.columns.end(), name);
    if (found == table.columns.end()) return std::nullopt;
    return static_cast<std::size_t>(found - table.columns.begin());
  };
  // pm4py's projection checks the frame first (check_pandas_dataframe_columns
  // with the case and activity keys): at least three columns, two of them
  // text and one a date, then the case column (present, text, no gap), then
  // the activity column (round FV, observed black-box). On the frame of an
  // XES upload a column without any value is absent (round FW); a CSV
  // frame keeps every header column.
  {
    std::optional<EventTable> storage;
    check_dataframe_columns(xes_columns ? xes_frame_typed_dtypes(present_columns(table, storage), true) : table,
                            DataFrameColumnOptions{
                                std::string(activity_key.empty() ? std::string_view("concept:name") : activity_key),
                                std::string(case_id_key), std::nullopt, std::nullopt, true});
  }
  const auto case_at = column_of(case_id_key);
  if (!case_at) return table;
  const auto time_at = column_of("time:timestamp");
  const auto dtype = [&](std::size_t at) -> std::string_view {
    return at < table.dtypes.size() ? std::string_view(table.dtypes[at]) : std::string_view();
  };
  const auto numeric = [](std::string_view kind) {
    return kind.rfind("int", 0) == 0 || kind.rfind("uint", 0) == 0 || kind.rfind("float", 0) == 0 ||
           kind.rfind("Int", 0) == 0 || kind.rfind("UInt", 0) == 0 || kind.rfind("Float", 0) == 0;
  };
  const auto text = [](std::string_view kind) {
    return kind == "str" || kind == "object" || kind == "string" || kind == "String";
  };
  const bool case_numeric = numeric(dtype(*case_at));
  const bool time_text = time_at && text(dtype(*time_at));
  // Round GO: an XES upload's time:timestamp column sorts as Polars' copy
  // types it (a missing value first, text by its bytes, numbers, dates and
  // booleans by value; xes_polars_cell_compare).
  const auto typed_time = xes_columns && time_at && table.dtypes.empty()
                              ? detail::xes_typed_column(table, "time:timestamp", true, true)
                              : std::optional<detail::XesTypedColumn>();
  const auto index_at = column_of("@@index");
  const bool index_numeric = index_at && numeric(dtype(*index_at));
  const bool index_integer = index_numeric && dtype(*index_at).find_first_of("fF") != 0;
  struct Key {
    std::optional<std::string> case_text;
    double case_number = 0;
    std::optional<std::string> time_text;
    std::optional<ParsedExactTimestamp> time;
    std::optional<std::string> index_text;
    long double index_number = 0;
  };
  std::vector<Key> keys(table.rows.size());
  for (std::size_t row = 0; row < table.rows.size(); ++row) {
    const auto& cell = table.rows[row][*case_at];
    if (cell && !cell->is_null) {
      keys[row].case_text = cell->value;
      if (case_numeric) {
        try { keys[row].case_number = std::stod(cell->value); } catch (const std::exception&) {}
      }
    }
    if (time_at) {
      const auto& time = table.rows[row][*time_at];
      if (time && !time->is_null) {
        keys[row].time_text = time->value;
        if (!time_text) keys[row].time = parse_timestamp_exact(time->value);
      }
    }
    if (index_at) {
      const auto& index = table.rows[row][*index_at];
      if (index && !index->is_null) {
        if (!index_numeric) {
          keys[row].index_text = index->value;
        } else {
          // A float column's NaN is a missing value (from_pandas).
          try {
            keys[row].index_number = index_integer ? static_cast<long double>(std::stoll(index->value))
                                                   : static_cast<long double>(std::stod(index->value));
            if (!std::isnan(keys[row].index_number)) keys[row].index_text = index->value;
          } catch (const std::exception&) {}
        }
      }
    }
  }
  std::vector<std::size_t> order(table.rows.size());
  for (std::size_t row = 0; row < order.size(); ++row) order[row] = row;
  // -1: lhs first, 1: rhs first, 0: tie.
  const auto compare = [&](std::size_t a, std::size_t b) -> int {
    const auto& lhs = keys[a];
    const auto& rhs = keys[b];
    if (!lhs.case_text || !rhs.case_text) {
      if (lhs.case_text || rhs.case_text) return !lhs.case_text ? -1 : 1;
    } else if (case_numeric ? lhs.case_number != rhs.case_number : *lhs.case_text != *rhs.case_text) {
      return (case_numeric ? lhs.case_number < rhs.case_number : *lhs.case_text < *rhs.case_text) ? -1 : 1;
    }
    if (typed_time) {
      if (const int by = detail::xes_polars_cell_compare(typed_time->cells[a], typed_time->cells[b])) return by;
    } else if (time_at) {
      if (time_text || !lhs.time || !rhs.time) {
        const auto& x = time_text ? lhs.time_text : lhs.time ? lhs.time_text : std::optional<std::string>();
        const auto& y = time_text ? rhs.time_text : rhs.time ? rhs.time_text : std::optional<std::string>();
        if (!x || !y) {
          if (x || y) return !x ? -1 : 1;
        } else if (*x != *y) {
          return *x < *y ? -1 : 1;
        }
      } else if (lhs.time->seconds != rhs.time->seconds) {
        return lhs.time->seconds < rhs.time->seconds ? -1 : 1;
      } else if (lhs.time->nanoseconds != rhs.time->nanoseconds) {
        return lhs.time->nanoseconds < rhs.time->nanoseconds ? -1 : 1;
      }
    }
    if (!index_at) return 0;
    if (!lhs.index_text || !rhs.index_text) {
      if (lhs.index_text || rhs.index_text) return !lhs.index_text ? -1 : 1;
      return 0;
    }
    if (index_numeric) {
      if (lhs.index_number != rhs.index_number) return lhs.index_number < rhs.index_number ? -1 : 1;
      return 0;
    }
    if (*lhs.index_text != *rhs.index_text) return *lhs.index_text < *rhs.index_text ? -1 : 1;
    return 0;
  };
  if (index_at) {
    polars_sort::sort_unstable(order, [&](std::size_t a, std::size_t b) { return compare(a, b) < 0; });
  } else {
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return compare(a, b) < 0; });
  }
  EventTable sorted = table;
  sorted.rows.clear();
  sorted.index.reset();
  sorted.rows.reserve(table.rows.size());
  for (const auto row : order) sorted.rows.push_back(std::move(table.rows[row]));
  return sorted;
}

EventLog event_data_log(const EventDataView& data,
                        std::string_view activity_key,
                        std::string_view case_id_key,
                        bool xes_columns = false) {
  validate_activity_key(activity_key);
  validate_case_selector_key(case_id_key);
  LogConversionOptions conversion;
  conversion.case_id_key = std::string(case_id_key);
  switch (data.kind()) {
    case EventDataKind::kEventLog:
      return *data.log();
    case EventDataKind::kEventStream: {
      auto stream = *data.stream();
      promote_selected(stream, activity_key, case_id_key);
      return event_data_to_event_log(EventDataView(stream), conversion);
    }
    case EventDataKind::kEventTable: {
      auto table = *data.table();
      promote_selected(table, activity_key, case_id_key);
      return event_data_to_event_log(EventDataView(table), conversion);
    }
    case EventDataKind::kLazyEventTable: {
      auto table = polars_projection_table(
          promoted_selector_table(*data.lazy_table(), std::string(activity_key),
                                  std::string(case_id_key))
              .materialize(),
          case_id_key, activity_key, xes_columns);
      return event_data_to_event_log(EventDataView(table), conversion);
    }
  }
  throw std::invalid_argument("event data: invalid input kind");
}

}  // namespace

DeclareModel::DeclareModel() {
  for (const auto& family : families()) rules.emplace(family, std::vector<DeclareConstraint>{});
}

void DeclareModel::add(std::string family, DeclareConstraint constraint) {
  rules[std::move(family)].push_back(std::move(constraint));
}

std::size_t DeclareModel::size() const noexcept {
  std::size_t n = 0;
  for (const auto& [_, values] : rules) n += values.size();
  return n;
}

DeclareModel discover_declare(const EventLog& log, const DeclareDiscoveryOptions& options) {
  const auto selected = selected_traces(log, options.activity_key);
  const auto activities = options.considered_activities.has_value()
                              ? to_set(*options.considered_activities)
                              : default_activities(selected);
  std::set<std::string> allowed;
  if (!options.allowed_templates.has_value())
    allowed.insert(families().begin(), families().end());
  else
    allowed = to_set(*options.allowed_templates);
  DeclareModel model;
  if (activities.empty() || log.traces.empty()) return model;

  // Intern the considered activities in their (sorted) set order, so index
  // order is the candidates' string order.
  const std::vector<std::string> names(activities.begin(), activities.end());
  std::unordered_map<std::string_view, std::size_t> ids;
  for (std::size_t i = 0; i < names.size(); ++i) ids.emplace(names[i], i);
  std::map<std::vector<std::size_t>, int> projected;
  for (const auto& trace : selected) {
    std::vector<std::size_t> variant;
    variant.reserve(trace.size());
    for (const auto& activity : trace) {
      const auto found = ids.find(activity);
      if (found != ids.end()) variant.push_back(found->second);
    }
    ++projected[std::move(variant)];
  }
  const auto stats = evaluate_dense(projected, names.size(), allowed);

  // Visit every candidate of a computed family in (family, first, second)
  // order, the order of the reference's candidate map.
  struct FamilyRef {
    const char* name;
    bool binary;
    std::size_t index;
  };
  std::vector<FamilyRef> order;
  for (std::size_t f = 0; f < kBinaryFamilies; ++f)
    if (stats.binary[f]) order.push_back({kBinaryFamilyNames[f], true, f});
  for (std::size_t f = 0; f < kUnaryFamilies; ++f)
    if (stats.unary[f]) order.push_back({kUnaryFamilyNames[f], false, f});
  std::sort(order.begin(), order.end(), [](const FamilyRef& x, const FamilyRef& y) {
    return std::string_view(x.name) < std::string_view(y.name);
  });
  const std::size_t n = names.size();
  const auto each_candidate = [&](auto&& visit) {
    for (const auto& family : order) {
      if (family.binary) {
        for (std::size_t a = 0; a < n; ++a)
          for (std::size_t b = 0; b < n; ++b) {
            if (a == b) continue;
            const auto index = stats.at(family.index, a, b);
            visit(family, a, b, stats.binary_support[index], stats.binary_confidence[index]);
          }
      } else {
        for (std::size_t a = 0; a < n; ++a) {
          const auto index = family.index * n + a;
          visit(family, a, n, stats.unary_support[index], stats.unary_confidence[index]);
        }
      }
    }
  };

  const int row_count = static_cast<int>(log.traces.size());
  double min_support = options.has_min_support_ratio ? options.min_support_ratio : 0.0;
  double min_confidence = options.has_min_confidence_ratio ? options.min_confidence_ratio : 0.0;
  if (!options.has_min_support_ratio && !options.has_min_confidence_ratio) {
    double best = -1.0;
    int best_support = 0, best_confidence = 0;
    // Ascending candidate order, so an equal product keeps the later
    // candidate (the reference's `best_key < candidate` tie rule).
    each_candidate([&](const FamilyRef&, std::size_t, std::size_t, int support, int confidence) {
      if (support <= 0) return;
      const double product = static_cast<double>(confidence) / row_count;
      if (product > best || product == best) {
        best = product; best_support = support; best_confidence = confidence;
      }
    });
    if (best >= 0.0) {
      min_support = static_cast<double>(best_support) / row_count * options.auto_selection_multiplier;
      min_confidence = static_cast<double>(best_confidence) / best_support * options.auto_selection_multiplier;
    }
  }
  each_candidate([&](const FamilyRef& family, std::size_t a, std::size_t b, int support,
                     int confidence) {
    // The reference uses positive comparisons (`>=`); spelling these as negated
    // `<` would accidentally accept NaN thresholds.
    if (!(support >= row_count * min_support) || support == 0 ||
        !(confidence >= support * min_confidence))
      return;
    DeclareConstraint constraint;
    constraint.first = names[a];
    if (family.binary) constraint.second = names[b];
    constraint.binary = family.binary;
    constraint.support = support;
    constraint.confidence = confidence;
    model.add(family.name, std::move(constraint));
  });
  return model;
}

DeclareModel discover_declare(std::initializer_list<Trace> traces,
                              const DeclareDiscoveryOptions& options) {
  EventLog log;
  log.traces.assign(traces.begin(), traces.end());
  return discover_declare(log, options);
}

DeclareModel discover_declare(const EventStream& stream,
                              const DeclareDiscoveryOptions& options) {
  return discover_declare(event_data_log(EventDataView(stream),
                                         options.activity_key,
                                         options.case_id_key),
                          options);
}

DeclareModel discover_declare(const EventTable& table,
                              const DeclareDiscoveryOptions& options) {
  return discover_declare(event_data_log(EventDataView(table),
                                         options.activity_key,
                                         options.case_id_key),
                          options);
}

DeclareModel discover_declare(const LazyEventTable& table,
                              const DeclareDiscoveryOptions& options) {
  return discover_declare(event_data_log(EventDataView(table),
                                         options.activity_key,
                                         options.case_id_key),
                          options);
}

DeclareModel discover_declare(const EventDataView& data,
                              const DeclareDiscoveryOptions& options) {
  if (data.kind() == EventDataKind::kEventLog)
    return discover_declare(*data.log(), options);
  return discover_declare(
      event_data_log(data, options.activity_key, options.case_id_key), options);
}

namespace {

// A model prepared for checking many traces: the families the check visits,
// in its order, with each rule's labels interned (trace labels outside the
// model get ids as they appear). Equal ids are equal labels, so each
// template reads the same occurrences as the string-keyed check.
class DeclareChecker {
 public:
  explicit DeclareChecker(const DeclareModel& model) {
    for (const auto& [_, values] : model.rules) total_ += static_cast<int>(values.size());
    const char* const order[] = {
        kDeclareExistence, kDeclareExactlyOne, kDeclareInit,
        kDeclareRespondedExistence, kDeclareCoexistence, kDeclareNonCoexistence,
        kDeclareResponse, kDeclarePrecedence, kDeclareSuccession,
        kDeclareAltResponse, kDeclareChainResponse, kDeclareAltPrecedence,
        kDeclareChainPrecedence, kDeclareAltSuccession, kDeclareChainSuccession,
        kDeclareAbsence, kDeclareNonSuccession, kDeclareNonChainSuccession};
    for (std::size_t kind = 0; kind < std::size(order); ++kind) {
      const auto it = model.rules.find(order[kind]);
      if (it == model.rules.end()) continue;
      Family family{kind, it->first, {}};
      for (const auto& rule : it->second)
        family.rules.push_back({id(rule.first), id(rule.second), &rule});
      families_.push_back(std::move(family));
    }
  }

  void intern(const std::vector<const std::string*>& trace, std::vector<std::uint32_t>& ids) {
    ids.clear();
    for (const auto* name : trace) ids.push_back(id(*name));
  }

  DeclareConformance check(const std::vector<std::uint32_t>& trace) {
    if (positions_.size() < names_.size()) positions_.resize(names_.size());
    for (const auto a : present_) positions_[a].clear();
    present_.clear();
    for (std::size_t i = 0; i < trace.size(); ++i) {
      auto& list = positions_[trace[i]];
      if (list.empty()) present_.push_back(trace[i]);
      list.push_back(static_cast<int>(i));
    }
    const int length = static_cast<int>(trace.size());
    const auto in = [&](std::uint32_t a) { return !positions_[a].empty(); };
    const auto at = [&](std::uint32_t a) -> const std::vector<int>& { return positions_[a]; };
    const auto chain_response = [&](std::uint32_t a, std::uint32_t b) {
      for (const int i : at(a))
        if (i + 1 >= length || trace[static_cast<std::size_t>(i) + 1] != b) return false;
      return true;
    };
    const auto chain_precedence = [&](std::uint32_t a, std::uint32_t b) {
      for (const int i : at(b))
        if (i == 0 || trace[static_cast<std::size_t>(i) - 1] != a) return false;
      return true;
    };
    const auto ordered = [&](std::uint32_t a, std::uint32_t b) {
      return in(a) && in(b) && at(a).front() < at(b).front() && at(a).back() < at(b).back();
    };

    DeclareConformance result;
    result.no_constr_total = total_;
    for (const auto& family : families_) {
      for (const auto& rule : family.rules) {
        const auto a = rule.first, b = rule.second;
        bool holds = true;
        switch (family.kind) {
          case 0: holds = in(a); break;
          case 1: holds = at(a).size() == 1; break;
          case 2: holds = !trace.empty() && trace.front() == a; break;
          case 3: holds = !in(a) || in(b); break;
          case 4: holds = in(a) == in(b); break;
          case 5: holds = !(in(a) && in(b)); break;
          // A self-pair (a, a) of a supplied model always holds, as in the
          // reference: its last (first) a is not after (before) itself.
          case 6: holds = !in(a) || (in(b) && at(a).back() <= at(b).back()); break;
          case 7: holds = !in(b) || (in(a) && at(a).front() <= at(b).front()); break;
          case 8:
            holds = (!in(a) && !in(b)) ||
                    (in(a) && in(b) && at(a).front() <= at(b).front() && at(a).back() <= at(b).back());
            break;
          case 9: holds = alt_response_positions(at(a), at(b), length); break;
          case 10: holds = chain_response(a, b); break;
          case 11: holds = alt_precedence_positions(at(a), at(b)); break;
          case 12: holds = chain_precedence(a, b); break;
          case 13:
            holds = alt_response_positions(at(a), at(b), length) &&
                    alt_precedence_positions(at(a), at(b));
            break;
          case 14: holds = chain_response(a, b) && chain_precedence(a, b); break;
          case 15: holds = !in(a); break;
          case 16: holds = (!in(a) && !in(b)) || !ordered(a, b); break;
          default:
            holds = !in(a) || !in(b) || !(chain_response(a, b) && chain_precedence(a, b));
        }
        if (!holds)
          result.deviations.push_back(
              {family.name, rule.constraint->first, rule.constraint->second, rule.constraint->binary});
      }
    }
    result.no_dev_total = static_cast<int>(result.deviations.size());
    result.dev_fitness = result.no_constr_total > 0 ? 1.0 - static_cast<double>(result.no_dev_total) / result.no_constr_total : 1.0;
    result.is_fit = result.no_dev_total == 0;
    return result;
  }

 private:
  struct Rule {
    std::uint32_t first, second;
    const DeclareConstraint* constraint;
  };
  struct Family {
    std::size_t kind;
    std::string name;
    std::vector<Rule> rules;
  };

  std::uint32_t id(const std::string& name) {
    const auto [it, added] = ids_.try_emplace(name, static_cast<std::uint32_t>(names_.size()));
    if (added) names_.push_back(&it->first);
    return it->second;
  }

  int total_ = 0;
  std::unordered_map<std::string, std::uint32_t> ids_;
  std::vector<const std::string*> names_;
  std::vector<Family> families_;
  std::vector<std::vector<int>> positions_;
  std::vector<std::uint32_t> present_;
};

struct IdSequenceHash {
  std::size_t operator()(const std::vector<std::uint32_t>& ids) const noexcept {
    std::uint64_t value = 1469598103934665603ull;
    for (const auto id : ids) value = (value ^ id) * 1099511628211ull;
    return static_cast<std::size_t>(value);
  }
};

}  // namespace

DeclareConformance check_declare_conformance(const std::vector<std::string>& trace,
                                             const DeclareModel& model) {
  DeclareChecker checker(model);
  std::vector<const std::string*> labels;
  labels.reserve(trace.size());
  for (const auto& activity : trace) labels.push_back(&activity);
  std::vector<std::uint32_t> ids;
  checker.intern(labels, ids);
  return checker.check(ids);
}

std::vector<DeclareConformance> check_declare_conformance(const EventLog& log,
                                                          const DeclareModel& model) {
  return check_declare_conformance(log, model, DeclareConformanceOptions{});
}

std::vector<DeclareConformance> check_declare_conformance(
    const EventLog& log, const DeclareModel& model,
    const DeclareConformanceOptions& options) {
  validate_activity_key(options.activity_key);
  std::vector<DeclareConformance> result;
  result.reserve(log.traces.size());
  // A trace's result depends only on its activity sequence, so each distinct
  // sequence (variant) is checked once.
  DeclareChecker checker(model);
  std::unordered_map<std::vector<std::uint32_t>, std::size_t, IdSequenceHash> checked;
  std::vector<const std::string*> labels;
  std::vector<std::uint32_t> ids;
  for (const auto& trace : log.traces) {
    labels.clear();
    for (const auto& event : trace.events) {
      const auto* value = resolve_event_field(event, options.activity_key);
      if (!value)
        throw std::invalid_argument(
            "declare: event activity selector is missing or null: '" +
            options.activity_key + "'");
      labels.push_back(value);
    }
    checker.intern(labels, ids);
    const auto found = checked.find(ids);
    if (found != checked.end()) {
      result.push_back(result[found->second]);
      continue;
    }
    result.push_back(checker.check(ids));
    checked.emplace(ids, result.size() - 1);
  }
  return result;
}

std::vector<DeclareConformance> check_declare_conformance(
    const EventStream& stream, const DeclareModel& model,
    const DeclareConformanceOptions& options) {
  return check_declare_conformance(
      event_data_log(EventDataView(stream), options.activity_key,
                     options.case_id_key),
      model,
      options);
}

std::vector<DeclareConformance> check_declare_conformance(
    const EventTable& table, const DeclareModel& model,
    const DeclareConformanceOptions& options) {
  return check_declare_conformance(
      event_data_log(EventDataView(table), options.activity_key,
                     options.case_id_key),
      model,
      options);
}

std::vector<DeclareConformance> check_declare_conformance(
    const LazyEventTable& table, const DeclareModel& model,
    const DeclareConformanceOptions& options) {
  return check_declare_conformance(
      event_data_log(EventDataView(table), options.activity_key,
                     options.case_id_key),
      model,
      options);
}

std::vector<DeclareConformance> check_declare_conformance(
    const EventDataView& data, const DeclareModel& model,
    const DeclareConformanceOptions& options) {
  if (data.kind() == EventDataKind::kEventLog)
    return check_declare_conformance(*data.log(), model, options);
  return check_declare_conformance(
      event_data_log(data, options.activity_key, options.case_id_key), model,
      options);
}

DeclareConformance check_declare_conformance(const Trace& trace,
                                              const DeclareModel& model) {
  return check_declare_conformance(trace, model, DeclareConformanceOptions{});
}

DeclareConformance check_declare_conformance(
    const Trace& trace, const DeclareModel& model,
    const DeclareConformanceOptions& options) {
  validate_activity_key(options.activity_key);
  std::vector<std::string> activities;
  activities.reserve(trace.events.size());
  for (const auto& event : trace.events)
    activities.push_back(selected_activity(event, options.activity_key));
  return check_declare_conformance(activities, model);
}

namespace {

void validate_case_id_key(std::string_view key) {
  if (key.empty() ||
      std::all_of(key.begin(), key.end(), [](char value) {
        return std::isspace(static_cast<unsigned char>(value)) != 0;
      }))
    throw std::invalid_argument(
        "declare diagnostics: case-id selector must not be blank");
}

std::optional<EventAttribute> trace_case_attribute(
    const Trace& trace, std::string_view case_id_key) {
  if (trace.case_id_attribute && trace.case_id_attribute->key == case_id_key) {
    if (trace.case_id_attribute->is_null) return std::nullopt;
    return *trace.case_id_attribute;
  }
  for (const auto& attribute : trace.attributes)
    if (attribute.key == case_id_key)
      return attribute.is_null ? std::nullopt
                               : std::optional<EventAttribute>(attribute);
  const bool intrinsic =
      case_id_key == "case:concept:name" || case_id_key == "case:id" ||
      case_id_key == "case_id" || case_id_key == "case" ||
      (!trace.case_id_source_key.empty() &&
       trace.case_id_source_key == case_id_key);
  if (intrinsic) {
    if (trace.case_id_state == FirstClassAttributeState::kNull ||
        !trace.has_case_id())
      return std::nullopt;
    if (!trace.case_id_source_key.empty() &&
        trace.case_id_source_key == case_id_key && trace.case_id_attribute)
      return *trace.case_id_attribute;
    return EventAttribute{std::string(case_id_key), trace.case_id};
  }
  for (const auto& event : trace.events) {
    if (!event.case_id_source_key.empty() &&
        event.case_id_source_key == case_id_key) {
      if (event.case_id_state == FirstClassAttributeState::kNull ||
          !event.has_case_id())
        return std::nullopt;
      return EventAttribute{std::string(case_id_key), event.case_id};
    }
  }
  return std::nullopt;
}

EventLog case_id_log(const EventDataView& data,
                     std::string_view case_id_key) {
  return event_data_log(data, "concept:name", case_id_key);
}

EventAttribute case_cell(EventAttribute result, std::string source_key) {
  result.key = "case_id";
  result.first_class_role = FirstClassAttributeRole::kCaseId;
  result.first_class_state = FirstClassAttributeState::kPresent;
  result.first_class_source_key = std::move(source_key);
  return result;
}

EventAttribute integer_cell(std::string key, std::int64_t value) {
  EventAttribute result{std::move(key), std::to_string(value),
                        AttributeKind::kInt};
  result.typed_value = value;
  return result;
}

EventAttribute floating_cell(std::string key, double value) {
  if (!std::isfinite(value))
    throw std::invalid_argument(
        "declare diagnostics: result fitness must be finite");
  std::ostringstream formatted;
  formatted << std::setprecision(std::numeric_limits<double>::max_digits10)
            << value;
  EventAttribute result{std::move(key), formatted.str(), AttributeKind::kFloat};
  result.typed_value = value;
  return result;
}

EventTable make_diagnostics_table(
    const EventLog& log, const std::vector<DeclareConformance>& results,
    const DeclareDiagnosticsOptions& options) {
  validate_case_id_key(options.case_id_key);
  if (results.size() != log.traces.size())
    throw std::invalid_argument(
        "declare diagnostics: result count does not match trace count");
  EventTable table;
  // The reference builds its frame from the result rows, so an empty log
  // gives a frame without columns.
  if (!results.empty())
    table.columns = {"case_id", "no_dev_total", "no_constr_total",
                     "dev_fitness"};
  table.rows.reserve(results.size());
  for (std::size_t index = 0; index < results.size(); ++index) {
    auto id = trace_case_attribute(log.traces[index], options.case_id_key);
    if (!id)
      throw std::invalid_argument(
          "declare diagnostics: case-id selector is missing or null");
    const auto& result = results[index];
    table.rows.push_back(
        {case_cell(std::move(*id), options.case_id_key),
         integer_cell("no_dev_total", result.no_dev_total),
         integer_cell("no_constr_total", result.no_constr_total),
         floating_cell("dev_fitness", result.dev_fitness)});
  }
  table.validate();
  return table;
}

}  // namespace

EventTable declare_conformance_diagnostics(
    const EventLog& log, const std::vector<DeclareConformance>& results,
    const DeclareDiagnosticsOptions& options) {
  return make_diagnostics_table(log, results, options);
}

EventTable declare_conformance_diagnostics(
    const EventStream& stream, const std::vector<DeclareConformance>& results,
    const DeclareDiagnosticsOptions& options) {
  return make_diagnostics_table(case_id_log(EventDataView(stream),
                                             options.case_id_key),
                                results, options);
}

EventTable declare_conformance_diagnostics(
    const EventTable& table, const std::vector<DeclareConformance>& results,
    const DeclareDiagnosticsOptions& options) {
  return make_diagnostics_table(case_id_log(EventDataView(table),
                                             options.case_id_key),
                                results, options);
}

EventTable declare_conformance_diagnostics(
    const LazyEventTable& table, const std::vector<DeclareConformance>& results,
    const DeclareDiagnosticsOptions& options) {
  return make_diagnostics_table(case_id_log(EventDataView(table),
                                             options.case_id_key),
                                results, options);
}

EventTable declare_conformance_diagnostics(
    const EventDataView& data, const std::vector<DeclareConformance>& results,
    const DeclareDiagnosticsOptions& options) {
  return make_diagnostics_table(case_id_log(data, options.case_id_key),
                                results, options);
}

DeclareModel discover_declare_as_uploaded(const EventLog& log,
                                          const DeclareDiscoveryOptions& options) {
  if (!log.dataframe_columns) return discover_declare(log, options);
  return discover_declare(frame_slice_cases(log), options);
}

namespace {

// A table in frame order with each case read as its slice: for every case
// (by first row) the rows from its first row on, as many as it has, each
// copy labelled with the case's id cell. Rows without a case cell (an XES
// trace without a name, on which pm4py's DataFrame Declare raises) keep
// their place, so each such trace stays its own case and the results stay
// one per trace (round BM). A table without the column is returned as is.
EventTable frame_slice_table(const EventTable& table, std::string_view case_id_key) {
  table.validate();
  const auto column = std::find(table.columns.begin(), table.columns.end(), case_id_key);
  if (column == table.columns.end()) return table;
  const auto index = static_cast<std::size_t>(column - table.columns.begin());
  struct Extent {
    std::size_t first = 0, count = 0;
  };
  std::vector<std::string> order;
  std::unordered_map<std::string, Extent> extent;
  for (std::size_t row = 0; row < table.rows.size(); ++row) {
    const auto& cell = table.rows[row][index];
    if (!cell || cell->is_null) continue;
    const auto [found, added] = extent.emplace(cell->value, Extent{row, 0});
    if (added) order.push_back(cell->value);
    ++found->second.count;
  }
  bool adjacent = true;
  for (const auto& key : order) {
    const auto& [first, count] = extent.at(key);
    for (std::size_t row = first; row < first + count && adjacent; ++row) {
      const auto& cell = table.rows[row][index];
      adjacent = cell && !cell->is_null && cell->value == key;
    }
  }
  if (adjacent) return table;
  EventTable sliced = table;
  sliced.rows.clear();
  sliced.index.reset();
  for (std::size_t at = 0; at < table.rows.size(); ++at) {
    const auto& label = table.rows[at][index];
    if (!label || label->is_null) {
      sliced.rows.push_back(table.rows[at]);
      continue;
    }
    const auto& [first, count] = extent.at(label->value);
    if (first != at) continue;
    for (std::size_t row = first; row < first + count; ++row) {
      sliced.rows.push_back(table.rows[row]);
      sliced.rows.back()[index] = label;
    }
  }
  return sliced;
}

// Round HA (observed black-box against pm4py 2.7.22 on pandas 3.0.5): pm4py's
// Declare discovery on a pandas frame keyed by a typed column (an XES column
// read_xes types as ints, floats, booleans, dates or objects, or a text
// column with gaps). The activities are the column's objects: equal numbers
// one activity (0, False and 0.0), each NaN of a float64 column an activity
// of its own (numpy hands out a new object per row), the one NaN object of an
// object or text column, NaT and None one activity each. NaN and NaT are
// unequal to themselves, so they pair with themselves in every binary
// template (where the chain templates and init, which compare with `==`,
// never hold). The rules table then becomes a pandas frame whose columns are
// a MultiIndex of (template, first, second): each level infers its dtype from
// its distinct values (the second level always has the unary rules' missing
// value), and iterating it hands out an int level as floats where the level
// has a missing value. Each such float is looked up again: an int past 2**53
// finds another activity's column or none (KeyError); a frame with two
// missing activities (two NaNs) has duplicate columns, where every lookup
// selects a frame and counts every row. The automatic thresholds sort
// (product, column) in reverse, which raises TypeError on columns whose
// labels do not order; a rule whose second label is missing or falsy (0,
// 0.0, False, '') keeps only its first label as its key, and later rules
// with an equal key replace earlier ones. The order in which pm4py visits the
// columns follows the hash seed (a set of tuples holding the template
// names), so the KeyError's column, the TypeError's operands and which of two
// rules sharing a key survives are declared; Minera visits the columns in
// (template, first, second) order.
struct TypedDeclareClass {
  python::Scalar rep;     // the activity's first object
  std::string type;       // its Python type name
  bool na = false;        // NaN, NaT or None: a pandas level's missing value
  bool unequal = false;   // NaN or NaT: != itself
  bool none = false;      // None: no init rule holds after it
};

enum class TypedLevelDtype { kInt64, kUInt64, kFloat64, kBool, kStr, kDatetime, kObject };

// One element of a column label as iterating the MultiIndex hands it out.
struct TypedLabel {
  enum class Kind { kValue, kFreshNaN, kSharedNaN, kNaT };
  Kind kind = Kind::kValue;
  python::Scalar value;
  std::string type;
};

struct TypedLevel {
  TypedLevelDtype dtype = TypedLevelDtype::kObject;
  std::vector<TypedLabel> categories;
  std::unordered_map<std::string, int> code_of;  // python::identity -> code
  std::vector<int> class_code;                   // activity -> code (-1 missing)
  bool has_na = false;

  [[nodiscard]] TypedLabel label(int code) const {
    TypedLabel out;
    if (code < 0) {
      out.value = python::real(std::numeric_limits<double>::quiet_NaN());
      out.type = "float";
      if (dtype == TypedLevelDtype::kInt64 || dtype == TypedLevelDtype::kUInt64 || dtype == TypedLevelDtype::kFloat64)
        out.kind = TypedLabel::Kind::kFreshNaN;
      else if (dtype == TypedLevelDtype::kDatetime) {
        out.kind = TypedLabel::Kind::kNaT;
        out.type = "NaTType";
      } else
        out.kind = TypedLabel::Kind::kSharedNaN;
      return out;
    }
    out = categories[static_cast<std::size_t>(code)];
    if (has_na && (dtype == TypedLevelDtype::kInt64 || dtype == TypedLevelDtype::kUInt64)) {
      out.value = python::real(std::strtod(python::str(out.value).c_str(), nullptr));
      out.type = "float";
    }
    return out;
  }
  // Index.get_loc of a label: -1 for a missing one, nullopt for a KeyError.
  [[nodiscard]] std::optional<int> get_loc(const TypedLabel& label) const {
    if (label.kind != TypedLabel::Kind::kValue) return -1;
    const auto found = code_of.find(python::identity(label.value));
    if (found == code_of.end()) return std::nullopt;
    return found->second;
  }
};

bool typed_int_above_int64(const python::Scalar& s) {
  return s.kind == python::Scalar::Kind::kInt && !s.negative &&
         python::compare_digits(s.digits, "9223372036854775807") > 0;
}

TypedLevel typed_declare_level(const std::vector<TypedDeclareClass>& classes, const std::vector<char>& present,
                               detail::XesFrameDtype column_dtype) {
  using K = python::Scalar::Kind;
  using D = detail::XesFrameDtype;
  TypedLevel level;
  level.class_code.assign(classes.size(), -1);
  std::vector<std::size_t> reps;
  for (std::size_t c = 0; c < classes.size(); ++c)
    if (present[c] && !classes[c].na) reps.push_back(c);
  if (reps.empty()) return level;
  bool all_bool = true, all_int = true, all_number = true, any_bool = false, all_text = true, all_date = true;
  bool one_offset = true, any_negative = false, any_above = false, all_in_int64 = true;
  std::vector<python::Scalar> labels;
  for (const auto c : reps) {
    const auto& s = classes[c].rep;
    all_bool = all_bool && s.kind == K::kBool;
    all_int = all_int && s.kind == K::kInt;
    all_number = all_number && (s.kind == K::kInt || s.kind == K::kFloat);
    any_bool = any_bool || s.kind == K::kBool;
    all_text = all_text && s.kind == K::kText;
    all_date = all_date && s.kind == K::kDate && s.date.timezone_aware;
    if (s.kind == K::kDate && classes[reps.front()].rep.kind == K::kDate)
      one_offset = one_offset && s.date.offset_minutes == classes[reps.front()].rep.date.offset_minutes;
    if (s.kind == K::kInt) {
      any_negative = any_negative || s.negative;
      any_above = any_above || typed_int_above_int64(s);
      if (s.negative && python::compare_digits(s.digits, "9223372036854775808") > 0) all_in_int64 = false;
      if (!s.negative && python::compare_digits(s.digits, "18446744073709551615") > 0) all_in_int64 = false;
    }
    labels.push_back(s);
  }
  switch (column_dtype) {
    case D::kInt: level.dtype = any_above ? TypedLevelDtype::kUInt64 : TypedLevelDtype::kInt64; break;
    case D::kFloat:
    case D::kNaNFloat: level.dtype = TypedLevelDtype::kFloat64; break;
    case D::kBool: level.dtype = TypedLevelDtype::kBool; break;
    case D::kDatetime: level.dtype = TypedLevelDtype::kDatetime; break;
    case D::kString: level.dtype = TypedLevelDtype::kStr; break;
    case D::kObject:
    case D::kNoneObject:
      if (all_bool) level.dtype = TypedLevelDtype::kBool;
      else if (all_int)
        level.dtype = !all_in_int64 || (any_negative && any_above) ? TypedLevelDtype::kObject
                      : any_above                                 ? TypedLevelDtype::kUInt64
                                                                  : TypedLevelDtype::kInt64;
      else if (all_number && !any_bool)
        level.dtype = python::float_level(labels) ? TypedLevelDtype::kFloat64 : TypedLevelDtype::kObject;
      else if (all_text) level.dtype = TypedLevelDtype::kStr;
      else if (all_date && one_offset) level.dtype = TypedLevelDtype::kDatetime;
      else level.dtype = TypedLevelDtype::kObject;
      break;
  }
  for (const auto c : reps) {
    TypedLabel category;
    category.value = classes[c].rep;
    category.type = classes[c].type;
    if (level.dtype == TypedLevelDtype::kFloat64 && category.value.kind == K::kInt) {
      category.value = python::real(std::strtod(python::str(category.value).c_str(), nullptr));
      category.type = "float";
    } else if (level.dtype == TypedLevelDtype::kDatetime) {
      category.type = "Timestamp";
    } else if (level.dtype == TypedLevelDtype::kInt64 || level.dtype == TypedLevelDtype::kUInt64) {
      category.type = "int";
    }
    const auto id = python::identity(category.value);
    const auto [found, added] = level.code_of.emplace(id, static_cast<int>(level.categories.size()));
    // ints past 2**53 beside floats can meet as one float64 category.
    if (!added) throw std::invalid_argument("ValueError: Categorical categories must be unique");
    level.class_code[c] = found->second;
    level.categories.push_back(std::move(category));
  }
  return level;
}

bool typed_same(const TypedLabel& a, const TypedLabel& b) {
  using L = TypedLabel::Kind;
  if (a.kind == L::kFreshNaN || b.kind == L::kFreshNaN) return false;
  if (a.kind != b.kind) return false;
  if (a.kind != L::kValue) return true;
  return python::equal(a.value, b.value);
}

bool typed_less(const TypedLabel& a, const TypedLabel& b) {
  using L = TypedLabel::Kind;
  const auto unorderable = [&]() -> bool {
    throw python::TypeError("'<' not supported between instances of '" + a.type + "' and '" + b.type + "'");
  };
  if (a.kind == L::kNaT || b.kind == L::kNaT) {
    const auto dated = [](const TypedLabel& x) {
      return x.kind == L::kNaT || (x.kind == L::kValue && x.value.kind == python::Scalar::Kind::kDate);
    };
    if (dated(a) && dated(b)) return false;
    return unorderable();
  }
  try {
    return python::compare(a.value, b.value) < 0;
  } catch (const python::TypeError& error) {
    if (std::string_view(error.what()).rfind("'<' not supported", 0) == 0) return unorderable();
    throw;
  }
}

std::string typed_label_repr(const TypedLabel& label) {
  using L = TypedLabel::Kind;
  if (label.kind == L::kNaT) return "NaT";
  if (label.kind != L::kValue) return "nan";
  if (label.value.kind == python::Scalar::Kind::kText) return detail::frame_python_str_repr(label.value.text);
  return python::str(label.value);
}

std::string typed_label_str(const TypedLabel& label) {
  using L = TypedLabel::Kind;
  if (label.kind == L::kNaT) return "NaT";
  if (label.kind != L::kValue) return "nan";
  return python::str(label.value);
}

bool typed_label_falsy(const TypedLabel& label) {
  using K = python::Scalar::Kind;
  if (label.kind != TypedLabel::Kind::kValue) return true;
  const auto& s = label.value;
  switch (s.kind) {
    case K::kBool: return !s.boolean;
    case K::kInt: return s.digits == "0";
    case K::kFloat: return s.real == 0.0;
    case K::kText: return s.text.empty();
    default: return false;
  }
}

// Activities and their traces as pm4py's pandas projection reads them; the
// model itself (classes per trace, nullopt when the column is plain text
// without gaps or outside the typed model).
struct TypedDeclareTraces {
  std::vector<TypedDeclareClass> classes;
  std::vector<std::vector<std::size_t>> traces;
  std::vector<std::string> case_ids;  // each trace's case (empty for a row without one)
  detail::XesFrameDtype dtype = detail::XesFrameDtype::kObject;
  std::optional<std::size_t> shared_nan;  // the class of the column's one NaN object
  std::optional<std::size_t> nat;         // the class of NaT
};

std::optional<TypedDeclareTraces> typed_declare_traces(const EventTable& table, std::string_view case_id_key,
                                                       std::string_view activity_name) {
  const auto column = detail::xes_frame_activity_column(table, std::string(activity_name), false);
  if (!column) return std::nullopt;
  bool gaps = false;
  for (const auto& cell : column->cells) gaps = gaps || cell.missing;
  if (!column->typed && !gaps) return std::nullopt;
  const auto found = std::find(table.columns.begin(), table.columns.end(), case_id_key);
  if (found == table.columns.end()) return std::nullopt;
  const auto case_at = static_cast<std::size_t>(found - table.columns.begin());
  TypedDeclareTraces out;
  out.dtype = column->dtype;
  const bool fresh_nan = column->dtype == detail::XesFrameDtype::kFloat || column->dtype == detail::XesFrameDtype::kNaNFloat;
  std::unordered_map<std::string, std::size_t> class_of;
  const auto class_for = [&](std::size_t row) {
    const auto& cell = column->cells[row];
    std::string key;
    if (cell.nat) key = "NaT";
    else if (cell.value.kind == python::Scalar::Kind::kNone) key = "N";
    else if (cell.missing) key = fresh_nan ? "nan#" + std::to_string(row) : "nan";
    else key = python::identity(cell.value);
    const auto [at, added] = class_of.emplace(key, out.classes.size());
    if (added) {
      if (key == "nan") out.shared_nan = out.classes.size();
      if (key == "NaT") out.nat = out.classes.size();
      TypedDeclareClass c;
      c.rep = cell.value;
      c.type = cell.type;
      c.na = cell.missing;
      c.unequal = cell.missing && (cell.nat || cell.value.kind != python::Scalar::Kind::kNone);
      c.none = !cell.nat && cell.value.kind == python::Scalar::Kind::kNone;
      out.classes.push_back(std::move(c));
    }
    return at->second;
  };
  // Each case as its slice of the frame's rows (from its first row on, as
  // many rows as it has), cases by first row (the order only decides which
  // of two equal objects names an activity, declared above).
  struct Extent {
    std::size_t first = 0, count = 0;
  };
  std::vector<std::string> order;
  std::unordered_map<std::string, Extent> extent;
  std::vector<std::size_t> loose;
  for (std::size_t row = 0; row < table.rows.size(); ++row) {
    const auto& cell = table.rows[row][case_at];
    if (!cell || cell->is_null) continue;
    const auto [at, added] = extent.emplace(cell->value, Extent{row, 0});
    if (added) order.push_back(cell->value);
    ++at->second.count;
  }
  for (std::size_t row = 0; row < table.rows.size(); ++row) {
    const auto& cell = table.rows[row][case_at];
    if (!cell || cell->is_null) {
      out.traces.push_back({class_for(row)});
      out.case_ids.emplace_back();
      continue;
    }
    const auto& [first, count] = extent.at(cell->value);
    if (first != row) continue;
    std::vector<std::size_t> trace;
    for (std::size_t r = first; r < first + count && r < table.rows.size(); ++r) trace.push_back(class_for(r));
    out.traces.push_back(std::move(trace));
    out.case_ids.push_back(cell->value);
  }
  return out;
}

// pm4py writes the derived binary families (succession, altsuccession,
// chainsuccession, coexistence, noncoexistence, nonsuccession,
// nonchainsuccession) act by act and partner by partner over a Python set of
// the activities, filled trace by trace (round HD, observed black-box with
// objects of chosen hashes). A collapsed key (act with a falsy or missing
// partner) is written once per such partner, so it keeps the stats of the
// partner the set lists last. The set order follows CPython 3.11: False, 0,
// 0.0 and '' hash to 0, ints and floats by the numeric hash, str by its
// SipHash under PYTHONHASHSEED=0, NaT to -2**63. NaN (np.nan in an object
// column, each cell's own object in a float64 one) hashes by its address,
// which moves with the process: Minera hashes it as 2**60 - 1, the last slot
// of the table (as python::numeric_hash does), which is where the
// reference's np.nan lands after False and True in the small tables these
// columns make in the processes observed; None hashes by its address in the
// interpreter binary (0xad8 in the pinned one's low bits). The base binary
// families come from a set of (template, act, act2) tuples, whose hash
// takes a NaN's whole address, so their collapsed keys keep a partner the
// address decides (declared). A date hashes as pandas' Timestamp does: an
// instant with nanoseconds as the int of its value, else as the timedelta
// (days, seconds, microseconds) of its UTC instant since 0001-01-01, a
// CPython tuple hash.
std::uint64_t timestamp_hash(const ParsedExactTimestamp& date) {
  const auto int_hash = [](std::int64_t v) -> std::uint64_t {
    constexpr std::uint64_t modulus = (std::uint64_t(1) << 61) - 1;
    const std::uint64_t magnitude = v < 0 ? std::uint64_t(0) - static_cast<std::uint64_t>(v) : static_cast<std::uint64_t>(v);
    std::int64_t h = static_cast<std::int64_t>(magnitude % modulus);
    if (v < 0) h = -h;
    if (h == -1) h = -2;
    return static_cast<std::uint64_t>(h);
  };
  // A Timestamp with nanoseconds has the ns unit, whose values fit int64.
  if (date.nanoseconds % 1000 != 0 && date.seconds > -9223372036 && date.seconds < 9223372036)
    return int_hash(date.seconds * 1000000000 + static_cast<std::int64_t>(date.nanoseconds));
  std::int64_t days = date.seconds / 86400, seconds = date.seconds % 86400;
  if (seconds < 0) {
    seconds += 86400;
    --days;
  }
  const std::int64_t items[3] = {days + 719163, seconds, static_cast<std::int64_t>(date.nanoseconds / 1000)};
  constexpr std::uint64_t p1 = 11400714785074694791ULL, p2 = 14029467366897019727ULL, p5 = 2870177450012600261ULL;
  std::uint64_t acc = p5;
  for (const auto item : items) {
    acc += int_hash(item) * p2;
    acc = (acc << 31) | (acc >> 33);
    acc *= p1;
  }
  acc += 3 ^ (p5 ^ 3527539ULL);
  if (acc == ~std::uint64_t(0)) acc = 1546275796;
  return acc;
}

// CPython 3.11's tuple hash (xxHash lanes over the items' hashes).
std::uint64_t python_tuple_hash(std::initializer_list<std::uint64_t> items) {
  constexpr std::uint64_t p1 = 11400714785074694791ULL, p2 = 14029467366897019727ULL, p5 = 2870177450012600261ULL;
  std::uint64_t acc = p5;
  for (const auto item : items) {
    acc += item * p2;
    acc = (acc << 31) | (acc >> 33);
    acc *= p1;
  }
  acc += static_cast<std::uint64_t>(items.size()) ^ (p5 ^ 3527539ULL);
  if (acc == ~std::uint64_t(0)) acc = 1546275796;
  return acc;
}

// The iteration order of a CPython 3.11 set (open addressing, nine linear
// probes, perturbation by 5 bits, resized to the next power of two above 4
// times its entries, twice past 50,000) filled in `insert` order; equal keys
// carry one id.
class CPythonSetOrder {
 public:
  void insert(std::size_t id, std::uint64_t hash) {
    if (!place(table_, id, hash, true)) return;
    ++used_;
    if (used_ * 5 < (table_.size() - 1) * 3) return;
    const std::size_t minused = used_ > 50000 ? used_ * 2 : used_ * 4;
    std::size_t size = 8;
    while (size <= minused) size <<= 1;
    std::vector<Slot> grown(size);
    for (const auto& old : table_)
      if (old.used) place(grown, old.id, old.hash, false);
    table_ = std::move(grown);
  }
  [[nodiscard]] std::vector<std::size_t> order() const {
    std::vector<std::size_t> out;
    out.reserve(used_);
    for (const auto& slot : table_)
      if (slot.used) out.push_back(slot.id);
    return out;
  }
  // set(this): CPython's copy resizes the empty table once, to the smallest
  // power of two above twice the entries, where 5 times them reach 3 times
  // its mask (21), then takes the same slots where the sizes agree, else
  // places each entry afresh in this table's slot order.
  [[nodiscard]] CPythonSetOrder copy() const {
    std::size_t size = 8;
    if (used_ * 5 >= 21)
      while (size <= used_ * 2) size <<= 1;
    CPythonSetOrder out;
    out.used_ = used_;
    if (size == table_.size()) {
      out.table_ = table_;
      return out;
    }
    out.table_.assign(size, Slot{});
    for (const auto& old : table_)
      if (old.used) place(out.table_, old.id, old.hash, false);
    return out;
  }

 private:
  struct Slot {
    std::size_t id = 0;
    std::uint64_t hash = 0;
    bool used = false;
  };
  static bool place(std::vector<Slot>& slots, std::size_t id, std::uint64_t hash, bool check) {
    const std::size_t mask = slots.size() - 1;
    std::uint64_t perturb = hash;
    std::size_t i = static_cast<std::size_t>(hash) & mask;
    for (;;) {
      std::size_t probes = i + 9 <= mask ? 9 : 0;
      for (std::size_t j = i;; ++j) {
        if (!slots[j].used) {
          slots[j] = {id, hash, true};
          return true;
        }
        if (check && slots[j].id == id) return false;
        if (probes == 0) break;
        --probes;
      }
      perturb >>= 5;
      i = (i * 5 + 1 + static_cast<std::size_t>(perturb)) & mask;
    }
  }
  std::vector<Slot> table_ = std::vector<Slot>(8);
  std::size_t used_ = 0;
};

// Each activity's hash: nullopt for a NaN, which hashes by its address.
// None hashes by its address in the pinned interpreter (600792).
std::vector<std::optional<std::uint64_t>> typed_class_hashes(const TypedDeclareTraces& input) {
  const auto& classes = input.classes;
  std::vector<std::optional<std::uint64_t>> hashes(classes.size());
  for (std::size_t c = 0; c < classes.size(); ++c) {
    const auto& rep = classes[c].rep;
    using K = python::Scalar::Kind;
    if (input.nat && *input.nat == c) hashes[c] = std::uint64_t(1) << 63;
    else if (classes[c].na && rep.kind != K::kNone) hashes[c] = std::nullopt;
    else if (rep.kind == K::kNone) hashes[c] = 600792;
    else if (rep.kind == K::kBool || rep.kind == K::kInt || rep.kind == K::kFloat) hashes[c] = python::numeric_hash(rep);
    else if (rep.kind == K::kDate) hashes[c] = timestamp_hash(rep.date);
    else hashes[c] = detail::python_str_hash_seed0(rep.text);
  }
  return hashes;
}

std::vector<std::size_t> typed_set_ranks(const TypedDeclareTraces& input) {
  const auto& classes = input.classes;
  const auto hashes = typed_class_hashes(input);
  CPythonSetOrder set;
  for (const auto& trace : input.traces)
    for (const auto cls : trace) set.insert(cls, hashes[cls].value_or((std::uint64_t(1) << 60) - 1));
  std::vector<std::size_t> rank(classes.size(), classes.size());
  std::size_t next = 0;
  // pm4py iterates a copy of the activity set (round HE, observed
  // black-box: set(set(...)), whose table differs from five activities on).
  for (const auto cls : set.copy().order()) rank[cls] = next++;
  for (auto& r : rank)
    if (r == classes.size()) r = next++;
  return rank;
}

// A rule key of the typed model as pm4py's rules dict holds it: (act, act2),
// or act alone where act2 is missing or falsy (`collapsed`).
struct TypedDeclareKey {
  TypedLabel first, second;
  bool collapsed = false;
};
using TypedDeclareRules = std::map<std::string, std::vector<TypedDeclareKey>>;

// A rules-frame column as one number (template, first, second).
std::uint64_t typed_column_key(bool binary, std::size_t family, std::size_t a, std::size_t b, std::size_t n) {
  const std::uint64_t f = binary ? family : kBinaryFamilies + family;
  return (static_cast<std::uint64_t>(f) * n + a) * (n + 1) + (binary ? b : n);
}

// The base families' columns in the rules frame's order (round HE, observed
// black-box): pm4py collects the keys of every variant's rules dict (in the
// variants' first-occurrence order, each dict filled template by template:
// existence, exactly_one and init over the activity set, then
// responded_existence, response, altresponse and chainresponse over each act
// of the trace's Counter and each act2 of the set, precedence,
// altprecedence and chainprecedence over each activated act2 and each act
// of the set) into a set of (template, act[, act2]) tuples, whose order
// becomes the frame's. Each key's position, or nullopt where an activity
// hashes by its address (NaN), whose order stays the columns' (declared).
template <class Pairs>
std::optional<std::unordered_map<std::uint64_t, std::size_t>> typed_base_column_order(
    const TypedDeclareTraces& input, const std::vector<std::size_t>& rank, const std::array<bool, kUnaryFamilies>& unary,
    const std::array<bool, kBinaryFamilies>& binary, const Pairs& pairs) {
  const auto hashes = typed_class_hashes(input);
  for (const auto& h : hashes)
    if (!h) return std::nullopt;
  const std::size_t n = input.classes.size();
  std::vector<std::size_t> activities(n);
  for (std::size_t c = 0; c < n; ++c) activities[c] = c;
  std::sort(activities.begin(), activities.end(), [&](std::size_t x, std::size_t y) { return rank[x] < rank[y]; });
  std::array<std::uint64_t, kUnaryFamilies> unary_hash{};
  std::array<std::uint64_t, kBinaryFamilies> binary_hash{};
  for (std::size_t f = 0; f < kUnaryFamilies; ++f)
    unary_hash[f] = detail::python_str_hash_seed0(kUnaryFamilyNames[f]).value_or(0);
  for (std::size_t f = 0; f < kBinaryFamilies; ++f)
    binary_hash[f] = detail::python_str_hash_seed0(kBinaryFamilyNames[f]).value_or(0);
  CPythonSetOrder set;
  std::unordered_set<std::uint64_t> seen;
  const auto add = [&](bool is_binary, std::size_t family, std::size_t a, std::size_t b) {
    const auto key = typed_column_key(is_binary, family, a, b, n);
    if (!seen.insert(key).second) return;
    const auto hash = is_binary ? python_tuple_hash({binary_hash[family], *hashes[a], *hashes[b]})
                                : python_tuple_hash({unary_hash[family], *hashes[a]});
    set.insert(static_cast<std::size_t>(key), hash);
  };
  std::set<std::vector<std::size_t>> variants;
  std::vector<std::size_t> counter;
  std::vector<char> in(n, 0);
  for (const auto& trace : input.traces) {
    if (!variants.insert(trace).second) continue;
    counter.clear();
    for (const auto c : trace)
      if (!in[c]) {
        in[c] = 1;
        counter.push_back(c);
      }
    for (const auto c : counter) in[c] = 0;
    for (const auto f : {kExists, kExactlyOne, kInit})
      if (unary[f])
        for (const auto a : activities) add(false, f, a, 0);
    for (const auto f : {kRe, kResp, kPrec, kAltResp, kChainResp, kAltPrec, kChainPrec}) {
      if (!binary[f]) continue;
      const bool on_second = f == kPrec || f == kAltPrec || f == kChainPrec;
      for (const auto x : counter)
        for (const auto y : activities) {
          const auto a = on_second ? y : x, b = on_second ? x : y;
          if (pairs(a, b)) add(true, f, a, b);
        }
    }
  }
  std::unordered_map<std::uint64_t, std::size_t> out;
  std::size_t next = 0;
  for (const auto id : set.order()) out.emplace(static_cast<std::uint64_t>(id), next++);
  return out;
}

DeclareModel typed_declare_model(const TypedDeclareTraces& input, const DeclareDiscoveryOptions& options,
                                 TypedDeclareRules* typed_rules = nullptr) {
  const auto& classes = input.classes;
  const std::size_t n = classes.size();
  DeclareModel model;
  const std::size_t nrows = input.traces.size();
  if (n == 0 || nrows == 0) return model;
  std::set<std::string> allowed;
  if (!options.allowed_templates.has_value()) allowed.insert(families().begin(), families().end());
  else allowed = to_set(*options.allowed_templates);
  const auto allows = [&](std::initializer_list<const char*> names) {
    for (const auto* name : names)
      if (!has(allowed, name)) return false;
    return true;
  };
  std::array<bool, kUnaryFamilies> unary{};
  std::array<bool, kBinaryFamilies> binary{};
  unary[kExists] = allows({kDeclareExistence});
  unary[kExactlyOne] = allows({kDeclareExactlyOne});
  unary[kInit] = allows({kDeclareInit});
  unary[kAbsent] = allows({kDeclareAbsence, kDeclareExistence});
  for (const auto f : {kRe, kResp, kPrec, kAltResp, kChainResp, kAltPrec, kChainPrec})
    binary[f] = allows({kBinaryFamilyNames[f]});
  binary[kSucc] = allows({kDeclareSuccession, kDeclareResponse, kDeclarePrecedence});
  binary[kAltSucc] = allows({kDeclareAltSuccession, kDeclareAltResponse, kDeclareAltPrecedence});
  binary[kChainSucc] = allows({kDeclareChainSuccession, kDeclareChainResponse, kDeclareChainPrecedence});
  binary[kCoex] = allows({kDeclareCoexistence, kDeclareRespondedExistence});
  binary[kNonCoex] = allows({kDeclareNonCoexistence, kDeclareCoexistence, kDeclareRespondedExistence});
  binary[kNonSucc] = allows({kDeclareNonSuccession, kDeclareSuccession, kDeclareResponse, kDeclarePrecedence});
  binary[kNonChainSucc] =
      allows({kDeclareNonChainSuccession, kDeclareChainSuccession, kDeclareChainResponse, kDeclareChainPrecedence});
  // A pair is a column where pm4py's `act2 != act` holds.
  const auto pairs = [&](std::size_t a, std::size_t b) { return a != b || classes[a].unequal; };

  // Support and confidence per column, over the variants (dense like evaluate_dense, with self pairs).
  std::map<std::vector<std::size_t>, int> variants;
  for (const auto& trace : input.traces) ++variants[trace];
  std::vector<int> bs(kBinaryFamilies * n * n, 0), bc(bs.size(), 0), us(kUnaryFamilies * n, 0), uc(us.size(), 0);
  const auto at = [&](std::size_t f, std::size_t a, std::size_t b) { return (f * n + a) * n + b; };
  std::vector<std::vector<int>> positions(n);
  std::vector<char> in(n, 0), visited(n * n, 0);
  std::vector<std::size_t> touched, present;
  for (const auto& [trace, count] : variants) {
    present.clear();
    for (std::size_t i = 0; i < trace.size(); ++i) {
      auto& list = positions[trace[i]];
      if (list.empty()) present.push_back(trace[i]);
      list.push_back(static_cast<int>(i));
    }
    for (const auto a : present) in[a] = 1;
    const int length = static_cast<int>(trace.size());
    const auto add_unary = [&](std::size_t f, std::size_t a, int status) {
      if (!unary[f]) return;
      if (status != 0) us[f * n + a] += count;
      if (status == 1) uc[f * n + a] += count;
    };
    for (std::size_t a = 0; a < n; ++a) {
      const int exists = in[a] ? 1 : -1;
      add_unary(kExists, a, exists);
      add_unary(kExactlyOne, a, in[a] && positions[a].size() == 1 ? 1 : -1);
      add_unary(kInit, a,
                !trace.empty() && !classes[trace.front()].none && !classes[a].unequal && trace.front() == a ? 1 : -1);
      add_unary(kAbsent, a, -exists);
    }
    // `==` of an event's object with an activity: never for NaN and NaT.
    const auto is = [&](std::size_t event_class, std::size_t activity) {
      return event_class == activity && !classes[activity].unequal;
    };
    const auto evaluate = [&](std::size_t a, std::size_t b) {
      if (!pairs(a, b) || visited[a * n + b]) return;
      visited[a * n + b] = 1;
      touched.push_back(a * n + b);
      const bool ia = in[a] != 0, ib = in[b] != 0;
      const auto& pa = positions[a];
      const auto& pb = positions[b];
      std::array<int, kBinaryFamilies> s{};
      s[kRe] = ia ? (ib ? 1 : -1) : 0;
      s[kResp] = ia ? ((!ib || pa.back() >= pb.back()) ? -1 : 1) : 0;
      s[kPrec] = ib ? ((!ia || pa.front() >= pb.front()) ? -1 : 1) : 0;
      s[kAltResp] = ia ? (alt_response_positions(pa, pb, length) ? 1 : -1) : 0;
      if (ia) {
        bool ok = true;
        for (const int i : pa)
          if (i + 1 >= length || !is(trace[static_cast<std::size_t>(i) + 1], b)) { ok = false; break; }
        s[kChainResp] = ok ? 1 : -1;
      }
      s[kAltPrec] = ib ? (alt_precedence_positions(pa, pb) ? 1 : -1) : 0;
      if (ib) {
        bool ok = true;
        for (const int i : pb)
          if (i == 0 || !is(trace[static_cast<std::size_t>(i) - 1], a)) { ok = false; break; }
        s[kChainPrec] = ok ? 1 : -1;
      }
      s[kSucc] = std::min(s[kResp], s[kPrec]);
      s[kAltSucc] = std::min(s[kAltResp], s[kAltPrec]);
      s[kChainSucc] = std::min(s[kChainResp], s[kChainPrec]);
      s[kCoex] = std::min(s[kRe], ib ? (ia ? 1 : -1) : 0);
      s[kNonCoex] = -s[kCoex];
      s[kNonSucc] = -s[kSucc];
      s[kNonChainSucc] = -s[kChainSucc];
      for (std::size_t f = 0; f < kBinaryFamilies; ++f) {
        if (!binary[f] || s[f] == 0) continue;
        bs[at(f, a, b)] += count;
        if (s[f] == 1) bc[at(f, a, b)] += count;
      }
    };
    for (const auto a : present)
      for (std::size_t b = 0; b < n; ++b) {
        evaluate(a, b);
        evaluate(b, a);
      }
    for (const auto index : touched) visited[index] = 0;
    touched.clear();
    for (const auto a : present) {
      in[a] = 0;
      positions[a].clear();
    }
  }

  // The columns, in (template, first, second) order.
  struct Column {
    std::size_t family = 0;
    bool binary = false;
    std::size_t a = 0, b = 0;
    std::string name;
  };
  std::vector<Column> columns;
  {
    std::vector<std::pair<std::string, std::pair<bool, std::size_t>>> order;
    for (std::size_t f = 0; f < kBinaryFamilies; ++f)
      if (binary[f]) order.push_back({kBinaryFamilyNames[f], {true, f}});
    for (std::size_t f = 0; f < kUnaryFamilies; ++f)
      if (unary[f]) order.push_back({kUnaryFamilyNames[f], {false, f}});
    std::sort(order.begin(), order.end());
    for (const auto& [name, family] : order) {
      for (std::size_t a = 0; a < n; ++a) {
        if (!family.first) {
          columns.push_back({family.second, false, a, 0, name});
          continue;
        }
        for (std::size_t b = 0; b < n; ++b)
          if (pairs(a, b)) columns.push_back({family.second, true, a, b, name});
      }
    }
  }
  if (columns.empty()) return model;
  std::vector<char> first_present(n, 0), second_present(n, 0);
  for (const auto& column : columns) {
    first_present[column.a] = 1;
    if (column.binary) second_present[column.b] = 1;
  }
  auto level1 = typed_declare_level(classes, first_present, input.dtype);
  auto level2 = typed_declare_level(classes, second_present, input.dtype);
  std::map<std::string, int> family_code;
  struct Codes {
    int f = 0, c1 = -1, c2 = -1;
    bool operator==(const Codes& o) const { return f == o.f && c1 == o.c1 && c2 == o.c2; }
  };
  struct CodesHash {
    std::size_t operator()(const Codes& c) const {
      return std::hash<long long>()((static_cast<long long>(c.f) * 1000003 + c.c1) * 1000003 + c.c2);
    }
  };
  std::vector<Codes> codes;
  codes.reserve(columns.size());
  for (const auto& column : columns) {
    const auto [it, added] = family_code.emplace(column.name, static_cast<int>(family_code.size()));
    (void)added;
    codes.push_back({it->second, level1.class_code[column.a], column.binary ? level2.class_code[column.b] : -1});
    level1.has_na = level1.has_na || codes.back().c1 < 0;
    level2.has_na = level2.has_na || codes.back().c2 < 0;
  }
  std::unordered_map<Codes, std::size_t, CodesHash> column_of;
  bool unique = true;
  for (std::size_t i = 0; i < codes.size(); ++i)
    if (!column_of.emplace(codes[i], i).second) unique = false;
  std::vector<std::array<TypedLabel, 2>> labels;
  labels.reserve(columns.size());
  for (const auto& c : codes) labels.push_back({level1.label(c.c1), level2.label(c.c2)});
  const auto stats_of = [&](std::size_t i) -> std::pair<int, int> {
    const auto& column = columns[i];
    if (column.binary) return {bs[at(column.family, column.a, column.b)], bc[at(column.family, column.a, column.b)]};
    return {us[column.family * n + column.a], uc[column.family * n + column.a]};
  };
  const auto full_repr = [&](std::size_t i) {
    return "(" + detail::frame_python_str_repr(columns[i].name) + ", " + typed_label_repr(labels[i][0]) + ", " +
           typed_label_repr(labels[i][1]) + ")";
  };
  // rules_df[column]: the column the iterated label finds.
  const auto lookup = [&](std::size_t i) -> std::pair<int, int> {
    const auto c1 = level1.get_loc(labels[i][0]);
    const auto c2 = level2.get_loc(labels[i][1]);
    if (unique) {
      if (!c1 || !c2) throw std::invalid_argument("KeyError: " + full_repr(i));
      const auto found = column_of.find(Codes{codes[i].f, *c1, *c2});
      if (found == column_of.end()) throw std::invalid_argument("KeyError: " + full_repr(i));
      return stats_of(found->second);
    }
    if (!c1) throw std::invalid_argument("KeyError: " + typed_label_repr(labels[i][0]));
    if (!c2) throw std::invalid_argument("KeyError: " + typed_label_repr(labels[i][1]));
    if (column_of.find(Codes{codes[i].f, *c1, *c2}) == column_of.end())
      throw std::invalid_argument("KeyError: " + full_repr(i));
    const int rows = static_cast<int>(nrows);
    return {rows, rows};
  };
  // The rules frame's column order (round HE): the base families' columns
  // in the step-1 set order (typed_base_column_order), then absence over the
  // activity set, then the derived binary families, each over the activity
  // set (round HD). Where an activity hashes by its address the base
  // families keep the columns' (template, first, second) order (declared).
  const auto rank = typed_set_ranks(input);
  const auto base = typed_base_column_order(input, rank, unary, binary, pairs);
  const auto derived = [](const Column& c) {
    return c.binary && (c.family == kSucc || c.family == kAltSucc || c.family == kChainSucc || c.family == kCoex ||
                        c.family == kNonCoex || c.family == kNonSucc || c.family == kNonChainSucc);
  };
  std::vector<std::size_t> frame_order(columns.size());
  for (std::size_t i = 0; i < frame_order.size(); ++i) frame_order[i] = i;
  if (base) {
    const auto group = [&](const Column& c) { return derived(c) ? 2 : (!c.binary && c.family == kAbsent) ? 1 : 0; };
    const auto position = [&](const Column& c) {
      const auto found = base->find(typed_column_key(c.binary, c.family, c.a, c.b, n));
      return found == base->end() ? std::numeric_limits<std::size_t>::max() : found->second;
    };
    std::stable_sort(frame_order.begin(), frame_order.end(), [&](std::size_t x, std::size_t y) {
      const auto& p = columns[x];
      const auto& q = columns[y];
      const int gp = group(p), gq = group(q);
      if (gp != gq) return gp < gq;
      if (gp == 0) return position(p) < position(q);
      if (gp == 1) return rank[p.a] < rank[q.a];
      if (p.family != q.family) return p.family < q.family;
      if (rank[p.a] != rank[q.a]) return rank[p.a] < rank[q.a];
      return rank[p.b] < rank[q.b];
    });
  }
  const double rows = static_cast<double>(nrows);
  double min_support = options.has_min_support_ratio ? options.min_support_ratio : 0.0;
  double min_confidence = options.has_min_confidence_ratio ? options.min_confidence_ratio : 0.0;
  if (!options.has_min_support_ratio && !options.has_min_confidence_ratio) {
    struct Product {
      double product;
      std::size_t column;
    };
    std::vector<Product> products;
    for (const auto i : frame_order) {
      const auto [support, confidence] = lookup(i);
      const double ratio = static_cast<double>(support) / rows;
      if (ratio > 0)
        products.push_back({ratio * (static_cast<double>(confidence) / static_cast<double>(support)), i});
    }
    if (products.empty()) return model;
    try {
      python::sort_reverse(products, [&](const Product& x, const Product& y) {
        if (x.product != y.product) return x.product < y.product;
        if (columns[x.column].name != columns[y.column].name) return columns[x.column].name < columns[y.column].name;
        for (std::size_t k = 0; k < 2; ++k) {
          const auto& p = labels[x.column][k];
          const auto& q = labels[y.column][k];
          if (typed_same(p, q)) continue;
          return typed_less(p, q);
        }
        return false;
      });
    } catch (const python::TypeError& error) {
      throw std::invalid_argument(std::string("TypeError: ") + error.what());
    }
    const auto [support, confidence] = lookup(products.front().column);
    min_support = static_cast<double>(support) / rows * options.auto_selection_multiplier;
    min_confidence = support > 0
                         ? static_cast<double>(confidence) / static_cast<double>(support) * options.auto_selection_multiplier
                         : 1.0;
  }
  // The write order: the frame's within each family (where the base order is
  // unknown, the derived binary families over the activity set's order).
  std::vector<std::size_t> writes = frame_order;
  if (!base)
    std::stable_sort(writes.begin(), writes.end(), [&](std::size_t x, std::size_t y) {
      const auto& p = columns[x];
      const auto& q = columns[y];
      if (p.name != q.name) return p.name < q.name;
      if (!derived(p)) {
        const bool pa = !p.binary && p.family == kAbsent;
        return pa && rank[p.a] < rank[q.a];
      }
      if (rank[p.a] != rank[q.a]) return rank[p.a] < rank[q.a];
      return rank[p.b] < rank[q.b];
    });
  std::map<std::string, std::unordered_map<std::string, std::size_t>> seen;
  for (const auto i : writes) {
    const auto [support, confidence] = lookup(i);
    if (!(support >= rows * min_support) || support == 0 || !(confidence >= support * min_confidence)) continue;
    const auto& first = labels[i][0];
    const auto& second = labels[i][1];
    const bool collapsed = typed_label_falsy(second);
    const auto id = [&](const TypedLabel& label, std::size_t k) {
      switch (label.kind) {
        case TypedLabel::Kind::kFreshNaN: return "f#" + std::to_string(i) + "#" + std::to_string(k);
        case TypedLabel::Kind::kSharedNaN: return std::string("nan");
        case TypedLabel::Kind::kNaT: return std::string("NaT");
        case TypedLabel::Kind::kValue: break;
      }
      return python::identity(label.value);
    };
    const auto key = collapsed ? id(first, 0) : id(first, 0) + "\x1f" + id(second, 1);
    auto& family = model.rules[columns[i].name];
    auto& keys = seen[columns[i].name];
    const auto [found, added] = keys.emplace(key, family.size());
    if (!added) {
      family[found->second].support = support;
      family[found->second].confidence = confidence;
      continue;
    }
    DeclareConstraint constraint;
    constraint.first = typed_label_str(first);
    if (!collapsed) constraint.second = typed_label_str(second);
    constraint.binary = !collapsed;
    constraint.support = support;
    constraint.confidence = confidence;
    family.push_back(std::move(constraint));
    if (typed_rules) (*typed_rules)[columns[i].name].push_back({first, second, collapsed});
  }
  return model;
}

// pm4py's Declare conformance against the typed model (round HB, observed
// black-box; checked first as a Python model, hb/x/cmodel.py, against
// pm4py on CSV logs and frames and XES frames): each trace of the
// projection checks the families in conformance_declare's order, each key
// of a family in the model's order. `in trace` and the trace's index dict
// find an object that is identical or ==, so the column's one NaN object and
// NaT find themselves, a NaN of a float64 level nothing; `!=` holds for NaN
// and NaT whatever the other. A collapsed key (act alone under a binary
// template) is subscripted: a str gives its characters (IndexError past its
// end), anything else TypeError "'<type>' object is not subscriptable";
// nonsuccession and nonchainsuccession unpack it ("cannot unpack
// non-iterable <type> object", or a str's ValueError).
std::vector<std::string> typed_code_points(const std::string& text) {
  std::vector<std::string> out;
  for (std::size_t i = 0; i < text.size();) {
    const auto lead = static_cast<unsigned char>(text[i]);
    const std::size_t length = lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xE ? 3 : (lead >> 3) == 0x1E ? 4 : 1;
    out.push_back(text.substr(i, length));
    i += length;
  }
  return out;
}

std::vector<DeclareConformance> typed_declare_conformance(const TypedDeclareTraces& input,
                                                          const TypedDeclareRules& rules) {
  const auto& classes = input.classes;
  std::unordered_map<std::string, std::size_t> value_class;
  for (std::size_t c = 0; c < classes.size(); ++c)
    if (!classes[c].na) value_class.emplace(python::identity(classes[c].rep), c);
  const auto class_of = [&](const TypedLabel& label) -> std::optional<std::size_t> {
    switch (label.kind) {
      case TypedLabel::Kind::kFreshNaN: return std::nullopt;
      case TypedLabel::Kind::kSharedNaN: return input.shared_nan;
      case TypedLabel::Kind::kNaT: return input.nat;
      case TypedLabel::Kind::kValue: break;
    }
    const auto found = value_class.find(python::identity(label.value));
    if (found == value_class.end()) return std::nullopt;
    return found->second;
  };
  const auto chars = [](const TypedLabel& label) -> std::optional<std::vector<std::string>> {
    if (label.kind != TypedLabel::Kind::kValue || label.value.kind != python::Scalar::Kind::kText) return std::nullopt;
    return typed_code_points(label.value.text);
  };
  const auto type_name = [](const TypedLabel& label) {
    if (label.kind == TypedLabel::Kind::kNaT) return std::string("NaTType");
    return label.type.empty() ? std::string("float") : label.type;
  };
  const auto text_label = [](const std::string& text) {
    TypedLabel out;
    out.value = python::text(text);
    out.type = "str";
    return out;
  };
  const auto subscript = [&](const TypedDeclareKey& key, std::size_t at) -> TypedLabel {
    if (!key.collapsed) return at == 0 ? key.first : key.second;
    if (const auto points = chars(key.first)) {
      if (at >= points->size()) throw std::invalid_argument("IndexError: string index out of range");
      return text_label((*points)[at]);
    }
    throw std::invalid_argument("TypeError: '" + type_name(key.first) + "' object is not subscriptable");
  };
  const auto unpack = [&](const TypedDeclareKey& key) -> std::pair<TypedLabel, TypedLabel> {
    if (!key.collapsed) return {key.first, key.second};
    if (const auto points = chars(key.first)) {
      if (points->size() < 2)
        throw std::invalid_argument("ValueError: not enough values to unpack (expected 2, got " +
                                    std::to_string(points->size()) + ")");
      if (points->size() > 2) throw std::invalid_argument("ValueError: too many values to unpack (expected 2)");
      return {text_label((*points)[0]), text_label((*points)[1])};
    }
    throw std::invalid_argument("TypeError: cannot unpack non-iterable " + type_name(key.first) + " object");
  };
  int total = 0;
  for (const auto& [family, keys] : rules) total += static_cast<int>(keys.size());
  static constexpr const char* kOrder[] = {
      kDeclareExistence, kDeclareExactlyOne, kDeclareInit, kDeclareRespondedExistence, kDeclareCoexistence,
      kDeclareNonCoexistence, kDeclareResponse, kDeclarePrecedence, kDeclareSuccession, kDeclareAltResponse,
      kDeclareChainResponse, kDeclareAltPrecedence, kDeclareChainPrecedence, kDeclareAltSuccession,
      kDeclareChainSuccession, kDeclareAbsence, kDeclareNonSuccession, kDeclareNonChainSuccession};
  std::vector<DeclareConformance> out;
  out.reserve(input.traces.size());
  std::vector<std::vector<int>> positions(classes.size());
  for (const auto& trace : input.traces) {
    for (std::size_t i = 0; i < trace.size(); ++i) positions[trace[i]].push_back(static_cast<int>(i));
    const int n = static_cast<int>(trace.size());
    const auto at = [&](const TypedLabel& label) -> const std::vector<int>* {
      const auto c = class_of(label);
      if (!c || positions[*c].empty()) return nullptr;
      return &positions[*c];
    };
    const auto in = [&](const TypedLabel& label) { return at(label) != nullptr; };
    // `trace[i] != label`
    const auto unequal = [&](std::size_t event, const TypedLabel& label) {
      if (label.kind != TypedLabel::Kind::kValue || classes[event].unequal) return true;
      const auto c = class_of(label);
      return !c || *c != event;
    };
    const auto alt_response = [&](const TypedLabel& a, const TypedLabel& b) {
      const auto* ai = at(a);
      if (!ai) return true;
      const auto* bi = at(b);
      if (!bi) return false;
      return alt_response_positions(*ai, *bi, n);
    };
    const auto chain_response = [&](const TypedLabel& a, const TypedLabel& b) {
      const auto* ai = at(a);
      if (!ai) return true;
      for (const int i : *ai)
        if (i + 1 >= n || unequal(trace[static_cast<std::size_t>(i) + 1], b)) return false;
      return true;
    };
    const auto alt_precedence = [&](const TypedLabel& a, const TypedLabel& b) {
      const auto* bi = at(b);
      if (!bi) return true;
      const auto* ai = at(a);
      if (!ai) return false;
      return alt_precedence_positions(*ai, *bi);
    };
    const auto chain_precedence = [&](const TypedLabel& a, const TypedLabel& b) {
      const auto* bi = at(b);
      if (!bi) return true;
      for (const int i : *bi)
        if (i == 0 || unequal(trace[static_cast<std::size_t>(i) - 1], a)) return false;
      return true;
    };
    DeclareConformance result;
    result.no_constr_total = total;
    for (const auto* family : kOrder) {
      const auto found = rules.find(family);
      if (found == rules.end()) continue;
      const std::string_view f = family;
      for (const auto& key : found->second) {
        bool deviates = false;
        if (f == kDeclareExistence) deviates = !in(key.first);
        else if (f == kDeclareAbsence) deviates = in(key.first);
        else if (f == kDeclareExactlyOne) {
          const auto* p = at(key.first);
          deviates = !p || p->size() != 1;
        } else if (f == kDeclareInit) {
          deviates = n == 0 || unequal(trace.front(), key.first);
        } else if (f == kDeclareRespondedExistence) {
          deviates = in(subscript(key, 0)) && !in(subscript(key, 1));
        } else if (f == kDeclareCoexistence) {
          deviates = (in(subscript(key, 0)) && !in(subscript(key, 1))) ||
                     (in(subscript(key, 1)) && !in(subscript(key, 0)));
        } else if (f == kDeclareNonCoexistence) {
          deviates = in(subscript(key, 0)) && in(subscript(key, 1));
        } else if (f == kDeclareResponse) {
          if (const auto* a = at(subscript(key, 0))) {
            const auto* b = at(subscript(key, 1));
            deviates = !b || a->back() > b->back();
          }
        } else if (f == kDeclarePrecedence) {
          if (const auto* b = at(subscript(key, 1))) {
            const auto* a = at(subscript(key, 0));
            deviates = !a || a->front() > b->front();
          }
        } else if (f == kDeclareSuccession) {
          const auto* a = at(subscript(key, 0));
          const auto* b = at(subscript(key, 1));
          if (!a && !b) continue;
          deviates = !a || !b || a->front() > b->front() || a->back() > b->back();
        } else if (f == kDeclareAltResponse) {
          deviates = !alt_response(subscript(key, 0), subscript(key, 1));
        } else if (f == kDeclareChainResponse) {
          deviates = !chain_response(subscript(key, 0), subscript(key, 1));
        } else if (f == kDeclareAltPrecedence) {
          deviates = !alt_precedence(subscript(key, 0), subscript(key, 1));
        } else if (f == kDeclareChainPrecedence) {
          deviates = !chain_precedence(subscript(key, 0), subscript(key, 1));
        } else if (f == kDeclareAltSuccession) {
          const auto a = subscript(key, 0), b = subscript(key, 1);
          deviates = !(alt_response(a, b) && alt_precedence(a, b));
        } else if (f == kDeclareChainSuccession) {
          const auto a = subscript(key, 0), b = subscript(key, 1);
          deviates = !(chain_response(a, b) && chain_precedence(a, b));
        } else {
          const auto [a, b] = unpack(key);
          const auto* pa = at(a);
          const auto* pb = at(b);
          if (!pa || !pb) continue;
          if (f == kDeclareNonSuccession) deviates = pa->front() < pb->front() && pa->back() < pb->back();
          else deviates = chain_response(a, b) && chain_precedence(a, b);
        }
        if (!deviates) continue;
        DeclareDeviation deviation;
        deviation.template_name = family;
        deviation.first = typed_label_str(key.first);
        if (!key.collapsed) deviation.second = typed_label_str(key.second);
        deviation.binary = !key.collapsed;
        result.deviations.push_back(std::move(deviation));
      }
    }
    result.no_dev_total = static_cast<int>(result.deviations.size());
    result.dev_fitness = total > 0 ? 1.0 - static_cast<double>(result.no_dev_total) / static_cast<double>(total) : 1.0;
    result.is_fit = result.no_dev_total == 0;
    out.push_back(std::move(result));
    for (const auto c : trace) positions[c].clear();
  }
  return out;
}

// An activity no model names: pm4py's pandas Declare conformance keeps a row
// whose activity is missing (NaN) as an event that matches no rule activity
// (round FW, observed black-box).
constexpr std::string_view kPandasMissingActivity = "\x1f__minera_pandas_missing_activity__\x1f";

// pm4py's Declare algorithm on a pandas frame (round FW, observed black-box):
// the frame check without column keys (three columns, two text, one date),
// then KeyError for a missing case column, activity column and
// "time:timestamp" (hard-coded, as on Polars), in that order, then
// np.unique(case column, return_index=True, return_counts=True) in frame
// order, numpy's stable object argsort, whose first comparison of a missing
// case id (NaN) with a named one raises Python's TypeError. Discovery then
// sorts the rule columns, which holds the activities; a missing activity
// (NaN) there raises TypeError too, its operands in an order that follows
// the hash seed (Minera names 'str' first, the usual order). Conformance
// keeps such a row as an event no rule names.
void check_pandas_declare_frame(const EventTable& source, std::string_view case_id_key,
                                std::string_view activity_key, bool discovery) {
  std::optional<EventTable> storage;
  const auto& table = present_columns(source, storage);
  check_dataframe_columns(table.dtypes.empty() ? xes_frame_typed_dtypes(table, false) : table, DataFrameColumnOptions{});
  const auto column_of = [&](std::string_view name) -> std::optional<std::size_t> {
    const auto found = std::find(table.columns.begin(), table.columns.end(), name);
    if (found == table.columns.end()) return std::nullopt;
    return static_cast<std::size_t>(found - table.columns.begin());
  };
  const auto activity_name = activity_key.empty() ? std::string_view("concept:name") : activity_key;
  const auto case_at = column_of(case_id_key);
  if (!case_at) throw std::invalid_argument("KeyError: '" + std::string(case_id_key) + "'");
  const auto activity_at = column_of(activity_name);
  if (!activity_at) throw std::invalid_argument("KeyError: '" + std::string(activity_name) + "'");
  if (!column_of("time:timestamp")) throw std::invalid_argument("KeyError: 'time:timestamp'");
  const auto scalar = [](const std::optional<EventAttribute>& cell) {
    if (!cell || cell->is_null) return python::real(std::numeric_limits<double>::quiet_NaN());
    return python::from_attribute(*cell);
  };
  std::vector<python::Scalar> cases;
  cases.reserve(table.rows.size());
  bool missing = false, named = false;
  for (const auto& row : table.rows) {
    cases.push_back(scalar(row[*case_at]));
    (python::is_missing(cases.back()) ? missing : named) = true;
  }
  if (missing && named)
    (void)detail::numpy_object_stable_argsort(cases.size(), [&](std::size_t a, std::size_t b) {
      return python::compare(cases[a], cases[b]);
    });
  if (!discovery) return;
  // Round GZ (observed black-box): an activity column of other values than
  // text sorts its objects (a float64 column's NaN a float that orders
  // with the rest; NaT apart): Python's TypeError where two do not order,
  // the operands in the set's order, which the hash seed decides (declared,
  // Minera sorts them in row order).
  if (const auto typed = detail::xes_frame_activity_column(table, std::string(activity_name), false);
      typed && typed->typed) {
    std::vector<python::Scalar> distinct;
    std::set<std::string> seen;
    for (std::size_t r = 0; r < typed->cells.size(); ++r) {
      const auto& cell = typed->cells[r];
      if (cell.nat) continue;
      const auto key = cell.missing && cell.value.kind != python::Scalar::Kind::kNone ? "nan#" + std::to_string(r)
                                                                                     : python::identity(cell.value);
      if (seen.insert(key).second) distinct.push_back(cell.value);
    }
    try {
      python::sort(distinct, [](const python::Scalar& a, const python::Scalar& b) { return python::less(a, b); });
    } catch (const python::TypeError& error) {
      throw std::invalid_argument("TypeError: " + detail::xes_frame_activity_type_error(*typed, error.what()));
    }
    return;
  }
  for (const auto& row : table.rows)
    if (!row[*activity_at] || row[*activity_at]->is_null)
      throw std::invalid_argument("TypeError: '<' not supported between instances of 'str' and 'float'");
}

// A row without an activity as an event no rule names (conformance).
void fill_missing_activities(EventTable& table, std::string_view activity_key) {
  const auto activity_name = activity_key.empty() ? std::string_view("concept:name") : activity_key;
  const auto found = std::find(table.columns.begin(), table.columns.end(), activity_name);
  if (found == table.columns.end()) return;
  const auto at = static_cast<std::size_t>(found - table.columns.begin());
  for (auto& row : table.rows) {
    if (at < row.size() && row[at] && !row[at]->is_null) continue;
    if (row.size() <= at) row.resize(at + 1);
    EventAttribute cell;
    cell.key = std::string(activity_name);
    cell.value = std::string(kPandasMissingActivity);
    row[at] = std::move(cell);
  }
}

EventDataView frame_slice_view(const EventDataView& data, std::string_view case_id_key,
                               std::string_view activity_key,
                               std::optional<EventLog>& log, std::optional<EventTable>& table,
                               bool discovery) {
  switch (data.kind()) {
    case EventDataKind::kEventLog:
      if (!data.log()->dataframe_columns) return data;
      log = frame_slice_cases(*data.log());
      return EventDataView(*log);
    case EventDataKind::kEventTable: {
      check_pandas_declare_frame(*data.table(), case_id_key, activity_key, discovery);
      table = frame_slice_table(*data.table(), case_id_key);
      if (!discovery) fill_missing_activities(*table, activity_key);
      return EventDataView(*table);
    }
    case EventDataKind::kLazyEventTable:
      table = polars_projection_table(data.lazy_table()->materialize(), case_id_key, activity_key, true);
      return EventDataView(*table);
    case EventDataKind::kEventStream:
      return data;
  }
  return data;
}

TypedDeclareResult typed_declare_result(const TypedDeclareTraces& traces, const DeclareDiscoveryOptions& options);

}  // namespace

std::optional<TypedDeclareResult> discover_declare_typed_frame(const EventTable& frame,
                                                               const DeclareDiscoveryOptions& options) {
  std::optional<EventTable> storage;
  const auto& present = present_columns(frame, storage);
  const auto activity_name =
      options.activity_key.empty() ? std::string_view("concept:name") : std::string_view(options.activity_key);
  // A column of text without gaps: the texts answer, and the caller's own
  // path runs the frame's checks.
  auto traces = typed_declare_traces(present, options.case_id_key, activity_name);
  if (!traces) return std::nullopt;
  check_pandas_declare_frame(frame, options.case_id_key, options.activity_key, false);
  return typed_declare_result(*traces, options);
}

namespace {

// The typed model, its conformance and its playout's error.
TypedDeclareResult typed_declare_result(const TypedDeclareTraces& traces, const DeclareDiscoveryOptions& options) {
  TypedDeclareRules rules;
  TypedDeclareResult out;
  out.model = typed_declare_model(traces, options, &rules);
  out.conformance.case_ids = traces.case_ids;
  try {
    out.conformance.results = typed_declare_conformance(traces, rules);
  } catch (const OperationCancelled&) {
    throw;
  } catch (const std::exception& error) {
    out.conformance.error = error.what();
  }
  // pm4py's dict order: the base binary families first (in an order the
  // rule columns' hash and address order decide; declared, it only decides
  // the type a model of mixed key types names), then the fixed rest; each
  // family's keys in the rules frame's column order, the levels sorted with
  // NaN and NaT last.
  static constexpr const char* kPlayoutOrder[] = {
      kDeclareRespondedExistence, kDeclareResponse, kDeclarePrecedence, kDeclareAltResponse,
      kDeclareAltPrecedence, kDeclareChainResponse, kDeclareChainPrecedence, kDeclareExistence,
      kDeclareExactlyOne, kDeclareInit, kDeclareAbsence, kDeclareSuccession, kDeclareAltSuccession,
      kDeclareChainSuccession, kDeclareCoexistence, kDeclareNonCoexistence, kDeclareNonSuccession,
      kDeclareNonChainSuccession};
  const auto missing = [](const TypedLabel& label) { return label.kind != TypedLabel::Kind::kValue; };
  for (const char* family : kPlayoutOrder) {
    const auto found = rules.find(family);
    if (found == rules.end()) continue;
    const std::string name = family;
    const bool unary = name == kDeclareExistence || name == kDeclareExactlyOne || name == kDeclareInit ||
                       name == kDeclareAbsence;
    const TypedDeclareKey* first = nullptr;
    for (const auto& key : found->second) {
      if (!key.collapsed) continue;
      const bool text = !missing(key.first) && key.first.value.kind == python::Scalar::Kind::kText;
      if (text && unary) continue;
      if (!first) { first = &key; continue; }
      if (missing(key.first)) continue;
      if (missing(first->first)) { first = &key; continue; }
      try {
        if (typed_less(key.first, first->first)) first = &key;
      } catch (const python::TypeError&) {
      }
    }
    if (!first) continue;
    if (!missing(first->first) && first->first.value.kind == python::Scalar::Kind::kText) {
      out.playout_error = "IndexError: tuple index out of range";
    } else {
      const auto type = first->first.kind == TypedLabel::Kind::kNaT ? std::string("NaTType")
                        : first->first.type.empty()                 ? std::string("float")
                                                                    : first->first.type;
      out.playout_error = "TypeError: '" + type + "' object is not subscriptable";
    }
    break;
  }
  return out;
}

}  // namespace

namespace {

// event[key] of pm4py's legacy event dict, as the iterparse importer stores
// an XES attribute (round HE, observed black-box; the classic heuristics'
// reading of rounds GK and GL): a string or id a str, an int an int of any
// width, a float float() of its text (each NaN its own object), a boolean
// True where its text is "true" in any case, a date a datetime.datetime
// whose wall clock is kept and marked UTC (fix_naivety drops the offset; the
// microseconds of a finer fraction), a valueless string or a <list> without
// children None, a <list> with children (or a scalar with children) a dict;
// a <container> is not stored. kUnmodelled where Minera cannot tell.
struct LegacyDeclareCell {
  enum class State { kValue, kAbsent, kDict, kUnmodelled };
  State state = State::kValue;
  python::Scalar value;
  std::string type;
};

LegacyDeclareCell legacy_declare_attribute(const EventAttribute& attribute) {
  using State = LegacyDeclareCell::State;
  LegacyDeclareCell out;
  const auto none = [&] {
    out.value = python::none();
    out.type = "NoneType";
    return out;
  };
  if (attribute.container != AttributeContainerKind::kScalar || attribute.scalar_wrapper ||
      !attribute.children.empty()) {
    if (attribute.xes_public_shape == XesPublicShape::kNone) return none();
    if (attribute.xes_public_shape == XesPublicShape::kAbsent) out.state = State::kAbsent;
    else if (attribute.xes_public_shape == XesPublicShape::kWrapper || attribute.scalar_wrapper) out.state = State::kDict;
    else out.state = State::kUnmodelled;
    return out;
  }
  if (attribute.is_null || is_native_na_attribute(attribute)) return none();
  switch (attribute.kind) {
    case AttributeKind::kString:
    case AttributeKind::kId:
      out.value = python::text(attribute.value);
      out.type = "str";
      return out;
    case AttributeKind::kInt:
      if (const auto* i = std::get_if<std::int64_t>(&attribute.typed_value)) out.value = python::integer(*i);
      else if (const auto* u = std::get_if<std::uint64_t>(&attribute.typed_value)) out.value = python::unsigned_integer(*u);
      else if (const auto parsed = python::integer_literal(attribute.value)) out.value = *parsed;
      else {
        out.state = State::kUnmodelled;
        return out;
      }
      out.type = "int";
      return out;
    case AttributeKind::kFloat:
      if (const auto* d = std::get_if<double>(&attribute.typed_value)) out.value = python::real(*d);
      else if (const auto parsed = detail::parse_python_float_text(attribute.value)) out.value = python::real(*parsed);
      else {
        out.state = State::kUnmodelled;
        return out;
      }
      out.type = "float";
      return out;
    case AttributeKind::kBoolean: {
      bool truth = false;
      if (const auto* b = std::get_if<bool>(&attribute.typed_value)) truth = *b;
      else {
        std::string lower = attribute.value;
        for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        truth = lower == "true";
      }
      out.value = python::boolean(truth);
      out.type = "bool";
      return out;
    }
    case AttributeKind::kDate: {
      auto parsed = parse_timestamp_exact(attribute.value);
      if (!parsed) {
        out.state = State::kUnmodelled;
        return out;
      }
      if (parsed->timezone_aware) parsed->seconds += static_cast<std::int64_t>(parsed->offset_minutes) * 60;
      parsed->offset_minutes = 0;
      parsed->timezone_aware = true;
      parsed->nanoseconds -= parsed->nanoseconds % 1000;
      out.value.kind = python::Scalar::Kind::kDate;
      out.value.date = *parsed;
      out.value.text = attribute.value;
      out.type = "datetime.datetime";
      return out;
    }
    default: out.state = State::kUnmodelled; return out;
  }
}

LegacyDeclareCell legacy_declare_cell(const Event& event, const std::string& key) {
  using State = LegacyDeclareCell::State;
  const auto key_of = [](const std::string& source, std::string_view standard) {
    return source.empty() ? std::string(standard) : source;
  };
  const auto attribute_value = [&] {
    for (const auto& attribute : event.attributes)
      if (attribute.key == key) return legacy_declare_attribute(attribute);
    LegacyDeclareCell out;
    out.state = State::kAbsent;
    return out;
  };
  // A first-class key the event holds as no scalar stays among its attributes.
  const auto field = [&](bool present, FirstClassAttributeState state, const std::optional<EventAttribute>& attribute,
                         const std::string& value, bool date) {
    if (!present) return attribute_value();
    if (attribute) return legacy_declare_attribute(*attribute);
    LegacyDeclareCell out;
    if (state == FirstClassAttributeState::kNull) {
      out.value = python::none();
      out.type = "NoneType";
      return out;
    }
    EventAttribute cell{key, value, date ? AttributeKind::kDate : AttributeKind::kString};
    return legacy_declare_attribute(cell);
  };
  if (key == key_of(event.timestamp_source_key, "time:timestamp"))
    return field(event.has_timestamp(), event.timestamp_state, event.timestamp_attribute, event.timestamp, true);
  if (key == key_of(event.activity_source_key, "concept:name"))
    return field(event.has_activity(), event.activity_state, event.activity_attribute, event.activity, false);
  if (key == key_of(event.resource_source_key, "org:resource"))
    return field(event.has_resource(), event.resource_state, event.resource_attribute, event.resource, false);
  if (key == key_of(event.lifecycle_source_key, "lifecycle:transition"))
    return field(event.has_lifecycle(), event.lifecycle_state, event.lifecycle_attribute, event.lifecycle, false);
  return attribute_value();
}

}  // namespace

std::optional<TypedDeclareResult> discover_declare_typed_log(const EventLog& log,
                                                             const DeclareDiscoveryOptions& options) {
  using State = LegacyDeclareCell::State;
  if (options.considered_activities) return std::nullopt;
  const std::string key = options.activity_key.empty() ? std::string("concept:name") : options.activity_key;
  std::vector<std::vector<LegacyDeclareCell>> cells(log.traces.size());
  bool absent = false, dict = false, typed = false;
  for (std::size_t t = 0; t < log.traces.size(); ++t) {
    cells[t].reserve(log.traces[t].events.size());
    for (const auto& event : log.traces[t].events) {
      auto cell = legacy_declare_cell(event, key);
      if (cell.state == State::kUnmodelled) return std::nullopt;
      absent = absent || cell.state == State::kAbsent;
      dict = dict || cell.state == State::kDict;
      typed = typed || (cell.state == State::kValue && cell.value.kind != python::Scalar::Kind::kText);
      cells[t].push_back(std::move(cell));
    }
  }
  // project_on_event_attribute reads every event first, then the activity set hashes them.
  if (absent) throw std::invalid_argument("KeyError: " + detail::frame_python_str_repr(key));
  if (dict) throw std::invalid_argument("TypeError: unhashable type: 'dict'");
  if (!typed) return std::nullopt;
  TypedDeclareTraces traces;
  traces.dtype = detail::XesFrameDtype::kObject;
  std::unordered_map<std::string, std::size_t> class_of;
  std::size_t fresh = 0;
  for (std::size_t t = 0; t < log.traces.size(); ++t) {
    std::vector<std::size_t> trace;
    trace.reserve(cells[t].size());
    for (auto& cell : cells[t]) {
      const bool nan = cell.value.kind == python::Scalar::Kind::kFloat && std::isnan(cell.value.real);
      const bool none = cell.value.kind == python::Scalar::Kind::kNone;
      // float() makes a new object per event: each NaN its own activity.
      const std::string id = nan ? "nan#" + std::to_string(fresh++) : python::identity(cell.value);
      const auto [at, added] = class_of.emplace(id, traces.classes.size());
      if (added) {
        TypedDeclareClass c;
        c.rep = std::move(cell.value);
        c.type = std::move(cell.type);
        c.na = nan || none;
        c.unequal = nan;
        c.none = none;
        traces.classes.push_back(std::move(c));
      }
      trace.push_back(at->second);
    }
    traces.traces.push_back(std::move(trace));
    traces.case_ids.push_back(log.traces[t].case_id);
  }
  return typed_declare_result(traces, options);
}

DeclareModel discover_declare_frame_slices(const EventDataView& data,
                                           const DeclareDiscoveryOptions& options,
                                           std::optional<DeclareModel>* shown,
                                           std::optional<DeclareShownConformance>* shown_conformance) {
  std::optional<EventLog> log;
  std::optional<EventTable> table;
  // Round HA (observed black-box): the Polars copy of an object column holds
  // str() of each pandas object (True, a date's str() with its offset, a
  // float's repr), which keys pm4py's Polars Declare; the session keeps the
  // texts.
  if (shown && data.kind() == EventDataKind::kLazyEventTable && !options.considered_activities) {
    const std::string activity_name = options.activity_key.empty() ? "concept:name" : options.activity_key;
    auto materialized = data.lazy_table()->materialize();
    const auto found = std::find(materialized.columns.begin(), materialized.columns.end(), activity_name);
    const auto column = detail::xes_frame_activity_column(materialized, activity_name, false);
    if (found != materialized.columns.end() && column && column->dtype == detail::XesFrameDtype::kObject) {
      const auto at = static_cast<std::size_t>(found - materialized.columns.begin());
      bool relabeled = false;
      for (std::size_t r = 0; r < materialized.rows.size() && r < column->cells.size(); ++r) {
        auto& row = materialized.rows[r];
        const auto& cell = column->cells[r];
        if (cell.missing || at >= row.size() || !row[at] || row[at]->is_null) continue;
        const auto label = detail::xes_frame_activity_label(cell);
        if (label == row[at]->value) continue;
        row[at]->value = label;
        relabeled = true;
      }
      if (relabeled) {
        auto session = discover_declare(frame_slice_view(data, options.case_id_key, options.activity_key, log, table, true),
                                        options);
        const auto projection =
            polars_projection_table(std::move(materialized), options.case_id_key, options.activity_key, true);
        *shown = discover_declare(EventDataView(projection), options);
        // Round HB: its conformance names the deviations by the same str()
        // labels.
        if (shown_conformance) {
          shown_conformance->emplace();
          DeclareConformanceOptions conformance;
          conformance.activity_key = options.activity_key;
          conformance.case_id_key = options.case_id_key;
          try {
            (*shown_conformance)->results = check_declare_conformance(EventDataView(projection), **shown, conformance);
          } catch (const OperationCancelled&) {
            throw;
          } catch (const std::exception& error) {
            (*shown_conformance)->error = error.what();
          }
        }
        return session;
      }
    }
  }
  if (shown && data.kind() == EventDataKind::kEventTable && !options.considered_activities) {
    // Round HA: the typed model (its frame checks first, then its errors);
    // round HB: its conformance.
    if (auto typed = discover_declare_typed_frame(*data.table(), options)) {
      *shown = std::move(typed->model);
      if (shown_conformance) *shown_conformance = std::move(typed->conformance);
      try {
        return discover_declare(frame_slice_view(data, options.case_id_key, options.activity_key, log, table, true),
                                options);
      } catch (const OperationCancelled&) {
        throw;
      } catch (const std::exception&) {
        // The texts hold no model (a gap): the session's model is empty, as
        // the shown one names no activity of the session's graph.
        return DeclareModel{};
      }
    }
  }
  return discover_declare(frame_slice_view(data, options.case_id_key, options.activity_key, log, table, true), options);
}

std::vector<DeclareConformance> check_declare_conformance_frame_slices(
    const EventDataView& data, const DeclareModel& model,
    const DeclareConformanceOptions& options) {
  if (data.kind() == EventDataKind::kEventLog)
    return check_declare_conformance_as_uploaded(*data.log(), model, options);
  std::optional<EventLog> log;
  std::optional<EventTable> table;
  return check_declare_conformance(frame_slice_view(data, options.case_id_key, options.activity_key, log, table, false),
                                   model, options);
}

// Round GO: conformance on an XES upload's Polars frame runs pm4py's frame
// check on the columns as read_xes types them (a dates column whose other
// cells are valueless lists stays a date column), the same check discovery
// runs (observed black-box).
std::vector<DeclareConformance> check_declare_conformance_xes_polars(
    const LazyEventTable& table, const DeclareModel& model, const DeclareConformanceOptions& options) {
  return check_declare_conformance(event_data_log(EventDataView(table), options.activity_key, options.case_id_key, true),
                                   model, options);
}

void check_declare_xes_polars_frame(const LazyEventTable& table, const DeclareConformanceOptions& options) {
  validate_activity_key(options.activity_key);
  validate_case_selector_key(options.case_id_key);
  (void)polars_projection_table(
      promoted_selector_table(table, options.activity_key, options.case_id_key).materialize(),
      options.case_id_key, options.activity_key, true);
}

EventTable declare_pandas_conformance_table(const EventTable& table, const DeclareConformanceOptions& options) {
  check_pandas_declare_frame(table, options.case_id_key, options.activity_key, false);
  auto filled = table;
  fill_missing_activities(filled, options.activity_key);
  return filled;
}

std::vector<DeclareConformance> check_declare_conformance_as_uploaded(
    const EventLog& log, const DeclareModel& model,
    const DeclareConformanceOptions& options) {
  if (!log.dataframe_columns) return check_declare_conformance(log, model, options);
  auto results = check_declare_conformance(frame_slice_cases(log), model, options);
  if (results.size() == log.traces.size()) return results;
  // A trace without rows has no slice; it keeps its own (empty) reading so
  // that the results stay one per trace.
  std::vector<DeclareConformance> all;
  all.reserve(log.traces.size());
  std::size_t next = 0;
  for (const auto& trace : log.traces)
    all.push_back(trace.events.empty() ? check_declare_conformance(trace, model, options)
                                       : std::move(results.at(next++)));
  return all;
}

}  // namespace minera
