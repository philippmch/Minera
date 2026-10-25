"""Seeded public PM4Py 2.7.22 profile-clustering capture; no native calls.

Seeded event logs (one to eighteen traces over a few variants, a
categorical trace attribute on some traces, a numeric and a categorical event
attribute on some events) go through the public
`algo.clustering.profiles.algorithm.apply` (SKLEARN_PROFILES, default
clusterer). Each call records the returned sublogs as lists of trace
positions, or the error class. Every case runs in fresh interpreters under
four PYTHONHASHSEED values and keeps every distinct outcome.

    python -I -B scripts/capture-profile-clustering-bank.py \\
        --output docs/profile-clustering-bank.json.gz
"""
import argparse
import concurrent.futures
import gzip
import json
import os
import random
import subprocess
import sys

BANK_SEED = 20261032
HASH_SEEDS = (0, 1, 2, 3)


def generate():
    rng = random.Random(BANK_SEED)
    cases = []
    for index in range(300):
        alphabet = 'abcdef'[:rng.randint(2, 6)]
        variants = [''.join(rng.choice(alphabet) for _ in range(rng.randint(1, 6)))
                    for _ in range(rng.randint(1, 6))]
        traces = []
        for _ in range(rng.randint(1, 18)):
            events = []
            for activity in rng.choice(variants):
                event = {'activity': activity}
                if rng.random() < 0.4:
                    event['amount'] = rng.choice((1, 2.5, 7, 30))
                if rng.random() < 0.3:
                    event['kind'] = rng.choice(('x', 'y'))
                events.append(event)
            traces.append({'events': events,
                           'region': rng.choice(('n', 's', 'e')) if rng.random() < 0.5 else None})
        cases.append({'name': f'profiles-{index:03d}', 'traces': traces})
    return cases


def run(cases):
    import datetime
    import warnings
    warnings.filterwarnings('ignore')
    from pm4py.objects.log.obj import Event, EventLog, Trace
    from pm4py.algo.clustering.profiles import algorithm
    origin = datetime.datetime(2024, 1, 1, tzinfo=datetime.timezone.utc)
    out = []
    for case in cases:
        log = EventLog()
        minute = 0
        for position, spec in enumerate(case['traces']):
            trace = Trace()
            trace.attributes['concept:name'] = str(position)
            if spec['region'] is not None:
                trace.attributes['region'] = spec['region']
            for event_spec in spec['events']:
                payload = {'concept:name': event_spec['activity'],
                           'time:timestamp': origin + datetime.timedelta(minutes=minute)}
                for key in ('amount', 'kind'):
                    if key in event_spec:
                        payload[key] = event_spec[key]
                trace.append(Event(payload))
                minute += 1
            log.append(trace)
        try:
            clusters = [[int(trace.attributes['concept:name']) for trace in sublog]
                        for sublog in algorithm.apply(log)]
            out.append({'clusters': clusters})
        except Exception as error:
            out.append({'error': type(error).__name__})
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
    import sklearn
    cases = generate()
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        runs = list(pool.map(capture, HASH_SEEDS))
    summary = {}
    for index, case in enumerate(cases):
        seen = []
        for run_result in runs:
            if run_result[index] not in seen:
                seen.append(run_result[index])
        case['outcome'] = seen[0] if len(seen) == 1 else {'observed': seen}
        kind = 'observed' if len(seen) > 1 else 'error' if 'error' in seen[0] else 'value'
        summary[kind] = summary.get(kind, 0) + 1
    bank = {'reference': f'pm4py {pm4py.__version__}, scikit-learn {sklearn.__version__}',
            'bank_seed': BANK_SEED, 'hash_seeds': list(HASH_SEEDS), 'cases': cases}
    data = json.dumps(bank, indent=0, sort_keys=True).encode('utf-8')
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0,
                                                      compresslevel=9, filename='') as stream:
        stream.write(data)
    print(f'{len(cases)} cases, outcomes {summary}')


if __name__ == '__main__':
    main()
