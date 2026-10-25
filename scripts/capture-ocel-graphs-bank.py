"""Public PM4Py 2.7.22 object-centric graph capture; no native calls.

Seeded OCELs (two to four object types, objects without events, events
without objects, shared and repeated objects, timestamp ties and logs whose
event table is not time-ordered) go through the public discovery dispatchers
`algo.discovery.ocel.{otg,etot,ocdfg}.algorithm.apply` (OCDFG with and without
edge performance, and with custom activity/object-type selector columns) and the public conformance dispatchers
`algo.conformance.ocel.{otg,etot,ocdfg}.algorithm.apply` against the graph
discovered from a sibling OCEL, with the OCEL and with its own discovered graph
as the real side, under default and custom parameters. Sets and dicts are
recorded tagged and sorted; lists keep their order. Every case runs in fresh interpreters under four PYTHONHASHSEED values
and keeps every distinct outcome.

    python -I -B scripts/capture-ocel-graphs-bank.py \
        --output docs/ocel-graphs-bank.json.gz
"""
import argparse
import concurrent.futures
import gzip
import json
import math
import os
from pathlib import Path
import random
import subprocess
import sys

BANK_SEED = 20261025
HASH_SEEDS = (0, 1, 2, 3)
TYPES = ('order', 'item', 'pack', 'cust')
CONFORMANCE = {
    # THETA is a per-relation map; relations it omits keep the default.
    'otg': {'custom': {'THETA': {'object_interaction': 0.5, 'object_cobirth': 0.0}, 'ALPHA': 2.0, 'BETA': 0.5,
                       'GAMMA': 1.5}},
    'etot': {'custom': {'THETA_REL': 0.3, 'ALPHA': 0.5, 'BETA': 2.0, 'GAMMA': 1.0}},
    'ocdfg': {'custom': {'THETA_ACT': 0.2, 'THETA_FLOW': 0.25, 'ALPHA': 2.0, 'BETA': 1.0, 'GAMMA': 0.5,
                         'DELTA': 1.5}},
}


def ocel(rng, types, activities):
    objects = []
    for type_name in types:
        for n in range(rng.randint(1, 4)):
            objects.append([f'{type_name[0]}{n}', type_name])
    events = []
    time = 0
    # Events without objects only in a quarter of the logs.
    sizes = (0, 1, 1, 2, 2, 3, 4) if rng.random() < 0.25 else (1, 1, 2, 2, 3, 4)
    for n in range(rng.randint(3, 20)):
        time += rng.choice((0, 0, 1, 2, 5))
        related = rng.sample([o[0] for o in objects], min(len(objects), rng.choice(sizes)))
        events.append([f'e{n:02d}', rng.choice(activities), time, related])
    if rng.random() < 0.15:
        rng.shuffle(events)
    return {'objects': objects, 'events': events}


def generate():
    rng = random.Random(BANK_SEED)
    cases = []
    for index in range(150):
        types = list(TYPES[:rng.randint(2, 4)])
        activities = [chr(ord('a') + n) for n in range(rng.randint(2, 6))]
        cases.append({'name': f'ocel-{index:03d}', 'real': ocel(rng, types, activities),
                      'normative': ocel(rng, types if rng.random() < 0.7 else types[:-1],
                                        activities if rng.random() < 0.7 else activities[:-1] + ['z'])})
    return cases


def number(value):
    value = float(value)
    return value if math.isfinite(value) else repr(value)


def ordered(value):
    """Sets and dicts tagged and in canonical order; tuples as lists."""
    if isinstance(value, dict):
        return {'dict': sorted([ordered(k), ordered(v)] for k, v in value.items())}
    if isinstance(value, (set, frozenset)):
        return {'set': sorted(ordered(v) for v in value)}
    if isinstance(value, (list, tuple)):
        return [ordered(v) for v in value]
    if isinstance(value, float):
        return number(value)
    if hasattr(value, 'item'):
        return ordered(value.item())
    return value


def run(cases):
    import warnings
    warnings.filterwarnings('ignore')
    import pandas as pd
    from pm4py.objects.ocel.obj import OCEL
    from pm4py.algo.discovery.ocel.otg import algorithm as otg
    from pm4py.algo.discovery.ocel.etot import algorithm as etot
    from pm4py.algo.discovery.ocel.ocdfg import algorithm as ocdfg
    from pm4py.algo.conformance.ocel.otg import algorithm as cotg
    from pm4py.algo.conformance.ocel.etot import algorithm as cetot
    from pm4py.algo.conformance.ocel.ocdfg import algorithm as cocdfg
    origin = pd.Timestamp('2024-01-01')

    def build(spec, labelled=False):
        events = pd.DataFrame({'ocel:eid': [e[0] for e in spec['events']],
                               'ocel:activity': [e[1] for e in spec['events']],
                               'ocel:timestamp': [origin + pd.Timedelta(minutes=e[2]) for e in spec['events']]})
        objects = pd.DataFrame({'ocel:oid': [o[0] for o in spec['objects']],
                                'ocel:type': [o[1] for o in spec['objects']]})
        types = dict(spec['objects'])
        rows = [{'ocel:eid': e[0], 'ocel:activity': e[1], 'ocel:timestamp': origin + pd.Timedelta(minutes=e[2]),
                 'ocel:oid': o, 'ocel:type': types[o], 'ocel:qualifier': None}
                for e in spec['events'] for o in e[3]]
        relations = pd.DataFrame(rows, columns=['ocel:eid', 'ocel:activity', 'ocel:timestamp', 'ocel:oid',
                                                'ocel:type', 'ocel:qualifier'])
        if labelled:
            # Custom selector columns: the activity and type spelled upper-case
            # (merged activities where two labels share one spelling).
            events['label'] = events['ocel:activity'].str.upper().replace({'B': 'A'})
            objects['kind'] = objects['ocel:type'].str.upper()
            relations['label'] = relations['ocel:activity'].str.upper().replace({'B': 'A'})
            relations['kind'] = relations['ocel:type'].str.upper()
        return OCEL(events=events, objects=objects, relations=relations)

    modules = {'otg': (otg, cotg), 'etot': (etot, cetot), 'ocdfg': (ocdfg, cocdfg)}
    out = []
    for case in cases:
        record = {}

        def call(key, function):
            try:
                record[key] = {'value': ordered(function())}
            except Exception as error:
                record[key] = {'error': type(error).__name__}

        for family, (discovery, conformance) in modules.items():
            call(f'discover/{family}', lambda: discovery.apply(build(case['real'])))
            if family == 'ocdfg':
                call('discover/ocdfg/no_performance', lambda: discovery.apply(
                    build(case['real']), parameters={'compute_edges_performance': False}))
                call('discover/ocdfg/selectors', lambda: discovery.apply(
                    build(case['real'], labelled=True),
                    parameters={'param:event:activity': 'label', 'param:object:type': 'kind'}))
            variant = list(conformance.Variants)[0].value
            for config in ('default', 'custom'):
                values = CONFORMANCE[family].get(config, {})
                parameters = {getattr(variant.Parameters, k): v for k, v in values.items()}
                for form in ('ocel', 'graph'):
                    def compare():
                        normative = discovery.apply(build(case['normative']))
                        real = build(case['real'])
                        if form == 'graph':
                            real = discovery.apply(real)
                        return conformance.apply(real, normative, parameters=dict(parameters))
                    call(f'conform/{family}/{config}/{form}', compare)
        out.append(record)
    return out


def capture(seed):
    env = dict(os.environ, PYTHONHASHSEED=str(seed))
    result = subprocess.run([sys.executable, '-I', '-B', os.path.abspath(__file__), '--child'],
                            capture_output=True, text=True, env=env)
    if result.returncode:
        raise SystemExit(result.stderr[-3000:])
    return json.loads(result.stdout.strip().splitlines()[-1])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path)
    parser.add_argument('--child', action='store_true')
    args = parser.parse_args()
    if args.child:
        print(json.dumps(run(generate())))
        return
    import pm4py
    cases = generate()
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        runs = list(pool.map(capture, HASH_SEEDS))
    summary = {}
    for index, case in enumerate(cases):
        case['calls'] = {}
        for call in runs[0][index]:
            seen = []
            for run_result in runs:
                if run_result[index][call] not in seen:
                    seen.append(run_result[index][call])
            merged = seen[0] if len(seen) == 1 else {'observed': seen}
            case['calls'][call] = merged
            kind = 'observed' if len(seen) > 1 else 'error' if 'error' in merged else 'value'
            summary[kind] = summary.get(kind, 0) + 1
    bank = {'reference': f'pm4py {pm4py.__version__}', 'bank_seed': BANK_SEED,
            'hash_seeds': list(HASH_SEEDS), 'conformance_parameters': CONFORMANCE, 'cases': cases}
    data = json.dumps(bank, indent=0, sort_keys=True).encode('utf-8')
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0,
                                                      compresslevel=9, filename='') as stream:
        stream.write(data)
    print(f'{len(cases)} cases, calls by outcome {summary}')


if __name__ == '__main__':
    main()
