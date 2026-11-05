"""Capture workflow-net to process-tree shapes from PM4Py 2.7.22.

Only public entry points are called; no native code runs. Each process tree
is converted with `convert_to_petri_net` and back with
`convert_to_process_tree`, as are a few literal chain nets and
activity-defines-place nets of DFGs (`convert_to_petri_net(dfg, start, end)`). The resulting tree is recorded in a canonical text
(children of exclusive choices and parallels sorted, since their order follows
set iteration) or, when the reference raises, as `!` followed by the
exception type. The reference folds the converted tree (nested sequences are
flattened, an exclusive choice keeps one tau) and raises on a net that is one
transition once its silent chain is contracted.

    python -I -B scripts/capture-net-to-tree-shape-reference.py \\
        --output src/backend/tests/net_to_tree_shape_reference_cases.inc
"""
import argparse
import importlib.util
import os
import random
import sys
import warnings
from pathlib import Path

if os.environ.get("PYTHONHASHSEED") != "0":
    os.environ["PYTHONHASHSEED"] = "0"
    os.execv(sys.executable, [sys.executable] + sys.orig_argv[1:])

warnings.filterwarnings("ignore")

import pm4py  # noqa: E402
from pm4py.objects.process_tree.obj import Operator, ProcessTree  # noqa: E402

_spec = importlib.util.spec_from_file_location(
    "tree_capture", Path(__file__).with_name("capture-workflow-net-tree-reference.py"))
_tree_capture = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_tree_capture)
compact_pnml = _tree_capture.compact_pnml
cpp_string = _tree_capture.cpp_string
literal_net = _tree_capture.literal_net

TREES = [
    "'a'", "tau", "->(tau,'a')", "->('a',tau)", "->(tau,'a',tau)", "->(tau,tau)",
    "->('a','b')", "->('a',->('b','c'))", "->(->('a','b'),'c')",
    "->('a',+('b','c'),'d')", "X(tau,tau,'a')", "X(tau,X(tau,'a'))",
    "X(X(tau,'a'),X(tau,'b'))", "+(X(tau,'a'),X(tau,tau,'b'))",
    "*(X(tau,tau),'a')", "->('a',X(tau,tau))", "X(tau,tau)", "+(tau,tau)",
    "+(tau,'a')", "X('a',->(tau,tau))", "X('a','b',+(tau,tau))",
    "*('a',X(tau,tau,'b'))", "*(*('a','b'),'c')", "X('a','a')",
]
RANDOM_SEEDS = range(20)
# Silent chains of two or more transitions convert; only a net of one
# transition raises (the tree nets above fold their silent steps away).
CHAINS = [["a"], [None], [None, "a"], ["a", None], [None, None, "a", None, None],
          [None, None, None], ["a", "b"]]


# Activity-defines-place nets of DFGs with self-loops at a loop's end place,
# which the reference reduces as ->(rest, *(tau, cycle)).
DFGS = [
    ({("c", "a"): 2, ("a", "a"): 1, ("a", "c"): 2, ("c", "b"): 1, ("b", "a"): 1},
     {"c": 1}, {"a": 1}),
    ({("a", "d"): 1, ("d", "e"): 1, ("e", "d"): 1, ("d", "a"): 1, ("a", "b"): 1,
      ("b", "d"): 1}, {"a": 1}, {"d": 1}),
    ({("a", "b"): 1, ("b", "b"): 1}, {"a": 1}, {"b": 1}),
    ({("a", "a"): 1, ("a", "b"): 1}, {"a": 1}, {"b": 1}),
]


def chain_net(labels):
    places = ["source"] + [f"p{index}" for index in range(len(labels) - 1)] + ["sink"]
    return literal_net(places, [(label, [places[index]], [places[index + 1]])
                                for index, label in enumerate(labels)])
SYMBOL = {Operator.SEQUENCE: "->", Operator.XOR: "X", Operator.PARALLEL: "+",
          Operator.LOOP: "*"}


def random_tree(seed):
    generator = random.Random(seed)
    labels = iter("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ")

    def node(depth, parent):
        if depth == 0 or generator.random() < 0.25:
            return ProcessTree(parent=parent,
                               label=None if generator.random() < 0.15 else next(labels))
        operator = generator.choice(list(SYMBOL))
        tree = ProcessTree(operator=operator, parent=parent)
        for _ in range(2 if operator == Operator.LOOP else generator.randint(2, 3)):
            tree.children.append(node(depth - 1, tree))
        return tree

    return node(3, None)


def canonical(tree):
    if tree.operator is None:
        return "tau" if tree.label is None else "'" + tree.label + "'"
    children = [canonical(child) for child in tree.children]
    if tree.operator in (Operator.XOR, Operator.PARALLEL):
        children.sort()
    return SYMBOL[tree.operator] + "(" + ",".join(children) + ")"


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    trees = [(text, pm4py.parse_process_tree(text)) for text in TREES]
    trees += [(f"random tree of seed {seed}", random_tree(seed)) for seed in RANDOM_SEEDS]
    nets = [(name, pm4py.convert_to_petri_net(tree)) for name, tree in trees]
    nets += [("chain " + " ".join(label or "tau" for label in labels), chain_net(labels))
             for labels in CHAINS]
    nets += [(f"activity-defines-place net of DFG {index}",
              pm4py.convert_to_petri_net(*dfg)) for index, dfg in enumerate(DFGS)]
    lines = [
        "// Generated by scripts/capture-net-to-tree-shape-reference.py from public",
        "// PM4Py 2.7.22: convert_to_process_tree(convert_to_petri_net(tree)) in a",
        "// canonical text (exclusive and parallel children sorted), or '!' and the",
        "// exception type when the reference raises.",
        "const std::vector<ReferenceTreeShapeCase> kReferenceTreeShapeCases{",
    ]
    for name, (net, initial, final) in nets:
        try:
            expected = canonical(pm4py.convert_to_process_tree(net, initial, final))
        except Exception as error:  # noqa: BLE001 - the reference raises plain exceptions
            expected = "!" + type(error).__name__
        lines.append(f"  {{{cpp_string(name)},\n   {cpp_string(compact_pnml(net, initial, final))},\n"
                     f"   {cpp_string(expected)}}},")
    lines.append("};")
    arguments.output.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
