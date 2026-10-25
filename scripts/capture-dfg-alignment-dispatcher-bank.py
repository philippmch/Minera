"""Public PM4Py 2.7.22 DFG alignment dispatcher capture; no native calls.

Seeded random DFGs (start/end maps included, sometimes disconnected or with
zero frequencies) and logs (unknown labels, empty traces, repeated variants)
are aligned through the public `algo.conformance.alignments.dfg.algorithm`
on an EventLog and on the equivalent DataFrame, with default costs or random
sync/model/log/internal cost functions (some deliberately missing an
activity). Each call runs in a fresh interpreter per PYTHONHASHSEED. Fields
equal under every seed are kept as values; a field that varies (the search
breaks ties by hash order, so alignments and state counters can) keeps its
distinct observed values.

    python -I -B scripts/capture-dfg-alignment-dispatcher-bank.py \
        --output docs/dfg-alignment-dispatcher-bank.json.gz
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
HASH_SEEDS = tuple(range(16))
LETTERS = 'abcdefg'


def generate(count):
    rng = random.Random(BANK_SEED)
    cases = []
    for index in range(count):
        activities = list(LETTERS[:rng.randint(2, 6)])
        edges = []
        for source in activities:
            for target in activities:
                if rng.random() < 0.3:
                    edges.append([source, target, rng.choice((0, 1, 2, 5))])
        starts = [[a, rng.choice((0, 1, 3))] for a in rng.sample(activities, rng.randint(1, 2))]
        ends = [[a, rng.choice((0, 1, 3))] for a in rng.sample(activities, rng.randint(1, 2))]
        alphabet = activities + (['x'] if rng.random() < 0.3 else [])
        traces = [[rng.choice(alphabet) for _ in range(rng.randint(0, 6))]
                  for _ in range(rng.randint(1, 5))]
        if rng.random() < 0.3:
            traces.append(list(traces[0]))
        costs = {}
        if rng.random() < 0.5:
            for name in ('sync', 'model', 'log', 'internal'):
                if rng.random() < 0.5:
                    domain = list(activities) if name in ('sync', 'model') else sorted(set(alphabet))
                    if rng.random() < 0.15 and len(domain) > 1:
                        domain = domain[1:]
                    costs[name] = {a: rng.choice((0, 1, 2, 3, 5, 10000)) for a in domain}
        cases.append({'name': f'dfg_{index:04d}', 'edges': edges, 'starts': starts,
                      'ends': ends, 'traces': traces, 'costs': costs})
    return cases


CHILD = r'''
import json, sys, warnings
warnings.filterwarnings('ignore')
import pandas as pd
from pm4py.algo.conformance.alignments.dfg import algorithm as dfg_alignment
from pm4py.algo.conformance.alignments.dfg.variants import classic
from pm4py.objects.log.obj import EventLog, Trace, Event
P = classic.Parameters
names = {'sync': P.SYNC_COST_FUNCTION, 'model': P.MODEL_MOVE_COST_FUNCTION,
         'log': P.LOG_MOVE_COST_FUNCTION, 'internal': P.INTERNAL_LOG_MOVE_COST_FUNCTION}
def summary(results, known):
    # The early sink transition carries a fresh UUID per call; it is
    # canonicalized the way native names it.
    out = []
    for r in results:
        row = {'alignment': [[m[0], m[1] if m[1] == '>>' or m[1] in known else '@@dfg_sink']
                             for m in r['alignment']]}
        for key in ('cost', 'internal_cost', 'visited_states', 'closed', 'fitness', 'bwc'):
            if key in r:
                row[key] = r[key]
        out.append(row)
    return out
def call(function, known):
    try:
        return {'results': summary(function(), known)}
    except Exception as error:
        return {'error': type(error).__name__}
cases = json.load(sys.stdin)
out = []
for case in cases:
    dfg = {(s, t): c for s, t, c in case['edges']}
    sa = {a: c for a, c in case['starts']}
    ea = {a: c for a, c in case['ends']}
    parameters = {names[k]: v for k, v in case['costs'].items()}
    log = EventLog([Trace([Event({'concept:name': a}) for a in trace],
                          attributes={'concept:name': str(i)})
                    for i, trace in enumerate(case['traces'])])
    rows = []
    stamp = pd.Timestamp('2020-01-01')
    for i, trace in enumerate(case['traces']):
        for j, a in enumerate(trace):
            rows.append({'case:concept:name': str(i), 'concept:name': a,
                         'time:timestamp': stamp + pd.Timedelta(seconds=len(rows))})
    frame = pd.DataFrame(rows, columns=['case:concept:name', 'concept:name', 'time:timestamp'])
    known = {a for edge in dfg for a in edge} | set(sa) | set(ea)
    out.append({'log': call(lambda: dfg_alignment.apply(log, dfg, sa, ea, parameters=dict(parameters)), known),
                'frame': call(lambda: dfg_alignment.apply(frame, dfg, sa, ea, parameters=dict(parameters)), known)})
print(json.dumps(out))
'''


def capture(cases, seed):
    env = dict(os.environ, PYTHONHASHSEED=str(seed))
    result = subprocess.run([sys.executable, '-I', '-B', '-c', CHILD], input=json.dumps(cases),
                            capture_output=True, text=True, env=env, check=True)
    return json.loads(result.stdout.strip().splitlines()[-1])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--count', type=int, default=1500)
    args = parser.parse_args()
    import pm4py
    cases = generate(args.count)
    runs = [capture(cases, seed) for seed in HASH_SEEDS]
    unstable = 0
    for index, case in enumerate(cases):
        for form in ('log', 'frame'):
            values = [run[index][form] for run in runs]
            if all(value == values[0] for value in values):
                case[form] = values[0]
                continue
            unstable += 1
            shapes = [sorted(value) for value in values]
            lengths = {len(value.get('results', ())) for value in values}
            if any(shape != ['results'] for shape in shapes) or len(lengths) != 1:
                case[form] = {'unstable': True}
                continue
            # Field by field: a value equal under every seed is kept, otherwise
            # the distinct observed values (native must equal one of them).
            rows = []
            for row_index in range(lengths.pop()):
                row = {}
                for key in values[0]['results'][row_index]:
                    seen = []
                    for value in values:
                        item = value['results'][row_index].get(key)
                        if item not in seen:
                            seen.append(item)
                    row[key] = seen[0] if len(seen) == 1 else {'observed': seen}
                rows.append(row)
            case[form] = {'results': rows}
    bank = {'reference': f'pm4py {pm4py.__version__}', 'bank_seed': BANK_SEED,
            'hash_seeds': list(HASH_SEEDS), 'varying_calls': unstable, 'cases': cases}
    data = json.dumps(bank, indent=0, sort_keys=True).encode('utf-8')
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0,
                                                      compresslevel=9, filename='') as stream:
        stream.write(data)
    errors = sum('error' in case['log'] for case in cases)
    print(f'{len(cases)} cases, {errors} log errors, {unstable} calls with varying fields')


if __name__ == '__main__':
    main()
