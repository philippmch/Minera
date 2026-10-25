"""Seeded public PM4Py 2.7.22 LLM log-abstraction capture; no native calls.

Seeded event logs (activities with repeats, UTC timestamps with ties and
seconds-to-hours gaps, a resource, an integer level on every event, numeric,
categorical, boolean and mixed string/integer event attributes on some
events, categorical, integer and boolean case attributes) go through the public
`algo.querying.llm.abstractions` modules for columns, DFG, variants,
features, event streams and log skeletons (the skeleton from the public
`discover_log_skeleton`), under default parameters and under length, header,
frequency and performance options. Each call records the text or the error
class. Every call gets a freshly built log, because the public columns, DFG
and variants abstractions add case attributes to the events of the log they
are given. Every case runs in fresh interpreters under four PYTHONHASHSEED values
and keeps every distinct outcome.

    python -I -B scripts/capture-llm-abstractions-bank.py \\
        --output docs/llm-abstractions-bank.json.gz
"""
import argparse
import concurrent.futures
import gzip
import json
import os
import random
import subprocess
import sys

BANK_SEED = 20261033
HASH_SEEDS = (0, 1, 2, 3)
CALLS = {
    'cols-default': ('log_to_cols_descr', {}),
    'cols-short': ('log_to_cols_descr', {'max_len': 420}),
    'dfg-default': ('log_to_dfg_descr', {}),
    'dfg-frequency': ('log_to_dfg_descr', {'include_performance': False}),
    'dfg-relative': ('log_to_dfg_descr', {'relative_frequency': True}),
    'dfg-aggregations': ('log_to_dfg_descr', {'primary_performance_aggregation': 'median',
                                              'secondary_performance_aggregation': 'max'}),
    'dfg-short': ('log_to_dfg_descr', {'max_len': 150, 'response_header': False}),
    'variants-default': ('log_to_variants_descr', {}),
    'variants-frequency': ('log_to_variants_descr', {'include_performance': False}),
    'variants-relative': ('log_to_variants_descr', {'relative_frequency': True}),
    'variants-aggregations': ('log_to_variants_descr', {'primary_performance_aggregation': 'median',
                                                        'secondary_performance_aggregation': 'stdev'}),
    'variants-short': ('log_to_variants_descr', {'max_len': 150, 'response_header': False}),
    'features-default': ('log_to_fea_descr', {}),
    'features-short': ('log_to_fea_descr', {'max_len': 600, 'include_header': False}),
    'stream-default': ('stream_to_descr', {}),
    'stream-short': ('stream_to_descr', {'max_len': 400, 'response_header': False}),
    'skeleton-default': ('logske_to_descr', {}),
    'skeleton-noheader': ('logske_to_descr', {'include_header': False}),
}


def generate():
    rng = random.Random(BANK_SEED)
    cases = []
    for index in range(150):
        alphabet = 'abcdef'[:rng.randint(2, 6)]
        variants = [''.join(rng.choice(alphabet) for _ in range(rng.randint(1, 6)))
                    for _ in range(rng.randint(1, 5))]
        traces = []
        second = 0
        for _ in range(rng.randint(1, 8)):
            events = []
            for activity in rng.choice(variants):
                event = {'activity': activity, 'second': second, 'resource': rng.choice(('r1', 'r2', 'r3')),
                         'level': rng.randint(0, 3)}
                if rng.random() < 0.6:
                    event['amount'] = rng.choice((1, 2.5, 10, 40))
                if rng.random() < 0.4:
                    event['kind'] = rng.choice(('x', 'y'))
                if rng.random() < 0.3:
                    event['flag'] = rng.random() < 0.5
                if rng.random() < 0.2:
                    event['code'] = rng.choice(('k1', 'k2', 7))
                events.append(event)
                second += rng.choice((0, 1, 30, 600, 3600, 7261))
            traces.append({'events': events, 'region': rng.choice(('n', 's')), 'priority': rng.randint(0, 2),
                           'urgent': rng.random() < 0.5})
        cases.append({'name': f'abstraction-{index:03d}', 'traces': traces})
    return cases


def run(cases):
    import datetime
    import importlib
    import warnings
    warnings.filterwarnings('ignore')
    import pm4py
    from pm4py.objects.log.obj import Event, EventLog, Trace
    origin = datetime.datetime(2024, 1, 1, tzinfo=datetime.timezone.utc)

    def build(case):
        log = EventLog()
        for position, spec in enumerate(case['traces']):
            trace = Trace()
            trace.attributes['concept:name'] = f'c{position}'
            for key in ('region', 'priority', 'urgent'):
                trace.attributes[key] = spec[key]
            for event_spec in spec['events']:
                payload = {'concept:name': event_spec['activity'], 'org:resource': event_spec['resource'],
                           'time:timestamp': origin + datetime.timedelta(seconds=event_spec['second'])}
                for key in ('level', 'amount', 'kind', 'flag', 'code'):
                    if key in event_spec:
                        payload[key] = event_spec[key]
                trace.append(Event(payload))
            log.append(trace)
        return log

    out = []
    for case in cases:
        record = {}
        for call, (module, parameters) in CALLS.items():
            abstraction = importlib.import_module('pm4py.algo.querying.llm.abstractions.' + module)
            log = build(case)
            try:
                source = pm4py.discover_log_skeleton(log) if module == 'logske_to_descr' else log
                record[call] = {'text': abstraction.apply(source, parameters=dict(parameters))}
            except Exception as error:
                record[call] = {'error': type(error).__name__}
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
            summary[(call.split('-')[0], kind)] = summary.get((call.split('-')[0], kind), 0) + 1
    bank = {'reference': f'pm4py {pm4py.__version__}', 'bank_seed': BANK_SEED, 'hash_seeds': list(HASH_SEEDS),
            'calls': {k: {'module': m, 'parameters': p} for k, (m, p) in CALLS.items()}, 'cases': cases}
    data = json.dumps(bank, indent=0, sort_keys=True).encode('utf-8')
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0,
                                                      compresslevel=9, filename='') as stream:
        stream.write(data)
    print(f'{len(cases)} cases, calls by outcome {sorted(summary.items())}')


if __name__ == '__main__':
    main()
