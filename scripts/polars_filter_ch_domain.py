"""Round CH: the Polars table filter domain for Polars' Float to String cast
and nested dict fields of the timeframe filter.

Builds the frames of scripts/polars_filter_cg_domain.py, but float activity
and case columns hold odd values (1e-5, 1e-7, 1e16, 0.1 on Float32, -0.0,
1/3, NaN, infinities, subnormals), where Polars' cast to String (paths
labels, variant strings, between case ids, struct casts of a column) and
Python's repr differ; sometimes the extra column "zz" is a String column of
struct texts ("{0.00001}", "{[1, 2]}", "{{1}}", "}"). Each case gets:

- the calls of the CF domain (every table filter with typed, edge and odd
  values; activity values of the calls are the column's odd floats);
- timestamp.filter_traces_attribute_in_timeframe with dict values whose
  fields hold odd floats, the strs Polars casts them to, lists (nested,
  empty, with None) and nested dicts (empty, repeated keys, two levels),
  on every column."""
import datetime as _dt
import importlib.util
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("polars_filter_cg_domain", HERE / "polars_filter_cg_domain.py")
cg = importlib.util.module_from_spec(_spec)
sys.modules["polars_filter_cg_domain"] = cg
_spec.loader.exec_module(cg)
cf = cg.cf
ce = cf.ce
cc = ce.cc
base = cc.base
codec = cc.codec
S, I, F, B, N = codec.S, codec.I, codec.F, codec.B, codec.N
L, T = cf.L, cf.T

# Activity codes (cc._code: a=1 ... z=8, others 40) to odd floats per kind.
ODD = {
    "Float64": {1: 1e-5, 2: 0.1, 3: 1e16, 4: 1e-7, 5: -0.0, 6: 1 / 3, 7: 2.5e-6, 8: 1.5e16, 40: 1.2345678901234568e17},
    "Float32": {1: 0.1, 2: 1e-5, 3: 1e15, 4: 3.3e-7, 5: 1 / 3, 6: 1e-6, 7: 16777217.0, 8: -0.0, 40: 1e13},
    "Float64x": {1: float("nan"), 2: float("inf"), 3: float("-inf"), 4: 1e-5, 5: 5e-324, 6: 1.7976931348623157e308,
                 7: 1e22, 8: 9.999999999999999e-6, 40: 0.30000000000000004},
}
# Case ids (1, 2, 3, 4, 9 and others) to odd floats.
CASE_ODD = {1: 1e-5, 2: 0.1, 3: 1e16, 4: 1e-7, 9: 2.5e-6}
FLOAT_KINDS = list(ODD)
# Polars' String casts of the odd floats (and Python's reprs, which differ).
CAST_STRS = ("0.00001", "1e-7", "1e+16", "0.1", "-0.0", "0.3333333333333333", "1e-05", "1e-07", "1e16", "0.33333334",
             "2.5e-6", "1000000000000000.0", "1e+15", "NaN", "inf", "1.5e+16", "16777216.0")
STRUCT_TEXTS = ("{0.00001}", "{1.0000e16}", "{[1, 2]}", "{{1}}", '{"a"}', "}", "{null}", "{0.1}", "{[0.00001]}",
                "{{1,2.5}}", "{1,{1}}")

_cc_act_value = cc.act_value
_cc_act_dtype = cc._act_dtype
_cc_case_series = cc.case_series
_cf_case_typed = cf.case_typed
_cg_frame_spec = cg.frame_spec
_cg_build_frame = cg.build_frame
_cg_columns = cg.columns


def act_value(kind, c):
    if kind in ODD:
        return None if c is None else ODD[kind].get(c, ODD[kind][40])
    return _cc_act_value(kind, c)


_cc_value_arg = cc._value_arg


def _value_arg(r, kind, text, mode):
    """An odd float as the call's value (non-finite ones never as an int)."""
    if kind in ODD:
        if text not in cc.ORDER or (mode == "mixed" and r.random() < 0.3):
            return {"s": text}
        value = act_value(kind, cc._code(text))
        # Ints past the UInt64 range stay floats (the native transport stops there).
        if value != value or abs(value) >= 2.0 ** 63 or r.random() < 0.7:
            return {"f": codec.fbits(value)}
        return {"i": str(int(value))}
    return _cc_value_arg(r, kind, text, mode)


def _act_dtype(pl, kind):
    return pl.Float64 if kind == "Float64x" else _cc_act_dtype(pl, kind)


def case_series(spec, pl, name):
    kind, cases = spec["case_kind"], spec["cases"]
    if kind in ("Float32", "Float64"):
        return pl.Series(name, [None if c is None else CASE_ODD.get(c, 1e-7 * c) for c in cases],
                         dtype=getattr(pl, kind))
    return _cc_case_series(spec, pl, name)


def case_typed(r, spec):
    if spec["case_kind"] in ("Float32", "Float64"):
        return F(CASE_ODD[r.choice([1, 2, 3, 4, 9])])
    return _cf_case_typed(r, spec)


def frame_spec(r):
    spec = _cg_frame_spec(r)
    if r.random() < 0.6:
        spec["act_kind"] = r.choice(FLOAT_KINDS)
    if r.random() < 0.35:
        spec["case_kind"] = r.choice(("Float32", "Float64"))
        if spec["n"] and r.random() < 0.2:
            spec["cases"][r.randrange(spec["n"])] = None
    spec["structs"] = not spec.get("late") and r.random() < 0.4
    return spec


def build_frame(spec):
    df = _cg_build_frame(spec)
    if spec.get("structs"):
        import polars as pl
        texts = [STRUCT_TEXTS[(i * 7 + spec["n"]) % len(STRUCT_TEXTS)] for i in range(df.height)]
        df = df.with_columns(pl.Series("zz", texts, dtype=pl.String))
    return df


def columns(spec):
    out = _cg_columns(spec)
    if spec.get("structs"):
        out += ["zz", "zz"]
    return out


def odd_float(r):
    pool = [v for table in ODD.values() for v in table.values()] + list(CASE_ODD.values())
    return F(r.choice(pool))


def list_field(r, depth):
    items = []
    for _ in range(r.choice((0, 1, 2, 2, 3, 5))):
        pick = r.random()
        if pick < 0.35:
            items.append(odd_float(r))
        elif pick < 0.55:
            items.append(I(r.choice((1, 2, -1, 2 ** 63))))
        elif pick < 0.65:
            items.append(N())
        elif pick < 0.75 and depth < 2:
            items.append(list_field(r, depth + 1))
        elif pick < 0.85:
            items.append(S(r.choice(("a", "0.00001"))))
        else:
            items.append(B(r.random() < 0.5))
    return L(*items) if r.random() < 0.85 else T(*items)


def nested_dict(r, spec, column, depth):
    keys = ("j", "i", "a")
    fields = []
    for _ in range(r.choice((0, 1, 1, 2, 2))):
        fields.append([S(r.choice(keys)), field_value(r, spec, column, depth + 1)])
    return {"m": fields}


def field_value(r, spec, column, depth=0):
    pick = r.random()
    if pick < 0.25:
        return odd_float(r)
    if pick < 0.38:
        return S(r.choice(CAST_STRS))
    if pick < 0.5 and depth < 2:
        return nested_dict(r, spec, column, depth)
    if pick < 0.58:
        return list_field(r, depth)
    if pick < 0.64:
        return N()
    if column == "zz" and spec.get("structs"):
        return S(r.choice(("a", "1", "0.00001")))
    return cg_field_value(r, spec, column)


def cg_field_value(r, spec, column):
    names = spec["names"]
    late = spec.get("late")
    source = names.get(late, late) if column == "zz" and late else column
    pick = r.random()
    if pick < 0.6:
        return cf.typed(r, spec, source)
    if pick < 0.8:
        return ce.edge_value(r)
    if pick < 0.9:
        return N()
    return S(r.choice(cg.STRS))


def dict_value(r, spec, column):
    pick = r.random()
    keys = ("a", "b", "r1")
    if pick < 0.06:
        return {"m": []}
    if pick < 0.65:
        return {"m": [[S(r.choice(keys)), field_value(r, spec, column)]]}
    if pick < 0.9:
        a, b = r.sample(keys, 2)
        return {"m": [[S(a), field_value(r, spec, column)], [S(b), field_value(r, spec, column)]]}
    k = r.choice(keys)
    return {"m": [[S(k), field_value(r, spec, column)], [S(k), field_value(r, spec, column)]]}


def timeframe_calls(r, spec):
    names = spec["names"]
    calls = []
    cols = columns(spec)
    for _ in range(r.choice((1, 2, 2, 3))):
        column = r.choice(cols)
        entries = cf.case_entries(r, spec, with_ts=True)
        value = dict_value(r, spec, column) if r.random() < 0.85 else cf.timeframe_value(r, spec, names["act"])
        lo, hi = cg.window(r)
        calls.append({"function": "timestamp.filter_traces_attribute_in_timeframe",
                      "args": [S(column), value, lo, hi, {"m": entries}], "kwargs": []})
    return calls


cc.act_value = act_value
cc._act_dtype = _act_dtype
cc._value_arg = _value_arg
cc.case_series = case_series
cf.case_typed = case_typed
base.frame_spec = frame_spec
base.build_frame = build_frame
_cf_generate = cf.generate_cases


def generate_cases(count, bank_seed):
    cases = _cf_generate(count, bank_seed)
    rng = random.Random(bank_seed ^ 0xC8)
    for case in cases:
        r = random.Random(rng.randrange(1 << 30))
        case["calls"] += timeframe_calls(r, case["spec"])
    return cases


run_call = cg.run_call
for _k in dir(cg):
    if not _k.startswith("__"):
        globals().setdefault(_k, getattr(cg, _k))
