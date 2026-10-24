#include "minera/log_timestamp_filter_workbench.hpp"
#include "minera/xes_export.hpp"
#include "log_timestamp_original_call_parser.hpp"
#include <map>

namespace minera {
namespace {
std::string timestamp_counts(const EventLog& log) {
  return "{\"traces\":" + std::to_string(log.trace_count()) + ",\"events\":" + std::to_string(log.event_count()) + '}';
}
}
DiscoveryWorkbenchResponse execute_log_timestamp_filter(const std::vector<DiscoveryWorkbenchPart>& parts,
    const std::vector<DiscoveryWorkbenchPart>& query, std::string_view transport_error) {
  namespace original = log_timestamp_original_call_detail;
  std::map<std::string, const DiscoveryWorkbenchPart*> fields;
  std::optional<EventLog> source, result;
  std::optional<original::Call> call;
  std::optional<bool> bool_result, unchanged, controls_unchanged;
  std::optional<std::string> raw_call, before, after, controls_before, controls_after, raw_result, value_json, xes, xes_error, projection, inspection_error;
  std::string operation, mode, format, phase = "control", error_type, error, result_kind;
  int status = 200;
  bool native_returned = false, primary_native_error = false, capture_stage = false;
  const auto inspect_failure = [&](std::string_view context, const std::exception& failure) {
    if (inspection_error) *inspection_error += '\n'; else inspection_error.emplace();
    *inspection_error += std::string(context) + ": " + failure.what();
    status = 500;
    if (!primary_native_error) { error_type = "internal_error"; error = *inspection_error; }
  };
  try {
    if (!transport_error.empty()) throw std::invalid_argument(std::string(transport_error));
    if (!query.empty()) throw std::invalid_argument("log timestamp controls require multipart fields; query fields are inactive");
    static const std::set<std::string> allowed = {"operation", "source_file", "call_json"};
    for (const auto& part : parts) {
      if (!allowed.contains(part.name)) throw std::invalid_argument("unknown log timestamp control: " + part.name);
      if (part.file && part.name != "source_file" && part.filename != part.name + ".txt") throw std::invalid_argument("log timestamp controls require text or canonical byte Files: " + part.name);
      if (!fields.emplace(part.name, &part).second) throw std::invalid_argument("duplicate log timestamp control: " + part.name);
    }
    if (fields.contains("call_json")) { raw_call = fields.at("call_json")->bytes; mode = "original"; }
    if (fields.contains("operation")) operation = fields.at("operation")->bytes;
    if (fields.size() != 3) throw std::invalid_argument("log timestamp filtering requires exactly operation, source_file and call_json");
    call = original::parse(*raw_call, operation);
    const auto& file = *fields.at("source_file");
    if (!file.file) throw std::invalid_argument("source_file requires an uploaded native log File");
    phase = "source_import"; format = "native_log";
    source = parse_embedding_workbench_raw_log_json(file.bytes);
    phase = "control";
    capture_stage = true;
    before = serialize_embedding_workbench_raw_log_json(*source);
    capture_stage = false;
    original::bind(*call, *source);
    capture_stage = true;
    projection = original::projection_json(*call, operation);
    controls_before = original::controls_json(*call, *source);
    capture_stage = false;
    phase = "native_call";
    auto& c = *call;
    const auto& a = c.args;
    if (c.spec.predicate) {
      const Trace& selected = source->traces.at(*c.source_trace_index);
      if (operation == "is_contained") bool_result = log_timestamp_is_contained(selected, a[0], a[1], a[2]);
      else if (operation == "is_intersecting") bool_result = log_timestamp_is_intersecting(selected, a[0], a[1], a[2]);
      else if (operation == "trace_attr_is_contained") bool_result = log_timestamp_trace_attribute_is_contained(selected, a[0], a[1], a[2]);
      else bool_result = log_timestamp_has_attribute_in_timeframe(selected, a[0], a[1], a[2], a[3], a[4]);
      native_returned = true; result_kind = "bool";
    } else {
      if (operation == "apply") result = apply_log_timestamp_filter(*source);
      else if (operation == "apply_auto_filter") result = apply_log_timestamp_auto_filter(*source);
      else if (operation == "apply_events") result = filter_log_timestamp_events(*source, a[0], a[1], *c.options);
      else if (operation == "filter_traces_contained") result = filter_log_timestamp_contained(*source, a[0], a[1], *c.options);
      else if (operation == "filter_traces_intersecting") result = filter_log_timestamp_intersecting(*source, a[0], a[1], *c.options);
      else if (operation == "filter_traces_starting_in_timeframe") result = filter_log_timestamp_starting(*source, a[0], a[1], *c.options);
      else if (operation == "filter_traces_completing_in_timeframe") result = filter_log_timestamp_completing(*source, a[0], a[1], *c.options);
      else if (operation == "filter_on_trace_attribute") result = filter_log_timestamp_trace_attribute(*source, a[0], a[1], *c.options);
      else result = filter_log_timestamp_attribute(*source, a[0], a[1], a[2], a[3], *c.options);
      native_returned = true; result_kind = "log";
    }
    phase = "result_export";
    if (result) {
      raw_result = serialize_embedding_workbench_raw_log_json(*result);
      try { XesExportOptions options; options.preserve_nulls = true; xes = serialize_xes(*result, options); }
      catch (const std::exception& failure) { xes_error = failure.what(); }
    } else {
      EventAttribute value{"", *bool_result ? "true" : "false", AttributeKind::kBoolean}; value.typed_value = *bool_result;
      value_json = original::cell_json(value);
    }
  } catch (const LogTimestampFilterError& failure) {
    if (phase == "native_call") { status = 400; primary_native_error = true; error_type = failure.type; error = failure.message; }
    else if (capture_stage) inspect_failure("before capture", failure);
    else { status = phase == "result_export" ? 500 : 400; error_type = status == 500 ? "internal_error" : "control_error"; error = failure.what(); }
  } catch (const std::exception& failure) {
    if (phase == "native_call") { status = 400; primary_native_error = true; error_type = "native_error"; error = failure.what(); }
    else if (capture_stage) inspect_failure("before capture", failure);
    else { status = phase == "result_export" ? 500 : 400; error_type = status == 500 ? "internal_error" : "control_error"; error = failure.what(); }
  }
  // These captures are deliberately independent. An error cannot hide a prior source mutation.
  if (source) {
    try { after = serialize_embedding_workbench_raw_log_json(*source); if (before) unchanged = *before == *after; }
    catch (const std::exception& failure) { inspect_failure("source after capture", failure); }
  }
  if (source && call && call->bound) {
    try { controls_after = original::controls_json(*call, *source); if (controls_before) controls_unchanged = *controls_before == *controls_after; }
    catch (const std::exception& failure) { inspect_failure("controls after capture", failure); }
  }
  if (status == 200) phase = "complete";
  const auto text = [](const std::string& value) { return value.empty() ? std::string("null") : original::q(value); };
  const auto optional = [](const std::optional<std::string>& value) { return value ? original::q(*value) : std::string("null"); };
  const auto boolean = [](const std::optional<bool>& value) { return value ? (*value ? "true" : "false") : "null"; };
  return {status, "{\"family\":\"REFERENCE_LOG_TIMESTAMP\",\"operation\":" + text(operation) + ",\"http_status\":" + std::to_string(status) + ",\"phase\":" + original::q(phase) +
    ",\"source_format\":" + text(format) + ",\"source\":" + (source ? timestamp_counts(*source) : "null") + ",\"result\":" + (result ? timestamp_counts(*result) : "null") +
    ",\"source_unchanged\":" + boolean(unchanged) + ",\"call_mode\":" + text(mode) + ",\"call_json\":" + optional(raw_call) +
    ",\"controls_before_json\":" + optional(controls_before) + ",\"controls_after_json\":" + optional(controls_after) + ",\"controls_unchanged\":" + boolean(controls_unchanged) +
    ",\"source_log_json\":" + optional(before) + ",\"source_after_log_json\":" + optional(after) + ",\"filtered_log_json\":" + optional(raw_result) +
    ",\"filtered_xes\":" + optional(xes) + ",\"xes_error\":" + optional(xes_error) + ",\"value_json\":" + optional(value_json) +
    ",\"error_type\":" + (status == 200 ? "null" : original::q(error_type)) + ",\"error\":" + (status == 200 ? "null" : original::q(error)) +
    ",\"native_returned\":" + (native_returned ? "true" : "false") + ",\"result_kind\":" + text(result_kind) + ",\"call_projection_json\":" + optional(projection) + ",\"inspection_error\":" + optional(inspection_error) + '}'};
}
} // namespace minera
