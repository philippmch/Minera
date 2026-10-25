"""Seeded public-call domain for the Polars attribute, between, case, ltl,
paths, timestamp and variants filters.

Covers every public callable of

  algo.filtering.polars.attributes.attributes_filter
  algo.filtering.polars.between.between_filter
  algo.filtering.polars.cases.case_filter
  algo.filtering.polars.ltl.ltl_checker
  algo.filtering.polars.paths.paths_filter
  algo.filtering.polars.timestamp.timestamp_filter
  algo.filtering.polars.variants.variants_filter

Each case is one generated Polars frame (from the frame specs of
scripts/pandas_filter_domain.py; its native event-table JSON travels as
"source"), passed to every call as a fresh LazyFrame, in the call format of
scripts/polars_sequence_filter_domain.py. Datetime arguments use the
{"w": [wall clock, UTC offset minutes or null]} form of
scripts/filtering_common_domain.py.

Domain limits: column labels are strings; business-hour case performance
uses the default slots; the decreasing factor is never set.
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
pandas_domain = _load("pandas_filter_domain")
sequence = _load("polars_sequence_filter_domain")
S, I, F, B, N = codec.S, codec.I, codec.F, codec.B, codec.N
L, T, W = pandas_domain.L, pandas_domain.T, pandas_domain.W
RESOURCES, KINDS, BASE = pandas_domain.RESOURCES, pandas_domain.KINDS, pandas_domain.BASE
CASE, ACT, TS = pandas_domain.CASE, pandas_domain.ACT, pandas_domain.TS
number, wild = pandas_domain.number, pandas_domain.wild

ROOT = "pm4py.algo.filtering.polars."
MODULES = {
    "attributes": ROOT + "attributes.attributes_filter",
    "between": ROOT + "between.between_filter",
    "cases": ROOT + "cases.case_filter",
    "ltl": ROOT + "ltl.ltl_checker",
    "paths": ROOT + "paths.paths_filter",
    "timestamp": ROOT + "timestamp.timestamp_filter",
    "variants": ROOT + "variants.variants_filter",
}

CONSTANTS = [(m, name) for m, names in {
    "attributes": ("CASE_CONCEPT_NAME", "DEFAULT_NAME_KEY", "PARAMETER_CONSTANT_ACTIVITY_KEY",
                   "PARAMETER_CONSTANT_ATTRIBUTE_KEY", "PARAMETER_CONSTANT_CASEID_KEY"),
    "ltl": ("CASE_CONCEPT_NAME", "PARAMETER_CONSTANT_ATTRIBUTE_KEY", "PARAMETER_CONSTANT_CASEID_KEY",
            "PARAMETER_CONSTANT_RESOURCE_KEY", "PARAMETER_CONSTANT_TIMESTAMP_KEY", "DEFAULT_NAME_KEY",
            "DEFAULT_RESOURCE_KEY", "DEFAULT_TIMESTAMP_KEY"),
    "paths": ("CASE_CONCEPT_NAME", "DEFAULT_NAME_KEY", "DEFAULT_TIMESTAMP_KEY", "PARAMETER_CONSTANT_ATTRIBUTE_KEY",
              "PARAMETER_CONSTANT_CASEID_KEY", "PARAMETER_CONSTANT_TIMESTAMP_KEY", "DEFAULT_VARIANT_SEP"),
    "timestamp": ("CASE_CONCEPT_NAME", "PARAMETER_CONSTANT_CASEID_KEY", "PARAMETER_CONSTANT_TIMESTAMP_KEY",
                  "DEFAULT_TIMESTAMP_KEY"),
    "variants": ("CASE_CONCEPT_NAME", "PARAMETER_CONSTANT_CASEID_KEY", "PARAMETER_CONSTANT_ACTIVITY_KEY"),
}.items() for name in names]


# ---------------------------------------------------------------- frames

def frame_spec(r):
    spec = pandas_domain.frame_spec(r)
    if r.random() < 0.08 and spec["case_kind"] == "str":
        spec["case_kind"] = "cat"
    return spec


def build_frame(spec):
    import polars as pl
    names = spec["names"]
    if spec["case_kind"] == "int":
        case_col = pl.Series(names["case"], spec["cases"], dtype=pl.Int64)
    else:
        values = [None if c is None else str(c) for c in spec["cases"]]
        case_col = pl.Series(names["case"], values, dtype=pl.Categorical if spec["case_kind"] == "cat" else pl.String)
    base = BASE.replace(tzinfo=datetime.timezone.utc)
    stamps = [None if m is None else base + datetime.timedelta(minutes=m) for m in spec["minutes"]]
    amounts = spec["amounts"]
    columns = [case_col,
               pl.Series(names["act"], spec["acts"], dtype=pl.String),
               pl.Series(names["ts"], stamps, dtype=pl.Datetime("us", "UTC")),
               pl.Series("org:resource", spec["resources"], dtype=pl.String),
               pl.Series("rd:amount", amounts, dtype=pl.Int64 if spec["amount_kind"] == "int" else pl.Float64),
               pl.Series("rd:kind", spec["kinds"], dtype=pl.String),
               pl.Series("rd", list(range(spec["n"])), dtype=pl.Int64)]
    df = pl.DataFrame(columns)
    if spec["order"] == "shuffled" and df.height:
        df = df.sample(fraction=1.0, shuffle=True, seed=spec["shuffle_seed"] % (1 << 31))
    return df


# ---------------------------------------------------------------- calls

def frame_calls(r, spec):
    c = pandas_domain.Calls(r, spec)
    names = spec["names"]
    positive = ("POSITIVE", "positive", [B(True), B(False), B(False), I(0), I(1), N()], 0.5)
    attr_choices = (names["act"], "org:resource", "rd:kind", names["act"])

    # attributes
    for fn in ("apply", "apply_events"):
        key = r.choice(attr_choices)
        pool = {names["act"]: c.labels + ["z"], "org:resource": list(RESOURCES), "rd:kind": list(KINDS)}[key]
        values = [S(v) for v in r.sample(pool, r.randint(0, min(3, len(pool))))]
        if r.random() < 0.1:
            values.append(N())
        extra = [("ATTRIBUTE_KEY", "pm4py:param:attribute_key", [S(key)], 0.85 if key != names["act"] else 0.3),
                 positive]
        c.add("attributes." + fn, [L(*values), c.params([CASE], extra)])
    for fn in ("apply_numeric", "apply_numeric_events"):
        extra = [("ATTRIBUTE_KEY", "pm4py:param:attribute_key",
                  [S("rd:amount"), S("rd:amount"), S("rd:amount"), S("rd"), S("rd:kind")], 0.95), positive]
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
    extra = [("ATTRIBUTE_KEY", "pm4py:param:attribute_key", [S(v) for v in attr_choices], 0.4),
             ("KEEP_ONCE_PER_CASE", "keep_once_per_case", [B(True), B(False)], 0.5)]
    c.add("attributes.filter_df_relative_occurrence_event_attribute",
          [number(r, (0.0, 0.1, 0.3, 0.5, 0.9, 1.0)), c.params([CASE], extra)])
    c.add("attributes.get_attribute_values", [S(r.choice(attr_choices + ("rd:amount", "missing"))),
                                              c.params([CASE])])

    # between
    one = lambda: L(*[c.act() for _ in range(r.randint(1, 2))]) if r.random() < 0.25 else c.act()
    extra = [("SUBCASE_CONCAT_STR", "subcase_concat_str", [S("##@@"), S("-"), S("")], 0.2)]
    c.add("between.apply", [one(), one(), c.params([CASE, ACT], extra)])

    # cases
    kw = []
    if c.custom or r.random() < 0.15:
        kw.append(["case_id_glue", S(names["case"] if r.random() < 0.9 else "missing")])
    c.add("cases.filter_on_ncases", [],
          kw + ([["max_no_cases", number(r, (0, 1, 2, 3, 1000, -1, 2.5))]] if r.random() < 0.85 else []))
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
        c.add("cases." + r.choice(("apply", "apply_auto_filter")), [c.params([CASE, TS])])

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
    c.add("ltl.A_next_B_next_C", [L(*[c.act() for _ in range(r.choice((3, 3, 3, 2, 4)))]),
                                  c.params([CASE, attr], [positive])])
    c.add("ltl.four_eyes_principle", [L(*[c.act() for _ in range(r.choice((2, 2, 2, 1, 3)))]),
                                      c.params([CASE, res, attr], [positive])])
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
    c.add("paths.apply_performance", [T(c.act(), c.act()),
                                      c.params([CASE, TS, ("ATTRIBUTE_KEY", "pm4py:param:attribute_key", ("org:resource",))], extra)])

    # timestamp
    for fn in ("filter_traces_contained", "filter_traces_intersecting", "apply_events"):
        c.add("timestamp." + fn, [c.dt(), c.dt(), c.params([CASE, TS])])
    key = r.choice((names["act"], "org:resource", "rd:kind"))
    pool = {names["act"]: c.labels, "org:resource": list(RESOURCES), "rd:kind": list(KINDS)}[key]
    c.add("timestamp.filter_traces_attribute_in_timeframe",
          [S(key), S(r.choice(pool)), c.dt(), c.dt(), c.params([CASE, TS])])
    if r.random() < 0.15:
        c.add("timestamp." + r.choice(("apply", "apply_auto_filter")), [c.params([CASE, TS])])

    # variants
    seen, order = [], []
    for case_id in spec["cases"]:
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
    strings = [S(",".join("" if x is None else x for x in v)) for v in r.sample(seen, min(len(seen), r.randint(0, 3)))]
    if r.random() < 0.3:
        strings.append(S(",".join(c.act()["s"] for _ in range(r.randint(1, 3)))))
    c.add("variants.apply_from_variant_list", [L(*strings), c.params([CASE, ACT], [positive])])
    c.add("variants.filter_variants_top_k", [number(r, (0, 1, 2, 3, 10, -1)), c.params([CASE, ACT])])
    if r.random() < 0.3:
        c.add("variants.apply_auto_filter", [c.params([CASE, ACT])])
    return c.calls


def generate_cases(count, bank_seed):
    rng = random.Random(bank_seed)
    cases = []
    for index in range(count):
        r = random.Random(rng.randrange(1 << 30))
        spec = frame_spec(r)
        calls = [(f"{m}.{name}", [], []) for m, name in CONSTANTS] if index == 0 else []
        calls += frame_calls(r, spec)
        cases.append({"spec": spec, "calls": [{"function": fn, "args": args, "kwargs": kw} for fn, args, kw in calls]})
    return cases


# ---------------------------------------------------------------- execution

def source_json(case):
    return tables.polars_table_json(build_frame(case["spec"]))


def run_call(case, call):
    function = call["function"]
    module_name, name = function.split(".")
    module = importlib.import_module(MODULES[module_name])
    target = getattr(module, name)
    if not callable(target):
        return {"value": codec.encode(target)}
    lf = build_frame(case["spec"]).lazy()
    args = [sequence.argument(a, module) for a in call["args"]]
    kwargs = {k: sequence.argument(v, module) for k, v in call["kwargs"]}
    try:
        outcome = {"value": sequence.encode_result(target(lf, *args, **kwargs))}
    except TypeError as error:
        if "no native cell" in str(error) or "unrepresentable" in str(error):
            outcome = {"unrepresentable": str(error)}
        else:
            outcome = {"error": [type(error).__name__, str(error)]}
    except Exception as error:  # noqa: BLE001 - every public outcome is evidence
        outcome = {"error": [type(error).__name__, str(error)]}
    return outcome


if __name__ == "__main__":
    import json
    import warnings
    warnings.filterwarnings("ignore")
    for case in generate_cases(int(sys.argv[1]) if len(sys.argv) > 1 else 3, 7):
        print(json.dumps({"source": source_json(case), "calls": [dict(c, outcome=run_call(case, c)) for c in case["calls"]]}))
