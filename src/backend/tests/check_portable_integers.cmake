# Fails when core code (outside comments) uses a compiler extension that the
# configured MSVC build cannot compile: 128-bit integer types, _BitInt or GCC
# builtins. Run as: cmake -DMINERA_SOURCE_DIR=<repo> -P check_portable_integers.cmake
file(GLOB_RECURSE sources
     "${MINERA_SOURCE_DIR}/include/*.hpp" "${MINERA_SOURCE_DIR}/include/*.h"
     "${MINERA_SOURCE_DIR}/src/backend/*.hpp" "${MINERA_SOURCE_DIR}/src/backend/*.cpp"
     "${MINERA_SOURCE_DIR}/src/backend/*.h")
set(pattern "__int128|__uint128_t|_BitInt|__builtin_")
set(found "")
foreach(source IN LISTS sources)
  if(source MATCHES "/src/backend/tests/")
    continue()
  endif()
  file(STRINGS "${source}" lines REGEX "${pattern}")
  foreach(line IN LISTS lines)
    string(REGEX REPLACE "//.*$" "" code "${line}")
    string(STRIP "${code}" code)
    if(code MATCHES "^(\\*|/\\*)")
      continue()
    endif()
    if(code MATCHES "${pattern}")
      string(APPEND found "\n  ${source}: ${line}")
    endif()
  endforeach()
endforeach()
if(found)
  message(FATAL_ERROR "non-portable integer or builtin extension in core code:${found}")
endif()
list(LENGTH sources count)
message(STATUS "portable integer check: ${count} core files clean")
