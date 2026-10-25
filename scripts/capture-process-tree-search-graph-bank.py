"""Capture the public bank for the process-tree SEARCH_GRAPH_PT variant and
its helpers: algo.conformance.alignments.process_tree.variants.search_graph_pt
and util.search_graph_pt_replay_semantics / search_graph_pt_frequency_
annotation.

Seeded process trees (sequence, choice, parallel and binary loop operators,
labels a..e with repeats, some tau leaves) go through
pm4py.objects.process_tree.utils.generic.parse. Replay records are random
walks over the replay semantics: from get_initial_state, each step calls one
of the public functions on a random node and usually moves to the state it
returns. Every replay case runs in fresh interpreters under four hash seeds,
which must agree.

Variant records align every tree against six traces (the empty one, model
walks and walks with a deleted or inserted event) with
search_graph_pt.align_variant, handing it the leaves as a preorder list of
(id(leaf), leaf) instead of the frozenset apply builds: apply's candidate order
follows object addresses, a list makes it fixed. Trees without repeated labels
also go through the public apply, where the order cannot matter. Both run
under the four hash seeds, which must agree.

Frequency-annotation records run the public SEARCH_GRAPH_PT variant on a
small log (seed 0 only: the search itself depends on object ids) and keep,
per alignment, the replay paths of its search-state chain (root first) next
to what search_graph_pt_frequency_annotation.apply writes on every node.
Trees whose get_bottomup_nodes set (which the annotation walks) differs
between hash seeds are left out.

The bank is a gzip text file of tagged lines read by
minera_process_tree_search_graph_parity_tests (states are strings of
E/O/C/F per node in preorder, nodes are preorder indices):

  C name
  T tree text
  I initial state
  R json   [[state, node, op, bool], ...] for can_enable/can_open/can_close,
           [[state, node, op, path or null, state or null], ...] for the
           vertex moves and shortest paths ([[node, state], ...] paths), and
           [[state, node, "transform_tree:X", path, state], ...]
  A json   [[[[state, [[node, state], ...]] per search state, root first]
           per alignment], [[num_events, num_cases] per node]] (null where
           apply leaves a node without the properties)
  V json   [[trace, cost, [[log or ">>", node or ">>"], ...]], ...] from
           align_variant with preorder candidates
  P json   [[trace, cost, alignment, fitness], ...] from apply (trees without
           repeated labels only)
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

BANK_SEED = 20261026
HASH_SEEDS = (0, 1, 2, 3)
ROOT = Path(__file__).resolve().parent.parent
LABELS = 'abcde'
OPS = ['can_enable', 'can_open', 'can_close', 'enable_vertex', 'open_vertex', 'close_vertex',
       'shortest_path_to_open', 'shortest_path_to_close', 'shortest_path_to_enable', 'transform_tree']


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
    for kind in range(5):
        trace = []
        walk(r, node, trace, [2])
        trace = trace[:6]
        if kind >= 2:
            action = r.random()
            if action < 0.5 and trace:
                del trace[r.randrange(len(trace))]
            else:
                trace.insert(r.randint(0, len(trace)), r.choice(LABELS + 'x'))
        out.append(trace)
    return out


def generate(count, steps):
    r = random.Random(BANK_SEED)
    cases = []
    for index in range(count):
        node = tree(r)
        cases.append({'name': f'tree{index}', 'tree': text(node), 'seed': r.randrange(1 << 30),
                      'steps': steps, 'traces': traces(r, node) if index % 3 == 0 else None})
        cases[-1]['variants'] = [[]] + traces(random.Random(cases[-1]['seed'] + 1), node)
    return cases


CHILD = r'''
import contextlib, datetime, io, json, random, sys, warnings
warnings.filterwarnings('ignore')
from pm4py.objects.log.obj import Event, EventLog, Trace
from pm4py.objects.process_tree.obj import ProcessTree
from pm4py.objects.process_tree.utils import bottomup
from pm4py.objects.process_tree.utils.generic import parse
from pm4py.algo.conformance.alignments.process_tree.util import search_graph_pt_replay_semantics as rs
from pm4py.algo.conformance.alignments.process_tree.util import search_graph_pt_frequency_annotation as fa
from pm4py.algo.conformance.alignments.process_tree.variants import search_graph_pt as sg
States = ProcessTree.OperatorState
origin = datetime.datetime(2024, 1, 1, tzinfo=datetime.timezone.utc)
annotate = sys.argv[1] == '1'
def preorder(tree):
    out = []
    def visit(node):
        out.append(node)
        for child in node.children:
            visit(child)
    visit(tree)
    return out
def encode(state, nodes):
    return ''.join(state[(id(n), n)].name[0] for n in nodes)
def path_of(path, index):
    return None if path is None else [[index[id(n)], s.name[0]] for n, s in path]
def moves(alignment, index):
    return [[a, b if isinstance(b, str) else index[id(b)]] for a, b in alignment]
def make_log(traces):
    log, minute = EventLog(), 0
    for k, trace in enumerate(traces):
        t = Trace()
        t.attributes['concept:name'] = f'c{k}'
        for activity in trace:
            t.append(Event({'concept:name': activity, 'time:timestamp': origin + datetime.timedelta(minutes=minute)}))
            minute += 1
        log.append(t)
    return log
out = []
for case in json.load(sys.stdin):
    tree = parse(case['tree'])
    nodes = preorder(tree)
    index = {id(n): i for i, n in enumerate(nodes)}
    item = {}
    if not annotate:
        r = random.Random(case['seed'])
        state = rs.get_initial_state(tree)
        item['initial'] = encode(state, nodes)
        item['bottomup'] = sorted(index[id(n)] for n in bottomup.get_bottomup_nodes(tree))
        records = []
        for _ in range(case['steps']):
            n = r.randrange(len(nodes))
            op = r.choice(%s)
            before = encode(state, nodes)
            if op == 'transform_tree':
                target = r.choice(list(States))
                path, after = rs.transform_tree(nodes[n], target, dict(state))
                records.append([before, n, 'transform_tree:' + target.name[0], path_of(path, index), encode(after, nodes)])
                continue
            result = getattr(rs, op)(nodes[n], state)
            if isinstance(result, bool):
                records.append([before, n, op, result])
                continue
            path, after = result
            records.append([before, n, op, path_of(path, index), None if after is None else encode(after, nodes)])
            if after is not None and r.random() < 0.8:
                state = after
            if all(v == States.CLOSED for v in state.values()) and r.random() < 0.5:
                state = rs.get_initial_state(tree)
        item['records'] = records
        leaves = [(id(n), n) for n in nodes if not n.children]
        item['variant'] = []
        for trace in case['variants']:
            aligned = sg.align_variant(tuple(trace), leaves, tree)
            item['variant'].append([trace, aligned['cost'], moves(aligned['alignment'], index)])
        labels = [n.label for n in nodes if not n.children and n.label is not None]
        if len(labels) == len(set(labels)):
            with contextlib.redirect_stderr(io.StringIO()):
                aligned = sg.apply(make_log(case['variants']), tree, parameters={'show_progress_bar': False})
            item['public'] = [[trace, a['cost'], moves(a['alignment'], index), a['fitness']]
                              for trace, a in zip(case['variants'], aligned)]
    elif case['traces'] is not None:
        log = make_log(case['traces'])
        with contextlib.redirect_stderr(io.StringIO()):
            aligned = sg.apply(log, tree, parameters={'show_progress_bar': False})
        paths = []
        for alignment in aligned:
            chain, s = [], alignment['state']
            while s is not None:
                chain.append(s)
                s = s.parent
            paths.append([[encode(s.state, nodes), path_of(s.path or [], index)] for s in reversed(chain)])
        fa.apply(tree, aligned)
        item['annotation'] = [paths, [[n._properties.get('num_events'), n._properties.get('num_cases')] for n in nodes]]
    out.append(item)
print(json.dumps(out))
''' % repr(OPS)


def capture(cases, seed, annotate):
    env = dict(os.environ, PYTHONHASHSEED=str(seed))
    result = subprocess.run([sys.executable, '-I', '-B', '-c', CHILD, '1' if annotate else '0'],
                            input=json.dumps(cases), capture_output=True, text=True, env=env)
    if result.returncode:
        raise SystemExit(result.stderr[-3000:])
    return json.loads(result.stdout.strip().splitlines()[-1])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--count', type=int, default=900)
    parser.add_argument('--steps', type=int, default=50)
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/process-tree-search-graph-bank.txt.gz')
    args = parser.parse_args()
    cases = generate(args.count, args.steps)
    chunks = [cases[i:i + 100] for i in range(0, len(cases), 100)]
    jobs = [(chunk, seed, False) for seed in HASH_SEEDS for chunk in chunks] + [(chunk, 0, True) for chunk in chunks]
    with concurrent.futures.ThreadPoolExecutor(8) as pool:
        results = list(pool.map(lambda job: capture(*job), jobs))
    per_seed = {}
    for (chunk, seed, annotate), result in zip(jobs, results):
        per_seed.setdefault((seed, annotate), []).extend(result)
    runs = [per_seed[(seed, False)] for seed in HASH_SEEDS]
    annotations = per_seed[(0, True)]
    lines, summary = [], {}
    for index, case in enumerate(cases):
        seen = [run[index] for run in runs]
        if any(value != seen[0] for value in seen[1:]):
            summary['varies'] = summary.get('varies', 0) + 1
            continue
        summary['records'] = summary.get('records', 0) + len(seen[0]['records'])
        lines += [f'C {case["name"]}', f'T {case["tree"]}', f'I {seen[0]["initial"]}',
                  'R ' + json.dumps(seen[0]['records'], separators=(',', ':')),
                  'V ' + json.dumps(seen[0]['variant'], separators=(',', ':'))]
        summary['variant_traces'] = summary.get('variant_traces', 0) + len(seen[0]['variant'])
        if 'public' in seen[0]:
            summary['public_traces'] = summary.get('public_traces', 0) + len(seen[0]['public'])
            lines.append('P ' + json.dumps(seen[0]['public'], separators=(',', ':')))
        if 'annotation' in annotations[index]:
            summary['annotated'] = summary.get('annotated', 0) + 1
            lines.append('A ' + json.dumps(annotations[index]['annotation'], separators=(',', ':')))
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0, filename='') as stream:
        stream.write(('\n'.join(lines) + '\n').encode())
    print(json.dumps(dict(sorted(summary.items()))))


if __name__ == '__main__':
    main()
