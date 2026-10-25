"""Public PM4Py 2.7.22 Heuristics Miner++ capture; no native calls.

Seeded interval logs are simulated from random process trees (sequence,
exclusive choice, interleaved parallelism with overlapping intervals, loops)
with integer-second start/complete timestamps, occasional noise (swapped or
dropped events) and logs without the `start_timestamp` attribute. Each log is
mined by the public `algo.discovery.heuristics.variants.plusplus.apply_heu`
under six parameter sets on the EventLog, by the public dispatcher
`algo.discovery.heuristics.algorithm.apply_heu` (PLUSPLUS) on the equivalent
DataFrame (its pandas route), and by the public dispatcher `apply` (PLUSPLUS,
Petri result) on both forms under two of them. The returned HeuristicsNet is recorded through its
public fields and per-node connections; Petri nets by place/transition names,
labels, arcs and markings. Every case runs in fresh interpreters under four
PYTHONHASHSEED values and keeps every distinct outcome. Twenty DataFrame
controls (pair-free, missing case id and missing activity rows under aware and
naive dtypes) go through the public `plusplus.apply_heu_pandas`.

    python -I -B scripts/capture-heuristics-plusplus-bank.py \
        --output docs/heuristics-plusplus-bank.json.gz
"""
import argparse
import concurrent.futures
import gzip
import json
import math
import os
from pathlib import Path
import random
import subprocess
import sys

BANK_SEED = 20261025
HASH_SEEDS = (0, 1, 2, 3)
CONFIGS = {
    'default': {},
    'dependency_high': {'DEPENDENCY_THRESH': 0.9},
    'dependency_zero_and_low': {'DEPENDENCY_THRESH': 0.0, 'AND_MEASURE_THRESH': 0.3},
    'and_high': {'AND_MEASURE_THRESH': 0.9},
    'counts': {'MIN_ACT_COUNT': 2, 'MIN_DFG_OCCURRENCES': 2},
    'performance': {'HEU_NET_DECORATION': 'performance'},
}
PETRI_CONFIGS = ('default', 'dependency_zero_and_low')


def tree(rng, labels, depth):
    if len(labels) == 1:
        return ('act', labels[0])
    if depth == 0 or rng.random() < 0.2:
        return (rng.choice(('seq', 'xor', 'and')), *[('act', label) for label in labels])
    kind = rng.choice(('seq', 'seq', 'xor', 'and', 'and', 'loop'))
    if kind == 'loop':
        cut = rng.randint(1, len(labels) - 1)
        return ('loop', tree(rng, labels[:cut], depth - 1), tree(rng, labels[cut:], depth - 1))
    parts = rng.randint(2, min(3, len(labels)))
    cuts = sorted(rng.sample(range(1, len(labels)), parts - 1))
    groups = [labels[a:b] for a, b in zip([0] + cuts, cuts + [len(labels)])]
    return (kind, *[tree(rng, group, depth - 1) for group in groups])


def simulate(rng, node, time, out):
    """Appends (label, start, end) intervals; returns the finishing time."""
    kind = node[0]
    if kind == 'act':
        start = time + rng.randint(0, 2)
        end = start + rng.randint(0, 4)
        out.append((node[1], start, end))
        return end
    if kind == 'seq':
        for child in node[1:]:
            time = simulate(rng, child, time, out)
        return time
    if kind == 'xor':
        return simulate(rng, rng.choice(node[1:]), time, out)
    if kind == 'and':
        return max(simulate(rng, child, time, out) for child in node[1:])
    time = simulate(rng, node[1], time, out)
    for _ in range(3):
        if rng.random() < 0.6:
            break
        time = simulate(rng, node[2], time, out)
        time = simulate(rng, node[1], time, out)
    return time


def generate():
    rng = random.Random(BANK_SEED)
    cases = []
    for index in range(160):
        labels = [chr(ord('a') + n) for n in range(rng.randint(3, 8))]
        model = tree(rng, labels, 3)
        with_start = rng.random() < 0.85
        traces = []
        for _ in range(rng.randint(4, 24)):
            intervals = []
            simulate(rng, model, 0, intervals)
            intervals.sort(key=lambda item: (item[1], item[2]))
            if rng.random() < 0.1 and len(intervals) > 1:
                position = rng.randrange(len(intervals) - 1)
                intervals[position], intervals[position + 1] = intervals[position + 1], intervals[position]
            if rng.random() < 0.08 and len(intervals) > 1:
                intervals.pop(rng.randrange(len(intervals)))
            traces.append([list(item) for item in intervals])
        cases.append({'name': f'plusplus-{index:03d}', 'with_start': with_start, 'traces': traces})
    # DataFrame controls for the pandas route's pair-free and missing-value
    # domains: rows are [case id or None, activity or None, start, complete],
    # under timezone-aware and naive dtypes.
    shapes = {
        'empty': [],
        'single-events': [['c0', 'a', 0, 1], ['c1', 'b', 0, 1]],
        'one-pair': [['c0', 'a', 0, 1], ['c0', 'b', 2, 3]],
        'missing-case-single': [[None, 'a', 0, 1], [None, 'b', 2, 3], ['c1', 'b', 0, 1]],
        'missing-case-pair': [[None, 'a', 0, 1], [None, 'b', 2, 3], ['c1', 'b', 0, 1], ['c1', 'b', 3, 4]],
        'missing-case-only': [[None, 'a', 0, 1], [None, 'b', 2, 3]],
        'missing-activity-pair': [['c0', None, 0, 1], ['c0', 'b', 2, 3]],
        'missing-activity-two-pairs': [['c0', None, 0, 1], ['c0', 'b', 2, 3], ['c1', 'a', 0, 1], ['c1', 'b', 2, 3]],
        'overlap-only': [['c0', 'a', 0, 5], ['c0', 'b', 2, 3]],
        'single-and-pair': [['c0', 'a', 0, 1], ['c1', 'a', 0, 1], ['c1', 'b', 2, 3]],
    }
    for shape, rows in shapes.items():
        for tz in (True, False):
            cases.append({'name': f'control-{shape}-{"aware" if tz else "naive"}', 'with_start': True,
                          'traces': [], 'frame_rows': rows, 'tz': tz})
    return cases


def number(value):
    value = float(value)
    return value if math.isfinite(value) else repr(value)


def nested(mapping):
    return sorted([a, b, number(v)] for a, inner in mapping.items() for b, v in inner.items())


def heuristics(heu):
    nodes = []
    for name, node in heu.nodes.items():
        def connections(side):
            return [[other.node_name, number(edge.dependency_value), number(edge.dfg_value), number(edge.repr_value)]
                    for other, edges in side.items() for edge in edges]
        nodes.append({'name': name, 'occ': node.node_occ, 'is_start': node.is_start_activity,
                      'is_end': node.is_end_activity,
                      'outputs': sorted(connections(node.output_connections)),
                      'inputs': sorted(connections(node.input_connections)),
                      'and_in': nested(node.and_measures_in), 'and_out': nested(node.and_measures_out),
                      'loop_two': sorted([k, number(v)] for k, v in node.loop_length_two.items())})
    return {'activities': list(heu.activities),
            'start_activities': [sorted([k, v] for k, v in item.items()) for item in heu.start_activities],
            'end_activities': [sorted([k, v] for k, v in item.items()) for item in heu.end_activities],
            'activities_occurrences': sorted([k, v] for k, v in heu.activities_occurrences.items()),
            'dfg': sorted([a, b, v] for (a, b), v in heu.dfg.items()),
            'dependency_matrix': nested(heu.dependency_matrix),
            'dfg_matrix': nested(heu.dfg_matrix),
            'performance_dfg': None if heu.performance_dfg is None else
            sorted([a, b, number(v)] for (a, b), v in heu.performance_dfg.items()),
            'performance_matrix': nested(heu.performance_matrix),
            'sojourn_times': sorted([k, number(v)] for k, v in heu.sojourn_times.items()),
            'concurrent_activities': sorted([a, b, v] for (a, b), v in heu.concurrent_activities.items()),
            'node_type': heu.node_type,
            'dfg_window_2': heu.dfg_window_2 is not None, 'freq_triples': heu.freq_triples is not None,
            'nodes': sorted(nodes, key=lambda item: item['name'])}


def petri(result):
    net, im, fm = result
    return {'places': sorted(p.name for p in net.places),
            'transitions': sorted([t.name, t.label] for t in net.transitions),
            'arcs': sorted([a.source.name, a.target.name, a.weight] for a in net.arcs),
            'initial': sorted([p.name, c] for p, c in im.items()),
            'final': sorted([p.name, c] for p, c in fm.items())}


def run(cases):
    import warnings
    warnings.filterwarnings('ignore')
    import pandas as pd
    from pm4py.objects.log.obj import EventLog, Trace, Event
    from pm4py.algo.discovery.heuristics.variants import plusplus as pp
    from pm4py.algo.discovery.heuristics import algorithm as ha
    origin = pd.Timestamp('2024-01-01', tz='UTC')
    out = []
    for case in cases:
        def stamp(seconds):
            return origin + pd.Timedelta(seconds=seconds)

        def log():
            return EventLog([Trace([Event({'concept:name': a, 'time:timestamp': stamp(e),
                                           **({'start_timestamp': stamp(s)} if case['with_start'] else {})})
                                    for a, s, e in trace], attributes={'concept:name': f'c{n:02d}'})
                             for n, trace in enumerate(case['traces'])])

        def frame():
            rows = []
            for n, trace in enumerate(case['traces']):
                for a, s, e in trace:
                    row = {'case:concept:name': f'c{n:02d}', 'concept:name': a, 'time:timestamp': stamp(e)}
                    if case['with_start']:
                        row['start_timestamp'] = stamp(s)
                    rows.append(row)
            data = pd.DataFrame(rows)
            for column in ('time:timestamp', 'start_timestamp'):
                if column in data:
                    data[column] = data[column].astype('datetime64[us, UTC]')
            return data

        record = {}
        if 'frame_rows' in case:
            data = pd.DataFrame({'case:concept:name': pd.Series([r[0] for r in case['frame_rows']], dtype='str'),
                                 'concept:name': pd.Series([r[1] for r in case['frame_rows']], dtype='str'),
                                 'time:timestamp': [origin + pd.Timedelta(seconds=r[3]) for r in case['frame_rows']],
                                 'start_timestamp': [origin + pd.Timedelta(seconds=r[2]) for r in case['frame_rows']]})
            for column in ('time:timestamp', 'start_timestamp'):
                data[column] = pd.to_datetime(data[column], utc=True).astype('datetime64[us, UTC]')
                if not case['tz']:
                    data[column] = data[column].dt.tz_localize(None)
            try:
                record['pandas/default/frame'] = heuristics(pp.apply_heu_pandas(data))
            except Exception as error:
                record['pandas/default/frame'] = {'error': type(error).__name__, 'message': str(error)}
            out.append(record)
            continue
        for config, values in CONFIGS.items():
            parameters = {getattr(pp.Parameters, k): v for k, v in values.items()}
            for form, build in (('log', log), ('frame', frame)):
                try:
                    if form == 'log':
                        value = pp.apply_heu(build(), parameters=dict(parameters))
                    else:
                        value = ha.apply_heu(build(), variant=ha.Variants.PLUSPLUS, parameters=dict(parameters))
                    record[f'heu/{config}/{form}'] = heuristics(value)
                except Exception as error:
                    record[f'heu/{config}/{form}'] = {'error': type(error).__name__}
                if config in PETRI_CONFIGS:
                    try:
                        record[f'petri/{config}/{form}'] = petri(ha.apply(build(), variant=ha.Variants.PLUSPLUS,
                                                                           parameters=dict(parameters)))
                    except Exception as error:
                        record[f'petri/{config}/{form}'] = {'error': type(error).__name__}
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
    bank = {'reference': f'pm4py {pm4py.__version__}', 'bank_seed': BANK_SEED,
            'hash_seeds': list(HASH_SEEDS), 'configs': CONFIGS, 'cases': cases}
    data = json.dumps(bank, indent=0, sort_keys=True).encode('utf-8')
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0,
                                                      compresslevel=9, filename='') as stream:
        stream.write(data)
    print(f'{len(cases)} cases, calls by outcome {summary}')


if __name__ == '__main__':
    main()
