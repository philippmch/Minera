#include "minera/declare.hpp"

#include "minera/log_conversion.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
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

bool in(const std::unordered_map<std::string, std::vector<int>>& idx,
        const std::string& a) {
  const auto it = idx.find(a);
  return it != idx.end() && !it->second.empty();
}

bool alt_response(const std::vector<std::string>& trace,
                  const std::unordered_map<std::string, std::vector<int>>& idx,
                  const std::string& a, const std::string& b) {
  const auto ia = idx.find(a);
  if (ia == idx.end()) return true;
  const auto ib = idx.find(b);
  if (ib == idx.end()) return false;
  std::size_t p = 0;
  for (std::size_t i = 0; i < ia->second.size(); ++i) {
    const int next = i + 1 < ia->second.size() ? ia->second[i + 1]
                                                : static_cast<int>(trace.size());
    while (p < ib->second.size() && ib->second[p] <= ia->second[i]) ++p;
    if (p == ib->second.size() || ib->second[p] >= next) return false;
  }
  return true;
}

bool alt_precedence(const std::unordered_map<std::string, std::vector<int>>& idx,
                    const std::string& a, const std::string& b) {
  const auto ib = idx.find(b);
  if (ib == idx.end()) return true;
  const auto ia = idx.find(a);
  if (ia == idx.end()) return false;
  std::size_t p = 0;
  int previous = -1;
  for (const int bi : ib->second) {
    while (p < ia->second.size() && ia->second[p] <= previous) ++p;
    if (p == ia->second.size() || ia->second[p] >= bi) return false;
    previous = bi;
  }
  return true;
}

bool chain_response(const std::vector<std::string>& trace,
                    const std::unordered_map<std::string, std::vector<int>>& idx,
                    const std::string& a, const std::string& b) {
  const auto ia = idx.find(a);
  if (ia == idx.end()) return true;
  for (const int i : ia->second) {
    if (i + 1 >= static_cast<int>(trace.size()) || trace[i + 1] != b) return false;
  }
  return true;
}

bool chain_precedence(const std::vector<std::string>& trace,
                      const std::unordered_map<std::string, std::vector<int>>& idx,
                      const std::string& a, const std::string& b) {
  const auto ib = idx.find(b);
  if (ib == idx.end()) return true;
  for (const int i : ib->second) {
    if (i == 0 || trace[i - 1] != a) return false;
  }
  return true;
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

EventLog event_data_log(const EventDataView& data,
                        std::string_view activity_key,
                        std::string_view case_id_key) {
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
      auto table = promoted_selector_table(*data.lazy_table(),
                                            std::string(activity_key),
                                            std::string(case_id_key));
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

DeclareConformance check_declare_conformance(const std::vector<std::string>& trace,
                                             const DeclareModel& model) {
  DeclareConformance result;
  for (const auto& [_, values] : model.rules) result.no_constr_total += static_cast<int>(values.size());
  std::unordered_map<std::string, std::vector<int>> idx;
  for (int i = 0; i < static_cast<int>(trace.size()); ++i) idx[trace[i]].push_back(i);
  auto add_dev = [&](const std::string& family, const DeclareConstraint& c) {
    result.deviations.push_back({family, c.first, c.second, c.binary});
  };
  auto check = [&](const std::string& family, auto&& predicate) {
    const auto it = model.rules.find(family); if (it == model.rules.end()) return;
    for (const auto& c : it->second) if (!predicate(c)) add_dev(family, c);
  };
  check(kDeclareExistence, [&](const auto& c){ return in(idx,c.first); });
  check(kDeclareExactlyOne, [&](const auto& c){
    const auto it = idx.find(c.first);
    return it != idx.end() && it->second.size() == 1;
  });
  check(kDeclareInit, [&](const auto& c){ return !trace.empty() && trace.front()==c.first; });
  check(kDeclareRespondedExistence, [&](const auto& c){ return !in(idx,c.first)||in(idx,c.second); });
  check(kDeclareCoexistence, [&](const auto& c){ return in(idx,c.first)==in(idx,c.second); });
  check(kDeclareNonCoexistence, [&](const auto& c){ return !(in(idx,c.first)&&in(idx,c.second)); });
  check(kDeclareResponse, [&](const auto& c){ return !in(idx,c.first)||(in(idx,c.second)&&idx.at(c.first).back()<idx.at(c.second).back()); });
  check(kDeclarePrecedence, [&](const auto& c){ return !in(idx,c.second)||(in(idx,c.first)&&idx.at(c.first).front()<idx.at(c.second).front()); });
  check(kDeclareSuccession, [&](const auto& c){ return (!in(idx,c.first)&&!in(idx,c.second)) || (in(idx,c.first)&&in(idx,c.second)&&idx.at(c.first).front()<idx.at(c.second).front()&&idx.at(c.first).back()<idx.at(c.second).back()); });
  check(kDeclareAltResponse, [&](const auto& c){ return alt_response(trace,idx,c.first,c.second); });
  check(kDeclareChainResponse, [&](const auto& c){ return chain_response(trace,idx,c.first,c.second); });
  check(kDeclareAltPrecedence, [&](const auto& c){ return alt_precedence(idx,c.first,c.second); });
  check(kDeclareChainPrecedence, [&](const auto& c){ return chain_precedence(trace,idx,c.first,c.second); });
  check(kDeclareAltSuccession, [&](const auto& c){ return alt_response(trace,idx,c.first,c.second)&&alt_precedence(idx,c.first,c.second); });
  check(kDeclareChainSuccession, [&](const auto& c){ return chain_response(trace,idx,c.first,c.second)&&chain_precedence(trace,idx,c.first,c.second); });
  check(kDeclareAbsence, [&](const auto& c){ return !in(idx,c.first); });
  check(kDeclareNonSuccession, [&](const auto& c){ return !in(idx,c.first)&&!in(idx,c.second) || !(in(idx,c.first)&&in(idx,c.second)&&idx.at(c.first).front()<idx.at(c.second).front()&&idx.at(c.first).back()<idx.at(c.second).back()); });
  check(kDeclareNonChainSuccession, [&](const auto& c){ return !in(idx,c.first)||!in(idx,c.second)||!(chain_response(trace,idx,c.first,c.second)&&chain_precedence(trace,idx,c.first,c.second)); });
  result.no_dev_total = static_cast<int>(result.deviations.size());
  result.dev_fitness = result.no_constr_total > 0 ? 1.0 - static_cast<double>(result.no_dev_total) / result.no_constr_total : 1.0;
  result.is_fit = result.no_dev_total == 0;
  return result;
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
  std::map<std::vector<std::string>, std::size_t> checked;
  for (const auto& trace : log.traces) {
    std::vector<std::string> activities;
    activities.reserve(trace.events.size());
    for (const auto& event : trace.events)
      activities.push_back(selected_activity(event, options.activity_key));
    const auto found = checked.find(activities);
    if (found != checked.end()) {
      result.push_back(result[found->second]);
      continue;
    }
    result.push_back(check_declare_conformance(activities, model));
    checked.emplace(std::move(activities), result.size() - 1);
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

}  // namespace minera
