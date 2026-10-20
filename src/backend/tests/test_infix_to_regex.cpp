#include "minera/infix_to_regex.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {
void infix_to_regex_module_acceptance() {
  // Literal results from 34 actual public calls against pinned 2.7.22.
  { // public call 1
    const std::vector<std::string> input = {};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x24", 2));
    assert(input == before);
  }
  { // public call 2
    const std::vector<std::string> input = {std::string("\x2e\x2e\x2e", 3)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x28\x2c\x5b\x5e\x2c\x5d\x2a\x29\x2a\x24", 10));
    assert(input == before);
  }
  { // public call 3
    const std::vector<std::string> input = {std::string("\x41", 1)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x41\x24", 3));
    assert(input == before);
  }
  { // public call 4
    const std::vector<std::string> input = {std::string("\x41", 1), std::string("\x42", 1)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x41\x2c\x42\x24", 5));
    assert(input == before);
  }
  { // public call 5
    const std::vector<std::string> input = {std::string("\x2e\x2e\x2e", 3), std::string("\x41", 1)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x28\x5b\x5e\x2c\x5d\x2a\x2c\x29\x2a\x41\x24", 12));
    assert(input == before);
  }
  { // public call 6
    const std::vector<std::string> input = {std::string("\x41", 1), std::string("\x2e\x2e\x2e", 3)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x41\x28\x2c\x5b\x5e\x2c\x5d\x2a\x29\x2a\x24", 12));
    assert(input == before);
  }
  { // public call 7
    const std::vector<std::string> input = {std::string("\x2e\x2e\x2e", 3), std::string("\x41", 1), std::string("\x2e\x2e\x2e", 3)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x28\x5b\x5e\x2c\x5d\x2a\x2c\x29\x2a\x41\x28\x2c\x5b\x5e\x2c\x5d\x2a\x29\x2a\x24", 21));
    assert(input == before);
  }
  { // public call 8
    const std::vector<std::string> input = {std::string("\x41", 1), std::string("\x2e\x2e\x2e", 3), std::string("\x42", 1)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x41\x2c\x28\x5b\x5e\x2c\x5d\x2a\x2c\x29\x2a\x42\x24", 14));
    assert(input == before);
  }
  { // public call 9
    const std::vector<std::string> input = {std::string("\x2e\x2e\x2e", 3), std::string("\x2e\x2e\x2e", 3)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x28\x5b\x5e\x2c\x5d\x2a\x2c\x29\x28\x2c\x5b\x5e\x2c\x5d\x2a\x29\x2a\x24", 19));
    assert(input == before);
  }
  { // public call 10
    const std::vector<std::string> input = {std::string("\x41", 1), std::string("\x2e\x2e\x2e", 3), std::string("\x2e\x2e\x2e", 3), std::string("\x42", 1)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x41\x2c\x28\x5b\x5e\x2c\x5d\x2a\x2c\x29\x2a\x28\x5b\x5e\x2c\x5d\x2a\x2c\x29\x2a\x42\x24", 23));
    assert(input == before);
  }
  { // public call 11
    const std::vector<std::string> input = {std::string("", 0)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x24", 2));
    assert(input == before);
  }
  { // public call 12
    const std::vector<std::string> input = {std::string("", 0), std::string("\x41", 1)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x2c\x41\x24", 4));
    assert(input == before);
  }
  { // public call 13
    const std::vector<std::string> input = {std::string("\x41", 1), std::string("", 0)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x41\x2c\x24", 4));
    assert(input == before);
  }
  { // public call 14
    const std::vector<std::string> input = {std::string("\x41\x2c\x42", 3)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x41\x2c\x42\x24", 5));
    assert(input == before);
  }
  { // public call 15
    const std::vector<std::string> input = {std::string("\x20\x41\x20", 3)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x5c\x20\x41\x5c\x20\x24", 7));
    assert(input == before);
  }
  { // public call 16
    const std::vector<std::string> input = {std::string("\x20\x2e\x2e\x2e\x20", 5)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x5c\x20\x5c\x2e\x5c\x2e\x5c\x2e\x5c\x20\x24", 12));
    assert(input == before);
  }
  { // public call 17
    const std::vector<std::string> input = {std::string("\x61\x2e\x62", 3)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x61\x5c\x2e\x62\x24", 6));
    assert(input == before);
  }
  { // public call 18
    const std::vector<std::string> input = {std::string("\x61\x2b\x62", 3)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x61\x5c\x2b\x62\x24", 6));
    assert(input == before);
  }
  { // public call 19
    const std::vector<std::string> input = {std::string("\x61\x3f\x62", 3)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x61\x5c\x3f\x62\x24", 6));
    assert(input == before);
  }
  { // public call 20
    const std::vector<std::string> input = {std::string("\x61\x2a\x62", 3)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x61\x5c\x2a\x62\x24", 6));
    assert(input == before);
  }
  { // public call 21
    const std::vector<std::string> input = {std::string("\x5b\x78\x5d", 3)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x5c\x5b\x78\x5c\x5d\x24", 7));
    assert(input == before);
  }
  { // public call 22
    const std::vector<std::string> input = {std::string("\x28\x78\x29", 3)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x5c\x28\x78\x5c\x29\x24", 7));
    assert(input == before);
  }
  { // public call 23
    const std::vector<std::string> input = {std::string("\x7b\x78\x7d", 3)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x5c\x7b\x78\x5c\x7d\x24", 7));
    assert(input == before);
  }
  { // public call 24
    const std::vector<std::string> input = {std::string("\x5e\x41\x24", 3)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x5c\x5e\x41\x5c\x24\x24", 7));
    assert(input == before);
  }
  { // public call 25
    const std::vector<std::string> input = {std::string("\x61\x7c\x62", 3)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x61\x5c\x7c\x62\x24", 6));
    assert(input == before);
  }
  { // public call 26
    const std::vector<std::string> input = {std::string("\x61\x5c\x62", 3)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x61\x5c\x5c\x62\x24", 6));
    assert(input == before);
  }
  { // public call 27
    const std::vector<std::string> input = {std::string("\x61\x2d\x62", 3)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x61\x5c\x2d\x62\x24", 6));
    assert(input == before);
  }
  { // public call 28
    const std::vector<std::string> input = {std::string("\x41\x0a\x42", 3)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x41\x5c\x0a\x42\x24", 6));
    assert(input == before);
  }
  { // public call 29
    const std::vector<std::string> input = {std::string("\x41\x0d\x0a\x42", 4)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x41\x5c\x0d\x5c\x0a\x42\x24", 8));
    assert(input == before);
  }
  { // public call 30
    const std::vector<std::string> input = {std::string("\x41\x00\x42", 3)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x41\x00\x42\x24", 5));
    assert(input == before);
  }
  { // public call 31
    const std::vector<std::string> input = {std::string("\xce\xa9", 2), std::string("\xf0\x9f\x98\x80", 4)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\xce\xa9\x2c\xf0\x9f\x98\x80\x24", 9));
    assert(input == before);
  }
  { // public call 32
    const std::vector<std::string> input = {std::string("\x2e\x2e\x2e", 3), std::string("\x61\x2e\x62", 3), std::string("\x2e\x2e\x2e", 3), std::string("\x41\x2c\x42", 3), std::string("\x2e\x2e\x2e", 3)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x28\x5b\x5e\x2c\x5d\x2a\x2c\x29\x2a\x61\x5c\x2e\x62\x2c\x28\x5b\x5e\x2c\x5d\x2a\x2c\x29\x2a\x41\x2c\x42\x28\x2c\x5b\x5e\x2c\x5d\x2a\x29\x2a\x24", 37));
    assert(input == before);
  }
  { // public call 33
    const std::vector<std::string> input = {std::string("\x2e\x2e\x2e", 3), std::string("", 0), std::string("\x2e\x2e\x2e", 3)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x28\x5b\x5e\x2c\x5d\x2a\x2c\x29\x2a\x28\x2c\x5b\x5e\x2c\x5d\x2a\x29\x2a\x24", 20));
    assert(input == before);
  }
  { // public call 34
    const std::vector<std::string> input = {std::string("\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5a\x5b\x5c\x5d\x5e\x5f\x60\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7a\x7b\x7c\x7d\x7e\x7f\xc2\xa0\xe2\x80\x89", 133)};
    const auto before = input;
    assert(minera::translate_infix_to_regex(input) == std::string("\x5e\x00\x01\x02\x03\x04\x05\x06\x07\x08\x5c\x09\x5c\x0a\x5c\x0b\x5c\x0c\x5c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x5c\x20\x21\x22\x5c\x23\x5c\x24\x25\x5c\x26\x27\x5c\x28\x5c\x29\x5c\x2a\x5c\x2b\x2c\x5c\x2d\x5c\x2e\x2f\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x3e\x5c\x3f\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5a\x5c\x5b\x5c\x5c\x5c\x5d\x5c\x5e\x5f\x60\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7a\x5c\x7b\x5c\x7c\x5c\x7d\x5c\x7e\x7f\xc2\xa0\xe2\x80\x89\x24", 159));
    assert(input == before);
  }
}
void native_byte_extension() {
  const std::string raw("\xff\x00\xed\xa0\x80", 5);
  assert(minera::translate_infix_to_regex({raw}) == "^" + raw + "$" );
}
}  // namespace

int main() {
  infix_to_regex_module_acceptance();
  native_byte_extension();
}
