#include "minera/decision_table.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "minera/conformance.hpp"
#include "minera/decision_mining.hpp"
#include "decision_replay.hpp"

namespace minera {
namespace {

struct RawRow {
  std::map<std::string, std::string> features;
  std::size_t target = 0;
};

std::optional<double> parse_number(std::string_view text) {
  if (text.empty()) return std::nullopt;
  std::string copy(text);
  char* end = nullptr;
  errno = 0;
  const double value = std::strtod(copy.c_str(), &end);
  if (end != copy.c_str() + copy.size() || !std::isfinite(value)) {
    return std::nullopt;
  }
  return value;
}

void write_json_string(std::ostream& out, std::string_view value) {
  out << '"';
  for (const unsigned char c : value) {
    switch (c) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (c < 0x20) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
              << static_cast<unsigned int>(c) << std::dec << std::setfill(' ');
        } else {
          out << static_cast<char>(c);
        }
    }
  }
  out << '"';
}

struct RawTables {
  std::uint64_t observed_traces = 0;
  std::uint64_t replayed_traces = 0;
  std::uint64_t aligned_traces = 0;
  std::uint64_t alignment_failures = 0;
  std::vector<std::size_t> decision_places;
  std::vector<std::vector<RawRow>> rows;
};

RawTables collect_rows(const EventLog& log, const PetriNet& net,
                       const DecisionTableOptions& options) {
  if (options.lookback == 0)
    throw std::invalid_argument("decision table lookback must be positive");
  if (options.lookback > kMaxDecisionLookback)
    throw std::invalid_argument("decision table lookback exceeds safety limit");

  std::vector<std::vector<std::size_t>> outgoing(net.place_count());
  for (std::size_t transition = 0; transition < net.transitions().size();
       ++transition) {
    for (const auto place : net.transitions()[transition].inputs) {
      if (place >= outgoing.size())
        throw std::invalid_argument("decision table transition place is invalid");
      outgoing[place].push_back(transition);
    }
  }
  std::vector<int> decision_index(net.place_count(), -1);
  std::vector<std::size_t> decision_places;
  for (std::size_t place = 0; place < outgoing.size(); ++place) {
    if (outgoing[place].size() >= 2) {
      decision_index[place] = static_cast<int>(decision_places.size());
      decision_places.push_back(place);
    }
  }

  const auto paths = detail::collect_decision_replay_paths(
      log, net, options.mining.activity_attribute);
  std::vector<std::vector<RawRow>> rows(decision_places.size());
  std::uint64_t observed = 0;
  for (std::size_t trace_index = 0; trace_index < log.traces.size();
       ++trace_index) {
    const auto& trace = log.traces[trace_index];
    const auto& path = paths.paths[paths.variant_of[trace_index]];
    if (!path.available) continue;
    ++observed;

    std::map<std::string, std::string> trace_context;
    if (options.include_trace_attributes) {
      for (const auto& attribute : trace.attributes)
        if (!attribute.is_null)
          trace_context[attribute.key] = attribute.value;
    }
    std::vector<std::optional<std::map<std::string, std::string>>> context_slots(
        options.lookback);
    std::size_t event_index = 0;
    const auto remember_event = [&](const Event& event) {
      std::map<std::string, std::string> snapshot;
      for (const auto& [key, value] : trace_context) {
        if (!options.attributes.empty() &&
            std::find(options.attributes.begin(), options.attributes.end(), key) ==
                options.attributes.end())
          continue;
        snapshot.emplace(key, value);
      }
      if (!event.resource.empty() &&
          (options.attributes.empty() ||
           std::find(options.attributes.begin(), options.attributes.end(),
                     "resource") != options.attributes.end()))
        snapshot["resource"] = event.resource;
      for (const auto& attribute : event.attributes) {
        if (attribute.is_null) continue;
        if (options.attributes.empty() ||
            std::find(options.attributes.begin(), options.attributes.end(),
                      attribute.key) != options.attributes.end())
          snapshot[attribute.key] = attribute.value;
      }
      const auto slot = event_index % options.lookback;
      context_slots[slot] = std::move(snapshot);
      ++event_index;
    };
    for (const auto& move : path.moves) {
      if (move.transition != static_cast<std::size_t>(-1) &&
          move.transition < net.transitions().size()) {
        const std::size_t transition = move.transition;
        for (const std::size_t place : net.transitions()[transition].inputs) {
          const int decision = decision_index[place];
          if (decision < 0) continue;
          const auto& choices = outgoing[place];
          const auto choice = std::find(choices.begin(), choices.end(), transition);
          if (choice == choices.end())
            throw std::logic_error("decision table transition is not outgoing");
          const auto target = static_cast<std::size_t>(
              std::distance(choices.begin(), choice));
          for (const auto& snapshot : context_slots) {
            if (!snapshot) continue;
            RawRow row;
            row.features = *snapshot;
            row.target = target;
            rows[static_cast<std::size_t>(decision)].push_back(std::move(row));
          }
        }
      }
      if (move.type == AlignmentMoveType::kSynchronous ||
          move.type == AlignmentMoveType::kLog) {
        if (event_index < trace.events.size())
          remember_event(trace.events[event_index]);
      }
    }
  }
  return RawTables{observed,
                   paths.replayed_traces,
                   paths.aligned_traces,
                   paths.alignment_failures,
                   std::move(decision_places),
                   std::move(rows)};
}

struct Column {
  std::string name;
  std::string key;
  std::optional<std::string> category;
};

EncodedDecisionTable encode_table(std::size_t place,
                                   const std::vector<std::string>& classes,
                                   const std::vector<RawRow>& rows,
                                   const DecisionTableOptions& options) {
  EncodedDecisionTable result;
  result.place = place;
  result.classes = classes;
  if (rows.empty()) return result;

  std::set<std::string> selected;
  if (options.attributes.empty()) {
    for (const auto& row : rows)
      for (const auto& [key, value] : row.features) selected.insert(key);
  } else {
    selected.insert(options.attributes.begin(), options.attributes.end());
    selected.erase("");
  }

  std::vector<Column> columns;
  for (const auto& key : selected) {
    std::set<std::string> categories;
    bool all_numeric = true;
    bool present = false;
    for (const auto& row : rows) {
      const auto it = row.features.find(key);
      if (it == row.features.end()) continue;
      present = true;
      if (!parse_number(it->second)) all_numeric = false;
      categories.insert(it->second);
    }
    if (!present) continue;
    if (all_numeric) {
      columns.push_back(Column{key, key, std::nullopt});
    } else {
      for (const auto& category : categories)
        columns.push_back(Column{key + "=" + category, key, category});
    }
  }
  for (const auto& column : columns) result.feature_names.push_back(column.name);

  for (const auto& raw : rows) {
    DecisionTableRow row;
    row.target = raw.target;
    if (row.target >= result.classes.size())
      throw std::invalid_argument("decision table target branch is invalid");
    row.features.reserve(columns.size());
    for (const auto& column : columns) {
      const auto it = raw.features.find(column.key);
      if (!column.category) {
        row.features.push_back(it == raw.features.end()
                                  ? std::nullopt
                                  : parse_number(it->second));
      } else {
        row.features.push_back(
            it != raw.features.end() && it->second == *column.category ? 1.0 : 0.0);
      }
    }
    result.rows.push_back(std::move(row));
  }
  return result;
}

}  // namespace

DecisionTablesResult mine_decision_tables(const EventLog& log,
                                          const PetriNet& net,
                                          const DecisionTableOptions& options) {
  const auto raw = collect_rows(log, net, options);
  const auto decisions = mine_decisions(log, net, options.mining);
  DecisionTablesResult result;
  result.observed_traces = raw.observed_traces;
  result.replayed_traces = raw.replayed_traces;
  result.aligned_traces = raw.aligned_traces;
  result.alignment_failures = raw.alignment_failures;
  result.trace_count = log.trace_count();
  result.activity_attribute = options.mining.activity_attribute.empty()
                                 ? "concept:name"
                                 : options.mining.activity_attribute;
  std::map<std::size_t, const DecisionPoint*> points;
  for (const auto& point : decisions.decision_points) points[point.place] = &point;
  for (std::size_t i = 0; i < raw.decision_places.size(); ++i) {
    if (raw.rows[i].empty()) continue;
    const auto point_it = points.find(raw.decision_places[i]);
    if (point_it == points.end()) continue;
    result.tables.push_back(encode_table(raw.decision_places[i],
                                          point_it->second->branches,
                                          raw.rows[i], options));
  }
  std::sort(result.tables.begin(), result.tables.end(),
            [](const auto& left, const auto& right) {
              return left.place < right.place;
            });
  return result;
}

std::string serialize_decision_tables_json(const DecisionTablesResult& result) {
  if (result.observed_traces > result.trace_count)
    throw std::invalid_argument("decision table observed trace count is invalid");
  std::ostringstream out;
  out << "{\"family\":\"DECISION_TABLES\",\"variant\":\"encoded\","
      << "\"activity_attribute\":";
  write_json_string(out, result.activity_attribute.empty()
                            ? "concept:name"
                            : result.activity_attribute);
  out << ","
      << "\"trace_count\":" << result.trace_count
      << ",\"observed_traces\":" << result.observed_traces
      << ",\"replayed_traces\":" << result.replayed_traces
      << ",\"aligned_traces\":" << result.aligned_traces
      << ",\"alignment_failures\":" << result.alignment_failures
      << ",\"tables\":[";
  for (std::size_t ti = 0; ti < result.tables.size(); ++ti) {
    if (ti != 0) out << ',';
    const auto& table = result.tables[ti];
    out << "{\"place\":" << table.place << ",\"classes\":[";
    for (std::size_t i = 0; i < table.classes.size(); ++i) {
      if (i != 0) out << ',';
      write_json_string(out, table.classes[i]);
    }
    out << "],\"feature_names\":[";
    for (std::size_t i = 0; i < table.feature_names.size(); ++i) {
      if (i != 0) out << ',';
      write_json_string(out, table.feature_names[i]);
    }
    out << "],\"rows\":[";
    for (std::size_t ri = 0; ri < table.rows.size(); ++ri) {
      if (ri != 0) out << ',';
      const auto& row = table.rows[ri];
      if (row.features.size() != table.feature_names.size())
        throw std::invalid_argument("decision table row width differs");
      out << "{\"features\":[";
      for (std::size_t fi = 0; fi < row.features.size(); ++fi) {
        if (fi != 0) out << ',';
        if (!row.features[fi]) {
          out << "null";
        } else {
          out << std::setprecision(17) << *row.features[fi];
        }
      }
      if (row.target >= table.classes.size())
        throw std::invalid_argument("decision table target is out of range");
      out << "],\"target\":" << row.target << '}';
    }
    out << "]}";
  }
  out << "]}";
  return out.str();
}

namespace {

bool reference_skipped_key(const std::string& key) {
  return key == "concept:name" || key == "time:timestamp" ||
         key == "lifecycle:transition";
}

// The encodable scalar attributes of one event, resource included.
std::vector<const EventAttribute*> reference_event_attributes(
    const Event& event, EventAttribute& resource) {
  std::vector<const EventAttribute*> out;
  if (event.has_resource()) {
    resource.key = "org:resource";
    resource.value = event.resource;
    resource.kind = AttributeKind::kString;
    out.push_back(&resource);
  }
  for (const auto& attribute : event.attributes) {
    if (attribute.is_null || attribute.container != AttributeContainerKind::kScalar ||
        reference_skipped_key(attribute.key) || attribute.key == "org:resource")
      continue;
    out.push_back(&attribute);
  }
  return out;
}

}  // namespace

ReferenceDecisionTable reference_decision_table(
    const EventLog& log, const PetriNet& net, std::size_t place,
    const TokenReplayOptions& replay) {
  if (place >= net.place_count())
    throw std::out_of_range("decision place is out of range");
  std::vector<std::optional<std::string>> labels;
  for (const auto& transition : net.transitions()) {
    if (std::find(transition.inputs.begin(), transition.inputs.end(), place) ==
        transition.inputs.end())
      continue;
    labels.push_back(transition.activity
                         ? std::optional<std::string>(net.activities().at(*transition.activity))
                         : std::nullopt);
  }

  // Attribute keys present in every trace, with the kind each one keeps.
  enum class Kind { kString, kNumber, kOther };
  std::map<std::string, Kind> common;
  bool first = true;
  for (const auto& trace : log.traces) {
    std::map<std::string, Kind> present;
    for (const auto& event : trace.events) {
      EventAttribute resource;
      for (const auto* attribute : reference_event_attributes(event, resource)) {
        const Kind kind = attribute->kind == AttributeKind::kString ? Kind::kString
                          : attribute->kind == AttributeKind::kInt ||
                                  attribute->kind == AttributeKind::kFloat
                              ? Kind::kNumber
                              : Kind::kOther;
        const auto [it, inserted] = present.emplace(attribute->key, kind);
        if (!inserted && it->second != kind) it->second = Kind::kOther;
      }
    }
    if (first) {
      common = std::move(present);
      first = false;
      continue;
    }
    for (auto it = common.begin(); it != common.end();) {
      const auto match = present.find(it->first);
      if (match == present.end()) {
        it = common.erase(it);
        continue;
      }
      if (match->second != it->second) it->second = Kind::kOther;
      ++it;
    }
  }

  const auto result = replay_token_result(log, net, replay);
  std::vector<std::vector<std::size_t>> fired(log.traces.size());
  for (const auto& row : result.traces)
    if (row.trace_index < fired.size()) fired[row.trace_index] = row.activated_transitions;

  std::vector<std::vector<std::string>> variant_keys;
  std::vector<std::vector<std::size_t>> variant_traces;
  for (std::size_t index = 0; index < log.traces.size(); ++index) {
    std::vector<std::string> key;
    for (const auto& event : log.traces[index].events) key.push_back(event.activity);
    const auto found = std::find(variant_keys.begin(), variant_keys.end(), key);
    if (found == variant_keys.end()) {
      variant_keys.push_back(std::move(key));
      variant_traces.push_back({index});
    } else {
      variant_traces[static_cast<std::size_t>(found - variant_keys.begin())].push_back(index);
    }
  }

  struct Observation {
    std::optional<std::string> label;
    const Event* event;
  };
  std::vector<Observation> observations;
  for (const auto& traces : variant_traces) {
    for (const auto index : traces) {
      const auto& events = log.traces[index].events;
      std::optional<std::size_t> previous;
      std::size_t consumed = 0;
      for (const auto transition_index : fired[index]) {
        const auto& transition = net.transitions().at(transition_index);
        const std::optional<std::string> label =
            transition.activity
                ? std::optional<std::string>(net.activities().at(*transition.activity))
                : std::nullopt;
        if (previous && *previous < events.size() &&
            std::find(labels.begin(), labels.end(), label) != labels.end())
          observations.push_back({label, &events[*previous]});
        if (transition.activity) previous = consumed++;
      }
    }
  }
  if (observations.empty())
    throw std::invalid_argument("the decision point has no observations");

  // Per row, the value of each common attribute, as text.
  std::vector<std::map<std::string, const EventAttribute*>> values(observations.size());
  std::vector<EventAttribute> resources(observations.size());
  for (std::size_t row = 0; row < observations.size(); ++row)
    for (const auto* attribute :
         reference_event_attributes(*observations[row].event, resources[row]))
      if (common.count(attribute->key)) values[row].emplace(attribute->key, attribute);

  ReferenceDecisionTable table;
  std::vector<std::pair<std::string, std::string>> one_hot;
  for (const auto& [key, kind] : common) {
    if (kind != Kind::kString) continue;
    std::set<std::string> seen;
    for (const auto& row : values)
      if (const auto it = row.find(key); it != row.end()) seen.insert(it->second->value);
    for (const auto& value : seen) {
      one_hot.emplace_back(key, value);
      table.columns.push_back(key + "_" + value);
      table.column_types.push_back(ReferenceDecisionColumnType::kBool);
    }
  }
  if (one_hot.empty())
    throw std::invalid_argument("the decision point has no string attribute to encode");
  std::vector<std::string> numeric;
  for (const auto& [key, kind] : common) {
    if (kind != Kind::kNumber) continue;
    bool any = false, integral = true;
    for (const auto& row : values) {
      const auto it = row.find(key);
      if (it == row.end()) {
        integral = false;
        continue;
      }
      any = true;
      if (it->second->kind != AttributeKind::kInt) integral = false;
    }
    if (!any) continue;
    numeric.push_back(key);
    table.columns.push_back(key);
    table.column_types.push_back(integral ? ReferenceDecisionColumnType::kInt64
                                          : ReferenceDecisionColumnType::kFloat64);
  }

  for (std::size_t row = 0; row < observations.size(); ++row) {
    std::vector<std::optional<double>> cells;
    for (const auto& [key, value] : one_hot) {
      const auto it = values[row].find(key);
      cells.push_back(it != values[row].end() && it->second->value == value ? 1.0 : 0.0);
    }
    for (const auto& key : numeric) {
      const auto it = values[row].find(key);
      cells.push_back(it == values[row].end() ? std::nullopt
                                              : parse_number(it->second->value));
    }
    table.rows.push_back(std::move(cells));
    const auto& label = observations[row].label;
    auto found = std::find(table.classes.begin(), table.classes.end(), label);
    if (found == table.classes.end()) {
      table.classes.push_back(label);
      found = table.classes.end() - 1;
    }
    table.targets.push_back(static_cast<std::size_t>(found - table.classes.begin()));
  }
  return table;
}

}  // namespace minera
