#pragma once

#include <array>
#include <stdexcept>
#include <string>
#include <utility>

namespace minera {

// Portable public exception data for duration arithmetic and empty inputs.
// Existing callers can continue catching std::invalid_argument.
class TokenReplayDurationError final : public std::invalid_argument {
 public:
  TokenReplayDurationError(std::string reference_type, std::string message)
      : std::invalid_argument(message),
        reference_type_(std::move(reference_type)),
        reference_args_{std::move(message)} {}

  [[nodiscard]] const std::string& reference_type() const noexcept {
    return reference_type_;
  }
  [[nodiscard]] const std::array<std::string, 1>& reference_args() const noexcept {
    return reference_args_;
  }

 private:
  std::string reference_type_;
  std::array<std::string, 1> reference_args_;
};

}  // namespace minera
