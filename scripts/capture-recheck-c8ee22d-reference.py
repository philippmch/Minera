#!/usr/bin/env python3
"""Fuzz cases and public PM4Py 2.7.22 results for the recheck of c8ee22d.

Run with the pinned reference environment (pm4py 2.7.22, pandas 3.0.5).
Only public calls are made; inputs are random tables in the token format the
native test reads (see src/backend/tests/test_recheck_c8ee22d.cpp).

  fuzz-filters OUT SEED N MAXROWS   random pandas sequence/case filter cases
  fuzz-filter-keys OUT SEED N       filter cases with 17-50 mixed-type case ids
  filters CASES OUT                 public results for filter cases (.jsonl)
  fuzz-batches OUT SEED N           random batch tables with varied intervals
  fuzz-batch-keys OUT SEED N        batch tables with 15-60 mixed-type
                                    activity or resource keys
  batches CASES OUT                 public results for batch/network cases
  sort-logs OUT SEED N              numpy's object argsorts (quicksort and
                                    stable), comparison by comparison

The key fuzzers reach past numpy's small-array paths (insertion sort up to 16
items), where the order of the comparisons that raise depends on its
partitioning.

Filter tokens: S text, I int, U unsigned int, F float, B bool (0/1), N None,
A pd.NA, D timestamp, J/G int/float the native side keeps only as text (a
log reader's lexical value), L/T list/tuple of comma-separated tokens.
"""
import gzip
import json
import math
import random
import sys
import warnings
from datetime import datetime
from pathlib import Path

OPS = ['rework', 'end', 'group_act', 'start', 'suffix', 'prefix', 'split', 'ncases', 'size', 'group_time',
       'performance_us']
KINDS = ['S', 'I', 'F', 'B', 'N', 'D', 'd', 'J']


def fuzz_filters(out, seed, n, maxrows):
    rnd = random.Random(seed)

    def tok(kind, pool):
        if kind == 'S': return 'S' + rnd.choice(pool)
        if kind == 'I': return 'I' + str(rnd.choice([0, 1, 2, 3]))
        if kind == 'F': return 'F' + rnd.choice(['1.0', '2.0', '1.5', '0.0', 'nan'])
        if kind == 'B': return 'B' + rnd.choice(['0', '1'])
        if kind == 'N': return 'N'
        if kind == 'D': return 'D' + rnd.choice(['2033-03-01T00:15:00Z', '2033-03-01T03:15:00+03:00', '2033-03-01T00:16:00Z'])
        if kind == 'd': return 'D' + rnd.choice(['2033-03-01T00:15:00', '2033-03-01T00:16:00'])
        return 'J' + rnd.choice(['18446744073709551617', '1180591620717411303425', '-9223372036854775809'])

    cases = []
    for i in range(n):
        op = rnd.choice(OPS)
        m = rnd.randint(1, maxrows)
        ckinds = rnd.sample(KINDS, rnd.choice([1, 1, 2]))
        akinds = rnd.sample(['S', 'I', 'F', 'B', 'N'], rnd.choice([1, 1, 2]))
        rows = [[tok(rnd.choice(ckinds), ['x', 'y', 'z']), tok(rnd.choice(akinds), ['A', 'B', 'C']),
                 'D2033-04-02T06:15:%02dZ' % rnd.randint(0, 59), 'Sp%d' % j, 'Srow%d' % j] for j in range(m)]
        lo = tok(rnd.choice(akinds + ['S']), ['A', 'B', 'C'])
        if op == 'split' and rnd.random() < 0.6:
            items = [tok(rnd.choice(akinds + ['S']), ['A', 'B', 'C']) for _ in range(rnd.randint(0, 3))]
            lo = rnd.choice('LT') + ','.join(items)
        hi = 'N'
        if op in ('prefix', 'suffix'): hi = rnd.choice(['B1', 'B0', 'N'])
        if op == 'size': lo, hi = 'I1', 'I4'
        if op == 'performance_us': lo, hi = 'I0', 'I100'
        cases.append({'id': f'q{seed}_{i}_{op}', 'op': op, 'lo': lo, 'hi': hi, 'keys': ['ticket', 'step', 'stamp'],
                      'rows': rows})
    Path(out).write_text(json.dumps(cases))


def fuzz_filter_keys(out, seed, n):
    rnd = random.Random(seed)

    def key(kind):
        if kind == 'S': return 'S' + rnd.choice('abcdefghij') * rnd.randint(1, 3)
        if kind == 'I': return 'I%d' % rnd.randint(-50, 50)
        if kind == 'F': return 'F' + repr(rnd.randint(-500, 500) / 7)
        if kind == 'B': return 'B' + rnd.choice('01')
        day = 'D2031-06-%02dT%02d:00:00' % (rnd.randint(1, 28), rnd.randint(0, 23))
        if kind == 'D': return day + rnd.choice(['Z', '+03:00'])
        return day

    cases = []
    for i in range(n):
        op = rnd.choice(OPS)
        kinds = rnd.sample(['S', 'I', 'F', 'B', 'D', 'd'], rnd.choice([1, 2, 2, 3]))
        weights = [rnd.random() for _ in kinds]
        rows = [[key(rnd.choices(kinds, weights)[0]), 'S' + rnd.choice('ABC'),
                 'D2033-04-02T06:15:%02dZ' % rnd.randint(0, 59), 'Sp%d' % j, 'Srow%d' % j]
                for j in range(rnd.randint(17, 50))]
        lo, hi = 'SA', 'N'
        if op in ('prefix', 'suffix'): hi = rnd.choice(['B1', 'B0', 'N'])
        if op == 'size': lo, hi = 'I1', 'I4'
        if op == 'performance_us': lo, hi = 'I0', 'I100'
        if op == 'split': lo = 'LSA,SB'
        cases.append({'id': f'k{seed}_{i}_{op}', 'op': op, 'lo': lo, 'hi': hi, 'keys': ['ticket', 'step', 'stamp'],
                      'rows': rows})
    Path(out).write_text(json.dumps(cases))


def filters(cases_path, out):
    import numpy as np
    import pandas as pd
    from pm4py.algo.filtering.pandas.activity_split import activity_split_filter
    from pm4py.algo.filtering.pandas.cases import case_filter
    from pm4py.algo.filtering.pandas.consecutive_act_case_grouping import consecutive_act_case_grouping_filter as group_act
    from pm4py.algo.filtering.pandas.end_activities import end_activities_filter as end_filter
    from pm4py.algo.filtering.pandas.prefixes import prefix_filter
    from pm4py.algo.filtering.pandas.rework import rework_filter
    from pm4py.algo.filtering.pandas.start_activities import start_activities_filter as start_filter
    from pm4py.algo.filtering.pandas.suffixes import suffix_filter
    from pm4py.algo.filtering.pandas.timestamp_case_grouping import timestamp_case_grouping_filter as group_time

    def decode(token):
        if token == 'N': return None
        if token == 'A': return pd.NA
        tag, text = token[:1], token[1:]
        if tag in ('I', 'U', 'J'): return int(text)
        if tag in ('F', 'G'): return float(text)
        if tag == 'B': return text == '1'
        if tag == 'D': return pd.NaT if text == 'NaT' else pd.Timestamp(text)
        if tag in 'LT':
            items = [decode(x) for x in text.split(',')] if text else []
            return items if tag == 'L' else tuple(items)
        return text

    def simple(v):
        if v is pd.NA: return {'null': 'NA'}
        if v is pd.NaT: return {'null': 'NaT'}
        if v is None: return {'null': 'None'}
        if isinstance(v, (float, np.floating)):
            return {'null': 'NaN'} if math.isnan(v) else float(v)
        if isinstance(v, np.integer): return int(v)
        if isinstance(v, pd.Timestamp): return {'timestamp': v.isoformat()}
        if isinstance(v, np.bool_): return bool(v)
        return v

    def run(case):
        op = case['op']
        ck, ak, tk = case['keys']
        columns = [ck, ak, tk, 'payload', 'row_id']
        data = [[decode(t) for t in row[:4]] + [i] for i, row in enumerate(case['rows'])]
        # Object cells built directly, so inference cannot turn None into NaN.
        df = pd.DataFrame(data, columns=columns, dtype=object)
        for c in [ck, ak, 'payload']: df[c] = df[c].astype(object)
        unit = op.split('_', 1)[1] if op.startswith('performance_') else 'ns'
        df[tk] = pd.to_datetime(df[tk], utc=True).astype('datetime64[' + unit + ', UTC]')
        df['row_id'] = df['row_id'].astype('int64')
        df.index = pd.Index([decode(row[4]) for row in case['rows']], dtype=object, name='custom_idx')
        lo, hi = decode(case['lo']), decode(case['hi'])
        params = {'pm4py:param:case_id_key': ck, 'pm4py:param:activity_key': ak, 'pm4py:param:timestamp_key': tk}
        if case['hi'] != 'N': params['strict'] = hi
        if op in ['start', 'end']: out = ({'start': start_filter, 'end': end_filter}[op]).apply(df, [lo], parameters=params)
        elif op == 'rework': out = rework_filter.apply(df, lo, parameters=params)
        elif op == 'split': out = activity_split_filter.apply(df, lo, parameters=params)
        elif op == 'size': out = case_filter.filter_on_case_size(df, case_id_glue=ck, min_case_size=lo, max_case_size=hi)
        elif op == 'ncases': out = case_filter.filter_on_ncases(df, case_id_glue=ck, max_no_cases=hi)
        elif op.startswith('performance'):
            out = case_filter.filter_on_case_performance(df, case_id_glue=ck, timestamp_key=tk,
                                                         min_case_performance=lo, max_case_performance=hi)
        elif op in ['prefix', 'suffix']: out = ({'prefix': prefix_filter, 'suffix': suffix_filter}[op]).apply(df, lo, parameters=params)
        else: out = ({'group_act': group_act, 'group_time': group_time}[op]).apply(df, parameters=params)
        return {'columns': list(out.columns), 'row_ids': list(out['row_id']), 'index': [simple(x) for x in out.index],
                'index_name': out.index.name, 'rows': [[simple(x) for x in row] for row in out.itertuples(index=False, name=None)],
                'dtypes': [str(x) for x in out.dtypes]}

    results = []
    with warnings.catch_warnings():
        warnings.simplefilter('ignore')
        for c in json.loads(Path(cases_path).read_text()):
            try:
                res = {'id': c['id'], 'status': 'OK', 'result': run(c)}
            except Exception as e:
                res = {'id': c['id'], 'status': type(e).__name__, 'error': str(e)}
            results.append(res)
    Path(out).write_text(''.join(json.dumps(x, ensure_ascii=False) + '\n' for x in results))


def fuzz_batches(out, seed, n):
    rnd = random.Random(seed)

    def ts(s): return f'1970-01-01T00:{s // 60:02d}:{s % 60:02d}Z'
    cases = []
    for i in range(n):
        span = rnd.choice([6, 12, 30])
        ckinds = rnd.choice([['int'], ['str'], ['int', 'float'], ['int', 'str']])
        rows = []
        for _ in range(rnd.randint(2, 14)):
            b = rnd.randint(0, span)
            f = b + rnd.choice([0, 0, 1, 2, 3, 5, 8])
            k = rnd.choice(ckinds)
            c = rnd.randint(0, 4) if k == 'int' else float(rnd.randint(0, 4)) if k == 'float' else rnd.choice('abcde')
            rows.append([c, rnd.choice(['Pack', 'Pack', 'Ship']), rnd.choice(['crew', 'crew', 'solo']), ts(b), ts(f),
                         rnd.choice(['x', 'y', 'z', 1, 2])])
        event_id = rnd.random() < 0.5
        if event_id:
            for r in rows: r[5] = rnd.choice(['x', 'y', 'z'])
        cases.append(dict(id=f'iv{seed}_{i}', kind='batches', rows=rows, minimum=rnd.choice([1, 1, 2, 3]), event_id=event_id))
    Path(out).write_text(json.dumps(cases))


def fuzz_batch_keys(out, seed, n):
    rnd = random.Random(seed)

    def key(kind):
        if kind == 'int': return rnd.randint(-50, 50)
        if kind == 'float': return rnd.randint(-500, 500) / 7
        if kind == 'str': return rnd.choice('abcdefghij') * rnd.randint(1, 3)
        if kind == 'bool': return rnd.choice([True, False])
        day = f'2031-06-{rnd.randint(1, 28):02d}T{rnd.randint(0, 23):02d}:00:00'
        return {'t': 'date', 'v': day + (rnd.choice(['Z', '+03:00']) if kind == 'aware' else '')}

    cases = []
    for i in range(n):
        kinds = rnd.sample(['int', 'float', 'str', 'aware', 'naive', 'bool'], rnd.choice([1, 2, 2, 3]))
        weights = [rnd.random() for _ in kinds]
        column = rnd.choice([1, 2])
        rows = []
        for j in range(rnd.randint(15, 60)):
            begun = rnd.randint(0, 20)
            row = [f'c{rnd.randint(0, 30)}', 'Pack', 'crew', f'1970-01-01T00:00:{begun:02d}Z',
                   f'1970-01-01T00:00:{begun + rnd.choice([0, 1, 3, 6]):02d}Z', 'plain']
            row[column] = key(rnd.choices(kinds, weights)[0])
            rows.append(row)
        cases.append(dict(id=f'wk{seed}_{i}', kind='batches', rows=rows, minimum=rnd.choice([1, 2]), event_id=False))
    Path(out).write_text(json.dumps(cases))


def sort_logs(out, seed, n):
    # One line per array and kind: kind, values, every comparison numpy
    # makes ("<i,j" for values[i] < values[j], ">i,j" for >), the argsort.
    import numpy as np
    rnd = random.Random(seed)
    log = []

    class Item:
        def __init__(self, index, value): self.index, self.value = index, value
        def __lt__(self, other): log.append(f'<{self.index},{other.index}'); return self.value < other.value
        def __gt__(self, other): log.append(f'>{self.index},{other.index}'); return self.value > other.value

    lines = []
    for _ in range(n):
        size = rnd.choice([rnd.randint(2, 20), rnd.randint(15, 40), rnd.randint(2, 120)])
        span = rnd.choice([2, 10, 1000])
        values = [rnd.randint(-span, span) for _ in range(size)]
        if rnd.random() < 0.3: values.sort(reverse=rnd.random() < 0.5)
        for kind in ['quicksort', 'stable']:
            log.clear()
            items = np.empty(size, dtype=object)
            for i, v in enumerate(values): items[i] = Item(i, v)
            order = np.argsort(items, kind=kind)
            lines.append('\t'.join([kind, ' '.join(map(str, values)), ' '.join(log), ' '.join(map(str, order))]))
    with gzip.GzipFile(out, 'wb', mtime=0) as handle:
        handle.write(('\n'.join(lines) + '\n').encode())


def batches_and_networks(cases_path, out):
    import pandas as pd
    from pm4py.algo.discovery.batches.variants import pandas as batches
    from pm4py.algo.organizational_mining.network_analysis.variants import dataframe as network
    cols = ['case_key', 'phase', 'worker', 'begun', 'finished', 'identity']

    def decode(v):
        if not isinstance(v, dict): return v
        if v['t'] == 'date': return pd.Timestamp(v['v'])
        if v['t'] == 'na': return pd.NA
        if v['t'] == 'float': return float(v['v'])
        return int(v['v'])

    def scalar(v): return None if v is None else str(v)

    def batchnorm(result):
        norm = []
        for (a, r), n, groups in result:
            bs = []
            for typ, items in groups.items():
                t = typ.lower()
                idx = 0 if 'simultaneous' in t else 1 if 'start' in t else 2 if 'end' in t else 3 if 'sequential' in t else 4
                for start, end, events in items:
                    bs.append([idx, float(start), float(end),
                               [[float(e[0]), float(e[1]), scalar(e[2]), scalar(e[3]) if len(e) > 3 else ''] for e in events]])
            norm.append([scalar(a), scalar(r), int(n), bs])
        return norm

    with open(out, 'w') as handle:
        for c in json.loads(Path(cases_path).read_text()):
            rec = {'id': c['id']}
            try:
                if c['kind'] == 'batches':
                    df = pd.DataFrame({k: pd.Series([decode(row[i]) for row in c['rows']],
                                                    dtype=c.get('id_dtype', 'object') if k == 'identity' else 'object')
                                       for i, k in enumerate(cols)})
                    for k in ['begun', 'finished']:
                        df[k] = pd.Series(pd.to_datetime(df[k], format='ISO8601', utc=True)).astype('datetime64[ns, UTC]')
                    params = {'pm4py:param:case_id_key': 'case_key', 'pm4py:param:activity_key': 'phase',
                              'pm4py:param:resource_key': 'worker', 'pm4py:param:start_timestamp_key': 'begun',
                              'pm4py:param:timestamp_key': 'finished', 'merge_distance': 0, 'min_batch_size': c['minimum']}
                    if c['event_id']: params['event_id_key'] = 'identity'
                    value = batchnorm(batches.apply(df, parameters=params))
                else:
                    rows = []
                    for i, x in enumerate(c['values']):
                        row = {'case:concept:name': 'root', 'concept:name': ['Open', 'Work', 'Close', 'Fourth', 'Fifth', 'Sixth'][i],
                               'org:resource': ['p', 'q', 'r', 's', 't', 'u'][i],
                               'time:timestamp': pd.Timestamp(f'1970-01-01T00:00:{[4, 19, 31, 40, 52, 58][i]:02d}Z'), 'target': 'q'}
                        row[c['key']] = decode(x)
                        rows.append(row)
                    df = pd.DataFrame(rows, dtype=object)
                    df['time:timestamp'] = pd.to_datetime(df['time:timestamp'], utc=True)
                    params = {'include_performance': True}
                    if c['role'] == 'relation': params.update(out_column=c['key'], in_column=c['key'])
                    elif c['role'] == 'source': params.update(node_column_source=c['key'], node_column_target='target', edge_column='target')
                    elif c['role'] == 'target': params.update(node_column_source='target', node_column_target=c['key'], edge_column='target')
                    else: params.update(node_column_source='target', node_column_target='target', edge_column=c['key'])
                    r = network.apply(df, parameters=params)
                    value = [[scalar(s), scalar(t), scalar(a), len(ts), [float(x) for x in ts]]
                             for (s, t), acts in r.items() for a, ts in acts.items()]
                rec.update(ok=True, value=value)
            except Exception as ex:
                rec.update(ok=False, error_type=type(ex).__name__, error=str(ex))
            handle.write(json.dumps(rec) + '\n')


if __name__ == '__main__':
    command, args = sys.argv[1], sys.argv[2:]
    if command == 'fuzz-filters': fuzz_filters(args[0], int(args[1]), int(args[2]), int(args[3]))
    elif command == 'fuzz-filter-keys': fuzz_filter_keys(args[0], int(args[1]), int(args[2]))
    elif command == 'filters': filters(args[0], args[1])
    elif command == 'fuzz-batch-keys': fuzz_batch_keys(args[0], int(args[1]), int(args[2]))
    elif command == 'fuzz-batches': fuzz_batches(args[0], int(args[1]), int(args[2]))
    elif command == 'batches': batches_and_networks(args[0], args[1])
    elif command == 'sort-logs': sort_logs(args[0], int(args[1]), int(args[2]))
    else: sys.exit(__doc__)
