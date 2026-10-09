#pragma once
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
namespace minera {
enum class WindowsEventSourceKind { kChannel, kEvtx };
struct WindowsEventSourceOptions {
  WindowsEventSourceKind kind=WindowsEventSourceKind::kChannel;
  std::string channel;
  std::filesystem::path path;
  std::string query="*";
  bool reverse=false;
  std::size_t max_events=1000;
  unsigned timeout_ms=1000;
};
struct WindowsEventCapture {
  WindowsEventSourceKind kind;
  std::string channel,query;
  bool reverse=false;
  std::size_t events=0;
  bool limited=false;
  std::string xml;
};
// One rendered event XML fragment per call; nullopt marks query exhaustion.
using WindowsEventNext=std::function<std::optional<std::string>()>;
[[nodiscard]] WindowsEventCapture capture_windows_events(const WindowsEventSourceOptions& options,WindowsEventNext next={});
[[nodiscard]] std::string windows_event_capture_json(const WindowsEventCapture& capture);
}  // namespace minera
