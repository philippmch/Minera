"""Seeded public PM4Py 2.7.22 log-to-features capture; no native calls.

Seeded event logs (activities with repeats, UTC timestamps with ties, a
resource, a numeric and a categorical event attribute on some events, a
numeric and a categorical case attribute) go through the public
`algo.transformation.log_to_features.algorithm.apply` with the EVENT_BASED
and TRACE_BASED variants under default parameters, all extra trace features,
explicit attribute lists, and an explicit succession attribute; the same
events, as a dataframe with stretched timestamps, go through TEMPORAL with
weekly and daily buckets. Each call
records the feature names and the rows (per event for EVENT_BASED), or the
error class. Every case runs in fresh interpreters under four PYTHONHASHSEED
values and keeps every distinct outcome.

    python -I -B scripts/capture-log-to-features-bank.py \\
        --output docs/log-to-features-bank.json.gz
"""
import argparse
import concurrent.futures
import gzip
import json
import math
import os
import random
import subprocess
import sys

BANK_SEED = 20261031
HASH_SEEDS = (0, 1, 2, 3)
CONFIGS = {
    'event-default': ('EVENT_BASED', {}),
    'event-explicit': ('EVENT_BASED', {'str_ev_attr': ['concept:name', 'kind'], 'num_ev_attr': ['amount']}),
    'trace-default': ('TRACE_BASED', {}),
    'trace-extras': ('TRACE_BASED', {'enable_all_extra_features': True}),
    'trace-explicit': ('TRACE_BASED', {'str_ev_attr': ['concept:name', 'kind'], 'num_ev_attr': ['amount'],
                                       'str_tr_attr': ['region'], 'num_tr_attr': ['size']}),
    'trace-succession': ('TRACE_BASED', {'str_ev_attr': ['concept:name'], 'str_evsucc_attr': ['org:resource'],
                                         'num_ev_attr': [], 'str_tr_attr': [], 'num_tr_attr': []}),
}
# TEMPORAL takes the log as a dataframe, with every minute stretched to
# TEMPORAL_SCALE minutes so that cases span several buckets.
TEMPORAL_SCALE = 300
TEMPORAL_CONFIGS = {
    'temporal-weekly': {},
    'temporal-daily': {'grouper_freq': 'D'},
}


def generate():
    rng = random.Random(BANK_SEED)
    cases = []
    for index in range(120):
        alphabet = 'abcde'[:rng.randint(2, 5)]
        traces = []
        minute = 0
        for _ in range(rng.randint(1, 7)):
            start = minute + rng.choice((0, 0, 1, 3, 10))
            minute = start
            events = []
            for _ in range(rng.randint(1, 6)):
                event = {'activity': rng.choice(alphabet), 'minute': minute,
                         'resource': rng.choice(('r1', 'r2', 'r3'))}
                if rng.random() < 0.6:
                    event['amount'] = rng.choice((1, 2.5, 10, 40))
                if rng.random() < 0.5:
                    event['kind'] = rng.choice(('x', 'y', 'z'))
                events.append(event)
                minute += rng.choice((0, 1, 2, 5))
            traces.append({'events': events, 'region': rng.choice(('n', 's', 'e')),
                           'size': rng.choice((1, 5, 9))})
            minute = start + rng.choice((0, 1, 4))
        cases.append({'name': f'features-{index:03d}', 'traces': traces})
    return cases


def number(value):
    value = float(value)
    return None if math.isnan(value) else value


def run(cases):
    import datetime
    import warnings
    warnings.filterwarnings('ignore')
    os.environ['TQDM_DISABLE'] = '1'
    import pandas as pd
    from pm4py.objects.log.obj import Event, EventLog, Trace
    from pm4py.algo.transformation.log_to_features import algorithm
    origin = datetime.datetime(2024, 1, 1, tzinfo=datetime.timezone.utc)
    out = []
    for case in cases:
        log = EventLog()
        for position, spec in enumerate(case['traces']):
            trace = Trace()
            trace.attributes['concept:name'] = f'c{position}'
            trace.attributes['region'] = spec['region']
            trace.attributes['size'] = spec['size']
            for event_spec in spec['events']:
                payload = {'concept:name': event_spec['activity'], 'org:resource': event_spec['resource'],
                           'time:timestamp': origin + datetime.timedelta(minutes=event_spec['minute'])}
                for key in ('amount', 'kind'):
                    if key in event_spec:
                        payload[key] = event_spec[key]
                trace.append(Event(payload))
            log.append(trace)
        record = {}
        for config, (variant, parameters) in CONFIGS.items():
            try:
                data, names = algorithm.apply(log, variant=getattr(algorithm.Variants, variant),
                                              parameters=dict(parameters))
                if variant == 'EVENT_BASED':
                    rows = [[[number(v) for v in event] for event in trace] for trace in data]
                else:
                    rows = [[number(v) for v in row] for row in data]
                record[config] = {'names': [str(n) for n in names], 'rows': rows}
            except Exception as error:
                record[config] = {'error': type(error).__name__}
        rows = [{'case:concept:name': f'c{position}', 'concept:name': e['activity'],
                 'org:resource': e['resource'],
                 'time:timestamp': origin + datetime.timedelta(minutes=TEMPORAL_SCALE * e['minute'])}
                for position, spec in enumerate(case['traces']) for e in spec['events']]
        frame = pd.DataFrame(rows)
        for config, parameters in TEMPORAL_CONFIGS.items():
            try:
                table = algorithm.apply(frame.copy(), variant=algorithm.Variants.TEMPORAL,
                                        parameters=dict(parameters))
                record[config] = {'names': [str(c) for c in table.columns],
                                  'dtypes': [str(d) for d in table.dtypes],
                                  'rows': [[v.isoformat() if hasattr(v, 'isoformat') else number(v)
                                            for v in row] for row in table.itertuples(index=False)]}
            except Exception as error:
                record[config] = {'error': type(error).__name__}
        out.append(record)
    return out


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
            summary[kind] = summary.get(kind, 0) + 1
    bank = {'reference': f'pm4py {pm4py.__version__}', 'bank_seed': BANK_SEED, 'hash_seeds': list(HASH_SEEDS),
            'configs': dict({k: {'variant': v, 'parameters': p} for k, (v, p) in CONFIGS.items()},
                            **{k: {'variant': 'TEMPORAL', 'parameters': p, 'minute_scale': TEMPORAL_SCALE}
                               for k, p in TEMPORAL_CONFIGS.items()}),
            'cases': cases}
    data = json.dumps(bank, indent=0, sort_keys=True).encode('utf-8')
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0,
                                                      compresslevel=9, filename='') as stream:
        stream.write(data)
    print(f'{len(cases)} cases, calls by outcome {summary}')


if __name__ == '__main__':
    main()
