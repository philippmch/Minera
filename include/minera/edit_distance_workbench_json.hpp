#pragma once
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "minera/edit_distance_alignment.hpp"
namespace minera {
// Transport parts keep original ordering, duplicate names and immutable bytes.
struct EditDistanceWorkbenchPart { std::string name, filename, bytes; bool file = false; };
struct EditDistanceWorkbenchResponse { int status = 200; std::string json; };
[[nodiscard]] EditDistanceOptions parse_edit_distance_workbench_options_json(std::string_view, std::set<std::string>* supplied = nullptr);
[[nodiscard]] std::string serialize_edit_distance_workbench_options_json(const EditDistanceOptions&);
[[nodiscard]] std::vector<EditDistanceAlignment> parse_edit_distance_workbench_alignments_json(std::string_view);
[[nodiscard]] std::vector<EditDistanceTypedAlignment> parse_edit_distance_workbench_typed_alignments_json(std::string_view);
[[nodiscard]] std::string serialize_edit_distance_workbench_alignments_json(const std::vector<EditDistanceAlignment>&);
[[nodiscard]] std::string serialize_edit_distance_workbench_alignments_json(const std::vector<EditDistanceTypedAlignment>&);
// Request-local owning constructors, finite cache handles and actual public API calls.
// No global application/session state is accessed or mutated.
[[nodiscard]] EditDistanceWorkbenchResponse execute_edit_distance_workbench(const std::vector<EditDistanceWorkbenchPart>&);
// Raw multipart framing retains physical part order and filename presence.
[[nodiscard]] std::vector<EditDistanceWorkbenchPart> parse_edit_distance_workbench_multipart(std::string_view content_type,std::string_view body);
[[nodiscard]] EditDistanceWorkbenchResponse execute_edit_distance_workbench_multipart(std::string_view content_type,std::string_view body,const std::vector<EditDistanceWorkbenchPart>& query = {},bool transport_complete = true);
} // namespace minera
