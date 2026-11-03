"""Capture CLASSIC DFG playouts of the reference.

    python scripts/capture-dfg-classic-playout-bank.py \\
        --output src/backend/tests/fixtures/dfg_classic_playout_bank.txt

Random frequency DFGs (edges as sorted pairs, as a DataFrame discovery gives
them) with random start and end activities and random CLASSIC parameters go
through algo.simulation.playout.dfg CLASSIC. Equal probabilities are common
(small integer counts, regular graphs), so the bank pins the tie order too.
The capture runs itself under PYTHONHASHSEED 0, 1 and 2 and requires
identical output.

Fixture lines:
    C <name> <max_variants> <min_weighted_probability> <max_occ_per_activity>
      <interrupt_when_complete> <add_if_new_elements> <min_variant_occ>
      <return_only_if_complete>
    E <a> <b> <count>            edges, in dict order
    S <a> <count>                start activities, in dict order
    N <a> <count>                end activities
    R <probability repr> <a> <b> ...   one line per trace, in log order
    Z                            the reference returned None (incomplete)
    X <type>                     the reference raised
"""
import argparse
import os
from pathlib import Path
import random
import subprocess
import sys
import warnings

warnings.filterwarnings('ignore')

BANK_SEED = 20261104
CASES = 400
HASH_SEEDS = [0, 1, 2]


def cases():
    rng = random.Random(BANK_SEED)
    for index in range(CASES):
        acts = [chr(97 + i) for i in range(rng.randint(1, 5))]
        regular = rng.random() < 0.4
        dfg = {}
        for x in acts:
            for y in acts:
                if rng.random() < 0.45:
                    dfg[(x, y)] = 1 if regular else rng.randint(1, 6)
        dfg = dict(sorted(dfg.items()))
        starts = [a for a in acts if rng.random() < 0.5] or [acts[0]]
        rng.shuffle(starts)
        sa = {a: (1 if regular else rng.randint(1, 4)) for a in starts}
        enders = [a for a in acts if rng.random() < 0.6] or [acts[-1]]
        ea = {a: (1 if regular else rng.randint(1, 6)) for a in enders}
        params = (rng.choice([1, 2, 3, 5, 10, 40, 3000]),
                  rng.choice([1, 1, 0.9, 0.5, 0.2]),
                  rng.choice([1, 2, 2, 3]),
                  rng.random() < 0.2,
                  rng.random() < 0.2,
                  rng.choice([1, 1, 1, 2, 5]),
                  rng.random() < 0.15)
        yield f'dfgc-{index:03d}', params, dfg, sa, ea


def child():
    from pm4py.algo.simulation.playout.dfg import algorithm as playout
    from pm4py.algo.simulation.playout.dfg.variants import classic
    P = classic.Parameters
    out = []
    for name, params, dfg, sa, ea in cases():
        max_variants, min_prob, max_occ, interrupt, add_new, min_occ, only_complete = params
        out.append(f'C {name} {max_variants} {min_prob!r} {max_occ} {int(interrupt)} {int(add_new)} '
                   f'{min_occ} {int(only_complete)}')
        out.extend(f'E {a} {b} {count}' for (a, b), count in dfg.items())
        out.extend(f'S {a} {count}' for a, count in sa.items())
        out.extend(f'N {a} {count}' for a, count in ea.items())
        try:
            log = playout.apply(dict(dfg), dict(sa), dict(ea), variant=playout.Variants.CLASSIC, parameters={
                P.MAX_NO_VARIANTS: max_variants, P.MIN_WEIGHTED_PROBABILITY: min_prob,
                P.MAX_NO_OCC_PER_ACTIVITY: max_occ, P.INTERRUPT_SIMULATION_WHEN_DFG_COMPLETE: interrupt,
                P.ADD_TRACE_IF_TAKES_NEW_ELS_TO_DFG: add_new, P.MIN_VARIANT_OCC: min_occ,
                P.RETURN_ONLY_IF_COMPLETE: only_complete, P.MAX_EXECUTION_TIME: 10 ** 9})
        except Exception as error:
            out.append('X ' + type(error).__name__)
            continue
        if log is None:
            # RETURN_ONLY_IF_COMPLETE on an incomplete search returns None.
            out.append('Z')
            continue
        for index, trace in enumerate(log):
            assert trace.attributes['concept:name'] == str(index)
            out.append(' '.join(['R', repr(trace.attributes['probability'])] +
                                [event['concept:name'] for event in trace]))
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
                           env=dict(os.environ, PYTHONHASHSEED=str(seed))).stdout
            for seed in HASH_SEEDS]
    assert all(run == runs[0] for run in runs), 'DFG playout depends on the hash seed'
    header = (f'# algo.simulation.playout.dfg CLASSIC, pm4py 2.7.22, bank seed {BANK_SEED}; '
              f'identical under PYTHONHASHSEED {HASH_SEEDS}.\n')
    args.output.write_bytes(header.encode() + runs[0])


if __name__ == '__main__':
    main()
