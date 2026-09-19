#undef NDEBUG
#include "token_replay_diagnostics_dataframe_reference_cases.hpp"
#include <cstdio>
#include <exception>
int main() {
  try { check_public_replay_dataframe_diagnostics(); }
  catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
  return 0;
}
