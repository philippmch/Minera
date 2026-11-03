# Runs one command-line invocation and checks its exact exit status, so a
# rejected value must exit cleanly (an abort or signal never matches).
#
#   cmake -DCOMMAND="prog|arg|..." -DEXPECT_EXIT=2
#         [-DEXPECT_STDERR=regex] [-DEXPECT_STDOUT=regex]
#         [-DEXPECT_STDOUT_COUNT=regex:n] -P check_cli_exit.cmake
if(NOT DEFINED COMMAND OR NOT DEFINED EXPECT_EXIT)
  message(FATAL_ERROR "COMMAND and EXPECT_EXIT are required")
endif()
# Arguments are separated by "|" so that no list escaping is needed on the
# way through add_test. An argument spelled @EMPTY@ is passed as an empty
# argument, which a plain list expansion would drop; the call is spelled out
# with quoted arguments for that reason.
string(REPLACE "|" ";" command_list "${COMMAND}")
set(quoted "")
foreach(arg IN LISTS command_list)
  if(arg STREQUAL "@EMPTY@")
    set(arg "")
  endif()
  string(REPLACE "\\" "\\\\" arg "${arg}")
  string(REPLACE "\"" "\\\"" arg "${arg}")
  string(REPLACE "$" "\\$" arg "${arg}")
  string(APPEND quoted " \"${arg}\"")
endforeach()
string(MD5 call_id "${quoted}")
set(call_file "${CMAKE_CURRENT_BINARY_DIR}/check_cli_exit_${call_id}.cmake")
file(WRITE "${call_file}"
     "execute_process(COMMAND${quoted} RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)\n")
include("${call_file}")
file(REMOVE "${call_file}")
if(NOT "${status}" STREQUAL "${EXPECT_EXIT}")
  message(FATAL_ERROR "expected exit ${EXPECT_EXIT}, got '${status}'\nstderr: ${err}")
endif()
if(DEFINED EXPECT_STDERR AND NOT err MATCHES "${EXPECT_STDERR}")
  message(FATAL_ERROR "stderr does not match '${EXPECT_STDERR}':\n${err}")
endif()
if(DEFINED EXPECT_STDOUT AND NOT out MATCHES "${EXPECT_STDOUT}")
  string(SUBSTRING "${out}" 0 600 head)
  message(FATAL_ERROR "stdout does not match '${EXPECT_STDOUT}':\n${head}")
endif()
if(DEFINED EXPECT_STDOUT_COUNT)
  string(REGEX MATCH "^(.*):([0-9]+)$" parsed "${EXPECT_STDOUT_COUNT}")
  set(pattern "${CMAKE_MATCH_1}")
  set(expected "${CMAKE_MATCH_2}")
  string(REGEX MATCHALL "${pattern}" matches "${out}")
  list(LENGTH matches actual)
  if(NOT actual EQUAL expected)
    message(FATAL_ERROR "expected ${expected} matches of '${pattern}', got ${actual}")
  endif()
endif()
