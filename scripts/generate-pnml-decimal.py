"""Generate the owned Unicode 14 decimal/accepted integer-space classifier."""
import hashlib
import json
from pathlib import Path
import sys
import unicodedata

ROOT = Path(__file__).resolve().parent.parent
assert sys.version_info[:3] == (3, 11, 9) and unicodedata.unidata_version == '14.0.0'
values = [unicodedata.decimal(chr(code), None) for code in range(0x110000)]
starts = [code for code, value in enumerate(values) if value == 0]
assert all(values[start:start + 10] == list(range(10)) for start in starts)
assert sum(value is not None for value in values) == len(starts) * 10
spaces = []
space_calls = 0
for code in range(0x110000):
    if not chr(code).isspace(): continue
    space_calls += 2
    try: leading = int(chr(code) + '1')
    except ValueError: leading = None
    try: trailing = int('1' + chr(code))
    except ValueError: trailing = None
    assert leading == trailing
    if leading == 1: spaces.append(code)
source = '''#pragma once
// Owned classifier generated against public Python 3.11 Unicode 14 operations.
#include <cstdint>
namespace minera::detail {
inline int pnml_decimal_digit(std::uint32_t codepoint) {
  constexpr std::uint32_t starts[] = {STARTS};
  for (const auto start : starts)
    if (codepoint >= start && codepoint - start < 10)
      return static_cast<int>(codepoint - start);
  return -1;
}
inline bool pnml_integer_space(std::uint32_t codepoint) {
  switch (codepoint) {
CASES
      return true;
    default: return false;
  }
}
}  // namespace minera::detail
'''.replace('STARTS', ','.join(hex(start) for start in starts)).replace('CASES', '\n'.join('    case ' + hex(code) + ':' for code in spaces))
header = ROOT / 'src/backend/pnml_decimal.hpp'
assert not header.exists()
header.write_text(source, encoding='utf-8', newline='\n')
classified = {start + digit: digit for start in starts for digit in range(10)}
assert all(classified.get(code) == value for code, value in enumerate(values))
record = {'python': sys.version, 'unicode': unicodedata.unidata_version, 'classification_codepoints': len(values),
    'round_trip_classifications': len(values), 'decimal_codepoints': len(classified), 'decimal_blocks': len(starts),
    'integer_space_codepoints': spaces, 'integer_space_calls': space_calls, 'pm4py_calls': 0,
    'generator_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), 'header_sha256': hashlib.sha256(header.read_bytes()).hexdigest()}
with (ROOT / 'build/pnml-extended-annotations-decimal-generation.json').open('x', encoding='utf-8') as stream:
    json.dump(record, stream, ensure_ascii=True, sort_keys=True, indent=2)
    stream.write('\n')
print(json.dumps({key: record[key] for key in ['classification_codepoints', 'decimal_codepoints', 'decimal_blocks', 'integer_space_calls', 'pm4py_calls']}))
