"""Round CN: pandas eventually_follows with list time bounds (scratch domain)."""
import importlib.util, random, sys
from pathlib import Path
HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("pandas_filter_domain", HERE / "pandas_filter_domain.py")
base = importlib.util.module_from_spec(_spec)
sys.modules["pandas_filter_domain"] = base
_spec.loader.exec_module(base)
codec = base.codec
S, I, F, B, N = codec.S, codec.I, codec.F, codec.B, codec.N
L, T = base.L, base.T

def item(r):
    roll = r.random()
    if roll < 0.55: return r.choice((I(0), I(60), I(1800), I(3600), F(5400.0), I(100000), B(True)))
    if roll < 0.7: return {"m": [[S("b"), I(1)]]}
    if roll < 0.8: return N()
    if roll < 0.9: return S("x")
    return L(I(1))

def bound(r):
    roll = r.random()
    if roll < 0.45: return r.choice((I(0), I(60), I(3600), I(100000)))
    n = r.choice((0, 1, 1, 2, 2, 3, 4))
    return L(*[item(r) for _ in range(n)])

def generate_cases(count, bank_seed):
    cases = base.generate_cases(count, bank_seed)
    rng = random.Random(bank_seed ^ 0xC7)
    for case in cases:
        r = random.Random(rng.randrange(1 << 30))
        names = {"case": "case:concept:name", "act": "concept:name", "ts": "time:timestamp"}
        if case["spec"].get("names"): names = case["spec"]["names"]
        acts = [a for a in case["spec"]["acts"] if a is not None] or ["a"]
        calls = []
        for _ in range(6):
            flow = [S(r.choice(acts)) for _ in range(r.choice((2, 2, 3, 3, 4)))]
            bounds = [T(bound(r), bound(r)) for _ in range(len(flow) - 1)]
            params = [[S("enum:TIMESTAMP_DIFF_BOUNDARIES"), L(*bounds)]]
            if names["case"] != "case:concept:name":
                params += [[S("enum:CASE_ID_KEY"), S(names["case"])], [S("enum:ATTRIBUTE_KEY"), S(names["act"])], [S("enum:TIMESTAMP_KEY"), S(names["ts"])]]
            calls.append({"function": "ltl.eventually_follows", "args": [L(*flow), {"m": params}], "kwargs": []})
        case["calls"] = calls
    return cases

for _k in dir(base):
    if not _k.startswith("__"):
        globals().setdefault(_k, getattr(base, _k))
globals()["generate_cases"] = generate_cases
