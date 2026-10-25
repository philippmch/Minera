"""Seeded public-call domain for the pandas attribute, between, case, ltl,
paths, timestamp and variants filters.

Covers every public callable of

  algo.filtering.pandas.attributes.attributes_filter
  algo.filtering.pandas.between.between_filter
  algo.filtering.pandas.cases.case_filter
  algo.filtering.pandas.ltl.ltl_checker
  algo.filtering.pandas.paths.paths_filter
  algo.filtering.pandas.timestamp.timestamp_filter
  algo.filtering.pandas.variants.variants_filter

Each case is one generated DataFrame (its native event-table JSON travels as
"source") and a list of calls on fresh copies of it, in the call format of
scripts/pandas_sequence_filter_domain.py: typed arguments, parameter
dictionaries {"m": [[key, value], ...]} whose "enum:NAME" keys are the
module's Parameters members, and keyword arguments [[name, value], ...].
Datetime arguments use the {"w": [wall clock, UTC offset minutes or null]}
form of scripts/filtering_common_domain.py.

Domain limits: column labels are strings; the grouped/variants dataframe
helpers (GROUPED_DATAFRAME, variants_df) stay unset; business-hour case
performance uses the default slots.
"""
import datetime
import importlib
import importlib.util
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent


def _load(name):
    spec = importlib.util.spec_from_file_location(name, HERE / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


codec = _load("filtering_common_domain")
tables = _load("repetition_domain")
sequence = _load("pandas_sequence_filter_domain")
S, I, F, B, N = codec.S, codec.I, codec.F, codec.B, codec.N

MODULES = {
    "attributes": "pm4py.algo.filtering.pandas.attributes.attributes_filter",
    "between": "pm4py.algo.filtering.pandas.between.between_filter",
    "cases": "pm4py.algo.filtering.pandas.cases.case_filter",
    "ltl": "pm4py.algo.filtering.pandas.ltl.ltl_checker",
    "paths": "pm4py.algo.filtering.pandas.paths.paths_filter",
    "timestamp": "pm4py.algo.filtering.pandas.timestamp.timestamp_filter",
    "variants": "pm4py.algo.filtering.pandas.variants.variants_filter",
}

LABELS = ("a", "b", "c", "d")
ODD_LABELS = ("", "a b", "é")
RESOURCES = ("r1", "r2", "r3")
KINDS = ("x", "y", "z")
BASE = datetime.datetime(2021, 3, 1, 8, 0, 0)


def L(*v): return {"l": list(v)}
def T(*v): return {"t": list(v)}
def W(dt, offset=None): return {"w": [dt.isoformat(sep=" "), offset]}


# ---------------------------------------------------------------- frames

def frame_spec(r):
    n = r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14])
    ncases = r.randint(1, 4)
    case_kind = "int" if r.random() < 0.15 else "str"
    cases = [r.randint(1, ncases) for _ in range(n)]
    if case_kind == "str" and n and r.random() < 0.06:
        cases[r.randrange(n)] = None
    pool = list(LABELS[: r.randint(1, 4)])
    if r.random() < 0.15:
        pool.append(r.choice(ODD_LABELS))
    acts = [r.choice(pool) for _ in range(n)]
    if n and r.random() < 0.12:
        acts[r.randrange(n)] = None
    minutes = [i * r.choice((30, 60, 90)) if r.random() < 0.7 else r.randint(0, 1200) for i in range(n)]
    if n > 1 and r.random() < 0.3:
        for _ in range(r.randint(1, 3)):
            minutes[r.randrange(n)] = minutes[r.randrange(n)]
    if n and r.random() < 0.05:
        minutes[r.randrange(n)] = None
    resources = [r.choice(RESOURCES) for _ in range(n)]
    if n and r.random() < 0.2:
        resources[r.randrange(n)] = None
    amount_kind = r.choice(["float", "float", "int"])
    if amount_kind == "int":
        amounts = [r.choice((0, 5, 10, 50, 100, -10)) for _ in range(n)]
    else:
        amounts = [r.choice((0.0, 0.5, 5.0, 10.0, 50.0, 100.0, -10.0, None)) for _ in range(n)]
    kinds = [r.choice(KINDS) for _ in range(n)]
    names = {"case": "case:concept:name", "act": "concept:name", "ts": "time:timestamp"}
    if r.random() < 0.12:
        names = {"case": "cid", "act": "act", "ts": "ts"}
    roll = r.random()
    if roll < 0.12:
        index = [r.randint(0, 30) for _ in range(n)]
    elif roll < 0.25:
        index = list(range(n))
        r.shuffle(index)
    else:
        index = None
    order = "shuffled" if r.random() < 0.2 else "natural"
    return {"n": n, "case_kind": case_kind, "cases": cases, "acts": acts, "minutes": minutes,
            "resources": resources, "amount_kind": amount_kind, "amounts": amounts, "kinds": kinds,
            "names": names, "index": index, "order": order, "shuffle_seed": r.randrange(1 << 30)}


def build_frame(spec):
    import pandas as pd
    names = spec["names"]
    if spec["case_kind"] == "int":
        case_col = pd.Series(spec["cases"], dtype="int64")
    else:
        case_col = pd.Series([None if c is None else str(c) for c in spec["cases"]], dtype="str")
    base = pd.Timestamp(BASE, tz="UTC")
    stamps = [pd.NaT if m is None else base + pd.Timedelta(minutes=m) for m in spec["minutes"]]
    data = {names["case"]: case_col,
            names["act"]: pd.Series(spec["acts"], dtype="str"),
            names["ts"]: pd.Series(stamps, dtype="datetime64[us, UTC]"),
            "org:resource": pd.Series(spec["resources"], dtype="str"),
            "rd:amount": pd.Series([float("nan") if v is None else v for v in spec["amounts"]],
                                   dtype="int64" if spec["amount_kind"] == "int" else "float64"),
            "rd:kind": pd.Series(spec["kinds"], dtype="str"),
            "rd": pd.Series(range(spec["n"]), dtype="int64")}
    df = pd.DataFrame(data)
    if spec["index"] is not None:
        df.index = pd.Index(spec["index"], dtype="int64")
    if spec["order"] == "shuffled" and len(df):
        df = df.sample(frac=1, random_state=spec["shuffle_seed"] % (1 << 31))
    return df


# ---------------------------------------------------------------- arguments

def param_key(r, enum_name, literal):
    roll = r.random()
    if roll < 0.55:
        return S("enum:" + enum_name)
    if roll < 0.95:
        return S(literal)
    return S(literal.upper())


def number(r, choices):
    v = r.choice(choices)
    return I(v) if isinstance(v, int) else F(v)


def wild(r):
    return r.choice((N(), S(""), S("a"), S("r1"), I(0), I(1), I(-1), F(0.5), F(float("nan")),
                     F(float("inf")), B(True), B(False), L(), T(), L(S("a"), I(1)), W(BASE)))


class Calls:
    def __init__(self, r, spec):
        self.r = r
        self.spec = spec
        self.names = spec["names"]
        self.custom = self.names["case"] != "case:concept:name"
        self.calls = []
        self.labels = sorted({a for a in spec["acts"] if a is not None}) or ["a"]

    def add(self, function, args, kwargs=()):
        r = self.r
        if r.random() < 0.08:
            args = [wild(r) if r.random() < 0.3 else a for a in args]
        self.calls.append((function, args, list(kwargs)))

    def act(self):
        r = self.r
        return S(r.choice(self.labels + ["z"]) if r.random() < 0.15 else r.choice(self.labels))

    def params(self, columns=(), extra=()):
        """A parameters argument: None, or a dict of column keys and extras."""
        r = self.r
        entries = []
        for enum_name, literal, role in columns:
            if role in self.names and (self.custom or r.random() < 0.12):
                value = self.names[role] if (self.custom and r.random() < 0.9) else r.choice(["missing", self.names[role]])
                entries.append([param_key(r, enum_name, literal), S(value)])
            elif role not in self.names and r.random() < 0.25:
                entries.append([param_key(r, enum_name, literal), S(r.choice(role))])
        for enum_name, literal, values, p in extra:
            if r.random() < p:
                value = r.choice(values) if r.random() < 0.93 else wild(r)
                entries.append([param_key(r, enum_name, literal), value])
        r.shuffle(entries)
        if not entries and r.random() < 0.4:
            return N()
        return {"m": entries}

    def dt(self):
        r = self.r
        base = BASE + datetime.timedelta(minutes=r.choice((0, 30, 60, 90, 180, 300, 600, 1200, -60, 3000)))
        roll = r.random()
        if roll < 0.6:
            return S(base.strftime("%Y-%m-%d %H:%M:%S"))
        if roll < 0.8:
            return W(base)
        if roll < 0.93:
            return W(base, 0)
        return r.choice((S("2021-03-01"), S("bad"), I(5), N(), W(base, 120)))


CASE = ("CASE_ID_KEY", "pm4py:param:case_id_key", "case")
ACT = ("ACTIVITY_KEY", "pm4py:param:activity_key", "act")
TS = ("TIMESTAMP_KEY", "pm4py:param:timestamp_key", "ts")


def frame_calls(r, spec):
    c = Calls(r, spec)
    names = spec["names"]
    positive = ("POSITIVE", "positive", [B(True), B(False), B(False), I(0), I(1), N()], 0.5)
    attr_choices = (names["act"], "org:resource", "rd:kind", names["act"])
    attribute = ("ATTRIBUTE_KEY", "pm4py:param:attribute_key", attr_choices)

    # attributes
    for fn in ("apply", "apply_events"):
        key = r.choice(attr_choices)
        pool = {names["act"]: c.labels + ["z"], "org:resource": list(RESOURCES), "rd:kind": list(KINDS)}[key]
        values = [S(v) for v in r.sample(pool, r.randint(0, min(3, len(pool))))]
        if r.random() < 0.1:
            values.append(N())
        extra = [("ATTRIBUTE_KEY", "pm4py:param:attribute_key", [S(key)], 0.85 if key != names["act"] else 0.3),
                 positive,
                 ("KEEP_NAN_VALUES", "keep_nan_values", [B(True), B(False)], 0.2)]
        c.add("attributes." + fn, [L(*values), c.params([CASE], extra)])
    for fn in ("apply_numeric", "apply_numeric_events"):
        extra = [("ATTRIBUTE_KEY", "pm4py:param:attribute_key", [S("rd:amount"), S("rd:amount"), S("rd:amount"), S("rd"), S("rd:kind")], 0.95),
                 positive]
        if fn == "apply_numeric" and r.random() < 0.3:
            extra += [("STREAM_FILTER_KEY1", "stream_filter_key1", [S(names["act"])], 1.0),
                      ("STREAM_FILTER_VALUE1", "stream_filter_value1", [c.act()], 1.0)]
            if r.random() < 0.4:
                extra += [("STREAM_FILTER_KEY2", "stream_filter_key2", [S("org:resource")], 1.0),
                          ("STREAM_FILTER_VALUE2", "stream_filter_value2", [S(r.choice(RESOURCES))], 1.0)]
        lo = number(r, (-10, 0, 0.5, 5, 10, 50))
        hi = number(r, (0, 10, 50, 100, 100.0, 1e4))
        c.add("attributes." + fn, [lo, hi, c.params([CASE], extra)])
    kw = []
    if c.custom or r.random() < 0.15:
        kw.append(["case_id_glue", S(names["case"] if r.random() < 0.9 else "missing")])
    key = r.choice(attr_choices)
    if key != "concept:name" or r.random() < 0.2:
        kw.append(["attribute_key", S(key)])
    if r.random() < 0.5:
        kw.append(["positive", r.choice([B(True), B(False), I(0), N()])])
    if r.random() < 0.3:
        kw.append(["keep_nan_values", r.choice([B(True), B(False)])])
    pool = {names["act"]: c.labels + ["z"], "org:resource": list(RESOURCES), "rd:kind": list(KINDS)}[key]
    values = [S(v) for v in r.sample(pool, r.randint(0, min(3, len(pool))))] + ([N()] if r.random() < 0.15 else [])
    c.add("attributes.filter_df_on_attribute_values", [L(*values)], kw)
    kw = []
    if r.random() < 0.5:
        counts = {}
        for _ in range(r.randint(0, 4)):
            counts[r.choice(c.labels + ["z"])] = r.randint(0, 5)
        kw.append(["act_count0", codec.encode(counts)])
    if c.custom or r.random() < 0.15:
        kw.append(["activity_key", S(names["act"] if r.random() < 0.85 else "org:resource")])
    if r.random() < 0.3:
        kw.append(["most_common_variant", L(*[c.act() for _ in range(r.randint(0, 3))])])
    c.add("attributes.filter_df_keeping_activ_exc_thresh", [number(r, (0, 1, 2, 3, 5, 1.5, -1))], kw)
    kw = []
    if c.custom or r.random() < 0.15:
        kw.append(["activity_key", S(names["act"] if r.random() < 0.85 else "org:resource")])
    if r.random() < 0.8:
        kw.append(["max_no_activities", number(r, (0, 1, 2, 3, 25, -1))])
    c.add("attributes.filter_df_keeping_spno_activities", [], kw)
    extra = [attribute + (0.4,), ("KEEP_ONCE_PER_CASE", "keep_once_per_case", [B(True), B(False)], 0.5)]
    extra = [(e[0], e[1], [S(v) for v in e[2]], e[3]) if e[0] == "ATTRIBUTE_KEY" else e for e in extra]
    c.add("attributes.filter_df_relative_occurrence_event_attribute",
          [number(r, (0.0, 0.1, 0.3, 0.5, 0.9, 1.0)), c.params([CASE], extra)])

    # between
    one = lambda: L(*[c.act() for _ in range(r.randint(1, 2))]) if r.random() < 0.25 else c.act()
    extra = [("SUBCASE_CONCAT_STR", "subcase_concat_str", [S("##@@"), S("-"), S("")], 0.2)]
    c.add("between.apply", [one(), one(), c.params([CASE, ACT], extra)])

    # cases
    kw = []
    if c.custom or r.random() < 0.15:
        kw.append(["case_id_glue", S(names["case"] if r.random() < 0.9 else "missing")])
    c.add("cases.filter_on_ncases", [], kw + ([["max_no_cases", number(r, (0, 1, 2, 3, 1000, -1, 2.5))]] if r.random() < 0.85 else []))
    kw2 = list(kw)
    if r.random() < 0.85:
        kw2.append(["min_case_size", number(r, (0, 1, 2, 3, 4, 2.5))])
    if r.random() < 0.6:
        kw2.append(["max_case_size", r.choice((number(r, (2, 3, 5, 1, 1.5)), N()))])
    c.add("cases.filter_on_case_size", [], kw2)
    kw3 = list(kw)
    if c.custom or r.random() < 0.1:
        kw3.append(["timestamp_key", S(names["ts"] if r.random() < 0.9 else "missing")])
    kw3.append(["min_case_performance", number(r, (0, 0, 60, 1800, 3600.5, 7200))])
    kw3.append(["max_case_performance", number(r, (0, 60, 3600, 7200, 36000, 1e10))])
    if r.random() < 0.2:
        kw3.append(["business_hours", B(r.random() < 0.7)])
    c.add("cases.filter_on_case_performance", [], kw3)
    extra = [("BUSINESS_HOURS", "business_hours", [B(True), B(False)], 0.15)]
    c.add("cases.filter_case_performance",
          [number(r, (0, 0, 60, 1800, 3600.5)), number(r, (0, 60, 3600, 7200, 36000, 1e10)), c.params([CASE, TS], extra)])
    if r.random() < 0.15:
        fn = r.choice(("apply", "apply_auto_filter"))
        c.add("cases." + fn, [c.params([CASE, TS])])

    # ltl
    flow = [c.act() for _ in range(r.choice((1, 2, 2, 2, 3, 3, 3, 3, 3)))]
    extra = [positive]
    if r.random() < 0.3:
        size = max(0, len(flow) - 1 + r.choice((0, 0, 0, -1, 1)))
        bounds = [T(number(r, (0, 0, 60)), number(r, (60, 3600, 1e6))) for _ in range(size)]
        extra.append(("TIMESTAMP_DIFF_BOUNDARIES", "timestamp_diff_boundaries", [L(*bounds)], 1.0))
    if r.random() < 0.3:
        extra.append(("ENABLE_TIMESTAMP", "enable_timestamp", [B(True), B(True), B(False)], 1.0))
    attr = ("ATTRIBUTE_KEY", "pm4py:param:attribute_key", ("org:resource",))
    res = ("RESOURCE_KEY", "pm4py:param:resource_key", ("org:resource", "rd:kind"))
    c.add("ltl.eventually_follows", [L(*flow), c.params([CASE, TS, attr], extra)])
    c.add("ltl.A_next_B_next_C", [c.act(), c.act(), c.act(), c.params([CASE, attr], [positive])])
    c.add("ltl.four_eyes_principle", [c.act(), c.act(), c.params([CASE, res, attr], [positive])])
    c.add("ltl.attr_value_different_persons", [c.act(), c.params([CASE, res, attr], [positive])])

    # paths
    pairs = [T(c.act(), c.act()) for _ in range(r.randint(0, 3))]
    if r.random() < 0.1:
        pairs.append(L(c.act(), c.act()))
    c.add("paths.apply", [L(*pairs), c.params([CASE, ("ATTRIBUTE_KEY", "pm4py:param:attribute_key", ("org:resource",)), TS],
                                              [positive])])
    extra = [positive,
             ("MIN_PERFORMANCE", "min_performance", [I(0), I(30), I(1800), I(3600), F(1800.5)], 0.5),
             ("MAX_PERFORMANCE", "max_performance", [I(0), I(3600), I(7200), F(86400.5)], 0.5)]
    c.add("paths.apply_performance", [T(c.act(), c.act()), c.params([CASE, TS, ("ATTRIBUTE_KEY", "pm4py:param:attribute_key", ("org:resource",))], extra)])

    # timestamp
    nan = ("KEEP_NAN_VALUES", "keep_nan_values", [B(True), B(False)], 0.15)
    for fn in ("filter_traces_contained", "filter_traces_intersecting", "apply_events",
               "filter_traces_starting_in_timeframe", "filter_traces_completing_in_timeframe"):
        c.add("timestamp." + fn, [c.dt(), c.dt(), c.params([CASE, TS], [nan])])
    key = r.choice((names["act"], "org:resource", "rd:kind"))
    pool = {names["act"]: c.labels, "org:resource": list(RESOURCES), "rd:kind": list(KINDS)}[key]
    c.add("timestamp.filter_traces_attribute_in_timeframe",
          [S(key), S(r.choice(pool)), c.dt(), c.dt(), c.params([CASE, TS], [nan])])
    if r.random() < 0.15:
        c.add("timestamp." + r.choice(("apply", "apply_auto_filter")), [c.params([CASE, TS])])

    # variants
    seen = []
    order = []
    for case_id, a in zip(spec["cases"], spec["acts"]):
        if case_id not in order:
            order.append(case_id)
    for case_id in order:
        v = [a for cid, a in zip(spec["cases"], spec["acts"]) if cid == case_id]
        if v not in seen:
            seen.append(v)
    admitted = [L(*[N() if x is None else S(x) for x in v]) for v in r.sample(seen, min(len(seen), r.randint(0, 3)))]
    if r.random() < 0.3:
        admitted.append(L(*[c.act() for _ in range(r.randint(0, 3))]))
    if r.random() < 0.1:
        admitted.append(T(*[c.act() for _ in range(r.randint(1, 2))]))
    c.add("variants.apply", [L(*admitted), c.params([CASE, ACT], [positive])])
    def counted():
        # The count filters also read the plain timestamp key (no enum member).
        p = c.params([CASE, ACT])
        if r.random() < 0.15:
            p = {"m": (p.get("m", []) if isinstance(p, dict) else [])
                 + [[S("pm4py:param:timestamp_key"), S(r.choice(["missing", c.names.get(TS[2], "time:timestamp")]))]]}
        return p
    c.add("variants.filter_variants_top_k", [number(r, (0, 1, 2, 3, 10, -1)), counted()])
    c.add("variants.filter_variants_by_coverage_percentage",
          [number(r, (0.0, 0.1, 0.2, 0.3, 0.5, 1.0)), counted()])
    c.add("variants.filter_variants_by_maximum_coverage_percentage",
          [number(r, (0.0, 0.1, 0.25, 0.5, 0.75, 1.0)), counted()])
    return c.calls


def generate_cases(count, bank_seed):
    rng = random.Random(bank_seed)
    cases = []
    for _ in range(count):
        r = random.Random(rng.randrange(1 << 30))
        spec = frame_spec(r)
        calls = frame_calls(r, spec)
        cases.append({"spec": spec, "calls": [{"function": fn, "args": args, "kwargs": kw} for fn, args, kw in calls]})
    return cases


# ---------------------------------------------------------------- execution

def source_json(case):
    return tables.pandas_table_json(build_frame(case["spec"]))


def run_call(case, call):
    function = call["function"]
    module_name, name = function.split(".")
    module = importlib.import_module(MODULES[module_name])
    target = getattr(module, name)
    df = build_frame(case["spec"])
    before = tables.pandas_table_json(df)
    args = [sequence.argument(a, module) for a in call["args"]]
    kwargs = {k: sequence.argument(v, module) for k, v in call["kwargs"]}
    try:
        value = target(df, *args, **kwargs)
    except Exception as error:  # noqa: BLE001 - every public outcome is evidence
        outcome = {"error": [type(error).__name__, str(error)]}
    else:
        try:
            outcome = {"value": sequence.encode_result(value)}
        except TypeError as error:
            outcome = {"unrepresentable": str(error)}
    if tables.pandas_table_json(df) != before:
        outcome["source_changed"] = True
    return outcome


if __name__ == "__main__":
    import json
    import warnings
    warnings.filterwarnings("ignore")
    for case in generate_cases(int(sys.argv[1]) if len(sys.argv) > 1 else 3, 7):
        print(json.dumps({"source": source_json(case), "calls": [dict(c, outcome=run_call(case, c)) for c in case["calls"]]}))
