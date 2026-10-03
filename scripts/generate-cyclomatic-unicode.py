"""Generate the Unicode word classifier from public Python character predicates."""
import hashlib
import json
from pathlib import Path
import sys
import unicodedata

root = Path(__file__).resolve().parent.parent
assert sys.version_info[:3] == (3, 11, 9) and unicodedata.unidata_version == '14.0.0'
ranges = []
first = None
for codepoint in range(0x110000):
    accepted = chr(codepoint).isalnum() or codepoint == 0x5f
    if accepted and first is None: first = codepoint
    if not accepted and first is not None:
        ranges.append((first, codepoint - 1))
        first = None
if first is not None: ranges.append((first, 0x10ffff))
lines = ['#pragma once', '', '// Generated from public Python 3.11 / Unicode 14.0.0 character predicates.',
         '// Word characters are alphanumeric characters and underscore.', '#include <array>', '#include <cstddef>', '',
         'namespace minera {', 'inline bool cyclomatic_unicode_word(char32_t codepoint) {',
         '  static constexpr std::array<std::array<char32_t, 2>, ' + str(len(ranges)) + '> ranges{{']
lines.extend('    {0x' + format(first, 'x') + ', 0x' + format(last, 'x') + '},' for first, last in ranges)
lines.extend(['  }};', '  std::size_t first = 0, last = ranges.size();', '  while (first < last) {',
              '    const auto middle = first + (last - first) / 2;',
              '    if (ranges[middle][1] < codepoint) first = middle + 1;', '    else last = middle;', '  }',
              '  return first < ranges.size() && codepoint >= ranges[first][0];', '}', '} // namespace minera', ''])
target = root / 'src/backend/cyclomatic_unicode.hpp'
assert not target.exists()
target.write_text('\n'.join(lines), encoding='utf-8', newline='\n')
for codepoint in range(0x110000):
    low, high = 0, len(ranges)
    while low < high:
        middle = low + (high - low) // 2
        if ranges[middle][1] < codepoint: low = middle + 1
        else: high = middle
    observed = low < len(ranges) and codepoint >= ranges[low][0]
    assert observed == (chr(codepoint).isalnum() or codepoint == 0x5f)
record = {'python': sys.version, 'unicode_version': unicodedata.unidata_version,
          'generator_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
          'header_sha256': hashlib.sha256(target.read_bytes()).hexdigest(), 'range_count': len(ranges),
          'classification_codepoints': 0x110000, 'round_trip_classifications': 0x110000,
          'pm4py_calls': 0, 'scope': 'Public Python str.isalnum character results plus underscore; no library implementation or bytecode inspection.'}
with (root / 'build/cyclomatic-unicode-generation.json').open('x', encoding='utf-8', newline='\n') as stream:
    json.dump(record, stream, ensure_ascii=True, sort_keys=True, indent=2)
    stream.write('\n')
print(json.dumps(record), flush=True)
