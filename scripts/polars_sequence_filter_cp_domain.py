"""Round CP: the Polars sequence filter domain with list case keys.

Loads scripts/polars_sequence_filter_ck_domain.py. In about half of the calls
that take a parameters dict, the case id key becomes a list or tuple of
0-3 items: the frame's own case, activity and other column names, a missing
name, and non-str values (ints, floats, bools, None, dicts, lists) that
Polars reads as literals. About a quarter of the frames are cut to one row
(the sort by a list literal then passes) and a few to none."""
import importlib.util
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("polars_sequence_filter_ck_domain",
                                               HERE / "polars_sequence_filter_ck_domain.py")
ck = importlib.util.module_from_spec(_spec)
sys.modules["polars_sequence_filter_ck_domain"] = ck
_spec.loader.exec_module(ck)
codec = ck.codec
S, I, F, B, N = codec.S, codec.I, codec.F, codec.B, codec.N

CASE_PARAMS = ("pm4py:param:case_id_key", "enum:CASE_ID_KEY")


def key_item(r, spec):
    names = spec["names"]
    pick = r.random()
    if pick < 0.3:
        return S(names["case"])
    if pick < 0.38:
        return S(names["act"])
    if pick < 0.44:
        return S(r.choice(("rd", names["ts"])))
    if pick < 0.5:
        return S("missing")
    if pick < 0.6:
        return I(r.choice((1, 0, -1, 40)))
    if pick < 0.66:
        return N()
    if pick < 0.7:
        return F(r.choice((1.5, 0.0)))
    if pick < 0.74:
        return B(r.random() < 0.5)
    if pick < 0.9:
        return ck.dict_node(r)
    if pick < 0.95:
        return {"l": [I(1), I(2)] if r.random() < 0.6 else [S("a")]}
    return {"m": []}


def list_key(r, spec):
    items = [key_item(r, spec) for _ in range(r.choice((0, 1, 1, 1, 2, 2, 3)))]
    return {"t" if r.random() < 0.1 else "l": items}


def with_list_key(r, spec, call):
    args = [dict(a) for a in call["args"]]
    if not args or not ("m" in args[-1] or "n" in args[-1]):
        return call
    entries = [list(e) for e in args[-1].get("m", [])]
    at = [k for k, e in enumerate(entries) if e[0].get("s") in CASE_PARAMS]
    if at and r.random() < 0.85:
        entries[r.choice(at)][1] = list_key(r, spec)
    else:
        entries.append([S(r.choice(CASE_PARAMS)), list_key(r, spec)])
    args[-1] = {"m": entries}
    return dict(call, args=args)


def cut(spec, n):
    spec = dict(spec)
    for k in ("cases", "acts", "hours"):
        spec[k] = spec[k][:n]
    if spec.get("extra") is not None:
        spec["extra"] = spec["extra"][:n]
    if isinstance(spec.get("index"), list):
        spec["index"] = spec["index"][:n]
    spec["n"] = n
    return spec


_ck_generate = ck.generate_cases


def generate_cases(count, bank_seed):
    cases = _ck_generate(count, bank_seed)
    rng = random.Random(bank_seed ^ 0xC9)
    for case in cases:
        r = random.Random(rng.randrange(1 << 30))
        roll = r.random()
        if case["spec"]["n"] > 1 and roll < 0.25:
            case["spec"] = cut(case["spec"], 1)
        elif case["spec"]["n"] > 0 and roll < 0.3:
            case["spec"] = cut(case["spec"], 0)
        case["calls"] = [with_list_key(r, case["spec"], c) if r.random() < 0.5 else c for c in case["calls"]]
    return cases


for _k in dir(ck):
    if not _k.startswith("__"):
        globals().setdefault(_k, getattr(ck, _k))
globals()["generate_cases"] = generate_cases
