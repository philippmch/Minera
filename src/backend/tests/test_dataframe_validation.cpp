#undef NDEBUG
#include "minera/dataframe_validation.hpp"
#include "minera/conformance.hpp"
#include "dataframe_column_validation_reference_cases.hpp"
#include <cstdio>
#include <exception>
int main() {
  try {
    check_public_dataframe_validation();
  } catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
  return 0;
}
