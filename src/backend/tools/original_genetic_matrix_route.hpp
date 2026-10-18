#pragma once
// Included in the server namespace after active-source byte/field helpers.
// Every value is request-owned; no cached session/model/source is consulted.
void handle_original_genetic_matrix_workbench(const httplib::Request& request,
                                               httplib::Response& response) {
 using namespace minera;
 // Preserve the existing scalar spelling only if JSON decoding restores the
 // identical bytes. active_source_safe_json alone only repairs WTF8 triples.
 const auto scalar = [&](std::string_view bytes) -> std::optional<std::string> {
  try {
   // The repaired writer tests quoted roundtrips with its unbounded output
   // parser. Never apply the bounded request parser to exporter output here.
   GeneticCausalMatrix holder; holder.activities.emplace_back(bytes);
   const auto encoded = serialize_genetic_matrix_workbench_model_json(holder);
   constexpr std::string_view prefix = "{\"activities\":[";
   if (encoded.starts_with(prefix) && encoded[prefix.size()] == '"') return active_source_quote(bytes);
  } catch (const std::invalid_argument&) { /* An exact byte tag remains valid. */ }
  return std::nullopt;
 };
 const auto q = [&](std::string_view bytes) { const auto text = scalar(bytes); return text ? *text : serialize_embedding_workbench_string_json(bytes); };
 const auto flag = [](bool value) { return value ? "true" : "false"; };
 const auto exception_name = [](const std::exception& error) {
  return dynamic_cast<const std::invalid_argument*>(&error) ? "invalid_argument" :
      dynamic_cast<const std::overflow_error*>(&error) ? "overflow_error" :
      dynamic_cast<const std::out_of_range*>(&error) ? "out_of_range" :
      dynamic_cast<const std::length_error*>(&error) ? "length_error" :
      dynamic_cast<const std::runtime_error*>(&error) ? "runtime_error" : "exception";
 };
 const auto error_value = [&](std::string_view type, const std::exception& error) {
  return "{\"type\":" + q(type) + ",\"message\":" + q(error.what()) + ",\"exception_type\":" + q(exception_name(error)) + '}';
 };
 const auto encoded = [](std::string_view bytes) {
  bool text = true;
  for (std::size_t i = 0; i < bytes.size() && text;) {
   const auto a = static_cast<unsigned char>(bytes[i++]); if (a < 128) continue;
   const int n = a >= 0xc2 && a <= 0xdf ? 1 : a >= 0xe0 && a <= 0xef ? 2 : a >= 0xf0 && a <= 0xf4 ? 3 : -1;
   if (n < 0 || i + static_cast<std::size_t>(n) > bytes.size()) { text = false; break; }
   unsigned code = a & (n == 1 ? 31 : n == 2 ? 15 : 7);
   for (int j = 0; j < n; ++j) { const auto b = static_cast<unsigned char>(bytes[i++]); if ((b & 192) != 128) { text = false; break; } code = (code << 6) | (b & 63); }
   if ((n == 1 && code < 128) || (n == 2 && code < 2048) || (n == 3 && code < 65536) || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) text = false;
  }
  std::string data(bytes);
  if (!text) { constexpr char hex[] = "0123456789abcdef"; data.clear(); for (const unsigned char c : bytes) { data += hex[c >> 4]; data += hex[c & 15]; } }
  return std::pair{text, std::move(data)};
 };
 const auto original = [&](std::string_view name, std::string_view bytes) {
  const auto [text, data] = encoded(bytes);
  return "{\"dataset\":" + q(name) + ",\"encoding\":" + q(text ? "utf8" : "hex") + ",\"data\":" + q(data) + ",\"bytes\":" + std::to_string(bytes.size()) + ",\"sha256\":" + q(footprints_workbench_sha256(bytes)) + '}';
 };
 try {
  if (!request.is_multipart_form_data()) throw std::invalid_argument("genetic matrix workbench requires multipart data");
  if (request.params.count("model_file") + request.files.count("model_file") > 1) throw std::invalid_argument("ambiguous duplicate genetic matrix upload: model_file");
  const auto supplied = [&](std::string_view name) { return active_source_supplied(request, name); };
  const auto field = [&](std::string_view name) { return active_source_field(request, name); };
  using Upload = std::decay_t<decltype(request.get_file_value("model_file"))>;
  const std::optional<Upload> upload = request.has_file("model_file") ? std::make_optional(request.get_file_value("model_file")) : std::nullopt;
  std::string raw_controls = "[";
  const auto control = [&](const std::string& name, const std::string& value) { if (raw_controls.size() > 1) raw_controls += ','; raw_controls += "{\"name\":" + q(name) + ",\"original\":" + original(name, value) + '}'; };
  for (const auto& [name, value] : request.params) control(name, value);
  for (const auto& [name, value] : request.files) if (name != "model_file") control(name, value.content);
  raw_controls += ']';
  const auto operation = supplied("operation") ? field("operation") : "convert";
  const auto format = supplied("model_format") ? field("model_format") : "exact";
  const auto options_text = supplied("options_json") ? field("options_json") : "{}";
  const std::string submitted_options = supplied("options_json") ? q(options_text) : "null";
  std::optional<GeneticCausalMatrix> model; std::optional<GeneticMatrixWorkbenchOptions> options; std::optional<GeneticMatrixConversionResult> result;
  std::set<std::string> model_members, option_members;
  std::string before = "null", after = "null", failure = "null", phase = "configuration_error", calls = "[", artifact_errors = "[", result_json = "null";
  bool executed = false, native_entered = false, native_returned = false;
  const auto invoke = [&](std::string_view name, std::string_view stage, const auto& call) {
   if (calls.size() > 1) calls += ',';
   const auto prefix = "{\"name\":" + q(name) + ",\"stage\":" + q(stage) + ",\"input_kind\":" + q(stage == "primary" ? "GeneticCausalMatrix" : "GeneticMatrixConversionResult");
   try { call(); calls += prefix + ",\"returned\":true,\"error\":null}"; }
   catch (const std::exception& error) { calls += prefix + ",\"returned\":false,\"error\":" + error_value(stage == "export" ? "export_error" : "native_error", error) + '}'; throw; }
  };
  const std::vector<std::string> legacy_artifacts = {"source_matrix_json", "normalized_matrix_json", "result_json", "result_pnml", "legacy_json"};
  const std::vector<std::pair<std::string, std::string>> names = {{"source_matrix_json", "source-matrix.json"}, {"normalized_matrix_json", "normalized-matrix.json"}, {"result_json", "result.json"}, {"result_pnml", "model.pnml"}, {"legacy_json", "legacy-result.json"}, {"net_constructor_json", "net-constructor.json"}, {"markings_json", "markings.json"}};
  std::map<std::string, std::optional<std::string>> exports;
  std::map<std::string, std::string> export_errors, byte_artifacts;
  for (const auto& [name, dataset] : names) { (void)dataset; byte_artifacts[name] = "null"; }
  for (const auto& name : legacy_artifacts) { exports[name] = std::nullopt; export_errors[name] = "null"; }
  const auto record_artifact_error = [&](const std::string& name, const std::exception& error) {
   const auto value = error_value("export_error", error);
   if (artifact_errors.size() > 1) artifact_errors += ',';
   artifact_errors += "{\"name\":" + q(name) + ",\"error\":" + value + '}';
   if (export_errors.contains(name)) export_errors[name] = value;
  };
  const auto retain_artifact = [&](const std::string& name, const std::string& value) {
   const auto found = std::find_if(names.begin(), names.end(), [&](const auto& item) { return item.first == name; });
   byte_artifacts[name] = original(found->second, value);
   if (exports.contains(name) && scalar(value)) exports[name] = value;
  };
  const auto artifact = [&](const std::string& name, const auto& make) { try { retain_artifact(name, make()); } catch (const std::exception& error) { record_artifact_error(name, error); } };
  try {
   const std::set<std::string> allowed = {"operation", "model_format", "options_json", "model_file"};
   const auto validate_control = [&](const std::string& name) {
    if (!allowed.contains(name)) throw std::invalid_argument("unknown genetic matrix control: " + name);
    if (request.params.count(name) + request.files.count(name) > 1) throw std::invalid_argument("ambiguous duplicate genetic matrix control: " + name);
   };
   for (const auto& [name, unused] : request.params) { (void)unused; validate_control(name); }
   for (const auto& [name, unused] : request.files) { (void)unused; validate_control(name); }
   if (operation != "convert") throw std::invalid_argument("unsupported genetic matrix operation: " + operation);
   if (format != "exact") throw std::invalid_argument("model_format must be exact");
   if (!upload) throw std::invalid_argument("model_file is required");
   options = parse_genetic_matrix_workbench_options_json(options_text, &option_members);
   phase = "preparation_error";
   model = parse_genetic_matrix_workbench_model_json(upload->content, &model_members);
   before = serialize_genetic_matrix_workbench_model_json(*model);
   phase = "native_error"; native_entered = true;
   invoke("convert_genetic_matrix(matrix, options.variant)", "primary", [&] { result = convert_genetic_matrix(*model, options->variant); native_returned = true; executed = true; });
  } catch (const std::exception& error) { failure = error_value(phase, error); }
  if (model) {
   try { after = serialize_genetic_matrix_workbench_model_json(*model); retain_artifact("source_matrix_json", after); }
   catch (const std::exception& error) { record_artifact_error("source_matrix_json", error); }
  }
  if (result) {
   try { const auto full = serialize_genetic_matrix_workbench_result_json(*result); result_json = "{\"type\":\"genetic_matrix_conversion\",\"value\":" + full + '}'; retain_artifact("result_json", full); }
   catch (const std::exception& error) { record_artifact_error("result_json", error); if (failure == "null") failure = error_value("result_serialization_error", error); }
   artifact("normalized_matrix_json", [&] { return serialize_genetic_matrix_workbench_model_json(result->matrix); });
   artifact("net_constructor_json", [&] { return serialize_oc_playout_workbench_petri_net_json(result->net); });
   artifact("markings_json", [&] { return serialize_genetic_matrix_workbench_markings_json(result->markings); });
   artifact("result_pnml", [&] { std::string value; invoke("to_pnml(result.net, result.markings.initial, result.markings.final)", "export", [&] { value = to_pnml(result->net, result->markings.initial, result->markings.final); }); return value; });
   artifact("legacy_json", [&] { std::string value; invoke("genetic_matrix_conversion_json(result)", "export", [&] { value = genetic_matrix_conversion_json(*result); }); return value; });
  }
  calls += ']'; artifact_errors += ']';
  const auto string_set = [&](const std::set<std::string>& values) { std::string json = "["; for (const auto& value : values) { if (json.size() > 1) json += ','; json += q(value); } return json + ']'; };
  std::string exports_json = "{";
  for (const auto& name : legacy_artifacts) { if (exports_json.size() > 1) exports_json += ','; exports_json += q(name) + ':' + (exports[name] ? *scalar(*exports[name]) : "null") + ',' + q(name + "_error") + ':' + export_errors[name]; }
  exports_json += '}';
  std::string bytes_json = "{";
  for (const auto& [name, dataset] : names) { (void)dataset; if (bytes_json.size() > 1) bytes_json += ','; bytes_json += q(name) + ':' + byte_artifacts[name]; }
  bytes_json += '}';
  const auto context = "{\"size_t_bytes\":" + std::to_string(sizeof(std::size_t)) + ",\"model_format\":" + q(format) + ",\"model_dataset\":" + (upload ? q(upload->filename) : "null") + ",\"original_model_bytes\":" + (upload ? std::to_string(upload->content.size()) : "null") + ",\"original_model_sha256\":" + (upload ? q(footprints_workbench_sha256(upload->content)) : "null") + ",\"model_unchanged\":" + (model && before != "null" && after != "null" ? flag(before == after) : "null") + ",\"model_supplied_members\":" + string_set(model_members) + '}';
  const auto model_json = "{\"original_model\":" + (upload ? original(upload->filename, upload->content) : "null") + ",\"matrix\":" + after + ",\"before\":" + before + ",\"after\":" + after + '}';
  response.status = failure == "null" || native_returned ? 200 : 400;
  response.set_content("{\"family\":\"ORIGINAL_GENETIC_MATRIX_WORKBENCH\",\"operation\":" + q(operation) + ",\"executed\":" + flag(executed) + ",\"native_entered\":" + flag(native_entered) + ",\"native_returned\":" + flag(native_returned) + ",\"native_calls\":" + calls + ",\"error\":" + failure + ",\"input_context\":" + context + ",\"model\":" + model_json + ",\"submitted_options\":" + submitted_options + ",\"configured_options\":" + (options ? serialize_genetic_matrix_workbench_options_json(*options) : "null") + ",\"supplied_options\":" + string_set(option_members) + ",\"raw_controls\":" + raw_controls + ",\"result\":" + result_json + ",\"exports\":" + exports_json + ",\"artifact_bytes\":" + bytes_json + ",\"artifact_errors\":" + artifact_errors + '}', "application/json");
 } catch (const std::exception& error) {
  response.status = 400; response.set_content("{\"error\":" + q(error.what()) + '}', "application/json");
 }
}
