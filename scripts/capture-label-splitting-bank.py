"""Seeded public PM4Py 2.7.22 contextual label-splitting capture; no native
calls.

Seeded event logs (one to eight traces of one to nine events over three to six
activities, some with multi-character names and repeated variants) go through
the public `algo.label_splitting.algorithm.apply` as dataframes under the
default parameters and under short and long contexts, an empty prefix, two
minimum edge weights, a target-activity subset, and a custom target column
with a custom suffix. Each call records the returned target-column labels in
row order with the returned `@@index` values, or the error class. Every case
runs in fresh interpreters under four PYTHONHASHSEED values and keeps every
distinct outcome.

    python -I -B scripts/capture-label-splitting-bank.py \\
        --output docs/label-splitting-bank.json.gz
"""
import argparse
import concurrent.futures
import gzip
import json
import os
import random
import subprocess
import sys

BANK_SEED = 20261028
HASH_SEEDS = (0, 1, 2, 3)
CONFIGS = {
    'default': {},
    'short': {'prefix_length': 1, 'suffix_length': 1},
    'long': {'prefix_length': 3, 'suffix_length': 3},
    'no_prefix': {'prefix_length': 0, 'suffix_length': 2},
    'weight_half': {'min_edge_weight': 0.5},
    'weight_high': {'min_edge_weight': 0.8},
    'targets': {'target_activities': ['a', 'b']},
    'column': {'target_column': 'split', 'activities_suffix': '#'},
}


def generate():
    rng = random.Random(BANK_SEED)
    cases = []
    for index in range(200):
        alphabet = list('abcdef'[:rng.randint(3, 6)])
        if rng.random() < 0.2:
            alphabet[rng.randrange(1, len(alphabet))] = rng.choice(('bc', 'xy', 'cab'))
        variants = [[rng.choice(alphabet) for _ in range(rng.randint(1, 9))]
                    for _ in range(rng.randint(1, 5))]
        traces = [list(rng.choice(variants)) for _ in range(rng.randint(1, 8))]
        cases.append({'name': f'split-{index:03d}', 'traces': traces})
    return cases


def run(cases):
    import warnings
    warnings.filterwarnings('ignore')
    import pandas as pd
    from pm4py.algo.label_splitting import algorithm
    origin = pd.Timestamp('2024-01-01', tz='UTC')
    out = []
    for case in cases:
        rows = []
        for trace_index, trace in enumerate(case['traces']):
            for activity in trace:
                rows.append({'case:concept:name': f'c{trace_index}', 'concept:name': activity,
                             'time:timestamp': origin + pd.Timedelta(minutes=len(rows))})
        record = {}
        for config, parameters in CONFIGS.items():
            frame = pd.DataFrame(rows)
            try:
                result = algorithm.apply(frame, parameters=dict(parameters))
                column = parameters.get('target_column', 'concept:name')
                record[config] = {'labels': [str(v) for v in result[column]],
                                  'index': [int(v) for v in result['@@index']]}
            except Exception as error:
                record[config] = {'error': type(error).__name__}
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
            merged = seen[0] if len(seen) == 1 else {'observed': seen}
            case['calls'][call] = merged
            kind = 'observed' if len(seen) > 1 else 'error' if 'error' in merged else 'value'
            summary[kind] = summary.get(kind, 0) + 1
    bank = {'reference': f'pm4py {pm4py.__version__}', 'bank_seed': BANK_SEED, 'hash_seeds': list(HASH_SEEDS),
            'configs': CONFIGS, 'cases': cases}
    data = json.dumps(bank, indent=0, sort_keys=True).encode('utf-8')
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0,
                                                      compresslevel=9, filename='') as stream:
        stream.write(data)
    print(f'{len(cases)} cases, calls by outcome {summary}')


if __name__ == '__main__':
    main()
