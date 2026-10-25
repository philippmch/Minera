"""Capture the public bank for algo.conformance.alignments.process_tree
DYNAMIC_PROGRAMMING (alignments included).

Seeded process trees (sequence, choice, parallel and loop operators, two or
three children, up to four levels, labels a..e with repeats, some tau
leaves) go through pm4py.objects.process_tree.utils.generic.parse and the
public algorithm.apply with variant DYNAMIC_PROGRAMMING on a log of eight
traces: random walks of the tree with and without noise and random words.
Every case runs in fresh interpreters under four hash seeds, which must
agree.

The bank is a gzip text file of tagged lines read by
minera_process_tree_dp_alignment_parity_tests:

  C name
  T tree text
  L json   the traces
  R json   per trace [cost, [[log, model], ...], fitness], or
           {"error": [type, message]}
"""
import argparse
import concurrent.futures
import gzip
import json
import os
from pathlib import Path
import random
import subprocess
import sys

BANK_SEED = 20261025
HASH_SEEDS = (0, 1, 2, 3)
ROOT = Path(__file__).resolve().parent.parent
LABELS = 'abcde'


def tree(r, depth=0):
    if depth >= 4 or r.random() < 0.25 + 0.15 * depth:
        return ('tau',) if r.random() < 0.15 else ('leaf', r.choice(LABELS))
    op = r.choice(['->', 'X', '+', '*'])
    count = 2 if op == '*' else r.choice([2, 2, 3])
    return (op, [tree(r, depth + 1) for _ in range(count)])


def text(node):
    if node[0] == 'tau':
        return 'tau'
    if node[0] == 'leaf':
        return f"'{node[1]}'"
    return f'{node[0]}( ' + ', '.join(text(child) for child in node[1]) + ' )'


def walk(r, node, out, budget):
    if node[0] == 'leaf':
        out.append(node[1])
    elif node[0] == '->':
        for child in node[1]:
            walk(r, child, out, budget)
    elif node[0] == 'X':
        walk(r, r.choice(node[1]), out, budget)
    elif node[0] == '+':
        parts = []
        for child in node[1]:
            part = []
            walk(r, child, part, budget)
            parts.append(part)
        while any(parts):
            part = r.choice([p for p in parts if p])
            out.append(part.pop(0))
    elif node[0] == '*':
        walk(r, node[1][0], out, budget)
        while budget[0] > 0 and r.random() < 0.4:
            budget[0] -= 1
            walk(r, node[1][1], out, budget)
            walk(r, node[1][0], out, budget)


def traces(r, node):
    out = []
    for kind in range(8):
        if kind < 5:
            trace = []
            walk(r, node, trace, [2])
            trace = trace[:7]
            if kind >= 2:
                for _ in range(r.randint(1, 2)):
                    action = r.random()
                    if action < 0.4 and trace:
                        del trace[r.randrange(len(trace))]
                    elif action < 0.8:
                        trace.insert(r.randint(0, len(trace)), r.choice(LABELS + 'x'))
                    elif len(trace) > 1:
                        i = r.randrange(len(trace) - 1)
                        trace[i], trace[i + 1] = trace[i + 1], trace[i]
        else:
            trace = [r.choice(LABELS + 'x') for _ in range(r.randint(0, 6))]
        out.append(trace)
    return out


def generate(count):
    r = random.Random(BANK_SEED)
    cases = []
    for index in range(count):
        node = tree(r)
        cases.append({'name': f'tree{index}', 'tree': text(node), 'traces': traces(r, node)})
    return cases


CHILD = r'''
import contextlib, datetime, io, json, sys, warnings
warnings.filterwarnings('ignore')
from pm4py.objects.log.obj import Event, EventLog, Trace
from pm4py.objects.process_tree.utils.generic import parse
from pm4py.algo.conformance.alignments.process_tree import algorithm
origin = datetime.datetime(2024, 1, 1, tzinfo=datetime.timezone.utc)
out = []
for case in json.load(sys.stdin):
    log, minute = EventLog(), 0
    for n, trace in enumerate(case['traces']):
        t = Trace()
        t.attributes['concept:name'] = f'c{n}'
        for activity in trace:
            t.append(Event({'concept:name': activity, 'time:timestamp': origin + datetime.timedelta(minutes=minute)}))
            minute += 1
        log.append(t)
    try:
        with contextlib.redirect_stderr(io.StringIO()):
            result = algorithm.apply(log, parse(case['tree']), variant=algorithm.Variants.DYNAMIC_PROGRAMMING)
        out.append([[float(a['cost']), [list(m) for m in a['alignment']], float(a['fitness'])] for a in result])
    except Exception as error:
        out.append({'error': [type(error).__name__, str(error)]})
print(json.dumps(out))
'''


def capture(cases, seed):
    env = dict(os.environ, PYTHONHASHSEED=str(seed))
    result = subprocess.run([sys.executable, '-I', '-B', '-c', CHILD], input=json.dumps(cases),
                            capture_output=True, text=True, env=env)
    if result.returncode:
        raise SystemExit(result.stderr[-3000:])
    return json.loads(result.stdout.strip().splitlines()[-1])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--count', type=int, default=1500)
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/process-tree-dp-alignment-bank.txt.gz')
    args = parser.parse_args()
    cases = generate(args.count)
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        runs = list(pool.map(lambda seed: capture(cases, seed), HASH_SEEDS))
    lines, summary = [], {}
    for index, case in enumerate(cases):
        seen = [run[index] for run in runs]
        if any(value != seen[0] for value in seen[1:]):
            summary['varies'] = summary.get('varies', 0) + 1
            continue
        key = 'error' if isinstance(seen[0], dict) else 'aligned'
        summary[key] = summary.get(key, 0) + 1
        lines += [f'C {case["name"]}', f'T {case["tree"]}', 'L ' + json.dumps(case['traces'], separators=(',', ':')),
                  'R ' + json.dumps(seen[0], separators=(',', ':'))]
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0, filename='') as stream:
        stream.write(('\n'.join(lines) + '\n').encode())
    print(json.dumps(dict(sorted(summary.items()))))


if __name__ == '__main__':
    main()
