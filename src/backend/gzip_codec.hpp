#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace minera::detail {

[[nodiscard]] bool is_gzip_data(std::string_view bytes) noexcept;
[[nodiscard]] std::string gzip_compress(std::string_view bytes);
[[nodiscard]] std::string gzip_decompress(
    std::string_view bytes, std::size_t max_output_bytes);

}  // namespace minera::detail
