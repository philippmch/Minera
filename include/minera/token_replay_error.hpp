#pragma once

#include <array>
#include <stdexcept>
#include <string>

namespace minera {

// Portable public KeyError data for a missing literal replay selector. Keys
// retain their complete UTF-8/WTF-8 bytes, including embedded NUL characters.
// Existing callers can continue catching std::invalid_argument.
class TokenReplayKeyError final : public std::invalid_argument {
 public:
  explicit TokenReplayKeyError(std::string key);

  [[nodiscard]] const std::string& reference_type() const noexcept {
    return reference_type_;
  }
  [[nodiscard]] const std::array<std::string, 1>& reference_args() const noexcept {
    return reference_args_;
  }

 private:
  std::string reference_type_ = "builtins.KeyError";
  std::array<std::string, 1> reference_args_;
};

}  // namespace minera
