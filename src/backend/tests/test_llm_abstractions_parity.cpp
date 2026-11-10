// LLM log abstractions against the public capture
// (docs/llm-abstractions-bank.json.gz, written to
// src/backend/tests/fixtures/llm_abstractions_bank.txt). Seeded logs go
// through the public column, DFG, variant, feature, event-stream and
// log-skeleton descriptions under default, budget, header, frequency and
// performance options; every text must match byte for byte, or the call must
// raise. The skeleton relation pairs follow Python set order, which moves with
// the hash seed, so each relation line is compared as a set of pairs.
#include "minera/csv_event_log_importer.hpp"
#include "minera/event_log.hpp"
#include "minera/llm_abstractions.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace minera;

std::vector<std::string> split(const std::string& text, char separator) {
  std::vector<std::string> parts;
  std::string part;
  std::istringstream stream(text);
  while (std::getline(stream, part, separator)) parts.push_back(part);
  if (!text.empty() && text.back() == separator) parts.emplace_back();
  return parts;
}

std::string unescape(const std::string& text) {
  std::string out;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\\' && i + 1 < text.size()) {
      out += text[++i] == 'n' ? '\n' : text[i];
    } else {
      out += text[i];
    }
  }
  return out;
}

struct Call {
  std::string name;
  std::optional<std::string> error;
  std::vector<std::string> observed;
};

struct Case {
  std::string name;
  EventLog log;
  std::vector<Call> calls;
};

std::string timestamp(int second) {
  char text[32];
  std::snprintf(text, sizeof text, "2024-01-%02dT%02d:%02d:%02dZ", 1 + second / 86400, second / 3600 % 24,
                second / 60 % 60, second % 60);
  return text;
}

EventAttribute typed(std::string key, const std::string& token) {
  const bool integral = token.find_first_of(".e") == std::string::npos;
  EventAttribute attribute{std::move(key), token, integral ? AttributeKind::kInt : AttributeKind::kFloat};
  if (integral)
    attribute.typed_value = static_cast<std::int64_t>(std::stoll(token));
  else
    attribute.typed_value = std::stod(token);
  return attribute;
}

EventAttribute boolean(std::string key, bool value) {
  EventAttribute attribute{std::move(key), value ? "true" : "false", AttributeKind::kBoolean};
  attribute.typed_value = value;
  return attribute;
}

void add_trace(EventLog& log, const std::vector<std::string>& tokens) {
  Trace trace;
  trace.case_id = "c" + std::to_string(log.traces.size());
  trace.case_id_state = FirstClassAttributeState::kPresent;
  trace.attributes.push_back({"region", tokens.at(1), AttributeKind::kString});
  trace.attributes.push_back(typed("priority", tokens.at(2)));
  trace.attributes.push_back(boolean("urgent", tokens.at(3) == "1"));
  for (std::size_t index = 4; index < tokens.size(); ++index) {
    const auto fields = split(tokens[index], ';');
    Event event{trace.case_id, fields.at(0), timestamp(std::stoi(fields.at(1))), fields.at(2), trace.events.size()};
    event.activity_state = event.timestamp_state = event.resource_state = FirstClassAttributeState::kPresent;
    event.attributes.push_back(typed("level", fields.at(3)));
    if (fields.at(4) != "-") event.attributes.push_back(typed("amount", fields[4]));
    if (fields.at(5) != "-") event.attributes.push_back({"kind", fields[5], AttributeKind::kString});
    if (fields.at(6) != "-") event.attributes.push_back(boolean("flag", fields[6] == "1"));
    if (fields.at(7) != "-") {
      if (fields[7].starts_with("s:"))
        event.attributes.push_back({"code", fields[7].substr(2), AttributeKind::kString});
      else
        event.attributes.push_back(typed("code", fields[7].substr(2)));
    }
    trace.events.push_back(std::move(event));
  }
  log.traces.push_back(std::move(trace));
}

std::vector<Case> load(const char* path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error(std::string("cannot read ") + path);
  std::vector<Case> cases;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#') continue;
    const auto tag = line.substr(0, 1);
    const auto rest = line.size() > 2 ? line.substr(2) : std::string();
    if (tag == "C") {
      cases.push_back({rest, {}, {}});
    } else if (tag == "T") {
      add_trace(cases.back().log, split(line, ' '));
    } else if (tag == "K") {
      cases.back().calls.push_back({rest, std::nullopt, {}});
    } else if (tag == "X") {
      cases.back().calls.back().error = rest;
    } else if (tag == "O") {
      cases.back().calls.back().observed.push_back(unescape(rest));
    }
  }
  return cases;
}

LlmAbstractionOptions options_for(const std::string& call) {
  LlmAbstractionOptions options;
  if (call == "cols-short") options.max_len = 420;
  if (call == "dfg-frequency" || call == "variants-frequency") options.include_performance = false;
  if (call == "dfg-relative" || call == "variants-relative") options.relative_frequency = true;
  if (call == "dfg-aggregations") {
    options.primary_performance_aggregation = "median";
    options.secondary_performance_aggregation = "max";
  }
  if (call == "variants-aggregations") {
    options.primary_performance_aggregation = "median";
    options.secondary_performance_aggregation = "stdev";
  }
  if (call == "dfg-short" || call == "variants-short") {
    options.max_len = 150;
    options.response_header = false;
  }
  if (call == "features-short") {
    options.max_len = 600;
    options.include_header = false;
  }
  if (call == "stream-short") {
    options.max_len = 400;
    options.response_header = false;
  }
  if (call == "skeleton-noheader") options.include_header = false;
  return options;
}

// Empty when the native text matches an observation, otherwise a description.
template <typename Describe, typename Normalize>
std::string text_case(const Call& call, Describe describe, Normalize normalize) {
  std::string text;
  try {
    text = describe();
  } catch (const std::exception& error) {
    return call.error ? "" : std::string("raised ") + error.what();
  }
  if (call.error) return "expected " + *call.error;
  for (const auto& observed : call.observed)
    if (normalize(observed) == normalize(text)) return "";
  const auto& expected = call.observed.front();
  std::size_t at = 0;
  while (at < expected.size() && at < text.size() && expected[at] == text[at]) ++at;
  return "text differs at byte " + std::to_string(at) + ": expected [" + expected.substr(at, 80) + "] got [" +
         text.substr(at, 80) + "]";
}

std::string same(const std::string& text) { return text; }

// Relation pairs sorted within each relation line.
std::string pair_sets(const std::string& text) {
  std::string out;
  for (const auto& line : split(text, '\n')) {
    const auto colon = line.find(": ");
    const auto label = colon == std::string::npos ? std::string() : line.substr(0, colon);
    if (label != "Equivalence" && label != "Always Before" && label != "Always After" && label != "Never Together" &&
        label != "Directly-Follows Constraints") {
      out += line + '\n';
      continue;
    }
    auto pairs = split(line.substr(colon + 2), '(');
    for (auto& pair : pairs)
      while (!pair.empty() && pair.back() == ' ') pair.pop_back();
    std::sort(pairs.begin(), pairs.end());
    out += label + ":";
    for (const auto& pair : pairs) out += pair + "|";
    out += '\n';
  }
  return out;
}

std::string columns_case(const EventLog& log, const Call& call) {
  return text_case(call, [&] { return llm_log_columns_description(log, options_for(call.name).max_len); }, same);
}

std::string dfg_case(const EventLog& log, const Call& call) {
  return text_case(call, [&] { return llm_log_dfg_description(log, options_for(call.name)); }, same);
}

std::string variants_case(const EventLog& log, const Call& call) {
  return text_case(call, [&] { return llm_log_variants_description(log, options_for(call.name)); }, same);
}

std::string features_case(const EventLog& log, const Call& call) {
  return text_case(call, [&] { return llm_log_features_description(log, options_for(call.name)); }, same);
}

std::string stream_case(const EventLog& log, const Call& call) {
  return text_case(call, [&] { return llm_event_stream_description(log, options_for(call.name)); }, same);
}

std::string skeleton_case(const EventLog& log, const Call& call) {
  return text_case(call, [&] { return llm_log_skeleton_description(log, options_for(call.name)); }, pair_sets);
}

// Review AV1: pm4py 2.7.22's abstract_event_stream, abstract_log_attributes
// and abstract_variants on read_csv + format_dataframe(case_id="cid",
// activity_key="act", timestamp_key="ts"): the frame's typed cells in its
// column order, its dtypes deciding the column descriptions, and a one-case
// stdev variant failing only once printed.
void review_av_frame_descriptions() {
  const auto upload = [](const std::string& text) {
    std::istringstream csv(text);
    CsvImportOptions import;
    import.case_id_column = "cid";
    import.activity_column = "act";
    import.timestamp_column = "ts";
    import.resource_column = "who";
    return import_csv_event_log(csv, import);
  };
  const auto typed = upload("cid,act,ts,who,n,f,flag,when\nc1,a,2020-01-01 10:00:00,R,1,1.5,True,2020-03-01 10:00:00+02:00\nc1,b,2020-01-01 11:00:00,S,2,,False,\nc2,a,2020-01-02 10:00:00,R,3,2.5,True,2020-03-02T10:00:00Z\n");
  LlmAbstractionOptions options;
  if (llm_event_stream_description(typed, options) != "\n\nIf I have the following stream of events:\n\n{'cid': 'c1', 'act': 'a', 'ts': Timestamp('2020-01-01 10:00:00+0000', tz='UTC'), 'who': 'R', 'n': 1, 'f': 1.5, 'flag': True, 'when': Timestamp('2020-03-01 08:00:00+0000', tz='UTC'), 'case:concept:name': 'c1', 'concept:name': 'a', 'time:timestamp': Timestamp('2020-01-01 10:00:00+0000', tz='UTC'), '@@index': 0, '@@case_index': 0}\n{'cid': 'c1', 'act': 'b', 'ts': Timestamp('2020-01-01 11:00:00+0000', tz='UTC'), 'who': 'S', 'n': 2, 'flag': False, 'case:concept:name': 'c1', 'concept:name': 'b', 'time:timestamp': Timestamp('2020-01-01 11:00:00+0000', tz='UTC'), '@@index': 1, '@@case_index': 0}\n{'cid': 'c2', 'act': 'a', 'ts': Timestamp('2020-01-02 10:00:00+0000', tz='UTC'), 'who': 'R', 'n': 3, 'f': 2.5, 'flag': True, 'when': Timestamp('2020-03-02 10:00:00+0000', tz='UTC'), 'case:concept:name': 'c2', 'concept:name': 'a', 'time:timestamp': Timestamp('2020-01-02 10:00:00+0000', tz='UTC'), '@@index': 2, '@@case_index': 1}\n\n") {
    std::fprintf(stderr, "AV1 stream: %s\n", llm_event_stream_description(typed, options).c_str());
    std::exit(1);
  }
  if (llm_log_columns_description(typed) != "concept:name  empty: 0 values: (a; freq. 2) (b; freq. 1)\nf  empty: 1  quantiles: {0.0: 1.5, 0.25: 1.75, 0.5: 2.0, 0.75: 2.25, 1.0: 2.5}\nflag  empty: 0 values: (True; freq. 2) (False; freq. 1)\ntime:timestamp  empty: 0  quantiles: {0.0: Timestamp('2020-01-01 10:00:00+0000', tz='UTC'), 0.25: Timestamp('2020-01-01 10:30:00+0000', tz='UTC'), 0.5: Timestamp('2020-01-01 11:00:00+0000', tz='UTC'), 0.75: Timestamp('2020-01-01 22:30:00+0000', tz='UTC'), 1.0: Timestamp('2020-01-02 10:00:00+0000', tz='UTC')}\nts  empty: 0  quantiles: {0.0: Timestamp('2020-01-01 10:00:00+0000', tz='UTC'), 0.25: Timestamp('2020-01-01 10:30:00+0000', tz='UTC'), 0.5: Timestamp('2020-01-01 11:00:00+0000', tz='UTC'), 0.75: Timestamp('2020-01-01 22:30:00+0000', tz='UTC'), 1.0: Timestamp('2020-01-02 10:00:00+0000', tz='UTC')}\nwhen  empty: 1  quantiles: {0.0: Timestamp('2020-03-01 08:00:00+0000', tz='UTC'), 0.25: Timestamp('2020-03-01 14:30:00+0000', tz='UTC'), 0.5: Timestamp('2020-03-01 21:00:00+0000', tz='UTC'), 0.75: Timestamp('2020-03-02 03:30:00+0000', tz='UTC'), 1.0: Timestamp('2020-03-02 10:00:00+0000', tz='UTC')}") {
    std::fprintf(stderr, "AV1 columns: %s\n", llm_log_columns_description(typed).c_str());
    std::exit(1);
  }
  const auto variants = upload("cid,act,ts,who\nc1,a,2020-01-01 10:00:00,R\nc1,b,2020-01-01 11:00:00,S\nc2,a,2020-01-02 10:00:00,R\nc2,b,2020-01-02 12:00:00,R\nc3,a,2020-01-03 10:00:00,R\n");
  options.primary_performance_aggregation = "stdev";
  options.max_len = 100;
  if (llm_log_variants_description(variants, options) != "If I have a process with the following process variants:\n\n a -> b ( frequency = 2  performance = 1800.000 )\n\n\n") {
    std::fprintf(stderr, "AV1 variants: %s\n", llm_log_variants_description(variants, options).c_str());
    std::exit(1);
  }
  options.max_len = 110;
  bool raised = false;
  try {
    (void)llm_log_variants_description(variants, options);
  } catch (const std::invalid_argument&) {
    raised = true;
  }
  if (!raised) {
    std::fprintf(stderr, "AV1 variants: a printed one-case stdev must fail\n");
    std::exit(1);
  }
}


}  // namespace

int main(int argc, char** argv) {
  review_av_frame_descriptions();
  const char* path = argc > 1 ? argv[1] : "src/backend/tests/fixtures/llm_abstractions_bank.txt";
  int failures = 0;
  std::size_t compared = 0, errors = 0;
  std::map<std::string, std::size_t> failed_by_call;
  for (const auto& item : load(path)) {
    for (const auto& call : item.calls) {
      ++compared;
      if (call.error) ++errors;
      const auto family = call.name.substr(0, call.name.find('-'));
      std::string detail;
      try {
        detail = family == "cols"       ? columns_case(item.log, call)
                 : family == "dfg"      ? dfg_case(item.log, call)
                 : family == "variants" ? variants_case(item.log, call)
                 : family == "features" ? features_case(item.log, call)
                 : family == "stream"   ? stream_case(item.log, call)
                                        : skeleton_case(item.log, call);
      } catch (const std::exception& error) {
        detail = std::string("threw ") + error.what();
      }
      if (!detail.empty()) {
        ++failures;
        ++failed_by_call[call.name];
        if (failures <= 30)
          std::fprintf(stderr, "%s %s: %s\n", item.name.c_str(), call.name.c_str(), detail.c_str());
      }
    }
  }
  for (const auto& [call, count] : failed_by_call) std::fprintf(stderr, "%s: %zu failing\n", call.c_str(), count);
  std::printf("%zu calls compared (%zu public errors)\n", compared, errors);
  if (compared < 2700 || errors < 60) {
    ++failures;
    std::fprintf(stderr, "bank coverage below the pinned floor\n");
  }
  return failures == 0 ? 0 : 1;
}
