"""Capture the public bank for algo.discovery.ocel.interleavings.utils.merge_dataframe_rel_cases.

Seeded DataFrames (str/int case ids, null ids and activities, NaT, hour ties,
UTC, Europe/Berlin and naive timestamps in us/ns, extra int/float/str/bool
columns, an existing index column, custom and shuffled indexes, custom keys,
INDEX_KEY and suffix parameters) and case-relation tables (unknown, null and
duplicate pairs, an extra column, a missing key column) go through the public
directly_follows_dataframe and merge_dataframes in fresh interpreters under
three hash seeds, which must agree. Every input and result travels as the
native event-table JSON. The bank is a gzip text file of tagged lines read by
minera_interleavings_merge_parity_tests:

  C name kind          one case, kind df or merge
  K PARAMETER value    a Parameters member
  L/R/P json           left, right and relations tables
  O json | E type msg  the reference result table or exception
"""
import argparse
import gzip
import importlib.util
import json
import os
import random
import subprocess
import sys
import warnings
from pathlib import Path

HERE = Path(__file__).resolve().parent
BANK_SEED = 20261025
spec = importlib.util.spec_from_file_location("repetition_domain", HERE / "repetition_domain.py")
rep = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rep)

EXTRA = {"n": "int64", "f": "float64", "s": "str", "b": "bool"}

def frame_spec(r, names, case_kind, pool_cases, tz):
    n = r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9])
    cases = [r.choice(pool_cases) for _ in range(n)]
    if case_kind == "str" and n and r.random() < 0.1:
        cases[r.randrange(n)] = None
    acts = [r.choice(["a", "b", "c", "d"]) for _ in range(n)]
    if n and r.random() < 0.1:
        acts[r.randrange(n)] = None
    hours = [r.randint(0, 6) for _ in range(n)]
    nat = [n and r.random() < 0.05 for _ in range(n)]
    extras = [k for k in EXTRA if r.random() < 0.3]
    values = {}
    for k in extras:
        if k == "n": values[k] = [r.randint(-3, 9) for _ in range(n)]
        if k == "f": values[k] = [r.choice([0.5, 2.0, None]) for _ in range(n)]
        if k == "s": values[k] = [r.choice(["u", "v", None]) for _ in range(n)]
        if k == "b": values[k] = [r.random() < 0.5 for _ in range(n)]
    order = [names["case"], names["act"], names["ts"]] + extras
    if r.random() < 0.08: order.append("@@index"); values["@@index"] = [r.randint(0, 5) for _ in range(n)]
    r.shuffle(order)
    roll = r.random()
    index = None
    if roll < 0.2: index = [r.randint(0, 30) for _ in range(n)]
    elif roll < 0.35: index = list(range(n)); r.shuffle(index)
    return {"n": n, "case_kind": case_kind, "cases": cases, "acts": acts, "hours": hours, "nat": nat,
            "values": values, "order": order, "names": names, "index": index, "tz": tz,
            "unit": r.choice(["us", "us", "ns"])}

def build(spec):
    import numpy as np
    import pandas as pd
    names = spec["names"]; n = spec["n"]
    data = {}
    if spec["case_kind"] == "int":
        data[names["case"]] = pd.Series(spec["cases"], dtype="int64")
    else:
        data[names["case"]] = pd.Series(spec["cases"], dtype="str")
    data[names["act"]] = pd.Series(spec["acts"], dtype="str")
    tz = spec["tz"]
    base = pd.Timestamp("2020-01-01", tz=tz) if tz else pd.Timestamp("2020-01-01")
    stamps = [pd.NaT if spec["nat"][i] else base + pd.Timedelta(hours=spec["hours"][i]) for i in range(n)]
    dt = f"datetime64[{spec['unit']}, {tz}]" if tz else f"datetime64[{spec['unit']}]"
    data[names["ts"]] = pd.Series(stamps, dtype=dt)
    for k, v in spec["values"].items():
        if k == "@@index": data[k] = pd.Series(v, dtype="int64")
        elif k == "f": data[k] = pd.Series([np.nan if x is None else x for x in v], dtype="float64")
        else: data[k] = pd.Series(v, dtype=EXTRA[k])
    df = pd.DataFrame({k: data[k] for k in spec["order"]})
    if spec["index"] is not None:
        df.index = pd.Index(spec["index"], dtype="int64")
    return df

def gen_case(r, kind):
    custom = r.random() < 0.15
    names = {"case": "cid", "act": "act", "ts": "ts"} if custom else {"case": "case:concept:name", "act": "concept:name", "ts": "time:timestamp"}
    case_kind = "int" if r.random() < 0.25 else "str"
    roll = r.random()
    tz = "UTC" if roll < 0.9 else ("Europe/Berlin" if roll < 0.95 else None)
    pool_l = [1, 2, 3] if case_kind == "int" else ["A", "B", "C", "10"]
    params = {}
    if custom:
        params = {"CASE_ID_KEY": "cid", "ACTIVITY_KEY": "act", "TIMESTAMP_KEY": "ts"}
    if r.random() < 0.1: params["INDEX_KEY"] = r.choice(["IDX", "@@index", "x"])
    case = {"kind": kind, "params": params, "left": frame_spec(r, names, case_kind, pool_l, tz)}
    if kind == "merge":
        pool_r = [4, 5, 2] if case_kind == "int" else ["X", "Y", "A", "Z"]
        case["right"] = frame_spec(r, names, case_kind, pool_r, tz)
        ls, rs = "_LEFT", "_RIGHT"
        if r.random() < 0.15:
            ls, rs = r.choice([("_L", "_R"), ("_l", "_r"), ("@l", "@r")])
            params["LEFT_SUFFIX"] = ls; params["RIGHT_SUFFIX"] = rs
        m = r.choice([0, 1, 2, 3, 4, 5, 6])
        pairs = [[r.choice(pool_l + ([None] if case_kind == "str" and r.random() < 0.1 else [])), r.choice(pool_r)] for _ in range(m)]
        cols = [names["case"] + ls, names["case"] + rs]
        extra = r.random() < 0.25
        if extra: cols.append("w")
        r.shuffle(cols)
        if r.random() < 0.04:
            cols.remove(r.choice([names["case"] + ls, names["case"] + rs]))
        case["relations"] = {"pairs": pairs, "cols": cols, "case_kind": case_kind, "lcol": names["case"] + ls, "rcol": names["case"] + rs,
                             "w": [r.randint(0, 9) for _ in range(m)] if extra else None}
    return case

def build_rel(rel):
    import pandas as pd
    d = {}
    for c in rel["cols"]:
        if c == "w": d[c] = pd.Series(rel["w"], dtype="int64")
        else:
            vals = [p[0] if c == rel["lcol"] else p[1] for p in rel["pairs"]]
            d[c] = pd.Series(vals, dtype="int64" if rel["case_kind"] == "int" else "str")
    return pd.DataFrame(d)

def run(case):
    from pm4py.algo.discovery.ocel.interleavings.utils import merge_dataframe_rel_cases as m
    params = {getattr(m.Parameters, k): v for k, v in case["params"].items()}
    try:
        if case["kind"] == "df":
            out = m.directly_follows_dataframe(build(case["left"]), parameters=params or None)
        else:
            out = m.merge_dataframes(build(case["left"]), build(case["right"]), build_rel(case["relations"]), parameters=params or None)
    except Exception as e:
        return {"error": [type(e).__name__, str(e)]}
    return {"table": rep.pandas_table_json(out)}


def cases(count, seed):
    r = random.Random(seed)
    return [gen_case(r, "df" if i % 2 == 0 else "merge") for i in range(count)]


def run_cell(count, seed, start, stop):
    warnings.filterwarnings("ignore")
    return [run(case) for case in cases(count, seed)[start:stop]]


def lines(index, case, result):
    out = [f"C case{index} {case['kind']}"]
    out += [f"K {key} {value}" for key, value in case["params"].items()]
    out.append("L " + json.dumps(rep.pandas_table_json(build(case["left"])), ensure_ascii=False))
    if case["kind"] == "merge":
        out.append("R " + json.dumps(rep.pandas_table_json(build(case["right"])), ensure_ascii=False))
        out.append("P " + json.dumps(rep.pandas_table_json(build_rel(case["relations"])), ensure_ascii=False))
    if "error" in result:
        out.append("E " + result["error"][0] + " " + result["error"][1])
    else:
        out.append("O " + json.dumps(result["table"], ensure_ascii=False))
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--count", type=int, default=6000)
    parser.add_argument("--seed", type=int, default=BANK_SEED)
    parser.add_argument("--hash-seeds", default="0,1,2")
    parser.add_argument("--cell", nargs=4, type=int, help=argparse.SUPPRESS)
    parser.add_argument("--output", type=Path, default=HERE.parent / "docs" / "interleavings-merge-bank.txt.gz")
    args = parser.parse_args()
    if args.cell:
        json.dump(run_cell(*args.cell), sys.stdout)
        return
    seeds = [int(s) for s in args.hash_seeds.split(",")]
    per_seed = []
    for seed in seeds:
        env = dict(os.environ, PYTHONHASHSEED=str(seed), TZ="Europe/Berlin")
        out = subprocess.run([sys.executable, "-X", "utf8", __file__, "--cell", str(args.count), str(args.seed),
                              "0", str(args.count)], env=env, check=True, capture_output=True, text=True).stdout
        per_seed.append(json.loads(out))
    if any(results != per_seed[0] for results in per_seed[1:]):
        raise SystemExit("hash seeds disagree")
    text = []
    errors = 0
    for index, (case, result) in enumerate(zip(cases(args.count, args.seed), per_seed[0])):
        errors += "error" in result
        text += lines(index, case, result)
    with open(args.output, "wb") as handle:
        with gzip.GzipFile(fileobj=handle, mode="wb", mtime=0) as gz:
            gz.write(("\n".join(text) + "\n").encode())
    print(json.dumps({"cases": args.count, "errors": errors, "hash_seeds": seeds}))


if __name__ == "__main__":
    main()
