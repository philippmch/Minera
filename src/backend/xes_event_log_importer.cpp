#include "minera/xes_event_log_importer.hpp"

#include "xml_pull_parser.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace minera {
namespace {

// The XES attribute element names and the kind each maps to. Anything else
// under an <event> (e.g. <list>, <container>, or an extension element) is not
// a scalar attribute and is skipped.
std::optional<AttributeKind> attribute_kind_of(std::string_view element) {
  if (element == "string") return AttributeKind::kString;
  if (element == "date") return AttributeKind::kDate;
  if (element == "int") return AttributeKind::kInt;
  if (element == "float") return AttributeKind::kFloat;
  if (element == "boolean") return AttributeKind::kBoolean;
  if (element == "id") return AttributeKind::kId;
  return std::nullopt;
}

int selector_match_rank(std::string_view key, std::string_view selector) {
  // The configured spelling is authoritative; its short/qualified alternate
  // is only a fallback when the authoritative key is absent.
  if (key == selector) return 2;
  if (selector == "activity" || selector == "concept:name")
    return key == "activity" || key == "concept:name" ? 1 : 0;
  if (selector == "resource" || selector == "org:resource")
    return key == "resource" || key == "org:resource" ? 1 : 0;
  if (selector == "timestamp" || selector == "time:timestamp")
    return key == "timestamp" || key == "time:timestamp" ? 1 : 0;
  if (selector == "lifecycle" || selector == "lifecycle:transition")
    return key == "lifecycle" || key == "lifecycle:transition" ? 1 : 0;
  return 0;
}

bool first_class_activity_selector(std::string_view selector) {
  return selector == "activity" || selector == "concept:name" ||
         selector == "resource" || selector == "org:resource" ||
         selector == "timestamp" || selector == "time:timestamp" ||
         selector == "lifecycle" || selector == "lifecycle:transition";
}

}  // namespace

EventLog import_xes_event_log(
    std::istream& input,
    const XesImportOptions& options) {
  std::string xml((std::istreambuf_iterator<char>(input)),
                  std::istreambuf_iterator<char>());

  // Strip a leading UTF-8 byte-order mark so the first '<' is found cleanly.
  if (xml.size() >= 3 && static_cast<unsigned char>(xml[0]) == 0xEF &&
      static_cast<unsigned char>(xml[1]) == 0xBB &&
      static_cast<unsigned char>(xml[2]) == 0xBF) {
    xml.erase(0, 3);
  }

  detail::XmlPullParser parser(xml);
  EventLog log;

  // Element nesting, so leaf attributes can be matched to their direct parent
  // (a <string> under <trace> is a case attribute; under <event>, an event
  // attribute; deeper nesting is a sub-attribute we ignore).
  std::vector<std::string> stack;

  Trace current_trace;
  std::string case_id;
  bool in_trace = false;

  Event current_event;
  bool in_event = false;
  int activity_match_rank = 0;
  int timestamp_match_rank = 0;
  int resource_match_rank = 0;
  int lifecycle_match_rank = 0;

  std::size_t input_order = 0;
  bool saw_timestamp = false;

  const auto finish_event = [&]() {
    if (current_event.activity.empty()) {
      throw std::runtime_error(
          "XES event is missing its activity attribute ('" +
          options.activity_attribute + "')");
    }
    // Keep a selected non-default activity key available in the generic bag
    // too, so replay can apply that selector consistently after import.
    if (!first_class_activity_selector(options.activity_attribute) &&
        current_event.attribute(options.activity_attribute) == nullptr) {
      current_event.attributes.push_back(EventAttribute{
          options.activity_attribute, current_event.activity,
          AttributeKind::kString});
    }
    current_event.input_order = input_order++;
    current_trace.events.push_back(std::move(current_event));
    current_event = Event{};
    in_event = false;
    activity_match_rank = 0;
    timestamp_match_rank = 0;
    resource_match_rank = 0;
    lifecycle_match_rank = 0;
  };

  const auto finish_trace = [&]() {
    if (case_id.empty()) {
      throw std::runtime_error(
          "XES trace is missing its case attribute ('" +
          options.case_attribute + "')");
    }
    current_trace.case_id = case_id;
    for (auto& event : current_trace.events) {
      event.case_id = case_id;
    }
    log.traces.push_back(std::move(current_trace));
    current_trace = Trace{};
    case_id.clear();
    in_trace = false;
  };

  while (true) {
    auto token = parser.next();
    if (token.kind == detail::XmlPullParser::Kind::EndOfInput) {
      break;
    }

    if (token.kind == detail::XmlPullParser::Kind::StartElement) {
      const std::string parent = stack.empty() ? std::string{} : stack.back();

      if (token.name == "trace" && parent == "log") {
        if (in_trace) {
          throw std::runtime_error("XES input nests a <trace> inside a <trace>");
        }
        in_trace = true;
      } else if (token.name == "event" && in_trace && parent == "trace") {
        in_event = true;
      } else if (in_event && parent == "event") {
        if (const auto* key = detail::attribute(token.attributes, "key")) {
          if (const auto* value = detail::attribute(token.attributes, "value")) {
            bool recognized = false;
            const auto activity_rank =
                selector_match_rank(*key, options.activity_attribute);
            if (activity_rank != 0) {
              if (activity_rank > activity_match_rank) {
                current_event.activity = *value;
                activity_match_rank = activity_rank;
              }
              recognized = true;
            }
            const auto timestamp_rank =
                selector_match_rank(*key, options.timestamp_attribute);
            if (timestamp_rank != 0) {
              if (timestamp_rank > timestamp_match_rank) {
                current_event.timestamp = *value;
                timestamp_match_rank = timestamp_rank;
              }
              saw_timestamp = true;
              recognized = true;
            }
            const auto resource_rank =
                selector_match_rank(*key, options.resource_attribute);
            if (resource_rank != 0) {
              if (resource_rank > resource_match_rank) {
                current_event.resource = *value;
                resource_match_rank = resource_rank;
              }
              recognized = true;
            }
            const auto lifecycle_rank =
                selector_match_rank(*key, options.lifecycle_attribute);
            if (lifecycle_rank != 0) {
              if (lifecycle_rank > lifecycle_match_rank) {
                current_event.lifecycle = *value;
                lifecycle_match_rank = lifecycle_rank;
              }
              recognized = true;
            }
            if (!recognized) {
              if (const auto kind = attribute_kind_of(token.name)) {
                // Any other scalar attribute is kept generically, with its XES
                // type so an export can round-trip it faithfully.
                current_event.attributes.push_back(
                    EventAttribute{*key, *value, *kind});
              }
            }
          }
        }
      } else if (in_trace && !in_event && parent == "trace") {
        if (const auto* key = detail::attribute(token.attributes, "key")) {
          if (const auto* value = detail::attribute(token.attributes, "value")) {
            if (*key == options.case_attribute) {
              case_id = *value;
            } else if (const auto kind = attribute_kind_of(token.name)) {
              // Any other scalar trace attribute is a case attribute (e.g.
              // BPI 2012's AMOUNT_REQ), kept with its XES type like the
              // generic event attributes.
              current_trace.attributes.push_back(
                  EventAttribute{*key, *value, *kind});
            }
          }
        }
      } else if (!in_trace && parent == "log") {
        if (const auto* key = detail::attribute(token.attributes, "key")) {
          if (const auto* value = detail::attribute(token.attributes, "value")) {
            // The log name is supplied separately by Minera's XES exporter;
            // retain every other scalar log attribute, including derived
            // projection metadata such as @@aligned_dfg.
            if (*key != "concept:name") {
              if (const auto kind = attribute_kind_of(token.name)) {
                log.attributes.push_back(EventAttribute{*key, *value, *kind});
              }
            }
          }
        }
      }

      if (token.self_closing) {
        // A self-closing trace/event still opens and closes here.
        if (token.name == "event" && in_event && parent == "trace") {
          finish_event();
        } else if (token.name == "trace" && in_trace && parent == "log") {
          finish_trace();
        }
      } else {
        stack.push_back(token.name);
      }
      continue;
    }

    // EndElement: unwind the stack to the matching open element. Tolerates
    // minor mismatches by popping until the name is found.
    while (!stack.empty() && stack.back() != token.name) {
      stack.pop_back();
    }
    if (!stack.empty()) {
      stack.pop_back();
    }
    if (token.name == "event" && in_event) {
      finish_event();
    } else if (token.name == "trace" && in_trace) {
      finish_trace();
    }
  }

  if (options.require_timestamp && !saw_timestamp) {
    throw std::runtime_error(
        "XES log has no timestamps (attribute '" +
        options.timestamp_attribute + "')");
  }

  // Mirror the CSV importer: when timestamps are present, order events within
  // each trace by timestamp, keeping document order for equal timestamps.
  if (saw_timestamp) {
    for (auto& trace : log.traces) {
      order_events_by_time(trace.events);
    }
  }

  return log;
}

EventLog import_xes_event_log(
    const std::filesystem::path& path,
    const XesImportOptions& options) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("Unable to open XES event log: " + path.string());
  }
  return import_xes_event_log(input, options);
}

}  // namespace minera
