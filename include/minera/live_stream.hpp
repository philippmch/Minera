#pragma once

#include <cstddef>
#include <functional>
#include <memory>

#include "minera/event_log.hpp"

namespace minera {

enum class StreamingStreamState {
  kInactive,
  kActive,
  kFinished,
  INACTIVE = kInactive,
  ACTIVE = kActive,
  FINISHED = kFinished,
};

// A thread-safe live event stream. Events may be queued before start(); stop()
// drains accepted events and waits for all registered observers to finish.
class LiveEventStream {
 public:
  using Observer = std::function<void(const Event&)>;

  explicit LiveEventStream(std::size_t thread_pool_size = 6);
  ~LiveEventStream();
  LiveEventStream(const LiveEventStream&) = delete;
  LiveEventStream& operator=(const LiveEventStream&) = delete;

  bool append(Event event);
  void start();
  void stop();
  [[nodiscard]] std::size_t register_observer(Observer observer);
  bool deregister_observer(std::size_t observer_id);
  [[nodiscard]] StreamingStreamState state() const noexcept;
  [[nodiscard]] std::size_t pending() const noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Trace-shaped counterpart of LiveEventStream. Observers receive complete
// traces and share the same lifecycle and drain guarantees.
class LiveTraceStream {
 public:
  using Observer = std::function<void(const Trace&)>;

  explicit LiveTraceStream(std::size_t thread_pool_size = 6);
  ~LiveTraceStream();
  LiveTraceStream(const LiveTraceStream&) = delete;
  LiveTraceStream& operator=(const LiveTraceStream&) = delete;

  bool append(Trace trace);
  void start();
  void stop();
  [[nodiscard]] std::size_t register_observer(Observer observer);
  bool deregister_observer(std::size_t observer_id);
  [[nodiscard]] StreamingStreamState state() const noexcept;
  [[nodiscard]] std::size_t pending() const noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace minera
