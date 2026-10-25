// algo.anonymization.pripel against the public bank
// (docs/pripel-reference-bank.txt, scripts/capture-pripel-reference-bank.py):
// each module's outcome (error class and message) must equal the reference's.
#include "minera/pripel_reference.hpp"

#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace minera;

const std::vector<std::vector<std::string>> kLog{{"a", "b", "c"}, {"a", "b", "d"}, {"a", "b", "c"}, {"a", "c"}};

template <typename Call>
std::string outcome(Call&& call) {
  try {
    call();
    return "ok";
  } catch (const PripelReferenceError& error) {
    return std::string("! ") + error.type + ' ' + error.what();
  }
}

std::string check_pripel() {
  return outcome([] { reference_pripel_apply(kLog, kLog, 1.0); });
}

std::string check_attribute_anonymizer() {
  return outcome([] { reference_pripel_attribute_anonymizer(); });
}

std::string check_trace_matcher() {
  return outcome([] { reference_pripel_trace_matcher(kLog, kLog); });
}

}  // namespace

int main(int argc, char** argv) try {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " BANK.txt\n";
    return 2;
  }
  std::ifstream input(argv[1]);
  if (!input) throw std::runtime_error(std::string("cannot read ") + argv[1]);
  const std::map<std::string, std::string> checks{{"algorithm", check_pripel()},
                                                 {"variants.pripel", check_pripel()},
                                                 {"util.AttributeAnonymizer", check_attribute_anonymizer()},
                                                 {"util.TraceMatcher", check_trace_matcher()}};
  std::size_t calls = 0, failures = 0;
  std::string line;
  while (std::getline(input, line)) {
    const auto at = line.find(" | ");
    if (at == std::string::npos) continue;
    const auto expected = line.substr(at + 3);
    const auto& actual = checks.at(line.substr(0, at));
    ++calls;
    if (actual != expected && ++failures <= 20) std::cerr << line << "\n  got " << actual << '\n';
  }
  std::cout << calls << " pripel modules compared, " << failures << " failures\n";
  return failures || calls != checks.size() ? 1 : 0;
} catch (const std::exception& error) {
  std::cerr << "error: " << error.what() << "\n";
  return 1;
}
