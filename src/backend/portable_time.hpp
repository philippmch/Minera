#pragma once

#include <ctime>

namespace minera {

// Thread-safe gmtime/localtime: POSIX has the _r forms, MSVC the _s forms
// with swapped arguments and an errno-style result.
inline bool utc_fields(std::time_t value, std::tm& fields) {
#if defined(_WIN32)
  return gmtime_s(&fields, &value) == 0;
#else
  return gmtime_r(&value, &fields) != nullptr;
#endif
}

inline bool local_fields(std::time_t value, std::tm& fields) {
#if defined(_WIN32)
  return localtime_s(&fields, &value) == 0;
#else
  return localtime_r(&value, &fields) != nullptr;
#endif
}

}  // namespace minera
