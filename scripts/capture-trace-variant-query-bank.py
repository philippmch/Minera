"""Capture the public bank for algo.anonymization.trace_variant_query.

Seeded calls of the public pieces of the package, with the Python and the
numpy global generators seeded before each call:

- util.exp_mech.exp_mech on np.arange(n) (the drawn element);
- util.behavioralAppropriateness.getBARelations on seeded traces (events in
  first-appearance order) and getBAViolations on seeded prefixes;
- util.util.generate_pm4py_log on seeded prefix frequencies;
- algorithm.apply with variant SACOFA on seeded event logs (labels a..e,
  repeated variants, some empty traces) and parameters epsilon, k, p, the
  returned dataframe read per case;
- algorithm.apply with variant LAPLACE (which raises in the pinned release).

Every call runs in fresh interpreters under two hash seeds, which must agree.

The bank is a gzip text file of tagged lines read by
minera_trace_variant_query_parity_tests. A trace is its labels written
together, '-' for the empty trace; '$' stands for TRACE_END in a prefix:

  E n epsilon seed index
  B traces | follows | precedes | prefix=violations ...
      (relations row-major over the events, one letter A/S/N per pair)
  G key=count ... | case=trace ...
  S traces epsilon k p seed | case=trace ...   or   | ! Type message
  L traces epsilon k p | ! Type message
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
HASH_SEEDS = (0, 1)
ROOT = Path(__file__).resolve().parent.parent
EPSILONS = [0.01, 0.05, 0.1, 0.3, 0.5, 1.0, 2.0, 5.0, 20.0, 300.0]


def word(r, alphabet, low, high):
    return ''.join(r.choice(alphabet) for _ in range(r.randint(low, high)))


def log(r):
    alphabet = 'abcde'[:r.choice([1, 2, 3, 3, 4, 4, 5])]
    variants = [word(r, alphabet, 0 if r.random() < 0.08 else 1, 5) for _ in range(r.randint(1, 5))]
    out = [v for v in variants for _ in range(r.randint(1, 7))]
    r.shuffle(out)
    return out


def generate(count):
    r = random.Random(BANK_SEED)
    cases = []
    for _ in range(count * 2):
        cases.append({'kind': 'E', 'n': r.choice([1, 2, 3, 5, 10, 40, 150, 290, 600]) if r.random() < 0.3 else r.randint(1, 30),
                      'epsilon': r.choice(EPSILONS), 'seed': r.randrange(2 ** 32)})
    for _ in range(count // 2):
        traces = log(r)
        events = sorted({e for t in traces for e in t})
        prefixes = []
        for _ in range(8):
            if not events:
                break
            prefix = list(word(r, ''.join(events), 1, 5))
            if r.random() < 0.4:
                prefix.append('TRACE_END')
            prefixes.append(prefix)
        cases.append({'kind': 'B', 'traces': traces, 'prefixes': prefixes})
    for _ in range(count // 4):
        freqs = []
        for _ in range(r.randint(0, 5)):
            key = '>>>'.join(word(r, 'abc', 0, 3))
            if not key or r.random() < 0.5:
                key = key + '>>>TRACE_END' if key else 'TRACE_END'
            freqs.append([key, r.randint(0, 3)])
        cases.append({'kind': 'G', 'freqs': freqs})
    for _ in range(count * 2):
        cases.append({'kind': 'S', 'traces': log(r) if r.random() > 0.02 else [], 'epsilon': r.choice(EPSILONS),
                      'k': r.choice([0, 1, 1, 2, 2, 3, 3, 4, 5]), 'p': r.choice([0, 1, 1, 2, 3, 5]),
                      'seed': r.randrange(2 ** 32)})
    for _ in range(20):
        cases.append({'kind': 'L', 'traces': log(r), 'epsilon': r.choice(EPSILONS), 'k': r.randint(0, 3), 'p': r.randint(0, 3)})
    return cases


CHILD = r'''
import contextlib, datetime, io, json, random, sys, warnings
warnings.filterwarnings('ignore')
import numpy as np
from pm4py.objects.log.obj import Event, EventLog, Trace
from pm4py.algo.anonymization.trace_variant_query import algorithm
from pm4py.algo.anonymization.trace_variant_query.util import behavioralAppropriateness as ba
from pm4py.algo.anonymization.trace_variant_query.util import exp_mech
from pm4py.algo.anonymization.trace_variant_query.util.util import generate_pm4py_log
origin = datetime.datetime(2024, 1, 1, tzinfo=datetime.timezone.utc)
def event_log(traces):
    log, minute = EventLog(), 0
    for n, trace in enumerate(traces):
        t = Trace()
        t.attributes['concept:name'] = f'c{n}'
        for activity in trace:
            t.append(Event({'concept:name': activity, 'time:timestamp': origin + datetime.timedelta(minutes=minute)}))
            minute += 1
        log.append(t)
    return log
def error(e):
    return ['!', type(e).__name__, str(e)]
out = []
for case in json.load(sys.stdin):
    kind = case['kind']
    if kind == 'E':
        np.random.seed(case['seed'])
        out.append(int(exp_mech.exp_mech(np.arange(case['n']), case['epsilon'])))
    elif kind == 'B':
        traces = [list(t) for t in case['traces']]
        events = list(dict.fromkeys(e for t in traces for e in t))
        follows, precedes = ba.getBARelations(traces, events)
        violations = [ba.getBAViolations(events + ['TRACE_END'], follows, precedes, p, 'TRACE_END') for p in case['prefixes']]
        out.append([events, ''.join(follows[(x, y)] for x in events for y in events),
                    ''.join(precedes[(x, y)] for x in events for y in events), violations])
    elif kind == 'G':
        log = generate_pm4py_log(dict(case['freqs']))
        cases = []
        for t in log:
            times = {str(e['time:timestamp']) for e in t}
            assert times <= {'1970-01-01 00:00:00+00:00'}, times
            cases.append([t.attributes['concept:name'], [e['concept:name'] for e in t]])
        out.append(cases)
    else:
        variant = algorithm.Variants.SACOFA if kind == 'S' else algorithm.Variants.LAPLACE
        params = {'epsilon': case['epsilon'], 'k': case['k'], 'p': case['p'], 'show_progress_bar': False}
        if kind == 'S':
            random.seed(case['seed'])
            np.random.seed(case['seed'])
        try:
            with contextlib.redirect_stderr(io.StringIO()):
                frame = algorithm.apply(event_log(case['traces']), variant=variant, parameters=params)
            if frame.shape[0] == 0:
                assert frame.shape == (0, 0), frame.shape
                out.append([])
                continue
            assert list(frame.columns) == ['concept:name', 'time:timestamp', 'case:concept:name'], list(frame.columns)
            assert {str(t) for t in frame['time:timestamp']} <= {'1970-01-01 00:00:00+00:00'}
            out.append([[int(c), list(g['concept:name'])] for c, g in frame.groupby('case:concept:name', sort=False)])
        except Exception as e:
            out.append(error(e))
print(json.dumps(out))
'''


def capture(cases, seed):
    env = dict(os.environ, PYTHONHASHSEED=str(seed))
    result = subprocess.run([sys.executable, '-I', '-B', '-c', CHILD], input=json.dumps(cases),
                            capture_output=True, text=True, env=env)
    if result.returncode:
        raise SystemExit(result.stderr[-3000:])
    return json.loads(result.stdout.strip().splitlines()[-1])


def traces_text(traces):
    return ','.join(t or '-' for t in traces) or '.'


def prefix_text(prefix):
    return ''.join('$' if e == 'TRACE_END' else e for e in prefix)


def cases_text(value):
    if value and value[0] == '!':
        return f'! {value[1]} {value[2]}'
    return ' '.join(f'{c}={"".join(t) or "-"}' for c, t in value)


def line(case, value):
    kind = case['kind']
    if kind == 'E':
        return f'E {case["n"]} {case["epsilon"]!r} {case["seed"]} {value}'
    if kind == 'B':
        events, follows, precedes, violations = value
        pairs = ' '.join(f'{prefix_text(p)}={v}' for p, v in zip(case['prefixes'], violations))
        return f'B {traces_text(case["traces"])} | {follows or "."} | {precedes or "."} | {pairs}'.rstrip()
    if kind == 'G':
        freqs = ' '.join(f'{k}={c}' for k, c in case['freqs']) or '.'
        return f'G {freqs} | {cases_text(value)}'.rstrip()
    if kind == 'S':
        return (f'S {traces_text(case["traces"])} {case["epsilon"]!r} {case["k"]} {case["p"]} {case["seed"]} | '
                f'{cases_text(value)}').rstrip()
    return f'L {traces_text(case["traces"])} {case["epsilon"]!r} {case["k"]} {case["p"]} | {cases_text(value)}'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--count', type=int, default=1200)
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/trace-variant-query-bank.txt.gz')
    args = parser.parse_args()
    cases = generate(args.count)
    runs = [capture(cases, seed) for seed in HASH_SEEDS]
    lines, summary = [], {}
    for index, case in enumerate(cases):
        seen = [run[index] for run in runs]
        if any(value != seen[0] for value in seen[1:]):
            raise SystemExit(f'hash seeds disagree on {case}')
        value = seen[0]
        key = case['kind'] + (' error' if isinstance(value, list) and value and value[0] == '!' else '')
        summary[key] = summary.get(key, 0) + 1
        lines.append(line(case, value))
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0, filename='') as stream:
        stream.write(('\n'.join(lines) + '\n').encode())
    print(json.dumps(dict(sorted(summary.items()))))


if __name__ == '__main__':
    main()
