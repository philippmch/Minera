#pragma once

#include <cstdint>

namespace minera {

// Returns one uniformly distributed 64-bit value from the host operating
// system's cryptographic random source. Throws when the source is unavailable.
[[nodiscard]] std::uint64_t secure_random_u64();

}  // namespace minera
