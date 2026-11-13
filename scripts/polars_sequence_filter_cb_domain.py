"""Round CB: the Polars sequence filter domain with temporal and integer
activity columns.

Loads scripts/polars_sequence_filter_typed_domain.py and adds activity
columns of Int64, UInt8, Datetime (us, naive or UTC; ns, naive or UTC; ms
in Europe/Berlin across the 2021 DST change) and Date dtypes (activities a, b, c, d and the odd ones map to fixed values). On
integer activity columns the activity arguments of the calls take the mapped
integers instead of the strings in about half of the cases."""
import importlib.util, sys
from pathlib import Path
HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("polars_sequence_filter_typed_domain", HERE / "polars_sequence_filter_typed_domain.py")
typed = importlib.util.module_from_spec(_spec)
sys.modules["polars_sequence_filter_typed_domain"] = typed
_spec.loader.exec_module(typed)
base = typed.base
ORDER = ["a", "b", "c", "d", "", "ab", "a,b", "...", "é", "z"]
ACT_KINDS = ["Int64", "UInt8", "Datetime", "DatetimeUTC", "DatetimeNs", "Date", "DatetimeNsUTC",
             "DatetimeMsBerlin"]
VALUE_FUNCTIONS = {"start_activities.apply", "end_activities.apply", "end_activities.filter_df_on_end_activities",
                   "traces.apply", "starts_with.apply", "ends_with.apply", "prefixes.apply", "suffixes.apply",
                   "rework.apply_activity_set", "activity_split.apply"}
_typed_spec = typed.frame_spec
def frame_spec(r):
    spec = _typed_spec(r)
    if r.random() < 0.8:
        spec["act_kind"] = r.choice(ACT_KINDS)
    spec["int_args"] = r.random() < 0.5
    return spec
def _code(a):
    return None if a is None else (ORDER.index(a) + 1 if a in ORDER else 40)
def _act(spec, pl, name):
    import datetime as dt
    k, acts = spec["act_kind"], spec["acts"]
    codes = [_code(a) for a in acts]
    if k in ("Int64", "UInt8"):
        return pl.Series(name, codes, dtype=getattr(pl, k))
    if k == "Date":
        return pl.Series(name, [None if c is None else dt.date(2021, 1, c) for c in codes], dtype=pl.Date)
    if k == "DatetimeMsBerlin":
        import zoneinfo
        berlin = zoneinfo.ZoneInfo("Europe/Berlin")
        values = [None if c is None else dt.datetime(2021, 3, 20 + min(c, 11), c % 24, 0, 0, 1000 * c, tzinfo=berlin)
                  for c in codes]
        return pl.Series(name, values, dtype=pl.Datetime("ms", "Europe/Berlin"))
    zone = dt.timezone.utc if k in ("DatetimeUTC", "DatetimeNsUTC") else None
    values = [None if c is None else dt.datetime(2021, 1, c, c % 24, tzinfo=zone) for c in codes]
    unit = "ns" if k in ("DatetimeNs", "DatetimeNsUTC") else "us"
    return pl.Series(name, values, dtype=pl.Datetime(unit, "UTC" if zone else None))
_typed_act = typed._act
def act_series(spec, pl, name):
    if spec["act_kind"] in ACT_KINDS:
        return _act(spec, pl, name)
    return _typed_act(spec, pl, name)
typed._act = act_series
def _to_int(node):
    (tag, value), = node.items()
    if tag == "s":
        return {"i": str(_code(value))} if value in ORDER else node
    if tag in ("l", "t", "S"):
        return {tag: [_to_int(v) for v in value]}
    return node
_typed_generate = typed.generate_cases
def generate_cases(count, bank_seed):
    cases = _typed_generate(count, bank_seed)
    for case in cases:
        spec = case["spec"]
        if spec.get("act_kind") in ("Int64", "UInt8") and spec.get("int_args"):
            for call in case["calls"]:
                if call["function"] in VALUE_FUNCTIONS and call["args"] and "m" not in call["args"][0]:
                    call["args"][0] = _to_int(call["args"][0])
    return cases
base.frame_spec = frame_spec
typed.frame_spec = frame_spec
for _k in dir(typed):
    if not _k.startswith("__"): globals().setdefault(_k, getattr(typed, _k))
globals()["frame_spec"] = frame_spec
globals()["generate_cases"] = generate_cases
