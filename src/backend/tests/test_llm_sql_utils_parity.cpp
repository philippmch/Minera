// algo.querying.llm.utils.sql_utils against the public seeded bank
// (docs/llm-sql-utils-bank.txt.gz, scripts/capture-llm-sql-utils-bank.py):
// every masked text and every restored text or error (class and message)
// must equal the reference's.
#include "minera/llm_sql_utils.hpp"

#include "../gzip_codec.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace minera;

// The bank's lines are [[code points], [code points] | {"error": [type, [code points]]}].
struct Reader {
  const char* at;
  void skip() { while (*at == ' ' || *at == ',' || *at == ':') ++at; }
  std::string codes() {
    skip();
    ++at;  // [
    std::string out;
    for (skip(); *at != ']'; skip()) {
      char* end = nullptr;
      const auto code = static_cast<char32_t>(std::strtoul(at, &end, 10));
      at = end;
      if (code < 0x80) {
        out += static_cast<char>(code);
      } else if (code < 0x800) {
        out += static_cast<char>(0xc0 | (code >> 6));
        out += static_cast<char>(0x80 | (code & 0x3f));
      } else if (code < 0x10000) {
        out += static_cast<char>(0xe0 | (code >> 12));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (code & 0x3f));
      } else {
        out += static_cast<char>(0xf0 | (code >> 18));
        out += static_cast<char>(0x80 | ((code >> 12) & 0x3f));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (code & 0x3f));
      }
    }
    ++at;
    return out;
  }
  std::string word() {
    skip();
    const char* start = ++at;
    while (*at != '"') ++at;
    return std::string(start, at++);
  }
};

void check_mask(const std::string& input, const std::string& expected, std::size_t& failures) {
  const auto actual = mask_non_alphanumeric(input);
  if (actual != expected && ++failures <= 20)
    std::cerr << "mask " << input << ": expected " << expected << ", got " << actual << '\n';
}

void check_restore(const std::string& input, const std::string& expected, std::size_t& failures) {
  std::string actual;
  try {
    actual = restore_non_alphanumeric(input);
  } catch (const LlmSqlUtilsError& error) {
    actual = "error " + error.type + ": " + error.what();
  }
  if (actual != expected && ++failures <= 20)
    std::cerr << "restore " << input << ": expected " << expected << ", got " << actual << '\n';
}

}  // namespace

int main(int argc, char** argv) try {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " BANK.txt.gz\n";
    return 2;
  }
  std::ifstream input(argv[1], std::ios::binary);
  if (!input) throw std::runtime_error(std::string("cannot read ") + argv[1]);
  const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  std::istringstream text(detail::gzip_decompress(bytes, std::size_t{1} << 31));
  std::size_t calls = 0, failures = 0;
  std::string line;
  while (std::getline(text, line)) {
    if (line.size() < 3) continue;
    Reader reader{line.c_str() + 2};
    reader.skip();
    ++reader.at;  // outer [
    const auto call_input = reader.codes();
    reader.skip();
    std::string expected;
    if (*reader.at == '{') {
      ++reader.at;
      (void)reader.word();  // "error"
      reader.skip();
      ++reader.at;  // [
      const auto type = reader.word();
      expected = "error " + type + ": " + reader.codes();
    } else {
      expected = reader.codes();
    }
    ++calls;
    if (line[0] == 'M') {
      check_mask(call_input, expected, failures);
    } else {
      check_restore(call_input, expected, failures);
    }
  }
  std::cout << calls << " sql_utils calls compared, " << failures << " failures\n";
  return failures || calls == 0 ? 1 : 0;
} catch (const std::exception& error) {
  std::cerr << "error: " << error.what() << "\n";
  return 1;
}
