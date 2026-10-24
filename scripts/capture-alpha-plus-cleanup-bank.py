"""Capture a disjoint public Alpha+ cleanup-selection bank across hash seeds.

The public runtime is observed only through its documented entry point and
the structural summarizer shared with capture-alpha-plus-reference.py.  Each
(hash seed, trace order) cell runs in a fresh interpreter, and every case is
called repeatedly with freshly built inputs so within-process stability is
recorded separately from cross-process variation.
"""
import argparse
import gzip
import hashlib
import importlib.util
import json
import os
import random
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
BANK_SEED = 20261024
# Ordinary labels are disjoint from every earlier Alpha+ corpus (a, b, L, M,
# label_*); the reserved boundary labels are included on purpose.
ORDINARY = ("p", "q", "r", "s")
RESERVED = ("artificial_start", "artificial_end")


def _shared():
    spec = importlib.util.spec_from_file_location(
        "alpha_plus_capture", HERE / "capture-alpha-plus-reference.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def generate_cases(count, bank_seed=BANK_SEED, ordinary=ORDINARY, max_traces=3, max_length=5):
    rng = random.Random(bank_seed)
    seen, cases = set(), []
    while len(cases) < count:
        reserved = rng.random() < 0.35
        alphabet = tuple(ordinary[:rng.randint(1, len(ordinary))]) + (RESERVED if reserved else ())
        traces = []
        for _ in range(rng.randint(1, max_traces)):
            word = []
            for _ in range(rng.randint(0, max_length)):
                label = rng.choice(alphabet)
                word.append(label)
                if rng.random() < 0.3:
                    word.append(label)  # immediate repetition creates a short loop
            traces.append(word)
        key = json.dumps(traces)
        # At least one ordinary label keeps banks with distinct label sets
        # disjoint from each other and from the earlier corpora.
        if not any(label in ordinary for word in traces for label in word):
            continue
        if key not in seen:
            seen.add(key)
            cases.append(traces)
    return cases


def run_cell(cases, order, calls):
    shared = _shared()
    _, plus, Event, EventLog, Trace = shared._load_public_reference_api()
    cells = []
    for traces in cases:
        ordered = list(reversed(traces)) if order == "reversed" else traces
        for cleanup in (False, True):
            outcomes = []
            for _ in range(calls):
                log = EventLog([Trace([Event({"concept:name": label}) for label in word])
                                for word in ordered])
                try:
                    observed = shared.summarize(*plus.apply(
                        log, parameters={"remove_unconnected": cleanup}))
                except Exception as error:  # the public failure class is recorded, not matched
                    observed = dict(error=type(error).__name__, message=str(error))
                outcomes.append(json.dumps(observed, sort_keys=True))
            cells.append(outcomes)
    return cells


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path)
    parser.add_argument("--cases", type=int, default=300)
    parser.add_argument("--seeds", type=int, default=8)
    parser.add_argument("--calls", type=int, default=3)
    parser.add_argument("--bank-seed", type=int, default=BANK_SEED)
    parser.add_argument("--labels", default=",".join(ORDINARY))
    parser.add_argument("--max-traces", type=int, default=3)
    parser.add_argument("--max-length", type=int, default=5)
    parser.add_argument("--cell", help=argparse.SUPPRESS)
    args = parser.parse_args()
    generator = dict(bank_seed=args.bank_seed, ordinary=args.labels.split(","),
                     max_traces=args.max_traces, max_length=args.max_length)
    cases = generate_cases(args.cases, **generator)
    if args.cell:
        json.dump(run_cell(cases, args.cell, args.calls), sys.stdout)
        return
    import pm4py
    assert pm4py.__version__ == "2.7.22"
    outcomes, index = [], {}
    def intern(text):
        if text not in index:
            index[text] = len(outcomes)
            outcomes.append(json.loads(text))
        return index[text]
    processes = []
    for seed in range(args.seeds):
        for order in ("original", "reversed"):
            env = dict(os.environ, PYTHONHASHSEED=str(seed))
            run = subprocess.run([sys.executable, "-B", "-X", "utf8", __file__, "--cell", order,
                                  "--cases", str(args.cases), "--calls", str(args.calls),
                                  "--bank-seed", str(args.bank_seed), "--labels", args.labels,
                                  "--max-traces", str(args.max_traces),
                                  "--max-length", str(args.max_length)],
                                 env=env, capture_output=True, text=True, encoding="utf-8", check=True)
            cells = json.loads(run.stdout)
            processes.append(dict(hash_seed=seed, trace_order=order,
                                  cells=[[intern(text) for text in cell] for cell in cells]))
            print(f"seed {seed} {order}: {len(cells)} cells", file=sys.stderr)
    rows = []
    for c, traces in enumerate(cases):
        for k, cleanup in enumerate((False, True)):
            rows.append(dict(case=c, traces=traces, remove_unconnected=cleanup,
                             observations=[dict(hash_seed=p["hash_seed"], trace_order=p["trace_order"],
                                                outcomes=p["cells"][2 * c + k]) for p in processes]))
    body = dict(reference=pm4py.__version__, generator=generator, python=sys.version.split()[0],
                calls_per_cell=args.calls, hash_seeds=list(range(args.seeds)),
                trace_orders=["original", "reversed"], outcomes=outcomes, rows=rows)
    data = json.dumps(body, sort_keys=True).encode("utf-8")
    with open(args.output, "wb") as raw, gzip.GzipFile(filename="", fileobj=raw, mode="wb", mtime=0) as out:
        out.write(data)
    print(f"{len(rows)} rows, {len(outcomes)} distinct outcomes, sha256 "
          f"{hashlib.sha256(data).hexdigest()}", file=sys.stderr)


if __name__ == "__main__":
    main()
