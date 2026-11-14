"""Round CJ: the Polars table filter domain for dict values in the bound and
anchor positions.

Builds the frames of scripts/polars_filter_ci_domain.py, with more typed
activity columns (integer, temporal, Categorical, Enum, Boolean and Null
kinds instead of the CH float kinds in about half of the cases). Each case
gets the calls of the CI domain and:

- attributes.apply_numeric and apply_numeric_events with dict, list-of-dict
  and empty-dict bounds (one or both) on any column, and stream filter
  values that are dicts or lists of dicts;
- attributes.filter_df_relative_occurrence_event_attribute with dict, list
  and odd thresholds;
- between.apply with dict, list-of-dict and mixed anchors on the activity
  column or another column;
- cases.filter_case_performance, filter_on_case_performance and
  filter_on_case_size with dict and list-of-dict bounds;
- paths.apply_performance with dict bounds and dict path items, and
  paths.apply with dict items on typed activity columns;
- attributes.apply, apply_events and filter_df_on_attribute_values with
  lists that mix plain lists, lists of dicts and scalars (Polars' "cannot
  build list with different dtypes"), and dict values on typed columns."""
import importlib.util
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("polars_filter_ci_domain", HERE / "polars_filter_ci_domain.py")
ci = importlib.util.module_from_spec(_spec)
sys.modules["polars_filter_ci_domain"] = ci
_spec.loader.exec_module(ci)
ch = ci.ch
cg = ci.cg
cf = ci.cf
ce = ci.ce
cc = ci.cc
base = ci.base
codec = ci.codec
S, I, F, B, N = codec.S, codec.I, codec.F, codec.B, codec.N
L, T = ci.L, ci.T

TYPED_KINDS = [k for k in cc.ACT_KINDS if k not in ("String", "Float32", "Float64")] + list(ce.NEW_KINDS)

_ch_frame_spec = base.frame_spec


def frame_spec(r):
    spec = _ch_frame_spec(r)
    if r.random() < 0.5:
        spec["act_kind"] = r.choice(TYPED_KINDS)
    return spec


base.frame_spec = frame_spec


def small_dict(r, spec, column):
    """A dict with zero to two fields of plain values."""
    fields = []
    for _ in range(r.choice((0, 1, 1, 1, 2))):
        pick = r.random()
        if pick < 0.4:
            v = I(r.choice((0, 1, 2, 60, 3600, -1)))
        elif pick < 0.6:
            v = ci.text_float(r)
        elif pick < 0.75:
            v = S(r.choice(("a", "x", "1")))
        elif pick < 0.85:
            v = N()
        else:
            v = ci.scalar(r, spec, column)
        fields.append([S(r.choice(("a", "b", "r1"))), v])
    return {"m": fields}


def dict_value(r, spec, column):
    pick = r.random()
    if pick < 0.35:
        return small_dict(r, spec, column)
    if pick < 0.5:
        return ci.struct(r, spec, column)
    if pick < 0.65:
        return L(*[small_dict(r, spec, column) for _ in range(r.choice((0, 1, 1, 2)))])
    if pick < 0.75:
        return ci.struct_list(r, spec, column)
    if pick < 0.85:
        return {"m": []}
    if pick < 0.92:
        return T(small_dict(r, spec, column))
    return ci.mixed_list(r, spec, column)


def bound(r, spec, column):
    pick = r.random()
    if pick < 0.7:
        return dict_value(r, spec, column)
    if pick < 0.85:
        return cf.typed(r, spec, column) if column in cf.columns(spec["names"]) else I(r.choice((0, 1, 2, 3600)))
    return I(r.choice((0, 1, 2, 3, 60, 3600, 7200)))


def bound_pair(r, spec, column):
    a, b = bound(r, spec, column), bound(r, spec, column)
    if r.random() < 0.3:
        a = I(r.choice((0, 1, 2)))
    elif r.random() < 0.3:
        b = I(r.choice((3, 3600, 10 ** 9)))
    return a, b


def numeric_calls(r, spec):
    names = spec["names"]
    calls = []
    for fn in ("apply_numeric", "apply_numeric_events"):
        for _ in range(r.choice((0, 1, 1, 2))):
            column = r.choice(cf.columns(names) + ["rd:amount", "rd", "rd:amount"])
            entries = [[S(r.choice(("pm4py:param:attribute_key", "enum:ATTRIBUTE_KEY"))), S(column)]]
            entries += cf.case_entries(r, spec, missing=0.03)
            cf.positive(r, entries, 0.3)
            if fn == "apply_numeric" and r.random() < 0.4:
                for i in range(1, r.choice((1, 1, 2)) + 1):
                    key = r.choice(cf.columns(names))
                    entries.append([S("stream_filter_key%d" % i), S(key)])
                    v = dict_value(r, spec, key) if r.random() < 0.7 else cf.typed(r, spec, key)
                    entries.append([S("stream_filter_value%d" % i), v])
            r.shuffle(entries)
            lo, hi = bound_pair(r, spec, column)
            calls.append({"function": "attributes." + fn, "args": [lo, hi, {"m": entries}], "kwargs": []})
    return calls


def relative_calls(r, spec):
    names = spec["names"]
    calls = []
    if r.random() < 0.4:
        entries = cf.case_entries(r, spec, missing=0.03)
        if r.random() < 0.5:
            entries.append([S(r.choice(("pm4py:param:attribute_key", "enum:ATTRIBUTE_KEY"))),
                            S(r.choice(cf.columns(names)))])
        if r.random() < 0.3:
            entries.append([S("keep_once_per_case"), B(r.random() < 0.5)])
        r.shuffle(entries)
        pick = r.random()
        if pick < 0.5:
            v = dict_value(r, spec, names["act"])
        elif pick < 0.7:
            v = L(*[F(r.choice((0.1, 0.5))) for _ in range(r.choice((0, 1, 2)))])
        else:
            v = ci.text_float(r)
        calls.append({"function": "attributes.filter_df_relative_occurrence_event_attribute",
                      "args": [v, {"m": entries}], "kwargs": []})
    return calls


def anchor(r, spec, column):
    pick = r.random()
    if pick < 0.45:
        return dict_value(r, spec, column)
    if pick < 0.65:
        items = [cf.typed(r, spec, column) for _ in range(r.randint(0, 2))]
        items.insert(r.randint(0, len(items)), small_dict(r, spec, column))
        return L(*items)
    return cf.act_typed(r, spec) if column == spec["names"]["act"] else cf.typed(r, spec, column)


def between_calls(r, spec):
    names = spec["names"]
    calls = []
    for _ in range(r.choice((0, 1, 1, 2))):
        column = names["act"] if r.random() < 0.7 else r.choice(("org:resource", "rd:amount", names["ts"], "rd:kind"))
        entries = cf.case_entries(r, spec, missing=0.03)
        if column != "concept:name" or r.random() < 0.1:
            entries.append([S(r.choice(("pm4py:param:activity_key", "enum:ACTIVITY_KEY"))), S(column)])
        r.shuffle(entries)
        a1, a2 = anchor(r, spec, column), anchor(r, spec, column)
        calls.append({"function": "between.apply", "args": [a1, a2, {"m": entries} if entries or r.random() < 0.5 else N()],
                      "kwargs": []})
    return calls


def case_calls(r, spec):
    names = spec["names"]
    calls = []
    custom = names["case"] != "case:concept:name"
    glue = [["case_id_glue", S(names["case"])]] if custom else []
    if r.random() < 0.6:
        lo, hi = bound_pair(r, spec, "rd")
        entries = cf.case_entries(r, spec, with_ts=True, missing=0.03)
        calls.append({"function": "cases.filter_case_performance",
                      "args": [lo, hi, {"m": entries} if entries or r.random() < 0.5 else N()], "kwargs": []})
    if r.random() < 0.5:
        lo, hi = bound_pair(r, spec, "rd")
        kw = list(glue)
        if names["ts"] != "time:timestamp":
            kw.append(["timestamp_key", S(names["ts"])])
        if r.random() < 0.85:
            kw.append(["min_case_performance", lo])
        if r.random() < 0.85:
            kw.append(["max_case_performance", hi])
        calls.append({"function": "cases.filter_on_case_performance", "args": [], "kwargs": kw})
    if r.random() < 0.6:
        lo, hi = bound_pair(r, spec, "rd")
        kw = list(glue) + [["min_case_size", lo]]
        if r.random() < 0.7:
            kw.append(["max_case_size", hi])
        calls.append({"function": "cases.filter_on_case_size", "args": [], "kwargs": kw})
    return calls


def path_calls(r, spec):
    names = spec["names"]
    calls = []
    if r.random() < 0.5:
        entries = cf.case_entries(r, spec, with_ts=True, missing=0.03)
        if names["act"] != "concept:name":
            entries.append([S("pm4py:param:attribute_key"), S(names["act"])])
        lo, hi = bound_pair(r, spec, "rd")
        if r.random() < 0.8:
            entries.append([S("min_performance"), lo])
        if r.random() < 0.6:
            entries.append([S("max_performance"), hi])
        r.shuffle(entries)
        path = cf.path_shape(r, spec)
        if r.random() < 0.3:
            items = list(path.get("t", path.get("l", [S("a")])))
            items[r.randrange(len(items))] = small_dict(r, spec, names["act"])
            path = T(*items)
        calls.append({"function": "paths.apply_performance", "args": [path, {"m": entries}], "kwargs": []})
    if r.random() < 0.5:
        entries = cf.case_entries(r, spec, with_ts=True, missing=0.03)
        if names["act"] != "concept:name":
            entries.append([S("pm4py:param:attribute_key"), S(names["act"])])
        cf.positive(r, entries)
        pairs = [T(cf.act_typed(r, spec), cf.act_typed(r, spec)) for _ in range(r.randint(0, 2))]
        d = dict_value(r, spec, names["act"]) if r.random() < 0.7 else small_dict(r, spec, names["act"])
        bad = r.choice((T(d, cf.act_typed(r, spec)), T(cf.act_typed(r, spec), d)))
        pairs.insert(r.randint(0, len(pairs)), bad)
        calls.append({"function": "paths.apply", "args": [L(*pairs), {"m": entries}], "kwargs": []})
    return calls


def nested_mix(r, spec, column):
    """Lists mixing plain lists, lists of dicts and scalars."""
    plain = L(*[I(r.choice((1, 2))) for _ in range(r.choice((0, 1, 1, 2)))])
    dicts = L(*[small_dict(r, spec, column) for _ in range(r.choice((1, 1, 2)))])
    scalar = r.choice((I(5), F(0.5), S("a"), N(), B(True)))
    shapes = (
        lambda: L(plain, dicts, scalar),
        lambda: L(plain, dicts),
        lambda: L(dicts, plain),
        lambda: L(dicts, plain, scalar),
        lambda: L(plain, scalar, dicts),
        lambda: L(L(S("a")), dicts),
        lambda: L(L(F(1.5)), dicts, scalar),
        lambda: L(dicts, L(small_dict(r, spec, column), I(1))),
        lambda: L(N(), plain, dicts),
        lambda: L(plain, L(), dicts),
    )
    return r.choice(shapes)()


def typed_columns(spec):
    names = spec["names"]
    return [names["act"], names["case"], names["ts"], names["act"]]


def attribute_calls(r, spec):
    names = spec["names"]
    calls = []
    for fn in ("apply", "apply_events", "filter_df_on_attribute_values"):
        if r.random() >= 0.5:
            continue
        column = r.choice(typed_columns(spec)) if r.random() < 0.7 else ci.target_column(r, spec)
        v = nested_mix(r, spec, column) if r.random() < 0.5 else dict_value(r, spec, column)
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


def timeframe_calls(r, spec):
    calls = []
    if r.random() < 0.5:
        column = r.choice(typed_columns(spec))
        entries = cf.case_entries(r, spec, with_ts=True)
        v = nested_mix(r, spec, column) if r.random() < 0.4 else dict_value(r, spec, column)
        lo, hi = cg.window(r)
        calls.append({"function": "timestamp.filter_traces_attribute_in_timeframe",
                      "args": [S(column), v, lo, hi, {"m": entries}], "kwargs": []})
    return calls


_ci_generate = ci.generate_cases


def generate_cases(count, bank_seed):
    cases = _ci_generate(count, bank_seed)
    rng = random.Random(bank_seed ^ 0xCA)
    for case in cases:
        r = random.Random(rng.randrange(1 << 30))
        spec = case["spec"]
        case["calls"] += (numeric_calls(r, spec) + relative_calls(r, spec) + between_calls(r, spec)
                          + case_calls(r, spec) + path_calls(r, spec) + attribute_calls(r, spec)
                          + timeframe_calls(r, spec))
    return cases


run_call = ci.run_call
for _k in dir(ci):
    if not _k.startswith("__"):
        globals().setdefault(_k, getattr(ci, _k))
globals()["generate_cases"] = generate_cases
globals()["run_call"] = run_call
globals()["frame_spec"] = frame_spec
