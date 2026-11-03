"""Capture seeded PERFORMANCE DFG playouts of the reference.

    TZ=UTC python scripts/capture-dfg-performance-playout-bank.py \\
        --output src/backend/tests/fixtures/dfg_performance_playout_bank.txt

Random frequency DFGs in the order a DataFrame discovery gives them (edges as
sorted pairs, start activities in first-occurrence order) with random mean
durations (some zero) go through algo.simulation.playout.dfg PERFORMANCE
after numpy.random.seed(s). The capture runs itself under PYTHONHASHSEED 0, 1
and 2 and requires identical output.

Fixture lines:
    C <name> <seed> <num_traces> <case_arrival_rate>
    E <a> <b> <count> <mean>     edges, in dict order
    S <a> <count>                start activities, in dict order
    N <a> <count>                end activities
    R <a>@<epoch microseconds> ...   one line per trace
    X <type>                     the reference raised
"""
import argparse
import calendar
import os
from pathlib import Path
import random
import subprocess
import sys
import warnings

warnings.filterwarnings('ignore')

BANK_SEED = 20261103
CASES = 300
HASH_SEEDS = [0, 1, 2]


def cases():
    rng = random.Random(BANK_SEED)
    for index in range(CASES):
        acts = [chr(97 + i) for i in range(rng.randint(1, 6))]
        dfg = {}
        for x in acts:
            for y in acts:
                if rng.random() < 0.45:
                    dfg[(x, y)] = rng.randint(1, 9)
        dfg = dict(sorted(dfg.items()))
        starts = [a for a in acts if rng.random() < 0.5] or [acts[0]]
        rng.shuffle(starts)
        sa = {a: rng.randint(1, 5) for a in starts}
        # Every activity can end, so cases terminate.
        ea = {a: rng.randint(1, 9) for a in acts}
        perf = {k: (0.0 if rng.random() < 0.15 else round(rng.uniform(0.001, 50000), 6)) for k in dfg}
        yield (f'dfgp-{index:03d}', rng.randint(0, 2 ** 32 - 1), rng.choice([1, 3, 10]),
               rng.choice([1, 2.5, 3600, 0.001]), dfg, sa, ea, perf)


def child():
    import numpy
    from pm4py.algo.simulation.playout.dfg import algorithm as playout
    out = []
    for name, seed, traces, rate, dfg, sa, ea, perf in cases():
        out.append(f'C {name} {seed} {traces} {rate!r}')
        out.extend(f'E {a} {b} {count} {perf[(a, b)]!r}' for (a, b), count in dfg.items())
        out.extend(f'S {a} {count}' for a, count in sa.items())
        out.extend(f'N {a} {count}' for a, count in ea.items())
        numpy.random.seed(seed)
        try:
            log = playout.apply(dict(dfg), dict(sa), dict(ea), variant=playout.Variants.PERFORMANCE,
                                parameters={'num_traces': traces, 'performance_dfg': dict(perf),
                                            'case_arrival_rate': rate})
        except Exception as error:
            out.append('X ' + type(error).__name__)
            continue
        for index, trace in enumerate(log):
            assert trace.attributes['concept:name'] == str(index)
            row = ['R']
            for event in trace:
                stamp = event['time:timestamp']
                seconds = calendar.timegm(stamp.utctimetuple())
                row.append(f"{event['concept:name']}@{seconds * 1000000 + stamp.microsecond}")
            out.append(' '.join(row))
    sys.stdout.write('\n'.join(out) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--child', action='store_true')
    args = parser.parse_args()
    if args.child:
        child()
        return
    import pm4py
    assert pm4py.__version__ == '2.7.22'
    runs = [subprocess.run([sys.executable, __file__, '--child'], check=True, capture_output=True,
                           env=dict(os.environ, PYTHONHASHSEED=str(seed), TZ='UTC')).stdout
            for seed in HASH_SEEDS]
    assert all(run == runs[0] for run in runs), 'DFG playout depends on the hash seed'
    header = (f'# algo.simulation.playout.dfg PERFORMANCE, pm4py 2.7.22, bank seed {BANK_SEED}; '
              f'identical under PYTHONHASHSEED {HASH_SEEDS}.\n')
    args.output.write_bytes(header.encode() + runs[0])
    print(CASES, 'cases')


if __name__ == '__main__':
    main()
