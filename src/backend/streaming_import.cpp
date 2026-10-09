#include "minera/streaming_import.hpp"

#include <utility>
#include <iterator>
#include <sstream>

namespace minera {
namespace {

std::optional<Event> apply_event_options(
    Event event, const StreamingEventReaderOptions& options) {
  if (options.transform) event = options.transform(std::move(event));
  if (options.acceptance && !options.acceptance(event)) return std::nullopt;
  return std::optional<Event>(std::move(event));
}

std::optional<Trace> apply_trace_options(
    Trace trace, const StreamingTraceReaderOptions& options) {
  if (options.transform) trace = options.transform(std::move(trace));
  if (options.acceptance && !options.acceptance(trace)) return std::nullopt;
  return std::optional<Trace>(std::move(trace));
}

}  // namespace

CsvEventStreamReader::CsvEventStreamReader(
    std::filesystem::path path, CsvImportOptions import_options,
    StreamingEventReaderOptions options)
    : path_(std::move(path)),
      import_options_(std::move(import_options)),
      options_(std::move(options)) {
  reset();
}

CsvEventStreamReader::CsvEventStreamReader(std::istream& input, CsvImportOptions import_options,
                                         StreamingEventReaderOptions options)
    : import_options_(std::move(import_options)), options_(std::move(options)),
      source_body_(std::string(std::istreambuf_iterator<char>(input), {})) { reset(); }
void CsvEventStreamReader::reset() {
  events_.clear();
  cursor_ = 0;
  std::istringstream input(source_body_.value_or(""));
  const auto log = source_body_ ? import_csv_event_log(input, import_options_)
                                : import_csv_event_log(path_, import_options_);
  copy_event_log_context(log, context_);
  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) {
      auto selected = apply_event_options(event, options_);
      if (selected) events_.push_back(std::move(*selected));
    }
  }
}

std::optional<Event> CsvEventStreamReader::read_event() {
  if (cursor_ >= events_.size()) return std::nullopt;
  return std::move(events_[cursor_++]);
}

bool CsvEventStreamReader::exhausted() const noexcept {
  return cursor_ >= events_.size();
}

std::size_t CsvEventStreamReader::to_event_stream(LiveEventStream& stream) {
  std::size_t delivered = 0;
  while (const auto event = read_event()) {
    if (stream.append(std::move(*event))) ++delivered;
  }
  return delivered;
}

XesEventStreamReader::XesEventStreamReader(
    std::filesystem::path path, XesImportOptions import_options,
    StreamingEventReaderOptions options)
    : path_(std::move(path)),
      import_options_(std::move(import_options)),
      options_(std::move(options)) {
  reset();
}

XesEventStreamReader::XesEventStreamReader(std::istream& input, XesImportOptions import_options,
                                         StreamingEventReaderOptions options)
    : import_options_(std::move(import_options)), options_(std::move(options)),
      source_body_(std::string(std::istreambuf_iterator<char>(input), {})) { reset(); }
void XesEventStreamReader::reset() {
  events_.clear();
  cursor_ = 0;
  std::istringstream input(source_body_.value_or(""));
  const auto log = source_body_ ? import_xes_event_log(input, import_options_)
                                : import_xes_event_log(path_, import_options_);
  copy_event_log_context(log, context_);
  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) {
      auto selected = apply_event_options(event, options_);
      if (selected) events_.push_back(std::move(*selected));
    }
  }
}

std::optional<Event> XesEventStreamReader::read_event() {
  if (cursor_ >= events_.size()) return std::nullopt;
  return std::move(events_[cursor_++]);
}

bool XesEventStreamReader::exhausted() const noexcept {
  return cursor_ >= events_.size();
}

std::size_t XesEventStreamReader::to_event_stream(LiveEventStream& stream) {
  std::size_t delivered = 0;
  while (const auto event = read_event()) {
    if (stream.append(std::move(*event))) ++delivered;
  }
  return delivered;
}

XesTraceStreamReader::XesTraceStreamReader(
    std::filesystem::path path, XesImportOptions import_options,
    StreamingTraceReaderOptions options)
    : path_(std::move(path)),
      import_options_(std::move(import_options)),
      options_(std::move(options)) {
  reset();
}

XesTraceStreamReader::XesTraceStreamReader(std::istream& input, XesImportOptions import_options,
                                         StreamingTraceReaderOptions options)
    : import_options_(std::move(import_options)), options_(std::move(options)),
      source_body_(std::string(std::istreambuf_iterator<char>(input), {})) { reset(); }
void XesTraceStreamReader::reset() {
  traces_.clear();
  cursor_ = 0;
  std::istringstream input(source_body_.value_or(""));
  const auto log = source_body_ ? import_xes_event_log(input, import_options_)
                                : import_xes_event_log(path_, import_options_);
  copy_event_log_context(log, context_);
  for (const auto& trace : log.traces) {
    auto selected = apply_trace_options(trace, options_);
    if (selected) traces_.push_back(std::move(*selected));
  }
}

std::optional<Trace> XesTraceStreamReader::read_trace() {
  if (cursor_ >= traces_.size()) return std::nullopt;
  return std::move(traces_[cursor_++]);
}

bool XesTraceStreamReader::exhausted() const noexcept {
  return cursor_ >= traces_.size();
}

std::size_t XesTraceStreamReader::to_trace_stream(LiveTraceStream& stream) {
  std::size_t delivered = 0;
  while (const auto trace = read_trace()) {
    if (stream.append(std::move(*trace))) ++delivered;
  }
  return delivered;
}

}  // namespace minera
