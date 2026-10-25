"""Seeded public PM4Py 2.7.22 OCEL LLM-abstraction capture; no native calls.

Seeded OCELs (two to four object types, objects without events, events
without objects, shared and repeated objects, timestamp ties and logs whose
event table is not time-ordered, as in the OCEL graphs bank) go through the
public `algo.querying.llm.abstractions.ocel_ocdfg_descr` (default, without
header, without performance, with a short budget, and with the discovery
told not to compute edge performance, with and without performance in the
text) and `ocel_fea_descr` for every object type (default,
and without header under a short budget). Each call records the text or the
error class; the progress lines the feature extraction prints are dropped.
Every call gets a freshly built OCEL. Every case runs in fresh interpreters
under four PYTHONHASHSEED values and keeps every distinct outcome.

    python -I -B scripts/capture-llm-ocel-abstractions-bank.py \\
        --output docs/llm-ocel-abstractions-bank.json.gz
"""
import argparse
import concurrent.futures
import gzip
import json
import os
import random
import subprocess
import sys

BANK_SEED = 20261034
HASH_SEEDS = (0, 1, 2, 3)
TYPES = ('order', 'item', 'pack', 'cust')
OCDFG_CALLS = {
    'ocdfg-default': {},
    'ocdfg-noheader': {'include_header': False},
    'ocdfg-noperformance': {'include_performance': False},
    'ocdfg-short': {'max_len': 400},
    'ocdfg-uncomputed': {'compute_edges_performance': False, 'include_performance': False},
    'ocdfg-uncomputed-performance': {'compute_edges_performance': False},
}
FEATURE_CALLS = {
    'features-default': {},
    'features-short': {'include_header': False, 'max_len': 600},
}


def ocel(rng, types, activities):
    objects = []
    for type_name in types:
        for n in range(rng.randint(1, 4)):
            objects.append([f'{type_name[0]}{n}', type_name])
    events = []
    time = 0
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
        cases.append({'name': f'ocel-{index:03d}', 'ocel': ocel(rng, types, activities)})
    return cases


def run(cases):
    import contextlib
    import io
    import warnings
    warnings.filterwarnings('ignore')
    import pandas as pd
    from pm4py.objects.ocel.obj import OCEL
    from pm4py.algo.querying.llm.abstractions import ocel_fea_descr, ocel_ocdfg_descr
    origin = pd.Timestamp('2024-01-01')

    def build(spec):
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
        return OCEL(events=events, objects=objects, relations=relations)

    out = []
    for case in cases:
        record = {}

        def call(key, function):
            try:
                with contextlib.redirect_stdout(io.StringIO()):
                    record[key] = {'text': function()}
            except Exception as error:
                record[key] = {'error': type(error).__name__}

        for key, parameters in OCDFG_CALLS.items():
            call(key, lambda: ocel_ocdfg_descr.apply(build(case['ocel']), parameters=dict(parameters)))
        for type_name in sorted({o[1] for o in case['ocel']['objects']}):
            for key, parameters in FEATURE_CALLS.items():
                call(f'{key}/{type_name}',
                     lambda: ocel_fea_descr.apply(build(case['ocel']), type_name, parameters=dict(parameters)))
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
    parser.add_argument('--output', type=os.path.abspath)
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
            case['calls'][call] = seen[0] if len(seen) == 1 else {'observed': seen}
            kind = 'observed' if len(seen) > 1 else 'error' if 'error' in seen[0] else 'value'
            family = call.split('-')[0]
            summary[(family, kind)] = summary.get((family, kind), 0) + 1
    bank = {'reference': f'pm4py {pm4py.__version__}', 'bank_seed': BANK_SEED, 'hash_seeds': list(HASH_SEEDS),
            'ocdfg_calls': OCDFG_CALLS, 'feature_calls': FEATURE_CALLS, 'cases': cases}
    data = json.dumps(bank, indent=0, sort_keys=True).encode('utf-8')
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0,
                                                      compresslevel=9, filename='') as stream:
        stream.write(data)
    print(f'{len(cases)} cases, calls by outcome {sorted(summary.items())}')


if __name__ == '__main__':
    main()
