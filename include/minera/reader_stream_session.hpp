#pragma once
#include <memory>
#include <string>
#include "minera/streaming_import.hpp"

namespace minera {
enum class ReaderStreamKind { kCsvEvents, kXesEvents, kXesTraces };
enum class ReaderStreamAction { kStart, kDrain, kFinish, kRegister, kDeregister, kResetReader };
struct ReaderStreamOptions {
  ReaderStreamKind kind = ReaderStreamKind::kCsvEvents;
  std::string source;
  CsvImportOptions csv;
  XesImportOptions xes;
  StreamingEventReaderOptions events;
  StreamingTraceReaderOptions traces;
};
// Restartable native readers with one ordered native event/trace dispatcher.
// The current readers eagerly import their source; cursor reads are bounded.
class ReaderStreamSession {
 public:
  explicit ReaderStreamSession(ReaderStreamOptions options);
  ~ReaderStreamSession();
  ReaderStreamSession(const ReaderStreamSession&) = delete;
  ReaderStreamSession& operator=(const ReaderStreamSession&) = delete;
  void read(std::size_t maximum_items, std::string batch_id, std::size_t expected_revision);
  void action(ReaderStreamAction action, std::size_t expected_revision);
  [[nodiscard]] std::size_t revision() const;
  [[nodiscard]] std::string snapshot_json(bool wait_until_drained = true);
  // Blocks until an active stream has delivered its queue.
  void wait_until_drained();
  [[nodiscard]] EventLog selected_log(bool delivered, bool wait_until_drained = true);
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}  // namespace minera
