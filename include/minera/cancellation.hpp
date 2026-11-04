#pragma once

#include <functional>
#include <stdexcept>
#include <string>

namespace minera {

// Thrown by a long computation whose caller-supplied `cancelled` callback
// returned true (the server sets one to notice a client that disconnected).
// Nothing is returned, so no partial result can be cached or shown. An unset
// callback never cancels, and a run that is not cancelled is unchanged.
class OperationCancelled : public std::runtime_error {
 public:
  explicit OperationCancelled(const std::string& what) : std::runtime_error(what) {}
};

// Throws OperationCancelled(what) when `cancelled` is set and returns true.
inline void throw_if_cancelled(const std::function<bool()>& cancelled,
                               const char* what) {
  if (cancelled && cancelled()) throw OperationCancelled(what);
}

// For hot loops: calls `cancelled` on every `period`-th check only (the
// server's callback is a system call) and throws OperationCancelled(what) once
// it returns true. An unset callback costs one branch per check.
class CancellationCheck {
 public:
  CancellationCheck(const std::function<bool()>& cancelled, const char* what,
                    unsigned period = 256) noexcept
      : cancelled_(cancelled), what_(what), period_(period == 0 ? 1 : period) {}
  void operator()() {
    if (!cancelled_) return;
    if (++calls_ < period_) return;
    calls_ = 0;
    throw_if_cancelled(cancelled_, what_);
  }

 private:
  const std::function<bool()>& cancelled_;
  const char* what_;
  unsigned period_;
  unsigned calls_ = 0;
};

}  // namespace minera
