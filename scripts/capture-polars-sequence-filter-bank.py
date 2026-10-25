"""Capture the public bank for the Polars sequence filters.

Generates seeded cases with scripts/polars_sequence_filter_domain.py and runs
every call REPEATS times on a fresh LazyFrame in fresh interpreters, one per
PYTHONHASHSEED (TZ=Europe/Berlin). Polars group_by and dict outputs may come in
a different order on every run: a call whose outcomes differ only in the order
of table rows or dict entries is stored once with "unordered": true (native
must match it up to that order); a call whose outcomes differ otherwise keeps
every observed outcome. Each case stores its source table once. The bank is
replayed natively by minera_polars_sequence_filter_bank_parity_tests.
"""
import argparse
import gzip
import importlib.util
import json
import os
import subprocess
import sys
import warnings
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent
BANK_SEED = 20261029
REPEATS = 3


def domain():
    spec = importlib.util.spec_from_file_location("polars_sequence_filter_domain",
                                                  HERE / "polars_sequence_filter_domain.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run_cell(count, bank_seed, start, stop):
    warnings.filterwarnings("ignore")
    d = domain()
    return [[[d.run_call(case, call) for _ in range(REPEATS)] for call in case["calls"]]
            for case in d.generate_cases(count, bank_seed)[start:stop]]


def canonical(node):
    """The outcome with table rows and dict entries in a fixed order."""
    if isinstance(node, dict):
        if set(node) == {"table"} and isinstance(node["table"], dict):
            table = dict(node["table"])
            table["rows"] = sorted(table["rows"], key=lambda row: json.dumps(row, sort_keys=True))
            return {"table": table}
        if set(node) == {"m"}:
            return {"m": sorted((canonical(e) for e in node["m"]), key=lambda e: json.dumps(e, sort_keys=True))}
        return {k: canonical(v) for k, v in node.items()}
    if isinstance(node, list):
        return [canonical(v) for v in node]
    return node


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--count", type=int, default=3000)
    parser.add_argument("--bank-seed", type=int, default=BANK_SEED)
    parser.add_argument("--hash-seeds", default="0,1,2,3")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--cell", nargs=4, type=int, help=argparse.SUPPRESS)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.cell:
        json.dump(run_cell(*args.cell), sys.stdout)
        return
    seeds = [int(s) for s in args.hash_seeds.split(",")]
    chunk = max(1, args.count // (args.jobs * 2))
    ranges = [(a, min(a + chunk, args.count)) for a in range(0, args.count, chunk)]

    def cell(seed, lo, hi):
        env = dict(os.environ, PYTHONHASHSEED=str(seed), TZ="Europe/Berlin")
        out = subprocess.run([sys.executable, "-X", "utf8", __file__, "--cell", str(args.count),
                              str(args.bank_seed), str(lo), str(hi)],
                             env=env, check=True, capture_output=True, text=True).stdout
        return json.loads(out)

    with ThreadPoolExecutor(args.jobs) as pool:
        futures = {seed: [pool.submit(cell, seed, lo, hi) for lo, hi in ranges] for seed in seeds}
        per_seed = {seed: [row for f in fs for row in f.result()] for seed, fs in futures.items()}
    d = domain()
    cases = []
    stats = {"cases": args.count, "calls": 0, "values": 0, "errors": 0, "unrepresentable": 0,
             "varying": 0, "unordered": 0}
    for index, case in enumerate(d.generate_cases(args.count, args.bank_seed)):
        out_calls = []
        for c, call in enumerate(case["calls"]):
            observed = []
            for seed in seeds:
                for outcome in per_seed[seed][index][c]:
                    if outcome not in observed:
                        observed.append(outcome)
            first = observed[0]
            stats["calls"] += 1
            kind = "values" if "value" in first else "errors" if "error" in first else "unrepresentable"
            stats[kind] += 1
            orders = {json.dumps(canonical(o), sort_keys=True) for o in observed}
            if len(observed) > 1 and len(orders) == 1:
                stats["unordered"] += 1
                call = dict(call, outcome=first, unordered=True)
            elif len(observed) > 1:
                stats["varying"] += 1
                call = dict(call, observed=observed)
            else:
                call = dict(call, outcome=first)
            out_calls.append(call)
        cases.append({"source": d.source_json(case), "calls": out_calls})
    bank = {"generator": "scripts/polars_sequence_filter_domain.py", "repeats": REPEATS, "bank_seed": args.bank_seed,
            "hash_seeds": seeds, "timezone": "Europe/Berlin", "stats": stats, "cases": cases}
    data = json.dumps(bank, sort_keys=True, ensure_ascii=True, separators=(",", ":")).encode()
    with open(args.output, "wb") as out:
        with gzip.GzipFile(fileobj=out, mode="wb", mtime=0) as gz:
            gz.write(data)
    print(json.dumps(stats))


if __name__ == "__main__":
    main()
