"""Round CI: the Polars table filter domain for floats printed as text,
lists of dicts and missing attribute columns.

Builds the frames of scripts/polars_filter_ch_domain.py (odd float activity
and case columns, struct texts in "zz"). Each case gets the calls of the CH
domain and:

- attributes.apply, apply_events and filter_df_on_attribute_values,
  ltl.eventually_follows, A_next_B_next_C, four_eyes_principle and
  attr_value_different_persons, and paths.apply with values that are dicts,
  lists and tuples of dicts (fields with odd floats, nested lists and
  dicts), lists that Polars cannot build into one Series (an odd float next
  to a str, a list next to a float, a dict next to a scalar), nested float
  lists and odd floats, on the activity column, another column or a missing
  one;
- timestamp.filter_traces_attribute_in_timeframe with the same values and
  with dicts whose fields are lists of dicts, on every column and on a
  missing one."""
import datetime as _dt
import importlib.util
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("polars_filter_ch_domain", HERE / "polars_filter_ch_domain.py")
ch = importlib.util.module_from_spec(_spec)
sys.modules["polars_filter_ch_domain"] = ch
_spec.loader.exec_module(ch)
cg = ch.cg
cf = ch.cf
ce = cf.ce
cc = ce.cc
base = cc.base
codec = cc.codec
S, I, F, B, N = codec.S, codec.I, codec.F, codec.B, codec.N
L, T = cf.L, cf.T
UTC = _dt.timezone.utc

# Floats whose Rust Display, Python repr, std::to_chars and Polars' plan
# display differ.
TEXT_FLOATS = (1e16, 1e17, 1e-7, 1e-5, 1e22, 1.5e300, 0.1, 1 / 3, 5e-324, 2.5e-6, 1234567.5, 999999.0, 1e6,
               123456789.0, -0.0, 0.30000000000000004, 9007199254740993.0, 1e15, 1.25e-4, 3e-6)


def text_float(r):
    if r.random() < 0.7:
        return F(r.choice(TEXT_FLOATS))
    return ch.odd_float(r)


def scalar(r, spec, column):
    pick = r.random()
    if pick < 0.4:
        return text_float(r)
    if pick < 0.55:
        return I(r.choice((1, 2, -1, 0, 2 ** 53 + 1, 2 ** 63)))
    if pick < 0.65:
        return N()
    if pick < 0.75:
        return S(r.choice(("a", "b", "0.00001", "1e-7", "x")))
    if pick < 0.82:
        return B(r.random() < 0.5)
    if pick < 0.9:
        return codec.wall(_dt.datetime(2021, 3, 2, 9, tzinfo=UTC if r.random() < 0.5 else None))
    return cg_typed(r, spec, column)


def cg_typed(r, spec, column):
    names = spec["names"]
    cols = cf.columns(names)
    return cf.typed(r, spec, column if column in cols else names["act"])


def struct(r, spec, column, depth=0):
    keys = ("a", "b", "r1")
    fields = []
    for _ in range(r.choice((0, 1, 1, 1, 2, 2, 3))):
        pick = r.random()
        if pick < 0.45:
            value = text_float(r)
        elif pick < 0.6 and depth < 2:
            value = struct(r, spec, column, depth + 1)
        elif pick < 0.7 and depth < 2:
            value = L(*[struct(r, spec, column, depth + 1) for _ in range(r.choice((0, 1, 2)))])
        elif pick < 0.8:
            value = L(*[text_float(r) for _ in range(r.choice((0, 1, 2, 3)))])
        else:
            value = scalar(r, spec, column)
        fields.append([S(r.choice(keys)), value])
    return {"m": fields}


def struct_list(r, spec, column):
    items = []
    for _ in range(r.choice((0, 1, 1, 2, 2, 3, 4))):
        pick = r.random()
        if pick < 0.75:
            items.append(struct(r, spec, column))
        elif pick < 0.85:
            items.append(N())
        else:
            items.append(scalar(r, spec, column))
    return (L if r.random() < 0.8 else T)(*items)


def mixed_list(r, spec, column):
    """Lists Polars cannot build into one Series (and a few it can)."""
    a = text_float(r)
    shapes = (
        lambda: L(a, S(r.choice(("a", "1e-7")))),
        lambda: L(S("a"), a),
        lambda: L(L(a), a),
        lambda: L(a, L(a)),
        lambda: L(N(), a, struct(r, spec, column)),
        lambda: L(struct(r, spec, column), a),
        lambda: L(codec.wall(_dt.datetime(2021, 3, 2, 9)), a),
        lambda: L(a, codec.wall(_dt.datetime(2021, 3, 2, 9))),
        lambda: L(B(True), a),
        lambda: L(a, B(False)),
        lambda: L(I(2 ** 63), a, I(-1)),
        lambda: L(L(a, a), L(text_float(r)), L()),
        lambda: L(L(L(a)), L(L(text_float(r)), L())),
        lambda: T(a, text_float(r), N()),
        lambda: L(S("a"), L(S("b"))),
        lambda: L(I(1), S("a")),
    )
    return r.choice(shapes)()


def value(r, spec, column):
    pick = r.random()
    if pick < 0.3:
        return struct_list(r, spec, column)
    if pick < 0.5:
        return struct(r, spec, column)
    if pick < 0.75:
        return mixed_list(r, spec, column)
    if pick < 0.9:
        return text_float(r)
    return L(*[text_float(r) for _ in range(r.choice((1, 2, 3)))])


def target_column(r, spec):
    names = spec["names"]
    pick = r.random()
    if pick < 0.6:
        return names["act"]
    if pick < 0.75:
        return "missing"
    return r.choice(ch.columns(spec))


def attribute_calls(r, spec):
    names = spec["names"]
    calls = []
    for fn in ("apply", "apply_events", "filter_df_on_attribute_values"):
        if r.random() >= 0.6:
            continue
        column = target_column(r, spec)
        v = value(r, spec, column)
        if r.random() < 0.4:
            v = L(v) if r.random() < 0.7 else L(v, scalar(r, spec, column))
        if fn == "filter_df_on_attribute_values":
            kwargs = [["attribute_key", S(column)]]
            if names["case"] != "case:concept:name":
                kwargs.append(["case_id_glue", S(names["case"])])
            if r.random() < 0.4:
                kwargs.append(["positive", B(r.random() < 0.5)])
            calls.append({"function": "attributes." + fn, "args": [v], "kwargs": kwargs})
            continue
        entries = [[S(r.choice(("pm4py:param:attribute_key", "enum:ATTRIBUTE_KEY"))), S(column)]]
        entries += cf.case_entries(r, spec, missing=0.05)
        cf.positive(r, entries)
        r.shuffle(entries)
        calls.append({"function": "attributes." + fn, "args": [v, {"m": entries}], "kwargs": []})
    return calls


def act(r, spec):
    return cf.act_typed(r, spec)


def ltl_calls(r, spec):
    names = spec["names"]
    calls = []
    for fn in ("eventually_follows", "A_next_B_next_C", "four_eyes_principle", "attr_value_different_persons"):
        if r.random() >= 0.45:
            continue
        column = target_column(r, spec)
        entries = cf.case_entries(r, spec, with_ts=fn == "eventually_follows", missing=0.03)
        if column != "concept:name" or r.random() < 0.2:
            entries.append([S(r.choice(("pm4py:param:attribute_key", "enum:ATTRIBUTE_KEY"))), S(column)])
        if fn in ("four_eyes_principle", "attr_value_different_persons") and r.random() < 0.3:
            entries.append([S("pm4py:param:resource_key"), S(r.choice(("org:resource", "rd:kind")))])
        cf.positive(r, entries)
        r.shuffle(entries)
        v = value(r, spec, column)
        if fn == "attr_value_different_persons":
            calls.append({"function": "ltl." + fn, "args": [v, {"m": entries}], "kwargs": []})
            continue
        size = {"eventually_follows": r.choice((2, 2, 3)), "A_next_B_next_C": 3, "four_eyes_principle": 2}[fn]
        items = [act(r, spec) for _ in range(size)]
        items[r.randrange(size)] = v
        calls.append({"function": "ltl." + fn, "args": [L(*items), {"m": entries}], "kwargs": []})
    return calls


def path_calls(r, spec):
    names = spec["names"]
    calls = []
    if r.random() < 0.35:
        entries = cf.case_entries(r, spec, with_ts=True, missing=0.03)
        if names["act"] != "concept:name":
            entries.append([S("pm4py:param:attribute_key"), S(names["act"])])
        cf.positive(r, entries)
        pairs = [T(act(r, spec), act(r, spec)) for _ in range(r.randint(0, 2))]
        bad = r.choice((T(value(r, spec, names["act"]), act(r, spec)), T(act(r, spec), value(r, spec, names["act"]))))
        pairs.insert(r.randint(0, len(pairs)), bad)
        calls.append({"function": "paths.apply", "args": [L(*pairs), {"m": entries}], "kwargs": []})
    return calls


def timeframe_calls(r, spec):
    calls = []
    cols = ch.columns(spec) + ["missing"]
    for _ in range(r.choice((1, 1, 2, 3))):
        column = r.choice(cols)
        entries = cf.case_entries(r, spec, with_ts=True)
        pick = r.random()
        if pick < 0.35:
            v = struct_list(r, spec, column)
        elif pick < 0.65:
            # A dict with a list of dicts among its fields.
            fields = [[S("a"), struct_list(r, spec, column)]]
            if r.random() < 0.5:
                fields.append([S(r.choice(("b", "r1"))), ch.field_value(r, spec, column)])
            r.shuffle(fields)
            v = {"m": fields}
        else:
            v = value(r, spec, column)
        lo, hi = cg.window(r)
        calls.append({"function": "timestamp.filter_traces_attribute_in_timeframe",
                      "args": [S(column), v, lo, hi, {"m": entries}], "kwargs": []})
    return calls


_ch_generate = ch.generate_cases


def generate_cases(count, bank_seed):
    cases = _ch_generate(count, bank_seed)
    rng = random.Random(bank_seed ^ 0xC9)
    for case in cases:
        r = random.Random(rng.randrange(1 << 30))
        spec = case["spec"]
        case["calls"] += attribute_calls(r, spec) + ltl_calls(r, spec) + path_calls(r, spec) + timeframe_calls(r, spec)
    return cases


run_call = ch.run_call
for _k in dir(ch):
    if not _k.startswith("__"):
        globals().setdefault(_k, getattr(ch, _k))
globals()["generate_cases"] = generate_cases
globals()["run_call"] = run_call
