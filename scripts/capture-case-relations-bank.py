"""Seeded public PM4Py 2.7.22 case-relations merge capture; no native calls.

Seeded base/incoming event tables (string or integer case ids, activities,
UTC timestamps with ties and missing values, some timestamp columns absent
from one side, numeric/string/missing extra columns, columns shared by both
sides) and relation tables (duplicate rows, ids unknown to either side, extra
columns, some colliding with event columns) go through the public
`algo.merging.case_relations.algorithm.apply` under the default parameters
and under custom case-id key, left/right suffixes and timestamp key. Inputs and
outputs are recorded as in docs/case-relations-reference.json.gz (columns,
dtypes, index labels, per-cell values and scalar kinds); errors by class name.
Every case runs in fresh interpreters under four PYTHONHASHSEED values and
keeps every distinct outcome. A second family pins the inner-merge row order
for incoming rows that match several relations while others match none.

    python -I -B scripts/capture-case-relations-bank.py \
        --output docs/case-relations-bank.json.gz
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
CONFIGS = {
    'default': {},
    'custom': {'pm4py:param:case_id_key': 'cid', 'left_suffix': '_L', 'right_suffix': '_R',
               'pm4py:param:timestamp_key': 'ts'},
}


def generate():
    rng = random.Random(BANK_SEED)
    cases = []
    for index in range(150):
        numeric_ids = rng.random() < 0.2
        left_ids = [n if numeric_ids else f'L{n}' for n in range(rng.randint(1, 4))]
        right_ids = [100 + n if numeric_ids else f'R{n}' for n in range(rng.randint(1, 4))]

        def table(ids):
            rows = []
            for _ in range(rng.randint(0, 9)):
                row = {'case': rng.choice(ids), 'activity': rng.choice('abcde'),
                       'minute': rng.choice((None, 0, 1, 1, 2, 3, 5, 8)) if rng.random() < 0.15
                       else rng.choice((0, 1, 1, 2, 3, 5, 8))}
                if rng.random() < 0.5:
                    row['amount'] = rng.choice((None, 1, 2.5, 7))
                if rng.random() < 0.4:
                    row['note'] = rng.choice((None, 'x', 'y', ''))
                rows.append(row)
            return rows
        relations = []
        for _ in range(rng.randint(0, 6)):
            left = rng.choice(left_ids + ([999 if numeric_ids else 'Lx'] if rng.random() < 0.2 else []))
            right = rng.choice(right_ids + ([998 if numeric_ids else 'Rx'] if rng.random() < 0.2 else []))
            relations.append({'left': left, 'right': right, 'kind': rng.choice(('k1', 'k2'))})
        cases.append({'name': f'merge-{index:03d}', 'base': table(left_ids), 'incoming': table(right_ids),
                      'relations': relations,
                      'drop_timestamp': rng.choice((None, None, None, None, 'base', 'incoming', 'both')),
                      'shared_column': rng.random() < 0.3, 'relation_collision': rng.random() < 0.2})
    # Join-order cases: the inner merge yields exactly one pair per incoming
    # row although some rows match several relations and others none, with
    # string, integer or float ids, optionally sorted, and optional null ids.
    while len(cases) < 220:
        kind = rng.choice(('str', 'int', 'float'))
        pool = list(range(5))
        convert = (lambda n: f'R{n}') if kind == 'str' else (lambda n: 100 + n) if kind == 'int' \
            else (lambda n: 100.5 + n)
        ids = [rng.choice(pool) for _ in range(rng.randint(2, 9))]
        rights = [rng.choice(pool) for _ in range(rng.randint(1, 7))]
        if kind != 'int' and rng.random() < 0.3:
            ids[rng.randrange(len(ids))] = None
            rights[rng.randrange(len(rights))] = None
        if rng.random() < 0.4:
            ids.sort(key=lambda n: -1 if n is None else n)
            rights.sort(key=lambda n: -1 if n is None else n)
        pairs = sum(1 for a in ids for b in rights if a == b)
        if pairs != len(ids) or all(sum(1 for b in rights if a == b) == 1 for a in ids):
            continue
        incoming = [{'case': None if n is None else convert(n), 'activity': rng.choice('abcde'),
                     'minute': rng.choice((0, 1, 1, 2, 3))} for n in ids]
        left_ids = ['L0', 'L1', 'L2'] if kind == 'str' else [0, 1, 2]
        base = [{'case': rng.choice(left_ids), 'activity': rng.choice('abcde'), 'minute': rng.choice((0, 1, 2))}
                for _ in range(rng.randint(0, 4))]
        relations = [{'left': rng.choice(left_ids), 'right': None if n is None else convert(n),
                      'kind': rng.choice(('k1', 'k2'))} for n in rights]
        cases.append({'name': f'merge-{len(cases):03d}', 'base': base, 'incoming': incoming,
                      'relations': relations, 'drop_timestamp': rng.choice((None, None, 'incoming')),
                      'shared_column': rng.random() < 0.5, 'relation_collision': False})
    return cases


def run(cases):
    import warnings
    warnings.filterwarnings('ignore')
    import importlib.util
    import pandas as pd
    from pm4py.algo.merging.case_relations import algorithm
    spec = importlib.util.spec_from_file_location(
        'table_reference', Path(__file__).with_name('capture-case-relations-table-reference.py'))
    reference = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reference)
    origin = pd.Timestamp('2024-01-01', tz='UTC')
    out = []
    for case in cases:
        record = {}
        for config, parameters in CONFIGS.items():
            case_key = parameters.get('pm4py:param:case_id_key', 'case:concept:name')
            time_key = parameters.get('pm4py:param:timestamp_key', 'time:timestamp')
            left = parameters.get('left_suffix', '_LEFT')
            right = parameters.get('right_suffix', '_RIGHT')

            def frame(rows, side):
                data = {case_key: [r['case'] for r in rows], 'concept:name': [r['activity'] for r in rows],
                        time_key: pd.to_datetime([None if r['minute'] is None else origin + pd.Timedelta(minutes=r['minute'])
                                                  for r in rows], utc=True)}
                for column in ('amount', 'note'):
                    if any(column in r for r in rows):
                        data[column] = [r.get(column) for r in rows]
                if case['shared_column']:
                    data['link'] = [f'{side}{n}' for n in range(len(rows))]
                result = pd.DataFrame(data)
                if case['drop_timestamp'] in (side, 'both'):
                    del result[time_key]
                return result
            relation_columns = {case_key + left: [r['left'] for r in case['relations']],
                                case_key + right: [r['right'] for r in case['relations']],
                                'kind': [r['kind'] for r in case['relations']]}
            if case['relation_collision']:
                relation_columns['link'] = ['rel' for _ in case['relations']]
            row = reference.run_case(case['name'], frame(case['base'], 'base'), frame(case['incoming'], 'incoming'),
                                     pd.DataFrame(relation_columns), None, parameters)
            row.pop('name')
            if 'error' in row:
                row['error'].pop('message', None)
            record[config] = row
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
    bank = {'reference': f'pm4py {pm4py.__version__}', 'bank_seed': BANK_SEED, 'hash_seeds': list(HASH_SEEDS),
            'configs': CONFIGS, 'cases': cases}
    data = json.dumps(bank, indent=0, sort_keys=True).encode('utf-8')
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0,
                                                      compresslevel=9, filename='') as stream:
        stream.write(data)
    print(f'{len(cases)} cases, calls by outcome {summary}')


if __name__ == '__main__':
    main()
