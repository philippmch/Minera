"""Capture the disjoint public EventLog filtering bank.

Generates seeded cases with scripts/log_filter_domain.py and runs every call of
every case in fresh interpreters, one per PYTHONHASHSEED (TZ=Europe/Berlin, as
the earlier captures). Each call runs on a deep copy of the imported log. A
call whose public outcome differs between hash seeds keeps every observed
outcome. The bank is replayed natively by minera_log_filter_bank_parity_tests.
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
BANK_SEED = 20261025


def domain():
    spec = importlib.util.spec_from_file_location("log_filter_domain", HERE / "log_filter_domain.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run_cell(count, bank_seed, start, stop):
    warnings.filterwarnings("ignore")
    d = domain()
    rows = []
    for spec in d.generate_cases(count, bank_seed)[start:stop]:
        xes, calls = d.build_case(spec)
        log = d.import_log(xes)
        rows.append([d.run_call(call, log) for call in calls])
    return rows


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
    stats = {"cases": args.count, "calls": 0, "logs": 0, "values": 0, "errors": 0,
             "unrepresentable": 0, "varying": 0}
    for index, spec in enumerate(d.generate_cases(args.count, args.bank_seed)):
        xes, calls = d.build_case(spec)
        out_calls = []
        for c, call in enumerate(calls):
            observed = []
            for seed in seeds:
                outcome = per_seed[seed][index][c]
                if outcome not in observed:
                    observed.append(outcome)
            first = observed[0]
            kind = next(iter(first))
            stats["calls"] += 1
            stats[{"log": "logs", "value": "values", "error": "errors",
                   "unrepresentable": "unrepresentable"}[kind]] += 1
            if len(observed) > 1:
                stats["varying"] += 1
                call = dict(call, observed=observed)
            else:
                call = dict(call, outcome=first)
            out_calls.append(call)
        cases.append({"xes": xes, "calls": out_calls})
    bank = {"generator": "scripts/log_filter_domain.py", "bank_seed": args.bank_seed,
            "hash_seeds": seeds, "timezone": "Europe/Berlin", "stats": stats, "cases": cases}
    data = json.dumps(bank, sort_keys=True, ensure_ascii=True, separators=(",", ":")).encode()
    with open(args.output, "wb") as out:
        with gzip.GzipFile(fileobj=out, mode="wb", mtime=0) as gz:
            gz.write(data)
    print(json.dumps(stats))


if __name__ == "__main__":
    main()
