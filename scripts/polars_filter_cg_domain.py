"""Round CG: the Polars table filter domain for struct (dict) values of the
timeframe filter, list and str bounds of the case filters and str bounds and
one-item paths of the paths performance filter.

Builds the frames of scripts/polars_filter_cf_domain.py (typed and edge-value
columns), sometimes with an extra column "zz" (a copy of another column, so
that an attribute column sorts after the timestamp column), and gives each
case only the calls below:

- timestamp.filter_traces_attribute_in_timeframe with dict values of none,
  one or two fields (a repeated key, None, typed, edge and str fields) on
  every column, mostly over a window that holds every event, sometimes an
  empty, inverted or random one; other values on "zz";
- cases.filter_case_performance, filter_on_case_performance and
  filter_on_case_size with list (typed, nested, empty, None items, wide
  ints), str (inverted or equal pairs), None and odd numeric bounds;
- paths.apply_performance with str bound pairs and one-item paths."""
import datetime as _dt
import importlib.util
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("polars_filter_cf_domain", HERE / "polars_filter_cf_domain.py")
cf = importlib.util.module_from_spec(_spec)
sys.modules["polars_filter_cf_domain"] = cf
_spec.loader.exec_module(cf)
ce = cf.ce
codec = cf.codec
S, I, F, B, N = codec.S, codec.I, codec.F, codec.B, codec.N
L, T = cf.L, cf.T
UTC = _dt.timezone.utc
LATE = (None, None, "act", "case", "rd:amount", "rd:kind", "rd", "org:resource")
STRS = ("a", "b", "", "zz", "zz9", "é", "r1", "1")

_cf_frame_spec = cf.frame_spec
_cf_build_frame = cf.build_frame


def frame_spec(r):
    spec = _cf_frame_spec(r)
    spec["late"] = r.choice(LATE)
    return spec


def build_frame(spec):
    df = _cf_build_frame(spec)
    late = spec.get("late")
    if late:
        names = spec["names"]
        source = names.get(late, late)
        df = df.with_columns(df[source].alias("zz"))
    return df


def columns(spec):
    out = cf.columns(spec["names"])
    if spec.get("late"):
        out += ["zz", "zz"]
    return out


def field_value(r, spec, column):
    names = spec["names"]
    source = names.get(spec["late"], spec["late"]) if column == "zz" else column
    pick = r.random()
    if pick < 0.6:
        return cf.typed(r, spec, source)
    if pick < 0.8:
        return ce.edge_value(r)
    if pick < 0.9:
        return N()
    return S(r.choice(STRS))


def dict_value(r, spec, column):
    pick = r.random()
    keys = ("a", "b", "r1")
    if pick < 0.08:
        return {"m": []}
    if pick < 0.7:
        return {"m": [[S(r.choice(keys)), field_value(r, spec, column)]]}
    if pick < 0.9:
        a, b = r.sample(keys, 2)
        return {"m": [[S(a), field_value(r, spec, column)], [S(b), field_value(r, spec, column)]]}
    k = r.choice(keys)
    return {"m": [[S(k), field_value(r, spec, column)], [S(k), field_value(r, spec, column)]]}


def window(r):
    pick = r.random()
    lo = codec.wall(_dt.datetime(2021, 3, 1, tzinfo=UTC))
    hi = codec.wall(_dt.datetime(2021, 3, 5, tzinfo=UTC))
    if pick < 0.6:
        return lo, hi
    if pick < 0.7:
        return hi, lo
    if pick < 0.78:
        return lo, lo
    return cf.time_bound(r), cf.time_bound(r)


def timeframe_calls(r, spec):
    names = spec["names"]
    calls = []
    cols = columns(spec)
    for _ in range(r.choice((1, 2, 2, 3))):
        column = r.choice(cols)
        entries = cf.case_entries(r, spec, with_ts=True)
        value = dict_value(r, spec, column) if r.random() < 0.8 or column != "zz" else cf.timeframe_value(r, spec, names["act"])
        lo, hi = window(r)
        calls.append({"function": "timestamp.filter_traces_attribute_in_timeframe",
                      "args": [S(column), value, lo, hi, {"m": entries}], "kwargs": []})
    return calls


def list_item(r):
    pick = r.random()
    if pick < 0.35:
        return I(r.choice((0, 1, 2, 60, -1, 3600)))
    if pick < 0.5:
        return ce.edge_value(r)
    if pick < 0.6:
        return N()
    if pick < 0.7:
        return L(*[I(r.choice((1, 2))) for _ in range(r.randint(0, 2))])
    if pick < 0.85:
        return S(r.choice(STRS))
    return F(r.choice((0.5, 1.5, -0.0, float("nan"))))


def case_bound(r):
    pick = r.random()
    if pick < 0.3:
        items = [list_item(r) for _ in range(r.randint(0, 3))]
        return T(*items) if r.random() < 0.15 else L(*items)
    if pick < 0.5:
        return S(r.choice(STRS))
    if pick < 0.6:
        return N()
    if pick < 0.8:
        return ce.edge_value(r)
    return I(r.choice((0, 1, 2, 3, 60, 3600, 7200))) if r.random() < 0.6 else F(r.choice((0.5, 1.5, 3600.5, 1e10)))


def bound_pair(r):
    pick = r.random()
    if pick < 0.15:
        a, b = sorted(r.sample(STRS, 2))
        return (S(b), S(a)) if r.random() < 0.6 else (S(a), S(b))
    if pick < 0.25:
        v = case_bound(r)
        return v, v
    return case_bound(r), case_bound(r)


def case_calls(r, spec):
    names = spec["names"]
    calls = []
    custom = names["case"] != "case:concept:name"
    glue = [["case_id_glue", S(names["case"] if r.random() < 0.95 else "missing")]] if custom or r.random() < 0.1 else []
    for _ in range(r.choice((1, 1, 2))):
        lo, hi = bound_pair(r)
        entries = cf.case_entries(r, spec, with_ts=True, missing=0.03)
        calls.append({"function": "cases.filter_case_performance", "args": [lo, hi, {"m": entries} if entries or r.random() < 0.5 else N()],
                      "kwargs": []})
    lo, hi = bound_pair(r)
    kw = list(glue)
    if names["ts"] != "time:timestamp":
        kw.append(["timestamp_key", S(names["ts"])])
    kw += [["min_case_performance", lo], ["max_case_performance", hi]]
    calls.append({"function": "cases.filter_on_case_performance", "args": [], "kwargs": kw})
    for _ in range(r.choice((1, 1, 2))):
        lo, hi = bound_pair(r)
        kw = list(glue) + [["min_case_size", lo]]
        if r.random() < 0.85:
            kw.append(["max_case_size", hi])
        calls.append({"function": "cases.filter_on_case_size", "args": [], "kwargs": kw})
    return calls


def paths_calls(r, spec):
    names = spec["names"]
    calls = []
    for _ in range(r.choice((0, 1, 1))):
        entries = cf.case_entries(r, spec, with_ts=True, missing=0.03)
        if names["act"] != "concept:name":
            entries.append([S("pm4py:param:attribute_key"), S(names["act"])])
        lo, hi = bound_pair(r)
        if r.random() < 0.9:
            entries.append([S("min_performance"), lo])
        if r.random() < 0.9:
            entries.append([S("max_performance"), hi])
        r.shuffle(entries)
        pick = r.random()
        path = cf.path_shape(r, spec) if pick < 0.7 else (L if r.random() < 0.6 else T)(
            *([] if r.random() < 0.15 else [cf.path_label(r, spec) if r.random() < 0.6 else ce.edge_value(r)]))
        calls.append({"function": "paths.apply_performance", "args": [path, {"m": entries}], "kwargs": []})
    return calls


def generate_cases(count, bank_seed):
    rng = random.Random(bank_seed)
    cases = []
    for _ in range(count):
        r = random.Random(rng.randrange(1 << 30))
        spec = frame_spec(r)
        calls = timeframe_calls(r, spec) + case_calls(r, spec) + paths_calls(r, spec)
        cases.append({"spec": spec, "calls": calls})
    return cases


_cf_run_call = cf.run_call


def run_call(case, call):
    return _cf_run_call(case, call)


for _k in dir(cf):
    if not _k.startswith("__"):
        globals().setdefault(_k, getattr(cf, _k))
globals()["generate_cases"] = generate_cases
globals()["run_call"] = run_call
globals()["build_frame"] = build_frame
globals()["frame_spec"] = frame_spec
cf.base.build_frame = build_frame
