"""Capture seeded random process trees of the reference generators.

    python scripts/capture-random-tree-bank.py \\
        --output src/backend/tests/fixtures/random_tree_bank.txt

algo.simulation.tree_generator BASIC (random.seed(s)) and PTANDLOGGENERATOR
(random.seed(s) and numpy.random.seed(s)) over random parameters. The capture
runs itself under PYTHONHASHSEED 0, 1 and 2 and requires identical output.

Fixture lines:
    C basic <name> <seed> <rec_depth> <min_rec_depth> <max_rec_depth> <prob_leaf>
    C ptand <name> <seed> <min> <mode> <max> <sequence> <choice> <parallel> <loop> <or>
            <silent> <duplicate> <no_models>
    T <tree>     one per generated model, prefix form (->, X, +, *, O, <>, a label
                 or tau)
    X <type>     the reference raised this exception
"""
import argparse
import contextlib
import io
import os
from pathlib import Path
import random
import signal
import subprocess
import sys
import warnings

warnings.filterwarnings('ignore')

BANK_SEED = 20261103
BASIC_CASES = 400
PTAND_CASES = 500
HASH_SEEDS = [0, 1, 2]


def text(tree):
    from pm4py.objects.process_tree.obj import Operator
    symbols = {Operator.SEQUENCE: '->', Operator.XOR: 'X', Operator.PARALLEL: '+',
               Operator.LOOP: '*', Operator.OR: 'O', Operator.INTERLEAVING: '<>'}
    if tree.operator is None:
        return 'tau' if tree.label is None else tree.label
    return symbols[tree.operator] + '(' + ','.join(text(child) for child in tree.children) + ')'


def cases():
    rng = random.Random(BANK_SEED)
    for index in range(BASIC_CASES):
        high = rng.randint(0, 5)
        low = rng.randint(0, high)
        depth = rng.randint(0, low)
        leaf = rng.choice([0.0, 0.1, 0.25, 0.5, 0.75, 1.0])
        yield ('basic', f'basic-{index:03d}', rng.randint(0, 2 ** 32 - 1), depth, low, high, leaf)
    for index in range(PTAND_CASES):
        low = rng.randint(1, 12)
        high = low + rng.choice([0, 0, rng.randint(1, 40)])
        mode = rng.randint(low, high)
        weights = [rng.choice([0, 0.25, 0.5, 1]) for _ in range(4)] + [rng.choice([0, 0, 0.3])]
        if not any(weights):
            weights[0] = 1
        yield ('ptand', f'ptand-{index:03d}', rng.randint(0, 2 ** 32 - 1), low, mode, high, *weights,
               rng.choice([0, 0.2, 0.5, 1]), rng.choice([0, 0, 0.2, 0.6]), rng.choice([1, 1, 2, 4]))


def _timeout(*_):
    raise TimeoutError()


def child():
    signal.signal(signal.SIGALRM, _timeout)
    from pm4py.algo.simulation.tree_generator import algorithm as generator
    out = []
    for case in cases():
        out.append('C ' + ' '.join(str(x) for x in case))
        seed = case[2]
        random.seed(seed)
        import numpy
        numpy.random.seed(seed)
        signal.alarm(20)  # the reference can loop forever; such cases are dropped
        try:
            with contextlib.redirect_stdout(io.StringIO()):
                if case[0] == 'basic':
                    trees = [generator.apply(variant=generator.Variants.BASIC, parameters={
                        'rec_depth': case[3], 'min_rec_depth': case[4], 'max_rec_depth': case[5],
                        'prob_leaf': case[6]})]
                else:
                    names = ['min', 'mode', 'max', 'sequence', 'choice', 'parallel', 'loop', 'or',
                             'silent', 'duplicate', 'no_models']
                    trees = generator.apply(variant=generator.Variants.PTANDLOGGENERATOR,
                                            parameters=dict(zip(names, case[3:])))
                    if not isinstance(trees, list):
                        trees = [trees]
            out.extend('T ' + text(tree) for tree in trees)
        except TimeoutError:
            out.pop()
            continue
        except Exception as error:  # the reference's own failure
            out.append('X ' + type(error).__name__)
        finally:
            signal.alarm(0)
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
    assert all(run == runs[0] for run in runs), 'tree generation depends on the hash seed'
    header = (f'# algo.simulation.tree_generator BASIC and PTANDLOGGENERATOR, pm4py 2.7.22, bank seed '
              f'{BANK_SEED}; identical under PYTHONHASHSEED {HASH_SEEDS}.\n')
    args.output.write_bytes(header.encode() + runs[0])
    print(BASIC_CASES + PTAND_CASES, 'cases')


if __name__ == '__main__':
    main()
