"""Capture the public bank for algo.reduction.process_tree (TREE_TR_BASED).

Seeded process trees (sequence, xor, parallel, loop and or nodes with one to
four children, loops mostly binary, activity and tau leaves, depth up to four)
and seeded traces (possibly empty, with activities outside the tree) go
through the public reducer.apply with the TREE_TR_BASED variant, through
tree_tr_based.apply with a custom ACTIVITY_KEY, and through reducer.apply
without a variant, in fresh interpreters under three hash seeds, which must
agree. The input tree must stay unchanged. The bank is a gzip text file of
tagged lines read by minera_tree_reduction_parity_tests:

  C name mode      mode: variant, custom_key, no_variant or fold
  T tree           the input tree
  A activities     the trace ("-" when empty)
  O tree | E type message   one line per observed outcome

Trees are written as "T" (tau), "L:label" or OP(child,...) with OP one of
->, X, +, *, O, <>; an operator may have no children ("X()").

After the seeded reduction cases come the extra cases: process_tree.utils.
generic.fold on fixed and seeded trees (mode fold, every operator including
interleaving, some operators without children, no trace), and the seeded
TREE_TR_BASED reduction of trees with interleaving nodes (mode variant, names
starting "il").
"""
import argparse
import gzip
import json
import os
import random
import subprocess
import sys
import warnings
from pathlib import Path

HERE = Path(__file__).resolve().parent
BANK_SEED = 20261025
OPS = ("->", "X", "+", "*", "O")
EXTRA_SEED = 20261103
ALL_OPS = OPS + ("<>",)
# Fixed fold cases: the reference keeps childless operators, collapses only a
# binary all-tau loop, and removes taus and absorbed children by equality, under
# which a childless operator equals tau.
FOLD_FIXED = (
    "*(T,T,T)", "*(T,T)", "*(L:a)", "*()", "<>(T,L:a)", "<>(L:a,<>(L:b,L:c))", "<>(L:a)", "+()", "->()",
    "X()", "O()", "<>()", "X(L:a,X())", "->(X(),T)", "->(T,X())", "X(T,X())", "X(X(),+())", "X(+(),X())",
    "->(L:a,<>())", "+(L:a,O(),T)", "+(L:a,O(),+(T))", "*(X(),T)", "X(L:a,X(L:b,T),T)", "O(T,L:a,T,O(L:b))",
    "->(L:a,->(L:b,T),T)", "+(T,T)", "O(T,T)",
)


def gen(r, depth):
    if depth == 0 or r.random() < 0.3:
        return "T" if r.random() < 0.15 else "L:" + r.choice("abcdef")
    op = r.choice(OPS)
    k = 2 if op == "*" and r.random() < 0.85 else r.randint(1, 4)
    return (op, [gen(r, depth - 1) for _ in range(k)])


def encode(node):
    if isinstance(node, str):
        return node
    return node[0] + "(" + ",".join(encode(c) for c in node[1]) + ")"


def gen_extra(r, depth, ops, empty):
    if depth == 0 or r.random() < 0.3:
        return "T" if r.random() < 0.25 else "L:" + r.choice("abcd")
    op = r.choice(ops)
    if op == "*":
        k = r.choice([2, 2, 3])
    elif empty and r.random() < 0.1:
        k = r.randint(0, 4)
    else:
        k = r.randint(1, 4)
    return (op, [gen_extra(r, depth - 1, ops, empty) for _ in range(k)])


def extra_cases(folds, interleaving):
    r = random.Random(EXTRA_SEED)
    out = [{"name": f"foldfixed{i}", "mode": "fold", "tree": t, "trace": []} for i, t in enumerate(FOLD_FIXED)]
    out.append({"name": "ilfixed0", "mode": "variant", "tree": "<>(L:a,X(L:b,T))", "trace": ["a"]})
    for i in range(folds):
        out.append({"name": f"fold{i}", "mode": "fold", "tree": encode(gen_extra(r, 4, ALL_OPS, True)),
                    "trace": []})
    while interleaving:
        tree = encode(gen_extra(r, 3, ALL_OPS, False))
        if "<>" not in tree:
            continue
        acts = [r.choice("abcde") for _ in range(r.randint(0, 4))]
        out.append({"name": f"il{len(out)}", "mode": "variant", "tree": tree, "trace": acts})
        interleaving -= 1
    return out


def cases(count, seed):
    r = random.Random(seed)
    out = []
    for i in range(count):
        roll = r.random()
        mode = "variant" if roll < 0.85 else "custom_key" if roll < 0.97 else "no_variant"
        tree = gen(r, r.choice([1, 2, 3, 3, 4]))
        acts = [r.choice("abcdefg") for _ in range(r.choice([0, 1, 2, 3, 4, 5, 7]))]
        out.append({"mode": mode, "tree": encode(tree), "trace": acts})
    return out


def all_cases(count, seed, folds, interleaving):
    return cases(count, seed) + extra_cases(folds, interleaving)


def parse(text):
    from pm4py.objects.process_tree.obj import Operator, ProcessTree
    ops = {"->": Operator.SEQUENCE, "X": Operator.XOR, "+": Operator.PARALLEL, "*": Operator.LOOP,
           "O": Operator.OR, "<>": Operator.INTERLEAVING}
    at = 0

    def node(parent):
        nonlocal at
        if text.startswith("T", at):
            at += 1
            return ProcessTree(parent=parent)
        if text.startswith("L:", at):
            end = at + 2
            while end < len(text) and text[end] not in ",)":
                end += 1
            label, at = text[at + 2:end], end
            return ProcessTree(parent=parent, label=label)
        op = next(o for o in ops if text.startswith(o + "(", at))
        at += len(op) + 1
        tree = ProcessTree(operator=ops[op], parent=parent)
        if text.startswith(")", at):
            at += 1
            return tree
        while True:
            tree.children.append(node(tree))
            at += 1
            if text[at - 1] == ")":
                return tree

    return node(None)


def dump(tree):
    from pm4py.objects.process_tree.obj import Operator
    names = {Operator.SEQUENCE: "->", Operator.XOR: "X", Operator.PARALLEL: "+", Operator.LOOP: "*",
             Operator.OR: "O", Operator.INTERLEAVING: "<>"}
    if tree.operator is None:
        return "T" if tree.label is None else "L:" + tree.label
    return names[tree.operator] + "(" + ",".join(dump(c) for c in tree.children) + ")"


def run(case):
    from pm4py.algo.reduction.process_tree import reducer
    from pm4py.algo.reduction.process_tree.variants import tree_tr_based
    from pm4py.objects.log.obj import Event, Trace
    from pm4py.objects.process_tree.utils import generic
    tree = parse(case["tree"])
    key = "act" if case["mode"] == "custom_key" else "concept:name"
    trace = Trace([Event({key: a}) for a in case["trace"]])
    try:
        if case["mode"] == "fold":
            result = generic.fold(tree)
        elif case["mode"] == "variant":
            result = reducer.apply(tree, trace, variant=reducer.Variants.TREE_TR_BASED)
        elif case["mode"] == "custom_key":
            result = tree_tr_based.apply(tree, trace, parameters={tree_tr_based.Parameters.ACTIVITY_KEY: "act"})
        else:
            result = reducer.apply(tree, trace)
        outcome = {"tree": dump(result)}
    except Exception as error:  # noqa: BLE001 - every public outcome is evidence
        outcome = {"error": [type(error).__name__, str(error)]}
    if dump(tree) != case["tree"]:
        outcome["changed"] = dump(tree)
    return outcome


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--count", type=int, default=6000)
    parser.add_argument("--seed", type=int, default=BANK_SEED)
    parser.add_argument("--hash-seeds", default="0,1,2,3,4,5,6,7")
    parser.add_argument("--folds", type=int, default=3000)
    parser.add_argument("--interleaving", type=int, default=2000)
    parser.add_argument("--cell", nargs=4, type=int, help=argparse.SUPPRESS)
    parser.add_argument("--output", type=Path, default=HERE.parent / "docs" / "tree-reduction-bank.txt.gz")
    args = parser.parse_args()
    if args.cell:
        warnings.filterwarnings("ignore")
        json.dump([run(case) for case in all_cases(*args.cell)], sys.stdout)
        return
    per_seed = []
    for seed in (int(s) for s in args.hash_seeds.split(",")):
        env = dict(os.environ, PYTHONHASHSEED=str(seed))
        cell = [str(args.count), str(args.seed), str(args.folds), str(args.interleaving)]
        out = subprocess.run([sys.executable, "-X", "utf8", __file__, "--cell", *cell],
                             env=env, check=True, capture_output=True, text=True).stdout
        per_seed.append(json.loads(out))
    lines, errors, varying = [], 0, 0
    for index, case in enumerate(all_cases(args.count, args.seed, args.folds, args.interleaving)):
        observed = []
        for results in per_seed:
            if "changed" in results[index]:
                raise SystemExit(f"case {index} changed its input tree")
            if results[index] not in observed:
                observed.append(results[index])
        varying += len(observed) > 1
        lines += [f"C {case.get('name', f'case{index}')} {case['mode']}", "T " + case["tree"], "A " + (" ".join(case["trace"]) or "-")]
        for outcome in observed:
            if "error" in outcome:
                errors += 1
                lines.append("E " + outcome["error"][0] + " " + outcome["error"][1])
            else:
                lines.append("O " + outcome["tree"])
    with open(args.output, "wb") as handle:
        with gzip.GzipFile(fileobj=handle, mode="wb", mtime=0) as gz:
            gz.write(("\n".join(lines) + "\n").encode())
    print(json.dumps({"cases": len(per_seed[0]), "errors": errors, "varying": varying}))


if __name__ == "__main__":
    main()
