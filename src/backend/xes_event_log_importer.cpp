#include "minera/xes_event_log_importer.hpp"

#include "gzip_codec.hpp"
#include "xml_pull_parser.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
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

// The XES attribute element names and the kind each maps to.
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

bool true_attribute(const std::vector<std::pair<std::string, std::string>>& attrs,
                   std::string_view name) {
  const auto* value = detail::attribute(attrs, name);
  if (!value) return false;
  std::string normalized = *value;
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  return normalized == "true" || normalized == "1" || normalized == "yes";
}

NativeAttributeValue parse_native_value(AttributeKind kind,
                                       std::string_view value,
                                       XesImportVariant variant) {
  switch (kind) {
    case AttributeKind::kInt: {
      if (value.empty())
        throw std::invalid_argument("XES int attribute has an empty value");
      const char* first = value.data();
      const char* last = value.data() + value.size();
      if (*first == '+') ++first;
      std::int64_t parsed = 0;
      const auto result = std::from_chars(first, last, parsed, 10);
      if (result.ec != std::errc{} || result.ptr != last) {
        throw std::invalid_argument("XES int attribute has an invalid value: " +
                                    std::string(value));
      }
      return parsed;
    }
    case AttributeKind::kFloat: {
      if (value == "INF" || value == "+INF")
        return std::numeric_limits<double>::infinity();
      if (value == "-INF")
        return -std::numeric_limits<double>::infinity();
      if (value == "NaN") return std::numeric_limits<double>::quiet_NaN();
      double parsed = 0.0;
      const char* first = value.data();
      if (!value.empty() && *first == '+') ++first;
      const auto result = std::from_chars(
          first, value.data() + value.size(), parsed,
          std::chars_format::general);
      if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        throw std::invalid_argument(
            "XES float attribute has an invalid value: " +
            std::string(value));
      }
      return parsed;
    }
    case AttributeKind::kBoolean:
      if (value == "true" || value == "1") return true;
      if (value == "false" || value == "0") return false;
      throw std::invalid_argument("XES boolean attribute has an invalid value: " +
                                  std::string(value));
    case AttributeKind::kDate: {
      auto parsed = parse_timestamp(value);
      if (!parsed) {
        throw std::invalid_argument("XES date attribute has an invalid value: " +
                                    std::string(value));
      }
      std::string wall_time;
      if (variant != XesImportVariant::kRustXes) {
        const auto time = value.find_first_of("Tt ");
        const auto offset = time == std::string_view::npos ? std::string_view::npos
            : value.find_first_of("+-", time + 1);
        if (offset != std::string_view::npos) {
          // These parser variations retain the clock fields and attach UTC.
          wall_time = std::string(value.substr(0, offset)) + 'Z';
          value = wall_time;
          parsed = parse_timestamp(value);
        }
      }
      if (const auto dot = value.find('.'); dot != std::string_view::npos) {
        auto end = dot + 1;
        while (end < value.size() && value[end] >= '0' && value[end] <= '9') ++end;
        if (end - dot - 1 > 3) {
          auto integral = std::string(value);
          integral.erase(dot, end - dot);
          const auto whole = parse_timestamp(integral);
          if (!whole) throw std::invalid_argument("XES date has an invalid integral instant");
          auto normalized = format_timestamp(*whole);
          normalized.resize(normalized.find('.'));
          normalized += '.';
          normalized += value.substr(dot + 1, std::min<std::size_t>(6, end - dot - 1));
          normalized += 'Z';
          return NativeDate{std::move(normalized)};
        }
      }
      return NativeDate{format_timestamp(*parsed)};
    }
    case AttributeKind::kString:
    case AttributeKind::kId:
      return std::monostate{};
  }
  return std::monostate{};
}

void skip_element(detail::XmlPullParser& parser,
                  const detail::XmlPullParser::Token& start) {
  constexpr std::size_t kMaxSkippedDepth = 128;
  if (start.self_closing) return;
  std::size_t depth = 1;
  while (depth != 0) {
    const auto token = parser.next();
    if (token.kind == detail::XmlPullParser::Kind::EndOfInput) return;
    if (token.kind == detail::XmlPullParser::Kind::StartElement &&
        !token.self_closing) {
      ++depth;
      if (depth > kMaxSkippedDepth)
        throw std::invalid_argument("XES attribute nesting exceeds the maximum depth");
    } else if (token.kind == detail::XmlPullParser::Kind::EndElement) {
      --depth;
    }
  }
}

std::optional<EventAttribute> parse_attribute_node(
    detail::XmlPullParser& parser,
    const detail::XmlPullParser::Token& start,
    XesImportVariant variant,
    std::size_t depth = 0) {
  constexpr std::size_t kMaxAttributeDepth = 128;
  if (depth > kMaxAttributeDepth)
    throw std::invalid_argument("XES attribute nesting exceeds the maximum depth");
  const auto element = local_name(start.name);
  const auto kind = attribute_kind_of(element);
  const bool list = element == "list";
  const bool container = element == "container";
  if (!kind && !list && !container) {
    skip_element(parser, start);
    return std::nullopt;
  }

  const auto* key = detail::attribute(start.attributes, "key");
  if (!key) {
    skip_element(parser, start);
    return std::nullopt;
  }
  const bool is_null = true_attribute(start.attributes, "null") ||
                       true_attribute(start.attributes, "is_null");
  EventAttribute result;
  result.key = *key;
  result.container = list ? AttributeContainerKind::kList
                          : container ? AttributeContainerKind::kContainer
                                      : AttributeContainerKind::kScalar;
  result.kind = kind.value_or(AttributeKind::kString);
  result.is_null = is_null;
  if (const auto* value = detail::attribute(start.attributes, "value")) {
    result.value = *value;
  } else if (!is_null && kind) {
    throw std::invalid_argument("XES scalar attribute is missing its value");
  }
  if (!is_null && kind) {
    result.typed_value = parse_native_value(*kind, result.value, variant);
    if (const auto* date = std::get_if<NativeDate>(&result.typed_value))
      result.value = date->value;
  }
  if (start.self_closing) return result;

  while (true) {
    const auto token = parser.next();
    if (token.kind == detail::XmlPullParser::Kind::EndOfInput) {
      throw std::runtime_error("XES attribute element is not closed");
    }
    if (token.kind == detail::XmlPullParser::Kind::EndElement) {
      if (local_name(token.name) == element) return result;
      continue;
    }
    if (token.kind != detail::XmlPullParser::Kind::StartElement) continue;
    if (!kind) {
      // The XES list grammar wraps its entries in a <values> element.  Older
      // producers also emit direct children, so accept both forms while
      // preserving every recursive typed attribute.
      if (list && local_name(token.name) == "values") {
        if (token.self_closing) continue;
        while (true) {
          const auto values_token = parser.next();
          if (values_token.kind == detail::XmlPullParser::Kind::EndOfInput)
            throw std::runtime_error("XES list <values> element is not closed");
          if (values_token.kind == detail::XmlPullParser::Kind::EndElement) {
            if (local_name(values_token.name) == "values") break;
            continue;
          }
          if (values_token.kind != detail::XmlPullParser::Kind::StartElement)
            continue;
          if (const auto child =
                  parse_attribute_node(parser, values_token, variant, depth + 1))
            result.children.push_back(*child);
        }
      } else if (const auto child =
                     parse_attribute_node(parser, token, variant, depth + 1)) {
        result.children.push_back(*child);
      }
    } else if (const auto child =
                   parse_attribute_node(parser, token, variant, depth + 1)) {
      // Scalar attributes are normally self-closing, but the attribute
      // representation also permits recursive metadata children. Preserve
      // those children instead of discarding the whole nested subtree.
      result.children.push_back(*child);
    }
  }
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

int case_selector_match_rank(std::string_view key, std::string_view selector) {
  if (key == selector) return 2;
  if (selector == "concept:name" || selector == "case_id" ||
      selector == "case:concept:name") {
    return key == "concept:name" || key == "case_id" ||
                   key == "case:concept:name"
               ? 1
               : 0;
  }
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

using TimestampOrderKey = std::pair<std::int64_t, std::int64_t>;

TimestampOrderKey required_timestamp(const Event& event) {
  if (event.timestamp.empty()) {
    throw std::runtime_error(
        "XES timestamp sorting requires a timestamp on every event");
  }
  const auto parsed = parse_timestamp(event.timestamp);
  if (!parsed) {
    throw std::runtime_error(
        "XES timestamp sorting encountered an invalid timestamp");
  }
  auto integral = event.timestamp;
  std::int64_t fraction = 0;
  if (const auto dot = integral.find('.'); dot != std::string::npos) {
    auto end = dot + 1;
    unsigned digits = 0;
    while (end < integral.size() && std::isdigit(static_cast<unsigned char>(integral[end]))) {
      if (digits++ < 9) fraction = fraction * 10 + integral[end] - '0';
      ++end;
    }
    while (digits++ < 9) fraction *= 10;
    integral.erase(dot, end - dot);
  }
  return {static_cast<std::int64_t>(*parse_timestamp(integral)), fraction};
}

void sort_log_by_timestamp(EventLog& log, bool reverse) {
  std::vector<std::pair<TimestampOrderKey, Trace>> timed_traces;
  timed_traces.reserve(log.traces.size());
  for (auto& trace : log.traces) {
    if (trace.events.empty()) continue;
    std::vector<std::pair<TimestampOrderKey, Event>> timed_events;
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
  std::vector<std::string> stack;

  Trace current_trace;
  bool in_trace = false;
  std::size_t case_match_rank = 0;
  LogGlobal* active_global = nullptr;

  Event current_event;
  bool in_event = false;
  int activity_match_rank = 0;
  int timestamp_match_rank = 0;
  int resource_match_rank = 0;
  int lifecycle_match_rank = 0;

  std::size_t input_order = 0;
  bool saw_timestamp = false;

  const auto erase_selected = [](std::vector<EventAttribute>& attributes,
                                 std::string_view key) {
    if (key.empty()) return;
    const auto found = std::find_if(
        attributes.begin(), attributes.end(),
        [&](const auto& attribute) { return attribute.key == key; });
    if (found != attributes.end() && found->children.empty())
      attributes.erase(found);
  };

  const auto apply_event_attribute = [&](EventAttribute attribute) {
    const auto key = std::string_view(attribute.key);
    const auto set_first_class = [&](int rank, int& current_rank,
                                     std::string& target,
                                     FirstClassAttributeState& state,
                                     std::string& source_key,
                                     std::optional<EventAttribute>& metadata) {
      if (rank == 0 || rank <= current_rank) return;
      current_rank = rank;
      target = attribute.value;
      state = attribute.is_null ? FirstClassAttributeState::kNull
                                : FirstClassAttributeState::kPresent;
      source_key = attribute.key;
      metadata = attribute;
    };
    const auto activity_rank = selector_match_rank(
        key, options.activity_attribute);
    const auto timestamp_rank = selector_match_rank(
        key, options.timestamp_attribute);
    const auto resource_rank = selector_match_rank(
        key, options.resource_attribute);
    const auto lifecycle_rank = selector_match_rank(
        key, options.lifecycle_attribute);
    const bool scalar = attribute.container == AttributeContainerKind::kScalar;
    if (scalar) {
      set_first_class(activity_rank, activity_match_rank,
                      current_event.activity, current_event.activity_state,
                      current_event.activity_source_key,
                      current_event.activity_attribute);
      set_first_class(timestamp_rank, timestamp_match_rank,
                      current_event.timestamp, current_event.timestamp_state,
                      current_event.timestamp_source_key,
                      current_event.timestamp_attribute);
      set_first_class(resource_rank, resource_match_rank,
                      current_event.resource, current_event.resource_state,
                      current_event.resource_source_key,
                      current_event.resource_attribute);
      set_first_class(lifecycle_rank, lifecycle_match_rank,
                      current_event.lifecycle, current_event.lifecycle_state,
                      current_event.lifecycle_source_key,
                      current_event.lifecycle_attribute);
      if (timestamp_rank != 0) saw_timestamp = true;
    }
    // Keep aliases and conflicting canonical keys.  The selected key is
    // removed once the complete event has been read, after selector ranking is
    // known.
    if (attributes_to_read.empty() || attributes_to_read.contains(attribute.key) ||
        activity_rank != 0 || timestamp_rank != 0 || resource_rank != 0 ||
        lifecycle_rank != 0) {
      current_event.attributes.push_back(std::move(attribute));
    }
  };

  const auto finish_event = [&]() {
    erase_selected(current_event.attributes, current_event.activity_source_key);
    erase_selected(current_event.attributes, current_event.timestamp_source_key);
    erase_selected(current_event.attributes, current_event.resource_source_key);
    erase_selected(current_event.attributes, current_event.lifecycle_source_key);
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
    erase_selected(current_trace.attributes, current_trace.case_id_source_key);
    for (auto& event : current_trace.events) {
      event.case_id = current_trace.case_id;
      event.case_id_state = current_trace.case_id_state;
      event.case_id_source_key = current_trace.case_id_source_key;
    }
    log.traces.push_back(std::move(current_trace));
    current_trace = Trace{};
    case_match_rank = 0;
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
      bool consumed_by_parser = false;

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
        LogGlobal global;
        if (const auto* scope = detail::attribute(token.attributes, "scope"))
          global.scope = *scope;
        log.metadata.globals.push_back(std::move(global));
        active_global = &log.metadata.globals.back();
      } else if (active_global && parent == "global") {
        consumed_by_parser = true;
        if (const auto attribute = parse_attribute_node(parser, token, options.variant)) {
          if (attributes_to_read.empty() ||
              attributes_to_read.contains(attribute->key)) {
            active_global->attributes.push_back(*attribute);
            if (active_global->scope == "trace")
              log.metadata.trace_globals.push_back(*attribute);
            if (active_global->scope == "event")
              log.metadata.event_globals.push_back(*attribute);
          }
        }
      } else if (in_event && parent == "event") {
        consumed_by_parser = true;
        if (const auto attribute = parse_attribute_node(parser, token, options.variant))
          apply_event_attribute(*attribute);
      } else if (in_trace && !in_event && parent == "trace") {
        consumed_by_parser = true;
        if (const auto attribute = parse_attribute_node(parser, token, options.variant)) {
          const auto rank = attribute->container == AttributeContainerKind::kScalar
                                ? case_selector_match_rank(
                                      attribute->key, options.case_attribute)
                                : 0;
          if (rank > static_cast<int>(case_match_rank)) {
            case_match_rank = static_cast<std::size_t>(rank);
            current_trace.case_id = attribute->value;
            current_trace.case_id_state =
                attribute->is_null ? FirstClassAttributeState::kNull
                                    : FirstClassAttributeState::kPresent;
            current_trace.case_id_source_key = attribute->key;
            current_trace.case_id_attribute = *attribute;
          }
          if (attributes_to_read.empty() ||
              attributes_to_read.contains(attribute->key) || rank != 0)
            current_trace.attributes.push_back(*attribute);
        }
      } else if (!in_trace && parent == "log") {
        consumed_by_parser = true;
        if (const auto attribute = parse_attribute_node(parser, token, options.variant);
            attribute && (attributes_to_read.empty() ||
                          attributes_to_read.contains(attribute->key)))
          log.attributes.push_back(*attribute);
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
      } else if (!consumed_by_parser) {
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

  // Materialize the legacy projections after all arbitrary scopes have been
  // parsed. Duplicate trace/event scopes retain their document order by
  // concatenating their attributes in the compatibility vectors.

  log.metadata.compatibility_snapshot_valid = true;
  log.metadata.compatibility_globals_snapshot = log.metadata.globals;
  log.metadata.compatibility_trace_globals_snapshot =
      log.metadata.trace_globals;
  log.metadata.compatibility_event_globals_snapshot =
      log.metadata.event_globals;

  if (options.timestamp_sort && options.variant != XesImportVariant::kChunkRegex)
    sort_log_by_timestamp(log, options.reverse_sort);

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
