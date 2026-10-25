"""Seeded public PM4Py 2.7.22 ILP-discovery capture; no native calls.

Seeded event logs go through the public `pm4py.discover_petri_net_ilp` (an
EventLog, or a DataFrame whose rows are not sorted by case or time) and
`algo.discovery.ilp.algorithm.apply` with explicit causal relations and
artificial start/end names. Each call records the net (transition names and
labels; place names with producing and consuming transition names; the
marked places) or the error class and message, under two PYTHONHASHSEED
values.

The reference solves one LP relaxation per causal pair and keeps the
variables above 0.5. Most of these LPs have tied optima, so the capture
also classifies each call with the LP model documented in
docs/reference-2.7.22-parity.md (solved here with scipy, the reference's own
dependency): `exact` when every pair's rounding is the same at every optimal
point, else `tied`. It checks that every numbered reference place is the
rounding of an optimal point of some contained causal pair's LP, and fails
otherwise.

    python -I -B scripts/capture-ilp-discovery-bank.py \\
        --output docs/ilp-discovery-bank.json.gz
"""
import argparse
import concurrent.futures
import gzip
import json
import os
import random
import subprocess
import sys

BANK_SEED = 20261025
HASH_SEEDS = (0, 1)
START, END = '▶', '■'


def walk_log(rng, max_traces=12, max_length=12):
    acts = 'abcdefg'[:rng.randint(2, 7)]
    succ = {a: rng.sample(acts, rng.randint(1, min(3, len(acts)))) for a in acts}
    starts = rng.sample(acts, rng.randint(1, min(2, len(acts))))
    ends = set(rng.sample(acts, rng.randint(1, min(3, len(acts)))))
    traces = []
    for _ in range(rng.randint(1, max_traces)):
        if rng.random() < 0.3:
            trace = rng.sample(acts, rng.randint(1, len(acts)))
        else:
            trace = [rng.choice(starts)]
            while len(trace) < max_length:
                if trace[-1] in ends and rng.random() < 0.5:
                    break
                trace.append(rng.choice(succ[trace[-1]]))
        traces.append(trace)
    if rng.random() < 0.05:
        traces.append([])
    return traces


def small_log(rng):
    acts = 'abcde'[:rng.randint(2, 5)]
    traces = []
    for _ in range(rng.randint(1, 5)):
        if rng.random() < 0.5:
            traces.append([rng.choice(acts) for _ in range(rng.randint(1, 6))])
        else:
            traces.append(rng.sample(acts, rng.randint(1, len(acts))))
    if rng.random() < 0.05:
        traces.append([])
    for _ in range(rng.randint(0, 2)):
        traces.append(list(rng.choice(traces)))
    return traces


def tiny_log(rng):
    acts = 'abc'[:rng.randint(1, 3)]
    return [[rng.choice(acts) for _ in range(rng.randint(1, 4))] for _ in range(rng.randint(1, 3))]


def block_log(rng):
    """Sequences of single, exclusive and concurrent blocks over fresh labels."""
    labels = iter('abcdefghijklmnop')
    blocks = []
    for _ in range(rng.randint(2, 5)):
        kind = rng.choice(('single', 'xor', 'and'))
        width = 1 if kind == 'single' else rng.randint(2, 3)
        blocks.append((kind, [next(labels) for _ in range(width)]))
    traces = []
    for _ in range(rng.randint(2, 8)):
        trace = []
        for kind, members in blocks:
            if kind == 'single':
                trace += members
            elif kind == 'xor':
                trace.append(rng.choice(members))
            else:
                trace += rng.sample(members, len(members))
        traces.append(trace)
    return traces


def dfg_pairs(traces, start, end):
    pairs = []
    for trace in traces:
        wrapped = [start] + trace + [end]
        for pair in zip(wrapped, wrapped[1:]):
            if pair not in pairs:
                pairs.append(pair)
    return pairs


def generate():
    rng = random.Random(BANK_SEED)
    cases = []

    def add(kind, traces, **extra):
        cases.append(dict({'name': f'ilp-{len(cases):04d}', 'kind': kind, 'traces': traces}, **extra))

    alphas = (1.0, 1.0, 0.9, 0.75, 0.5, 0.3, 0.25, 0.0)
    for _ in range(260):
        add('public', walk_log(rng), alpha=rng.choice(alphas))
    for _ in range(420):
        add('public', small_log(rng), alpha=rng.choice(alphas + (2.0, -1.0, 0.7)))
    for _ in range(160):
        add('public', tiny_log(rng), alpha=rng.choice(alphas))
    for _ in range(120):
        add('public', block_log(rng), alpha=rng.choice(alphas))
    for _ in range(40):
        add('frame', small_log(rng), alpha=rng.choice(alphas))
    for _ in range(120):
        traces = small_log(rng) if rng.random() < 0.6 else walk_log(rng, 6, 8)
        pairs = dfg_pairs(traces, START, END)
        extra = [(a, b) for a in sorted({x for t in traces for x in t}) for b in sorted({x for t in traces for x in t})]
        chosen = rng.sample(pairs, rng.randint(1, len(pairs)))
        chosen += rng.sample(extra, min(len(extra), rng.randint(0, 3)))
        roll = rng.random()
        if roll < 0.06:
            chosen = [p for p in chosen if START not in p] or [('a', 'b')]
        elif roll < 0.12:
            chosen = [p for p in chosen if END not in p] + [(START, chosen[0][0] if chosen[0][0] != END else 'a')]
        elif roll < 0.18:
            chosen.insert(rng.randrange(len(chosen) + 1), (rng.choice((START, 'a')), 'zz'))
        else:
            if not any(START in p for p in chosen):
                chosen.append((START, traces[0][0] if traces[0] else END))
            if not any(END in p for p in chosen):
                chosen.append((traces[0][-1] if traces[0] else START, END))
        unique = []
        for pair in chosen:
            if pair not in unique:
                unique.append(pair)
        add('causal', traces, alpha=rng.choice(alphas), causal=[list(p) for p in unique])
    for _ in range(40):
        start, end = rng.choice((('S', 'E'), ('[start]', '[end]'), ('begin', 'finish')))
        add('names', small_log(rng), alpha=rng.choice(alphas), start=start, end=end)
    for traces in ([], [[]], [[], []], [[], ['a']], [['a']], [['a', 'a']], [['a', 'b'], ['b', 'a']],
                   [['a', 'b', 'c']] * 3, [['a'], ['a', 'a'], ['a', 'a', 'a']]):
        add('public', [list(t) for t in traces], alpha=1.0)
    return cases


def describe(net, im, fm):
    names = {t: t.name for t in net.transitions}
    return {
        'transitions': sorted([t.name, t.label] for t in net.transitions),
        'places': sorted([p.name, sorted(names[a.source] for a in p.in_arcs),
                          sorted(names[a.target] for a in p.out_arcs)] for p in net.places),
        'weights': sorted({a.weight for a in net.arcs}),
        'initial': sorted([p.name, n] for p, n in im.items()),
        'final': sorted([p.name, n] for p, n in fm.items())}


def run(cases):
    import warnings
    warnings.filterwarnings('ignore')
    os.environ['TQDM_DISABLE'] = '1'
    import datetime
    import pandas as pd
    import pm4py
    from pm4py.objects.log.obj import Event, EventLog, Trace
    from pm4py.algo.discovery.ilp import algorithm
    origin = datetime.datetime(2024, 1, 1)
    out = []
    for case in cases:
        try:
            if case['kind'] == 'frame':
                # Rows interleave the cases and run backwards in time; the
                # case ids are not sorted.
                rows = []
                ids = [f'k{900 - 7 * i:03d}' for i in range(len(case['traces']))]
                position = 0
                for i, trace in enumerate(case['traces']):
                    for j, activity in enumerate(trace):
                        rows.append((ids[i], activity, origin - datetime.timedelta(minutes=position), i, j))
                        position += 1
                rows.sort(key=lambda r: (r[4], r[3]))
                frame = pd.DataFrame([r[:3] for r in rows],
                                     columns=['case:concept:name', 'concept:name', 'time:timestamp'])
                result = pm4py.discover_petri_net_ilp(frame, alpha=case['alpha'])
            else:
                log = EventLog()
                for i, trace in enumerate(case['traces']):
                    t = Trace()
                    t.attributes['concept:name'] = f'c{i}'
                    for j, activity in enumerate(trace):
                        t.append(Event({'concept:name': activity,
                                        'time:timestamp': origin + datetime.timedelta(minutes=100 * i + j)}))
                    log.append(t)
                if case['kind'] == 'public':
                    result = pm4py.discover_petri_net_ilp(log, alpha=case['alpha'])
                else:
                    parameters = {algorithm.classic.Parameters.ALPHA: case['alpha']}
                    if case['kind'] == 'causal':
                        parameters[algorithm.classic.Parameters.CAUSAL_RELATION] = {
                            tuple(p): 1 for p in case['causal']}
                    else:
                        parameters[algorithm.classic.Parameters.PARAM_ARTIFICIAL_START_ACTIVITY] = case['start']
                        parameters[algorithm.classic.Parameters.PARAM_ARTIFICIAL_END_ACTIVITY] = case['end']
                    result = algorithm.apply(log, variant=algorithm.Variants.CLASSIC, parameters=parameters)
            out.append({'net': describe(*result)})
        except Exception as error:
            out.append({'error': [type(error).__name__, str(error)]})
    return out


def frame_traces(case):
    """The DataFrame rows grouped by case in first-appearance order."""
    if case['kind'] != 'frame':
        return case['traces']
    order = []
    rows = []
    for i, trace in enumerate(case['traces']):
        for j, activity in enumerate(trace):
            rows.append((j, i, activity))
    rows.sort()
    grouped = {}
    for _, i, activity in rows:
        if i not in grouped:
            grouped[i] = []
            order.append(i)
        grouped[i].append(activity)
    return [grouped[i] for i in order]


def classify(case, outcome):
    """LP-model classification of one call (see the module docstring)."""
    import numpy as np
    from scipy.optimize import linprog
    if 'error' in outcome:
        return 'error'
    start, end = case.get('start', START), case.get('end', END)
    traces = [[start] + t + [end] for t in frame_traces(case)]
    acts = []
    for t in traces:
        for a in t:
            if a not in acts:
                acts.append(a)
    n = len(acts)
    idx = {a: i for i, a in enumerate(acts)}
    alpha = case['alpha']
    cont = {}
    for t in traces:
        cnt = np.zeros(n, int)
        for a in t:
            cont.setdefault(cnt.tobytes(), {}).setdefault(a, 0)
            cont[cnt.tobytes()][a] += 1
            cnt = cnt.copy()
            cnt[idx[a]] += 1
    keep = {k: {a for a, w in d.items() if w >= (1 - alpha) * max(d.values())} for k, d in cont.items()}
    ub, eq, obj, seen = [], [], np.zeros(2 * n), set()
    for t in [list(v) for v in dict.fromkeys(tuple(t) for t in traces)]:
        cnt = np.zeros(n, int)
        complete = True
        for a in t:
            if a not in keep[cnt.tobytes()]:
                complete = False
                break
            row = np.concatenate([cnt, -cnt]).astype(float)
            row[n + idx[a]] -= 1
            if row.tobytes() not in seen:
                seen.add(row.tobytes())
                ub.append(-row)
            cnt = cnt.copy()
            cnt[idx[a]] += 1
            obj[:n] += cnt
            obj[n:] -= cnt
        if complete:
            row = np.concatenate([cnt, -cnt]).astype(float)
            if b'f' + row.tobytes() not in seen:
                seen.add(b'f' + row.tobytes())
                eq.append(row)
    Aub = np.array(ub) if ub else np.zeros((0, 2 * n))
    Aeq = np.array(eq) if eq else None
    beq = np.zeros(len(eq)) if eq else None
    if case['kind'] == 'causal':
        pairs = [tuple(p) for p in case['causal']]
    else:
        d = []
        for t in traces:
            for p in zip(t, t[1:]):
                if p not in d:
                    d.append(p)
        pairs = [(a, b) for a, b in d if a != b and (b, a) not in d]

    def bounds(pair):
        b = [(0, 1)] * (2 * n)
        b[idx[pair[0]]] = (1, 1)
        b[n + idx[pair[1]]] = (1, 1)
        return b

    def lp(c, A, bvec, b):
        return linprog(c, A_ub=A if len(A) else None, b_ub=bvec if len(A) else None,
                       A_eq=Aeq, b_eq=beq, bounds=b)

    tol = 1e-7
    status = 'exact'
    optimum = {}
    for pair in pairs:
        b = bounds(pair)
        r = lp(obj, Aub, np.zeros(len(Aub)), b)
        if r.status != 0:
            continue
        optimum[pair] = r.fun
        A2 = np.vstack([Aub, obj])
        b2 = np.concatenate([np.zeros(len(Aub)), [r.fun + 1e-9]])
        for k in range(2 * n):
            if b[k][0] == b[k][1]:
                continue
            e = np.zeros(2 * n)
            e[k] = 1
            low = lp(e, A2, b2, b).fun
            high = -lp(-e, A2, b2, b).fun
            if not (low > 0.5 + tol or high < 0.5 - tol):
                status = 'tied'
    for name, inputs, outputs in outcome['net']['places']:
        if name in ('source', 'sink'):
            continue
        ok = False
        for pair in pairs:
            if pair not in optimum or pair[0] not in inputs or pair[1] not in outputs:
                continue
            b = list(bounds(pair))
            for a in acts:
                for k, chosen in ((idx[a], inputs), (n + idx[a], outputs)):
                    lo, hi = b[k]
                    b[k] = (max(lo, 0.5 - tol), hi) if a in chosen else (lo, min(hi, 0.5 + tol))
            if any(lo > hi for lo, hi in b):
                continue
            A2 = np.vstack([Aub, obj])
            b2 = np.concatenate([np.zeros(len(Aub)), [optimum[pair] + 1e-7]])
            if lp(np.zeros(2 * n), A2, b2, b).status == 0:
                ok = True
                break
        if not ok:
            raise SystemExit(f"{case['name']}: place {name} is not an optimal rounding")
    return status


def capture(seed):
    env = dict(os.environ, PYTHONHASHSEED=str(seed), TQDM_DISABLE='1')
    result = subprocess.run([sys.executable, '-I', '-B', os.path.abspath(__file__), '--child'],
                            capture_output=True, text=True, env=env)
    if result.returncode:
        raise SystemExit(result.stderr[-3000:])
    return json.loads(result.stdout.strip().splitlines()[-1])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=os.path.abspath)
    parser.add_argument('--child', action='store_true')
    args = parser.parse_args()
    if args.child:
        print(json.dumps(run(generate())))
        return
    import pm4py
    cases = generate()
    with concurrent.futures.ThreadPoolExecutor(len(HASH_SEEDS)) as pool:
        runs = list(pool.map(capture, HASH_SEEDS))
    variation = 0
    counts = {}
    for index, case in enumerate(cases):
        seen = []
        for run_result in runs:
            if run_result[index] not in seen:
                seen.append(run_result[index])
        if len(seen) > 1:
            variation += 1
        case['observed'] = seen
        if case['kind'] == 'frame':
            case['log_traces'] = frame_traces(case)
        case['model'] = classify(case, seen[0])
        counts[case['model']] = counts.get(case['model'], 0) + 1
    bank = {'reference': f'pm4py {pm4py.__version__}', 'bank_seed': BANK_SEED, 'hash_seeds': list(HASH_SEEDS),
            'cases': cases}
    data = json.dumps(bank, indent=0, sort_keys=True, ensure_ascii=False).encode('utf-8')
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0,
                                                      compresslevel=9, filename='') as stream:
        stream.write(data)
    print(f'{len(cases)} cases, {variation} with hash-seed variation, {counts}')


if __name__ == '__main__':
    main()
