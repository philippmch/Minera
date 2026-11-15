"""Round CK: the Polars table filter domain with dicts as column keys and
more mixed list shapes.

Builds the cases of scripts/polars_filter_cj_domain.py. In about 45% of the
cases some calls get a dict (or a list holding a dict) where a column name
goes: the case id, timestamp, attribute and activity key parameters
(CASE_ID_KEY values), or the case_id_glue, attribute_key, timestamp_key
and activity_key keyword arguments. The attribute value calls (apply,
apply_events, filter_df_on_attribute_values, the timeframe filter) also
get lists that start with a dict and continue with a plain list or a
scalar ([{"a": 1}, [-1], 5] and its relatives)."""
import importlib.util
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("polars_filter_cj_domain", HERE / "polars_filter_cj_domain.py")
cj = importlib.util.module_from_spec(_spec)
sys.modules["polars_filter_cj_domain"] = cj
_spec.loader.exec_module(cj)
ci = cj.ci
cf = cj.cf
codec = cj.codec
S, I, F, B, N = codec.S, codec.I, codec.F, codec.B, codec.N
L, T = cj.L, cj.T

KEY_PARAMS = ("pm4py:param:case_id_key", "enum:CASE_ID_KEY", "pm4py:param:timestamp_key", "enum:TIMESTAMP_KEY",
              "pm4py:param:attribute_key", "enum:ATTRIBUTE_KEY", "pm4py:param:activity_key", "enum:ACTIVITY_KEY")
KEY_KWARGS = ("case_id_glue", "attribute_key", "timestamp_key", "activity_key", "case_id_key")


def key_dict(r, spec):
    names = spec["names"]
    pick = r.random()
    if pick < 0.5:
        return {"m": [[S(r.choice(("a", names["case"], "x"))), r.choice((I(1), S(names["case"]), S("a"), N()))]]}
    if pick < 0.65:
        return {"m": []}
    if pick < 0.8:
        return L(cj.small_dict(r, spec, names["act"]))
    if pick < 0.9:
        return L(S(names["case"]), cj.small_dict(r, spec, names["act"]))
    return cj.small_dict(r, spec, names["act"])


def dict_keys(r, spec, call):
    """The call with one column key made a dict."""
    args = [dict(a) for a in call["args"]]
    kwargs = [list(k) for k in call["kwargs"]]
    maps = [i for i, a in enumerate(args) if "m" in a and i == len(args) - 1]
    kw_keys = [i for i, (k, _) in enumerate(kwargs) if k in KEY_KWARGS]
    pick = r.random()
    if maps and (pick < 0.6 or not kw_keys):
        i = maps[0]
        entries = [list(e) for e in args[i]["m"]]
        present = [k for k, (name, _) in enumerate(entries) if name.get("s") in KEY_PARAMS]
        if present and r.random() < 0.6:
            entries[r.choice(present)][1] = key_dict(r, spec)
        else:
            entries.append([S(r.choice(KEY_PARAMS[::2])), key_dict(r, spec)])
        args[i] = {"m": entries}
    elif kw_keys:
        i = r.choice(kw_keys)
        kwargs[i][1] = key_dict(r, spec)
    else:
        return call
    return dict(call, args=args, kwargs=kwargs)


def dict_first(r, spec, column):
    d = cj.small_dict(r, spec, column)
    tails = (
        lambda: [L(I(-1)), I(5)],
        lambda: [L(I(-1))],
        lambda: [I(5), L(I(-1))],
        lambda: [L(S("a")), S("b")],
        lambda: [L(F(1.5)), N()],
        lambda: [L(), I(1)],
        lambda: [L(cj.small_dict(r, spec, column)), I(5)],
        lambda: [T(I(1)), I(2)],
        lambda: [L(N()), B(True)],
    )
    return L(d, *r.choice(tails)())


def mixed_calls(r, spec):
    names = spec["names"]
    calls = []
    for fn in ("apply", "apply_events", "filter_df_on_attribute_values"):
        if r.random() >= 0.35:
            continue
        column = r.choice(cj.typed_columns(spec)) if r.random() < 0.7 else ci.target_column(r, spec)
        v = dict_first(r, spec, column)
        if fn == "filter_df_on_attribute_values":
            kwargs = [["attribute_key", S(column)]]
            if names["case"] != "case:concept:name":
                kwargs.append(["case_id_glue", S(names["case"])])
            calls.append({"function": "attributes." + fn, "args": [v], "kwargs": kwargs})
            continue
        entries = [[S("pm4py:param:attribute_key"), S(column)]] + cf.case_entries(r, spec, missing=0.0)
        calls.append({"function": "attributes." + fn, "args": [v, {"m": entries}], "kwargs": []})
    if r.random() < 0.2:
        column = r.choice(cj.typed_columns(spec))
        lo, hi = cj.cg.window(r)
        calls.append({"function": "timestamp.filter_traces_attribute_in_timeframe",
                      "args": [S(column), dict_first(r, spec, column), lo, hi,
                               {"m": cf.case_entries(r, spec, with_ts=True)}], "kwargs": []})
    return calls


_cj_generate = cj.generate_cases


def generate_cases(count, bank_seed):
    cases = _cj_generate(count, bank_seed)
    rng = random.Random(bank_seed ^ 0xCB)
    for case in cases:
        r = random.Random(rng.randrange(1 << 30))
        spec = case["spec"]
        if r.random() < 0.45:
            case["calls"] = [dict_keys(r, spec, c) if r.random() < 0.15 else c for c in case["calls"]]
        case["calls"] += mixed_calls(r, spec)
    return cases


for _k in dir(cj):
    if not _k.startswith("__"):
        globals().setdefault(_k, getattr(cj, _k))
globals()["generate_cases"] = generate_cases
