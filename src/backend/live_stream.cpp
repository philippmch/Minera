#include "minera/live_stream.hpp"

#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace minera {
namespace {

template <typename Item>
class StreamCore {
 public:
  using Observer = std::function<void(const Item&)>;

  explicit StreamCore(std::size_t thread_pool_size)
      : thread_pool_size_(thread_pool_size) {
    if (thread_pool_size_ == 0)
      throw std::invalid_argument("live stream thread pool must be positive");
  }

  ~StreamCore() { stop(); }

  bool append(Item item) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == StreamingStreamState::kFinished) return false;
    queue_.push_back(std::move(item));
    condition_.notify_one();
    return true;
  }

  void start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == StreamingStreamState::kFinished)
      throw std::logic_error("live stream cannot restart after stop");
    if (state_ == StreamingStreamState::kActive) return;
    stop_requested_ = false;
    state_ = StreamingStreamState::kActive;
    workers_.reserve(thread_pool_size_);
    try {
      for (std::size_t i = 0; i < thread_pool_size_; ++i)
        workers_.emplace_back([this] { worker(); });
    } catch (...) {
      stop_requested_ = true;
      condition_.notify_all();
      throw;
    }
  }

  void stop() noexcept {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (state_ == StreamingStreamState::kFinished) return;
      if (state_ == StreamingStreamState::kInactive) {
        queue_.clear();
        state_ = StreamingStreamState::kFinished;
        return;
      }
      state_ = StreamingStreamState::kFinished;
      stop_requested_ = true;
      condition_.notify_all();
      drained_.wait(lock, [this] {
        return queue_.empty() && active_items_ == 0;
      });
    }
    for (auto& worker_thread : workers_)
      if (worker_thread.joinable()) worker_thread.join();
    workers_.clear();
  }

  std::size_t register_observer(Observer observer) {
    if (!observer) throw std::invalid_argument("live stream observer is empty");
    std::lock_guard<std::mutex> lock(mutex_);
    const auto id = next_observer_id_++;
    observers_.emplace(id, std::move(observer));
    return id;
  }

  bool deregister_observer(std::size_t observer_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return observers_.erase(observer_id) != 0;
  }

  StreamingStreamState state() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
  }

  std::size_t pending() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }

 private:
  void worker() noexcept {
    for (;;) {
      Item item;
      std::vector<Observer> observers;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] {
          return stop_requested_ || !queue_.empty();
        });
        if (queue_.empty() && stop_requested_) return;
        item = std::move(queue_.front());
        queue_.pop_front();
        ++active_items_;
        observers.reserve(observers_.size());
        for (const auto& [id, observer] : observers_) {
          (void)id;
          observers.push_back(observer);
        }
      }
      for (const auto& observer : observers) {
        try {
          observer(item);
        } catch (...) {
          // A faulty observer must not terminate the stream worker or prevent
          // other observers from receiving later items.
        }
      }
      {
        std::lock_guard<std::mutex> lock(mutex_);
        --active_items_;
        if (queue_.empty() && active_items_ == 0) drained_.notify_all();
      }
    }
  }

  const std::size_t thread_pool_size_;
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::condition_variable drained_;
  std::deque<Item> queue_;
  std::map<std::size_t, Observer> observers_;
  std::vector<std::thread> workers_;
  std::size_t next_observer_id_ = 1;
  std::size_t active_items_ = 0;
  bool stop_requested_ = false;
  StreamingStreamState state_ = StreamingStreamState::kInactive;
};

}  // namespace

struct LiveEventStream::Impl {
  explicit Impl(std::size_t pool_size) : core(pool_size) {}
  StreamCore<Event> core;
};

LiveEventStream::LiveEventStream(std::size_t thread_pool_size)
    : impl_(std::make_unique<Impl>(thread_pool_size)) {}
LiveEventStream::~LiveEventStream() = default;
bool LiveEventStream::append(Event event) {
  return impl_->core.append(std::move(event));
}
void LiveEventStream::start() { impl_->core.start(); }
void LiveEventStream::stop() { impl_->core.stop(); }
std::size_t LiveEventStream::register_observer(Observer observer) {
  return impl_->core.register_observer(std::move(observer));
}
bool LiveEventStream::deregister_observer(std::size_t observer_id) {
  return impl_->core.deregister_observer(observer_id);
}
StreamingStreamState LiveEventStream::state() const noexcept {
  return impl_->core.state();
}
std::size_t LiveEventStream::pending() const noexcept {
  return impl_->core.pending();
}

struct LiveTraceStream::Impl {
  explicit Impl(std::size_t pool_size) : core(pool_size) {}
  StreamCore<Trace> core;
};

LiveTraceStream::LiveTraceStream(std::size_t thread_pool_size)
    : impl_(std::make_unique<Impl>(thread_pool_size)) {}
LiveTraceStream::~LiveTraceStream() = default;
bool LiveTraceStream::append(Trace trace) {
  return impl_->core.append(std::move(trace));
}
void LiveTraceStream::start() { impl_->core.start(); }
void LiveTraceStream::stop() { impl_->core.stop(); }
std::size_t LiveTraceStream::register_observer(Observer observer) {
  return impl_->core.register_observer(std::move(observer));
}
bool LiveTraceStream::deregister_observer(std::size_t observer_id) {
  return impl_->core.deregister_observer(observer_id);
}
StreamingStreamState LiveTraceStream::state() const noexcept {
  return impl_->core.state();
}
std::size_t LiveTraceStream::pending() const noexcept {
  return impl_->core.pending();
}

}  // namespace minera
