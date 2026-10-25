#pragma once

// algo.anonymization.pripel of the PM4Py 2.7.22 reference as installed with
// its pinned requirements: diffprivlib, which util.AttributeAnonymizer
// imports, is not among them, so importing the package or any of its modules
// (algorithm, variants.pripel, util.AttributeAnonymizer, util.TraceMatcher)
// raises ModuleNotFoundError "No module named 'diffprivlib'", whatever the
// input. These functions raise the same; the working PRIPEL of Minera is
// minera/pripel.hpp.

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace minera {

class PripelReferenceError : public std::runtime_error {
 public:
  PripelReferenceError(std::string type, const std::string& message)
      : std::runtime_error(message), type(std::move(type)) {}
  std::string type;  // the Python exception class
};

// algorithm.apply / variants.pripel.apply.
[[noreturn]] void reference_pripel_apply(const std::vector<std::vector<std::string>>& log,
                                         const std::vector<std::vector<std::string>>& trace_variant_query,
                                         double epsilon);

// util.AttributeAnonymizer.AttributeAnonymizer.
[[noreturn]] void reference_pripel_attribute_anonymizer();

// util.TraceMatcher.TraceMatcher.
[[noreturn]] void reference_pripel_trace_matcher(const std::vector<std::vector<std::string>>& trace_variant_query,
                                                 const std::vector<std::vector<std::string>>& log);

}  // namespace minera
