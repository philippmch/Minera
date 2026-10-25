"""Capture the public bank for algo.querying.llm.utils.sql_utils.

Seeded texts (ASCII, control characters, letters and digits of other
scripts, symbols, emoji, non-ASCII spaces) go through the public
mask_non_alphanumeric; masked texts, mutated masked texts and handmade
marker sequences (signs, underscores, spaces, non-ASCII digits, large and
out-of-range code points) go through restore_non_alphanumeric. The calls run
in a fresh interpreter under two hash seeds, which must agree.

The bank is a gzip text file of tagged lines read by
minera_llm_sql_utils_parity_tests; texts are lists of code points:

  M [input, output]
  R [input, output]  or  R [input, {"error": [type, message code points]}]
"""
import argparse
import gzip
import json
import os
from pathlib import Path
import random
import subprocess
import sys

BANK_SEED = 20261025
ROOT = Path(__file__).resolve().parent.parent
POOL = (list(range(0x20, 0x7f)) * 3 + list(range(0, 0x20)) + [0x7f, 0x85, 0xa0, 0xad, 0xb2, 0xb5, 0xbc, 0xc0, 0xdf,
        0xe9, 0xf7, 0x2160, 0x2003, 0x3000, 0x3007, 0x4e2d, 0x0663, 0x06f5, 0x0966, 0xff16, 0x1d7d8, 0x0301, 0x200b,
        0x2028, 0x1f600, 0x1f1e9, 0x20ac, 0x00b9, 0x2460, 0x0e51, 0x10fffd, 0xfeff, 0x0370, 0x03a9, 0x05d0, 0x0627])
MARK = 'ordabcchr'
PIECES = ['ordabcchr', 'ordabc', 'ordabcch', 'ORDABCCHR', 'chr', '65', '0', '-1', '+66', '6_7', '6__7', '_6', '6_', ' 72 ',
          '\t9\n', '٣٢', '６５', ' ', '　', '\x1c', '55296', '57343', '1114111', '1114112',
          '2147483647', '2147483648', '-2147483649', '99999999999999999999', '0x41', '65.0', '1e3', 'x', '', '9' * 4301,
          '1_' * 2200 + '1', "'", '"', '\\']


def text_case(r):
    return ''.join(chr(r.choice(POOL)) for _ in range(r.randint(0, 12)))


def restore_case(r, masked):
    kind = r.random()
    if kind < 0.35:
        return masked
    if kind < 0.6 and masked:
        chars = list(masked)
        for _ in range(r.randint(1, 3)):
            at = r.randrange(len(chars) + 1)
            chars[at:at] = list(r.choice(PIECES))
        return ''.join(chars)
    out = []
    for _ in range(r.randint(1, 4)):
        if r.random() < 0.6:
            out.append(MARK + 'ordabc' + r.choice(PIECES) + MARK)
        else:
            out.append(r.choice(PIECES + [text_case(r)]))
    return ''.join(out)


CHILD = r'''
import json, sys, warnings
warnings.filterwarnings('ignore')
from pm4py.algo.querying.llm.utils import sql_utils
out = []
for kind, codes in json.load(sys.stdin):
    text = ''.join(map(chr, codes))
    function = sql_utils.mask_non_alphanumeric if kind == 'M' else sql_utils.restore_non_alphanumeric
    try:
        out.append([ord(c) for c in function(text)])
    except Exception as error:
        out.append({'error': [type(error).__name__, [ord(c) for c in str(error)]]})
print(json.dumps(out))
'''


def capture(calls, seed):
    env = dict(os.environ, PYTHONHASHSEED=str(seed))
    result = subprocess.run([sys.executable, '-I', '-B', '-c', CHILD], input=json.dumps(calls),
                            capture_output=True, text=True, env=env)
    if result.returncode:
        raise SystemExit(result.stderr[-3000:])
    return json.loads(result.stdout.strip().splitlines()[-1])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--count', type=int, default=3000)
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/llm-sql-utils-bank.txt.gz')
    args = parser.parse_args()
    r = random.Random(BANK_SEED)
    texts = [text_case(r) for _ in range(args.count)]
    calls = [['M', [ord(c) for c in t]] for t in texts]
    masked = capture(calls, 0)
    calls += [['R', [ord(c) for c in restore_case(r, ''.join(map(chr, m)))]] for m in masked]
    runs = [capture(calls, seed) for seed in (0, 1)]
    if runs[0] != runs[1]:
        raise SystemExit('hash-seed dependent results')
    lines, summary = [], {}
    for (kind, codes), result in zip(calls, runs[0]):
        key = kind + (':error' if isinstance(result, dict) else '')
        summary[key] = summary.get(key, 0) + 1
        lines.append(kind + ' ' + json.dumps([codes, result], separators=(',', ':'), ensure_ascii=True))
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0, filename='') as stream:
        stream.write(('\n'.join(lines) + '\n').encode())
    print(json.dumps(dict(sorted(summary.items()))))


if __name__ == '__main__':
    main()
