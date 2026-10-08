#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "minera/event_log.hpp"
#include "minera/petri_net.hpp"
#include "minera/pnml.hpp"
#include "minera/ocel.hpp"

namespace minera {
struct LlmContextOptions {
 std::string variant="dfg";
 std::size_t max_len=10000;
 std::size_t case_index=0;
 bool include_header=true,include_timestamps=true,include_case_attributes=true,include_event_attributes=true;
 bool include_performance=true,relative_frequency=false,include_lifecycle_paths=true;
 std::string primary_aggregation="mean",secondary_aggregation;
 std::string activity_attribute,timestamp_attribute,object_type;
};
struct LlmContextResult {std::string variant,text;std::size_t characters=0;bool truncated=false;};
[[nodiscard]] std::vector<std::string> llm_context_variants();
[[nodiscard]] bool llm_context_is_ocel(std::string_view variant);
[[nodiscard]] LlmContextResult build_llm_context(const EventLog& log,const LlmContextOptions& options={},const PetriNet* net=nullptr,const PnmlDocument* document=nullptr);
[[nodiscard]] LlmContextResult build_llm_context(const Ocel& log,const LlmContextOptions& options);
[[nodiscard]] std::string llm_context_json(const LlmContextResult& result,std::string_view source_id={});
} // namespace minera
