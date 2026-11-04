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

}  // namespace minera
