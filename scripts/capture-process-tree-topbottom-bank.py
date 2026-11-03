"""Capture seeded TOPBOTTOM process-tree playouts of the reference.

    python scripts/capture-process-tree-topbottom-bank.py \\
        --output src/backend/tests/fixtures/process_tree_topbottom_bank.txt

Random trees (sequence, exclusive choice, parallel, loop with two or three
children, interleaving; visible and silent leaves, repeated labels) are played
out with algo.simulation.playout.process_tree TOPBOTTOM after random.seed(s),
for three seeds per tree. The capture runs itself under PYTHONHASHSEED 0, 1
and 2 and requires the three runs to agree before writing the fixture.

Fixture lines:
    C <name> <seed> <num_traces>
    T <tree>          prefix form: ->(...), X(...), +(...), *(...), <>(...),
                      a label, or tau
    R <activity> ...  one line per trace, in log order
"""
import argparse
import os
from pathlib import Path
import random
import subprocess
import sys
import warnings

warnings.filterwarnings('ignore')

BANK_SEED = 20261103
TREES = 300
SEEDS_PER_TREE = 3
NUM_TRACES = 10
HASH_SEEDS = [0, 1, 2]
OPS = ['->', 'X', '+', '*', '<>']


def random_tree(rng, depth, labels):
    if depth == 0 or rng.random() < 0.3:
        if rng.random() < 0.15:
            return 'tau'
        return rng.choice(labels)
    op = rng.choice(OPS)
    count = rng.randint(2, 3) if op == '*' else rng.randint(1, 4)
    return (op, [random_tree(rng, depth - 1, labels) for _ in range(count)])


def text(node):
    if isinstance(node, str):
        return node
    return node[0] + '(' + ','.join(text(child) for child in node[1]) + ')'


def to_pm4py(node, parent=None):
    from pm4py.objects.process_tree.obj import ProcessTree, Operator
    ops = {'->': Operator.SEQUENCE, 'X': Operator.XOR, '+': Operator.PARALLEL,
           '*': Operator.LOOP, '<>': Operator.INTERLEAVING}
    if isinstance(node, str):
        return ProcessTree(label=None if node == 'tau' else node, parent=parent)
    tree = ProcessTree(operator=ops[node[0]], parent=parent)
    for child in node[1]:
        tree.children.append(to_pm4py(child, tree))
    return tree


def cases():
    rng = random.Random(BANK_SEED)
    for index in range(TREES):
        pool = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'][:rng.randint(2, 8)]
        tree = random_tree(rng, rng.randint(1, 4), pool)
        for k in range(SEEDS_PER_TREE):
            yield f'tb-{index:03d}-{k}', rng.randint(0, 2 ** 32 - 1), tree


def child():
    from pm4py.algo.simulation.playout.process_tree import algorithm as playout
    out = []
    for name, seed, tree in cases():
        # A fresh tree per call: TOPBOTTOM shuffles interleaving children in
        # place, so the order would otherwise leak between calls.
        model = to_pm4py(tree)
        random.seed(seed)
        log = playout.apply(model, variant=playout.Variants.TOPBOTTOM,
                            parameters={'num_traces': NUM_TRACES})
        out.append(f'C {name} {seed} {NUM_TRACES}')
        out.append('T ' + text(tree))
        for trace in log:
            out.append(' '.join(['R'] + [event['concept:name'] for event in trace]))
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
    assert all(run == runs[0] for run in runs), 'TOPBOTTOM output depends on the hash seed'
    header = (f'# algo.simulation.playout.process_tree TOPBOTTOM, pm4py 2.7.22, bank seed {BANK_SEED};\n'
              f'# {TREES} trees x {SEEDS_PER_TREE} seeds x {NUM_TRACES} traces, identical under '
              f'PYTHONHASHSEED {HASH_SEEDS}.\n')
    args.output.write_bytes(header.encode() + runs[0])
    print(TREES * SEEDS_PER_TREE, 'cases')


if __name__ == '__main__':
    main()
