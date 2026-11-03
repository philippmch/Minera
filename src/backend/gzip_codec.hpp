#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace minera::detail {

// Thrown when inflating would pass max_output_bytes, so callers can name the
// limit and how to raise it.
struct GzipOutputLimitError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

[[nodiscard]] bool is_gzip_data(std::string_view bytes) noexcept;
[[nodiscard]] std::string gzip_compress(std::string_view bytes);
[[nodiscard]] std::string gzip_decompress(
    std::string_view bytes, std::size_t max_output_bytes);

}  // namespace minera::detail
