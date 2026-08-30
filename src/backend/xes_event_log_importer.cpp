#include "minera/xes_event_log_importer.hpp"

#include "gzip_codec.hpp"
#include "xml_pull_parser.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

// XML names may be namespace-qualified.  The XES vocabulary is identified by
// its local name, while the attribute values remain untouched.
std::string_view local_name(std::string_view name) {
  const auto separator = name.rfind(':');
  return separator == std::string_view::npos ? name
                                             : name.substr(separator + 1);
}

// The XES attribute element names and the kind each maps to. Anything else
// under an <event> (e.g. <list>, <container>, or an extension element) is not
// a scalar attribute and is skipped.
std::optional<AttributeKind> attribute_kind_of(std::string_view element) {
  element = local_name(element);
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

// Classifier keys are conventionally whitespace-separated, with single
// quotes around keys that contain whitespace.  Accept both quote styles and
// a backslash escape so classifier declarations survive a complete round
// trip even when a key itself contains a quote.
std::vector<std::string> tokenize_classifier_keys(std::string_view text) {
  std::vector<std::string> keys;
  std::size_t position = 0;
  while (position < text.size()) {
    while (position < text.size() &&
           std::isspace(static_cast<unsigned char>(text[position]))) {
      ++position;
    }
    if (position == text.size()) break;

    std::string key;
    const char quote = text[position] == '\'' || text[position] == '"'
                           ? text[position]
                           : '\0';
    if (quote != '\0') ++position;
    bool closed = quote == '\0';
    while (position < text.size()) {
      const char current = text[position++];
      if (current == '\\' && position < text.size()) {
        // Escapes are useful for quoted keys and harmless for an unquoted
        // key; retain the escaped character without its quoting backslash.
        key.push_back(text[position++]);
        continue;
      }
      if (quote != '\0' && current == quote) {
        closed = true;
        break;
      }
      if (quote == '\0' &&
          std::isspace(static_cast<unsigned char>(current))) {
        break;
      }
      key.push_back(current);
    }
    // An unterminated quote is kept as a best-effort token.  This mirrors the
    // permissive handling of other unknown XES metadata while never dropping
    // a classifier key from the imported document.
    (void)closed;
    keys.push_back(std::move(key));
  }
  return keys;
}

std::optional<EventAttribute> scalar_attribute(
    std::string_view element,
    const std::vector<std::pair<std::string, std::string>>& attributes) {
  const auto kind = attribute_kind_of(element);
  if (!kind) return std::nullopt;
  const auto* key = detail::attribute(attributes, "key");
  const auto* value = detail::attribute(attributes, "value");
  if (!key || !value) return std::nullopt;
  return EventAttribute{*key, *value, *kind};
}

bool first_class_activity_selector(std::string_view selector) {
  return selector == "activity" || selector == "concept:name" ||
         selector == "resource" || selector == "org:resource" ||
         selector == "timestamp" || selector == "time:timestamp" ||
         selector == "lifecycle" || selector == "lifecycle:transition";
}

void validate_variant(XesImportVariant variant) {
  switch (variant) {
    case XesImportVariant::kIterparse:
    case XesImportVariant::kLineByLine:
    case XesImportVariant::kIterparseMemoryCompressed:
    case XesImportVariant::kIterparse20:
    case XesImportVariant::kChunkRegex:
    case XesImportVariant::kRustXes:
      return;
  }
  throw std::invalid_argument("XES import: unsupported parser variant");
}

double required_timestamp(const Event& event) {
  if (event.timestamp.empty()) {
    throw std::runtime_error(
        "XES timestamp sorting requires a timestamp on every event");
  }
  const auto parsed = parse_timestamp(event.timestamp);
  if (!parsed) {
    throw std::runtime_error(
        "XES timestamp sorting encountered an invalid timestamp");
  }
  return *parsed;
}

void sort_log_by_timestamp(EventLog& log, bool reverse) {
  std::vector<std::pair<double, Trace>> timed_traces;
  timed_traces.reserve(log.traces.size());
  for (auto& trace : log.traces) {
    if (trace.events.empty()) continue;
    std::vector<std::pair<double, Event>> timed_events;
    timed_events.reserve(trace.events.size());
    for (auto& event : trace.events) {
      const auto timestamp = required_timestamp(event);
      timed_events.emplace_back(timestamp, std::move(event));
    }
    std::stable_sort(timed_events.begin(), timed_events.end(),
                     [reverse](const auto& lhs, const auto& rhs) {
                       return reverse ? lhs.first > rhs.first
                                      : lhs.first < rhs.first;
                     });
    trace.events.clear();
    trace.events.reserve(timed_events.size());
    for (auto& entry : timed_events)
      trace.events.push_back(std::move(entry.second));
    timed_traces.emplace_back(timed_events.front().first, std::move(trace));
  }
  std::stable_sort(timed_traces.begin(), timed_traces.end(),
                   [reverse](const auto& lhs, const auto& rhs) {
                     return reverse ? lhs.first > rhs.first
                                    : lhs.first < rhs.first;
                   });
  log.traces.clear();
  log.traces.reserve(timed_traces.size());
  for (auto& entry : timed_traces)
    log.traces.push_back(std::move(entry.second));
}

}  // namespace

XesImportVariant parse_xes_import_variant(std::string_view value) {
  std::string normalized(value);
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  if (normalized == "iterparse" || normalized == "lxml")
    return XesImportVariant::kIterparse;
  if (normalized == "line_by_line" || normalized == "nonstandard")
    return XesImportVariant::kLineByLine;
  if (normalized == "iterparse_mem_compressed")
    return XesImportVariant::kIterparseMemoryCompressed;
  if (normalized == "iterparse_20") return XesImportVariant::kIterparse20;
  if (normalized == "chunk_regex") return XesImportVariant::kChunkRegex;
  if (normalized == "rustxes" || normalized == "r4pm")
    return XesImportVariant::kRustXes;
  throw std::invalid_argument(
      "XES import variant must be iterparse, line_by_line, "
      "iterparse_mem_compressed, iterparse_20, chunk_regex, or rustxes");
}

std::string_view xes_import_variant_name(XesImportVariant variant) {
  switch (variant) {
    case XesImportVariant::kIterparse: return "iterparse";
    case XesImportVariant::kLineByLine: return "line_by_line";
    case XesImportVariant::kIterparseMemoryCompressed:
      return "iterparse_mem_compressed";
    case XesImportVariant::kIterparse20: return "iterparse_20";
    case XesImportVariant::kChunkRegex: return "chunk_regex";
    case XesImportVariant::kRustXes: return "rustxes";
  }
  throw std::invalid_argument("XES import: unsupported parser variant");
}

EventLog import_xes_event_log(
    std::istream& input,
    const XesImportOptions& options) {
  validate_variant(options.variant);
  if (options.case_attribute.empty() || options.activity_attribute.empty() ||
      options.timestamp_attribute.empty() || options.resource_attribute.empty() ||
      options.lifecycle_attribute.empty()) {
    throw std::invalid_argument("XES import selectors must not be empty");
  }
  std::unordered_set<std::string> attributes_to_read(
      options.attributes_to_read.begin(), options.attributes_to_read.end());
  const auto keeps_attribute = [&](std::string_view key) {
    return attributes_to_read.empty() || attributes_to_read.contains(std::string(key));
  };

  std::string xml((std::istreambuf_iterator<char>(input)),
                  std::istreambuf_iterator<char>());
  if (detail::is_gzip_data(xml)) {
    xml = detail::gzip_decompress(xml, options.max_decompressed_bytes);
  } else if (xml.size() > options.max_decompressed_bytes) {
    throw std::runtime_error("XES input exceeds the configured size limit");
  }

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
  bool case_attribute_present = false;
  std::vector<EventAttribute>* active_global = nullptr;

  Event current_event;
  bool in_event = false;
  bool activity_attribute_present = false;
  int activity_match_rank = 0;
  int timestamp_match_rank = 0;
  int resource_match_rank = 0;
  int lifecycle_match_rank = 0;

  std::size_t input_order = 0;
  bool saw_timestamp = false;

  const auto finish_event = [&]() {
    if (!activity_attribute_present) {
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
    activity_attribute_present = false;
    activity_match_rank = 0;
    timestamp_match_rank = 0;
    resource_match_rank = 0;
    lifecycle_match_rank = 0;
  };

  const auto finish_trace = [&]() {
    if (!case_attribute_present) {
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
    case_attribute_present = false;
    in_trace = false;
  };

  while (true) {
    auto token = parser.next();
    if (token.kind == detail::XmlPullParser::Kind::EndOfInput) {
      break;
    }

    if (token.kind == detail::XmlPullParser::Kind::StartElement) {
      const std::string parent =
          stack.empty() ? std::string{} : std::string(local_name(stack.back()));
      const std::string element = std::string(local_name(token.name));

      if (element == "trace" && parent == "log") {
        if (log.traces.size() >= options.max_traces) break;
        if (in_trace) {
          throw std::runtime_error("XES input nests a <trace> inside a <trace>");
        }
        in_trace = true;
      } else if (element == "event" && in_trace && parent == "trace") {
        in_event = true;
      } else if (!in_trace && parent == "log" && element == "extension") {
        // Keep declarations in document order.  Missing optional values are
        // represented as empty strings so a permissive read does not discard
        // a declaration from an otherwise usable log.
        LogExtension extension;
        if (const auto* value = detail::attribute(token.attributes, "name"))
          extension.name = *value;
        if (const auto* value = detail::attribute(token.attributes, "prefix"))
          extension.prefix = *value;
        if (const auto* value = detail::attribute(token.attributes, "uri"))
          extension.uri = *value;
        log.metadata.extensions.push_back(std::move(extension));
      } else if (!in_trace && parent == "log" && element == "classifier") {
        LogClassifier classifier;
        if (const auto* value = detail::attribute(token.attributes, "name"))
          classifier.name = *value;
        if (const auto* value = detail::attribute(token.attributes, "keys"))
          classifier.keys = tokenize_classifier_keys(*value);
        log.metadata.classifiers.push_back(std::move(classifier));
      } else if (!in_trace && parent == "log" && element == "global") {
        active_global = nullptr;
        if (const auto* scope = detail::attribute(token.attributes, "scope")) {
          if (*scope == "trace") {
            active_global = &log.metadata.trace_globals;
          } else if (*scope == "event") {
            active_global = &log.metadata.event_globals;
          }
        }
      } else if (active_global && parent == "global") {
        if (const auto attribute = scalar_attribute(element,
                                                     token.attributes);
            attribute && keeps_attribute(attribute->key)) {
          active_global->push_back(*attribute);
        }
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
                activity_attribute_present = true;
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
            if (!recognized && keeps_attribute(*key)) {
              if (const auto kind = attribute_kind_of(element)) {
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
              case_attribute_present = true;
            } else if (keeps_attribute(*key)) {
              if (const auto kind = attribute_kind_of(element)) {
                // Any other scalar trace attribute is a case attribute, kept
                // with its scalar type like the generic event attributes.
                current_trace.attributes.push_back(
                    EventAttribute{*key, *value, *kind});
              }
            }
          }
        }
      } else if (!in_trace && parent == "log") {
        if (const auto* key = detail::attribute(token.attributes, "key")) {
          if (const auto* value = detail::attribute(token.attributes, "value")) {
            // The log name is supplied separately by Minera's XES exporter;
            // retain every other scalar log attribute, including derived
            // projection metadata such as @@aligned_dfg.
            if (*key != "concept:name" && keeps_attribute(*key)) {
              if (const auto kind = attribute_kind_of(element)) {
                log.attributes.push_back(EventAttribute{*key, *value, *kind});
              }
            }
          }
        }
      }

      if (token.self_closing) {
        // A self-closing trace/event still opens and closes here.
        if (element == "event" && in_event && parent == "trace") {
          finish_event();
        } else if (element == "trace" && in_trace && parent == "log") {
          finish_trace();
        } else if (element == "global") {
          active_global = nullptr;
        }
      } else {
        stack.push_back(token.name);
      }
      continue;
    }

    // EndElement: unwind the stack to the matching open element. Tolerates
    // minor mismatches by popping until the name is found.
    while (!stack.empty() && local_name(stack.back()) != local_name(token.name)) {
      stack.pop_back();
    }
    if (!stack.empty()) {
      stack.pop_back();
    }
    const auto element = local_name(token.name);
    if (element == "global") {
      active_global = nullptr;
    }
    if (element == "event" && in_event) {
      finish_event();
    } else if (element == "trace" && in_trace) {
      finish_trace();
    }
  }

  if (options.require_timestamp && !saw_timestamp) {
    throw std::runtime_error(
        "XES log has no timestamps (attribute '" +
        options.timestamp_attribute + "')");
  }

  // Keep the selectors used to interpret this document alongside the native
  // data.  These properties are deterministic and intentionally distinct from
  // scalar log attributes; they allow a table/stream adapter to reproduce the
  // same interpretation without introducing another side-channel.  Do not
  // append a key twice if this function is extended with another selector.
  const auto append_property = [&](std::string_view key,
                                   std::string_view value) {
    const auto duplicate = std::any_of(
        log.metadata.properties.begin(), log.metadata.properties.end(),
        [&](const auto& property) { return property.key == key; });
    if (!duplicate) {
      log.metadata.properties.push_back(
          {std::string(key), std::string(value), AttributeKind::kString});
    }
  };
  append_property("case_attribute", options.case_attribute);
  append_property("activity_attribute", options.activity_attribute);
  append_property("timestamp_attribute", options.timestamp_attribute);
  append_property("resource_attribute", options.resource_attribute);
  append_property("lifecycle_attribute", options.lifecycle_attribute);

  if (options.timestamp_sort) sort_log_by_timestamp(log, options.reverse_sort);

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
