"""Seeded public PM4Py 2.7.22 trace-attribute-driven clustering capture; no
native calls.

Seeded event logs (two to fourteen traces over a small alphabet with repeated
activities, a string or integer trace attribute with one to six values, some
traces without the attribute) go through the public
`algo.clustering.trace_attribute_driven.algorithm.apply` under all five
variants. Each call records the returned cluster tree and leaf list, or the
error class, and the condensed group distances of the public evaluation
function behind the variant. Every case runs in fresh interpreters under four PYTHONHASHSEED
values and keeps every distinct outcome.

    python -I -B scripts/capture-trace-attribute-clustering-bank.py \\
        --output docs/trace-attribute-clustering-bank.json.gz
"""
import argparse
import concurrent.futures
import gzip
import json
import os
import random
import subprocess
import sys

BANK_SEED = 20261029
HASH_SEEDS = (0, 1, 2, 3)
DISTANCES = {'VARIANT_DMM_LEVEN': 'eval_DMM_leven', 'VARIANT_AVG_LEVEN': 'eval_avg_leven',
             'VARIANT_DMM_VEC': 'eval_DMM_variant', 'VARIANT_AVG_VEC': 'eval_avg_variant'}
VARIANTS = ('VARIANT_DMM_LEVEN', 'VARIANT_AVG_LEVEN', 'VARIANT_DMM_VEC', 'VARIANT_AVG_VEC', 'DFG')


def generate():
    rng = random.Random(BANK_SEED)
    cases = []
    for index in range(240):
        alphabet = 'abcdef'[:rng.randint(2, 6)]
        variants = [''.join(rng.choice(alphabet) for _ in range(rng.randint(1, 6)))
                    for _ in range(rng.randint(1, 6))]
        numeric = rng.random() < 0.25
        values = [n if numeric else f'G{n}' for n in range(rng.randint(1, 6))]
        traces = []
        for _ in range(rng.randint(2, 14)):
            value = None if rng.random() < 0.1 else rng.choice(values)
            traces.append({'value': value, 'activities': list(rng.choice(variants))})
        cases.append({'name': f'cluster-{index:03d}', 'traces': traces})
    return cases


def run(cases):
    import warnings
    warnings.filterwarnings('ignore')
    import datetime
    from pm4py.objects.log.obj import Event, EventLog, Trace
    from pm4py.algo.clustering.trace_attribute_driven import algorithm
    from pm4py.algo.clustering.trace_attribute_driven.util import evaluation
    origin = datetime.datetime(2024, 1, 1, tzinfo=datetime.timezone.utc)
    out = []
    for case in cases:
        log = EventLog()
        minute = 0
        for position, spec in enumerate(case['traces']):
            trace = Trace()
            trace.attributes['concept:name'] = f'c{position}'
            if spec['value'] is not None:
                trace.attributes['dept'] = spec['value']
            for activity in spec['activities']:
                trace.append(Event({'concept:name': activity,
                                    'time:timestamp': origin + datetime.timedelta(minutes=minute)}))
                minute += 1
            log.append(trace)
        record = {}
        for variant in VARIANTS:
            try:
                tree, leaves = algorithm.apply(log, 'dept', variant=getattr(algorithm.Variants, variant, None)
                                               or getattr(algorithm, variant))
                record[variant] = {'tree': tree, 'leaves': [str(leaf) for leaf in leaves]}
                if variant in DISTANCES:
                    # Condensed group distances from the public evaluation
                    # function over the per-value sublogs in first-seen order.
                    sublogs = {}
                    for trace in log:
                        if 'dept' in trace.attributes:
                            sublogs.setdefault(trace.attributes['dept'], EventLog()).append(trace)
                    record[variant]['distances'] = [
                        float(value) for value in getattr(evaluation, DISTANCES[variant])(
                            list(sublogs.values()), 1, 0.5)]
            except Exception as error:
                record[variant] = {'error': type(error).__name__}
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
            'cases': cases}
    data = json.dumps(bank, indent=0, sort_keys=True).encode('utf-8')
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0,
                                                      compresslevel=9, filename='') as stream:
        stream.write(data)
    print(f'{len(cases)} cases, calls by outcome {summary}')


if __name__ == '__main__':
    main()
