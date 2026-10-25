"""Capture the public bank for algo.conformance.alignments.process_tree.variants.milp.

Seeded process trees (sequence, choice, parallel and loop operators, labels
a..e with repeats, some tau leaves) go through
pm4py.objects.process_tree.utils.generic.parse and ProcessTreeAligner. Per tree
the bank keeps the flow graph the aligner builds (its networkx node order and
data, and its edge order with label, capacity and cost) and, per trace (the
empty one, model walks and walks with a deleted or inserted event), the
program ProcessTreeAligner.align hands pm4py.util.lp.solver.apply (objective,
bounds, integrality, equality and inequality rows), the solution vector it
reads back and the (cost, alignment) it returns. The public apply over the
same traces adds cost, alignment and fitness per trace. Everything runs in
fresh interpreters under four hash seeds, which must agree.

The bank is a gzip text file of tagged lines read by
minera_process_tree_milp_parity_tests:

  C name
  T tree text
  G json   [[[node, {data}], ...], [[from, to, label or null, capacity, cost], ...]]
  L json   [trace, program or null, solution or null (null entries where the
           solver reports none), cost or null or "raises", alignment]
           with program {"c", "lb", "ub", "int", "eq": [[[col, value], ...]],
           "beq", "aub": [[[col, value], ...]], "bub"}
  P json   [[cost, alignment, fitness], ...] from apply ("raises" when it
           raises)
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

BANK_SEED = 20261027
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
    out = [[]]
    for kind in range(4):
        trace = []
        walk(r, node, trace, [2])
        trace = trace[:6]
        if kind >= 2:
            if r.random() < 0.5 and trace:
                del trace[r.randrange(len(trace))]
            else:
                trace.insert(r.randint(0, len(trace)), r.choice(LABELS + 'x'))
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
import numpy as np
from pm4py.objects.log.obj import Event, EventLog, Trace
from pm4py.objects.process_tree.utils.generic import parse
from pm4py.algo.conformance.alignments.process_tree.variants import milp
origin = datetime.datetime(2024, 1, 1, tzinfo=datetime.timezone.utc)
solve = milp.solver.apply
read = milp.solver.get_points_from_sol
seen = {}
def rows(matrix):
    matrix = np.array(matrix)
    if matrix.size == 0:
        return [[] for _ in range(matrix.shape[0])] if matrix.ndim == 2 and matrix.shape[1] == 0 else []
    return [[[int(j), float(row[j])] for j in np.nonzero(row)[0]] for row in matrix]
def capture(c, Aub, bub, Aeq, beq, variant=None, parameters=None):
    bounds = parameters['bounds']
    seen['program'] = {'c': [float(x) for x in c], 'lb': [float(b[0]) for b in bounds],
                       'ub': [float(b[1]) for b in bounds], 'int': [int(x) for x in parameters['integrality']],
                       'eq': rows(Aeq), 'beq': [float(x) for x in beq], 'aub': rows(Aub), 'bub': [float(x) for x in bub]}
    result = solve(c, Aub, bub, Aeq, beq, variant=variant, parameters=parameters)
    seen['result'] = result
    return result
def points(result, variant=None, parameters=None):
    values = read(result, variant=variant, parameters=parameters)
    seen['solution'] = None if values is None else [None if x is None else float(x) for x in values]
    return values
milp.solver.apply = capture
milp.solver.get_points_from_sol = points
def moves(alignment):
    return [[a, b] for a, b in alignment]
out = []
for case in json.load(sys.stdin):
    aligner = milp.ProcessTreeAligner(parse(case['tree']))
    graph = [[[n, d] for n, d in aligner.graph.nodes(data=True)],
             [[u, v, d['label'], d['capacity'], d['cost']] for u, v, d in aligner.graph.edges(data=True)]]
    records = []
    for trace in case['traces']:
        seen.clear()
        try:
            cost, alignment = aligner.align(tuple(trace))
            records.append([trace, seen.get('program'), seen.get('solution'), cost, moves(alignment)])
        except Exception:
            records.append([trace, seen.get('program'), seen.get('solution'), 'raises', []])
    log, minute = EventLog(), 0
    for k, trace in enumerate(case['traces']):
        t = Trace()
        t.attributes['concept:name'] = f'c{k}'
        for activity in trace:
            t.append(Event({'concept:name': activity, 'time:timestamp': origin + datetime.timedelta(minutes=minute)}))
            minute += 1
        log.append(t)
    try:
        with contextlib.redirect_stderr(io.StringIO()):
            aligned = milp.apply(log, parse(case['tree']), parameters={'show_progress_bar': False})
        public = [[a['cost'], moves(a['alignment']), a['fitness']] for a in aligned]
    except Exception:
        public = 'raises'
    out.append({'graph': graph, 'records': records, 'public': public})
print(json.dumps(out))
'''


def capture(cases, seed):
    env = dict(os.environ, PYTHONHASHSEED=str(seed))
    result = subprocess.run([sys.executable, '-I', '-B', '-c', CHILD],
                            input=json.dumps(cases), capture_output=True, text=True, env=env)
    if result.returncode:
        raise SystemExit(result.stderr[-3000:])
    return json.loads(result.stdout.strip().splitlines()[-1])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--count', type=int, default=300)
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/process-tree-milp-bank.txt.gz')
    args = parser.parse_args()
    cases = generate(args.count)
    chunks = [cases[i:i + 25] for i in range(0, len(cases), 25)]
    jobs = [(chunk, seed) for seed in HASH_SEEDS for chunk in chunks]
    with concurrent.futures.ThreadPoolExecutor(8) as pool:
        results = list(pool.map(lambda job: capture(*job), jobs))
    per_seed = {}
    for (chunk, seed), result in zip(jobs, results):
        per_seed.setdefault(seed, []).extend(result)
    runs = [per_seed[seed] for seed in HASH_SEEDS]
    lines, summary = [], {}
    for index, case in enumerate(cases):
        seen = [run[index] for run in runs]
        if any(value != seen[0] for value in seen[1:]):
            summary['varies'] = summary.get('varies', 0) + 1
            continue
        item = seen[0]
        lines += [f'C {case["name"]}', f'T {case["tree"]}', 'G ' + json.dumps(item['graph'], separators=(',', ':'))]
        for record in item['records']:
            lines.append('L ' + json.dumps(record, separators=(',', ':')))
            summary['traces'] = summary.get('traces', 0) + 1
        lines.append('P ' + json.dumps(item['public'], separators=(',', ':')))
        summary['raises' if item['public'] == 'raises' else 'applied'] = \
            summary.get('raises' if item['public'] == 'raises' else 'applied', 0) + 1
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0, filename='') as stream:
        stream.write(('\n'.join(lines) + '\n').encode())
    print(json.dumps(dict(sorted(summary.items()))))


if __name__ == '__main__':
    main()
