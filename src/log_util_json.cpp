#include "minera/log_util_json.hpp"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace minera {
namespace {

void write_json_string(std::ostream& out, std::string_view value) {
  out << '"';
  for (const unsigned char ch : value) {
    switch (ch) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (ch < 0x20) {
          out << "\\u00" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(ch) << std::dec << std::setfill('0');
        } else {
          out << static_cast<char>(ch);
        }
    }
  }
  out << '"';
}

const char* attribute_kind_name(AttributeKind kind) {
  switch (kind) {
    case AttributeKind::kString: return "string";
    case AttributeKind::kDate: return "date";
    case AttributeKind::kInt: return "int";
    case AttributeKind::kFloat: return "float";
    case AttributeKind::kBoolean: return "boolean";
    case AttributeKind::kId: return "id";
  }
  return "string";
}

void write_attributes(std::ostream& out,
                      const std::vector<EventAttribute>& attributes) {
  out << '[';
  for (std::size_t i = 0; i < attributes.size(); ++i) {
    if (i != 0) out << ',';
    const auto& attribute = attributes[i];
    out << "{\"key\":";
    write_json_string(out, attribute.key);
    out << ",\"value\":";
    write_json_string(out, attribute.value);
    out << ",\"kind\":";
    write_json_string(out, attribute_kind_name(attribute.kind));
    out << '}';
  }
  out << ']';
}

void write_event_log(std::ostream& out, const EventLog& log) {
  out << "{\"attributes\":";
  write_attributes(out, log.attributes);
  out << ",\"traces\":[";
  for (std::size_t i = 0; i < log.traces.size(); ++i) {
    if (i != 0) out << ',';
    const auto& trace = log.traces[i];
    out << "{\"case_id\":";
    write_json_string(out, trace.case_id);
    out << ",\"attributes\":";
    write_attributes(out, trace.attributes);
    out << ",\"events\":[";
    for (std::size_t j = 0; j < trace.events.size(); ++j) {
      if (j != 0) out << ',';
      const auto& event = trace.events[j];
      out << "{\"case_id\":";
      write_json_string(out, event.case_id);
      out << ",\"activity\":";
      write_json_string(out, event.activity);
      out << ",\"timestamp\":";
      write_json_string(out, event.timestamp);
      out << ",\"resource\":";
      write_json_string(out, event.resource);
      out << ",\"lifecycle\":";
      write_json_string(out, event.lifecycle);
      out << ",\"input_order\":" << event.input_order
          << ",\"attributes\":";
      write_attributes(out, event.attributes);
      out << '}';
    }
    out << "]}";
  }
  out << "]}";
}

const char* variant_name(LogUtilityVariant variant) {
  switch (variant) {
    case LogUtilityVariant::kSplitTrainTest: return "SPLIT_TRAIN_TEST";
    case LogUtilityVariant::kFixedPrefixes: return "FIXED_PREFIXES";
    case LogUtilityVariant::kAllPrefixes: return "ALL_PREFIXES";
    case LogUtilityVariant::kUntilActivity: return "UNTIL_ACTIVITY";
  }
  return "SPLIT_TRAIN_TEST";
}

}  // namespace

std::string log_train_test_json(const LogTrainTestSplit& result) {
  std::ostringstream out;
  out << "{\"family\":\"LOG_UTIL\",\"variant\":\"SPLIT_TRAIN_TEST\",\"training\":";
  write_event_log(out, result.training);
  out << ",\"testing\":";
  write_event_log(out, result.testing);
  out << '}';
  return out.str();
}

std::string log_prefix_json(
    const EventLog& log, LogUtilityVariant variant,
    const std::vector<std::vector<std::size_t>>& change_indexes) {
  if (variant != LogUtilityVariant::kFixedPrefixes &&
      variant != LogUtilityVariant::kAllPrefixes) {
    throw std::invalid_argument("log prefix JSON requires a prefix variant");
  }
  if (variant == LogUtilityVariant::kAllPrefixes) {
    for (const auto& row : change_indexes) {
      for (const auto index : row) {
        if (index >= log.traces.size())
          throw std::invalid_argument("prefix change index is outside the log");
      }
    }
  }
  std::ostringstream out;
  out << "{\"family\":\"LOG_UTIL\",\"variant\":";
  write_json_string(out, variant_name(variant));
  out << ",\"log\":";
  write_event_log(out, log);
  if (variant == LogUtilityVariant::kAllPrefixes) {
    out << ",\"change_indexes\":[";
    for (std::size_t i = 0; i < change_indexes.size(); ++i) {
      if (i != 0) out << ',';
      out << '[';
      for (std::size_t j = 0; j < change_indexes[i].size(); ++j) {
        if (j != 0) out << ',';
        out << change_indexes[i][j];
      }
      out << ']';
    }
    out << ']';
  }
  out << '}';
  return out.str();
}

std::string log_until_activity_json(const LogUntilActivityResult& result) {
  if (result.log.traces.size() != result.elapsed_seconds.size()) {
    throw std::invalid_argument("until-activity durations do not match the log");
  }
  std::ostringstream out;
  out << "{\"family\":\"LOG_UTIL\",\"variant\":\"UNTIL_ACTIVITY\",\"log\":";
  write_event_log(out, result.log);
  out << ",\"elapsed_seconds\":[" << std::setprecision(17);
  for (std::size_t i = 0; i < result.elapsed_seconds.size(); ++i) {
    if (i != 0) out << ',';
    out << result.elapsed_seconds[i];
  }
  out << "]}";
  return out.str();
}

}  // namespace minera
