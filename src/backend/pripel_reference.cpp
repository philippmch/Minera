#include "minera/pripel_reference.hpp"

namespace minera {
namespace {

[[noreturn]] void missing_diffprivlib() {
  throw PripelReferenceError("ModuleNotFoundError", "No module named 'diffprivlib'");
}

}  // namespace

void reference_pripel_apply(const std::vector<std::vector<std::string>>&, const std::vector<std::vector<std::string>>&,
                            double) {
  missing_diffprivlib();
}

void reference_pripel_attribute_anonymizer() { missing_diffprivlib(); }

void reference_pripel_trace_matcher(const std::vector<std::vector<std::string>>&,
                                    const std::vector<std::vector<std::string>>&) {
  missing_diffprivlib();
}

}  // namespace minera
