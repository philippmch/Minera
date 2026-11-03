"""Capture the trace distribution of the reference BASIC_PLAYOUT on small trees.

    python scripts/capture-process-tree-basic-distribution.py \\
        --output src/backend/tests/fixtures/process_tree_basic_distribution.txt

BASIC_PLAYOUT keeps its enabled vertices in a Python set whose order follows
the string hash, so a seed does not reproduce its traces; the contract is the
distribution. For each tree the reference plays out TRACES traces (one run per
PYTHONHASHSEED 0..3, pooled) and the fixture keeps the count of every
activity word. minera_process_tree_playout_tests compares Minera's BASIC
playout with these counts by a chi-square homogeneity test.

Fixture lines:
    C <tree>            prefix form as in the TOPBOTTOM bank, O(...) for OR
    F <count> <word>    activities joined by '.', '-' for the empty trace
"""
import argparse
from collections import Counter
import json
import os
from pathlib import Path
import random
import subprocess
import sys
import warnings

warnings.filterwarnings('ignore')

TRACES = 10000
HASH_SEEDS = [0, 1, 2, 3]
TREES = [
    "+(a,->(b,c))",
    "O(a,b)",
    "*(a,b)",
    "+(a,*(b,c))",
    "+(a,*(b,c,d))",
    "<>(a,->(b,c),d)",
    "+(X(a,b),tau,c)",
    "->(*(a,tau),+(b,c))",
    "+(O(a,b),c)",
    "+(<>(a,b),c)",
    "X(+(a,b,c),*(d,tau,e))",
    "+(*(a,tau),*(b,tau))",
]


def pm4py_text(tree):
    # pm4py's parser wants quoted labels and calls the inclusive choice O.
    out, token = [], ''
    for ch in tree + ',':
        if ch in '(),':
            if token:
                out.append(token if token in ('tau',) else f"'{token}'")
                token = ''
            out.append(ch)
        else:
            token += ch
    text = ''.join(out[:-1])
    for op in ("'->'", "'X'", "'+'", "'*'", "'O'", "'<>'"):
        text = text.replace(op + '(', op.strip("'") + '(')
    return text


def child():
    from pm4py.objects.process_tree.utils.generic import parse
    from pm4py.algo.simulation.playout.process_tree import algorithm as playout
    random.seed(int(os.environ['PYTHONHASHSEED']) + 7)
    rows = {}
    for tree in TREES:
        model = parse(pm4py_text(tree))
        log = playout.apply(model, variant=playout.Variants.BASIC_PLAYOUT,
                            parameters={'num_traces': TRACES})
        rows[tree] = Counter('.'.join(e['concept:name'] for e in trace) or '-' for trace in log)
    json.dump(rows, sys.stdout)


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
    pooled = {tree: Counter() for tree in TREES}
    for seed in HASH_SEEDS:
        output = subprocess.run([sys.executable, __file__, '--child'], check=True, capture_output=True,
                                env=dict(os.environ, PYTHONHASHSEED=str(seed))).stdout
        for tree, counts in json.loads(output).items():
            pooled[tree].update(counts)
    lines = [f'# algo.simulation.playout.process_tree BASIC_PLAYOUT, pm4py 2.7.22; {TRACES} traces '
             f'per tree under each PYTHONHASHSEED {HASH_SEEDS}, pooled.']
    for tree in TREES:
        lines.append('C ' + tree)
        for word, count in sorted(pooled[tree].items(), key=lambda item: (-item[1], item[0])):
            lines.append(f'F {count} {word}')
    args.output.write_text('\n'.join(lines) + '\n')
    print(len(TREES), 'trees')


if __name__ == '__main__':
    main()
