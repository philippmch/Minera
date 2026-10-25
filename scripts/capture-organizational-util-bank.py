"""Capture the public bank for algo.organizational_mining.util.

Seeded EventLogs and DataFrames (activities, resources and groups that are
missing or None/NaN, custom keys, shuffled rows) go through the public
get_groups_from_log, get_res_act_from_log and get_resources_from_log in fresh
interpreters under three hash seeds, which must agree. The bank is a gzip
text file of tagged lines read by minera_organizational_util_parity_tests:

  C name kind custom          kind log or df; custom 1 uses activity/resource/group keys
  V trace act res grp         a log event ("-" missing, "!" None)
  R act res grp               a DataFrame row in frame order ("!" NaN)
  G|A|S json                  groups, (res->act, act->res), resources;
                              dicts as [key, value] lists, a tuple as {"t": [...]}
  G!|A!|S! type message       a raised exception
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
def gen(r):
    n_cases=r.randint(0,4); traces=[]
    for c in range(n_cases):
        evs=[]
        for k in range(r.randint(0,5)):
            e={}
            if r.random()<0.92: e["act"]=r.choice(["a","b","c","d"])
            roll=r.random()
            if roll<0.8: e["res"]=r.choice(["r1","r2","r3","r10"])
            elif roll<0.9: e["res"]=None
            roll=r.random()
            if roll<0.6: e["grp"]=r.choice(["g1","g2","g3"])
            elif roll<0.75: e["grp"]=None
            evs.append(e)
        traces.append(evs)
    custom=r.random()<0.2
    return {"traces":traces,"custom":custom,"kind":r.choice(["log","df"]),"shuffle":r.random()<0.3,"seed":r.randrange(1<<30)}
def keys(case):
    return ("activity","resource","group") if case["custom"] else ("concept:name","org:resource","org:group")
def build(case):
    import pandas as pd
    from pm4py.objects.log.obj import EventLog, Trace, Event
    ak,rk,gk=keys(case)
    if case["kind"]=="log":
        log=EventLog()
        for i,evs in enumerate(case["traces"]):
            t=Trace(attributes={"concept:name":str(i)})
            for e in evs:
                d={}
                if "act" in e: d[ak]=e["act"]
                if "res" in e: d[rk]=e["res"]
                if "grp" in e: d[gk]=e["grp"]
                t.append(Event(d))
            log.append(t)
        return log
    rows=[]
    for i,evs in enumerate(case["traces"]):
        for e in evs:
            rows.append({"case:concept:name":str(i),ak:e.get("act"),rk:e.get("res"),gk:e.get("grp")})
    df=pd.DataFrame(rows,columns=["case:concept:name",ak,rk,gk])
    for c in df.columns: df[c]=df[c].astype("str")  if False else df[c]
    if case["shuffle"] and len(df): df=df.sample(frac=1,random_state=case["seed"]%(1<<31)).reset_index(drop=True)
    return df
def enc(v):
    if isinstance(v,dict): return [[enc(k),enc(x)] for k,x in v.items()]
    if isinstance(v,tuple): return {"t":[enc(x) for x in v]}
    if isinstance(v,float) and v!=v: return "NaN"
    if hasattr(v,"item"): v=v.item()
    return v
def run(case):
    from pm4py.algo.organizational_mining import util as u
    ak,rk,gk=keys(case)
    params={u.Parameters.ACTIVITY_KEY:ak,u.Parameters.RESOURCE_KEY:rk,u.Parameters.GROUP_KEY:gk} if case["custom"] else None
    out={}
    for f in ("get_groups_from_log","get_res_act_from_log","get_resources_from_log"):
        try: out[f]=enc(getattr(u,f)(build(case),parameters=params))
        except Exception as e: out[f]={"error":[type(e).__name__,str(e)]}
    return out


def lines(index, case, outcome):
    out = [f"C case{index} {case['kind']} {int(case['custom'])}"]
    if case["kind"] == "log":
        for t, evs in enumerate(case["traces"]):
            for e in evs:
                out.append("V " + str(t) + " " + " ".join(
                    "-" if k not in e else "!" if e[k] is None else e[k] for k in ("act", "res", "grp")))
    else:
        df = build(case)
        ak, rk, gk = keys(case)
        for _, row in df.iterrows():
            out.append("R " + " ".join("!" if not isinstance(row[k], str) else row[k] for k in (ak, rk, gk)))
    for tag, name in (("G", "get_groups_from_log"), ("A", "get_res_act_from_log"), ("S", "get_resources_from_log")):
        value = outcome[name]
        if isinstance(value, dict) and "error" in value:
            out.append(tag + "! " + value["error"][0] + " " + value["error"][1])
        else:
            out.append(tag + " " + json.dumps(value, separators=(",", ":")))
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--count", type=int, default=4000)
    parser.add_argument("--seed", type=int, default=BANK_SEED)
    parser.add_argument("--hash-seeds", default="0,1,2")
    parser.add_argument("--cell", nargs=2, type=int, help=argparse.SUPPRESS)
    parser.add_argument("--output", type=Path, default=HERE.parent / "docs" / "organizational-util-bank.txt.gz")
    args = parser.parse_args()
    if args.cell:
        warnings.filterwarnings("ignore")
        r = random.Random(args.cell[1])
        json.dump([run(gen(r)) for _ in range(args.cell[0])], sys.stdout)
        return
    per_seed = []
    for seed in (int(s) for s in args.hash_seeds.split(",")):
        env = dict(os.environ, PYTHONHASHSEED=str(seed))
        out = subprocess.run([sys.executable, "-X", "utf8", __file__, "--cell", str(args.count), str(args.seed)],
                             env=env, check=True, capture_output=True, text=True).stdout
        per_seed.append(json.loads(out))
    if any(results != per_seed[0] for results in per_seed[1:]):
        raise SystemExit("hash seeds disagree")
    r = random.Random(args.seed)
    text = []
    for index, outcome in enumerate(per_seed[0]):
        text += lines(index, gen(r), outcome)
    with open(args.output, "wb") as handle:
        with gzip.GzipFile(fileobj=handle, mode="wb", mtime=0) as gz:
            gz.write(("\n".join(text) + "\n").encode())
    print(json.dumps({"cases": args.count}))


if __name__ == "__main__":
    main()
