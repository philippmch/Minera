"""Round CF: the Polars table filter domain with typed and edge values in
the remaining argument positions.

Loads scripts/polars_filter_ce_domain.py (typed and edge-value frames and
calls) and adds, per case:

- scalar membership: attributes.apply, apply_events and
  filter_df_on_attribute_values with one value (not a list) of the column's
  type, an edge value or a str, on any column, with the case key sometimes
  missing so that the resolved plan of the scalar is_in is printed;
- numeric filters on every column with typed and edge bounds (wide ints,
  NaN, infinities, None, dates, strs, bools; both bounds may be odd) and up
  to two stream filters on distinct or repeated columns with typed, edge or
  None values;
- between.apply with typed, edge, None, empty, nested and tuple anchors, on
  the activity column or another column as activity key;
- the timestamp filters with edge bounds (dates before 1970, ints, floats,
  strs, None) and filter_traces_attribute_in_timeframe with typed, edge,
  list, all-str list, tuple and dict values on any column;
- paths.apply_performance with odd path shapes (3-tuples, lists, strs) and
  None, str, list, float, date, bool, wide-int and NaN bounds, and paths.apply
  with typed and edge pairs."""
import datetime as _dt
import importlib.util
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("polars_filter_ce_domain", HERE / "polars_filter_ce_domain.py")
ce = importlib.util.module_from_spec(_spec)
sys.modules["polars_filter_ce_domain"] = ce
_spec.loader.exec_module(ce)
cc = ce.cc
base = cc.base
codec = cc.codec
S, I, F, B, N = codec.S, codec.I, codec.F, codec.B, codec.N
RESOURCES, KINDS = base.RESOURCES, base.KINDS
BASE = base.BASE


def L(*items):
    return {"l": list(items)}


def T(*items):
    return {"t": list(items)}


def columns(names):
    return [names["act"], names["act"], names["case"], names["ts"], "org:resource", "rd:amount", "rd:kind", "rd"]


def act_typed(r, spec):
    kind = spec["act_kind"]
    text = r.choice(cc.ORDER)
    if kind == "Boolean":
        return B(cc._code(text) % 2 == 1)
    if kind == "Null":
        return ce.edge_value(r)
    if kind in ("String", "cat", "enum"):
        return S(text)
    return cc._value_arg(r, kind, text, "value")


def case_typed(r, spec):
    kind = spec["case_kind"]
    c = r.choice([1, 2, 3, 4, 9])
    if kind in ("str", "cat", "enum"):
        return S(str(c))
    if kind == "Float32":
        return F(c + 0.5)
    if kind == "Datetime":
        return codec.wall(_dt.datetime(2021, 5, c, 7))
    if kind == "Date":
        return codec.wall(_dt.datetime(2021, 5, c))
    return I(c)


def ts_typed(r):
    stamp = BASE + _dt.timedelta(minutes=r.choice((0, 30, 60, 90, 180, 600, -60, 3000)))
    pick = r.random()
    if pick < 0.5:
        return codec.wall(stamp.replace(tzinfo=_dt.timezone.utc))
    if pick < 0.8:
        return codec.wall(stamp)
    return S(stamp.strftime("%Y-%m-%d %H:%M:%S"))


def typed(r, spec, column):
    """A value of the column's type (sometimes an edge value or None)."""
    names = spec["names"]
    pick = r.random()
    if pick < 0.12:
        return ce.edge_value(r)
    if pick < 0.17:
        return N()
    if column == names["act"]:
        return act_typed(r, spec)
    if column == names["case"]:
        return case_typed(r, spec)
    if column == names["ts"]:
        return ts_typed(r)
    if column == "org:resource":
        return S(r.choice(RESOURCES + ("zz",)))
    if column == "rd:kind":
        return S(r.choice(KINDS + ("q",)))
    if column in ("rd:amount", "rd"):
        v = r.choice((0, 1, 2, 3, 5, 10, 50, 100))
        return I(v) if r.random() < 0.6 else F(v + r.choice((0.0, 0.5)))
    return S("a")


def odd_bound(r):
    pick = r.random()
    if pick < 0.15:
        return N()
    if pick < 0.55:
        return ce.edge_value(r)
    if pick < 0.65:
        return {"l": [ce.edge_value(r) for _ in range(r.randint(0, 2))]}
    return I(r.choice((0, 1, 2, 5, 10, 60, 3600))) if r.random() < 0.6 else F(r.choice((0.5, 1800.5, 1e10)))


def case_entries(r, spec, with_ts=False, missing=0.1):
    names = spec["names"]
    entries = []
    custom = names["case"] != "case:concept:name"
    if custom or r.random() < 0.15:
        entries.append([S(r.choice(("pm4py:param:case_id_key", "enum:CASE_ID_KEY"))),
                        S("missing" if r.random() < missing else names["case"])])
    if with_ts and (names["ts"] != "time:timestamp" or r.random() < 0.1):
        entries.append([S(r.choice(("pm4py:param:timestamp_key", "enum:TIMESTAMP_KEY"))), S(names["ts"])])
    return entries


def positive(r, entries, p=0.5):
    if r.random() < p:
        entries.append([S("positive"), r.choice((B(True), B(False), I(0), I(1), N()))])


def scalar_calls(r, spec):
    names = spec["names"]
    calls = []
    for fn in ("apply", "apply_events", "filter_df_on_attribute_values"):
        if r.random() >= 0.65:
            continue
        column = r.choice(columns(names))
        value = typed(r, spec, column)
        if fn == "filter_df_on_attribute_values":
            kwargs = [["attribute_key", S(column)]]
            if names["case"] != "case:concept:name" or r.random() < 0.3:
                kwargs.append(["case_id_glue", S("missing" if r.random() < 0.3 else names["case"])])
            if r.random() < 0.5:
                kwargs.append(["positive", r.choice((B(True), B(False), I(0), N()))])
            calls.append({"function": "attributes." + fn, "args": [value], "kwargs": kwargs})
            continue
        entries = [[S(r.choice(("pm4py:param:attribute_key", "enum:ATTRIBUTE_KEY"))), S(column)]]
        entries += case_entries(r, spec, missing=0.35 if fn == "apply" else 0.1)
        positive(r, entries)
        r.shuffle(entries)
        calls.append({"function": "attributes." + fn, "args": [value, {"m": entries}], "kwargs": []})
    return calls


def numeric_calls(r, spec):
    names = spec["names"]
    calls = []
    for fn in ("apply_numeric", "apply_numeric_events"):
        for _ in range(r.choice((0, 1, 1, 2))):
            column = r.choice(columns(names) + ["rd:amount", "rd"])
            entries = [[S(r.choice(("pm4py:param:attribute_key", "enum:ATTRIBUTE_KEY"))), S(column)]]
            entries += case_entries(r, spec)
            positive(r, entries, 0.3)
            if fn == "apply_numeric":
                keys = r.choice((0, 1, 2, 2))
                picked = [r.choice(columns(names)) for _ in range(keys)]
                for i, key in enumerate(picked, 1):
                    entries.append([S("stream_filter_key%d" % i), S(key)])
                    entries.append([S("stream_filter_value%d" % i), typed(r, spec, key) if r.random() < 0.75 else odd_bound(r)])
            r.shuffle(entries)
            if r.random() < 0.5:
                lo, hi = odd_bound(r), odd_bound(r)
            else:
                lo, hi = typed(r, spec, column), typed(r, spec, column)
            calls.append({"function": "attributes." + fn, "args": [lo, hi, {"m": entries}], "kwargs": []})
    return calls


def anchor(r, spec, column):
    pick = r.random()
    if pick < 0.45:
        return typed(r, spec, column)
    if pick < 0.75:
        return L(*[typed(r, spec, column) for _ in range(r.randint(1, 3))])
    if pick < 0.82:
        return T(*[typed(r, spec, column) for _ in range(r.randint(1, 2))])
    if pick < 0.88:
        return L()
    if pick < 0.94:
        return N()
    return L(L(typed(r, spec, column)), typed(r, spec, column))


def between_calls(r, spec):
    names = spec["names"]
    calls = []
    for _ in range(r.choice((0, 1, 1, 2))):
        column = names["act"] if r.random() < 0.7 else r.choice(("org:resource", "rd:amount", names["ts"], "rd:kind"))
        entries = case_entries(r, spec)
        if column != "concept:name" or r.random() < 0.1:
            entries.append([S(r.choice(("pm4py:param:activity_key", "enum:ACTIVITY_KEY"))), S(column)])
        if r.random() < 0.2:
            entries.append([S("subcase_concat_str"), r.choice((S("##@@"), S("-"), S(""), I(1), N()))])
        r.shuffle(entries)
        calls.append({"function": "between.apply", "args": [anchor(r, spec, column), anchor(r, spec, column),
                                                               {"m": entries} if entries or r.random() < 0.5 else N()],
                      "kwargs": []})
    return calls


def time_bound(r):
    pick = r.random()
    if pick < 0.6:
        return ts_typed(r)
    if pick < 0.78:
        return codec.wall(r.choice(ce.EDGE_DATES))
    if pick < 0.85:
        return N()
    return ce.edge_value(r)


def timeframe_value(r, spec, column):
    pick = r.random()
    if pick < 0.45:
        return typed(r, spec, column)
    if pick < 0.6:
        return L(*[typed(r, spec, column) for _ in range(r.randint(0, 3))])
    if pick < 0.7:
        return L(*[S(r.choice(("a", "b", "r1", "x", "zz"))) for _ in range(r.randint(1, 3))])
    if pick < 0.78:
        return T(*[typed(r, spec, column) for _ in range(r.randint(1, 2))])
    if pick < 0.9:
        return {"m": [[S(r.choice(("a", "b", "r1"))), typed(r, spec, column)] for _ in range(r.randint(0, 2))]}
    return N()


def timestamp_calls(r, spec):
    names = spec["names"]
    calls = []
    for fn in ("filter_traces_contained", "filter_traces_intersecting", "apply_events"):
        if r.random() < 0.5:
            entries = case_entries(r, spec, with_ts=True)
            calls.append({"function": "timestamp." + fn, "args": [time_bound(r), time_bound(r), {"m": entries}], "kwargs": []})
    for _ in range(r.choice((0, 1, 1, 2))):
        column = r.choice(columns(names))
        entries = case_entries(r, spec, with_ts=True)
        calls.append({"function": "timestamp.filter_traces_attribute_in_timeframe",
                      "args": [S(column), timeframe_value(r, spec, column), time_bound(r), time_bound(r), {"m": entries}],
                      "kwargs": []})
    return calls


def path_label(r, spec):
    labels = sorted({a for a in spec["acts"] if a is not None}) or ["a"]
    return S(r.choice(labels + ["z"])) if r.random() < 0.7 else act_typed(r, spec)


def path_shape(r, spec):
    a, b = path_label(r, spec), path_label(r, spec)
    pick = r.random()
    if pick < 0.55:
        return T(a, b)
    if pick < 0.7:
        return T(a, b, path_label(r, spec))
    if pick < 0.85:
        return L(a, b)
    return S("ab")


def paths_calls(r, spec):
    names = spec["names"]
    calls = []
    for _ in range(r.choice((0, 1, 1, 2))):
        entries = case_entries(r, spec, with_ts=True)
        if names["act"] != "concept:name" or r.random() < 0.1:
            entries.append([S(r.choice(("pm4py:param:attribute_key", "enum:ATTRIBUTE_KEY"))), S(names["act"])])
        positive(r, entries, 0.3)
        for key in ("min_performance", "max_performance"):
            if r.random() < 0.7:
                entries.append([S(key), odd_bound(r) if r.random() < 0.6 else I(r.choice((0, 60, 3600, 7200)))])
        r.shuffle(entries)
        calls.append({"function": "paths.apply_performance", "args": [path_shape(r, spec), {"m": entries}], "kwargs": []})
    if r.random() < 0.5:
        entries = case_entries(r, spec, with_ts=True)
        if names["act"] != "concept:name" or r.random() < 0.1:
            entries.append([S(r.choice(("pm4py:param:attribute_key", "enum:ATTRIBUTE_KEY"))), S(names["act"])])
        positive(r, entries, 0.5)
        pairs = [T(path_label(r, spec), path_label(r, spec)) for _ in range(r.randint(0, 3))]
        calls.append({"function": "paths.apply", "args": [L(*pairs), {"m": entries}], "kwargs": []})
    return calls


_ce_run_call = ce.run_call


def run_call(case, call):
    """Polars 1.44.1 panics on a few inputs (pyo3's PanicException is no
    Exception); the panic is kept as an error outcome, which the bank test
    declares and does not compare."""
    try:
        return _ce_run_call(case, call)
    except BaseException as error:  # noqa: BLE001 - pyo3_runtime.PanicException
        if type(error).__name__ != "PanicException":
            raise
        return {"error": ["PanicException", str(error).split("\n")[0]]}


_ce_generate = ce.generate_cases


def generate_cases(count, bank_seed):
    cases = _ce_generate(count, bank_seed)
    rng = random.Random(bank_seed ^ 0xCF)
    for case in cases:
        spec = case["spec"]
        r = random.Random(rng.randrange(1 << 30))
        case["calls"] += (scalar_calls(r, spec) + numeric_calls(r, spec) + between_calls(r, spec) +
                          timestamp_calls(r, spec) + paths_calls(r, spec))
    return cases


for _k in dir(ce):
    if not _k.startswith("__"):
        globals().setdefault(_k, getattr(ce, _k))
globals()["generate_cases"] = generate_cases
globals()["run_call"] = run_call
