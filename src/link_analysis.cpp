#include "minera/link_analysis.hpp"

#include "minera/timestamp.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace minera {
namespace {

struct Row {
  const Trace* trace = nullptr;
  const Event* event = nullptr;
  std::size_t trace_index = 0;
  std::size_t event_index = 0;
  std::size_t input_index = 0;
  std::size_t index = 0;
  std::string out_value;
  std::string in_value;
  std::string sorting_value;
};

std::string value_for(const Trace& trace, const Event& event,
                      std::size_t event_index, std::string_view column) {
  if (column == "case_id" || column == "case:concept:name" ||
      column == "case")
    return trace.case_id;
  if (column == "activity" || column == "concept:name") return event.activity;
  if (column == "timestamp" || column == "time:timestamp")
    return event.timestamp;
  if (column == "resource" || column == "org:resource") return event.resource;
  if (column == "lifecycle" || column == "lifecycle:transition")
    return event.lifecycle;
  if (column == "input_order") return std::to_string(event.input_order);
  if (column == "event_index") return std::to_string(event_index);

  if (const auto* value = event.attribute(column)) return *value;
  std::string_view trace_column = column;
  if (trace_column.starts_with("case:")) trace_column.remove_prefix(5);
  if (const auto* value = trace.attribute(trace_column)) return *value;
  if (const auto* value = trace.attribute(column)) return *value;
  return {};
}

bool is_builtin_column(std::string_view column) {
  return column == "case_id" || column == "case:concept:name" ||
         column == "case" || column == "activity" ||
         column == "concept:name" || column == "timestamp" ||
         column == "time:timestamp" || column == "resource" ||
         column == "org:resource" || column == "lifecycle" ||
         column == "lifecycle:transition" || column == "input_order" ||
         column == "event_index";
}

void validate_column(const EventLog& log, std::string_view column,
                     std::string_view option_name) {
  if (column.empty())
    throw std::invalid_argument(std::string(option_name) +
                                " must not be empty");
  if (is_builtin_column(column)) return;
  for (const auto& trace : log.traces) {
    if (trace.attribute(column) != nullptr) return;
    std::string_view stripped = column;
    if (stripped.starts_with("case:")) stripped.remove_prefix(5);
    if (trace.attribute(stripped) != nullptr) return;
    for (const auto& event : trace.events) {
      if (event.attribute(column) != nullptr) return;
    }
  }
  throw std::invalid_argument(std::string(option_name) +
                              " does not exist in the event log");
}

LinkAnalysisEvent event_ref(const Row& row, std::string value) {
  return {row.index, row.trace_index, row.event_index, row.trace->case_id,
          row.event->activity, row.event->timestamp, std::move(value)};
}

void write_json_string(std::ostream& out, std::string_view value) {
  out << '"';
  for (const char c : value) {
    switch (c) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          constexpr char hex[] = "0123456789abcdef";
          out << "\\u00" << hex[(static_cast<unsigned char>(c) >> 4) & 0xF]
              << hex[static_cast<unsigned char>(c) & 0xF];
        } else {
          out << c;
        }
    }
  }
  out << '"';
}

void write_event(std::ostream& out, const LinkAnalysisEvent& event) {
  out << "{\"index\":" << event.index << ",\"trace_index\":"
      << event.trace_index << ",\"event_index\":" << event.event_index
      << ",\"case_id\":";
  write_json_string(out, event.case_id);
  out << ",\"activity\":";
  write_json_string(out, event.activity);
  out << ",\"timestamp\":";
  write_json_string(out, event.timestamp);
  out << ",\"value\":";
  write_json_string(out, event.value);
  out << '}';
}

}  // namespace

LinkAnalysisResult discover_link_analysis(const EventLog& log,
                                          const LinkAnalysisOptions& options) {
  validate_column(log, options.out_column, "link_analysis_out_column");
  validate_column(log, options.in_column, "link_analysis_in_column");
  validate_column(log, options.sorting_column,
                  "link_analysis_sorting_column");
  if (options.max_links == 0)
    throw std::invalid_argument("link_analysis_max_links must be positive");

  std::vector<Row> rows;
  for (std::size_t trace_index = 0; trace_index < log.traces.size();
       ++trace_index) {
    const auto& trace = log.traces[trace_index];
    for (std::size_t event_index = 0; event_index < trace.events.size();
         ++event_index) {
      const auto& event = trace.events[event_index];
      Row row;
      row.trace = &trace;
      row.event = &event;
      row.trace_index = trace_index;
      row.event_index = event_index;
      row.input_index = rows.size();
      row.out_value = value_for(trace, event, event_index, options.out_column);
      row.in_value = value_for(trace, event, event_index, options.in_column);
      row.sorting_value =
          value_for(trace, event, event_index, options.sorting_column);
      rows.push_back(std::move(row));
    }
  }

  const auto timestamp_sort = options.sorting_column == "timestamp" ||
                              options.sorting_column == "time:timestamp";
  std::stable_sort(rows.begin(), rows.end(), [&](const Row& left,
                                                const Row& right) {
    if (timestamp_sort) {
      const auto left_time = parse_timestamp(left.sorting_value);
      const auto right_time = parse_timestamp(right.sorting_value);
      if (left_time && right_time && *left_time != *right_time)
        return *left_time < *right_time;
      if (left_time.has_value() != right_time.has_value())
        return left_time.has_value();
    }
    if (left.sorting_value != right.sorting_value)
      return left.sorting_value < right.sorting_value;
    return left.input_index < right.input_index;
  });
  for (std::size_t index = 0; index < rows.size(); ++index)
    rows[index].index = index;

  std::map<std::string, std::vector<std::size_t>> targets;
  for (const auto& row : rows) targets[row.in_value].push_back(row.index);

  std::map<std::size_t, std::set<std::size_t>> associations;
  std::size_t association_count = 0;
  for (const auto& source : rows) {
    const auto target_it = targets.find(source.out_value);
    if (target_it == targets.end()) continue;
    auto& linked = associations[source.index];
    for (const auto target_index : target_it->second) {
      if (options.look_forward && target_index <= source.index) continue;
      if (linked.insert(target_index).second) {
        ++association_count;
        if (association_count > options.max_links)
          throw std::invalid_argument("link analysis exceeded max_links");
      }
      if (options.keep_first_occurrence) break;
    }
  }

  if (options.propagate) {
    bool changed = true;
    while (changed) {
      changed = false;
      for (auto& [source, linked] : associations) {
        std::vector<std::size_t> additions;
        for (const auto target : linked) {
          const auto next = associations.find(target);
          if (next == associations.end()) continue;
          for (const auto candidate : next->second) {
            if (!linked.contains(candidate)) additions.push_back(candidate);
          }
        }
        for (const auto target : additions) {
          if (linked.insert(target).second) {
            changed = true;
            ++association_count;
            if (association_count > options.max_links)
              throw std::invalid_argument("link analysis exceeded max_links");
          }
        }
      }
    }
  }

  LinkAnalysisResult result;
  result.options = options;
  result.event_count = rows.size();
  for (const auto& [source_index, linked] : associations) {
    const auto& source = rows[source_index];
    for (const auto target_index : linked) {
      const auto& target = rows[target_index];
      if (result.links.size() >= options.max_links)
        throw std::invalid_argument("link analysis exceeded max_links");
      result.links.push_back({event_ref(source, source.out_value),
                              event_ref(target, target.in_value)});
    }
  }
  return result;
}

std::string link_analysis_json(const LinkAnalysisResult& result) {
  std::ostringstream out;
  out << "{\"family\":\"LINK_ANALYSIS\",\"variant\":\"CLASSIC\""
      << ",\"out_column\":";
  write_json_string(out, result.options.out_column);
  out << ",\"in_column\":";
  write_json_string(out, result.options.in_column);
  out << ",\"sorting_column\":";
  write_json_string(out, result.options.sorting_column);
  out << ",\"look_forward\":"
      << (result.options.look_forward ? "true" : "false")
      << ",\"keep_first_occurrence\":"
      << (result.options.keep_first_occurrence ? "true" : "false")
      << ",\"propagate\":" << (result.options.propagate ? "true" : "false")
      << ",\"event_count\":" << result.event_count << ",\"links\":[";
  for (std::size_t i = 0; i < result.links.size(); ++i) {
    if (i != 0) out << ',';
    out << "{\"source\":";
    write_event(out, result.links[i].source);
    out << ",\"target\":";
    write_event(out, result.links[i].target);
    out << '}';
  }
  out << "]}";
  return out.str();
}

}  // namespace minera
