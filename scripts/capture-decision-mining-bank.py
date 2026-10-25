"""Seeded public PM4Py 2.7.22 decision-mining capture; no native calls.

Seeded event logs (traces over a small alphabet with choices, loops and
skips; numeric, categorical and boolean event attributes on some events, some
missing; a numeric and a categorical trace attribute) are mined with the
public inductive miner, and every decision point of
`algo.decision_mining.algorithm.get_decision_points` goes through the public
`algo.decision_mining.algorithm.apply` (the encoded decision table),
`get_decision_tree`, and once per log `create_data_petri_nets_with_decisions`.
Each case records the net as PNML text (from the first run; identifiers are
random), and each call the table columns,
dtypes, cells, targets and classes, or the error class; data Petri nets are
recorded through the transition guards. Every case runs in fresh interpreters
under four PYTHONHASHSEED values and keeps every distinct outcome.

    python -I -B scripts/capture-decision-mining-bank.py \\
        --output docs/decision-mining-bank.json.gz
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

BANK_SEED = 20261030
HASH_SEEDS = (0, 1, 2, 3)


def generate():
    rng = random.Random(BANK_SEED)
    cases = []
    for index in range(150):
        alphabet = 'abcdef'[:rng.randint(3, 6)]
        variants = []
        for _ in range(rng.randint(2, 5)):
            variants.append(''.join(rng.choice(alphabet) for _ in range(rng.randint(1, 6))))
        traces = []
        for position in range(rng.randint(3, 14)):
            events = []
            for activity in rng.choice(variants):
                event = {'activity': activity}
                if rng.random() < 0.5:
                    event['amount'] = rng.choice((1, 2.5, 10, 40, None))
                if rng.random() < 0.4:
                    event['kind'] = rng.choice(('x', 'y', 'z'))
                if rng.random() < 0.2:
                    event['flag'] = rng.random() < 0.5
                events.append(event)
            traces.append({'events': events, 'region': rng.choice(('n', 's')),
                           'size': rng.choice((1, 5, 9))})
        cases.append({'name': f'decision-{index:03d}', 'traces': traces})
    return cases


def cell(value):
    if value is None or (isinstance(value, float) and math.isnan(value)):
        return None
    if isinstance(value, bool) or type(value).__name__ == 'bool_':
        return bool(value)
    if hasattr(value, 'item'):
        value = value.item()
    return value


def run(cases):
    import io
    import tempfile
    import warnings
    warnings.filterwarnings('ignore')
    os.environ['TQDM_DISABLE'] = '1'
    import datetime
    import pm4py
    from pm4py.objects.log.obj import Event, EventLog, Trace
    from pm4py.algo.decision_mining import algorithm
    origin = datetime.datetime(2024, 1, 1, tzinfo=datetime.timezone.utc)
    out = []
    for case in cases:
        log = EventLog()
        minute = 0
        for position, spec in enumerate(case['traces']):
            trace = Trace()
            trace.attributes['concept:name'] = f'c{position}'
            trace.attributes['region'] = spec['region']
            trace.attributes['size'] = spec['size']
            for event_spec in spec['events']:
                payload = {'concept:name': event_spec['activity'],
                           'time:timestamp': origin + datetime.timedelta(minutes=minute)}
                for key in ('amount', 'kind', 'flag'):
                    if key in event_spec and event_spec[key] is not None:
                        payload[key] = event_spec[key]
                trace.append(Event(payload))
                minute += 1
            log.append(trace)
        net, im, fm = pm4py.discover_petri_net_inductive(log)
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, 'net.pnml')
            pm4py.write_pnml(net, im, fm, path)
            with open(path, encoding='utf-8') as stream:
                pnml = stream.read()
        record = {'pnml': pnml, 'points': {}}
        try:
            points = algorithm.get_decision_points(net, labels=True)
        except Exception as error:
            record['points_error'] = type(error).__name__
            points = {}
        for place in sorted(points):
            entry = {'labels': sorted(str(label) for label in points[place])}
            try:
                X, y, classes = algorithm.apply(log, net, im, fm, decision_point=place)
                entry['table'] = {
                    'columns': [str(column) for column in X.columns],
                    'dtypes': [str(dtype) for dtype in X.dtypes],
                    'rows': [[cell(value) for value in row] for row in X.itertuples(index=False)],
                    'targets': [cell(value) for value in y],
                    'classes': [str(label) for label in classes]}
            except Exception as error:
                entry['table'] = {'error': type(error).__name__}
            try:
                algorithm.get_decision_tree(log, net, im, fm, decision_point=place)
                entry['tree'] = 'ok'
            except Exception as error:
                entry['tree'] = {'error': type(error).__name__}
            record['points'][place] = entry
        try:
            data_net, _, _ = algorithm.create_data_petri_nets_with_decisions(log, net, im, fm)
            record['guards'] = sorted([str(t.label), json.dumps(dict(t.properties), default=str, sort_keys=True)]
                                      for t in data_net.transitions)
        except Exception as error:
            record['guards'] = {'error': type(error).__name__}
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
    variation = 0
    for index, case in enumerate(cases):
        # Transition and net identifiers are random, so the PNML text comes
        # from the first run and the outcomes are compared without it.
        case['pnml'] = runs[0][index]['pnml']
        seen = []
        for run_result in runs:
            outcome = {key: value for key, value in run_result[index].items() if key != 'pnml'}
            if outcome not in seen:
                seen.append(outcome)
        if len(seen) > 1:
            variation += 1
        case['observed'] = seen
    bank = {'reference': f'pm4py {pm4py.__version__}', 'bank_seed': BANK_SEED, 'hash_seeds': list(HASH_SEEDS),
            'cases': cases}
    data = json.dumps(bank, indent=0, sort_keys=True).encode('utf-8')
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0,
                                                      compresslevel=9, filename='') as stream:
        stream.write(data)
    print(f'{len(cases)} cases, {variation} with hash-seed variation')


if __name__ == '__main__':
    main()
