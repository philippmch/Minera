#include "minera/dfg_precision.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace minera {
namespace {

using Prefix = std::vector<std::string>;
using ActivitySet = std::set<std::string>;

const std::string& event_activity(const Event& event,
                                  std::string_view attribute) {
  if (attribute == "activity" || attribute == "concept:name" ||
      attribute == "Activity")
    return event.activity;
  const auto* value = event.attribute(attribute);
  if (!value)
    throw std::invalid_argument("DFG precision: missing activity attribute " +
                                std::string(attribute));
  return *value;
}

bool allowed_prefix(const Prefix& prefix,
                    const ActivitySet& starts,
                    const std::map<std::string, ActivitySet>& exiting) {
  if (prefix.empty()) return true;
  if (!starts.contains(prefix.front())) return false;
  for (std::size_t i = 1; i < prefix.size(); ++i) {
    const auto previous = exiting.find(prefix[i - 1]);
    if (previous == exiting.end() || !previous->second.contains(prefix[i]))
      return false;
  }
  return exiting.contains(prefix.back());
}

std::string json_quote(std::string_view value) {
  std::ostringstream out;
  out << '"';
  for (const unsigned char character : value) {
    switch (character) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (character < 0x20U) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
              << static_cast<unsigned int>(character) << std::dec
              << std::setfill(' ');
        } else {
          out << static_cast<char>(character);
        }
    }
  }
  out << '"';
  return out.str();
}

}  // namespace

DfgPrecisionResult evaluate_dfg_precision(
    const EventLog& log, const DfgCleanAnalysis& model,
    const DfgPrecisionOptions& options) {
  if (options.activity_attribute.empty())
    throw std::invalid_argument("DFG precision: activity attribute must not be empty");

  std::map<std::string, ActivitySet> exiting;
  for (const auto& [edge, count] : model.graph) {
    if (edge.first.empty() || edge.second.empty())
      throw std::invalid_argument("DFG precision: model labels must not be empty");
    // The graph maps describe the model relation; their frequencies are not
    // used by escaping-edge precision. Keep zero-frequency entries visible so
    // imported sparse graphs retain their declared edges.
    exiting[edge.first].insert(edge.second);
  }
  ActivitySet starts;
  for (const auto& [activity, count] : model.start_activities) {
    if (activity.empty())
      throw std::invalid_argument("DFG precision: model labels must not be empty");
    // As with graph edges, a declared start key is a model start relation even
    // when its optional frequency is zero.
    starts.insert(activity);
  }

  struct PrefixObservation {
    ActivitySet next;
    std::uint64_t count = 0;
  };
  std::map<Prefix, PrefixObservation> observations;
  for (const auto& trace : log.traces) {
    Prefix prefix;
    for (const auto& event : trace.events) {
      const auto& activity = event_activity(event, options.activity_attribute);
      auto& observation = observations[prefix];
      observation.next.insert(activity);
      if (observation.count == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("DFG precision: prefix count overflow");
      ++observation.count;
      prefix.push_back(activity);
    }
  }

  DfgPrecisionResult result;
  for (const auto& [prefix, observation] : observations) {
    if (result.prefix_occurrences >
        std::numeric_limits<std::uint64_t>::max() - observation.count)
      throw std::overflow_error("DFG precision: prefix count overflow");
    result.prefix_occurrences += observation.count;
    if (!allowed_prefix(prefix, starts, exiting)) continue;

    if (result.allowed_prefix_occurrences >
        std::numeric_limits<std::uint64_t>::max() - observation.count)
      throw std::overflow_error("DFG precision: prefix count overflow");
    result.allowed_prefix_occurrences += observation.count;
    const ActivitySet* activated = nullptr;
    if (prefix.empty()) {
      activated = &starts;
    } else {
      const auto it = exiting.find(prefix.back());
      if (it == exiting.end()) continue;
      activated = &it->second;
    }
    const auto weight = observation.count;
    if (weight != 0 && activated->size() >
                           std::numeric_limits<std::uint64_t>::max() / weight)
      throw std::overflow_error("DFG precision: transition count overflow");
    const auto activated_count =
        static_cast<std::uint64_t>(activated->size()) * weight;
    if (activated_count > std::numeric_limits<std::uint64_t>::max() -
                              result.activated_transitions)
      throw std::overflow_error("DFG precision: transition count overflow");
    result.activated_transitions += activated_count;
    std::uint64_t escaping = 0;
    for (const auto& activity : *activated)
      if (!observation.next.contains(activity)) ++escaping;
    if (weight != 0 && escaping >
                           std::numeric_limits<std::uint64_t>::max() / weight)
      throw std::overflow_error("DFG precision: transition count overflow");
    const auto escaping_count = escaping * weight;
    if (escaping_count > std::numeric_limits<std::uint64_t>::max() -
                             result.escaping_transitions)
      throw std::overflow_error("DFG precision: transition count overflow");
    result.escaping_transitions += escaping_count;
  }
  if (result.activated_transitions != 0) {
    result.precision =
        1.0 - static_cast<double>(result.escaping_transitions) /
                    static_cast<double>(result.activated_transitions);
    result.precision = std::clamp(result.precision, 0.0, 1.0);
  }
  result.computed = true;
  return result;
}

std::string serialize_dfg_precision_json(const DfgPrecisionResult& result) {
  if (!result.computed || !std::isfinite(result.precision) ||
      result.precision < 0.0 || result.precision > 1.0)
    throw std::invalid_argument("DFG precision: result is not serializable");
  std::ostringstream out;
  out << std::setprecision(17)
      << "{\"family\":\"DFG_PRECISION\",\"computed\":"
      << (result.computed ? "true" : "false")
      << ",\"precision\":" << result.precision
      << ",\"prefix_occurrences\":" << result.prefix_occurrences
      << ",\"allowed_prefix_occurrences\":"
      << result.allowed_prefix_occurrences
      << ",\"activated_transitions\":" << result.activated_transitions
      << ",\"escaping_transitions\":" << result.escaping_transitions
      << ",\"error\":" << json_quote(result.error) << '}';
  return out.str();
}

}  // namespace minera
