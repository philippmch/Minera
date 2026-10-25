"""Seeded public-call domain for the pandas sequence filters.

Covers every public callable of

  algo.filtering.pandas.pd_filtering_constants
  algo.filtering.pandas.start_activities.start_activities_filter
  algo.filtering.pandas.end_activities.end_activities_filter
  algo.filtering.pandas.traces.trace_filter
  algo.filtering.pandas.starts_with.starts_with_filter
  algo.filtering.pandas.ends_with.ends_with_filter
  algo.filtering.pandas.rework.rework_filter
  algo.filtering.pandas.prefixes.prefix_filter
  algo.filtering.pandas.suffixes.suffix_filter
  algo.filtering.pandas.consecutive_act_case_grouping.consecutive_act_case_grouping_filter
  algo.filtering.pandas.timestamp_case_grouping.timestamp_case_grouping_filter
  algo.filtering.pandas.activity_split.activity_split_filter

Each case is one generated DataFrame (its native event-table JSON travels as
"source") and a list of calls on fresh copies of it. Scalar arguments use the
typed JSON of scripts/filtering_common_domain.py; parameter dictionaries are
{"m": [[key, value], ...]} where a key "enum:NAME" is the module's
Parameters member. A returned DataFrame travels as {"table": event-table JSON}
and a returned tuple as {"t": [...]}; "source_changed" marks a call that
changed its input frame.

Domain limits: column labels are strings; activity_split frames are ordered by
case (its public result is not row-aligned on frames whose cases interleave);
trace patterns never hold two adjacent "..." wildcards; the grouped_df
arguments stay None.
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
S, I, F, B, N = codec.S, codec.I, codec.F, codec.B, codec.N

MODULES = {
    "pd_filtering_constants": "pm4py.algo.filtering.pandas.pd_filtering_constants",
    "start_activities": "pm4py.algo.filtering.pandas.start_activities.start_activities_filter",
    "end_activities": "pm4py.algo.filtering.pandas.end_activities.end_activities_filter",
    "traces": "pm4py.algo.filtering.pandas.traces.trace_filter",
    "starts_with": "pm4py.algo.filtering.pandas.starts_with.starts_with_filter",
    "ends_with": "pm4py.algo.filtering.pandas.ends_with.ends_with_filter",
    "rework": "pm4py.algo.filtering.pandas.rework.rework_filter",
    "prefixes": "pm4py.algo.filtering.pandas.prefixes.prefix_filter",
    "suffixes": "pm4py.algo.filtering.pandas.suffixes.suffix_filter",
    "consecutive_act_case_grouping":
        "pm4py.algo.filtering.pandas.consecutive_act_case_grouping.consecutive_act_case_grouping_filter",
    "timestamp_case_grouping":
        "pm4py.algo.filtering.pandas.timestamp_case_grouping.timestamp_case_grouping_filter",
    "activity_split": "pm4py.algo.filtering.pandas.activity_split.activity_split_filter",
}

CONSTANTS = [
    ("pd_filtering_constants", "MIN_NO_OF_ACTIVITIES_TO_RETAIN_FOR_DIAGRAM"),
    ("pd_filtering_constants", "MAX_NO_OF_ACTIVITIES_TO_RETAIN_FOR_DIAGRAM"),
    ("rework", "INT_CASE_ACT_SIZE"),
] + [(m, name) for m in ("start_activities", "end_activities")
     for name in ("CASE_CONCEPT_NAME", "DEFAULT_NAME_KEY", "PARAMETER_CONSTANT_CASEID_KEY",
                  "PARAMETER_CONSTANT_ACTIVITY_KEY", "GROUPED_DATAFRAME")] + [
    ("end_activities", "RETURN_EA_COUNT_DICT_AUTOFILTER"),
] + [(m, name) for m in ("traces", "starts_with", "ends_with")
     for name in ("CASE_CONCEPT_NAME", "PARAMETER_CONSTANT_CASEID_KEY", "PARAMETER_CONSTANT_ACTIVITY_KEY")]

ACTIVITIES = ("a", "b", "c", "d")
ODD_ACTIVITIES = ("", "ab", "a,b", "...", "é")


# ---------------------------------------------------------------- frames

def frame_spec(r):
    """A JSON-able description of one DataFrame, rebuilt by build_frame."""
    n = r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14])
    ncases = r.randint(1, 4)
    case_kind = "int" if r.random() < 0.15 else "str"
    cases = [r.randint(1, ncases) for _ in range(n)]
    pool = list(ACTIVITIES[: r.randint(1, 4)])
    if r.random() < 0.2:
        pool.append(r.choice(ODD_ACTIVITIES))
    acts = [r.choice(pool) for _ in range(n)]
    if n and r.random() < 0.2:
        for _ in range(r.randint(1, 2)):
            acts[r.randrange(n)] = None
    if case_kind == "str" and n and r.random() < 0.08:
        cases[r.randrange(n)] = None
    hours = [r.randint(0, 20) if r.random() < 0.3 else i for i in range(n)]
    if n > 1 and r.random() < 0.3:
        for _ in range(r.randint(1, 3)):
            hours[r.randrange(n)] = hours[r.randrange(n)]
    names = {"case": "case:concept:name", "act": "concept:name", "ts": "time:timestamp"}
    if r.random() < 0.15:
        names = {"case": "cid", "act": "act", "ts": "ts"}
    extra = r.random() < 0.3
    extra_values = [r.choice([1.5, 2.0, None, 4.25]) for _ in range(n)] if extra else None
    order = "natural"
    roll = r.random()
    if roll < 0.15:
        index = [r.randint(0, 30) for _ in range(n)]
    elif roll < 0.3:
        index = list(range(n))
        r.shuffle(index)
    else:
        index = None
    if r.random() < 0.25:
        order = "shuffled"
    return {"n": n, "case_kind": case_kind, "cases": cases, "acts": acts, "hours": hours, "names": names,
            "extra": extra_values, "index": index, "order": order, "shuffle_seed": r.randrange(1 << 30)}


def build_frame(spec, sort_by_case=False):
    import pandas as pd
    names = spec["names"]
    if spec["case_kind"] == "int":
        case_col = pd.Series(spec["cases"], dtype="int64")
    else:
        case_col = pd.Series([None if c is None else str(c) for c in spec["cases"]], dtype="str")
    base = pd.Timestamp("2020-01-01", tz="UTC")
    data = {names["case"]: case_col,
            names["act"]: pd.Series(spec["acts"], dtype="str"),
            names["ts"]: pd.Series([base + pd.Timedelta(hours=h) for h in spec["hours"]], dtype="datetime64[us, UTC]"),
            "rd": pd.Series(range(spec["n"]), dtype="int64")}
    if spec["extra"] is not None:
        data["x"] = pd.Series([float("nan") if v is None else v for v in spec["extra"]], dtype="float64")
    df = pd.DataFrame(data)
    if spec["index"] is not None:
        df.index = pd.Index(spec["index"], dtype="int64")
    if spec["order"] == "shuffled" and len(df):
        df = df.sample(frac=1, random_state=spec["shuffle_seed"] % (1 << 31))
    if sort_by_case:
        df = df.sort_values(names["case"], kind="stable")
    return df


# ---------------------------------------------------------------- arguments

def param_key(r, enum_name, literal):
    roll = r.random()
    if roll < 0.55:
        return S("enum:" + enum_name)
    if roll < 0.95:
        return S(literal)
    return S(literal.upper())


def column_params(r, spec, keys, entries):
    names = spec["names"]
    custom = names["case"] != "case:concept:name"
    for enum_name, literal, role in keys:
        if custom or r.random() < 0.12:
            value = names[role] if (custom and r.random() < 0.9) else r.choice(["missing", names[role]])
            entries.append([param_key(r, enum_name, literal), S(value)])


def truthy(r):
    return r.choice([B(True), B(False), B(True), B(False), I(0), I(1), N()])


def values_list(r):
    pool = [S(a) for a in ACTIVITIES] + [S(a) for a in ODD_ACTIVITIES] + [N()]
    return {"l": [r.choice(pool) for _ in range(r.randint(0, 3))]}


PATTERNS = [["a"], ["a", "b"], ["b"], "ab", "a", ["c", "a"], [], ["a", "a"], ("b", "c"), ["..."],
            ["a", "..."], ["...", "b"], ["...", "a", "..."], ["a", "...", "b"], ["a,b"], ["ab"], [""],
            ["a", "", "b"], ["b", "a", "c", "d"], ["...", "c"]]


def pattern_node(p):
    if isinstance(p, str):
        return S(p)
    if isinstance(p, tuple):
        return {"t": [S(x) for x in p]}
    return {"l": [S(x) for x in p]}


def patterns(r):
    return {"l": [pattern_node(r.choice(PATTERNS)) for _ in range(r.randint(0, 3))]}


def activity_arg(r):
    return S(r.choice(["a", "b", "c", "z", "", "ab"]))


def frame_calls(r, spec):
    names = spec["names"]
    custom = names["case"] != "case:concept:name"
    calls = []
    ca = [("CASE_ID_KEY", "pm4py:param:case_id_key", "case"), ("ACTIVITY_KEY", "pm4py:param:activity_key", "act")]

    def params(extra_keys):
        entries = []
        column_params(r, spec, ca, entries)
        for item in extra_keys:
            if r.random() < 0.6:
                enum_name, literal, values = item
                entries.append([param_key(r, enum_name, literal), r.choice(values)])
        r.shuffle(entries)
        if not entries and r.random() < 0.3:
            return N()
        return {"m": entries}

    positive = ("POSITIVE", "positive", [B(True), B(False), I(0), I(1), N()])
    for module in ("start_activities", "end_activities"):
        calls.append((module + ".apply", [values_list(r), params([positive])], []))
        kw = []
        if custom or r.random() < 0.1:
            kw.append(["case_id_glue", S(names["case"] if r.random() < 0.9 else "missing")])
            kw.append(["activity_key", S(names["act"] if r.random() < 0.9 else "missing")])
        if r.random() < 0.5:
            kw.append(["positive", truthy(r)])
        fn = "filter_df_on_start_activities" if module == "start_activities" else "filter_df_on_end_activities"
        calls.append((module + "." + fn, [values_list(r)], kw))
        nocc = r.choice([I(0), I(1), I(2), I(3), F(1.5), I(-1), I(5)])
        kw = []
        if custom or r.random() < 0.1:
            kw.append(["case_id_glue", S(names["case"])])
            kw.append(["activity_key", S(names["act"])])
        if r.random() < 0.25:
            counts = {}
            for _ in range(r.randint(0, 3)):
                counts[r.choice(ACTIVITIES)] = r.randint(0, 4)
            kw.append(["sa_count0" if module == "start_activities" else "ea_count0", codec.encode(counts)])
        if module == "end_activities":
            if r.random() < 0.5:
                kw.append(["return_dict", B(True)])
            if r.random() < 0.15:
                kw.append(["most_common_variant", {"l": [S("a")]}])
        fn = fn + "_nocc"
        calls.append((module + "." + fn, [nocc], kw))
    for module in ("traces", "starts_with", "ends_with"):
        calls.append((module + ".apply", [patterns(r), params([positive])], []))
    calls.append(("rework.apply", [activity_arg(r), params([
        ("MIN_OCCURRENCES", "min_occurrences", [I(0), I(1), I(2), I(3), F(2.5), B(True), S("2"), N()]),
        positive])], []))
    order = ("FIRST_OR_LAST", "first_or_last", [S("first"), S("last"), S("other")])
    strict = ("STRICT", "strict", [B(True), B(False), I(0), I(1)])
    extremes = ("USE_EXTREMES_TIMESTAMP", "use_extremes_timestamp", [B(True), B(False)])
    for module in ("prefixes", "suffixes"):
        calls.append((module + ".apply", [activity_arg(r), params([order, strict, extremes])], []))
    ftype = ("FILTER_TYPE", "filter_type", [S("first"), S("last"), S("other")])
    calls.append(("consecutive_act_case_grouping.apply", [params([ftype])], []))
    tkeys = ca + [("TIMESTAMP_KEY", "pm4py:param:timestamp_key", "ts")]
    entries = []
    column_params(r, spec, tkeys, entries)
    if r.random() < 0.7:
        entries.append([param_key(r, "FILTER_TYPE", "filter_type"), r.choice(ftype[2])])
    calls.append(("timestamp_case_grouping.apply", [{"m": entries}], []))
    return calls


def split_calls(r, spec):
    names = spec["names"]
    entries = []
    column_params(r, spec, [("CASE_ID_KEY", "pm4py:param:case_id_key", "case"),
                            ("ACTIVITY_KEY", "pm4py:param:activity_key", "act")], entries)
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
        cases.append({"spec": spec, "sorted": split,
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


def encode_result(value):
    import pandas as pd
    if isinstance(value, pd.DataFrame):
        return {"table": tables.pandas_table_json(value)}
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
    return tables.pandas_table_json(build_frame(case["spec"], case["sorted"]))


def run_call(case, call):
    function = call["function"]
    module_name, name = function.split(".")
    module = importlib.import_module(MODULES[module_name])
    target = getattr(module, name)
    if not callable(target):
        return {"value": codec.encode(target)}
    df = build_frame(case["spec"], case["sorted"])
    before = tables.pandas_table_json(df)
    args = [argument(a, module) for a in call["args"]]
    kwargs = {k: argument(v, module) for k, v in call["kwargs"]}
    try:
        value = target(df, *args, **kwargs)
    except Exception as error:  # noqa: BLE001 - every public outcome is evidence
        outcome = {"error": [type(error).__name__, str(error)]}
    else:
        try:
            outcome = {"value": encode_result(value)}
        except TypeError as error:
            outcome = {"unrepresentable": str(error)}
    if tables.pandas_table_json(df) != before:
        outcome["source_changed"] = True
    return outcome


if __name__ == "__main__":
    import json
    for case in generate_cases(int(sys.argv[1]) if len(sys.argv) > 1 else 3, 7):
        print(json.dumps({"source": source_json(case), "calls": [dict(c, outcome=run_call(case, c)) for c in case["calls"]]}))
