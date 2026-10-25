"""Capture the public bank for algo.anonymization.pripel.

The pinned requirements of the reference do not install diffprivlib, which
util.AttributeAnonymizer imports at module level. Each module of the package
is imported in a fresh interpreter under two hash seeds (which must agree);
the public entry points are then called on a seeded log when the import
succeeds. The bank is a text file of lines read by
minera_pripel_reference_parity_tests:

  module | ! Type message   or   module | ok
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
MODULES = ['algorithm', 'variants.pripel', 'util.AttributeAnonymizer', 'util.TraceMatcher']

CHILD = r'''
import datetime, importlib, sys, warnings
warnings.filterwarnings('ignore')
from pm4py.objects.log.obj import Event, EventLog, Trace
name = sys.argv[1]
try:
    module = importlib.import_module('pm4py.algo.anonymization.pripel.' + name)
    origin = datetime.datetime(2024, 1, 1, tzinfo=datetime.timezone.utc)
    log = EventLog()
    for n, word in enumerate(['abc', 'abd', 'abc', 'ac']):
        t = Trace()
        t.attributes['concept:name'] = f'c{n}'
        for i, a in enumerate(word):
            t.append(Event({'concept:name': a, 'time:timestamp': origin + datetime.timedelta(minutes=10 * n + i)}))
        log.append(t)
    if name in ('algorithm', 'variants.pripel'):
        module.apply(log, log, parameters={'epsilon': 1.0})
    print('ok')
except Exception as error:
    print('!', type(error).__name__, error)
'''


def capture(name, seed):
    env = dict(os.environ, PYTHONHASHSEED=str(seed))
    result = subprocess.run([sys.executable, '-I', '-B', '-c', CHILD, name], capture_output=True, text=True, env=env)
    if result.returncode:
        raise SystemExit(result.stderr[-3000:])
    return result.stdout.strip().splitlines()[-1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/pripel-reference-bank.txt')
    args = parser.parse_args()
    lines = []
    for name in MODULES:
        seen = {capture(name, seed) for seed in (0, 1)}
        if len(seen) != 1:
            raise SystemExit(f'hash seeds disagree on {name}: {seen}')
        lines.append(f'{name} | {seen.pop()}')
    args.output.write_text('\n'.join(lines) + '\n')
    print('\n'.join(lines))


if __name__ == '__main__':
    main()
