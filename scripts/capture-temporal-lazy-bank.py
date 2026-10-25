"""Capture the bank for algo.transformation.log_to_features.variants.temporal_lazy.

Seeded Polars LazyFrames (one to eight cases of one to five events, string or
integer case identifiers and activities, activities and resources that are
sometimes null, UTC or naive microsecond timestamps with whole-hour or
microsecond offsets, an optional start timestamp column, custom column names
passed through the parameters, single-row and empty frames, and frames
missing a column) go through the public temporal_lazy.apply under seeded
grouper frequencies (every unit the variant maps, multiples including 0 and
leading zeros, pandas-style suffixes, and strings it rejects). Separately,
_freq_to_polars_duration runs on seeded strings over ASCII letters, digits,
signs, separators and a few non-ASCII letters, digits and spaces.

Polars' group means come out in a thread-dependent order, so every case runs
in four fresh interpreters (PYTHONHASHSEED 0..3), twice in each, and keeps
every distinct outcome.

The bank is a gzip text file of tagged lines read by
minera_temporal_lazy_parity_tests:

  C name
  K freq | apply
  S json   freq: {"freq": string or null}
           apply: {"columns": [{"name", "kind": text|int|time, "cells"}],
                   "utc": bool, "params": {...}}; times are epoch microseconds
  F json   freq: {"result": string} or {"raises": type, "message"}
           apply: {"observed": [outcome, ...]}, each outcome {"rows": [[label,
                   13 values], ...]} or {"raises": type}

    python -I -B scripts/capture-temporal-lazy-bank.py \\
        --output docs/temporal-lazy-bank.txt.gz
"""
import argparse
import concurrent.futures
import datetime
import gzip
import json
import os
import random
import subprocess
import sys
from pathlib import Path

BANK_SEED = 20261101
HASH_SEEDS = (0, 1, 2, 3)
REPEATS = 2
ROOT = Path(__file__).resolve().parent.parent
EPOCH = datetime.datetime(1970, 1, 1)
FREQS = ['W', 'w', '2W', '3W', 'W-SUN', 'W-MON', '02W', ' 2W ', 'D', 'd', '3D', '2.5D', '1_0D', 'H', 'h', '12H',
         '5h', 'T', '90T', '90min', '30MIN', 'mins', 'S', 's', '45s', '3sec', 'second', 'M', 'm', 'ME', 'MS', '2ME',
         '5MS', 'ms', 'Q', 'QE', 'QS', '2Q', 'Q-DEC', 'Y', 'YE', 'YS', 'A', 'AS', '3Y', 'A-DEC', '0W', '0D', '0H',
         '0M', '0Q', '0Y', '0S', 'W 2', '+2W', 'Wx', 'B', 'BME', 'SM', 'L', 'us', 'N', '', ' ', '3', '-2W', 'x',
         'HOUR', 'mo', '1mo', '²W', '1١D', 'ſ', 'Mß']
FREQ_ALPHABET = list('SECONDMIHWTQYAXBLUsecondmihwtqyaxbl') + list('0123456789') * 2 + \
    [' ', '-', '+', '.', '_', ',', '\t', 'ß', 'ı', 'ſ', 'é', '²', '١', 'Ⅳ', '\xa0', ' ', 'ﬆ', '\x1c', '\x85']


def generate_freq(rng):
    if rng.random() < 0.25:
        return rng.choice(FREQS + [None])
    return ''.join(rng.choice(FREQ_ALPHABET) for _ in range(rng.randint(0, 7)))


def generate_apply(rng):
    utc = rng.random() < 0.5
    origin = EPOCH + datetime.timedelta(days=rng.randint(19000, 20500), hours=rng.randint(0, 23))
    fine = rng.random() < 0.4
    int_ids = rng.random() < 0.3
    int_activities = rng.random() < 0.2
    with_start = rng.random() < 0.4
    names = {'case': 'case:concept:name', 'activity': 'concept:name', 'resource': 'org:resource',
             'timestamp': 'time:timestamp', 'start': 'start_timestamp'}
    params = {}
    if rng.random() < 0.2:
        names = {'case': 'cid', 'activity': 'act', 'resource': 'res', 'timestamp': 'ts', 'start': 'st'}
        params = {'pm4py:param:case_id_key': 'cid', 'pm4py:param:activity_key': 'act',
                  'pm4py:param:resource_key': 'res', 'pm4py:param:timestamp_key': 'ts'}
    if with_start:
        params['pm4py:param:start_timestamp_key'] = names['start']
    if rng.random() < 0.9:
        params['grouper_freq'] = rng.choice(FREQS[:56]) if rng.random() < 0.9 else rng.choice(FREQS)
    if rng.random() < 0.1:
        params.update({'arrival_rate': 'ar', 'finish_rate': 'fr', 'service_time': 'sv', 'waiting_time': 'wt',
                       'sojourn_time': 'sj', 'diff_start_end': 'dse'})
    scale = rng.choice((300, 60, 1440, 20000))
    rows = []
    shape = rng.random()
    case_count = 1 if shape < 0.1 else rng.randint(1, 8)
    for case in range(case_count):
        identifier = case * rng.choice((1, 3, 7)) if int_ids else f'c{case * rng.choice((1, 3, 7))}'
        minute = rng.randint(0, 80)
        for _ in range(1 if shape < 0.04 else rng.randint(1, 5)):
            minute += rng.choice((0, 1, 5, 20, 60))
            timestamp = origin + datetime.timedelta(minutes=scale * minute // 60,
                                                    microseconds=rng.randint(0, 10 ** 6) if fine else 0)
            start = timestamp - datetime.timedelta(minutes=rng.choice((0, 0, 30, 600)),
                                                   microseconds=rng.randint(0, 999999) if fine else 0)
            activity = rng.choice('abc')
            rows.append({'case': identifier,
                         'activity': None if rng.random() < 0.1 else ('abc'.index(activity) if int_activities else activity),
                         'resource': None if rng.random() < 0.1 else rng.choice(('r1', 'r2', 'r3')),
                         'timestamp': timestamp, 'start': start})
    rng.shuffle(rows)
    if shape > 0.98:
        rows = []
    if shape < 0.04:
        rows = rows[:1]
    keys = ['case', 'activity', 'resource', 'timestamp'] + (['start'] if with_start else [])
    if rng.random() < 0.05:
        keys.remove(rng.choice(keys))
    rng.shuffle(keys)
    micros = lambda moment: (moment - EPOCH) // datetime.timedelta(microseconds=1)
    columns = []
    for key in keys:
        kind = 'time' if key in ('timestamp', 'start') else \
            'int' if (key == 'case' and int_ids) or (key == 'activity' and int_activities) else 'text'
        cells = [micros(row[key]) if kind == 'time' else row[key] for row in rows]
        columns.append({'name': names[key], 'kind': kind, 'cells': cells})
    return {'columns': columns, 'utc': utc, 'params': params}


def generate():
    rng = random.Random(BANK_SEED)
    cases = [('freq', {'freq': f}) for f in FREQS + [None]]
    cases += [('freq', {'freq': generate_freq(rng)}) for _ in range(1500)]
    cases += [('apply', generate_apply(rng)) for _ in range(3000)]
    return cases


def frame_of(source):
    import polars as pl
    zone = 'UTC' if source['utc'] else None
    series = []
    for column in source['columns']:
        if column['kind'] == 'time':
            values = [EPOCH + datetime.timedelta(microseconds=v) for v in column['cells']]
            series.append(pl.Series(column['name'], values, dtype=pl.Datetime('us')).dt.replace_time_zone(zone))
        else:
            series.append(pl.Series(column['name'], column['cells'],
                                    dtype=pl.Int64 if column['kind'] == 'int' else pl.Utf8))
    return pl.DataFrame(series).lazy()


def micros_of(value):
    return (value.replace(tzinfo=None) - EPOCH) // datetime.timedelta(microseconds=1)


def run(kind, source):
    from pm4py.algo.transformation.log_to_features.variants import temporal_lazy
    if kind == 'freq':
        try:
            return {'result': temporal_lazy._freq_to_polars_duration(source['freq'])}
        except Exception as error:
            return {'raises': type(error).__name__, 'message': str(error)}
    try:
        table = temporal_lazy.apply(frame_of(source), parameters=dict(source['params'])).collect()
        return {'rows': [[micros_of(row[0])] + list(row[1:]) for row in table.iter_rows()]}
    except Exception as error:
        return {'raises': type(error).__name__}


def child():
    import warnings
    warnings.filterwarnings('ignore')
    out = []
    for kind, source in generate():
        out.append([run(kind, source) for _ in range(REPEATS if kind == 'apply' else 1)])
    print(json.dumps(out))


def capture(seed):
    env = dict(os.environ, PYTHONHASHSEED=str(seed))
    result = subprocess.run([sys.executable, '-I', '-B', os.path.abspath(__file__), '--child'],
                            capture_output=True, text=True, env=env)
    if result.returncode:
        raise SystemExit(result.stderr[-2000:])
    return json.loads(result.stdout.strip().splitlines()[-1])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/temporal-lazy-bank.txt.gz')
    parser.add_argument('--child', action='store_true')
    args = parser.parse_args()
    if args.child:
        child()
        return
    cases = generate()
    with concurrent.futures.ThreadPoolExecutor(len(HASH_SEEDS)) as pool:
        runs = list(pool.map(capture, HASH_SEEDS))
    lines, summary = [], {}
    for index, (kind, source) in enumerate(cases):
        seen = []
        for run_outcomes in runs:
            for outcome in run_outcomes[index]:
                if outcome not in seen:
                    seen.append(outcome)
        if kind == 'freq':
            assert len(seen) == 1
            outcome = seen[0]
            key = 'freq raising' if 'raises' in outcome else 'freq'
        else:
            outcome = {'observed': seen}
            key = 'apply raising' if 'raises' in seen[0] else 'apply varying' if len(seen) > 1 else 'apply'
        summary[key] = summary.get(key, 0) + 1
        lines += [f'C temporal-lazy-{index:04d}', f'K {kind}', 'S ' + json.dumps(source, ensure_ascii=False),
                  'F ' + json.dumps(outcome, ensure_ascii=False, separators=(',', ':'))]
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0, filename='') as stream:
        stream.write(('\n'.join(lines) + '\n').encode())
    print(json.dumps(dict(sorted(summary.items()))))


if __name__ == '__main__':
    main()
