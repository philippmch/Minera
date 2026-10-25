"""Seeded public-call domain for the Polars sequence filters.

Covers every public callable of

  algo.filtering.polars.polars_filtering_constants
  algo.filtering.polars.start_activities.start_activities_filter
  algo.filtering.polars.end_activities.end_activities_filter
  algo.filtering.polars.traces.trace_filter
  algo.filtering.polars.starts_with.starts_with_filter
  algo.filtering.polars.ends_with.ends_with_filter
  algo.filtering.polars.rework.rework_filter
  algo.filtering.polars.prefixes.prefix_filter
  algo.filtering.polars.suffixes.suffix_filter
  algo.filtering.polars.consecutive_act_case_grouping.consecutive_act_case_grouping_filter
  algo.filtering.polars.timestamp_case_grouping.timestamp_case_grouping_filter
  algo.filtering.polars.activity_split.activity_split_filter

Each case is one generated Polars frame (its native event-table JSON travels
as "source"), passed to every call as a fresh LazyFrame. Arguments use the
typed JSON of scripts/filtering_common_domain.py; parameter dictionaries are
{"m": [[key, value], ...]} where a key "enum:NAME" is the module's
Parameters member. A returned frame (collected when lazy) travels as
{"table": event-table JSON}, list cells as native list containers; a tuple as
{"t": [...]}. Errors raised while collecting count as the call's error.

Domain limits: column labels are strings; the grouped-dataframe and
decreasing-factor parameters are never set.
"""
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
pandas_domain = _load("pandas_sequence_filter_domain")
S, I, F, B, N = codec.S, codec.I, codec.F, codec.B, codec.N

ROOT = "pm4py.algo.filtering.polars."
MODULES = {
    "polars_filtering_constants": ROOT + "polars_filtering_constants",
    "start_activities": ROOT + "start_activities.start_activities_filter",
    "end_activities": ROOT + "end_activities.end_activities_filter",
    "traces": ROOT + "traces.trace_filter",
    "starts_with": ROOT + "starts_with.starts_with_filter",
    "ends_with": ROOT + "ends_with.ends_with_filter",
    "rework": ROOT + "rework.rework_filter",
    "prefixes": ROOT + "prefixes.prefix_filter",
    "suffixes": ROOT + "suffixes.suffix_filter",
    "consecutive_act_case_grouping": ROOT + "consecutive_act_case_grouping.consecutive_act_case_grouping_filter",
    "timestamp_case_grouping": ROOT + "timestamp_case_grouping.timestamp_case_grouping_filter",
    "activity_split": ROOT + "activity_split.activity_split_filter",
}

COMMON = ("CASE_CONCEPT_NAME", "PARAMETER_CONSTANT_CASEID_KEY", "PARAMETER_CONSTANT_ACTIVITY_KEY")
CONSTANTS = [
    ("polars_filtering_constants", "MIN_NO_OF_ACTIVITIES_TO_RETAIN_FOR_DIAGRAM"),
    ("polars_filtering_constants", "MAX_NO_OF_ACTIVITIES_TO_RETAIN_FOR_DIAGRAM"),
] + [(m, name) for m in ("start_activities", "end_activities") for name in COMMON + ("DEFAULT_NAME_KEY",)] + [
    ("end_activities", "GROUPED_DATAFRAME"), ("end_activities", "RETURN_EA_COUNT_DICT_AUTOFILTER"),
] + [(m, name) for m in ("traces", "starts_with", "ends_with", "rework", "prefixes", "suffixes") for name in COMMON] + [
    ("timestamp_case_grouping", name) for name in COMMON + (
        "PARAMETER_CONSTANT_TIMESTAMP_KEY", "DEFAULT_TIMESTAMP_KEY", "DEFAULT_NAME_KEY")]

ACTIVITIES = pandas_domain.ACTIVITIES


# ---------------------------------------------------------------- frames

def frame_spec(r):
    spec = pandas_domain.frame_spec(r)
    if spec["extra"] and r.random() < 0.3:
        spec["extra"][r.randrange(len(spec["extra"]))] = "nan"
    if r.random() < 0.08:
        spec["case_kind"] = "cat"
    return spec


def build_frame(spec, sort_by_case=False):
    import datetime as dt
    import polars as pl
    names = spec["names"]
    if spec["case_kind"] == "int":
        case_col = pl.Series(names["case"], spec["cases"], dtype=pl.Int64)
    else:
        values = [None if c is None else str(c) for c in spec["cases"]]
        case_col = pl.Series(names["case"], values, dtype=pl.Categorical if spec["case_kind"] == "cat" else pl.String)
    base = dt.datetime(2020, 1, 1, tzinfo=dt.timezone.utc)
    columns = [case_col,
               pl.Series(names["act"], spec["acts"], dtype=pl.String),
               pl.Series(names["ts"], [base + dt.timedelta(hours=h) for h in spec["hours"]],
                         dtype=pl.Datetime("us", "UTC")),
               pl.Series("rd", list(range(spec["n"])), dtype=pl.Int64)]
    if spec["extra"] is not None:
        columns.append(pl.Series("x", [float("nan") if v == "nan" else v for v in spec["extra"]], dtype=pl.Float64))
    df = pl.DataFrame(columns)
    if spec["order"] == "shuffled" and df.height:
        df = df.sample(fraction=1.0, shuffle=True, seed=spec["shuffle_seed"] % (1 << 31))
    if sort_by_case:
        df = df.sort(names["case"], maintain_order=True, nulls_last=False)
    return df


# ---------------------------------------------------------------- arguments

param_key = pandas_domain.param_key
column_params = pandas_domain.column_params
truthy = pandas_domain.truthy
values_list = pandas_domain.values_list
CA = [("CASE_ID_KEY", "pm4py:param:case_id_key", "case"), ("ACTIVITY_KEY", "pm4py:param:activity_key", "act")]


def pattern_lists(r):
    """Lists of activity lists (prefixes, suffixes, traces)."""
    out = []
    for _ in range(r.randint(0, 3)):
        p = r.choice(pandas_domain.PATTERNS)
        out.append(pandas_domain.pattern_node(p))
    return {"l": out}


def suffix_strings(r):
    pool = ["a", "b", "a,b", "b,c", "", "c", "a,a", "d", "...", "z"]
    return {"l": [S(r.choice(pool)) for _ in range(r.randint(0, 3))]}


def frame_calls(r, spec):
    names = spec["names"]
    custom = names["case"] != "case:concept:name"
    calls = []

    def params(extra_keys, keys=CA):
        entries = []
        column_params(r, spec, keys, entries)
        for enum_name, literal, values in extra_keys:
            if r.random() < 0.6:
                entries.append([param_key(r, enum_name, literal), r.choice(values)])
        r.shuffle(entries)
        if not entries and r.random() < 0.3:
            return N()
        return {"m": entries}

    def glue_kwargs(case_name="case_id_glue"):
        kw = []
        if custom or r.random() < 0.1:
            kw.append([case_name, S(names["case"] if r.random() < 0.9 else "missing")])
            kw.append(["activity_key", S(names["act"] if r.random() < 0.9 else "missing")])
        return kw

    positive = ("POSITIVE", "positive", [B(True), B(False), I(0), I(1), N()])
    calls.append(("start_activities.apply", [values_list(r), params([positive])], []))
    calls.append(("end_activities.apply", [values_list(r), params([positive])], []))
    kw = glue_kwargs()
    if r.random() < 0.5:
        kw.append(["positive", truthy(r)])
    calls.append(("end_activities.filter_df_on_end_activities", [values_list(r)], kw))
    kw = glue_kwargs()
    if r.random() < 0.25:
        counts = {}
        for _ in range(r.randint(0, 3)):
            counts[r.choice(ACTIVITIES)] = r.randint(0, 4)
        kw.append(["ea_count0", codec.encode(counts)])
    if r.random() < 0.5:
        kw.append(["return_dict", B(True)])
    if r.random() < 0.15:
        kw.append(["most_common_variant", {"l": [S(r.choice(ACTIVITIES))]}])
    calls.append(("end_activities.filter_df_on_end_activities_nocc",
                  [r.choice([I(0), I(1), I(2), I(3), F(1.5), I(-1), I(5)])], kw))
    calls.append(("end_activities.get_end_activities", [], glue_kwargs("case_id_key")))
    calls.append(("traces.apply", [pattern_lists(r), params([positive])], []))
    calls.append(("traces.get_variants_df", [], glue_kwargs("case_id_key")))
    calls.append(("starts_with.apply", [pattern_lists(r), params([positive])], []))
    calls.append(("ends_with.apply", [suffix_strings(r) if r.random() < 0.6 else pattern_lists(r),
                                      params([positive])], []))
    calls.append(("ends_with.get_variants_df", [], glue_kwargs("case_id_key")))
    occurrences = ("MIN_OCCURRENCES", "min_occurrences", [I(0), I(1), I(2), I(3), F(2.5), B(True), N()])
    calls.append(("rework.apply", [params([occurrences])], []))
    activities = r.choice([{"S": sorted((S(a) for a in r.sample(ACTIVITIES, r.randint(0, 3))), key=codec.text)},
                           {"l": [S(r.choice(ACTIVITIES))]}, {"S": [S("z")]}])
    calls.append(("rework.apply_activity_set", [activities, params([occurrences])], []))
    for module in ("prefixes", "suffixes"):
        calls.append((module + ".apply", [pattern_lists(r), params([positive])], []))
    ftype = ("FILTER_TYPE", "filter_type", [S("first"), S("last"), S("other")])
    calls.append(("consecutive_act_case_grouping.apply", [params([ftype])], []))
    tkeys = CA + [("TIMESTAMP_KEY", "pm4py:param:timestamp_key", "ts")]
    calls.append(("timestamp_case_grouping.apply", [params([ftype], tkeys)], []))
    return calls


def split_calls(r, spec):
    entries = []
    column_params(r, spec, CA, entries)
    if r.random() < 0.6:
        entries.append([param_key(r, "CUT_MODE", "cut_mode"), S(r.choice(["this", "next", "other"]))])
    if r.random() < 0.4:
        entries.append([param_key(r, "SUBCASE_CONCAT_STR", "subcase_concat_str"), S(r.choice(["##@@", "_", ""]))])
    act = r.choice([S("a"), S("b"), {"l": [S("a"), S("b")]}, {"l": []}, S("z")])
    return [("activity_split.apply", [act, {"m": entries} if entries or r.random() < 0.5 else N()], [])]


def generate_cases(count, bank_seed):
    rng = random.Random(bank_seed)
    cases = []
    for index in range(count):
        seed = rng.randrange(1 << 30)
        r = random.Random(seed)
        spec = frame_spec(r)
        split = r.random() < 0.2
        calls = []
        if index == 0:
            calls += [(f"{m}.{name}", [], []) for m, name in CONSTANTS]
        calls += split_calls(r, spec) if split else frame_calls(r, spec)
        cases.append({"spec": spec, "sorted": False,
                      "calls": [{"function": fn, "args": args, "kwargs": kw} for fn, args, kw in calls]})
    return cases


# ---------------------------------------------------------------- execution

def argument(node, module):
    value = codec.decode(node)
    if isinstance(value, dict) and module is not None:
        out = {}
        for key, item in value.items():
            if isinstance(key, str) and key.startswith("enum:"):
                key = getattr(module.Parameters, key[5:])
            out[key] = item
        return out
    return value


def table_json(df):
    """polars_table_json with List columns as native list containers."""
    import polars as pl
    lists = [c for c in df.columns if isinstance(df[c].dtype, pl.List)]
    if not lists:
        return tables.polars_table_json(df)
    out = tables.polars_table_json(df.drop(lists)) if len(lists) < len(df.columns) else {
        "columns": [], "dtypes": [], "rows": [[] for _ in range(df.height)]}
    columns = list(df.columns)
    plain = [c for c in columns if c not in lists]
    rows = []
    for r in range(df.height):
        by_name = dict(zip(plain, out["rows"][r]))
        row = []
        for c in columns:
            if c in lists:
                items = df[c][r]
                if items is None:
                    row.append({"key": c, "null": True, "container": "list"})
                else:
                    row.append({"key": c, "container": "list",
                                "children": [tables.cell("", v) for v in items.to_list()]})
            else:
                row.append(by_name[c])
        rows.append(row)
    result = {"columns": columns, "dtypes": [str(t) for t in df.dtypes], "rows": rows}
    if "categorical_dtypes" in out:
        cats = dict(zip(plain, out["categorical_dtypes"]))
        result["categorical_dtypes"] = [cats.get(c) for c in columns]
    return result


def encode_result(value):
    import polars as pl
    if isinstance(value, pl.LazyFrame):
        value = value.collect()
    if isinstance(value, pl.DataFrame):
        return {"table": table_json(value)}
    if isinstance(value, tuple):
        return {"t": [encode_result(v) for v in value]}
    if isinstance(value, dict):
        out = []
        for k, v in value.items():
            key = {"f": codec.fbits(k)} if isinstance(k, float) else codec.encode(k)
            out.append([key, codec.encode(v)])
        return {"m": out}
    return codec.encode(value)


def source_json(case):
    return tables.polars_table_json(build_frame(case["spec"], case["sorted"]))


def run_call(case, call):
    function = call["function"]
    module_name, name = function.split(".")
    module = importlib.import_module(MODULES[module_name])
    target = getattr(module, name)
    if not callable(target):
        return {"value": codec.encode(target)}
    lf = build_frame(case["spec"], case["sorted"]).lazy()
    args = [argument(a, module) for a in call["args"]]
    kwargs = {k: argument(v, module) for k, v in call["kwargs"]}
    try:
        outcome = {"value": encode_result(target(lf, *args, **kwargs))}
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
    for case in generate_cases(int(sys.argv[1]) if len(sys.argv) > 1 else 3, 7):
        print(json.dumps({"source": source_json(case), "calls": [dict(c, outcome=run_call(case, c)) for c in case["calls"]]}))
