"""Capture the bank for algo.conformance.alignments.process_tree.variants.approximated.

Seeded n-ary process trees (sequence, choice, parallel and loop operators,
labels a..f with repeats, some tau leaves, a few single-child operators that
the reference rejects) go through pm4py.objects.process_tree.utils.generic
and the approximated original variant. Per tree the bank keeps the binary
tree process_tree_to_binary_process_tree makes, the activity, start-activity,
end-activity sets, tau flags and heights of its nodes (preorder), and one
public apply over an event log of the tree's traces under one of five
(max_trace_length, max_process_tree_height) settings. The apply is observed
from the outside of its helpers: every program handed to PuLP (objective,
constant, rows) with the solution read back, every choice, every call of
calculate_optimal_alignment with its result, and every operator's returned
alignment, in call order per aligned variant. The base alignments break ties
by object addresses, so each case runs once, under hash seed index % 4.

The bank is a gzip text file of tagged lines read by
minera_process_tree_approximated_parity_tests:

  C name
  T json   [tree text, binary tree text or "raises", max_trace_length,
           max_process_tree_height, the log's traces]
  N json   [[A, SA, EA, tau, height], ...] per binary node in preorder
  V json   [trace, [event, ...]] per aligned variant, events being
           ["L", node, op, trace, objective, constant, rows, solution, status]
             (rows [[[name, coef], ...], sense, constant] as PuLP keeps
             them, sense -1 <=, 0 =, 1 >=; solution [[name, value], ...] for
             the nonzero values)
           ["C", node, trace, picked child's node]
           ["B", node, trace, alignment or null]
           ["R", node, op, alignment or null]
  P json   [[alignment, cost, fitness] or null, ...] per log trace, or
           "raises"
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

BANK_SEED = 20261028
HASH_SEEDS = (0, 1, 2, 3)
ROOT = Path(__file__).resolve().parent.parent
LABELS = 'abcdef'
SETTINGS = ((1, 1), (1, 1), (0, 1), (2, 1), (1, 2), (3, 2), (1, 0))


def tree(r, depth=0):
    if depth >= 4 or r.random() < 0.2 + 0.15 * depth:
        return ('tau',) if r.random() < 0.12 else ('leaf', r.choice(LABELS))
    op = r.choice(['->', 'X', '+', '*'])
    count = r.choice([2, 2, 3]) if op != '*' else r.choice([2, 2, 2, 3])
    if r.random() < 0.01:
        count = 1
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
        while budget[0] > 0 and r.random() < 0.45:
            budget[0] -= 1
            walk(r, r.choice(node[1][1:] or node[1]), out, budget)
            walk(r, node[1][0], out, budget)


def traces(r, node):
    out = []
    for kind in range(5):
        trace = []
        if kind == 4:
            trace = [r.choice(LABELS + 'z') for _ in range(r.randint(0, 6))]
        else:
            walk(r, node, trace, [3])
            trace = trace[:8]
            for _ in range(kind):
                if r.random() < 0.5 and trace:
                    del trace[r.randrange(len(trace))]
                else:
                    trace.insert(r.randint(0, len(trace)), r.choice(LABELS + 'z'))
        out.append(trace)
    if r.random() < 0.3:
        out.append(list(r.choice(out)))
    if r.random() < 0.2:
        out.append([])
    return out


def generate(count):
    r = random.Random(BANK_SEED)
    cases = []
    for index in range(count):
        node = tree(r)
        cases.append({'name': f'tree{index}', 'tree': text(node), 'traces': traces(r, node),
                      'setting': r.choice(SETTINGS), 'seed': HASH_SEEDS[index % len(HASH_SEEDS)]})
    return cases


CHILD = r'''
import contextlib, copy, io, json, sys, warnings
warnings.filterwarnings('ignore')
import pulp
from pm4py.objects.log.obj import Event, EventLog, Trace
from pm4py.objects.process_tree.utils.generic import parse, process_tree_to_binary_process_tree, get_process_tree_height
from pm4py.algo.conformance.alignments.process_tree.variants.approximated import original
from pm4py.algo.conformance.alignments.process_tree.variants.approximated.calculate_a_sa_ea_sets import initialize_a_sa_ea_tau_sets
G = original.__dict__
state = {'index': None, 'events': None, 'variants': []}
def names(trace):
    return [e['concept:name'] for e in trace]
def preorder(root):
    order, stack = {}, [root]
    while stack:
        node = stack.pop()
        order[id(node)] = len(order)
        stack.extend(reversed(node.children))
    return order
def moves(alignment):
    return None if alignment is None else [[a, b] for a, b in alignment]
solve = pulp.LpProblem.solve
def capture_solve(self, *args, **kwargs):
    result = solve(self, *args, **kwargs)
    rows = [[[[v.name, k] for v, k in c.items()], c.sense, c.constant] for c in self.constraints.values()]
    solution = [[v.name, v.varValue] for v in self.variables() if v.varValue]
    mark, head = state['ops'][-1]
    state['events'][mark] = head + [[[v.name, k] for v, k in self.objective.items()], self.objective.constant,
                                    rows, solution, self.status]
    return result
pulp.LpProblem.solve = capture_solve
for op in ('sequence', 'loop', 'parallel', 'choice'):
    inner = G['__approximate_alignment_on_' + op]
    def wrapped(pt, trace, a, sa, ea, tf, tl, th, parameters=None, inner=inner, op=op):
        node, events = state['index'][id(pt)], state['events']
        if op == 'choice':
            state['choice'] = node
        else:
            state['ops'].append([len(events), ['L', node, op, names(trace)]])
            events.append(None)
        try:
            result = inner(pt, trace, a, sa, ea, tf, tl, th, parameters)
        finally:
            if op != 'choice':
                state['ops'].pop()
        events.append(['R', node, op, moves(result)])
        return result
    G['__approximate_alignment_on_' + op] = wrapped
for_trace = G['__approximate_alignment_for_trace']
def capture_for_trace(pt, a, sa, ea, tf, trace, tl, th, parameters=None):
    if state['depth'] == 0:
        state['index'] = preorder(pt)
        state['events'] = []
        state['variants'].append([names(trace), state['events']])
    if state['choice'] is not None:
        state['events'].append(['C', state['choice'], names(trace), state['index'][id(pt)]])
        state['choice'] = None
    state['depth'] += 1
    try:
        return for_trace(pt, a, sa, ea, tf, trace, tl, th, parameters)
    finally:
        state['depth'] -= 1
G['__approximate_alignment_for_trace'] = capture_for_trace
optimal = G['calculate_optimal_alignment']
def capture_optimal(pt, trace, parameters=None):
    result = optimal(pt, trace, parameters)
    state['events'].append(['B', state['index'][id(pt)], names(trace), moves(copy.deepcopy(result))])
    return result
G['calculate_optimal_alignment'] = capture_optimal
def sets_of(binary):
    a, sa, ea, tf = initialize_a_sa_ea_tau_sets(binary)
    out, stack = [], [binary]
    while stack:
        node = stack.pop()
        out.append([sorted(a[node]), sorted(sa[node]), sorted(ea[node]), tf[node], get_process_tree_height(node)])
        stack.extend(reversed(node.children))
    return out
out = []
for case in json.load(sys.stdin):
    tl, th = case['setting']
    try:
        binary = process_tree_to_binary_process_tree(parse(case['tree']))
        text = str(binary)
        nodes = sets_of(binary)
    except Exception:
        text, nodes = 'raises', []
    log = EventLog()
    for k, trace in enumerate(case['traces']):
        t = Trace()
        t.attributes['concept:name'] = f'c{k}'
        for activity in trace:
            t.append(Event({'concept:name': activity}))
        log.append(t)
    state.update(index=None, events=None, variants=[], depth=0, ops=[], choice=None)
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            aligned = original.apply(log, parse(case['tree']), parameters={
                original.Parameters.MAX_TRACE_LENGTH: tl, original.Parameters.MAX_PROCESS_TREE_HEIGHT: th})
        public = [None if a is None else [moves(a['alignment']), a['cost'], a['fitness']] for a in aligned]
    except Exception:
        public = 'raises'
    out.append({'binary': text, 'nodes': nodes, 'variants': state['variants'], 'public': public})
print(json.dumps(out))
'''


def capture(cases, seed):
    env = dict(os.environ, PYTHONHASHSEED=str(seed))
    # CBC writes its log to the inherited stdout; the child prints its JSON last.
    result = subprocess.run([sys.executable, '-I', '-B', '-c', CHILD],
                            input=json.dumps(cases), capture_output=True, text=True, env=env)
    if result.returncode:
        raise SystemExit(result.stderr[-3000:])
    return json.loads(result.stdout.strip().splitlines()[-1])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--count', type=int, default=400)
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/process-tree-approximated-bank.txt.gz')
    args = parser.parse_args()
    cases = generate(args.count)
    jobs = []
    for seed in HASH_SEEDS:
        mine = [case for case in cases if case['seed'] == seed]
        jobs += [(mine[i:i + 10], seed) for i in range(0, len(mine), 10)]
    with concurrent.futures.ThreadPoolExecutor(8) as pool:
        results = list(pool.map(lambda job: capture(*job), jobs))
    found = {}
    for (chunk, seed), result in zip(jobs, results):
        for case, item in zip(chunk, result):
            found[case['name']] = item
    lines, summary = [], {}
    def count(key, amount=1):
        summary[key] = summary.get(key, 0) + amount
    for case in cases:
        item = found[case['name']]
        tl, th = case['setting']
        lines += [f'C {case["name"]}',
                  'T ' + json.dumps([case['tree'], item['binary'], tl, th, case['traces']], separators=(',', ':')),
                  'N ' + json.dumps(item['nodes'], separators=(',', ':'))]
        for variant in item['variants']:
            lines.append('V ' + json.dumps(variant, separators=(',', ':')))
            count('variants')
            for event in variant[1]:
                count({'L': 'programs', 'C': 'choices', 'B': 'base_alignments', 'R': 'returns'}[event[0]])
        lines.append('P ' + json.dumps(item['public'], separators=(',', ':')))
        if item['public'] == 'raises':
            count('raises')
        else:
            count('traces', len(item['public']))
            count('none_results', sum(1 for a in item['public'] if a is None))
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0, filename='') as stream:
        stream.write(('\n'.join(lines) + '\n').encode())
    print(json.dumps(dict(sorted(summary.items()))))


if __name__ == '__main__':
    main()
