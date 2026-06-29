#include "minera/xes_event_log_importer.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace minera {
namespace {

// Decodes the five predefined XML entities plus numeric character references
// into a UTF-8 string. XES attribute values (activity names, case ids) may
// legitimately contain '&', '<', and friends in escaped form.
void append_utf8(std::string& out, unsigned long code_point) {
  if (code_point <= 0x7F) {
    out.push_back(static_cast<char>(code_point));
  } else if (code_point <= 0x7FF) {
    out.push_back(static_cast<char>(0xC0 | (code_point >> 6)));
    out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
  } else if (code_point <= 0xFFFF) {
    out.push_back(static_cast<char>(0xE0 | (code_point >> 12)));
    out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (code_point >> 18)));
    out.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
  }
}

std::string decode_entities(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '&') {
      out.push_back(text[i]);
      continue;
    }
    const auto semicolon = text.find(';', i + 1);
    if (semicolon == std::string_view::npos) {
      out.push_back('&');
      continue;
    }
    const std::string_view entity = text.substr(i + 1, semicolon - i - 1);
    if (entity == "amp") {
      out.push_back('&');
    } else if (entity == "lt") {
      out.push_back('<');
    } else if (entity == "gt") {
      out.push_back('>');
    } else if (entity == "quot") {
      out.push_back('"');
    } else if (entity == "apos") {
      out.push_back('\'');
    } else if (!entity.empty() && entity.front() == '#') {
      try {
        const bool hex = entity.size() > 1 && (entity[1] == 'x' || entity[1] == 'X');
        const auto code = std::stoul(
            std::string(entity.substr(hex ? 2 : 1)), nullptr, hex ? 16 : 10);
        append_utf8(out, code);
      } catch (const std::exception&) {
        out.push_back('&');
        out.append(entity);
        out.push_back(';');
      }
    } else {
      // Unknown entity: pass it through verbatim rather than losing data.
      out.push_back('&');
      out.append(entity);
      out.push_back(';');
    }
    i = semicolon;
  }
  return out;
}

bool is_name_char(char ch) {
  return !std::isspace(static_cast<unsigned char>(ch)) && ch != '/' &&
         ch != '>' && ch != '=';
}

// A minimal pull parser over the XES/XML subset Minera needs: it yields start
// and end elements with their attributes and skips prologs, comments, CDATA,
// DOCTYPE, and character data (which XES never uses for the fields we read).
class XmlPullParser {
 public:
  explicit XmlPullParser(std::string_view xml) : xml_(xml) {}

  enum class Kind { StartElement, EndElement, EndOfInput };

  struct Token {
    Kind kind = Kind::EndOfInput;
    std::string name;
    std::vector<std::pair<std::string, std::string>> attributes;
    bool self_closing = false;
  };

  Token next() {
    while (pos_ < xml_.size()) {
      const auto open = xml_.find('<', pos_);
      if (open == std::string_view::npos) {
        pos_ = xml_.size();
        return {};
      }
      pos_ = open + 1;
      if (pos_ >= xml_.size()) {
        throw std::runtime_error("XES input ends inside a tag");
      }

      const char marker = xml_[pos_];
      if (marker == '?') {
        skip_until("?>");
        continue;
      }
      if (marker == '!') {
        if (starts_with("!--")) {
          skip_until("-->");
        } else if (starts_with("![CDATA[")) {
          skip_until("]]>");
        } else {
          skip_declaration();
        }
        continue;
      }
      if (marker == '/') {
        ++pos_;
        return read_end_tag();
      }
      return read_start_tag();
    }
    return {};
  }

 private:
  bool starts_with(std::string_view literal) const {
    return xml_.substr(pos_, literal.size()) == literal;
  }

  void skip_until(std::string_view terminator) {
    const auto end = xml_.find(terminator, pos_);
    if (end == std::string_view::npos) {
      throw std::runtime_error("XES input has an unterminated markup section");
    }
    pos_ = end + terminator.size();
  }

  // Skips a <!DOCTYPE …> declaration, tolerating an internal [ … ] subset that
  // may itself contain '>' characters.
  void skip_declaration() {
    int bracket_depth = 0;
    while (pos_ < xml_.size()) {
      const char ch = xml_[pos_++];
      if (ch == '[') {
        ++bracket_depth;
      } else if (ch == ']') {
        if (bracket_depth > 0) --bracket_depth;
      } else if (ch == '>' && bracket_depth == 0) {
        return;
      }
    }
    throw std::runtime_error("XES input has an unterminated declaration");
  }

  void skip_space() {
    while (pos_ < xml_.size() &&
           std::isspace(static_cast<unsigned char>(xml_[pos_]))) {
      ++pos_;
    }
  }

  std::string read_name() {
    const auto start = pos_;
    while (pos_ < xml_.size() && is_name_char(xml_[pos_])) {
      ++pos_;
    }
    if (pos_ == start) {
      throw std::runtime_error("XES tag is missing a name");
    }
    return std::string(xml_.substr(start, pos_ - start));
  }

  Token read_end_tag() {
    Token token;
    token.kind = Kind::EndElement;
    token.name = read_name();
    const auto close = xml_.find('>', pos_);
    if (close == std::string_view::npos) {
      throw std::runtime_error("XES end tag is not closed");
    }
    pos_ = close + 1;
    return token;
  }

  Token read_start_tag() {
    Token token;
    token.kind = Kind::StartElement;
    token.name = read_name();

    while (true) {
      skip_space();
      if (pos_ >= xml_.size()) {
        throw std::runtime_error("XES start tag is not closed");
      }
      const char ch = xml_[pos_];
      if (ch == '>') {
        ++pos_;
        break;
      }
      if (ch == '/') {
        ++pos_;
        if (pos_ >= xml_.size() || xml_[pos_] != '>') {
          throw std::runtime_error("XES self-closing tag is malformed");
        }
        ++pos_;
        token.self_closing = true;
        break;
      }

      std::string attr_name = read_name();
      skip_space();
      if (pos_ >= xml_.size() || xml_[pos_] != '=') {
        throw std::runtime_error("XES attribute '" + attr_name +
                                 "' is missing a value");
      }
      ++pos_;  // '='
      skip_space();
      if (pos_ >= xml_.size() || (xml_[pos_] != '"' && xml_[pos_] != '\'')) {
        throw std::runtime_error("XES attribute '" + attr_name +
                                 "' value is not quoted");
      }
      const char quote = xml_[pos_++];
      const auto value_start = pos_;
      const auto value_end = xml_.find(quote, pos_);
      if (value_end == std::string_view::npos) {
        throw std::runtime_error("XES attribute '" + attr_name +
                                 "' has an unterminated value");
      }
      auto value =
          decode_entities(xml_.substr(value_start, value_end - value_start));
      pos_ = value_end + 1;
      token.attributes.emplace_back(std::move(attr_name), std::move(value));
    }
    return token;
  }

  std::string_view xml_;
  std::size_t pos_ = 0;
};

const std::string* attribute(
    const std::vector<std::pair<std::string, std::string>>& attributes,
    std::string_view name) {
  for (const auto& [key, value] : attributes) {
    if (key == name) {
      return &value;
    }
  }
  return nullptr;
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

  XmlPullParser parser(xml);
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

  std::size_t input_order = 0;
  bool saw_timestamp = false;

  const auto finish_event = [&]() {
    if (current_event.activity.empty()) {
      throw std::runtime_error(
          "XES event is missing its activity attribute ('" +
          options.activity_attribute + "')");
    }
    current_event.input_order = input_order++;
    current_trace.events.push_back(std::move(current_event));
    current_event = Event{};
    in_event = false;
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
    if (token.kind == XmlPullParser::Kind::EndOfInput) {
      break;
    }

    if (token.kind == XmlPullParser::Kind::StartElement) {
      const std::string parent = stack.empty() ? std::string{} : stack.back();

      if (token.name == "trace" && parent == "log") {
        if (in_trace) {
          throw std::runtime_error("XES input nests a <trace> inside a <trace>");
        }
        in_trace = true;
      } else if (token.name == "event" && in_trace && parent == "trace") {
        in_event = true;
      } else if (in_event && parent == "event") {
        if (const auto* key = attribute(token.attributes, "key")) {
          if (const auto* value = attribute(token.attributes, "value")) {
            if (*key == options.activity_attribute) {
              current_event.activity = *value;
            } else if (*key == options.timestamp_attribute) {
              current_event.timestamp = *value;
              saw_timestamp = true;
            } else if (*key == options.resource_attribute) {
              current_event.resource = *value;
            }
          }
        }
      } else if (in_trace && !in_event && parent == "trace") {
        if (const auto* key = attribute(token.attributes, "key")) {
          if (*key == options.case_attribute) {
            if (const auto* value = attribute(token.attributes, "value")) {
              case_id = *value;
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
