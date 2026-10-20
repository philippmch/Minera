#include "minera/source_acquisition.hpp"
#include "minera/xes_export.hpp"
#include "repository_history_runtime.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/exact_filter_discovery_json.hpp"
#include <algorithm>
#include <iterator>
#include <set>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using Part = minera::DiscoveryWorkbenchPart;
std::string safe(std::string_view value) {
  return minera::serialize_embedding_workbench_string_json(value);
}
std::string read_exact_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::invalid_argument("could not open the supplied file");
  std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  if (input.bad()) throw std::runtime_error("could not read the supplied file");
  return bytes;
}
void write_exact_file(const std::filesystem::path& path, const std::string& bytes) {
  std::ofstream output(path, std::ios::binary);
  if (!output) throw std::runtime_error("could not create output file");
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  output.flush();
  if (!output) throw std::runtime_error("could not write output file");
}
std::string path_bytes(const std::filesystem::path& path) {
  const auto bytes = path.u8string();
  return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}
std::string parts_json(const std::vector<Part>& parts, bool query) {
  std::string out = "[";
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (i) out += ',';
    const auto& part = parts[i];
    out += "{\"field\":" + safe(part.name) + ",\"bytes\":" + safe(part.bytes);
    if (!query) out += ",\"filename\":" + (part.file ? safe(part.filename) : "null") +
        ",\"file\":" + (part.file ? "true" : "false");
    out += '}';
  }
  return out + ']';
}
std::string call_json(bool entered, bool returned, const std::string& error) {
  return std::string("{\"entered\":") + (entered ? "true" : "false") +
      ",\"returned\":" + (returned ? "true" : "false") +
      ",\"error\":" + (error.empty() ? "null" : safe(error)) + '}';
}
bool exact_cli_flag(std::string_view flag) {
  return flag == "--exact-result" || flag == "--exact-controls-report" ||
      flag == "--xes-export-options-json" || flag == "--xes-export-options-file" ||
      flag == "--xes-variant" || flag == "--xes-compress" || flag == "--xes-name" ||
      flag == "--xes-infer-metadata" || flag == "--xes-preserve-nulls";
}
bool exact_cli_requested(int argc, char** argv) {
  for (int i = 2; i < argc; ++i) {
    const std::string_view key = argv[i];
    if (!key.starts_with("--")) continue;
    if (exact_cli_flag(key)) return true;
    // Values are never interpreted as option positions.
    if (key != "--help") ++i;
  }
  return false;
}
bool exact_bool(const std::string& value) {
  if (value == "true") return true;
  if (value == "false") return false;
  throw std::invalid_argument("XES boolean controls require true or false");
}
int exact_cli_main(int argc, char** argv) {
  minera::SourceAcquisitionInput input;
  input.connector = argc > 1 ? argv[1] : "";
  std::vector<Part> original, query;
  std::map<std::string, std::string> flags;
  std::optional<minera::SourceAcquisitionResult> acquired;
  std::string phase = "parse", error, acquire_error, report_path;
  std::string export_report = "null", output_artifacts = "[]", configured_options = "null", provenance_error;
  bool entered = false, returned = false, exact_stdout = false;
  int status = 200;
  // Collect the complete supplied transport before validation. This does not
  // resolve secrets, and preserves duplicate/raw controls on parser failures.
  query.push_back({"connector", {}, input.connector, false});
  for (int i = 2; i < argc; ++i) {
    const std::string key = argv[i];
    if (!key.starts_with("--")) {
      query.push_back({"source_path", {}, key, false});
      const auto path = std::filesystem::u8path(key);
      try {
        if (std::filesystem::is_regular_file(path)) {
          if (std::filesystem::file_size(path) > 512ULL * 1024 * 1024)
            throw std::length_error("source acquisition: file exceeds 512 MiB");
          original.push_back({"source", path_bytes(path.filename()), read_exact_file(path), true});
        }
      } catch (const std::exception& failure) { if (provenance_error.empty()) provenance_error = failure.what(); }
      continue;
    }
    if (key == "--exact-result") {
      exact_stdout = true;
      query.push_back({key, {}, "true", false});
      continue;
    }
    if (key.starts_with("--xes-")) exact_stdout = true;
    if (i + 1 >= argc) { query.push_back({key, {}, "", false}); continue; }
    const std::string value = argv[++i];
    const bool supported = exact_cli_flag(key) || key == "--param" || key == "--token-env" ||
        key == "--database-connection-env" || key == "--output-table" || key == "--output-formatted" ||
        key == "--output-xes" || key == "--output-capture";
    query.push_back({key, {}, supported ? value : std::string{}, false});
    if (key == "--exact-controls-report" && report_path.empty()) report_path = value;
    if (key == "--xes-export-options-json") original.push_back({"xes_export_options_json", {}, value, false});
    if (key == "--xes-export-options-file") {
      try {
        const auto path = std::filesystem::u8path(value);
        original.push_back({"xes_export_options_file", path_bytes(path.filename()), read_exact_file(path), true});
      } catch (const std::exception& failure) { if (provenance_error.empty()) provenance_error = failure.what(); }
    }
  }
  try {
    if (!provenance_error.empty()) throw std::invalid_argument(provenance_error);
    for (int i = 2; i < argc; ++i) {
      const std::string key = argv[i];
      if (!key.starts_with("--")) {
        if (!input.server_path.empty()) throw std::invalid_argument("only one source path is accepted");
        input.server_path = key;
        continue;
      }
      if (key == "--exact-result") {
        if (flags.contains(key)) throw std::invalid_argument("duplicate exact CLI flag: " + key);
        flags.emplace(key, "true"); continue;
      }
      if (++i >= argc) throw std::invalid_argument("missing option value");
      const std::string value = argv[i];
      // Credential values are resolved from named environments only. The names,
      // rather than the resulting token/connection bytes, are safe provenance.
      if (key == "--param") {
        const auto separator = value.find('=');
        if (separator == std::string::npos || separator == 0 ||
            !input.parameters.emplace(value.substr(0, separator), value.substr(separator + 1)).second)
          throw std::invalid_argument("native parameters require unique key=value entries");
        continue;
      }
      if (!flags.emplace(key, value).second) throw std::invalid_argument("duplicate exact CLI flag: " + key);
      if (key == "--token-env") {
        const auto* token = std::getenv(value.c_str());
        if (!token || !*token) throw std::invalid_argument("the named token environment variable is empty");
        input.auth_token = token;
      } else if (key == "--database-connection-env") {
        const auto* connection = std::getenv(value.c_str());
        if (!connection || !*connection) throw std::invalid_argument("the named database connection environment variable is empty");
        input.database_connection = connection;
      } else if (key != "--output-table" && key != "--output-formatted" && key != "--output-xes" &&
          key != "--output-capture" && !exact_cli_flag(key)) throw std::invalid_argument("unknown option: " + key);
    }
    const auto has = [&](const std::string& key) { return flags.contains(key); };
    const bool full_json = has("--xes-export-options-json"), full_file = has("--xes-export-options-file");
    if (full_json && full_file) throw std::invalid_argument("XES options JSON and File conflict by presence");
    bool xes_controls = full_json || full_file;
    for (const auto* key : {"--xes-variant", "--xes-compress", "--xes-name", "--xes-infer-metadata", "--xes-preserve-nulls"}) {
      xes_controls = xes_controls || has(key);
      if ((full_json || full_file) && has(key)) throw std::invalid_argument("full XES options conflict with scalar controls by presence");
    }
    if (xes_controls && !has("--output-xes")) throw std::invalid_argument("XES controls require --output-xes");
    std::optional<minera::XesExportOptions> options;
    if (has("--output-xes")) {
      if (full_json || full_file) {
        const auto it = std::find_if(original.begin(), original.end(), [&](const auto& part) {
          return part.name == (full_file ? "xes_export_options_file" : "xes_export_options_json");
        });
        options = minera::parse_source_acquisition_xes_options_json(it->bytes);
      } else {
        options.emplace();
        options->preserve_nulls = true;
        if (has("--xes-variant")) options->variant = minera::parse_xes_export_variant(flags.at("--xes-variant"));
        if (has("--xes-compress")) {
          const auto& value = flags.at("--xes-compress");
          if (value != "auto") options->compress = exact_bool(value);
        }
        if (has("--xes-name")) options->name = flags.at("--xes-name");
        if (has("--xes-infer-metadata")) options->infer_metadata = exact_bool(flags.at("--xes-infer-metadata"));
        if (has("--xes-preserve-nulls")) options->preserve_nulls = exact_bool(flags.at("--xes-preserve-nulls"));
      }
    }
    if (options) configured_options = minera::source_acquisition_xes_options_json(*options);
    input.repository_transport = minera::tools::repository_history_http_transport();
    phase = "native";
    entered = true;
    try { acquired = minera::acquire_source(input); returned = true; }
    catch (const std::exception& failure) { acquire_error = failure.what(); throw; }
    if (acquired->original_bytes && std::none_of(original.begin(), original.end(), [](const auto& part) { return part.name == "source"; })) {
      const auto path = std::filesystem::u8path(input.server_path);
      original.push_back({"source", path_bytes(path.filename()), *acquired->original_bytes, !input.server_path.empty()});
    }
    phase = "export";
    if (options) {
      const auto before = minera::exact_source_snapshot(minera::EventDataView{acquired->log});
      const auto options_before = minera::source_acquisition_xes_options_json(*options);
      std::string after = before, options_after = options_before, export_error, actual_path, bytes;
      bool export_returned = false;
      try {
        const auto path = minera::export_xes_event_log(std::filesystem::u8path(flags.at("--output-xes")), acquired->log, *options);
        export_returned = true;
        actual_path = path_bytes(path);
        bytes = read_exact_file(path); // Only read this invocation's returned path after success.
      } catch (const std::exception& failure) { export_error = failure.what(); }
      after = minera::exact_source_snapshot(minera::EventDataView{acquired->log});
      options_after = minera::source_acquisition_xes_options_json(*options);
      const bool compressed = bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0x1f && static_cast<unsigned char>(bytes[1]) == 0x8b;
      export_report = "{\"family\":\"EXACT_SOURCE_EXPORT\",\"operation\":\"export\",\"status\":" +
          std::to_string(export_error.empty() ? 200 : 400) + ",\"source_id\":null,\"format\":\"xes\",\"input_order\":\"provided_order\",\"original_inputs\":" +
          parts_json(original, false) + ",\"query_inputs\":" + parts_json(query, true) +
          ",\"native_call\":" + call_json(true, export_returned, export_returned ? std::string{} : export_error) +
          ",\"error\":" + (export_error.empty() ? "null" : "{\"phase\":\"export\",\"message\":" + safe(export_error) + '}') +
          ",\"source_before\":" + before + ",\"source_after\":" + after +
          ",\"source_unchanged\":" + (before == after ? "true" : "false") +
          ",\"options_before\":" + options_before + ",\"options_after\":" + options_after +
          ",\"options_unchanged\":" + (options_before == options_after ? "true" : "false") +
          ",\"output\":" + (export_error.empty() ? "{\"bytes\":" + safe(bytes) + ",\"mime_type\":" +
              safe(compressed ? "application/gzip" : "application/xml") + ",\"actual_path\":" + safe(actual_path) + '}' : "null") +
          ",\"result\":null,\"artifacts\":{\"legacy_log_json\":{\"bytes\":null,\"error\":null}}}";
      if (!export_error.empty()) throw std::runtime_error(export_error);
    }
    output_artifacts = "[";
    bool first = true;
    for (const auto& [flag, formatted] : {std::pair{"--output-capture", false}, std::pair{"--output-table", false}, std::pair{"--output-formatted", true}}) {
      if (!has(flag)) continue;
      std::string bytes;
      if (std::string_view(flag) == "--output-capture") {
        if (acquired->outlook_capture) bytes = acquired->outlook_capture->item_table_json;
        else if (acquired->windows_capture) bytes = acquired->windows_capture->xml;
        else throw std::invalid_argument("this source has no native event/item capture");
      } else {
        if (acquired->outlook_capture) bytes = formatted ? acquired->outlook_capture->formatted_table_json : acquired->outlook_capture->item_table_json;
        else if (acquired->database_capture) bytes = formatted ? acquired->database_capture->formatted_table_json : acquired->database_capture->raw_table_json;
        else throw std::invalid_argument("this source has no database query/Outlook item table");
      }
      write_exact_file(std::filesystem::u8path(flags.at(flag)), bytes);
      if (!first) output_artifacts += ',';
      first = false;
      output_artifacts += "{\"flag\":" + safe(flag) + ",\"path\":" + safe(flags.at(flag)) + ",\"bytes\":" + safe(bytes) + '}';
    }
    output_artifacts += ']';
  } catch (const std::exception& failure) {
    error = failure.what(); status = 400;
    if (!output_artifacts.ends_with(']')) output_artifacts += ']';
  }
  std::string configuration = "[";
  bool first = true;
  for (const auto& [key, value] : input.parameters) {
    if (!first) configuration += ',';
    first = false;
    configuration += "{\"key\":" + safe(key) + ",\"value\":" + safe(value) + '}';
  }
  configuration += ']';
  const auto provenance = "\"original_inputs\":" + parts_json(original, false) +
      ",\"query_inputs\":" + parts_json(query, true);
  const auto source = "{\"name\":" + safe(input.server_path.empty() ? input.connector : path_bytes(std::filesystem::u8path(input.server_path).filename())) +
      ",\"mode\":" + safe(input.server_path.empty() ? "native" : "path") + ",\"path\":" + safe(input.server_path) +
      ",\"has_original\":" + (acquired && acquired->original_bytes ? "true" : "false") +
      ",\"size\":" + (acquired && acquired->original_bytes ? std::to_string(acquired->original_bytes->size()) : "null") +
      ",\"provided_sha256\":\"\",\"authenticated\":" + (!input.auth_token.empty() || !input.database_connection.empty() ? "true" : "false") +
      ",\"uploaded_filename\":null}";
  const auto report = "{\"family\":\"EXACT_SOURCE_ACQUISITION\",\"operation\":\"acquire\",\"status\":" +
      std::to_string(status) + ",\"source_id\":null,\"input_order\":\"provided_order\"," + provenance +
      ",\"error\":" + (error.empty() ? "null" : "{\"phase\":" + safe(phase) + ",\"message\":" + safe(error) + '}') +
      ",\"native_call\":" + call_json(entered, returned, acquire_error) +
      ",\"source\":" + source + ",\"configuration\":" + configuration + ",\"acquisition_inputs\":{" + provenance +
      "},\"result\":" + (acquired ? minera::source_acquisition_result_exact_json(*acquired) : "null") +
      ",\"artifacts\":" + (acquired ? minera::source_acquisition_artifacts_json(*acquired) : "{}") +
      ",\"released\":false,\"configured_xes_options\":" + configured_options + ",\"xes_export\":" + export_report + ",\"output_artifacts\":" + output_artifacts + '}';
  try {
    if (!report_path.empty()) write_exact_file(std::filesystem::u8path(report_path), report + '\n');
    if (exact_stdout) std::cout << report << '\n';
    else if (status == 200 && acquired) std::cout << minera::source_acquisition_json(*acquired) << '\n';
  } catch (const std::exception& failure) { std::cerr << "error: " << failure.what() << '\n'; return 1; }
  if (!error.empty()) std::cerr << "error: " << error << '\n';
  return status == 200 ? 0 : 1;
}
void exact_usage() {
  std::cout << "Usage: minera_acquire_source CONNECTOR [PATH] [legacy options]\n"
      "  --exact-result                 Safe complete acquisition report on stdout\n"
      "  --exact-controls-report PATH   Save report; alone preserves legacy stdout\n"
      "  --xes-export-options-json JSON | --xes-export-options-file PATH\n"
      "  --xes-variant VALUE --xes-compress true|false|auto --xes-name VALUE\n"
      "  --xes-infer-metadata true|false --xes-preserve-nulls true|false\n"
      "  XES controls require --output-xes PATH; full options conflict with scalars.\n"
      "  compress=auto/null uses the native output suffix inference.\n"
      "  Credentials: --token-env NAME / --database-connection-env NAME.\n";
}
}  // namespace

int main(int argc,char** argv) {
  if (argc == 2 && std::string_view(argv[1]) == "--help") { exact_usage(); return 0; }
  if (exact_cli_requested(argc, argv)) return exact_cli_main(argc, argv);
  try {
    if(argc<2){std::cerr<<"Usage: minera_acquire_source CONNECTOR [PATH] [--param key=value] [--token-env NAME] [--output-xes PATH] [--output-capture PATH] [--database-connection-env NAME] [--output-table PATH] [--output-formatted PATH]\n";return 2;}
    minera::SourceAcquisitionInput input;input.connector=argv[1];std::string output,capture_path,table_path,formatted_path;
    for(int i=2;i<argc;++i){const std::string key=argv[i];if(!key.starts_with("--")){if(!input.server_path.empty())throw std::invalid_argument("only one source path is accepted");input.server_path=key;continue;}if(++i>=argc)throw std::invalid_argument("missing option value");const std::string value=argv[i];
      if(key=="--param"){const auto separator=value.find('=');if(separator==std::string::npos||separator==0||!input.parameters.emplace(value.substr(0,separator),value.substr(separator+1)).second)throw std::invalid_argument("native parameters require unique key=value entries");}
      else if(key=="--token-env"){const auto* token=std::getenv(value.c_str());if(!token||!*token)throw std::invalid_argument("the named token environment variable is empty");input.auth_token=token;}
      else if(key=="--database-connection-env"){const auto* connection=std::getenv(value.c_str());if(!connection||!*connection)throw std::invalid_argument("the named database connection environment variable is empty");input.database_connection=connection;}
      else if(key=="--output-table")table_path=value;else if(key=="--output-formatted")formatted_path=value;
      else if(key=="--output-xes")output=value;else if(key=="--output-capture")capture_path=value;else throw std::invalid_argument("unknown option: "+key);
    }
    input.repository_transport=minera::tools::repository_history_http_transport();const auto result=minera::acquire_source(input);
    if(!output.empty()){minera::XesExportOptions options;options.preserve_nulls=true;(void)minera::export_xes_event_log(std::filesystem::u8path(output),result.log,options);}
    if(!capture_path.empty()){if(!result.windows_capture&&!result.outlook_capture)throw std::invalid_argument("this source has no native event/item capture");std::ofstream capture(std::filesystem::u8path(capture_path),std::ios::binary);capture<<(result.outlook_capture?result.outlook_capture->item_table_json:result.windows_capture->xml);if(!capture)throw std::runtime_error("could not write native event/item capture");}
    for(const auto& [path,formatted]:{std::pair{table_path,false},std::pair{formatted_path,true}})if(!path.empty()){if(!result.database_capture&&!result.outlook_capture)throw std::invalid_argument("this source has no database query/Outlook item table");std::ofstream file(std::filesystem::u8path(path),std::ios::binary);if(result.outlook_capture)file<<(formatted?result.outlook_capture->formatted_table_json:result.outlook_capture->item_table_json);else file<<(formatted?result.database_capture->formatted_table_json:result.database_capture->raw_table_json);if(!file)throw std::runtime_error("could not write native query/item table");}
    std::cout<<minera::source_acquisition_json(result)<<'\n';
  }catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}return 0;
}
