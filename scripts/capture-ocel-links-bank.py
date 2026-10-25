"""Public PM4Py 2.7.22 link analysis and timestamp interleavings capture; no
native calls.

Link analysis: seeded event tables (case, activity, minute timestamps with
ties up to sixteen rows, rows not in time order, `doc`/`ref` link columns drawn from a small
pool) go through the public `algo.discovery.ocel.link_analysis.algorithm.apply`
with the default columns and with `doc`→`ref` under every combination of
LOOK_FORWARD, KEEP_FIRST_OCCURRENCE and PROPAGATE. Each result row is recorded
by its generated `@@index_out`/`@@index_in` pair, in order, with the column
list.

Timestamp interleavings: seeded left/right logs (timezone-aware minute
timestamps with ties) and case relations (some naming unknown cases) go
through the public `algo.discovery.ocel.interleavings.algorithm.apply`; each
row is recorded by its `@@` columns, in order. Naive timestamps are kept as
raising controls. Every case runs in fresh interpreters under four
PYTHONHASHSEED values and keeps every distinct outcome.

    python -I -B scripts/capture-ocel-links-bank.py \
        --output docs/ocel-links-bank.json.gz
"""
import argparse
import concurrent.futures
import gzip
import itertools
import json
import math
import os
from pathlib import Path
import random
import subprocess
import sys

BANK_SEED = 20261025
HASH_SEEDS = (0, 1, 2, 3)
LINK_CONFIGS = {'default': {}}
for forward, first, propagate in itertools.product((True, False), repeat=3):
    LINK_CONFIGS[f'doc-ref/{int(forward)}{int(first)}{int(propagate)}'] = {
        'out_column': 'doc', 'in_column': 'ref', 'look_forward': forward,
        'keep_first_occurrence': first, 'propagate': propagate}


def table(rng, prefix, activities, cases):
    rows = []
    for n in range(cases):
        time = rng.randint(0, 5)
        for _ in range(rng.randint(1, 5)):
            rows.append([f'{prefix}{n}', rng.choice(activities), time])
            time += rng.choice((0, 1, 2, 3))
    return rows


def generate():
    rng = random.Random(BANK_SEED)
    links, interleavings = [], []
    for index in range(100):
        pool = [f'd{n}' for n in range(rng.randint(2, 5))]
        rows = table(rng, 'c', list('abcd'), rng.randint(1, 5))
        for row in rows:
            row.extend([rng.choice(pool), rng.choice(pool + ['x'])])
        if len(rows) > 16:
            # Beyond sixteen rows the reference's timestamp sort is not
            # stable (its tie order is the sort implementation's), so larger
            # tables get distinct timestamps.
            for position, row in enumerate(sorted(rows, key=lambda r: r[2])):
                row[2] = position
        if rng.random() < 0.5:
            rng.shuffle(rows)
        links.append({'name': f'links-{index:03d}', 'rows': rows})
    for index in range(100):
        left = table(rng, 'o', ['create', 'pay', 'ship'], rng.randint(1, 4))
        right = table(rng, 'i', ['pick', 'pack', 'load'], rng.randint(1, 4))
        lefts = sorted({r[0] for r in left})
        rights = sorted({r[0] for r in right})
        relations = sorted({(rng.choice(lefts), rng.choice(rights)) for _ in range(rng.randint(1, 5))})
        if rng.random() < 0.2:
            relations.append(('o9', rights[0]))
        if rng.random() < 0.5:
            rng.shuffle(left)
            rng.shuffle(right)
        interleavings.append({'name': f'interleavings-{index:03d}', 'left': left, 'right': right,
                              'relations': [list(r) for r in relations], 'aware': index % 10 != 9})
    return links, interleavings


def value(item):
    if hasattr(item, 'item'):
        item = item.item()
    if isinstance(item, float):
        return item if math.isfinite(item) else repr(item)
    if hasattr(item, 'isoformat'):
        return str(item)
    return item


def run(links, interleavings):
    import warnings
    warnings.filterwarnings('ignore')
    import pandas as pd
    from pm4py.algo.discovery.ocel.link_analysis import algorithm as la
    from pm4py.algo.discovery.ocel.interleavings import algorithm as il
    out = []
    for case in links:
        origin = pd.Timestamp('2024-01-01')
        frame = pd.DataFrame({'case:concept:name': [r[0] for r in case['rows']],
                              'concept:name': [r[1] for r in case['rows']],
                              'time:timestamp': [origin + pd.Timedelta(minutes=r[2]) for r in case['rows']],
                              'doc': [r[3] for r in case['rows']], 'ref': [r[4] for r in case['rows']]})
        record = {}
        for config, parameters in LINK_CONFIGS.items():
            try:
                result = la.apply(frame.copy(), parameters=dict(parameters))
                record[config] = {'columns': list(result.columns),
                                  'pairs': [[int(a), int(b)] for a, b in
                                            zip(result['@@index_out'], result['@@index_in'])]}
            except Exception as error:
                record[config] = {'error': type(error).__name__}
        out.append(record)
    for case in interleavings:
        origin = pd.Timestamp('2024-01-01', tz='UTC' if case['aware'] else None)

        def frame(rows):
            return pd.DataFrame({'case:concept:name': [r[0] for r in rows], 'concept:name': [r[1] for r in rows],
                                 'time:timestamp': [origin + pd.Timedelta(minutes=r[2]) for r in rows]})
        relations = pd.DataFrame({'case:concept:name_LEFT': [r[0] for r in case['relations']],
                                  'case:concept:name_RIGHT': [r[1] for r in case['relations']]})
        try:
            result = il.apply(frame(case['left']), frame(case['right']), relations)
            columns = ['@@source_activity', '@@target_activity', '@@source_timestamp', '@@target_timestamp',
                       '@@left_index', '@@right_index', '@@direction', '@@timestamp_diff']
            record = {'rows': [[value(row[c]) for c in columns] for _, row in result.iterrows()],
                      'columns': list(result.columns)}
        except Exception as error:
            record = {'error': type(error).__name__}
        out.append({'apply': record})
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
        print(json.dumps(run(*generate())))
        return
    import pm4py
    links, interleavings = generate()
    cases = links + interleavings
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
            'link_configs': LINK_CONFIGS, 'cases': cases}
    data = json.dumps(bank, indent=0, sort_keys=True).encode('utf-8')
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0,
                                                      compresslevel=9, filename='') as stream:
        stream.write(data)
    print(f'{len(cases)} cases, calls by outcome {summary}')


if __name__ == '__main__':
    main()
