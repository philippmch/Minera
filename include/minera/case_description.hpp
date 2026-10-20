#pragma once
#include "minera/llm_context.hpp"
#include <stdexcept>
#include <utility>

namespace minera {
// A supplied case is an ordered dictionary of values, followed by ordered
// event dictionaries. It has no synthesized case identifier or field aliases.
// Sequence members are items; dictionary items alternate key and value.
enum class CaseValueKind { kNull, kString, kBoolean, kInteger, kFloat, kDate, kBytes, kList, kTuple, kDictionary };
struct CaseDescriptionValue {
 CaseValueKind kind=CaseValueKind::kNull;
 std::string text; // String, decimal integer, lexical datetime, or raw bytes.
 bool boolean=false;
 double real=0;
 std::vector<CaseDescriptionValue> items;
};
using CaseDescriptionAttributes=std::vector<std::pair<std::string,CaseDescriptionValue>>;
struct CaseDescription {
 CaseDescriptionAttributes attributes;
 std::vector<CaseDescriptionAttributes> events;
};
struct CaseDescriptionOptions {
 bool include_case_attributes=true,include_event_attributes=true,include_timestamp=true,include_header=true;
 std::string activity_key="concept:name",timestamp_key="time:timestamp";
};
class CaseDescriptionKeyError:public std::out_of_range {
 public: explicit CaseDescriptionKeyError(std::string key);
 const std::string key;
};
// Direct original-case formatting: no discovery, projection, truncation,
// character budget or result byte cap. The supplied dictionaries stay intact.
[[nodiscard]] LlmContextResult abstract_case(const CaseDescription&,const CaseDescriptionOptions& = {});
} // namespace minera
