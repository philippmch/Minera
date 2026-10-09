#pragma once
#include <map>
#include <optional>
#include <string>
#include "minera/event_log.hpp"
#include "minera/repository_history.hpp"
#include "minera/windows_event_source.hpp"

namespace minera {
// Uploads and explicit server paths feed the existing native connector importers.
// Repository acquisition uses the supplied transport; tokens are never returned.
struct SourceAcquisitionInput {
  std::string connector;
  std::map<std::string,std::string> parameters;
  std::optional<std::string> source_bytes;
  std::string server_path;
  std::string auth_token;
  RepositoryHistoryTransport repository_transport;
  WindowsEventNext windows_event_reader;
};
struct SourceAcquisitionResult {
  std::string connector;
  EventLog log;
  std::string native_result_json;
  std::optional<std::string> original_bytes;
  std::optional<WindowsEventCapture> windows_capture;
};
[[nodiscard]] SourceAcquisitionResult acquire_source(const SourceAcquisitionInput& input);
[[nodiscard]] std::string source_acquisition_json(const SourceAcquisitionResult& result);
}  // namespace minera
