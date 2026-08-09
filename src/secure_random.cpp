#include "minera/secure_random.hpp"

#include <stdexcept>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#else
#include <fstream>
#endif

namespace minera {

std::uint64_t secure_random_u64() {
  std::uint64_t value = 0;
#if defined(_WIN32)
  const auto status = BCryptGenRandom(
      nullptr, reinterpret_cast<PUCHAR>(&value), sizeof(value),
      BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  if (status != 0)
    throw std::runtime_error("secure random source unavailable");
#else
  std::ifstream source("/dev/urandom", std::ios::in | std::ios::binary);
  if (!source.read(reinterpret_cast<char*>(&value), sizeof(value)))
    throw std::runtime_error("secure random source unavailable");
#endif
  return value;
}

}  // namespace minera
