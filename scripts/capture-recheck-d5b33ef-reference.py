#!/usr/bin/env python3
"""Fuzz cases and public PM4Py 2.7.22 results for the recheck of d5b33ef.

Run with the pinned reference environment (pm4py 2.7.22, pandas 3.0.5,
numpy 2.4.6). Only public calls are made; inputs are random tables in the
token format of src/backend/tests/test_recheck_d5b33ef.cpp.

  fuzz-run-keys OUT SEED N   consecutive activity/case grouping over wide
                             object integers and typed int64/uint64/bool/
                             float64 case and activity columns (N3)
  filters CASES OUT          public results for those cases (.jsonl)
  fuzz-org OUT SEED N        network analysis with tied, paired and missing
                             sorting keys (2-90 rows, N2) and batches with
                             equal mixed-type ids, NaN/None cells and object,
                             str or float64 case and id columns (N4)
  fuzz-representatives OUT SEED N
                             batches whose case ids are equal values of
                             different types (True/1/1.0, False/0/0.0)
  org CASES OUT [SEEDS]      public results for org cases (.jsonl), run in
                             SEEDS (default 40) processes with hash seeds
                             0..SEEDS-1; a case whose outcome or value differs
                             is marked unstable, one that always raises but
                             with different messages any-error, and both list
                             every distinct observed result as alternatives
  pnml OUT                   public PNML imports of stochastic properties
                             (weight, priority, invisible; N5)

Tokens: S text, I int, U unsigned int, J int kept as text, F float, B bool
(0/1), N None, A pd.NA, D timestamp.
"""
import io
import json
import math
import os
import random
import subprocess
import sys
import warnings
from pathlib import Path


def decode(token):
    import pandas as pd
    if token == 'N': return None
    if token == 'A': return pd.NA
    tag, text = token[:1], token[1:]
    if tag in ('I', 'U', 'J'): return int(text)
    if tag in ('F', 'G'): return float(text)
    if tag == 'B': return text == '1'
    if tag == 'D': return pd.NaT if text == 'NaT' else pd.Timestamp(text)
    return text


# ---------------------------------------------------------------- run keys

def fuzz_run_keys(out, seed, n):
    rnd = random.Random(seed)
    i64, u64 = (-2 ** 63, 2 ** 63 - 1), (0, 2 ** 64 - 1)

    def wide():
        digits = rnd.choice([18, 19, 20, 29, 30, 31, 32, 40, 60, 100, 120])
        return rnd.randint(10 ** (digits - 1), 10 ** digits - 1) * rnd.choice([1, -1])

    def near(value):
        return value + rnd.choice([-2, -1, 0, 1, 2])

    cases = []
    for i in range(n):
        mode = rnd.choice(['obj', 'obj', 'obj', 'i64', 'u64', 'mix', 'bool', 'f64'])
        base, swap, rows = wide(), rnd.random() < 0.5, []
        for j in range(rnd.randint(2, 9)):
            if mode == 'obj':
                k = rnd.random()
                c = ('J' + str(near(base)) if k < 0.7 else 'I' + str(rnd.randint(-3, 3)) if k < 0.85
                     else 'B' + rnd.choice('01') if k < 0.95 else 'F' + rnd.choice(['0.5', '1.0', '1e30', '-2.0']))
                a = ('I' + str(rnd.choice([0, 0, 1, -1, 2])) if rnd.random() < 0.8
                     else 'J' + str(-near(base)) if rnd.random() < 0.5 else 'B1')
                cd, ad = 'object', 'object'
            elif mode == 'i64':
                b = rnd.choice([i64[0], i64[1], 0, rnd.randint(*i64)])
                c = 'I' + str(max(i64[0], min(i64[1], near(b))))
                a = 'I' + str(rnd.choice([i64[0], i64[1], 0, 1, -1, 2]))
                cd, ad = 'int64', 'int64'
            elif mode == 'u64':
                b = rnd.choice([u64[1], 0, rnd.randint(*u64)])
                c = 'U' + str(max(0, min(u64[1], near(b))))
                a = 'U' + str(rnd.choice([u64[1], 0, 1, 2]))
                cd, ad = 'uint64', 'uint64'
            elif mode == 'mix':
                c = 'I' + str(rnd.choice([i64[1], i64[0], 5, -5]))
                a = 'U' + str(rnd.choice([u64[1], 2 ** 63, 0, 1]))
                cd, ad = 'int64', 'uint64'
                if swap: c, a, cd, ad = a, c, ad, cd
            elif mode == 'bool':
                c, a, cd, ad = 'B' + rnd.choice('01'), 'B' + rnd.choice('01'), 'bool', 'bool'
            else:
                c = 'F' + rnd.choice(['1e30', '1.5', '-0.0', '9007199254740993'])
                a = 'I' + str(rnd.choice([0, 1, 2 ** 53]))
                cd, ad = 'float64', 'int64'
            rows.append([c, a, 'D2035-09-08T12:00:%02dZ' % rnd.randint(0, 5), 'Spayload%d' % j, 'Sindex%d' % j])
        cases.append({'id': f'runkey{seed}_{i}_{mode}', 'op': rnd.choice(['group_act', 'group_act_last']),
                      'lo': 'N', 'hi': 'N', 'keys': ['ticket', 'step', 'stamp'], 'cdtype': cd, 'adtype': ad,
                      'rows': rows})
    Path(out).write_text(json.dumps(cases))


def simple(v):
    import numpy as np
    import pandas as pd
    if v is pd.NA: return {'null': 'NA'}
    if v is pd.NaT: return {'null': 'NaT'}
    if v is None: return {'null': 'None'}
    if isinstance(v, (float, np.floating)):
        return {'null': 'NaN'} if math.isnan(v) else float(v)
    if isinstance(v, np.integer): return int(v)
    if isinstance(v, pd.Timestamp): return {'timestamp': v.isoformat()}
    if isinstance(v, np.bool_): return bool(v)
    return v


def filters(cases_path, out):
    import pandas as pd
    from pm4py.algo.filtering.pandas.consecutive_act_case_grouping import consecutive_act_case_grouping_filter as group_act

    def run(case):
        ck, ak, tk = case['keys']
        data = [[decode(t) for t in row[:4]] + [i] for i, row in enumerate(case['rows'])]
        df = pd.DataFrame(data, columns=[ck, ak, tk, 'payload', 'row_id'], dtype=object)
        for c in [ck, ak, 'payload']: df[c] = df[c].astype(object)
        if case['cdtype'] != 'object':
            df[ck] = pd.Series([decode(r[0]) for r in case['rows']], dtype=case['cdtype']).values
        if case['adtype'] != 'object':
            df[ak] = pd.Series([decode(r[1]) for r in case['rows']], dtype=case['adtype']).values
        df[tk] = pd.to_datetime(df[tk], utc=True).astype('datetime64[ns, UTC]')
        df['row_id'] = df['row_id'].astype('int64')
        df.index = pd.Index([decode(row[4]) for row in case['rows']], dtype=object, name='custom_idx')
        params = {'pm4py:param:case_id_key': ck, 'pm4py:param:activity_key': ak, 'pm4py:param:timestamp_key': tk}
        if case['op'].endswith('_last'): params['filter_type'] = 'last'
        result = group_act.apply(df, parameters=params)
        return {'columns': list(result.columns), 'row_ids': list(result['row_id']),
                'index': [simple(x) for x in result.index], 'index_name': result.index.name,
                'rows': [[simple(x) for x in row] for row in result.itertuples(index=False, name=None)],
                'dtypes': [str(x) for x in result.dtypes]}

    with open(out, 'w') as handle, warnings.catch_warnings():
        warnings.simplefilter('ignore')
        for case in json.loads(Path(cases_path).read_text()):
            try: record = {'id': case['id'], 'status': 'OK', 'result': run(case)}
            except Exception as e: record = {'id': case['id'], 'status': type(e).__name__, 'error': str(e)}
            handle.write(json.dumps(record, ensure_ascii=False) + '\n')


# ---------------------------------------------------------------- org cases

def ts(seconds):
    return 'D1970-01-01T%02d:%02d:%02dZ' % (seconds // 3600, (seconds // 60) % 60, seconds % 60)


def fuzz_org(out, seed, n):
    rnd = random.Random(seed)
    pools = {'I': ['I0', 'I1', 'I2', 'I-1'], 'F': ['F0.5', 'F1.0', 'F2.25', 'F-1.0'], 'S': ['Sa', 'Sb', 'Sc', 'S'],
             'D': ['D1970-01-02T00:00:00Z', 'D1970-01-03T00:00:00Z', 'D1970-01-01T00:00:00Z'], 'B': ['B0', 'B1'],
             'IF': ['I1', 'F1.0', 'I0', 'F0.5']}
    cases = []
    for i in range(n):
        if rnd.random() < 0.5:
            mode = rnd.choice(['sort', 'sort', 'group'])
            m = rnd.choice([rnd.randint(2, 16), rnd.randint(15, 20), rnd.randint(30, 34), rnd.randint(60, 90)])
            rk = rnd.choice(list(pools))
            style = rnd.choice(['all', 'pairs', 'few', 'missing', 'distinct', 'three'])
            pool = pools[rk]
            stamps = (sorted(rnd.sample(range(100, 100 + 3 * m), m)) if rnd.random() < 0.7
                      else [rnd.randint(100, 140) for _ in range(m)])
            rows = []
            for j in range(m):
                if style == 'all': r = pool[0]
                elif style == 'pairs': r = pool[(j // 2) % len(pool)]
                elif style == 'few': r = rnd.choice(pool[:2])
                elif style == 'missing': r = rnd.choice([pool[0], 'N'])
                elif style == 'three': r = pool[j % 3 if len(pool) > 2 else j % 2]
                else:
                    r = {'I': lambda: 'I%d' % rnd.randint(-50, 50), 'F': lambda: 'F%s' % (rnd.randint(-50, 50) / 4),
                         'S': lambda: 'S' + rnd.choice('abcdefghij'), 'D': lambda: ts(rnd.randint(0, 80000)),
                         'B': lambda: rnd.choice(['B0', 'B1']),
                         'IF': lambda: rnd.choice(['I%d' % rnd.randint(0, 9), 'F%d.5' % rnd.randint(0, 9)])}[rk]()
                rows.append(['Scase', 'SA%d' % rnd.randint(0, 4), 'SR%d' % rnd.randint(0, m), ts(stamps[j]), r,
                             rnd.choice(['Sg', 'Sg', 'Sh']) if rnd.random() < 0.8 else 'Sg'])
            cases.append({'id': f'net{seed}_{i}_{mode}_{rk}_{style}_{m}', 'kind': 'network', 'mode': mode,
                          'minimum': 1, 'distance': 0, 'rows': rows})
            continue
        cd = rnd.choice(['object', 'object', 'str', 'float64'])
        withid = rnd.random() < 0.7
        idd = rnd.choice(['object', 'object', 'str', 'float64']) if withid else 'object'
        span, rows = rnd.choice([0, 4, 10, 30]), []
        for j in range(rnd.randint(2, 12)):
            c = rnd.choice({'object': ['B0', 'I0', 'F0.0', 'B1', 'I1', 'F1.0', 'Sc', 'Fnan', 'N', 'I2'],
                            'str': ['Sc', 'Sd', 'Fnan', 'N'], 'float64': ['F0.0', 'F1.0', 'Fnan', 'F2.5']}[cd])
            e = rnd.choice({'object': ['B0', 'I0', 'F0.0', 'B1', 'I1', 'F1.0', 'Sx', 'Fnan', 'N'],
                            'str': ['Sx', 'Sy', 'Fnan', 'N'], 'float64': ['F0.0', 'F1.0', 'Fnan']}[idd])
            if cd == 'object' and idd == 'object' and rnd.random() < 0.5:
                c = rnd.choice(['B0', 'I0', 'F0.0', 'B1', 'I1', 'F1.0'])
                e = rnd.choice(['B0', 'I0', 'F0.0', 'B1', 'I1', 'F1.0'])
            b = rnd.randint(0, span)
            rows.append([c, rnd.choice(['SPack', 'SPack', 'SShip']), 'Screw', ts(b), ts(b + rnd.choice([0, 0, 1, 3])),
                         e if withid else 'Sx'])
        cases.append({'id': f'batch{seed}_{i}_{cd}_{idd}', 'kind': 'batches',
                      'mode': {'object': 'default', 'str': 'case-str', 'float64': 'case-float64'}[cd],
                      'case_dtype': cd, 'id_dtype': idd, 'event_id': withid, 'minimum': rnd.choice([1, 1, 2]),
                      'distance': rnd.choice([0, 0, 2]), 'rows': rows})
    Path(out).write_text(json.dumps(cases))


def fuzz_representatives(out, seed, n):
    rnd = random.Random(seed)
    values = ['B1', 'I1', 'F1.0', 'B0', 'I0', 'F0.0', 'I2', 'F2.0']
    cases = []
    for i in range(n):
        span, rows = rnd.choice([3, 6, 10]), []
        for _ in range(rnd.randint(3, 10)):
            b = rnd.randint(0, span)
            rows.append([rnd.choice(values), 'SA', 'Sr', ts(b), ts(b + rnd.choice([0, 1, 1, 2])), 'Sx'])
        cases.append({'id': f'rep{seed}_{i}', 'kind': 'batches', 'mode': 'default', 'case_dtype': 'object',
                      'id_dtype': 'object', 'event_id': False, 'minimum': 1, 'distance': rnd.choice([0, 0, 1, 2]),
                      'rows': rows})
    Path(out).write_text(json.dumps(cases))


def org_once(cases_path, out):
    import numpy as np
    import pandas as pd
    from pm4py.algo.discovery.batches.variants import pandas as batches
    from pm4py.algo.organizational_mining.network_analysis.variants import dataframe as network

    def scalar(v): return None if v is None else str(v)

    def batch_values(result):
        groups = []
        for (activity, resource), count, by_type in result:
            found = []
            for name, items in by_type.items():
                t = name.lower()
                index = 0 if 'simultaneous' in t else 1 if 'start' in t else 2 if 'end' in t else 3 if 'sequential' in t else 4
                for start, end, events in items:
                    found.append([index, float(start), float(end),
                                  [[float(e[0]), float(e[1]), scalar(e[2]), scalar(e[3]) if len(e) > 3 else '']
                                   for e in events]])
            groups.append([scalar(activity), scalar(resource), int(count), found])
        return groups

    with open(out, 'w') as handle, warnings.catch_warnings():
        warnings.simplefilter('ignore')
        for case in json.loads(Path(cases_path).read_text()):
            record = {'id': case['id']}
            try:
                if case['kind'] == 'batches':
                    columns = ['case', 'activity', 'resource', 'start', 'end', 'identity']
                    df = pd.DataFrame({k: pd.Series([decode(row[i]) for row in case['rows']],
                                                    dtype=case['id_dtype'] if k == 'identity' else
                                                    case['case_dtype'] if k == 'case' else 'object')
                                       for i, k in enumerate(columns)})
                    for k in ['start', 'end']: df[k] = pd.to_datetime(df[k], utc=True).astype('datetime64[ns, UTC]')
                    p = {'pm4py:param:case_id_key': 'case', 'pm4py:param:activity_key': 'activity',
                         'pm4py:param:resource_key': 'resource', 'pm4py:param:start_timestamp_key': 'start',
                         'pm4py:param:timestamp_key': 'end', 'merge_distance': case['distance'],
                         'min_batch_size': case['minimum']}
                    if case['event_id']: p['event_id_key'] = 'identity'
                    value = batch_values(batches.apply(df, parameters=p))
                else:
                    columns = ['case:concept:name', 'concept:name', 'org:resource', 'time:timestamp', 'rank', 'relation']
                    df = pd.DataFrame({k: pd.Series([decode(row[i]) for row in case['rows']], dtype='object')
                                       for i, k in enumerate(columns)})
                    df['time:timestamp'] = pd.to_datetime(df['time:timestamp'], utc=True).astype('datetime64[ns, UTC]')
                    p = {'include_performance': True, 'sorting_column': 'rank', 'out_column': 'relation',
                         'in_column': 'relation'}
                    if case['mode'] == 'group': p.update(node_column_target='concept:name', edge_column='relation')
                    result = network.apply(df, parameters=p)
                    value = [[scalar(s), scalar(t), scalar(a), len(times), [float(x) for x in times]]
                             for (s, t), acts in result.items() for a, times in acts.items()]
                record.update(ok=True, value=value)
            except Exception as e:
                record.update(ok=False, error_type=type(e).__name__, error=str(e))
            handle.write(json.dumps(record) + '\n')


def org(cases_path, out, seeds=40):
    runs = []
    for seed in range(seeds):
        path = f'{out}.seed{seed}'
        subprocess.run([sys.executable, '-I', __file__, 'org-once', cases_path, path], check=True,
                       env={**os.environ, 'PYTHONHASHSEED': str(seed)})
        runs.append({r['id']: r for r in map(json.loads, Path(path).read_text().splitlines())})
        Path(path).unlink()

    def canonical(record):
        if not record['ok']: return 'error:' + record['error']
        return json.dumps(sorted(json.dumps(group if len(group) != 4 else
                                            group[:3] + [sorted(json.dumps(b[:3] + [sorted(map(json.dumps, b[3]))])
                                                                for b in group[3])])
                                 for group in record['value']))

    with open(out, 'w') as handle:
        for case_id, first in runs[0].items():
            seen = {}
            for run in runs:
                seen.setdefault(canonical(run[case_id]), run[case_id])
            if len(seen) > 1:
                outcomes = {r['ok'] for r in seen.values()}
                first['stability'] = 'unstable' if len(outcomes) > 1 or first['ok'] else 'any-error'
                first['alternatives'] = [{k: r[k] for k in ('ok', 'value', 'error') if k in r} for r in seen.values()]
            handle.write(json.dumps(first) + '\n')


# ---------------------------------------------------------------- PNML properties

PROPERTY_CASES = (
    [('weight', w) for w in [
        '1_000.5', '٣.٥', '1.25junk', '0x1p+2', '1e', '1,5', '', None, ' 3.5 ', ' 3.5 ', '1__0',
        '_1', '1_', '1_e5', '1e1_0', '1._5', '1_.5', '.5', '5.', '.', '-.5e-3', 'infinity', '-iNfInItY', 'nAn',
        '+inf', 'infin', '1e999', '٣_٥', '\U0001d7d1.\U0001d7d3', '1' * 400, '5e-324', '1e-400',
        '2.2250738585072014e-308', '1.7976931348623157e308', '0', '+0.0', '7', '1E+2', '1e-2', '  ', 'xé',
        '1 2', '+-1', '0.1e1_0', '١٢٣', '３.5', '1.5\n', '\t2\r']] +
    [('priority', p) for p in [
        '1_0', ' 2 ', '٣', '1.5', '', None, '-3', '+4', '0x1', '00', '1e2', '९', '2147483647', '-2147483648',
        '2147483648', '99999999999999999999', '  ٣_٤ ', '_1', '1_', '1__2', '+', '- 1', '0' * 300 + '7',
        'x' * 250]] +
    [('invisible', v) for v in ['true', 'True', 'TRUE', 'tRuE', ' true', '1', 'false', 'yes', '', None, 'true ']])


def pnml_model(key, text):
    from xml.etree import ElementTree as ET
    root = ET.Element('pnml')
    net = ET.SubElement(root, 'net', id='n', type='http://www.pnml.org/version-2009/grammar/pnmlcoremodel')
    page = ET.SubElement(net, 'page', id='page')
    for i in range(2):
        place = ET.SubElement(page, 'place', id=f'p{i}')
        ET.SubElement(ET.SubElement(place, 'name'), 'text').text = f'p{i}'
        if i == 0: ET.SubElement(ET.SubElement(place, 'initialMarking'), 'text').text = '1'
    t = ET.SubElement(page, 'transition', id='t0')
    ET.SubElement(ET.SubElement(t, 'name'), 'text').text = 'A'
    tool = ET.SubElement(t, 'toolspecific', tool='StochasticPetriNet', version='0.2')
    props = {'distributionType': 'EXPONENTIAL', 'distributionParameters': '1.0', 'priority': '0',
             'invisible': 'false', 'weight': '1.0'}
    for name, value in props.items():
        element = ET.SubElement(tool, 'property', key=name)
        element.text = text if name == key else value
    ET.SubElement(page, 'arc', id='a0', source='p0', target='t0')
    ET.SubElement(page, 'arc', id='a1', source='t0', target='p1')
    final = ET.SubElement(ET.SubElement(net, 'finalmarkings'), 'marking')
    ET.SubElement(ET.SubElement(final, 'place', idref='p1'), 'text').text = '1'
    return ET.tostring(root, encoding='unicode')


def pnml(out):
    import contextlib
    import tempfile
    from pm4py.objects.petri_net.importer import importer
    parameter = importer.Variants.PNML.value.Parameters.RETURN_STOCHASTIC_MAP
    with open(out, 'w') as handle, tempfile.TemporaryDirectory() as tmp:
        for key, text in PROPERTY_CASES:
            path = Path(tmp) / 'model.pnml'
            path.write_text(pnml_model(key, text), encoding='utf-8')
            record = {'key': key, 'text': text}
            try:
                with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                    net, _, _, smap = importer.apply(str(path), parameters={parameter: True})
                value = list(smap.values())[0]
                transition = list(net.transitions)[0]
                record.update(ok=True, weight=repr(value.get_weight()), priority=repr(value.get_priority()),
                              invisible=transition.label is None)
            except Exception as e:
                record.update(ok=False, error_type=type(e).__name__, error=str(e))
            handle.write(json.dumps(record, ensure_ascii=False) + '\n')


def main():
    command, args = sys.argv[1], sys.argv[2:]
    if command == 'fuzz-run-keys': fuzz_run_keys(args[0], int(args[1]), int(args[2]))
    elif command == 'filters': filters(args[0], args[1])
    elif command == 'fuzz-org': fuzz_org(args[0], int(args[1]), int(args[2]))
    elif command == 'fuzz-representatives': fuzz_representatives(args[0], int(args[1]), int(args[2]))
    elif command == 'org': org(args[0], args[1], *map(int, args[2:]))
    elif command == 'org-once': org_once(args[0], args[1])
    elif command == 'pnml': pnml(args[0])
    else: raise SystemExit(__doc__)


if __name__ == '__main__':
    main()
