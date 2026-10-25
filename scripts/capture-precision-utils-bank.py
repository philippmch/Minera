"""Capture the public bank for algo.evaluation.precision.utils.

Three call kinds go through the module's public functions in fresh
interpreters under three hash seeds, which must agree:

- prefixes: get_log_prefixes on seeded EventLogs and DataFrames (string or
  integer case ids, activities that contain commas, a custom activity key);
- fake: form_fake_log on seeded prefix keys (empty keys and empty parts);
- search: the module's __search on synchronous products of seeded prefix
  trace nets and inductive-miner nets with seeded move costs, stopping at
  the trace net's final place.

The bank is a gzip text file of tagged lines read by
minera_precision_utils_parity_tests (JSON payloads):

  C name kind
  I json      the input
  O json      the output (sets as sorted lists, dicts as [key, value] lists)
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
SKIP = ">>"


def gen_prefixes(r):
    acts = ["a", "b", "c", "d"] + (["a,b"] if r.random() < 0.15 else [])
    kind = r.choice(["log", "df"])
    ids = r.choice(["str", "int"])
    traces = [[r.choice(acts) for _ in range(r.randint(0, 5))] for _ in range(r.randint(0, 5))]
    names = [r.choice([1, 2, 3, 10, 12]) for _ in traces]
    return {"kind": kind, "ids": ids, "names": names, "traces": traces,
            "key": "act" if r.random() < 0.15 else "concept:name", "shuffle": kind == "df" and r.random() < 0.3,
            "seed": r.randrange(1 << 30)}


def run_prefixes(case):
    import pandas as pd
    from pm4py.algo.evaluation.precision import utils
    from pm4py.objects.log.obj import Event, EventLog, Trace
    key = case["key"]
    if case["kind"] == "log":
        data = EventLog([Trace([Event({key: a}) for a in t]) for t in case["traces"]])
    else:
        rows = [{"case:concept:name": (str(n) if case["ids"] == "str" else n), key: a}
                for n, t in zip(case["names"], case["traces"]) for a in t]
        data = pd.DataFrame(rows, columns=["case:concept:name", key])
        if case["shuffle"] and len(data):
            data = data.sample(frac=1, random_state=case["seed"] % (1 << 31)).reset_index(drop=True)
        case["rows"] = [[str(row["case:concept:name"]), row[key]] for _, row in data.iterrows()]
    prefixes, counts = utils.get_log_prefixes(data, activity_key=key)
    return {"prefixes": [[k, sorted(v)] for k, v in prefixes.items()], "counts": [[k, v] for k, v in counts.items()]}


def gen_fake(r):
    parts = ["a", "b", "", " c", "d"]
    return {"keys": [",".join(r.choice(parts) for _ in range(r.randint(1, 3))) for _ in range(r.randint(0, 4))],
            "key": "act" if r.random() < 0.2 else "concept:name"}


def run_fake(case):
    from pm4py.algo.evaluation.precision import utils
    log = utils.form_fake_log(case["keys"], activity_key=case["key"])
    return {"traces": [[dict(e) for e in t] for t in log], "trace_attributes": [dict(t.attributes) for t in log]}


def gen_search(r):
    return {"seed": r.randrange(1 << 30)}


def build_search(case):
    import pm4py
    from pm4py.objects.log.obj import Event, EventLog, Trace
    from pm4py.objects.petri_net.obj import Marking
    from pm4py.objects.petri_net.utils import synchronous_product
    from pm4py.objects.petri_net.utils.petri_utils import construct_trace_net_cost_aware
    r = random.Random(case["seed"])
    acts = "abcd"
    log = EventLog([Trace([Event({"concept:name": r.choice(acts)}) for _ in range(r.randint(1, 4))])
                    for _ in range(r.randint(1, 5))])
    net, im, fm = pm4py.discover_petri_net_inductive(log, noise_threshold=r.choice([0, 0, 0.3]))
    prefix = [r.choice(acts) for _ in range(r.randint(0, 4))]
    trace_net, tim, tfm, trace_costs = construct_trace_net_cost_aware(
        Trace([Event({"concept:name": a}) for a in prefix]), [r.choice([1000, 1000, 1])] * len(prefix))
    # Costs are drawn per label (transition sets iterate in hash order).
    labels = sorted({str(t.label) for t in net.transitions})
    label_costs = {label: (r.choice([0, 1]) if label == "None" else r.choice([1000, 1000, 2])) for label in labels}
    model_costs = {t: label_costs[str(t.label)] for t in net.transitions}
    move_costs = {(a.name, label): r.choice([0, 0, 1]) for a in sorted(trace_net.transitions, key=lambda t: t.name)
                  for label in labels}
    sync_costs = {(a, b): move_costs[(a.name, str(b.label))] for a in trace_net.transitions
                  for b in net.transitions if a.label == b.label}
    sync, sim, sfm, costs = synchronous_product.construct_cost_aware(
        trace_net, tim, tfm, net, im, fm, SKIP, trace_costs, model_costs, sync_costs)
    end = [p for p in sync.places if p in sfm and p.name[1] == SKIP]
    return sync, sim, sfm, Marking({end[0]: 1}), costs


def run_search(case):
    from pm4py.algo.evaluation.precision import utils
    sync, sim, sfm, stop, costs = build_search(case)
    places = sorted(sync.places, key=lambda p: str(p.name))
    index = {p: i for i, p in enumerate(places)}
    transitions = sorted(sync.transitions, key=lambda t: (str(t.label), str(t.name)))
    case["net"] = {
        "places": [str(p.name) for p in places],
        "transitions": [{"label": list(t.label), "cost": costs[t],
                         "in": sorted([index[a.source], a.weight] for a in t.in_arcs),
                         "out": sorted([index[a.target], a.weight] for a in t.out_arcs)} for t in transitions],
        "initial": sorted([index[p], n] for p, n in sim.items()),
        "final": sorted([index[p], n] for p, n in sfm.items()),
        "stop": sorted([index[p], n] for p, n in stop.items())}
    found = getattr(utils, "__search")(sync, sim, sfm, stop, costs, SKIP)
    if found is None:
        return {"markings": None}
    return {"markings": sorted(sorted([index[p], n] for p, n in m.items()) for m in found)}


KINDS = {"prefixes": (gen_prefixes, run_prefixes), "fake": (gen_fake, run_fake), "search": (gen_search, run_search)}


def cases(count, seed):
    r = random.Random(seed)
    out = []
    for i in range(count):
        kind = ("prefixes", "fake", "search")[i % 3]
        out.append(dict(KINDS[kind][0](r), call=kind))
    return out


def run_cell(count, seed):
    warnings.filterwarnings("ignore")
    out = []
    for case in cases(count, seed):
        try:
            result = KINDS[case["call"]][1](case)
        except Exception as error:  # noqa: BLE001 - every public outcome is evidence
            result = {"error": [type(error).__name__, str(error)]}
        out.append({"input": case, "output": result})
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--count", type=int, default=3000)
    parser.add_argument("--seed", type=int, default=BANK_SEED)
    parser.add_argument("--hash-seeds", default="0,1,2")
    parser.add_argument("--cell", nargs=2, type=int, help=argparse.SUPPRESS)
    parser.add_argument("--output", type=Path, default=HERE.parent / "docs" / "precision-utils-bank.txt.gz")
    args = parser.parse_args()
    if args.cell:
        json.dump(run_cell(*args.cell), sys.stdout)
        return
    per_seed = []
    for seed in (int(s) for s in args.hash_seeds.split(",")):
        env = dict(os.environ, PYTHONHASHSEED=str(seed))
        out = subprocess.run([sys.executable, "-X", "utf8", __file__, "--cell", str(args.count), str(args.seed)],
                             env=env, check=True, capture_output=True, text=True).stdout
        per_seed.append(json.loads(out))
    outputs = [[item["output"] for item in results] for results in per_seed]
    if any(o != outputs[0] for o in outputs[1:]):
        raise SystemExit("hash seeds disagree")
    text = []
    for index, item in enumerate(per_seed[0]):
        text += [f"C case{index} {item['input']['call']}", "I " + json.dumps(item["input"], separators=(",", ":")),
                 "O " + json.dumps(item["output"], separators=(",", ":"))]
    with open(args.output, "wb") as handle:
        with gzip.GzipFile(fileobj=handle, mode="wb", mtime=0) as gz:
            gz.write(("\n".join(text) + "\n").encode())
    print(json.dumps({"cases": args.count}))


if __name__ == "__main__":
    main()
