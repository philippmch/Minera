"""Round CC: the Polars table filter domain with typed case and activity
columns.

Loads scripts/polars_filter_domain.py and rebuilds its frames with typed
activity columns (Categorical, Enum, Int64, the narrow integers Int8 to
UInt64, Float32/Float64, Date, Datetime in us/ns/ms naive, UTC, Europe/Berlin
and Asia/Kolkata; activities a, b, c, d and the odd ones map to fixed values,
null stays null) and typed case columns (Categorical, Enum, the narrow
integers and Float32 with nulls, Datetime, Date). On non-String activity
columns about half of the cases pass the activity values of the calls as
values of the column's type (ints, floats, Python datetimes {"w": ...}),
sometimes mixed with strings in one list."""
import datetime as _dt
import importlib.util
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("polars_filter_domain", HERE / "polars_filter_domain.py")
base = importlib.util.module_from_spec(_spec)
sys.modules["polars_filter_domain"] = base
_spec.loader.exec_module(base)
codec = base.codec

ORDER = ["a", "b", "c", "d", "", "a b", "é", "z"]
INTS = ["Int64", "UInt8", "Int8", "Int16", "Int32", "UInt16", "UInt32", "UInt64"]
FLOATS = ["Float32", "Float64"]
TEMPORAL = ["Date", "Datetime", "DatetimeNs", "DatetimeUTC", "DatetimeMs", "DatetimeMsBerlin", "DatetimeKolkata"]
ACT_KINDS = ["String", "cat", "enum"] + INTS + FLOATS + TEMPORAL
CASE_KINDS = ["str", "int", "cat", "enum", "Int8", "Int16", "Int32", "UInt8", "UInt16", "UInt32", "UInt64",
              "Float32", "Datetime", "Date"]
FUNCTIONS = {"attributes.apply": [0], "attributes.apply_events": [0], "attributes.filter_df_on_attribute_values": [0],
             "between.apply": [0, 1], "ltl.eventually_follows": [0], "ltl.A_next_B_next_C": [0],
             "ltl.four_eyes_principle": [0], "ltl.attr_value_different_persons": [0], "paths.apply": [0],
             "paths.apply_performance": [0], "timestamp.filter_traces_attribute_in_timeframe": [1],
             "variants.apply": [0]}
_base_spec = base.frame_spec


def frame_spec(r):
    spec = _base_spec(r)
    spec["act_kind"] = r.choice(ACT_KINDS) if r.random() < 0.85 else "String"
    if r.random() < 0.5:
        spec["case_kind"] = r.choice(CASE_KINDS)
        if spec["case_kind"] not in ("str", "int") and spec["n"] and r.random() < 0.3:
            spec["cases"][r.randrange(spec["n"])] = None
        if spec["case_kind"] == "int":
            spec["cases"] = [1 if c is None else c for c in spec["cases"]]
    spec["arg_mode"] = r.choice(["plain", "value", "value", "mixed"])
    return spec


def _code(a):
    return None if a is None else (ORDER.index(a) + 1 if a in ORDER else 40)


def _zone(name):
    import zoneinfo
    return zoneinfo.ZoneInfo(name)


def act_value(kind, c):
    if c is None:
        return None
    if kind in INTS:
        return c
    if kind in FLOATS:
        return float(c)
    if kind == "Date":
        return _dt.date(2021, 1, c)
    if kind in ("Datetime", "DatetimeNs"):
        return _dt.datetime(2021, 1, c, c % 24, 0, 0, 1000 * c)
    if kind == "DatetimeUTC":
        return _dt.datetime(2021, 1, c, c % 24, tzinfo=_dt.timezone.utc)
    if kind == "DatetimeMs":
        return _dt.datetime(2021, 2, min(c, 28), c % 24, 30, 0, 7000 * c)
    if kind == "DatetimeMsBerlin":
        return _dt.datetime(2021, 3, 20 + min(c, 11), c % 24, 0, 0, 1000 * c, tzinfo=_zone("Europe/Berlin"))
    if kind == "DatetimeKolkata":
        return _dt.datetime(2021, 1, c, c % 24, 15, tzinfo=_zone("Asia/Kolkata"))
    return c


def _act_dtype(pl, kind):
    return {"Date": pl.Date, "Datetime": pl.Datetime("us"), "DatetimeNs": pl.Datetime("ns"),
            "DatetimeUTC": pl.Datetime("us", "UTC"), "DatetimeMs": pl.Datetime("ms"),
            "DatetimeMsBerlin": pl.Datetime("ms", "Europe/Berlin"),
            "DatetimeKolkata": pl.Datetime("us", "Asia/Kolkata")}.get(kind) or getattr(pl, kind)


def act_series(spec, pl, name):
    kind, acts = spec["act_kind"], spec["acts"]
    if kind == "String":
        return pl.Series(name, acts, dtype=pl.String)
    if kind == "cat":
        return pl.Series(name, acts, dtype=pl.Categorical)
    if kind == "enum":
        pool = sorted({a for a in acts if a is not None} | {"a", "b"}, reverse=True) + ["zz"]
        return pl.Series(name, acts, dtype=pl.Enum(pool))
    return pl.Series(name, [act_value(kind, _code(a)) for a in acts], dtype=_act_dtype(pl, kind))


def case_series(spec, pl, name):
    kind, cases = spec["case_kind"], spec["cases"]
    if kind == "int":
        return pl.Series(name, cases, dtype=pl.Int64)
    text = [None if c is None else str(c) for c in cases]
    if kind == "str":
        return pl.Series(name, text, dtype=pl.String)
    if kind == "cat":
        return pl.Series(name, text, dtype=pl.Categorical)
    if kind == "enum":
        return pl.Series(name, text, dtype=pl.Enum(["4", "3", "2", "1", "9"]))
    if kind == "Float32":
        return pl.Series(name, [None if c is None else c + 0.5 for c in cases], dtype=pl.Float32)
    if kind == "Datetime":
        return pl.Series(name, [None if c is None else _dt.datetime(2021, 5, c, 7) for c in cases],
                         dtype=pl.Datetime("us"))
    if kind == "Date":
        return pl.Series(name, [None if c is None else _dt.date(2021, 5, c) for c in cases], dtype=pl.Date)
    return pl.Series(name, cases, dtype=getattr(pl, kind))


def build_frame(spec):
    import polars as pl
    names = spec["names"]
    b = base.BASE.replace(tzinfo=_dt.timezone.utc)
    stamps = [None if m is None else b + _dt.timedelta(minutes=m) for m in spec["minutes"]]
    columns = [case_series(spec, pl, names["case"]),
               act_series(spec, pl, names["act"]),
               pl.Series(names["ts"], stamps, dtype=pl.Datetime("us", "UTC")),
               pl.Series("org:resource", spec["resources"], dtype=pl.String),
               pl.Series("rd:amount", spec["amounts"], dtype=pl.Int64 if spec["amount_kind"] == "int" else pl.Float64),
               pl.Series("rd:kind", spec["kinds"], dtype=pl.String),
               pl.Series("rd", list(range(spec["n"])), dtype=pl.Int64)]
    df = pl.DataFrame(columns)
    if spec["order"] == "shuffled" and df.height:
        df = df.sample(fraction=1.0, shuffle=True, seed=spec["shuffle_seed"] % (1 << 31))
    return df


def _value_arg(r, kind, text, mode):
    if text not in ORDER or (mode == "mixed" and r.random() < 0.3):
        return {"s": text}
    value = act_value(kind, _code(text))
    if isinstance(value, bool) or value is None:
        return {"s": text}
    if isinstance(value, int):
        pick = r.random()
        if pick < 0.75:
            return {"i": str(value)}
        return {"f": codec.fbits(float(value))}
    if isinstance(value, float):
        return {"f": codec.fbits(value)} if r.random() < 0.6 else {"i": str(int(value))}
    if isinstance(value, _dt.datetime):
        pick = r.random()
        if value.tzinfo is not None:
            if pick < 0.5:
                return codec.wall(value)
            if pick < 0.8:
                return codec.wall(value.astimezone(_dt.timezone.utc))
            return codec.wall(value.replace(tzinfo=None))
        if pick < 0.85 or mode != "mixed":
            return codec.wall(value)
        return codec.wall(value.replace(tzinfo=_dt.timezone.utc))
    if isinstance(value, _dt.date):
        return codec.wall(_dt.datetime(value.year, value.month, value.day))
    return {"s": text}


def transform(r, node, kind, mode):
    (tag, value), = node.items()
    if tag == "s":
        return _value_arg(r, kind, value, mode)
    if tag in ("l", "t"):
        return {tag: [transform(r, v, kind, mode) for v in value]}
    return node


_base_generate = base.generate_cases


def generate_cases(count, bank_seed):
    cases = _base_generate(count, bank_seed)
    rng = random.Random(bank_seed ^ 0xCC)
    for case in cases:
        spec = case["spec"]
        r = random.Random(rng.randrange(1 << 30))
        kind, mode = spec["act_kind"], spec["arg_mode"]
        if mode == "plain" or kind in ("String", "cat", "enum"):
            continue
        for call in case["calls"]:
            for position in FUNCTIONS.get(call["function"], []):
                if position < len(call["args"]) and "m" not in call["args"][position]:
                    call["args"][position] = transform(r, call["args"][position], kind, mode)
    return cases


base.frame_spec = frame_spec
base.build_frame = build_frame
for _k in dir(base):
    if not _k.startswith("__"):
        globals().setdefault(_k, getattr(base, _k))
globals()["frame_spec"] = frame_spec
globals()["build_frame"] = build_frame
globals()["generate_cases"] = generate_cases
