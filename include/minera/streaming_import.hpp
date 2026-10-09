#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <istream>
#include <memory>
#include <optional>
#include <vector>

#include "minera/csv_event_log_importer.hpp"
#include "minera/live_stream.hpp"
#include "minera/xes_event_log_importer.hpp"

namespace minera {

struct StreamingEventReaderOptions {
  std::function<Event(Event)> transform;
  std::function<bool(const Event&)> acceptance;
};

struct StreamingTraceReaderOptions {
  std::function<Trace(Trace)> transform;
  std::function<bool(const Trace&)> acceptance;
};

// A restartable event reader over a CSV file. Records are exposed one at a
// time and can be delivered directly to a LiveEventStream.
class CsvEventStreamReader {
 public:
  explicit CsvEventStreamReader(
      std::filesystem::path path, CsvImportOptions import_options = {},
      StreamingEventReaderOptions options = {});
  explicit CsvEventStreamReader(std::istream& input, CsvImportOptions import_options = {},
                                StreamingEventReaderOptions options = {});
  [[nodiscard]] const EventLog& source_context() const noexcept { return context_; }

  void reset();
  [[nodiscard]] std::optional<Event> read_event();
  [[nodiscard]] bool exhausted() const noexcept;
  [[nodiscard]] std::size_t to_event_stream(LiveEventStream& stream);

 private:
  std::filesystem::path path_;
  CsvImportOptions import_options_;
  StreamingEventReaderOptions options_;
  std::vector<Event> events_;
  std::size_t cursor_ = 0;
  std::optional<std::string> source_body_;
  EventLog context_;
};

// A restartable event reader over XES. The importer preserves generic scalar
// attributes and canonical activity, timestamp, resource, and lifecycle keys.
class XesEventStreamReader {
 public:
  explicit XesEventStreamReader(
      std::filesystem::path path, XesImportOptions import_options = {},
      StreamingEventReaderOptions options = {});
  explicit XesEventStreamReader(std::istream& input, XesImportOptions import_options = {},
                                StreamingEventReaderOptions options = {});
  [[nodiscard]] const EventLog& source_context() const noexcept { return context_; }

  void reset();
  [[nodiscard]] std::optional<Event> read_event();
  [[nodiscard]] bool exhausted() const noexcept;
  [[nodiscard]] std::size_t to_event_stream(LiveEventStream& stream);

 private:
  std::filesystem::path path_;
  XesImportOptions import_options_;
  StreamingEventReaderOptions options_;
  std::vector<Event> events_;
  std::size_t cursor_ = 0;
  std::optional<std::string> source_body_;
  EventLog context_;
};

// A restartable trace reader over XES. A trace is accepted or transformed as
// a whole before it is returned or delivered.
class XesTraceStreamReader {
 public:
  explicit XesTraceStreamReader(
      std::filesystem::path path, XesImportOptions import_options = {},
      StreamingTraceReaderOptions options = {});
  explicit XesTraceStreamReader(std::istream& input, XesImportOptions import_options = {},
                                StreamingTraceReaderOptions options = {});
  [[nodiscard]] const EventLog& source_context() const noexcept { return context_; }

  void reset();
  [[nodiscard]] std::optional<Trace> read_trace();
  [[nodiscard]] bool exhausted() const noexcept;
  [[nodiscard]] std::size_t to_trace_stream(LiveTraceStream& stream);

 private:
  std::filesystem::path path_;
  XesImportOptions import_options_;
  StreamingTraceReaderOptions options_;
  std::vector<Trace> traces_;
  std::size_t cursor_ = 0;
  std::optional<std::string> source_body_;
  EventLog context_;
};

}  // namespace minera
