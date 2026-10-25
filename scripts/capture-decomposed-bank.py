"""Capture the bank for algo.conformance.alignments.decomposed.variants.recompos_maximal.

Seeded process trees (sequence, choice, parallel and loop operators over
labels a..e with repeats and tau leaves) are converted to accepting Petri nets
with the reference tree converter, and a small event log of tree runs, noisy
runs and random words (some with activities outside the net, some empty) goes
through the public recompos_maximal.apply. The bank keeps the net as the
reference iterates it (places and transitions in set order, which the
decomposition follows), the components decompose returns, and the apply
observed from the outside of its helpers: the alignment of the whole net for
the empty trace (the best-worst cost), and per aligned variant every call of
align with its result, every merge_sublist_nets call and the component list
handed to recompose_alignment, components being numbered in the order they
are first seen (the decomposition first). align breaks ties between optimal
alignments by object addresses and the recomposition can loop forever, so
each case runs once, under hash seed index % 4, with a 3 second limit.

The bank is a gzip text file of tagged lines read by
minera_decomposed_parity_tests:

  C name
  T json   [tree text, the log's traces]
  N json   [places, [[transition, label or null], ...], [[source, target], ...],
           im, fm] (markings as [[place, tokens], ...])
  D json   [[places, transitions, arcs, im, fm, lvis], ...] per component,
           sorted within each component
  B json   [alignment, cost] of the empty trace on the whole net
  V json   [trace, [event, ...]] per aligned variant, events being
           ["A", component, projected trace, [alignment, cost] or null]
           ["M", [component, ...], new component, the merge as in D]
           ["R", [component, ...]]
           (moves [[trace transition, model transition], [log, model]])
  P json   [{"cost", "alignment", "fitness", "bwc"} or null, ...] per log
           trace, or "hangs"
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

BANK_SEED = 20261029
HASH_SEEDS = (0, 1, 2, 3)
ROOT = Path(__file__).resolve().parent.parent
LABELS = 'abcde'


def tree(r, depth=0):
    if depth >= 3 or r.random() < 0.15 + 0.15 * depth:
        return ('tau',) if r.random() < 0.15 else ('leaf', r.choice(LABELS))
    op = r.choice(['->', 'X', '+', '*'])
    count = r.choice([2, 2, 3])
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
            walk(r, r.choice(node[1][1:]), out, budget)
            walk(r, node[1][0], out, budget)


def traces(r, node):
    out = []
    for kind in range(4):
        trace = []
        if kind == 3:
            trace = [r.choice(LABELS + 'z') for _ in range(r.randint(0, 5))]
        else:
            walk(r, node, trace, [2])
            trace = trace[:7]
            for _ in range(kind):
                if r.random() < 0.5 and trace:
                    del trace[r.randrange(len(trace))]
                else:
                    trace.insert(r.randint(0, len(trace)), r.choice(LABELS + 'z'))
        out.append(trace)
    if r.random() < 0.3:
        out.append(list(r.choice(out)))
    if r.random() < 0.15:
        out.append([])
    return out


def generate(count):
    r = random.Random(BANK_SEED)
    cases = []
    for index in range(count):
        node = tree(r)
        cases.append({'name': f'tree{index}', 'tree': text(node), 'traces': traces(r, node),
                      'seed': HASH_SEEDS[index % len(HASH_SEEDS)]})
    return cases


CHILD = r'''
import json, signal, sys, warnings
warnings.filterwarnings('ignore')
from pm4py.objects.process_tree.utils.generic import parse
from pm4py.objects.conversion.process_tree import converter as ptc
from pm4py.algo.conformance.alignments.decomposed.variants import recompos_maximal as m
from pm4py.objects.petri_net.utils import decomposition as d
from pm4py.objects.log.obj import Trace, Event, EventLog
class Hang(Exception):
    pass
def alarm(*args):
    raise Hang()
signal.signal(signal.SIGALRM, alarm)
S = {}
def oid(component):
    if id(component[0]) not in S['ids']:
        S['ids'][id(component[0])] = len(S['ids'])
        S['keep'].append(component)
    return S['ids'][id(component[0])]
def marking(mk):
    return sorted([p.name, k] for p, k in mk.items())
def describe(net, im, fm):
    return [sorted(p.name for p in net.places), sorted([t.name, t.label] for t in net.transitions),
            sorted([a.source.name, a.target.name] for a in net.arcs), marking(im), marking(fm),
            list(net.lvis_labels)]
def result(r):
    return None if r is None else [[[list(a), list(b)] for a, b in r['alignment']], r['cost']]
decompose = d.decompose
def capture_decompose(*args, **kwargs):
    out = decompose(*args, **kwargs)
    S['components'] = [describe(*c) for c in out]
    for c in out:
        oid(c)
    return out
m.decomp_utils.decompose = capture_decompose
align = m.__dict__['align']
def capture_align(trace, net, im, fm, parameters=None):
    out = align(trace, net, im, fm, parameters)
    if not S['variants']:
        S['bwc'] = result(out[0])
    else:
        S['variants'][-1][1].append(['A', oid((net,)), [e['concept:name'] for e in trace], result(out[0])])
    return out
m.__dict__['align'] = capture_align
merge = d.merge_sublist_nets
def capture_merge(nets):
    inputs = [oid(c) for c in nets]
    out = merge(nets)
    S['variants'][-1][1].append(['M', inputs, oid(out), describe(*out)])
    return out
m.decomp_utils.merge_sublist_nets = capture_merge
apply_trace = m.apply_trace
def capture_apply_trace(trace, list_nets, parameters=None):
    S['variants'].append([[e['concept:name'] for e in trace], []])
    return apply_trace(trace, list_nets, parameters)
m.apply_trace = capture_apply_trace
recompose = m.__dict__['recompose_alignment']
def capture_recompose(cons_nets, results):
    S['variants'][-1][1].append(['R', [oid(c) for c in cons_nets]])
    return recompose(cons_nets, results)
m.__dict__['recompose_alignment'] = capture_recompose
out = []
for case in json.load(sys.stdin):
    net, im, fm = ptc.apply(parse(case['tree']))
    shape = [[p.name for p in net.places], [[t.name, t.label] for t in net.transitions],
             sorted([a.source.name, a.target.name] for a in net.arcs), marking(im), marking(fm)]
    log = EventLog()
    for k, trace in enumerate(case['traces']):
        t = Trace()
        t.attributes['concept:name'] = f'c{k}'
        for activity in trace:
            t.append(Event({'concept:name': activity}))
        log.append(t)
    S.update(ids={}, keep=[], variants=[], components=None, bwc=None)
    signal.setitimer(signal.ITIMER_REAL, 3.0)
    try:
        aligned = m.apply(log, net, im, fm, parameters={m.Parameters.SHOW_PROGRESS_BAR: False})
        public = [None if a is None else dict(a, alignment=[list(x) for x in a['alignment']]) for a in aligned]
    except Hang:
        public = 'hangs'
    finally:
        signal.setitimer(signal.ITIMER_REAL, 0)
    out.append({'net': shape, 'components': S['components'], 'bwc': S['bwc'], 'variants': S['variants'],
                'public': public})
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
    parser.add_argument('--count', type=int, default=400)
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/decomposed-bank.txt.gz')
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
    dump = lambda value: json.dumps(value, separators=(',', ':'))
    for case in cases:
        item = found[case['name']]
        lines += [f'C {case["name"]}', 'T ' + dump([case['tree'], case['traces']]), 'N ' + dump(item['net']),
                  'D ' + dump(item['components']), 'B ' + dump(item['bwc'])]
        count('components', len(item['components']))
        for variant in item['variants']:
            lines.append('V ' + dump(variant))
            count('variants')
            for event in variant[1]:
                count({'A': 'aligns', 'M': 'merges', 'R': 'recompositions'}[event[0]])
        lines.append('P ' + dump(item['public']))
        if item['public'] == 'hangs':
            count('hangs')
        else:
            count('traces', len(item['public']))
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0, filename='') as stream:
        stream.write(('\n'.join(lines) + '\n').encode())
    print(json.dumps(dict(sorted(summary.items()))))


if __name__ == '__main__':
    main()
