"""Capture the public bank for the pandas sequence filters.

Generates seeded cases with scripts/pandas_sequence_filter_domain.py and runs
every call on a fresh DataFrame in fresh interpreters, one per PYTHONHASHSEED
(TZ=Europe/Berlin). A call whose public outcome differs between hash seeds
keeps every observed outcome. Each case stores its source table once. The bank
is replayed natively by minera_pandas_sequence_filter_bank_parity_tests.

--domain names another domain module in scripts/ (round CA: the
*_sequence_filter_typed_domain.py modules give the typed-key banks). A call
with no public outcome (an unrepresentable value) is counted, not stored.
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
BANK_SEED = 20261027


DOMAIN = "pandas_sequence_filter_domain.py"


def domain(name=None):
    spec = importlib.util.spec_from_file_location("pandas_sequence_filter_domain", HERE / (name or DOMAIN))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run_cell(count, bank_seed, start, stop, name=None):
    warnings.filterwarnings("ignore")
    d = domain(name)
    return [[d.run_call(case, call) for call in case["calls"]]
            for case in d.generate_cases(count, bank_seed)[start:stop]]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--count", type=int, default=3000)
    parser.add_argument("--bank-seed", type=int, default=BANK_SEED)
    parser.add_argument("--hash-seeds", default="0,1,2,3")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--cell", nargs=4, type=int, help=argparse.SUPPRESS)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--domain", default=DOMAIN,
                        help="domain module in scripts/ (e.g. pandas_sequence_filter_typed_domain.py)")
    args = parser.parse_args()
    if args.cell:
        json.dump(run_cell(*args.cell, args.domain), sys.stdout)
        return
    seeds = [int(s) for s in args.hash_seeds.split(",")]
    chunk = max(1, args.count // (args.jobs * 2))
    ranges = [(a, min(a + chunk, args.count)) for a in range(0, args.count, chunk)]

    def cell(seed, lo, hi):
        env = dict(os.environ, PYTHONHASHSEED=str(seed), TZ="Europe/Berlin")
        out = subprocess.run([sys.executable, "-X", "utf8", __file__, "--cell", str(args.count),
                              str(args.bank_seed), str(lo), str(hi), "--domain", args.domain],
                             env=env, check=True, capture_output=True, text=True).stdout
        return json.loads(out)

    with ThreadPoolExecutor(args.jobs) as pool:
        futures = {seed: [pool.submit(cell, seed, lo, hi) for lo, hi in ranges] for seed in seeds}
        per_seed = {seed: [row for f in fs for row in f.result()] for seed, fs in futures.items()}
    d = domain(args.domain)
    cases = []
    stats = {"cases": args.count, "calls": 0, "values": 0, "errors": 0, "unrepresentable": 0,
             "varying": 0, "source_changed": 0}
    for index, case in enumerate(d.generate_cases(args.count, args.bank_seed)):
        out_calls = []
        for c, call in enumerate(case["calls"]):
            observed = []
            for seed in seeds:
                outcome = per_seed[seed][index][c]
                if outcome not in observed:
                    observed.append(outcome)
            first = observed[0]
            stats["calls"] += 1
            kind = "values" if "value" in first else "errors" if "error" in first else "unrepresentable"
            stats[kind] += 1
            if all("value" not in o and "error" not in o for o in observed):
                continue  # no public outcome to replay (e.g. a dict keyed by pd.NA)
            stats["source_changed"] += 1 if first.get("source_changed") else 0
            if len(observed) > 1:
                stats["varying"] += 1
                call = dict(call, observed=observed)
            else:
                call = dict(call, outcome=first)
            out_calls.append(call)
        cases.append({"source": d.source_json(case), "calls": out_calls})
    bank = {"generator": "scripts/" + args.domain, "bank_seed": args.bank_seed,
            "hash_seeds": seeds, "timezone": "Europe/Berlin", "stats": stats, "cases": cases}
    data = json.dumps(bank, sort_keys=True, ensure_ascii=True, separators=(",", ":")).encode()
    with open(args.output, "wb") as out:
        with gzip.GzipFile(fileobj=out, mode="wb", mtime=0) as gz:
            gz.write(data)
    print(json.dumps(stats))


if __name__ == "__main__":
    main()
