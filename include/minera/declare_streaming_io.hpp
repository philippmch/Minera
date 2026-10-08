#pragma once
#include <string>
#include <string_view>
#include "minera/declare_streaming.hpp"
namespace minera {
// Flat named rules or a complete Minera export with activity IDs and declare.
[[nodiscard]] DeclareModel parse_declare_stream_model_json(std::string_view json);
[[nodiscard]] std::string serialize_declare_stream_model_json(const DeclareModel& model);
[[nodiscard]] std::string serialize_declare_streaming_json(const DeclareStreamingSnapshot& snapshot,const DeclareModel& model);
[[nodiscard]] std::string serialize_declare_stream_event_json(const DeclareStreamingEventResult& event);
}
